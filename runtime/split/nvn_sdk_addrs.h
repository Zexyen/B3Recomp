/* runtime/split: nvn_sdk_addrs.h for the runtime built apart from the player's tables (README.md). The sdk offsets of
 * runtime/gen/nvn_sdk_addrs.h are data in game.so (runtime/wt_tables.c), read at run time. */
#ifndef NVN_SDK_ADDRS_H
#define NVN_SDK_ADDRS_H
extern const unsigned long long wt_sdka_chip_descriptor;
extern const unsigned long long wt_sdka_format_count;
extern const unsigned long long wt_sdka_format_table;
extern const unsigned long long wt_sdka_nvnTextureBuilderGetRawStorageClass;
extern const unsigned long long wt_sdka_nvnTextureBuilderGetSparseTileLayout;
extern const unsigned long long wt_sdka_nvnTextureBuilderGetStorageAlignment;
extern const unsigned long long wt_sdka_nvnTextureBuilderGetStorageClass;
extern const unsigned long long wt_sdka_nvnTextureBuilderGetStorageSize;
extern const unsigned long long wt_sdka_nvnTextureGetRawStorageClass;
extern const unsigned long long wt_sdka_nvnTextureGetSparseTileLayout;
extern const unsigned long long wt_sdka_nvnTextureGetStorageClass;
extern const unsigned long long wt_sdka_nvnTextureGetStorageSize;
extern const unsigned long long wt_sdka_texture_layout;
extern const unsigned long long wt_sdka_vt_hal_gm200;
extern const unsigned long long wt_sdka_vt_nvgpu_device;
#define SDKA_chip_descriptor                      wt_sdka_chip_descriptor
#define SDKA_format_count                         wt_sdka_format_count
#define SDKA_format_table                         wt_sdka_format_table
#define SDKA_nvnTextureBuilderGetRawStorageClass  wt_sdka_nvnTextureBuilderGetRawStorageClass
#define SDKA_nvnTextureBuilderGetSparseTileLayout wt_sdka_nvnTextureBuilderGetSparseTileLayout
#define SDKA_nvnTextureBuilderGetStorageAlignment wt_sdka_nvnTextureBuilderGetStorageAlignment
#define SDKA_nvnTextureBuilderGetStorageClass     wt_sdka_nvnTextureBuilderGetStorageClass
#define SDKA_nvnTextureBuilderGetStorageSize      wt_sdka_nvnTextureBuilderGetStorageSize
#define SDKA_nvnTextureGetRawStorageClass         wt_sdka_nvnTextureGetRawStorageClass
#define SDKA_nvnTextureGetSparseTileLayout        wt_sdka_nvnTextureGetSparseTileLayout
#define SDKA_nvnTextureGetStorageClass            wt_sdka_nvnTextureGetStorageClass
#define SDKA_nvnTextureGetStorageSize             wt_sdka_nvnTextureGetStorageSize
#define SDKA_texture_layout                       wt_sdka_texture_layout
#define SDKA_vt_hal_gm200                         wt_sdka_vt_hal_gm200
#define SDKA_vt_nvgpu_device                      wt_sdka_vt_nvgpu_device
#endif
