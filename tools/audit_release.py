"""Privacy gate for source and generated release files; never prints matched secrets."""
import argparse
import base64
import gzip
import json
from pathlib import Path
import re
import sys
import zlib

ROOT = Path(__file__).resolve().parent.parent
IGNORED = {".git", ".pio", "managed_components", ".venv", "venv", "__pycache__", "artifacts", "node_modules", "test-results"}

def expansions(data):
    yield data
    # Firmware contains individually gzipped HTML/CSS/JS; inspecting raw strings is insufficient.
    start = 0
    while True:
        start = data.find(b"\x1f\x8b\x08", start)
        if start < 0:
            break
        try:
            decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
            expanded = decoder.decompress(data[start:], 5 * 1024 * 1024)
            if decoder.eof:
                yield expanded
        except zlib.error:
            pass
        start += 3
    # Static byte-array recordings and long hex fields can conceal readable secrets.
    for match in re.finditer(rb"\[\]\s*=\s*\{((?:\s|,|0x[0-9a-fA-F]{2})+)\}", data):
        tokens = re.findall(rb"0x([0-9a-fA-F]{2})", match[1])
        if tokens:
            yield bytes(int(token, 16) for token in tokens)

def source_files():
    return sorted(p for p in ROOT.rglob("*") if p.is_file() and not any(
        part in IGNORED for part in p.relative_to(ROOT).parts))

def audit(deny=()):
    failures = []
    blocked = []
    for value in deny:
        if not isinstance(value, str) or not value:
            continue
        blocked += [value.encode(), value.encode("utf-16le"), base64.b64encode(value.encode()),
                    base64.b64encode(("admin:" + value).encode())]
    for path in source_files():
        relative = path.relative_to(ROOT).as_posix()
        if path.suffix.lower() in {".elf", ".map", ".dmp", ".pcap", ".pcapng", ".log"}:
            failures.append((relative, "non-release diagnostic file"))
        data = path.read_bytes()
        for part in expansions(data):
            if any(value in part for value in blocked):
                failures.append((relative, "private baseline match"))
            if re.search(rb"[A-Za-z]:[/\\]Users[/\\][^/\\\x00\r\n]+", part):
                failures.append((relative, "personal Windows build path"))
    catalog = json.loads((ROOT / "src/panel_catalog.json").read_text(encoding="ascii"))
    if "uid" in catalog["sysParams"] or catalog["regParams"]["curr"] or catalog["regParamsData"]["data"]:
        failures.append(("src/panel_catalog.json", "identity or live readings"))
    if catalog["editParams"]["data"] or catalog["rmCurrentDataParamsEdits"]["data"]:
        failures.append(("src/panel_catalog.json", "live edit values"))
    if any(row.get("value") is not None for row in catalog["rmParamsData"]["data"]):
        failures.append(("src/panel_catalog.json", "stored setting value"))
    if re.search(r"os\.environ|os\.getenv", (ROOT / "embed_web.py").read_text()):
        failures.append(("embed_web.py", "environment credential provisioning"))
    if any((ROOT / "src" / name).exists() for name in ("sensor_fixtures.hpp", "protocol_fixtures.hpp")):
        failures.append(("src", "recorded fixtures must be generated synthetically"))
    return sorted(set(failures))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-baseline", type=Path,
                        help="PRIVATE JSON list of forbidden values outside this repository")
    args = parser.parse_args()
    deny = []
    if args.private_baseline:
        resolved = args.private_baseline.resolve()
        if resolved.is_relative_to(ROOT):
            raise ValueError("Private baseline must stay outside the public repository")
        deny = json.loads(resolved.read_text(encoding="utf-8"))
        if not isinstance(deny, list):
            raise ValueError("Private baseline must be a list")
    failures = audit(deny)
    for path, reason in failures:
        print("FAIL " + path + ": " + reason)
    if failures:
        return 1
    print("PASS privacy audit: " + str(len(source_files())) + " files; no matched secret values printed")
    return 0

if __name__ == "__main__":
    sys.exit(main())
