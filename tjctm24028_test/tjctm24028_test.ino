/*--------------------------------------------------------------------------------------
  Bench test for the TJCTM24028-SPI (2.8", ILI9341, 240x320, XPT2046).
  W6300-EVB-Pico2. Do not stack the module. Dupont only.

  Wiz pin 1 is the pin that used to drive the small-panel backlight (GP0).
  Count along that header the same way as the audio HAT wiring.

    Signal     Wiz pin   Pico      Module pin
    5V         40        VBUS      VCC          J1 stays open (onboard 3.3 V regulator)
    GND        38        GND       GND
    CS         11        GP8       CS
    RESET      19        GP14      RESET
    DC         12        GP9       DC / RS
    MOSI       5         GP3       SDI and T_DIN
    SCK        4         GP2       SCK and T_CLK
    BLK        1         GP0       LED          high = backlight on
    MISO       2         GP1       SDO and T_DO
    T_CS       6         GP4       T_CS
    T_IRQ      7         GP5       T_IRQ        low while touched

  Leave the SD header unconnected and do not insert a card.
  Do not use GP10-12 (I2S) or GP16-22 (W6300).

  Serial 115200. The Pico LED blinks. The panel shows colour bars, a label,
  and a white box under the stylus. Raw touch numbers are on the serial line.
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
static constexpr int PIN_BLK = 0;
static constexpr int PIN_TCS = 4;
static constexpr int PIN_TIRQ = 5;
static constexpr int PIN_PICOLED = 25;

static constexpr int LCD_W = 240;
static constexpr int LCD_H = 320;
// MX | BGR. 0x08 painted a left-right mirror on this red module.
static constexpr uint8_t LCD_MADCTL = 0x48;

static constexpr uint16_t C_BLACK = 0x0000;
static constexpr uint16_t C_WHITE = 0xFFFF;
static constexpr uint16_t C_RED = 0xF800;
static constexpr uint16_t C_GREEN = 0x07E0;
static constexpr uint16_t C_BLUE = 0x001F;
static constexpr uint16_t C_YELLOW = 0xFFE0;

// 5x7, columns, bit0 = top. ASCII 32..90.
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

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_W) w = LCD_W - x;
    if (y + h > LCD_H) h = LCD_H - y;
    if (w <= 0 || h <= 0) return;

    lcd_window(x, y, w, h);
    uint8_t line[LCD_W * 2];
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)color;
    for (int i = 0; i < w; i++) {
        line[i * 2] = hi;
        line[i * 2 + 1] = lo;
    }
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    // The one-buffer transfer writes MISO back over the colour. With RX
    // unconnected that turns every row after the first into black.
    for (int row = 0; row < h; row++) SPI.transfer(line, nullptr, (size_t)w * 2);
    digitalWrite(PIN_CS, HIGH);
}

static void draw_char(int x, int y, char ch, uint16_t color, int scale)
{
    if (ch < 32 || ch > 90) ch = ' ';
    const uint8_t *col = FONT5x7[ch - 32];
    for (int c = 0; c < 5; c++) {
        uint8_t bits = col[c];
        for (int r = 0; r < 7; r++) {
            if (bits & (1 << r))
                fill_rect(x + c * scale, y + r * scale, scale, scale, color);
        }
    }
}

static void draw_text(int x, int y, const char *s, uint16_t color, int scale)
{
    while (*s) {
        char ch = *s++;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        draw_char(x, y, ch, color, scale);
        x += 6 * scale;
    }
}

static void lcd_init()
{
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_DC, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_BLK, OUTPUT);
    pinMode(PIN_TCS, OUTPUT);
    pinMode(PIN_TIRQ, INPUT_PULLUP);
    digitalWrite(PIN_CS, HIGH);
    digitalWrite(PIN_TCS, HIGH);
    digitalWrite(PIN_BLK, HIGH);

    digitalWrite(PIN_RST, LOW);
    delay(20);
    digitalWrite(PIN_RST, HIGH);
    delay(120);

    // GP1 is not a legal SPI0 MISO pin. Claiming it panics and drops USB.
    // The panel is write-only here. Touch is bit-banged on the same wires.
    SPI.setSCK(PIN_SCK);
    SPI.setTX(PIN_MOSI);
    SPI.setRX(NOPIN);
    SPI.begin(false);
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

    lcd_cmd(0x01); delay(150);          // SWRESET
    lcd_cmd(0x11); delay(150);          // SLPOUT
    const uint8_t pw[] = { 0x23 };
    lcd_cmd(0xC0); lcd_data(pw[0]);     // VRH: panel voltage, brighter than the reset default
    lcd_cmd(0xC1); lcd_data(0x10);
    lcd_cmd(0xC5); lcd_data(0x3E); lcd_data(0x28);
    lcd_cmd(0xC7); lcd_data(0x86);
    lcd_cmd(0x3A); lcd_data(0x55);      // 16-bit
    lcd_cmd(0x36); lcd_data(LCD_MADCTL);
    lcd_cmd(0xB1); lcd_data(0x00); lcd_data(0x18);
    const uint8_t pgam[] = {
        0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00
    };
    const uint8_t ngam[] = {
        0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F
    };
    lcd_cmd(0xE0);
    for (uint8_t b : pgam) lcd_data(b);
    lcd_cmd(0xE1);
    for (uint8_t b : ngam) lcd_data(b);
    lcd_cmd(0x13);                      // NORON
    lcd_cmd(0x29);                      // DISPON
    delay(20);
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
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
}

static void bb_byte(uint8_t v)
{
    for (int i = 7; i >= 0; --i) {
        digitalWrite(PIN_MOSI, (v >> i) & 1);
        digitalWrite(PIN_SCK, HIGH);
        digitalWrite(PIN_SCK, LOW);
    }
}

// XPT2046, 12-bit. 0x90 = X, 0xD0 = Y, 0xB0 = Z1. Bit-banged: GP1 is not SPI0 MISO.
static int xpt_read(uint8_t cmd)
{
    touch_bus();
    digitalWrite(PIN_TCS, LOW);
    bb_byte(cmd);
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

static bool read_touch(int &x, int &y, int &z)
{
    z = xpt_read(0xB0);
    if (z < 300 && digitalRead(PIN_TIRQ) != LOW) return false;
    // A few samples. The first after a CS change is often stale.
    xpt_read(0x90);
    x = (xpt_read(0x90) + xpt_read(0x90)) / 2;
    xpt_read(0xD0);
    y = (xpt_read(0xD0) + xpt_read(0xD0)) / 2;
    z = xpt_read(0xB0);
    return z > 200;
}

static int map_raw(int v, int lo, int hi, int out)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int)((long)(v - lo) * (out - 1) / (hi - lo));
}

static void paint_base()
{
    fill_rect(0, 0, LCD_W, LCD_H, C_BLACK);
    fill_rect(0, 0, LCD_W, 28, C_RED);
    fill_rect(0, 28, LCD_W, 28, C_GREEN);
    fill_rect(0, 56, LCD_W, 28, C_BLUE);
    draw_text(8, 100, "TJCTM24028", C_WHITE, 2);
    draw_text(8, 122, "240X320", C_YELLOW, 2);
    draw_text(8, 150, "TOUCH THE GLASS", C_GREEN, 1);
    draw_text(8, 290, "R G B BARS ABOVE", C_WHITE, 1);
}

void setup()
{
    pinMode(PIN_PICOLED, OUTPUT);
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("[lcd] TJCTM24028 test");
    Serial.println("[lcd] VCC wiz40  CS wiz11/GP8  RST wiz19/GP14  DC wiz12/GP9");
    Serial.println("[lcd] SCK wiz4/GP2  MOSI wiz5/GP3  MISO wiz2/GP1  LED wiz1/GP0");
    Serial.println("[lcd] T_CS wiz6/GP4  T_IRQ wiz7/GP5  GND wiz38  J1 open");

    lcd_init();
    paint_base();
    Serial.println("[lcd] panel up");
}

void loop()
{
    static uint32_t nextBlink = 0;
    static bool led = false;
    static int lastBoxX = -1, lastBoxY = -1;
    static char lastLine[24] = "";

    if ((int32_t)(millis() - nextBlink) >= 0) {
        nextBlink = millis() + 400;
        led = !led;
        digitalWrite(PIN_PICOLED, led);
    }

    int rx, ry, z;
    if (!read_touch(rx, ry, z)) {
        if (lastBoxX >= 0) {
            fill_rect(lastBoxX, lastBoxY, 36, 36, C_BLACK);
            lastBoxX = lastBoxY = -1;
        }
        delay(15);
        return;
    }

    // With the header at the bottom, 0x90 runs vertically and 0xD0 runs horizontally.
    int px = LCD_W - 1 - map_raw(ry, 300, 3800, LCD_W);
    // The glass reads about 5 mm high. 5 mm of 57.6 mm is ~28 rows.
    int py = map_raw(rx, 300, 3800, LCD_H) + 28;
    char line[24];
    snprintf(line, sizeof(line), "X%4d Y%4d Z%4d", rx, ry, z);
    if (strcmp(line, lastLine) != 0) {
        fill_rect(4, 180, 230, 16, C_BLACK);
        draw_text(4, 180, line, C_WHITE, 1);
        memcpy(lastLine, line, sizeof(lastLine));
        Serial.printf("[lcd] touch %s irq %d\n", line, digitalRead(PIN_TIRQ) == LOW);
    }

    constexpr int box = 36;
    int bx = px - box / 2;
    int by = py - box / 2;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;
    if (bx > LCD_W - box) bx = LCD_W - box;
    if (by > LCD_H - box) by = LCD_H - box;
    if (lastBoxX >= 0 && (lastBoxX != bx || lastBoxY != by))
        fill_rect(lastBoxX, lastBoxY, box, box, C_BLACK);
    fill_rect(bx, by, box, box, C_WHITE);
    lastBoxX = bx;
    lastBoxY = by;
    delay(20);
}
