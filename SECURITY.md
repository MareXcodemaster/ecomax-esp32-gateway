# Security

- Local trusted networks only. HTTP is not encrypted. Do not forward ports 80
  or any diagnostic interface to the Internet. Remote access belongs behind
  a properly configured VPN/router; this firmware is not a Tailscale client.
- Each erased board creates a random password for its setup AP and admin
  login. Retrieve it with `tools/setup.py` over USB. Do not post its output.
- The secret and home Wi-Fi credentials are stored in the device's NVS.
  Flash encryption and secure boot are not configured. Physical access to
  the ESP/USB/flash therefore means trusted administrative access.
- A remembered browser session grants control. Use only your own devices;
  log out before giving a tablet/phone to someone else.
- Wi-Fi changes and USB setup do not write boiler parameters. Explicit
  parameter/control commands can affect heating and hot water.
- Boiler writes are single dispatches with readback checks, not blind retries.
  Unknown catalogue versions and changed identities must remain blocked.
- Do not post `/api/export`, serial setup output, network scans or packet
  captures in public issues without redacting identity, network and settings.
- Report a suspected leak through GitHub's private vulnerability reporting,
  if enabled for the repository. Otherwise contact the maintainer privately
  through the GitHub profile; do not put credentials in a public issue.
