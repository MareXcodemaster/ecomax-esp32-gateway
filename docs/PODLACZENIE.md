# Polaczenie RS485 Z Kotlem

Instrukcja obrazkowa: [pierwsze uruchomienie krok po kroku](START_PL.md).

![Polaczenie sygnalow A+ do D+ i B- do D-](images/polaczenie-rs485.svg)

To schemat sygnalow, nie kolejnosc pinow wtyczki RJ. Zdjecie zlacz plytki
od producenta znajduje sie w instrukcji obrazkowej.

## Bezpieczenstwo

W obudowie regulatora sa odsloniete obwody **230 V**. Prace wewnatrz kotla
powinna wykonac osoba z odpowiednimi kwalifikacjami. Wylacz i odizoluj
zasilanie kotla przed podlaczaniem przewodow; nie zmieniaj okablowania
przy dzialajacym urzadzeniu. Nie ruszaj STB, czujnikow zabezpieczajacych,
obwodow palnika ani sterowania pompami.

Instrukcja dotyczy profilu ecoMAX860P-LZ TOUCH / ecoMAX860P4-LZ.
W innych odmianach numer gniazda i wyprowadzenia moga byc inne.
Zawsze sprawdz oznaczenia na konkretnej plycie i jej aktualna dokumentacje.

## Wymagane Elementy

- Waveshare ESP32-S3-RS485-CAN lub ESP32-S3-RS485-CAN-U,
  flash 16 MB / PSRAM 8 MB. Polaczenie RS485 i firmware sa takie same.
- W wariancie -U podlacz antene zewnetrzna Wi-Fi 2,4 GHz do SMA przed
  wlaczeniem. W wersji bez -U antena zewnetrzna wymaga sprzetowego
  przelaczenia toru RF wedlug dokumentacji producenta, nie opcji w WWW.
- Kabel z jedna skrecona para, np. para ze skretki UTP, do sygnalow D+/D-.
- Osobne bezpieczne zasilanie plytki: USB-C 5 V albo zgodny z dokumentacja
  Waveshare zasilacz DC 7-36 V na dedykowanych zaciskach zasilania.
- Wbudowany transceiver RS485 jest izolowany. Nie potrzeba dodatkowego
  modulu TTL-RS485. Zlacze CAN nie jest uzywane.

Nie pobieraj zasilania z przypadkowego pinu G4. Nie podlaczaj zasilania
12/24/230 V do wejsc UART, A+/B- ani do USB.

## Sygnaly

Na sprawdzonej wersji regulatora uzywana jest magistrala panelu **G4**, z
sygnalami oznaczonymi **D+** i **D-**. **G1 / M-BUS nie jest tym portem**.
Nie ustalaj wyprowadzen wedlug kolorow z cudzego przewodu lub zdjecia.

```text
Waveshare - zaciski RS485       ecoMAX - magistrala panelu

A+  -------------------------- D+
B-  -------------------------- D-
```

To opis sygnalow, nie numerow pinow dowolnej wtyczki RJ. Jesli nie masz
jednoznacznych opisow D+/D-, nie podlaczaj na probe. Potrzebny jest schemat
konkretnej wersji regulatora lub pomoc instalatora.

Nie lacz automatycznie GND USB/ESP z PE ani z przypadkowym zaciskiem kotla.
Ewentualny przewod odniesienia RS485 wymaga potwierdzonego, izolowanego
zacisku po obu stronach zgodnie z dokumentacja; powyzsza proba dotyczy
pary roznicowej A+/B-.

Uzywaj magistrali liniowej z krotkim odgalezieniem do ESP, nie rozleglej
gwiazdy. Terminator 120 R na Waveshare na poczatek pozostaw wylaczony,
jesli dolaczasz sie do istniejacej, zakonczonej magistrali panelu. Rezystory
terminujace naleza do dwoch koncow magistrali, nie do kazdego urzadzenia.

## Konflikt Z Fizycznym ecoNET

Aktywny ESP uzywa adresu magistrali ecoNET **0x56 (86)**. Dwa aktywne moduly
pod tym adresem moga kolidowac. Nie jest to konflikt adresow IP Wi-Fi.

- **RS485 bezposrednio**: fizyczny ecoNET musi byc odlaczony/nieaktywny na
  tej magistrali. Panel kotla i fizyczny ecoSTER pozostaja na swoich miejscach.
- **ecoNET lokalny**: ESP wysyla polecenia przez HTTP do Twojego ecoNET.
- **Hybrydowy**: RS485 sluzy pasywnie do odczytow, zapis idzie przez ecoNET.

Firmware ma blokade nadawania po wykryciu innego ecoNET i okres ciszy przed
wlaczeniem transmisji. Nie jest to zamiennik poprawnego podlaczenia.
Nie obchodz tej blokady przez zmiane adresu na losowe urzadzenie.

## Kontrola Po Podlaczeniu

1. Zamknij obudowe i dopiero wtedy przywroc zasilanie.
2. Sprawdz panel ESP: odczyty, wersje modulu/panelu, liczniki ramek,
   brak konfliktu adresu i bledow UART.
3. Pierwsze identyfikowanie i katalog moga wymagac kilkudziesieciu sekund.
4. ESP przypisuje sie do pierwszego poprawnie rozpoznanego kotla i zapisuje
   jego UID lokalnie. Inny UID/model blokuje zapis; nie jest ignorowany.
5. Przy niezgodnej wersji katalogu nie odblokowuj zapisow na sile.
   Przeczytaj [zgodnosc](ZGODNOSC.md) i zglos wersje bez numeru seryjnego.

Piny UART wewnatrz Waveshare sa juz dobrane: TX GPIO17, RX GPIO18,
kierunek GPIO21; 115200 bit/s, 8N1. Nie lacz tych GPIO bezposrednio z kotlem.

## Dokumentacja Producentow

- [Waveshare: ESP32-S3-RS485-CAN](https://www.waveshare.com/wiki/ESP32-S3-RS485-CAN)
- [Waveshare: schemat plytki](https://files.waveshare.com/wiki/ESP32-S3-RS485-CAN/ESP32-S3-RS485-CAN-Schematic.pdf)

Projekt nie dolacza cudzych instrukcji PDF, zdjec instalacji ani grafiki
producenta. Aktualna instrukcje dla swojego regulatora pobierz od producenta.
