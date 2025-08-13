#ifndef USE_TUNER_H_GUARD
#define USE_TUNER_H_GUARD	
 
#if	__PPU && (__DEV || __PROFILE)
#include "sn\libsntuner.h"
#define  SN_START_MARKER(a,b)	snStartMarker((a),(b))
#define  SN_STOP_MARKER(a)	snStopMarker((a))
#else
#define  SN_START_MARKER(a,b)	
#define  SN_STOP_MARKER(a)
#endif

#endif //USE_TUNER_H_GUARD
