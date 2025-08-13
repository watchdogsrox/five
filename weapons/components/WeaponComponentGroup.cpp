//
// weapons/weaponcomponentgroup.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Components/WeaponComponentGroup.h"

// Game headers
#include "Weapons/Components/WeaponComponentFactory.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentGroupInfo::CWeaponComponentGroupInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentGroupInfo::~CWeaponComponentGroupInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentGroup::CWeaponComponentGroup()
{
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentGroup::CWeaponComponentGroup(const CWeaponComponentGroupInfo* pInfo, CWeapon* pWeapon, CDynamicEntity* pDrawable)
: superclass(pInfo, pWeapon, pDrawable)
{
	const CWeaponComponentGroupInfo::Infos& infos = pInfo->GetInfos();
	for(s32 i = 0; i < infos.GetCount(); i++)
	{
		CWeaponComponent* pComponent = WeaponComponentFactory::Create(infos[i], pWeapon, pDrawable);
		if(pComponent)
		{
			m_Components.Push(pComponent);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentGroup::~CWeaponComponentGroup()
{
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		delete m_Components[i];
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentGroup::Process(CEntity* pFiringEntity)
{
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->Process(pFiringEntity);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentGroup::ProcessPostPreRender(CEntity* pFiringEntity)
{
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->ProcessPostPreRender(pFiringEntity);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentGroup::ModifyAccuracy(sWeaponAccuracy& inOutAccuracy)
{
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->ModifyAccuracy(inOutAccuracy);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentGroup::ModifyDamage(f32& inOutDamage)
{
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->ModifyDamage(inOutDamage);
	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CWeaponComponentGroup::RenderDebug(CEntity* pFiringEntity) const
{
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->RenderDebug(pFiringEntity);
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
