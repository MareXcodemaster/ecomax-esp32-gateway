# Instalacja USB I Aktualizacja OTA

**[Pelna instrukcja po polsku ze zdjeciami i schematem](START_PL.md)**.
W pobranej paczce mozesz tez otworzyc `docs/START_PL.html` bez instalowania
dodatkowych programow. Zrzuty ekranu i schemat sa lokalne; zdjecie producenta
wymaga polaczenia z Internetem.

## Przed Instalacja

- Wymagana plytka: Waveshare ESP32-S3-RS485-CAN, 16 MB flash, 8 MB PSRAM.
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
   indywidualne haslo Twojego urzadzenia; nie trzeba wpisywac polecen.
6. Wpisz nazwe i haslo wlasnej sieci 2,4 GHz i nacisnij Zapisz i polacz.
7. Po uzyskaniu adresu nacisnij Otworz panel ESP. Uzyj loginu admin i hasla ESP
   wyswietlonego w kroku 3 instalatora. Zachowaj je w menedzerze hasel.

Domowe haslo Wi-Fi idzie tylko przez USB, nie do GitHuba ani autora. Skrypty
instalatora i pliki BIN sa hostowane razem, bez zewnetrznego CDN i analityki.
Przy ponownej konfiguracji sieci wystarczy Polacz ESP przez USB, bez wgrywania.
Mozesz tez uzyc telefonu przez siec ESP, zgodnie z [WIFI.md](WIFI.md).

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

Program wysyla tylko lokalne polecenie USB `setup`. Wyswietla indywidualne
haslo sieci konfiguracyjnej i panelu oraz login `admin`. Zachowaj haslo
prywatnie; nie wysylaj tego ekranu do publicznego zgloszenia.

Przy pierwszym starcie haslo jest generowane przez ESP i utrwalane w NVS.
Zwykla aktualizacja OTA go nie zmienia. Wyczyszczenie NVS/flash powoduje
wygenerowanie nowego hasla. Haslo nie jest wspolne dla wszystkich plytek.

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
3. Zaloguj sie indywidualnym haslem urzadzenia.
4. W sekcji aktualizacji wybierz obraz OTA, nie obraz scalony.
5. Wyslij go raz. Nie odlaczaj zasilania podczas zapisu i restartu.
6. Po powrocie panelu sprawdz wersje, odczyty i ewentualny komunikat bledu.

Przy nieznanym wyniku najpierw sprawdz dostepnosc i wersje. Nie wysylaj
wielokrotnie obrazu ani polecen zapisujacych nastawy w petli.

Przejscie ze starszego prywatnego firmware do tej publicznej wersji nie
jest traktowane jako przetestowana sciezka OTA. Pierwsze publiczne testy
zaleca sie wykonywac na osobnej plytce, nie zastepowac dzialajacej instalacji.
