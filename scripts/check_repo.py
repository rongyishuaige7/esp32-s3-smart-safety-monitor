#!/usr/bin/env python3
"""Validate public-repository contracts that do not require connected hardware."""
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REQUIRED = [
    ".github/platformio-requirements.in", ".github/platformio-requirements.txt", ".github/workflows/validate.yml",
    ".gitignore", ".markdownlint-cli2.jsonc", "CMakeLists.txt", "HARDWARE.md", "LICENSE", "README.md", "SECURITY.md",
    "THIRD_PARTY_NOTICES.md", "docs/GITHUB_METADATA.md", "docs/HARDWARE_LAB_CARD.md", "docs/PROJECT_STATUS.md",
    "docs/PROTOCOL.md", "docs/SOURCE_PROVENANCE.md", "docs/VERIFICATION.md", "hardware/BOM.csv", "hardware/wiring-diagram.svg",
    "main/CMakeLists.txt", "main/app_config.h", "main/local_config.example.h", "main/main.c", "pio_pre_embed_web.py", "tools/embed_web.py",
    "main/web/index.html", "main/web_server.c", "main/web_server.h", "platformio.ini", "scripts/check_repo.py",
    "scripts/secret_scan.py", "scripts/verify.sh", "tests/test_source_contracts.py",
]
FORBIDDEN_NAMES = {".env", "local_config.h", "sdkconfig", "sdkconfig.old", "web_index_embed.c", "id_rsa", "id_ed25519", "local.properties"}
FORBIDDEN_DIRS = {".git", ".pio", ".vscode", ".idea", "build", "dist", "managed_components", "__pycache__"}
FORBIDDEN_SUFFIXES = {".o", ".a", ".elf", ".bin", ".map", ".hex", ".pyc", ".apk", ".aab", ".pem", ".key", ".zip", ".7z", ".tar", ".gz"}


def files(root: Path) -> list[Path]:
    try:
        raw = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], check=True, capture_output=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        raw = b""
    if raw:
        return [root / item.decode("utf-8", "surrogateescape") for item in raw.split(b"\0") if item]
    return sorted(path for path in root.rglob("*") if path.is_file() and not any(part in FORBIDDEN_DIRS for part in path.relative_to(root).parts))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = Path(parser.parse_args().root).resolve()
    errors: list[str] = []
    for rel in REQUIRED:
        if not (root / rel).is_file():
            errors.append(f"missing required file: {rel}")
    checked = files(root)
    for path in checked:
        rel = path.relative_to(root)
        if path.name in FORBIDDEN_NAMES:
            errors.append(f"forbidden local/generated file: {rel}")
        if any(part in FORBIDDEN_DIRS for part in rel.parts):
            errors.append(f"forbidden local/generated directory: {rel}")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            errors.append(f"forbidden binary/archive/key artifact: {rel}")
        if path.stat().st_size > 5 * 1024 * 1024:
            errors.append(f"file exceeds 5 MiB: {rel}")

    contracts = {
        "platformio.ini": ["platform = espressif32@6.13.0", "board = esp32-s3-devkitc-1", "framework = espidf"],
        "main/local_config.example.h": ["#define SAFETY_MONITOR_AP_SSID \"\"", "#define SAFETY_MONITOR_AP_PASSWORD \"\""],
        "main/web_server.c": [
            '#if __has_include("local_config.h")', '#include "local_config.example.h"',
            "web_server_has_local_network_config", "credentials suppressed", "local HTTP status page is disabled",
        ],
        "main/CMakeLists.txt": ["${CMAKE_SOURCE_DIR}/.pio/generated/web_index_embed.c"],
        "pio_pre_embed_web.py": ["web_index_embed.c"],
        "main/web/index.html": ["L1 &ge;800", "L2 &ge;1500", "L3 &ge;2200", "RAW &ge;2000", "静态预览：未连接设备"],
        "README.md": ["不是火灾报警器", "无认证、没有 TLS"],
        "docs/SOURCE_PROVENANCE.md": ["c0932ed4497638972aa6a46013bd93a98bfb827acf717ff98befa67e18beb1b9", "38 个源码文件与桌面原始树中对应的 38 个文件逐字节一致"],
        "HARDWARE.md": ["首次上电", "不是火灾报警器", "无认证、无 TLS"],
    }
    for rel, values in contracts.items():
        path = root / rel
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        for value in values:
            if value not in text:
                errors.append(f"fact contract missing in {rel}: {value}")

    source = (root / "main/web_server.c").read_text(encoding="utf-8") if (root / "main/web_server.c").is_file() else ""
    page = (root / "main/web/index.html").read_text(encoding="utf-8") if (root / "main/web/index.html").is_file() else ""
    ignored = (root / ".gitignore").read_text(encoding="utf-8") if (root / ".gitignore").is_file() else ""
    for forbidden in ['#define WIFI_PASSWORD', '#define WIFI_SSID', '"SafetyMonitor"', '"12345678"', 'PASS=%s', 'SSID=%s']:
        if forbidden in source:
            errors.append(f"public web server contains forbidden fixed credential/log behavior: {forbidden}")
    for forbidden in ["startDemo", "Math.random", "192.168.4.1", "SYSTEM ONLINE", "fonts.googleapis.com"]:
        if forbidden in page:
            errors.append(f"public status page contains unsupported generated/fake status behavior: {forbidden}")
    if "main/local_config.h" not in ignored:
        errors.append(".gitignore must protect main/local_config.h")
    if (root / "main/local_config.h").exists():
        errors.append("main/local_config.h must not exist in public candidate")
    if (root / "main/web_index_embed.c").exists():
        errors.append("main/web_index_embed.c must be generated only in build directory")

    for rel in ["README.md", "docs/PROJECT_STATUS.md", "docs/HARDWARE_LAB_CARD.md"]:
        path = root / rel
        text = path.read_text(encoding="utf-8").lower() if path.is_file() else ""
        for claim in ["current hardware verified", "production ready", "fire alarm verified", "gas alarm verified", "automatic fire suppression verified"]:
            if claim in text:
                errors.append(f"unsupported claim in {rel}: {claim}")
    try:
        ET.parse(root / "hardware/wiring-diagram.svg")
    except (ET.ParseError, OSError) as exc:
        errors.append(f"invalid wiring SVG: {exc}")
    try:
        rows = list(csv.DictReader((root / "hardware/BOM.csv").open(newline="", encoding="utf-8")))
        if len(rows) < 12:
            errors.append("BOM must contain at least 12 component rows")
    except (OSError, csv.Error) as exc:
        errors.append(f"invalid BOM.csv: {exc}")
    if errors:
        print("Repository check: FAIL", file=sys.stderr)
        for item in sorted(set(errors)):
            print(f"- {item}", file=sys.stderr)
        return 1
    print(f"Repository check: PASS ({len(checked)} files checked)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
