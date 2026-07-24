from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "work" / "assets" / "switchcast-controller-mark.png"
ROMFS = ROOT / "SwitchCastConfig" / "romfs"


def cropped_mark() -> Image.Image:
    source = Image.open(SOURCE).convert("RGBA")
    alpha = source.getchannel("A")
    bounds = alpha.getbbox()
    if bounds is None:
        raise RuntimeError("approved SwitchCast mark is fully transparent")
    return source.crop(bounds)


def fit(mark: Image.Image, size: tuple[int, int]) -> Image.Image:
    output = mark.copy()
    output.thumbnail(size, Image.Resampling.LANCZOS)
    return output


def centered_canvas(
    mark: Image.Image,
    size: tuple[int, int],
    mark_size: tuple[int, int],
    background: tuple[int, int, int, int],
) -> Image.Image:
    canvas = Image.new("RGBA", size, background)
    fitted = fit(mark, mark_size)
    position = (
        (size[0] - fitted.width) // 2,
        (size[1] - fitted.height) // 2,
    )
    canvas.alpha_composite(fitted, position)
    return canvas


def main() -> None:
    mark = cropped_mark()

    header = centered_canvas(
        mark,
        (360, 160),
        (340, 145),
        (0, 0, 0, 0),
    )
    header.save(ROMFS / "logo.png", optimize=True)

    menu_mark = centered_canvas(
        mark,
        (190, 88),
        (180, 80),
        (0, 0, 0, 0),
    )
    menu_mark.save(ROMFS / "SwitchCast.png", optimize=True)

    icon = centered_canvas(
        mark,
        (256, 256),
        (226, 176),
        (7, 12, 43, 255),
    ).convert("RGB")
    icon.save(
        ROOT / "SwitchCastConfig" / "icon.jpg",
        quality=96,
        subsampling=0,
        optimize=True,
    )

    preview = centered_canvas(
        mark,
        (1024, 1024),
        (900, 700),
        (7, 12, 43, 255),
    )
    preview.save(
        ROOT / "work" / "assets" / "switchcast-controller-mark-preview.png",
        optimize=True,
    )


if __name__ == "__main__":
    main()
