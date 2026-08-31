"""Build the single browser-ready image used for each homepage artwork."""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
SOURCE = ASSETS / "homepage-concept"
RUNTIME = ASSETS / "homepage-runtime"

# One runtime derivative per live image. The review/source PNGs remain untouched.
WEBP_ASSETS = (
    ("hero-living-observatory.png", "hero-living-observatory.webp", (1228, 941), 90, False),
    ("hero-observation-animal-identity.png", "hero-observation-animal-identity.webp", (720, 240), 100, True),
    ("hero-observation-colony-state.png", "hero-observation-colony-state.webp", (720, 240), 100, True),
    ("hero-observation-molecular-logic.png", "hero-observation-molecular-logic.webp", (720, 240), 100, True),
    ("principle-name-leaf-transparent.png", "principle-name-leaf.webp", (896, 896), 90, False),
    ("principle-observe-binoculars-transparent.png", "principle-observe-binoculars.webp", (512, 512), 90, False),
    ("principle-understand-magnifier-transparent.png", "principle-understand-magnifier.webp", (512, 512), 88, False),
    ("project-fauna-bear-panel.png", "project-fauna-bear-panel.webp", (1122, 1402), 88, False),
    ("project-molecular-protein-panel.png", "project-molecular-protein-panel.webp", (1000, 1250), 88, False),
    ("project-cellular-cow-panel.png", "project-cellular-cow-panel.webp", (1122, 1402), 88, False),
    ("evidence-band-overlay.png", "evidence-band-overlay.webp", (1672, 941), 84, False),
    ("opensource-seed-v2-transparent.png", "opensource-seed-v2.webp", (896, 896), 88, False),
)


def image_mode(image: Image.Image) -> str:
    return "RGBA" if "A" in image.getbands() else "RGB"


def resize(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    if image.size == size:
        return image
    if image.mode == "RGBA":
        return image.convert("RGBa").resize(size, Image.Resampling.LANCZOS).convert("RGBA")
    return image.resize(size, Image.Resampling.LANCZOS)


def build_webp(
    source_name: str,
    target_name: str,
    size: tuple[int, int],
    quality: int,
    lossless: bool,
) -> None:
    source_path = SOURCE / source_name
    target_path = RUNTIME / target_name

    with Image.open(source_path) as source:
        image = source.convert(image_mode(source))
        image = resize(image, size)
        image.save(
            target_path,
            "WEBP",
            quality=quality,
            method=6,
            lossless=lossless,
            alpha_quality=100,
            exact=True,
        )

    print(f"{target_name:<48} {target_path.stat().st_size:>10,} bytes  {size[0]}x{size[1]}")


def build_favicon() -> None:
    source_path = SOURCE / "favicon-stone-n-source.png"
    target_path = ASSETS / "favicon.png"

    with Image.open(source_path) as source:
        image = resize(source.convert("RGBA"), (256, 256))
        image.save(target_path, "PNG", optimize=True, compress_level=9)

    print(f"{'favicon.png':<48} {target_path.stat().st_size:>10,} bytes  256x256")


def main() -> None:
    RUNTIME.mkdir(parents=True, exist_ok=True)
    for source_name, target_name, size, quality, lossless in WEBP_ASSETS:
        build_webp(source_name, target_name, size, quality, lossless)
    build_favicon()


if __name__ == "__main__":
    main()
