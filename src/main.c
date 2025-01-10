#define WADDLE_IMPLEMENTATION
#include "waddle.h"

i32 main(void) {
    wdl_init();

    WDL_INFO("Hello, world!");

    wdl_terminate();
    return 0;
}
