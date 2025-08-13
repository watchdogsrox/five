//
// scene/world/WorldDebugHelper.h
//
// Copyright (C) 1999-2009 Rockstar Games. All Rights Reserved.
//

#ifndef __INC_WORLDDEBUGHELPER_H_
#define __INC_WORLDDEBUGHELPER_H_

#if __BANK

// Rage headers

// Framework headers
#include "fwscene/world/worldmgr.h"

// Game headers
#include "scene/RegdRefTypes.h"

namespace rage { class bkBank; }
namespace rage { class Vector2; }
//namespace rage { typedef struct DefragPool_SGapInfo; }

class CEntity;

#if WORLDMGR_STATS
	#define WorldDebugHelper_ResetStats()   {CWorldDebugHelper::UpdateWorldSearchModuleFlags(); WorldMgr_ResetStats();}
	#define WorldDebugHelper_CommitStats()  {WorldMgr_CommitStats();}
#else
	#define WorldDebugHelper_ResetStats() {}
	#define WorldDebugHelper_CommitStats() {}
#endif


class CWorldDebugHelper
{
public:
	static void AddBankWidgets(bkBank* pBank);
	static void UpdateDebug();
	static void DrawDebug();

	static inline bool EarlyRejectExtraDistCull()					{ return bEarlyRejectExtraDistCull; }
	static inline int  GetEarlyRejectMaxDist()						{ return extraCullMaxDist; }
	static inline int  GetEarlyRejectMinDist()						{ return extraCullMinDist; }

	static inline void SetEarlyRejectMaxDist(int maxDist)			{ extraCullMaxDist = maxDist; }
	static inline void SetEarlyRejectMinDist(int minDist)			{ extraCullMinDist = minDist; }
	static inline void SetEarlyRejectExtraDistCull(bool enabled)	{ bEarlyRejectExtraDistCull = enabled; }

	static inline CEntity* GetSelectedEntity() { return pEntity; }

private:
	static void ToggleEkgActiveAddsRemoves();
	static void ToggleDrawEntireMapAABB();
	static void CullDepthAltered();
	static void PrintEntityContainerStats();

	static void DefragPoolCopyGapsToBuffer(fwDescPool* pPool);
	static void DefragPoolDrawDebugAllGaps(Vector2& vTopLeft, float fCellW, float fCellH, u32 nGridW);

	static bool bShowDataFragmentation;
	static bool bShowEntityList;
	static bool bShowBoundingSphere;
	static bool bShowSolidBoundingSphere;
	static bool bShowBoundingBox;
	static bool bEarlyRejectExtraDistCull;
	static bool bEkgEnabledAddsRemoves;
	static bool bDrawEntireMapAABB;
	static bool bEntityContainerStats;

	static int listOffset;
	static int lodOffset;
	static int extraCullMaxDist;
	static int extraCullMinDist;
	static int cullDepth;

	static RegdEnt pEntity; // SR this is dependent on game code. Need to swap for 

	static DefragPool_SGapInfo*	m_asBankGapBuffer;
	static u32					m_nBankNumGaps;
};

#endif // __BANK
#endif // __INC_WORLDDEBUGHELPER_H_
