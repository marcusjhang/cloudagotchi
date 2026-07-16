# Cloudagotchi 👻

A virtual pet that lives on a **1.8" AMOLED ESP32-S3 board** — but whose *brain* lives in **AWS**.

Cloudagotchi gets hungry while you sleep (EventBridge Scheduler says so), sulks when you ignore it (DynamoDB remembers), and every morning it trots on screen with a tiny newspaper to **read you the AWS news out loud** (Bedrock writes the briefing, Polly gives it a voice).

![Architecture](docs/architecture.png)

## The article series

This repository accompanies a 4-part article series. Each article has its own git branch containing the project exactly as it stands at the end of that article — `main` is the finished project.

| # | Article | Branch | What gets built |
|---|---------|--------|-----------------|
| 1 | Meet Cloudagotchi: A Virtual Pet with a Cloud Brain | [`article-1`](../../tree/article-1) | AWS IoT Core connection, device identity, MQTT plumbing |
| 2 | Giving It a Face: LVGL on a 1.8" AMOLED | [`article-2`](../../tree/article-2) | The animated pet, touch interactions, shake detection |
| 3 | It Gets Hungry While You Sleep | [`article-3`](../../tree/article-3) | Lambda + DynamoDB + EventBridge pet state machine |
| 4 | My Tamagotchi Reads Me the AWS News | [`article-4`](../../tree/article-4) | Bedrock + Polly daily news briefing, played on-device |

```bash
# Read along with article 2? Check out its branch:
git checkout article-2
```

## Repository layout

```
cloudagotchi/
├── firmware/          # ESP-IDF project for the ESP32-S3 board
├── backend/           # AWS CDK app (TypeScript) — all the cloud resources
├── scripts/           # Device provisioning helpers
└── articles/          # The articles themselves (one folder per article)
```

## Hardware

- [Waveshare ESP32-S3 Touch AMOLED 1.8](https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm)
  (368×448 AMOLED, capacitive touch, QMI8658 IMU, ES8311 audio codec, mic, RTC, battery support)

## Quick start (full project, `main` branch)

1. **Backend:** `cd backend && npm install && npx cdk deploy --all`
2. **Provision the device:** `./scripts/provision-device.sh cloudagotchi-01`
3. **Firmware:** `cd firmware && idf.py menuconfig` (set Wi-Fi + IoT endpoint) `&& idf.py flash monitor`

Each article walks through its part in detail.

## License

MIT — see [LICENSE](LICENSE).
