# Third-Party Software And Protocol References

The repository's MIT license does not replace the licenses of dependencies.
Full notices collected from the pinned sources are in `LICENSES/` and travel
with the installation ZIP. The collection may include notices for SDK
components that are not linked in this particular build.

## Firmware Dependencies

- **SortableJS 1.15.7**, MIT: touch and mouse tile reordering, bundled locally.
  https://github.com/SortableJS/Sortable/tree/1.15.7
- **Lato**, packaged by @fontsource/lato 5.2.7, SIL Open Font License 1.1.
  Latin and Latin-ext subsets are bundled locally; full notices are included
  in `LICENSES/installer`. https://github.com/fontsource/fontsource

- **ESP-IDF 5.5.0**, supplied by PlatformIO Espressif32 6.12.0. Espressif
  code is generally Apache-2.0; bundled FreeRTOS, lwIP, mbedTLS, radio blobs
  and other components retain their individual license/notice files.
  Source: https://github.com/espressif/esp-idf/tree/v5.5
- **espressif/mdns**, pinned by `dependencies.lock`, Apache-2.0.
  Source: https://components.espressif.com/components/espressif/mdns
- **joltwallet/littlefs 1.20.4**, MIT; bundled upstream littlefs retains its
  BSD-3-Clause notice. Source:
  https://components.espressif.com/components/joltwallet/littlefs/versions/1.20.4
- **Lucide 0.468.0**, ISC, with Feather portions originally MIT.
  No remote CDN is used. The library retains its header and full notices
  are included. Source: https://github.com/lucide-icons/lucide/tree/0.468.0

## Protocol Interoperability

PyPlumIO, MIT, Copyright (c) 2022 Denis Paavilainen (@denpamusic),
commit `033c157810bdfa1aa479a361cf809d12c4b532a9`, is a reference for wire
layouts, sensor data, frame constants and thermostat/schedule parameter
formats. Its complete license is included in `LICENSES/PyPlumIO-MIT.txt`.
The ESP does not execute Python or Home Assistant.
https://github.com/denpamusic/PyPlumIO

Local ecoNET API paths and identifiers were checked against the MIT project
below; its application code and manufacturer JavaScript are not bundled:
https://github.com/jontofront/ecoNET-300-Home-Assistant-Integration
commit `d72c76670506906d243d9d2270453ea6c9041ecb`.

Additional numeric protocol facts were independently implemented with the
reference below. Its program source is NOT redistributed and no license
for that program is assumed:
https://gist.github.com/denpamusic/6375c2a57716dc42861350f0eebc1e68
revision `aadb92fed3d7a17a6bcb074c501fa40d5f07c016`.

The compatibility profile contains wire schema, short functional names,
units, enumerations and metadata fingerprints. It contains no controller
UID, actual settings, network information, recordings or personal data.
Parameter descriptions in this public build are generic project text, not
copied manufacturer instructions. Decoder fixtures are generated synthetic
examples from the metadata, never real device captures.

## External Installation Tools

The browser installer uses **ESP Web Tools 10.4.0** (Apache-2.0) and its
locked npm dependencies, including esptool-js, Lit, Material Web and the
optional Improv library. They are bundled locally, not loaded from a CDN.
Firmware uses its own USB JSON provisioning, not Improv. Licenses for
these packages are collected in LICENSES/installer. Vite and Playwright
are development tools only. https://github.com/esphome/esp-web-tools

The Polish HTML guide is rendered at build time using Marked 18.1.1 (MIT).
Its license is included in LICENSES/installer; no Marked runtime is sent to ESP.
The guides link four official Waveshare product photos hosted by Waveshare,
with attribution (model variants and terminal labels). These photos are not
bundled or covered by this project's MIT license. Installer and DEMO panel
screenshots, the wiring diagram and the BOOT/USB procedural illustration
are original project assets. The latter is a schematic top view, not a
manufacturer photograph; component positions and BOOT 13 / RESET 14 labels
were checked against the manufacturer's component map:
https://docs.waveshare.net/ESP32-S3-RS485-CAN/

PlatformIO Core, esptool (GPL-2.0-or-later) and pyserial are installed
separately from their official packages. They are tools, not linked into
the firmware. Their source/licensing information is available through
https://pypi.org/project/platformio/ and https://pypi.org/project/esptool/.

Manufacturer logos, photo/bitmap sprites, font and hydraulic SVG obtained
from a private ecoNET installation are not distributed. Brand names are
used only to describe compatibility, not affiliation or endorsement.

The hydraulic drawing src/panel_web/assets/heating-schema.svg is original
project artwork, not a manufacturer's installation plan. The photographic
background boiler-room.jpg is AI-generated original artwork, not a photo
of a supported product or a user's installation. It was generated in
text-to-image mode: bright neutral utility room, generic unbranded pellet
boiler and hot water cylinder on the right, red/blue pipes, no logos or
text, landscape composition. The optimized JPEG is bundled locally; no
external image service is contacted by the panel.
