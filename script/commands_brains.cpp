// Rage headers
#include "script/wrapper.h"
// Game headers
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "objects/Object.h"
#include "peds/ped.h"
#include "network/NetworkInterface.h"
#include "scene/world/gameWorld.h"
#include "scene/worldpoints.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "script/script_brains.h"
#include "script/streamedscripts.h"
#include "control/replay/ReplaySettings.h"


namespace brain_commands
{

void CommandAllocateScriptToRandomPed(const char* pScriptName, int PedModelHashKey, int PercentageChance, bool UNUSED_PARAM(bScenarioPedsOnly))
{
    // this script command is not approved for use in network scripts
    if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "ADD_SCRIPT_TO_RANDOM_PED - This script command is not allowed in network game scripts!"))
	{
		fwModelId PedModelId;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) PedModelHashKey, &PedModelId);		//	ignores return value
		if(SCRIPT_VERIFY( PedModelId.IsValid(), "ADD_SCRIPT_TO_RANDOM_PED - this is not a valid model index"))
		{
			s32 ScriptId = g_StreamedScripts.FindSlot(pScriptName).Get();
			if (scriptVerifyf(ScriptId != -1, "%s: ADD_SCRIPT_TO_RANDOM_PED - script.rpf doesn't contain a file called %s so this brain won't be registered", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptName))
			{
				CTheScripts::GetScriptsForBrains().AddScriptBrainForEntity(strStreamingObjectNameString(pScriptName), PedModelHashKey, static_cast<u8>(PercentageChance), CScriptsForBrains::PED_STREAMED, -1, -1.0f, 0);
			}
		}
	}
}

void CommandRegisterObjectScriptBrain(const char* pScriptName, int ObjectModelHashKey, int PercentageChance, float fActivationRange, int ObjectGroupingID, s32 SetToWhichThisBrainBelongs)
{
	scriptDisplayf("REGISTER_OBJECT_SCRIPT_BRAIN - script name %s model hash = %d", pScriptName, ObjectModelHashKey);

	if(SCRIPT_VERIFY( ObjectModelHashKey != 0, "REGISTER_OBJECT_SCRIPT_BRAIN - this is not a valid model hash key"))
	{
		s32 ScriptId = g_StreamedScripts.FindSlot(pScriptName).Get();
		if (scriptVerifyf(ScriptId != -1, "%s: REGISTER_OBJECT_SCRIPT_BRAIN - script.rpf doesn't contain a file called %s so this brain won't be registered", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptName))
		{
			CTheScripts::GetScriptsForBrains().AddScriptBrainForEntity(strStreamingObjectNameString(pScriptName), ObjectModelHashKey, static_cast<u8>(PercentageChance), CScriptsForBrains::OBJECT_STREAMED, static_cast<s8>(ObjectGroupingID), fActivationRange, static_cast<u32>(SetToWhichThisBrainBelongs));
		}
	}
}


bool CommandIsObjectWithinBrainActivationRange(int ObjectIndex)
{
    bool LatestCmpFlagResult = false;
//	if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "IS_OBJECT_WITHIN_BRAIN_ACTIVATION_RANGE - This script command is not allowed in network game scripts!"))
	{
		const CObject *pObject = CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectIndex);

		if(SCRIPT_VERIFY(pObject, "IS_OBJECT_WITHIN_BRAIN_ACTIVATION_RANGE - Object doesn't exist"))
		{
			if ( CGameWorld::FindLocalPlayer() && pObject)
			{
				const Vector3 PlayerPos = CGameWorld::FindLocalPlayerCentreOfWorld();
				if (CTheScripts::GetScriptsForBrains().IsObjectWithinActivationRange(pObject, PlayerPos))
				{
					LatestCmpFlagResult = true;
				}
			}
		}
	}
	return (LatestCmpFlagResult);
}


void CommandRegisterWorldPointScriptBrain(const char* pScriptName, float fActivationRange, s32 SetToWhichThisBrainBelongs)
{
	if(SCRIPT_VERIFY(fActivationRange < WORLDPOINT_SCAN_RANGE, "REGISTER_WORLD_POINT_SCRIPT_BRAIN - ActivationRange must be less than WORLDPOINT_SCAN_RANGE (210.0 metres)"))
	{
		s32 ScriptId = g_StreamedScripts.FindSlot(pScriptName).Get();
		if (scriptVerifyf(ScriptId != -1, "%s: REGISTER_WORLD_POINT_SCRIPT_BRAIN - script.rpf doesn't contain a file called %s so this brain won't be registered", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptName))
		{
			CTheScripts::GetScriptsForBrains().AddScriptBrainForWorldPoint(strStreamingObjectNameString(pScriptName), CScriptsForBrains::WORLD_POINT, fActivationRange, static_cast<u32>(SetToWhichThisBrainBelongs));
		}
	}
}


bool CommandIsWorldPointWithinBrainActivationRange()
{

#if GTA_REPLAY
	// Always out of range for replay
	if( CReplayMgr::IsReplayInControlOfWorld() == false )
#endif	//GTA_REPLAY
	{
	//	if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "IS_WORLD_POINT_WITHIN_BRAIN_ACTIVATION_RANGE - This script command is not allowed in network game scripts!"))
		{
			int WorldPointIndex = CTheScripts::GetCurrentGtaScriptThread()->EntityIndexForScriptBrain;
			if(SCRIPT_VERIFY(WorldPointIndex >= 0 && WorldPointIndex < CModelInfo::GetWorldPointStore().GetCount(fwFactory::GLOBAL), "Value of EntityIndexForScriptBrain is bogus!"))
			{
				C2dEffect *pWorldPointRunningCurrentScript = &CModelInfo::GetWorldPointStore().GetEntry(fwFactory::GLOBAL, WorldPointIndex);//CWorldPoints::GetWorldPointFromIndex(WorldPointIndex);

				if ( CGameWorld::FindLocalPlayer() && pWorldPointRunningCurrentScript) 
				{
					const Vector3 PlayerPos = CGameWorld::FindLocalPlayerCentreOfWorld();

					if (CTheScripts::GetScriptsForBrains().IsWorldPointWithinActivationRange(pWorldPointRunningCurrentScript, PlayerPos) )
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}


void CommandEnableScriptBrainSet(s32 set)
{
	CTheScripts::GetScriptsForBrains().EnableBrainSet(static_cast<u32>(set));
}

void CommandDisableScriptBrainSet(s32 set)
{
	CTheScripts::GetScriptsForBrains().DisableBrainSet(static_cast<u32>(set));
}

//	I think REACTIVATE_ALL_WORLD_POINT_BRAINS_THAT_ARE_WAITING_TILL_OUT_OF_RANGE is too long a name for the script
//	compiler to deal with so I've removed "POINT_"
void CommandReactivateAllWorldBrainsThatAreWaitingTillOutOfRange()
{
	CWorldPoints::ReactivateAllWorldPointBrainsThatAreWaitingTillOutOfRange();
}

void CommandReactivateAllObjectBrainsThatAreWaitingTillOutOfRange()
{
	CTheScripts::GetScriptBrainDispatcher().ReactivateAllObjectBrainsThatAreWaitingTillOutOfRange();
}

void CommandReactivateNamedWorldBrainsWaitingTillOutOfRange(const char *pScriptName)
{
	CWorldPoints::ReactivateNamedWorldPointBrainsWaitingTillOutOfRange(pScriptName);
}

void CommandReactivateNamedObjectBrainsWaitingTillOutOfRange(const char *pScriptName)
{
	CTheScripts::GetScriptBrainDispatcher().ReactivateNamedObjectBrainsWaitingTillOutOfRange(pScriptName);
}


#if __BANK
	void CommandDebugApplyScriptBrainOnObject(int ObjectIndex)
#else
	void CommandDebugApplyScriptBrainOnObject(int )
#endif //__BANK
	{
#if __BANK
		// this script command is not approved for use in network scripts
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "IS_WORLD_POINT_WITHIN_BRAIN_ACTIVATION_RANGE - This script command is not allowed in network game scripts!"))
		{
			CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

			if(pObject)
			{
				CTheScripts::GetScriptsForBrains().CheckAndApplyIfNewEntityNeedsScript(pObject, CScriptsForBrains::OBJECT_STREAMED);
			}
		}
#endif // __BANK
	}


	void CommandRegisterScriptBrainForScenarioPeds(const char *pScriptName, const char *pScenarioName, int PercentageChance, float fActivationRange, bool UNUSED_PARAM(bSafeForNetworkGame))
	{
		// this script command is not approved for use in network scripts yet
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "REGISTER_SCRIPT_BRAIN_FOR_SCENARIO_PEDS - This script command is not allowed in network game scripts yet! It takes a bSafeForNetworkGame parameter in case it is allowed in network games later"))
		{
			s32 ScriptId = g_StreamedScripts.FindSlot(pScriptName).Get();
			if (scriptVerifyf(ScriptId != -1, "%s: REGISTER_SCRIPT_BRAIN_FOR_SCENARIO_PEDS - script.rpf doesn't contain a file called %s so this brain won't be registered", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptName))
			{
				CTheScripts::GetScriptsForBrains().AddScriptBrainForScenarioPed(strStreamingObjectNameString(pScriptName), pScenarioName, static_cast<u8>(PercentageChance), fActivationRange, 0);
			}
		}
	}


	bool CommandIsScenarioPedWithinBrainActivationRange(int PedIndex, float fRangeExtension)
	{
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "IS_SCENARIO_PED_WITHIN_BRAIN_ACTIVATION_RANGE - This script command is not allowed in network game scripts!"))
		{
			const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				if (CGameWorld::FindLocalPlayer())
				{
					const Vector3 PlayerPos = CGameWorld::FindLocalPlayerCentreOfWorld();
					if (CTheScripts::GetScriptsForBrains().IsScenarioPedWithinActivationRange(pPed, PlayerPos, fRangeExtension))
					{
						return true;
					}
				}
			}
		}
		return false;
	}


	void CommandDontRetriggerBrainForThisScenarioPed(int PedIndex)
	{
		if(SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "DONT_RETRIGGER_BRAIN_FOR_THIS_SCENARIO_PED - This script command is not allowed in network game scripts!"))
		{
			CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
			if(pPed)
			{
				CTheScripts::GetScriptsForBrains().RemoveScenarioPedFromBrainDispatcherArray(pPed);
			}
		}
	}

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(ADD_SCRIPT_TO_RANDOM_PED,0xdf6f66b0424a2fb3, CommandAllocateScriptToRandomPed);

		SCR_REGISTER_SECURE(REGISTER_OBJECT_SCRIPT_BRAIN,0x59c0d9624ee20434, CommandRegisterObjectScriptBrain);
		SCR_REGISTER_SECURE(IS_OBJECT_WITHIN_BRAIN_ACTIVATION_RANGE,0xa7fb445015a2af1c, CommandIsObjectWithinBrainActivationRange);

		SCR_REGISTER_SECURE(REGISTER_WORLD_POINT_SCRIPT_BRAIN,0x9863a0ffed964a8b, CommandRegisterWorldPointScriptBrain);
		SCR_REGISTER_SECURE(IS_WORLD_POINT_WITHIN_BRAIN_ACTIVATION_RANGE,0xc92f9f18e020b461, CommandIsWorldPointWithinBrainActivationRange);

		SCR_REGISTER_SECURE(ENABLE_SCRIPT_BRAIN_SET,0x1b776653e338e92f, CommandEnableScriptBrainSet);
		SCR_REGISTER_SECURE(DISABLE_SCRIPT_BRAIN_SET,0xbce91688f4863b3c, CommandDisableScriptBrainSet);

		SCR_REGISTER_SECURE(REACTIVATE_ALL_WORLD_BRAINS_THAT_ARE_WAITING_TILL_OUT_OF_RANGE,0xe8fd65c4cf1171e3, CommandReactivateAllWorldBrainsThatAreWaitingTillOutOfRange);
		SCR_REGISTER_SECURE(REACTIVATE_ALL_OBJECT_BRAINS_THAT_ARE_WAITING_TILL_OUT_OF_RANGE,0x683628636cbf3c22, CommandReactivateAllObjectBrainsThatAreWaitingTillOutOfRange);

		SCR_REGISTER_SECURE(REACTIVATE_NAMED_WORLD_BRAINS_WAITING_TILL_OUT_OF_RANGE,0xc2f690e5f28eef10, CommandReactivateNamedWorldBrainsWaitingTillOutOfRange);
		SCR_REGISTER_SECURE(REACTIVATE_NAMED_OBJECT_BRAINS_WAITING_TILL_OUT_OF_RANGE,0x25d05e6515ade956, CommandReactivateNamedObjectBrainsWaitingTillOutOfRange);

		SCR_REGISTER_UNUSED(DEBUG_APPLY_SCRIPT_BRAIN_ON_OBJECT,0x88dfc11056976551, CommandDebugApplyScriptBrainOnObject);

		SCR_REGISTER_UNUSED(REGISTER_SCRIPT_BRAIN_FOR_SCENARIO_PEDS,0xe33fe82738acc589, CommandRegisterScriptBrainForScenarioPeds);
		SCR_REGISTER_UNUSED(IS_SCENARIO_PED_WITHIN_BRAIN_ACTIVATION_RANGE,0x1bb1d7346368b89b, CommandIsScenarioPedWithinBrainActivationRange);
		SCR_REGISTER_UNUSED(DONT_RETRIGGER_BRAIN_FOR_THIS_SCENARIO_PED,0xf0fb7f52ef584ee6, CommandDontRetriggerBrainForThisScenarioPed);
	}

}	//	end of namespace brain_commands
