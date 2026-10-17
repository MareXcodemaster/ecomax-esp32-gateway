# Changelog

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
