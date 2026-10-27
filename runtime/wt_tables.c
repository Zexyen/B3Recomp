/* The player's own tables as data, compiled into game.so by `wtbuild link` (runtime/split/README.md): the values of
 * runtime/gen/image_layout.h, nvn_devinfo.h and nvn_sdk_addrs.h, which the runtime is built without. Compiled with
 * -I runtime/gen, never -I runtime/split, so these includes are the generated headers. */
#include "image_layout.h"
#include "nvn_devinfo.h"
#include "nvn_sdk_addrs.h"

const unsigned long long wt_layout_FILE_IMAGE = FILE_IMAGE;
const unsigned long long wt_layout_IMAGE_SIZE = IMAGE_SIZE;
const unsigned long long wt_layout_GUEST_RO_END = GUEST_RO_END;
const unsigned long long wt_layout_RELA = RELA;
const unsigned long long wt_layout_RELASZ = RELASZ;
const unsigned long long wt_layout_JMPREL = JMPREL;
const unsigned long long wt_layout_PLTRELSZ = PLTRELSZ;
const unsigned long long wt_layout_MAIN_SYMTAB = MAIN_SYMTAB;
const unsigned long long wt_layout_MAIN_STRTAB = MAIN_STRTAB;
const unsigned long long wt_layout_INIT_ARRAY_OFF = INIT_ARRAY_OFF;
const unsigned long long wt_layout_INIT_ARRAY_SIZE = INIT_ARRAY_SIZE;
const unsigned long long wt_layout_DT_INIT_OFF = DT_INIT_OFF;
const unsigned long long wt_layout_SDK_IMAGE_SPAN = SDK_IMAGE_SPAN;
const unsigned long long wt_layout_NNINIT_STARTUP_OFF = NNINIT_STARTUP_OFF;
const unsigned long long wt_layout_NN_MAIN_OFF = NN_MAIN_OFF;
const unsigned long long wt_layout_GAME_ALIGNED_ALLOC_OFF = GAME_ALIGNED_ALLOC_OFF;
const unsigned long long wt_layout_GAME_FREE_OFF = GAME_FREE_OFF;
const unsigned long long wt_layout_GAME_OPDELETE_OFF = GAME_OPDELETE_OFF;

const unsigned long long wt_sdka_chip_descriptor = SDKA_chip_descriptor;
const unsigned long long wt_sdka_format_count = SDKA_format_count;
const unsigned long long wt_sdka_format_table = SDKA_format_table;
const unsigned long long wt_sdka_nvnTextureBuilderGetRawStorageClass = SDKA_nvnTextureBuilderGetRawStorageClass;
const unsigned long long wt_sdka_nvnTextureBuilderGetSparseTileLayout = SDKA_nvnTextureBuilderGetSparseTileLayout;
const unsigned long long wt_sdka_nvnTextureBuilderGetStorageAlignment = SDKA_nvnTextureBuilderGetStorageAlignment;
const unsigned long long wt_sdka_nvnTextureBuilderGetStorageClass = SDKA_nvnTextureBuilderGetStorageClass;
const unsigned long long wt_sdka_nvnTextureBuilderGetStorageSize = SDKA_nvnTextureBuilderGetStorageSize;
const unsigned long long wt_sdka_nvnTextureGetRawStorageClass = SDKA_nvnTextureGetRawStorageClass;
const unsigned long long wt_sdka_nvnTextureGetSparseTileLayout = SDKA_nvnTextureGetSparseTileLayout;
const unsigned long long wt_sdka_nvnTextureGetStorageClass = SDKA_nvnTextureGetStorageClass;
const unsigned long long wt_sdka_nvnTextureGetStorageSize = SDKA_nvnTextureGetStorageSize;
const unsigned long long wt_sdka_texture_layout = SDKA_texture_layout;
const unsigned long long wt_sdka_vt_hal_gm200 = SDKA_vt_hal_gm200;
const unsigned long long wt_sdka_vt_nvgpu_device = SDKA_vt_nvgpu_device;

const int wt_nvn_devinfo_count = NVN_DEVINFO_SDK_COUNT;
const int *const wt_nvn_devinfo_sdk = nvn_devinfo_sdk;
