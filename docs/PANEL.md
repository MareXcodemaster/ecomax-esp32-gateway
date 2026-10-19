# Panel I Harmonogramy

## Codzienna Obsluga

Gorny pasek: **Kafelki** wlacza edycje ukladu (przycisk zmienia sie na
**Gotowe**), **Odswiez** pobiera aktualne dane. **Narzedzia** rozwija liste:
Zmiana sieci Wi-Fi, ustawienia ESP Wi-Fi, polaczenie ecoNET,
sprawdzenie API i katalogu, raport JSON, diagnostyka/OTA i przywrocenie
ukladu podczas jego edycji. **Wyloguj** jest na koncu listy.
Menu mozna zamknac kliknieciem poza nim lub Escape; obsluguje tez Tab
i strzalki klawiatury. Samo otwarcie menu niczego nie wysyla do kotla.
W diagnostyce przycisk **Wroc do panelu** jest w stale widocznym naglowku.
Dziala tez z okna logowania i wraca do tego samego ESP, bez zmiany ustawien.
Lewa sekcja **Diagnostyka lokalna** jest domyslnie zwinieta. Nacisnij jej
naglowek ze strzalka, aby pokazac lub schowac wszystkie pozycje ponizej.
Przycisk **Aktualizacja** pojawia sie w tym samym gornym pasku, gdy dostepne
jest nowsze wydanie. Oznaczenie BETA pozostaje widoczne; nacisniecie otwiera
ustawienia aktualizacji, a nie rozpoczyna instalacji.
Po udanym sprawdzeniu bez nowszego wydania ma nazwe **Aktualne**.
Przy braku aktualnego potwierdzenia pokazuje neutralne **Aktualizacje**.

Kolejnosc kafelkow w Danych biezacych mozesz zmienic przyciskiem
**Edytuj uklad** (symbol siatki). Przeciagnij uchwyt wybranego kafelka
myszka lub przytrzymaj go palcem. Uchwyt obsluguje tez strzalki klawiatury.
Przycisk powrotu przywraca domyslny uklad. Kolejnosc zapisuje sie tylko
w tej przegladarce, osobno na kazdym telefonie/tablecie. Usuniecie danych
przegladarki usuwa wlasny uklad; nie zmienia to zadnych nastaw kotla.

Zakladka Pellet pokazuje graficzny poziom i procent podawany przez kociol.
Brak aktualnego odczytu jest oznaczony znakiem zapytania, nie zerem.
To nie jest niezalezny czujnik ani pomiar masy pelletu.

Panel otwierasz z telefonu/tabletu w tej samej sieci co ESP. Po zalogowaniu
wybierasz transport odczytu/zapisu. Brak aktualnego odczytu ma blokowac
edycje, a nie pokazywac zapamietana wartosc jako aktualna.

Od beta.17 panel odpytuje ESP co 2 sekundy rowniez w karcie dzialajacej
w tle. Powrot do strony dodatkowo pobiera aktualny stan, bez resetowania ESP.
Przegladarka lub system telefonu moze ograniczac prace w tle albo usypiac
cala aplikacje; panel nie omija tych ograniczen. ESP samodzielnie odczytuje
kociol takze wtedy, gdy strona jest zamknieta. Podczas zapisu nie naklada
odczytow na trwajaca operacje; po jej zakonczeniu odswiezanie wraca.

Zapis to przygotowanie polecenia, jedna wysylka i kontrola odpowiedzi/odczytu.
Samo ACK nie dowodzi, ze zadana wartosc zostala zastosowana. Niepotwierdzone
polecenie nie jest automatycznie powtarzane ani automatycznie cofane.
Zmiany wykonane na kotle/termostacie lub przez ecoNET moga zmieniac nastawy
niezaleznie od otwartej strony.

Od beta.16 zwykle nastawy zapisujesz jedna operacja. Zmiana wartosci na
innym urzadzeniu nie blokuje wpisanej nowej nastawy; ESP odczytuje aktualna
wartosc i sprawdza zakres przed wyslaniem. Ostatnie polecenie przyjete przez
kociol wyznacza nowa nastawe, ktora pojawia sie na pozostalych panelach po
odswiezeniu odczytu. Nie oznacza to natychmiastowej zmiany temperatury wody.
Limity, blokady serwisowe i sprawdzanie odpowiedzi kotla pozostaja aktywne.
Harmonogram calego tygodnia nadal wymaga zgodnej podstawy, aby nie zastapic
niezauwazenie planu zmienionego rownolegle przez druga osobe.

Piec kresek na gornej belce dotyczy Wi-Fi ESP, nie modulu ecoNET ani kotla.
Procent jest szacunkiem na podstawie RSSI (-100 dBm to 0%, -50 dBm to 100%),
nie pomiarem predkosci lacza. Kazda kreska odpowiada kolejnym 20 punktom
procentowym. Podpowiedz zawiera dokladne dBm; klikniecie otwiera okno Wi-Fi.
Brak aktualnego odczytu pokazuje --%, nie zapamietana sile jako aktualna.

## JSON Do Analizy Pracy Kotla

Otworz **Narzedzia > Pobierz analize spalania JSON** albo przycisk
**JSON do analizy** w Historii danych. Wybierz 1 godzine, 6 godzin,
24 godziny lub ostatnie 3 dni i nacisnij **Pobierz JSON**.

Raport zawiera wszystkie dostepne zapisane probki, bez dodatkowego
usredniania do wykresu: temperatury zmierzone i zadane, moc, nadmuch,
prace podajnika i pomp, stany kotla, poziom i strumien paliwa oraz tlen,
jesli sterownik je udostepnia. Zapis flash jest minutowy, a nowszy bufor
RAM ma nominalnie probki co 10 sekund i przejscia stanow. Nie oznacza
to, ze dostepny jest pomiar zuzycia paliwa albo kazdy krotki impuls podajnika.

Plik zawiera czas UTC, jednostki, braki jako null, przerwy jako gapBefore
i informacje o stanie archiwum. Osobno sa aktualne nastawy z poczatku
i konca pobierania. Nie wolno uznawac ich za nastawe obowiazujaca przez
cale 3 dni. Zamknieta przegladarka nie zatrzymuje archiwum pomiarow ESP.

Od beta.18 ESP prowadzi tez osobny, trwaly rejestr nastaw do 3 dni.
Zapisuje dostepne nastawy temperatur oraz aktualnie odczytany katalog
menu RS485, w tym udostepnione nastawy spalania. Nie wykonuje dodatkowych
komend zmiany ani nie zapisuje zapamietanych wartosci jako swiezego odczytu.
Rejestr dziala bez telefonu i zachowuje zapisane rekordy po restarcie ESP.
Zbieranie zaczyna sie dopiero po aktualizacji; starszych ustawien nie odtworzy.

W JSON sekcja **settings_history** zawiera:

- **baseline**: komplet dostepnych wartosci na poczatku nowej godziny lub po restarcie;
- **changes**: tylko zmienione wartosci, a null oznacza utrate dostepnego odczytu;
- **observed**: potwierdzenie kolejnego odczytu, okolo co 10 minut bez zmian.

Czas oznacza zaobserwowanie nastawy przez ESP, nie dokladny moment zmiany
na kotle, termostacie czy innym panelu. Poczatkowy rekord moze poprzedzac
wybrany zakres, aby mozna bylo odtworzyc znany stan na jego poczatku.
Braki, opoznienia i bledy rejestru sa sygnalizowane w quality oraz warnings.
Nastawy nieudostepnione przez sterownik nie pojawia sie w historii.

Pobieranie jest odczytem, bez komend zmiany nastaw. Dane skladane sa
z ograniczonych porcji w przegladarce. Przerwane pobieranie lub niezgodny
format nie tworzy pliku z pozornie kompletna historia. Raport nie zawiera
hasel, tokenow sesji, SSID, adresow sieciowych ani UID. Same pomiary
i nastawy nadal opisuja prywatna instalacje; udostepniaj je swiadomie.

Taki plik pomaga w analizie pracy, ale nie wyznacza automatycznie idealnych
nastaw i nie zastepuje pomiaru analizatorem spalin.

## Wi-Fi Bez Diagnostyki

**Narzedzia > Zmien siec Wi-Fi** otwiera formularz bez opuszczania panelu.
Wyszukaj sieci, wybierz swoje Wi-Fi 2,4 GHz i wpisz haslo. Dla sieci
ukrytej mozesz wpisac nazwe recznie. Zapis zmienia siec ESP, nie nastawy kotla.
Po zmianie przelacz telefon na nowa siec i otworz ponownie adres ESP.
Siec awaryjna Piec-Setup pozostaje dostepna, takze przy blednym hasle routera.
Otwarcie okna nie zapisuje niczego; skan wymaga nacisniecia Wyszukaj sieci.

**Narzedzia > Ustawienia ESP Wi-Fi** pokazuje SSID, IP, sygnal i formularz
zmiany nazwy `piec.local`. Nie jest to strona sieci modulu ecoNET.

## Odczyt Ramek RS485

W **Narzedzia > Diagnostyka/OTA > RS485** tabela pokazuje nazwy i stan
rozpoznania odebranych ramek. **Dane** otwiera odswiezany podglad temperatur,
czasu, identyfikacji lub wersji modulow. **Czesciowo rozpoznane** oznacza,
ze nie wszystkie pola maja potwierdzone znaczenie; nie oznacza alarmu kotla.
Szczegoly HEX pozostaja dostepne. [Opis ramek i ograniczen](DIAGNOSTYKA.md).

## Schemat

Graficzny podglad kotla, mieszacza 1, pokoju i CWU pokazuje temperatury
zmierzone oraz aktualne cele sterownika. Zielona pompa oznacza prace,
szara postoj, kreski brak aktualnego odczytu. Widok jest pogladowy;
nie zastapi dokumentacji rzeczywistej instalacji ani schematu podlaczenia.

## Hasla Dostepu

Domyslnie panel: **admin / admin**, Piec-Setup: **piecwifi**.
W **Ustawienia urzadzenia > Hasla dostepu** wpisz obecne haslo panelu,
nowe haslo i jego powtorzenie. Pole Piec-Setup mozesz zostawic puste,
jesli nie zmieniasz hasla tej sieci. Zapis restartuje tylko ESP.
Nastawy kotla i domowe Wi-Fi nie sa zmieniane. Kolejne OTA zachowuja
wlasne hasla; nie ma generowania losowych hasel.

## Co Robi Ktory Harmonogram

| Harmonogram | Do czego sluzy |
| --- | --- |
| Zbiornik CWU | Obniza zadana temperature wody w bojlerze do kranow/kapieli w wybranych godzinach. Nie dotyczy temperatury pokoju. |
| Termostat ecoSTER 1 | Przelacza cel pokojowy miedzy komfortowym/dziennym i ekonomicznym/nocnym. Wymaga trybu Harmonogram. |
| Mieszacz 1-4 | Obniza temperature wody plynacej na grzejniki/podlogowke, nie bezposrednio temperature pokoju. |
| Kociol | Obniza zadana temperature wody w samym kotle. |
| Praca kotla | Wyznacza godziny dozwolonej pracy i przerwy. Praca dozwolona nie oznacza ciaglego palenia. |
| Pompa cyrkulacyjna | Wyznacza godziny cyrkulacji, dzieki ktorej ciepla woda szybciej dociera do kranow, jesli taka pompa jest zamontowana. |
| Czyszczenie kotla/wymiennika | Wyznacza dozwolone godziny automatycznego czyszczenia, jesli sterownik obsluguje mechanizm. |

Jedna kratka to 30 minut; wiersze to dni pn-nd. Znaczenie kolorow zalezy
od wybranego planu i jest wyswietlane nad tabela. Zmiany zaczynaja
obowiazywac dopiero po Zapisz. Anuluj zmiany usuwa lokalny projekt, nie
zmienia planu w kotle.

### CWU - Przyklad

Przykladowa nastawa 45 C i obnizenie 5 C:

- turkusowe godziny: cel 45 C;
- szare godziny: cel 40 C, a nie calkowite wylaczenie CWU.

Jesli zmierzona woda ma 38 C, pozostaje zmierzona 38 C. Cel 40/45 C to
temperatura, do ktorej sterownik ma dogrzewac. Nie jest to ta sama rzecz
co histereza. Inne ograniczenia/tryby regulatora nadal obowiazuja.

### ecoSTER - Przyklad

Przykladowo dzienna 22 C, nocna 20 C, turkusowe kratki 06:00-22:00:
w tych godzinach cel pokojowy jest 22 C, poza nimi 20 C. W trybie staly
Komfortowy/Ekonomiczny, Party lub Wakacje plan moze nie decydowac o celu.
Nocna nazwa nie wymusza nizszej wartosci: obie nastawy sa osobne.

Temperatury: Parametry uzytkownika > ecoSTER T1 > Temperatura zadana
dzienna/nocna. CWU: Parametry uzytkownika > Ustawienia CWU > Temperatura
zadana CWU. To przyklady, nie odczyt Twoich ustawien.

## Historia I Alarmy

Czerwony okragly licznik przy **Alarmy** pokazuje biezace zgloszenia
sterownika, nie liczbe zapisow w historii. Zero lub nieaktualny odczyt
ukrywa licznik. Jest odswiezany razem z danymi panelu.

Archiwum jest w ESP, nie w pamieci telefonu. Retencja wynosi 3 dni,
probkowanie dlugiego archiwum raz na minute, wykres ma ograniczona liczbe
punktow. Przerwy w danych nie sa wypelniane sztucznymi zerami.
Po pierwszym uruchomieniu kilka dni zapelni sie dopiero z czasem.

Alarmy to odczyt informacji przekazanych przez regulator. Skasowanie
historii lub ukrycie komunikatu nie usuwa przyczyny alarmu i nie powinno
byc uzywane do obchodzenia zabezpieczen. Ocen stan samego kotla.
