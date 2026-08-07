/**
 * @file pyscriptrun.h
 * @brief PyScriptRun - a script runner app for Flipper Zero.
 *
 * Reads .psr script files from the SD card and interprets commands such as
 * subinit, nfcinit, irinit, rfidinit, gpioinit, print, delay, led, vibro,
 * gpioset, and more.
 */

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/popup.h>
#include <storage/storage.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** View indices used with the ViewDispatcher */
typedef enum {
    PyScriptViewMain,
    PyScriptViewFileBrowser,
    PyScriptViewOutput,
    PyScriptViewPopup,
} PyScriptView;

/** Main-menu item indices */
typedef enum {
    PyScriptMenuRun,
    PyScriptMenuAbout,
} PyScriptMenuItem;

/** Application state */
typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    FileBrowser* file_browser;
    TextBox* text_box;
    Popup* popup;

    FuriString* script_path;   /**< Path selected in the file browser */
    FuriString* output_buf;    /**< Accumulated script output */
} PyScriptApp;

/** Application entry point */
int32_t pyscriptrun_app(void* p);

#ifdef __cplusplus
}
#endif
