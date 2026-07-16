# Device credentials go here

Run `../../scripts/provision-device.sh <thing-name>` to generate:

- `device-cert.pem` — the device certificate (gitignored)
- `device-key.pem` — the private key (gitignored, **never** commit this)
- `AmazonRootCA1.pem` — Amazon's root CA (public, committed)

The firmware build embeds all three into the binary.
