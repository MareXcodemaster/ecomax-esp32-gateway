# Stan testów integracji Tailnet — próby beta.48–54

## Sprawdzone przed testem sprzętowym

- Kompilacja ESP32-S3, ESP-IDF 5.5.0, flash 16 MB / PSRAM 8 MB.
  Kod i zasoby beta.54: 2 194 979 B z limitu 3 145 728 B aktywnego slotu OTA;
  obraz aplikacji OTA: 2 195 376 B.
  Sekcje statycznego RAM-u: 62 704 B (beta.47: 60 416 B).
  Nie jest to pomiar wolnej pamięci podczas działania VPN.
- Testy C++: granice pamięci, składnia klucza, dokładny adres i port docelowy,
  krótkie/błędne pakiety, fragmenty, UDP/ICMP/IPv6 i obce adresy.
  Pełny dotychczasowy zestaw testów dekoderów i zapisu konfiguracji również przeszedł.
- Rzeczywisty kod C polityki MicroLink: brakujące i błędne reguły, zmiana klucza,
  odebranie uprawnień, rozłączenie i ponowna synchronizacja, odmowa alokacji,
  100 000 błędnych pakietów, 4000 publikacji reguł z trzema czytelnikami.
  Test odbioru WireGuard używa atrap kryptografii i interfejsu sieciowego.
- 193 testy JavaScript i 8 testów Python: zakończone powodzeniem.
- Przeglądarka: konfiguracja Tailnet przy 390 i 1280 px, domyślne wyłączenie,
  zachowanie wpisywanego tekstu podczas odświeżania, czyszczenie pola klucza
  po zapisie, brak nieaktualnego linku, błędny zapis i anulowanie zmian.
- Istniejące testy galerii, pamięci ESP i nawigacji na telefonie i komputerze.
  Testy korzystały wyłącznie z danych demonstracyjnych, bez nastaw kotła lub pompy.
- Skan prywatności źródeł i zasobów z prywatną listą zabronionych wartości.

## Dotychczasowy test sprzętowy (07.11.2026)

- Beta.48: uruchomienie z VPN wyłączonym, odczyty kotła, Tapo i odtworzenie archiwum działały. Zapis konfiguracji VPN wymagał poprawienia rozpoznawania lokalnego IPv4 w gnieździe IPv6.
- Beta.49: rzeczywisty zapis konfiguracji zwraca HTTP 200. Klucz pozostaje zapisany po OTA, API go nie ujawnia. Pierwszy start VPN zatrzymał się na rezerwie RAM-u, bez resetu ESP.
- Beta.50: pula WireGuard w PSRAM; próba po zakończeniu sprawdzania aktualizacji. Najniższy zaobserwowany zapas wewnętrznego RAM-u: 54 167 B. Po trzech minutach bez gotowego połączenia VPN zatrzymany, pamięć odzyskana. Panel i Tapo pozostawały dostępne.
- Beta.51: 63/63 testy HTTP po starcie, TLS/Noise oraz rejestracja przechodzą. Urządzenie ma wpis w konsoli Tailscale, ale nie utrzymuje połączenia. Próba wraca z etapu pobierania mapy sieci. Najniższy odczytany zapas RAM-u: 51 251 B.
- W sieci testowej jest dziewięć urządzeń; limit czterech slotów metadanych może blokować przyjęcie całej listy. Beta.52 podnosi spójnie limity do 16 i dodaje kody odrzucenia mapy. Testy hosta rzeczywistego kodu parsowania/zarządzania przeszły przy 16 peerach, zmianach kluczy, usuwaniu, błędach kolejki i alokacji. Transport i NVS są atrapami w tych testach.
- Beta.52: rejestracja i mapa uprawnień przyjęte, pięć peerów, status Connected w konsoli Tailscale. Próba HTTP z telefonu nie otworzyła strony.
- Beta.53: po poprawieniu obsługi dopełnienia WireGuard użytkownik potwierdził częściowe ładowanie strony przez Internet komórkowy. ESP przyjęło 152 pakiety panelu, filtr nie odrzucił żadnego. VPN zatrzymał się na progu pamięci: minimum obserwowane przez zarządcę 40 839 B, minimum globalne od startu 19 259 B. Panel lokalny pozostał dostępny, bez restartu i bez przepełnienia UART w tej próbie.
- Beta.54: konfiguracja preferuje PSRAM dla buforów Wi-Fi/lwIP, bez zmniejszania stosów lub progów ochronnych. Po OTA zakończono 63/63 testy HTTP. VPN połączył się z zachowaną tożsamością i pięcioma peerami. Zapas wewnętrznego RAM-u po połączeniu około 82 kB.
- Beta.54: trzy rundy równoległego pobierania zasobów panelu przez LAN przy włączonym VPN: 135 odpowiedzi HTTP 200, 597 282 B, cztery równoległe połączenia. VPN pozostał połączony, brak restartu i przepełnień UART. To test obciążenia serwera, nie zastępuje pobrania całego panelu przez tunel z telefonu.
- Beta.54: pojedyncze sprawdzenie wydań GitHub podczas aktywnego VPN zakończyło się odpowiedzią limitu API, bez zerwania VPN. Minimum globalne wewnętrznego RAM-u od startu po tej próbie: 55 311 B. Nie pobierano firmware przez VPN. Odczyt Tapo pozostawał świeży, błędów logowania 0.
- Beta.54: użytkownik potwierdził pełne załadowanie panelu i bieżących temperatur przez Tailscale na telefonie bez Wi-Fi. Kontrola po potwierdzeniu: 1541 przyjętych pakietów panelu, 0 odrzuconych przez filtr aplikacji; połączenie nadal aktywne, 82 079 B wolnego wewnętrznego RAM-u. Czas pracy ESP 479 s, bez restartu, Tapo świeże i 0 błędów logowania. Licznik przepełnień UART wzrósł do 1 przy 2491 poprawnych ramkach; ta krótka próba nie potwierdza bezstratnego odbioru ani stabilności wielogodzinnej.
- W próbach występowały pojedyncze przepełnienia UART; nie deklarujemy braku utraconych ramek ani stabilności długoterminowej. Nastaw kotła nie zmieniano.

## Do potwierdzenia na sprzęcie

1. Osobne potwierdzenie ścieżki bezpośredniej i DERP; brak uprawnień drugiego klienta i odebranie
   dostępu aktywnemu klientowi. Sam test portu 80 z hosta nie wystarcza.
2. Rozłączenie Wi-Fi, brak Internetu, utrata serwera koordynacji, wyłączenie VPN,
   restart ESP i trwałość tożsamości. Reguły dostępu nie mogą być pomijane.
3. Rzeczywiste zużycie i minimalny zapas RAM-u/stosów, przepełnienia UART oraz
   dłuższa praca równocześnie z historią i Tapo. Co najmniej doba próby przed
   rekomendowaniem włączenia na instalacji użytkownika.

Pełna lista prób powyżej nie została zakończona. Wersja jest kandydatem eksperymentalnym,
nie potwierdzeniem stabilnego klienta VPN. Nie wykonano zmian nastaw kotła
w celu przygotowania integracji.
