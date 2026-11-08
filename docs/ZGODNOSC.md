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
edycji. Nowy wariant pobiera własny katalog; nie otrzymuje zgadywanych
indeksów. Odczyt danych i mozliwosc zapisu to dwie osobne kwestie.

## Wspólny firmware od beta.42

ESP rozpoznaje sterownik ecoMAX-P i odczytuje jego własny katalog przez
RS485. Nie wymaga identycznego numeru firmware jak model referencyjny.
Obsługiwany jest protokół zdalnego menu 1.0, do 255 nastaw. Od beta.59
drzewo menu może mieć ponad 256 elementów (kategorie, odczyty i nastawy),
w granicach ramki 1000 bajtów / 493 słów struktury i 256 kategorii.
Katalog jest pobierany etapami, z zachowaniem odebranych stron po
chwilowym braku odpowiedzi. Własne nazwy, jednostki,
opcje, metadane i struktura muszą być kompletne oraz zgodne przed i po
odczycie. Zmiana wersji katalogu uruchamia ponowne pobranie.

Standardowe START/STOP (59 / 187) nie jest blokowane wyłącznie przez inny
numer firmware producenta. Nadal wymaga potwierdzonej tożsamości,
aktualnego rozpoznanego stanu oraz — dla START — braku zgłoszonych alarmów.
Sam odczyt raportu nie dowodzi wykonania komendy: po ręcznym poleceniu ESP
sprawdza odpowiedź i nowy stan. Automatycznie nie uruchamia kotła.

Nieznany wariant schematu otrzymuje podgląd obiegów obecnych w odczytach:
CWU, mieszaczy, bufora lub sprzęgła. To widok poglądowy; protokół nie
opisuje pełnego fizycznego przebiegu rur. Brak odczytu nie jest stanem OFF.
Schemat 23 zachowuje dotychczasowy rysunek.

## ecoMAX810P-L TOUCH — profil od beta.40

**Użyj beta.42 lub nowszej.** Raport wariantu modułu A **10.40.186.R5**
i panelu **10.14.50** zawiera 169 pozycji oraz dwa mieszacze. Służy jako
przypadek testowy. Od beta.42 katalog jest odczytywany z podłączonego
sterownika, bez listy dopuszczonych numerów tej wersji.

Nastawy zmienia się w **Parametrach użytkownika / Parametrach serwisowych**.
Po aktualizacji trzeba poczekać na pobranie nazw i zakresów. Jeśli nie
otrzymano kompletnego katalogu, odczyt jest ponawiany po minucie.
Wtedy raport zgodności zawiera powód przerwania odczytu.

**Rzeczywiste polecenia na innym kotle wymagają sprawdzenia u właściciela.**
Testy protokołu obejmują różne numery firmware, nie wszystkie instalacje.
Kalibracja paliwa, specjalne ustawienia producenta, bezpośrednie skróty
nastaw i mapy termostatów/harmonogramów pozostają dostępne wyłącznie tam,
gdzie ich kodowanie jest potwierdzone. Nie są kopiowane z innego modelu.

## ecoMAX860P3-O — poprawka katalogu od beta.59

Raport sterownika **ecoMAX860P3-O**, moduł A **16.33.91.P1**, panel
**16.11.10**, zawiera 198 pozycji metadanych i zakresów oraz 274 elementy
drzewa menu. W drzewie jest 106 odwołań do nastaw; edycja każdej zależy
od jej obecności, zakresu i uprawnień zgłoszonych przez regulator.
Raport pokazuje także moduł B 1.31.14, dwa mieszacze i dwa termostaty.

Beta.59 usuwa zbyt mały limit drzewa i pobiera własne nazwy, jednostki,
opcje oraz uprawnienia w krótkich partiach. Nie wgrywa nazw ani indeksów
z kotła autora. Na pierwsze pobranie należy dać ESP kilka minut; panel
pokazuje etap i liczbę odebranych nazw. Po pełnym odczycie dostępne
nastawy pojawiają się w **Parametrach użytkownika / Parametrach serwisowych**.
Chwilowy brak odpowiedzi ponawia tę samą stronę. Trzy kolejne nieudane
próby, zmiana katalogu lub przekroczenie limitu kończą sesję; nowa zaczyna
się po minucie. Niepełny katalog nie udostępnia zapisu.

Testy na komputerze obejmują metadane i strukturę z raportu oraz
symulowane tabele tekstowe, utratę odpowiedzi i zmiany wersji katalogu.
**Zapis na fizycznym kotle tego właściciela pozostaje do potwierdzenia.**
Właściciel może sprawdzić pojedynczą nastawę temperatury: zanotować
poprzednią wartość, zmienić o 1°C, sprawdzić potwierdzenie i ekran kotła,
a następnie przywrócić poprzednią wartość. Nie trzeba uruchamiać palnika.

ecoSTER **1.21.27** zgłasza inny układ danych niż referencyjny 1.21.38.
Ta poprawka nie oznacza potwierdzenia jego osobnych komend ani
harmonogramów. Bezpośrednie polecenia producenta, kasowanie i kalibracja
nie są odblokowywane samą nazwą nowego modelu.

## Inny kocioł lub blokada zapisu — raport zgodności

Od **0.7.0-beta.39** panel ma **Narzędzia → Raport zgodności kotła**.
Ten sam przycisk jest w **Ustawieniach urządzenia** oraz przy komunikacie
o nieobsługiwanym modelu lub innej tożsamości kotła.

1. Podłącz ESP do magistrali regulatora zgodnie z instrukcją i otwórz panel.
2. Wybierz **Narzędzia → Raport zgodności kotła**.
3. Pozostaw zaznaczone **Uzupełnij zapytaniami odczytu RS485** i naciśnij
   **Odczytaj i pobierz JSON**. Odczyt może potrwać około minuty.
4. Prześlij autorowi pobrany plik `ecomax-zgodnosc-….json`. Możesz dołączyć
   model regulatora, wersję z ekranu oraz zdjęcia brakujących pozycji menu.

Nie trzeba włączać palnika ani zmieniać temperatur do wykonania raportu.
Raport działa również wtedy, gdy model nie ma profilu zapisu. Zawiera
rzeczywiście odebraną identyfikację modelu, wersje modułów, metadane,
wartości i strukturę dostępnych parametrów, odczyty czujników oraz wyniki
poszczególnych zapytań. Nazwy i jednostki nieznanego modelu nie są
zastępowane nazwami z profilu referencyjnego. `reference_profile` oznacza
wzorzec firmware, a nie wykryty kocioł. Identyfikacja jest sprawdzana na
początku i na końcu zbierania danych.

Zapytania korzystają wyłącznie z zamkniętej listy odczytów. Funkcja nie
zmienia nastaw, przypisania kotła, trybu połączenia ani konfiguracji ESP.
Przy braku odpowiedzi kończy odczyt po ograniczonej liczbie prób. Gdy działa
ecoNET, występuje konflikt adresu lub nadawanie RS485 jest niedostępne,
powstaje **raport częściowy z nasłuchu**. Taki plik także można przesłać;
zawiera opis brakujących danych. Nie przełączaj połączeń tylko po to,
aby uzyskać napis o kompletnym raporcie.

Plik pomija hasła, ustawienia sieci, UID kotła i identyfikatory sprzętowe
modułów. Zawiera temperatury i nastawy, więc przed publicznym udostępnieniem
możesz go obejrzeć. Nic nie jest automatycznie wysyłane. Ostatni raport
pozostaje w RAM ESP do następnego raportu lub restartu; nie zapisuje się
go w pamięci flash.

**Raport pomaga diagnozować obsługę nowego wariantu; sam plik nie
odblokowuje poleceń.** Część nazw i poleceń może wymagać dalszych porównań z ekranem
regulatora oraz osobnego testu. Komplet odpowiedzi nie jest potwierdzeniem
poprawności wszystkich zapisów ani obsługi wszystkich modułów.

### Co oznaczają komunikaty?

| Komunikat | Znaczenie |
| --- | --- |
| Ten model kotła nie ma jeszcze profilu zapisu | Nazwa modelu różni się od zweryfikowanego profilu. Pobierz raport. |
| ESP jest przypisane do innego kotła | To ESP wcześniej zapamiętało identyfikator innego regulatora. Raport nie kasuje przypisania. |
| Brak zweryfikowanego katalogu RS485 | Odczyt katalogu jest niedostępny albo firmware/struktura nie odpowiada zweryfikowanemu profilowi. Raport pomoże ustalić przyczynę. |

Nowa, czysta instalacja nie zawiera przypisania do kotła autora. Zgodny
regulator jest zapamiętywany przy pierwszej poprawnej identyfikacji.
Zmiana modelu lub wersji oprogramowania nie oznacza automatycznie zgodności
kodów nastaw.

## Sterowniki: Lista I Status

**Posiadanie ecoNET nie jest potwierdzeniem pelnej zgodnosci tej bety.**
To niezalezny panel, nie kopia calego oprogramowania ecoNET. Lokalny
modul HTTP pomaga ujednolicic odczyty, ale wersje API, nazwy pol,
indeksy nastaw, termostaty i uprawnienia moga sie roznic.

| Regulator | Status w tym projekcie |
| --- | --- |
| ecoMAX860P-LZ TOUCH, modul A 8.41.38.L0, panel 8.30.28 | Profil referencyjny, uzywany w instalacji prywatnej; nie kazdy zapis przetestowany |
| ecoMAX860P4-LZ | Oznaczenie sprzetowe modulu w tej samej instalacji referencyjnej, nie osobny drugi test |
| ecoMAX810P-L TOUCH, moduł A 10.40.186.R5, panel 10.14.50 | Raport testowy: 169 nastaw; od beta.42 automatyczny katalog i standardowe START/STOP. Własne nazwy i menu pobierane z kotła. Zapis wymaga testu u właściciela |
| ecoMAX860P3-O, moduł A 16.33.91.P1, panel 16.11.10 | Od beta.59 poprawka dużego katalogu: 198 pozycji, 274 elementy drzewa. Odczyt schematu potwierdzony raportem i testami; zapis wymaga potwierdzenia u właściciela |
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
Specjalne nastawy pokojowe nadal zależą od mapy danego modelu.
Standardowe START/STOP rozpoznaje rodzinę ecoMAX i kontroluje bieżący stan.
Nie należy podmieniać identyfikatora modelu.

**Bezpośredni RS485:** od beta.42 standardowe sterowniki ecoMAX-P
pobierają własny katalog menu 1.0, niezależnie od numeru firmware.
Niepełne lub niezgodne dane nie udostępniają zapisu nastaw.
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
