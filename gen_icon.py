# -*- coding: utf-8 -*-
# 生成 appicon.ico（多尺寸：256/48/32/16），可爱风：粉嫩渐变圆角方块 + 微笑表情
# 不依赖 PIL，纯标准库实现（PNG 内嵌于 ICO，Win7+ 原生支持）
import struct, zlib, math

def lerp(a, b, t):
    return a + (b - a) * t

def set_px(buf, size, x, y, color, alpha=255):
    if 0 <= x < size and 0 <= y < size:
        idx = (y * size + x) * 4
        # 简单 alpha 混合（底色不透明时直接覆盖）
        buf[idx] = color[0]; buf[idx + 1] = color[1]
        buf[idx + 2] = color[2]; buf[idx + 3] = alpha

def fill_circle(buf, size, nx, ny, nr, color, alpha=255):
    r2 = nr * nr
    for y in range(size):
        for x in range(size):
            dx = x / size - nx; dy = y / size - ny
            if dx * dx + dy * dy <= r2:
                set_px(buf, size, x, y, color, alpha)

def draw_smile(buf, size, cx, cy, r, thick, color):
    # 微笑 = 下半圆弧（U 形）
    for x in range(size):
        dx = x / size - cx
        if abs(dx) > r:
            continue
        dy = math.sqrt(max(0.0, r * r - dx * dx))
        yc = int((cy + dy) * size)
        t = int(thick * size)
        for ty in range(yc - t, yc + t + 1):
            set_px(buf, size, x, ty, color)

def draw_icon(size):
    buf = bytearray(size * size * 4)
    inset = size * 0.10
    rad = size * 0.30
    # 粉嫩渐变：左上蜜桃 -> 右下粉红
    c1 = (0xFF, 0xE2, 0x9E)
    c2 = (0xFF, 0xB3, 0xC8)
    denom = (size - 2 * inset)
    for y in range(size):
        for x in range(size):
            if x < inset or x >= size - inset or y < inset or y >= size - inset:
                continue
            cx = min(max(x, inset + rad), size - inset - rad)
            cyy = min(max(y, inset + rad), size - inset - rad)
            dx = x - cx; dy = y - cyy
            if dx * dx + dy * dy > rad * rad:
                continue
            t = ((x - inset) / denom + (y - inset) / denom) / 2.0
            R = int(lerp(c1[0], c2[0], t))
            G = int(lerp(c1[1], c2[1], t))
            B = int(lerp(c1[2], c2[2], t))
            idx = (y * size + x) * 4
            buf[idx] = R; buf[idx + 1] = G; buf[idx + 2] = B; buf[idx + 3] = 255
    # 腮红
    fill_circle(buf, size, 0.30, 0.55, 0.055, (0xFF, 0x8F, 0xB0))
    fill_circle(buf, size, 0.70, 0.55, 0.055, (0xFF, 0x8F, 0xB0))
    # 眼睛（深棕，带高光更萌）
    fill_circle(buf, size, 0.38, 0.42, 0.05, (0x5A, 0x40, 0x40))
    fill_circle(buf, size, 0.62, 0.42, 0.05, (0x5A, 0x40, 0x40))
    fill_circle(buf, size, 0.40, 0.40, 0.018, (0xFF, 0xFF, 0xFF))
    fill_circle(buf, size, 0.64, 0.40, 0.018, (0xFF, 0xFF, 0xFF))
    # 微笑
    draw_smile(buf, size, cx=0.5, cy=0.47, r=0.11, thick=0.022, color=(0x5A, 0x40, 0x40))
    return bytes(buf)

def make_png(width, height, pixels):
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(pixels[y * stride:(y + 1) * stride])
    comp = zlib.compress(bytes(raw), 9)
    def chunk(typ, data):
        return struct.pack(">I", len(data)) + typ + data + struct.pack(">I", zlib.crc32(typ + data) & 0xffffffff)
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", comp) + chunk(b"IEND", b"")

def build_ico(sizes):
    pngs = [make_png(s, s, draw_icon(s)) for s in sizes]
    out = bytearray()
    out += struct.pack("<HHH", 0, 1, len(pngs))
    offset = 6 + 16 * len(pngs)
    for s, png in zip(sizes, pngs):
        bw = s if s < 256 else 0
        out += struct.pack("<BBBBHHII", bw, bw, 0, 0, 1, 32, len(png), offset)
        offset += len(png)
    for png in pngs:
        out += png
    return bytes(out)

if __name__ == "__main__":
    data = build_ico([256, 48, 32, 16])
    with open("appicon.ico", "wb") as f:
        f.write(data)
    print("appicon.ico 生成成功，大小 = %d 字节" % len(data))
