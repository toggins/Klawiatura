#include "K_interface.h"

void start(const char*) {
    WARN("There's no logo yet!");
    set_screen(SCR_MENU, NULL);
}

const ScreenTable TAB_LOGO = {.start = start};
