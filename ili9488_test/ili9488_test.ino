/*--------------------------------------------------------------------------------------
  Bench test for the Binghe 3.5" KMRTM35018-SPI (ILI9488, 480x320 landscape).
  4-wire SPI is 18-bit: three bytes per pixel. An ILI9341 image is garbage here.
  Do not flash verpixeld_audio.ino.uf2 or tjctm24028-test.uf2 onto this panel.

  Same Pico pins as the 2.8" TJCTM. Dupont only, do not stack the module.

    Signal     Wiz pin   Pico      Module
    5V         40        VBUS      VCC          J1 open (module regulates 5 V to 3.3 V)
    GND        38        GND       GND
    CS         11        GP8       CS
    RESET      19        GP14      RESET
    DC         12        GP9       DC / RS
    MOSI       5         GP3       SDI and T_DIN
    SCK        4         GP2       SCK and T_CLK
    LED        36        3V3       LED          backlight supply, not a GPIO, not 5 V
    T_DO       2         GP1       T_DO         touch data out only
    T_CS       6         GP4       T_CS
    T_IRQ      7         GP5       T_IRQ        low while touched
    (none)     —         —         SDO          leave the display data-out pin open

  The display SDO stays driven and fights the touch if it is tied to T_DO.
  SD header empty. Do not use GP10-12 (I2S) or GP16-22 (W6300).
  GP26 (audio button, to GND) or serial 20 / 40 / 62 selects the SPI clock.
  Starts at 40 MHz. Serial 115200.
--------------------------------------------------------------------------------------*/
#if !defined(ARDUINO_ARCH_RP2040)
#error "RP2040/RP2350 only."
#endif

#include <SPI.h>

static constexpr int PIN_SCK = 2;
static constexpr int PIN_MOSI = 3;
static constexpr int PIN_MISO = 1;
static constexpr int PIN_CS = 8;
static constexpr int PIN_DC = 9;
static constexpr int PIN_RST = 14;
static constexpr int PIN_TCS = 4;
static constexpr int PIN_TIRQ = 5;
static constexpr int PIN_BTN = 26;
static constexpr int PIN_PICOLED = 25;

static constexpr int LCD_W = 480;
static constexpr int LCD_H = 320;
static constexpr uint8_t LCD_MADCTL = 0x28; // MV | BGR, landscape, pin header on the right

static constexpr uint16_t C_BLACK = 0x0000;
static constexpr uint16_t C_WHITE = 0xFFFF;
static constexpr uint16_t C_RED = 0xF800;
static constexpr uint16_t C_GREEN = 0x07E0;
static constexpr uint16_t C_BLUE = 0x001F;
static constexpr uint16_t C_YELLOW = 0xFFE0;
static constexpr uint16_t C_CYAN = 0x07FF;

static const uint32_t SPEEDS[] = { 20000000u, 40000000u, 62000000u };
static int g_speed = 1;

static const uint8_t FONT5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x00,0x50,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
};

static const uint16_t BARS[] = { C_RED, C_GREEN, C_BLUE, C_YELLOW, C_CYAN, C_WHITE };

static void lcd_cmd(uint8_t c)
{
    digitalWrite(PIN_DC, LOW);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(c);
    digitalWrite(PIN_CS, HIGH);
}

static void lcd_data(uint8_t v)
{
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(v);
    digitalWrite(PIN_CS, HIGH);
}

static void lcd_window(int x, int y, int w, int h)
{
    uint16_t x1 = (uint16_t)(x + w - 1);
    uint16_t y1 = (uint16_t)(y + h - 1);
    lcd_cmd(0x2A);
    lcd_data(x >> 8); lcd_data(x); lcd_data(x1 >> 8); lcd_data(x1);
    lcd_cmd(0x2B);
    lcd_data(y >> 8); lcd_data(y); lcd_data(y1 >> 8); lcd_data(y1);
    lcd_cmd(0x2C);
}

static void pix18(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b)
{
    r = (uint8_t)((color & 0xF800) >> 8);
    g = (uint8_t)((color & 0x07E0) >> 3);
    b = (uint8_t)((color & 0x001F) << 3);
}

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_W) w = LCD_W - x;
    if (y + h > LCD_H) h = LCD_H - y;
    if (w <= 0 || h <= 0) return;

    lcd_window(x, y, w, h);
    uint8_t line[480 * 3];
    uint8_t r, g, b;
    pix18(color, r, g, b);
    for (int i = 0; i < w; i++) {
        line[i * 3] = r;
        line[i * 3 + 1] = g;
        line[i * 3 + 2] = b;
    }
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    for (int row = 0; row < h; row++)
        SPI.transfer(line, nullptr, (size_t)w * 3);
    digitalWrite(PIN_CS, HIGH);
}

static uint16_t bar_color(int x)
{
    int band = x * 6 / LCD_W;
    if (band < 0) band = 0;
    if (band > 5) band = 5;
    return BARS[band];
}

static void glyph(int x, int y, char ch, uint16_t color, uint16_t bg, int scale)
{
    if (scale < 1 || scale > 3) return;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    if (ch < 32 || ch > 'Z') ch = ' ';
    const uint8_t *g = FONT5x7[ch - 32];
    int gw = 5 * scale, gh = 7 * scale;
    uint8_t buf[15 * 21 * 3];
    uint8_t r, gc, b, br, bgc, bb;
    pix18(color, r, gc, b);
    pix18(bg, br, bgc, bb);
    int n = 0;
    for (int row = 0; row < gh; row++) {
        int src = row / scale;
        for (int col = 0; col < gw; col++) {
            bool on = g[col / scale] & (1u << src);
            buf[n++] = on ? r : br;
            buf[n++] = on ? gc : bgc;
            buf[n++] = on ? b : bb;
        }
    }
    lcd_window(x, y, gw, gh);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(buf, nullptr, (size_t)n);
    digitalWrite(PIN_CS, HIGH);
}

static void text(int x, int y, const char *s, uint16_t color, uint16_t bg, int scale)
{
    for (; *s; s++) {
        glyph(x, y, *s, color, bg, scale);
        x += 6 * scale;
        if (x > LCD_W - 6 * scale) break;
    }
}

static void lcd_apply_clock()
{
    SPI.endTransaction();
    SPI.beginTransaction(SPISettings(SPEEDS[g_speed], MSBFIRST, SPI_MODE0));
}

static void lcd_init()
{
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_DC, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_TCS, OUTPUT);
    pinMode(PIN_TIRQ, INPUT_PULLUP);
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_MISO, INPUT);
    digitalWrite(PIN_CS, HIGH);
    digitalWrite(PIN_TCS, HIGH);

    digitalWrite(PIN_RST, LOW);
    delay(20);
    digitalWrite(PIN_RST, HIGH);
    delay(120);

    SPI.setSCK(PIN_SCK);
    SPI.setTX(PIN_MOSI);
    SPI.setRX(NOPIN);
    SPI.begin(false);
    lcd_apply_clock();

    lcd_cmd(0x01); delay(150);
    lcd_cmd(0x11); delay(120);
    const uint8_t pgam[] = {
        0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F
    };
    const uint8_t ngam[] = {
        0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F
    };
    lcd_cmd(0xE0);
    for (uint8_t v : pgam) lcd_data(v);
    lcd_cmd(0xE1);
    for (uint8_t v : ngam) lcd_data(v);
    lcd_cmd(0xC0); lcd_data(0x17); lcd_data(0x15);
    lcd_cmd(0xC1); lcd_data(0x41);
    lcd_cmd(0xC5); lcd_data(0x00); lcd_data(0x12); lcd_data(0x80);
    lcd_cmd(0x36); lcd_data(LCD_MADCTL);
    lcd_cmd(0x3A); lcd_data(0x66);
    lcd_cmd(0xB0); lcd_data(0x00);
    lcd_cmd(0xB1); lcd_data(0xA0);
    lcd_cmd(0xB4); lcd_data(0x02);
    lcd_cmd(0xB6); lcd_data(0x02); lcd_data(0x02);
    lcd_cmd(0xE9); lcd_data(0x00);
    lcd_cmd(0xF7); lcd_data(0xA9); lcd_data(0x51); lcd_data(0x2C); lcd_data(0x82);
    lcd_cmd(0x13);
    lcd_cmd(0x29);
    delay(25);
}

static void touch_bus()
{
    SPI.endTransaction();
    pinMode(PIN_SCK, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_MISO, INPUT);
    digitalWrite(PIN_SCK, LOW);
}

static void lcd_bus()
{
    SPI.setSCK(PIN_SCK);
    SPI.setTX(PIN_MOSI);
    SPI.setRX(NOPIN);
    SPI.begin(false);
    lcd_apply_clock();
}

static void touch_byte(uint8_t v)
{
    for (int i = 7; i >= 0; --i) {
        digitalWrite(PIN_MOSI, (v >> i) & 1);
        digitalWrite(PIN_SCK, HIGH);
        digitalWrite(PIN_SCK, LOW);
    }
}

static int xpt_read(uint8_t cmd)
{
    touch_bus();
    digitalWrite(PIN_TCS, LOW);
    touch_byte(cmd);
    uint16_t v = 0;
    for (int i = 0; i < 16; i++) {
        digitalWrite(PIN_SCK, HIGH);
        v = (uint16_t)((v << 1) | (digitalRead(PIN_MISO) ? 1 : 0));
        digitalWrite(PIN_SCK, LOW);
    }
    digitalWrite(PIN_TCS, HIGH);
    lcd_bus();
    return (int)(v >> 3) & 0x0FFF;
}

static int map_raw(int v, int lo, int hi, int out)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int)((long)(v - lo) * (out - 1) / (hi - lo));
}

// Header on the right. 0x90 was the short axis on the 2.8" and is X here.
static bool finger(int &x, int &y)
{
    if (digitalRead(PIN_TIRQ) != LOW) return false;
    int z = xpt_read(0xB0);
    if (z < 400) return false;
    xpt_read(0x90);
    int rx = (xpt_read(0x90) + xpt_read(0x90)) / 2;
    xpt_read(0xD0);
    int ry = (xpt_read(0xD0) + xpt_read(0xD0)) / 2;
    x = LCD_W - 1 - map_raw(rx, 300, 3800, LCD_W);
    y = map_raw(ry, 300, 3800, LCD_H);
    return true;
}

static uint32_t bench_full()
{
    uint32_t t0 = micros();
    for (int i = 0; i < 6; i++)
        fill_rect(i * (LCD_W / 6), 0, LCD_W / 6, LCD_H, BARS[i]);
    return (micros() - t0) / 1000u;
}

static char g_hud[40];

static void paint_hud(uint32_t fullMs)
{
    unsigned fps = fullMs ? (unsigned)(1000u / fullMs) : 0;
    snprintf(g_hud, sizeof(g_hud), "%luMHZ  %lums  %uFPS",
             (unsigned long)(SPEEDS[g_speed] / 1000000u),
             (unsigned long)fullMs, fps);
    fill_rect(8, 8, 280, 20, C_BLACK);
    text(10, 10, g_hud, C_WHITE, C_BLACK, 2);
    text(8, 292, "BTN OR SERIAL 20 40 62", C_BLACK, C_YELLOW, 1);
    Serial.printf("[lcd] %s\n", g_hud);
}

static void column(int x, int w, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (x + w > LCD_W) w = LCD_W - x;
    if (w <= 0) return;
    fill_rect(x, 40, w, LCD_H - 70, color);
}

void setup()
{
    pinMode(PIN_PICOLED, OUTPUT);
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[lcd] Binghe ILI9488 480x320 18-bit");
    Serial.println("[lcd] SDO open. LED on 3V3 wiz36, not 5V, not GP0.");

    lcd_init();
    uint32_t ms = bench_full();
    paint_hud(ms);
    text(300, 10, "TL", C_WHITE, C_RED, 2);
}

void loop()
{
    static uint32_t nextBlink = 0;
    static bool led = false;
    static int sweep = 40;
    static int prev = -1;
    static int boxX = -1, boxY = -1;
    static bool btnWas = false;
    static uint32_t nextSweep = 0;

    if ((int32_t)(millis() - nextBlink) >= 0) {
        nextBlink = millis() + 500;
        led = !led;
        digitalWrite(PIN_PICOLED, led);
    }

    bool btn = digitalRead(PIN_BTN) == LOW;
    if (btn && !btnWas) {
        g_speed = (g_speed + 1) % 3;
        lcd_apply_clock();
        paint_hud(bench_full());
        prev = -1;
    }
    btnWas = btn;

    if (Serial.available()) {
        int mhz = Serial.parseInt();
        int pick = -1;
        if (mhz == 20) pick = 0;
        else if (mhz == 40) pick = 1;
        else if (mhz == 62) pick = 2;
        if (pick >= 0 && pick != g_speed) {
            g_speed = pick;
            lcd_apply_clock();
            paint_hud(bench_full());
            prev = -1;
        }
    }

    if ((int32_t)(millis() - nextSweep) >= 0) {
        nextSweep = millis() + 30;
        if (prev >= 0 && prev != sweep)
            column(prev, 8, bar_color(prev));
        column(sweep, 8, C_WHITE);
        prev = sweep;
        sweep += 8;
        if (sweep > LCD_W - 16) sweep = 8;
    }

    int x, y;
    if (!finger(x, y)) {
        if (boxX >= 0) {
            fill_rect(boxX, boxY, 24, 24, bar_color(boxX + 12));
            boxX = boxY = -1;
        }
        return;
    }
    int bx = x - 12;
    int by = y - 12;
    if (bx < 0) bx = 0;
    if (by < 40) by = 40;
    if (bx > LCD_W - 24) bx = LCD_W - 24;
    if (by > LCD_H - 54) by = LCD_H - 54;
    if (boxX >= 0 && (boxX != bx || boxY != by))
        fill_rect(boxX, boxY, 24, 24, bar_color(boxX + 12));
    fill_rect(bx, by, 24, 24, C_WHITE);
    boxX = bx;
    boxY = by;
}
