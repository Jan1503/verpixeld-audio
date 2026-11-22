# verpixeld-audio on the ESP32-P4 (KIT-D): plan

Status: preparation. Hardware arrives in mid-December 2026. Nothing here has been tested on the board yet.

## Hardware

Waveshare **ESP32-P4-NANO-WIFI6-DB-KIT-D** (SKU 35559):

| Part | Detail | Still to verify on the board |
| --- | --- | --- |
| SoC | ESP32-P4NRW32X, 32 MB PSRAM, 16 MB flash | |
| Radio | ESP32-C5-MINI-1U, Wi-Fi 6 2.4 + 5 GHz, external antenna | C5 slave firmware version (esp-hosted) |
| Ethernet | 100 Mbit RJ45, PoE module in the kit | PHY type |
| Display | 10.1" MIPI-DSI 800×1280 portrait, GT9271 touch | LCD controller and init sequence (Waveshare BSP) |
| Audio | onboard codec, mic, one 8 Ω 2 W speaker | codec chip (likely ES8311 mono + NS4150 amp) |
| Header | 2 × 13 pins, 28 GPIOs | which pins are free for the WM8960 |

## Goals

1. **Wi-Fi first.** 5 GHz is the normal case. Ethernet with PoE is the fallback.
2. Same PixTone behaviour as the 3.5" board: clean sound, synced to the wall, and a fluid preview in the screensaver.
3. A full touch menu, including Wi-Fi scan and connect.
4. Optional stereo through the existing SeenGreat WM8960 (wired, not stacked).
5. Later: control verpixeld from the panel.

## Firmware stack

- **ESP-IDF 5.5+**, its own project at `verpixeld-audio/verpixeld_audio_p4/`. It is not an Arduino sketch.
- Wi-Fi: `esp_wifi_remote` + `esp_hosted` over SDIO to the C5. `WIFI_PS_NONE` always; power save adds 100+ ms latency spikes.
- Ethernet: `esp_eth` on the internal EMAC. Both interfaces may be up; the route prefers Ethernet when a cable has link.
- UI: **LVGL 9** through `esp_lvgl_port`, DSI via the Waveshare BSP, rotation via PPA (hardware), not in software.
- Preview: host sends JPEG, decoded by the P4 hardware JPEG decoder, scaled and blended by PPA.
- Audio: I2S DMA to the onboard codec (downmix L+R to mono) or to the WM8960 (I2S slave, codec is master with its own 24.576 MHz oscillator, as on the Pico).
- Settings in NVS (Wi-Fi credentials, network mode, volume, output, UI options). Same debounce rule as the Pico: no flash writes while audio is playing.
- OTA over the web page on port 5000, same as the Pico boards.

## Wi-Fi changes the audio requirements

Wired LAN almost never loses or reorders UDP. Wi-Fi does both, and adds jitter in bursts (retries, scans, other clients). The Pico firmware ignores `seq` and `sampleIndex`. On the P4 that is not good enough:

| Topic | Pico today | P4 |
| --- | --- | --- |
| Jitter buffer | fixed 85 ms ring + 85 ms DMA queue | PSRAM ring, target fill adjustable (e.g. 60–150 ms) |
| Lost packet | gap → underrun click | place by `sampleIndex`; conceal a gap with a short crossfade |
| Reordered packet | appended in arrival order | placed by `sampleIndex` |
| Clock drift host ↔ codec crystal | ring slowly over/underruns | adaptive resampler (±500 ppm) holds the target fill |
| Latency the host compensates | constant `PixToneDeviceLatencyMs = 80` in `VideoPlayer.cs` | board reports its real latency; host uses that value |
| Packet redundancy | none | optional: each packet also carries the previous 7.5 ms (≈ +1.5 Mbit/s) |

The resampler and the `sampleIndex` placement help the Pico boards too. They belong in shared C code that both firmwares copy.

## PixTone extensions (host and firmware)

All of these extensions are backwards compatible: old boards ignore unknown magics.

- `/status` gets `caps`: screen width/height, supported preview formats (`rgb565`, `jpeg`), max preview size, current latency in ms, link type (`eth`/`wifi`), RSSI.
- `VJPG`: a JPEG preview frame in chunks (frame id, index, count). At 800×400 a frame is about 30–60 KB. At 25 fps that is about 6–12 Mbit/s, comfortable on 5 GHz.
- Feedback gains `latencyMs` and a lost-packet counter. `PixToneSender` reads the feedback (today nobody does).
- `PixToneOutputService.Host` must stay valid across Wi-Fi/Ethernet switches: keep the IP via DHCP reservation, or resolve via mDNS (`audio-xxxxxx.local`).

## Menu (LVGL)

The first boot without saved Wi-Fi opens the Wi-Fi page directly. The touchscreen replaces a SoftAP captive portal.

- **Now playing**: cover (large), title, time, VU meter, volume slider, mute, preview thumbnail. A tap on the cover opens the screensaver.
- **Screensaver**: full-screen wall preview at max fps, title, clock. When idle: a drifting clock on black (same behaviour as the 3.5").
- **Network**
  - Wi-Fi: scan list (SSID, band 2.4/5, RSSI bars, lock icon), connect with on-screen keyboard, saved networks, forget, auto-reconnect, show IP/RSSI/channel/PHY rate.
  - Ethernet: link, speed, IP.
  - Priority: Ethernet first / Wi-Fi only / Ethernet only. DHCP or static per interface.
- **Audio**: output (onboard / WM8960), test tone, latency target (jitter buffer), live stats (fill, underruns, lost packets, drift ppm).
- **Display**: brightness, rotation (landscape/portrait), screensaver timeout, preview fit/fill, preview fps.
- **System**: name, firmware version, OTA, reboot, factory reset, uptime, free PSRAM.
- *Later*: **verpixeld control** (playlist, extensions, brightness of the wall) over the verpixeld web API.

## Milestones

1. Bring-up: display + touch + LVGL hello; Ethernet DHCP; Wi-Fi via C5 connects to 5 GHz.
2. Audio: PixTone receive → ring → I2S → onboard speaker. Measure loss/jitter on Wi-Fi with `iperf` and the feedback counters.
3. Jitter buffer with `sampleIndex`, resampler, latency reporting; host reads the feedback.
4. Menu: Now playing, Network (scan/connect), Audio, Display, System.
5. Preview: `VJPG` on host and board, screensaver at full fps.
6. WM8960 option.
7. verpixeld control.

## Open decisions

- Mount orientation: landscape 1280×800 (fits a 2:1 wall better) or portrait 800×1280.
- Is the onboard speaker enough, or is the WM8960 the default output?
