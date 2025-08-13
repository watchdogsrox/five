// Title	:	Globals.cpp
// Author	:	Obbe Vermeij
// Started	:	15/6/00

// game headers
#include "debugglobals.h"

#if !__FINAL
bool	gbBigWhiteDebugLightSwitchedOn = false;
bool	gbShowCarRoadGroups = false;
bool	gbShowPedRoadGroups = false; 
bool	gbDebugStuffInRelease = false; 
#endif

#if !__FINAL
bool	gbShowCollisionPolysLighting = false;
bool	gbShowCollisionPolysReflections = false;
bool	gbShowCollisionPolysNoShadows = false; 
bool	gbShowCollisionPolys = false;
bool	gbShowCollisionPolysShootThrough = false;
bool	gbShowCollisionPolysSeeThrough = false;
bool	gbShowCollisionPolysStairs = false;
bool	gbShowCollisionPolysDrawLast = false;
bool	gbShowCollisionPolysPavement = false;
s32 	gShowSurfaceType = 0;
#endif
 
#if __PPU
#elif __XENON
#else
#endif 

#if !__FINAL
bool gbStopVehiclesDamaging = false;
bool gbDontDisplayDirtOnVehicles = false;
bool gbAllowPedGeneration		 = true;
bool gbAllowAmbientPedGeneration = true;
bool gbAllowVehGenerationOrRemoval = true;
bool gbAllowCopGenerationOrRemoval = true;
bool gbAllowTrainGenerationOrRemoval = true;
bool gbAllowScriptBrains = true;
#endif

#if __DEV
bool gbDisplayUnmatchedBigBuildings = false;
bool gbStopVehiclesExploding = false;
bool gbRenderPetrolTankDamage = false;
#endif //!DEV


