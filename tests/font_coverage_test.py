"""Check actual LVGL font assets against the screen text and GB2312 repertoire."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent


class FontAsset:
    def __init__(self, path):
        source = Path(path).read_text(encoding="utf-8")
        self.bpp = int(re.search(r"\.bpp\s*=\s*(\d+)", source)[1])
        self.line_height = int(re.search(r"\.line_height\s*=\s*(\d+)", source)[1])
        self.base_line = int(re.search(r"\.base_line\s*=\s*(\d+)", source)[1])
        arrays = dict(re.findall(r"(?:uint8_t|uint16_t)\s+(\w+)\[\]\s*=\s*\{(.*?)\};", source, re.S))

        def numbers(name):
            text = re.sub(r"/\*.*?\*/", "", arrays[name], flags=re.S)
            return [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", text)]

        self.bitmap = bytes(numbers("glyph_bitmap"))
        descriptors = re.search(r"glyph_dsc\[\]\s*=\s*\{(.*?)\};", source, re.S)[1]
        self.glyphs = [dict((key, int(value)) for key, value in
                      re.findall(r"\.(\w+)\s*=\s*(-?\d+)", block))
                      for block in re.findall(r"\{([^{}]*)\}", descriptors)]
        cmaps = re.search(r"cmaps\[\]\s*=\s*\{(.*?)\};", source, re.S)[1]
        self.mapping = {}
        for block in re.findall(r"\{([^{}]*)\}", cmaps):
            fields = dict(re.findall(r"\.(\w+)\s*=\s*(\w+)", block))
            start = int(fields["range_start"], 0)
            glyph_start = int(fields["glyph_id_start"], 0)
            kind = fields["type"]
            if "SPARSE" in kind:
                offsets = numbers(fields["unicode_list"])
            else:
                offsets = range(int(fields["range_length"], 0))
            ids = None if fields["glyph_id_ofs_list"] == "NULL" else numbers(fields["glyph_id_ofs_list"])
            for index, offset in enumerate(offsets):
                self.mapping[start + offset] = glyph_start + (index if ids is None else ids[index])

    def require(self, characters):
        missing = set()
        for char in characters:
            glyph_id = self.mapping.get(ord(char), 0)
            if glyph_id == 0 or glyph_id >= len(self.glyphs):
                missing.add(char)
                continue
            glyph = self.glyphs[glyph_id]
            assert glyph["adv_w"] > 0, f"Empty advance for {char!r}"
            byte_count = (glyph["box_w"] * glyph["box_h"] * self.bpp + 7) // 8
            assert glyph["bitmap_index"] + byte_count <= len(self.bitmap), f"Bitmap out of bounds for {char!r}"
            if ord(char) >= 0x4E00 and ord(char) <= 0x9FFF:
                assert glyph["box_w"] > 0 and glyph["box_h"] > 0, f"Empty Han glyph: {char}"
        assert not missing, "Missing glyphs: " + "".join(sorted(missing))


def main():
    small = FontAsset(ROOT / "main/fonts/s31_cn_16.c")
    normal = FontAsset(ROOT / "main/fonts/s31_cn_24.c")
    large = FontAsset(ROOT / "main/fonts/s31_cn_32.c")
    ui_characters = set()
    main_characters = set()
    for name in ("main.c", "wifi_setup.c", "wifi_credentials.c", "camera_preview.c"):
        source = (ROOT / "main" / name).read_text(encoding="utf-8")
        for literal in re.findall(r'"(?:\\.|[^"\\])*"', source):
            characters = {c for c in literal if ord(c) >= 128}
            ui_characters.update(characters)
            if name == "main.c":
                main_characters.update(characters)
    small.require(ui_characters | set(chr(i) for i in range(32, 127)))
    normal.require(ui_characters)
    large.require(main_characters)

    hanzi = set()
    for code in range(0xB0A1, 0xF7FF):
        try:
            char = code.to_bytes(2, "big").decode("gb2312")
        except UnicodeDecodeError:
            continue
        if len(char) == 1 and 0x4E00 <= ord(char) <= 0x9FFF:
            hanzi.add(char)
    assert len(hanzi) == 6763
    normal.require(hanzi)
    assert normal.bpp == 2 and len(normal.bitmap) < 1024 * 1024
    print(f"Font coverage passed: {len(ui_characters)} UI characters and 6763 GB2312 Han characters.")


if __name__ == "__main__":
    main()
