//
// weapons/weaponcomponent.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponent.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Weapons/Helpers/WeaponHelpers.h"
#include "fwscene/stores/txdstore.h"
#include "modelinfo\BaseModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Objects/object.h"
#include "entity/drawdata.h"
#include "shaders/CustomShaderEffectWeapon.h"
WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentInfo::CWeaponComponentInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentInfo::~CWeaponComponentInfo()
{
	if(m_AccuracyModifier)
	{
		delete m_AccuracyModifier;
		m_AccuracyModifier = NULL;
	}

	if(m_DamageModifier)
	{
		delete m_DamageModifier;
		m_DamageModifier = NULL;
	}

	if(m_FallOffModifier)
	{
		delete m_FallOffModifier;
		m_FallOffModifier = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponent::CWeaponComponent()
: m_pInfo(NULL)
, m_pWeapon(NULL)
, m_pDrawableEntity(NULL)
, m_pStandardDetailShaderEffect(NULL)
, m_componentLodState(WCLS_HD_NONE)
, m_uTintIndex(0)
, m_bExplicitHdRequest(false)
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponent::CWeaponComponent(const CWeaponComponentInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
: m_pInfo(pInfo)
, m_pWeapon(pWeapon)
, m_pDrawableEntity(pDrawable)
, m_pStandardDetailShaderEffect(NULL)
, m_componentLodState(WCLS_HD_NONE)
, m_uTintIndex(0)
, m_bExplicitHdRequest(false)
{
	if (pDrawable)
	{
		if (pDrawable->GetIsTypeObject())
		{
			CObject *pObject = static_cast<CObject*>(pDrawable);

			pObject->SetWeaponComponent(this);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponent::~CWeaponComponent()
{
	if(m_componentLodState == WCLS_HD_AVAILABLE)
	{
		ShaderEffect_HD_DestroyInstance();
		m_componentLodState = WCLS_HD_REMOVING;
	}

	if(m_componentLodState != WCLS_HD_NONE && m_componentLodState != WCLS_HD_NA)
	{
		CBaseModelInfo* pModelInfo = m_pDrawableEntity ? m_pDrawableEntity->GetBaseModelInfo() : NULL;
		Assertf(pModelInfo, "No model info to remove HD instance");

		if(pModelInfo)	
		{
			static_cast<CWeaponModelInfo*>(pModelInfo)->RemoveFromHDInstanceList((size_t)this);
		}
	}

	if (m_pDrawableEntity) 
	{ 
		if (m_pDrawableEntity->GetIsTypeObject()) 
		{ 
			CObject *pObject = static_cast<CObject*>(m_pDrawableEntity.Get());   

			if (pObject->GetWeaponComponent()) 
			{ 
				pObject->SetWeaponComponent(NULL); 
			} 
		} 
	}
}


//////////////////////////////////////////////////////////////////////////////

void CWeaponComponent::Update_HD_Models(bool requireHighModel)
{
	m_bExplicitHdRequest = false;

	if (m_componentLodState == WCLS_HD_NA)
	{
		return;
	}

	CWeaponModelInfo *pWeaponModelInfo = NULL;

	CBaseModelInfo* pModelInfo = m_pDrawableEntity ? m_pDrawableEntity->GetBaseModelInfo() : NULL;
	if(pModelInfo && pModelInfo->GetModelType() == MI_TYPE_WEAPON)	
	{
		pWeaponModelInfo = static_cast<CWeaponModelInfo*>(pModelInfo);
	}

	if (!pWeaponModelInfo)
	{
		return;
	}

	if (!pWeaponModelInfo->GetHasHDFiles())
	{
		m_componentLodState = WCLS_HD_NA;
		return;
	}
	
	switch (m_componentLodState)
	{
		case WCLS_HD_NONE:
			if (requireHighModel)
			{
				pWeaponModelInfo->AddToHDInstanceList((size_t)this);
				m_componentLodState = WCLS_HD_REQUESTED;
			}
			break;
		case WCLS_HD_REQUESTED:
			if (pWeaponModelInfo->GetAreHDFilesLoaded())
			{
				ShaderEffect_HD_CreateInstance();
				m_componentLodState = WCLS_HD_AVAILABLE;
			}
			break;
		case WCLS_HD_AVAILABLE:
			if (!requireHighModel)
			{
				ShaderEffect_HD_DestroyInstance();
				m_componentLodState = WCLS_HD_REMOVING;
			}
			break;
		case WCLS_HD_REMOVING:
			pWeaponModelInfo->RemoveFromHDInstanceList((size_t)this);
			m_componentLodState = WCLS_HD_NONE;
			break;
		case WCLS_HD_NA :
		default :
			weaponAssert(false);		// illegal cases
			break;
	}
}


void CWeaponComponent::ShaderEffect_HD_CreateInstance()
{
	FastAssert(sysThreadType::IsUpdateThread());

	Assertf(m_pDrawableEntity, "m_pDrawableEntity is NULL");

	m_pStandardDetailShaderEffect = m_pDrawableEntity->GetDrawHandler().GetShaderEffect();		// store the old one
	// Some weapons (molotov) don't have shader effects
	if (m_pStandardDetailShaderEffect)
	{
		CWeaponModelInfo *pWeaponInfo  =  static_cast<CWeaponModelInfo *>(m_pDrawableEntity->GetBaseModelInfo());

		CCustomShaderEffectWeaponType *pMasterShaderEffectType = pWeaponInfo->GetHDMasterCustomShaderEffectType();
		if(pMasterShaderEffectType)
		{
			CCustomShaderEffectWeapon* pHDShaderEffect = NULL;
			Assert(pMasterShaderEffectType->GetIsHighDetail());
			pHDShaderEffect = static_cast<CCustomShaderEffectWeapon*>(pMasterShaderEffectType->CreateInstance(m_pDrawableEntity));
			Assert(pHDShaderEffect);

			// copy CSE std->HD settings:
			pHDShaderEffect->CopySettings((CCustomShaderEffectWeapon*)m_pStandardDetailShaderEffect);

			m_pDrawableEntity->GetDrawHandler().SetShaderEffect(pHDShaderEffect);
			UpdateShaderVariables(m_uTintIndex);
		}
		else
		{
			m_pStandardDetailShaderEffect = NULL;
		}
	}
}

void CWeaponComponent::ShaderEffect_HD_DestroyInstance()
{
	if (m_pStandardDetailShaderEffect)
	{
		fwCustomShaderEffect* pHDShaderEffect = m_pDrawableEntity->GetDrawHandler().GetShaderEffect();
		Assert(pHDShaderEffect);

		if(pHDShaderEffect)
		{
			Assert(pHDShaderEffect->GetType()==CSE_WEAPON);
			Assert((static_cast<CCustomShaderEffectWeapon*>(pHDShaderEffect))->GetCseType()->GetIsHighDetail());
			if(m_pStandardDetailShaderEffect)
			{
				// copy CSE HD->std settings back:
				((CCustomShaderEffectWeapon*)m_pStandardDetailShaderEffect)->CopySettings((CCustomShaderEffectWeapon*)pHDShaderEffect);
			}
			pHDShaderEffect->DeleteInstance();
		}

		m_pDrawableEntity->GetDrawHandler().SetShaderEffect(m_pStandardDetailShaderEffect);

		UpdateShaderVariables(m_uTintIndex);
		m_pStandardDetailShaderEffect = NULL;
	}
}

void CWeaponComponent::UpdateShaderVariables(u8 uTintIndex)
{
	m_uTintIndex = uTintIndex;
	m_pWeapon->UpdateShaderVariablesForDrawable(m_pDrawableEntity.Get(), m_uTintIndex);
}

////////////////////////////////////////////////////////////////////////////////

const CTaskAimGun* CWeaponComponent::GetAimTask(const CPed* pPed) const
{
	CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
	if(pAimGunTask)
		return pAimGunTask;

	pAimGunTask = static_cast<CTaskAimGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
	if(pAimGunTask)
		return pAimGunTask;

	pAimGunTask = static_cast<CTaskAimGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_BLIND_FIRE));
	if(pAimGunTask)
		return pAimGunTask;

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////
