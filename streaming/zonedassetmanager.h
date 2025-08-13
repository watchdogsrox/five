//
// streaming/zonedassetmanager.h
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef __STREAMING_ZONEDASSETMANAGER_H__
#define __STREAMING_ZONEDASSETMANAGER_H__

#include "atl/array.h"
#include "atl/vector.h"
#include "atl/map.h"
#include "atl/hashstring.h"
#include "vectormath/vec3v.h"
#include "bank/bank.h"

#include "streaming/streamingrequest.h"

// forward declarations
class CZonedAssets;

namespace rage {

class CZonedAssetManager
{
public:
	static void Init();
	static void Shutdown(u32 shutdownMode);
	static void Update(Vec3V_In focus);

	static void AddRequest(atHashString modelName);
	static void ClearRequests();
	static void Reset();

	static void SetEnabled(bool enabled);

	static void EnumAndProcessFolder(char const* pFolder, void(*process)(char const*));
	static void LoadFile(char const* filename);
	static void UnloadFile(char const* filename);

private:
	static Vec3V							sm_LastFocus;
	static atMap<atHashString, strRequest>	sm_Requests; 
	static atVector<CZonedAssets*>			sm_Zones;

#if __BANK
public:
	static void InitWidgets(bkBank& bank);
	static void ShowZonedAssets();
	static void ShowZoneDetails();
	static void GetZonedAssets(atArray<strIndex>& assets);
	static u32 GetZoneCost(s32 index);

private:
	static void DrawZoneBoxes();

#endif
};

} // namespace rage 

#endif //__STREAMING_ZONEDASSETMANAGER_H__
