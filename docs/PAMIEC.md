# Pamięć i odświeżanie panelu

Od beta.47 panel oddziela definicje menu od aktualnych odczytów. Przy pierwszym
otwarciu pobiera oba zestawy. Kolejne odświeżenia zawierają temperatury, nastawy,
stan kotła, dostępność zapisu i połączeń. Zmiany wykonane na ekranie kotła lub
z innego urządzenia nadal pojawiają się automatycznie. Przełączenie zakładki
przeglądarki nie zatrzymuje odpytywania.

Definicje są przechowywane tylko w pamięci danej strony. Nowy katalog lub restart
ESP powoduje pobranie aktualnych definicji. Eksporty JSON zawierają pełne dane.
Na przykładowym rzeczywistym odczycie z 180 pozycjami menu odpowiedź okresowa
zmalała z około 185 kB do 49 kB. Rozmiar zależy od sterownika i dostępnych danych.

## Diagnostyka ramek

Na stronie **/diagnostics → RS485** przycisk
**Rozszerzone przechwytywanie na 10 minut** zwiększa bufor surowych rekordów
z 256 do 2048. Każdy zajmuje 1024 bajty. Po 10 minutach ESP wraca do zwykłego
bufora, zachowując najnowsze rekordy. Można zakończyć rozszerzony tryb wcześniej.
Brak wolnej PSRAM uniemożliwia rozszerzenie bufora, ale nie wyłącza dekodowania.

Ten bufor jest niezależny od trzydniowej historii temperatur i nastaw zapisanej
w pamięci flash. Raport zgodności należy pobrać w ciągu 15 minut od jego utworzenia.

## Ustawienia ESP

Pule **RAM wewnętrzny**, **PSRAM**, **firmware** i **archiwum** są mierzone osobno.
PSRAM służy większym danym; nie zastępuje pamięci wewnętrznej potrzebnej sterownikom
Wi-Fi, przerwaniom i zadaniom systemowym.

W szczegółach pokazujemy również najmniejszy odnotowany zapas stosu uruchomionych
zadań. Zadania aktualizatora i konsoli USB pojawiają się podczas ich używania.
Aktualizator sprawdza dostępność wydań zgodnie z konfiguracją; instalacja nadal
wymaga przycisku użytkownika. Konsola USB startuje po podłączeniu komputera.

Najmniejsza ilość wolnej pamięci obejmuje cały czas od restartu, również testy
startowe i chwilowe operacje HTTPS. Do porównania wersji warto sprawdzić zarówno
bieżącą wolną pamięć, jak i największy wolny blok przy tych samych funkcjach
i liczbie otwartych paneli.
