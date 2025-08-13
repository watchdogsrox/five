/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodDrawable.h
// PURPOSE : Drawable LODs related code.
// AUTHOR :  Alex Hadjadj
// CREATED : 29/10/12
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_LOD_LODDRAWABLE_H_
#define _SCENE_LOD_LODDRAWABLE_H_

#include "system/bit.h"
#include "vector/vector3.h"

#include "fwdrawlist/drawlistmgr.h"

namespace rage
{
	class rmcLodGroup;
	class rmcDrawable;
}

class CBaseModelInfo;

namespace LODDrawable
{
	enum LODFlags {
		LODFLAG_NONE				= 0x7, // if flag is this value, then no foced LOD index ..
		LODFLAG_FORCE_LOD_LEVEL		= 0x4, // else if this bit is set, lower 2 bits are LOD index

		LODFLAG_PHASE_DEFAULT		= 0,
		LODFLAG_PHASE_SHADOW,
		LODFLAG_PHASE_REFLECTION,
		LODFLAG_PHASE_WATER,
		LODFLAG_PHASE_MIRROR,
		LODFLAG_PHASE_COUNT,

		LODFLAG_SHIFT				= 3,
		LODFLAG_SHIFT_DEFAULT		= LODFLAG_SHIFT*LODFLAG_PHASE_DEFAULT,
		LODFLAG_SHIFT_SHADOW		= LODFLAG_SHIFT*LODFLAG_PHASE_SHADOW,
		LODFLAG_SHIFT_REFLECTION	= LODFLAG_SHIFT*LODFLAG_PHASE_REFLECTION,
		LODFLAG_SHIFT_WATER			= LODFLAG_SHIFT*LODFLAG_PHASE_WATER,
		LODFLAG_SHIFT_MIRROR		= LODFLAG_SHIFT*LODFLAG_PHASE_MIRROR,

		LODFLAG_MASK				= BIT(LODFLAG_SHIFT) - 1,
		LODFLAG_NONE_ALL			= BIT(LODFLAG_SHIFT*LODFLAG_PHASE_COUNT) - 1,
	};

	enum crossFadeDistanceIdx {
		CFDIST_MAIN = 0,
		CFDIST_ALTERNATE = 1
	};

	u32 CalculateDrawableLODFlags(const rmcLodGroup &lodGroup, u32 bucketType);

	// Crossfade and LOD Disances
	inline float CalcLODDistance(const Vector3& pos)
	{
		Vector3 delta = (pos - gDrawListMgr->GetCalcPosDrawableLOD());
		return(delta.Mag());
	}

	void CalcLODLevelAndCrossfade(const rmcDrawable* pDrawable, float dist, u32 &LODLevel, s32 &crossFade, crossFadeDistanceIdx distIdx, u32 lastLODIdx ASSERT_ONLY(, const CBaseModelInfo *modelInfo), float typeScale = 1.0f);
	void CalcLODLevelAndCrossfadeForProxyLod(const rmcDrawable* pDrawable, float dist, u32 selectedLODLevel, u32 &LODLevel, s32 &crossFade, crossFadeDistanceIdx distIdx, u32 lastLODIdx, float typeScale = 1.0f);

	extern DECLARE_MTR_THREAD bool s_isRenderingLowestCascade;

	inline bool IsRenderingLowestCascade(){ return s_isRenderingLowestCascade;}
	inline void SetRenderingLowestCascade(bool v) { s_isRenderingLowestCascade = v;}
}

#endif
