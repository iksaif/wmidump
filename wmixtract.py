#!/usr/bin/env python3
"""Extract _WDG and WQXX buffers from decompiled ACPI DSDT .dsl files."""

import re
import sys
from pathlib import Path

file_counts: dict[str, int] = {}


def dump_file(buf: dict[str, str | list[str]]) -> None:
    name = str(buf["name"])

    if name not in file_counts:
        file_counts[name] = 0
    else:
        file_counts[name] += 1
        name = f"{name}{file_counts[name]}"

    print(f"Writing {buf['name']} to {name} and {name}.bin")

    data = buf["data"]
    assert isinstance(data, list)

    Path(name).write_text(",".join(data))

    with Path(f"{name}.bin").open("wb") as fp:
        for byte in data:
            byte = byte.strip()
            if byte:
                fp.write(bytes([int(byte, 16)]))


def find_buffer(name: str, data: str):
    pattern = (
        r"Name\s*\((%s),\s*Buffer\s*\(((0x)?[\dA-Fa-f]+)\)\s*\{(.*?)\}\s*\)"
        % name
    )

    for group in re.findall(pattern, data, re.DOTALL):
        buf: dict[str, str | list[str]] = {
            "name": group[0],
            "size": group[1],
        }

        cleaned = re.sub(r"\s*/\*.*?\*/\s*", "", group[3])
        buf["data"] = re.split(r"\s*,\s*|\s+", cleaned)

        yield buf


def main() -> int:
    if len(sys.argv) >= 2:
        if sys.argv[1] == "--help":
            print(f"Usage: {sys.argv[0]} [DSDT.dsl]")
            return 0
        source = Path(sys.argv[1]).read_text()
    else:
        source = sys.stdin.read()

    for buf in find_buffer("_WDG", source):
        dump_file(buf)
    for buf in find_buffer(r"WQ[A-Z]{2}", source):
        dump_file(buf)

    return 0


if __name__ == "__main__":
    sys.exit(main())
