#include "security/ragesecengine.h"
#ifndef SECURITY_DEBUGGERCHECKPLUGIN_H
#define SECURITY_DEBUGGERCHECKPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK

#if RSG_BANK

void DebuggerCheckPlugin_Test();

#endif //RSG_BANK

bool DebuggerCheckPlugin_Init();
bool DebuggerCheckPlugin_Work();

#endif // RAGE_SEC_TASK_BASIC_DEBUGGER_CHECK

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H