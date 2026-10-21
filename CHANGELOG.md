# Changelog

## 0.7.0-beta.23 - 2026-10-21

- Dolna belka Parametrow serwisowych z przyciskiem Ustawienia zaawansowane
  i oznaczeniem menu 8888. U gory pozostaje kompaktowy skrot z tarcza.
- Opis odroznia kod fizycznego menu kotla od hasla logowania do ESP.
  Nie dodano nowego hasla ani zadnej zmiany nastaw czy komend RS485.
- Test obu drog nawigacji, dopasowania belki na telefonie i braku zapisu
  przy samym otwieraniu menu; nowy zrzut ekranu i aktualne instrukcje.

## 0.7.0-beta.22 - 2026-10-21

- Poprawka interpretacji poziomu paliwa na ecoMAX860P-LZ TOUCH,
  modul A 8.41.38.L0 / panel 8.30.28: kod powyzej 100 oznacza
  Kalibracja (CAL), a nie procenty pomniejszone o 101.
- Kalibracja jest widoczna na kafelku, w zakladce Pellet i na schemacie.
  Nie jest prezentowana jako pusty zasobnik ani zapisywana jako 0%
  w nowych probkach historii. Nie zmieniono juz zapisanych probek.
- Surowy kod i osobny stan kalibracji sa dostepne w odczytach JSON.
  Brak danych i nieaktualny odczyt nie sa uznawane za aktywna kalibracje.
- Zachowanie dotychczasowego kodowania przesunietego o 101 dla innych,
  nierozpoznanych wersji. Bez zmian polecen kalibracji i nastaw kotla.
- Testy dekodera, widokow 320/390/1280 px, zwyklego 0%, 100%, CAL
  i utraty odczytu; zaktualizowane instrukcje i galeria.

## 0.7.0-beta.21 - 2026-10-21

- Parametry serwisowe: osobna sekcja Ustawienia zaawansowane z dziewiecioma
  nastawami rozpoznanymi dla ecoMAX860P-LZ TOUCH / 8.41.38.L0.
  Dodatkowe potwierdzenie jest sprawdzane zarowno przy przygotowaniu,
  jak i wykonaniu zapisu. Inne menu producenta pozostaja zablokowane.
- Pellet: punkty kalibracji 100% i 0%, rozpoznane w pasywnym zapisie
  polecen fizycznego panelu 8.30.28 i odpowiedzi regulatora.
  Osobne potwierdzenie rzeczywistego poziomu, identyfikacja kotla,
  kontrola konfiguracji, pojedyncza wysylka i wynik ACK bez ponawiania.
  ACK nie oznacza ukonczenia ani poprawnosci calej kalibracji.
- Kasowanie alarmow i licznikow nadal nie jest udostepnione: obserwacja
  nacisniecia przy pustej liscie nie pozwolila ustalic polecenia.
  Harmonogram Cisza nocna nie ma jeszcze potwierdzonego mapowania.
- Testy protokolu i potwierdzen, widoki telefonu/komputera,
  zaktualizowana instrukcja i galeria. Bez automatycznych zmian nastaw.

## 0.7.0-beta.20 - 2026-10-21

- Odczyt ecoSTER przed zapisem rozroznia nieprawidlowe dane od opoznionej
  zgodnej ramki czujnikow. Przy samym opoznieniu czeka do 2 sekund,
  a przed dalszym krokiem ponownie odczytuje parametry termostatu.
  Normalny, zgodny odczyt nie ma dodatkowego oczekiwania.
- Znikanie starego bledu przygotowania nastawy dopiero po nowym, pelnym
  odczycie. Panel nie odtwarza go przy ponownym otwarciu strony, jezeli
  polaczenie i nastawa sa juz aktualne. Nieudany zapis nie jest ponawiany.
- Osobne oznaczenie rozpoczecia wysylki. Ostrzezenia po probie wyslania,
  przy nieznanym wyniku, niezgodnym modelu i braku lacznosci pozostaja widoczne.
- Zachowanie pierwotnej przyczyny bledu RS485 zamiast ogolnego komunikatu.
  Testy opoznionych danych, brakow, zmiany trybu i odzyskania odczytu.

## 0.7.0-beta.19 - 2026-10-20

- Ustawienia urzadzenia: rzeczywiste zajecie programu w aktywnym slocie OTA,
  wewnetrznej sterty RAM, sterty PSRAM oraz pamieci historii we flash.
  Wartosci zajete/wolne, procenty i osobne wskazniki dla kazdej puli.
- Parametry ESP: model i rewizja, rdzenie i taktowanie CPU, fizyczna pojemnosc
  flash, wersja ESP-IDF, czas od uruchomienia i przyczyna ostatniego restartu.
  Minimalna wolna pamiec i najwieksze wolne bloki pomagaja ocenic zapas RAM.
- Odczyt korzysta z dotychczasowego odswiezania panelu, bez nowych zapytan
  RS485. Dziala takze bez polaczenia z kotlem; utrata ESP usuwa stare wskazania.
- Nowe testy danych i widoku telefonu/komputera oraz aktualna galeria DEMO.
  Bez zmian nastaw kotla, konfiguracji Wi-Fi i formatu archiwum pomiarow.

## 0.7.0-beta.18 - 2026-10-19

- Pobieranie JSON do analizy pracy kotla z Narzedzi oraz Historii danych.
  Zakres 1/6/24/72 godziny, wszystkie dostepne zapisane probki bez redukcji do wykresu.
- Eksport uwzglednia temperatury, nastawy temperatur, moc, nadmuch, prace podajnika
  i pomp, stany kotla oraz dostepne dane pelletu i sondy lambda.
- Osobny trwaly rejestr odczytanych nastaw: poczatkowy stan, zmiany i potwierdzenie
  odczytu co 10 minut, autonomicznie bez otwartego panelu. Retencja do 3 dni.
  Obejmuje dostepne nastawy temperatur i odczytany katalog menu RS485, takze nastawy spalania.
  Zbieranie zaczyna sie po aktualizacji; nie odtwarza starszych ustawien.
- Jednostki, czas UTC, jawne braki danych/przerwy i stan archiwum. Czas zmiany nastawy
  oznacza jej zaobserwowanie przez ESP, nie dokladna chwile wydania komendy.
- Dane pobierane sa godzinami i skladane w przegladarce. Mozliwe anulowanie;
  bledna lub przerwana odpowiedz nie powoduje zapisania niepelnego pliku jako poprawnego.
- Eksport analityczny nie zawiera hasel, sieci, tokenow ani UID i nie wysyla polecen do kotla.
- Format istniejacego archiwum flash i zapisane w nim pomiary pozostaja bez zmian.

## 0.7.0-beta.17 - 2026-10-18

- Panel odpytuje ESP co 2 sekundy takze w karcie przegladarki dzialajacej w tle.
  Usunieto programowe zatrzymywanie odczytow zalezne od widocznosci strony.
- Powrot do karty lub okna wywoluje dodatkowy odczyt; nie resetuje ESP.
  Nie nakladaja sie rownolegle zapytania i pozostaje automatyczne wznowienie po bledzie odczytu.
- System telefonu lub przegladarka nadal moze usypiac aplikacje i ograniczac zegary w tle.
  ESP niezaleznie komunikuje sie z kotlem, takze bez otwartego panelu.
- Bez zmian nastaw kotla, schematu, tla, konfiguracji Wi-Fi i regul zapisu.

## 0.7.0-beta.16 - 2026-10-18

- Zwykle nastawy sa zapisywane jedna operacja po aktualnym odczycie kotla.
  Stara wartosc w formularzu nie blokuje zapisu po zmianie z innego urzadzenia.
  Limity, uprawnienia, identyfikacja i potwierdzenie odczytem pozostaja aktywne.
- Wynik zakonczonej operacji pozostaje dostepny, gdy drugi klient zaczyna kolejna.
  Panel czeka na jawnie zajete ESP, ale nie ponawia zapisu po utracie odpowiedzi.
- Piec kresek i szacowany procent sygnalu ESP na gornej belce; dBm w podpowiedzi.
- Wi-Fi zeruje licznik kolejnych prob po uzyskaniu IP, szybciej wznawia polaczenie
  i obsluguje utrate adresu IP bez restartu ESP. Ukryte karty nie odpytuja panelu.
- Brak zmian schematu, tla, nastaw kotla i danych zapisanych w NVS.

## 0.7.0-beta.15 - 2026-10-17

- Zwijana sekcja Diagnostyka lokalna w lewym menu; domyslnie zamknieta.
- Strzalka rozwija wszystkie pozycje ponizej: Pola danych, Katalog nastaw, API, Testy zapisu oraz adres urzadzenia.
- Obsluga myszka, dotykiem i klawiatura przez natywny element details; bez nowych zapytan do ESP i kotla.
- Pozostale menu, schemat, tlo i dzialanie diagnostyki z beta.14 bez zmian.

## 0.7.0-beta.14 - 2026-10-17

- Diagnostyka RS485: nazwy i opisy typow 10, 106, 107, 137, 185, 192, 234 i 235.
- Czytelny podglad czasu, modelu i temperatur z ramki ecoSTER, identyfikacji kotla oraz wersji i dat kompilacji modulow.
- Osobne oznaczenie Czesciowo rozpoznane: brakujace znaczenie pol pozostaje jawne, surowe dane sa zachowane w szczegolach.
- Otwarty podglad odswieza sie wraz z tabela, bez nowych zapytan RS485; zamkniecie zatrzymuje pobieranie podgladu.
- Testy granic, dat, wersji struktur i blednych ramek oraz widoku telefonu i komputera. Bez zmian schematu, tla i polecen zapisu.

## 0.7.0-beta.13 - 2026-10-17

- Widoczny przycisk Wroc do panelu w naglowku diagnostyki, dostepny rowniez po przewinieciu strony.
- Powrot jest tez dostepny w oknie logowania do diagnostyki; nie wymaga logowania ani historii przegladarki.
- Przycisk wraca do strony glownej tego samego ESP, takze po IP, wlasnej nazwie .local i przez siec Piec-Setup.

## 0.7.0-beta.12 - 2026-10-17

- Przywrocono dokladne poprzednie pliki schematu i jasnego tla, zamiast nowych grafik z beta.10.
- Schemat zachowuje pionowa kolumne wskaznikow po lewej, poprzedni kociol, grzejnik, mieszacz i CWU.
- Bez zmian geometrii i kolorow grafiki. Aktualne pomiary i stany pomp nadal sa odswiezane, stare odczyty czyszczone.
- Zachowano poprawki wyszukiwania aktualizacji z beta.11, Narzedzia i zmiane Wi-Fi.
- Testy kontroluja sumy SHA-256 obu przywroconych plikow, aby nie zastapic ich ponownie innymi grafikami.

## 0.7.0-beta.11 - 2026-10-17

- Aktualizator przeglada kolejne strony wydan GitHuba, zamiast tylko pierwszych pieciu pozycji.
- Wybiera najwyzsza zgodna wersje niezaleznie od kolejnosci listy; niepelny odczyt nie pokazuje Aktualne.
- Limity stron, pamieci i czasu; zachowane TLS, SHA-256, kontrola projektu oraz brak samoczynnej instalacji.
- Zmiana kanalu Stabilne i beta / Tylko stabilne zapisuje sie od razu, bez przelaczania automatycznych sprawdzen.

## 0.7.0-beta.10 - 2026-10-17

- Graficzny schemat kotla, mieszacza i CWU z aktualnymi temperaturami i stanem pomp.
- Wlasne fotograficzne tlo panelu, bez logo producenta kotla.
- Narzedzia: bezposrednia zmiana Wi-Fi z wyszukiwaniem sieci oraz ustawienia ESP Wi-Fi.
- Zmiana sieci i skanowanie zablokowane podczas operacji panelu lub aktualizacji.
- Instrukcja BOOT przy pustej pamieci, montazu DIN i doboru zewnetrznej anteny SMA.
- Bez zmian polecen RS485 i nastaw kotla. Testy poprawki nie zmieniaja ustawien urzadzenia.

## 0.7.0-beta.9 - 2026-10-17

- Domyslny panel admin / admin; siec Piec-Setup z haslem piecwifi.
- Bez losowego generowania hasel. Migracja poprzednich hasel automatycznych.
- Wlasne hasla panelu i Piec-Setup w Ustawieniach urzadzenia, trwale w NVS.
- Wymagane obecne haslo, token panelu i potwierdzenie restartu przy zmianie.
- Polaczenie ecoNET przeniesione do Narzedzi; Wyloguj pozostaje ostatnie.
- Ikona walca zamiast klosa przy Pellet.
- Czerwony okragly licznik aktywnych alarmow w menu, z kontrola aktualnosci.

## 0.7.0-beta.8 - 2026-10-17

- Aktualizacja przeniesiona do wspolnego gornego paska, bez osobnego wiersza.
- Aktualne dopiero po udanym sprawdzeniu; brak polaczenia nie udaje aktualnosci.
- Porownanie numeru wydania, oddzielnie od technicznego numeru kompilacji.
- Widoczne oznaczenie BETA i numer dostepnej wersji w podpowiedzi.
- Na telefonie czytelny uklad przyciskow w dwoch kolumnach.
- Przycisk nadal tylko otwiera ustawienia; instalacja wymaga potwierdzenia.

## 0.7.0-beta.7 - 2026-10-17

- Podpisane szybkie akcje Kafelki i Odswiez oraz rozwijane Narzedzia.
- Nazwy funkcji zamiast szeregu anonimowych ikon; Wyloguj na koncu listy.
- Obsluga Escape, klawiatury, zamykania poza menu i ekranow od 320 px.
- Dziewiaty zrzut w galerii i instrukcja uporzadkowanego paska.
- Bez zmian komunikacji, nastaw kotla, logowania i zachowania wylogowania.

## 0.7.0-beta.6 - 2026-10-17

- Przywrocona zielona kolorystyka panelu zgodnie z wyborem uzytkownika.
- Zasobnik pelletu wysrodkowany dokladnie nad procentem w kafelku.
- Test geometrii kafelka na komputerze i telefonie.
- Przycisk edycji ukladu w glownym pasku, mniej pustych wierszy nad kafelkami.
- Pozostale funkcje wersji beta.5 bez zmian.

## 0.7.0-beta.5 - 2026-10-17

- Zmienna kolejnosc kafelkow: mysz, dotyk i klawiatura, lokalny zapis ukladu.
- Graficzny poziom pelletu i procent; usuniete liczniki ESP i formularz wazony.
- Aktualizator pokazuje takze BETA, z wyborem kanalu i recznym potwierdzeniem.
- Porownywanie numerow beta, walidacja metadanych, digest i TLS zachowane.
- Fioletowy naglowek i lokalna czcionka Lato; osiem aktualnych zdjec panelu.
- Brak zmian w protokole i nastawach kotla.

## 0.7.0-beta.4 - 2026-10-17

- Wyszukaj sieci: lista Wi-Fi 2,4 GHz w instalatorze USB i konfiguracji
  przez telefon; reczne wpisanie nazwy zostaje dla sieci ukrytych.
- Siec konfiguracyjna Piec-Setup-XXXX, neutralne nazwy panelu i diagnostyki.
- Prosta instrukcja w 7 krokach, tabela regulatorow i termostatow ze statusem testow.

- Doprecyzowane dwa warianty sprzetu: ESP32-S3-RS485-CAN i -CAN-U.
- Ten sam firmware; wyjasnienie anteny SMA w -U i sprzetowej zmiany toru
  RF w wersji bez -U. Instalator wymienia oba modele.
- Galeria rzeczywistego panelu z szescioma zrzutami na sztucznych danych.
- Zdjecia obu modeli producenta, schemat G4 dla profilu referencyjnego.
- Poprawiony opis "Cel teraz" w kafelkach: tekst pod wartoscia, nie w ikonie.

## 0.7.0-beta.3 - 2026-10-17

- Optional stable-only GitHub checks, daily or manually, with a saved opt-out.
- Explicit owner confirmation before OTA; no mandatory or unattended installation.
- TLS, GitHub SHA-256, bounded transfers and image/project/version validation.
- Shared OTA flash lock and rejected new panel commands during installation.
- Private-to-public automatic migration remains blocked.
- Polish guide revision 3, update instructions and corrected transport selector.
- Stable-image GitHub installation remains unverified: no stable release exists yet.

## 0.7.0-beta.2 - 2026-10-16

- Clear Polish project title; repository address unchanged.
- Illustrated Polish installation and Wi-Fi guide, wiring diagram and offline guide bundle.
- Removed the two requested empty pellet tiles: flow and ESP-counted consumption.
- Default piec.local address with a persistent, editable hostname in ESP Wi-Fi.
- No changes to bus protocol, boiler settings or write validation.
- Removed public financial-support placeholders from the README and release description.

## 0.7.0-beta.1 - 2026-10-16

First public beta, based on the private 0.6.31 feature set.

- Wi-Fi, ecoNET credentials, controller UID, settings values and recordings
  removed from the published sources and firmware.
- Unique 128-bit device password generated on first boot, saved in NVS and
  retrieved over physical USB; no shared administrator password in firmware.
- First supported controller identity is enrolled from an actual response;
  later identity changes still block writes. Firmware/catalogue guards remain.
- Independent, locally stored UI assets: Lucide icons and an installation
  readings view replace manufacturer logos, images, font and hydraulic SVG.
- User/service settings, room thermostat profiles, weekly half-hour schedules,
  alarm history and three-day measurement archive retained.
- USB installation bundle, OTA image, privacy checks and connection guide.

The new first-boot provisioning and identity enrolment have NOT yet been
verified by flashing a second, factory-erased physical board. Do not interpret
this beta as certified or universally compatible boiler-control firmware.
