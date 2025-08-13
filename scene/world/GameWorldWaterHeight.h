// ==================================
// scene/world/GameWorldWaterHeight.h
// (c) 2012 RockstarNorth
// ==================================

#ifndef _SCENE_WORLD_GAMEWORLDWATERHEIGHT_H_
#define _SCENE_WORLD_GAMEWORLDWATERHEIGHT_H_

#include "vectormath/vec3v.h"
#include "vectormath/vec4v.h"

namespace rage { class bkBank; }

class CGameWorldWaterHeight
{
public:
	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

#if __BANK
	static void AddWidgets(bkBank* pBank);
	static void DebugDraw(Vec3V_In camPos);
#endif // __BANK

	static bool IsEnabled(); // system can be turned on or off (not just in rag, potentially) .. not sure if this is useful or not, we'll see
	static void Update();
	static float GetWaterHeight();
	static float GetWaterHeightAtPos(Vec3V_In pos);
	static Vec4V_Out GetClosestPoolSphere(); // returns closest pool sphere, or 0 if closest object is not a pool
	static float GetDistanceToClosestRiver(); // returns distance from camera to closest river, or FLT_MAX if not close to a river
	static float GetDistanceToClosestRiver(Vec3V_In pos);
	static float GetDistanceToClosestWater(Vec3V_In pos);
};

#endif // _SCENE_WORLD_GAMEWORLDWATERHEIGHT_H_
