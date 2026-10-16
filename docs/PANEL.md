# Panel I Harmonogramy

## Codzienna Obsluga

Panel otwierasz z telefonu/tabletu w tej samej sieci co ESP. Po zalogowaniu
wybierasz transport odczytu/zapisu. Brak aktualnego odczytu ma blokowac
edycje, a nie pokazywac zapamietana wartosc jako aktualna.

Zapis to przygotowanie polecenia, jedna wysylka i kontrola odpowiedzi/odczytu.
Samo ACK nie dowodzi, ze zadana wartosc zostala zastosowana. Niepotwierdzone
polecenie nie jest automatycznie powtarzane ani automatycznie cofane.
Zmiany wykonane na kotle/termostacie lub przez ecoNET moga zmieniac nastawy
niezaleznie od otwartej strony.

## Co Robi Ktory Harmonogram

| Harmonogram | Do czego sluzy |
| --- | --- |
| Zbiornik CWU | Obniza zadana temperature wody w bojlerze do kranow/kapieli w wybranych godzinach. Nie dotyczy temperatury pokoju. |
| Termostat ecoSTER 1 | Przelacza cel pokojowy miedzy komfortowym/dziennym i ekonomicznym/nocnym. Wymaga trybu Harmonogram. |
| Mieszacz 1-4 | Obniza temperature wody plynacej na grzejniki/podlogowke, nie bezposrednio temperature pokoju. |
| Kociol | Obniza zadana temperature wody w samym kotle. |
| Praca kotla | Wyznacza godziny dozwolonej pracy i przerwy. Praca dozwolona nie oznacza ciaglego palenia. |
| Pompa cyrkulacyjna | Wyznacza godziny cyrkulacji, dzieki ktorej ciepla woda szybciej dociera do kranow, jesli taka pompa jest zamontowana. |
| Czyszczenie kotla/wymiennika | Wyznacza dozwolone godziny automatycznego czyszczenia, jesli sterownik obsluguje mechanizm. |

Jedna kratka to 30 minut; wiersze to dni pn-nd. Znaczenie kolorow zalezy
od wybranego planu i jest wyswietlane nad tabela. Zmiany zaczynaja
obowiazywac dopiero po Zapisz. Anuluj zmiany usuwa lokalny projekt, nie
zmienia planu w kotle.

### CWU - Przyklad

Przykladowa nastawa 45 C i obnizenie 5 C:

- turkusowe godziny: cel 45 C;
- szare godziny: cel 40 C, a nie calkowite wylaczenie CWU.

Jesli zmierzona woda ma 38 C, pozostaje zmierzona 38 C. Cel 40/45 C to
temperatura, do ktorej sterownik ma dogrzewac. Nie jest to ta sama rzecz
co histereza. Inne ograniczenia/tryby regulatora nadal obowiazuja.

### ecoSTER - Przyklad

Przykladowo dzienna 22 C, nocna 20 C, turkusowe kratki 06:00-22:00:
w tych godzinach cel pokojowy jest 22 C, poza nimi 20 C. W trybie staly
Komfortowy/Ekonomiczny, Party lub Wakacje plan moze nie decydowac o celu.
Nocna nazwa nie wymusza nizszej wartosci: obie nastawy sa osobne.

Temperatury: Parametry uzytkownika > ecoSTER T1 > Temperatura zadana
dzienna/nocna. CWU: Parametry uzytkownika > Ustawienia CWU > Temperatura
zadana CWU. To przyklady, nie odczyt Twoich ustawien.

## Historia I Alarmy

Archiwum jest w ESP, nie w pamieci telefonu. Retencja wynosi 3 dni,
probkowanie dlugiego archiwum raz na minute, wykres ma ograniczona liczbe
punktow. Przerwy w danych nie sa wypelniane sztucznymi zerami.
Po pierwszym uruchomieniu kilka dni zapelni sie dopiero z czasem.

Alarmy to odczyt informacji przekazanych przez regulator. Skasowanie
historii lub ukrycie komunikatu nie usuwa przyczyny alarmu i nie powinno
byc uzywane do obchodzenia zabezpieczen. Ocen stan samego kotla.
