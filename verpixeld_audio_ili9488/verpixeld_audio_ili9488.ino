/*--------------------------------------------------------------------------------------
  verpixeld audio endpoint. W6300-EVB-Pico2 + Seengreat WM8960 Audio Codec LCD HAT.

  Do not stack the HAT on the Pico. The HAT header is NOT the Raspberry Pi pinout.
  SeenGreat schematic FH-00339 puts 5V on pins 1 and 3. With the speaker connector
  at the lower left, pin 1 is the upper-right end of its row. Pin 2 is beside it
  on the other row. Count both rows from that end toward the left.

    HAT signal   HAT pin   Pico
    5V           1 (or 3)  VBUS
    GND          5         GND
    SDA          4         GP6
    SCL          6         GP7
    BCLK         11        GP10     (LRCLK is the next Pico pin, GP11)
    LRCLK        36        GP11
    DAC data     39        GP12
    SPI SCK      24        GP2      also TJCTM T_CLK
    SPI MOSI     20        GP3      also TJCTM T_DIN
    TFT CS       23        GP8
    TFT DC       17        GP9
    TFT RST      21        GP14
    TFT MISO     —         GP1      TJCTM SDO and T_DO, not the old panel
    TFT T_CS     —         GP4
    TFT T_IRQ    —         GP5
    TFT LED      —         3V3      Wiz pin 36, not GP0
    BUTTON       12        GP26

  MCLK is the onboard 24.576 MHz oscillator. Do not wire one.

  PixTone, UDP 7779, one datagram per 7.5 ms, no IP fragments:

    0   magic u32   'PTON' (0x4E4F5450)
    4   seq u32
    8   sampleIndex u32   first frame in this packet
    12  flags u16         bit0 = discontinuity (flush the ring)
    14  reserved u16
    16  pcm s16le stereo, 360 frames, 1440 bytes

  Control, same socket, not mixed into the PCM:

    'VVOL' u32, volume u8 (0..100), mute u8, pad u16
    'VTIT' u32, utf-8 title, up to 40 bytes
    'VPLY' u32, state u8 (0 stop, 1 play, 2 pause), flags u8 (1 seek, 2 prev, 4 next), pad u16,
           position ms u32, duration ms u32 (0 = live)
    'VKEY' board to host: u32, key u8 (1 play/pause, 2 stop, 3 prev, 4 next, 5 seek), seq u8,
           arg u16 (seek: per mille), list generation u8, pad u8. Sent three times to the
           sender of the last VPLY. Keys 6-12: shuffle, repeat, sleep, A/V sync (arg ms+2000),
           wall brightness (arg %), wall on/off, list select (arg list<<12 | index).
    'VSET' u32, flags u8 (1 shuffle, 2 repeat, 4 wall on, 8 radio), brightness u8,
           sleep minutes u16, A/V sync ms s16
    'VLST' u32, list u8 (1 favorites, 2 radio, 3 layouts), generation u8, total u8, first u8,
           count u8, active u8 (0xFF none), pad u16, then count names of 28 bytes

  Feedback, 12 bytes, back to the last sender about every 100 ms:

    'PTON' u32, fillFrames u16, underruns u16, overruns u16, flags u16
    flags: bit0 receiving, bit1 link up, bit2 mute

  Before verpixeld sends anything, the serial console speaks:
    tone / stop, vol <0-100>, mute, net, net dhcp, net static <ip> <mask> <gw>, save
--------------------------------------------------------------------------------------*/
#if !defined(ARDUINO_ARCH_RP2040)
#error "RP2040/RP2350 only."
#endif

#include <SPI.h>
#include <I2S.h>
#include <hardware/flash.h>
#include <EEPROM.h>
#include <math.h>
#include <LittleFS.h>
#include <Updater.h>
#include "pico/unique_id.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#define FW_VERSION "35-5.5"   // 5.4 + console channel test (ch l/r/lr/swap)

// The display SPI divides clk_sys by an even number. 120 MHz gives exactly 60 MHz;
// the stock 150 MHz build lands at 37.5 MHz.
#if F_CPU != 120000000L && F_CPU != 240000000L
#warning "Build with freq=120 (fqbn ...:freq=120). Other clocks run the display SPI below 60 MHz."
#endif

// W6300, fixed on the EVB. Same pins as verpixeld-panel.
#define W6_CS   16
#define W6_SCLK 17
#define W6_IO0  18
#define W6_IO1  19
#define W6_IO2  20
#define W6_IO3  21
#define W6_RST  22
#define IO_MASK ((1u << W6_IO0) | (1u << W6_IO1) | (1u << W6_IO2) | (1u << W6_IO3))
#define BSB_COMMON 0x00
#define BSB_S0_REG 0x01
#define BSB_S0_TX  0x02
#define BSB_S0_RX  0x03
#define BSB_S1_REG 0x05
#define BSB_S1_TX  0x06
#define BSB_S1_RX  0x07
#define BSB_S2_REG 0x09
#define BSB_S2_RX  0x0B
#define WEB_PORT 5000
#define WEB_RXBUF_KB 2
#define WEB_TXBUF_KB 4
#define QSPI_MODE_QUAD 0x80
// SCLK = sysclk / (2 * div). Hold about 30.7 MHz on any build clock (div 2.5 at 153.6 MHz).
#define W6_CLKDIV ((float)clock_get_hz(clk_sys) / 61440000.0f)

#define UDP_PORT 7779
#define RXBUF_KB 16
// Wall preview on its own socket. A burst of picture chunks fills this buffer, not the
// audio one, so it can never push a PCM packet out of the W6300.
#define PIC_PORT 7780
#define PIC_RXBUF_KB 8

// HAT jumpers. LRCLK is BCLK + 1 because the I2S PIO pairs them.
#define PIN_SDA   6
#define PIN_SCL   7
#define PIN_BCLK  10
#define PIN_DOUT  12
#define PIN_MCLK  13   // unused; codec clocks itself
#define PIN_SCK   2
#define PIN_MOSI  3
#define PIN_MISO  1    // touch data out, bit-banged; not a legal SPI0 pin
#define PIN_CS    8
#define PIN_DC    9
#define PIN_RST   14
#define PIN_TCS   4
#define PIN_TIRQ  5
#define PIN_BTN   26
// Backlight is wired to the 3.3 V rail (Wiz pin 36), not a GPIO.

#define SAMPLE_RATE 48000
#define PACKET_FRAMES 360
#define PACKET_BYTES (PACKET_FRAMES * 4)
#define HDR_BYTES 16
#define RING_FRAMES 4096          // 85 ms, power of two
#define TARGET_IDLE_MS 250

#define MAGIC_PTON 0x4E4F5450u    // 'PTON'
#define MAGIC_VVOL 0x4C4F5656u    // 'VVOL'
#define MAGIC_VTIT 0x54495456u    // 'VTIT'
#define MAGIC_VCLK 0x4B4C4356u    // 'VCLK'
#define MAGIC_VPIC 0x43495056u    // 'VPIC'
#define MAGIC_VCOV 0x564F4356u    // 'VCOV'
#define MAGIC_VFIT 0x54494656u    // 'VFIT' center crop on this glass only
#define MAGIC_VPLY 0x594C5056u    // 'VPLY' host play state
#define MAGIC_VKEY 0x59454B56u    // 'VKEY' panel key to the host
#define MAGIC_VSET 0x54455356u    // 'VSET' shuffle, repeat, wall, sleep, A/V sync
#define MAGIC_VLST 0x54534C56u    // 'VLST' favorites, radio stations, layouts
#define MAGIC_VSPC 0x43505356u    // 'VSPC' spectrum bands to the host (wall visualizer)
#define FLAG_DISCONT 0x0001u

static uint8_t W6_MAC[6];
static char g_hostLabel[16];
static uint8_t g_uid[8];

#include "w6qspi.pio.h"
#include "W6300.h"
#include "Codec.h"
static volatile uint8_t g_volume = 70;
static volatile bool g_mute = false;
static volatile bool g_volDirty = false;
static volatile uint32_t g_volDirtyAt = 0;
static volatile bool g_forceSilence = false;
static volatile bool g_fillCrop = false;
// Volume and mute live in the codec's output stages (zero-cross, 1 dB steps), so the
// samples keep all 16 bits. Only if the codec answers on I2C; otherwise the samples are scaled.
static volatile bool g_hwVol = false;   // true once the codec answered on I2C

#include "Status.h"

enum { BOOT_INIT, BOOT_LINK, BOOT_DHCP, BOOT_DHCP_FAIL, BOOT_STATIC, BOOT_READY };

static I2S i2s(OUTPUT, PIN_BCLK, PIN_DOUT, PIN_MCLK);

static int16_t g_ring[RING_FRAMES * 2];
static volatile uint32_t g_wframe = 0;
static volatile uint32_t g_rframe = 0;
static volatile uint32_t g_underrun = 0;
static volatile uint32_t g_overrun = 0;
static volatile uint32_t g_lastPkt = 0;
static volatile uint32_t g_pkts = 0;

static volatile bool g_tone = false;
// Channel test from the console, not saved: 0 both, 1 left only, 2 right only, 3 swapped.
static volatile uint8_t g_chMode = 0;
static char g_title[97];

static volatile bool g_audioReady = false;
static volatile bool g_codecOk = false;
// No WM8960 answered and nothing drives BCLK: plain I2S DACs (MAX98357A, PCM5102) hang on the
// bus, and the Pico is the master. 32-bit slots, so BCLK is 64 fs, which every such DAC takes
// (the PCM5102 makes its system clock from it), and the software volume keeps its low bits.
static volatile bool g_dacMode = false;
static int g_i2sFrameBytes = 4;            // bytes per stereo frame in the I2S queue
static int g_i2sQueueBytes = 16 * 256 * 4; // the whole queue
static volatile bool g_netReady = false;
static volatile bool g_linkUp = true;
static volatile bool g_dhcpOk = false;
static volatile uint8_t g_bootPhase = BOOT_INIT;
static volatile uint8_t g_ip[4] = {0, 0, 0, 0};
static volatile uint8_t g_mask[4] = {0, 0, 0, 0};
static volatile uint8_t g_gw[4] = {0, 0, 0, 0};

static uint8_t g_peerIp[4];
static uint16_t g_peerPort = 0;
static bool g_havePeer = false;
// Where panel keys go: the address and port the last VPLY came from (works through NAT).
static uint8_t g_ctlIp[4];
static uint16_t g_ctlPort = 0;

static int16_t g_sine[48];

#define CFG_MAGIC 0x4E4F5450u
struct PersistCfg {
    uint32_t magic;
    uint8_t netMode;
    uint8_t ip[4];
    uint8_t mask[4];
    uint8_t gw[4];
    uint8_t volume;
    uint8_t saverVu;    // 1 off; anything else (older configs too) on
};
static const PersistCfg CFG_DEFAULT = {
    CFG_MAGIC, 0, {192, 168, 10, 180}, {255, 255, 255, 0}, {192, 168, 10, 1}, 70, 0
};
static uint8_t g_netMode = 0;
static uint8_t g_cfgIp[4], g_cfgMask[4], g_cfgGw[4];

static void init_identity()
{
    pico_unique_board_id_t uid;
    pico_get_unique_board_id(&uid);
    memcpy(g_uid, uid.id, 8);
    W6_MAC[0] = (uint8_t)((uid.id[0] & 0xFE) | 0x02);
    W6_MAC[1] = uid.id[1];
    W6_MAC[2] = uid.id[2];
    W6_MAC[3] = uid.id[3];
    W6_MAC[4] = uid.id[4];
    W6_MAC[5] = uid.id[5];
    snprintf(g_hostLabel, sizeof(g_hostLabel), "audio-%02x%02x%02x",
             W6_MAC[3], W6_MAC[4], W6_MAC[5]);
}

static void applyCfg(const PersistCfg &c)
{
    g_netMode = c.netMode;
    memcpy(g_cfgIp, c.ip, 4);
    memcpy(g_cfgMask, c.mask, 4);
    memcpy(g_cfgGw, c.gw, 4);
    g_volume = c.volume > 100 ? 70 : c.volume;
    g_saverVu = c.saverVu != 1;
}
static void loadCfg()
{
    PersistCfg c;
    EEPROM.get(0, c);
    applyCfg(c.magic == CFG_MAGIC ? c : CFG_DEFAULT);
}
// Starts I2S for the hardware found at boot. The queue holds about 85 ms either way.
static bool i2sStart()
{
    if (g_dacMode) {
        i2s.setBitsPerSample(32);
        i2s.setBuffers(16, 512);
        g_i2sFrameBytes = 8;
        g_i2sQueueBytes = 16 * 512 * 4;
        return i2s.begin(SAMPLE_RATE);
    }
    i2s.setBitsPerSample(16);
    i2s.setBuffers(16, 256);
    i2s.setSlave();
    g_i2sFrameBytes = 4;
    g_i2sQueueBytes = 16 * 256 * 4;
    return i2s.begin(SAMPLE_RATE);
}

// ---- Volume log ------------------------------------------------------------------------
// saveCfg() erases the flash sector, which stalls the I2S DMA and clicks, so it only runs
// while the stream is quiet, and a volume changed during playback was lost on reset. The
// volume goes into a log in the rest of the same sector instead: programming one record into
// erased flash needs no erase and takes about a millisecond, which the I2S queue (85 ms)
// covers. 960 records; saveCfg() erases the sector, takes the volume into the config and
// starts the log over.
#define VOL_LOG_OFF 256u
#define VOL_LOG_N ((4096u - VOL_LOG_OFF) / 4u)
extern "C" uint8_t _EEPROM_start;
static uint32_t g_volLogNext = VOL_LOG_N;   // next free record (full until scanned)
static uint8_t g_volSaved;                  // the volume flash holds

static const volatile uint8_t *volLogRec(uint32_t i) { return &_EEPROM_start + VOL_LOG_OFF + i * 4u; }

// Finds the end of the log; with apply, the newest record becomes the volume.
static void volLogScan(bool apply)
{
    uint32_t i = 0;
    for (; i < VOL_LOG_N; i++) {
        const volatile uint8_t *r = volLogRec(i);
        if (r[0] == 0xFF) break;
        if (apply && r[0] == 0x5A && (uint8_t)(r[1] ^ r[2]) == 0xFF && r[1] <= 100) g_volume = r[1];
    }
    g_volLogNext = i;
    if (apply) g_volSaved = g_volume;
}

static bool volLogWrite(uint8_t vol)
{
    if (g_volLogNext >= VOL_LOG_N) return false;
    uint32_t off = VOL_LOG_OFF + g_volLogNext * 4u;
    uint32_t page = off & ~255u;
    // A whole page of 0xFF leaves every other byte as it is; only the new record is programmed.
    static uint8_t buf[256];
    memset(buf, 0xFF, sizeof(buf));
    uint8_t *r = buf + (off - page);
    r[0] = 0x5A;
    r[1] = vol;
    r[2] = (uint8_t)~vol;
    r[3] = 0x00;
    uint32_t base = (uint32_t)((uintptr_t)&_EEPROM_start - (uintptr_t)XIP_BASE);
    noInterrupts();
    rp2040.idleOtherCore();
    flash_range_program(base + page, buf, 256);
    rp2040.resumeOtherCore();
    interrupts();
    g_volLogNext++;
    g_volSaved = vol;
    return true;
}

static void pumpAudio();

static void saveCfg()
{
    PersistCfg c = {};
    c.magic = CFG_MAGIC;
    c.netMode = g_netMode;
    c.volume = g_volume;
    c.saverVu = g_saverVu ? 0 : 1;
    memcpy(c.ip, g_cfgIp, 4);
    memcpy(c.mask, g_cfgMask, 4);
    memcpy(c.gw, g_cfgGw, 4);
    EEPROM.put(0, c);
    // An unchanged config would skip the erase and leave old log records behind: touch a spare byte.
    if (g_volLogNext > 0) EEPROM.write(255, (uint8_t)(EEPROM.read(255) + 1));
    if (!g_codecOk && !g_dacMode) {
        EEPROM.commit();
        volLogScan(false);
        g_volSaved = c.volume;
        return;
    }
    // Sector erase stalls the I2S DMA IRQ and the DAC pops. Mute, clock out
    // zeros, stop the DMA, then erase while DOUT is held low.
    g_forceSilence = true;
    uint32_t t0 = millis();
    while ((uint32_t)(millis() - t0) < 100) pumpAudio();
    if (g_codecOk) wm_reg(0x05, 0x0008);
    i2s.end();
    gpio_set_function(PIN_DOUT, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_DOUT, GPIO_OUT);
    gpio_put(PIN_DOUT, 0);
    EEPROM.commit();
    volLogScan(false);
    g_volSaved = c.volume;
    i2sStart();
    t0 = millis();
    while ((uint32_t)(millis() - t0) < 20) pumpAudio();
    if (g_codecOk) wm_reg(0x05, 0x0000);
    g_forceSilence = false;
}

static bool parseIp(const char *s, uint8_t *out)
{
    int a, b, c, d;
    if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 &&
        (unsigned)a < 256 && (unsigned)b < 256 && (unsigned)c < 256 && (unsigned)d < 256) {
        out[0] = (uint8_t)a; out[1] = (uint8_t)b; out[2] = (uint8_t)c; out[3] = (uint8_t)d;
        return true;
    }
    return false;
}

// ---- I2C test (35-3.5t) -------------------------------------------------------------
// "i2ctest [n]": n writes (default 400) of the current output level to the speaker and
// headphone registers while the codec clocks run, and counts NACKs. The value written is
// what the codec already holds, so a clean bus changes nothing audible. A write can also
// arrive corrupted with an ACK (no read-back on the WM8960): power the board off and on
// after a test with failures.
static uint8_t codecVolReg(uint8_t vol, bool mute);
static void i2cTest(int n)
{
    if (!g_codecOk) { Serial.println(F("[i2ctest] no codec")); return; }
    if (n <= 0) n = 400;
    if (n > 5000) n = 5000;
    uint16_t lvl = g_hwVol ? (uint16_t)(0x0080 | codecVolReg(g_volume, g_mute)) : 0x0079;
    static const uint8_t regs[4] = { 0x28, 0x29, 0x02, 0x03 };
    int okCnt = 0, addrFail = 0, dataFail = 0, firstFail = -1;
    Serial.printf("[i2ctest] %d writes of 0x%03X (%s volume), clocks running\n", n, lvl, g_hwVol ? "codec" : "software");
    for (int i = 0; i < n; i++) {
        uint8_t r = regs[i & 3];
        uint16_t v = (r == 0x29 || r == 0x03) ? (uint16_t)(0x0100 | lvl) : lvl;
        int e = wm_write(r, v);
        if (e == 0) okCnt++;
        else {
            if (e == 2) addrFail++; else dataFail++;
            if (firstFail < 0) firstFail = i;
        }
        uint32_t t0 = micros();
        while ((uint32_t)(micros() - t0) < 2000) pumpAudio();
    }
    Serial.printf("[i2ctest] ok %d  addr-NACK %d  data-NACK %d  first fail at %d\n", okCnt, addrFail, dataFail, firstFail);
    Serial.println(okCnt == n ? F("[i2ctest] CLEAN - codec volume is safe on this wiring")
                              : F("[i2ctest] FAILURES - keep software volume; power-cycle the board now"));
}

static void handleLine(char *line)
{
    while (*line == ' ') line++;
    if (!*line) return;
    if (!strcmp(line, "tone")) {
        g_tone = true;
        Serial.printf("[audio] 1 kHz, i2s space %d\n", i2s.availableForWrite());
        return;
    }
    if (!strncmp(line, "ch", 2) && (line[2] == 0 || line[2] == ' ')) {
        const char *a = line + 2;
        while (*a == ' ') a++;
        if (!strcmp(a, "l")) g_chMode = 1;
        else if (!strcmp(a, "r")) g_chMode = 2;
        else if (!strcmp(a, "swap")) g_chMode = 3;
        else if (!strcmp(a, "lr") || !strcmp(a, "both")) g_chMode = 0;
        else if (*a) { Serial.println(F("[ch] use: ch l | ch r | ch lr | ch swap")); return; }
        static const char *const names[4] = { "both", "LEFT only", "RIGHT only", "SWAPPED" };
        Serial.printf("[ch] %s (tone and stream; 'tone' for 1 kHz, 'stop' to end)\n", names[g_chMode]);
        return;
    }
    if (!strncmp(line, "i2ctest", 7)) { i2cTest(atoi(line + 7)); return; }
    if (!strcmp(line, "stop")) { g_tone = false; Serial.println(F("[audio] tone off")); return; }
    if (!strcmp(line, "mute")) { g_mute = !g_mute; return; }
    if (!strncmp(line, "vol", 3)) {
        int v = atoi(line + 3);
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        g_volume = (uint8_t)v;
        g_mute = false;
        Serial.printf("[audio] vol %u\n", g_volume);
        return;
    }
    if (!strcmp(line, "save")) { saveCfg(); Serial.println(F("[cfg] saved")); return; }
    if (!strncmp(line, "spi", 3)) {
        int mhz = atoi(line + 3);
        st_spi((uint32_t)mhz * 1000000u);
        Serial.printf("[lcd] spi %lu MHz\n", (unsigned long)(g_spiHz / 1000000));
        return;
    }
    if (!strcmp(line, "reboot")) { Serial.println(F("[audio] reboot")); delay(20); rp2040.reboot(); return; }
    if (!strcmp(line, "applynet dhcp")) { g_netMode = 0; saveCfg(); rp2040.reboot(); return; }
    if (!strncmp(line, "applynet static", 15)) {
        char ip[20], mk[20], gw[20];
        if (sscanf(line + 15, "%19s %19s %19s", ip, mk, gw) == 3 &&
            parseIp(ip, g_cfgIp) && parseIp(mk, g_cfgMask) && parseIp(gw, g_cfgGw)) {
            g_netMode = 1;
            saveCfg();
            rp2040.reboot();
        } else Serial.println(F("usage: applynet static <ip> <mask> <gw>"));
        return;
    }
    if (!strcmp(line, "net")) {
        Serial.printf("[net] %s  %u.%u.%u.%u\n", g_netMode ? "static" : "dhcp",
                      g_ip[0], g_ip[1], g_ip[2], g_ip[3]);
        return;
    }
    if (!strcmp(line, "net dhcp")) { g_netMode = 0; Serial.println(F("[net] DHCP (save, reboot)")); return; }
    if (!strncmp(line, "net static", 10)) {
        char ip[20], mk[20], gw[20];
        if (sscanf(line + 10, "%19s %19s %19s", ip, mk, gw) == 3 &&
            parseIp(ip, g_cfgIp) && parseIp(mk, g_cfgMask) && parseIp(gw, g_cfgGw)) {
            g_netMode = 1;
            Serial.println(F("[net] static set (save, reboot)"));
        } else Serial.println(F("usage: net static <ip> <mask> <gw>"));
        return;
    }
    Serial.println(F("tone | stop | vol <0-100> | mute | spi <8-62> | net | net dhcp | net static <ip> <mask> <gw> | save"));
}

static void pollSerial()
{
    static char line[72];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (n) { line[n] = 0; n = 0; handleLine(line); }
            continue;
        }
        if (n < sizeof(line) - 1) line[n++] = c;
    }
}

static void pollButton()
{
    static bool down = false;
    static uint32_t t = 0;
    bool pressed = gpio_get(PIN_BTN) == 0;
    if (pressed && !down) t = millis();
    if (!pressed && down) {
        uint32_t held = (uint32_t)(millis() - t);
        if (held > 25 && held < 1500) {
            g_mute = !g_mute;
            Serial.printf("[audio] %s\n", g_mute ? "mute" : "unmute");
        }
    }
    down = pressed;
}

// WM8960 LOUT1/ROUT1 (headphone) and LOUT2/ROUT2 (speaker): 0x79 = 0 dB, 1 dB per step,
// 0x30 = -73 dB, below that mute. Same curve as the old sample scale: 70 → about -4.5 dB.
static uint8_t codecVolReg(uint8_t vol, bool mute)
{
    if (mute || vol == 0) return 0x00;
    float db = 40.0f * log10f((float)vol / 90.5f);
    if (db > 0.0f) db = 0.0f;
    int r = 0x79 + (int)lroundf(db);
    if (r < 0x30) r = 0x30;
    return (uint8_t)r;
}

// Core 0 only. One write is about 0.7 ms with interrupts off; the I2S DMA queue holds 85 ms.
static void codecVolumeService()
{
    static int applied = -1;
    static bool probed = false;
    static uint32_t lastAt = 0;
    if (!g_codecOk) return;
    if (!probed) {
        // Right after a cold init the outputs are still settling. Give the probe a second
        // and up to ten tries 100 ms apart before falling back to software volume.
        static uint8_t tries = 0;
        static uint32_t nextTry = 0;
        if (!nextTry) nextTry = millis() + 1000;
        if ((int32_t)(millis() - nextTry) < 0) return;
        nextTry = millis() + 100;
        if (wm_write(0x17, 0x01C1) != 0 && ++tries < 10) return;

        // Ten NACKs in a row: volume stays in the samples, as in 35-2.6 and earlier.
        probed = true;
        // Zero-cross needs the slow clock, or a change waits for a crossing that silence never has.
        g_hwVol = tries < 10;
        Serial.println(g_hwVol ? F("[codec] volume in codec (I2C answers)")
                               : F("[codec] volume in software (codec busy on I2C while clocks run)"));
        lastAt = millis();
    }
    if (!g_hwVol) return;
    static uint8_t fails = 0;
    uint8_t want = codecVolReg(g_volume, g_mute);
    // The WM8960 cannot be read back, and a write can be ACKed but land wrong,
    // which leaves one channel louder. Rewrite all four every 2 s; zero-cross
    // keeps an unchanged value silent.
    bool refresh = (uint32_t)(millis() - lastAt) >= 2000;
    if (want == applied && !refresh) return;
    if ((uint32_t)(millis() - lastAt) < 30) return;
    lastAt = millis();
    // Every write is sent even if an earlier one failed, so no channel is left behind.
    // Left without the update bit, then right with it: both channels latch together.
    uint16_t zc = (uint16_t)(0x0080 | want);
    bool ok = wm_reg(0x28, zc);
    ok = wm_reg(0x29, (uint16_t)(0x0100 | zc)) && ok;
    ok = wm_reg(0x02, zc) && ok;
    ok = wm_reg(0x03, (uint16_t)(0x0100 | zc)) && ok;
    if (ok) {
        applied = want;
        fails = 0;
        return;
    }
    // Try again on the next pass (30 ms). Only a codec that stays silent loses the volume.
    applied = -1;
    if (++fails < 20) return;
    g_hwVol = false;
    Serial.println(F("[codec] volume write failed 20 times, scaling samples instead"));
}

static int16_t applyGain(int16_t s)
{
    if (g_forceSilence) return 0;
    if (g_hwVol) return s;
    if (g_mute || g_volume == 0) return 0;
    // 100 → unity, 70 → about −4.5 dB, 20 → about −26 dB.
    int gain = (int)g_volume * (int)g_volume;
    gain >>= 5;
    if (gain > 256) gain = 256;
    return (int16_t)((s * gain) >> 8);
}

// The same curve as applyGain, 256 times finer, so quiet settings keep their low bits.
static int32_t applyGain32(int16_t s)
{
    if (g_forceSilence || g_mute || g_volume == 0) return 0;
    int32_t gain = (int32_t)g_volume * (int32_t)g_volume * 8;   // 100 → unity (65536)
    if (gain > 65536) gain = 65536;
    return (int32_t)s * gain;   // at most 2^31 in size, fits
}

static int16_t toneSample()
{
    static uint8_t i = 0;
    int16_t s = g_sine[i];
    if (++i >= 48) i = 0;
    return s;
}

static void notePeak(int16_t l, int16_t r)
{
    uint16_t a = (uint16_t)(l < 0 ? -(int32_t)l : l);
    uint16_t b = (uint16_t)(r < 0 ? -(int32_t)r : r);
    if (b > a) a = b;
    if (a > g_vuPeak) g_vuPeak = a;
}


// ---- Clock drift -----------------------------------------------------------------------
// The codec crystal and the host clock never run at exactly the same rate. Left alone, the
// buffer slowly fills (late sound, then overruns) or drains (underruns) over a long film.
// About two seconds after a stream starts, the total buffer (ring + I2S queue) becomes the
// target. When it wanders more than 1 ms, playback runs 200 ppm faster or slower through a
// 4-point Hermite interpolator until it is back; then samples pass through untouched again.
#define DRIFT_STEP 858993            // 200 ppm of 2^32
#define DRIFT_START 48               // frames off target before correcting (1 ms)
static volatile uint32_t g_flushAt;  // core 1 asks core 0 to drop the ring up to here
static volatile uint8_t g_flushSeq, g_flushSeen;
static volatile int32_t g_driftPpm;
static volatile uint32_t g_driftFixes;
static volatile uint32_t g_bufFrames;

static inline int16_t hermite(int16_t p0, int16_t p1, int16_t p2, int16_t p3, float t)
{
    float c1 = 0.5f * (float)(p2 - p0);
    float c2 = (float)p0 - 2.5f * (float)p1 + 2.0f * (float)p2 - 0.5f * (float)p3;
    float c3 = 0.5f * (float)(p3 - p0) + 1.5f * (float)(p1 - p2);
    float y = ((c3 * t + c2) * t + c1) * t + (float)p1;
    if (y > 32767.0f) return 32767;
    if (y < -32768.0f) return -32768;
    return (int16_t)lrintf(y);
}

static void pumpAudio()
{
    static bool starved = true;
    static bool fromTone = false;
    static int16_t holdL = 0, holdR = 0;
    static int16_t hl[4], hr[4];
    static uint8_t have = 0;
    static uint32_t frac = 0;
    static int32_t adj = 0;
    static bool stopAdj = false;
    static uint32_t ctlAt = 0, settleAt = 0;
    static float ema = 0.0f;
    static int32_t target = -1;
    static int64_t slipSum = 0;
    static uint32_t outCount = 0;

    uint8_t fs = g_flushSeq;
    if (fs != g_flushSeen) {
        __dmb();
        g_rframe = g_flushAt;
        g_flushSeen = fs;
        have = 0;
        frac = 0;
        adj = 0;
        stopAdj = false;
        target = -1;
        settleAt = millis() + 2000;
    }

    uint32_t now = millis();
    if ((uint32_t)(now - ctlAt) >= 10) {
        ctlAt = now;
        int avail = i2s.availableForWrite();
        int32_t queued = (int32_t)(g_wframe - g_rframe) + have + (g_i2sQueueBytes - avail) / g_i2sFrameBytes;
        bool live = !g_tone && g_lastPkt && (uint32_t)(now - g_lastPkt) < TARGET_IDLE_MS;
        if (!live || starved) {
            target = -1;
            settleAt = now + 2000;
            ema = (float)queued;
            if (!live) { adj = 0; stopAdj = false; frac = 0; }
        } else {
            ema += ((float)queued - ema) * (1.0f / 64.0f);
            if (target < 0) {
                if ((int32_t)(now - settleAt) >= 0) {
                    target = (int32_t)ema;
                    slipSum = 0;
                    outCount = 0;
                }
            } else {
                float err = ema - (float)target;
                if (adj == 0) {
                    if (err > DRIFT_START) { adj = DRIFT_STEP; g_driftFixes++; }
                    else if (err < -DRIFT_START) { adj = -DRIFT_STEP; g_driftFixes++; }
                } else if (!stopAdj && ((adj > 0 && err < 4.0f) || (adj < 0 && err > -4.0f))) {
                    stopAdj = true;
                }
                if (outCount > 96000u)
                    g_driftPpm = (int32_t)((double)slipSum / 4294967296.0 / (double)outCount * 1e6);
            }
        }
        g_bufFrames = (uint32_t)(queued > 0 ? queued : 0);
    }

    for (int n = 0; n < 256 && i2s.availableForWrite() >= g_i2sFrameBytes; n++) {
        int16_t l = 0, r = 0;
        if (g_tone) {
            l = r = toneSample();
            starved = false;
            fromTone = true;
            holdL = l;
            holdR = r;
            have = 0;
            frac = 0;
        } else {
            uint32_t rf = g_rframe;
            uint32_t wf = g_wframe;
            while (have < 4 && rf != wf) {
                uint32_t i = rf & (RING_FRAMES - 1);
                hl[have] = g_ring[i * 2];
                hr[have] = g_ring[i * 2 + 1];
                have++;
                rf++;
            }
            if (have == 4) {
                if (frac == 0 && adj == 0) {
                    l = hl[1];
                    r = hr[1];
                } else {
                    float t = (float)frac * (1.0f / 4294967296.0f);
                    l = hermite(hl[0], hl[1], hl[2], hl[3], t);
                    r = hermite(hr[0], hr[1], hr[2], hr[3], t);
                }
                uint64_t acc = (uint64_t)frac + 4294967296ull + (uint64_t)(int64_t)adj;
                uint32_t steps = (uint32_t)(acc >> 32);
                frac = (uint32_t)acc;
                if (adj) slipSum += adj;
                outCount++;
                // Back on target: finish at the next whole sample, then pass samples through.
                if (stopAdj && (frac < (1u << 26) || frac > 0xFC000000u)) {
                    if (frac > 0xFC000000u) steps++;
                    frac = 0;
                    adj = 0;
                    stopAdj = false;
                }
                for (uint32_t k = 0; k < steps && have > 0; k++) {
                    hl[0] = hl[1]; hl[1] = hl[2]; hl[2] = hl[3];
                    hr[0] = hr[1]; hr[1] = hr[2]; hr[2] = hr[3];
                    have--;
                }
                while (have < 4 && rf != wf) {
                    uint32_t i = rf & (RING_FRAMES - 1);
                    hl[have] = g_ring[i * 2];
                    hr[have] = g_ring[i * 2 + 1];
                    have++;
                    rf++;
                }
                g_rframe = rf;
                starved = false;
                fromTone = false;
                holdL = l;
                holdR = r;
                st_spec_push(l, r);
            } else {
                g_rframe = rf;
                // A hard jump to zero is the click. Fade the last sample out instead.
                if (!starved && !fromTone) g_underrun++;
                starved = true;
                fromTone = false;
                holdL = (int16_t)((holdL * 3) / 4);
                holdR = (int16_t)((holdR * 3) / 4);
                l = holdL;
                r = holdR;
            }
        }
        switch (g_chMode) {
        case 1: r = 0; break;
        case 2: l = 0; break;
        case 3: { int16_t t = l; l = r; r = t; } break;
        }
        notePeak(l, r);
        if (g_dacMode) i2s.write32(applyGain32(l), applyGain32(r));
        else i2s.write16(applyGain(l), applyGain(r));
    }
}


// The info lines on the MORE page.
static void hud_stats(bool force)
{
    static uint32_t last = 0;
    if (g_saver || !g_netReady || g_page != PAGE_MORE) return;
    uint32_t now = millis();
    if (!force && (uint32_t)(now - last) < 1000) return;
    last = now;
    unsigned ms = (unsigned)(g_bufFrames * 1000u / 48000u);
    unsigned up = (unsigned)(now / 1000u);
    char l[5][40];
    snprintf(l[0], 40, "%u.%u.%u.%u %s  VOL %s", g_ip[0], g_ip[1], g_ip[2], g_ip[3],
             g_netMode ? "STATIC" : "DHCP", g_hwVol ? "CODEC" : g_dacMode ? "DAC" : "SOFT");
    snprintf(l[1], 40, "SPI %u  BUF %uMS  DRIFT %+ldPPM/%lu", (unsigned)(g_spiHz / 1000000u), ms,
             (long)g_driftPpm, (unsigned long)g_driftFixes);
    snprintf(l[2], 40, "UP %uM%02u  UND %lu  OVR %lu  %s", up / 60u, up % 60u,
             (unsigned long)g_underrun, (unsigned long)g_overrun, FW_VERSION);
    l[3][0] = 0;
    l[4][0] = 0;
    st_info_lines(l, force);
}

static bool audioLive()
{
    if (g_tone) return true;
    return g_lastPkt && (uint32_t)(millis() - g_lastPkt) < 2000;
}

static bool bootIntro();

static void paint()
{
    bool playing = g_netReady && audioLive();
    // Something worth the whole screen: sound, pictures of the wall, or a wall view opened by tap.
    bool wall = g_netReady && st_pic_live();
    bool show = playing || wall || g_wallView;
    if (g_saver && !g_wander && !show)
        st_leave_saver();
    else if (g_saver && g_wander && playing)
        st_leave_saver();
    else if (g_saver && g_wander && wall) {
        // the wandering clock gives way to the wall picture, still as screensaver
        g_wander = false;
        g_saverClear = true;
        g_saverKey = -1;
    }
    if (st_in_saver()) {
        if (!show && !g_wander) {
            g_wander = true;
            g_saverClear = true;
            g_saverKey = -1;
            g_wanderX = -1;
        }
        st_draw_saver();
        return;
    }
    g_wander = false;
    char ip[20], state[24], mid[28], tail[28];
    if (!g_netReady || bootIntro()) return;   // boot screen and intro: bootAnim()
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", g_ip[0], g_ip[1], g_ip[2], g_ip[3]);
    bool live = g_lastPkt && (uint32_t)(millis() - g_lastPkt) < TARGET_IDLE_MS;
    uint16_t col = ST_DIM;
    if (!g_linkUp) { snprintf(state, sizeof(state), "NO LINK"); col = ST_RED; }
    else if (!g_codecOk && !g_dacMode) { snprintf(state, sizeof(state), "NO CODEC"); col = ST_RED; }
    else if (g_tone) { snprintf(state, sizeof(state), "TONE"); col = ST_YELLOW; }
    else if (g_mute) { snprintf(state, sizeof(state), "MUTE"); col = ST_YELLOW; }
    else if (st_play_state() == 2) { snprintf(state, sizeof(state), "PAUSE"); col = ST_YELLOW; }
    else if (live) { snprintf(state, sizeof(state), "PLAY"); col = ST_GREEN; }
    else { snprintf(state, sizeof(state), "IDLE"); col = ST_DIM; }

    snprintf(mid, sizeof(mid), "%uM", (unsigned)(g_spiHz / 1000000u));
    snprintf(tail, sizeof(tail), "%s", g_netMode ? "STATIC" : "DHCP");
    st_set_scroll(live ? g_title : "");
    static char shown[120];
    static char stateShown[24];
    if (st_take_wake()) { shown[0] = 0; stateShown[0] = 0; }
    char sig[120];
    snprintf(sig, sizeof(sig), "%s|%s|%s|%s", g_hostLabel, ip, mid, tail);
    if (strcmp(sig, shown) != 0) {
        memcpy(shown, sig, sizeof(shown));
        memcpy(stateShown, state, sizeof(stateShown));
        st_frame(g_hostLabel, "", state, col, mid, tail);
        hud_stats(true);
        return;
    }
    if (strcmp(state, stateShown) != 0) {
        memcpy(stateShown, state, sizeof(stateShown));
        st_state(state, col);
        st_mute_button();
    }
}

#include "Dhcp.h"
#include "WebServer.h"

static bool udp_send(const uint8_t *dip, uint16_t dport, const uint8_t *data, uint16_t len)
{
    if (w6_rd8(BSB_S0_REG, 0x0010)) return false;    // command still running
    w6_write(BSB_S0_REG, 0x0120, dip, 4);            // Sn_DIPR
    w6_wr16(BSB_S0_REG, 0x0140, dport);              // Sn_DPORTR
    uint16_t wr = w6_rd16(BSB_S0_REG, 0x020C);
    w6_write(BSB_S0_TX, wr, data, len);
    w6_wr16(BSB_S0_REG, 0x020C, (uint16_t)(wr + len));
    w6_wr8(BSB_S0_REG, 0x0010, 0x20);                // SEND
    return true;
}

// Core 1: the spectrum for the wall visualizer, when it changed.
static void sendSpectrum()
{
    static uint8_t sent = 0;
    uint8_t seq = g_spcSeq;
    if (seq == sent || !g_ctlPort) return;
    uint8_t b[6 + SPEC_BANDS];
    uint32_t magic = MAGIC_VSPC;
    memcpy(b, &magic, 4);
    b[4] = seq;
    b[5] = SPEC_BANDS;
    memcpy(b + 6, g_spcOut, SPEC_BANDS);
    if (udp_send(g_ctlIp, g_ctlPort, b, sizeof(b))) sent = seq;
}

// Core 1: a panel key goes out three times; the host drops repeats by sequence.
static void sendKey()
{
    if (!g_keyLeft || !g_ctlPort) return;
    uint8_t b[10];
    uint32_t magic = MAGIC_VKEY;
    uint16_t arg = g_keyArg;
    memcpy(b, &magic, 4);
    b[4] = g_keyCode;
    b[5] = g_keySeq;
    memcpy(b + 6, &arg, 2);
    b[8] = g_keyGen;
    b[9] = 0;
    if (udp_send(g_ctlIp, g_ctlPort, b, sizeof(b))) g_keyLeft = (uint8_t)(g_keyLeft - 1);
}

static void sendFeedback()
{
    if (!g_havePeer) return;
    uint8_t b[12];
    uint32_t magic = MAGIC_PTON;
    uint16_t fill = (uint16_t)(g_wframe - g_rframe);
    uint16_t und = (uint16_t)g_underrun;
    uint16_t ovr = (uint16_t)g_overrun;
    bool live = g_lastPkt && (uint32_t)(millis() - g_lastPkt) < TARGET_IDLE_MS;
    uint16_t flags = (live ? 1u : 0u) | (g_linkUp ? 2u : 0u) | (g_mute ? 4u : 0u);
    memcpy(b, &magic, 4);
    memcpy(b + 4, &fill, 2);
    memcpy(b + 6, &und, 2);
    memcpy(b + 8, &ovr, 2);
    memcpy(b + 10, &flags, 2);
    udp_send(g_peerIp, g_peerPort, b, sizeof(b));
}

// W6300 UDP packet-info: length in [0:1] (same as the panel), peer IPv4 at [2:5],
// source port big-endian at [6:7].
static void notePeer(const uint8_t *hdr)
{
    memcpy(g_peerIp, hdr + 2, 4);
    g_peerPort = (uint16_t)((hdr[6] << 8) | hdr[7]);
    g_havePeer = g_peerPort != 0;
}

// hdr is null for the picture socket: feedback keeps going to the audio sender.
static void takePacket(const uint8_t *hdr, const uint8_t *pl, uint16_t dsize)
{
    if (dsize < 4) return;
    uint32_t magic;
    memcpy(&magic, pl, 4);
    if (hdr) notePeer(hdr);

    if (magic == MAGIC_VVOL && dsize >= 6) {
        uint8_t vol = pl[4];
        if (vol > 100) vol = 100;
        g_volume = vol;
        g_mute = pl[5] != 0;
        return;
    }
    if (magic == MAGIC_VTIT && dsize > 4) {
        uint16_t n = (uint16_t)(dsize - 4);
        if (n > 96) n = 96;
        memcpy(g_title, pl + 4, n);
        g_title[n] = 0;
        st_set_scroll(g_title);
        return;
    }
    if (magic == MAGIC_VCLK && dsize >= 7) {
        st_set_clock(pl[4], pl[5], pl[6]);
        return;
    }
    if (magic == MAGIC_VPIC && dsize > 12) {
        st_pic_chunk(pl + 4, (uint16_t)(dsize - 4));
        return;
    }
    if (magic == MAGIC_VCOV && dsize >= 8) {
        st_cover_chunk(pl + 4, (uint16_t)(dsize - 4));
        return;
    }
    if (magic == MAGIC_VFIT && dsize >= 5) {
        g_fillCrop = pl[4] != 0;
        return;
    }
    if (magic == MAGIC_VSET && dsize >= 10) {
        st_set_note(pl + 4, (uint16_t)(dsize - 4));
        return;
    }
    if (magic == MAGIC_VLST && dsize >= 12) {
        st_list_chunk(pl + 4, (uint16_t)(dsize - 4));
        return;
    }
    if (magic == MAGIC_VPLY && dsize >= 16) {
        uint32_t pos, dur;
        memcpy(&pos, pl + 8, 4);
        memcpy(&dur, pl + 12, 4);
        st_play_note(pl[4], pl[5], pos, dur);
        if (hdr) {
            memcpy(g_ctlIp, hdr + 2, 4);
            g_ctlPort = (uint16_t)((hdr[6] << 8) | hdr[7]);
        }
        return;
    }
    if (magic != MAGIC_PTON || dsize < HDR_BYTES + 8) return;
    uint16_t flags;
    memcpy(&flags, pl + 12, 2);
    uint32_t nbytes = (uint32_t)(dsize - HDR_BYTES);
    uint32_t frames = nbytes / 4;
    if (frames == 0 || frames > PACKET_FRAMES) return;

    uint32_t wf = g_wframe;
    uint32_t rf = g_rframe;
    // g_rframe belongs to core 0. A discontinuity asks core 0 to drop the ring; until it has,
    // count the free space as if it had.
    if (g_flushSeq != g_flushSeen) rf = g_flushAt;
    if (flags & FLAG_DISCONT) {
        g_flushAt = wf;
        __dmb();
        g_flushSeq = (uint8_t)(g_flushSeq + 1);
        rf = wf;
    }
    // Drop this packet when the ring is full. Skipping ahead to keep the newest
    // samples plays audio from the future and leaves the picture behind.
    if ((wf - rf) + frames > RING_FRAMES) { g_overrun++; return; }

    const int16_t *pcm = (const int16_t *)(pl + HDR_BYTES);
    for (uint32_t i = 0; i < frames; i++) {
        uint32_t slot = (wf + i) & (RING_FRAMES - 1);
        g_ring[slot * 2] = pcm[i * 2];
        g_ring[slot * 2 + 1] = pcm[i * 2 + 1];
    }
    __sync_synchronize();
    g_wframe = wf + frames;
    g_lastPkt = millis();
    g_pkts++;
    g_tone = false;
}

static void phy_poll()
{
    static uint32_t last = 0, downSince = 0, upSince = 0;
    uint32_t now = millis();
    if ((uint32_t)(now - last) < 100) return;
    last = now;
    bool up = (w6_rd8(BSB_COMMON, 0x3000) & 1) != 0;
    if (up) {
        downSince = 0;
        if (!g_linkUp) {
            if (!upSince) upSince = now;
            if ((uint32_t)(now - upSince) >= 300) { g_linkUp = true; upSince = 0; }
        }
    } else {
        upSince = 0;
        if (g_linkUp) {
            if (!downSince) downSince = now;
            if ((uint32_t)(now - downSince) >= 500) { g_linkUp = false; downSince = 0; }
        }
    }
}

static uint8_t g_pkt[8 + HDR_BYTES + PACKET_BYTES];

// Reads up to budget datagrams from one UDP socket. Returns how many were taken.
static int udp_drain(uint8_t regBsb, uint8_t rxBsb, int budget, bool audio)
{
    int taken = 0;
    uint16_t rsr = w6_rd16(regBsb, 0x0224);
    uint16_t rd = w6_rd16(regBsb, 0x0228);
    while (rsr >= 8 && taken < budget) {
        w6_read(rxBsb, rd, g_pkt, 8);
        uint16_t dsize = (uint16_t)(((g_pkt[0] & 0x07) << 8) | g_pkt[1]);
        if (dsize < 4 || dsize > HDR_BYTES + PACKET_BYTES) {
            rd = (uint16_t)(rd + rsr);
            w6_wr16(regBsb, 0x0228, rd);
            w6_wr8(regBsb, 0x0010, 0x40);
            break;
        }
        if ((uint32_t)8 + dsize > rsr) break;
        w6_read(rxBsb, (uint16_t)(rd + 8), g_pkt + 8, dsize);
        rd = (uint16_t)(rd + 8 + dsize);
        w6_wr16(regBsb, 0x0228, rd);
        w6_wr8(regBsb, 0x0010, 0x40);
        rsr = (uint16_t)(rsr - (8 + dsize));
        takePacket(audio ? g_pkt : nullptr, g_pkt + 8, dsize);
        taken++;
    }
    return taken;
}

void setup()
{
    // Leave the boot clock alone. setSysClk() after USB is up drops the COM port.
    // At any freq other than 150 MHz the SDK parks clk_peri on the 48 MHz USB PLL,
    // which caps SPI at 24 MHz. Put it back on clk_sys so 120 MHz gives 60 MHz SPI.
    // Only SPI and UART run from clk_peri; USB serial and PIO do not.
    clock_configure_undivided(clk_peri, 0,
                              CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                              clock_get_hz(clk_sys));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.begin(115200);
    delay(300);
    LittleFS.begin();
    init_identity();
    EEPROM.begin(64);
    loadCfg();
    volLogScan(true);

    for (int i = 0; i < 48; i++)
        g_sine[i] = (int16_t)(sinf(2.0f * 3.14159265f * i / 48.0f) * 8000.0f);

    pinMode(PIN_BTN, INPUT_PULLUP);
    gpio_set_function(PIN_BTN, GPIO_FUNC_SIO);
    gpio_pull_up(PIN_BTN);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
    g_codecOk = false;
    uint32_t bclkEdges = i2s_bclk_edges();
    bool codecLive = bclkEdges > 100;
    if (codecLive) {
        // Already master from the last run. I2C NACKs while those clocks run,
        // and the registers are already programmed, so leave the chip alone.
        g_codecOk = true;
    } else {
        // After a power cut the HAT can come up later than the Pico. Keep trying for 3 s.
        uint32_t t0 = millis();
        while (!g_codecOk && (uint32_t)(millis() - t0) < 3000) {
            bb_recover();
            g_codecOk = wm_init();
            if (!g_codecOk) delay(100);
        }
        if (g_codecOk && millis() - t0 > 200)
            Serial.printf("[codec] answered after %lu ms\n", (unsigned long)(millis() - t0));
    }

    st_yield = pumpAudio;
    st_init();
    // The volume goes to the codec output stages from loop() (codecVolumeService).
    // Without a codec answer and with BCLK quiet, plain DACs are on the bus: the Pico drives
    // the clocks. (A WM8960 that did not answer is still a slave after power-up, so it never
    // drives against the Pico.)
    g_dacMode = !g_codecOk && !codecLive;
    bool i2sOk = i2sStart();

    const char *codecState = g_dacMode ? "none, I2S DAC master" : "MISSING";
    if (g_codecOk) codecState = codecLive ? "ok live" : "ok master";
    Serial.printf("[audio] %s  %s  codec %s  edges %lu  i2s %s  space %d  sys %lu Hz\n",
                  g_hostLabel, FW_VERSION, codecState, (unsigned long)bclkEdges,
                  i2sOk ? "ok" : "FAIL", i2s.availableForWrite(),
                  (unsigned long)clock_get_hz(clk_sys));
    Serial.printf("[lcd] spi %lu Hz (asked %lu, peri %lu Hz)\n", (unsigned long)g_spiHz,
                  (unsigned long)g_spiWant, (unsigned long)clock_get_hz(clk_peri));
    g_audioReady = true;
}

// ======================================================================================
// Boot screen and intro, in the style of the LED panels: a rainbow frame, the name in
// rainbow letters, the current step with running dots, a bouncing bar. Once the network is
// up, the address in large rainbow digits for a few seconds, then the menu.
// ======================================================================================
#define INTRO_MS 4000u
static uint32_t g_introStart;   // 0 until the network is ready

static uint16_t bootHue(uint8_t h)
{
    uint8_t region = h / 43, rem = (uint8_t)((h - region * 43) * 6);
    uint8_t q = (uint8_t)(255 - rem), t = rem;
    uint8_t r, g, b;
    switch (region) {
    case 0: r = 255; g = t; b = 0; break;
    case 1: r = q; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = t; break;
    case 3: r = 0; g = q; b = 255; break;
    case 4: r = t; g = 0; b = 255; break;
    default: r = 255; g = 0; b = q; break;
    }
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void bootBorder(int hue)
{
    const int seg = 16, th = 4;
    for (int x = 0; x < ST_W; x += seg) {
        uint16_t c = bootHue((uint8_t)(x / 2 + hue));
        st_fill(x, 0, seg, th, c);
        st_fill(ST_W - seg - x, ST_H - th, seg, th, bootHue((uint8_t)(x / 2 + hue + 128)));
    }
    for (int y = th; y < ST_H - th; y += seg) {
        int h = y + seg > ST_H - th ? ST_H - th - y : seg;
        st_fill(0, y, th, h, bootHue((uint8_t)(y / 2 + hue + 192)));
        st_fill(ST_W - th, ST_H - th - y - h + th, th, h, bootHue((uint8_t)(y / 2 + hue + 64)));
    }
}

static int bootTextW(const char *t, int scale) { return (int)strlen(t) * 6 * scale - scale; }

static void bootRainbow(int y, const char *t, int scale, int hue)
{
    int x = (ST_W - bootTextW(t, scale)) / 2;
    for (int i = 0; t[i]; i++, x += 6 * scale)
        st_glyph(x, y, t[i], bootHue((uint8_t)(hue + i * 14)), ST_BLACK, scale);
}

// A centred line that is cleared over the whole width, so a shorter text leaves nothing behind.
static void bootLine(int y, const char *t, uint16_t color, int scale)
{
    st_fill(8, y, ST_W - 16, 7 * scale, ST_BLACK);
    st_text((ST_W - bootTextW(t, scale)) / 2, y, t, color, ST_BLACK, scale);
}

static void bootBar(int pos, bool determinate, int hue)
{
    const int x0 = 40, y0 = 262, w = ST_W - 80, h = 8, span = 96;
    st_fill(x0, y0, w, h, ST_TRACK);
    if (determinate) {
        int f = pos < 0 ? 0 : pos > w ? w : pos;
        for (int i = 0; i < f; i += 8) st_fill(x0 + i, y0, f - i < 8 ? f - i : 8, h, bootHue((uint8_t)(i / 2 - hue)));
        return;
    }
    int travel = w - span;
    int p = pos % (2 * travel);
    if (p > travel) p = 2 * travel - p;
    for (int i = 0; i < span; i += 8) st_fill(x0 + p + i, y0, 8, h, bootHue((uint8_t)(i * 2 - hue)));
}

static const char *bootStep(uint8_t phase)
{
    switch (phase) {
    case BOOT_LINK: return "WAITING FOR LINK";
    case BOOT_DHCP: return "ACQUIRING IP VIA DHCP";
    case BOOT_DHCP_FAIL: return "DHCP FAILED - FALLBACK";
    case BOOT_STATIC: return "STATIC IP CONFIG";
    case BOOT_READY: return "NETWORK READY";
    default: return "STARTING UP";
    }
}

// True while the intro still runs (after the network came up).
static bool bootIntro()
{
    if (!g_netReady) return false;
    if (!g_introStart) g_introStart = millis() | 1;
    return (uint32_t)(millis() - g_introStart) < INTRO_MS;
}

static void bootAnim()
{
    static uint32_t nextFrame = 0;
    static bool cleared = false, introShown = false;
    static char stepShown[40], hostShown[40];
    uint32_t now = millis();
    if ((int32_t)(now - nextFrame) < 0) return;
    nextFrame = now + 50;
    int hue = (int)(now / 12);
    bool intro = g_netReady;

    if (!cleared || (intro && !introShown)) {
        st_fill(0, 0, ST_W, ST_H, ST_BLACK);
        cleared = true;
        introShown = intro;
        stepShown[0] = hostShown[0] = 0;
    }
    bootBorder(hue);
    bootRainbow(36, "VERPIXELD AUDIO", 4, hue);

    char line[40];
    if (!intro) {
        uint8_t phase = g_bootPhase;
        bool busy = phase == BOOT_INIT || phase == BOOT_LINK || phase == BOOT_DHCP;
        snprintf(line, sizeof(line), "%s%.*s", bootStep(phase), busy ? (int)((now / 400) % 4) : 0, "...");
        if (strcmp(line, stepShown)) {
            strncpy(stepShown, line, sizeof(stepShown) - 1);
            bootLine(124, line, phase == BOOT_DHCP_FAIL ? ST_RED : ST_WHITE, 2);
        }
        char host[40];
        snprintf(host, sizeof(host), "%s.local", g_hostLabel);
        if (strcmp(host, hostShown)) {
            strncpy(hostShown, host, sizeof(hostShown) - 1);
            bootLine(160, host, ST_DIM, 2);
            bootLine(196, g_codecOk ? "CODEC OK" : g_dacMode ? "I2S DAC" : "NO CODEC",
                     g_codecOk || g_dacMode ? ST_GREEN : ST_RED, 2);
            char fw[24];
            snprintf(fw, sizeof(fw), "FW %s", FW_VERSION);
            bootLine(284, fw, ST_DIM, 2);
        }
        bootBar((int)(now / 4), false, hue);
        return;
    }

    // intro: the address, large
    char ip[20];
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", g_ip[0], g_ip[1], g_ip[2], g_ip[3]);
    // st_glyph draws up to scale 4 (its line buffer); larger scales draw nothing.
    bootRainbow(122, ip, 4, -2 * hue);
    if (!stepShown[0]) {
        snprintf(stepShown, sizeof(stepShown), "%s  FW %s", g_dhcpOk ? "DHCP" : "STATIC", FW_VERSION);
        bootLine(182, stepShown, ST_WHITE, 2);
        char host[40];
        snprintf(host, sizeof(host), "%s.local", g_hostLabel);
        bootLine(212, host, ST_DIM, 2);
        bootLine(284, "", ST_BLACK, 2);
    }
    uint32_t el = now - g_introStart;
    bootBar((int)((ST_W - 80) * (el > INTRO_MS ? INTRO_MS : el) / INTRO_MS), true, hue);
}

void loop()
{
    handleWebCmd();
    pollButton();
    pollSerial();
    pumpAudio();
    // No codec I2C once its clocks run: on this wiring a write can land on the wrong
    // register (35-4.0 lost the sound that way). Volume stays in the samples.
    // codecVolumeService();
    if (!g_netReady || bootIntro()) {
        bootAnim();
    } else {
        st_poll();
        st_spectrum();
        if (g_saver && g_picGen != g_picDrawn && g_picGen != 0)
            st_draw_saver();
        st_scroll_tick();
        st_menu_clock(false);
        st_cover_service();
        st_transport_service();
        st_pages_service();
        hud_stats(false);
        if (g_saver && g_saverClear) st_draw_saver();
        // Flash erase stalls the I2S DMA IRQ and clicks. Keep the new volume in
        // RAM and write it once the stream has gone quiet.
        bool quiet = !g_tone && (!g_lastPkt || (uint32_t)(millis() - g_lastPkt) > 300);
        // The volume, from the panel, the host or the console, goes into the log once it has
        // stood still for a second, playing or not. A full log falls back to saveCfg().
        static uint8_t volSeen;
        static uint32_t volSeenAt;
        if (g_volume != volSeen) {
            volSeen = g_volume;
            volSeenAt = millis();
        }
        if (g_volume != g_volSaved && !g_volDirty && (uint32_t)(millis() - volSeenAt) > 1000) {
            if (volLogWrite(g_volume)) {
                Serial.printf("[cfg] volume %u (log %lu)\n", g_volume, (unsigned long)g_volLogNext);
            } else {
                g_volDirtyAt = millis();
                g_volDirty = true;
            }
        }
        if (g_cfgDirty) {
            g_cfgDirty = false;
            g_volDirtyAt = millis();
            g_volDirty = true;
        }
        if (g_volDirty && quiet && (uint32_t)(millis() - g_volDirtyAt) > 400) {
            g_volDirty = false;
            saveCfg();
            Serial.printf("[cfg] volume %u\n", g_volume);
        }
    }
    digitalWrite(LED_BUILTIN, (millis() / 500) & 1);

    static uint32_t painted = 0;
    if ((uint32_t)(millis() - painted) >= 400) {
        painted = millis();
        paint();
    }
}

void setup1()
{
    while (!g_audioReady) delay(1);
    g_bootPhase = BOOT_LINK;
    w6_reset();
    w6_pio_init();

    uint32_t tr = millis();
    uint8_t cid = 0;
    while (millis() - tr < 800) {
        cid = w6_rd8(BSB_COMMON, 0x0000);
        if (cid == 0x61) break;
        delay(2);
    }
    Serial.printf("[pioq] CIDR0=0x%02X %s\n", cid, cid == 0x61 ? "(ready)" : "(NO COMMS)");

    w6_write(BSB_COMMON, 0x4120, W6_MAC, 6);
    for (uint8_t s = 0; s < 8; s++) {
        uint8_t bsb = (uint8_t)((s << 2) | 1);
        uint8_t rx = 0, tx = 0;
        if (s == 0) { rx = RXBUF_KB; tx = 2; }
        else if (s == 1) { rx = WEB_RXBUF_KB; tx = WEB_TXBUF_KB; }
        else if (s == 2) { rx = PIC_RXBUF_KB; tx = 0; }
        w6_wr8(bsb, 0x0220, rx);
        w6_wr8(bsb, 0x0200, tx);
    }

    uint8_t ip[4], mask[4], gw[4];
    bool viaDhcp = false;
    if (g_netMode == 0) {
        delay(2500);
        g_bootPhase = BOOT_DHCP;
        viaDhcp = dhcp_run(ip, mask, gw);
        if (!viaDhcp) g_bootPhase = BOOT_DHCP_FAIL;
    } else g_bootPhase = BOOT_STATIC;
    if (!viaDhcp) {
        memcpy(ip, g_cfgIp, 4);
        memcpy(mask, g_cfgMask, 4);
        memcpy(gw, g_cfgGw, 4);
    }

    w6_write(BSB_COMMON, 0x4130, gw, 4);
    w6_write(BSB_COMMON, 0x4134, mask, 4);
    w6_write(BSB_COMMON, 0x4138, ip, 4);
    memcpy((void *)g_ip, ip, 4);
    memcpy((void *)g_mask, mask, 4);
    memcpy((void *)g_gw, gw, 4);
    g_dhcpOk = viaDhcp;

    w6_wr8(BSB_S0_REG, 0x0000, 0x02);            // UDP
    w6_wr16(BSB_S0_REG, 0x0114, UDP_PORT);
    w6_wr8(BSB_S0_REG, 0x0010, 0x01);            // OPEN
    uint32_t t0 = millis();
    while (w6_rd8(BSB_S0_REG, 0x0010) && millis() - t0 < 200) {}

    w6_wr8(BSB_S2_REG, 0x0000, 0x02);            // UDP, wall preview
    w6_wr16(BSB_S2_REG, 0x0114, PIC_PORT);
    w6_wr8(BSB_S2_REG, 0x0010, 0x01);
    t0 = millis();
    while (w6_rd8(BSB_S2_REG, 0x0010) && millis() - t0 < 200) {}

    Serial.printf("[net] %s  %u.%u.%u.%u  UDP %d/%d  web http://%u.%u.%u.%u:%d/\n",
                  viaDhcp ? "DHCP" : "STATIC", ip[0], ip[1], ip[2], ip[3], UDP_PORT, PIC_PORT,
                  ip[0], ip[1], ip[2], ip[3], WEB_PORT);
    g_bootPhase = BOOT_READY;
    g_netReady = true;
}

void loop1()
{
    if (!g_netReady) return;

    // Audio first and all of it; then a few picture chunks, so a preview burst
    // never keeps core 1 away from the PCM socket for long.
    udp_drain(BSB_S0_REG, BSB_S0_RX, 64, true);
    udp_drain(BSB_S2_REG, BSB_S2_RX, 4, false);

    static uint32_t fb = 0;
    if ((uint32_t)(millis() - fb) >= 100) {
        fb = millis();
        sendFeedback();
    }
    sendKey();
    sendSpectrum();
    web_poll();
    phy_poll();
}
