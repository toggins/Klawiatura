#include "K_interface.h"

void start() {
    WARN("There's no logo yet!");
    set_screen(SCR_MENU, NULL);
}

const ScreenTable TAB_LOGO = {.start = start};
