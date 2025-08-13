#ifndef SCENE_PORTALS_INTERIORGROUPMANAGER_INCLUDE
#define SCENE_PORTALS_INTERIORGROUPMANAGER_INCLUDE

// framework headers

// rage headers
#include "atl/map.h"
#include "atl/array.h"

// game headers
#include "scene/portals/InteriorInst.h"

class CInteriorGroupManager
{
private:

	static atMap< u32, atArray< const CInteriorInst* > >		ms_exits;
	static atMap< const CInteriorInst*, atArray< spdAABB > >		ms_portalBounds;

#if __BANK
	static bool		ms_useGroupPolicyForPopulation;
	static bool		ms_showExitPortals;
#endif // __BANK

public:

	static void Init();
	static void Shutdown();

	static void RegisterGroupExit(const CInteriorInst* pIntInst);
	static void UnregisterGroupExit(const CInteriorInst* pIntInst);

	static bool HasGroupExits(const u32 groupId);
	static bool RayIntersectsExitPortals(const u32 groupId, const Vector3& rayStart, const Vector3& rayDirection);

#if __BANK
	static void AddWidgets(bkBank* pBank);
	static void DrawDebug();

	static bool UseGroupPolicyForPopulation()		{ return ms_useGroupPolicyForPopulation; }
#else // __BANK
	static bool UseGroupPolicyForPopulation()		{ return true; }
#endif // else __BANK
};

#endif // !defined SCENE_PORTALS_INTERIORGROUPMANAGER_INCLUDE
