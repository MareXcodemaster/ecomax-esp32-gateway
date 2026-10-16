"""Install only a verified public merged image on the selected ESP32-S3."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent

def verified_image():
    manifest = json.loads((ROOT / "dist/manifest.json").read_text(encoding="utf-8"))
    if manifest.get("chip") != "esp32s3" or manifest.get("flash_mb") != 16:
        raise ValueError("Niezgodny manifest sprzetu.")
    image = ROOT / "dist/firmware-merged.bin"
    wanted = manifest["files"][image.name]["sha256"]
    if hashlib.sha256(image.read_bytes()).hexdigest() != wanted:
        raise ValueError("Niezgodna suma firmware. Pobierz ponownie kompletna paczke.")
    return image, manifest

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument("--yes", action="store_true", help="Potwierdz nadpisanie programu i NVS bez pytania")
    args = parser.parse_args()
    if args.list_ports:
        from serial.tools.list_ports import comports
        for port in comports():
            print(port.device + "  " + str(port.description))
        return 0
    if not args.port:
        parser.error("Wybierz --port; liste pokazuje --list-ports")
    image, manifest = verified_image()
    print("Wersja: " + manifest["version"])
    print("Port: " + args.port + "; adres obrazu: 0x0")
    print("Nadpiszesz program i NVS na tym ESP. Odlacz je od magistrali kotla.")
    if not args.yes and input("Wpisz TAK, aby kontynuowac: ").strip() != "TAK":
        print("Anulowano; nie wyslano nic do urzadzenia.")
        return 1
    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port]
    info = subprocess.run(command + ["flash_id"], check=True, capture_output=True, text=True)
    print(info.stdout)
    if not re.search(r"Detected flash size:\s*16MB", info.stdout):
        raise ValueError("Nie potwierdzono 16 MB flash. Niczego nie wgrano.")
    subprocess.run(command + ["--baud", "460800", "write_flash", "0x0", str(image)], check=True)
    print("\nWgrano obraz. Uruchom ESP normalnie; zaczekaj na ponowne pojawienie sie portu.")
    print("Odczytaj nowe haslo: python tools/setup.py --port " + args.port)
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
