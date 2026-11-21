// Codec.h — WM8960 over bit-banged I2C on GP6/GP7.
// The hardware Wire peripheral returns 4/5 once I2S DMA is running.
// Bit-banging with interrupts off for one transaction does not touch that IRQ.
#pragma once

static constexpr uint8_t WM_ADDR = 0x1A;

static void bb_delay() { delayMicroseconds(10); }

static bool bb_wait_scl_high()
{
    pinMode(PIN_SCL, INPUT_PULLUP);
    for (int i = 0; i < 50; i++) {
        if (digitalRead(PIN_SCL) == HIGH) return true;
        delayMicroseconds(2);
    }
    return false;
}

static void bb_sda(bool high)
{
    if (high) pinMode(PIN_SDA, INPUT_PULLUP);
    else {
        pinMode(PIN_SDA, OUTPUT);
        digitalWrite(PIN_SDA, LOW);
    }
}

static void bb_scl(bool high)
{
    if (high) pinMode(PIN_SCL, INPUT_PULLUP);
    else {
        pinMode(PIN_SCL, OUTPUT);
        digitalWrite(PIN_SCL, LOW);
    }
}

static void bb_start()
{
    bb_sda(true);
    bb_scl(true);
    bb_delay();
    bb_sda(false);
    bb_delay();
    bb_scl(false);
}

static void bb_stop()
{
    bb_sda(false);
    bb_delay();
    bb_scl(true);
    bb_delay();
    bb_sda(true);
    bb_delay();
}

// Returns true if the codec ACKed.
static bool bb_byte(uint8_t v)
{
    for (int i = 0; i < 8; i++) {
        bb_sda(v & 0x80);
        bb_delay();
        bb_scl(true);
        bb_delay();
        bb_scl(false);
        v <<= 1;
    }
    bb_sda(true);
    delayMicroseconds(5);
    if (!bb_wait_scl_high()) {
        bb_scl(false);
        return false;
    }
    // One sample is not enough: a slow ACK or a coupled edge looks like a NACK.
    int low = 0;
    for (int s = 0; s < 8; s++) {
        if (digitalRead(PIN_SDA) == LOW) low++;
        delayMicroseconds(3);
    }
    bb_scl(false);
    bb_delay();
    return low >= 2;
}

// 0 ok, 2 address NACK, 3 data NACK.
// Both data bytes are always clocked out. A false NACK must not skip the second byte.
static int wm_write(uint8_t reg, uint16_t val)
{
    noInterrupts();
    bb_start();
    bool a = bb_byte((uint8_t)(WM_ADDR << 1));
    bool b = bb_byte((uint8_t)((reg << 1) | (val >> 8)));
    bool c = bb_byte((uint8_t)(val & 0xFF));
    bb_stop();
    interrupts();
    if (!a) return 2;
    if (!b || !c) return 3;
    return 0;
}

static bool wm_reg(uint8_t reg, uint16_t val)
{
    int e = 0;
    for (int n = 0; n < 5; n++) {
        e = wm_write(reg, val);
        if (!e) return true;
        delayMicroseconds(400);
    }
    Serial.printf("[codec] reg 0x%02X fail %d\n", reg, e);
    return false;
}

// Codec-master BCLK is ~1.5 MHz push-pull. A floating slave pin stays low
// with the pulldown, so a warm BOOTSEL (clocks already running) is obvious.
static uint32_t i2s_bclk_edges()
{
    gpio_set_function(PIN_BCLK, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_BCLK, GPIO_IN);
    gpio_pull_down(PIN_BCLK);
    delayMicroseconds(50);
    uint32_t edges = 0;
    int last = gpio_get(PIN_BCLK);
    uint32_t t0 = micros();
    while ((uint32_t)(micros() - t0) < 1000) {
        int v = gpio_get(PIN_BCLK);
        if (v != last) {
            edges++;
            last = v;
        }
    }
    gpio_disable_pulls(PIN_BCLK);
    return edges;
}

// Only called when BCLK is idle. A running master ignores this bus.
static bool wm_wake()
{
    int e = wm_write(0x0F, 0x0000);
    if (e) Serial.printf("[codec] reset fail %d\n", e);
    return e == 0;
}

// Codec crystal generates BCLK and LRCLK. The master bit is the last byte.
static bool wm_init()
{
    if (!wm_wake()) return false;
    delay(10);
    if (!wm_reg(0x19, 0x01C0)) return false;
    delay(150);
    if (!wm_reg(0x04, 0x0010)) return false;   // MCLK, DAC /512 = 48 kHz
    if (!wm_reg(0x08, 0x01CA)) return false;   // BCLK = MCLK/16 once master
    if (!wm_reg(0x0A, 0x01FF)) return false;
    if (!wm_reg(0x0B, 0x01FF)) return false;
    if (!wm_reg(0x22, 0x0100)) return false;
    if (!wm_reg(0x25, 0x0100)) return false;
    if (!wm_reg(0x2F, 0x000C)) return false;
    if (!wm_reg(0x1A, 0x01FA)) return false;
    if (!wm_reg(0x31, 0x00F7)) return false;
    delay(50);
    wm_reg(0x19, 0x00C0);
    delay(20);
    wm_reg(0x05, 0x0000);
    wm_reg(0x02, 0x0179);
    wm_reg(0x03, 0x0179);
    wm_reg(0x28, 0x0179);
    wm_reg(0x29, 0x0179);
    if (!wm_reg(0x07, 0x0042)) return false;   // I2S master, 16-bit. Clocks start here.
    delay(2);
    return true;
}
