# Pierwsze Polaczenie Wi-Fi

ESP nie ma zaszytej domowej sieci ani hasla autora.

## Najprosciej: Formularz W Instalatorze

Po wgraniu firmware zamknij okno wgrywania i wybierz Polacz ESP przez USB.
Instalator odczyta haslo dostepu do ESP. Wpisz nazwe i haslo swojej sieci
2,4 GHz, nacisnij Zapisz i polacz i poczekaj na przycisk Otworz panel ESP.
Nie trzeba instalowac dodatkowego programu ani wpisywac polecen.
Tego samego formularza mozna uzyc pozniej bez ponownego wgrywania.
Hasla nie sa zapisywane w stronie, historii adresow ani wysylane przez Internet.

## Konfiguracja Telefonem

1. Zainstaluj firmware i odczytaj indywidualne haslo przez USB, zgodnie
   z [instrukcja instalacji](INSTALACJA.md).
2. Polacz telefon z siecia `Lazar-Setup-XXXX` utworzona przez Twoja plytke.
   `XXXX` wynika z adresu sprzetowego. Haslo jest tym odczytanym przez USB.
3. Telefon moze ostrzegac o braku Internetu. Pozostan w tej sieci.
4. Otworz w przegladarce **http://192.168.4.1/diagnostics**. Nie wpisuj HTTPS.
5. Login: `admin`. Haslo: indywidualne haslo Twojego ESP.
6. W sekcji Domowe Wi-Fi wpisz wlasny SSID i haslo i nacisnij Zapisz Wi-Fi.
   Wymagane jest Wi-Fi 2,4 GHz; 5 GHz nie jest obslugiwane.
7. Sprawdz status polaczenia i adres IP w diagnostyce. ESP zapisuje pare
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

W glownym panelu wybierz ESP Wi-Fi > Konfiguruj siec Wi-Fi. Otworzy sie
lokalna diagnostyka z formularzem. Haslo domowego Wi-Fi nie jest zwracane
przez API statusu. Polecenie USB `setup` ujawnia tylko haslo dostepu do ESP,
nie zapisane haslo domowej sieci.

Siec konfiguracyjna pozostaje dostepna i wymaga indywidualnego hasla.
Chron to haslo tak samo jak dostep administracyjny do kotla.

Opcjonalny lokalny ecoNET konfiguruje sie osobno w panelu: adres IP, lokalny
login i lokalne haslo modulu, nie dane konta ecoNET24. Nie ma domyslnego
adresu ani hasla modulu nalezacego do autora.
