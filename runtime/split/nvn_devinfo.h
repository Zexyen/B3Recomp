/* runtime/split: nvn_devinfo.h for the runtime built apart from the player's tables (README.md). The answers of
 * runtime/gen/nvn_devinfo.h are data in game.so (runtime/wt_tables.c), read at run time. */
extern const int wt_nvn_devinfo_count;
extern const int *const wt_nvn_devinfo_sdk;
#define NVN_DEVINFO_SDK_COUNT wt_nvn_devinfo_count
#define nvn_devinfo_sdk wt_nvn_devinfo_sdk
