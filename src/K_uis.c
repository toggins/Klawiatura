#include "K_interface.h"

#define DUI(ident)                                                                                                     \
    extern const UITable TAB_##ident;                                                                                  \
    UIS[UI_##ident] = &TAB_##ident;

void POPULATE_UIS_TABLE() {
    extern const UITable* UIS[UI_SIZE];

    DUI(OPTIONS);

    static const UITable TAB_NULL = {NULL};
    UIS[UI_NULL] = &TAB_NULL;
}
