# Jak Wyglada Panel ecoMAX Wi-Fi

Zrzuty rzeczywistego interfejsu wersji **0.7.0-beta.8**, uruchomionego
lokalnie z **danymi przykladowymi (DEMO)**. Nie jest to potwierdzenie
obslugi kazdej nastawy w dowolnym kotle.
Kliknij zdjecie, aby je powiekszyc w GitHubie.

## Odczyty I Temperatury

Stan kotla, temperatury zmierzone i zadane, pokoj, mieszacz oraz poziom
paliwa, jezeli regulator udostepnia te dane.

![Panel glowny ecoMAX Wi-Fi, dane przykladowe](images/panel/01-odczyty.png)

## Harmonogram Tygodniowy

Dni od poniedzialku do niedzieli, godziny 00-24 i kratki po 30 minut.
Przy wybranym harmonogramie widac opis jego dzialania.

![Harmonogram CWU, przykladowy tydzien](images/panel/02-harmonogram.png)

## Historia Temperatur

Wykresy temperatur i nastaw, wybor zakresu do 3 dni, stany pracy i CSV.
Pokazane przebiegi sa wygenerowanym przykladem.

![Wykresy temperatur CO i CWU z przykladowych danych](images/panel/03-historia.png)

## Rozpalanie I Stany Kotla

Osobny widok stanow: m.in. rozpalanie, stabilizacja, praca i wygaszanie.

![Historia stanow kotla, dane DEMO](images/panel/04-stany-kotla.png)

## Zmiana Nastawy

Przykladowe okno zadanej temperatury CWU: wartosc zmierzona, obecna
nastawa, dopuszczalny zakres i zapis. Podczas robienia zdjecia nie
wysylano polecenia do zadnego kotla.

![Okno ustawiania temperatury CWU, bez wykonania zapisu](images/panel/05-nastawa.png)

## Poziom Pelletu

Graficzny zasobnik z procentami z kotla. Brak danych nie oznacza 0%.

![Graficzny poziom pelletu, dane DEMO](images/panel/07-pellet.png)

## Wlasna Kolejnosc Kafelkow

Przycisk edycji ukladu, przeciaganie myszka lub dotykiem i przywracanie
domyslnej kolejnosci. Kazda przegladarka zapamietuje wlasny uklad.

![Zmiana ukladu kafelkow, dane DEMO](images/panel/08-uklad-kafelkow.png)

## Panel Na Telefonie

Ten sam panel na ekranie telefonu; menu boczne jest zwijane.

![Panel w widoku telefonu, dane przykladowe](images/panel/06-telefon.png)

## Narzedzia Panelu

Podpisane przyciski Kafelki i Odswiez oraz menu z nazwami pozostalych funkcji.
Wyloguj jest ostatnia pozycja, oddzielona od narzedzi.

![Nazwane narzedzia i Wyloguj na koncu listy](images/panel/09-narzedzia.png)

## Instalacja I Sprzet

Oba modele **Waveshare ESP32-S3-RS485-CAN** i **ESP32-S3-RS485-CAN-U**
(antena zewnetrzna) uzywaja tego samego firmware, 16 MB flash / 8 MB PSRAM.
Zobacz [instrukcje ze zdjeciami i schematem](START_PL.md),
[zgodnosc i ograniczenia](ZGODNOSC.md) oraz
[instalator USB](https://marexcodemaster.github.io/ecomax-esp32-gateway/).
