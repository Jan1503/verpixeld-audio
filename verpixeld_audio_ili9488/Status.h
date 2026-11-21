// Status screen for the Binghe 3.5" (ILI9488). Landscape 480x320, pin header on the right.
// SPI is 18-bit, three bytes per pixel. Touch is bit-banged. The display SDO stays open.
#pragma once

static constexpr int ST_W = 480;
static constexpr int ST_H = 320;
static constexpr uint8_t ST_MADCTL = 0x28;   // MV | BGR, landscape

static constexpr uint16_t ST_BLACK = 0x0000;
static constexpr uint16_t ST_WHITE = 0xFFFF;
static constexpr uint16_t ST_GREEN = 0x07E0;
static constexpr uint16_t ST_YELLOW = 0xFFE0;
static constexpr uint16_t ST_RED = 0xF800;
static constexpr uint16_t ST_DIM = 0x7BEF;
static constexpr uint16_t ST_HDR = 0x1082;
static constexpr uint16_t ST_TRACK = 0x2124;
static constexpr uint16_t ST_BTN = 0x3186;

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

static void (*st_yield)() = nullptr;
static volatile uint16_t g_vuPeak = 0;
static int g_vuDrawn = -1;

static void st_cmd(uint8_t c)
{
    digitalWrite(PIN_DC, LOW);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(c);
    digitalWrite(PIN_CS, HIGH);
}

static void st_d8(uint8_t v)
{
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(v);
    digitalWrite(PIN_CS, HIGH);
}

static void st_pix(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b)
{
    r = (uint8_t)((color & 0xF800) >> 8);
    g = (uint8_t)((color & 0x07E0) >> 3);
    b = (uint8_t)((color & 0x001F) << 3);
}

static void st_window(int x, int y, int w, int h)
{
    uint16_t x1 = (uint16_t)(x + w - 1);
    uint16_t y1 = (uint16_t)(y + h - 1);
    st_cmd(0x2A);
    st_d8(x >> 8); st_d8(x); st_d8(x1 >> 8); st_d8(x1);
    st_cmd(0x2B);
    st_d8(y >> 8); st_d8(y); st_d8(y1 >> 8); st_d8(y1);
    st_cmd(0x2C);
}

static void st_fill(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST_W) w = ST_W - x;
    if (y + h > ST_H) h = ST_H - y;
    if (w <= 0 || h <= 0) return;

    st_window(x, y, w, h);
    uint8_t line[ST_W * 3];
    uint8_t r, g, b;
    st_pix(color, r, g, b);
    for (int i = 0; i < w; i++) {
        line[i * 3] = r;
        line[i * 3 + 1] = g;
        line[i * 3 + 2] = b;
    }
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    for (int row = 0; row < h; row++) {
        SPI.transfer(line, nullptr, (size_t)w * 3);
        if (st_yield && (row & 3) == 3) st_yield();
    }
    digitalWrite(PIN_CS, HIGH);
}

static void st_glyph(int x, int y, char ch, uint16_t color, uint16_t bg, int scale)
{
    if (scale < 1 || scale > 4) return;
    const uint8_t *g = FONT5x7[ch - 32];
    int gw = 5 * scale, gh = 7 * scale;
    uint8_t buf[20 * 28 * 3];
    uint8_t r, gc, b, br, bgc, bb;
    st_pix(color, r, gc, b);
    st_pix(bg, br, bgc, bb);
    int n = 0;
    for (int row = 0; row < gh; row++) {
        int srcRow = row / scale;
        for (int col = 0; col < gw; col++) {
            bool on = g[col / scale] & (1u << srcRow);
            buf[n++] = on ? r : br;
            buf[n++] = on ? gc : bgc;
            buf[n++] = on ? b : bb;
        }
    }
    st_window(x, y, gw, gh);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(buf, nullptr, (size_t)n);
    digitalWrite(PIN_CS, HIGH);
}

static void st_text(int x, int y, const char *s, uint16_t color, uint16_t bg, int scale)
{
    for (; *s; s++) {
        char ch = *s;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        st_glyph(x, y, ch, color, bg, scale);
        x += 6 * scale;
        if (x > ST_W - 6 * scale) break;
    }
}

static volatile uint32_t g_spiHz = 62000000;

static void st_spi(uint32_t hz)
{
    if (hz < 8000000) hz = 8000000;
    if (hz > 62000000) hz = 62000000;
    g_spiHz = hz;
    SPI.endTransaction();
    SPI.beginTransaction(SPISettings(g_spiHz, MSBFIRST, SPI_MODE0));
}

static void st_init()
{
    pinMode(PIN_CS, OUTPUT);
    pinMode(PIN_DC, OUTPUT);
    pinMode(PIN_RST, OUTPUT);
    pinMode(PIN_TCS, OUTPUT);
    pinMode(PIN_TIRQ, INPUT_PULLUP);
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
    SPI.beginTransaction(SPISettings(g_spiHz, MSBFIRST, SPI_MODE0));

    st_cmd(0x01); delay(150);
    st_cmd(0x11); delay(120);
    const uint8_t pgam[] = {
        0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F
    };
    const uint8_t ngam[] = {
        0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F
    };
    st_cmd(0xE0);
    for (uint8_t b : pgam) st_d8(b);
    st_cmd(0xE1);
    for (uint8_t b : ngam) st_d8(b);
    st_cmd(0xC0); st_d8(0x17); st_d8(0x15);
    st_cmd(0xC1); st_d8(0x41);
    st_cmd(0xC5); st_d8(0x00); st_d8(0x12); st_d8(0x80);
    st_cmd(0x36); st_d8(ST_MADCTL);
    st_cmd(0x3A); st_d8(0x66);
    st_cmd(0xB0); st_d8(0x00);
    st_cmd(0xB1); st_d8(0xA0);
    st_cmd(0xB4); st_d8(0x02);
    st_cmd(0xB6); st_d8(0x02); st_d8(0x02);
    st_cmd(0xE9); st_d8(0x00);
    st_cmd(0xF7); st_d8(0xA9); st_d8(0x51); st_d8(0x2C); st_d8(0x82);
    st_cmd(0x13);
    st_cmd(0x29);
    delay(25);
}

static void st_volume_bar()
{
    const int x0 = 16, y0 = 140, bw = 448, bh = 48;
    st_fill(x0, y0, bw, bh, ST_TRACK);
    int filled = (int)g_volume * bw / 100;
    int fillRight = x0 + filled;
    if (filled > 0) st_fill(x0, y0, filled, bh, ST_GREEN);

    const char *label = "VOLUME";
    int x = x0 + 16;
    for (const char *s = label; *s; s++) {
        bool on = x + 8 < fillRight;
        st_glyph(x, y0 + 17, *s, on ? ST_BLACK : ST_WHITE, on ? ST_GREEN : ST_TRACK, 2);
        x += 12;
    }
    char num[4];
    snprintf(num, sizeof(num), "%u", (unsigned)g_volume);
    int nw = (int)strlen(num) * 12;
    x = x0 + bw - 16 - nw;
    for (char *s = num; *s; s++) {
        bool on = x + 8 < fillRight;
        st_glyph(x, y0 + 17, *s, on ? ST_BLACK : ST_WHITE, on ? ST_GREEN : ST_TRACK, 2);
        x += 12;
    }
}

static void st_mute_button()
{
    uint16_t btn = g_mute ? ST_YELLOW : ST_BTN;
    uint16_t fg = g_mute ? ST_BLACK : ST_WHITE;
    st_fill(16, 196, 448, 44, btn);
    const char *label = g_mute ? "MUTED" : "MUTE";
    int tw = (int)strlen(label) * 12;
    st_text((ST_W - tw) / 2, 210, label, fg, btn, 2);
}

static void st_state(const char *s, uint16_t color)
{
    st_fill(ST_W - 200, 0, 200, 36, ST_HDR);
    if (!s || !s[0]) return;
    int sw = (int)strlen(s) * 12;
    if (sw > 180) sw = 180;
    st_text(ST_W - 16 - sw, 11, s, color, ST_HDR, 2);
}

static void st_info(const char *line)
{
    st_fill(0, 292, ST_W, 28, ST_BLACK);
    if (line && line[0]) st_text(16, 300, line, ST_DIM, ST_BLACK, 1);
}

static void st_frame(const char *a, const char *b, const char *c, uint16_t ccol,
                     const char *d, const char *e)
{
    st_fill(0, 0, ST_W, ST_H, ST_BLACK);
    st_fill(0, 0, ST_W, 36, ST_HDR);
    st_text(16, 11, a, ST_WHITE, ST_HDR, 2);
    if (c && c[0]) {
        int sw = (int)strlen(c) * 12;
        if (sw > 180) sw = 180;
        st_text(ST_W - 16 - sw, 11, c, ccol, ST_HDR, 2);
    }
    st_text(16, 44, b, ST_WHITE, ST_BLACK, 2);
    (void)d;
    (void)e;
    st_volume_bar();
    st_mute_button();
    g_vuDrawn = -1;
}

static uint8_t g_pic[2][480 * 160 * 2];
static uint8_t g_picSeen[160];
static uint8_t g_picFrame = 0xFF, g_picCount, g_picBuild, g_picFront;
static volatile uint8_t g_picGen, g_picDrawn;
static volatile uint8_t g_blitting, g_picBusy, g_swapPending;
static uint8_t g_buildY, g_buildH, g_frontY, g_frontH, g_drawnY, g_drawnH;
static uint16_t g_buildW, g_frontW, g_drawnW;
static char g_scroll[97];
static int g_scrollOff;
static uint32_t g_scrollMs;
static uint8_t g_scrollGen, g_scrollShown;
static uint32_t g_picMs;
static uint8_t g_hh, g_mm, g_ss;
static uint32_t g_clockMs, g_touchMs;
static bool g_haveClock, g_saver, g_wakeMenu, g_saverClear, g_wander;
static int g_wanderX = -1, g_wanderY;
static uint32_t g_wanderAt;
static uint32_t g_saverLock;
static int g_saverKey = -1;

static void st_vu()
{
    static uint32_t last = 0;
    uint32_t now = millis();
    if ((uint32_t)(now - last) < 30) return;
    if (g_wander || (g_saver && g_picDrawn)) return;
    last = now;

    uint16_t peak = g_vuPeak;
    g_vuPeak = (uint16_t)(peak - (peak >> 2));
    static const uint16_t thr[16] = {
        1200, 2500, 4500, 7000, 10000, 13000, 16000, 19000,
        21500, 23500, 25500, 27000, 28500, 30000, 31500, 32700
    };
    int level = 0;
    for (int i = 0; i < 16; i++) if (peak >= thr[i]) level = i + 1;
    if (level == g_vuDrawn) return;
    g_vuDrawn = level;

    const int x0 = 16, y0 = 252, w = 448, h = 22;
    static uint8_t buf[448 * 22 * 3];
    int lit = level * w / 16;
    int n = 0;
    for (int x = 0; x < w; x++) {
        uint16_t c = ST_BLACK;
        if (x < lit) {
            int seg = x * 16 / w;
            c = seg >= 13 ? ST_RED : (seg >= 10 ? ST_YELLOW : ST_GREEN);
        }
        uint8_t r, g, b;
        st_pix(c, r, g, b);
        buf[n++] = r;
        buf[n++] = g;
        buf[n++] = b;
    }
    int rowBytes = n;
    for (int y = 1; y < h; y++) memcpy(buf + y * rowBytes, buf, (size_t)rowBytes);
    st_window(x0, y0, w, h);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(buf, nullptr, (size_t)rowBytes * h);
    digitalWrite(PIN_CS, HIGH);
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
    SPI.beginTransaction(SPISettings(g_spiHz, MSBFIRST, SPI_MODE0));
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

static constexpr int ST_COVER = 80;
static constexpr int ST_COVER_X = 384;
static constexpr int ST_COVER_Y = 40;
static void st_draw_saver();

// Landscape with the pin header on the right. 0x90 was portrait-vertical and is now X.
// 0xD0 was portrait-horizontal and is now Y. The point is the finger.
static bool st_finger(int &x, int &y)
{
    // The cover sits in the corner, where the pen-IRQ is often silent. Probe
    // there too, but not on every loop — a constant bit-bang stalls the DMA.
    static uint32_t nextProbe = 0;
    bool irq = digitalRead(PIN_TIRQ) == LOW;
    if (!irq && (int32_t)(millis() - nextProbe) < 0) return false;
    int z = xpt_read(0xB0);
    if (z < 180) {
        nextProbe = millis() + 40;
        return false;
    }
    nextProbe = 0;
    xpt_read(0x90);
    int rx = (xpt_read(0x90) + xpt_read(0x90)) / 2;
    xpt_read(0xD0);
    int ry = (xpt_read(0xD0) + xpt_read(0xD0)) / 2;
    x = ST_W - 1 - map_raw(rx, 300, 3800, ST_W);
    y = map_raw(ry, 300, 3800, ST_H);
    return true;
}

static void st_poll()
{
    static bool was = false;
    static bool swallow = false;
    static bool woke = false;
    static uint32_t muteAt = 0;
    static uint32_t coverBlock = 0;
    int x, y;
    bool down = st_finger(x, y);
    if (!down) {
        if (woke) coverBlock = millis() + 500;
        woke = false;
        was = false;
        swallow = false;
        return;
    }
    bool held = swallow || (g_saver && (int32_t)(g_saverLock - millis()) > 0);
    if (held) { was = true; return; }
    if (g_saver) {
        g_touchMs = millis();
        g_saver = false;
        g_wakeMenu = true;
        g_saverKey = -1;
        g_saverClear = true;
        g_drawnY = 0xFF;
        g_drawnH = 0xFF;
        swallow = true;
        woke = true;
        was = true;
        return;
    }
    bool coverBlocked = (int32_t)(coverBlock - millis()) > 0;
    if (!coverBlocked &&
        x >= ST_COVER_X && x < ST_COVER_X + ST_COVER &&
        y >= ST_COVER_Y && y < ST_COVER_Y + ST_COVER) {
        g_saver = true;
        g_saverKey = -1;
        g_saverClear = true;
        g_touchMs = millis() - 31000;
        g_saverLock = millis() + 700;
        swallow = true;
        was = true;
        return;
    }
    g_touchMs = millis();

    if (y >= 140 && y < 188) {
        int v = (x - 16) * 100 / 448;
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        if (g_volume != (uint8_t)v) {
            g_volume = (uint8_t)v;
            g_mute = false;
            g_volDirtyAt = millis();
            g_volDirty = true;
            st_volume_bar();
        }
    } else if (!was && y >= 196 && y < 240 && (uint32_t)(millis() - muteAt) > 250) {
        g_mute = !g_mute;
        muteAt = millis();
        st_mute_button();
        Serial.printf("[audio] %s\n", g_mute ? "mute" : "unmute");
    }
    was = true;
}

static void st_set_clock(uint8_t h, uint8_t m, uint8_t s)
{
    g_hh = h;
    g_mm = m;
    g_ss = s;
    g_clockMs = millis();
    g_haveClock = true;
}

static void st_pic_flip()
{
    uint8_t show = g_picBuild;
    g_picBuild ^= 1;
    g_frontY = g_buildY;
    g_frontH = g_buildH;
    g_frontW = g_buildW;
    g_picFront = show;
    g_picMs = millis();
    g_swapPending = 0;
    g_picGen++;
}

static void st_pic_chunk(const uint8_t *p, uint16_t n)
{
    if (n < 12) return;
    if (g_swapPending && !g_blitting) st_pic_flip();
    uint8_t frame = p[0], index = p[1], count = p[2];
    uint8_t oy = p[3], th = p[4];
    uint16_t w, h;
    memcpy(&w, p + 8, 2);
    memcpy(&h, p + 10, 2);
    if (w < 2 || w > 480 || h < 2 || h > 160 || count == 0 || count > 160 || index >= count) return;
    if (th == 0 || oy >= h || (uint16_t)oy + th > h) return;
    g_picBusy = 1;
    if (frame != g_picFrame || index == 0) {
        g_picFrame = frame;
        g_picCount = count;
        g_buildY = oy;
        g_buildH = th;
        g_buildW = w;
        memset(g_picSeen, 0, sizeof(g_picSeen));
        g_swapPending = 0;
    }
    uint16_t bytes = (uint16_t)(n - 12);
    uint16_t chunk = 960;
    memcpy(&chunk, p + 5, 2);
    if (chunk < 64 || chunk > 1440) chunk = 960;
    uint32_t off = (uint32_t)g_buildY * g_buildW * 2u + (uint32_t)index * chunk;
    if (off + bytes > sizeof(g_pic[0])) { g_picBusy = 0; return; }
    memcpy(g_pic[g_picBuild] + off, p + 12, bytes);
    g_picSeen[index] = 1;
    for (uint8_t i = 0; i < count; i++) if (!g_picSeen[i]) { g_picBusy = 0; return; }
    if (g_blitting) g_swapPending = 1;
    else st_pic_flip();
    g_picBusy = 0;
}

static void st_leave_saver()
{
    g_saver = false;
    g_wander = false;
    g_wakeMenu = true;
    g_saverKey = -1;
    g_saverClear = true;
    g_touchMs = millis();
    g_wanderX = -1;
}

static bool st_in_saver()
{
    if (g_saver) return true;
    if (g_touchMs == 0) g_touchMs = millis();
    if ((uint32_t)(millis() - g_touchMs) < 30000) return false;
    if (!g_saver) {
        g_saverKey = -1;
        g_saverClear = true;
    }
    g_saver = true;
    return true;
}

static bool st_take_wake()
{
    bool w = g_wakeMenu;
    g_wakeMenu = false;
    return w;
}

static void civil(int &h, int &m)
{
    if (!g_haveClock) { h = m = -1; return; }
    uint32_t tot = (uint32_t)g_ss + (millis() - g_clockMs) / 1000u;
    tot = (uint32_t)g_mm + tot / 60u;
    m = (int)(tot % 60u);
    h = (int)(((uint32_t)g_hh + tot / 60u) % 24u);
}

static void st_blit_pic()
{
    while (g_picBusy) {}
    if (g_swapPending) st_pic_flip();
    uint8_t y0 = g_frontY;
    uint8_t th = g_frontH ? g_frontH : 120;
    if ((uint16_t)y0 + th > 160) th = (uint8_t)(160 - y0);
    if (y0 != g_drawnY || th != g_drawnH || g_frontW != g_drawnW) {
        st_fill(0, 0, ST_W, ST_H, ST_BLACK);
        g_drawnY = y0;
        g_drawnH = th;
        g_drawnW = g_frontW;
        g_saverKey = -1;
        g_scrollShown = (uint8_t)(g_scrollGen - 1);
    }
    g_blitting = 1;
    uint8_t row[480 * 3];
    const uint8_t *src = g_pic[g_picFront];
    int fw = g_frontW ? g_frontW : 160;
    int scale = (fw > 0 && fw <= ST_W && (ST_W % fw) == 0) ? ST_W / fw : 1;
    if (scale > 3) scale = 3;
    if (scale < 1) scale = 1;
    while (scale > 1 && th * scale > 276) scale--;
    int bandPx = th * scale;
    int yOrg = (276 - bandPx) / 2;
    if (yOrg < 0) yOrg = 0;
    int xOrg = (ST_W - fw * scale) / 2;
    if (xOrg < 0) xOrg = 0;
    int dw = fw * scale;
    st_window(xOrg, yOrg, dw, bandPx);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    for (int sy = y0; sy < y0 + th; sy++) {
        const uint8_t *s = src + sy * fw * 2;
        int n = 0;
        for (int sx = 0; sx < fw; sx++) {
            uint16_t p = (uint16_t)((s[sx * 2] << 8) | s[sx * 2 + 1]);
            uint8_t r, g, b;
            st_pix(p, r, g, b);
            for (int repx = 0; repx < scale; repx++) {
                row[n++] = r;
                row[n++] = g;
                row[n++] = b;
            }
        }
        for (int rep = 0; rep < scale; rep++)
            SPI.transfer(row, nullptr, (size_t)n);
        if (st_yield && ((sy - y0) & 7) == 7) st_yield();
    }
    digitalWrite(PIN_CS, HIGH);
    g_blitting = 0;
    while (g_picBusy) {}
    if (g_swapPending) st_pic_flip();
}

static void st_set_scroll(const char *s)
{
    if (!s) s = "";
    if (!strcmp(g_scroll, s)) return;
    strncpy(g_scroll, s, 96);
    g_scroll[96] = 0;
    g_scrollOff = 0;
    g_scrollGen++;
}

static int st_scroll_cols(const char *s)
{
    int n = 0;
    for (; *s; s++) n++;
    return n;
}

static void st_scroll_draw(int y, int viewW)
{
    static uint8_t buf[480 * 8 * 3];
    if (viewW > ST_W) viewW = ST_W;
    memset(buf, 0, (size_t)viewW * 24);
    int cols = st_scroll_cols(g_scroll);
    int total = cols * 6;
    int gap = 24;
    int cycle = total + gap;
    bool slide = total > viewW;
    int origin = slide ? -g_scrollOff : (viewW - total) / 2;
    if (!slide && origin < 0) origin = 0;
    for (int copy = 0; copy < (slide ? 2 : 1); copy++) {
        int base = origin + copy * cycle;
        for (int i = 0; i < cols; i++) {
            char ch = g_scroll[i];
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
            if (ch < 32 || ch > 'Z') ch = ' ';
            const uint8_t *g = FONT5x7[ch - 32];
            int cx = base + i * 6;
            for (int col = 0; col < 5; col++) {
                int x = cx + col;
                if (x < 0 || x >= viewW) continue;
                uint8_t bits = g[col];
                for (int row = 0; row < 7; row++) {
                    if (bits & (1u << row)) {
                        int o = (row * viewW + x) * 3;
                        buf[o] = 0xFC;
                        buf[o + 1] = 0xFC;
                        buf[o + 2] = 0xFC;
                    }
                }
            }
        }
    }
    st_window(0, y, viewW, 8);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(buf, nullptr, (size_t)viewW * 24);
    digitalWrite(PIN_CS, HIGH);
}

static void st_scroll_clear()
{
    if (g_saver && g_picDrawn) st_fill(0, 282, ST_W, 8, ST_BLACK);
    else if (g_saver) st_fill(0, 210, ST_W, 8, ST_BLACK);
    else st_fill(0, 124, ST_W, 8, ST_BLACK);
}

static void st_scroll_paint()
{
    if (g_wander) return;
    if (!g_scroll[0]) st_scroll_clear();
    else if (g_saver && g_picDrawn) st_scroll_draw(282, ST_W);
    else if (g_saver) st_scroll_draw(210, ST_W);
    else st_scroll_draw(124, ST_W);
    g_scrollShown = g_scrollGen;
}

static void st_scroll_tick()
{
    if (g_scrollShown != g_scrollGen) {
        st_scroll_paint();
        if (!g_scroll[0]) return;
    }
    if (!g_scroll[0]) return;
    int total = st_scroll_cols(g_scroll) * 6;
    if (total <= ST_W) return;
    uint32_t now = millis();
    if ((uint32_t)(now - g_scrollMs) < 30) return;
    g_scrollMs = now;
    int cycle = total + 24;
    g_scrollOff++;
    if (g_scrollOff >= cycle) g_scrollOff = 0;
    st_scroll_paint();
}

static void st_wander_clock()
{
    int hh, mm;
    civil(hh, mm);
    int key = g_haveClock ? (hh * 60 + mm) : -2;
    const int scale = 4;
    const int cols = 5;
    const int tw = cols * 6 * scale;
    const int th = 7 * scale;
    const int pad = 6;
    uint32_t now = millis();
    bool move = g_wanderX < 0 || (uint32_t)(now - g_wanderAt) >= 10000;
    if (!move && key == g_saverKey) return;
    if (g_wanderX >= 0)
        st_fill(g_wanderX - pad, g_wanderY - pad, tw + pad * 2, th + pad * 2, ST_BLACK);
    if (move) {
        static bool seeded = false;
        if (!seeded) { randomSeed(micros()); seeded = true; }
        int maxX = ST_W - tw - pad;
        int maxY = ST_H - th - pad;
        int nx = g_wanderX, ny = g_wanderY;
        for (int n = 0; n < 8; n++) {
            nx = pad + (int)random(maxX - pad + 1);
            ny = pad + (int)random(maxY - pad + 1);
            if (g_wanderX < 0 || abs(nx - g_wanderX) > 48 || abs(ny - g_wanderY) > 28) break;
        }
        g_wanderX = nx;
        g_wanderY = ny;
        g_wanderAt = now;
    }
    char t[8];
    if (hh < 0) snprintf(t, sizeof(t), "--:--");
    else snprintf(t, sizeof(t), "%02d:%02d", hh, mm);
    st_text(g_wanderX, g_wanderY, t, ST_WHITE, ST_BLACK, scale);
    g_saverKey = key;
}

static void st_draw_saver()
{
    if (g_wander) {
        if (g_saverClear) {
            st_fill(0, 0, ST_W, ST_H, ST_BLACK);
            g_saverClear = false;
            g_wanderX = -1;
            g_saverKey = -1;
            g_vuDrawn = -1;
        }
        st_wander_clock();
        return;
    }
    if (g_picDrawn && (uint32_t)(millis() - g_picMs) > 12000) {
        g_picDrawn = 0;
        g_picGen = 0;
        g_saverKey = -1;
        g_saverClear = true;
    }
    if (g_saverClear) {
        st_fill(0, 0, ST_W, ST_H, ST_BLACK);
        g_saverClear = false;
        g_drawnY = 0xFF;
        g_drawnH = 0xFF;
        g_saverKey = -1;
        g_scrollShown = (uint8_t)(g_scrollGen - 1);
        g_vuDrawn = -1;
    }
    int hh, mm;
    civil(hh, mm);
    int key = g_haveClock ? (hh * 60 + mm) : -2;
    bool newPic = g_picGen != g_picDrawn && g_picGen != 0;
    if (newPic) {
        st_blit_pic();
        g_picDrawn = g_picGen;
    }
    if (!newPic && g_picDrawn == 0 && g_saverKey < 0)
        st_fill(0, 0, ST_W, ST_H, ST_BLACK);
    bool clockSame = key == g_saverKey;
    bool titleSame = g_scrollShown == g_scrollGen;
    if (clockSame && titleSame) return;
    char t[8];
    if (hh < 0) snprintf(t, sizeof(t), "--:--");
    else snprintf(t, sizeof(t), "%02d:%02d", hh, mm);
    int cols = 0;
    for (char *p = t; *p; p++) cols++;
    if (!clockSame) {
        if (g_picDrawn) {
            int tw = cols * 12;
            int x = (ST_W - tw) / 2;
            st_fill(x - 8, 292, tw + 16, 22, ST_BLACK);
            st_text(x, 294, t, ST_WHITE, ST_BLACK, 2);
        } else {
            int tw = cols * 24;
            int x = (ST_W - tw) / 2;
            st_fill(x - 8, 130, tw + 16, 36, ST_BLACK);
            st_text(x, 134, t, ST_WHITE, ST_BLACK, 4);
        }
        g_saverKey = key;
    }
    if (!titleSame) st_scroll_paint();
}

static uint8_t g_cover[ST_COVER * ST_COVER * 2];
static uint8_t g_coverSeen[16];
static uint8_t g_coverCount;
static volatile uint8_t g_coverReady, g_coverDrawn;

static void st_cover_service()
{
    if (g_saver) return;
    if (g_coverReady) {
        if (g_coverDrawn) return;
        uint8_t row[ST_COVER * 3];
        st_window(ST_COVER_X, ST_COVER_Y, ST_COVER, ST_COVER);
        digitalWrite(PIN_DC, HIGH);
        digitalWrite(PIN_CS, LOW);
        for (int y = 0; y < ST_COVER; y++) {
            const uint8_t *s = g_cover + y * ST_COVER * 2;
            int n = 0;
            for (int x = 0; x < ST_COVER; x++) {
                uint16_t p = (uint16_t)((s[x * 2] << 8) | s[x * 2 + 1]);
                uint8_t r, g, b;
                st_pix(p, r, g, b);
                row[n++] = r;
                row[n++] = g;
                row[n++] = b;
            }
            SPI.transfer(row, nullptr, sizeof(row));
        }
        digitalWrite(PIN_CS, HIGH);
        g_coverDrawn = 1;
        return;
    }
    if (!g_coverDrawn) return;
    st_fill(ST_COVER_X, ST_COVER_Y, ST_COVER, ST_COVER, ST_BLACK);
    g_coverDrawn = 0;
}

static void st_cover_chunk(const uint8_t *p, uint16_t n)
{
    if (n < 4) return;
    uint8_t index = p[0], count = p[1], size = p[2];
    if (count == 0) {
        g_coverReady = 0;
        return;
    }
    if (size != ST_COVER || count > 16 || index >= count) return;
    if (index == 0) {
        g_coverCount = count;
        memset(g_coverSeen, 0, sizeof(g_coverSeen));
        g_coverReady = 0;
    }
    uint16_t bytes = (uint16_t)(n - 4);
    uint32_t off = (uint32_t)index * 960u;
    if (off + bytes > sizeof(g_cover)) return;
    memcpy(g_cover + off, p + 4, bytes);
    g_coverSeen[index] = 1;
    for (uint8_t i = 0; i < count; i++) if (!g_coverSeen[i]) return;
    g_coverReady = 1;
    g_coverDrawn = 0;
}

static void st_menu_clock(bool force)
{
    if (g_saver) return;
    int hh, mm;
    civil(hh, mm);
    int key = g_haveClock ? (hh * 60 + mm) : -2;
    static int drawn = -9999;
    if (!force && key == drawn) return;
    drawn = key;
    char t[8];
    if (hh < 0) snprintf(t, sizeof(t), "--:--");
    else snprintf(t, sizeof(t), "%02d:%02d", hh, mm);
    st_fill(16, 64, 84, 20, ST_BLACK);
    st_text(16, 66, t, ST_WHITE, ST_BLACK, 2);
}
