# ecoMAX embedding notes

Upstream source is pinned in `UPSTREAM.json`, including original file hashes.
Licenses and attribution remain intact. This is a vendored component, not a
second firmware or the gateway application.

Local patches:

- `wireguard-platform.h` uses the same configured peer limit as MicroLink;
  `wireguardif.c` allocates/frees the device/session pool in PSRAM. The original
  sixteen-peer pool consumed 14,800 internal bytes even with four active slots.
  VPN initialization waits while the GitHub worker is running, avoiding
  simultaneous startup peaks. The 48 KiB internal-memory guard is retained.

- `CMakeLists.txt` defines `ECOMAX_EXTERNAL_JSON_ALLOCATOR`. `microlink.c`
  then leaves the application's existing cJSON PSRAM allocator unchanged.
  Reinitializing global allocator hooks while other tasks use JSON is unsafe.
- `microlink_internal.h` does not fall back from PSRAM allocations to internal
  RAM. A large VPN buffer must not consume the reserve needed by RS485/Wi-Fi.
- `ml_wg_mgr.c` uses `ecomax_tailnet_input` instead of direct `tcpip_input`.
  This extra gate accepts only IPv4 TCP port 80 for the assigned ESP VPN IP,
  after upstream authenticated peer ownership and ACL checks. It is closed
  during reconfiguration, shutdown and incomplete policy synchronization.
- `ml_derp.c` always verifies relay certificates. The application's fixed
  `https://controlplane.tailscale.com` URL also enables certificate verification
  for coordination and the server's Noise key bootstrap; no HTTP fallback.
- `microlink.c` fails initialization if persistent private-key storage cannot
  be read/committed. It derives public keys from valid 32-byte private keys
  and does not silently generate an ephemeral identity on NVS errors.

Embedding lifecycle and memory guard are in `src/tailnet.cpp` and
`src/tailnet_guard.hpp`. The optional component starts only after existing boot
self-tests. Worker priorities remain below RS485 reception. A stopped context
is not freed until its workers finish; incomplete shutdown is reported instead
of force-deleting workers or resetting the ESP. Key string storage stays alive
for the full client lifetime.

Configuration disables the upstream configuration HTTP server, cellular UART,
zero-copy receive path and IP forwarding. It limits active peers to sixteen,
the persistent peer catalogue to sixteen and JSON/H2 buffers to 128 KiB.
TCP/IP task stack is 6 KiB. ChaCha20, Poly1305 and their AEAD are enabled in
the pinned ESP-IDF mbedTLS build. No router, hotspot DHCP, DNS proxy, exit-node
selection, telemetry dashboard or credentials from the reference gateway app
are imported.

Validation: `tools/host_tests.py`, `tools/test_tailnet_authorization.py`,
`tests/test_tailnet.cjs`, `tests/tailnet_browser.mjs` and HTTP loopback checks.
Host policy tests execute the vendored C with synthetic maps, clocks, pbufs
and mocked transport/crypto. They do not prove a physical end-to-end tunnel.

The initial implementation needs physical registration, LTE-to-ESP HTTP,
policy revocation, reboot/reconnect, resource and long-duration tests. No
claim of production readiness is made by a successful compile or host test.

- Diagnostyka koordynatora przekazuje do aplikacji wyłącznie stałe nazwy etapów oraz numery błędów TLS. Nie przekazuje kluczy, odpowiedzi rejestracji, nazw kont ani adresów autoryzacji.
