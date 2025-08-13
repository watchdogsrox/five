/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : uiInputEnums.h
// PURPOSE : Extracted out enums from legacy code that we need in other locations,
//           but want to avoid cyclic dependencies
//
// AUTHOR  : james.strain
// STARTED : February 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_INPUT_ENUMS_H
#define UI_INPUT_ENUMS_H

// Migrated out of PauseMenu.h
enum eFRONTEND_INPUT
{
    FRONTEND_INPUT_UP = 0,
    FRONTEND_INPUT_DOWN,
    FRONTEND_INPUT_LEFT,
    FRONTEND_INPUT_RIGHT,
    FRONTEND_INPUT_RDOWN,
    FRONTEND_INPUT_RLEFT,
    FRONTEND_INPUT_RRIGHT,
    FRONTEND_INPUT_RUP,
    FRONTEND_INPUT_ACCEPT,
    FRONTEND_INPUT_X,
    FRONTEND_INPUT_Y,
    FRONTEND_INPUT_BACK,
    FRONTEND_INPUT_START,
    FRONTEND_INPUT_SPECIAL_UP,
    FRONTEND_INPUT_SPECIAL_DOWN,
    FRONTEND_INPUT_RSTICK_LEFT,
    FRONTEND_INPUT_RSTICK_RIGHT,
    FRONTEND_INPUT_LT,
    FRONTEND_INPUT_RT,
    FRONTEND_INPUT_LB,
    FRONTEND_INPUT_RB,
    FRONTEND_INPUT_LT_SPECIAL,
    FRONTEND_INPUT_RT_SPECIAL,
    FRONTEND_INPUT_SELECT,
    FRONTEND_INPUT_R3,

    // Used for pointing devices (mouse button, touch pad etc).
    FRONTEND_INPUT_CURSOR_ACCEPT,
#if RSG_PC
    FRONTEND_INPUT_CURSOR_BACK,
#endif

    FRONTEND_INPUT_L3,

    FRONTEND_INPUT_MAX
};

#endif // UI_INPUT_ENUMS_H