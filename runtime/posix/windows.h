/* Linux build only (on the include path before the system's): the Win32 subset the runtime uses, from plat.h, and the
 * Windows features Linux does not have, failing cleanly (win32_absent.h) */
#include "plat.h"
#include "win32_absent.h"
