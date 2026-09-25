from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / ".downloads" / "animalpackss.png"
OUTPUT = ROOT / "src" / "game" / "KidsAnimalSpritesGenerated.inc"

# Cat, duck, rabbit, crocodile, pig and penguin from the CC0 sheet.
X_RANGES = [(198, 254), (370, 431), (713, 775),
            (806, 864), (1255, 1315), (1514, 1570)]
WIDTH, HEIGHT = 36, 44
TRANSPARENT = 0x0001


def rgb565(rgb):
    r, g, b = rgb
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


image = Image.open(SOURCE).convert("RGB")
sprites = []
for left, right in X_RANGES:
    crop = image.crop((left, 0, right + 1, image.height))
    bounds = crop.getbbox()
    # getbbox sees the white background; derive a non-white content box.
    pixels = crop.load()
    points = [(x, y) for y in range(crop.height) for x in range(crop.width)
              if pixels[x, y] != (255, 255, 255)]
    min_x = min(x for x, _ in points)
    max_x = max(x for x, _ in points)
    min_y = min(y for _, y in points)
    max_y = max(y for _, y in points)
    crop = crop.crop((min_x, min_y, max_x + 1, max_y + 1))
    scale = min(WIDTH / crop.width, HEIGHT / crop.height)
    size = (max(1, round(crop.width * scale)), max(1, round(crop.height * scale)))
    crop = crop.resize(size, Image.Resampling.NEAREST)
    canvas = Image.new("RGB", (WIDTH, HEIGHT), "white")
    canvas.paste(crop, ((WIDTH - size[0]) // 2, (HEIGHT - size[1]) // 2))
    sprites.append([
        TRANSPARENT if pixel == (255, 255, 255) else rgb565(pixel)
        for pixel in canvas.getdata()
    ])

with OUTPUT.open("w", encoding="utf-8", newline="\n") as stream:
    stream.write("// Generated from gameplayarts' CC0 Animal sprites pack.\n")
    stream.write("// Source: https://opengameart.org/content/animal-sprites-1\n")
    stream.write(f"constexpr uint8_t kKidsAnimalSpriteWidth = {WIDTH};\n")
    stream.write(f"constexpr uint8_t kKidsAnimalSpriteHeight = {HEIGHT};\n")
    stream.write(f"constexpr uint16_t kKidsAnimalTransparent = 0x{TRANSPARENT:04X};\n")
    stream.write(f"constexpr uint16_t kKidsAnimalSprites[{len(sprites)}][{WIDTH * HEIGHT}] = {{\n")
    for sprite in sprites:
        stream.write("  {\n")
        for row in range(HEIGHT):
            values = sprite[row * WIDTH:(row + 1) * WIDTH]
            stream.write("    " + ",".join(f"0x{value:04X}" for value in values) + ",\n")
        stream.write("  },\n")
    stream.write("};\n")

print(f"Generated {len(sprites)} kids animal sprites ({WIDTH}x{HEIGHT})")
