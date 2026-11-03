# Jak Wyglada Panel ecoMAX Wi-Fi

Zrzuty rzeczywistego interfejsu wersji **0.7.0-beta.43**, uruchomionego
lokalnie z **danymi przykladowymi (DEMO)**. Nie jest to potwierdzenie
obslugi kazdej nastawy w dowolnym kotle.
Aktualizacja zdjec: **03.11.2026**. Kliknij zdjecie, aby je powiekszyc w GitHubie.

## Cyrkulacja CWU

Opcjonalna cyrkulacja CWU z gniazdkiem Tapo: progi temperatury, tryb reczny
i opcjonalny harmonogram. [Instrukcja](POMPKA_CWU.md).

Na gorze zakladki jest tez **Priorytet grzania CWU** z przyciskiem
**Wylacz priorytet / Wlacz priorytet**. Dotyczy grzania bojlera przez kociol;
nie zmienia trybu ani harmonogramu gniazdka Tapo.

![Pompka CWU z progami i tygodniowa siatka godzin, DEMO](images/panel/0.7.0-beta.43/18-pompka-cwu.png)

Schemat obiegu ze zbiornikiem CWU, rurami, pompa i realistycznym kranem.
Animacja w rurach dziala po potwierdzeniu wlaczenia gniazdka; niebieska
rura oznacza powrot CWU. W ESP nie ma plikow filmowych.

Tryb reczny ma osobne przyciski wlaczenia i wylaczenia. W automatycznym
od razu widac opcjonalny harmonogram.

![Cyrkulacja CWU: sterowanie reczne, dane DEMO](images/panel/0.7.0-beta.43/19-cyrkulacja-recznie.png)

Pobor mocy oraz zuzycie energii z ostatnich 7 dni, odczytywane z gniazdka.

![Energia pompy, dane DEMO](images/panel/0.7.0-beta.43/20-energia-pompy.png)

Panel i diagnostyka maja ikone karty z bialym plomieniem na zielonym tle.
Ikona jest dostepna lokalnie na ESP, takze dla skrotu na ekranie telefonu.
Ten sam znak jest widoczny obok napisu ESP32 w menu bocznym.

## Odczyty I Temperatury

Kafelek Temperatury CWU ma przycisk **Wylacz priorytet / Wlacz priorytet**.
Zmienia pierwszenstwo grzania CWU przed CO, bez wylaczania grzania wody.
Wskazanie jest aktualizowane z odczytu sterownika, takze po zmianach
wykonanych z innego panelu.

Male podpisy temperatur korzystaja z czcionki Inter Medium 12-13 px,
zapisanej w ESP. Kolorowy plomien zmienia ksztalt podczas grzania.
Wentylator obraca sie szybciej przy wiekszej mocy, a wolniej przy mniejszej;
to wizualizacja procentu mocy, nie pomiar obrotow. Przy postoju lub braku
aktualnych danych ruch ustaje. Ograniczenie ruchu w telefonie wylacza ruch.

Stan kotla, temperatury zmierzone i zadane, pokoj, mieszacz oraz poziom
paliwa, jezeli regulator udostepnia te dane. Na gornej belce widac
pieciokreskowy wskaznik i szacowany procent sygnalu Wi-Fi ESP.

![Panel glowny ecoMAX Wi-Fi, dane przykladowe](images/panel/0.7.0-beta.43/01-odczyty.png)

## Schemat Instalacji

Kociol, mieszacz 1, grzejniki, pokoj i zbiornik CWU. Biezace temperatury
oraz stan pomp; brak odczytu pokazuje kreski. Jest to grafika pogladowa,
nie dokumentacja polaczen hydraulicznych konkretnego domu.

![Schemat instalacji z temperaturami, przykladowe dane DEMO](images/panel/0.7.0-beta.43/10-schemat.png)

## Zmiana Sieci Wi-Fi

**Narzedzia > Zmien siec Wi-Fi**: wyszukiwanie sieci, wybor i haslo.
**Narzedzia > Ustawienia ESP Wi-Fi**: status polaczenia, IP i nazwa .local.

![Bezposrednia zmiana sieci Wi-Fi z panelu](images/panel/0.7.0-beta.43/11-zmiana-wifi.png)

## Harmonogram Tygodniowy

Dni od poniedzialku do niedzieli, godziny 00-24 i kratki po 30 minut.
Przy wybranym harmonogramie widac opis jego dzialania.

![Harmonogram CWU, przykladowy tydzien](images/panel/0.7.0-beta.43/02-harmonogram.png)

## Historia Temperatur

Wykresy temperatur i nastaw, wybor zakresu do 3 dni, stany pracy i CSV.
Pokazane przebiegi sa wygenerowanym przykladem.

![Wykresy temperatur CO i CWU z przykladowych danych](images/panel/0.7.0-beta.43/03-historia.png)

## Rozpalanie I Stany Kotla

Osobny widok stanow: m.in. rozpalanie, stabilizacja, praca i wygaszanie.

![Historia stanow kotla, dane DEMO](images/panel/0.7.0-beta.43/04-stany-kotla.png)

## Zmiana Nastawy

Przykladowe okno zadanej temperatury CWU: wartosc zmierzona, obecna
nastawa, dopuszczalny zakres i zapis. Podczas robienia zdjecia nie
wysylano polecenia do zadnego kotla.

![Okno ustawiania temperatury CWU, bez wykonania zapisu](images/panel/0.7.0-beta.43/05-nastawa.png)

## Poziom Pelletu

Animowany podajnik slimakowy przesuwa pellet do kotla tylko przy
aktualnym odczycie pracy podajnika. Po zatrzymaniu lub utracie odczytu
animacja staje. To wizualizacja wyjscia sterownika, nie pomiar obrotow
sruby ani ilosci paliwa. Lekka grafika SVG/CSS, bez filmu.

Graficzny zasobnik z procentami z kotla. Brak danych nie oznacza 0%.

![Graficzny poziom pelletu, dane DEMO](images/panel/0.7.0-beta.43/07-pellet.png)

## Wlasna Kolejnosc Kafelkow

Przycisk edycji ukladu, przeciaganie myszka lub dotykiem i przywracanie
domyslnej kolejnosci. Kazda przegladarka zapamietuje wlasny uklad.

![Zmiana ukladu kafelkow, dane DEMO](images/panel/0.7.0-beta.43/08-uklad-kafelkow.png)

## Panel Na Telefonie

Ten sam panel na ekranie telefonu; menu boczne jest zwijane.

![Panel w widoku telefonu, dane przykladowe](images/panel/0.7.0-beta.43/06-telefon.png)

## Narzedzia Panelu

Podpisane przyciski Kafelki i Odswiez oraz menu z nazwami pozostalych funkcji.
Wyloguj jest ostatnia pozycja, oddzielona od narzedzi.

![Nazwane narzedzia i Wyloguj na koncu listy](images/panel/0.7.0-beta.43/09-narzedzia.png)

## Aktualizacje ESP

Obecna i najnowsza wersja, wybor kanalow stabilnych lub beta oraz
sprawdzanie aktualizacji. Gdy ESP ma najnowsza wersje, belka pokazuje
**Aktualne**. Instalacja nowszego wydania wymaga potwierdzenia.

![Aktualizacje ESP z GitHuba: aktualna wersja beta.37, dane DEMO](images/panel/0.7.0-beta.43/12-aktualizacje.png)

## JSON Do Analizy Pracy Kotla

Pobieranie zapisanych pomiarow z okresu do 3 dni oraz aktualnych nastaw.
Jednostki, czas UTC i oznaczenia brakow danych pozostaja w pliku.

![Eksport JSON do analizy spalania, dane DEMO](images/panel/0.7.0-beta.43/13-analiza-json.png)

## Pamiec I Parametry ESP

Zajetosc programu w slocie OTA, RAM wewnetrznej, PSRAM i archiwum flash.
Osobno parametry procesora, fizyczna pojemnosc flash, czas pracy i restart.
Wskazania ponizej sa przykladowe, nie odczytem prywatnego urzadzenia.

![Pamiec i parametry ESP: cztery pule pamieci i dane procesora, DEMO](images/panel/0.7.0-beta.43/14-pamiec-esp.png)

## Ustawienia Zaawansowane

Osobna sekcja w Parametrach serwisowych. Przykladowe wartosci DEMO,
nie zalecenia konfiguracji; zapis wymaga osobnego potwierdzenia.

Zielony przycisk na dolnej belce Parametrow serwisowych prowadzi
do menu zaawansowanego. Nie ma drugiego przycisku u gory.
8888 to oznaczenie kodu menu na fizycznym ekranie opisanego kotla,
nie haslo logowania do ESP. W innych sterownikach kod moze byc inny.

![Dolna belka z przyciskiem Ustawienia zaawansowane 8888, DEMO](images/panel/0.7.0-beta.43/17-menu-zaawansowane.png)

![Dziewiec rozpoznanych ustawien zaawansowanych, dane DEMO](images/panel/0.7.0-beta.43/15-zaawansowane.png)

## Kalibracja Poziomu Paliwa

Stan Kalibracja jest osobny od 0% i 100%. Widoczny przy aktualnym sygnale
rozpoznanego regulatora; nie oznacza pustego zbiornika. Dane DEMO.

![Stan Kalibracja zamiast falszywego 0 procent, dane DEMO](images/panel/0.7.0-beta.43/16-kalibracja-paliwa.png)

## Instalacja I Sprzet

Oba modele **Waveshare ESP32-S3-RS485-CAN** i **ESP32-S3-RS485-CAN-U**
(antena zewnetrzna) uzywaja tego samego firmware, 16 MB flash / 8 MB PSRAM.
Zobacz [instrukcje ze zdjeciami i schematem](START_PL.md),
[zgodnosc i ograniczenia](ZGODNOSC.md) oraz
[instalator USB](https://marexcodemaster.github.io/ecomax-esp32-gateway/).

## Odczytane obiegi innych konfiguracji

![CWU, grzejniki, podłogówka i odczyty czujników — dane DEMO](images/panel/0.7.0-beta.43/21-schemat-obiegi.png)

Od beta.43 schemat innych konfiguracji wykorzystuje grafiki kotła, grzejnika i CWU.
W „Opis obiegów na schemacie” można przypisać grzejniki, podłogówkę lub ogólny obieg do każdego mieszacza. To ustawienie samego rysunku w przeglądarce, bez polecenia do kotła.
