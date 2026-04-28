/**
 * @file pyscriptrun.c
 * @brief PyScriptRun – main application entry-point and UI.
 *
 * UI flow:
 *   Main menu  -->  File browser  -->  Output (TextBox)
 *              -->  About (Popup)
 *
 * Press Back at any view to return to the previous one.
 * Scripts are plain-text .psr files stored on the SD card under
 *   /ext/apps_data/pyscriptrun/scripts/
 * but any location on the SD card is also browsable.
 */

#include "pyscriptrun.h"
#include "script_engine.h"

#define TAG "PyScriptRun"

/* Script files live here on the SD card. */
#define PYSCRIPTRUN_SCRIPTS_DIR APP_DATA_PATH("scripts")
/* File extension for PyScriptRun scripts */
#define PYSCRIPTRUN_EXTENSION ".psr"

/* ------------------------------------------------------------------ */
/*  Navigation callbacks                                                */
/* ------------------------------------------------------------------ */

static bool pyscriptrun_back_event_callback(void* context) {
    furi_assert(context);
    PyScriptApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Main-menu callbacks                                                 */
/* ------------------------------------------------------------------ */

static void pyscriptrun_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    PyScriptApp* app = context;

    if(index == PyScriptMenuRun) {
        /* Switch to the file browser */
        view_dispatcher_switch_to_view(app->view_dispatcher, PyScriptViewFileBrowser);
    } else if(index == PyScriptMenuAbout) {
        popup_set_header(app->popup, "PyScriptRun", 64, 3, AlignCenter, AlignTop);
        popup_set_text(
            app->popup,
            "Run .psr scripts from\n"
            "your SD card.\n"
            "Commands:\n"
            "subinit nfcinit\n"
            "irinit rfidinit\n"
            "gpioinit print\n"
            "delay led vibro\n"
            "gpioset beep",
            64,
            16,
            AlignCenter,
            AlignTop);
        popup_set_timeout(app->popup, 0);
        popup_disable_timeout(app->popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, PyScriptViewPopup);
    }
}

/* ------------------------------------------------------------------ */
/*  File-browser callback – called when user selects a file            */
/* ------------------------------------------------------------------ */

static void pyscriptrun_file_selected_callback(void* context) {
    furi_assert(context);
    PyScriptApp* app = context;

    const char* path = furi_string_get_cstr(app->script_path);
    FURI_LOG_I(TAG, "Running script: %s", path);

    /* Clear previous output */
    furi_string_reset(app->output_buf);
    furi_string_cat(app->output_buf, "Script: ");
    /* Show just the filename (last '/' component) */
    const char* slash = strrchr(path, '/');
    furi_string_cat(app->output_buf, slash ? slash + 1 : path);
    furi_string_cat(app->output_buf, "\n\n");

    /* Run the script – output is appended to output_buf */
    script_engine_run(path, app->output_buf);

    furi_string_cat(app->output_buf, "\n--- done ---\n");

    /* Display output */
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->output_buf));

    view_dispatcher_switch_to_view(app->view_dispatcher, PyScriptViewOutput);
}

/* ------------------------------------------------------------------ */
/*  View "previous" callbacks (Back button from sub-views)             */
/* ------------------------------------------------------------------ */

static uint32_t pyscriptrun_return_to_main(void* context) {
    UNUSED(context);
    return PyScriptViewMain;
}

static uint32_t pyscriptrun_return_to_browser(void* context) {
    UNUSED(context);
    return PyScriptViewFileBrowser;
}

/* ------------------------------------------------------------------ */
/*  Alloc / Free                                                        */
/* ------------------------------------------------------------------ */

static PyScriptApp* pyscriptrun_alloc(void) {
    PyScriptApp* app = malloc(sizeof(PyScriptApp));

    app->script_path = furi_string_alloc();
    app->output_buf = furi_string_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, pyscriptrun_back_event_callback);

    /* --- Main menu (Submenu) --- */
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "PyScriptRun");
    submenu_add_item(
        app->submenu, "Run Script", PyScriptMenuRun, pyscriptrun_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", PyScriptMenuAbout, pyscriptrun_submenu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, PyScriptViewMain, submenu_get_view(app->submenu));

    /* --- File browser --- */
    app->file_browser = file_browser_alloc(app->script_path);
    file_browser_configure(
        app->file_browser,
        PYSCRIPTRUN_EXTENSION,
        PYSCRIPTRUN_SCRIPTS_DIR,
        true,
        true,
        NULL,
        false);
    file_browser_set_callback(app->file_browser, pyscriptrun_file_selected_callback, app);
    view_set_previous_callback(
        file_browser_get_view(app->file_browser), pyscriptrun_return_to_main);
    view_dispatcher_add_view(
        app->view_dispatcher, PyScriptViewFileBrowser, file_browser_get_view(app->file_browser));

    /* --- Output (TextBox) --- */
    app->text_box = text_box_alloc();
    view_set_previous_callback(
        text_box_get_view(app->text_box), pyscriptrun_return_to_browser);
    view_dispatcher_add_view(
        app->view_dispatcher, PyScriptViewOutput, text_box_get_view(app->text_box));

    /* --- About (Popup) --- */
    app->popup = popup_alloc();
    view_set_previous_callback(popup_get_view(app->popup), pyscriptrun_return_to_main);
    view_dispatcher_add_view(
        app->view_dispatcher, PyScriptViewPopup, popup_get_view(app->popup));

    return app;
}

static void pyscriptrun_free(PyScriptApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, PyScriptViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, PyScriptViewFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, PyScriptViewOutput);
    view_dispatcher_remove_view(app->view_dispatcher, PyScriptViewPopup);

    submenu_free(app->submenu);
    file_browser_free(app->file_browser);
    text_box_free(app->text_box);
    popup_free(app->popup);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    furi_string_free(app->script_path);
    furi_string_free(app->output_buf);

    free(app);
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                         */
/* ------------------------------------------------------------------ */

int32_t pyscriptrun_app(void* p) {
    UNUSED(p);

    /* Make sure the scripts directory exists on the SD card. */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PYSCRIPTRUN_SCRIPTS_DIR);
    furi_record_close(RECORD_STORAGE);

    PyScriptApp* app = pyscriptrun_alloc();

    /* Start file browser from the scripts directory */
    furi_string_set(app->script_path, PYSCRIPTRUN_SCRIPTS_DIR);
    file_browser_start(app->file_browser, app->script_path);

    /* Show the main menu */
    view_dispatcher_switch_to_view(app->view_dispatcher, PyScriptViewMain);

    /* Blocking run until back is pressed on the main menu */
    view_dispatcher_run(app->view_dispatcher);

    file_browser_stop(app->file_browser);

    pyscriptrun_free(app);

    return 0;
}
