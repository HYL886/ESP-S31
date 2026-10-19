"""Generate the bitmap fonts used by this project (requires lv_font_conv 1.5.3)."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
UI_FILES = ("main.c", "wifi_setup.c", "wifi_credentials.c")


def text_characters(filenames):
    result = set()
    for filename in filenames:
        source = (ROOT / "main" / filename).read_text(encoding="utf-8")
        for literal in re.findall(r'"(?:\\.|[^"\\])*"', source):
            result.update(c for c in literal if ord(c) >= 128)
    return result


def gb2312_hanzi():
    result = set()
    for high in range(0xB0, 0xF8):
        for low in range(0xA1, 0xFF):
            try:
                char = bytes((high, low)).decode("gb2312")
            except UnicodeDecodeError:
                continue
            if "\u4e00" <= char <= "\u9fff":
                result.add(char)
    assert len(result) == 6763
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", required=True, type=Path, help="SourceHanSansSC-Normal.otf")
    parser.add_argument("--converter", required=True, type=Path, help="lv_font_conv.js")
    parser.add_argument("--node", default="node")
    args = parser.parse_args()
    output = ROOT / "main" / "fonts"
    output.mkdir(exist_ok=True)
    ui = text_characters(UI_FILES)
    specifications = ((16, 4, ui, True),
                      (24, 2, gb2312_hanzi() | ui, False),
                      (32, 4, text_characters(("main.c",)), False))
    for size, bpp, characters, ascii_range in specifications:
        name = f"s31_cn_{size}"
        target = output / f"{name}.c"
        command = [args.node, str(args.converter.resolve()), "--font", str(args.font.resolve()),
                   "--symbols", "".join(sorted(characters)), "--size", str(size),
                   "--bpp", str(bpp), "--no-compress", "--no-kerning", "--format", "lvgl",
                   "--lv-include", "lvgl.h", "--lv-font-name", name, "-o", str(target)]
        if ascii_range:
            command.extend(("--range", "0x20-0x7e"))
        subprocess.run(command, check=True)
        source = target.read_text(encoding="utf-8")
        source = re.sub(r" \* Opts:.*", " * Source: SourceHanSansSC-Normal.otf; see OFL-SourceHanSans.txt.\n"
                        " * Reproduce with tools/generate_fonts.py (lv_font_conv 1.5.3).", source)
        target.write_bytes((source.rstrip() + "\n").encode("utf-8"))
        print(f"{target.name}: {len(characters)} non-ASCII characters; {target.stat().st_size} bytes")


if __name__ == "__main__":
    main()
