#include "K_interface.h"

#define SCREEN(ident)                                                                                                  \
    extern const ScreenTable TAB_##ident;                                                                              \
    SCREENS[SCR_##ident] = &TAB_##ident;

void POPULATE_SCREENS_TABLE() {
    extern const ScreenTable* SCREENS[SCR_SIZE];

    SCREEN(LOGO);
    SCREEN(MENU);
    SCREEN(MAP);
    SCREEN(GAME);

    static const ScreenTable TAB_NULL = {NULL};
    SCREENS[SCR_NULL] = &TAB_NULL;
}
