# Instalacja USB I Aktualizacja OTA

**[Pelna instrukcja po polsku ze zdjeciami i schematem](START_PL.md)**.
W pobranej paczce mozesz tez otworzyc `docs/START_PL.html` bez instalowania
dodatkowych programow. Zrzuty ekranu i schemat sa lokalne; zdjecie producenta
wymaga polaczenia z Internetem.

## Przed Instalacja

- Wymagana plytka: Waveshare ESP32-S3-RS485-CAN lub ESP32-S3-RS485-CAN-U,
  16 MB flash, 8 MB PSRAM. Ten sam obraz dla obu modeli. Wariant -U ma
  fabrycznie aktywna antene zewnetrzna SMA; podlacz antene Wi-Fi 2,4 GHz.
- Odlacz RS485 od kotla na czas pierwszego uruchomienia i konfiguracji.
- Potrzebny jest kabel USB-C z transmisja danych. Instalator w przegladarce
  nie wymaga Pythona; Python 3.10+ jest potrzebny tylko do wariantu recznego.
- Nie uzywaj obrazu na zwyklym ESP32, innym module S3 lub flash 4/8 MB.
- Wgranie obrazu scalonego pod 0x0 nadpisuje program i obszar NVS.
  Zachowaj potrzebne ustawienia poprzedniego programu przed instalacja.
  Nie wgrywaj zrzutu flash z innego urzadzenia.

## Najprosciej: Instalator W Przegladarce

1. Otworz https://marexcodemaster.github.io/ecomax-esp32-gateway/ na komputerze
   w Chrome lub Edge.
2. Potwierdz model i rozmiar pamieci z opisu swojej plytki. Samo wykrycie
   rodziny ESP32-S3 nie potwierdza obecnosci 16 MB flash ani 8 MB PSRAM.
3. Nacisnij Wgraj na ESP, wybierz port USB i potwierdz wgranie. Nie odlaczaj
   zasilania podczas zapisu. Pierwsza instalacja nadpisuje program i NVS,
   nawet gdy nie wybierzesz dodatkowego kasowania calej pamieci.
4. Zamknij okno wgrywania. Jesli ESP pozostalo w bootloaderze, nacisnij RESET.
5. Nacisnij Polacz ESP przez USB i ponownie wybierz port. Instalator odczyta
   dane dostepu; domyslnie panel ma login admin i haslo admin.
6. Kliknij Wyszukaj sieci, wybierz wlasne Wi-Fi 2,4 GHz z listy,
   wpisz tylko haslo i nacisnij Zapisz i polacz.
7. Po uzyskaniu adresu nacisnij Otworz panel ESP. Uzyj loginu admin i hasla admin
   (lub wlasnego hasla ustawionego w panelu).

Domowe haslo Wi-Fi jest zapisywane na ESP lokalnie przez USB.
Przy ponownej konfiguracji sieci wystarczy Polacz ESP przez USB, bez wgrywania.
Mozesz tez uzyc telefonu przez siec ESP, zgodnie z [WIFI.md](WIFI.md).

## Pusta Pamiec: USB Nie Laczy Lub Ciagle Znika

Po pelnym wyczyszczeniu flash modul moze restartowac sie, a instalator
czekac bez konca lub gubic port USB. Wejdz recznie w tryb wgrywania:

1. Odlacz USB i inne zasilanie ESP; RS485 do kotla ma byc odlaczone.
2. Przytrzymaj **BOOT**, nie RESET.
3. **Trzymajac BOOT, podlacz USB do komputera.**
4. Po 2-3 sekundach pusc BOOT, ponownie wybierz port i rozpocznij wgrywanie.

![BOOT przed podlaczeniem USB: rysunek i cztery kroki](images/boot-usb.svg)

[Zdjecie producenta: BOOT nr 13, RESET nr 14](SPRZET.md#boot-usb).
Jesli obudowa zakrywa przycisk, dostep przygotuj przy odlaczonym zasilaniu.
Nie zwieraj pinow. Sam BOOT nie kasuje pamieci; nie trzymaj go przez caly
zapis i nie odlaczaj USB w trakcie zapisu firmware.
Zrodlo: [procedura Waveshare](https://docs.waveshare.net/ESP32-S3-RS485-CAN/Firmware-Flashing/).

## Zapasowo: Wariant Z Gotowej Paczki

Pobierz ZIP instalacyjny z Releases i rozpakuj go. W terminalu otwartym
w rozpakowanym katalogu wykonaj (na Windows mozna uzyc `py` zamiast `python`):

```powershell
python -m pip install -r requirements.txt
python tools/flash.py --list-ports
python tools/flash.py --port COM7
```

`COM7` to tylko przyklad. Wybierz port, ktory pojawia sie po podlaczeniu
Twojej plytki. Linux: np. `/dev/ttyACM0`, macOS: np. `/dev/cu.usbmodem...`.

Instalator sprawdza sume pliku, typ ukladu i raportowany rozmiar flash.
Pyta o zgode przed zapisem. Nie odczytuje ani nie zapisuje nastaw kotla.

Gdy bootloader nie odpowiada: przytrzymaj BOOT, nacisnij i zwolnij RESET,
zwolnij BOOT, sprawdz ponownie port i ponow instalacje. Nie zwieraj
przypadkowych pinow. Po zakonczeniu uruchom normalnie przyciskiem RESET.

## Haslo Twojego Urzadzenia

Po restarcie odczytaj je przez USB:

```powershell
python tools/setup.py --port COM7
```

Program wysyla tylko lokalne polecenie USB `setup`.
Domyslny panel: login **admin**, haslo **admin**.
Domyslne Wi-Fi **Piec-Setup-XXXX**: haslo **piecwifi**.
Nie ma losowego generowania hasel. Odczyt USB pokazuje tez wlasne hasla,
jezeli zostaly zmienione. Nie publikuj ich.

Hasla zmienisz w **Ustawienia urzadzenia > Hasla dostepu**.
Od beta.9 zapisane tam wlasne hasla sa zachowywane przez kolejne OTA.
Przejscie z beta.8 lub starszej zastapi dawne automatyczne haslo powyzszymi
domyslnymi danymi. Wyczyszczenie NVS przywraca wartosci domyslne.

Dalej wykonaj [konfiguracje Wi-Fi](WIFI.md).

## Reczne Wgranie Przez esptool

Po sprawdzeniu modelu i sumy SHA256 mozna zamiast instalatora uzyc:

```powershell
python -m esptool --chip esp32s3 --port COM7 --baud 460800 write_flash 0x0 dist/firmware-merged.bin
```

- `firmware-merged.bin`: pierwsza instalacja USB, adres **0x0**.
- `firmware-ota.bin`: tylko aplikacja, **nie wgrywaj pod 0x0**.
- Nie wykonuj `read_flash` na swoim urzadzeniu w celu przygotowania pliku
  dla innych ludzi: zrzut moze zawierac hasla, UID i historie.

Pelne `erase_flash` kasuje takze historie, tozsamosc i wszystkie ustawienia.
Nie jest czescia zwyklej OTA. Uzywaj go wylacznie swiadomie, na swoim ESP,
po odlaczeniu przewodow od kotla i zachowaniu potrzebnych danych.

## OTA Z Telefonu Lub Tabletu

1. Pobierz `firmware-ota.bin` dla zgodnej plytki i wersji partycji.
2. Wejdz na `http://piec.local/diagnostics` lub `http://ADRES_ESP/diagnostics`.
3. Zaloguj sie jako admin / admin (lub wlasnym haslem panelu).
4. W sekcji aktualizacji wybierz obraz OTA, nie obraz scalony.
5. Wyslij go raz. Nie odlaczaj zasilania podczas zapisu i restartu.
6. Po powrocie panelu sprawdz wersje, odczyty i ewentualny komunikat bledu.

Przy nieznanym wyniku najpierw sprawdz dostepnosc i wersje. Nie wysylaj
wielokrotnie obrazu ani polecen zapisujacych nastawy w petli.

Przejscie ze starszego prywatnego firmware do tej publicznej wersji nie
jest traktowane jako przetestowana sciezka OTA. Pierwsze publiczne testy
zaleca sie wykonywac na osobnej plytce, nie zastepowac dzialajacej instalacji.
