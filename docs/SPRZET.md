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
