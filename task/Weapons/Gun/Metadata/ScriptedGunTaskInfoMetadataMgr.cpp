//
// Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h"

// Rage Headers
#include "bank/bkmgr.h"
#include "fwanimation/clipsets.h"
#include "parser/manager.h"

// Parser Header
#include "ScriptedGunTaskInfoMetadataMgr_parser.h"

// Game Headers
#include "Debug/DebugScene.h"
#include "Event/Events.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Scene/World/GameWorld.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskAimGunScripted.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfo.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

CScriptedGunTaskMetadataMgr CScriptedGunTaskMetadataMgr::ms_Instance;

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskMetadataMgr::Init(unsigned UNUSED_PARAM(initMode))
{
	// Load
	Load();
	ms_Instance.m_bIsInitialised = true;
}

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskMetadataMgr::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Reset();
	ms_Instance.m_bIsInitialised = false;
}

////////////////////////////////////////////////////////////////////////////////

const CScriptedGunTaskInfo* CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(u32 uHash)
{
	for (s32 i = 0; i < ms_Instance.m_ScriptedGunTaskInfos.GetCount(); i++)
	{
		if (uHash == ms_Instance.m_ScriptedGunTaskInfos[i]->GetName().GetHash())
		{
			return ms_Instance.m_ScriptedGunTaskInfos[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CScriptedGunTaskInfo* CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByIndex(u32 uIndex)
{
	if (aiVerifyf(uIndex < ms_Instance.m_ScriptedGunTaskInfos.GetCount(), "Invalid index"))
	{
		return ms_Instance.m_ScriptedGunTaskInfos[uIndex];
	}
	return NULL;
}


////////////////////////////////////////////////////////////////////////////////

u32 CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoHashFromIndex(s32 iIndex)
{
	const CScriptedGunTaskInfo* pScriptedGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByIndex(iIndex);
	if (aiVerifyf(pScriptedGunTaskInfo, "Couldn't find info for index %i", iIndex))
	{
		return pScriptedGunTaskInfo->GetName().GetHash();
	}
	return 0; 
}

////////////////////////////////////////////////////////////////////////////////

CScriptedGunTaskMetadataMgr::CScriptedGunTaskMetadataMgr()
{
	m_bIsInitialised = false;
}

////////////////////////////////////////////////////////////////////////////////

CScriptedGunTaskMetadataMgr::~CScriptedGunTaskMetadataMgr()
{
}

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskMetadataMgr::Reset()
{
	// Delete each entry from each array
	for (s32 i=0; i<ms_Instance.m_ScriptedGunTaskInfos.GetCount(); i++)
	{
		if (ms_Instance.m_ScriptedGunTaskInfos[i])
			delete ms_Instance.m_ScriptedGunTaskInfos[i];
	}
	ms_Instance.m_ScriptedGunTaskInfos.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskMetadataMgr::Load()
{
	// Delete any existing data
	Reset();

	// Load
	PARSER.LoadObject("common:/data/ai/scriptedguntasks", "meta", ms_Instance);

#if __BANK
	CScriptedGunTaskDebug::InitBank();
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CScriptedGunTaskMetadataMgr::Save()
{
	vehicleVerifyf(PARSER.SaveObject("common:/data/ai/scriptedguntasks", "meta", &ms_Instance, parManager::XML), "Failed to save scripted gun task infos");
}

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskMetadataMgr::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Scripted Gun Task Metadata");

	// Load/Save
	bank.AddButton("Load", Load);
	bank.AddButton("Save", Save);

	// Scripted Gun Task Infos
	bank.PushGroup("Scripted Gun Task Infos");
	for(s32 i = 0; i < ms_Instance.m_ScriptedGunTaskInfos.GetCount(); i++)
	{
		bank.PushGroup(ms_Instance.m_ScriptedGunTaskInfos[i]->GetName().GetCStr());
		PARSER.AddWidgets(bank, ms_Instance.m_ScriptedGunTaskInfos[i]);
		bank.PopGroup();
	}
	bank.PopGroup(); // "Scripted Gun Task Infos"

	bank.PopGroup(); // "Scripted Gun Task Metadata"
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK

bkBank*		CScriptedGunTaskDebug::ms_pBank = NULL;
bkButton*	CScriptedGunTaskDebug::ms_pCreateButton = NULL;
bkCombo*	CScriptedGunTaskDebug::ms_pTasksCombo = NULL;

s32 CScriptedGunTaskDebug::ms_iSelectedGunTask = 0;

atArray<const char*> CScriptedGunTaskDebug::ms_ScriptedGunTaskNames;

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskDebug::InitBank()
{
	if (ms_pBank)
	{
		ShutdownBank();
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("Scripted Gun Tasks", 0, 0, false); 
	if (aiVerifyf(ms_pBank, "Failed to create scripted gun task bank"))
	{
		ms_pCreateButton = ms_pBank->AddButton("Create scripted gun task widgets", datCallback(CFA1(CScriptedGunTaskDebug::AddWidgets), ms_pBank));
	}
}

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskDebug::ShutdownBank()
{
	if (ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskDebug::AddWidgets()
{
	// destroy first widget which is the create button
	if (!ms_pCreateButton)
		return;

	ms_pCreateButton->Destroy();
	ms_pCreateButton = NULL;

	for (s32 i = 0; i < CScriptedGunTaskMetadataMgr::GetNumberOfScriptedGunTaskInfos(); i++)
	{
		ms_ScriptedGunTaskNames.Grow() = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByIndex(i)->GetName().GetCStr();
	}

	ms_pTasksCombo = ms_pBank->AddCombo("Type:", &ms_iSelectedGunTask, ms_ScriptedGunTaskNames.GetCount(), &ms_ScriptedGunTaskNames[0], NullCB, "Scripted Gun Task harness selection");
	ms_pBank->AddButton("Give Ped Scripted Gun Task", &GivePedTaskCB);

	CTaskAimGunScripted::AddWidgets(*ms_pBank);

	CScriptedGunTaskMetadataMgr::AddWidgets(*ms_pBank);
}

////////////////////////////////////////////////////////////////////////////////

void CScriptedGunTaskDebug::GivePedTaskCB()
{
	CPed* pFocusPed = NULL;

	// Get the focus entities
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);

	// Select the ped from the first focus entity
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity1);
	}

	// If still no success, use the player
	if (!pFocusPed)
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if (pFocusPed)
	{
		CTask* pTaskToGiveToFocusPed = rage_new CTaskGun(CWeaponController::WCT_Player, CTaskTypes::TASK_AIM_GUN_SCRIPTED, CWeaponTarget(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition())));
		const CScriptedGunTaskInfo* pGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByIndex(ms_iSelectedGunTask);
		if (pGunTaskInfo)
		{
			CTaskGun* pGunTask = static_cast<CTaskGun*>(pTaskToGiveToFocusPed);
			pGunTask->SetScriptedGunTaskInfoHash(pGunTaskInfo->GetName().GetHash());
			pGunTask->GetGunFlags().SetFlag(GF_ForceAimState);
			pGunTask->GetGunFlags().SetFlag(GF_DisableBlockingClip);
			pGunTask->GetGunFlags().SetFlag(GF_DisableTorsoIk);	
			CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTaskToGiveToFocusPed, false, E_PRIORITY_GIVE_PED_TASK);
			pFocusPed->GetPedIntelligence()->AddEvent(event);
		}
		else
		{
			delete pTaskToGiveToFocusPed;
			pTaskToGiveToFocusPed = NULL;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

#endif // __BANK