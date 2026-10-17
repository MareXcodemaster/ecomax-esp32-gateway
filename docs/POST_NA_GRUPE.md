Dostepny jest bezplatny, niezalezny projekt **ecoMAX Wi-Fi - sterowanie kotlem z telefonu**.
To lokalny panel na ESP32, rozwijany jako alternatywa dla modulu ecoNET300,
bez koniecznosci korzystania z chmury i bez stale wlaczonego komputera.

W skrocie: podglad temperatur i pracy kotla, zmiana obslugiwanych nastaw
kotla/CWU/mieszacza/pokoju, tryby ecoSTER, edycja harmonogramow, alarmy,
historia do 3 dni i poziom pelletu, jesli sterownik go udostepnia.
Komunikacja bezposrednio po RS485; lokalny ecoNET jest opcjonalna droga.

Sprzet: Waveshare ESP32-S3-RS485-CAN lub CAN-U z antena zewnetrzna,
16 MB flash. Jest instalator przez przegladarke, wyszukiwanie Wi-Fi,
instrukcja po polsku ze zdjeciami i schematem oraz podglad panelu.
Po konfiguracji otwierasz piec.local na telefonie lub tablecie.
Aktualizacje sa dobrowolne.

**To bezplatna BETA, nie oficjalny produkt PLUM.** Sprawdzony zestaw
referencyjny to ecoMAX860P-LZ TOUCH / ecoMAX860P4-LZ z eSTER_x40.
Nie ma gwarancji dzialania wszystkich funkcji z kazdym kotlem; sprawdzcie
liste zgodnosci. Podlaczenie wewnatrz kotla zleccie osobie z kwalifikacjami.

Projekt, zdjecia i pobieranie:
https://github.com/MareXcodemaster/ecomax-esp32-gateway

Prosta instrukcja i instalator:
https://marexcodemaster.github.io/ecomax-esp32-gateway/szybki-start.html

Zgodnosc zalezy od modelu i wersji oprogramowania sterownika.
