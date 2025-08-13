#include "script/script_brains.h"

// Rage headers

// Game headers
#include "control/gamelogic.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "network/NetworkInterface.h"
#include "peds/ped.h"
#include "scene/world/GameWorld.h"
#include "scene/worldpoints.h"
#include "script/script.h"
#include "script/script_helper.h"
#include "script/streamedscripts.h"
#include "streaming/streamingvisualize.h"
#include "debug/debugglobals.h"
#include "scene/DataFileMgr.h"
#include "control/replay/ReplaySettings.h"

#define DEFAULT_OBJECT_ACTIVATION_RANGE (5.0f)

class CScriptBrainFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		switch(file.m_fileType)
		{
			case CDataFileMgr::SCRIPT_BRAIN_FILE: CTheScripts::GetScriptsForBrains().AddScriptBrainsFromFile(file.m_filename); break;
			default: return false;
		}
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		switch(file.m_fileType)
		{
			case CDataFileMgr::SCRIPT_BRAIN_FILE: CTheScripts::GetScriptsForBrains().RemoveScriptBrainsFromFile(file.m_filename); break;
			default: break;
		}
	}
} g_ScriptBrainFileMounter;

//	start of script brain functions

void CScriptsForBrains::script_brain_struct::Initialise()
{
	m_ScriptFileName.Clear();
	m_fBrainActivationRange = DEFAULT_OBJECT_ACTIVATION_RANGE;
	m_ScenarioName.Clear();	//	Used by Scenario Ped brains
	m_ModelHashkey = 0;
	m_TypeOfBrain = -1;
	m_ObjectGroupingID = -1;
	m_PercentageChance = 0;
	m_bBrainActive = true;
	m_SetToWhichThisBrainBelongs = 0;
//	bOnlyGiveBrainToScenarioPeds = false;
}

void CScriptsForBrains::AddScriptBrainsFromFile(const char* filePath)
{
	sScriptBrainLists brainsToLoad;

	if (PARSER.LoadObject(filePath, NULL, brainsToLoad))
	{
		for (u32 i = 0; i < brainsToLoad.m_worldPointBrains.GetCount(); i++)
		{
			sWorldPointScriptBrains& currBrain = brainsToLoad.m_worldPointBrains[i];

			AddScriptBrainForWorldPoint(strStreamingObjectNameString(currBrain.m_ScriptName),
				CScriptsForBrains::WORLD_POINT, currBrain.m_ActivationRange, static_cast<u32>(currBrain.m_setToWhichThisBrainBelongs));
		}
	}
}

void CScriptsForBrains::RemoveScriptBrainsFromFile(const char* filePath)
{
	sScriptBrainLists brainsToLoad;

	if (PARSER.LoadObject(filePath, NULL, brainsToLoad))
	{
		for (u32 i = 0; i < brainsToLoad.m_worldPointBrains.GetCount(); i++)
		{
			sWorldPointScriptBrains& currBrain = brainsToLoad.m_worldPointBrains[i];
			s16 brainIndex = (s16)GetIndexOfScriptBrainWithThisName(strStreamingObjectName(currBrain.m_ScriptName.GetHash()), 
				CScriptsForBrains::WORLD_POINT);

			CWorldPoints::RemoveScriptBrain(brainIndex);
			ResetSlot(brainIndex);
		}
	}
}

void CScriptsForBrains::Init(void)
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::SCRIPT_BRAIN_FILE, &g_ScriptBrainFileMounter, eDFMI_UnloadFirst);

	for (u32 loop = 0; loop < MAX_DIFFERENT_SCRIPT_BRAINS; loop++)
	{
		m_ScriptBrainArray[loop].Initialise();
	}

	m_SetsEnabled = 0;

//	bEnableBrainAllocation = true;	//	needed when getclosestped style commands convert a dummy ped to a real ped
}

//	void CScriptsForBrains::SetEnableBrainAllocationFlag(bool bEnableAllocation)
//	{
//		bEnableBrainAllocation = bEnableAllocation;
//	}

//	This is used to turn off all the casino object brains in Paul's Heist9 mission
/*
void CScriptsForBrains::SwitchAllObjectBrainsWithThisID(int object_grouping_id, bool bBrainOn)
{
	if (object_grouping_id < 0)
		return;
	
	for (u32 loop = 0; loop < MAX_DIFFERENT_SCRIPT_BRAINS; loop++)
	{
		if (m_ScriptBrainArray[loop].m_ObjectGroupingID == object_grouping_id)
		{
			m_ScriptBrainArray[loop].m_bBrainActive = bBrainOn;
		}
	}
}
*/


void CScriptsForBrains::EnableBrainSet(u32 setToEnable)
{
	Assertf(setToEnable > 0 && setToEnable <= (1 << 31), "CScriptsForBrains::EnableBrainSet - brain set can only be 1, 2, 4, 8, ... 2147483648");
	m_SetsEnabled |= setToEnable;
	scriptDebugf2("CScriptsForBrains::EnableBrainSet - called for %u. m_SetsEnabled is now %u", setToEnable, m_SetsEnabled);
}

void CScriptsForBrains::DisableBrainSet(u32 setToDisable)
{
	Assertf(setToDisable > 0 && setToDisable <= (1 << 31), "CScriptsForBrains::DisableBrainSet - brain set can only be 1, 2, 4, 8, ... 2147483648");
	m_SetsEnabled &= ~setToDisable;
	scriptDebugf2("CScriptsForBrains::DisableBrainSet - called for %u. m_SetsEnabled is now %u", setToDisable, m_SetsEnabled);
}

void CScriptsForBrains::ResetSlot(s32 index)
{
	if (index >= 0 && index < MAX_DIFFERENT_SCRIPT_BRAINS)
	{
		m_ScriptBrainArray[index].Initialise();
	}
}

s32 CScriptsForBrains::FindFreeSlot()
{
	s32 loop = 0;
	while (loop < MAX_DIFFERENT_SCRIPT_BRAINS)
	{
		if (m_ScriptBrainArray[loop].m_ScriptFileName.GetLength() == 0)
		{
			return loop;
		}
		loop++;
	}
	return -1;
}

void CScriptsForBrains::AddScriptBrainForEntity(const strStreamingObjectNameString pScriptName, u32 EntityModelHashkey, u8 Percentage, s8 BrainType, s8 object_grouping_id, float ObjectActivationRange, u32 SetToWhichThisBrainBelongs)
{
	scriptAssertf( (Percentage > 0) && (Percentage <= 100), "CScriptsForBrains::AddScriptBrainForEntity - Percentage must be greater than 0 and less than 101");

	s32 free_slot = FindFreeSlot();

	if (scriptVerifyf(free_slot >= 0, "CScriptsForBrains::AddScriptBrainForEntity - No more space for script brains"))
	{
		m_ScriptBrainArray[free_slot].m_ScriptFileName = pScriptName;

		if (ObjectActivationRange <= 0.0f)
		{
			m_ScriptBrainArray[free_slot].m_fBrainActivationRange = DEFAULT_OBJECT_ACTIVATION_RANGE;
		}
		else
		{
			m_ScriptBrainArray[free_slot].m_fBrainActivationRange = ObjectActivationRange;
		}
		m_ScriptBrainArray[free_slot].m_ScenarioName.Clear();	//	Only used by Scenario Ped brains

		m_ScriptBrainArray[free_slot].m_ModelHashkey= EntityModelHashkey;
		m_ScriptBrainArray[free_slot].m_TypeOfBrain = BrainType;
		m_ScriptBrainArray[free_slot].m_ObjectGroupingID = object_grouping_id;
		m_ScriptBrainArray[free_slot].m_PercentageChance = Percentage;
		m_ScriptBrainArray[free_slot].m_bBrainActive = true;
		m_ScriptBrainArray[free_slot].m_SetToWhichThisBrainBelongs = SetToWhichThisBrainBelongs;
//		m_ScriptBrainArray[free_slot].bOnlyGiveBrainToScenarioPeds = bScenarioPedsOnly;
		
#if __ASSERT
		u32 TotalPercentage = 0;
		for (u32 loop = 0; loop < MAX_DIFFERENT_SCRIPT_BRAINS; loop++)
		{
			if ( (m_ScriptBrainArray[loop].m_ModelHashkey == EntityModelHashkey) && (m_ScriptBrainArray[loop].m_TypeOfBrain == BrainType) )
			{
				TotalPercentage += m_ScriptBrainArray[loop].m_PercentageChance;
			}
		}
		scriptAssertf( (TotalPercentage > 0) && (TotalPercentage < 101), "CScriptsForBrains::AddScriptBrainForEntity - Total percentage for this ped is out of range");
#endif	//	 __ASSERT

#if __DEV
		u32 totalNumberOfUsedBrains = 0;
		for (u32 loop = 0; loop < MAX_DIFFERENT_SCRIPT_BRAINS; loop++)
		{
			if (m_ScriptBrainArray[loop].m_ScriptFileName.GetLength() > 0)
			{
				totalNumberOfUsedBrains++;
			}
		}
		scriptDisplayf("CScriptsForBrains::AddScriptBrainForEntity - Added %s Total number of used brains = %u", pScriptName.GetCStr(), totalNumberOfUsedBrains);
#endif	//	__DEV
	}
}


void CScriptsForBrains::AddScriptBrainForWorldPoint(const strStreamingObjectNameString pScriptName, s8 BrainType, float ActivationRange, u32 SetToWhichThisBrainBelongs)
{
	s32 free_slot = FindFreeSlot();

	if(scriptVerifyf(free_slot >= 0, "CScriptsForBrains::AddScriptBrainForWorldPoint - No more space for script brains"))
	{
		m_ScriptBrainArray[free_slot].m_ScriptFileName = pScriptName;
		m_ScriptBrainArray[free_slot].m_fBrainActivationRange = ActivationRange;
		m_ScriptBrainArray[free_slot].m_ScenarioName.Clear();	//	Only used by Scenario Ped brains
		m_ScriptBrainArray[free_slot].m_ModelHashkey = 0;
		m_ScriptBrainArray[free_slot].m_TypeOfBrain = BrainType;
		m_ScriptBrainArray[free_slot].m_ObjectGroupingID = -1;
		m_ScriptBrainArray[free_slot].m_PercentageChance = 0;
		m_ScriptBrainArray[free_slot].m_bBrainActive = true;
		m_ScriptBrainArray[free_slot].m_SetToWhichThisBrainBelongs = SetToWhichThisBrainBelongs;
//		m_ScriptBrainArray[free_slot].bOnlyGiveBrainToScenarioPeds = false;

#if __DEV
		u32 totalNumberOfUsedBrains = 0;
		for (u32 loop = 0; loop < MAX_DIFFERENT_SCRIPT_BRAINS; loop++)
		{
			if (m_ScriptBrainArray[loop].m_ScriptFileName.GetLength() > 0)
			{
				totalNumberOfUsedBrains++;
			}
		}
		scriptDisplayf("CScriptsForBrains::AddScriptBrainForWorldPoint - Added %s Total number of used brains = %u", pScriptName.GetCStr(), totalNumberOfUsedBrains);
#endif	//	__DEV
	}
}

void CScriptsForBrains::AddScriptBrainForScenarioPed(const strStreamingObjectNameString pScriptName, const atHashWithStringNotFinal ScenarioName, u8 Percentage, float fActivationRange, u32 SetToWhichThisBrainBelongs)
{
	scriptAssertf( (Percentage > 0) && (Percentage <= 100), "CScriptsForBrains::AddScriptBrainForScenarioPed - Percentage must be greater than 0 and less than 101");

	s32 free_slot = FindFreeSlot();

	if(scriptVerifyf(free_slot >= 0, "CScriptsForBrains::AddScriptBrainForScenarioPed - No more space for script brains"))
	{
		m_ScriptBrainArray[free_slot].m_ScriptFileName = pScriptName;
		if (fActivationRange <= 0.0f)
		{
			m_ScriptBrainArray[free_slot].m_fBrainActivationRange = DEFAULT_OBJECT_ACTIVATION_RANGE;
		}
		else
		{
			m_ScriptBrainArray[free_slot].m_fBrainActivationRange = fActivationRange;
		}

		m_ScriptBrainArray[free_slot].m_ScenarioName = ScenarioName;
		m_ScriptBrainArray[free_slot].m_ModelHashkey = 0;
		m_ScriptBrainArray[free_slot].m_TypeOfBrain = SCENARIO_PED;
		m_ScriptBrainArray[free_slot].m_ObjectGroupingID = -1;
		m_ScriptBrainArray[free_slot].m_PercentageChance = Percentage;
		m_ScriptBrainArray[free_slot].m_bBrainActive = true;
		m_ScriptBrainArray[free_slot].m_SetToWhichThisBrainBelongs = SetToWhichThisBrainBelongs;
//		m_ScriptBrainArray[free_slot].bOnlyGiveBrainToScenarioPeds = false;

#if __DEV
		u32 totalNumberOfUsedBrains = 0;
		for (u32 loop = 0; loop < MAX_DIFFERENT_SCRIPT_BRAINS; loop++)
		{
			if (m_ScriptBrainArray[loop].m_ScriptFileName.GetLength() > 0)
			{
				totalNumberOfUsedBrains++;
			}
		}
		scriptDisplayf("CScriptsForBrains::AddScriptBrainForScenarioPed - Added %s Total number of used brains = %u", pScriptName.GetCStr(), totalNumberOfUsedBrains);
#endif	//	__DEV
	}
}


void CScriptsForBrains::CheckAndApplyIfNewEntityNeedsScript(CEntity *pNewEntity, s8 BrainType)
{
	scriptAssertf( (BrainType == PED_STREAMED) || (BrainType == OBJECT_STREAMED), "CScriptsForBrains::CheckAndApplyIfNewEntityNeedsScript - can only call for PED_STREAMED, OBJECT_STREAMED");

	if ( BrainType == PED_STREAMED )
	{
		scriptAssertf( ((CPed *)pNewEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain ) == false, "CScriptsForBrains::CheckAndApplyIfNewEntityNeedsScript - can't start a second script brain for ped");
		scriptAssertf( ((CPed *)pNewEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad ) == false, "CScriptsForBrains::CheckAndApplyIfNewEntityNeedsScript - already waiting for a streamed script brain to load for this ped");
		if ( (((CPed *)pNewEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain )) || (((CPed *)pNewEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad )) )
		{
			return;
		}

//		bPedCreatedForScenario = ((CPed *)pNewEntity)->PopTypeGet() == POPTYPE_RANDOM_SCENARIO;
	}
	else
	{
		scriptAssertf( ((CObject *)pNewEntity)->m_nObjectFlags.ScriptBrainStatus == CObject::OBJECT_DOESNT_USE_SCRIPT_BRAIN, "CScriptsForBrains::CheckAndApplyIfNewEntityNeedsScript - this object has already been set up with a script brain");
		if (((CObject *)pNewEntity)->m_nObjectFlags.ScriptBrainStatus != CObject::OBJECT_DOESNT_USE_SCRIPT_BRAIN)
		{
			return;
		}
	}

	s32 scriptBrainIndexFound = -1;
	u32 loop = 0;
	while(loop < MAX_DIFFERENT_SCRIPT_BRAINS)
	{
		bool bMatch = false;
		if( (m_ScriptBrainArray[loop].m_TypeOfBrain == BrainType) &&
			(pNewEntity->GetBaseModelInfo() != NULL) &&
			(m_ScriptBrainArray[loop].m_ModelHashkey == pNewEntity->GetBaseModelInfo()->GetModelNameHash()) )
		{
			bMatch = true;
		}

		if (bMatch)
		{
			s32 RandomInt = fwRandom::GetRandomNumberInRange(0, 100);
			if (RandomInt < m_ScriptBrainArray[loop].m_PercentageChance)
			{
				scriptBrainIndexFound = loop;
			}
		}
		loop++;
	}

	if(scriptBrainIndexFound >= 0)
	{
		StartOrRequestNewStreamedScriptBrain(scriptBrainIndexFound, pNewEntity, BrainType, true);
	}
}

s32 CScriptsForBrains::GetBrainIndexForNewScenarioPed(u32 HashOfScenarioName)
{
	u32 loop = 0;
	while(loop < MAX_DIFFERENT_SCRIPT_BRAINS)
	{
		if ( (m_ScriptBrainArray[loop].m_TypeOfBrain == SCENARIO_PED)
			&& (m_ScriptBrainArray[loop].m_ScenarioName.GetHash() == HashOfScenarioName) )
		{
			s32 RandomInt = fwRandom::GetRandomNumberInRange(0, 100);
			if (RandomInt < m_ScriptBrainArray[loop].m_PercentageChance)
			{
				return loop;
			}
		}

		loop++;
	}

	return -1;
}

void CScriptsForBrains::GiveScenarioBrainToPed(CPed *pPed, s32 ScenarioBrainIndex)
{
	if (scriptVerifyf(ScenarioBrainIndex >= 0, "CScriptsForBrains::GiveScenarioBrainToPed - brain index should be >= 0"))
	{
		if (scriptVerifyf(m_ScriptBrainArray[ScenarioBrainIndex].m_TypeOfBrain == SCENARIO_PED, "CScriptsForBrains::GiveScenarioBrainToPed - brain with index %d is not a SCENARIO_PED brain", ScenarioBrainIndex))
		{
			if (scriptVerifyf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain ) == false, "CScriptsForBrains::GiveScenarioBrainToPed - can't start a second script brain for ped"))
			{
				if (scriptVerifyf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad ) == false, "CScriptsForBrains::GiveScenarioBrainToPed - already waiting for a streamed script brain to load for this ped"))
				{
					StartOrRequestNewStreamedScriptBrain(ScenarioBrainIndex, pPed, SCENARIO_PED, true);
				}
			}
		}
	}
}

bool CScriptsForBrains::CanBrainSafelyRun(s32 ScriptBrainIndex)
{
	if (scriptVerifyf( (ScriptBrainIndex >= 0) && (ScriptBrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS), "CScriptsForBrains::CanBrainSafelyRun - script brain index %d is out of range 0 to %d", ScriptBrainIndex, MAX_DIFFERENT_SCRIPT_BRAINS))
	{
		if ( (OBJECT_STREAMED == m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain) || (WORLD_POINT == m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain) )
		{
			if ( (m_ScriptBrainArray[ScriptBrainIndex].m_SetToWhichThisBrainBelongs & m_SetsEnabled) == 0)
			{
				return false;
			}
		}
		else
		{
			if (NetworkInterface::IsGameInProgress())
			{
				return false;
			}
		}
	}

	return true;
}

void CScriptsForBrains::ClearMissionRequiredFlagOnTheStreamedScript(s32 ScriptBrainIndex)
{
	if ( scriptVerifyf((ScriptBrainIndex >= 0) && (ScriptBrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS), "CScriptsForBrains::ClearMissionRequiredFlagOnTheStreamedScript - ScriptBrainIndex %d is out of range. Should be >= 0 and < %d", ScriptBrainIndex, MAX_DIFFERENT_SCRIPT_BRAINS) )
	{
		s32 ScriptId = g_StreamedScripts.FindSlot(m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName).Get();
		if (scriptVerifyf(ScriptId != -1, "CScriptsForBrains::ClearMissionRequiredFlagOnTheStreamedScript - couldn't find a script called %s", m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr()))
		{
			scriptDisplayf("CScriptsForBrains::ClearMissionRequiredFlagOnTheStreamedScript - clearing STRFLAG_MISSION_REQUIRED for %s", m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr());
			g_StreamedScripts.ClearRequiredFlag(ScriptId, STRFLAG_MISSION_REQUIRED);
		}
	}
}

void CScriptsForBrains::StartOrRequestNewStreamedScriptBrain(s32 ScriptBrainIndex, void *pEntityOrWorldPoint, s32 BrainType, bool bFirstTime)
{
	CPed *pPed = NULL;
	CObject *pObject = NULL;
	C2dEffect *pEff = NULL;

#if !__FINAL
	if (!gbAllowScriptBrains)
	{
		return;
	}
#endif

	if ( (OBJECT_STREAMED == BrainType) || (WORLD_POINT == BrainType) )
	{
		if ( (m_ScriptBrainArray[ScriptBrainIndex].m_SetToWhichThisBrainBelongs & m_SetsEnabled) == 0)
		{
			return;
		}
	}
	else
	{
		if (NetworkInterface::IsNetworkOpen())
		{
			return;
		}
	}

	STRVIS_AUTO_CONTEXT(strStreamingVisualize::SCRIPTBRAIN);
	Vector3 brainPosition = VEC3_ZERO;

	switch (BrainType)
	{
		case PED_STREAMED :
		case SCENARIO_PED :
			pPed = static_cast<CPed*>(pEntityOrWorldPoint);
			brainPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			break;

		case OBJECT_STREAMED :
			pObject = static_cast<CObject*>(pEntityOrWorldPoint);
			brainPosition = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
			break;

		case WORLD_POINT :
			pEff = static_cast<C2dEffect*>(pEntityOrWorldPoint);
			pEff->GetPos(brainPosition);
			break;
			
		default :
			scriptAssertf(0, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - unknown brain type");
			break;
	}

	if (bFirstTime)
	{
		if (m_ScriptBrainArray[ScriptBrainIndex].m_bBrainActive == false)
		{
			return;
		}

		switch (BrainType)
		{
			case PED_STREAMED :
			case SCENARIO_PED :
				scriptAssertf( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain ) == false, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - can't start a second script brain for ped");
				scriptAssertf( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad ) == false, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - already waiting for a streamed script brain to load for this ped");

				if ( (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain )) || (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad )) )
				{
					return;
				}

//				scriptAssertf( pPed->PopTypeGet() == POPTYPE_RANDOM_SCENARIO || !m_ScriptBrainArray[ScriptBrainIndex].bOnlyGiveBrainToScenarioPeds, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - trying to give a scenario-only brain to a non-scenario ped");
//				if (pPed->PopTypeGet() != POPTYPE_RANDOM_SCENARIO && m_ScriptBrainArray[ScriptBrainIndex].bOnlyGiveBrainToScenarioPeds)
//				{
//					return;
//				}
			break;

			case OBJECT_STREAMED :
				scriptAssertf( pObject->m_nObjectFlags.ScriptBrainStatus == CObject::OBJECT_DOESNT_USE_SCRIPT_BRAIN, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - this object has already been set up with a script brain");
			
				if (pObject->m_nObjectFlags.ScriptBrainStatus != CObject::OBJECT_DOESNT_USE_SCRIPT_BRAIN)
				{
					return;
				}
				break;

			case WORLD_POINT :
			{
				CWorldPointAttr* wp = pEff->GetWorldPoint();

				scriptAssertf( wp->BrainStatus == CWorldPoints::OUT_OF_BRAIN_RANGE, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - this world point has already been set up with a script brain");
			
				if (wp->BrainStatus != CWorldPoints::OUT_OF_BRAIN_RANGE)
				{
					return;
				}
			}
				break;
				
			default :
				scriptAssertf(0, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - unknown brain type");
				break;
		}
	}

	strLocalIndex ScriptId = g_StreamedScripts.FindSlot(m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName);
	if (ScriptId == -1)
	{
		scriptAssertf(0, "%s: CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - Script doesn't exist", m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr());
		return;
	}

	if (BrainType == OBJECT_STREAMED)	
	{									
		//	always add objects to the array even if the script has already streamed
		//	I think this is because the level designer can terminate the object brain script
		//	if the object is outside its defined activation range.
		//	Since the object remains in the array until the object is deleted, the script will
		//	be re-triggered if the player re-enters the activation range.

		//	I think this comment must have been added before !bFirstTime was added to 
		//	g_StreamedScripts.HasObjectLoaded(ScriptId) below
										
		if (bFirstTime)
		{
			pObject->StreamedScriptBrainToLoad = (s16)ScriptBrainIndex;
			pObject->m_nObjectFlags.ScriptBrainStatus = CObject::OBJECT_WAITING_TILL_IN_BRAIN_RANGE;
			CTheScripts::GetScriptBrainDispatcher().AddWaitingObject(pObject, ScriptBrainIndex);
		}
		else
		{
			pObject->m_nObjectFlags.ScriptBrainStatus = CObject::OBJECT_WAITING_FOR_SCRIPT_BRAIN_TO_LOAD;
		}
	}

	if (!bFirstTime && (g_StreamedScripts.HasObjectLoaded(ScriptId)) )
	{	
		CGameScriptId scriptId(m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr(), brainPosition);

		if (!CTheScripts::GetScriptHandlerMgr().GetScriptHandler(scriptId))
		{
			//	Never start a script the first time - to solve problem with over 50 Empire State telescopes
			//	being converted from dummy to real objects in one frame and starting a script each despite not being in activation range
			StartNewStreamedScriptBrain(ScriptBrainIndex, pEntityOrWorldPoint);
		}
	}
	else
	{
		const s32 streamingFlags = STRFLAG_MISSION_REQUIRED|STRFLAG_PRIORITY_LOAD;
		switch (BrainType)
		{
			case PED_STREAMED :
				g_StreamedScripts.StreamingRequest(ScriptId, streamingFlags);
				if (bFirstTime)
				{
					pPed->SetStreamedScriptBrainToLoad( (s16)ScriptBrainIndex );
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad, true );
					CTheScripts::GetScriptBrainDispatcher().AddWaitingPed(static_cast<CPed*>(pEntityOrWorldPoint), ScriptBrainIndex);
				}
				break;
		
			case OBJECT_STREAMED :
				g_StreamedScripts.StreamingRequest(ScriptId, streamingFlags);
				//	most of this was already handled at the start of the function
				break;

			case WORLD_POINT :
				g_StreamedScripts.StreamingRequest(ScriptId, streamingFlags);
				if (bFirstTime)
				{
					pEff->GetWorldPoint()->BrainStatus = CWorldPoints::WAITING_FOR_BRAIN_TO_LOAD;
					CTheScripts::GetScriptBrainDispatcher().AddWaitingWorldPoint(static_cast<C2dEffect*>(pEntityOrWorldPoint), ScriptBrainIndex);
				}
				break;
				
			case SCENARIO_PED :
				if (bFirstTime)
				{
					pPed->SetStreamedScriptBrainToLoad( (s16)ScriptBrainIndex );
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad, true );
					CTheScripts::GetScriptBrainDispatcher().AddWaitingPed(static_cast<CPed*>(pEntityOrWorldPoint), ScriptBrainIndex);
				}
				else
				{	//	Only request the scenario ped script once the player is within the activation range
					//	The range check is done within CScriptBrainDispatcher::Process()
					g_StreamedScripts.StreamingRequest(ScriptId, streamingFlags);
				}
				break;
			default :
				scriptAssertf(0, "CScriptsForBrains::StartOrRequestNewStreamedScriptBrain - unknown brain type");
				break;
		}
	}
}


void CScriptsForBrains::StartNewStreamedScriptBrain(u32 ScriptBrainIndex, void *pEntityOrWorldPoint)
{
	int EntityOrWorldPointIndex = -1;

	strLocalIndex id = g_StreamedScripts.FindSlot(m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName);
	scriptAssertf(id != -1, "%s:CScriptsForBrains::StartNewStreamedScriptBrain - Script doesn't exist", m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr());
	scriptAssertf(g_StreamedScripts.Get(id), "%s:CScriptsForBrains::StartNewStreamedScriptBrain - script brain has not loaded yet", m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr());

	GtaThread *pNewOne = NULL;

	scrValue NewThreadID;
	//	Maximum size needed for LocalVariablesForNewScript is for WORLD_POINTs
	//	Array of MAX_NUM_COORDS_FOR_WORLD_POINT Vectors
	//		plus 1 for the size of array (created by the script compiler)
	//	Array of MAX_NUM_COORDS_FOR_WORLD_POINT Headings
	//		plus 1 for the size of array (created by the script compiler)
	//		plus 1 for the script's number_of_coords variable
	scrValue *LocalVariablesForNewScript = rage_new scrValue[(4 * (MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS+1)) + 3];

	scrValue InputParams[4];

	InputParams[0].String = scrEncodeString(m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr());
	InputParams[1].Reference = scrEncodeReference(&LocalVariablesForNewScript[0]);	//	static_cast<scrPointer<scrValue>>
	InputParams[2].Int = 1;
	InputParams[3].Int = GtaThread::GetDefaultStackSize();

	scrThread::Info InfoForNewScript(&NewThreadID, 4, &InputParams[0]);

	switch (m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain)
	{
		case PED_STREAMED :
		case SCENARIO_PED :
				{	// The first local variable of the rage_new script will contain a pointer to this ped
					CPed *pPed = static_cast<CPed*>(pEntityOrWorldPoint);

					EntityOrWorldPointIndex = CTheScripts::GetGUIDFromEntity(*pPed);
					LocalVariablesForNewScript[0].Int = EntityOrWorldPointIndex;
					scrThread::StartNewScriptWithArgs(InfoForNewScript);

//	GW 14/09/06 - The brain scripts were adding two refs for each thread started. I've tried to remove one of those refs.
//					g_StreamedScripts.AddRef(id);
//	Now that the script has at least one ref it can't be streamed out so it is safe to clear STRFLAG_MISSION_REQUIRED
					g_StreamedScripts.ClearRequiredFlag(id.Get(), STRFLAG_MISSION_REQUIRED);

					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain, true );
					
					if (m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain != SCENARIO_PED)
					{	//	Don't remove scenario peds from the brain dispatcher array here.
						//	This will allow the brain to be retriggered if the player moves away and back again.
						//	The brain script can call DONT_RETRIGGER_BRAIN_FOR_THIS_SCENARIO_PED to remove 
						//	the entry from the brain dispatcher array so that the brain won't retrigger.
						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad, false );
						CTheScripts::GetScriptBrainDispatcher().RemoveWaitingPed(pPed, pPed->GetStreamedScriptBrainToLoad());
						pPed->SetStreamedScriptBrainToLoad(-1);
					}
				}
				break;
				
		case OBJECT_STREAMED :
				{	// The first local variable of the rage_new script will contain a pointer to this object
					CObject *pObject = static_cast<CObject*>(pEntityOrWorldPoint);

#if !__NO_OUTPUT
					Vec3V vecObjCoords = pObject->GetTransform().GetPosition();
					scriptDisplayf("CScriptsForBrains::StartNewStreamedScriptBrain - Starting script brain for object %s at %f, %f, %f", pObject->GetModelName(), vecObjCoords.GetXf(), vecObjCoords.GetYf(), vecObjCoords.GetZf());

					CInteriorProxy *pInteriorProxy = NULL;
					if (pObject->GetIsRetainedByInteriorProxy())
					{
						pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(pObject->GetRetainingInteriorProxy());
						if (pInteriorProxy)
						{
							scriptDisplayf("CScriptsForBrains::StartNewStreamedScriptBrain - Object retained by interior %s", pInteriorProxy->GetName().GetCStr());
						}
					}
					else
					{
						fwInteriorLocation interiorLoc = pObject->GetInteriorLocation();
						if (interiorLoc.IsValid())
						{
							pInteriorProxy = CInteriorProxy::GetFromLocation(interiorLoc);

							if (pInteriorProxy)
							{
								scriptDisplayf("CScriptsForBrains::StartNewStreamedScriptBrain - Object in interior %s:%d", pInteriorProxy->GetName().GetCStr(), interiorLoc.GetInteriorProxyIndex());

								if (interiorLoc.IsAttachedToRoom())
								{
									scriptDisplayf("CScriptsForBrains::StartNewStreamedScriptBrain - Object in room %d", interiorLoc.GetRoomIndex());
								}
								else if (interiorLoc.IsAttachedToPortal())
								{
									scriptDisplayf("CScriptsForBrains::StartNewStreamedScriptBrain - Object attached to portal %d", interiorLoc.GetPortalIndex());
								}
							}
						}
					}
#endif	//	!__NO_OUTPUT

					EntityOrWorldPointIndex = CTheScripts::GetGUIDFromEntity(*pObject);
					LocalVariablesForNewScript[0].Int = EntityOrWorldPointIndex;

					scrThread::StartNewScriptWithArgs(InfoForNewScript);

//	GW 14/09/06 - The brain scripts were adding two refs for each thread started. I've tried to remove one of those refs.
//					g_StreamedScripts.AddRef(id);
//	Now that the script has one ref it can't be streamed out so it is safe to clear STRFLAG_MISSION_REQUIRED
					g_StreamedScripts.ClearRequiredFlag(id.Get(), STRFLAG_MISSION_REQUIRED);
					pObject->m_nObjectFlags.ScriptBrainStatus = CObject::OBJECT_RUNNING_SCRIPT_BRAIN;

				}
				break;

		case WORLD_POINT :
				{	// The first local variable of the rage_new script will contain the number of points
					// The second local variable will be the number of elements in the array (all arrays have this before the first element)
					// This will be followed by three floats for the x,y,z of each point
					// There must be at least one point

					int loop = 0;
					C2dEffect *pEffect = static_cast<C2dEffect*>(pEntityOrWorldPoint);
					CWorldPointAttr* wp = pEffect->GetWorldPoint();
					Vector3 Pos;
					pEffect->GetPos(Pos);

					LocalVariablesForNewScript[0].Int = wp->num_of_extra_coords+1;
					LocalVariablesForNewScript[1].Int = MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS+1;

					for (loop = 0; loop < MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS+1; loop++)
					{
						LocalVariablesForNewScript[(3 * loop) + 2].Float = 0.0f;
						LocalVariablesForNewScript[(3 * loop) + 3].Float = 0.0f;
						LocalVariablesForNewScript[(3 * loop) + 4].Float = 0.0f;

						LocalVariablesForNewScript[loop + (3 * (MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS)) + 6].Float = 0.0f;

					}

					LocalVariablesForNewScript[(3 * 0) + 2].Float = Pos.x;		// Coordinates of main point of 2d effect
					LocalVariablesForNewScript[(3 * 0) + 3].Float = Pos.y;
					LocalVariablesForNewScript[(3 * 0) + 4].Float = Pos.z;
					LocalVariablesForNewScript[ (3 * MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS) + 6].Float = wp->extraCoordOrientations[0];

					for (loop = 0; loop < wp->num_of_extra_coords; loop++)
					{
						LocalVariablesForNewScript[(3 * (loop+1)) + 2].Float = wp->extraCoords[loop].x;
						LocalVariablesForNewScript[(3 * (loop+1)) + 3].Float = wp->extraCoords[loop].y;
						LocalVariablesForNewScript[(3 * (loop+1)) + 4].Float = wp->extraCoords[loop].z;

						LocalVariablesForNewScript[ (loop+1) + (3 * MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS) + 6].Float = wp->extraCoordOrientations[loop+1];
					}

					LocalVariablesForNewScript[(3 * MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS) + 5].Int = MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS+1;



					//	Array of MAX_NUM_COORDS_FOR_WORLD_POINT Vectors
					//		plus 1 for the size of array (created by the script compiler)
					//	Array of MAX_NUM_COORDS_FOR_WORLD_POINT Headings
					//		plus 1 for the size of array (created by the script compiler)
					//		plus 1 for the script's number_of_coords variable
					InputParams[2].Int = (4 * (MAX_NUMBER_OF_EXTRA_WORLD_POINT_COORDS+1)) + 3;	//	override the default value of 1



					scrThread::StartNewScriptWithArgs(InfoForNewScript);

//	GW 14/09/06 - The brain scripts were adding two refs for each thread started. I've tried to remove one of those refs.
//					g_StreamedScripts.AddRef(id);
//	Now that the script has at least one ref it can't be streamed out so it is safe to clear STRFLAG_MISSION_REQUIRED
					g_StreamedScripts.ClearRequiredFlag(id.Get(), STRFLAG_MISSION_REQUIRED);

					if(pEffect->GetWorldPoint()){
						EntityOrWorldPointIndex = CModelInfo::GetWorldPointStore().GetElementIndex(fwFactory::GLOBAL, pEffect->GetWorldPoint());
					}

					wp->BrainStatus = CWorldPoints::RUNNING_BRAIN;
					CTheScripts::GetScriptBrainDispatcher().RemoveWaitingWorldPoint(pEffect, wp->ScriptBrainToLoad);
//	Don't reset scriptBrainIndexToLoad here as IsWorldPointWithinActivationRange needs the link from WorldPoint to m_ScriptBrainArray
//					pWorldPoint->scriptBrainIndexToLoad = -1;
				}
			break;

		default :
				scriptAssertf(0, "CScriptsForBrains::StartNewStreamedScriptBrain - unknown brain type");
				break;
	}

	pNewOne = static_cast<GtaThread *> (scrThread::GetThread(static_cast<scrThreadId>(NewThreadID.Int)));
	if (scriptVerifyf(pNewOne, "CScriptsForBrains::StartNewStreamedScriptBrain - Could not get a thread %s", m_ScriptBrainArray[ScriptBrainIndex].m_ScriptFileName.GetCStr()))
	{
		pNewOne->ScriptBrainType = m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain;
		scriptAssertf(EntityOrWorldPointIndex >= 0, "CScriptsForBrains::StartNewStreamedScriptBrain - EntityOrWorldPointIndex should have been assigned by now");
		pNewOne->EntityIndexForScriptBrain = EntityOrWorldPointIndex;

		if (pNewOne->m_Handler)
		{
			CTheScripts::GetScriptHandlerMgr().RecalculateScriptId(*pNewOne->m_Handler);
		}
	}
	delete[] LocalVariablesForNewScript;
}

s32 CScriptsForBrains::GetIndexOfScriptBrainWithThisName(const strStreamingObjectName pScriptName, s8 BrainType)
{
	u32 loop = 0;
	while (loop < MAX_DIFFERENT_SCRIPT_BRAINS)
	{
		if ((m_ScriptBrainArray[loop].m_TypeOfBrain == BrainType) && (m_ScriptBrainArray[loop].m_ScriptFileName.GetHash() == pScriptName.GetHash()))
		{
			return loop;
		}
		else
		{
			loop++;
		}
	}

	return -1;
}

bool CScriptsForBrains::IsObjectWithinActivationRange(const CObject *pObject, const Vector3 &vecCentrePoint)
{
#if GTA_REPLAY
	// Always out of range for replay
	if( CReplayMgr::IsReplayInControlOfWorld() == false )
#endif	//GTA_REPLAY
	{
		s32 ScriptBrainIndex = pObject->StreamedScriptBrainToLoad;
		if (scriptVerifyf( (ScriptBrainIndex >= 0) && (ScriptBrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS), "CScriptsForBrains::IsObjectWithinActivationRange - Script brain index is out of range"))
		{
			if (scriptVerifyf(m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain == OBJECT_STREAMED, "CScriptsForBrains::IsObjectWithinActivationRange - Expected brain type to be Object"))
			{
				const Vector3 ObjectPos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
				if (ArePointsWithinActivationRange(vecCentrePoint, ObjectPos, ScriptBrainIndex, 0.0f, false))
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

bool CScriptsForBrains::IsScenarioPedWithinActivationRange(const CPed *pPed, const Vector3 &vecCentrePoint, float fRangeExtension)
{
#if GTA_REPLAY
	// Always out of range for replay
	if( CReplayMgr::IsReplayInControlOfWorld() == false )
#endif	//GTA_REPLAY
	{
		s32 ScriptBrainIndex =  pPed->GetStreamedScriptBrainToLoad();
		if (scriptVerifyf( (ScriptBrainIndex >= 0) && (ScriptBrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS), "CScriptsForBrains::IsScenarioPedWithinActivationRange - Script brain index is out of range"))
		{
			if (scriptVerifyf(m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain == SCENARIO_PED, "CScriptsForBrains::IsScenarioPedWithinActivationRange - Expected brain type to be SCENARIO_PED"))
			{
				const Vector3 PedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				if (ArePointsWithinActivationRange(vecCentrePoint, PedPos, ScriptBrainIndex, fRangeExtension, false))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CScriptsForBrains::IsWorldPointWithinActivationRange(C2dEffect *pEff, const Vector3 &vecCentrePoint)
{
#if GTA_REPLAY
	// Always out of range for replay
	if( CReplayMgr::IsReplayInControlOfWorld() == false )
#endif	//GTA_REPLAY
	{
		CWorldPointAttr* wp = pEff->GetWorldPoint();
		if (!wp)
		{
			scriptAssertf(0, "CScriptsForBrains::IsWorldPointWithinActivationRange - Type = %d, Pos = %f %f %f", pEff->GetType(), pEff->GetPosDirect().x, pEff->GetPosDirect().y, pEff->GetPosDirect().z);
			return false;
		}

		int ScriptBrainIndex = wp->ScriptBrainToLoad;
		Vector3 vecWorldPointPos;
		pEff->GetPos(vecWorldPointPos);

		if (ScriptBrainIndex < 0) return false;		// This will happen frequently if a worldpoint has been set up in max but the corresponding brain hasn't been registered by the script.

		if (scriptVerifyf(ScriptBrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS, "CScriptsForBrains::IsWorldPointWithinActivationRange - Script brain index is out of range"))
		{
			if (scriptVerifyf(m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain == WORLD_POINT, "CScriptsForBrains::IsWorldPointWithinActivationRange - Expected brain type to be World Point"))
			{
			//	Tom's window cleaner lift needs to work for tall buildings so ignore z for all world points
			//	so that the script doesn't terminate if the player is 200 metres above the ground
				if (ArePointsWithinActivationRange(vecWorldPointPos, vecCentrePoint, ScriptBrainIndex, 0.0f, true))
				{
					return true;
				}
			}
		}
	}

	return false;
}

const char *CScriptsForBrains::GetScriptFileName(s32 BrainIndex)
{
	if (scriptVerifyf((BrainIndex >= 0) && (BrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS), "CScriptsForBrains::GetScriptFileName - brain index is out of range"))
	{
		return m_ScriptBrainArray[BrainIndex].m_ScriptFileName.GetCStr();
	}

	return NULL;
}

s8 CScriptsForBrains::GetBrainType(s32 BrainIndex)
{
	if (scriptVerifyf((BrainIndex >= 0) && (BrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS), "CScriptsForBrains::GetBrainType - brain index is out of range"))
	{
		return m_ScriptBrainArray[BrainIndex].m_TypeOfBrain;
	}

	return -1;
}

bool CScriptsForBrains::ArePointsWithinActivationRange(const Vector3 &vecCoord1, const Vector3 &vecCoord2, s32 ScriptBrainIndex, float fRangeExtension, bool bIgnoreZ)
{
	Vector3 vecDiff = (vecCoord1 - vecCoord2);
	if (bIgnoreZ)
	{
		vecDiff.z = 0.0f;
	}

	float distance_to_player_squared = vecDiff.Mag2();

	float fActivationRange = m_ScriptBrainArray[ScriptBrainIndex].m_fBrainActivationRange + fRangeExtension;

	if (distance_to_player_squared < (fActivationRange * fActivationRange) )
	{
		return true;
	}

	return false;
}


void CScriptsForBrains::RemoveScenarioPedFromBrainDispatcherArray(CPed *pPed)
{
	if (scriptVerifyf(pPed, "CScriptsForBrains::RemoveScenarioPedFromBrainDispatcherArray - ped doesn't exist"))
	{
		s16 ScriptBrainIndex = pPed->GetStreamedScriptBrainToLoad();
		if (scriptVerifyf(ScriptBrainIndex >= 0, "CScriptsForBrains::RemoveScenarioPedFromBrainDispatcherArray - ped doesn't have an associated script brain"))
		{
			if (scriptVerifyf(ScriptBrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS, "CScriptsForBrains::RemoveScenarioPedFromBrainDispatcherArray - Script brain index is out of range"))
			{
				if (scriptVerifyf(m_ScriptBrainArray[ScriptBrainIndex].m_TypeOfBrain == SCENARIO_PED, "CScriptsForBrains::RemoveScenarioPedFromBrainDispatcherArray - Expected brain type to be SCENARIO_PED"))
				{
					CTheScripts::GetScriptBrainDispatcher().RemoveWaitingPed(pPed, ScriptBrainIndex);

//	I can't reset the following two variables here because the script brain might still call GetBrainActivationRangeSquaredOfScenarioPed
//	which relies on GetStreamedScriptBrainToLoad() so I'll leave it for the Ped destructor to reset them
//					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad, false );
//					pPed->SetStreamedScriptBrainToLoad(-1);
				}
			}
		}
	}
}

//	end of script brain functions


void CScriptBrainDispatcher::ItemWaitingForBrain::Clear()
{
	m_pWaitingObject = NULL;
	m_pWaitingPed = NULL;
	m_pWaitingWorldPoint = NULL;

	m_scriptBrainIndex = -1;
	m_waitingItemsType = INVALID_WAITING;
}

void CScriptBrainDispatcher::Init(void)
{
	for (u32 loop = 0; loop < MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN; loop++)
	{
		ItemsWaitingForBrains[loop].Clear();
	}
}
void CScriptBrainDispatcher::AddWaitingObject(CObject* pItem, s32 scriptBrainIndexToLoad)
{
	const WaitingItemType itemType = OBJECT_WAITING;
	AddWaitingItem(static_cast<void*>(pItem), scriptBrainIndexToLoad, itemType);
}
void CScriptBrainDispatcher::AddWaitingPed(CPed* pItem, s32 scriptBrainIndexToLoad)
{
	const WaitingItemType itemType = PED_WAITING;
	AddWaitingItem(static_cast<void*>(pItem), scriptBrainIndexToLoad, itemType);
}
void CScriptBrainDispatcher::AddWaitingWorldPoint(C2dEffect* pItem, s32 scriptBrainIndexToLoad)
{
	const WaitingItemType itemType = WORLD_POINT_WAITING;
	AddWaitingItem(static_cast<void*>(pItem), scriptBrainIndexToLoad, itemType);
}
void CScriptBrainDispatcher::AddWaitingItem(void *pEntityOrPoint, s32 scriptBrainIndexToLoad, WaitingItemType itemType)
{
	scriptAssertf(itemType != INVALID_WAITING, "CScriptBrainDispatcher::AddWaitingItem -Adding a waiting item with invalid type is not allowed.");

	s32 free_index = -1;
	u32 loop = 0;
	while (loop < MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN)
	{
		if (ItemsWaitingForBrains[loop].m_waitingItemsType == INVALID_WAITING)
		{
			free_index = loop;
		}
		else
		{
			if (ItemsWaitingForBrains[loop].m_scriptBrainIndex != scriptBrainIndexToLoad)
			{
				bool itemsAreSame = false;
				switch (itemType)
				{
				case PED_WAITING :
					itemsAreSame = (ItemsWaitingForBrains[loop].m_pWaitingPed == pEntityOrPoint);
					break;
				case OBJECT_WAITING :
					itemsAreSame = (ItemsWaitingForBrains[loop].m_pWaitingObject == pEntityOrPoint);
					break;
				case WORLD_POINT_WAITING :
					itemsAreSame = (ItemsWaitingForBrains[loop].m_pWaitingWorldPoint == pEntityOrPoint);
					break;
				default :
					scriptAssertf(0, "CScriptBrainDispatcher::AddWaitingItem - Item is not a ped, object or world point");
					break;
				}

				if (itemsAreSame)
				{	//	this entity is already in the list so just return
					scriptAssertf(0, "CScriptBrainDispatcher::AddWaitingItem - item is already in the list for a different brain");
					return;
				}
			}
		}
		
		loop++;
	}
	
	scriptAssertf(free_index >= 0, "CScriptBrainDispatcher::AddWaitingItem - no space left in list");
	
	if (free_index >= 0)
	{
		switch (itemType)
		{
			case PED_WAITING :
				ItemsWaitingForBrains[free_index].m_pWaitingPed = static_cast<CPed*>(pEntityOrPoint);
				break;
			case OBJECT_WAITING :
				ItemsWaitingForBrains[free_index].m_pWaitingObject = static_cast<CObject*>(pEntityOrPoint);
				break;
			case WORLD_POINT_WAITING :
				ItemsWaitingForBrains[free_index].m_pWaitingWorldPoint = static_cast<C2dEffect*>(pEntityOrPoint);
				break;
			default :
				scriptAssertf(0, "CScriptBrainDispatcher::AddWaitingItem - New item is not a ped, object or world point");
				break;
		}
		ItemsWaitingForBrains[free_index].m_scriptBrainIndex = scriptBrainIndexToLoad;
		ItemsWaitingForBrains[free_index].m_waitingItemsType = itemType;
	}
}
void CScriptBrainDispatcher::RemoveWaitingObject(CObject* pItem, s32 scriptBrainIndexToLoad)
{
	const WaitingItemType itemType = OBJECT_WAITING;
	RemoveWaitingItem(static_cast<void*>(pItem), scriptBrainIndexToLoad, itemType);
}
void CScriptBrainDispatcher::RemoveWaitingPed(CPed* pItem, s32 scriptBrainIndexToLoad)
{
	const WaitingItemType itemType = PED_WAITING;
	RemoveWaitingItem(static_cast<void*>(pItem), scriptBrainIndexToLoad, itemType);
}
void CScriptBrainDispatcher::RemoveWaitingWorldPoint(C2dEffect* pItem, s32 scriptBrainIndexToLoad)
{
	const WaitingItemType itemType = WORLD_POINT_WAITING;
	RemoveWaitingItem(static_cast<void*>(pItem), scriptBrainIndexToLoad, itemType);
}
void CScriptBrainDispatcher::RemoveWaitingItem(void *pEntityOrPoint, s32 scriptBrainIndexToLoad, WaitingItemType itemType)
{
	u32 loop = 0;
	while (loop < MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN)
	{
		bool itemsAreSame = false;
		switch (itemType)
		{
		case PED_WAITING :
			itemsAreSame = (ItemsWaitingForBrains[loop].m_pWaitingPed == pEntityOrPoint);
			break;
		case OBJECT_WAITING :
			itemsAreSame = (ItemsWaitingForBrains[loop].m_pWaitingObject == pEntityOrPoint);
			break;
		case WORLD_POINT_WAITING :
			itemsAreSame = (ItemsWaitingForBrains[loop].m_pWaitingWorldPoint == pEntityOrPoint);
			break;
		default :
			scriptAssertf(0, "CScriptBrainDispatcher::RemoveWaitingItem - Item is not a ped, object or world point");
			break;
		}

		if (itemsAreSame)
		{
			if (ItemsWaitingForBrains[loop].m_scriptBrainIndex != scriptBrainIndexToLoad)
			{
				scriptAssertf(0, "CScriptBrainDispatcher::RemoveWaitingItem - Entity is in the list for a different brain");
			}
			else if (ItemsWaitingForBrains[loop].m_waitingItemsType != itemType)
			{
				scriptAssertf(0, "CScriptBrainDispatcher::RemoveWaitingItem - m_waitingItemsType doesn't match");
			}
			else
			{
				ItemsWaitingForBrains[loop].Clear();
			}
		}
		
		loop++;
	}
}
//	Object remains in array for as long as it exists
//
//	Script is marked as no longer needed when the object is deleted and in StartNewStreamedScriptBrain()

//	For scenario peds, they'll remain in the array until the ped is deleted or the script brain itself says to remove the array entry.
//	This should allow the brain to be retriggered if the player moves away and back again.

void CScriptBrainDispatcher::Process(void)
{
	CEntity *pEntity = NULL;
	C2dEffect *pEffect = NULL;
	CPed *pPed = NULL;
	CObject *pObject = NULL;
	
	if (!CGameWorld::FindLocalPlayer())
	{	//	can't get the player's position until he has been created
		return;
	}
	
	const Vector3 PlayerPos = CGameWorld::FindLocalPlayerCentreOfWorld();

	u32 loop = 0;
	while (loop < MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN)
	{
		if (ItemsWaitingForBrains[loop].m_waitingItemsType != INVALID_WAITING)
		{
			pEntity = NULL;
			pEffect = NULL;
			switch (ItemsWaitingForBrains[loop].m_waitingItemsType)
			{
				case PED_WAITING :
					pEntity = static_cast<CEntity*>(ItemsWaitingForBrains[loop].m_pWaitingPed);
					break;	
				case OBJECT_WAITING :
					pEntity = static_cast<CEntity*>(ItemsWaitingForBrains[loop].m_pWaitingObject);
					break;	
				case WORLD_POINT_WAITING :
					pEffect = ItemsWaitingForBrains[loop].m_pWaitingWorldPoint;
					break;
				default :
					scriptAssertf(0, "CScriptBrainDispatcher::Process - Item is not a ped, object or world point");
					break;
			}

			s8 BrainType = CTheScripts::GetScriptsForBrains().GetBrainType(ItemsWaitingForBrains[loop].m_scriptBrainIndex);
			switch (BrainType)
			{
				case CScriptsForBrains::PED_STREAMED :
					scriptAssertf(pEntity->GetType() == ENTITY_TYPE_PED, "CScriptBrainDispatcher::Process - Ped script brain expects ped entity");
					pPed = (CPed *) pEntity;
					scriptAssertf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad ), "CScriptBrainDispatcher::Process - Ped's waiting for script brain flag should be set");
					CTheScripts::GetScriptsForBrains().StartOrRequestNewStreamedScriptBrain(pPed->GetStreamedScriptBrainToLoad(), pPed, BrainType, false);	//	, pPed->ScriptTriggerMethod);	//	CScriptsForBrains::PED_STREAMED);
					break;
				
				case CScriptsForBrains::OBJECT_STREAMED :
					scriptAssertf(pEntity->GetType() == ENTITY_TYPE_OBJECT, "CScriptBrainDispatcher::Process - Object script brain expects object entity");
					pObject = (CObject *) pEntity;

					switch (pObject->m_nObjectFlags.ScriptBrainStatus)
					{
						case CObject::OBJECT_WAITING_TILL_IN_BRAIN_RANGE :
							if (CTheScripts::GetScriptsForBrains().IsObjectWithinActivationRange(pObject, PlayerPos))
							{
								CTheScripts::GetScriptsForBrains().StartOrRequestNewStreamedScriptBrain(pObject->StreamedScriptBrainToLoad, pObject, BrainType, false);	//	, TRIGGER_METHOD_NA);	//	CScriptsForBrains::OBJECT_STREAMED);
							}							
							break;
							
						case CObject::OBJECT_WAITING_FOR_SCRIPT_BRAIN_TO_LOAD :
							CTheScripts::GetScriptsForBrains().StartOrRequestNewStreamedScriptBrain(pObject->StreamedScriptBrainToLoad, pObject, BrainType, false);	//	, TRIGGER_METHOD_NA);	//	CScriptsForBrains::OBJECT_STREAMED);
							break;

						case CObject::OBJECT_WAITING_TILL_OUT_OF_BRAIN_RANGE :
							if (CTheScripts::GetScriptsForBrains().IsObjectWithinActivationRange(pObject, PlayerPos) == false)
							{
								pObject->m_nObjectFlags.ScriptBrainStatus = CObject::OBJECT_WAITING_TILL_IN_BRAIN_RANGE;
							}
							break;
							
						case CObject::OBJECT_RUNNING_SCRIPT_BRAIN :
							//	do nothing - already running the script - it is up to the script to check if the object is within range of the player
							break;

						default :
							scriptAssertf(0, "CScriptBrainDispatcher::Process - unexpected script brain status for object");
							break;
					}
					
					break;
				case CScriptsForBrains::WORLD_POINT :
					{
						if (!CTheScripts::GetScriptsForBrains().CanBrainSafelyRun(pEffect->GetWorldPoint()->ScriptBrainToLoad))
						{
							CTheScripts::GetScriptsForBrains().ClearMissionRequiredFlagOnTheStreamedScript(pEffect->GetWorldPoint()->ScriptBrainToLoad);

							pEffect->GetWorldPoint()->Reset(false);

							ItemsWaitingForBrains[loop].Clear();
						}
						else
						{
							CTheScripts::GetScriptsForBrains().StartOrRequestNewStreamedScriptBrain(pEffect->GetWorldPoint()->ScriptBrainToLoad, pEffect, BrainType, false);	//	, TRIGGER_METHOD_NA);
						}
					}
					break;

				case CScriptsForBrains::SCENARIO_PED :
				{
					scriptAssertf(pEntity->GetType() == ENTITY_TYPE_PED, "CScriptBrainDispatcher::Process - Scenario ped script brain expects ped entity");
					pPed = (CPed *) pEntity;

					if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain ))
					{
						scriptAssertf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad ), "CScriptBrainDispatcher::Process - Scenario ped's waiting for script brain flag should be set");
						if (CTheScripts::GetScriptsForBrains().IsScenarioPedWithinActivationRange(pPed, PlayerPos, 0.0f))
						{
							CTheScripts::GetScriptsForBrains().StartOrRequestNewStreamedScriptBrain(pPed->GetStreamedScriptBrainToLoad(), pPed, BrainType, false);
						}
					}
				}
					break;

				default :
					scriptAssertf(0, "CScriptBrainDispatcher::Process - unknown brain type");
					break;
			}
		}
	
		loop++;
	}
}

void CScriptBrainDispatcher::ReactivateAllObjectBrainsThatAreWaitingTillOutOfRange()
{
	int loop = 0;
	while (loop < MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN)
	{
		if (ItemsWaitingForBrains[loop].m_waitingItemsType == OBJECT_WAITING)
		{
			if (ItemsWaitingForBrains[loop].m_pWaitingObject)
			{
				if (ItemsWaitingForBrains[loop].m_pWaitingObject->m_nObjectFlags.ScriptBrainStatus == CObject::OBJECT_WAITING_TILL_OUT_OF_BRAIN_RANGE)
				{
					ItemsWaitingForBrains[loop].m_pWaitingObject->m_nObjectFlags.ScriptBrainStatus = CObject::OBJECT_WAITING_TILL_IN_BRAIN_RANGE;
				}				
			}
		}
		loop++;
	}
}


void CScriptBrainDispatcher::ReactivateNamedObjectBrainsWaitingTillOutOfRange(const char *pNameToSearchFor)
{
	int loop = 0;
	while (loop < MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN)
	{
		if (ItemsWaitingForBrains[loop].m_waitingItemsType == OBJECT_WAITING)
		{
			if (ItemsWaitingForBrains[loop].m_pWaitingObject)
			{
				if (ItemsWaitingForBrains[loop].m_pWaitingObject->m_nObjectFlags.ScriptBrainStatus == CObject::OBJECT_WAITING_TILL_OUT_OF_BRAIN_RANGE)
				{
					if (scriptVerifyf(ItemsWaitingForBrains[loop].m_scriptBrainIndex != -1, "CScriptBrainDispatcher::ReactivateNamedObjectBrainsWaitingTillOutOfRange - didn't expect m_scriptBrainIndex to be -1 here"))
					{
						const char *pNameOfThisScript = CTheScripts::GetScriptsForBrains().GetScriptFileName(ItemsWaitingForBrains[loop].m_scriptBrainIndex);
						if (scriptVerifyf(pNameOfThisScript, "CScriptBrainDispatcher::ReactivateNamedObjectBrainsWaitingTillOutOfRange - failed to find the name of script in ItemsWaitingForBrains array"))
						{
							if (stricmp(pNameToSearchFor, pNameOfThisScript) == 0)
							{
								ItemsWaitingForBrains[loop].m_pWaitingObject->m_nObjectFlags.ScriptBrainStatus = CObject::OBJECT_WAITING_TILL_IN_BRAIN_RANGE;
							}
						}
					}
				}				
			}
		}
		loop++;
	}
}


/*
void CScriptBrainDispatcher::PurgeWaitingItems(void)
{
	int loop = 0;
	while (loop < MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN)
	{
		if (ItemsWaitingForBrains[loop].m_waitingItemsType != INVALID_WAITING)
		{
			switch (ItemsWaitingForBrains[loop].m_waitingItemsType)
			{
				case PED_WAITING :
					{
						CPed *pPed = ItemsWaitingForBrains[loop].m_pWaitingPed;
						scriptAssertf(pPed->GetStreamedScriptBrainToLoad() == ItemsWaitingForBrains[loop].m_scriptBrainIndex, "CScriptBrainDispatcher::PurgeWaitingItems - Brain Index should be the same within ped and within waiting array");
						scriptAssertf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain ) == false, "CScriptBrainDispatcher::PurgeWaitingItems - ped shouldn't be in the waiting array if it already has a brain");
						scriptAssertf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad ), "CScriptBrainDispatcher::PurgeWaitingItems - ped in the waiting array should have its waiting flag set");

						pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WaitingForScriptBrainToLoad, false );
						pPed->SetStreamedScriptBrainToLoad(-1);
					}
					break;
				case OBJECT_WAITING :
					{
#if __ASSERT
                        CObject *pObject = ItemsWaitingForBrains[loop].m_pWaitingObject;
						scriptAssertf(pObject->StreamedScriptBrainToLoad == ItemsWaitingForBrains[loop].m_scriptBrainIndex, "CScriptBrainDispatcher::PurgeWaitingItems - Brain Index should be the same within object and within waiting array");
						scriptAssertf(pObject->m_nObjectFlags.ScriptBrainStatus != CObject::OBJECT_DOESNT_USE_SCRIPT_BRAIN, "CScriptBrainDispatcher::PurgeWaitingItems - object brain status shouldn't be OBJECT_DOESNT_USE_SCRIPT_BRAIN");
#endif
//	Any script that is already running should be left to terminate itself

//	Leave it up to the object destructor to reset these
//		m_nObjectFlags.ScriptBrainStatus
//		StreamedScriptBrainToLoad
					}
					break;
				case WORLD_POINT_WAITING :
					{
						C2dEffect *pEffect = ItemsWaitingForBrains[loop].m_pWaitingWorldPoint;
						CWorldPointAttr* wp = pEffect->GetWorldPoint();

						scriptAssertf(wp->BrainStatus == CWorldPoints::WAITING_FOR_BRAIN_TO_LOAD, "CScriptBrainDispatcher::PurgeWaitingItems - World point in waiting array doesn't have WAITING status");
						scriptAssertf(wp->ScriptBrainToLoad == ItemsWaitingForBrains[loop].m_scriptBrainIndex, "CScriptBrainDispatcher::PurgeWaitingItems - Brain Index should be the same within world point and within waiting array");

						//	Setting this to WAITING_TILL_OUT_OF_BRAIN_RANGE instead of OUT_OF_BRAIN_RANGE should prevent the brain triggering 
						//	as soon as the player leaves the network game if he is standing within the activation range
						wp->BrainStatus = CWorldPoints::WAITING_TILL_OUT_OF_BRAIN_RANGE;
//	Don't reset scriptBrainIndexToLoad here as IsWorldPointWithinActivationRange needs the link from WorldPoint to ScriptBrainArray
//					pWorldPoint->scriptBrainIndexToLoad = -1;
					}
					break;
				default :
					scriptAssertf(0, "CScriptBrainDispatcher::PurgeWaitingItems - Item is not a ped, object or world point");
					break;
			}

			s32 nBrainIndex = ItemsWaitingForBrains[loop].m_scriptBrainIndex;
			scriptAssertf( (nBrainIndex >= 0) && (nBrainIndex < MAX_DIFFERENT_SCRIPT_BRAINS), "CScriptBrainDispatcher::PurgeWaitingItems - script brain index is out of range");
			s32 StreamedScriptId = g_StreamedScripts.FindSlot(CTheScripts::GetScriptsForBrains().ScriptBrainArray[nBrainIndex].m_ScriptFileName);
			scriptAssertf(StreamedScriptId != -1, "%s:CScriptBrainDispatcher::PurgeWaitingItems - Script doesn't exist", CTheScripts::GetScriptsForBrains().ScriptBrainArray[nBrainIndex].m_ScriptFileName.GetCStr());
			g_StreamedScripts.ClearRequiredFlag(StreamedScriptId, STRFLAG_MISSION_REQUIRED);

			ItemsWaitingForBrains[loop].m_pWaitingObject = NULL;
			ItemsWaitingForBrains[loop].m_pWaitingPed = NULL;
			ItemsWaitingForBrains[loop].m_pWaitingWorldPoint = NULL;

			ItemsWaitingForBrains[loop].m_scriptBrainIndex = -1;
			ItemsWaitingForBrains[loop].m_waitingItemsType = INVALID_WAITING;
		}
		loop++;
	}
}
bool CScriptBrainDispatcher::AreAnyItemsWaiting(void)
{
	int loop = (MAX_ENTITIES_WAITING_FOR_SCRIPT_BRAIN - 1);
	while (loop >= 0)
	{
		if (ItemsWaitingForBrains[loop].m_waitingItemsType != INVALID_WAITING)
		{
			return true;
		}
		loop--;
	}

	return false;
}
*/


