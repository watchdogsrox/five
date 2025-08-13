//
// weapons/weaponcomponentpool.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Components/WeaponComponent.h"

// Game headers
#include "Weapons/Components/WeaponComponentClip.h"
#include "Weapons/Components/WeaponComponentFlashLight.h"
#include "Weapons/Components/WeaponComponentGroup.h"
#include "Weapons/Components/WeaponComponentLaserSight.h"
#include "Weapons/Components/WeaponComponentProgrammableTargeting.h"
#include "Weapons/Components/WeaponComponentScope.h"
#include "Weapons/Components/WeaponComponentSuppressor.h"
#include "Weapons/Components/WeaponComponentVariantModel.h"
#include "Weapons/Weapon.h"

WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////


// The largest info class that we size the pool to
#define LARGEST_COMPONENT_INFO_CLASS sizeof(CWeaponComponentFlashLightInfo)

CompileTimeAssert(sizeof(CWeaponComponentInfo)							<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentClipInfo)						<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentFlashLightInfo)				<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentGroupInfo)						<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentLaserSightInfo)				<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentProgrammableTargetingInfo)		<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentScopeInfo)						<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentSuppressorInfo)				<= LARGEST_COMPONENT_INFO_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentVariantModelInfo)				<= LARGEST_COMPONENT_INFO_CLASS);

FW_INSTANTIATE_BASECLASS_POOL(CWeaponComponentInfo, CWeaponComponentInfo::MAX_STORAGE, atHashString("CWeaponComponentInfo",0xbae33ccf), LARGEST_COMPONENT_INFO_CLASS);

////////////////////////////////////////////////////////////////////////////////

// The largest info class that we size the pool to
#define LARGEST_COMPONENT_CLASS sizeof(CWeaponComponentScope)

CompileTimeAssert(sizeof(CWeaponComponent)								<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentClip)							<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentFlashLight)					<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentGroup)							<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentLaserSight)					<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentProgrammableTargeting)			<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentScope)							<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentSuppressor)					<= LARGEST_COMPONENT_CLASS);
CompileTimeAssert(sizeof(CWeaponComponentVariantModel)					<= LARGEST_COMPONENT_CLASS);

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CWeaponComponent, CWeaponComponent::MAX_STORAGE, 0.23f, atHashString("CWeaponComponent",0x2f040858), LARGEST_COMPONENT_CLASS);

////////////////////////////////////////////////////////////////////////////////

#if __BANK
template<> void fwPool<CWeaponComponent>::PoolFullCallback()
{
	USE_DEBUG_MEMORY();

	s32 size = GetSize();
	int iIndex = 0;
	while(size--)
	{
		CWeaponComponent* pComponent = GetSlot(iIndex);
		if(pComponent)
		{
			const CWeaponComponentInfo* pComponentInfo = pComponent->GetInfo();
			const CWeapon* pWeapon = pComponent->GetWeapon();
			const CDynamicEntity* pWeaponEntity = pComponent->GetDrawable();

			Displayf("%i, type: %i, name: %s, model: %s, attach bone: %p, weapon: %s, weapon is attached to: %p."
				, iIndex, pComponentInfo->GetClassId(), pComponentInfo->GetName(), pComponentInfo->GetModelName(), &pComponentInfo->GetAttachBone(), pWeapon->GetWeaponInfo()->GetName(), pWeaponEntity && pWeaponEntity->GetIsAttached()? pWeaponEntity->GetAttachParent():0);

		}
		else
		{
			Displayf("%i, NULL component.", iIndex);
		}

		iIndex++;
	}
}
#endif	// __BANK
