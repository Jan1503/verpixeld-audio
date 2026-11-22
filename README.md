# verpixeld-audio

Speaker and front panel for a [verpixeld](https://github.com/Jan1503/verpixeld) wall. A **W6300-EVB-Pico2** (RP2350) talks PixTone with the host and drives a **WM8960** codec plus a small SPI TFT.

The host protocol lives in [pixtone](https://github.com/Jan1503/pixtone). This repo is the firmware.

Two product sketches share the codec, Ethernet and web UI. The display driver and the on-screen layout differ. From `35-3.0` the 3.5" board is also a remote for verpixeld (transport, favorites, radio, wall, settings). The 2.8" glass is too small for it and keeps the volume-and-mute menu; it ignores those messages.

| Sketch | Panel | Glass | Pixels | Colour |
| --- | --- | --- | --- | --- |
| `verpixeld_audio` | TJCTM24028-SPI | 2.8" ILI9341 | 320×240 landscape | RGB565 |
| `verpixeld_audio_ili9488` | Binghe KMRTM35018-SPI | 3.5" ILI9488 | 480×320 landscape | 18-bit (3 bytes/pixel) |

`ili9488_test` and `tjctm24028_test` are bench sketches for a bare panel. They are not the audio image.

Firmware versions on the boot screen: `28-1.2` and `35-3.4`.

## What both boards do

- PixTone audio on UDP **7779** (not the panel port 7777). Discovery is `GET /status` on TCP 5000, `kind` = `verpixeld-audio`.
- Volume and mute on the glass. The level is stored in EEPROM and is not overwritten when a video starts. The flash write waits until the stream is quiet, mutes the DAC, and holds the I2S data line low so the sector erase does not click.
- Title marquee, Berlin time from the host, and a VU meter.
- Wall preview while something is playing. The 2.8" board shows the 160-wide preview, doubled to fill 320×240. The 3.5" board shows 160, 240 or 480 (set in verpixeld) as a letterboxed band.
- Cover art. The host sends an 80×80 image. The 3.5" draws it 1:1 at the top right. The 2.8" draws every other pixel as a 40×40 image in the same corner. A tap on that rectangle opens the screensaver. A tap anywhere else does not.
- Screensaver while audio is playing: the wall preview, the title, and a fixed clock. The first touch only wakes the menu.
- When playback stops, the screensaver leaves and the menu returns. If it stays quiet, the screensaver comes back as a clock on black that jumps to a new position every 10 seconds. The next stream brings the menu back and the original behaviour.
- DHCP and link wait show a boot screen without the menu: name, firmware, codec state, `LINK…` / `DHCP…` in the header and a bar along the bottom edge. The 3.5" menu and tabs appear once the network is up.

Both boards (from `35-2.7` and `28-1.2`) take the wall preview on their own UDP port **7780** and report it as `"pic":7780` in `/status`. verpixeld sends `VPIC` there, so a burst of picture chunks fills a separate 8 KB W6300 buffer and never pushes a PCM packet out. Core 1 drains the audio socket completely, then at most four picture chunks per pass. Boards without `pic` keep getting `VPIC` on 7779. Above a preview width of 160 the host sends only the picture, not the black side bars: a 2:1 wall at 480 arrives as 320×160.

On both boards, volume and mute go to the WM8960 output stages (speaker and headphone, 1 dB steps, zero-cross) when the codec answers on I2C. On the current wiring it does not answer while its clocks run, so the firmware asks once at boot, prints `[codec] volume in software …`, and scales the samples as before.

verpixeld reads `vol` and `mute` from `/status` once a second, so a change on the panel shows up in its web UI.

The board web page builds its SPI clock list from `peri` in `/status` (the peripheral clock in MHz). It only offers clocks the SPI divider can make: 60, 30, 20, 15 … MHz on the 120 MHz build. `spik` is the actual clock in kHz.

The 3.5" picture path accepts the fast 1440-byte chunks. The 2.8" path stays on 960-byte chunks and 160×120 frames, which is what the host sends when the preview width is 160.

## 3.5" panel (from `35-3.4`)

Pictures of the pages and the screensaver: [`docs/panel-35-3.1-play.png`](docs/panel-35-3.1-play.png), [`-radio`](docs/panel-35-3.1-radio.png), [`-more`](docs/panel-35-3.1-more.png), [`-saver`](docs/panel-35-3.1-saver.png). The plan for a 10.1" ESP32-P4 panel is in [`docs/p4-kit-d.md`](docs/p4-kit-d.md).

Five pages, chosen in the tab bar at the bottom (y 284–320): **PLAY · FAVS · RADIO · WALL · MORE**. The first touch on the screensaver only wakes it; the page stays where it was.

| Page | What |
| --- | --- |
| PLAY | Clock, play time `1:23 / 4:56`, progress bar (tap to seek), cover top right (tap for the screensaver), title marquee (tap to remember the song), volume, transport row ⏮ ■ ▶/❚❚ ⏭ MUTE, spectrum analyzer |
| FAVS | verpixeld's favorites, six per page, ◀ ▶ to page. A tap plays the entry; the playing one is cyan |
| RADIO | The saved radio stations from verpixeld's Radio page. A tap tunes in |
| WALL | Wall brightness (drag), WALL ON/OFF, SPECTRUM (the board's spectrum as a visualizer on the wall), and the saved layouts (a tap loads one) |
| MORE | SHUFFLE, REPEAT, SLEEP (off → 15 → 30 → 60 → 90 min), VU (spectrum in the screensaver on or off, saved on the board), A/V sync − / + in 20 ms steps (tap the value for 0), the alarm (ALARM on/off, − / + in 15-minute steps; it is the first alarm on verpixeld's Radio page), and the board's status: IP, SPI clock, buffer, clock drift, uptime, underruns and overruns, firmware, where the volume is set |

While radio plays, ⏮ and ⏭ step through the saved stations. ▶ on a stopped panel brings the last station back.

verpixeld sends `VPLY` (state, what is allowed, position, duration), `VSET` (shuffle, repeat, wall on, brightness, sleep minutes, A/V sync) and `VLST` (the three lists, 28-byte names) about once a second and at once on a change; the lists again every 5 seconds. A press goes back as `VKEY` to the address and port the last `VPLY` came from. That is a reply on the same UDP flow, so it gets through Docker's NAT without another published port. Each list carries a generation, and a select key names it, so a tap that crosses a list update still picks the entry that was on the glass. The panel shows a change at once; the next `VSET` or `VPLY` confirms it. With no word from verpixeld for 4 seconds the row goes grey.

**Spectrum.** A 1024-point FFT of the mono mix, 28 bands from 50 Hz to 16 kHz on a log scale, about 30 frames a second, segmented bars with peak caps. It sits under the transport row and, larger, in the screensaver when no wall picture is coming.

**Screensaver for music and radio.** verpixeld marks music and radio as audio only (`VSET` flag 16). Then the screensaver ignores the wall picture and shows the cover at 160×160, the clock, the title in up to two lines with the artist under it (`Artist - Title` is split at the dash) and the large spectrum (unless VU is off). Without a cover everything is centred. A video keeps the wall-picture screensaver.

**Screensaver text.** Title and artist are each one line of large text that slides when it is wider than the space beside the cover. "Audio only" is held for 6 seconds, so a single late packet no longer flips the screensaver to the wall-picture layout for a moment, and verpixeld no longer sends the wall picture while music or radio plays.

**Wall spectrum.** With SPECTRUM on, verpixeld sets `VSET` flag 32 and the board sends its 28 bands as `VSPC` about 30 times a second, back on the same UDP flow as `VKEY`. verpixeld's visualizer draws them on a canvas (the one used last, else Main), so the wall gets a visualizer without a local audio device, also in the NAS container.

**Station covers.** verpixeld builds the radio cover itself: the logo from Radio Browser, else the station website's touch icon, page image or largest icon, whatever Skia can decode (PNG, JPEG, WebP, ICO; not SVG). The logo is fitted whole on a tile in its own edge colour. With nothing usable it draws a radio on a colour from the station name.

**Clock drift.** The codec crystal and the host clock never agree exactly, so over a long film the buffer would slowly fill or drain. Two seconds after a stream starts, the board takes the total buffer (ring plus I2S queue) as its target. When it moves more than 1 ms away, playback runs 200 ppm faster or slower through a 4-point Hermite interpolator until it is back, then the samples pass through untouched again. MORE shows the measured drift and how often it corrected.

**Display by DMA.** The wall picture, the spectrum, the large cover and every larger fill go out by DMA while the CPU prepares the next row; waiting for the DMA keeps the audio pumped.

**Core races fixed.** Core 1 no longer writes the ring read index; a discontinuity asks core 0 to drop the ring. The preview buffer swap is guarded by a hardware spinlock, and a finished frame that waits for the blit is no longer overwritten by the next one.

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

Arduino-Pico core, 4 MB flash with a 1 MB filesystem. Build both sketches at **120 MHz** (`28-1.2` and `35-2.8` on):

```powershell
arduino-cli compile `
  --fqbn "rp2040:rp2040:rpipico2:flash=4194304_1048576,freq=120" `
  --output-dir _build `
  verpixeld_audio
```

```powershell
arduino-cli compile `
  --fqbn "rp2040:rp2040:rpipico2:flash=4194304_1048576,freq=120" `
  --output-dir _build `
  verpixeld_audio_ili9488
```

The SPI block divides the system clock by an even number. At the stock 150 MHz, the requested 62 MHz is really 37.5 MHz. At 120 MHz it is exactly 60 MHz, about 60 % more display throughput. Off 150 MHz the SDK runs the peripheral clock from the 48 MHz USB PLL, which caps SPI at 24 MHz; from `35-2.8` and `28-1.2` the firmware moves it back to the system clock at boot. The firmware prints the real rate (`[lcd] spi … Hz`), and the HUD shows it. If the glass shows errors at 60 MHz, `spi 40` on the serial port or web page drops to 30 MHz. The W6300 QSPI divider follows the system clock on its own.

Flash the `.uf2` from `_build` with BOOTSEL. Do not use `--upload` unless you mean to replace the running image over USB.

`Codec.h`, `Dhcp.h`, `W6300.h`, `WebServer.h` and `w6qspi.pio.h` are the same bytes in both sketch folders. They are copies, not links: after editing one, copy it to the other folder. Arduino CLI copies a sketch to a temp directory, so a parent `#include` does not work. Keep the two copies identical.

## Serial

115200 baud, after the codec has started:

`tone` · `stop` · `vol <0-100>` · `mute` · `spi <8-62>` · `net` · `net dhcp` · `net static <ip> <mask> <gw>` · `save` · `reboot`

The same commands are on `http://<board-ip>:5000`.
