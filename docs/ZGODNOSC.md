# Zgodnosc I Ograniczenia Bety

## Profil Referencyjny

| Element | Profil |
| --- | --- |
| Plytka | Waveshare ESP32-S3-RS485-CAN lub ESP32-S3-RS485-CAN-U |
| Pamiec | 16 MB flash, 8 MB PSRAM |
| Model regulatora | ecoMAX860P-LZ TOUCH / ecoMAX860P4-LZ |
| Modul A | 8.41.38.L0 |
| Panel kotla | 8.30.28 |
| Termostat | ecoSTER / eSTER x40, zaobserwowany uklad 1.21.38 |
| RS485 | 115200, 8N1, GPIO17/18/21 na wskazanej plytce |

Oba warianty uzywaja tego samego obrazu. Wedlug
[Waveshare](https://www.waveshare.com/esp32-s3-rs485-can.htm) wariant -U
ma SMA i domyslnie wlaczona antene zewnetrzna, a wersja bez -U korzysta
domyslnie z anteny wbudowanej. Wybor toru anteny jest sprzetowy.
Nie jest to osobny test fizyczny publicznej bety na wariancie -U.
Z poziomu programu nie da sie potwierdzic polozenia rezystora ani
odczytac etykiety obudowy; samo wykrycie ESP32-S3 nie identyfikuje modelu.

To nie znaczy, ze wszystkie kotly z nazwa ecoMAX860P maja identyczny
protokol, katalog i mozliwosci zapisu. Wersje nazw, jednostek, enumow,
metadanych oraz struktury i uprawnien sa sprawdzane przed udostepnieniem
edycji. Nieznany wariant ma pozostac zablokowany, nie otrzymac zgadywanych
indeksow. Odczyt danych i mozliwosc zapisu to dwie osobne kwestie.

## Sterowniki: Lista I Status

**Posiadanie ecoNET nie jest potwierdzeniem pelnej zgodnosci tej bety.**
To niezalezny panel, nie kopia calego oprogramowania ecoNET. Lokalny
modul HTTP pomaga ujednolicic odczyty, ale wersje API, nazwy pol,
indeksy nastaw, termostaty i uprawnienia moga sie roznic.

| Regulator | Status w tym projekcie |
| --- | --- |
| ecoMAX860P-LZ TOUCH, modul A 8.41.38.L0, panel 8.30.28 | Profil referencyjny, uzywany w instalacji prywatnej; nie kazdy zapis przetestowany |
| ecoMAX860P4-LZ | Oznaczenie sprzetowe modulu w tej samej instalacji referencyjnej, nie osobny drugi test |
| ecoMAX810P-L TOUCH | Wymieniony przez zewnetrzna integracje ecoNET300; w tym panelu brak testu zgodnosci |
| ecoMAX850R2-X | Wymieniony przez zewnetrzna integracje ecoNET300; w tym panelu brak testu zgodnosci |
| ecoMAX360 | Wymieniony przez zewnetrzna integracje ecoNET300; w tym panelu brak testu zgodnosci |
| ecoMAX860P2-N | Wymieniony przez zewnetrzna integracje ecoNET300; w tym panelu brak testu zgodnosci |
| ecoMAX860P3-V | Wymieniony przez zewnetrzna integracje ecoNET300; w tym panelu brak testu zgodnosci |
| Inny ecoMAX lub inny firmware modelu powyzej | Niezweryfikowany; nie obiecujemy pelnego odczytu i zapisu |

Lista pieciu dodatkowych modeli pochodzi z
[README projektu jontofront](https://github.com/jontofront/ecoNET-300-Home-Assistant-Integration#-supported-devices),
sprawdzonego 17.10.2026. To **lista innego projektu**, a nie deklaracja
obslugi przez ten firmware. Nie przenosimy z niej obslugi ecoSOL,
pomp ciepla ani innych urzadzen do panelu kotla.

**Lokalny ecoNET:** czesc odczytow moze dzialac przez wspolne API.
Specjalne nastawy pokojowe i sterowanie kotlem w obecnym kodzie maja
kontrole konkretnego modelu i firmware. Nie nalezy ich odblokowywac
przez podmiane identyfikatora modelu.

**Bezposredni RS485:** identyfikacja i katalog zapisu sa ograniczone
do profilu referencyjnego. Inny model nie otrzymuje zgadywanych komend.
Fakt, ze PyPlumIO obsluguje wiecej wariantow protokolu, nie oznacza,
ze wszystkie jego mozliwosci zostaly przeniesione do tej aplikacji C++.

## Termostaty I Panele Pokojowe

| Urzadzenie | Status w tym projekcie |
| --- | --- |
| eSTER_x40, pierwszy termostat instalacji referencyjnej | Odczyt i nastawy udostepniane przez zgodny kociol; obserwowany uklad parametrow 1.21.38 |
| eSTER_x80 | Rodzina urzadzen wspolpracujacych z wybranymi ecoMAX; brak osobnego testu tego panelu |
| ecoSTER TOUCH | Brak osobnego testu odczytu i edycji w tym projekcie |
| ecoSTER200 | Brak osobnego testu odczytu i edycji w tym projekcie |
| Zwykly termostat ON/OFF | Nie przekazuje przez styk cyfrowej temperatury i profili do edycji |
| Wlasne czujniki DS18B20, Zigbee, BLE, Tuya | Nie sa w tej wersji bezposrednimi czujnikami pokojowymi ESP |

ESP komunikuje sie z kotlem i odczytuje dane udostepnione przez regulator.
Nie laczy sie radiowo bezposrednio z eSTER, nie emuluje nowego termostatu
i nie zastepuje zabezpieczen ogrzewania. Brak termostatu w instalacji
oznacza brak jego danych, a nie wartosc zero.

Zrodla rodzin urzadzen i ograniczen protokolu:
[instrukcja regulatora opublikowana przez PLUM](https://hvac.plum.pl/wp-content/uploads/2023/12/ecoMAX362P1-KV_T4_DTR_UA_wydanie_1.0.pdf),
[PyPlumIO: mieszacze i termostaty](https://pyplumio.denpa.pro/mixers_thermostats.html),
[PyPlumIO: dane regulatora sa zalezne od urzadzenia](https://pyplumio.denpa.pro/reading.html).
Nowszych modeli o podobnej nazwie nie uznajemy automatycznie za ten sam termostat.

## Czy Bedzie Dzialac Z Wiekszoscia Kotlow?

To mozliwy kierunek rozwoju, **nie potwierdzony stan tej wersji**.
Do rozszerzenia listy potrzeba odczytu modelu i wersji, zanonimizowanego
katalogu/API lub ramek oraz osobnego potwierdzenia konkretnych zapisow.
Nie wystarczy ten sam wyglad strony w chmurze. Producent ecoNET300 sam
zastrzega zgodnosc z odpowiednim oprogramowaniem regulatora:
[instrukcja ecoNET300, rozdzial 5](https://hvac.plum.pl/wp-content/uploads/2024/02/ecoNET300_DTR_EN_wydanie_1.2.pdf).

## Weryfikacja Wydania

Poprzednia wersja prywatna byla uzywana z powyzszym profilem. Publiczna beta
zachowuje jej logike protokolu i zabezpieczen, ale zmienia provisioning,
przypisanie tozsamosci oraz zasoby interfejsu.

Oczyszczenie, testy parserow/interfejsu i kompilacja sa sprawdzane programowo.
Nowy proces od wymazanego ESP do wlasnej sieci i obcego, zgodnego kotla
**nie zostal jeszcze potwierdzony na osobnym fizycznym zestawie**.
Dlatego wydanie jest beta, a nie stabilna, certyfikowana premiera.

Nie testowano na kazdym kotle kazdej nastawy. Nie ma gwarancji zastapienia
wszystkich funkcji chmury ecoNET24. Nie wlaczaj edycji serwisowej, jesli
nie rozumiesz skutku parametru, nawet gdy sterownik udostepnia zakres.

## Znane Ograniczenia

- Nie ma osobnej symulacji fizycznego termostatu ani nowych czujnikow
  pokojowych. ecoSTER jest odczytywany i konfigurowany, nie zastapiony.
- Poziom i strumien pelletu zaleza od danych sterownika. Obliczenia ESP sa
  szacunkiem. Brak potwierdzonej uniwersalnej komendy kalibracji 100% kotla.
- Historia zaczyna sie od uruchomienia archiwum. Nie odtwarza przeszlosci
  z chmury. Probki minutowe moga pominac bardzo krotkie stany.
- Po restarcie potrzebny jest prawidlowy czas. Otworz panel z urzadzenia
  z poprawnym zegarem. Opcjonalny aktualizator moze pobrac czas przez NTP;
  bez wiarygodnej daty archiwum nie tworzy fikcyjnych wpisow.
- Po zaniku zasilania ostatnia minuta archiwum moze nie zostac zapisana.
- Obraz pierwszej instalacji i OTA to rozne pliki. Zmiana tablicy partycji
  nie jest zwykla aktualizacja OTA.
- Nie ma HTTPS, bezposredniego klienta Tailscale ani automatycznej chmury.
