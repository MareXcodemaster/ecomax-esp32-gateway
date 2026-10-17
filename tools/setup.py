"""Local USB provisioning. Never save or send credentials to an Internet service."""
import argparse
import getpass
import json
import sys
import time

def exchange(port, payload, expected, timeout=12):
    port.write((json.dumps(payload, ensure_ascii=True) + "\n").encode("ascii"))
    port.flush()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode("utf-8", errors="replace").strip()
        if not line.startswith("[USB] "):
            continue
        try:
            reply = json.loads(line[6:])
        except json.JSONDecodeError:
            continue
        if isinstance(reply, dict) and (reply.get("command") == expected or
                                       expected == "status" and "wifi" in reply):
            return reply
    raise RuntimeError("Brak odpowiedzi USB. Sprawdz port, normalny start ESP i zamknij monitor portu.")

def validate_wifi(ssid, password):
    if not ssid or len(ssid.encode("utf-8")) > 32 or any(ord(c) < 32 or ord(c) == 127 for c in ssid + password):
        raise ValueError("SSID musi miec 1-32 bajty UTF-8.")
    if password and not 8 <= len(password.encode("utf-8")) <= 63:
        raise ValueError("Haslo Wi-Fi musi miec 8-63 bajty lub byc puste dla sieci otwartej.")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Port Twojej plytki, np. COM7 lub /dev/ttyACM0")
    parser.add_argument("--wifi", action="store_true", help="Interaktywnie zapisz wlasne Wi-Fi przez USB")
    args = parser.parse_args()
    import serial
    with serial.Serial(args.port, 115200, timeout=.5, write_timeout=3) as port:
        port.rts = False
        port.dtr = True
        reply = exchange(port, {"command": "setup"}, "setup", timeout=20)
        secret = reply.get("password", "")
        ap_secret=reply.get("ap_password",secret)
        legacy=len(secret)==32 and all(c in "0123456789abcdef" for c in secret)
        current=reply.get("access_schema")==1 and 5<=len(secret)<=48 and 8<=len(ap_secret)<=48 and all(32<=ord(c)<=126 for c in secret+ap_secret)
        if not (legacy or current):
            raise RuntimeError("Odpowiedz nie zawiera poprawnego hasla tej wersji publicznej.")
        print("\nHaslo panelu ESP: " + secret)
        print("Haslo Wi-Fi Piec-Setup: " + ap_secret)
        print("Login: " + reply.get("username", "admin"))
        print("Konfiguracja telefonu: " + reply.get("setup_url", "http://192.168.4.1/diagnostics"))
        print("Wybierz siec Piec-Setup-XXXX swojej plytki. Haslo pozostaje po zwyklej OTA.\n")
        if args.wifi:
            ssid = input("Twoja siec Wi-Fi 2,4 GHz (SSID): ").strip()
            password = getpass.getpass("Haslo tej sieci (nie jest wyswietlane): ")
            validate_wifi(ssid, password)
            result = exchange(port, {"command": "wifi", "ssid": ssid, "password": password}, "wifi")
            password = ""
            if not result.get("ok"):
                raise RuntimeError("Nie zapisano Wi-Fi: " + str(result.get("result", "blad")))
            print("Zapisano dane w Twoim ESP. Nie zapisano ich w pliku na komputerze.")
            time.sleep(3)
            status = exchange(port, {"command": "status"}, "status")
            wifi = status.get("wifi", {})
            print("Stan: " + ("polaczony, IP=" + str(wifi.get("ip", ""))
                               if wifi.get("connected") else "jeszcze oczekuje; sprawdz diagnostyke"))
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, ValueError, OSError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
