# Pierwsze Polaczenie Wi-Fi

Przy pierwszym uruchomieniu podaj swoja domowa siec Wi-Fi 2,4 GHz.

## Najprosciej: Formularz W Instalatorze

Po wgraniu firmware zamknij okno wgrywania i wybierz Polacz ESP przez USB.
Instalator odczyta haslo dostepu do ESP. Nacisnij Wyszukaj sieci, wybierz
swoja siec 2,4 GHz z listy i wpisz tylko haslo. Nacisnij Zapisz i polacz
i poczekaj na przycisk Otworz panel ESP. Dla sieci ukrytej wybierz wpisanie nazwy recznie.
Nie trzeba instalowac dodatkowego programu ani wpisywac polecen.
Tego samego formularza mozna uzyc pozniej bez ponownego wgrywania.
Hasla nie sa zapisywane w stronie, historii adresow ani wysylane przez Internet.

## Konfiguracja Telefonem

1. Zainstaluj firmware, zgodnie
   z [instrukcja instalacji](INSTALACJA.md).
2. Polacz telefon z siecia `Piec-Setup-XXXX` utworzona przez Twoja plytke.
   `XXXX` wynika z adresu sprzetowego. Domyslne haslo tej sieci: `piecwifi`.
3. Telefon moze ostrzegac o braku Internetu. Pozostan w tej sieci.
4. Otworz w przegladarce **http://192.168.4.1/**. Nie wpisuj HTTPS.
5. Login: `admin`. Haslo domyslne panelu: `admin`.
6. Wybierz **Narzedzia > Zmien siec Wi-Fi**, nacisnij **Wyszukaj sieci**,
   wybierz wlasna siec z listy, wpisz haslo i nacisnij **Zapisz i polacz**.
   Wymagane jest Wi-Fi 2,4 GHz; 5 GHz nie jest obslugiwane.
7. Sprawdz status i IP w **Narzedzia > Ustawienia ESP Wi-Fi**. ESP zapisuje pare
   SSID/haslo w NVS i laczy sie ponownie po restarcie.
8. Przelacz telefon na swoja domowa siec i wejdz na `http://piec.local/`
   albo IP odczytane z diagnostyki/routera. Ten adres IP nie jest na sztywno.

Gdy mDNS nie dziala w danym telefonie/routerze/VPN, uzyj adresu IP.
Rezerwacja DHCP w routerze ulatwia utrzymanie stalego IP. Siec goscinna
z izolacja klientow moze blokowac dostep telefonu do ESP.

Nieudana proba polaczenia nie wylacza sieci konfiguracyjnej. Mozesz wrocic
do niej i poprawic dane. Nie trzeba rekompilowac firmware.

## Opcjonalnie Konfiguracja Przez USB

```powershell
python tools/setup.py --port COM7 --wifi
```

SSID jest wpisywany interaktywnie, haslo nie jest wyswietlane. Nie podawaj
domowego hasla w argumentach polecenia, zeby nie trafilo do historii terminala.

## Zmiana Sieci Pozniej

W glownym panelu wybierz **Narzedzia > Zmien siec Wi-Fi**. Formularz otwiera
sie od razu w panelu, bez diagnostyki. Kliknij Wyszukaj sieci, wybierz siec,
wpisz haslo i zapisz. Siec ukryta ma opcje recznego wpisania nazwy.
Po zmianie polacz telefon z nowa siecia i ponownie otworz adres ESP.
Nie trzeba wgrywac firmware ani kasowac pamieci. W razie blednego hasla
polacz sie ponownie z Piec-Setup i popraw dane.

Haslo domowego Wi-Fi nie jest zwracane
przez API statusu. Polecenie USB `setup` ujawnia tylko haslo dostepu do ESP,
nie zapisane haslo domowej sieci.

Siec konfiguracyjna pozostaje dostepna. Domyslnie ma haslo `piecwifi`.
Oba hasla mozesz zmienic w Ustawienia urzadzenia > Hasla dostepu.
Nie wystawiaj ESP bezposrednio do Internetu.

Opcjonalny lokalny ecoNET konfiguruje sie osobno w panelu: adres IP, lokalny
login i lokalne haslo modulu, nie dane konta ecoNET24.
