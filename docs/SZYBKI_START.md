# ecoMAX Wi-Fi: Prosta Instrukcja Instalacji

**Wersja programu 0.7.0-beta.9. Instrukcja 5, 17.10.2026.**
Nie musisz programowac ani wpisywac komend. Komputer jest potrzebny
tylko przy pierwszym wgraniu; pozniej korzystasz z telefonu.

## 1. Sprawdz, Czy To Zestaw Dla Ciebie

Potrzebujesz modulu **Waveshare ESP32-S3-RS485-CAN** lub
**ESP32-S3-RS485-CAN-U**, z pamiecia **16 MB flash i 8 MB PSRAM**.
Wariant **-U** ma antene zewnetrzna. Potrzebne sa tez kabel USB-C
z transmisja danych, komputer z Chrome lub Edge i domowe Wi-Fi 2,4 GHz.

**Najpierw sprawdz [liste regulatorow i termostatow](ZGODNOSC.md).**
Nie kazdy kociol z ecoNET jest jeszcze obslugiwany przez ten program.
To nadal beta, nie uniwersalny zamiennik dla kazdego kotla.

[Zdjecia modulu: co kupic i gdzie sa zaciski](SPRZET.md).

## 2. Podlacz Sam Modul Do Komputera

Na poczatek ESP ma byc **bez przewodow RS485 do kotla**.
Podlacz je kablem USB-C do komputera. Jesli masz wersje -U, przed
wlaczeniem podlacz jej antene Wi-Fi 2,4 GHz.

**Nie otwieraj pracujacego kotla.** Odlaczenie lub podlaczenie przewodow
wewnatrz obudowy powierz instalatorowi; sa tam obwody 230 V.

## 3. Kliknij Wgraj Na ESP

Otworz **[instalator ecoMAX Wi-Fi](https://marexcodemaster.github.io/ecomax-esp32-gateway/)**
na komputerze w Chrome albo Edge.

1. Zaznacz potwierdzenie wlasciwego modelu modulu.
2. Kliknij **Wgraj na ESP**.
3. Wybierz port USB swojego modulu i kliknij **Polacz / Connect**.
4. W oknie wgrywania wybierz **Install** i potwierdz zastapienie programu.
5. Poczekaj do konca. **Nie odlaczaj kabla.** Potem zamknij okno wgrywania.

![Instalator: potwierdzenie plytki i przycisk wgrywania](images/01-instalator.png)

Wgranie usuwa poprzedni program i ustawienia ESP. Nie uzywaj tego
przycisku tylko po to, zeby zmienic Wi-Fi dzialajacego juz modulu.

<a id="boot-usb"></a>

### Puste ESP: USB Znika Albo Laczenie Trwa Bez Konca

Po wyczyszczeniu pamieci ESP moze ciagle restartowac sie, a port USB
pojawia sie i znika lub instalator nie konczy laczenia. Wtedy:

1. **Odlacz USB oraz inne zasilanie samego ESP.** Modul nadal ma byc bez RS485 do kotla.
2. **Przytrzymaj przycisk BOOT na urzadzeniu** - nie RESET.
3. **Nie puszczajac BOOT, podlacz kabel USB do komputera.**
4. Po 2-3 sekundach zwolnij BOOT. W instalatorze ponownie wybierz port i rozpocznij wgrywanie.

![Puste ESP: odlacz zasilanie, trzymaj BOOT, podlacz USB, po 2-3 sekundach pusc BOOT](images/boot-usb.svg)

To tryb wgrywania; samo przytrzymanie BOOT nie kasuje pamieci. Nie trzeba
trzymac go przez caly zapis. **Podczas wgrywania nie odlaczaj USB.**
Jesli przycisk jest zakryty obudowa ESP, dostep do niego przygotuj dopiero
po odlaczeniu calego zasilania. Nie otwieraj w tym celu pracujacego kotla.
[Gdzie jest BOOT: zdjecie producenta i oznaczenia 13/14](SPRZET.md#boot-usb).

Zrodlo: [Waveshare: BOOT przy ponownym podaniu zasilania](https://docs.waveshare.net/ESP32-S3-RS485-CAN/Firmware-Flashing/).

## 4. Wpisz Swoje Wi-Fi

Na tej samej stronie przejdz do **Polacz swoje Wi-Fi**.

1. Kliknij **Polacz ESP przez USB** i jeszcze raz wybierz ten sam modul.
2. Kliknij **Wyszukaj sieci** i poczekaj kilka sekund.
3. Wybierz swoje Wi-Fi z listy. Wpisz tylko jego haslo.
4. Kliknij **Zapisz i polacz**. Poczekaj, az pojawi sie adres panelu.

![Formularz: nazwa Twojej sieci i haslo domowego Wi-Fi](images/02-siec-wifi.png)

Nazwa na zdjeciu jest przykladem; **wybierz wlasne Wi-Fi**, nie przepisuj
przykladowej nazwy. Siec ukryta: z listy wybierz wpisanie nazwy recznie.

## 5. Otworz Panel

Kliknij **Otworz panel ESP**. Domyslny login: **admin**, haslo: **admin**.
Nie ma losowego generowania hasel. W **Ustawienia urzadzenia > Hasla dostepu**
mozesz ustawic wlasne haslo panelu oraz sieci Piec-Setup. Aktualizacja je zachowa.
Nie udostepniaj panelu bezposrednio w Internecie.

Na telefonie polacz sie z **tym samym domowym Wi-Fi**, a potem otworz:

**[http://piec.local/](http://piec.local/)**

Gdy ta nazwa nie dziala, uzyj adresu IP pokazanego przez instalator.
Adres mozesz dodac do zakladek lub ekranu glownego telefonu.
W **ESP Wi-Fi** mozesz pozniej zmienic nazwe `piec.local` na wlasna.

## 6. Instalator Podlacza Kociol

Po sprawdzeniu Wi-Fi i zgodnosci instalator moze podlaczyc magistrale
przy **wylaczonym i bezpiecznie odizolowanym zasilaniu kotla**.

![Polaczenie RS485 do G4 tylko dla potwierdzonej wersji regulatora](images/polaczenie-rs485.svg)

W potwierdzonej wersji ecoMAX860P-LZ TOUCH / ecoMAX860P4-LZ chodzi
o magistrale **G4: D+ i D-**, nie G1/M-BUS. Waveshare **A+ do D+**,
**B- do D-**, jedna skrecona para przewodow. Rysunek nie pokazuje
numerow pinow wtyczki i nie dotyczy automatycznie innych regulatorow.

**[Pelny opis podlaczenia i zasad wyboru trybu](PODLACZENIE.md)**.
Nie podlaczaj dwoch aktywnych urzadzen pod adresem RS485 0x56.

## 7. Sprawdz Temperatury Na Telefonie

Poczekaj na polaczenie i odczyty. Porownaj temperatury z ekranem kotla.
Samo dzialajace Wi-Fi nie oznacza jeszcze polaczenia z regulatorem.
Nie zmieniaj parametrow serwisowych, ktorych znaczenia nie znasz.

Gotowe: komputer nie jest juz potrzebny. ESP pamieta Wi-Fi po restarcie.
Nie wystawiaj panelu bezposrednio do Internetu.

## Gdy Cos Nie Dziala

| Problem | Najpierw sprawdz |
| --- | --- |
| Puste ESP, USB znika lub laczy sie bez konca | Odlacz cale zasilanie ESP, przytrzymaj BOOT, podlacz USB, po 2-3 s pusc BOOT i ponownie wybierz port. Rysunek powyzej |
| Nie widac portu USB | Kabel z transmisja danych, inny port USB i powyzsza procedura BOOT |
| Wgrywanie nie rusza | Zamknij inne programy USB. Wykonaj powyzsza procedure; alternatywnie trzymaj BOOT, nacisnij i zwolnij RESET, potem zwolnij BOOT |
| Nie laczy sie Wi-Fi | Nazwa, haslo, 2,4 GHz i zasieg |
| Nie otwiera piec.local | Ten sam domowy router; sprobuj IP pokazanego przez instalator |
| Panel dziala, ale temperatur brak | Tryb RS485/ecoNET, zgodnosc regulatora i polaczenie wykonane przez instalatora |

Wiecej: [pelna instrukcja ze zdjeciami](START_PL.md),
[jak wyglada panel](GALERIA.md), [zdjecia sprzetu](SPRZET.md).
Awaryjna konfiguracja telefonem uzywa sieci **Piec-Setup-XXXX**
z domyslnym haslem **piecwifi**; opis jest w pelnej instrukcji.
