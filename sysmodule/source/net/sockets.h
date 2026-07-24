#pragma once
#include <switch.h>
#include <arpa/inet.h> // htons
#include <switch/services/bsd.h>

#define SOCKET_INVALID -1

void SocketInit();

void SocketDeinit();

int SocketUdp();

int SocketTcpListen(short port);

int SocketTcpConnect(u32 ipv4Address, short port);

typedef enum {
	// Accept succeeded, guaranteed to have a valid socket in outAccepted
	SocketAcceptError_OK,
	// Accept failed, the console was put into sleep mode and the listener needs to be reset
	SocketAcceptError_NetDown,
	// Accept failed, either no pending connections or an error occured, try again
	SocketAcceptError_Fail,
} SocketAcceptResult;

SocketAcceptResult SocketTcpAccept(int listenerHandle, int* outAccepted, struct sockaddr* addr, socklen_t* addrlen);

// Coses a socket, also sets it to SOCKET_INVALID
void SocketClose(int* socket);

// Returns true on success
bool SocketSendAll(int socket, const void* buffer, u32 size);

// Returns true on success
bool SocketUDPSendTo(int socket, const void* data, u32 size, struct sockaddr* addr, socklen_t addrlen);

// Returns 0 when no datagram is pending, -1 on error, or the received size.
s32 SocketUDPRecvFrom(int socket, void* data, u32 size, struct sockaddr* addr, socklen_t* addrlen);

bool SocketBind(int socket, struct sockaddr* addr, socklen_t addrlen);

bool SocketSetReuseAddress(int socket, bool allow);

bool SocketJoinMulticast(int socket, u32 multicastAddress);

typedef enum {
	SocketWaitResult_Timeout,
	SocketWaitResult_Readable,
	SocketWaitResult_Closed,
	SocketWaitResult_Error
} SocketWaitResult;

// Reports whether the socket became readable, timed out, closed, or failed.
SocketWaitResult SocketWaitReadableEx(int socket, int timeoutMs);

// Returns true when the socket became readable before timeoutMs.
bool SocketWaitReadable(int socket, int timeoutMs);

// Returns true on success
bool SocketSetBroadcast(int socket, bool allow);

// Accounts for the network mask
s32 SocketGetBroadcastAddress();

// Returns 0 on timeout, -1 on error, otherwise the number of bytes received
s32 SocketRecv(int socket, void* buffer, u32 size);

bool SocketRecevExact(int socket, void* buffer, u32 size);

bool SocketMakeNonBlocking(int socket);

int SocketNativeErrno();
