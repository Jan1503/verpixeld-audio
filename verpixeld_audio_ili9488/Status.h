// Status screen for the Binghe 3.5" (ILI9488). Landscape 480x320, pin header on the right.
// SPI is 18-bit, three bytes per pixel. Touch is bit-banged. The display SDO stays open.
#pragma once
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/dma.h"

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
static constexpr uint16_t ST_ACCENT = 0x05BF;   // cyan, transport and progress

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

#include "FontLatin.h"

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

// ---- SPI by DMA ------------------------------------------------------------------------
// A row goes out by DMA while the CPU converts the next one; waiting pumps audio.
static int g_dmaCh = -1;

static void st_dma_wait()
{
    if (g_dmaCh < 0) return;
    while (dma_channel_is_busy(g_dmaCh)) {
        if (st_yield) st_yield();
    }
    while (spi_get_hw(spi0)->sr & SPI_SSPSR_BSY_BITS) {}
    // Nothing reads SPI0 RX: drop what came in and clear the overrun flag.
    while (spi_get_hw(spi0)->sr & SPI_SSPSR_RNE_BITS) (void)spi_get_hw(spi0)->dr;
    spi_get_hw(spi0)->icr = SPI_SSPICR_RORIC_BITS;
}

static void st_dma_send(const uint8_t *p, size_t n)
{
    if (g_dmaCh < 0) {
        SPI.transfer(p, nullptr, n);
        return;
    }
    dma_channel_config c = dma_channel_get_default_config((uint)g_dmaCh);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(spi0, true));
    dma_channel_configure((uint)g_dmaCh, &c, &spi_get_hw(spi0)->dr, p, (uint32_t)n, true);
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
    if (h >= 4 && g_dmaCh >= 0) {
        for (int row = 0; row < h; row++) {
            st_dma_wait();
            st_dma_send(line, (size_t)w * 3);
        }
        st_dma_wait();
    } else {
        for (int row = 0; row < h; row++) SPI.transfer(line, nullptr, (size_t)w * 3);
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

// Requested rate. The SPI block divides clk_peri (= clk_sys) by an even prescale and
// (1 + SCR), so 62 MHz on a 150 MHz build is really 37.5 MHz. g_spiHz is what the bus runs at.
static uint32_t g_spiWant = 62000000;
static volatile uint32_t g_spiHz = 62000000;

static void st_spi_note()
{
    g_spiHz = spi_get_baudrate(spi0);
}

static void st_spi(uint32_t hz)
{
    if (hz < 8000000) hz = 8000000;
    if (hz > 62000000) hz = 62000000;
    g_spiWant = hz;
    SPI.endTransaction();
    SPI.beginTransaction(SPISettings(g_spiWant, MSBFIRST, SPI_MODE0));
    st_spi_note();
}

static spin_lock_t *g_picLock = nullptr;   // preview buffer swap, core 0 against core 1

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
    SPI.beginTransaction(SPISettings(g_spiWant, MSBFIRST, SPI_MODE0));
    st_spi_note();
    if (g_dmaCh < 0) g_dmaCh = dma_claim_unused_channel(false);
    if (!g_picLock) g_picLock = spin_lock_init((uint)spin_lock_claim_unused(true));

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

// ======================================================================================
// Screen state
// ======================================================================================
enum { PAGE_PLAY, PAGE_FAVS, PAGE_RADIO, PAGE_WALL, PAGE_MORE, PAGE_N };
static uint8_t g_page = PAGE_PLAY;
static constexpr int TAB_Y = 284, TAB_H = 36, TAB_W = 96;
static const char *const TAB_NAMES[PAGE_N] = {"PLAY", "FAVS", "RADIO", "WALL", "MORE"};

static uint8_t g_pic[2][480 * 160 * 2];
static uint8_t g_picSeen[160];
static uint8_t g_picFrame = 0xFF, g_picCount, g_picBuild, g_picFront;
static volatile uint8_t g_picGen, g_picDrawn;
static volatile uint8_t g_blitting, g_swapPending;
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
static int g_saverCov = -2;     // cover generation shown in the saver, -1 none, -2 unknown
static uint32_t g_titleSavedAt;

static constexpr int ST_COVER = 80;
static constexpr int ST_COVER_X = 384;
static constexpr int ST_COVER_Y = 40;
static uint8_t g_cover[ST_COVER * ST_COVER * 2];
static uint8_t g_coverSeen[16];
static uint8_t g_coverCount;
static volatile uint8_t g_coverReady, g_coverDrawn, g_coverGen;

static char g_hdrA[24], g_hdrC[24];
static uint16_t g_hdrCol = ST_DIM;
static char g_info[5][40];

// Drawn further down; st_frame and the pages call them.
static void st_buttons();
static void st_now_paint(bool force);
static void st_scroll_paint();
static void st_menu_clock(bool force);
static void st_cover_service();
static void st_spec_reset();
static void st_list_page(int listId, bool full);
static void st_wall_page(bool full);
static void st_more_page(bool full);
static void st_draw_saver();
static void st_mute_button();

// Glyph from FontLatin (any code point), same raster as st_glyph.
static void st_glyph_ptr(int x, int y, const uint8_t *g, uint16_t color, uint16_t bg, int scale)
{
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

static int st_u8_cols(const char *s)
{
    const uint8_t *p = (const uint8_t *)s;
    int n = 0;
    while (st_utf8(p)) n++;
    return n;
}

// UTF-8 text, at most maxCols glyphs. Returns how many were drawn.
static int st_text_u8(int x, int y, const char *s, uint16_t color, uint16_t bg, int scale, int maxCols)
{
    const uint8_t *p = (const uint8_t *)s;
    int n = 0;
    uint32_t cp;
    while (n < maxCols && (cp = st_utf8(p)) != 0) {
        st_glyph_ptr(x, y, st_font(cp), color, bg, scale);
        x += 6 * scale;
        n++;
    }
    return n;
}

// Where the next line starts when s is wrapped at cols glyphs (at the last space if there is one).
static const char *st_u8_wrap(const char *s, int cols, int *taken)
{
    const uint8_t *p = (const uint8_t *)s;
    const uint8_t *lastSpace = nullptr;
    int n = 0, atSpace = 0;
    while (n < cols) {
        const uint8_t *before = p;
        uint32_t cp = st_utf8(p);
        if (!cp) { *taken = n; return (const char *)before; }
        n++;
        if (cp == ' ') { lastSpace = p; atSpace = n; }
    }
    const uint8_t *q = p;
    if (st_utf8(q) != 0 && lastSpace && atSpace > cols / 2) {
        *taken = atSpace - 1;
        return (const char *)lastSpace;
    }
    *taken = n;
    return (const char *)p;
}

// Bar with a label left and the value right: volume and wall brightness.
static void st_slider(int y0, const char *label, uint8_t value, uint16_t fillc)
{
    const int x0 = 16, bw = 448, bh = 48;
    int filled = (int)value * bw / 100;
    int fillRight = x0 + filled;
    if (filled > 0) st_fill(x0, y0, filled, bh, fillc);
    if (filled < bw) st_fill(x0 + filled, y0, bw - filled, bh, ST_TRACK);
    int x = x0 + 16;
    for (const char *s = label; *s; s++) {
        bool on = x + 8 < fillRight;
        st_glyph(x, y0 + 17, *s, on ? ST_BLACK : ST_WHITE, on ? fillc : ST_TRACK, 2);
        x += 12;
    }
    char num[4];
    snprintf(num, sizeof(num), "%u", (unsigned)value);
    x = x0 + bw - 16 - (int)strlen(num) * 12;
    for (char *s = num; *s; s++) {
        bool on = x + 8 < fillRight;
        st_glyph(x, y0 + 17, *s, on ? ST_BLACK : ST_WHITE, on ? fillc : ST_TRACK, 2);
        x += 12;
    }
}

static uint8_t g_volShown = 0xFF;

static void st_volume_bar()
{
    if (g_saver || g_page != PAGE_PLAY) return;
    g_volShown = g_volume;
    st_slider(140, "VOLUME", g_volume, ST_GREEN);
}

static void st_state(const char *s, uint16_t color)
{
    if (s != g_hdrC) {
        strncpy(g_hdrC, s ? s : "", sizeof(g_hdrC) - 1);
        g_hdrC[sizeof(g_hdrC) - 1] = 0;
    }
    g_hdrCol = color;
    if (g_saver) return;
    st_fill(ST_W - 200, 0, 200, 36, ST_HDR);
    if (!g_hdrC[0]) return;
    int sw = (int)strlen(g_hdrC) * 12;
    if (sw > 180) sw = 180;
    st_text(ST_W - 16 - sw, 11, g_hdrC, color, ST_HDR, 2);
}

static void st_tabs()
{
    for (int i = 0; i < PAGE_N; i++) {
        bool on = i == g_page;
        uint16_t bg = on ? ST_BTN : ST_HDR;
        int x = i * TAB_W;
        st_fill(x, TAB_Y, TAB_W, TAB_H, bg);
        if (on) st_fill(x, TAB_Y, TAB_W, 3, ST_ACCENT);
        else if (i > 0) st_fill(x, TAB_Y + 8, 1, TAB_H - 16, ST_TRACK);
        const char *t = TAB_NAMES[i];
        int tw = (int)strlen(t) * 12 - 2;
        st_text(x + (TAB_W - tw) / 2, TAB_Y + 13, t, on ? ST_WHITE : ST_DIM, bg, 2);
    }
}

static void st_page_body()
{
    switch (g_page) {
    case PAGE_PLAY:
        st_volume_bar();
        st_buttons();
        st_now_paint(true);
        st_spec_reset();
        g_scrollShown = (uint8_t)(g_scrollGen - 1);
        st_menu_clock(true);
        g_coverDrawn = 0;
        st_cover_service();
        break;
    case PAGE_FAVS: st_list_page(1, true); break;
    case PAGE_RADIO: st_list_page(2, true); break;
    case PAGE_WALL: st_wall_page(true); break;
    case PAGE_MORE: st_more_page(true); break;
    }
}

// Whole screen: header, the current page, the tab bar. b is the boot text (version).
static void st_frame(const char *a, const char *b, const char *c, uint16_t ccol,
                     const char *d, const char *e)
{
    if (a != g_hdrA) {
        strncpy(g_hdrA, a ? a : "", sizeof(g_hdrA) - 1);
        g_hdrA[sizeof(g_hdrA) - 1] = 0;
    }
    if (c != g_hdrC) {
        strncpy(g_hdrC, c ? c : "", sizeof(g_hdrC) - 1);
        g_hdrC[sizeof(g_hdrC) - 1] = 0;
    }
    g_hdrCol = ccol;
    (void)e;
    st_fill(0, 0, ST_W, ST_H, ST_BLACK);
    st_fill(0, 0, ST_W, 36, ST_HDR);
    st_text(16, 11, g_hdrA, ST_WHITE, ST_HDR, 2);
    if (g_hdrC[0]) {
        int sw = (int)strlen(g_hdrC) * 12;
        if (sw > 180) sw = 180;
        st_text(ST_W - 16 - sw, 11, g_hdrC, ccol, ST_HDR, 2);
    }
    if (b && b[0]) {
        // Boot: no menu yet (nothing to touch until the network is up). Version and codec state,
        // and the progress bar along the bottom runs on black.
        const char *name = "VERPIXELD AUDIO";
        st_text((ST_W - (int)strlen(name) * 18) / 2, 120, name, ST_WHITE, ST_BLACK, 3);
        int bw = (int)strlen(b) * 12;
        st_text((ST_W - bw) / 2, 162, b, ST_DIM, ST_BLACK, 2);
        if (d && d[0]) {
            int dw = (int)strlen(d) * 12;
            st_text((ST_W - dw) / 2, 196, d, ST_DIM, ST_BLACK, 2);
        }
        return;
    }
    st_page_body();
    st_tabs();
}

static void st_redraw() { st_frame(g_hdrA, "", g_hdrC, g_hdrCol, "", ""); }

static void st_page_go(uint8_t p)
{
    if (p >= PAGE_N || p == g_page) return;
    g_page = p;
    st_redraw();
}

// ======================================================================================
// Transport: the host's play state in, panel keys out
// ======================================================================================
// The host sends 'VPLY' about once a second. Keys go back as 'VKEY' from core 1.
enum { TP_PREV, TP_STOP, TP_PLAY, TP_NEXT, TP_MUTE, TP_N };
static constexpr int TP_Y = 196, TP_H = 44;
static const int16_t TP_X[TP_N] = {16, 100, 184, 304, 388};
static const int16_t TP_W[TP_N] = {76, 76, 112, 76, 76};
static constexpr int PG_X = 16, PG_Y = 92, PG_W = 352, PG_H = 10;
enum { KEY_PLAYPAUSE = 1, KEY_STOP = 2, KEY_PREV = 3, KEY_NEXT = 4, KEY_SEEK = 5,
       KEY_SHUFFLE = 6, KEY_REPEAT = 7, KEY_SLEEP = 8, KEY_SYNC = 9, KEY_BRIGHT = 10,
       KEY_WALL = 11, KEY_SELECT = 12, KEY_VISUAL = 13, KEY_ALARM = 14, KEY_ALARM_TIME = 15,
       KEY_SAVE_TITLE = 16 };

static volatile uint8_t g_playState;          // 0 stopped, 1 playing, 2 paused
static volatile uint8_t g_playFlags;          // bit0 seek, bit1 previous, bit2 next
static volatile uint32_t g_playPos, g_playDur, g_playAt;
static volatile uint8_t g_keyCode, g_keySeq, g_keyLeft, g_keyGen;
static volatile uint16_t g_keyArg;
static int8_t g_tpFlash = -1;
static uint32_t g_tpFlashAt;

static void st_play_note(uint8_t state, uint8_t flags, uint32_t pos, uint32_t dur)
{
    g_playState = state > 2 ? 0 : state;
    g_playFlags = flags;
    g_playPos = pos;
    g_playDur = dur;
    g_playAt = millis() | 1;
}

// A host that stopped talking for 4 s is gone; the row goes dim.
static bool st_play_known() { return g_playAt && (uint32_t)(millis() - g_playAt) < 4000; }
static uint8_t st_play_state() { return st_play_known() ? g_playState : 0; }

static uint32_t st_play_pos()
{
    uint32_t p = g_playPos;
    if (st_play_state() == 1) p += (uint32_t)(millis() - g_playAt);
    if (g_playDur && p > g_playDur) p = g_playDur;
    return p;
}

static void st_key(uint8_t code, uint16_t arg)
{
    static const char *const names[] = {"", "play/pause", "stop", "previous", "next", "seek", "shuffle",
                                        "repeat", "sleep", "sync", "brightness", "wall", "select",
                                        "visualizer", "alarm", "alarm time", "save title"};
    g_keyCode = code;
    g_keyArg = arg;
    g_keySeq = (uint8_t)(g_keySeq + 1);
    g_keyLeft = 3;
    Serial.printf("[key] %s %u\n", code < 17 ? names[code] : "?", (unsigned)arg);
}

// Triangle of height h, pointing right or left, about 7/8 h wide.
static void st_tri(int x, int y, int h, uint16_t c, bool right)
{
    int w = h * 7 / 8;
    for (int i = 0; i < h; i++) {
        int d = i < h / 2 ? i : h - 1 - i;
        int rw = w * (2 * d + 1) / h;
        if (rw < 1) rw = 1;
        st_fill(right ? x : x + w - rw, y + i, rw, 1, c);
    }
}

static void st_tp_button(int i, bool pressed)
{
    int x = TP_X[i], w = TP_W[i];
    bool known = st_play_known();
    uint8_t ps = st_play_state();
    uint16_t bg = ST_BTN, fg = ST_WHITE;
    if (i == TP_MUTE) {
        if (g_mute) { bg = ST_YELLOW; fg = ST_BLACK; }
    } else if (!known) {
        fg = ST_DIM;
    } else if (i == TP_PLAY) {
        // Stopped still works: the host brings the last radio station back.
        if (ps == 1) { bg = ST_ACCENT; fg = ST_BLACK; }
    } else if (i == TP_STOP) {
        if (ps == 0) fg = ST_DIM;
    } else if (i == TP_PREV) {
        if (!(g_playFlags & 2)) fg = ST_DIM;
    } else if (i == TP_NEXT) {
        if (!(g_playFlags & 4)) fg = ST_DIM;
    }
    if (pressed) { bg = ST_WHITE; fg = ST_BLACK; }
    st_fill(x, TP_Y, w, TP_H, bg);
    const int h = 20;
    int cx = x + w / 2, top = TP_Y + (TP_H - h) / 2;
    switch (i) {
    case TP_PREV:
        st_fill(cx - 10, top, 3, h, fg);
        st_tri(cx - 6, top, h, fg, false);
        break;
    case TP_NEXT:
        st_tri(cx - 10, top, h, fg, true);
        st_fill(cx + 8, top, 3, h, fg);
        break;
    case TP_STOP:
        st_fill(cx - 9, top + 1, 18, 18, fg);
        break;
    case TP_PLAY:
        if (ps == 1) {
            st_fill(cx - 9, top, 6, h, fg);
            st_fill(cx + 3, top, 6, h, fg);
        } else {
            st_tri(cx - 7, top, h, fg, true);
        }
        break;
    case TP_MUTE: {
        const char *label = g_mute ? "MUTED" : "MUTE";
        int tw = (int)strlen(label) * 12 - 2;
        st_text(cx - tw / 2, TP_Y + 15, label, fg, bg, 2);
        break;
    }
    }
}

static uint8_t g_tpDrawnKey = 0xFF;

static uint8_t st_tp_key()
{
    return (uint8_t)(st_play_state() | ((g_playFlags & 7) << 2) | (st_play_known() ? 0x20 : 0) |
                     (g_mute ? 0x40 : 0));
}

static void st_buttons()
{
    for (int i = 0; i < TP_N; i++) st_tp_button(i, false);
    g_tpDrawnKey = st_tp_key();
    g_tpFlash = -1;
}

static void st_fmt_time(char *o, size_t n, uint32_t ms, bool hours)
{
    unsigned long s = ms / 1000u;
    if (hours) snprintf(o, n, "%lu:%02lu:%02lu", s / 3600u, (s / 60u) % 60u, s % 60u);
    else snprintf(o, n, "%lu:%02lu", s / 60u, s % 60u);
}

// Play time right of the clock, progress bar under both. Tapping the bar seeks.
static void st_now_paint(bool force)
{
    static long lastSec = -2;
    static uint32_t lastDur = 0xFFFFFFFF;
    static int lastPx = -2;
    static uint8_t lastState = 0xFF;
    if (g_saver || g_page != PAGE_PLAY) return;
    bool known = st_play_known();
    uint8_t ps = st_play_state();
    uint32_t dur = known ? g_playDur : 0;
    uint32_t pos = st_play_pos();
    long sec = known && ps ? (long)(pos / 1000u) : -1;
    if (force || sec != lastSec || dur != lastDur || ps != lastState) {
        lastSec = sec;
        lastDur = dur;
        st_fill(120, 48, 250, 18, ST_BLACK);
        if (sec >= 0) {
            char a[12], b[12], line[28];
            bool hours = dur >= 3600000u || pos >= 3600000u;
            st_fmt_time(a, sizeof(a), pos, hours);
            if (dur) {
                st_fmt_time(b, sizeof(b), dur, hours);
                snprintf(line, sizeof(line), "%s / %s", a, b);
            } else {
                snprintf(line, sizeof(line), "%s", a);
            }
            int tw = (int)strlen(line) * 12 - 2;
            st_text(368 - tw, 50, line, ps == 2 ? ST_YELLOW : ST_WHITE, ST_BLACK, 2);
        }
    }
    int px = -1;
    if (sec >= 0 && dur) {
        px = (int)((uint64_t)pos * PG_W / dur);
        if (px > PG_W) px = PG_W;
    }
    if (force || px != lastPx || ps != lastState) {
        lastPx = px;
        lastState = ps;
        uint16_t fillc = ps == 2 ? ST_DIM : ST_ACCENT;
        int f = px < 0 ? 0 : px;
        if (f > 0) st_fill(PG_X, PG_Y, f, PG_H, fillc);
        if (f < PG_W) st_fill(PG_X + f, PG_Y, PG_W - f, PG_H, ST_TRACK);
    }
}

static void st_tp_press(int i)
{
    if (i == TP_MUTE) {
        g_mute = !g_mute;
        st_tp_button(TP_MUTE, false);
        g_tpDrawnKey = st_tp_key();
        Serial.printf("[audio] %s\n", g_mute ? "mute" : "unmute");
        return;
    }
    static const uint8_t codes[] = {KEY_PREV, KEY_STOP, KEY_PLAYPAUSE, KEY_NEXT};
    st_key(codes[i], 0);
    // Show the result at once; the next VPLY from the host confirms or corrects it.
    if (i == TP_PLAY && st_play_known() && g_playState) {
        uint32_t pos = st_play_pos();
        g_playState = g_playState == 1 ? 2 : 1;
        g_playPos = pos;
        g_playAt = millis() | 1;
    }
    st_tp_button(i, true);
    g_tpFlash = (int8_t)i;
    g_tpFlashAt = millis();
}

static void st_seek_at(int x)
{
    if (!st_play_known() || !g_playState || !(g_playFlags & 1) || !g_playDur) return;
    int v = (x - PG_X) * 1000 / PG_W;
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    st_key(KEY_SEEK, (uint16_t)v);
    g_playPos = (uint32_t)((uint64_t)g_playDur * (uint32_t)v / 1000u);
    g_playAt = millis() | 1;
    st_now_paint(true);
}

// From loop(): button flash, host state changes, the running clock.
static void st_transport_service()
{
    static uint32_t last = 0;
    if (g_saver || g_page != PAGE_PLAY) return;
    uint32_t now = millis();
    if (g_tpFlash >= 0 && (uint32_t)(now - g_tpFlashAt) >= 140) {
        st_tp_button(g_tpFlash, false);
        g_tpFlash = -1;
    }
    if ((uint32_t)(now - last) < 100) return;
    last = now;
    if (g_tpFlash < 0 && st_tp_key() != g_tpDrawnKey) st_buttons();
    // Volume set by verpixeld, the web page or Home Assistant (VVOL): redraw the bar.
    if (g_volume != g_volShown) st_volume_bar();
    st_now_paint(false);
}

static void st_mute_button()
{
    if (!g_saver && g_page == PAGE_PLAY) st_buttons();
}

// ======================================================================================
// Lists and settings from the host
// ======================================================================================
// 'VLST' fills a list (favorites, radio stations, wall layouts); 'VSET' carries shuffle,
// repeat, wall, sleep timer and A/V sync. Both arrive on core 1 and are drawn on core 0.
enum { LIST_FAVS = 1, LIST_RADIO = 2, LIST_LAYOUTS = 3 };
static constexpr int LIST_MAX = 64, NAME_LEN = 28;
struct PanelList {
    volatile uint8_t gen, total, active, valid, dirty;
    char name[LIST_MAX][NAME_LEN];
};
static PanelList g_lists[3];
static uint8_t g_listPage[3];
static uint32_t g_listFlashAt;

static volatile uint8_t g_setFlags, g_setBright, g_setDirty;   // flags: 1 shuffle, 2 repeat, 4 wall on, 8 radio, 16 audio only
// Spectrum in the screensaver; MORE page, saved with the config.
static volatile bool g_saverVu = true;
static volatile bool g_cfgDirty = false;
static volatile uint16_t g_setSleep;
static volatile int16_t g_setSync;
static volatile uint32_t g_setAt;
static volatile uint32_t g_audioOnlyAt;     // last VSET that said "audio only"
static volatile uint16_t g_alarmMin = 0xFFFF;
static volatile uint8_t g_alarmDays;
static int16_t g_locAlarm = -1;
static uint32_t g_locAlarmAt;
static int16_t g_locSync;
static uint32_t g_locSyncAt;
static uint8_t g_locBright, g_locFlags, g_locFlagMask;
static uint32_t g_locBrightAt, g_locFlagsAt;
static uint8_t g_pendKey;
static uint16_t g_pendArg;
static uint32_t g_pendAt;

static void st_list_chunk(const uint8_t *p, uint16_t n)
{
    if (n < 8) return;
    uint8_t id = p[0], gen = p[1], total = p[2], first = p[3], count = p[4], active = p[5];
    if (id < 1 || id > 3) return;
    PanelList &L = g_lists[id - 1];
    if (total > LIST_MAX) total = LIST_MAX;
    bool changed = L.gen != gen || L.total != total || L.active != active || !L.valid;
    L.gen = gen;
    L.total = total;
    L.active = active;
    L.valid = 1;
    for (int i = 0; i < count; i++) {
        int idx = first + i;
        if (idx >= LIST_MAX || 8 + (i + 1) * NAME_LEN > n) break;
        const char *s = (const char *)p + 8 + i * NAME_LEN;
        if (memcmp(L.name[idx], s, NAME_LEN - 1) != 0) {
            memcpy(L.name[idx], s, NAME_LEN - 1);
            changed = true;
        }
        L.name[idx][NAME_LEN - 1] = 0;
    }
    if (changed) L.dirty = 1;
}

static void st_set_note(const uint8_t *p, uint16_t n)
{
    if (n < 6) return;
    uint16_t sleep = (uint16_t)(p[2] | (p[3] << 8));
    int16_t sync = (int16_t)(p[4] | (p[5] << 8));
    if (p[0] != g_setFlags || p[1] != g_setBright || sleep != g_setSleep || sync != g_setSync) g_setDirty = 1;
    g_setFlags = p[0];
    g_setBright = p[1] > 100 ? 100 : p[1];
    g_setSleep = sleep;
    g_setSync = sync;
    g_setAt = millis() | 1;
    if (p[0] & 16) g_audioOnlyAt = g_setAt;
    if (n >= 9) {
        uint16_t am = (uint16_t)(p[6] | (p[7] << 8));
        if (am != g_alarmMin || p[8] != g_alarmDays) g_setDirty = 1;
        g_alarmMin = am;
        g_alarmDays = p[8];
    }
}

// Music or radio. Held for 6 s, so one late or odd packet does not flip the screensaver.
static bool st_audio_only() { return g_audioOnlyAt && (uint32_t)(millis() - g_audioOnlyAt) < 6000; }
static int st_alarm_min()
{
    if (g_locAlarmAt && (uint32_t)(millis() - g_locAlarmAt) < 2500) return g_locAlarm;
    return g_alarmMin < 1440 ? g_alarmMin : -1;
}

static bool st_set_known() { return g_setAt && (uint32_t)(millis() - g_setAt) < 4000; }
static uint8_t st_bright() { return g_locBrightAt && (uint32_t)(millis() - g_locBrightAt) < 2500 ? g_locBright : g_setBright; }
static int16_t st_sync() { return g_locSyncAt && (uint32_t)(millis() - g_locSyncAt) < 2500 ? g_locSync : g_setSync; }
static uint8_t st_flags()
{
    if (g_locFlagsAt && (uint32_t)(millis() - g_locFlagsAt) < 2500)
        return (uint8_t)((g_setFlags & ~g_locFlagMask) | (g_locFlags & g_locFlagMask));
    return g_setFlags;
}

// A key that follows a drag or repeated taps: sent once things settle.
static void st_key_later(uint8_t code, uint16_t arg)
{
    g_pendKey = code;
    g_pendArg = arg;
    g_pendAt = millis() | 1;
}

static void st_keys_service()
{
    if (!g_pendKey || !g_pendAt) {
        if (g_pendKey && !g_pendAt) { st_key(g_pendKey, g_pendArg); g_pendKey = 0; }
        return;
    }
    if ((uint32_t)(millis() - g_pendAt) < 350) return;
    st_key(g_pendKey, g_pendArg);
    g_pendKey = 0;
}

static void st_key_select(int listId, int index)
{
    g_keyGen = g_lists[listId - 1].gen;
    st_key(KEY_SELECT, (uint16_t)((listId << 12) | (index & 0x0FFF)));
}

// Page arrows at the right of a row: page text, then left and right buttons.
static void st_pager(int y, int page, int pages)
{
    char t[12];
    snprintf(t, sizeof(t), "%d/%d", page + 1, pages);
    int tw = (int)strlen(t) * 12 - 2;
    st_fill(280, y, 58, 32, ST_BLACK);
    st_text(336 - tw, y + 9, t, ST_DIM, ST_BLACK, 2);
    bool canL = page > 0, canR = page + 1 < pages;
    st_fill(340, y, 56, 32, ST_BTN);
    st_tri(340 + 21, y + 8, 16, canL ? ST_WHITE : ST_TRACK, false);
    st_fill(408, y, 56, 32, ST_BTN);
    st_tri(408 + 22, y + 8, 16, canR ? ST_WHITE : ST_TRACK, true);
}

static int st_list_pages(int listId, int rows)
{
    int total = g_lists[listId - 1].total;
    int pages = (total + rows - 1) / rows;
    return pages < 1 ? 1 : pages;
}

static void st_list_rows(int listId, int y0, int rows, int pitch, int h, int flashIdx)
{
    PanelList &L = g_lists[listId - 1];
    int pages = st_list_pages(listId, rows);
    uint8_t &pg = g_listPage[listId - 1];
    if (pg >= pages) pg = (uint8_t)(pages - 1);
    for (int r = 0; r < rows; r++) {
        int idx = pg * rows + r;
        int y = y0 + r * pitch;
        if (idx >= L.total) {
            st_fill(16, y, 448, h, ST_BLACK);
            continue;
        }
        bool act = idx == L.active;
        uint16_t bg = act ? ST_ACCENT : ST_BTN, fg = act ? ST_BLACK : ST_WHITE;
        if (idx == flashIdx) { bg = ST_WHITE; fg = ST_BLACK; }
        st_fill(16, y, 448, h, bg);
        if (act) st_tri(24, y + (h - 12) / 2, 12, fg, true);
        st_text_u8(44, y + (h - 14) / 2, L.name[idx], fg, bg, 2, 34);
    }
    if (L.total == 0) {
        const char *msg = st_play_known() ? "NOTHING HERE YET" : "WAITING FOR VERPIXELD";
        int tw = (int)strlen(msg) * 12;
        st_text((ST_W - tw) / 2, y0 + pitch, msg, ST_DIM, ST_BLACK, 2);
    }
}

static constexpr int LST_TOP = 42, LST_Y = 80, LST_ROWS = 6, LST_PITCH = 33, LST_H = 31;
static constexpr int WALL_TOP = 98, WALL_Y = 136, WALL_ROWS = 4, WALL_PITCH = 36, WALL_H = 33;
static int g_listFlashIdx = -1, g_listFlashList = 0;

static void st_list_page(int listId, bool full)
{
    PanelList &L = g_lists[listId - 1];
    L.dirty = 0;
    if (full) {
        st_fill(0, 38, ST_W, TAB_Y - 38, ST_BLACK);
        st_text(16, LST_TOP + 9, listId == LIST_FAVS ? "FAVORITES" : "RADIO", ST_WHITE, ST_BLACK, 2);
    }
    st_pager(LST_TOP, g_listPage[listId - 1], st_list_pages(listId, LST_ROWS));
    st_list_rows(listId, LST_Y, LST_ROWS, LST_PITCH, LST_H, g_listFlashList == listId ? g_listFlashIdx : -1);
}

static void st_toggle_btn(int x, int y, int w, int h, const char *label, bool on)
{
    uint16_t bg = on ? ST_ACCENT : ST_BTN, fg = on ? ST_BLACK : ST_WHITE;
    st_fill(x, y, w, h, bg);
    int tw = (int)strlen(label) * 12 - 2;
    st_text(x + (w - tw) / 2, y + (h - 14) / 2, label, fg, bg, 2);
}

static void st_wall_page(bool full)
{
    g_lists[LIST_LAYOUTS - 1].dirty = 0;
    if (full) {
        st_fill(0, 38, ST_W, TAB_Y - 38, ST_BLACK);
    }
    uint8_t b = st_bright();
    st_slider(44, "WALL", b, ST_ACCENT);
    st_toggle_btn(16, WALL_TOP, 140, 32, (st_flags() & 4) ? "WALL ON" : "WALL OFF", (st_flags() & 4) != 0);
    st_toggle_btn(164, WALL_TOP, 114, 32, "SPECTRUM", (st_flags() & 32) != 0);
    st_pager(WALL_TOP, g_listPage[LIST_LAYOUTS - 1], st_list_pages(LIST_LAYOUTS, WALL_ROWS));
    st_list_rows(LIST_LAYOUTS, WALL_Y, WALL_ROWS, WALL_PITCH, WALL_H,
                 g_listFlashList == LIST_LAYOUTS ? g_listFlashIdx : -1);
}

static void st_info_draw(int i)
{
    if (i >= 3) return;
    int y = 206 + i * 24;
    st_fill(16, y - 2, 448, 18, ST_BLACK);
    st_text(16, y, g_info[i], i == 0 ? ST_WHITE : ST_DIM, ST_BLACK, 2);
}

// The info lines on the MORE page; hud_stats in the sketch fills them.
static void st_info_lines(char lines[5][40], bool force)
{
    for (int i = 0; i < 5; i++) {
        if (!force && !strcmp(g_info[i], lines[i])) continue;
        strncpy(g_info[i], lines[i], 39);
        g_info[i][39] = 0;
        if (!g_saver && g_page == PAGE_MORE) st_info_draw(i);
    }
}

static void st_more_page(bool full)
{
    if (full) st_fill(0, 38, ST_W, TAB_Y - 38, ST_BLACK);
    uint8_t f = st_flags();
    st_toggle_btn(16, 44, 106, 44, "SHUFFLE", (f & 1) != 0);
    st_toggle_btn(130, 44, 106, 44, "REPEAT", (f & 2) != 0);
    char sl[16];
    if (g_setSleep) snprintf(sl, sizeof(sl), "SLEEP %u", (unsigned)g_setSleep);
    else snprintf(sl, sizeof(sl), "SLEEP");
    st_toggle_btn(244, 44, 106, 44, sl, g_setSleep != 0);
    st_toggle_btn(358, 44, 106, 44, g_saverVu ? "VU ON" : "VU OFF", g_saverVu);
    // A/V sync: minus, value (tap = 0), plus
    st_fill(16, 96, 76, 44, ST_BTN);
    st_fill(16 + 28, 96 + 21, 20, 3, ST_WHITE);
    st_fill(388, 96, 76, 44, ST_BTN);
    st_fill(388 + 28, 96 + 21, 20, 3, ST_WHITE);
    st_fill(388 + 37, 96 + 12, 3, 20, ST_WHITE);
    st_fill(100, 96, 280, 44, ST_HDR);
    char sy[24];
    int16_t ms = st_sync();
    if (ms) snprintf(sy, sizeof(sy), "A/V SYNC %+dMS", (int)ms);
    else snprintf(sy, sizeof(sy), "A/V SYNC 0MS");
    int tw = (int)strlen(sy) * 12 - 2;
    st_text(240 - tw / 2, 111, sy, ms ? ST_YELLOW : ST_WHITE, ST_HDR, 2);
    // Alarm: on/off, minus, time and days, plus (15-minute steps)
    int am = st_alarm_min();
    bool aon = (f & 64) != 0 && am >= 0;
    st_toggle_btn(16, 148, 106, 44, aon ? "ALARM" : "ALARM", aon);
    st_fill(130, 148, 60, 44, ST_BTN);
    st_fill(130 + 20, 148 + 21, 20, 3, ST_WHITE);
    st_fill(404, 148, 60, 44, ST_BTN);
    st_fill(404 + 20, 148 + 21, 20, 3, ST_WHITE);
    st_fill(404 + 29, 148 + 12, 3, 20, ST_WHITE);
    st_fill(198, 148, 198, 44, ST_HDR);
    char at[24];
    if (am < 0) {
        snprintf(at, sizeof(at), "NO ALARM");
    } else {
        uint8_t dm = g_alarmDays;
        const char *days = dm == 0x7F ? "DAILY" : dm == 0x1F ? "MO-FR" : dm == 0x60 ? "SA-SU" : dm == 0 ? "ONCE" : "DAYS";
        snprintf(at, sizeof(at), "%02d:%02d %s", am / 60, am % 60, days);
    }
    int aw = (int)strlen(at) * 12 - 2;
    st_text(297 - aw / 2, 163, at, aon ? ST_WHITE : ST_DIM, ST_HDR, 2);
    if (full)
        for (int i = 0; i < 3; i++) st_info_draw(i);
}

static void st_list_touch(int listId, int x, int y, int topY, int y0, int rows, int pitch, int h)
{
    PanelList &L = g_lists[listId - 1];
    int pages = st_list_pages(listId, rows);
    uint8_t &pg = g_listPage[listId - 1];
    if (y >= topY && y < topY + 32) {
        if (x >= 336 && x < 400 && pg > 0) pg--;
        else if (x >= 404 && pg + 1 < pages) pg++;
        else return;
        st_pager(topY, pg, pages);
        st_list_rows(listId, y0, rows, pitch, h, -1);
        return;
    }
    if (y < y0) return;
    int r = (y - y0) / pitch;
    if (r >= rows || (y - y0) % pitch >= h) return;
    int idx = pg * rows + r;
    if (idx >= L.total) return;
    st_key_select(listId, idx);
    L.active = (uint8_t)idx;
    g_listFlashList = listId;
    g_listFlashIdx = idx;
    g_listFlashAt = millis() | 1;
    st_list_rows(listId, y0, rows, pitch, h, idx);
}

static void st_more_touch(int x, int y)
{
    if (y >= 44 && y < 88) {
        if (x < 126) {
            g_locFlags = (uint8_t)(st_flags() ^ 1);
            g_locFlagMask = 1;
            g_locFlagsAt = millis() | 1;
            st_key(KEY_SHUFFLE, 0);
        } else if (x < 240) {
            g_locFlags = (uint8_t)(st_flags() ^ 2);
            g_locFlagMask = 2;
            g_locFlagsAt = millis() | 1;
            st_key(KEY_REPEAT, 0);
        } else if (x < 354) {
            st_key(KEY_SLEEP, 0);
        } else {
            g_saverVu = !g_saverVu;
            g_cfgDirty = true;
            Serial.printf("[saver] spectrum %s\n", g_saverVu ? "on" : "off");
        }
        st_more_page(false);
        return;
    }
    if (y >= 148 && y < 192) {
        int am = st_alarm_min();
        if (x < 126) {
            g_locFlags = (uint8_t)(st_flags() ^ 64);
            g_locFlagMask = 64;
            g_locFlagsAt = millis() | 1;
            st_key(KEY_ALARM, 0);
        } else {
            if (am < 0) am = 7 * 60;
            else if (x < 194) am -= 15;
            else if (x >= 400) am += 15;
            else return;
            am = (am + 1440) % 1440;
            g_locAlarm = (int16_t)am;
            g_locAlarmAt = millis() | 1;
            st_key_later(KEY_ALARM_TIME, (uint16_t)am);
        }
        st_more_page(false);
        return;
    }
    if (y >= 96 && y < 140) {
        int16_t ms = st_sync();
        if (x < 96) ms = (int16_t)(ms - 20);
        else if (x >= 384) ms = (int16_t)(ms + 20);
        else ms = 0;
        if (ms < -2000) ms = -2000;
        if (ms > 2000) ms = 2000;
        g_locSync = ms;
        g_locSyncAt = millis() | 1;
        st_key_later(KEY_SYNC, (uint16_t)(ms + 2000));
        st_more_page(false);
    }
}

// From loop(): settled keys, list tap flash, redraws when the host sent something new.
static void st_pages_service()
{
    st_keys_service();
    if (g_saver) return;
    if (g_listFlashAt && (uint32_t)(millis() - g_listFlashAt) > 160) {
        g_listFlashAt = 0;
        g_listFlashIdx = -1;
        if (g_page == PAGE_FAVS || g_page == PAGE_RADIO)
            st_list_rows(g_page == PAGE_FAVS ? 1 : 2, LST_Y, LST_ROWS, LST_PITCH, LST_H, -1);
        else if (g_page == PAGE_WALL)
            st_list_rows(LIST_LAYOUTS, WALL_Y, WALL_ROWS, WALL_PITCH, WALL_H, -1);
    }
    if (g_listFlashAt) return;
    switch (g_page) {
    case PAGE_FAVS:
        if (g_lists[0].dirty) st_list_page(1, false);
        break;
    case PAGE_RADIO:
        if (g_lists[1].dirty) st_list_page(2, false);
        break;
    case PAGE_WALL:
        if (g_lists[2].dirty || g_setDirty) {
            g_setDirty = 0;
            st_wall_page(false);
        }
        break;
    case PAGE_MORE:
        if (g_setDirty) {
            g_setDirty = 0;
            st_more_page(false);
        }
        break;
    }
}

// ======================================================================================
// Spectrum: 1024-point FFT of the mono mix, 28 log-spaced bands, 50 Hz to 16 kHz, ~30 Hz
// ======================================================================================
static constexpr int SPEC_N = 1024, SPEC_BANDS = 28;
static int16_t g_specBuf[SPEC_N];
static uint16_t g_specPos;
static bool g_specFresh;
static float g_fre[SPEC_N], g_fim[SPEC_N];
static float g_hann[SPEC_N];
static float g_twc[SPEC_N / 2], g_tws[SPEC_N / 2];
static uint16_t g_bandLo[SPEC_BANDS], g_bandHi[SPEC_BANDS];
static float g_bandTilt[SPEC_BANDS];
static float g_bandLvl[SPEC_BANDS], g_bandPeak[SPEC_BANDS];
static uint32_t g_bandPeakAt[SPEC_BANDS];
static int16_t g_bandDrawn[SPEC_BANDS], g_peakDrawn[SPEC_BANDS];
static bool g_specReady;
// Bands for the wall visualizer: core 0 fills, core 1 sends 'VSPC'.
static uint8_t g_spcOut[SPEC_BANDS];
static volatile uint8_t g_spcSeq;

// Core 0, from pumpAudio: one output frame.
static inline void st_spec_push(int16_t l, int16_t r)
{
    g_specBuf[g_specPos] = (int16_t)((l >> 1) + (r >> 1));
    g_specPos = (uint16_t)((g_specPos + 1) & (SPEC_N - 1));
    g_specFresh = true;
}

static void st_spec_reset()
{
    for (int b = 0; b < SPEC_BANDS; b++) {
        g_bandDrawn[b] = -1;
        g_peakDrawn[b] = -1;
    }
}

static void st_spec_init()
{
    const float tau = 6.2831853f;
    for (int i = 0; i < SPEC_N; i++) g_hann[i] = 0.5f - 0.5f * cosf(tau * i / (SPEC_N - 1));
    for (int k = 0; k < SPEC_N / 2; k++) {
        g_twc[k] = cosf(tau * k / SPEC_N);
        g_tws[k] = -sinf(tau * k / SPEC_N);
    }
    const float f0 = 50.0f, f1 = 16000.0f, binHz = 48000.0f / SPEC_N;
    for (int b = 0; b < SPEC_BANDS; b++) {
        float lo = f0 * powf(f1 / f0, (float)b / SPEC_BANDS);
        float hi = f0 * powf(f1 / f0, (float)(b + 1) / SPEC_BANDS);
        int bl = (int)(lo / binHz + 0.5f), bh = (int)(hi / binHz + 0.5f);
        if (bl < 1) bl = 1;
        if (bh < bl) bh = bl;
        if (bh > SPEC_N / 2 - 1) bh = SPEC_N / 2 - 1;
        g_bandLo[b] = (uint16_t)bl;
        g_bandHi[b] = (uint16_t)bh;
        // Music falls off with frequency; lift the top so the bars use the height.
        g_bandTilt[b] = 3.0f * log2f(sqrtf(lo * hi) / 1000.0f);
    }
    st_spec_reset();
    g_specReady = true;
}

static void st_fft()
{
    for (int i = 1, j = 0; i < SPEC_N; i++) {
        int bit = SPEC_N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = g_fre[i]; g_fre[i] = g_fre[j]; g_fre[j] = t;
            t = g_fim[i]; g_fim[i] = g_fim[j]; g_fim[j] = t;
        }
    }
    for (int len = 2; len <= SPEC_N; len <<= 1) {
        int half = len >> 1, step = SPEC_N / len;
        for (int i = 0; i < SPEC_N; i += len) {
            for (int k = 0; k < half; k++) {
                float wr = g_twc[k * step], wi = g_tws[k * step];
                int a = i + k, b = a + half;
                float tr = g_fre[b] * wr - g_fim[b] * wi;
                float ti = g_fre[b] * wi + g_fim[b] * wr;
                g_fre[b] = g_fre[a] - tr;
                g_fim[b] = g_fim[a] - ti;
                g_fre[a] += tr;
                g_fim[a] += ti;
            }
        }
    }
}

static void st_spec_compute(bool live)
{
    uint32_t now = millis();
    if (live) {
        uint16_t pos = g_specPos;
        for (int i = 0; i < SPEC_N; i++) {
            g_fre[i] = (float)g_specBuf[(pos + i) & (SPEC_N - 1)] * g_hann[i];
            g_fim[i] = 0.0f;
        }
        st_fft();
        const float norm = 1.0f / (32768.0f * SPEC_N / 4.0f);
        for (int b = 0; b < SPEC_BANDS; b++) {
            float m = 0.0f;
            for (int k = g_bandLo[b]; k <= g_bandHi[b]; k++) {
                float p = g_fre[k] * g_fre[k] + g_fim[k] * g_fim[k];
                if (p > m) m = p;
            }
            float db = 10.0f * log10f(m * norm * norm + 1e-12f) + g_bandTilt[b];
            float v = (db + 60.0f) / 54.0f;
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            float l = g_bandLvl[b] - 0.04f;
            g_bandLvl[b] = v > l ? v : (l > 0.0f ? l : 0.0f);
        }
    } else {
        for (int b = 0; b < SPEC_BANDS; b++) {
            float l = g_bandLvl[b] - 0.04f;
            g_bandLvl[b] = l > 0.0f ? l : 0.0f;
        }
    }
    for (int b = 0; b < SPEC_BANDS; b++) {
        if (g_bandLvl[b] >= g_bandPeak[b]) {
            g_bandPeak[b] = g_bandLvl[b];
            g_bandPeakAt[b] = now;
        } else if ((uint32_t)(now - g_bandPeakAt[b]) > 600) {
            float p = g_bandPeak[b] - 0.015f;
            g_bandPeak[b] = p > 0.0f ? p : 0.0f;
        }
    }
}

// Bars in segments of 4 px (3 lit, 1 gap), green / yellow / red, with a peak cap.
static void st_spec_draw(int y0, int h)
{
    const int x0 = 16, pitch = 16, bw = 14, seg = 4;
    static uint8_t col[2][14 * 48 * 3];
    static int cur = 0;
    bool busy = false;
    uint8_t gr, gg, gb, yr, yg, yb, rr, rg, rb, dr, dg, db, pr, pg, pb;
    st_pix(ST_GREEN, gr, gg, gb);
    st_pix(ST_YELLOW, yr, yg, yb);
    st_pix(ST_RED, rr, rg, rb);
    st_pix(0x10A2, dr, dg, db);
    st_pix(ST_WHITE, pr, pg, pb);
    for (int b = 0; b < SPEC_BANDS; b++) {
        int px = (int)(g_bandLvl[b] * h);
        px -= px % seg;
        int pk = (int)(g_bandPeak[b] * h);
        pk -= pk % seg;
        if (pk < seg) pk = 0;
        if (px == g_bandDrawn[b] && pk == g_peakDrawn[b]) continue;
        g_bandDrawn[b] = (int16_t)px;
        g_peakDrawn[b] = (int16_t)pk;
        uint8_t *c = col[cur];
        int n = 0;
        for (int row = 0; row < h; row++) {
            int yb2 = h - 1 - row;                // 0 at the bottom
            bool gap = (yb2 % seg) == seg - 1;
            uint8_t r = 0, g = 0, bl = 0;
            if (!gap) {
                if (yb2 < px) {
                    if (yb2 * 100 < h * 60) { r = gr; g = gg; bl = gb; }
                    else if (yb2 * 100 < h * 85) { r = yr; g = yg; bl = yb; }
                    else { r = rr; g = rg; bl = rb; }
                } else if (pk > px && yb2 >= pk - seg && yb2 < pk) {
                    r = pr; g = pg; bl = pb;
                } else {
                    r = dr; g = dg; bl = db;
                }
            }
            for (int x = 0; x < bw; x++) {
                c[n++] = r;
                c[n++] = g;
                c[n++] = bl;
            }
        }
        if (busy) {
            st_dma_wait();
            digitalWrite(PIN_CS, HIGH);
        }
        st_window(x0 + b * pitch, y0, bw, h);
        digitalWrite(PIN_DC, HIGH);
        digitalWrite(PIN_CS, LOW);
        st_dma_send(c, (size_t)n);
        busy = true;
        cur ^= 1;
    }
    if (busy) {
        st_dma_wait();
        digitalWrite(PIN_CS, HIGH);
    }
}

static void st_spectrum()
{
    static uint32_t last = 0;
    uint32_t now = millis();
    if ((uint32_t)(now - last) < 33) return;
    int y0 = 0, h = 0;
    bool draw = false;
    if (g_wander) {
    } else if (g_saver) {
        if (!g_picDrawn && g_saverVu) { y0 = 228; h = 48; draw = true; }
    } else if (g_page == PAGE_PLAY) {
        y0 = 246;
        h = 32;
        draw = true;
    }
    bool wall = st_set_known() && (g_setFlags & 32);
    if (!draw && !wall) return;
    last = now;
    if (!g_specReady) st_spec_init();
    bool live = g_specFresh;
    g_specFresh = false;
    if (!live) {
        bool any = false;
        for (int b = 0; b < SPEC_BANDS && !any; b++)
            any = g_bandLvl[b] > 0.0f || g_bandPeak[b] > 0.0f || g_bandDrawn[b] != 0 || g_peakDrawn[b] != 0;
        if (!any && !wall) return;
    }
    st_spec_compute(live);
    if (wall) {
        for (int b = 0; b < SPEC_BANDS; b++) {
            int v = (int)(g_bandLvl[b] * 255.0f + 0.5f);
            g_spcOut[b] = (uint8_t)(v > 255 ? 255 : v);
        }
        g_spcSeq = (uint8_t)(g_spcSeq + 1);
    }
    if (draw) st_spec_draw(y0, h);
}

// ======================================================================================
// Touch
// ======================================================================================
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
    SPI.beginTransaction(SPISettings(g_spiWant, MSBFIRST, SPI_MODE0));
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
    static uint32_t tapAt = 0;
    static uint32_t coverBlock = 0;
    int x, y;
    bool down = st_finger(x, y);
    if (!down) {
        if (woke) coverBlock = millis() + 500;
        // Brightness goes out when the finger lifts.
        if (was && g_pendKey == KEY_BRIGHT) g_pendAt = 0;
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
    g_touchMs = millis();
    bool tap = !was && (uint32_t)(millis() - tapAt) > 200;
    if (tap) tapAt = millis();

    if (y >= TAB_Y - 4) {
        if (tap) {
            int t = x / TAB_W;
            st_page_go((uint8_t)(t < PAGE_N ? t : PAGE_N - 1));
        }
        was = true;
        return;
    }

    switch (g_page) {
    case PAGE_PLAY: {
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
        } else if (tap && y >= TP_Y - 4 && y < TP_Y + TP_H + 6) {
            for (int i = 0; i < TP_N; i++) {
                if (x < TP_X[i] - 4 || x >= TP_X[i] + TP_W[i] + 4) continue;
                st_tp_press(i);
                break;
            }
        } else if (tap && y >= PG_Y - 14 && y < PG_Y + PG_H + 14 && x >= PG_X - 4 && x < PG_X + PG_W + 4) {
            st_seek_at(x);
        } else if (tap && y >= 117 && y < 138 && g_scroll[0]) {
            st_key(KEY_SAVE_TITLE, 0);
            g_titleSavedAt = millis() | 1;
            st_scroll_paint();
        }
        break;
    }
    case PAGE_FAVS:
    case PAGE_RADIO:
        if (tap) st_list_touch(g_page == PAGE_FAVS ? 1 : 2, x, y, LST_TOP, LST_Y, LST_ROWS, LST_PITCH, LST_H);
        break;
    case PAGE_WALL:
        if (y >= 44 && y < 92) {
            int v = (x - 16) * 100 / 448;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            if (v != st_bright()) {
                g_locBright = (uint8_t)v;
                g_locBrightAt = millis() | 1;
                g_locFlags = v ? 4 : 0;
                g_locFlagMask = 4;
                g_locFlagsAt = g_locBrightAt;
                st_slider(44, "WALL", (uint8_t)v, ST_ACCENT);
                st_key_later(KEY_BRIGHT, (uint16_t)v);
            }
        } else if (tap && y >= WALL_TOP && y < WALL_TOP + 32 && x >= 164 && x < 280) {
            g_locFlags = (uint8_t)(st_flags() ^ 32);
            g_locFlagMask = 32;
            g_locFlagsAt = millis() | 1;
            st_key(KEY_VISUAL, 0);
            st_toggle_btn(164, WALL_TOP, 114, 32, "SPECTRUM", (st_flags() & 32) != 0);
        } else if (tap && y >= WALL_TOP && y < WALL_TOP + 32 && x < 160) {
            g_locFlags = (uint8_t)(st_flags() ^ 4);
            g_locFlagMask = 4;
            g_locFlagsAt = millis() | 1;
            st_key(KEY_WALL, 0);
            st_toggle_btn(16, WALL_TOP, 140, 32, (st_flags() & 4) ? "WALL ON" : "WALL OFF", (st_flags() & 4) != 0);
        } else if (tap) {
            st_list_touch(LIST_LAYOUTS, x, y, WALL_TOP, WALL_Y, WALL_ROWS, WALL_PITCH, WALL_H);
        }
        break;
    case PAGE_MORE:
        if (tap) st_more_touch(x, y);
        break;
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

// Core 1. The lock keeps core 0 from swapping buffers while a chunk is copied in.
static void st_pic_chunk(const uint8_t *p, uint16_t n)
{
    if (n < 12) return;
    uint8_t frame = p[0], index = p[1], count = p[2];
    uint8_t oy = p[3], th = p[4];
    uint16_t w, h;
    memcpy(&w, p + 8, 2);
    memcpy(&h, p + 10, 2);
    if (w < 2 || w > 480 || h < 2 || h > 160 || count == 0 || count > 160 || index >= count) return;
    if (th == 0 || oy >= h || (uint16_t)oy + th > h) return;
    uint16_t bytes = (uint16_t)(n - 12);
    uint16_t chunk = 960;
    memcpy(&chunk, p + 5, 2);
    if (chunk < 64 || chunk > 1440) chunk = 960;
    uint32_t irq = spin_lock_blocking(g_picLock);
    if (g_swapPending && !g_blitting) st_pic_flip();
    if (frame != g_picFrame || index == 0) {
        // A finished frame still waits for the blit: do not write over it.
        if (g_swapPending) {
            spin_unlock(g_picLock, irq);
            return;
        }
        g_picFrame = frame;
        g_picCount = count;
        g_buildY = oy;
        g_buildH = th;
        g_buildW = w;
        memset(g_picSeen, 0, sizeof(g_picSeen));
    }
    uint32_t off = (uint32_t)g_buildY * g_buildW * 2u + (uint32_t)index * chunk;
    if (off + bytes <= sizeof(g_pic[0])) {
        memcpy(g_pic[g_picBuild] + off, p + 12, bytes);
        g_picSeen[index] = 1;
        bool full = true;
        for (uint8_t i = 0; i < count; i++) if (!g_picSeen[i]) { full = false; break; }
        if (full) {
            if (g_blitting) g_swapPending = 1;
            else st_pic_flip();
        }
    }
    spin_unlock(g_picLock, irq);
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
    uint32_t irq = spin_lock_blocking(g_picLock);
    if (g_swapPending) st_pic_flip();
    g_blitting = 1;
    uint8_t front = g_picFront;
    uint8_t y0 = g_frontY;
    uint8_t th = g_frontH ? g_frontH : 120;
    int fw = g_frontW ? g_frontW : 160;
    spin_unlock(g_picLock, irq);

    if ((uint16_t)y0 + th > 160) th = (uint8_t)(160 - y0);
    static uint8_t drawnCrop = 0xFF;
    uint8_t cropNow = g_fillCrop ? 1 : 0;
    if (y0 != g_drawnY || th != g_drawnH || fw != g_drawnW || cropNow != drawnCrop) {
        st_fill(0, 0, ST_W, ST_H, ST_BLACK);
        g_drawnY = y0;
        g_drawnH = th;
        g_drawnW = (uint16_t)fw;
        drawnCrop = cropNow;
        g_saverKey = -1;
        g_scrollShown = (uint8_t)(g_scrollGen - 1);
    }
    const uint8_t *src = g_pic[front];
    int scale = (fw > 0 && fw <= ST_W && (ST_W % fw) == 0) ? ST_W / fw : 1;
    if (scale > 3) scale = 3;
    if (scale < 1) scale = 1;
    int srcX = 0;
    int srcW = fw;
    if (g_fillCrop && th > 0) {
        scale = 1;
        while (scale < 4 && (scale + 1) * (int)th <= ST_H) scale++;
        srcW = ST_W / scale;
        if (srcW > fw) srcW = fw;
        if (srcW < 1) srcW = 1;
        srcX = (fw - srcW) / 2;
        if ((int)th * scale > ST_H) {
            int srcRows = ST_H / scale;
            if (srcRows < 1) srcRows = 1;
            y0 = (uint8_t)(y0 + ((int)th - srcRows) / 2);
            th = (uint8_t)srcRows;
        }
    } else {
        while (scale > 1 && th * scale > 276) scale--;
    }
    int bandPx = th * scale;
    int yOrg = g_fillCrop ? (ST_H - bandPx) / 2 : (276 - bandPx) / 2;
    if (yOrg < 0) yOrg = 0;
    int dw = srcW * scale;
    int xOrg = (ST_W - dw) / 2;
    if (xOrg < 0) xOrg = 0;

    // Two row buffers: one goes out by DMA while the next is converted.
    static uint8_t rows[2][480 * 3];
    int cur = 0;
    bool busy = false;
    st_window(xOrg, yOrg, dw, bandPx);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    for (int sy = y0; sy < y0 + th; sy++) {
        uint8_t *row = rows[cur];
        const uint8_t *s = src + sy * fw * 2 + srcX * 2;
        int n = 0;
        for (int sx = 0; sx < srcW; sx++) {
            uint16_t p = (uint16_t)((s[sx * 2] << 8) | s[sx * 2 + 1]);
            uint8_t r, g, b;
            st_pix(p, r, g, b);
            for (int repx = 0; repx < scale; repx++) {
                row[n++] = r;
                row[n++] = g;
                row[n++] = b;
            }
        }
        for (int rep = 0; rep < scale; rep++) {
            if (busy) st_dma_wait();
            st_dma_send(row, (size_t)n);
            busy = true;
        }
        cur ^= 1;
    }
    if (busy) st_dma_wait();
    digitalWrite(PIN_CS, HIGH);

    irq = spin_lock_blocking(g_picLock);
    g_blitting = 0;
    if (g_swapPending) st_pic_flip();
    spin_unlock(g_picLock, irq);
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
    const uint8_t *p = (const uint8_t *)s;
    int n = 0;
    while (st_utf8(p)) n++;
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
        const uint8_t *p = (const uint8_t *)g_scroll;
        int base = origin + copy * cycle;
        for (int i = 0; i < cols; i++) {
            const uint8_t *g = st_font(st_utf8(p));
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

// Saver without a picture: title and artist ("Artist - Title" split at the dash), each a
// line of large text that slides when it is wider than the space beside the cover.
struct Marquee {
    int x, y, w;
    uint16_t color;
    int off, cols;
    uint32_t next;
    char text[97];
};
static Marquee g_mq[2];
static uint8_t g_mqBuf[456 * 14 * 3];

static void st_mq_draw(Marquee &m)
{
    if (m.w <= 0) return;
    int w = m.w > 456 ? 456 : m.w;
    memset(g_mqBuf, 0, (size_t)w * 14 * 3);
    int total = m.cols * 12;
    bool slide = total > w;
    int cycle = total + 48;
    uint8_t cr, cg, cb;
    st_pix(m.color, cr, cg, cb);
    int origin = slide ? -m.off : 0;
    for (int copy = 0; copy < (slide ? 2 : 1); copy++) {
        const uint8_t *p = (const uint8_t *)m.text;
        int base = origin + copy * cycle;
        uint32_t cp;
        int i = 0;
        while ((cp = st_utf8(p)) != 0) {
            int gx = base + i * 12;
            i++;
            if (gx >= w || gx + 10 <= 0) continue;
            const uint8_t *g = st_font(cp);
            for (int col = 0; col < 10; col++) {
                int x = gx + col;
                if (x < 0 || x >= w) continue;
                uint8_t bits = g[col / 2];
                for (int row = 0; row < 14; row++) {
                    if (!(bits & (1u << (row / 2)))) continue;
                    int o = (row * w + x) * 3;
                    g_mqBuf[o] = cr;
                    g_mqBuf[o + 1] = cg;
                    g_mqBuf[o + 2] = cb;
                }
            }
        }
    }
    st_window(m.x, m.y, w, 14);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    st_dma_send(g_mqBuf, (size_t)w * 14 * 3);
    st_dma_wait();
    digitalWrite(PIN_CS, HIGH);
}

static void st_mq_set(Marquee &m, int x, int y, int w, const char *s, uint16_t color, bool center)
{
    strncpy(m.text, s, 96);
    m.text[96] = 0;
    m.cols = st_u8_cols(m.text);
    int total = m.cols * 12;
    m.color = color;
    m.off = 0;
    m.next = millis() + 1500;
    if (center && total <= w) {
        m.x = x + (w - total) / 2;
        m.w = total;
    } else {
        m.x = x;
        m.w = w;
    }
    m.y = y;
    if (m.text[0]) st_mq_draw(m);
}

static void st_mq_tick(Marquee &m)
{
    if (!m.text[0] || m.cols * 12 <= m.w) return;
    uint32_t now = millis();
    if ((int32_t)(now - m.next) < 0) return;
    m.next = now + 40;
    m.off += 2;
    if (m.off >= m.cols * 12 + 48) {
        m.off = 0;
        m.next = now + 1500;
    }
    st_mq_draw(m);
}

static void st_saver_info()
{
    bool cover = g_saverCov >= 0;
    int x0 = cover ? 208 : 12;
    int w = cover ? ST_W - 8 - x0 : ST_W - 24;
    st_fill(cover ? x0 : 0, 104, cover ? ST_W - x0 : ST_W, 100, ST_BLACK);
    g_mq[0].text[0] = 0;
    g_mq[1].text[0] = 0;
    char artist[97], title[97];
    artist[0] = 0;
    strncpy(title, g_scroll, 96);
    title[96] = 0;
    const char *dash = strstr(g_scroll, " - ");
    if (dash) {
        size_t a = (size_t)(dash - g_scroll);
        if (a > 96) a = 96;
        memcpy(artist, g_scroll, a);
        artist[a] = 0;
        strncpy(title, dash + 3, 96);
        title[96] = 0;
    }
    if (!title[0]) return;
    st_mq_set(g_mq[0], x0, 116, w, title, ST_WHITE, !cover);
    if (artist[0]) st_mq_set(g_mq[1], x0, 148, w, artist, ST_DIM, !cover);
}

static void st_scroll_paint()
{
    if (g_wander) return;
    if (g_saver) {
        if (g_picDrawn) {
            if (!g_scroll[0]) st_fill(0, 282, ST_W, 8, ST_BLACK);
            else st_scroll_draw(282, ST_W);
        } else {
            st_saver_info();
        }
    } else if (g_page == PAGE_PLAY) {
        if (g_titleSavedAt && (uint32_t)(millis() - g_titleSavedAt) < 1500) {
            st_fill(0, 124, ST_W, 8, ST_BLACK);
            const char *t = "SONG SAVED";
            st_text((ST_W - (int)strlen(t) * 6) / 2, 124, t, ST_ACCENT, ST_BLACK, 1);
        } else if (!g_scroll[0]) st_fill(0, 124, ST_W, 8, ST_BLACK);
        else st_scroll_draw(124, ST_W);
    }
    g_scrollShown = g_scrollGen;
}

static void st_scroll_tick()
{
    if (g_titleSavedAt && (uint32_t)(millis() - g_titleSavedAt) >= 1500) {
        g_titleSavedAt = 0;
        g_scrollShown = (uint8_t)(g_scrollGen - 1);
    }
    if (g_titleSavedAt) return;
    if (g_scrollShown != g_scrollGen) {
        st_scroll_paint();
        if (!g_scroll[0]) return;
    }
    if (!g_scroll[0] || g_wander) return;
    if (g_saver && !g_picDrawn) {
        st_mq_tick(g_mq[0]);
        st_mq_tick(g_mq[1]);
        return;
    }
    bool marquee = g_saver ? g_picDrawn != 0 : g_page == PAGE_PLAY;
    if (!marquee) return;
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

// 80×80 cover drawn at 2× (160×160).
static void st_cover2x(int x0, int y0)
{
    static uint8_t row[2][ST_COVER * 2 * 3];
    int cur = 0;
    bool busy = false;
    st_window(x0, y0, ST_COVER * 2, ST_COVER * 2);
    digitalWrite(PIN_DC, HIGH);
    digitalWrite(PIN_CS, LOW);
    for (int y = 0; y < ST_COVER; y++) {
        const uint8_t *s = g_cover + y * ST_COVER * 2;
        uint8_t *r = row[cur];
        int n = 0;
        for (int x = 0; x < ST_COVER; x++) {
            uint16_t p = (uint16_t)((s[x * 2] << 8) | s[x * 2 + 1]);
            uint8_t cr, cg, cb;
            st_pix(p, cr, cg, cb);
            for (int k = 0; k < 2; k++) {
                r[n++] = cr;
                r[n++] = cg;
                r[n++] = cb;
            }
        }
        for (int k = 0; k < 2; k++) {
            if (busy) st_dma_wait();
            st_dma_send(r, (size_t)n);
            busy = true;
        }
        cur ^= 1;
    }
    if (busy) st_dma_wait();
    digitalWrite(PIN_CS, HIGH);
}

static void st_draw_saver()
{
    if (g_wander) {
        if (g_saverClear) {
            st_fill(0, 0, ST_W, ST_H, ST_BLACK);
            g_saverClear = false;
            g_wanderX = -1;
            g_saverKey = -1;
        }
        st_wander_clock();
        return;
    }
    // Music or radio: the cover layout, whatever the wall shows.
    bool audioOnly = st_audio_only();
    if (g_picDrawn && (audioOnly || (uint32_t)(millis() - g_picMs) > 12000)) {
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
        g_saverCov = -2;
        g_scrollShown = (uint8_t)(g_scrollGen - 1);
        st_spec_reset();
    }
    int hh, mm;
    civil(hh, mm);
    int key = g_haveClock ? (hh * 60 + mm) : -2;
    bool newPic = !audioOnly && g_picGen != g_picDrawn && g_picGen != 0;
    if (newPic) {
        st_blit_pic();
        g_picDrawn = g_picGen;
    }
    if (!g_picDrawn) {
        int cov = g_coverReady ? g_coverGen : -1;
        if (cov != g_saverCov) {
            if (g_saverCov != -2) st_fill(0, 36, ST_W, 190, ST_BLACK);
            g_saverCov = cov;
            if (cov >= 0) st_cover2x(24, 40);
            g_saverKey = -3;
            g_scrollShown = (uint8_t)(g_scrollGen - 1);
        }
    }
    bool clockSame = key == g_saverKey;
    bool titleSame = g_scrollShown == g_scrollGen;
    if (clockSame && titleSame) return;
    char t[8];
    if (hh < 0) snprintf(t, sizeof(t), "--:--");
    else snprintf(t, sizeof(t), "%02d:%02d", hh, mm);
    int cols = (int)strlen(t);
    if (!clockSame) {
        if (g_picDrawn) {
            int tw = cols * 12;
            int x = (ST_W - tw) / 2;
            st_fill(x - 8, 292, tw + 16, 22, ST_BLACK);
            st_text(x, 294, t, ST_WHITE, ST_BLACK, 2);
        } else {
            int tw = cols * 24;
            int x = g_saverCov >= 0 ? 208 : (ST_W - tw) / 2;
            st_fill(x - 8, 44, tw + 16, 36, ST_BLACK);
            st_text(x, 48, t, ST_WHITE, ST_BLACK, 4);
        }
        g_saverKey = key;
    }
    if (!titleSame) st_scroll_paint();
}

static void st_cover_service()
{
    if (g_saver || g_page != PAGE_PLAY) return;
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
    g_coverGen = (uint8_t)((g_coverGen + 1) & 0x7F);
    g_coverReady = 1;
    g_coverDrawn = 0;
}

static void st_menu_clock(bool force)
{
    if (g_saver || g_page != PAGE_PLAY) return;
    int hh, mm;
    civil(hh, mm);
    int key = g_haveClock ? (hh * 60 + mm) : -2;
    static int drawn = -9999;
    if (!force && key == drawn) return;
    drawn = key;
    char t[8];
    if (hh < 0) snprintf(t, sizeof(t), "--:--");
    else snprintf(t, sizeof(t), "%02d:%02d", hh, mm);
    st_fill(16, 44, 96, 26, ST_BLACK);
    st_text(16, 46, t, ST_WHITE, ST_BLACK, 3);
}
