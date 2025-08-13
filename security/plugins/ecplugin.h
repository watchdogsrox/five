#include "security/ragesecengine.h"
#ifndef SECURITY_ECPLUGIN_H
#define SECURITY_ECPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_EC
bool EcPlugin_Init();
bool EcPlugin_Work();

void EcPlugin_GetMemoryCheck(int index, MemoryCheck& check);
u32 EcPlugin_GetNumMemoryChecks();

#if __BANK
bool EcPlugin_InsertMemoryCheck(u32 versionNumber, u8 sku, const int pageLow, const int pageHigh, const int actionFlags, const char* inputBytes);
#endif //__BANK

#endif // RAGE_SEC_TASK_EC

#endif  // USE_RAGESEC

#endif  // SECURITY_ECPLUGIN_H