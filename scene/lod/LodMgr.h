/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodMgr.h
// PURPOSE : Primary interface to lod tree systems
// AUTHOR :  Ian Kiigan
// CREATED : 05/03/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_LOD_LODMGR_H_
#define _SCENE_LOD_LODMGR_H_

#include "fwscene/Ipl.h"

#include "camera/viewports/ViewportManager.h"
#include "fwscene/lod/LodTypes.h"
#include "fwutil/PtrList.h"
#include "fwsys/timer.h"
#include "scene/ContinuityMgr.h"
#include "scene/Entity.h"
#include "scene/lod/LodScale.h"

class CLodMgrSlodMode
{
public:
	void Reset() { m_bActive=false; }
	void Start(u32 lodLevelMask) { m_lodLevelMask=lodLevelMask; m_bActive=true; }
	void Stop() { m_bActive=false; }
	bool IsActive() const { return m_bActive; }
	void SetEntityAlpha(fwLodData& lodData)
	{
		lodData.SetAlpha( ((lodData.GetLodFlag()&m_lodLevelMask)!=0) ? LODTYPES_ALPHA_VISIBLE : LODTYPES_ALPHA_INVISIBLE );
	}
	u32 GetRequiredLodLevelMask() const { return (m_bActive ? m_lodLevelMask : LODTYPES_MASK_ALL); }

private:
	u32 m_lodLevelMask;
	bool m_bActive;
};

class CLodMgr
{
public:

	enum eLodFilterMode
	{
		LODFILTER_MODE_PLAYERSWITCH,
		LODFILTER_MODE_SCRIPT,
		LODFILTER_MODE_DEBUG
	};

	CLodMgr();

	void					Reset();
	void					PreUpdate(bool bMajorCameraTransitionOccurred);
	inline Vector3&			GetCameraPosition()									{ return m_vCameraPos; }
	
	inline u32				GetHierarchyNoiseFactor(CEntity* pEntity)	{ return (u32)(size_t)pEntity->GetRootLod(); }
	bool					WithinVisibleRange(CEntity* pEntity, bool bIncludePreStream=false, float fadeDistMod = 1.f);
	void					UpdateAlpha(CEntity* pEntity, float fDistToCam, u32 targetAlpha, bool bFadeRelatedToChildren = true);

	__forceinline u32		GetAlphaPt1_FadeUp(fwLodData& lodData, u32 targetAlpha);
	__forceinline void		UpdateAlphaPt2_FadeDownRelativeToChildren(CEntity* pEntity, bool& bShouldBeVisible);

	inline void				GetBasisPivot(fwEntity* pEntity, Vector3& vBasisPivot);

	void					OverrideLodDist(CEntity* pEntity, u32 lodDist);
	bool					AllowTimeBasedFadingThisFrame() const { return m_bAllowTimeBasedFading; }

	float					GetFadeDistance(u32 lodType) const { return m_aFadeDists[lodType]; }

	void					ScriptForceAllowTimeBasedFadingThisFrame() { m_bScriptForceAllowTimeBasedFading=true; }

	//////////////////////////////////////////////////////////////////////////
	void					ScriptRequestRenderHdOnly() { m_bScriptRequestHdOnly=true; }
	void					StartSlodMode(u32 lodLevelMask, eLodFilterMode filterMode) { m_filterMode=filterMode; m_slodMode.Start(lodLevelMask); }
	void					StopSlodMode() { m_slodMode.Stop(); }
	void					UpdateAlpha_SlodMode(fwLodData& lodData) { m_slodMode.SetEntityAlpha(lodData); }
	bool					IsSlodModeActive() const { return m_slodMode.IsActive(); }
	u32						GetSlodModeRequiredMask() const { return m_slodMode.GetRequiredLodLevelMask(); }
	//////////////////////////////////////////////////////////////////////////

	u32						GetMapDataAssetMaskForLodLevel(u32 lodLevel) const;
	u32						GetMapDataAssetMaskForLodMask(u32 lodLevelMask) const;

	inline bool				LodLevelMayTimeBasedFade(u32 lodLevel) { return ((1 << lodLevel) & m_lodFlagsForTimeBasedFade) != 0; }
	__forceinline bool		TimeFadeAllowedForEntity(const fwLodData& lodData) const
	{
		return lodData.IsVisible() && ( (lodData.GetLodFlag()&m_lodFlagsForTimeBasedFade) || lodData.IsRootLod() );
	}
	inline bool				AllowedToForceDrawParent(const fwLodData& lodData) const
	{
		return ( (lodData.GetLodFlag()&m_lodFlagsForTimeBasedFade)!=0 );
	}
	
#if __BANK
	void					OverrideFadeDistance(u32 lodType, float fFadeDistance) { m_aFadeDists[lodType] = fFadeDistance;}
	static void				StartSlodModeCb();
	static void				StopSlodModeCb();
	static bool				ms_abSlodModeLevels[LODTYPES_DEPTH_TOTAL];
#endif	//__BANK

private:
	__forceinline u8		CalcCrossFadeAlpha(const float fDistFromCamera, const float fStartDist, const float fEndDist, const bool bFadingUp);

	void					DecideWhatLodLevelsCanDoTimeBasedFade();
	
	u32 m_lodFlagsForTimeBasedFade;
	Vector3 m_vCameraPos;
	bool m_bAllowTimeBasedFading;
	bool m_bScriptForceAllowTimeBasedFading;
	float m_aFadeDists[LODTYPES_DEPTH_TOTAL];

	eLodFilterMode m_filterMode;
	CLodMgrSlodMode m_slodMode;
	bool m_bScriptRequestHdOnly;

};

inline void CLodMgr::GetBasisPivot(fwEntity* pEntity, Vector3& vBasisPivot)
{
	fwEntity* pLod = pEntity->GetLod();
	const fwEntity* pBasisEntity = pLod ? pLod : pEntity;
	vBasisPivot = VEC3V_TO_VECTOR3(pBasisEntity->GetTransform().GetPosition());
}

inline u8 CLodMgr::CalcCrossFadeAlpha(const float fDistFromCamera, const float fStartDist, const float fEndDist, const bool bFadingUp)
{
	const float fFadeRange = fStartDist - fEndDist;
	const float fDist = Clamp(fDistFromCamera, fEndDist, fStartDist);
	float fAlpha = (fStartDist - fDist) / fFadeRange;
	if (!bFadingUp) { fAlpha = 1.0f - fAlpha; }
	return (u8) (fAlpha * 255.0f);
}

inline u32 CLodMgr::GetAlphaPt1_FadeUp(fwLodData& lodData, u32 targetAlpha)
{
	const u32 oldAlpha = lodData.GetAlpha();

	// FadeToZero is rare - check for the common branch first.
	if (!lodData.GetFadeToZero())
	{
		// fading up / down at edge of entity visibility
		if ((targetAlpha > oldAlpha) && m_bAllowTimeBasedFading && TimeFadeAllowedForEntity(lodData))
		{
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
			//lodData.SetAlpha( Min(oldAlpha+LODTYPES_ALPHA_STEP * rage::fwTimer::GetTimeStepInMilliseconds() / LODTYPES_TARGET_MILLIS, targetAlpha) );
			return Min(oldAlpha+LODTYPES_ALPHA_STEP * rage::fwTimer::GetTimeStepInMilliseconds() / LODTYPES_TARGET_MILLIS, targetAlpha);
#else
			//lodData.SetAlpha( Min(oldAlpha+LODTYPES_ALPHA_STEP, targetAlpha) );
			return Min(oldAlpha+LODTYPES_ALPHA_STEP, targetAlpha);
#endif
		}
		else
		{
			//lodData.SetAlpha( targetAlpha );
			return targetAlpha;
		}
	}
	else
	{
		// if script has requested the entity fade to zero
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
		//lodData.SetAlpha( Max((s32)(oldAlpha-LODTYPES_FADETOZERO_ALPHA_STEP * rage::fwTimer::GetTimeStepInMilliseconds() / LODTYPES_TARGET_MILLIS), 0) );
		return Max((s32)(oldAlpha-LODTYPES_FADETOZERO_ALPHA_STEP * rage::fwTimer::GetTimeStepInMilliseconds() / LODTYPES_TARGET_MILLIS), 0);
#else
		//lodData.SetAlpha( Max((s32)oldAlpha-LODTYPES_FADETOZERO_ALPHA_STEP, 0) );
		return Max((s32)oldAlpha-LODTYPES_FADETOZERO_ALPHA_STEP, 0);
#endif
	}
}

inline void CLodMgr::UpdateAlphaPt2_FadeDownRelativeToChildren(CEntity* pEntity, bool& bShouldBeVisible)
{
	fwLodData& lodData = pEntity->GetLodData();
	const u32 childDepth = lodData.GetChildLodType();
	const float fChildLodDist = ((float) (lodData.GetChildLodDistance())) * g_LodScale.GetPerLodLevelScale(childDepth);
	const float fDistStartFadeDown = fChildLodDist + LODTYPES_FADE_DISTF - m_aFadeDists[childDepth];

	// yuck. this is the distance from the children basis pivot to camera
	const float fDistFromThisNodeToCam2 = (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - m_vCameraPos).Mag2();
	if (fDistFromThisNodeToCam2 <= (fDistStartFadeDown*fDistStartFadeDown))
	{
		bShouldBeVisible = false;
		lodData.SetChildDrawing(true);

		const float fDistStopFadeDown = fDistStartFadeDown - LODTYPES_FADE_DISTF;
		lodData.SetAlpha( CalcCrossFadeAlpha(sqrt(fDistFromThisNodeToCam2), fDistStartFadeDown, fDistStopFadeDown, false) );
	}
}

extern CLodMgr g_LodMgr;

#endif
