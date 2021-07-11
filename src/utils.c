#include "uf2.h"
#include "neopixel.h"

static uint32_t timerLow;
uint32_t timerHigh, resetHorizon;

void delay(uint32_t ms) {
// SAMD21 starts up at 1mhz by default.
#ifdef SAMD21
    ms <<= 8;
#endif
#ifdef SAML21
    ms <<= 8;
#endif
// SAMD51 starts up at 48mhz by default.
#ifdef SAMD51
    ms <<= 12;
#endif
    for (int i = 1; i < ms; ++i) {
        asm("nop");
    }
}

void timerTick(void) {
    if (timerLow-- == 0) {
        timerLow = TIMER_STEP;
        timerHigh++;
        if (resetHorizon && timerHigh >= resetHorizon) {
            resetHorizon = 0;
            resetIntoApp();
        }
    }
}

void panic(int code) {
    logval("PANIC", code);
    while (1) {
    }
}

int writeNum(char *buf, uint32_t n, bool full) {
    int i = 0;
    int sh = 28;
    while (sh >= 0) {
        int d = (n >> sh) & 0xf;
        if (full || d || sh == 0 || i) {
            buf[i++] = d > 9 ? 'A' + d - 10 : '0' + d;
        }
        sh -= 4;
    }
    return i;
}

void resetIntoApp() {
    // reset without waiting for double tap (only works for one reset)
    RGBLED_set_color(COLOR_LEAVE);
    *DBL_TAP_PTR = DBL_TAP_MAGIC_QUICK_BOOT;
    NVIC_SystemReset();
}

void resetIntoBootloader() {
    // reset without waiting for double tap (only works for one reset)
    *DBL_TAP_PTR = DBL_TAP_MAGIC;
    NVIC_SystemReset();
}

#if USE_LOGS
struct LogStore logStoreUF2;

void logreset() {
    logStoreUF2.ptr = 0;
    logmsg("Reset logs.");
}

void logwritenum(uint32_t n) {
    char buff[9];
    buff[writeNum(buff, n, false)] = 0;
    logwrite("0x");
    logwrite(buff);
}

void logwrite(const char *msg) {
    const int jump = sizeof(logStoreUF2.buffer) / 4;
    if (logStoreUF2.ptr >= sizeof(logStoreUF2.buffer) - jump) {
        logStoreUF2.ptr -= jump;
        memmove(logStoreUF2.buffer, logStoreUF2.buffer + jump, logStoreUF2.ptr);
    }
    int l = strlen(msg);
    if (l + logStoreUF2.ptr >= sizeof(logStoreUF2.buffer)) {
        logwrite("TOO LONG!\n");
        return;
    }
    memcpy(logStoreUF2.buffer + logStoreUF2.ptr, msg, l);
    logStoreUF2.ptr += l;
    logStoreUF2.buffer[logStoreUF2.ptr] = 0;
}

void logmsg(const char *msg) {
    logwrite(msg);
    logwrite("\n");
}

void logval(const char *lbl, uint32_t v) {
    logwrite(lbl);
    logwrite(": ");
    logwritenum(v);
    logwrite("\n");
}
#endif

static uint32_t now;
static uint32_t signal_end;
int8_t led_tick_step = 1;
static uint8_t limit = 200;

void led_tick() {
    now++;
    if (signal_end) {
        if (now == signal_end - 1000) {
            LED_MSC_ON();
        }
        if (now == signal_end) {
            signal_end = 0;
        }
    } else {
        uint8_t curr = now & 0xff;
        if (curr == 0) {
            LED_MSC_ON();
            if (limit < 10 || limit > 250) {
                led_tick_step = -led_tick_step;
            }
            limit += led_tick_step;
        } else if (curr == limit) {
            LED_MSC_OFF();
        }
    }
}

void led_signal() {
    if (signal_end < now) {
        signal_end = now + 2000;
        LED_MSC_OFF();
    }
}

void led_init() {
#if defined(LED_PIN)
    PINOP(LED_PIN, DIRSET);
#endif
    LED_MSC_ON();

#if defined(BOARD_RGBLED_CLOCK_PIN)
    // using APA102, set pins to outputs
    PINOP(BOARD_RGBLED_CLOCK_PIN, DIRSET);
    PINOP(BOARD_RGBLED_DATA_PIN, DIRSET);

    // This won't work for neopixel, because we're running at 1MHz or thereabouts...
    RGBLED_set_color(COLOR_LEAVE);
#endif

#if USE_SCREEN
    // turn display backlight
    screen_early_init();
#endif
}

#if defined(BOARD_RGBLED_CLOCK_PIN)
void write_apa_byte(uint8_t x) {
    for (uint8_t i = 0x80; i != 0; i >>= 1) {
        if (x & i)
            PINOP(BOARD_RGBLED_DATA_PIN, OUTSET);
        else
            PINOP(BOARD_RGBLED_DATA_PIN, OUTCLR);

        PINOP(BOARD_RGBLED_CLOCK_PIN, OUTSET);
        // for (uint8_t j=0; j<25; j++) /* 0.1ms */
        //  __asm__ __volatile__("");

        PINOP(BOARD_RGBLED_CLOCK_PIN, OUTCLR);
        // for (uint8_t j=0; j<25; j++) /* 0.1ms */
        //  __asm__ __volatile__("");
    }
}
#endif

void RGBLED_set_color(uint32_t color) {
#if defined(BOARD_RGBLED_CLOCK_PIN)
    write_apa_byte(0x0);
    write_apa_byte(0x0);
    write_apa_byte(0x0);
    write_apa_byte(0x0);

    write_apa_byte(0xFF);
    write_apa_byte(color >> 16);
    write_apa_byte(color >> 8);
    write_apa_byte(color);

    write_apa_byte(0xFF);
    write_apa_byte(0xFF);
    write_apa_byte(0xFF);
    write_apa_byte(0xFF);

    // set clock port low for ~10ms
    delay(50);
#elif defined(BOARD_NEOPIXEL_PIN)
    uint8_t buf[BOARD_NEOPIXEL_COUNT * 3];
#if 0
    memset(buf, 0, sizeof(buf));
    buf[0] = color >> 8;
    buf[1] = color >> 16;
    buf[2] = color;
#else
    for (int i = 0; i < BOARD_NEOPIXEL_COUNT * 3; i += 3) {
        buf[i + 0] = color >> 8;
        buf[i + 1] = color >> 16;
        buf[i + 2] = color;
    }
#endif
    neopixel_send_buffer(buf, BOARD_NEOPIXEL_COUNT * 3);
#endif
}
