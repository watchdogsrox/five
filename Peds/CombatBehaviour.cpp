//
// Peds\CombatBehaviour.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Peds/CombatBehaviour.h"

// Rage Headers
#include "ai/aichannel.h"
#include "bank/bkmgr.h"
#include "parser/manager.h"

// Parser Header
#include "CombatInfo_parser.h"

// Game Headers
#include "Debug/DebugScene.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "scene/DataFileMgr.h"
#include "scene/dlc_channel.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

// Instantiate pools
FW_INSTANTIATE_CLASS_POOL(CCombatInfo, CONFIGURED_FROM_FILE, atHashString("CCombatInfo",0x2c1f0ac4));

CompileTimeAssert(CCombatData::BF_CanTauntInVehicle == 20);	// "COMBAT_ATTRIBUTE enum out of sync with commands_ped.sch"

////////////////////////////////////////////////////////////////////////////////

class CCombatInfoDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		dlcDebugf3("CCombatInfoDataFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);

		// Override using the specified file.
 		CCombatInfoMgr::Override(file.m_filename);
 		return true;
 	}
 
 	virtual void UnloadDataFile(const CDataFileMgr::DataFile & OUTPUT_ONLY(file)) 
	{
		dlcDebugf3("CCombatInfoDataFileMounter::UnloadDataFile: %s type = %d", file.m_filename, file.m_fileType);

		// Load the original file.
		CCombatInfoMgr::Revert();
	}
 
} g_CombatInfoDataFileMounter;


////////////////////////////////////////////////////////////////////////////////

CCombatBehaviour::CCombatBehaviour()
: m_InjuredOnGroundDuration(15000)
, m_iPreferredCoverGuid(-1)
{
}

////////////////////////////////////////////////////////////////////////////////

void CCombatBehaviour::InitFromCombatData(atHashWithStringNotFinal iCombatInfoHash)
{
	const CCombatInfo* pCombatInfo = CCombatInfoMgr::GetCombatInfoByHash(iCombatInfoHash);

	aiFatalAssertf(pCombatInfo, "Combat info %s didn't exist, check combatbehaviour.meta", iCombatInfoHash.GetCStr());

	m_FiringPatternHash = pCombatInfo->GetFiringPatternHash();
	m_FiringPattern.SetFiringPattern(m_FiringPatternHash);

	m_CombatMovement		= pCombatInfo->GetCombatMovement();
	m_BehaviourFlags		= pCombatInfo->GetBehaviourFlags();
	m_CombatAbility			= pCombatInfo->GetCombatAbility();
	m_AttackRanges			= pCombatInfo->GetAttackRanges();
	m_TargetLossResponse	= pCombatInfo->GetTargetLossResponse();
	m_TargetInjuredReaction = pCombatInfo->GetTargetInjuredReaction();
	SetShootRateModifier(pCombatInfo->GetWeaponShootRateModifier());
	m_fShootRateModifierThisFrame = m_fShootRateModifier;

	for(unsigned int i = 0; i < kMaxCombatFloats; i++)
	{
		m_aCombatFloats[(CombatFloats)i] = pCombatInfo->GetCombatFloat((CombatFloats)i);
	}
}

////////////////////////////////////////////////////////////////////////////////

CCombatBehaviour::CombatType CCombatBehaviour::GetCombatType( const CPed* pPed )
{
	if( !pPed )
	{
		return CCombatBehaviour::CT_OnFootArmed;
	}
	else if( pPed->GetIsSwimming() )
	{
		return CCombatBehaviour::CT_Underwater;
	}
	else if (!pPed->GetWeaponManager())
	{
		return CCombatBehaviour::CT_OnFootUnarmed;
	}
	else
	{
		weaponAssert(pPed->GetWeaponManager());
		u32 uWeaponObjectHash = pPed->GetWeaponManager()->GetEquippedWeaponObjectHash();
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponObjectHash);
		if( pPed->GetWeaponManager()->GetIsArmedGun() || pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER)
			|| (pWeaponInfo && !pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsUnarmed()))
		{
			if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover))
			{
				return CCombatBehaviour::CT_OnFootArmedSeekCoverOnly;
			}
			else
			{
				return CCombatBehaviour::CT_OnFootArmed;
			}
		}
		else
		{
			return CCombatBehaviour::CT_OnFootUnarmed;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CCombatBehaviour::GetDefaultCombatFloat(const CPed& rPed, CombatFloats eCombatFloat, float& fValue)
{
	//Ensure the ped model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!aiVerifyf(pPedModelInfo, "Ped model info is invalid."))
	{
		return false;
	}

	//Ensure the combat info is valid.
	u32 uCombatInfoHash = pPedModelInfo->GetCombatInfoHash();
	const CCombatInfo* pCombatInfo = CCombatInfoMgr::GetCombatInfoByHash(uCombatInfoHash);
	if(!taskVerifyf(pCombatInfo, "Combat info is invalid."))
	{
		return false;
	}

	//Set the value.
	fValue = pCombatInfo->GetCombatFloat(eCombatFloat);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CCombatBehaviour::SetDefaultCombatMovement(CPed& rPed)
{
	//Ensure the ped model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!aiVerifyf(pPedModelInfo, "Ped model info is invalid."))
	{
		return;
	}

	//Ensure the combat info is valid.
	u32 uCombatInfoHash = pPedModelInfo->GetCombatInfoHash();
	const CCombatInfo* pCombatInfo = CCombatInfoMgr::GetCombatInfoByHash(uCombatInfoHash);
	if(!taskVerifyf(pCombatInfo, "Combat info is invalid."))
	{
		return;
	}

	//Set the default combat movement.
	rPed.GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(pCombatInfo->GetCombatMovement());
}

////////////////////////////////////////////////////////////////////////////////

void CCombatBehaviour::SetDefaultTargetLossResponse(CPed& rPed)
{
	//Ensure the ped model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!aiVerifyf(pPedModelInfo, "Ped model info is invalid."))
	{
		return;
	}

	//Ensure the combat info is valid.
	u32 uCombatInfoHash = pPedModelInfo->GetCombatInfoHash();
	const CCombatInfo* pCombatInfo = CCombatInfoMgr::GetCombatInfoByHash(uCombatInfoHash);
	if(!taskVerifyf(pCombatInfo, "Combat info is invalid."))
	{
		return;
	}

	//Set the default target loss response.
	rPed.GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse(pCombatInfo->GetTargetLossResponse());
}

////////////////////////////////////////////////////////////////////////////////

CCombatInfoMgr CCombatInfoMgr::ms_Instance;

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::Init(unsigned UNUSED_PARAM(initMode))
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::COMBAT_BEHAVIOUR_OVERRIDE_FILE, &g_CombatInfoDataFileMounter, eDFMI_UnloadFirst);

	// Initialise pools
	CCombatInfo::InitPool( MEMBUCKET_GAMEPLAY );

	// Load
	Load();
}

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Reset();

	// Now the pools shouldn't be referenced anywhere, so shut them down
	CCombatInfo::ShutdownPool();
}

////////////////////////////////////////////////////////////////////////////////

const CCombatInfo* CCombatInfoMgr::GetCombatInfoByHash(u32 uHash)
{
	for (s32 i = 0; i < ms_Instance.m_CombatInfos.GetCount(); i++)
	{
		if (uHash == ms_Instance.m_CombatInfos[i]->GetName().GetHash())
		{
			return ms_Instance.m_CombatInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CCombatInfo* CCombatInfoMgr::GetCombatInfoByIndex(u32 uIndex)
{
	return ms_Instance.m_CombatInfos[uIndex];
}

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::Reset()
{
	// Delete each entry from each array
	for (s32 i=0; i<ms_Instance.m_CombatInfos.GetCount(); i++)
	{
		if (ms_Instance.m_CombatInfos[i])
			delete ms_Instance.m_CombatInfos[i];
	}
	ms_Instance.m_CombatInfos.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::Load()
{
	// Delete any existing data
	Reset();

	// Load
	PARSER.LoadObject("common:/data/ai/combatbehaviour", "meta", ms_Instance);
#if __BANK
	// Re-init widgets
	CCombatBehaviourDebug::InitGroup();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::Override(const char* fileName)
{
	// Delete any existing data.
	Reset();

	// Load specified data file.
	PARSER.LoadObject(fileName, "meta", ms_Instance);
}

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::Revert()
{
	// Delete any existing data
	Reset();

	// Load
	PARSER.LoadObject("common:/data/ai/combatbehaviour", "meta", ms_Instance);
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
bkCombo*				CCombatInfoMgr::ms_pCombatInfoCombo = NULL;
s32						CCombatInfoMgr::ms_iSelectedCombatInfo = 0;
atArray<const char*>	CCombatInfoMgr::ms_CombatInfoNames;

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::Save()
{
	vehicleVerifyf(PARSER.SaveObject("common:/data/ai/combatbehaviour", "meta", &ms_Instance, parManager::XML), "Failed to save combat infos");
}

////////////////////////////////////////////////////////////////////////////////

void CCombatInfoMgr::AddWidgets(bkBank& bank)
{
	ms_CombatInfoNames.clear();

	bank.PushGroup("Combat Metadata");

	// Load/Save
	bank.AddButton("Load", Load);
	bank.AddButton("Save", Save);

	// Combat Infos
	bank.PushGroup("Combat Infos");
	for(s32 i = 0; i < ms_Instance.m_CombatInfos.GetCount(); i++)
	{
		bank.PushGroup(ms_Instance.m_CombatInfos[i]->GetName().GetCStr());
		PARSER.AddWidgets(bank, ms_Instance.m_CombatInfos[i]);
		bank.PopGroup();
	}
	bank.PopGroup(); // "Combat Infos"

	for (s32 i = 0; i < CCombatInfoMgr::GetNumberOfCombatInfos(); i++)
	{
		ms_CombatInfoNames.Grow() = CCombatInfoMgr::GetCombatInfoByIndex(i)->GetName().GetCStr();
	}

	ms_pCombatInfoCombo = bank.AddCombo("Type:", &ms_iSelectedCombatInfo, ms_CombatInfoNames.GetCount(), &ms_CombatInfoNames[0], NullCB, "Combat info selection");
	bank.AddButton("Give Ped Selected Combat Info", &GiveFocusPedSelectedCombatInfo);

	bank.PopGroup(); // "Combat Metadata"
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CCombatInfoMgr::GiveFocusPedSelectedCombatInfo()
{
	CPed* pFocusPed = NULL;

	// Get the focus entities
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);

	// Select the ped from the first focus entity
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity1);
	}

	if (pFocusPed)
	{
		pFocusPed->GetPedIntelligence()->GetCombatBehaviour().InitFromCombatData(atHashWithStringNotFinal(ms_CombatInfoNames[ms_iSelectedCombatInfo]));
	}
}

////////////////////////////////////////////////////////////////////////////////

bkBank* CCombatBehaviourDebug::ms_pBank = NULL;

void CCombatBehaviourDebug::InitGroup()
{
	ms_pBank = BANKMGR.FindBank("A.I.");

	if (ms_pBank)
	{
		ShutdownGroup();
	}
	else
	{
		CPedDebugVisualiserMenu::InitBank();
		ms_pBank = BANKMGR.FindBank("A.I.");
	}

	if(aiVerifyf(ms_pBank, "Failed to create A.I bank"))
	{
		ms_pBank->AddButton("Create combat behaviour widgets", datCallback(CFA1(CCombatBehaviourDebug::AddWidgets), ms_pBank));
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCombatBehaviourDebug::AddWidgets(bkBank& bank)
{
	// destroy first widget which is the create button
	bkWidget* pWidget = BANKMGR.FindWidget("A.I./Create combat behaviour widgets");
	if(pWidget == NULL)
		return;
	pWidget->Destroy();

	// Inventory widgets
	CCombatInfoMgr::AddWidgets(bank);
}

////////////////////////////////////////////////////////////////////////////////

void CCombatBehaviourDebug::ShutdownGroup()
{
	if(ms_pBank)
	{
		bkWidget* pWidget = BANKMGR.FindWidget("A.I./Combat Metadata");
		if(pWidget == NULL)
			return;
		pWidget->Destroy();
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
