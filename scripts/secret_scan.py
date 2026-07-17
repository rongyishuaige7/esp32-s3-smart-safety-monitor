#!/usr/bin/env python3
"""Fail publication checks on likely secrets, local configuration and generated output."""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

SKIP_DIRS = {".git", ".pio", "build", "managed_components", ".vscode", ".idea", "__pycache__"}
FORBIDDEN_NAMES = {".env", "local_config.h", "sdkconfig", "sdkconfig.old", "id_rsa", "id_ed25519", "local.properties"}
FORBIDDEN_SUFFIXES = {".bin", ".elf", ".map", ".o", ".a", ".zip", ".7z", ".tar", ".gz", ".pem", ".key", ".apk", ".aab"}
TEXT_SUFFIXES = {"", ".c", ".h", ".md", ".py", ".csv", ".svg", ".sh", ".yml", ".yaml", ".json", ".jsonc", ".ini", ".defaults", ".txt"}
PATTERNS = [
    ("private key", re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----")),
    ("GitHub token", re.compile(r"\b(?:gh[opusr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b")),
    ("OpenAI-style key", re.compile(r"\bsk-[A-Za-z0-9_-]{16,}\b")),
    ("private LAN literal", re.compile(r"(?<![\d.])(?:10\.|192\.168\.|172\.(?:1[6-9]|2\d|3[01])\.)\d{1,3}\.\d{1,3}(?![\d.])")),
    ("absolute home path", re.compile(r"(?<![\w.-])/home/(?!rongyi/桌面/(?:智能安全监测系统|esp32-s3-smart-safety-monitor)\b)[^\s`\"']+")),
    ("fixed public AP password", re.compile(r"(?:WIFI_PASSWORD|AP_PASSWORD|SAFETY_MONITOR_AP_PASSWORD)\s+\"(?!\")[^\"]{8,}\"")),
    ("fixed public AP SSID", re.compile(r"(?:WIFI_SSID|AP_SSID|SAFETY_MONITOR_AP_SSID)\s+\"(?!\")[^\"]{1,}\"")),
]
ALLOWED_PATH_FILES = {"docs/SOURCE_PROVENANCE.md", "scripts/secret_scan.py"}


def tracked_files(root: Path) -> list[Path]:
    try:
        raw = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], check=True, capture_output=True).stdout
    except (FileNotFoundError, subprocess.CalledProcessError):
        raw = b""
    if raw:
        return [root / item.decode("utf-8", "surrogateescape") for item in raw.split(b"\0") if item]
    return sorted(path for path in root.rglob("*") if path.is_file() and not any(part in SKIP_DIRS for part in path.relative_to(root).parts))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = Path(parser.parse_args().root).resolve()
    errors: list[str] = []
    for path in tracked_files(root):
        rel = path.relative_to(root)
        if path.name in FORBIDDEN_NAMES:
            errors.append(f"{rel}: forbidden local/config file")
        if any(part in SKIP_DIRS for part in rel.parts):
            errors.append(f"{rel}: forbidden generated/editor directory")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            errors.append(f"{rel}: forbidden binary/archive/key artifact")
        if path.stat().st_size > 5 * 1024 * 1024:
            errors.append(f"{rel}: file exceeds 5 MiB")
        if path.suffix.lower() not in TEXT_SUFFIXES or path.stat().st_size > 2_000_000:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        for number, line in enumerate(text.splitlines(), 1):
            for label, pattern in PATTERNS:
                if label == "absolute home path" and rel.as_posix() in ALLOWED_PATH_FILES:
                    continue
                if rel.as_posix() in {"scripts/secret_scan.py", "scripts/check_repo.py", "tests/test_source_contracts.py"}:
                    continue
                if pattern.search(line):
                    errors.append(f"{rel}:{number}: {label}")
    if errors:
        print("Secret scan: FAIL", file=sys.stderr)
        print("\n".join(sorted(set(errors))), file=sys.stderr)
        return 1
    print("Secret scan: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
