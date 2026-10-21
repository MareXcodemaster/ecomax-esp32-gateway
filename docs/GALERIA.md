# Jak Wyglada Panel ecoMAX Wi-Fi

Zrzuty rzeczywistego interfejsu wersji **0.7.0-beta.22**, uruchomionego
lokalnie z **danymi przykladowymi (DEMO)**. Nie jest to potwierdzenie
obslugi kazdej nastawy w dowolnym kotle.
Aktualizacja zdjec: **21.10.2026**. Kliknij zdjecie, aby je powiekszyc w GitHubie.

## Odczyty I Temperatury

Stan kotla, temperatury zmierzone i zadane, pokoj, mieszacz oraz poziom
paliwa, jezeli regulator udostepnia te dane. Na gornej belce widac
pieciokreskowy wskaznik i szacowany procent sygnalu Wi-Fi ESP.

![Panel glowny ecoMAX Wi-Fi, dane przykladowe](images/panel/0.7.0-beta.22/01-odczyty.png)

## Schemat Instalacji

Kociol, mieszacz 1, grzejniki, pokoj i zbiornik CWU. Biezace temperatury
oraz stan pomp; brak odczytu pokazuje kreski. Jest to grafika pogladowa,
nie dokumentacja polaczen hydraulicznych konkretnego domu.

![Schemat instalacji z temperaturami, przykladowe dane DEMO](images/panel/0.7.0-beta.22/10-schemat.png)

## Zmiana Sieci Wi-Fi

**Narzedzia > Zmien siec Wi-Fi**: wyszukiwanie sieci, wybor i haslo.
**Narzedzia > Ustawienia ESP Wi-Fi**: status polaczenia, IP i nazwa .local.

![Bezposrednia zmiana sieci Wi-Fi z panelu](images/panel/0.7.0-beta.22/11-zmiana-wifi.png)

## Harmonogram Tygodniowy

Dni od poniedzialku do niedzieli, godziny 00-24 i kratki po 30 minut.
Przy wybranym harmonogramie widac opis jego dzialania.

![Harmonogram CWU, przykladowy tydzien](images/panel/0.7.0-beta.22/02-harmonogram.png)

## Historia Temperatur

Wykresy temperatur i nastaw, wybor zakresu do 3 dni, stany pracy i CSV.
Pokazane przebiegi sa wygenerowanym przykladem.

![Wykresy temperatur CO i CWU z przykladowych danych](images/panel/0.7.0-beta.22/03-historia.png)

## Rozpalanie I Stany Kotla

Osobny widok stanow: m.in. rozpalanie, stabilizacja, praca i wygaszanie.

![Historia stanow kotla, dane DEMO](images/panel/0.7.0-beta.22/04-stany-kotla.png)

## Zmiana Nastawy

Przykladowe okno zadanej temperatury CWU: wartosc zmierzona, obecna
nastawa, dopuszczalny zakres i zapis. Podczas robienia zdjecia nie
wysylano polecenia do zadnego kotla.

![Okno ustawiania temperatury CWU, bez wykonania zapisu](images/panel/0.7.0-beta.22/05-nastawa.png)

## Poziom Pelletu

Graficzny zasobnik z procentami z kotla. Brak danych nie oznacza 0%.

![Graficzny poziom pelletu, dane DEMO](images/panel/0.7.0-beta.22/07-pellet.png)

## Wlasna Kolejnosc Kafelkow

Przycisk edycji ukladu, przeciaganie myszka lub dotykiem i przywracanie
domyslnej kolejnosci. Kazda przegladarka zapamietuje wlasny uklad.

![Zmiana ukladu kafelkow, dane DEMO](images/panel/0.7.0-beta.22/08-uklad-kafelkow.png)

## Panel Na Telefonie

Ten sam panel na ekranie telefonu; menu boczne jest zwijane.

![Panel w widoku telefonu, dane przykladowe](images/panel/0.7.0-beta.22/06-telefon.png)

## Narzedzia Panelu

Podpisane przyciski Kafelki i Odswiez oraz menu z nazwami pozostalych funkcji.
Wyloguj jest ostatnia pozycja, oddzielona od narzedzi.

![Nazwane narzedzia i Wyloguj na koncu listy](images/panel/0.7.0-beta.22/09-narzedzia.png)

## Aktualizacje ESP

Obecna i najnowsza wersja, wybor kanalow stabilnych lub beta oraz
sprawdzanie aktualizacji. Gdy ESP ma najnowsza wersje, belka pokazuje
**Aktualne**. Instalacja nowszego wydania wymaga potwierdzenia.

![Aktualizacje ESP z GitHuba: aktualna wersja beta.22, dane DEMO](images/panel/0.7.0-beta.22/12-aktualizacje.png)

## JSON Do Analizy Pracy Kotla

Pobieranie zapisanych pomiarow z okresu do 3 dni oraz aktualnych nastaw.
Jednostki, czas UTC i oznaczenia brakow danych pozostaja w pliku.

![Eksport JSON do analizy spalania, dane DEMO](images/panel/0.7.0-beta.22/13-analiza-json.png)

## Pamiec I Parametry ESP

Zajetosc programu w slocie OTA, RAM wewnetrznej, PSRAM i archiwum flash.
Osobno parametry procesora, fizyczna pojemnosc flash, czas pracy i restart.
Wskazania ponizej sa przykladowe, nie odczytem prywatnego urzadzenia.

![Pamiec i parametry ESP: cztery pule pamieci i dane procesora, DEMO](images/panel/0.7.0-beta.22/14-pamiec-esp.png)

## Ustawienia Zaawansowane

Osobna sekcja w Parametrach serwisowych. Przykladowe wartosci DEMO,
nie zalecenia konfiguracji; zapis wymaga osobnego potwierdzenia.

![Dziewiec rozpoznanych ustawien zaawansowanych, dane DEMO](images/panel/0.7.0-beta.22/15-zaawansowane.png)

## Kalibracja Poziomu Paliwa

Stan Kalibracja jest osobny od 0% i 100%. Widoczny przy aktualnym sygnale
rozpoznanego regulatora; nie oznacza pustego zbiornika. Dane DEMO.

![Stan Kalibracja zamiast falszywego 0 procent, dane DEMO](images/panel/0.7.0-beta.22/16-kalibracja-paliwa.png)

## Instalacja I Sprzet

Oba modele **Waveshare ESP32-S3-RS485-CAN** i **ESP32-S3-RS485-CAN-U**
(antena zewnetrzna) uzywaja tego samego firmware, 16 MB flash / 8 MB PSRAM.
Zobacz [instrukcje ze zdjeciami i schematem](START_PL.md),
[zgodnosc i ograniczenia](ZGODNOSC.md) oraz
[instalator USB](https://marexcodemaster.github.io/ecomax-esp32-gateway/).
