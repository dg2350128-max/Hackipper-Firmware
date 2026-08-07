/**
 * @file script_engine.h
 * @brief PyScriptRun script interpreter.
 *
 * Supported commands (one per line, # starts a comment):
 *
 *   subinit              - initialise Sub-GHz HAL
 *   nfcinit              - initialise NFC HAL
 *   irinit               - initialise Infrared RX
 *   rfidinit             - initialise RFID HAL
 *   gpioinit             - initialise user-facing GPIO pins as outputs
 *   print <text>         - append <text> to the output buffer
 *   delay <ms>           - sleep for <ms> milliseconds
 *   led <r|g|b|off>      - set the RGB LED colour
 *   vibro <0|1>          - enable / disable the vibration motor
 *   gpioset <pin> <0|1>  - set external GPIO pin high or low
 *                          pin: 1=PC0, 2=PC1, 3=PC3, 4=PB2, 5=PB3,
 *                               6=PA4, 7=PA6, 8=PA7
 *   beep <freq> <ms>     - play a tone (frequency Hz, duration ms)
 */

#pragma once

#include <furi.h>
#include <furi_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Execute a script file and write human-readable output into @p output.
 *
 * @param path    Full path to the .psr script on the SD card.
 * @param output  FuriString that receives execution log lines.
 */
void script_engine_run(const char* path, FuriString* output);

#ifdef __cplusplus
}
#endif
