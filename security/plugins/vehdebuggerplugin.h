#include "security/ragesecengine.h"
#ifndef SECURITY_VEHDEBUGGERPLUGIN_H
#define SECURITY_VEHDEBUGGERPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_VEH_DEBUGGER

bool VehDebuggerPlugin_Create();
bool VehDebuggerPlugin_Configure();
bool VehDebuggerPlugin_Init();
bool VehDebuggerPlugin_Work();

#endif // RAGE_SEC_TASK_VEH_DEBUGGER

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H