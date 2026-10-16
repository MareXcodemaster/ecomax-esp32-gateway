# Budowanie I Wydawanie

Wymagany Python 3.10+; Node.js 22 do testow interfejsu/logiki.

```text
python -m pip install -r requirements-build.txt -r requirements.txt
node --test tests/*.cjs
python -m unittest discover -s tests -p "test_*.py"
pio run -e waveshare_s3
python tools/package_release.py
pnpm install --frozen-lockfile --ignore-scripts
pnpm build
pnpm exec playwright install chromium
pnpm test:browser
```

Na Windows, jesli w sciezce sa spacje, uzyj `./Build.ps1` po instalacji
PlatformIO w jego standardowym katalogu. Skrypt uzywa dwoch wolnych
liter dyskow i usuwa swoje mapowania na koniec. Nie mapuje COM ani nie
wgrywa niczego do urzadzenia.

Publiczny build nie czyta zmiennych srodowiska z prywatnymi danymi Wi-Fi
lub ecoNET. Te dane konfiguruje sie na konkretnym urzadzeniu, nigdy w kodzie.
Nie uzywaj starego `sdkconfig.waveshare_s3` ani starych katalogow `.pio`.

`tools/package_release.py` tworzy obrazy z nowych plikow kompilacji:
bootloader, tablica partycji, poczatkowy selektor OTA i aplikacja. Nie
korzysta z odczytu fizycznej pamieci flash. Sprawdza puste NVS w obrazie
scalonym, wersje aplikacji i sumy plikow. Uruchamia tez audyt prywatnosci.

`dist/` i `artifacts/` sa lokalnymi wynikami. Nie sa historiami Git.
ZIP instalacyjny ma allow-liste plikow; `.git`, `.pio`, managed_components,
zrzuty, raporty z rzeczywistych urzadzen i dane prywatne sa wykluczone.

Przed publikacja:

1. Wykonaj testy i czysty build.
2. Sprawdz audyt z prywatna lista zakazanych wartosci poza repozytorium,
   jesli przygotowujesz wydanie z wczesniejszej instalacji prywatnej.
3. Zweryfikuj zawartosc ZIP i SHA256SUMS, nie tylko pliki tekstowe.
4. Sprawdz start na wyczyszczonej, osobnej plytce, pobranie hasla przez USB,
   konfiguracje wlasnego Wi-Fi i poprawna blokade obcego modelu/UID.
5. Dopiero po wynikach testow decyduj o stabilnym wydaniu. Beta pozostaje
   oznaczona jako prerelease; nie usuwaj ostrzezenia o braku testu sprzetowego.

Zaleznosci pobieraja sie z przypietych wersji PlatformIO/ESP-IDF/component
managera. Nie dolaczamy zmodyfikowanych binariow bibliotek spoza ich
oficjalnych zrodel. Licencje sa w LICENSES i THIRD_PARTY.md.

Instalator: zrodla w installer/, wynik site/ (poza Git). ESP Web Tools
10.4.0 odpowiada za programowanie, a adapter USB JSON za lokalny formularz
Wi-Fi. Firmware nie deklaruje obslugi Improv. Bundle nie laduje CDN.
pnpm-lock.yaml przypina zaleznosci. Lokalny podglad: pnpm dev; wymaga
localhost albo HTTPS dla Web Serial. pnpm build sprawdza SHA256 obrazu.
W GitHub Settings > Pages wybierz Source: GitHub Actions. Workflow najpierw
sprawdza testy i build, potem publikuje instalator przez GitHub Pages.
Testy przegladarkowe symuluja USB, nie zastepuja proby na fizycznym ESP.
