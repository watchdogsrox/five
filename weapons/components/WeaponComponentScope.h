//
// weapons/weaponcomponentscope.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_SCOPE_H
#define WEAPON_COMPONENT_SCOPE_H

#include "vector/color32.h"

// Game headers
#include "Weapons/Components/WeaponComponent.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentScopeInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentScopeInfo, CWeaponComponentInfo, WCT_Scope);
public:

	// Special Type
	enum SpecialScopeType
	{
		None = 0,
		NightVision,
		ThermalVision
	};

	CWeaponComponentScopeInfo();
	virtual ~CWeaponComponentScopeInfo();

	// PURPOSE: Get the scope camera hash
	u32 GetCameraHash() const;

	// PURPOSE: Get the recoil shake amplitude
	f32 GetRecoilShakeAmplitude() const;

	// PURPOSE: Get the additional zoom factor to apply in accurate aim mode when this scope is equipped
	f32 GetExtraZoomFactorForAccurateMode() const;

	// PURPOSE: The reticule that should be displayed when aiming.
	u32 GetReticuleHash() const;

	// PURPOSE: Get special scope type for full screen postfx.
	bool GetIsSpecialScopeType() const { return m_SpecialScopeType != 0; }
	bool GetIsNightVisionScope() const { return m_SpecialScopeType == NightVision; }
	bool GetIsThermalVisionScope() const { return m_SpecialScopeType == ThermalVision; }

private:

	//
	// Members
	//

	// PURPOSE: Camera to use when scope active
	atHashWithStringNotFinal m_CameraHash;

	// PURPOSE: The amplitude of any recoil shake to be applied when scope active
	float m_RecoilShakeAmplitude;

	// PURPOSE: The additional zoom factor to apply in accurate aim mode when this scope is equipped
	float m_ExtraZoomFactorForAccurateMode;

	// PURPOSE: The reticule that should be displayed when aiming.
	atHashWithStringNotFinal m_ReticuleHash;

	CWeaponComponentScopeInfo::SpecialScopeType m_SpecialScopeType;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentScope : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentScope, CWeaponComponent, WCT_Scope);
public:

	CWeaponComponentScope();
	CWeaponComponentScope(const CWeaponComponentScopeInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentScope();

	// PURPOSE: Process component
	virtual void Process(CEntity* pFiringEntity);

	// PURPOSE: Get the camera hash
	u32 GetCameraHash() const;

	// PURPOSE: Get the recoil shake amplitude
	f32 GetRecoilShakeAmplitude() const;

	// PURPOSE: Get is active
	bool GetIsActive() const;

	// PURPOSE: Access the const info
	const CWeaponComponentScopeInfo* GetInfo() const;

	// PURPOSE: Set that we're rendering first person scope
	void SetRenderingFirstPersonScope(bool bValue) { m_bFirstPersonScopeActive = bValue; }

	// PURPOSE: Updates special scope controls / activation
	void UpdateSpecialScope(CEntity* pFiringEntity);

	// PURPOSE: Sets active history directly (restored from weapon inventory)
	void SetActiveHistory(bool bValue) { m_bActiveHistory = bValue; }

private:

	// PURPOSE: Updates the active state in the ped inventory, so we can persist through weapon swaps
	void UpdateInventoryHistory(CPed* pPed);

	// PURPOSE: Updates any active PostFX settings on active change (night vision, thermal vision, etc.)
	void UpdatePostFX();

	// PURPOSE: Updates the camera hash and recoil
	void UpdateCamera();

	// PURPOSE: Cache and restore see-through settings
	void CacheSeeThroughSettings();
	void RestoreSeeThroughSettings();

	// PURPOSE: Sets tint for special scope type
	void SetSpecialScopeTint();

	//
	// Members
	//

	// PURPOSE: The camera we should use
	u32 m_CameraHash;

	// PURPOSE: The amplitude of any recoil shake to be applied
	float m_RecoilShakeAmplitude;

	// PURPOSE: Is the scope active (applying postfx)?
	bool m_bActive;

	// PURPOSE: Was the scope active the last time we were using it?
	bool m_bActiveHistory;

	// PURPOSE: Is the first person scope active?
	bool m_bFirstPersonScopeActive;
	bool m_bFirstPersonScopeActiveHistory;
	u32 m_uScopeLastProcessedFrameCount;

	// PURPOSE: Were any postfx effects active before we started looking down the scope?
	bool m_bNightVisionEnabledPreScope;
	bool m_bThermalVisionEnabledPreScope;
	
	// PURPOSE: Used to initialise thermal scope tint.
	bool 		m_bInitialisedSpecialScopeTint;

	// PURPOSE: Local player that owns this scope component (so we can cleanup PostFX properly on destruction)
	CPed* m_pLocalPedOwner;

	// PURPOSE: Cached see-through values from before we applied our own settings (so we can restore later)
	float	m_fCachedSeeThroughHeatScaleDead;
	float	m_fCachedSeeThroughHeatScaleCold;
	float	m_fCachedSeeThroughHeatScaleWarm;
	float	m_fCachedSeeThroughHeatScaleHot;
	float	m_fCachedSeeThroughFadeStartDistance;
	float	m_fCachedSeeThroughFadeEndDistance;
	float	m_fCachedSeeThroughMaxThickness;
	float	m_fCachedSeeThroughNoiseMin;
	float	m_fCachedSeeThroughNoiseMax;
	float	m_fCachedSeeThroughHiLightIntensity;
	float	m_fCachedSeeThroughHiLightNoise;
	Color32 m_fCachedSeeThroughColorNear;
	Color32 m_fCachedSeeThroughColorFar;
	Color32 m_fCachedSeeThroughColorVisibleBase;
	Color32 m_fCachedSeeThroughColorVisibleWarm;
	Color32 m_fCachedSeeThroughColorVisibleHot;
};

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentScopeInfo::GetCameraHash() const
{
	return m_CameraHash.GetHash();
}

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeaponComponentScopeInfo::GetRecoilShakeAmplitude() const
{
	return m_RecoilShakeAmplitude;
}

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeaponComponentScopeInfo::GetExtraZoomFactorForAccurateMode() const
{
	return m_ExtraZoomFactorForAccurateMode;
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentScopeInfo::GetReticuleHash() const
{
	return m_ReticuleHash.GetHash();
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentScope::GetCameraHash() const
{
	return m_CameraHash;
}

////////////////////////////////////////////////////////////////////////////////

inline f32 CWeaponComponentScope::GetRecoilShakeAmplitude() const
{
	return m_RecoilShakeAmplitude;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CWeaponComponentScope::GetIsActive() const
{
	return m_bActive;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentScopeInfo* CWeaponComponentScope::GetInfo() const
{
	return static_cast<const CWeaponComponentScopeInfo*>(superclass::GetInfo());
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_SCOPE_H
