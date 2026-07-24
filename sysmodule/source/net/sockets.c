#include "../core.h"

#if NEEDS_SOCKETS
#include <string.h>
#include <fcntl.h> // fcntl
#include <netinet/tcp.h> // IPPROTO_TCP, TCP_NODELAY

#include "sockets.h"
#define NX_EAGAIN 11
#define NX_O_NONBLOCK 0x800

#include "../modes/modes.h"
#include "../capture.h"

// We want to statically allocate all socketing resources to avoid heap fragmentation
// This means we use the bsd service API directly instead of the libnx wrapper

static bool SocketReady;
static bool HasNifm;

#define PAGE_ALIGN(x) ((x + 0xFFF) &~ 0xFFF)

#define NON_ZERO(x, y) (x == 0 ? y : x)

// This is our biggest offender in memory usage but lower values may cause random hanging over long streaming sessions.
// They can be workarounded with auto reconnection and non-blocking sockets but i prefer keeping the connection stable
// Having enough capacity ensures that we can handle any packet size without dropping frames.
#define TCP_TX_SZ (16 * 1024)
#define TCP_RX_SZ (8 * 1024)

#define TCP_TX_MAX_SZ (200 * 1024)
#define TCP_RX_MAX_SZ 0

#define UDP_TX_SZ (8 * 1024)
#define UDP_RX_SZ (4 * 1024)

#define SOCK_EFFICIENCY 2

// Formula taken from libnx itslf
#define TMEM_SIZE PAGE_ALIGN(NON_ZERO(TCP_TX_MAX_SZ, TCP_TX_SZ) + NON_ZERO(TCP_RX_MAX_SZ, TCP_RX_SZ) + UDP_TX_SZ + UDP_RX_SZ) * SOCK_EFFICIENCY

static u8 alignas(0x1000) TmemBackingBuffer[TMEM_SIZE];

// Match libnx's socket wrapper. Newer bsd service versions are required for
// correct multicast behavior, which Chromecast discovery depends on.
static u32 SocketSelectServiceVersion()
{
	if (hosversionBefore(3, 0, 0))
		return 1;
	if (hosversionBefore(4, 0, 0))
		return 2;
	if (hosversionBefore(5, 0, 0))
		return 3;
	if (hosversionBefore(6, 0, 0))
		return 4;
	if (hosversionBefore(8, 0, 0))
		return 5;
	if (hosversionBefore(9, 0, 0))
		return 6;
	if (hosversionBefore(13, 0, 0))
		return 7;
	if (hosversionBefore(16, 0, 0))
		return 8;
	return 9;
}

#if UDP_LOGGING
#define TARGET_DEBUG_IP "192.168.178.66"

#include <stdarg.h>
#include "../third_party/nanoprintf.h"
static struct sockaddr_in loggingDest;
static int udpLogSocket;

void LogFunctionImpl(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buf[0x100];
	int n = npf_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (loggingDest.sin_port != 0)
		SocketUDPSendTo(udpLogSocket, buf, n, (struct sockaddr*)&loggingDest, sizeof(loggingDest));
}
#endif

void SocketInit()
{
	if (SocketReady)
		return;

	memset(TmemBackingBuffer, 0, sizeof(TmemBackingBuffer));

	const BsdInitConfig config = {
		.version = SocketSelectServiceVersion(),
		.tmem_buffer = TmemBackingBuffer,
		.tmem_buffer_size = TMEM_SIZE,

		.tcp_tx_buf_size = TCP_TX_SZ,
		.tcp_rx_buf_size = TCP_RX_SZ,
		.tcp_tx_buf_max_size = TCP_TX_MAX_SZ,
		.tcp_rx_buf_max_size = TCP_RX_MAX_SZ,

		.udp_tx_buf_size = UDP_TX_SZ,
		.udp_rx_buf_size = UDP_RX_SZ,

		.sb_efficiency = SOCK_EFFICIENCY
	};

	R_THROW(bsdInitialize(&config, 3, BsdServiceType_User));

#if UDP_LOGGING
	udpLogSocket = SocketUdp();
	// parse ip
	loggingDest.sin_family = AF_INET;
	loggingDest.sin_port = htons(9999);
	loggingDest.sin_addr.s_addr = inet_addr(TARGET_DEBUG_IP);
#endif

	LOG("Initialied BSD with tmem size %x\n", TMEM_SIZE);
	SocketReady = true;

	Result rc = nifmInitialize(NifmServiceType_User);
	if (R_FAILED(rc))
		LOG("Failed to initialize nifm: %x\n", rc);
	else
		HasNifm = true;
}

void SocketDeinit()
{
	if (!SocketReady)
		return;

	if (HasNifm)
	{
		LOG("Exiting NIFM\n");
		nifmExit();
		HasNifm = false;
	}

	LOG("Exiting BSD\n");
	bsdExit();

	SocketReady = false;
}

void SocketClose(int* socket)
{
	if (*socket != SOCKET_INVALID)
	{
		bsdClose(*socket);
		*socket = SOCKET_INVALID;
	}
}

int SocketUdp()
{
	return bsdSocket(AF_INET, SOCK_DGRAM, 0);
}

int SocketTcpListen(short port)
{
	while (true) {
		int socket = bsdSocket(AF_INET, SOCK_STREAM, 0);
		if (socket < 0)
			goto failed;

		if (!SocketMakeNonBlocking(socket))
			goto failed;

		const int optVal = 1;
		if (bsdSetSockOpt(socket, SOL_SOCKET, SO_REUSEADDR, (void*)&optVal, sizeof(optVal)) == -1)
			goto failed;

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);

		if (bsdBind(socket, (struct sockaddr*)&addr, sizeof(addr)) == -1)
			goto failed;

		// Cast Web Receiver pages request HTML, CSS, and JavaScript in quick
		// succession. Keep a small pending queue so those local asset requests
		// are not refused while the launch loop services the previous one.
		if (bsdListen(socket, 4) == -1)
			goto failed;

		LOG("%d listening on %d\n", socket, (int)port);
		return socket;

	failed:
		LOG("SocketTcpListen failed %d\n", (int)port);

		if (socket != SOCKET_INVALID)
			bsdClose(socket);

		svcSleepThread(1);
		continue;
	}
}

int SocketTcpConnect(u32 ipv4Address, short port)
{
	int socket = bsdSocket(AF_INET, SOCK_STREAM, 0);
	if (socket == SOCKET_INVALID)
		return SOCKET_INVALID;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ipv4Address;
	addr.sin_port = htons(port);

	if (bsdConnect(socket, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		SocketClose(&socket);
		return SOCKET_INVALID;
	}

	const int optVal = 1;
	bsdSetSockOpt(socket, IPPROTO_TCP, TCP_NODELAY, &optVal, sizeof(optVal));
	return socket;
}

// This function must be called after SocketTcpAccept returns SOCKET_INVALID
// We use a weird hack, we need to figure out when the console is in sleep mode
// and reset the listening socket when it wakes up, the only way i found to get
// a meaningful error code is from poll, which will return 0 when no connection is pending
// and 1 otherwise but if Accept fails with EAGAIN, we know the console was in sleep mode.
// We should use nifm but that comes with its share of weirdness.....
static bool SocketIsListenNetDown()
{
	bool rc = g_bsdErrno == NX_EAGAIN;
#if UDP_LOGGING
	// Reset the logging socket if we lost connection
	if (rc)
	{
		SocketClose(&udpLogSocket);
		udpLogSocket = SocketUdp();
	}
#endif
	return rc;
}

SocketAcceptResult SocketTcpAccept(int listenerHandle, int* outAccepted, struct sockaddr* out_addr, socklen_t* out_addrlen)
{
	struct pollfd pollinfo;
	pollinfo.fd = listenerHandle;
	pollinfo.events = POLLIN;
	pollinfo.revents = 0;

	int rc = bsdPoll(&pollinfo, 1, 0);
	if (rc > 0)
	{
		if (pollinfo.revents & POLLIN)
		{
			int accepted = bsdAccept(listenerHandle, out_addr, out_addrlen);

			if (accepted != SOCKET_INVALID)
			{
				// Set TCP_NODELAY
				int optVal = 1;
				bsdSetSockOpt(accepted, IPPROTO_TCP, TCP_NODELAY, &optVal, sizeof(optVal));
				*outAccepted = accepted;
				return SocketAcceptError_OK;
			}
			else
			{
				*outAccepted = SOCKET_INVALID;
				if (SocketIsListenNetDown())
					return SocketAcceptError_NetDown;
				else
					return SocketAcceptError_Fail;
			}
		}
	}

	*outAccepted = SOCKET_INVALID;
	return SocketAcceptError_Fail;
}

bool SocketUDPSendTo(int socket, const void* data, u32 size, struct sockaddr* addr, socklen_t addrlen)
{
	return bsdSendTo(socket, data, size, 0, addr, addrlen) == size;
}

s32 SocketUDPRecvFrom(int socket, void* data, u32 size, struct sockaddr* addr, socklen_t* addrlen)
{
	ssize_t result = bsdRecvFrom(socket, data, size, MSG_DONTWAIT, addr, addrlen);
	if (result == -1)
	{
		if (g_bsdErrno == NX_EAGAIN)
			return 0;
		return -1;
	}
	return (s32)result;
}

bool SocketBind(int socket, struct sockaddr* addr, socklen_t addrlen)
{
	return bsdBind(socket, addr, addrlen) != -1;
}

bool SocketSetReuseAddress(int socket, bool allow)
{
	const int optVal = allow ? 1 : 0;
	return bsdSetSockOpt(socket, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal)) != -1;
}

bool SocketJoinMulticast(int socket, u32 multicastAddress)
{
	struct ip_mreq request;
	memset(&request, 0, sizeof(request));
	request.imr_multiaddr.s_addr = multicastAddress;
	request.imr_interface.s_addr = INADDR_ANY;
	return bsdSetSockOpt(
		socket,
		IPPROTO_IP,
		IP_ADD_MEMBERSHIP,
		&request,
		sizeof(request)) != -1;
}

SocketWaitResult SocketWaitReadableEx(int socket, int timeoutMs)
{
	struct pollfd pollinfo;
	pollinfo.fd = socket;
	pollinfo.events = POLLIN;
	pollinfo.revents = 0;

	int result = bsdPoll(&pollinfo, 1, timeoutMs);
	if (result == 0)
		return SocketWaitResult_Timeout;
	if (result < 0)
		return SocketWaitResult_Error;
	if (pollinfo.revents & (POLLERR | POLLHUP | POLLNVAL))
		return SocketWaitResult_Closed;
	if (pollinfo.revents & POLLIN)
		return SocketWaitResult_Readable;
	return SocketWaitResult_Error;
}

bool SocketWaitReadable(int socket, int timeoutMs)
{
	return SocketWaitReadableEx(socket, timeoutMs) ==
		SocketWaitResult_Readable;
}

typedef enum {
	PollResult_Timeout,
	PollResult_Disconnected,
	PollResult_CanWrite,
	PollResult_CanRead,
	PollResult_Other
} PollResult;

static PollResult PolLScoket(int socket, int timeoutMs)
{
	struct pollfd pollinfo;
	pollinfo.fd = socket;
	pollinfo.events = POLLOUT | POLLIN;
	pollinfo.revents = 0;

	int rc = bsdPoll(&pollinfo, 1, timeoutMs);
	LOG("%d poll %x\n", socket, pollinfo.revents);
	if (rc > 0)
	{
		if (pollinfo.revents & (POLLERR | POLLHUP | POLLNVAL))
			return PollResult_Disconnected;

		// POLLIN only means that data (or an orderly shutdown) can be read; it
		// does not mean the connection is dead. In particular, an HTTP client
		// can leave request bytes readable while POLLOUT also says the rest of
		// a large segment can be sent. Prefer the writable condition.
		if (pollinfo.revents & POLLOUT)
			return PollResult_CanWrite;
		if (pollinfo.revents & POLLIN)
			return PollResult_CanRead;

		return PollResult_Other;
	}

	return PollResult_Timeout;
}

bool SocketSendAll(int sock, const void* buffer, u32 size)
{
	if (sock == SOCKET_INVALID)
		return false;

	u32 sent = 0;
	while (sent < size)
	{
		int res = bsdSend(sock, (const char*)buffer + sent, size - sent, 0);
		if (res == -1)
		{
			if (g_bsdErrno == NX_EAGAIN)
			{
				int pollCount = 0;
			poll_again:
				PollResult pollRes = PolLScoket(sock, 1000);

				// Leave early if we're switching modes
				if (!IsThreadRunning)
					return false;

				// after 10 seconds we give up (and the user probably did too)
				if (++pollCount >= 10)
					return false;

				// If we can write, retry
				if (pollRes == PollResult_CanWrite)
					continue;
				else if (pollRes == PollResult_Timeout)
					goto poll_again;

				// We don't expect to receive data from the client, so any other
				// result is probably an error and we close the socket on our end
				return false;
			}
			// Any other error is fatal
			return false;
		}
		else if (res == 0)
			return false;

		sent += res;
	}

	return true;
}

s32 SocketRecv(int socket, void* buffer, u32 size)
{
	ssize_t r = bsdRecv(socket, buffer, size, MSG_DONTWAIT);
	if (r == -1)
	{
		if (g_bsdErrno == NX_EAGAIN) 
		{
			svcSleepThread(2);
			return 0;
		}
		else
			return -1;
	}
	return (s32)r;
}

bool SocketRecevExact(int socket, void* buffer, u32 size)
{
	u32 received = 0;
	while (received < size && IsThreadRunning)
	{
		int res = SocketRecv(socket, (char*)buffer + received, size - received);
		if (res < 0)
			return false;

		received += res;
	}

	return true;
}

bool SocketMakeNonBlocking(int socket)
{
	return bsdFcntl(socket, F_SETFL, NX_O_NONBLOCK) != -1;
}

int SocketNativeErrno()
{
	return g_bsdErrno;
}

bool SocketSetBroadcast(int socket, bool allow)
{
	int optVal = allow ? 1 : 0;
	return bsdSetSockOpt(socket, SOL_SOCKET, SO_BROADCAST, &optVal, sizeof(optVal)) != -1;
}

s32 SocketGetBroadcastAddress()
{
	if (HasNifm)
	{
		u32 addr, mask, gw, dns1, dns2;
		Result rc = nifmGetCurrentIpConfigInfo(&addr, &mask, &gw, &dns1, &dns2);

		if (R_SUCCEEDED(rc))
			return (s32)(addr | ~mask);
		else 
			LOG("Nifm failed with %x, fallback to INADDR_BROADCAST\n", rc);
	}

	// This does not seem to work on switch
	return INADDR_BROADCAST;
}

#endif
