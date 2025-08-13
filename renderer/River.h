////////////////////////////////////////////////////////////////////////////////////
// Title	:	River.h
// Author	:	Randy
// Started	:	11-16-2010
////////////////////////////////////////////////////////////////////////////////////
#ifndef _RIVER_H_
#define _RIVER_H_

#include "grcore/effect_typedefs.h"
#include "bank/bank.h"
#include "scene/Entity.h"
#include "vector/vector3.h"
#if RSG_PC
#include "grcore/texture_d3d11.h"
#endif

namespace rage{	class grcRenderTarget; }

#define MAXRIVERENTITIES 64

struct WaterEntity
{
	RegdEnt			m_Entity;
	bool			m_IsRiver;
	Vec3V			m_Min, m_Max;
#if RSG_ORBIS || RSG_PC
	u8*				flowTextureData;
#endif
#if RSG_PC
	bool			m_WaitingForFlowData;
#endif
};

namespace River
{
	void	Init();
	void	SetFogPassGlobalVars();
	void	SetWaterPassGlobalVars(float elapsedTime, bool useMirrorWaterRT);
	void	SetDecalPassGlobalVars();
	void	BeginRenderRiverFog();
	void	EndRenderRiverFog();
	void	AddToRiverEntityList(CEntity* entity);
	void	RemoveFromRiverEntityList(const CEntity* entity);
#if RSG_PC
	void UpdateFlowMaps();
#endif
	void	GetRiverFlowFromEntity(WaterEntity& waterEntity, const Vector3& pos, Vector2& outFlow);
	void	GetRiverFlowFromPosition(Vec3V_In pos, Vector2& outFlow);
	void	DrawRiverCaustics(grmShader* shader, grcEffectTechnique technique, s32 pass);
	void	FillRiverDrawList();
	void	ResetRiverDrawList();
	float	GetRiverHeightAtCamera();
	void	Update();
	float	GetRiverPushForce();
	bool	CameraOnRiver();

	s32		GetRiverEntityCount();//All entities that contain river type drawables loaded
	s32		GetRiverDrawableCount();//Number of standalone rivers in the draw list
	s32		GetWaterBucketCount();//All drawables that contain water pass meshes in the draw list

	const CEntity* GetRiverEntity(s32 entityIndex);

#if __BANK
	void InitWidgets(bkBank& bank);
#endif

	grcEffectGlobalVar GetBumpTextureVar();
	grcEffectGlobalVar GetWetTextureVar();

	bool	IsPositionNearRiver_Impl(Vec3V_In pos, s32* pEntityIndex, bool riversOnly, VecBoolV_In channelsToConsider, ScalarV_In maxDistanceToConsider);

	__forceinline bool IsPositionNearRiver(Vec3V_In pos, s32* pEntityIndex = NULL, bool riversOnly = false, bool shouldConsiderZ = false, ScalarV_In maxDistanceToConsider = ScalarV(V_ZERO))
	{
		if (shouldConsiderZ)
		{
			return IsPositionNearRiver_Impl(pos, pEntityIndex, riversOnly, VecBoolV(V_T_T_T_F), maxDistanceToConsider);
		}
		else
		{
			return IsPositionNearRiver_Impl(pos, pEntityIndex, riversOnly, VecBoolV(V_T_T_F_F), maxDistanceToConsider);
		}
	}
};
#endif //_RIVER_H_...
