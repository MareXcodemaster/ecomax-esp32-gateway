# Ramki RS485

Od beta.14: **Narzedzia > Diagnostyka/OTA > RS485 > Dane**.
Typ jest podany dziesietnie; HEX w szczegolach jest zapisem szesnastkowym.
Przyklady testowe i zrzuty ekranu nie sa pomiarami konkretnego kotla.

| Typ | Rozpoznana zawartosc | Ograniczenia |
| --- | --- | --- |
| 10 / 0x0A | Puste zapytanie z kotla | W obserwowanym ruchu poprzedza odpowiedz panelu z czasem. Dokladna funkcja niepotwierdzona. |
| 106 / 0x6A | Panel do ecoISM (0x59); piec bajtow powtarzanych w odpowiedzi | Znaczenie bajtow i dalszych pol niepotwierdzone. |
| 234 / 0xEA | Odpowiedz ecoISM do panelu | Zachowuje powtarzane bajty i pozostale nieopisane dane. |
| 107 / 0x6B | Model i zaobserwowana wersja kotla, przesylane do ecoISM | Pozostale pola nieopisane; nie sluzy do zapisu nastaw. |
| 235 / 0xEB | Dane zwrotne ecoISM | Brak potwierdzonego znaczenia liczb. Nie sa przedstawiane jako temperatury lub nastawy. |
| 137 / 0x89 | Znacznik TIME, czas; w wariancie ecoSTER takze model oraz temperatura zadana i zmierzona | Inne pola i warianty pozostaja nieopisane. Czas pochodzi z urzadzenia, bez przeliczania strefy czasowej i ustawiania zegara. |
| 185 / 0xB9 | Model, UID, typ i ID produktu oraz ID grafik | UID identyfikuje urzadzenie; nie publikuj swojego raportu bez sprawdzenia jego zawartosci. |
| 192 / 0xC0 | Wersje modulow i struktur, daty kompilacji, jesli przeslane | Znane struktury 0-5; nieznana struktura nie jest zgadywana. |

**Odczytane** oznacza rozpoznany format, **Czesciowo rozpoznane** zawiera
znane dane i nieopisane pola, a **Inny format** sygnalizuje niezgodnosc
z obslugiwanym formatem. Nie dopisujemy nazw do nieznanych wartosci.
Stan rozpoznania nie jest stanem alarmowym kotla.

Podglad odswieza tylko zapisane ramki w ESP, maksymalnie co okolo 3 sekundy.
Nie uruchamia nowych zapytan ani zapisow na RS485. Po zamknieciu podgladu
ustaje dodatkowy odczyt jego szczegolow. Stan i temperatury glownego panelu
korzystaja nadal z dotychczasowych zrodel; nowa diagnostyka ich nie zastepuje.

Zrodla formatu: [PyPlumIO](https://github.com/denpamusic/PyPlumIO),
[analizator ecoSTER](https://github.com/twkrol/econetanalyze/blob/master/ecoster.py),
[kod referencyjny ecoNET](https://gist.github.com/denpamusic/6375c2a57716dc42861350f0eebc1e68)
(m.in. adres ecoISM i struktury wersji). Rozpoznanie wymiany 106/234 i 107/235
opiera sie takze na obserwacji ruchu; nie jest kompletna dokumentacja protokolu.
