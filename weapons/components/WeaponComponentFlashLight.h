//
// weapons/weaponcomponentflashlight.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_FLASH_LIGHT_H
#define WEAPON_COMPONENT_FLASH_LIGHT_H

#include "vector/color32.h"

// Game headers
#include "control/replay/ReplaySettings.h"
#include "Weapons/Components/WeaponComponent.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentFlashLightInfo : public CWeaponComponentInfo
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentFlashLightInfo, CWeaponComponentInfo, WCT_FlashLight);
public:

	CWeaponComponentFlashLightInfo();
	virtual ~CWeaponComponentFlashLightInfo();

	// PURPOSE: Get the Main light tuning values
	f32 GetMainLightIntensity() const; 
	Color32 GetMainLightColor() const;
	f32 GetMainLightRange() const;
	f32 GetMainLightFalloffExponent() const;
	f32 GetMainLightInnerAngle() const;
	f32 GetMainLightOuterAngle() const;
	f32 GetMainLightCoronaIntensity() const;
	f32 GetMainLightCoronaSize() const;
	f32 GetMainLightVolumeIntensity() const;
	f32 GetMainLightVolumeSize() const;
	f32 GetMainLightVolumeExponent() const;
	Color32 GetMainLightVolumeOuterColor() const;
	f32 GetMainLightShadowFadeDistance() const;
	f32 GetMainLightSpecularFadeDistance() const;
	int GetTextureId() const;
	
	// PURPOSE: Get the Secondary light tuning values
	f32 GetSecondaryLightIntensity() const;
	Color32 GetSecondaryLightColor() const;
	f32 GetSecondaryLightRange() const;
	f32 GetSecondaryLightFalloffExponent() const;
	f32 GetSecondaryLightInnerAngle() const;
	f32 GetSecondaryLightOuterAngle() const;
	f32 GetSecondaryLightVolumeIntensity() const;
	f32 GetSecondaryLightVolumeSize() const;
	f32 GetSecondaryLightVolumeExponent() const;
	Color32 GetSecondaryLightVolumeOuterColor() const;
	f32 GetSecondaryLightFadeDistance() const;
	f32 GetTargetDistAlongAimCamera() const;
	
	// PURPOSE: Get the flash light bone helper
	const CWeaponBoneId& GetFlashLightBone() const;

	// PURPOSE: Get the flash light's bulb on bone helper
	const CWeaponBoneId& GetFlashLightBoneBulbOn() const;

	// PURPOSE: Get the flash light's bulb off bone helper
	const CWeaponBoneId& GetFlashLightBoneBulbOff() const;

	bool GetToggleWhenAiming() const;



private:

	//
	// Members
	//

	// PURPOSE: Main light tuning values
	f32 m_MainLightIntensity;
	Color32 m_MainLightColor;
	f32 m_MainLightRange;
	f32 m_MainLightFalloffExponent;
	f32 m_MainLightInnerAngle;
	f32 m_MainLightOuterAngle;
	f32 m_MainLightCoronaIntensity;
	f32 m_MainLightCoronaSize;
	f32 m_MainLightVolumeIntensity;
	f32 m_MainLightVolumeSize;
	f32 m_MainLightVolumeExponent;
	Color32 m_MainLightVolumeOuterColor;
	f32 m_MainLightShadowFadeDistance;
	f32 m_MainLightSpecularFadeDistance;
	
	// PURPOSE: Secondary light tuning values
	f32 m_SecondaryLightIntensity;
	Color32 m_SecondaryLightColor;
	f32 m_SecondaryLightRange;
	f32 m_SecondaryLightFalloffExponent;
	f32 m_SecondaryLightInnerAngle;
	f32 m_SecondaryLightOuterAngle;
	f32 m_SecondaryLightVolumeIntensity;
	f32 m_SecondaryLightVolumeSize;
	f32 m_SecondaryLightVolumeExponent;
	Color32 m_SecondaryLightVolumeOuterColor;
	f32 m_SecondaryLightFadeDistance;
	f32 m_fTargetDistalongAimCamera;

	// PURPOSE: The corresponding bone index for the light emit point
	CWeaponBoneId m_FlashLightBone;
	
	// PURPOSE: The corresponding bone index for the bulb on model
	CWeaponBoneId m_FlashLightBoneBulbOn;
	
	// PURPOSE: The corresponding bone index for the bulb off model
	CWeaponBoneId m_FlashLightBoneBulbOff;

	// PURPOSE: Should we link the light being on/off to aiming/idle (true), or allow manual toggle via d-pad (false)?
	bool m_ToggleWhenAiming;

	int m_texId;
	
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentFlashLight : public CWeaponComponent
{
	DECLARE_RTTI_DERIVED_CLASS_WITH_ID(CWeaponComponentFlashLight, CWeaponComponent, WCT_FlashLight);
public:

	CWeaponComponentFlashLight();
	CWeaponComponentFlashLight(const CWeaponComponentFlashLightInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponentFlashLight();

	// PURPOSE: Process component
	virtual void Process(CEntity* pFiringEntity);

	// PURPOSE: Process after attachments/ik has been done
	virtual void ProcessPostPreRender(CEntity* pFiringEntity);

	// PURPOSE: Access the const info
	const CWeaponComponentFlashLightInfo* GetInfo() const;

	// PURPOSE: Set the remote Active state on/off
	void SetActive(bool bValue) { m_bSetBulbState |= (m_bActive != bValue); m_bActive = bValue; }

	// PURPOSE: Get the current Active state on/off
	const bool GetActive() const { return m_bActive; }

	// PURPOSE: Toggles the Active state on/off
	void ToggleActive(CPed* pPed);

	// PURPOSE: Set the history of the light so that it can be enabled next when aiming.
	void SetActiveHistory(bool bValue) { m_bActiveHistory = bValue; }

	void SetForceFlashLightOn(bool bValue) { m_bForceFlashLightOn = bValue; }

	// PURPOSE: Transfer activeness and LOD of the flashlight component from a previous flashlight component
	void TransferActiveness(CWeaponComponentFlashLight* pPrevFlashlightComp);

	bool GetIsVisible() const;

	bool IsNight() const;

	bool CouldBeVisible(CPed *pFiringEntity);
	bool CouldBeVisible();
	bool CouldBeVisible(Matrix34 &lightMatrix);
	
#if GTA_REPLAY
	//Override the lod during replay playback as we don't have the required data to add the light into flashlightList.
	void OverrideLod(u8 lod) { m_lod = lod; }
#endif

private:

	//
	// Members
	//

	// PURPOSE: The corresponding bone index for the light emit point
	s32 m_iFlashLightBoneIdx;

	// PURPOSE: The corresponding bone index for the bulb on model
	s32 m_iFlashLightBoneBulbOnIdx;

	// PURPOSE: The corresponding bone index for the bulb off model
	s32 m_iFlashLightBoneBulbOffIdx;

	// PURPOSE: Used to interpolate the light direction from light bone forward to pointing at the reticle
	f32 m_fInterpolateLightDirection;

	// PURPOSE: Turn off the flash light after X seconds
	f32 m_OffTimer;

	// PURPOSE: Turn on the flash light in FP after X seconds
	f32 m_FPSOnTimer;

	// PURPOSE: The LOD level for this flash light to use
	u8  m_lod;

	// PURPOSE: Is the flash light on?
	bool m_bActive : 1;

	// PURPOSE: Do we need to change the state of the light bulb 
	bool m_bSetBulbState : 1;

	// PURPOSE: We automatically turn off the flash light for the player when not aiming, Used to know if it was one before hand. 
	bool m_bActiveHistory  : 1;

	// PURPOSE: Ability to force the flashlight on, even if there is no firingentity
	bool m_bForceFlashLightOn : 1;

	// PURPOSE: To mark whether this flashlight has been processed for this frame
	bool m_bProcessed : 1;


	//
	// Static Members
	//	
private:
	static u8 sm_LightFadeDist;
public: 
	struct flashlightPriorityItem {
		CWeaponComponentFlashLight *pFlashlight;
		RegdPed owner;
		Vec3V pos;
		int priority;
	};
	static atFixedArray<flashlightPriorityItem, 16> flashlightList;
	static atFixedArray<u8, 16> flashlightLookupList;

	static void CalcFlashlightPriorities();

	// If distance is <= 0, will use specularFade*2. Otherwise, this sets override
	static void SetFlashlightFadeDistance(float distance);
};

////////////////////////////////////////////////////////////////////////////////

// PURPOSE: Main light tuning values
inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightIntensity() const
{
	return m_MainLightIntensity;
}

inline 	Color32 CWeaponComponentFlashLightInfo::GetMainLightColor() const
{
	return m_MainLightColor;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightRange() const
{
	return m_MainLightRange;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightFalloffExponent() const
{
	return m_MainLightFalloffExponent;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightInnerAngle() const
{
	return m_MainLightInnerAngle;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightOuterAngle() const
{
	return m_MainLightOuterAngle;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightCoronaIntensity() const
{
	return m_MainLightCoronaIntensity;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightCoronaSize() const
{
	return m_MainLightCoronaSize;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightVolumeIntensity() const
{
	return m_MainLightVolumeIntensity;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightVolumeSize() const
{
	return m_MainLightVolumeSize;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightVolumeExponent() const
{
	return m_MainLightVolumeExponent;
}

inline 	Color32 CWeaponComponentFlashLightInfo::GetMainLightVolumeOuterColor() const
{
	return m_MainLightVolumeOuterColor;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightShadowFadeDistance() const
{
	return m_MainLightShadowFadeDistance;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetMainLightSpecularFadeDistance() const
{
	return m_MainLightSpecularFadeDistance;
}
	
inline 	int CWeaponComponentFlashLightInfo::GetTextureId() const
{
	return m_texId;
}

////////////////////////////////////////////////////////////////////////////////

// PURPOSE: Secondary light tuning values
inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightIntensity() const
{
	return m_SecondaryLightIntensity;
}

inline 	Color32 CWeaponComponentFlashLightInfo::GetSecondaryLightColor() const
{
	return m_SecondaryLightColor;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightRange() const
{
	return m_SecondaryLightRange;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightFalloffExponent() const
{
	return m_SecondaryLightFalloffExponent;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightInnerAngle() const
{
	return m_SecondaryLightInnerAngle;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightOuterAngle() const
{
	return m_SecondaryLightOuterAngle;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightVolumeIntensity() const
{
	return m_SecondaryLightVolumeIntensity;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightVolumeSize() const
{
	return m_SecondaryLightVolumeSize;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightVolumeExponent() const
{
	return m_SecondaryLightVolumeExponent;
}

inline 	Color32 CWeaponComponentFlashLightInfo::GetSecondaryLightVolumeOuterColor() const
{
	return m_SecondaryLightVolumeOuterColor;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetSecondaryLightFadeDistance() const
{
	return m_SecondaryLightFadeDistance;
}

inline 	f32 CWeaponComponentFlashLightInfo::GetTargetDistAlongAimCamera() const
{
	return m_fTargetDistalongAimCamera;
}
	
////////////////////////////////////////////////////////////////////////////////

inline const CWeaponBoneId& CWeaponComponentFlashLightInfo::GetFlashLightBone() const
{
	return m_FlashLightBone;
}

// PURPOSE: Get the flash light's bulb on bone helper
inline const CWeaponBoneId& CWeaponComponentFlashLightInfo::GetFlashLightBoneBulbOn() const
{
	return m_FlashLightBoneBulbOn;
}

// PURPOSE: Get the flash light's bulb off bone helper
inline const CWeaponBoneId& CWeaponComponentFlashLightInfo::GetFlashLightBoneBulbOff() const
{
	return m_FlashLightBoneBulbOff;
}

////////////////////////////////////////////////////////////////////////////////

// PURPOSE: Should we link the light being on/off to aiming/idle (true), or allow manual toggle via d-pad (false)?
inline bool CWeaponComponentFlashLightInfo::GetToggleWhenAiming() const
{
	return m_ToggleWhenAiming;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentFlashLightInfo* CWeaponComponentFlashLight::GetInfo() const
{
	return static_cast<const CWeaponComponentFlashLightInfo*>(superclass::GetInfo());
}

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_FLASH_LIGHT_H
