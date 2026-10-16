# Zgodnosc I Ograniczenia Bety

## Profil Referencyjny

| Element | Profil |
| --- | --- |
| Plytka | Waveshare ESP32-S3-RS485-CAN |
| Pamiec | 16 MB flash, 8 MB PSRAM |
| Model regulatora | ecoMAX860P-LZ TOUCH / ecoMAX860P4-LZ |
| Modul A | 8.41.38.L0 |
| Panel kotla | 8.30.28 |
| Termostat | ecoSTER / eSTER x40, zaobserwowany uklad 1.21.38 |
| RS485 | 115200, 8N1, GPIO17/18/21 na wskazanej plytce |

To nie znaczy, ze wszystkie kotly z nazwa ecoMAX860P maja identyczny
protokol, katalog i mozliwosci zapisu. Wersje nazw, jednostek, enumow,
metadanych oraz struktury i uprawnien sa sprawdzane przed udostepnieniem
edycji. Nieznany wariant ma pozostac zablokowany, nie otrzymac zgadywanych
indeksow. Odczyt danych i mozliwosc zapisu to dwie osobne kwestie.

## Co Zostalo Sprawdzone

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
  z poprawnym zegarem. Ta wersja nie dodaje klienta NTP; bez wiarygodnej
  daty nie tworzy fikcyjnych wpisow archiwum.
- Po zaniku zasilania ostatnia minuta archiwum moze nie zostac zapisana.
- Obraz pierwszej instalacji i OTA to rozne pliki. Zmiana tablicy partycji
  nie jest zwykla aktualizacja OTA.
- Nie ma HTTPS, bezposredniego klienta Tailscale ani automatycznej chmury.
