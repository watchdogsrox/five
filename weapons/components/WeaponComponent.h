//
// weapons/weaponcomponent.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef WEAPON_COMPONENT_H
#define WEAPON_COMPONENT_H

// Rage headers
#include "parser/macros.h"

// Framework headers
#include "fwutil/rtti.h"

// Game headers
#include "Scene/RegdRefTypes.h"
#include "Weapons/Helpers/WeaponHelpers.h"
#include "Weapons/Modifiers/WeaponAccuracyModifier.h"
#include "Weapons/Modifiers/WeaponDamageModifier.h"
#include "Weapons/Modifiers/WeaponFallOffModifier.h"
#include "Weapons/WeaponChannel.h"
#include "atl/string.h"

// Forward declarations
class CEntity;
class CTaskAimGun;
class CWeapon;
namespace rage
{
	class fwCustomShaderEffect;
}

////////////////////////////////////////////////////////////////////////////////

enum WeaponComponentType
{
	WCT_Base,
	WCT_Clip,
	WCT_FlashLight,
	WCT_LaserSight,
	WCT_ProgrammableTargeting,
	WCT_Scope,
	WCT_Suppressor,
	WCT_VariantModel,
	WCT_Group,
};

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponentInfo : public fwRefAwareBase
{
	DECLARE_RTTI_BASE_CLASS_WITH_ID(CWeaponComponentInfo, WCT_Base);
public:
	FW_REGISTER_CLASS_POOL(CWeaponComponentInfo);

	// The maximum infos we can allocate
	static const s32 MAX_STORAGE = 425;

	CWeaponComponentInfo();
	virtual ~CWeaponComponentInfo();

	// PURPOSE: Get the name hash
	u32 GetHash() const;

	// PURPOSE: Get the model hash
	u32 GetModelHash() const;

	// PURPOSE: Get the attach bone helper
	const CWeaponBoneId& GetAttachBone() const;

	// PURPOSE: Get any accuracy modifier
	const CWeaponAccuracyModifier* GetAccuracyModifier() const;

	// PURPOSE: Get any damage modifier
	const CWeaponDamageModifier* GetDamageModifier() const;

	// PURPOSE: Get any fall-off modifier
	const CWeaponFallOffModifier* GetFallOffModifier() const;

#if !__FINAL
	// PURPOSE: Get the name
	const char* GetName() const;
	// PURPOSE: Get the model name
	const char* GetModelName() const;
#endif // !__FINAL

	// PURPOSE: Get the localization keys
	const atFinalHashString GetLocKey() const { return m_LocName; }
	const atFinalHashString GetLocDesc() const { return m_LocDesc; }

	// PURPOSE: Is shown on wheel?
	const bool IsShownOnWheel() const { return m_bShownOnWheel; }

	// PURPOSE: Should we create this CObject when creating the component?
	const bool GetCreateObject() const { return m_CreateObject; }

	// PURPOSE: Should we apply the main weapon object's tint/spec settings onto this component?
	const bool GetApplyWeaponTint() const { return m_ApplyWeaponTint; }

	// PURPOSE: UI bars
	s8 GetHudDamage() const { return m_HudDamage; }
	s8 GetHudSpeed() const { return m_HudSpeed; }
	s8 GetHudCapacity() const { return m_HudCapacity; }
	s8 GetHudAccuracy() const { return m_HudAccuracy; }
	s8 GetHudRange() const { return m_HudRange; }

private:

	//
	// Members
	//

	// PURPOSE: The hash of the name
	atHashWithStringNotFinal m_Name;

	// PURPOSE: The model hash
	atHashWithStringNotFinal m_Model;

	// PURPOSE: Localized name key
	atFinalHashString m_LocName;

	// PURPOSE: Localized description (for shops)
	atFinalHashString m_LocDesc;

	// PURPOSE: The corresponding bone index for the attach bone
	CWeaponBoneId m_AttachBone;

	// PURPOSE: Accuracy modifier
	CWeaponAccuracyModifier* m_AccuracyModifier;

	// PURPOSE: Damage modifier
	CWeaponDamageModifier* m_DamageModifier;

	// PURPOSE: Fall-off modifier
	CWeaponFallOffModifier* m_FallOffModifier;

	// PURPOSE: Determine if this is suppressed from the weapon wheel's display
	bool m_bShownOnWheel;

	// PURPOSE: Should we create a CObject for this component?
	bool m_CreateObject;

	// PURPOSE: Should we apply the main weapon object's tint/spec settings onto this component?
	bool m_ApplyWeaponTint;

	// UI bars
	s8 m_HudDamage;
	s8 m_HudSpeed;
	s8 m_HudCapacity;
	s8 m_HudAccuracy;
	s8 m_HudRange;

	PAR_PARSABLE;
};

typedef	fwRegdRef<const CWeaponComponentInfo> RegdWeaponComponentInfo;

////////////////////////////////////////////////////////////////////////////////

class CWeaponComponent
{
	DECLARE_RTTI_BASE_CLASS_WITH_ID(CWeaponComponent, WCT_Base);
public:
	FW_REGISTER_CLASS_POOL(CWeaponComponent);

	enum eWeaponComponentLodState 
	{
		WCLS_HD_NA			= 0, //	- cannot go into HD at all
		WCLS_HD_NONE		= 1, //	- no HD resources currently Loaded
		WCLS_HD_REQUESTED	= 2, // - HD resource requests are in flight
		WCLS_HD_AVAILABLE	= 3, // - HD resources available & can be used
		WCLS_HD_REMOVING	= 4	 // - HD not available & scheduled for removal
	};

	// The maximum infos we can allocate
	static const s32 MAX_STORAGE = 220;

	CWeaponComponent();
	CWeaponComponent(const CWeaponComponentInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable);
	virtual ~CWeaponComponent();

	// PURPOSE: Process component
	virtual void Process(CEntity* pFiringEntity);

	// PURPOSE: Process after attachments/ik has been done
	virtual void ProcessPostPreRender(CEntity* pFiringEntity);

	// PURPOSE: Modify the passed in accuracy
	virtual void ModifyAccuracy(sWeaponAccuracy& inOutAccuracy);

	// PURPOSE: Modify the passed in damage
	virtual void ModifyDamage(f32& inOutDamage);

	// PURPOSE: Modify the passed in fall-off values
	virtual void ModifyFallOff(f32& inOutRange, f32& inOutDamage);

	//
	// Accessors
	//

	// PURPOSE: Access the const info
	const CWeaponComponentInfo* GetInfo() const;

	// PURPOSE: Access the const weapon
	const CWeapon* GetWeapon() const;

	// PURPOSE: Access the weapon
	CWeapon* GetWeapon();

	// PURPOSE: Access the const drawable entity
	const CDynamicEntity* GetDrawable() const;

	// PURPOSE: Access the drawable entity
	CDynamicEntity* GetDrawable();

	// PURPOSE: Access the tint index
	u8 GetTintIndex() const { return m_uTintIndex; }

#if __BANK
	// PURPOSE: Debug rendering
	virtual void RenderDebug(CEntity* pFiringEntity) const;
#endif //__BANK

	void Update_HD_Models(bool requireHighModel);	// handle requests for high detail models
	void ShaderEffect_HD_CreateInstance();
	void ShaderEffect_HD_DestroyInstance();

	void UpdateShaderVariables(u8 uTintIndex);

	void RequestHdAssets() { m_bExplicitHdRequest = true; }
	bool GetIsCurrentlyHD() const { return m_componentLodState == WCLS_HD_AVAILABLE; }
	void SetComponentLodState(u8 state) { m_componentLodState = state; }
	u8 GetComponentLodState() { return m_componentLodState; }

protected:

	// PURPOSE: Get the active aim task
	const CTaskAimGun* GetAimTask(const CPed* pPed) const;

private:

	//
	// Members
	//

	// PURPOSE: The info that describes the basic component attributes
	RegdWeaponComponentInfo m_pInfo;

	// PURPOSE: Owner weapon
	RegdWeapon m_pWeapon; 

	// PURPOSE: Entity that represents our drawable/skeleton etc. Used to look up bones or play animations on
	RegdDyn m_pDrawableEntity;

	rage::fwCustomShaderEffect* m_pStandardDetailShaderEffect;

	u8 m_componentLodState;
	u8 m_uTintIndex;
	u8 m_bExplicitHdRequest : 1;

};

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentInfo::GetHash() const
{
	return m_Name.GetHash();
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CWeaponComponentInfo::GetModelHash() const
{
	return m_Model.GetHash();
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponBoneId& CWeaponComponentInfo::GetAttachBone() const
{
	return m_AttachBone;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponAccuracyModifier* CWeaponComponentInfo::GetAccuracyModifier() const
{
	return m_AccuracyModifier;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponDamageModifier* CWeaponComponentInfo::GetDamageModifier() const
{
	return m_DamageModifier;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponFallOffModifier* CWeaponComponentInfo::GetFallOffModifier() const
{
	return m_FallOffModifier;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
inline const char* CWeaponComponentInfo::GetName() const
{
	return m_Name.GetCStr();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
inline const char* CWeaponComponentInfo::GetModelName() const
{
	return m_Model.GetCStr();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponComponent::Process(CEntity* UNUSED_PARAM(pFiringEntity))
{
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponComponent::ProcessPostPreRender(CEntity* UNUSED_PARAM(pFiringEntity))
{
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponComponent::ModifyAccuracy(sWeaponAccuracy& inOutAccuracy)
{
	if(GetInfo()->GetAccuracyModifier())
	{
		GetInfo()->GetAccuracyModifier()->ModifyAccuracy(inOutAccuracy);
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponComponent::ModifyDamage(f32& inOutDamage)
{
	if(GetInfo()->GetDamageModifier())
	{
		GetInfo()->GetDamageModifier()->ModifyDamage(inOutDamage);
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CWeaponComponent::ModifyFallOff(f32& inOutRange, f32& inOutDamage)
{
	if(GetInfo()->GetFallOffModifier())
	{
		GetInfo()->GetFallOffModifier()->ModifyRange(inOutRange);
		GetInfo()->GetFallOffModifier()->ModifyDamage(inOutDamage);
	}
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponComponentInfo* CWeaponComponent::GetInfo() const
{
	weaponFatalAssertf(m_pInfo, "m_pInfo is NULL");
	return m_pInfo;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeapon* CWeaponComponent::GetWeapon() const
{
	return m_pWeapon.Get();
}

////////////////////////////////////////////////////////////////////////////////

inline CWeapon* CWeaponComponent::GetWeapon()
{
	return m_pWeapon;
}

////////////////////////////////////////////////////////////////////////////////

inline const CDynamicEntity* CWeaponComponent::GetDrawable() const
{
	return m_pDrawableEntity.Get();
}

////////////////////////////////////////////////////////////////////////////////

inline CDynamicEntity* CWeaponComponent::GetDrawable()
{
	return m_pDrawableEntity;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
inline void CWeaponComponent::RenderDebug(CEntity* UNUSED_PARAM(pFiringEntity)) const
{
}
// forward declare pool full callback so we don't get two versions of it
namespace rage { template<> void fwPool<CWeaponComponent>::PoolFullCallback(); }
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#endif // WEAPON_COMPONENT_H
