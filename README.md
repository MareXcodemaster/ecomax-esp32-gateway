# ecoMAX Wi-Fi - sterowanie kotłem z telefonu

Lokalny panel kotla ecoMAX na ESP32: **RS485 lub lokalny ecoNET**, telefon/tablet,
nastawy, ecoSTER, harmonogramy, alarmy i historia pomiarow z ostatnich 3 dni.
Bez konta w chmurze do codziennej pracy. Interfejs i instrukcje po polsku.

**Wersja 0.7.0-beta.2. Projekt niezalezny, nieoficjalny. Nie jest produktem
PLUM, HKS Lazar ani Waveshare. Nie jest uniwersalnym firmware dla wszystkich
kotlow.** Nie omija zabezpieczen ani nie zastepuje regulatora kotla.

## Zacznij Tutaj

**[Polska instrukcja ze zdjeciami: od USB do sterowania kotlem](docs/START_PL.md)**

![Instalator ecoMAX Wi-Fi - pierwszy krok](docs/images/01-instalator.png)

1. Przeczytaj [zgodnosc i ograniczenia](docs/ZGODNOSC.md).
2. Na komputerze otworz [instalator USB](https://marexcodemaster.github.io/ecomax-esp32-gateway/)
   w Chrome lub Edge. Podlacz plytke kablem USB z transmisja danych.
3. Potwierdz model **Waveshare ESP32-S3-RS485-CAN, 16 MB flash / 8 MB PSRAM**,
   nacisnij **Wgraj na ESP** i wybierz port. Inne plytki nie sa objete wydaniem.
4. Zamknij okno wgrywania, nacisnij **Polacz ESP przez USB** i wpisz wlasne
   Wi-Fi 2,4 GHz. Instalator odczyta haslo ESP i pokaze przycisk otwarcia panelu.
5. Dopiero potem, po bezpiecznym wylaczeniu zasilania kotla,
   wykonaj [polaczenie RS485](docs/PODLACZENIE.md).
6. Otworz `http://piec.local/` lub adres IP nadany ESP przez Twoj router.

Domyslna nazwe `piec.local` mozesz zmienic w zakladce **ESP Wi-Fi**,
np. na `kotlownia.local`. Zostaje zapamietana po restarcie.

Pierwsza instalacja wymaga komputera i kabla USB z transmisja danych.
Potem ESP dziala samodzielnie, a panel i OTA obsluzysz telefonem/tabletem.

Instalator nie wymaga Pythona ani edycji kodu. Wi-Fi mozna tez skonfigurowac
[telefonem przez siec ESP](docs/WIFI.md). Opcja zapasowa:
[paczka ZIP/BIN](https://github.com/MareXcodemaster/ecomax-esp32-gateway/releases)
i [instrukcja recznej instalacji](docs/INSTALACJA.md).
Instrukcje mozna tez otworzyc [bezposrednio przy instalatorze](https://marexcodemaster.github.io/ecomax-esp32-gateway/instrukcja.html).

## Co Zawiera

| Funkcja | Zakres |
| --- | --- |
| Dane biezace | Tylko rzeczywiscie dostepne temperatury, stany i wyjscia |
| Nastawy | Odczyt, zakres i uprawnienia sprawdzane przed zapisem |
| ecoSTER | Profile temperatur pokojowych i tryby, jesli sterownik je udostepnia |
| Harmonogramy | Tydzien pn-nd, kratki po 30 minut, opisy dzialania i edycja |
| Alarmy | Licznik powiadomien i odczytywana historia, nie obejscie alarmow |
| Historia | 3 dni w flash, probki minutowe; dokladniejszy krotki bufor RAM |
| Pellet | Dane sterownika, jesli wystepuja, i oddzielne obliczenia ESP |
| Polaczenia | RS485, HTTP do lokalnego ecoNET, tryb hybrydowy |
| OTA | Aktualizacja obrazu aplikacji przez panel diagnostyczny |

Szczegoly: [instrukcja panelu](docs/PANEL.md),
[bezpieczenstwo](SECURITY.md), [budowanie](docs/BUDOWANIE.md).

## Prywatnosc

- W firmware nie ma sieci Wi-Fi autora, hasel ecoNET, UID kotla, konta ecoNET24,
  historii pomiarow ani tokenow sesji.
- Haslo administratora i sieci konfiguracyjnej powstaje losowo na kazdej
  wyczyszczonej plytce. Odczytasz je lokalnie przez USB, nie z GitHuba.
- Wlasna siec, hasla i tozsamosc kotla pozostaja w NVS Twojego ESP.
- Katalog zawiera nazwy i metadane zgodnosci; wszystkie poczatkowe wartosci
  nastaw/pomiarow sa puste. Testy uzywaja sztucznych danych.
- Nie dodano telemetrii, analityki ani automatycznej transmisji do autora.
  Wybrany tryb ecoNET odpytuje wskazany przez Ciebie adres w sieci lokalnej.

Nie wystawiaj panelu bezposrednio do Internetu. HTTP nie zapewnia szyfrowania;
zdalny dostep powinien byc realizowany przez VPN na routerze/bramce.

## Stan Wydania

Wydanie opiera sie na funkcjach poprzedniej wersji prywatnej. Publiczne
zmiany pierwszego startu, generowania hasla i przypisania nowego kotla
wymagaja jeszcze proby na drugiej, fabrycznie wyczyszczonej plytce.
Testy programowe i kompilacja nie zastepuja tego testu sprzetowego.

Licencja kodu projektu: [MIT](LICENSE). Zaleznosci zachowuja wlasne
licencje i atrybucje: [THIRD_PARTY.md](THIRD_PARTY.md).
