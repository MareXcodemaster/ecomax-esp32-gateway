# Aktualizacje ESP Z GitHuba

Od **0.7.0-beta.3** publiczny panel zawiera opcjonalny aktualizator
w **Ustawienia urządzenia → Aktualizacje ESP z GitHuba**.

## Stabilne I Beta

- Repozytorium: **MareXcodemaster/ecomax-esp32-gateway**.
- Od **0.7.0-beta.5** wybierz **Stabilne i beta** (domyslnie) albo
  **Tylko stabilne**. Wybor jest zapamietywany w ESP.
- Beta ma oznaczenie **BETA** przy numerze i ostrzezenie przy instalacji.
  To wydanie testowe, ktore moze zawierac bledy.
- Szkice wydan sa zawsze pomijane. W kanale laczonym sprawdzane jest
  piec ostatnio opublikowanych wydan; wybierana jest najwyzsza zgodna wersja.
- Automatyczne sprawdzanie raz na dobę jest domyślnie włączone.
  Możesz je wyłączyć; ustawienie jest zapamiętywane.
- Po błędzie kolejna automatyczna próba sprawdzenia jest opóźniona do godziny.
- **Sprawdź teraz** odczytuje informacje o wydaniu, ale niczego nie instaluje.
- Komunikat o nowej wersji nie zmienia nastaw ani nie restartuje ESP.
  **Nie ma obowiązku aktualizacji.**

Brak stabilnego wydania jest prawidlowy, gdy projekt ma tylko bety.
Mozesz wtedy przelaczyc kanal; nie zmieniaj oznaczenia wydania na stabilne.

## Instalacja

Od **0.7.0-beta.8** przycisk jest w gornej belce panelu:
**Aktualne** oznacza udane sprawdzenie bez nowszego wydania w wybranym
kanale (wynik nie starszy niz 25 godzin). **Aktualizacja** oznacza
dostepne nowsze wydanie. **Aktualizacje** oznacza brak aktualnego
potwierdzenia, np. po bledzie sieci. Przycisk tylko otwiera ustawienia.
Oznaczenie **BETA** pozostaje widoczne przy wydaniu testowym.

Porownywany jest numer wydania. Numer technicznej kompilacji jest
pokazywany osobno, jezeli sie rozni. Nie omija to blokady migracji
prywatnego firmware do publicznego.

1. Sprawdź aktualną wersję i dostępność nowszej.
2. Naciśnij **Zaktualizuj** i potwierdź restart ESP.
3. Nie odłączaj zasilania ESP. Pobierany jest tylko **firmware-ota.bin**,
   nigdy obraz scalony, partycje ani NVS.
4. Po powrocie panelu sprawdź wersję i ponowny odczyt danych kotła.

Zwykłe, zgodne OTA zachowuje NVS. Kasowanie flash i pierwsza instalacja USB
są innymi operacjami. Jeżeli zniknie odpowiedź, najpierw sprawdź wersję.
Nie wysyłaj instalacji w pętli. Panel sam nie ponawia POST instalacji.

## Sprawdzanie Obrazu

- HTTPS z weryfikacją certyfikatu i nazwy serwera, bez wyłączania TLS.
- SHA-256 raportowany przez GitHuba dla dokładnie tego pliku.
  Brak poprawnego digestu blokuje propozycję.
- Numer wersji musi byc nowszy od obecnego. Beta nie zastapi nowszej
  stabilnej wersji o tym samym numerze glownym.
- Rozmiar metadanych, obrazu i czas operacji są ograniczone.
- Przekierowania dopuszczają tylko wskazane domeny GitHuba.
- ESP-IDF sprawdza obraz; program porównuje projekt i wersję przed
  wybraniem nowej partycji startowej. Aktywna aplikacja nie jest nadpisywana.
- Ręczne OTA i aktualizator współdzielą blokadę flash.
  Podczas instalacji nowe polecenia panelu są odrzucane.
- Pozostają testy startowe i mechanizm zatwierdzenia/rollback ESP-IDF.

SHA-256 z repozytorium nie zastępuje niezależnego podpisu firmware.
Trzeba ufać właścicielowi repozytorium i jego kontu GitHub. Mechanizm
nie oznacza certyfikacji sterowania kotłem ani zgodności z dowolną płytką.

## Internet I Prywatność

Sprawdzanie i pobieranie wymagają Internetu. Jego brak nie powinien
zatrzymywać lokalnego odbioru RS485/ecoNET ani sam powodować restartu.
Aktualizator pracuje w osobnym zadaniu o niskim priorytecie, nie w UART.

GitHub widzi połączenie i zwykły identyfikator programu, ale nie otrzymuje
SSID, haseł, UID, pomiarów ani nastaw. Przy braku prawidłowego czasu ESP
może użyć **pool.ntp.org**, aby zweryfikować certyfikat.

## Instalacje Prywatne I Ograniczenia

Starsze prywatne wydania 0.6.x maja inna inicjalizacje dostepu niz publiczne
0.7.x. Automatyczna migracja jest zablokowana i wymaga osobnej procedury:
zapisania konfiguracji w NVS, utworzenia i zabezpieczenia unikalnego hasla
oraz sprawdzenia zgodnosci partycji. Wersja przejsciowa jest przygotowywana
indywidualnie, nie jest publicznym obrazem pierwszej instalacji.
Nie obchodz blokady przez podmiane pliku lub numeru wersji.

Parser i panel są testowane syntetycznie. Pełne pobranie i uruchomienie
nowego stabilnego obrazu z GitHuba nie jest jeszcze potwierdzoną próbą
sprzętową, ponieważ nie ma stabilnego wydania. Pierwszy start publicznego
firmware na dodatkowej, wyczyszczonej płytce także nadal wymaga testu.

Dokumentacja użytych API:
[GitHub Releases](https://docs.github.com/en/rest/releases/releases),
[ESP HTTP Client](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_http_client.html),
[ESP-IDF OTA](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/ota.html).
