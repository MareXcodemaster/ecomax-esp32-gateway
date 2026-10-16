"""Create public images from build outputs, never from a device flash dump."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

from audit_release import ROOT, audit

TOP_FILES = {"README.md", "LICENSE", "NOTICE", "THIRD_PARTY.md", "SECURITY.md", "CHANGELOG.md",
             "platformio.ini", "CMakeLists.txt", "partitions.csv", "sdkconfig.defaults",
             "dependencies.lock", "embed_web.py", "Build.ps1", "requirements.txt",
             "requirements-build.txt", ".gitignore", "package.json", "pnpm-lock.yaml"}
SOURCE_DIRS = {"src", "profiles", "tests", "tools", "docs", "LICENSES", ".github", "installer"}

def source_paths():
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file():
            continue
        parts = path.relative_to(ROOT).parts
        if any(part in {"__pycache__", ".git", ".pio", "managed_components", ".venv", "artifacts", "dist", "node_modules", "site", "test-results"}
               for part in parts):
            continue
        if len(parts) == 1 and parts[0] in TOP_FILES or parts[0] in SOURCE_DIRS:
            yield path

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / ".pio/build/waveshare_s3")
    parser.add_argument("--private-baseline", type=Path)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    version = re.search(r'set\(PROJECT_VER "([^"]+)"\)', (ROOT / "CMakeLists.txt").read_text())[1]
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.]+)?", version):
        raise ValueError("Invalid release version")
    app = (build / "firmware.bin").read_bytes()
    if app[32:36] != bytes.fromhex("3254cdab") or app[48:80].split(b"\0", 1)[0].decode() != version:
        raise ValueError("Build version mismatch; do a clean build")
    if not 1024 < len(app) < 0x300000:
        raise ValueError("Application does not fit the OTA slot")
    configuration = json.loads((build / "flasher_args.json").read_text())
    if configuration["flash_settings"]["flash_size"] != "16MB":
        raise ValueError("Wrong flash size")
    offsets = {"bootloader.bin": "0x0", "partitions.bin": "0x8000", "ota_data_initial.bin": "0xf000"}
    if set(configuration["flash_files"]) != {"0x0", "0x8000", "0xf000", "0x20000"}:
        raise ValueError("Unexpected first-install flash segments")
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    shutil.copyfile(build / "firmware.bin", dist / "firmware-ota.bin")
    segments = []
    for name, offset in offsets.items():
        shutil.copyfile(build / name, dist / name)
        segments += [offset, str(dist / name)]
    segments += ["0x20000", str(dist / "firmware-ota.bin")]
    subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin",
                    "-o", str(dist / "firmware-merged.bin"), "--flash_mode", "dio",
                    "--flash_freq", "80m", "--flash_size", "16MB", *segments], check=True)
    merged = (dist / "firmware-merged.bin").read_bytes()
    if merged[0x9000:0xf000] != b"\xff" * 0x6000:
        raise ValueError("Merged NVS must be blank; refusing release")
    files = {name: {"size": (dist / name).stat().st_size,
                    "sha256": hashlib.sha256((dist / name).read_bytes()).hexdigest()}
             for name in [*offsets, "firmware-ota.bin", "firmware-merged.bin"]}
    manifest = {"version": version, "chip": "esp32s3", "flash_mb": 16, "psram_mb": 8,
                "first_install_address": "0x0", "first_install_image": "firmware-merged.bin",
                "ota_image": "firmware-ota.bin", "nvs_blank": True,
                "hardware_first_boot_verified": False, "files": files}
    (dist / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="ascii")
    (dist / "SHA256SUMS").write_text("".join(v["sha256"] + "  " + k + "\n" for k, v in files.items()), encoding="ascii")
    deny = []
    if args.private_baseline:
        if args.private_baseline.resolve().is_relative_to(ROOT):
            raise ValueError("Private baseline cannot be inside public source")
        deny = json.loads(args.private_baseline.read_text(encoding="utf-8"))
    failures = audit(deny)
    if failures:
        raise ValueError("Privacy gate failed: " + repr(failures))
    artifacts = ROOT / "artifacts"
    artifacts.mkdir(exist_ok=True)
    output = artifacts / ("ecomax-esp32-gateway-" + version + ".zip")
    prefix = "ecomax-esp32-gateway-" + version + "/"
    paths = [*source_paths(), *(dist / name for name in [*files, "manifest.json", "SHA256SUMS"])]
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in paths:
            relative = path.relative_to(ROOT).as_posix()
            info = zipfile.ZipInfo(prefix + relative, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None or len(archive.infolist()) != len(paths):
            raise ValueError("ZIP verification failed")
        for info in archive.infolist():
            if not info.filename.startswith(prefix) or ".." in Path(info.filename).parts:
                raise ValueError("Unsafe archive path")
    print("PUBLIC RELEASE: " + output.name + "; files=" + str(len(paths)))
    print("Merged SHA256: " + files["firmware-merged.bin"]["sha256"])
    print("OTA SHA256: " + files["firmware-ota.bin"]["sha256"])
    return 0

if __name__ == "__main__":
    sys.exit(main())
