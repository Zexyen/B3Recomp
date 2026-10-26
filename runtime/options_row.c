/* [b3recomp] "PC Settings": a row of the game's own Options screen (page Screen Display) that opens the PC settings panel
 * (pcmenu.c). docs/RE_NOTES.md section 8 has the Options screen as found in main.
 *
 * The screen's 25 rows are objects behind a table of pointers (ROWS; slot 25 is 0); a page shows rows by number, Screen
 * Display the 7 of a table in main's read-only data. The lift reads that page's numbers from OPTX_BASE instead, 8 of them
 * (lifter/recomp_eh.py, _options_row_patches); here: that page, mapped after main's image, holding the 7 numbers with row 25
 * before Restore Defaults, and row 25: an object whose method table is the Brightness row's (an action row: a label, the
 * Confirm / Back button guide, no value, nothing to restore) with two methods of ours: the label (the message
 * MENU_OPTION_LIST_PC_SETTINGS, added to the game's text by scripts/pc_settings_mod.py) and "decide", which opens the panel
 * where the Brightness row opens its screen. */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "plat.h"
#include "runtime.h"
#include "hostrt.h"

#define OPTX_BASE   0x7105a00000ULL      /* lifter/recomp_eh.py OPTX_PAGE */
#define ROWS        0x7101c2b1a8ULL      /* the Options screen's row objects */
#define SCREEN_ROWS 0x71018511bcULL      /* Screen Display's 7 row numbers (18 .. 24) in main */
#define BRIGHTNESS  18
#define PC_ROW      25
#define PC_LABEL    0x1a9a646fu          /* crc32("menu_option_list_pc_settings") & 0x7fffffff, scripts/pc_settings_mod.py */
#define DECIDE_MASK 0x7100983930ULL      /* -> w0: the pad bits of "confirm" (A, or B with the buttons swapped) */
#define TRIGGERED   0x7100983660ULL      /* (w0 bits) -> w0: pressed this frame */
#define MENU_SOUND  0x710095a110ULL      /* (w0 sound): 2 confirm, 8 refused */

void pcmenu_open_game(void);             /* pcmenu.c */

static void pc_row_label(Ctx *c) { c->x[0] = PC_LABEL; }
/* method +0x08 (row, screen, row state, widget) -> 1 = the input is used; called every frame for the row under the cursor */
static void pc_row_decide(Ctx *c) {
    uint64_t state = c->x[2], lr = c->x[30];
    call_guest(c, DECIDE_MASK);
    call_guest(c, TRIGGERED);
    int on = (uint32_t)c->x[0] != 0, greyed = on && rd32(state + 0x28);
    if (on) { c->x[0] = greyed ? 8 : 2; call_guest(c, MENU_SOUND); }
    if (on && !greyed) pcmenu_open_game();
    c->x[0] = on && !greyed;
    c->x[30] = lr;
}

void options_row_install(void) {
    uint8_t *page = plat_map_fixed(OPTX_BASE, 0x1000);
    if (!page) { fprintf(stderr, "options row: cannot map 0x%llx (the Options screen reads its Screen Display rows there)\n", OPTX_BASE); exit(1); }
    uint32_t *rows = (uint32_t *)page;
    uint64_t bright = rd64(ROWS + 8 * BRIGHTNESS), vt = bright ? rd64(bright) : 0;
    int known = rd64(ROWS + 8 * PC_ROW) == 0 && vt && rd64(vt + 0x38) == GUEST_BASE + 0xa72c2c;    /* Brightness's label method */
    for (int i = 0; i < 7; i++) known &= rd32(SCREEN_ROWS + 4 * i) == (uint32_t)(BRIGHTNESS + i);
    for (int i = 0; i < 7; i++) rows[i] = (uint32_t)(BRIGHTNESS + i);
    rows[7] = BRIGHTNESS + 6;                /* not known: Restore Defaults twice, rather than a row that is not there */
    if (!known) { printf("options row: the Options screen's rows are not the ones known: no PC Settings row\n"); return; }
    uint64_t *mvt = (uint64_t *)(page + 0x110), *obj = (uint64_t *)(page + 0x200);   /* +0x100: the table's two header words, 0 */
    for (int i = 0; i < 9; i++) mvt[i] = rd64(vt + 8 * (uint64_t)i);
    mvt[1] = hostfn_address("options row: PC Settings, decide", pc_row_decide);
    mvt[7] = hostfn_address("options row: PC Settings, label", pc_row_label);
    obj[0] = (uint64_t)(uintptr_t)mvt;
    rows[6] = PC_ROW;
    wr64(ROWS + 8 * PC_ROW, (uint64_t)(uintptr_t)obj);
}
