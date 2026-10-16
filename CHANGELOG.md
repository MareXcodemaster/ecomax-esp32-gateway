# Changelog

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
