#!/usr/bin/env python3
"""将本地 HTML 状态页生成 C 数组；生成文件只写入构建目录。"""
from __future__ import annotations

import pathlib
import sys


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: embed_web.py INPUT.html OUTPUT.c")
    source = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])
    data = source.read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = ["#include <stdint.h>", "", "const uint8_t index_html_start[] = {"]
    for index in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[index:index + 16]) + ",")
    lines.extend(["};", "", f"const uint32_t index_html_len = {len(data)}u;", ""])
    output.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
