/* runtime/split: image_layout.h for the runtime built apart from the player's tables (README.md). The values of
 * runtime/gen/image_layout.h are data in game.so (runtime/wt_tables.c), read at run time. */
#ifndef IMAGE_LAYOUT_H
#define IMAGE_LAYOUT_H
extern const unsigned long long wt_layout_FILE_IMAGE;
extern const unsigned long long wt_layout_IMAGE_SIZE;
extern const unsigned long long wt_layout_GUEST_RO_END;
extern const unsigned long long wt_layout_RELA;
extern const unsigned long long wt_layout_RELASZ;
extern const unsigned long long wt_layout_JMPREL;
extern const unsigned long long wt_layout_PLTRELSZ;
extern const unsigned long long wt_layout_MAIN_SYMTAB;
extern const unsigned long long wt_layout_MAIN_STRTAB;
extern const unsigned long long wt_layout_INIT_ARRAY_OFF;
extern const unsigned long long wt_layout_INIT_ARRAY_SIZE;
extern const unsigned long long wt_layout_DT_INIT_OFF;
extern const unsigned long long wt_layout_SDK_IMAGE_SPAN;
extern const unsigned long long wt_layout_NNINIT_STARTUP_OFF;
extern const unsigned long long wt_layout_NN_MAIN_OFF;
extern const unsigned long long wt_layout_GAME_ALIGNED_ALLOC_OFF;
extern const unsigned long long wt_layout_GAME_FREE_OFF;
extern const unsigned long long wt_layout_GAME_OPDELETE_OFF;
#define FILE_IMAGE             wt_layout_FILE_IMAGE
#define IMAGE_SIZE             wt_layout_IMAGE_SIZE
#define GUEST_RO_END           wt_layout_GUEST_RO_END
#define RELA                   wt_layout_RELA
#define RELASZ                 wt_layout_RELASZ
#define JMPREL                 wt_layout_JMPREL
#define PLTRELSZ               wt_layout_PLTRELSZ
#define MAIN_SYMTAB            wt_layout_MAIN_SYMTAB
#define MAIN_STRTAB            wt_layout_MAIN_STRTAB
#define INIT_ARRAY_OFF         wt_layout_INIT_ARRAY_OFF
#define INIT_ARRAY_SIZE        wt_layout_INIT_ARRAY_SIZE
#define DT_INIT_OFF            wt_layout_DT_INIT_OFF
#define SDK_IMAGE_SPAN         wt_layout_SDK_IMAGE_SPAN
#define NNINIT_STARTUP_OFF     wt_layout_NNINIT_STARTUP_OFF
#define NN_MAIN_OFF            wt_layout_NN_MAIN_OFF
#define GAME_ALIGNED_ALLOC_OFF wt_layout_GAME_ALIGNED_ALLOC_OFF
#define GAME_FREE_OFF          wt_layout_GAME_FREE_OFF
#define GAME_OPDELETE_OFF      wt_layout_GAME_OPDELETE_OFF
#endif
