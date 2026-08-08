/* Encoder-driven UI: view cycling, settings menu, idle timeout.
 *
 * Modes: NAV (rotate cycles views 1→2→3), MENU (in settings view after a
 * push: rotate moves the selection), EDIT (push on an item: rotate changes
 * the value, push confirms + saves). DISPLAY_IDLE_TIMEOUT_S of no input
 * returns to view 1; the panel already deep-sleeps after every refresh, so
 * "idle" just means no further refreshes. */
#pragma once

#include "esp_err.h"

esp_err_t ui_init(void);
