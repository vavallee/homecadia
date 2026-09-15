/* Light-sleep instrumentation for the diagnostic image
 * (CONFIG_HOMECADIA_SLEEP_DIAG). Both functions are empty otherwise. */
#pragma once

#define SLEEP_DIAG_LINES 10
#define SLEEP_DIAG_COLS  50 /* 296 px / 6 px per glyph = 49 chars + NUL */

#ifdef __cplusplus
extern "C" {
#endif

void sleep_diag_init(void);

/* Fill up to SLEEP_DIAG_LINES lines; returns the count. Task context only:
 * it runs esp_timer_dump() into a heap buffer. */
int sleep_diag_format(char lines[][SLEEP_DIAG_COLS]);

#ifdef __cplusplus
}
#endif
