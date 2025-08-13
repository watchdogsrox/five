// Rage headers
#include "bank/bkmgr.h"
#include "bank/group.h"
#include "parser/manager.h"

// Framework headers
#include "fwmaths/random.h"

// Game headers
#include "game/Dispatch/DispatchData.h"
#include "game/Dispatch/DispatchData_parser.h"
#include "Game/Dispatch/DispatchServices.h"
#include "game/Wanted.h"
#include "network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "peds/PopCycle.h"
#include "scene/DataFileMgr.h"

AI_OPTIMISATIONS()


CompileTimeAssert(parser_DispatchType_Count == DT_Max + 1);

// Static Manager object
CDispatchData CDispatchData::m_DispatchDataInstance;

//////////////////////////////////////////////////////////////////////////

s8 CWantedResponse::CalculateNumPedsToSpawn(DispatchType nDispatchType)
{
	//Check if the override is valid.
	if(CWantedResponseOverrides::GetInstance().HasNumPedsToSpawn(nDispatchType))
	{
		return CWantedResponseOverrides::GetInstance().GetNumPedsToSpawn(nDispatchType);
	}

	//Ensure the dispatch response is valid.
	const CDispatchResponse* pDispatchResponse = GetDispatchServices(nDispatchType);
	if(!pDispatchResponse)
	{
		return 0;
	}

	return pDispatchResponse->GetNumPedsToSpawn();
}

CDispatchResponse* CWantedResponse::GetDispatchServices(DispatchType dispatchType)
{
    for(int i = 0; i < m_DispatchServices.GetCount(); i++)
    {
        if(m_DispatchServices[i].GetType() == dispatchType)
        {
            return &m_DispatchServices[i];
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////

CWantedResponseOverrides::CWantedResponseOverrides()
{
	Reset();
}

CWantedResponseOverrides::~CWantedResponseOverrides()
{

}

CWantedResponseOverrides& CWantedResponseOverrides::GetInstance()
{
	static CWantedResponseOverrides sInst;
	return sInst;
}

s8 CWantedResponseOverrides::GetNumPedsToSpawn(DispatchType nDispatchType) const
{
	return m_aNumPedsToSpawn[nDispatchType];
}

bool CWantedResponseOverrides::HasNumPedsToSpawn(DispatchType nDispatchType) const
{
	return (m_aNumPedsToSpawn[nDispatchType] >= 0);
}

void CWantedResponseOverrides::Reset()
{
	//Reset the number of peds to spawn.
	ResetNumPedsToSpawn();
}

void CWantedResponseOverrides::ResetNumPedsToSpawn()
{
	//Iterate over the number of peds to spawn.
	int iCount = m_aNumPedsToSpawn.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Reset the number of peds to spawn.
		m_aNumPedsToSpawn[i] = -1;
	}
}

void CWantedResponseOverrides::SetNumPedsToSpawn(DispatchType nDispatchType, s8 iNumPedsToSpawn)
{
	//Set the number of peds to spawn.
	m_aNumPedsToSpawn[nDispatchType] = iNumPedsToSpawn;
}

//////////////////////////////////////////////////////////////////////////

CDispatchData::CDispatchData() {}

void CDispatchData::Init(unsigned /*initMode*/)
{
    // Load our wanted data
    if(LoadWantedData())
    {
        // Verify some array counts
        Assertf(m_DispatchDataInstance.m_WantedLevelThresholds.GetCount() == WANTED_LEVEL_LAST, "Wanted Level threshold count is invalid in data/dispatch.meta, need WANTED_CLEAN through WANTED_LEVEL5");
        Assertf(m_DispatchDataInstance.m_SingleplayerWantedLevelRadius.GetCount() == WANTED_LEVEL_LAST, "Singleplayer Wanted Level radius count is invalid in data/dispatch.meta, need WANTED_CLEAN through WANTED_LEVEL5");
		Assertf(m_DispatchDataInstance.m_MultiplayerWantedLevelRadius.GetCount() == WANTED_LEVEL_LAST, "Multiplayer Wanted Level radius count is invalid in data/dispatch.meta, need WANTED_CLEAN through WANTED_LEVEL5");
		Assertf(m_DispatchDataInstance.m_HiddenEvasionTimes.GetCount() == WANTED_LEVEL_LAST, "Wanted level hidden times count is invalid in data/dispatch.meta, need WANTED_CLEAN through WANTED_LEVEL5");
		Assertf(m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes.GetCount() == WANTED_LEVEL_LAST, "Multiplayer Wanted level hidden times count is invalid in data/dispatch.meta, need WANTED_CLEAN through WANTED_LEVEL5");
		Assertf(m_DispatchDataInstance.m_WantedResponses.GetCount() == WANTED_LEVEL5, "Wanted Level response count is invalid in data/dispatch.meta, need WANTED_LEVEL1 through WANTED_LEVEL5");
		Assertf(m_DispatchDataInstance.m_LawSpawnDelayMin.GetCount() == NUM_LAW_RESPONSE_DELAY_TYPES , "Not enough spawn delay values in data/dispatch.meta");
		Assertf(m_DispatchDataInstance.m_LawSpawnDelayMax.GetCount() == NUM_LAW_RESPONSE_DELAY_TYPES , "Not enough spawn delay values in data/dispatch.meta");
		Assertf(m_DispatchDataInstance.m_AmbulanceSpawnDelayMin.GetCount() == NUM_AMBULANCE_RESPONSE_DELAY_TYPES , "Not enough spawn delay values in data/dispatch.meta");
		Assertf(m_DispatchDataInstance.m_AmbulanceSpawnDelayMax.GetCount() == NUM_AMBULANCE_RESPONSE_DELAY_TYPES , "Not enough spawn delay values in data/dispatch.meta");
		Assertf(m_DispatchDataInstance.m_FireSpawnDelayMin.GetCount() == NUM_FIRE_RESPONSE_DELAY_TYPES , "Not enough spawn delay values in data/dispatch.meta");
		Assertf(m_DispatchDataInstance.m_FireSpawnDelayMax.GetCount() == NUM_FIRE_RESPONSE_DELAY_TYPES , "Not enough spawn delay values in data/dispatch.meta");

#if __BANK
        bkGroup *wantedLevelGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Game Logic/Wanted levels"));
        if(Verifyf(wantedLevelGroup, "Failed to find Wanted levels group!") && m_DispatchDataInstance.m_SingleplayerWantedLevelRadius.GetCount() == WANTED_LEVEL_LAST)
        {
            wantedLevelGroup->AddSlider("SP Zone Radius Wanted 1", &m_DispatchDataInstance.m_SingleplayerWantedLevelRadius[WANTED_LEVEL1], 0.0f, 3000.0f, 10.0f);
            wantedLevelGroup->AddSlider("SP Zone Radius Wanted 2", &m_DispatchDataInstance.m_SingleplayerWantedLevelRadius[WANTED_LEVEL2], 0.0f, 3000.0f, 10.0f);
            wantedLevelGroup->AddSlider("SP Zone Radius Wanted 3", &m_DispatchDataInstance.m_SingleplayerWantedLevelRadius[WANTED_LEVEL3], 0.0f, 3000.0f, 10.0f);
            wantedLevelGroup->AddSlider("SP Zone Radius Wanted 4", &m_DispatchDataInstance.m_SingleplayerWantedLevelRadius[WANTED_LEVEL4], 0.0f, 3000.0f, 10.0f);
            wantedLevelGroup->AddSlider("SP Zone Radius Wanted 5", &m_DispatchDataInstance.m_SingleplayerWantedLevelRadius[WANTED_LEVEL5], 0.0f, 3000.0f, 10.0f);

            wantedLevelGroup->AddSlider("MP Zone Radius Wanted 1", &m_DispatchDataInstance.m_MultiplayerWantedLevelRadius[WANTED_LEVEL1], 0.0f, 3000.0f, 10.0f);
            wantedLevelGroup->AddSlider("MP Zone Radius Wanted 2", &m_DispatchDataInstance.m_MultiplayerWantedLevelRadius[WANTED_LEVEL2], 0.0f, 3000.0f, 10.0f);
            wantedLevelGroup->AddSlider("MP Zone Radius Wanted 3", &m_DispatchDataInstance.m_MultiplayerWantedLevelRadius[WANTED_LEVEL3], 0.0f, 3000.0f, 10.0f);
            wantedLevelGroup->AddSlider("MP Zone Radius Wanted 4", &m_DispatchDataInstance.m_MultiplayerWantedLevelRadius[WANTED_LEVEL4], 0.0f, 3000.0f, 10.0f);
			wantedLevelGroup->AddSlider("MP Zone Radius Wanted 5", &m_DispatchDataInstance.m_MultiplayerWantedLevelRadius[WANTED_LEVEL5], 0.0f, 3000.0f, 10.0f);

			wantedLevelGroup->AddSlider("Hidden Timer Wanted 1", &m_DispatchDataInstance.m_HiddenEvasionTimes[WANTED_LEVEL1], 0, 200000, 10);
			wantedLevelGroup->AddSlider("Hidden Timer Wanted 2", &m_DispatchDataInstance.m_HiddenEvasionTimes[WANTED_LEVEL2], 0, 200000, 10);
			wantedLevelGroup->AddSlider("Hidden Timer Wanted 3", &m_DispatchDataInstance.m_HiddenEvasionTimes[WANTED_LEVEL3], 0, 200000, 10);
			wantedLevelGroup->AddSlider("Hidden Timer Wanted 4", &m_DispatchDataInstance.m_HiddenEvasionTimes[WANTED_LEVEL4], 0, 200000, 10);
			wantedLevelGroup->AddSlider("Hidden Timer Wanted 5", &m_DispatchDataInstance.m_HiddenEvasionTimes[WANTED_LEVEL5], 0, 200000, 10);

			if(m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes.GetCount() == WANTED_LEVEL_LAST)
			{
				wantedLevelGroup->AddSlider("MP Hidden Timer Wanted 1", &m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes[WANTED_LEVEL1], 0, 200000, 10);
				wantedLevelGroup->AddSlider("MP Hidden Timer Wanted 2", &m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes[WANTED_LEVEL2], 0, 200000, 10);
				wantedLevelGroup->AddSlider("MP Hidden Timer Wanted 3", &m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes[WANTED_LEVEL3], 0, 200000, 10);
				wantedLevelGroup->AddSlider("MP Hidden Timer Wanted 4", &m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes[WANTED_LEVEL4], 0, 200000, 10);
				wantedLevelGroup->AddSlider("MP Hidden Timer Wanted 5", &m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes[WANTED_LEVEL5], 0, 200000, 10);
			}

			wantedLevelGroup->AddSlider("Parole duration", &m_DispatchDataInstance.m_ParoleDuration, 0, 300000, 1000);
			wantedLevelGroup->AddSlider("In heli wanted radius multiplier", &m_DispatchDataInstance.m_InHeliRadiusMultiplier, 0.0f, 10.0f, 0.05f);
            wantedLevelGroup->AddSlider("Law spawn delay min (SLOW)", &m_DispatchDataInstance.m_LawSpawnDelayMin[LAW_RESPONSE_DELAY_SLOW], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Law spawn delay max (SLOW)", &m_DispatchDataInstance.m_LawSpawnDelayMax[LAW_RESPONSE_DELAY_SLOW], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Law spawn delay min (MEDIUM)", &m_DispatchDataInstance.m_LawSpawnDelayMin[LAW_RESPONSE_DELAY_MEDIUM], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Law spawn delay max (MEDIUM)", &m_DispatchDataInstance.m_LawSpawnDelayMax[LAW_RESPONSE_DELAY_MEDIUM], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Law spawn delay min (FAST)", &m_DispatchDataInstance.m_LawSpawnDelayMin[LAW_RESPONSE_DELAY_FAST], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Law spawn delay max (FAST)", &m_DispatchDataInstance.m_LawSpawnDelayMax[LAW_RESPONSE_DELAY_FAST], 0.0f, 100.0f, 0.1f);
        
			wantedLevelGroup->AddSlider("Ambulance spawn delay min (SLOW)", &m_DispatchDataInstance.m_AmbulanceSpawnDelayMin[AMBULANCE_RESPONSE_DELAY_SLOW], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Ambulance spawn delay max (SLOW)", &m_DispatchDataInstance.m_AmbulanceSpawnDelayMax[AMBULANCE_RESPONSE_DELAY_SLOW], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Ambulance spawn delay min (MEDIUM)", &m_DispatchDataInstance.m_AmbulanceSpawnDelayMin[AMBULANCE_RESPONSE_DELAY_MEDIUM], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Ambulance spawn delay max (MEDIUM)", &m_DispatchDataInstance.m_AmbulanceSpawnDelayMax[AMBULANCE_RESPONSE_DELAY_MEDIUM], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Ambulance spawn delay min (FAST)", &m_DispatchDataInstance.m_AmbulanceSpawnDelayMin[AMBULANCE_RESPONSE_DELAY_FAST], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Ambulance spawn delay max (FAST)", &m_DispatchDataInstance.m_AmbulanceSpawnDelayMax[AMBULANCE_RESPONSE_DELAY_FAST], 0.0f, 100.0f, 0.1f);

			wantedLevelGroup->AddSlider("Fire spawn delay min (SLOW)", &m_DispatchDataInstance.m_LawSpawnDelayMin[FIRE_RESPONSE_DELAY_SLOW], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Fire spawn delay max (SLOW)", &m_DispatchDataInstance.m_LawSpawnDelayMax[FIRE_RESPONSE_DELAY_SLOW], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Fire spawn delay min (MEDIUM)", &m_DispatchDataInstance.m_LawSpawnDelayMin[FIRE_RESPONSE_DELAY_MEDIUM], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Fire spawn delay max (MEDIUM)", &m_DispatchDataInstance.m_LawSpawnDelayMax[FIRE_RESPONSE_DELAY_MEDIUM], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Fire spawn delay min (FAST)", &m_DispatchDataInstance.m_LawSpawnDelayMin[FIRE_RESPONSE_DELAY_FAST], 0.0f, 100.0f, 0.1f);
			wantedLevelGroup->AddSlider("Fire spawn delay max (FAST)", &m_DispatchDataInstance.m_LawSpawnDelayMax[FIRE_RESPONSE_DELAY_FAST], 0.0f, 100.0f, 0.1f);

		}
#endif
    }

	//Init dispatch timers, we don't want a fire truck instantly spawning
	CDispatchService* pFireDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_FireDepartment);
	if(pFireDispatchService)
	{
		pFireDispatchService->ResetSpawnTimer();
	}

	//Init dispatch timers, we don't want an ambulance instantly spawning
	CDispatchService* pAmbulanceDispatchService = CDispatchManager::GetInstance().GetDispatch(DT_AmbulanceDepartment);
	if(pAmbulanceDispatchService)
	{
		pAmbulanceDispatchService->ResetSpawnTimer();
	}

    // Init the wanted system static variables
    CWanted::InitialiseStaticVariables();
}

//////////////////////////////////////////////////////////////////////////

bool CDispatchData::LoadWantedData()
{
    // Load  files
    const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DISPATCH_DATA_FILE);

    if(DATAFILEMGR.IsValid(pData))
    {
        // Load
        if(Verifyf(PARSER.LoadObject(pData->m_filename, "meta", m_DispatchDataInstance), "Dispatch data file, %s, could not load.", pData->m_filename))
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

float CDispatchData::GetWantedZoneRadius(s32 iWantedLevel, const CPed* pPlayerPed) 
{ 
    Assert(iWantedLevel>=0 && iWantedLevel < WANTED_LEVEL_LAST);

	float fRadius = 0.0f;
    if(!NetworkInterface::IsGameInProgress())
    {
        if(m_DispatchDataInstance.m_SingleplayerWantedLevelRadius.GetCount() == WANTED_LEVEL_LAST)
        {
            fRadius = m_DispatchDataInstance.m_SingleplayerWantedLevelRadius[iWantedLevel]; 
        }
    }
    else
    {
        if(m_DispatchDataInstance.m_MultiplayerWantedLevelRadius.GetCount() == WANTED_LEVEL_LAST)
        {
            fRadius = m_DispatchDataInstance.m_MultiplayerWantedLevelRadius[iWantedLevel]; 
        }
    }

	if(pPlayerPed && pPlayerPed->GetVehiclePedInside() && pPlayerPed->GetVehiclePedInside()->InheritsFromHeli())
	{
		fRadius *= GetInHeliRadiusMultiplier();
	}

    return fRadius;
}

//////////////////////////////////////////////////////////////////////////

int CDispatchData::GetLawResponseDelayTypeFromString(char* pString)
{
	if(stricmp(pString, "MEDIUM") == 0)
	{
		return LAW_RESPONSE_DELAY_MEDIUM;
	}
	else if(stricmp(pString, "SLOW") == 0)
	{
		return LAW_RESPONSE_DELAY_SLOW;
	}
	else if(stricmp(pString, "FAST") == 0)
	{
		return LAW_RESPONSE_DELAY_FAST;
	}
	else
	{
		Assertf(0, "Invalid law response delay type (%s)", pString);
	}

	return LAW_RESPONSE_DELAY_MEDIUM;
}

//////////////////////////////////////////////////////////////////////////

int CDispatchData::GetVehicleResponseTypeFromString(char* pString)
{
	if(stricmp(pString, "VEHICLE_DEFAULT") == 0)
	{
		return VEHICLE_RESPONSE_DEFAULT;
	}
	else if(stricmp(pString, "VEHICLE_COUNTRYSIDE") == 0)
	{
		return VEHICLE_RESPONSE_COUNTRYSIDE;
	}
	else if(stricmp(pString, "VEHICLE_ARMY_BASE") == 0)
	{
		return VEHICLE_RESPONSE_ARMY_BASE;
	}
	else
	{
		Assertf(0, "Invalid vehicle response type (%s)", pString);
	}

	return VEHICLE_RESPONSE_DEFAULT;
}

//////////////////////////////////////////////////////////////////////////

float CDispatchData::GetMaxLawSpawnDelay(int iLawResponseDelayType)
{
	Assertf(iLawResponseDelayType >= 0 && iLawResponseDelayType < m_DispatchDataInstance.m_LawSpawnDelayMax.GetCount(),
		"Law response delay type %d is out of bounds. Bad type or not enough Law Spawn Delays defined in dispatch.meta",iLawResponseDelayType);

	return m_DispatchDataInstance.m_LawSpawnDelayMax[iLawResponseDelayType];
}

//////////////////////////////////////////////////////////////////////////

float CDispatchData::GetRandomLawSpawnDelay(int iLawResponseDelayType)
{
	Assertf(iLawResponseDelayType >= 0 && iLawResponseDelayType < m_DispatchDataInstance.m_LawSpawnDelayMax.GetCount() && 
		    iLawResponseDelayType < m_DispatchDataInstance.m_LawSpawnDelayMin.GetCount(),
			"Law response delay type %d is out of bounds. Bad type or not enough Law Spawn Delays defined in dispatch.meta",iLawResponseDelayType);

    return fwRandom::GetRandomNumberInRange(m_DispatchDataInstance.m_LawSpawnDelayMin[iLawResponseDelayType], 
											m_DispatchDataInstance.m_LawSpawnDelayMax[iLawResponseDelayType]);
}

//////////////////////////////////////////////////////////////////////////

float CDispatchData::GetRandomAmbulanceSpawnDelay(int iAmbulanceResponseDelayType)
{
	Assertf(iAmbulanceResponseDelayType >= 0 && iAmbulanceResponseDelayType < m_DispatchDataInstance.m_AmbulanceSpawnDelayMax.GetCount() && 
		iAmbulanceResponseDelayType < m_DispatchDataInstance.m_AmbulanceSpawnDelayMin.GetCount(),
		"Ambulance response delay type %d is out of bounds. Bad type or not enough Ambulance Spawn Delays defined in dispatch.meta",iAmbulanceResponseDelayType);

	return fwRandom::GetRandomNumberInRange(m_DispatchDataInstance.m_AmbulanceSpawnDelayMin[iAmbulanceResponseDelayType], 
		m_DispatchDataInstance.m_AmbulanceSpawnDelayMax[iAmbulanceResponseDelayType]);
}

//////////////////////////////////////////////////////////////////////////

float CDispatchData::GetRandomFireSpawnDelay(int iFireResponseDelayType)
{
	Assertf(iFireResponseDelayType >= 0 && iFireResponseDelayType < m_DispatchDataInstance.m_FireSpawnDelayMax.GetCount() && 
		iFireResponseDelayType < m_DispatchDataInstance.m_LawSpawnDelayMin.GetCount(),
		"Fire response delay type %d is out of bounds. Bad type or not enough Fire Spawn Delays defined in dispatch.meta",iFireResponseDelayType);

	return fwRandom::GetRandomNumberInRange(m_DispatchDataInstance.m_FireSpawnDelayMin[iFireResponseDelayType], 
		m_DispatchDataInstance.m_FireSpawnDelayMax[iFireResponseDelayType]);
}


//////////////////////////////////////////////////////////////////////////

float CDispatchData::GetOnFootOrderDistance(DispatchType eDispatchType)
{
    Assert(eDispatchType > DT_Invalid && eDispatchType < DT_Max);
    const int num = m_DispatchDataInstance.m_DispatchOrderDistances.GetCount();
    for (s32 i=0; i<num; i++)
    {
        if (m_DispatchDataInstance.m_DispatchOrderDistances[i].m_DispatchType == eDispatchType)
        {
            return m_DispatchDataInstance.m_DispatchOrderDistances[i].m_OnFootOrderDistance;
        }
    }
    Assertf(0, "Couldn't find on foot order distance for dispatch %i", (s32)eDispatchType);
    return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CDispatchData::GetMinMaxInVehicleOrderDistances(DispatchType eDispatchType, float& fMinDistance, float& fMaxDistance)
{
    Assert(eDispatchType > DT_Invalid && eDispatchType < DT_Max);
    const int num = m_DispatchDataInstance.m_DispatchOrderDistances.GetCount();
    for (s32 i=0; i<num; i++)
    {
        if (m_DispatchDataInstance.m_DispatchOrderDistances[i].m_DispatchType == eDispatchType)
        {
            fMinDistance = m_DispatchDataInstance.m_DispatchOrderDistances[i].m_MinInVehicleOrderDistance;
            fMaxDistance = m_DispatchDataInstance.m_DispatchOrderDistances[i].m_MaxInVehicleOrderDistance;
            Assertf(fMinDistance <= fMaxDistance, "Min In Vehicle Order Dist Should Be less than or the same as Max");
            return true;
        }
    }
    Assertf(0, "Couldn't find in vehicle order distance for dispatch %i", (s32)eDispatchType);
    return false;
}

//////////////////////////////////////////////////////////////////////////

s32 CDispatchData::GetWantedLevelThreshold(s32 iWantedLevel) 
{ 
    Assert(iWantedLevel>=0 && iWantedLevel < WANTED_LEVEL_LAST);
    if(m_DispatchDataInstance.m_WantedLevelThresholds.GetCount() == WANTED_LEVEL_LAST)
    {
        return m_DispatchDataInstance.m_WantedLevelThresholds[iWantedLevel];
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////

u32 CDispatchData::GetHiddenEvasionTime(s32 iWantedLevel) 
{ 
	Assert(iWantedLevel>=0 && iWantedLevel < WANTED_LEVEL_LAST);

	u32 evasionTime = 0;
	
	if(!NetworkInterface::IsGameInProgress())
	{
		if(m_DispatchDataInstance.m_HiddenEvasionTimes.GetCount() == WANTED_LEVEL_LAST)
		{
			evasionTime = m_DispatchDataInstance.m_HiddenEvasionTimes[iWantedLevel];
		}
	}
	else
	{
		if(m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes.GetCount() == WANTED_LEVEL_LAST)
		{
			evasionTime = m_DispatchDataInstance.m_MultiplayerHiddenEvasionTimes[iWantedLevel];
		}
		else if(m_DispatchDataInstance.m_HiddenEvasionTimes.GetCount() == WANTED_LEVEL_LAST)
		{
			evasionTime = m_DispatchDataInstance.m_HiddenEvasionTimes[iWantedLevel];
		}

		CWanted* pWanted = CGameWorld::FindLocalPlayerWanted();
		if (pWanted)
		{
			//Use evasion time override if it is set
			u32 hiddenEvasionTime = pWanted->m_Overrides.GetMultiplayerHiddenEvasionTimesOverride(iWantedLevel);
			if (hiddenEvasionTime > 0)
			{
				evasionTime = hiddenEvasionTime;
			}
		}

		static u32 wantedModifierHash[WANTED_LEVEL_LAST] = 
		{
			ATSTRINGHASH("WANTED_LEVEL_CLEAN_HIDDEN_EVASION_MODIFIER", 0xa7271636),
			ATSTRINGHASH("WANTED_LEVEL_1_HIDDEN_EVASION_MODIFIER", 0xdb3d3860),
			ATSTRINGHASH("WANTED_LEVEL_2_HIDDEN_EVASION_MODIFIER", 0x4a018131),
			ATSTRINGHASH("WANTED_LEVEL_3_HIDDEN_EVASION_MODIFIER", 0xd41fc0d9),
			ATSTRINGHASH("WANTED_LEVEL_4_HIDDEN_EVASION_MODIFIER", 0x52a87bbd),
			ATSTRINGHASH("WANTED_LEVEL_5_HIDDEN_EVASION_MODIFIER", 0x0dac44f5),
		};

		//! Cloud tunable modification.
		float fDefaultMultiplier = 1.0f;
		float fMultiplier = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, wantedModifierHash[iWantedLevel], fDefaultMultiplier);
		
		evasionTime = (u32)((float)evasionTime * fMultiplier);
	}
	
	return evasionTime;
}

//////////////////////////////////////////////////////////////////////////
// PURPOSE: Get a wanted response for a specific wanted level
CWantedResponse* CDispatchData::GetWantedResponse(s32 iWantedLevel) 
{ 
    Assert(iWantedLevel>=0 && iWantedLevel < WANTED_LEVEL_LAST);
    if(iWantedLevel > 0 && m_DispatchDataInstance.m_WantedResponses.GetCount() == (WANTED_LEVEL_LAST - 1))
    {
        iWantedLevel--;
        return &m_DispatchDataInstance.m_WantedResponses[iWantedLevel];
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
// PURPOSE: Get a emergency response for a specific dispatch type
CDispatchResponse* CDispatchData::GetEmergencyResponse(DispatchType eDispatchType)
{
    for(int i = 0; i < m_DispatchDataInstance.m_EmergencyResponses.GetCount(); i++)
    {
        if(m_DispatchDataInstance.m_EmergencyResponses[i].GetType() == eDispatchType)
        {
            return &m_DispatchDataInstance.m_EmergencyResponses[i];
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
// PURPOSE: To find a vehicle set based on a hash
CConditionalVehicleSet* CDispatchData::FindConditionalVehicleSet(u32 name)
{
	//Set the current zone type.
	eZoneVehicleResponseType nCurrentZoneType = (eZoneVehicleResponseType)CPopCycle::GetCurrentZoneVehicleResponseType();
	
	//Set the current game type.
	eGameVehicleResponseType nCurrentGameType = NetworkInterface::IsGameInProgress() ? VEHICLE_RESPONSE_GAMETYPE_MP : VEHICLE_RESPONSE_GAMETYPE_SP;
	
	//Iterate over the conditional vehicle sets.
    for(int i = 0; i < m_DispatchDataInstance.m_VehicleSets.GetCount(); i++)
    {
        if(m_DispatchDataInstance.m_VehicleSets[i].GetNameHash() == name)
		{
			atArray<CConditionalVehicleSet>& aConditionalVehicleSets = m_DispatchDataInstance.m_VehicleSets[i].GetConditionalVehicleSets();
			for(int j = 0; j < aConditionalVehicleSets.GetCount(); j++)
			{
				//Grab the conditional vehicle set.
				CConditionalVehicleSet& rConditionalVehicleSet = aConditionalVehicleSets[j];
				
				//Ensure the game type matches.
				eGameVehicleResponseType nConditionalGameType = aConditionalVehicleSets[j].GetGameType();
				if(nConditionalGameType != VEHICLE_RESPONSE_GAMETYPE_ALL && (nConditionalGameType != nCurrentGameType))
				{
					continue;
				}
				
				//Ensure the zone type matches.
				eZoneVehicleResponseType nConditionalZoneType = aConditionalVehicleSets[j].GetZoneType();
				if(nConditionalZoneType != VEHICLE_RESPONSE_DEFAULT && (nConditionalZoneType != nCurrentZoneType))
				{
					continue;
				}
				
				return &rConditionalVehicleSet;
			}
        }
    }
    
    return NULL;
}

sDispatchModelVariations* CDispatchData::FindDispatchModelVariations(CPed& rPed)
{
	return m_DispatchDataInstance.m_PedVariations.SafeGet(rPed.GetPedModelInfo()->GetModelNameHash());
}
