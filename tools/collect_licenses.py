"""Collect dependency notices from official installed SDK/component sources."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import urllib.request

ROOT = Path(__file__).resolve().parent.parent

def collect(idf, managed):
    out = ROOT / "LICENSES"
    pattern = re.compile(r"^(licen[sc]e|copying|notice)(\..*)?$", re.I)
    for name, source in (("esp-idf", Path(idf)), ("components", Path(managed))):
        if not source.is_dir():
            raise ValueError("Missing official dependency sources: " + name)
        for path in source.rglob("*"):
            if path.is_file() and pattern.match(path.name):
                target = out / name / path.relative_to(source)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
    urls = {
        "PyPlumIO-MIT.txt": "https://raw.githubusercontent.com/denpamusic/PyPlumIO/033c157810bdfa1aa479a361cf809d12c4b532a9/LICENSE",
        "Lucide-ISC.txt": "https://raw.githubusercontent.com/lucide-icons/lucide/0.468.0/LICENSE",
        "Feather-MIT.txt": "https://raw.githubusercontent.com/feathericons/feather/v4.29.2/LICENSE",
    }
    for name, url in urls.items():
        with urllib.request.urlopen(url, timeout=30) as response:
            payload = response.read()
        if len(payload) < 200 or len(payload) > 100000:
            raise ValueError("Unexpected license payload")
        out.mkdir(exist_ok=True)
        (out / name).write_bytes(payload)
    manifest = {p.relative_to(out).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(out.rglob("*")) if p.is_file() and p.name != "manifest.json"}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="ascii")
    print("Collected " + str(len(manifest)) + " dependency license files")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--idf", required=True)
    parser.add_argument("--managed", required=True)
    args = parser.parse_args()
    collect(args.idf, args.managed)
