#include "security/ragesecengine.h"
#ifndef SECURITY_TUNABLESVERIFIERPLUGIN_H
#define SECURITY_TUNABLESVERIFIERPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_TUNABLES_VERIFIER
void TunablesVerifierPlugin_SetHash(unsigned char *in);
bool TunablesVerifierPlugin_Create();
bool TunablesVerifierPlugin_Configure();
bool TunablesVerifierPlugin_Init();
bool TunablesVerifierPlugin_Work();
#endif // RAGE_SEC_TASK_TUNABLES_VERIFIER

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H