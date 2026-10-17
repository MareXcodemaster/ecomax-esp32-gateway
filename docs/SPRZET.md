# Jaki Modul Kupic I Gdzie Podlaczyc Przewody

Wybierz dokladnie **Waveshare ESP32-S3-RS485-CAN** albo
**Waveshare ESP32-S3-RS485-CAN-U**, **16 MB flash / 8 MB PSRAM**.
Nie zamawiaj innej plytki tylko dlatego, ze ma ESP32-S3 lub RS485.
Oba ponizsze modele korzystaja z tego samego firmware.

## Antena Zewnetrzna: Wariant -U

**ESP32-S3-RS485-CAN-U** ma zlacze SMA i fabrycznie aktywny tor anteny
zewnetrznej. Podlacz antene Wi-Fi 2,4 GHz przy wylaczonym zasilaniu.

![Waveshare ESP32-S3-RS485-CAN-U, model ze zlaczem anteny SMA](https://www.waveshare.com/img/devkit/ESP32-S3-RS485-CAN/ESP32-S3-RS485-CAN-details-2-2.jpg)

## Antena Wbudowana: Wariant Bez -U

**ESP32-S3-RS485-CAN** domyslnie uzywa anteny wbudowanej. Dodatkowa antena
wymaga sprzetowej zmiany polozenia rezystora zgodnie z instrukcja
producenta; samo wpiecie przewodu nie wystarcza. W programie nie ma
przelacznika anteny. Prace lutownicze zlec serwisowi.

## Montaz Na Szynie I Antena Poza Obudowa

Obudowa tego Waveshare ma mocowanie na **szyne DIN 35 mm**. Instalator moze
umiescic modul w szafce automatyki kotla albo w osobnej skrzynce obok.
Wewnatrz kotla tylko wtedy, gdy sa odpowiednie miejsce, temperatura,
odstepy od 230 V i bezpieczne zasilanie zgodne z dokumentacja plytki.
Nie montuj ESP w komorze spalania, przy goracym czopuchu ani w wilgoci.
Nie pobieraj zasilania z przypadkowych pinow sterownika.

Metalowa oslona kotla tlumi Wi-Fi. Mozna dokupic antene **Wi-Fi 2,4 GHz,
50 omow** i pasujacy przedluzacz antenowy, aby antena byla **na zewnatrz
metalowej obudowy**, a modul zostal na szynie wewnatrz. To moze poprawic
zasieg; efekt zalezy od miejsca montazu. Krotki przewod antenowy ogranicza
straty. Przejscie przez obudowe powinno miec ochrone przed przetarciem,
a przewod nie moze dotykac goracych elementow ani przewodow sieciowych.
Montaz wykonuje instalator przy bezpiecznie odlaczonym zasilaniu.

**Uwaga na zlacze: producent opisuje wariant -U jako SMA, nie RP-SMA.**
SMA i RP-SMA nie sa zamienne; sprawdz zarowno gwint, jak i srodkowy styk.
Dobierz antene i przewod do rzeczywistego zlacza swojej rewizji. Antena
RP-SMA wymaga odpowiednio dobranego adaptera. W wariancie bez -U nadal
konieczne jest sprzetowe przelaczenie toru antenowego, opisane wyzej.
Przed zakupem sprawdz, czy antena nie jest juz w zestawie sprzedawcy.

Zrodlo: [Waveshare: SMA, warianty anteny i obudowa DIN 35 mm](https://www.waveshare.com/ESP32-S3-RS485-CAN.htm).

![Waveshare ESP32-S3-RS485-CAN, model z antena wbudowana](https://www.waveshare.com/img/devkit/ESP32-S3-RS485-CAN/ESP32-S3-RS485-CAN-details-2-1.jpg)

Zrodlo zdjec i porownania: [Waveshare - oba warianty produktu](https://www.waveshare.com/product/iot-communication/short-range-wireless/esp32-s3-rs485-can.htm).
Zdjecia sa ladowane od producenta, wymagaja Internetu i nie sa objete
licencja MIT projektu. Projekt nie prowadzi sklepu i nie potwierdza
sprzetowo kazdej rewizji plytki. Porownaj etykiete, pamiec i zlacza.

## Gdzie Wpiac Sie Do Waveshare

Wybierz zaciski srubowe **RS485 A+ i B-**, nie CAN H/L ani zasilanie.
USB-C sluzy do pierwszego wgrania i zasilania 5 V. Osobne wejscie
**DC 7-36 V** to alternatywne zasilanie, nie magistrala kotla.

![Waveshare: oznaczenia zaciskow RS485, CAN, zasilania i USB-C](https://www.waveshare.com/img/devkit/ESP32-S3-RS485-CAN/ESP32-S3-RS485-CAN-details-17.jpg)

<a id="boot-usb"></a>

## Gdzie Jest Przycisk BOOT

Na zdjeciu producenta **BOOT to element 13**, a sasiedni **RESET to 14**.
Szukaj napisu BOOT na plytce, obok kolumny diod. Zdjecie pokazuje plytke
bez obudowy; dostep do przyciskow zalezy od wersji obudowy. Jezeli trzeba
zdjac obudowe ESP, najpierw odlacz USB i wszystkie inne zrodla zasilania.
Nie otwieraj pracujacego kotla i nie zwieraj pinow ani zaciskow.

![Zdjecie Waveshare: BOOT oznaczony numerem 13, RESET numerem 14, USB-C numerem 5](https://docs.waveshare.net/assets/images/ESP32-S3-RS485-CAN-details-intro-259fd812b2f10c65831ab2efc6c161ef.webp)

Zdjecie ladowane od producenta, wymaga Internetu i nie jest objete MIT
projektu. [Opis elementow 13/14 w dokumentacji Waveshare](https://docs.waveshare.net/ESP32-S3-RS485-CAN/#%E8%B5%84%E6%BA%90%E7%AE%80%E4%BB%8B).

Przy pustym flash i znikajacym USB: odlacz cale zasilanie ESP, przytrzymaj
BOOT, trzymajac przycisk podlacz USB do komputera, po 2-3 sekundach pusc
BOOT i ponownie wybierz port w instalatorze.

![Rysunek pogladowy: wyrozniony BOOT i kolejnosc podlaczania pustego ESP](images/boot-usb.svg)

Zrodlo procedury: [Waveshare: BOOT podczas ponownego podania zasilania](https://docs.waveshare.net/ESP32-S3-RS485-CAN/Firmware-Flashing/).
Sam BOOT nie kasuje pamieci. Nie trzeba trzymac go przez caly zapis.

## Gdzie Wpiac Sie Do Kotla

**Tylko dla potwierdzonej wersji ecoMAX860P-LZ TOUCH / ecoMAX860P4-LZ:**
magistrala panelu **G4**, sygnaly **D+ i D-**. **Nie G1 / M-BUS.**
Jezeli do G4 jest juz podlaczony ekran, ESP dolacza do tej samej
magistrali; ekran zostaje podlaczony. Nie zwieraj pozostalych pinow.

![Schemat G4 dla potwierdzonego profilu: A+ do D+, B- do D-](images/polaczenie-rs485.svg)

| Waveshare, RS485 | Potwierdzona magistrala G4 regulatora |
| --- | --- |
| A+ | D+ |
| B- | D- |

Uzyj jednej skreconej pary przewodow, np. pary z UTP. To schemat
**sygnalow**, nie uniwersalny rozklad pinow wtyczki RJ i nie schemat
wszystkich modeli ecoMAX. Nie zgaduj numerow pinow na podstawie kolorow.
Na innej plycie potwierdz gniazdo i sygnaly w jej dokumentacji.
Nie pobieraj zasilania ESP z niepotwierdzonych pinow G4.

**Wewnatrz kotla wystepuje 230 V.** Podlaczenie wykonuje instalator po
bezpiecznym odlaczeniu zasilania. Nie ruszaj obwodow STB, palnika i pomp.
Przeczytaj [pelne zasady podlaczenia, terminacji i konfliktu z ecoNET](PODLACZENIE.md)
przed uruchomieniem. Aktywny RS485 ESP i fizyczny ecoNET nie moga
nadawac na tej samej magistrali pod adresem 0x56.

Dalej: [instalacja i wlasne Wi-Fi krok po kroku](START_PL.md),
[galeria panelu](GALERIA.md), [zgodnosc i ograniczenia](ZGODNOSC.md).
