# verpixeld-audio

Speaker and front panel for a [verpixeld](https://github.com/Jan1503/verpixeld) wall. A **W6300-EVB-Pico2** (RP2350) talks PixTone with the host and drives a **WM8960** codec plus a small SPI TFT.

The host protocol lives in [pixtone](https://github.com/Jan1503/pixtone). This repo is the firmware.

Two product sketches share the codec, Ethernet and web UI. Only the display driver and the on-screen layout differ.

| Sketch | Panel | Glass | Pixels | Colour |
| --- | --- | --- | --- | --- |
| `verpixeld_audio` | TJCTM24028-SPI | 2.8" ILI9341 | 320×240 landscape | RGB565 |
| `verpixeld_audio_ili9488` | Binghe KMRTM35018-SPI | 3.5" ILI9488 | 480×320 landscape | 18-bit (3 bytes/pixel) |

`ili9488_test` and `tjctm24028_test` are bench sketches for a bare panel. They are not the audio image.

Firmware versions on the boot screen: `28-1.0` and `35-2.4`.

## What both boards do

- PixTone audio on UDP **7779** (not the panel port 7777). Discovery is `GET /status` on TCP 5000, `kind` = `verpixeld-audio`.
- Volume and mute on the glass. The level is stored in EEPROM and is not overwritten when a video starts. The flash write waits until the stream is quiet, mutes the DAC, and holds the I2S data line low so the sector erase does not click.
- Title marquee, Berlin time from the host, and a VU meter.
- Wall preview while something is playing. The 2.8" board shows the 160-wide preview, doubled to fill 320×240. The 3.5" board shows 160, 240 or 480 (set in verpixeld) as a letterboxed band.
- Cover art. The host sends an 80×80 image. The 3.5" draws it 1:1 at the top right. The 2.8" draws every other pixel as a 40×40 image in the same corner. A tap on that rectangle opens the screensaver. A tap anywhere else does not.
- Screensaver while audio is playing: the wall preview, the title, and a fixed clock. The first touch only wakes the menu.
- When playback stops, the screensaver leaves and the menu returns. If it stays quiet, the screensaver comes back as a clock on black that jumps to a new position every 10 seconds. The next stream brings the menu back and the original behaviour.
- DHCP and link wait show a short status (`LINK…`, `DHCP…`) and a bar along the bottom edge, not across the volume control.

The 3.5" picture path accepts the fast 1440-byte chunks. The 2.8" path stays on 960-byte chunks and 160×120 frames, which is what the host sends when the preview width is 160.

## Do not stack the codec HAT

The SeenGreat WM8960 HAT header is **not** the Raspberry Pi pinout. Schematic FH-00339 puts 5 V on HAT pins 1 and 3. With the speaker connector at the lower left, pin 1 is the upper-right end of its row. Wire the signals to the Pico header instead of plugging the HAT on.

The codec is I2S master. Its onboard 24.576 MHz oscillator is MCLK. Do not wire a master clock from the Pico, and do not call `i2s.setSysClk`.

Do not use GP10–GP12 for anything but I2S, or GP16–GP22 for anything but the W6300 QSPI. GP15 is the W6300 interrupt.

## Pinout

Wiz pin numbers are the silkscreen on the W6300-EVB-Pico2 header. GP is the RP2350 GPIO.

### Codec and button (both boards)

| Signal | WM8960 HAT pin | Wiz pin | GPIO |
| --- | --- | --- | --- |
| 5 V | 1 (or 3) | 40 | 5 V |
| GND | 5 | 38 | GND |
| SDA | 4 | 9 | GP6 |
| SCL | 6 | 10 | GP7 |
| BCLK | 11 | 14 | GP10 |
| LRCLK | 36 | 15 | GP11 |
| DAC data | 39 | 16 | GP12 |
| Button to GND | 12 | 31 | GP26, pull-up |

Leave the HAT J1 jumper open when VCC is 5 V. I2C is bit-banged on GP6/GP7 before the codec starts its clocks. After that, the chip NACKs a re-init; a warm reboot leaves the registers alone.

### Display and touch (both boards)

| Signal | Wiz pin | GPIO | Notes |
| --- | --- | --- | --- |
| SCK and T_CLK | 4 | GP2 | Shared SPI clock |
| MOSI and T_DIN | 5 | GP3 | Shared SPI data out |
| TFT CS | 11 | GP8 | |
| TFT DC | 12 | GP9 | |
| TFT RESET | 19 | GP14 | |
| Backlight LED | 36 | 3.3 V rail | Supply, about 60–80 mA. Not a GPIO and not 5 V |
| T_DO | 2 | GP1 | Touch data out, bit-banged. GP1 is not a legal SPI0 MISO |
| T_CS | 6 | GP4 | |
| T_IRQ | 7 | GP5 | Low while the panel is touched |
| VCC | 40 | 5 V | |
| GND | 38 | GND | |

Leave the SD slot empty.

Landscape on both: pin header on the **right**, MADCTL `0x28` (`MV|BGR`).

**2.8" ILI9341.** RGB565 (`COLMOD 0x55`). Display SDO and T_DO may share GP1; that panel releases SDO when CS is high. Touch maps directly: X from `0x90`, Y from `0xD0`.

**3.5" ILI9488.** 4-wire SPI is 18-bit only (`COLMOD 0x66`). An ILI9341 init sequence draws garbage. Display SDO (module pin 9) does **not** tri-state when CS is high. Leave SDO open. Only T_DO goes to GP1. Touch X is mirrored (`ST_W - 1 - raw`) so a finger on the right is the right side of the glass. Y is still `0xD0`.

## Build

Arduino-Pico core, 4 MB flash with a 1 MB filesystem:

```powershell
arduino-cli compile `
  --fqbn "rp2040:rp2040:rpipico2:flash=4194304_1048576" `
  --output-dir _build `
  verpixeld_audio
```

The 3.5" sketch is `verpixeld_audio_ili9488`. Flash the `.uf2` from `_build` with BOOTSEL. Do not use `--upload` unless you mean to replace the running image over USB.

`Codec.h`, `Dhcp.h`, `W6300.h`, `WebServer.h` and `w6qspi.pio.h` are the same bytes in both sketch folders (hard links in this checkout). Arduino CLI copies a sketch to a temp directory, so a parent `#include` does not work. Keep the two copies identical.

## Serial

115200 baud, after the codec has started:

`tone` · `stop` · `vol <0-100>` · `mute` · `spi <8-62>` · `net` · `net dhcp` · `net static <ip> <mask> <gw>` · `save` · `reboot`

The same commands are on `http://<board-ip>:5000`.
