#include "security/ragesecengine.h"
#ifndef SECURITY_REVOLVINGCHECKERPLUGIN_H
#define SECURITY_REVOLVINGCHECKERPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_REVOLVING_CHECKER

bool RevolvingCheckerPlugin_Create();
bool RevolvingCheckerPlugin_Configure();
bool RevolvingCheckerPlugin_Init();
bool RevolvingCheckerPlugin_Work();

#endif // RAGE_SEC_TASK_REVOLVING_CHECKER

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H