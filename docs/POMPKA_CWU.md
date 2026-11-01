# Pompka Cyrkulacyjna CWU Z Tapo P110

Funkcja zewnetrznej cyrkulacji wody do kranow. Nie steruje pompa ladujaca
bojler, nie emuluje modulu C na magistrali PLUM i nie odblokowuje jego
pozostalych funkcji. Pierwsze wydanie jest testowe, domyslnie nieaktywne.

## Polaczenie

1. Skonfiguruj P110 w aplikacji Tapo i podlacz go do tej samej sieci LAN
   co ESP. Nie podlaczaj pompki do wyjscia kotla i gniazdka jednoczesnie.
   Sposob zasilania i obciazalnosc powinien potwierdzic instalator.
2. W panelu otworz **Pompka CWU > Polaczenie Tapo P110 > Wyszukaj P110**.
   Wybierz model, IP i MAC swojego gniazdka. Mozna tez wpisac lokalny IP.
3. Od beta.30 ESP automatycznie wykrywa **TPAP** i tworzy lokalna,
   szyfrowana sesje z potwierdzeniem znajomosci hasla po obu stronach.
   Przy dzialajacym TPAP nie trzeba wlaczac zgodnosci aplikacji innych firm.
   Starszy wariant KLAP v2 pozostaje obslugiwany, gdy gniazdko go udostepnia.
   Dostepnosc KLAP moze zalezec od opcji zgodnosci w Tapo; zmieniasz ja
   swiadomie samodzielnie. [Wyjasnienie producenta](https://www.tapo.com/uk/faqs/frequently-asked-questions-about-the-third-party-compatibility-feature/).
4. Wpisz dane konta Tapo tylko w lokalnym panelu, przez zaufana siec.
   Polaczenie z panelem jest HTTP, nie HTTPS. Haslo nie jest zwracane,
   eksportowane ani zapisywane w przegladarce przez aplikacje. ESP zapisuje
   lokalny odcisk uwierzytelniania KLAP i poswiadczenie TPAP password-shadow,
   nie haslo do konta; te odciski nadal
   jest tajnym poswiadczeniem, a pamiec NVS nie jest szyfrowana.
5. **Polacz lokalnie** wykonuje odczyt, bez zmiany stanu gniazdka.
   Sprawdz nazwe i model przed zapisaniem trybu sterowania.

W TPAP pole e-mail jest opcjonalne; do KLAP potrzebny jest tez e-mail
konta. Po aktualizacji z beta.29 wpisz haslo jeden raz ponownie:
stary odcisk KLAP nie pozwala odtworzyc poswiadczenia TPAP. Nastepne
polaczenia i restarty nie wymagaja wpisywania hasla. Nie przesylaj go
autorowi projektu ani na GitHub.

Jesli gniazdko odrzuci haslo lub nie potwierdzi jego znajomosci,
ESP wstrzymuje kolejne proby logowania do czasu zapisania poprawnych
danych. Blokada jest zachowywana po restarcie. Nie ponawiaj logowania
wielokrotnie przy blednym hasle; P110 ma wlasne ograniczenie prob.

ESP zapamietuje MAC i skrot identyfikatora odczytanego po zalogowaniu.
Po utracie lacznosci wyszukuje ten sam MAC w swojej podsieci. Przed
kolejnym przelaczeniem ponownie sprawdza model, MAC i identyfikator.
Zmiana IP nie powinna wymagac ponownego parowania; izolacja klientow
Wi-Fi i rozne VLAN-y moga blokowac wyszukiwanie. Rezerwacja DHCP jest
dodatkowym, przydatnym zabezpieczeniem przed zmiana adresu.

## Temperatury I Tryby

- **Automatycznie**: wlaczenie po osiagnieciu gornego progu CWU,
  wylaczenie po spadku do dolnego. Miedzy progami stan zapotrzebowania
  pozostaje zachowany. Przyklad: wlacz od 45 C, wylacz przy 40 C.
- Po starcie w srodku przedzialu ESP czeka na gorny prog zamiast wlaczac
  pompke na podstawie starego stanu. Progi sa ustawiane niezaleznie od
  temperatury zadanej CWU w kotle; zmiana nie zapisuje niczego do kotla.
- **Wlaczona recznie**: wymuszenie pracy z pominieciem harmonogramu
  i progow, ale nadal wymaga aktualnego, poprawnego czujnika CWU.
- **Wylaczona**: polecenie wylaczenia gniazdka. Samo zapisanie pliku
  konfiguracji nie oznacza potwierdzonego przelaczenia.
- Przerwa po potwierdzonym wylaczeniu wynosi co najmniej 30 sekund,
  zanim ESP pozwoli ponownie wlaczyc pompke. Wylaczenie nie jest opozniane.
- Przed kazdym poleceniem i po nim odczytywany jest stan P110.
  Powtorzenie tego samego zadania HTTP nie zapisuje drugi raz konfiguracji.

## Harmonogram

Opcja **Uzywaj harmonogramu** jest niezalezna od zapisanej siatki tygodnia.
Wylaczona: automatyka temperatury dziala przez cala dobe.
Wlaczona: automatyka moze uruchamiac pompke tylko w zaznaczonych polach.
Nie wymusza to ciaglej pracy; temperatura nadal musi na nia pozwalac.

Siatka jest od poniedzialku do niedzieli, w odcinkach po 30 minut.
Mozna malowac prostokat obejmujacy kilka dni i godzin. Pusta siatka
z wlaczonym harmonogramem oznacza brak godzin dozwolonej pracy.
Wylaczenie harmonogramu nie kasuje siatki. Zapis dotyczy tylko ESP.

Czas lokalny: Europe/Warsaw, z automatyczna zmiana lato/zima. ESP moze
otrzymac zegar z otwartego panelu; po restarcie probuje tez odczytac czas
z powiazanego P110. Jesli zaden zegar nie jest prawidlowy, harmonogram
nie uruchamia pompki. Po utracie zasilania wszystkich urzadzen i przy
braku internetu moze byc potrzebne otwarcie panelu, aby ustawic czas.

## Ograniczenia

- Wylacz kolidujace harmonogramy, tryb poza domem i automatyczne
  wylaczenie w aplikacji Tapo, jezeli zrodlem sterowania ma byc ESP.
  Oprogramowanie nie kasuje automatycznie Twoich ustawien w Tapo.
- Przy braku poprawnej temperatury CWU ESP zleca wylaczenie. Jezeli
  jednak P110 nie odpowiada, ESP nie moze fizycznie go wylaczyc:
  wyswietla **Stan nieznany**, a gniazdko moze pozostac wlaczone.
- Utrata zasilania ESP sama w sobie nie odcina zasilania P110.
  Ta funkcja nie jest zabezpieczeniem kotla i nie zastepuje instalacji
  elektrycznej ani zabezpieczen hydraulicznych producenta.
- Konto Tapo i urzadzenie musza zostac wczesniej skonfigurowane w aplikacji.
  Pozniejsze sterowanie odbywa sie bez chmury, ale wymaga dzialajacego LAN.
- Testy automatyki, kryptografii i formularza sa lokalne i syntetyczne.
  Rzeczywisty P110 nalezy sparowac i sprawdzic przed pozostawieniem
  automatyki bez nadzoru. Samo ACK nie jest potwierdzeniem pracy pompki.
