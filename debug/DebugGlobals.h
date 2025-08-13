// Title	:	Globals.h
// Author	:	Obbe Vermeij
// Started	:	15/6/00

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#if !__FINAL
extern bool	gbBigWhiteDebugLightSwitchedOn;
extern bool	gbShowPedRoadGroups;
extern bool	gbShowCarRoadGroups;
extern bool	gbDebugStuffInRelease;
#endif //!__FINAL

#if !__FINAL
extern bool gbShowCollisionPolys;
extern bool gbShowCollisionPolysReflections;
extern bool gbShowCollisionPolysNoShadows;
extern bool gbShowCollisionPolysLighting;
extern bool gbShowCollisionPolysShootThrough;
extern bool gbShowCollisionPolysSeeThrough;
extern bool gbShowCollisionPolysStairs;
extern bool gbShowCollisionPolysDrawLast;
extern bool gbShowCollisionPolysPavement;
extern s32 gShowSurfaceType;
#endif //!__FINAL

extern bool gbDontRenderBuildingsInShadow;

#if !__FINAL
extern bool gbStopVehiclesDamaging;
extern bool gbDontDisplayDirtOnVehicles;
extern bool gbAllowPedGeneration;
extern bool gbAllowAmbientPedGeneration;
extern bool gbAllowVehGenerationOrRemoval;
extern bool gbAllowCopGenerationOrRemoval;
extern bool gbAllowTrainGenerationOrRemoval;
extern bool gbAllowScriptBrains;
#endif

#if __DEV
extern bool gbDisplayPedsAndCarsNumbers;
extern bool gbDisplayUnmatchedBigBuildings;
extern bool gbStopVehiclesExploding;
extern bool gbRenderPetrolTankDamage;
#endif //__DEV

#if __DEV
extern float gAaronsLODMultiplier;
#endif

#if __BANK
extern bool gbWhatAmILookingAt;
extern bool gbLookingAtMobileActorsOnly;
extern bool gbWhatAmIClosestTo;
extern bool gbDisplaySubBoundsOfDebugEntity;
#endif //__BANK

#endif

