/**
 * @file script_engine.c
 * @brief PyScriptRun – line-by-line script interpreter.
 */

#include "script_engine.h"

#include <furi_hal_gpio.h>
#include <furi_hal_light.h>
#include <furi_hal_vibro.h>
#include <furi_hal_speaker.h>
#include <furi_hal_subghz.h>
#include <furi_hal_infrared.h>
#include <furi_hal_rfid.h>
#include <furi_hal_nfc.h>
#include <furi_hal_resources.h>
#include <storage/storage.h>

#include <string.h>
#include <stdlib.h>

#define TAG              "PyScriptRun"
#define MAX_LINE_LEN     256
#define MAX_OUTPUT_LEN   4096

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

/** Skip leading whitespace. */
static const char* skip_ws(const char* s) {
    while(*s == ' ' || *s == '\t') s++;
    return s;
}

/** Trim trailing CR/LF from a mutable string. */
static void trim_nl(char* s) {
    size_t len = strlen(s);
    while(len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

/** Map pin-number (1..8) to an external GPIO pin record index, or -1. */
static const GpioPin* get_ext_pin(int num) {
    switch(num) {
    case 1: return &gpio_ext_pc0;
    case 2: return &gpio_ext_pc1;
    case 3: return &gpio_ext_pc3;
    case 4: return &gpio_ext_pb2;
    case 5: return &gpio_ext_pb3;
    case 6: return &gpio_ext_pa4;
    case 7: return &gpio_ext_pa6;
    case 8: return &gpio_ext_pa7;
    default: return NULL;
    }
}

/* ------------------------------------------------------------------ */
/*  Command handlers                                                    */
/* ------------------------------------------------------------------ */

static void cmd_subinit(FuriString* out) {
    furi_hal_subghz_init();
    furi_string_cat(out, "[subinit] Sub-GHz HAL initialised\n");
}

static void cmd_nfcinit(FuriString* out) {
    furi_hal_nfc_acquire();
    furi_hal_nfc_low_power_mode_start();
    furi_string_cat(out, "[nfcinit] NFC HAL initialised\n");
}

static void cmd_irinit(FuriString* out) {
    furi_hal_infrared_async_rx_start();
    furi_hal_infrared_async_rx_set_timeout(10000);
    furi_string_cat(out, "[irinit] Infrared RX initialised\n");
}

static void cmd_rfidinit(FuriString* out) {
    furi_hal_rfid_init();
    furi_string_cat(out, "[rfidinit] RFID HAL initialised\n");
}

static void cmd_gpioinit(FuriString* out) {
    /* Configure all 8 external pins as push-pull outputs, low. */
    for(int i = 1; i <= 8; i++) {
        const GpioPin* pin = get_ext_pin(i);
        if(pin) {
            furi_hal_gpio_init(pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
            furi_hal_gpio_write(pin, false);
        }
    }
    furi_string_cat(out, "[gpioinit] GPIO pins 1-8 configured as outputs (low)\n");
}

static void cmd_print(const char* args, FuriString* out) {
    const char* text = skip_ws(args);
    furi_string_cat(out, text);
    furi_string_cat(out, "\n");
}

static void cmd_delay(const char* args, FuriString* out) {
    int ms = atoi(skip_ws(args));
    if(ms <= 0) {
        furi_string_cat(out, "[delay] invalid argument\n");
        return;
    }
    if(ms > 30000) ms = 30000; /* safety cap: 30 s */
    furi_delay_ms((uint32_t)ms);
    char buf[48];
    snprintf(buf, sizeof(buf), "[delay] slept %d ms\n", ms);
    furi_string_cat(out, buf);
}

static void cmd_led(const char* args, FuriString* out) {
    const char* color = skip_ws(args);
    if(strncmp(color, "off", 3) == 0 && (color[3] == '\0' || color[3] == ' ' || color[3] == '\t')) {
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 0);
        furi_hal_light_set(LightBlue, 0);
        furi_string_cat(out, "[led] off\n");
    } else if(strncmp(color, "red", 3) == 0 && (color[3] == '\0' || color[3] == ' ' || color[3] == '\t')) {
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightGreen, 0);
        furi_hal_light_set(LightBlue, 0);
        furi_string_cat(out, "[led] red\n");
    } else if(strncmp(color, "green", 5) == 0 && (color[5] == '\0' || color[5] == ' ' || color[5] == '\t')) {
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 255);
        furi_hal_light_set(LightBlue, 0);
        furi_string_cat(out, "[led] green\n");
    } else if(strncmp(color, "blue", 4) == 0 && (color[4] == '\0' || color[4] == ' ' || color[4] == '\t')) {
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 0);
        furi_hal_light_set(LightBlue, 255);
        furi_string_cat(out, "[led] blue\n");
    } else {
        furi_string_cat(out, "[led] unknown colour (use red/green/blue/off)\n");
    }
}

static void cmd_vibro(const char* args, FuriString* out) {
    int val = atoi(skip_ws(args));
    furi_hal_vibro_on(val != 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "[vibro] %s\n", val ? "on" : "off");
    furi_string_cat(out, buf);
}

static void cmd_gpioset(const char* args, FuriString* out) {
    int pin = 0, level = 0;
    const char* p = skip_ws(args);
    pin = atoi(p);
    /* advance past the first integer */
    while(*p && *p != ' ' && *p != '\t') p++;
    level = atoi(skip_ws(p));

    const GpioPin* gpio = get_ext_pin(pin);
    if(!gpio) {
        furi_string_cat(out, "[gpioset] invalid pin (1-8)\n");
        return;
    }
    furi_hal_gpio_write(gpio, level != 0);
    char buf[48];
    snprintf(buf, sizeof(buf), "[gpioset] pin %d = %d\n", pin, level ? 1 : 0);
    furi_string_cat(out, buf);
}

static void cmd_beep(const char* args, FuriString* out) {
    int freq = 0, ms = 0;
    const char* p = skip_ws(args);
    freq = atoi(p);
    while(*p && *p != ' ' && *p != '\t') p++;
    ms = atoi(skip_ws(p));

    if(freq <= 0 || ms <= 0) {
        furi_string_cat(out, "[beep] usage: beep <freq_hz> <duration_ms>\n");
        return;
    }
    if(ms > 5000) ms = 5000;

    if(furi_hal_speaker_acquire(500)) {
        furi_hal_speaker_start((float)freq, 0.5f);
        furi_delay_ms((uint32_t)ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "[beep] %d Hz for %d ms\n", freq, ms);
    furi_string_cat(out, buf);
}

/* ------------------------------------------------------------------ */
/*  Single-line dispatcher (shared by the main loop and last-line)      */
/* ------------------------------------------------------------------ */

static void dispatch_line(const char* raw_line, uint32_t line_num, FuriString* output) {
    char line_buf[MAX_LINE_LEN];
    strncpy(line_buf, raw_line, MAX_LINE_LEN - 1);
    line_buf[MAX_LINE_LEN - 1] = '\0';
    trim_nl(line_buf);

    const char* cmd = skip_ws(line_buf);

    if(*cmd == '\0' || *cmd == '#') return;

    if(furi_string_size(output) >= MAX_OUTPUT_LEN) {
        furi_string_cat(output, "[truncated]\n");
        return;
    }

    if(strncmp(cmd, "subinit", 7) == 0 && (cmd[7] == '\0' || cmd[7] == ' ')) {
        cmd_subinit(output);
    } else if(strncmp(cmd, "nfcinit", 7) == 0 && (cmd[7] == '\0' || cmd[7] == ' ')) {
        cmd_nfcinit(output);
    } else if(strncmp(cmd, "irinit", 6) == 0 && (cmd[6] == '\0' || cmd[6] == ' ')) {
        cmd_irinit(output);
    } else if(strncmp(cmd, "rfidinit", 8) == 0 && (cmd[8] == '\0' || cmd[8] == ' ')) {
        cmd_rfidinit(output);
    } else if(strncmp(cmd, "gpioinit", 8) == 0 && (cmd[8] == '\0' || cmd[8] == ' ')) {
        cmd_gpioinit(output);
    } else if(strncmp(cmd, "print", 5) == 0 && (cmd[5] == '\0' || cmd[5] == ' ')) {
        cmd_print(cmd + 5, output);
    } else if(strncmp(cmd, "delay", 5) == 0 && (cmd[5] == '\0' || cmd[5] == ' ')) {
        cmd_delay(cmd + 5, output);
    } else if(strncmp(cmd, "led", 3) == 0 && (cmd[3] == '\0' || cmd[3] == ' ')) {
        cmd_led(cmd + 3, output);
    } else if(strncmp(cmd, "vibro", 5) == 0 && (cmd[5] == '\0' || cmd[5] == ' ')) {
        cmd_vibro(cmd + 5, output);
    } else if(strncmp(cmd, "gpioset", 7) == 0 && (cmd[7] == '\0' || cmd[7] == ' ')) {
        cmd_gpioset(cmd + 7, output);
    } else if(strncmp(cmd, "beep", 4) == 0 && (cmd[4] == '\0' || cmd[4] == ' ')) {
        cmd_beep(cmd + 4, output);
    } else {
        char buf[MAX_LINE_LEN + 32];
        snprintf(
            buf,
            sizeof(buf),
            "[line %lu] unknown command: %s\n",
            (unsigned long)line_num,
            cmd);
        furi_string_cat(output, buf);
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

void script_engine_run(const char* path, FuriString* output) {
    furi_assert(path);
    furi_assert(output);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        furi_string_cat(output, "ERROR: cannot open script file\n");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    char line[MAX_LINE_LEN];
    size_t line_len = 0;
    char ch = 0;
    uint32_t line_num = 0;
    bool truncated = false;

    while(!truncated && storage_file_read(file, &ch, 1) == 1) {
        if(ch == '\n' || ch == '\r') {
            line[line_len] = '\0';
            line_num++;
            if(line_len > 0) {
                dispatch_line(line, line_num, output);
                if(furi_string_size(output) >= MAX_OUTPUT_LEN) truncated = true;
            }
            line_len = 0;
        } else {
            if(line_len < MAX_LINE_LEN - 1) {
                line[line_len++] = ch;
            }
        }
    }

    /* Handle last line if file has no trailing newline */
    if(!truncated && line_len > 0) {
        line[line_len] = '\0';
        dispatch_line(line, line_num + 1, output);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
