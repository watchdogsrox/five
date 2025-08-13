/////////////////////////////////////////////////////////////////////////////////
// Title	:	PopCycle.h
// Author	:	Obbe, Adam Croston
// Started	:	5/12/03
// Purpose	:	To keep track of the current population disposition.
/////////////////////////////////////////////////////////////////////////////////
#include "PopCycle.h"
// Parser file
#include "PopCycle_parser.h"


// Std headers
#include <Stdio.h>
#include <String.h>

// Rage headers
#include "fwsys/fileExts.h"
#include "parser/restparserservices.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/random.h"
#include "fwSys/fileExts.h"

// Game headers
#include "ai/ambient/AmbientModelSet.h"
#include "Control/gamelogic.h"
#include "Core/game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Debug/debug.h"
#include "Game/cheat.h"
#include "Game/clock.h"
#include "Game/Dispatch/DispatchData.h"
#include "Game/modelindices.h"
#include "Game/Performance.h"
#include "game/PopMultiplierAreas.h"
#include "Game/weather.h"
#include "Game/zones.h"
#include "modelinfo/MloModelInfo.h"
#include "ModelInfo/pedmodelinfo.h"
#include "ModelInfo/vehiclemodelinfo.h"
#include "network/NetworkInterface.h"
#include "peds/Ped.h"
#include "Peds/pedpopulation.h"
#include "Peds/PedDebugVisualiser.h" // for CPedDebugVisualiserMenu::GetForceAllCops();
#include "peds/popzones.h"
#include "Peds/WildlifeManager.h"
//#include "Renderer/font.h"
#include "Renderer/zonecull.h"
#include "Scene/datafilemgr.h"
#include "Scene/fileloader.h"
#include "peds/PlayerInfo.h"
#include "scene/focusentity.h"
#include "scene/world/gameWorld.h"
#include "Script/script.h"
#include "script/script_cars_and_peds.h"
#include "Streaming/populationstreaming.h"
#include "Streaming/streaming.h"
#include "System/filemgr.h"
#include "vehicles/Automobile.h"
#include "vehicles/cargen.h"
#include "vehicles/vehicle.h"
#include "Vehicles/vehiclepopulation.h"
#include "Vfx/metadata/VfxRegionInfo.h"
#include "scene/dlc_channel.h"

AI_OPTIMISATIONS()


CPopModelAndVariations::~CPopModelAndVariations() 
{ 
	delete m_Variations; 
}

CPopModelAndVariations& CPopModelAndVariations::CopyTo(const CPopModelAndVariations& rhs)
{
	m_Name = rhs.m_Name;

	if (rhs.m_Variations)
	{
		m_Variations = rhs.m_Variations->Clone();
		Assertf(m_Variations, "CPopModelAndVariations - Clone failed!");
	}

	return *this;
}

//////////////////////////////////////////////////////////////////////////
// CPopGroup
//////////////////////////////////////////////////////////////////////////



#if __BANK
float CPopAllocation::sm_maxPedsMultiplier = 1.0f;
float CPopAllocation::sm_maxCarsMultiplier = 1.0f;
u8 CPopAllocation::sm_forceMaxPeds = 0;
u8 CPopAllocation::sm_forceMaxCars = 0;
bool CPopCycle::sm_enableOverridePedDirtScale = false;
float CPopCycle::sm_overridePedDirtScale = 0.0f;
bool CPopCycle::sm_enableOverrideVehDirtScale = false;
float CPopCycle::sm_overridevehDirtScale = 0.0f;
bool CPopCycle::sm_enableOverrideDirtColor = false;
Color32 CPopCycle::sm_overrideDirtColor;
#endif

u32	CPopCycle::sm_popSchedulesMaxNumCars = 0;
bool CPopCycle::sm_populationChanged = true;
int CPopCycle::sm_nCurrentZoneVehicleResponseType = VEHICLE_RESPONSE_DEFAULT;
s32 CPopCycle::sm_nCurrentZonePopScheduleIndex = -1;
s32 CPopCycle::sm_nCurrentDaySubdivision = 0;
s32 CPopCycle::sm_nCurrentWeekSubdivision = 0;
atArray<u8> CPopCycle::sm_currentPedPercentages;
atArray<u8> CPopCycle::sm_currentVehPercentages;

#if !__FINAL
atFinalHashString CPopCycle::sm_nCurrentZoneName;
#endif

CPopZone *CPopCycle::sm_pCurrZone   = NULL;
const CPopAllocation *CPopCycle::sm_pCurrPopAllocation = NULL;
const CPopSchedule* CPopCycle::sm_pCurrPopSchedule = NULL;
s32 CPopCycle::sm_currentMaxNumCopPeds, CPopCycle::sm_currentMaxNumAmbientPeds, CPopCycle::sm_currentMaxNumScenarioPeds;
u32 CPopCycle::sm_currentNumPedsInCombat = 0;
bank_bool CPopCycle::sm_bUseCombatMultiplier = true;
bank_u32 CPopCycle::sm_iMinPedsInCombatForPopMultiplier = 8;
bank_u32 CPopCycle::sm_iMaxPedsInCombatForPopMultiplier = 24;
bank_float CPopCycle::sm_fPedsInCombatMinPopMultiplier = 0.5f;
bank_float CPopCycle::sm_fMinWantedMultiplierMP = 0.6f;
CPopGroupList CPopCycle::sm_popGroups;
atArray<atHashString> CPopCycle::sm_popGroupPatches;
CPopScheduleList CPopCycle::sm_popSchedules;

#define POPULATION_RECALCULATE_COUNTDOWN 3 // number of times to use the cached result (not including the time it was calculated) -> 0 = no caching
u8 CPopCycle::sm_PopulationRecalculateCountdown = 0;
u64 CPopCycle::sm_CurrentPopZoneId = 0;
u32 CPopCycle::sm_CachedMostWantedPopGroup = 0xFFFFFFFF;

s32 CPopCycle::sm_preferredSpawnGroupForVehs = -1;

float CPopCycle::sm_distanceDeltaForUpdateSqr = 5.f * 5.f; // update the pop zone data every 5 meters
u32 CPopCycle::sm_nextTimeForUpdate = 0;
Vector3 CPopCycle::sm_lastUpdatePos;
u32 CPopCycle::sm_lastTimePedsInCombatCounted = 0;

u32 CPopCycle::sm_commonVehicleGroup = ~0U;
u32 CPopCycle::sm_vehicleBikeGroup = ~0U;

#if !__FINAL
bool  CPopCycle::sm_bDisplayDebugInfo = false;
bool  CPopCycle::sm_bDisplayScenarioDebugInfo = false;
bool  CPopCycle::sm_bDisplaySimpleDebugInfo = false;
bool  CPopCycle::sm_bDisplayInVehDeadDebugInfo = false;
bool  CPopCycle::sm_bDisplayInVehNoPretendModelDebugInfo = false;
bool  CPopCycle::sm_bDisplayCarDebugInfo = false;
bool  CPopCycle::sm_bEnableVehPopDebugOutput = false;
bool  CPopCycle::sm_bDisplayCarSimpleDebugInfo = false;
bool  CPopCycle::sm_bDisplayCarPopulationFailureCounts = false;
bool  CPopCycle::sm_bDisplayPedPopulationFailureCounts = false;
bool  CPopCycle::sm_bDisplayScenarioPedPopulationFailureCountsOnVectorMap = false;
bool  CPopCycle::sm_bDisplayScenarioVehiclePopulationFailureCountsOnVectorMap = false;
bool  CPopCycle::sm_bDisplayWanderingPedPopulationFailureCountsOnVectorMap = false;
bool  CPopCycle::sm_bDisplayPedPopulationFailureCountsDebugSpew= false;
bool  CPopCycle::sm_bDisplayScenarioPedPopulationFailuresInTheWorld = false;
#endif	

XPARAM(maxVehicles);
XPARAM(maxPeds);
PARAM(pedMult, "Multiplier for ped counts in pop-cycle");
PARAM(carMult, "Multiplier for car counts in pop-cycle");

////////////////////////////////////////////////////////////////////////////////
// Data file loader interface
////////////////////////////////////////////////////////////////////////////////
class CPopulationDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		dlcDebugf3("CPopulationDataFileMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);
		switch(file.m_fileType)
		{
			case CDataFileMgr::DLC_POP_GROUPS: 
				CPopCycle::LoadPopGroupsFile(file.m_filename); 
				break;

			case CDataFileMgr::POPGRP_FILE_PATCH: 
				CPopCycle::LoadPopGroupPatch(file.m_filename); 
				break;

			case CDataFileMgr::POPSCHED_FILE:
				CPopCycle::LoadPopSchedFile(file.m_filename);
				break;

			case CDataFileMgr::ZONEBIND_FILE:
				CPopCycle::LoadZoneBindFile(file.m_filename);
				break;

			default: 
				return false;
		}
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file)
	{
		dlcDebugf3("CPopulationDataFileMounter::UnloadDataFile %s type = %d", file.m_filename, file.m_fileType);
		switch(file.m_fileType)
		{
			case CDataFileMgr::DLC_POP_GROUPS: 
				CPopCycle::UnloadPopGroupsFile(file.m_filename); 
				break;

			case CDataFileMgr::POPGRP_FILE_PATCH: 
				CPopCycle::UnloadPopGroupPatch(file.m_filename); 
				break;

			case CDataFileMgr::POPSCHED_FILE:
				CPopCycle::UnloadPopSchedFile(file.m_filename);
				break;

			case CDataFileMgr::ZONEBIND_FILE:
				CPopCycle::UnloadZoneBindFile(file.m_filename);
				break;

			default: 
				break;
		}
	}
} g_PopulationDataFileMounter;

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CpopAllocation
// Purpose		:	To store a description of a current population disposition.
//					Namely, their counts and percentages.
/////////////////////////////////////////////////////////////////////////////////
void CPopAllocation::Reset()
{
	m_nMaxNumAmbientPeds		= 0;
	m_nMaxNumScenarioPeds		= 0;
	m_nPercentageOfMaxCars		= 0;
	m_nMaxNumParkedCars			= 0;
	m_nMaxNumLowPrioParkedCars	= 0;
	m_nPercCopsCars				= 0;
	m_nPercCopsPeds				= 0;
	m_nMaxScenarioPedModels		= 0;
	m_nMaxScenarioVehicleModels	= 2;
	m_nMaxPreassignedParkedCars = 1;
	m_pedGroupPercentages.Reset();
	m_vehGroupPercentages.Reset();
}

void CPopAllocation::NormalizePercentages()
{
	NormalizePercentages(m_pedGroupPercentages);
	NormalizePercentages(m_vehGroupPercentages);
}

void CPopAllocation::NormalizePercentages(atArray<CPopGroupPercentage>& percentages)
{
	// Normalize the values to 100%
	const u32 groupCount = percentages.GetCount();

	Assertf(groupCount != 0, "Empty pop group found - possibly a problem in popcycle.dat?");
	if (groupCount == 0)
	{
		return;
	}

	s32 totalVal = 0;
	for (u32 i = 0; i < groupCount; i++)
	{
		totalVal += percentages[i].m_percentage;
	}
	Assertf(totalVal > 0, "The percentages add up to 0");
	for (u32 i = 0; i < groupCount; i++)
	{
		percentages[i].m_percentage = (u8)(percentages[i].m_percentage * (100.0f / totalVal));
	}

	// Because of rounding errors we might have less than 100. Give the remainder to the highest one.
	totalVal = 0;
	s32 highestVal = 0, indexOfHighestVal = 0;
	for (u32 i = 0; i < groupCount; i++)
	{
		totalVal += percentages[i].m_percentage;
		if (percentages[i].m_percentage >= highestVal)
		{
			highestVal = percentages[i].m_percentage;
			indexOfHighestVal = i;
		}
	}
	Assert(totalVal <= 100);
	percentages[indexOfHighestVal].m_percentage += 100 - (u8)totalVal;
}

atHashWithStringNotFinal CPopAllocation::PickRandomGroup(const atArray<CPopGroupPercentage>& percentages) const
{
	s32 random = fwRandom::GetRandomNumberInRange(0, 100);
	u32 popGroupIndex = 0;
#if __ASSERT
	const u32 groupCount = percentages.GetCount();
#endif
	while(percentages[popGroupIndex].m_percentage < random)
	{
		random = random - (s32)percentages[popGroupIndex].m_percentage;
		popGroupIndex++;
		Assert(popGroupIndex < groupCount);
	}
	return popGroupIndex;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopSchedule
// Purpose		:	To store a series of a population dispositions over time.
/////////////////////////////////////////////////////////////////////////////////
CPopSchedule::CPopSchedule()
{
	Reset();
}

void CPopSchedule::Reset(void)
{
	// Clear the info in the group so we can re-use it.
	m_Name = atHashWithStringNotFinal::Null();

	for (int dayType = 0; dayType < POPCYCLE_WEEKSUBDIVISIONS; dayType++)
	{
		for (int timeType = 0; timeType < POPCYCLE_DAYSUBDIVISIONS; timeType++)
		{
			CPopAllocation& popAllocation = GetAllocation(dayType, timeType);
			popAllocation.Reset();
		}
	}
	m_overridePedPercentage = 0;
	m_overrideVehPercentage = 0;
	m_overrideVehModel      = fwModelId::MI_INVALID;
}

void CPopSchedule::SetOverridePedGroup(u32 overridePedGroup, u32 overridePercentage)
{
	Assertf(overridePercentage <=100, "Override pedgroup percentage %d is out of range", overridePercentage);
	m_overridePedPercentage = overridePercentage;
	m_OverridePedGroup = overridePedGroup;
	CPopCycle::SetPopulationChanged();
}

void CPopSchedule::SetOverrideVehGroup(u32 overrideVehGroup, u32 overridePercentage)
{
	Assertf(overridePercentage <=100, "Override vehgroup percentage %d is out of range", overridePercentage);
	m_overrideVehPercentage = overridePercentage;
	m_OverrideVehGroup = overrideVehGroup;
	CPopCycle::SetPopulationChanged();
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopScheduleList
// Purpose		:	To store a series of a named population schedules.
/////////////////////////////////////////////////////////////////////////////////

CPopScheduleList::CPopScheduleList()
{
	Reset();
}

void CPopScheduleList::Reset()
{
	const int popScheduleCount = m_schedules.GetCount();
	for(int popScheduleIndex = 0; popScheduleIndex < popScheduleCount; ++popScheduleIndex)
	{
		m_schedules[popScheduleIndex].Reset();
	}
	m_schedules.Reset();
}

bool CPopScheduleList::GetIndexFromNameHash (u32 popScheduleNameHash, s32& outPopScheduleIndex) const
{
	outPopScheduleIndex = INVALID_SCHEDULE;
	const s32 popScheduleCount = m_schedules.GetCount();
	for(s32 popScheduleIndex = 0; popScheduleIndex < popScheduleCount; ++popScheduleIndex)
	{
		if(m_schedules[popScheduleIndex].m_Name == popScheduleNameHash)
		{
			outPopScheduleIndex = popScheduleIndex;
			return true;
		}
	}
	return false;
}

void CPopScheduleList::Load (const char* fileName)
{
	FileHandle	fd;
	char line[1024];

	fd = CFileMgr::OpenFile(fileName, "rb");
	Assertf(CFileMgr::IsValidFileHandle(fd), "problem loading popcycle.dat");

	bool			bReadingSchedule		= false;
	bool			bReadingpopAllocation	= false;
	int				weekSubdivision			= 0;
	int				daySubdivision			= 0;
	CPopSchedule	popSchedule;

	s32 iMaxCarsValue = 0;


	while(CFileMgr::ReadLine(fd, &line[0], 1024))
	{
		// Check for empty or commented out lines...
		if(line[0] == '/' || line[0] == '\0' || line[1] == '\0' || line[2] == '\0')	// Note: ps3 returns empty lines as space,0
		{
			continue;
		}

		// Check if this is the beginning or end of a schedule.
		if(strncmp(line, "POP_SCHEDULE:", 13)==0)
		{
			// Update the reading state.
			bReadingSchedule		= true;
			bReadingpopAllocation	= false;
			weekSubdivision			= 0;
			daySubdivision			= 0;
			iMaxCarsValue			= 0;
			continue;
		}
		else if(strncmp(line, "END_POP_SCHEDULE", 16)==0)
		{
			// Add the group.
			m_schedules.PushAndGrow(popSchedule);

			// Clear the info in the group so we can re-use it.
			popSchedule.Reset();

			// Update the reading state.
			bReadingSchedule		= false;
			bReadingpopAllocation	= false;
			weekSubdivision			= 0;
			daySubdivision			= 0;
			continue;
		}

		// Depending on current state interpret the line.
		if( bReadingSchedule )
		{
			// Read the schedule name.
			popSchedule.m_Name = line;

			// Update the reading state.
			bReadingSchedule		= false;
			bReadingpopAllocation	= true;
			weekSubdivision			= 0;
			daySubdivision			= 0;
			continue;
		}
		else if( bReadingpopAllocation )
		{
			CPopAllocation& popAllocation = popSchedule.GetAllocation(weekSubdivision, daySubdivision);

			// Read the allotment line.
			const char* seps = " ,\t";
			char* pToken = NULL;
			pToken = strtok(line, seps);
			Assert(pToken);
			popAllocation.m_nMaxNumAmbientPeds = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nMaxNumScenarioPeds = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nPercentageOfMaxCars = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nMaxNumParkedCars = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nMaxNumLowPrioParkedCars= (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nPercCopsCars = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nPercCopsPeds = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nMaxScenarioPedModels = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nMaxScenarioVehicleModels = (u8)atoi(pToken);
			pToken = strtok(NULL, seps);
			Assert(pToken);
			popAllocation.m_nMaxPreassignedParkedCars = (u8)atoi(pToken);

			Assert(popAllocation.m_nPercentageOfMaxCars <= 100);

			iMaxCarsValue = Max(iMaxCarsValue, (s32)popAllocation.m_nPercentageOfMaxCars);

			pToken = strtok(NULL, seps);
			Assertf(!strcmp(pToken, "peds"), "corrupt popcycle.dat: couldn't find the ped tag");
			char* pGroupName = strtok(NULL, seps);
			bool bDoingPeds = true;
			while(pGroupName)
			{
				if(!strcmp(pGroupName, "cars"))
				{
					bDoingPeds = false;
					pGroupName = strtok(NULL, seps);
				}
				const char* pPercent = strtok(NULL, seps);
				if(pGroupName == NULL || pPercent == NULL)
					break;
				u32 popGroup = 0;
				int percentage = atoi(pPercent);
				if(bDoingPeds)
				{
					if(Verifyf(CPopCycle::GetPopGroups().FindPedGroupFromNameHash(atHashValue(pGroupName), popGroup), "Cannot find ped group %s in schedule %s",
						pGroupName, popSchedule.GetName().GetCStr()))
						popAllocation.m_pedGroupPercentages.PushAndGrow(CPopGroupPercentage(pGroupName, (u8)percentage));
				}
				else
				{
					if(Verifyf(CPopCycle::GetPopGroups().FindVehGroupFromNameHash(atHashValue(pGroupName), popGroup), "Cannot find car group %s in schedule %s",
						pGroupName, popSchedule.GetName().GetCStr()))
						popAllocation.m_vehGroupPercentages.PushAndGrow(CPopGroupPercentage(pGroupName, (u8)percentage));
				}

				pGroupName = strtok(NULL, seps);
			}
			popAllocation.NormalizePercentages(); // disable when saving!

			// Update the reading state.
			// Increment the time of day and day of week.
			bReadingSchedule		= false;
			bReadingpopAllocation	= true;
			++daySubdivision;
			if(daySubdivision == CPopSchedule::POPCYCLE_DAYSUBDIVISIONS)
			{
				daySubdivision = 0;
				++weekSubdivision;
			}
			continue;
		}
	}

	CFileMgr::CloseFile(fd);
}

void CPopScheduleList::PostLoad()
{
	/*
	m_percentageOfMaxCars = 0;
	for(int sch = 0; sch < m_schedules.GetCount(); sch++)
	{
		for (int dayType = 0; dayType < CPopSchedule::POPCYCLE_WEEKSUBDIVISIONS; dayType++)
		{
			for (int timeType = 0; timeType < CPopSchedule::POPCYCLE_DAYSUBDIVISIONS; timeType++)
			{
				CPopAllocation& popAllocation = m_schedules[sch].GetAllocation(dayType, timeType);
				if(popAllocation.m_nPercentageOfMaxCars > m_percentageOfMaxCars)
				{
					m_percentageOfMaxCars = popAllocation.m_nPercentageOfMaxCars;
				}
			}
		}
	}
	*/

	//Save("popcycle.datnew");
}

#if !__FINAL
void CPopScheduleList::Save(const char* fileName)
{
	char		line_out[2048];
	FileHandle	fd_out=INVALID_FILE_HANDLE;	

	fd_out = CFileMgr::OpenFileForWriting(fileName);
	Assertf(CFileMgr::IsValidFileHandle(fd_out), "problem opening popcycle.datnew");

	sprintf(line_out, "// For each schedule (Northwood, North Holland etc) we have a set of numbers that controls the ped densities.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// There is a set of values for each time of the day in 2 hour increments (midnight, 2am, 4 am etc)\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// There are 2 sets of values: Weekday & Weekend.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #Peds is the maximum number of ambient peds you can ever have.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #Scenario is the maximum number of scenario peds you can ever have.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #Cars is the maximum number of cars you can ever have.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #prkdcrs is the maximum number of parked cars you can ever have.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #lowprkdcrs is the maximum number of low priority parked cars you can ever have.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #PercCopCars is percentage of cars that are cop cars.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #PercCopPed is percentage of peds that are cops.\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	sprintf(line_out, "// #MaxScenPeds is the max number of ped models that may be streamed in for scenarios.\r\n\r\n\r\n");
	CFileMgr::Write(fd_out, line_out, istrlen(line_out));

	// DEBUG!! -AC, update this to be data driven...
	// Mention about the rest of the percentages.

	// List the popgroups that the pop cycle system is aware of.
	// END DEBUG!!

	for(int sch=0; sch<m_schedules.GetCount(); sch++)
	{
		const CPopSchedule& schedule = m_schedules[sch];
		sprintf(line_out, "\r\n////////////////////////////////////////////////////////////////////\r\n");
		CFileMgr::Write(fd_out, line_out, istrlen(line_out));
		sprintf(line_out, "POP_SCHEDULE:\r\n%s\r\n", schedule.GetName().GetCStr());
		CFileMgr::Write(fd_out, line_out, istrlen(line_out));

		for(int dayType=0; dayType<CPopSchedule::POPCYCLE_WEEKSUBDIVISIONS; dayType++)
		{
			if (dayType == 0)
				sprintf(line_out, "// Weekday\r\n//\r\n");
			else
				sprintf(line_out, "// Weekend\r\n//\r\n");
			CFileMgr::Write(fd_out, line_out, istrlen(line_out));
			sprintf(line_out, "//  #Peds  #Scenario #Cars  #prkdcrs #lowprkdcrs    PercCopCars PercCopPed  MaxScenPeds MaxScenVehs MaxPreAssignedParked\r\n");
			CFileMgr::Write(fd_out, line_out, istrlen(line_out));

			for(int timeType=0; timeType<CPopSchedule::POPCYCLE_DAYSUBDIVISIONS; timeType++)
			{
				const CPopAllocation& popAllocation = schedule.GetAllocation(dayType, timeType);
				// Write the pop allotment for this particular day subdivision.
				sprintf(line_out, "      %02d    %3d       %3d      %02d         %02d         %3d         %3d         %3d         %3d                %3d         ",
					(s32)(popAllocation.m_nMaxNumAmbientPeds),
					(s32)(popAllocation.m_nMaxNumScenarioPeds),
					(s32)(popAllocation.m_nPercentageOfMaxCars),
					(s32)(popAllocation.m_nMaxNumParkedCars),
					(s32)(popAllocation.m_nMaxNumLowPrioParkedCars),
					popAllocation.m_nPercCopsCars,
					popAllocation.m_nPercCopsPeds,
					popAllocation.m_nMaxScenarioPedModels,
					popAllocation.m_nMaxScenarioVehicleModels,
					popAllocation.m_nMaxPreassignedParkedCars);
				strcat(line_out, "  peds");
				for(int i=0; i<popAllocation.m_pedGroupPercentages.GetCount(); i++)
				{
					char groupInfoBuff[128];
					sprintf(groupInfoBuff, "  %s %02d", popAllocation.m_pedGroupPercentages[i].m_GroupName.GetCStr(), 
						popAllocation.m_pedGroupPercentages[i].m_percentage);
					strcat(line_out, groupInfoBuff);
				}
				strcat(line_out, "  cars");
				for(int i=0; i<popAllocation.m_vehGroupPercentages.GetCount(); i++)
				{
					char groupInfoBuff[128];
					sprintf(groupInfoBuff, "  %s %02d", popAllocation.m_vehGroupPercentages[i].m_GroupName.GetCStr(), 
						popAllocation.m_vehGroupPercentages[i].m_percentage);
					strcat(line_out, groupInfoBuff);
				}
				strcat(line_out, "\r\n");

				CFileMgr::Write(fd_out, line_out, istrlen(line_out));

			}
		}

		sprintf(line_out, "END_POP_SCHEDULE\r\n");
		CFileMgr::Write(fd_out, line_out, istrlen(line_out));
	}
	CFileMgr::CloseFile(fd_out);

}

const char* CPopScheduleList::GetNameFromIndex (s32 popScheduleIndex) const
{
	return m_schedules[popScheduleIndex].m_Name.GetCStr();
}
#endif // !__FINAL


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopulationGroup
// Purpose		:	List of popgroups.
/////////////////////////////////////////////////////////////////////////////////
CPopulationGroup::CPopulationGroup()
{
	Reset();
}

void CPopulationGroup::Reset()
{
	m_Name = atHashWithStringNotFinal::Null();
	m_flags = POPGROUP_AMBIENT|POPGROUP_SCENARIO;
	m_empty = false;

	m_models.Reset();
}

u32 CPopulationGroup::GetModelIndex(u32 index) const
{
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_models[index].m_Name, &modelId);
	return(modelId.GetModelIndex());
}

bool CPopulationGroup::IsIndexMember(u32 modelIndex) const
{
	u32 modelHash = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetHashKey();

	for (u32 i = 0; i < m_models.GetCount(); i++)
	{
		if (modelHash == m_models[i].m_Name)
		{
			return true;
		}
	}
	return false;
}


CAmbientModelVariations* CPopulationGroup::FindVariations(u32 modelIndex) const
{
	u32 modelHash = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)))->GetHashKey();

	for (u32 i = 0; i < m_models.GetCount(); i++)
	{
		if (modelHash == m_models[i].m_Name)
		{
			return m_models[i].m_Variations;
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopGroupList
// Purpose		:	List of popgroups.
/////////////////////////////////////////////////////////////////////////////////
CPopGroupList::CPopGroupList()
{
	Reset();
}

void CPopGroupList::Flush()
{
	// Purge if we already have something loaded
	if (GetPedCount() > 0 || GetVehCount() > 0)
	{
		CPopulationStreaming::FlushAllVehicleModelsHard();
	}
}

void CPopGroupList::Reset()
{
//	m_popGroups.Reset();
	m_pedGroups.Reset();
	m_vehGroups.Reset();
}

void CPopGroupList::LoadPatch(const char* file)
{
	CPopGroupList patchData;

	Flush();

	if (Verifyf(PARSER.LoadObject(file, NULL, patchData), "CPopGroupList::LoadPatch - could not load %s", file))
	{
		s32 wildLifeHabitatsCount = patchData.m_wildlifeHabitats.GetCount();
		s32 vehGroupsCount = patchData.m_vehGroups.GetCount();
		s32 pedGroupsCount = patchData.m_pedGroups.GetCount();

		for (s32 i = 0; i < wildLifeHabitatsCount; i++)
		{
			atHashString& habitatPatchData = patchData.m_wildlifeHabitats[i];

			if (Verifyf(m_wildlifeHabitats.Find(habitatPatchData) == -1, "CPopGroupList::LoadPatch - Duplicate wildlife %s in %s", habitatPatchData.GetCStr(), file))
				m_wildlifeHabitats.PushAndGrow(habitatPatchData);
		}

		for (s32 i = 0; i < vehGroupsCount; i++)
		{
			CPopulationGroup& vehGroupPatchData = patchData.m_vehGroups[i];

			if (Verifyf(m_vehGroups.Find(vehGroupPatchData) == -1, "CPopGroupList::LoadPatch - Duplicate vehicle %s in %s", vehGroupPatchData.m_Name.GetCStr(), file))
				m_vehGroups.PushAndGrow(vehGroupPatchData);
		}

		for (s32 i = 0; i < pedGroupsCount; i++)
		{
			CPopulationGroup& pedGroupPatchData = patchData.m_pedGroups[i];

			if (Verifyf(m_pedGroups.Find(pedGroupPatchData) == -1, "CPopGroupList::LoadPatch - Duplicate ped %s in %s", pedGroupPatchData.m_Name.GetCStr(), file))
				m_pedGroups.PushAndGrow(pedGroupPatchData);
		}
	}
}

void CPopGroupList::UnloadPatch(const char* file)
{
	CPopGroupList patchData;

	Flush();

	if (Verifyf(PARSER.LoadObject(file, NULL, patchData), "CPopGroupList::UnloadPatch - could not load %s", file))
	{
		s32 wildLifeHabitatsCount = patchData.m_wildlifeHabitats.GetCount();
		s32 vehGroupsCount = patchData.m_vehGroups.GetCount();
		s32 pedGroupsCount = patchData.m_pedGroups.GetCount();

		for (s32 i = 0; i < wildLifeHabitatsCount; i++)
			m_wildlifeHabitats.DeleteMatches(patchData.m_wildlifeHabitats[i]);

		for (s32 i = 0; i < vehGroupsCount; i++)
		{
			s32 index = m_vehGroups.Find(patchData.m_vehGroups[i]);

			if (index != -1)
			{
				m_vehGroups[index].Reset();
				m_vehGroups.Delete(index);
			}
		}

		for (s32 i = 0; i < pedGroupsCount; i++)
		{
			s32 index = m_pedGroups.Find(patchData.m_pedGroups[i]);

			if (index != -1)
			{
				m_pedGroups[index].Reset();
				m_pedGroups.Delete(index);
			}
		}
	}
}

/*u32 CPopGroupList::GetPedHash(u32 popGroupIndex, u32 pedWithinGroup) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());
	return m_popGroups[popGroupIndex].m_pedModels[pedWithinGroup];
}

u32 CPopGroupList::GetPedIndex(u32 popGroupIndex, u32 pedWithinGroup) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());

	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_popGroups[popGroupIndex].m_pedModels[pedWithinGroup], &modelId);
	return(modelId.GetModelIndex());
}

u32	CPopGroupList::GetNumPeds(u32 popGroupIndex) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());
	return m_popGroups[popGroupIndex].m_pedModels.GetCount();
}

u32 CPopGroupList::GetVehHash(u32 popGroupIndex, u32 vehWithinGroup) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());
	return m_popGroups[popGroupIndex].m_vehModels[vehWithinGroup];
}

u32 CPopGroupList::GetVehIndex(u32 popGroupIndex, u32 vehWithinGroup) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());

	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_popGroups[popGroupIndex].m_vehModels[vehWithinGroup], &modelId);
	return(modelId.GetModelIndex());
}

u32	CPopGroupList::GetNumVehs(u32 popGroupIndex) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());
	return m_popGroups[popGroupIndex].m_vehModels.GetCount();
}

bool CPopGroupList::IsPedIndexMember(u32 popGroupIndex, u32 pedModelIndex) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());
	const CPopGroup& popGroup = m_popGroups[popGroupIndex];
	u32 pedModelHash = CModelInfo::GetBaseModelInfo(fwModelId(pedModelIndex))->GetHashKey();

	const u32 numPedsInGroup = popGroup.m_pedModels.GetCount();
	for (u32 indexOfPedInGroup = 0; indexOfPedInGroup < numPedsInGroup; indexOfPedInGroup++)
	{
		if (pedModelHash == popGroup.m_pedModels[indexOfPedInGroup])
		{
			return true;
		}
	}
	return false;
}

bool CPopGroupList::IsVehIndexMember(u32 popGroupIndex, u32 vehModelIndex) const
{
	Assert(popGroupIndex < (u32)m_popGroups.GetCount());
	const CPopGroup& popGroup = m_popGroups[popGroupIndex];
	u32 vehModelHash = CModelInfo::GetBaseModelInfo(fwModelId(vehModelIndex))->GetHashKey();

	const u32 numVehsInGroup = popGroup.m_vehModels.GetCount();
	for (u32 indexOfVehInGroup = 0; indexOfVehInGroup < numVehsInGroup; indexOfVehInGroup++)
	{
		if (vehModelHash == popGroup.m_vehModels[indexOfVehInGroup])
		{
			return true;
		}
	}
	return false;
}*/

bool CPopGroupList::FindPedGroupFromNameHash(u32 pedGroupNameHash, u32& outPedGroupIndex) const
{
	const u32 pedGroupCount = m_pedGroups.GetCount();
	for(u32 pedGroupIndex = 0; pedGroupIndex < pedGroupCount; pedGroupIndex++)
	{
		if(m_pedGroups[pedGroupIndex].m_Name == pedGroupNameHash)
		{
			outPedGroupIndex = pedGroupIndex;
			return true;
		}
	}
	return false;
}

bool CPopGroupList::FindVehGroupFromNameHash(u32 vehGroupNameHash, u32& outVehGroupIndex) const
{
	const u32 vehGroupCount = m_vehGroups.GetCount();
	for(u32 vehGroupIndex = 0; vehGroupIndex < vehGroupCount; vehGroupIndex++)
	{
		if(m_vehGroups[vehGroupIndex].m_Name == vehGroupNameHash)
		{
			outVehGroupIndex = vehGroupIndex;
			return true;
		}
	}
	return false;
}

void CPopGroupList::PostLoad()
{

#if __ASSERT
	for(u32 i=0; i<m_pedGroups.GetCount(); i++)
	{
		CPopulationGroup& popGroup = m_pedGroups[i];
		for(u32 j=0; j<popGroup.GetCount(); j++)
		{
			Assertf(popGroup.GetModelIndex(j) != fwModelId::MI_INVALID, "Model %s in pedgroup %s doesn't exist", popGroup.GetModelName(j).GetCStr(), popGroup.GetName().GetCStr());
			Assertf(popGroup.GetModelIndex(j) == fwModelId::MI_INVALID || CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(popGroup.GetModelIndex(j))))->GetModelType() == MI_TYPE_PED, "Model %s in pedgroup %s is not a ped", popGroup.GetModelName(j).GetCStr(), popGroup.GetName().GetCStr());
		}
	}
	for(u32 i=0; i<m_vehGroups.GetCount(); i++)
	{
		CPopulationGroup& popGroup = m_vehGroups[i];
		for(u32 j=0; j<popGroup.GetCount(); j++)
		{
            Assertf(popGroup.GetModelIndex(j) != fwModelId::MI_INVALID, "Model %s in vehicle group %s doesn't exist", popGroup.GetModelName(j).GetCStr(), popGroup.GetName().GetCStr());
            Assertf(popGroup.GetModelIndex(j) == fwModelId::MI_INVALID || CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(popGroup.GetModelIndex(j))))->GetModelType() == MI_TYPE_VEHICLE, "Model %s in pedgroup %s is not a vehicle", popGroup.GetModelName(j).GetCStr(), popGroup.GetName().GetCStr());
		}
	}
#endif // __ASSERT
	// setup gang cars for gang car groups
	for (u32 i = 0;  i < m_vehGroups.GetCount(); i++)
	{
		if(!GetVehGroup(i).IsFlagSet(POPGROUP_IS_GANG))
		{
			continue;
		}


        m_vehGroups[i].SetIsEmpty(true);
		for (u32 e = 0; e < GetVehGroup(i).GetCount(); e++)
		{
			u32 mi = GetVehGroup(i).GetModelIndex(e);
			if (!CModelInfo::IsValidModelInfo(mi))
			{
				continue;
			}

			CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(mi)));

            m_vehGroups[i].SetIsEmpty(false);

			if (pModelInfo->GetGangPopGroup() >= 0)
			{
				continue;
			}

			pModelInfo->SetGangPopGroup(i);

			for (s32 n = 0; n < MAX_VEH_POSSIBLE_COLOURS; n++)
			{
				for (s32 t = 0; t < NUM_VEH_BASE_COLOURS; t++)
				{
					pModelInfo->SetPossibleColours(t, n, pModelInfo->GetPossibleColours(t, n+1));
				}
			}
			pModelInfo->SetNumPossibleColours(pModelInfo->GetNumPossibleColours()-1);
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////
// Name			:	CPopCycle
// Purpose		:	To keep track of the current population disposition, based
//					on CPopSchedules and and CPopGroups.
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// Name			:	InitLevel
// Purpose		:	Inits CPopCycle object
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::Init(unsigned initMode)
{
#if __BANK
	float multiplier = 1.0f;
	if(PARAM_maxVehicles.Get())
		CPopAllocation::SetForceMaxCars((u32) CVehicle::GetPool()->GetSize());
	if(PARAM_maxPeds.Get())
		CPopAllocation::SetForceMaxPeds((u32) CPed::GetPool()->GetSize());
	if(PARAM_pedMult.Get(multiplier))
		CPopAllocation::SetMaxPedsMultiplier(multiplier);
	if(PARAM_carMult.Get(multiplier))
		CPopAllocation::SetMaxCarsMultiplier(multiplier);
#endif

	Displayf("Intialising CPopCycle...\n");

	CDataFileMount::RegisterMountInterface(CDataFileMgr::DLC_POP_GROUPS, &g_PopulationDataFileMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::POPGRP_FILE_PATCH, &g_PopulationDataFileMounter, eDFMI_UnloadFirst);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::POPSCHED_FILE, &g_PopulationDataFileMounter, eDFMI_UnloadLast);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::ZONEBIND_FILE, &g_PopulationDataFileMounter, eDFMI_UnloadLast);

	if(initMode == INIT_AFTER_MAP_LOADED)
    {
        // Load the groups before the time cycle data as the
	    // time cycle data references the groups.
	    ResetPopGroupsAndSchedules();
	    LoadPopGroups();
	    LoadPopSchedules();
	    LoadPopZoneToPopScheduleBindings();
    }
    else if(initMode == INIT_SESSION)
    {
	    sm_nCurrentDaySubdivision = 0;
	    sm_nCurrentWeekSubdivision = 0;
	    sm_pCurrZone = 0;
		sm_nCurrentZoneVehicleResponseType = VEHICLE_RESPONSE_DEFAULT;
	    sm_nCurrentZonePopScheduleIndex = -1;
		sm_pCurrPopAllocation = NULL;
		sm_pCurrPopSchedule = NULL;
		sm_nextTimeForUpdate = 0;
		sm_lastTimePedsInCombatCounted = 0;

#if !__FINAL
	    sm_nCurrentZoneName.Clear();
	    sm_bDisplayDebugInfo = false;
	    sm_bDisplaySimpleDebugInfo = false;
		sm_bDisplayInVehDeadDebugInfo = false;
		sm_bDisplayInVehNoPretendModelDebugInfo = false;
		sm_bDisplayScenarioDebugInfo = false;
#endif
    }

	Displayf("CPopCycle ready\n");
}

#if __BANK
void CPopCycle::InitWidgets()
{
	bkBank *bank = CGameLogic::GetGameLogicBank();
	if(!bank)
	{
		Assertf(bank, "Somebody removed the CGameLogic RAG bank.");
		return;
	}

	bank->PushGroup("Pop Cycle", false);
	bank->AddButton("Reload zonebind.meta", ReloadPopZoneToPopScheduleBindings);
	bank->AddSlider("WANTED minimum multiplier (MP)", &sm_fMinWantedMultiplierMP, 0.0f, 1.0f, 0.01f);

	bank->AddToggle("Use COMBAT multiplier", &sm_bUseCombatMultiplier);
	bank->AddSlider("COMBAT minimum multiplier", &sm_fPedsInCombatMinPopMultiplier, 0.0, 1.0f, 0.01f);
	bank->AddSlider("COMBAT multiplier (min num peds)", &sm_iMinPedsInCombatForPopMultiplier, 0, 100, 1);
	bank->AddSlider("COMBAT multiplier (max num peds)", &sm_iMaxPedsInCombatForPopMultiplier, 0, 100, 1);

	bank->AddToggle("Overide ped dirt scale", &sm_enableOverridePedDirtScale);
	bank->AddSlider("Ped dirt scale",&sm_overridePedDirtScale, 0.0f, 1.0f, 0.1f);
	bank->AddToggle("Overide veh dirt scale", &sm_enableOverrideVehDirtScale);
	bank->AddSlider("Veh dirt scale",&sm_overridevehDirtScale, 0.0f, 1.0f, 0.1f);
	bank->AddToggle("Overide dirt color", &sm_enableOverrideDirtColor);
	bank->AddColor("Veh color",&sm_overrideDirtColor);
	
	bank->PopGroup();
}
#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// Name			:	ShutdownLevel
// Purpose		:	Shuts down CPopCycle object
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::Shutdown(unsigned shutdownMode)
{
	sm_popGroupPatches.Reset();

	if(shutdownMode == SHUTDOWN_SESSION)
	{
		for(int i=0; i<sm_popSchedules.GetCount(); i++)
		{
			sm_popSchedules.GetSchedule(i).SetOverridePedGroup(0, 0);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	Update
// Purpose		:	Updates game population cycle
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::Update()
{
	s32 OldDaySubdivision = sm_nCurrentDaySubdivision;
	s32 OldTimeOfWeek = sm_nCurrentWeekSubdivision;
	s32 OldZoneIndex = sm_nCurrentZonePopScheduleIndex;

	sm_nCurrentWeekSubdivision = CPopSchedule::POPCYCLE_WEEKDAY;

	/*
	switch (CClock::GetDayOfWeek())
	{	
		case Date::SATURDAY:
			sm_nCurrentWeekSubdivision = CPopSchedule::POPCYCLE_WEEKEND;
			break;
		case Date::SUNDAY:	// Sunday
			if (CClock::GetHour() < 20) sm_nCurrentWeekSubdivision = CPopSchedule::POPCYCLE_WEEKEND; else sm_nCurrentWeekSubdivision = CPopSchedule::POPCYCLE_WEEKDAY;
			break;
		case Date::MONDAY:// Intentional fall though.
		case Date::TUESDAY:// Intentional fall though.
		case Date::WEDNESDAY:// Intentional fall though.
		case Date::THURSDAY:
			sm_nCurrentWeekSubdivision = CPopSchedule::POPCYCLE_WEEKDAY;
			break;
		case Date::FRIDAY:	// Friday
			if (CClock::GetHour() < 20) sm_nCurrentWeekSubdivision = CPopSchedule::POPCYCLE_WEEKDAY; else sm_nCurrentWeekSubdivision = CPopSchedule::POPCYCLE_WEEKEND;
			break;
		default:
			Assert(0);
			break;
	}
	*/

	sm_nCurrentDaySubdivision = CClock::GetHour() / 2;

	UpdateCurrZone();

	// If population info has changed we might want to load in new cars/peds etc.
	if (sm_populationChanged ||
		OldDaySubdivision != sm_nCurrentDaySubdivision ||
		OldTimeOfWeek != sm_nCurrentWeekSubdivision ||
		OldZoneIndex != sm_nCurrentZonePopScheduleIndex)
	{
		gPopStreaming.HandlePopCycleInfoChange(OldZoneIndex != sm_nCurrentZonePopScheduleIndex);
        CVehiclePopulation::HandlePopCycleInfoChange();
		sm_populationChanged = false;
	}

	UpdatePercentages();

	CountNumPedsInCombat();

}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	UpdateCurrZone
// Purpose		:	Updates game population zone
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::UpdateCurrZone(void)
{
	UpdateCurrZoneFromCoors(CFocusEntityMgr::GetMgr().GetPopPos());
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	UpdateCurrZone
// Purpose		:	Updates game population zone
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::UpdateCurrZoneFromCoors(const Vector3 &playerPos)
{
	u32 curTime = fwTimer::GetTimeInMilliseconds();
	const float distSqr = playerPos.Dist2(sm_lastUpdatePos);
	if (curTime < sm_nextTimeForUpdate && distSqr < sm_distanceDeltaForUpdateSqr)
		return;

	sm_lastUpdatePos = playerPos;
	sm_nextTimeForUpdate = curTime + 1000; // don't update more often than once per second

    if(NetworkInterface::IsGameInProgress())
    {
		sm_pCurrZone   = CPopZones::FindSmallestForPosition(&playerPos, ZONECAT_ANY, ZONETYPE_MP);
        sm_nCurrentZonePopScheduleIndex = (sm_pCurrZone && sm_popSchedules.GetCount() > 0) ? CPopZones::GetPopZonePopScheduleIndexForMP(sm_pCurrZone) : -1;
    }
    else
    {
		sm_pCurrZone   = CPopZones::FindSmallestForPosition(&playerPos, ZONECAT_ANY, ZONETYPE_SP);
        sm_nCurrentZonePopScheduleIndex = (sm_pCurrZone && 	sm_popSchedules.GetCount() > 0) ? CPopZones::GetPopZonePopScheduleIndexForSP(sm_pCurrZone) : -1;
    }

	sm_nCurrentZoneVehicleResponseType = sm_pCurrZone ? sm_pCurrZone->m_vehicleResponseType : VEHICLE_RESPONSE_DEFAULT;

    if(sm_nCurrentZonePopScheduleIndex != -1)
	{
		sm_pCurrPopSchedule = &sm_popSchedules.GetSchedule(sm_nCurrentZonePopScheduleIndex);
		sm_pCurrPopAllocation = &sm_pCurrPopSchedule->GetAllocation(sm_nCurrentWeekSubdivision, sm_nCurrentDaySubdivision);
	}
	else
	{
		sm_pCurrPopSchedule = NULL;
		sm_pCurrPopAllocation = NULL;
	}

	gPopStreaming.SetMaxScenarioPedModelsLoadedPerSlot(sm_pCurrPopAllocation ? sm_pCurrPopAllocation->GetMaxScenarioPedModels() : MAX_NUMBER_STREAMED_SCENARIO_PEDS_PER_SLOT);

	// Setup the current zone popgroup percentages
	int pedSize = sm_popGroups.GetPedCount();
	for(int i=0; i<pedSize; i++)
		sm_currentPedPercentages[i] = 0;
	int vehSize = sm_popGroups.GetVehCount();
	for(int i=0; i<vehSize; i++)
		sm_currentVehPercentages[i] = 0;

	// if there is a valid pop allocation fill out percentages array
	if(HasValidCurrentPopAllocation())
	{
		const CPopAllocation& allocation = GetCurrentPopAllocation();
		
		// Set up ped group percentages (including overriding)
		u32 overridePedPercentage = sm_pCurrPopSchedule->GetOverridePedPercentage();
		float overridePedMultiplier = (100.0f - (float)overridePedPercentage) / 100.0f;
		int overridePedGroup = sm_pCurrPopSchedule->GetOverridePedGroup();
		u32 totalPedPercentage = 0;

		for(int i=0; i<allocation.GetNumberOfPedGroups(); i++)
		{
			u32 pedGroupIndex;
			if(sm_popGroups.FindPedGroupFromNameHash(allocation.GetPedGroupPercentage(i).m_GroupName, pedGroupIndex))
			{
				sm_currentPedPercentages[pedGroupIndex] = (u8)(allocation.GetPedGroupPercentage(i).m_percentage * overridePedMultiplier);
				totalPedPercentage += sm_currentPedPercentages[pedGroupIndex];
			}
		}
		if(overridePedPercentage > 0)
		{
			sm_currentPedPercentages[overridePedGroup] += (u8)(100 - totalPedPercentage);
		}

		// Set up veh group percentages (including overriding)
		u32 overrideVehPercentage = sm_pCurrPopSchedule->GetOverrideVehPercentage();
		float overrideVehMultiplier = (100.0f - (float)overrideVehPercentage) / 100.0f;
		int overrideVehGroup = sm_pCurrPopSchedule->GetOverrideVehGroup();
		u32 totalVehPercentage = 0;

		for(int i=0; i<allocation.GetNumberOfVehGroups(); i++)
		{
			u32 vehGroupIndex;
			if(sm_popGroups.FindVehGroupFromNameHash(allocation.GetVehGroupPercentage(i).m_GroupName, vehGroupIndex))
			{
                if (sm_popGroups.GetVehGroup(vehGroupIndex).IsEmpty())
                {
                    sm_currentVehPercentages[vehGroupIndex] = 0;
                }
                else
                {
                    sm_currentVehPercentages[vehGroupIndex] = (u8)(allocation.GetVehGroupPercentage(i).m_percentage * overrideVehMultiplier);
                    totalVehPercentage += sm_currentVehPercentages[vehGroupIndex];
                }
			}
		}
		if(overrideVehPercentage > 0)
		{
			sm_currentVehPercentages[overrideVehGroup] += (u8)(100 - totalVehPercentage);
		}
	}

#if !__FINAL
    CPopZone *currentActiveZone = GetCurrentActiveZone();
	if (currentActiveZone) sm_nCurrentZoneName = currentActiveZone->m_id;
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// TODO
/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
// Name			:	Display
// Purpose		:	Displays game population cycle debug data
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::Display()
{

#if DEBUG_DRAW
    CPopZone *currentActiveZone = GetCurrentActiveZone();

	if (currentActiveZone)
	{
		if (sm_bDisplayDebugInfo || sm_bDisplayCarDebugInfo)
		{
			grcDebugDraw::AddDebugOutput("Current Zone (%s) popsched: (%s) ", 
				sm_nCurrentZoneName.TryGetCStr(), sm_popSchedules.GetNameFromIndex(sm_nCurrentZonePopScheduleIndex));

			grcDebugDraw::AddDebugOutput("Current Day Subdivision:%d", sm_nCurrentDaySubdivision);
		
			if (CCullZones::FewerPeds())
			{
				grcDebugDraw::AddDebugOutput("FEWER PEDS SET IN THIS AREA");
			}
			
			grcDebugDraw::AddDebugOutput("PercCopsCars:%d PercCopsPeds:%d NumAmbientPeds:%d NumScenarioPeds:%d", GetCurrentDesiredPercentageOfCopsCars(), GetCurrentDesiredPercentageOfCopsPeds(), (s32)(GetCurrentMaxNumAmbientPeds() * CalcWeatherMultiplier() * CPopCycle::CalcHighWayPedMultiplier()), (s32)(GetCurrentMaxNumScenarioPeds() * CalcWeatherMultiplier() * CPopCycle::CalcHighWayPedMultiplier()));

			if (CalcWeatherMultiplier() < 0.99f || CalcHighWayPedMultiplier() < 0.9f || CPopCycle::GetInteriorPedMultiplier() < 0.99f || CPopCycle::GetWantedPedMultiplier() < 0.99f || CPopCycle::GetCombatPedMultiplier() < 0.99f || CPedPopulation::GetAmbientPedDensityMultiplier() < 0.99f)
			{
				grcDebugDraw::AddDebugOutput("RANDOM MULTIPLIERS (Weather:%.2f HighWay:%.2f Interior:%.2f Wanted:%.2f Combat:%.2f AmbientPedDensity:%.2f)",
							CalcWeatherMultiplier(),
							CalcHighWayPedMultiplier(),
							CPopCycle::GetInteriorPedMultiplier(),
							CPopCycle::GetWantedPedMultiplier(),
							CPopCycle::GetCombatPedMultiplier(),
							CPedPopulation::GetAmbientPedDensityMultiplier());
			}

			float fInteriorMult = 1.0f;
			float fExteriorMult = 1.0f;
			CPedPopulation::GetTotalScenarioPedDensityMultipliers( fInteriorMult, fExteriorMult );
			if(fInteriorMult < 0.99f || fExteriorMult < 0.99f)
			{
				grcDebugDraw::AddDebugOutput("ScenarioPed Interior Mult:%.2f, ScenarioPed Exterior Mult:%.2f", fInteriorMult, fExteriorMult);
			}

			// Handle outputting group percentages.
			if (HasValidCurrentPopAllocation())
			{
				bool lineHandled = false;
				char lineBuff[1024];
				lineBuff[0] = 0;
				for(u32 i=0; i<sm_currentPedPercentages.GetCount(); i++)
				{
					if(sm_currentPedPercentages[i] > 0)
					{
						char itemBuff[128];
						itemBuff[0] = 0;

						lineHandled = false;
						sprintf(itemBuff, "%s:%d", GetPopGroups().GetPedGroup(i).GetName().GetCStr(), sm_currentPedPercentages[i]);
						strcat(lineBuff, itemBuff);
						itemBuff[0] = 0;
						if((i+1)%7 == 0)
						{
							grcDebugDraw::AddDebugOutput(lineBuff);
							lineBuff[0] = 0;
							lineHandled = true;
						}
					}
				}

				if(!lineHandled)
				{
					grcDebugDraw::AddDebugOutput(lineBuff);
					lineBuff[0] = 0;
				}
			}
		}

		if(sm_bDisplayScenarioDebugInfo)
		{
			grcDebugDraw::AddDebugOutput("");
			int limit = gPopStreaming.GetMaxScenarioPedModelsLoadedPerSlot();
			const int override = gPopStreaming.GetMaxScenarioPedModelsLoadedOverride();
			const char* overrideStr = "";
			if(override >= 0)
			{
				limit = override;
				overrideStr = " [script override]";
			}

			fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
			atArray<CPedModelInfo*> pedTypeArray;
			pedModelInfoStore.GatherPtrs(pedTypeArray);

			//Iterate over each of the peds, pushing the debug info into an array of character arrays based on streaming type.
			atRangeArray<s16, NUM_SCENARIO_POP_SLOTS> aNumFound;
			atRangeArray<atArray<ConstString>, NUM_SCENARIO_POP_SLOTS> aPedStrings;
			for(unsigned int i=0; i < aNumFound.GetMaxCount(); i++)
			{
				aNumFound[i] = 0;
			}
			for(int i = 0; i < pedTypeArray.GetCount(); i++)
			{
				const CPedModelInfo* modelInfo = pedTypeArray[i];
				if(!modelInfo)
				{
					continue;
				}
				if(!modelInfo->GetCountedAsScenarioPed())
				{
					continue;
				}
				eScenarioPopStreamingSlot eStreamSlot = modelInfo->GetScenarioPedStreamingSlot();

				// Is this really the best way to get an fwModelId if we have a CPedModelInfo?
				fwModelId pedModelId;
				CModelInfo::GetBaseModelInfoFromHashKey(modelInfo->GetModelNameHash(), &pedModelId);

				const bool loaded = CModelInfo::HaveAssetsLoaded(pedModelId);
				const bool deletable = CModelInfo::GetAssetsAreDeletable(pedModelId);
				const bool requested = !loaded && CModelInfo::AreAssetsRequested(pedModelId);

				const char* name = modelInfo->GetModelName();
				char pedDebugLine[1024];
				// Print the debug info into a temp buffer and push it into the ped debug array.
				formatf(pedDebugLine, "  %-32s%s%s%s", name, loaded ? " loaded" : "", deletable ? " deletable" : "", requested ? " requested" : "");
				aPedStrings[eStreamSlot].PushAndGrow(ConstString(pedDebugLine));
				aNumFound[eStreamSlot]++;
			}
#if !__FINAL
			for(unsigned int i=0; i < aPedStrings.GetMaxCount(); i++)
			{
				//Draw label for the different scenario streaming types.
				grcDebugDraw::AddDebugOutputEx(false, "%s (%d / %d%s):", CPopulationStreaming::GetScenarioSlotName((eScenarioPopStreamingSlot)i), gPopStreaming.GetNumScenarioPedModelsLoaded((eScenarioPopStreamingSlot)i), limit, overrideStr);
				//Draw debug info for each ped inside the array associated with that streaming type.
				for(int j=0; j < aPedStrings[i].GetCount(); j++) 
				{
					grcDebugDraw::AddDebugOutputEx(false, aPedStrings[i][j].c_str());
				}
			}
#endif

			// Make sure that the number of peds found is in fact equal to the number of peds streamed in.
			for(unsigned int i=0; i < aNumFound.GetMaxCount(); i++)
			{
				Assert(aNumFound[i] == gPopStreaming.GetNumScenarioPedModelsLoaded((eScenarioPopStreamingSlot)i));
			}
		}

		if (sm_bDisplayDebugInfo || sm_bDisplaySimpleDebugInfo)
		{
			grcDebugDraw::AddDebugOutput("");
			grcDebugDraw::AddDebugOutput("We want (%d), [desired] Cops:%d Ambient:%d Scenario:%d",
				GetCurrentMaxNumCopPeds()+GetCurrentMaxNumAmbientPeds()+GetCurrentMaxNumScenarioPeds(),
				sm_currentMaxNumCopPeds,
				sm_currentMaxNumAmbientPeds,
				sm_currentMaxNumScenarioPeds);

			int nRealPeds = CPed::GetPool()->GetNoOfUsedSpaces();

			// Remove any playerpeds as they're not tallied in the breakdown.
			int playerPedCount = 0;
			CPed::Pool* pPool = CPed::GetPool();
			for(int i=0;i<pPool->GetSize();i++)
			{
				CPed* pPed = pPool->GetSlot(i);
				if(	pPed )
				{
					if(pPed->IsPlayer())
					{
						playerPedCount++;
					}
				}
			}
			nRealPeds -= playerPedCount;

			char szModeString[128];
			if( CPerformance::GetInstance()->EstimateLowPerformance() )
			{
				sprintf(szModeString, "Reduced(");
				if( CPerformance::GetInstance()->GetLowPerformanceFlags() & CPerformance::PF_MissionFlag )
					strcat(szModeString, "Mission " );
				if( CPerformance::GetInstance()->GetLowPerformanceFlags() & CPerformance::PF_PlayerSpeed )
					strcat(szModeString, "Speed " );
				if( CPerformance::GetInstance()->GetLowPerformanceFlags() & CPerformance::PF_WantedLevel )
					strcat(szModeString, "WLevel " );
				strcat(szModeString, ")" );

			}
			else
			{
				szModeString[0] = 0;
			}

			grcDebugDraw::AddDebugOutput("We have (%d/%d), [onFoot/InVeh] Cops:%d/%d, [onFoot/InVeh] Swat:%d/%d Ambient:%d/%d Scenario:%d/%d Other:%d/%d (Real:%d) (%s)",
				(nRealPeds),
				CPed::GetPool()->GetSize(),
				CPedPopulation::ms_nNumOnFootCop, CPedPopulation::ms_nNumInVehCop,
				CPedPopulation::ms_nNumOnFootSwat, CPedPopulation::ms_nNumInVehSwat,
				CPedPopulation::ms_nNumOnFootAmbient,CPedPopulation::ms_nNumInVehAmbient,
				CPedPopulation::ms_nNumOnFootScenario,CPedPopulation::ms_nNumInVehScenario,
				CPedPopulation::ms_nNumOnFootOther, CPedPopulation::ms_nNumInVehOther,
				nRealPeds, szModeString);
			
			grcDebugDraw::AddDebugOutput("Peds Pending Deletion: %u", CPedPopulation::GetPedsPendingDeletion());

			if(!sm_bDisplaySimpleDebugInfo)
			{
				gPopStreaming.PrintDebugForPeds();
			}
		}

		if( sm_bDisplayInVehDeadDebugInfo )
		{
			grcDebugDraw::AddDebugOutput("PedsInVehAmbientDead: %d", CPedPopulation::ms_nNumInVehAmbientDead);
		}

		if( sm_bDisplayInVehNoPretendModelDebugInfo )
		{
			grcDebugDraw::AddDebugOutput("PedsInVehAmbientNoPretendModel: %d", CPedPopulation::ms_nNumInVehAmbientNoPretendModel);
		}

		if (sm_bDisplayCarDebugInfo || sm_bDisplayCarSimpleDebugInfo)
		{
			// Count the number of cars in the world.
			s32	MissionCars = 0, OtherCars = 0, ParkedCars = 0, ParkedMissionCars = 0;
			CVehicle::Pool& VehPool = *CVehicle::GetPool();
			CVehicle* pVehicle;
			s32 i = (s32) VehPool.GetSize();
			while(i--)
			{
				pVehicle = VehPool.GetSlot(i);
				if (pVehicle)
				{
					if (pVehicle->InheritsFromBike() ||
						pVehicle->InheritsFromAutomobile() ||
						pVehicle->InheritsFromBoat() ||
						pVehicle->InheritsFromTrain())
					{
						if (pVehicle->PopTypeIsMission())
						{
							if (pVehicle->m_nVehicleFlags.bCountAsParkedCarForPopulation)
							{
								ParkedMissionCars++;
							}
							else
							{
								MissionCars++;
							}
						}
						else
						{
							if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
							{
								ParkedCars++;
							}
							else
							{
								OtherCars++;
							}
						}
					}
				}
			}

			if (CCullZones::FewerCars())
			{
				grcDebugDraw::AddDebugOutput("FEWER CARS SET IN THIS AREA");
			}

			const float fPercentage = CPopCycle::GetCurrentPercentageOfMaxCars();

			int iVehiclesUpperLimit;
			float fDesertedStreetsMult, fInteriorMult, fWantedMult, fCombatMult, fHighwayMult, fBudgetMult;
			float fDesiredNumAmbientVehicles = CVehiclePopulation::GetDesiredNumberAmbientVehicles(&iVehiclesUpperLimit, &fDesertedStreetsMult, &fInteriorMult, &fWantedMult, &fCombatMult, &fHighwayMult, &fBudgetMult);

			grcDebugDraw::AddDebugOutput("TOTAL num cars in pool : %d / %d", MissionCars + ParkedCars + OtherCars, CVehicle::GetPool()->GetSize());
			grcDebugDraw::AddDebugOutput("Num AMBIENT cars desired : (%d) popcycle=%i%% (%i x %f x %f x %f x %f x %f x %f)"
							,CVehiclePopulation::ms_overrideNumberOfCars != -1 ? CVehiclePopulation::ms_overrideNumberOfCars : (int)fDesiredNumAmbientVehicles
							,(s32) fPercentage
							,iVehiclesUpperLimit
							,fDesertedStreetsMult 
							,fInteriorMult
							,fWantedMult
							,fCombatMult
							,fHighwayMult
							,fBudgetMult);
			grcDebugDraw::AddDebugOutput("Num AMBIENT cars we have : (%d)", OtherCars);
			grcDebugDraw::AddDebugOutput("Num PARKED cars we have : (%d) (of which %d are low prio)", ParkedCars, CVehiclePopulation::ms_numLowPrioParkedCars);
			grcDebugDraw::AddDebugOutput("Num MISSION cars we have : (%d)", MissionCars);
			grcDebugDraw::AddDebugOutput("Num cars in cargen queue : (%d)", CTheCarGenerators::GetNumQueuedVehs());
			grcDebugDraw::AddDebugOutput("Num entries in automobile placement queue: (%d)", CAutomobile::GetAsyncQueueCount());

			float areaMultiplier = CThePopMultiplierAreas::GetTrafficDensityMultiplier(CGameWorld::FindLocalPlayerCoors());
			grcDebugDraw::AddDebugOutput("Area density multiplier: %.2f", areaMultiplier);

			char buf[256] = {0};
			char item[64] = {0};
			for (s32 i = 0; i < CTheCarGenerators::GetPreassignedModels().GetCount(); ++i)
			{
				fwModelId mi((strLocalIndex(CTheCarGenerators::GetPreassignedModels()[i])));
				if (mi.IsValid())
				{
					CBaseModelInfo* bmi = CModelInfo::GetBaseModelInfo(mi);
					formatf(item, " %s", bmi->GetModelName());
					safecat(buf, item);
				}
			}
			grcDebugDraw::AddDebugOutput("Preassigned cargen models: (%d/%d)%s", CTheCarGenerators::GetPreassignedModels().GetCount(), GetMaxCargenModels(), buf);

			buf[0] = '\0';
			const atArray<u32>& scenarioModels = CTheCarGenerators::GetScenarioModels();
			for(int i = 0; i < scenarioModels.GetCount(); i++)
			{
				fwModelId mi((strLocalIndex(scenarioModels[i])));
				if (mi.IsValid())
				{
					CBaseModelInfo* bmi = CModelInfo::GetBaseModelInfo(mi);
					formatf(item, " %s", bmi->GetModelName());
					safecat(buf, item);
				}
			}
			grcDebugDraw::AddDebugOutput("Scenario models: (%d/%d)%s", scenarioModels.GetCount(), GetMaxScenarioVehicleModels(), buf);

#if !__FINAL
			grcDebugDraw::AddDebugOutput("ms_numGenerationLinks : (%i/%i) used, ms_iNumActiveLinks : %i.  Range scale = %.1f",
				CVehiclePopulation::GetNumGenerationLinks(),
				MAX_GENERATION_LINKS,
				CVehiclePopulation::GetNumActiveLinks(),
				CVehiclePopulation::GetPopulationRangeScale() );
#endif

			if (sm_bDisplayCarDebugInfo)
			{
				if (CVehiclePopulation::HasInterestingVehicle())
				{
					u32 timeLeft = CVehiclePopulation::GetInterestingVehicleClearTime() - fwTimer::GetTimeInMilliseconds();
					u32 secs = timeLeft / 1000;
					u32 mins = secs / 60;
					secs = secs - (mins * 60);
					grcDebugDraw::AddDebugOutput("Time until interesting vehicle is cleared: %2dm%2ds", mins, secs);
				}

				if (CVehiclePopulation::GetDesertedStreetsMultiplier() < 0.999f)
				{
					grcDebugDraw::AddDebugOutput("Deserted streets multiplier: %f", CVehiclePopulation::GetDesertedStreetsMultiplier());
				}
				if (CPopCycle::GetInteriorCarMultiplier() < 0.999f || CPopCycle::GetWantedCarMultiplier() < 0.99f || CPopCycle::GetCombatCarMultiplier() < 0.99f)
				{
					grcDebugDraw::AddDebugOutput("Interior car multiplier:%.2f WantedCarMult:%.2f CombatCarMult:%.2f", CPopCycle::GetInteriorCarMultiplier(), CPopCycle::GetWantedCarMultiplier(), CPopCycle::GetCombatCarMultiplier());
				}
				if (CPopCycle::GetTemporaryMaxNumScenarioPeds() < 1000 || CPopCycle::GetTemporaryMaxNumAmbientPeds() < 1000 || CPopCycle::GetTemporaryMaxNumCars() < 1000)
				{
					grcDebugDraw::AddDebugOutput("TemporaryMaxNumScenarioPeds:%d TemporaryMaxNumAmbientPeds:%d TemporaryMaxNumCars:%d", CPopCycle::GetTemporaryMaxNumScenarioPeds(), CPopCycle::GetTemporaryMaxNumAmbientPeds(), CPopCycle::GetTemporaryMaxNumCars());
				}

				// Handle outputting group percentages.
				if (HasValidCurrentPopAllocation())
				{
					CLoadedModelGroup loadedVehs;
					loadedVehs.Merge(&gPopStreaming.GetAppropriateLoadedCars(), &gPopStreaming.GetLoadedBoats());
					const s32 numCars = loadedVehs.CountMembers();

					bool lineHandled = false;
					char lineBuff[1024];
					lineBuff[0] = 0;
					const u32 vehGroupCount = GetCurrentPopAllocation().GetNumberOfVehGroups();
					for(u32 i = 0; i < vehGroupCount; ++i)
					{
						lineHandled = false;

						float currentPercentage = 0.f;
						u32 vehGroupIndex;
						if(sm_popGroups.FindVehGroupFromNameHash(GetCurrentPopAllocation().GetVehGroupPercentage(i).m_GroupName, vehGroupIndex))
						{
							s32 totalModels = 0;
							for (s32 m = 0; m < numCars; m++)
							{
								u32 model = loadedVehs.GetMember(m);
								fwModelId modelId((strLocalIndex(model)));
								if (!CModelInfo::GetAssetsAreDeletable(modelId))
								{
									totalModels++;
									if (GetPopGroups().IsVehIndexMember(vehGroupIndex, model))
									{
										currentPercentage += 1.f;
									}
								}
							}
							currentPercentage /= rage::Max(1, totalModels);
						}

						char itemBuff[255];
						itemBuff[0] = 0;

						u32 percent = GetCurrentPopAllocation().GetVehGroupPercentage(i).m_percentage;
						const char* pGroupName = GetCurrentPopAllocation().GetVehGroupPercentage(i).m_GroupName.GetCStr();
						sprintf(itemBuff, "%s:%d(%.2f) ", pGroupName, percent, currentPercentage);

						strcat(lineBuff, itemBuff);
						itemBuff[0] = 0;

						if((i+1)%7 == 0)
						{
							grcDebugDraw::AddDebugOutput(lineBuff);
							lineBuff[0] = 0;
							lineHandled = true;
						}
					}
					if(!lineHandled)
					{
						grcDebugDraw::AddDebugOutput(lineBuff);
						lineBuff[0] = 0;
					}
				}

				grcDebugDraw::AddDebugOutput("CVehiclePopulation::ms_NumRandomCars:%d ms_NumMissionCars:%d", CVehiclePopulation::ms_numRandomCars, CVehiclePopulation::ms_numMissionCars);

				s32 maxParkedCars = GetCurrentMaxNumParkedCars();
				s32 maxLowPrioCars = GetCurrentMaxNumLowPrioParkedCars();
				grcDebugDraw::AddDebugOutput("Parked Cars we want(lowprio):%d(%d) We have(lowprio):%d(%d)   (Parked:%d ParkedMission:%d)",
					maxParkedCars, maxLowPrioCars, ParkedCars + ParkedMissionCars, CVehiclePopulation::ms_numLowPrioParkedCars, ParkedCars, ParkedMissionCars);

				gPopStreaming.PrintDebugForVehicles();
			}
		}
	}

#if __BANK
    if(sm_bDisplayCarPopulationFailureCounts)
    {
        grcDebugDraw::AddDebugOutput("Car population failure reasons");
        grcDebugDraw::AddDebugOutput("--------------------------");
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNoCreationPosFound?Color_red:Color_white,					"No creation pos                             %d", CVehiclePopulation::m_populationFailureData.m_numNoCreationPosFound);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNoLinkBetweenNodes?Color_red:Color_white,					"No link between nodes                       %d", CVehiclePopulation::m_populationFailureData.m_numNoLinkBetweenNodes);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numChosenModelNotLoaded?Color_red:Color_white,				"Chosen car model not loaded                 %d", CVehiclePopulation::m_populationFailureData.m_numChosenModelNotLoaded);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numSpawnLocationInViewFrustum?Color_red:Color_white,			"In view frustum                             %d", CVehiclePopulation::m_populationFailureData.m_numSpawnLocationInViewFrustum);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numSingleTrackRejection?Color_red:Color_white,				"Single track rejection                      %d", CVehiclePopulation::m_populationFailureData.m_numSingleTrackRejection);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numTrajectoryOfMarkedVehicleRejection?Color_red:Color_white,	"Trajectory Collision Rejection              %d", CVehiclePopulation::m_populationFailureData.m_numTrajectoryOfMarkedVehicleRejection);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNoVehModelFound?Color_red:Color_white,					"No car model                                %d", CVehiclePopulation::m_populationFailureData.m_numNoVehModelFound);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNetworkPopulationFail?Color_red:Color_white,				"Network population                          %d", CVehiclePopulation::m_populationFailureData.m_numNetworkPopulationFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numVehsGoingAgainstTrafficFlow?Color_red:Color_white,		"Going against traffic flow                  %d", CVehiclePopulation::m_populationFailureData.m_numVehsGoingAgainstTrafficFlow);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numDeadEndsChosen?Color_red:Color_white,						"Dead end chosen                             %d", CVehiclePopulation::m_populationFailureData.m_numDeadEndsChosen);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNoPathNodesAvailable?Color_red:Color_white,				"No path nodes available                     %d", CVehiclePopulation::m_populationFailureData.m_numNoPathNodesAvailable);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numGroundPlacementFail?Color_red:Color_white,				"Ground placement                            %d", CVehiclePopulation::m_populationFailureData.m_numGroundPlacementFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numGroundPlacementSpecialFail?Color_red:Color_white,			"Ground placement (special)                  %d", CVehiclePopulation::m_populationFailureData.m_numGroundPlacementFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNetworkVisibilityFail?Color_red:Color_white,				"Network visibility                          %d", CVehiclePopulation::m_populationFailureData.m_numNetworkVisibilityFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numPointWithinCreationDistFail?Color_red:Color_white,		"Point within creation distance              %d", CVehiclePopulation::m_populationFailureData.m_numPointWithinCreationDistFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numPointWithinCreationDistSpecialFail?Color_red:Color_white,	"Point within creation distance (special)    %d", CVehiclePopulation::m_populationFailureData.m_numPointWithinCreationDistSpecialFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numRelativeMovementFail?Color_red:Color_white,				"Relative movement                           %d", CVehiclePopulation::m_populationFailureData.m_numRelativeMovementFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numBoundingBoxFail?Color_red:Color_white,					"Bounding box                                %d", CVehiclePopulation::m_populationFailureData.m_numBoundingBoxFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numBoundingBoxSpecialFail?Color_red:Color_white,				"Bounding box (special)                      %d", CVehiclePopulation::m_populationFailureData.m_numBoundingBoxSpecialFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numVehicleCreateFail?Color_red:Color_white,					"Vehicle creation                            %d", CVehiclePopulation::m_populationFailureData.m_numVehicleCreateFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numDriverAddFail?Color_red:Color_white,						"Adding a driver                             %d", CVehiclePopulation::m_populationFailureData.m_numDriverAddFail);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numFallbackPedNotAvailable?Color_red:Color_white,			"No fallback ped                             %d", CVehiclePopulation::m_populationFailureData.m_numFallbackPedNotAvailable);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numPopulationIsFull?Color_red:Color_white,					"Population is full                          %d", CVehiclePopulation::m_populationFailureData.m_numPopulationIsFull);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNodesSwitchedOff?Color_red:Color_white,					"Nodes switched off                          %d", CVehiclePopulation::m_populationFailureData.m_numNodesSwitchedOff);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numIdenticalModelTooClose?Color_red:Color_white,				"Identical model too close                   %d", CVehiclePopulation::m_populationFailureData.m_numIdenticalModelTooClose);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numNotNetworkTurnToCreateVehicleAtPos?Color_red:Color_white,	"Not network turn to create vehicle at pos   %d", CVehiclePopulation::m_populationFailureData.m_numNotNetworkTurnToCreateVehicleAtPos);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_sameVehicleModelAsLastTime?Color_red:Color_white,			"Same vehicle model as last time             %d", CVehiclePopulation::m_populationFailureData.m_sameVehicleModelAsLastTime);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numChosenBoatModelNotLoaded?Color_red:Color_white,			"Chosen boat model not loaded                %d", CVehiclePopulation::m_populationFailureData.m_numChosenBoatModelNotLoaded);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numRandomBoatsNotAllowed?Color_red:Color_white,				"Random boats not allowed                    %d", CVehiclePopulation::m_populationFailureData.m_numRandomBoatsNotAllowed);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numBoatAlreadyHere?Color_red:Color_white,					"Boat already here                           %d", CVehiclePopulation::m_populationFailureData.m_numBoatAlreadyHere);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numWaterLevelFuckup?Color_red:Color_white,					"Water level fuckup                          %d", CVehiclePopulation::m_populationFailureData.m_numWaterLevelFuckup);
		grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numLinkDirectionInvalid?Color_red:Color_white,				"Link Direction Invalid                      %d", CVehiclePopulation::m_populationFailureData.m_numLinkDirectionInvalid);
        grcDebugDraw::AddDebugOutput(CVehiclePopulation::m_populationFailureData.m_numOutOfLocalPlayerScope? Color_red : Color_white,           "Out of local player scope                   %d", CVehiclePopulation::m_populationFailureData.m_numOutOfLocalPlayerScope);

		if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
			grcDebugDraw::AddDebugOutput("Blocked by cutscene");

#if GTA_REPLAY
		if( CReplayMgr::IsEditModeActive())
			grcDebugDraw::AddDebugOutput("Blocked by replay edit mode");
#endif

		if(fwTimer::IsGamePaused())
			grcDebugDraw::AddDebugOutput("Blocked by game pause");

		if(CVehiclePopulation::ms_bGenerationSkippedDueToNoLinksWithDisparity)
			grcDebugDraw::AddDebugOutput("Blocked by no links with disparity");
    }

	if(sm_bDisplayPedPopulationFailureCounts)
	{
		grcDebugDraw::AddDebugOutput("Ped population failure reasons");
		grcDebugDraw::AddDebugOutput("--------------------------");

		for( int i = 0; i < CPedPopulation::PedPopulationFailureData::FR_Max; i++ )
		{
			CPedPopulation::PedPopulationFailureData::FailureInfo* pFailureInfo = CPedPopulation::ms_populationFailureData.GetFailureEnumDebugInfo((CPedPopulation::PedPopulationFailureData::FailureType)i);
			if( pFailureInfo )
			{
				Color32 colour = pFailureInfo->m_colour;
				if( !pFailureInfo->m_bActive )
				{
					colour = Color_grey;
				}
				else if( pFailureInfo->m_iCount == 0 )
				{
					colour = Color_white;
				}
				grcDebugDraw::AddDebugOutput(colour, "%s          %d", pFailureInfo->m_szName, pFailureInfo->m_iCount);
			}
		}

        if(NetworkInterface::IsGameInProgress())
        {
            grcDebugDraw::AddDebugOutput("MP Visibility Failure Count: %d", CPedPopulation::GetMPVisibilityFailureCount());
        }
	}
#endif // __DEV
#endif // DEBUG_DRAW
}

static const u32 MaxPopCycleGroups = 250;

/////////////////////////////////////////////////////////////////////////////////
// Name			:	FindNewPedType
// Purpose		:	This function will return the modelindex of the next ped to create.
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
bool CPopCycle::FindNewPedModelIndex(u32 *pNewPedModelIndex, bool bNoCops, bool forceCivPedCreation, u32 requiredPopGroupFlags, CPopCycleConditions& conditions, const CAmbientModelVariations*& pVariationsOut, const Vector3* pos)
{
	CPopZone *currentActiveZone = GetCurrentActiveZone();
	if (!currentActiveZone) return false;	
	
	if(currentActiveZone->m_id.GetHash() != sm_CurrentPopZoneId)
	{
		// popzone has changed
		sm_CurrentPopZoneId = currentActiveZone->m_id;
		sm_PopulationRecalculateCountdown = 0;
		sm_CachedMostWantedPopGroup = MaxPopCycleGroups;
	}

	// Determine the shortage of other peds if any.
	s32 ambientShortage = sm_currentMaxNumAmbientPeds - CPedPopulation::ms_nNumOnFootAmbient;

	// Determine the shortage of cops if any.
	s32 copShortage = sm_currentMaxNumCopPeds - CPedPopulation::ms_nNumOnFootCop;
	if(bNoCops 
		|| !CPedPopulation::GetAllowCreateRandomCops() 
		|| (currentActiveZone->m_bNoCops) 
		|| (CGameWorld::FindLocalPlayerWanted() ? CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() > WANTED_CLEAN : false)	// Don't want to create peds on foot with wanted level. They show up on radar.
		|| MI_PED_COP == fwModelId::MI_INVALID)	// If the cop model doesn't exist, we would otherwise crash on the HasObjectLoaded().
	{
		copShortage = 0;
	}

// DEBUG!! -AC, Just here for testing...
	if(forceCivPedCreation)
	{
		ambientShortage = 1;
		copShortage = 0;
	}
#if DEBUG_DRAW
	if(	CPedPopulation::ms_nNumOnFootCop < 30 &&
		CPedDebugVisualiserMenu::GetForceAllCops())
	{
		ambientShortage = 0;
		copShortage = 1;
	}
#endif // !__FINAL
// END DEBUG!!

	// Determine what ped type to create if any.
	if((copShortage <= 0) && (ambientShortage <= 0))
	{
		// We have enough of everything. Don't create a new ped.
		return false;
	}
	else if (copShortage >= ambientShortage)
	{
		// We need more cops.
		*pNewPedModelIndex = fwModelId::MI_INVALID;

		if(CModelInfo::GetStreamingModule()->HasObjectLoaded(strLocalIndex(MI_PED_COP)))
		{
				*pNewPedModelIndex = MI_PED_COP;
		}

		if( *pNewPedModelIndex == fwModelId::MI_INVALID)
		{
			// If modelIndex == fwModelId::MI_INVALID we haven't succeeded.
			return false;
		}
		else
		{
			return true;
		}
	}
	else // OtherShortage > copShortage
	{
		// We need more civilians.
		// Work out which new ped model we'd like to load.
		u32 popGroupCount = MaxPopCycleGroups;
		u32 wantedGroup = ~0U;
		// Check for a previously cached value
		if(sm_CachedMostWantedPopGroup < popGroupCount && sm_PopulationRecalculateCountdown > 0)
		{
			// used cached pop group
			wantedGroup = sm_CachedMostWantedPopGroup;
			sm_PopulationRecalculateCountdown--;
		}

		if(wantedGroup >= popGroupCount)
		{
			// Recalculate the wanted group
			float aRequestedPercentages[MaxPopCycleGroups] = {0};
			float aPercentages[MaxPopCycleGroups] = {0};
			u32 aTimesUsed[MaxPopCycleGroups] = {0};

			if(HasValidCurrentPopAllocation())
			{
				Assertf(sm_popGroups.GetPedCount() <= MaxPopCycleGroups, "CPopCycle::FindNewPedModelIndex - popcycle.dat has more than %d popgroup columns. MaxPopCycleGroups will need to be increased in code", MaxPopCycleGroups);
				popGroupCount = Min(MaxPopCycleGroups, (u32)sm_popGroups.GetPedCount());

				u32	popGroupIndex, totalModels = 0;

				// init percentages
				float probSum = 0.0f;

				bool bSpawnMoreAquaticPeds = CWildlifeManager::GetInstance().ShouldSpawnMoreAquaticPeds();

				// We want to try and spawn more aerial/ground peds if there are no cached ped gen points, because those kinds of peds do not require
				// those points (they get their points from the wildlife manager).
				bool bSpawnMoreAerialPeds = CWildlifeManager::GetInstance().ShouldSpawnMoreAerialPeds();
				bool bSpawnMoreGroundWildlifePeds = CWildlifeManager::GetInstance().ShouldSpawnMoreGroundWildlifePeds();

				for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
				{
					float prob = GetCurrentPedGroupPercentage(popGroupIndex) * 0.01f;

					if(requiredPopGroupFlags)
					{
						u32 iGroupFlags = sm_popGroups.GetPedGroup(popGroupIndex).GetFlags();
						if(( iGroupFlags & requiredPopGroupFlags) != requiredPopGroupFlags)
						{
							// The flags don't match, set the probability of using this group to zero.
							prob = 0.0f;
						} 
						else if (bSpawnMoreAquaticPeds && iGroupFlags & POPGROUP_AQUATIC)
						{
							prob *= CWildlifeManager::GetInstance().GetIncreasedAquaticSpawningFactor();
						}
						else if (bSpawnMoreAerialPeds && iGroupFlags & POPGROUP_AERIAL)
						{
							prob *= CWildlifeManager::GetInstance().GetIncreasedAerialSpawningFactor();
						}
						else if (bSpawnMoreGroundWildlifePeds && iGroupFlags & POPGROUP_WILDLIFE)
						{
							prob *= CWildlifeManager::GetInstance().GetIncreasedGroundWildlifeSpawningFactor();
						}
					}
					probSum += prob;

					aRequestedPercentages[popGroupIndex] = prob;
				}

				if(requiredPopGroupFlags && probSum >= SMALL_FLOAT)
				{
					// Scale the probabilities so they sum up to 100% again, after we cleared out the
					// ones with invalid flags.
					float probScale = 1.0f/probSum;
					for(popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
					{
						aRequestedPercentages[popGroupIndex] *= probScale;
					}
				}

				// Go through all of the appropriate loaded peds and give each group that they belong to points based on the ped refs
				s32 numPeds = gPopStreaming.GetAppropriateLoadedPeds().CountMembers();

				for (s32 i = 0; i < numPeds; ++i)
				{
					strLocalIndex mi = strLocalIndex(gPopStreaming.GetAppropriateLoadedPeds().GetMember(i));
					fwModelId modelId(mi);
					if (!modelId.IsValid())
						continue;

					// Only consider models that aren't about to be streamed out.
					if (!CModelInfo::GetAssetsAreDeletable(modelId))
					{
						CPedModelInfo* pedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);
						if (!pedModelInfo)
							continue;

						if (CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(mi.Get(), pedModelInfo))
							continue;

						for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
						{
							if(aRequestedPercentages[popGroupIndex] > 0.0f)
							{
								if (GetPopGroups().IsPedIndexMember(popGroupIndex, mi.Get()))
								{
									u32 modelCount = pedModelInfo->GetNumPedModelRefs() - pedModelInfo->GetNumTimesInReusePool(); // don't count models in the reuse pool
									aTimesUsed[popGroupIndex] += modelCount;
									totalModels += modelCount;
								}
							}
						}
					}
				}

				// calculate current percentages and which group has the largest deviation from requested value
				totalModels = MAX(1, totalModels);
				float largestDeviation = -9999.f;

				for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
				{
					aPercentages[popGroupIndex] = (aTimesUsed[popGroupIndex] * 1.f) / totalModels;

					if(requiredPopGroupFlags)
					{
						// Even though we have already cleared out aRequestedPercentages for groups that
						// don't match the required flags, we have to explicitly reject them here as well,
						// because otherwise they can still be considered the best group.
						if((sm_popGroups.GetPedGroup(popGroupIndex).GetFlags() & requiredPopGroupFlags) != requiredPopGroupFlags)
						{
							continue;
						}

						// Potentially the population spawner could be in a situation where aquatic peds are the only group spawned so far,
						// And as such will never be selected to spawn again regardless of how high their requested percentage would be.
						// (If requested < 1 and aPercentages == 1 then the deviation is always negative)
						// Here we correspondingly lower the amount of peds the spawner thinks are in the world so that this doesn't happen.
						if (bSpawnMoreAquaticPeds && sm_popGroups.GetPedGroup(popGroupIndex).GetFlags() & POPGROUP_AQUATIC)
						{
							aPercentages[popGroupIndex] /= CWildlifeManager::GetInstance().GetIncreasedAquaticSpawningFactor();
						}
						else if (bSpawnMoreAerialPeds && sm_popGroups.GetPedGroup(popGroupIndex).GetFlags() & POPGROUP_AERIAL)
						{
							aPercentages[popGroupIndex] /= CWildlifeManager::GetInstance().GetIncreasedAerialSpawningFactor();
						}
						else if (bSpawnMoreGroundWildlifePeds && sm_popGroups.GetPedGroup(popGroupIndex).GetFlags() & POPGROUP_WILDLIFE)
						{
							aPercentages[popGroupIndex] /= CWildlifeManager::GetInstance().GetIncreasedGroundWildlifeSpawningFactor();
						}
					}

					Assert(aPercentages[popGroupIndex] >= 0.0f && aPercentages[popGroupIndex] <= 1.0f);
					Assert(aRequestedPercentages[popGroupIndex] >= 0.0f);

					float deviation = aRequestedPercentages[popGroupIndex] - aPercentages[popGroupIndex];
					if (deviation > largestDeviation)
					{
						largestDeviation = deviation;
						wantedGroup = popGroupIndex;
					}
				}				

				// store the wanted group for next time
				sm_CachedMostWantedPopGroup = wantedGroup;
				// Reset the countdown
				sm_PopulationRecalculateCountdown = POPULATION_RECALCULATE_COUNTDOWN;
			}
		}

		// Check if we found any group we can use.
		if(wantedGroup < (u32)sm_popGroups.GetPedCount())
		{
			CLoadedModelGroup loadedPeds = gPopStreaming.GetAppropriateLoadedPeds();
			loadedPeds.RemoveModelsNotInGroup(wantedGroup, true);
			u32 numAvailablePeds = loadedPeds.CountMembers();

			if (numAvailablePeds > 0)
			{
				if (pos)
				{
					loadedPeds.SortPedsByDistance(*pos);
					*pNewPedModelIndex = loadedPeds.GetMember(0);
				}
				else
					*pNewPedModelIndex = loadedPeds.PickRandomPedModel();

				if (*pNewPedModelIndex != fwModelId::MI_INVALID)
				{
					pVariationsOut = sm_popGroups.GetPedGroup(wantedGroup).FindVariations(*pNewPedModelIndex);
				}
			}
		}

		if (*pNewPedModelIndex == fwModelId::MI_INVALID)
		{
			CPedPopulation::ChooseCivilianPedModelIndexArgs args;
			args.m_RequiredPopGroupFlags = requiredPopGroupFlags;
			args.m_AllowFlyingPeds = true;
			args.m_AllowSwimmingPeds = true;
			if(pos)
			{
				args.m_CreationCoors = pos;	
				args.m_SortByPosition = true; // care about which ped models are closest to the spawn position
			}

			// if we're in this situation we don't want to spawn a gang ped unless requested
			if ((requiredPopGroupFlags & POPGROUP_IS_GANG) == 0)
			{
				args.m_RequiredGangMembership = NOT_GANG_MEMBER;
			}

			args.m_PedModelVariationsOut = &pVariationsOut;
			*pNewPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(args);

			
			// Have the conditions for spawn location match the default for the ped.
			if (*pNewPedModelIndex != fwModelId::MI_INVALID)
			{
				const CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(fwModelId((strLocalIndex(*pNewPedModelIndex)))));
				if (Verifyf(pModelInfo, "Valid ped model index but NULL model info!"))
				{
					conditions.m_bSpawnInAir = pModelInfo->ShouldSpawnInAirByDefault();
					conditions.m_bSpawnInWater = pModelInfo->ShouldSpawnInWaterByDefault();
					conditions.m_bSpawnAsWildlife = pModelInfo->ShouldSpawnAsWildlifeByDefault();
				}
			}
		}
		else
		{
			// Have the conditions for spawn location selection match that of the group selected.
			conditions.m_bSpawnInWater = (sm_popGroups.GetPedGroup(wantedGroup).GetFlags() & POPGROUP_AQUATIC) != 0;
			conditions.m_bSpawnInAir = (sm_popGroups.GetPedGroup(wantedGroup).GetFlags() & POPGROUP_AERIAL) != 0;
			conditions.m_bSpawnAsWildlife = (sm_popGroups.GetPedGroup(wantedGroup).GetFlags() & POPGROUP_WILDLIFE) != 0;
		}

		if (*pNewPedModelIndex == fwModelId::MI_INVALID)
		{
			// If modelIndex == fwModelId::MI_INVALID we haven't succeeded.
			return false;
		}
		else
		{
#if __ASSERT
			const CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(*pNewPedModelIndex))));
			Assert(pModelInfo);
			ePedType pedType = pModelInfo->GetDefaultPedType();
			Assertf((pedType >= PEDTYPE_CIVMALE && pedType <= PEDTYPE_PROSTITUTE) || pedType == PEDTYPE_ANIMAL || pedType == PEDTYPE_ARMY, "Ped type is not set correctly (%d)", pedType);
#endif // __ASSERT

			return true;
		}
	}
}

s32 GroupPercentageCompare(const sGroupPercentage* group0, const sGroupPercentage* group1)
{
    // if we have zero vehicles from a group, it ranks the
    // highest, no matter what the actual deviation is, just to get at least one model in each group streamed in
    if (group0->zeroInstances && group1->zeroInstances)
        return group1->randNum - group0->randNum;
	else if (group0->zeroInstances)
        return -1;
	else if (group1->zeroInstances)
        return 1;

    if (fabs(group0->deviation - group1->deviation) < 0.0001f)
		return group1->randNum - group0->randNum;

	if (group0->deviation < group1->deviation)
		return 1;
	else if (group0->deviation > group1->deviation)
		return -1;

	return 0;
}

s32 AvailableModelDistanceCompare(const sAvailableModel* model0, const sAvailableModel* model1)
{
	if (model0->distanceSquared > model1->distanceSquared)
	{
		return -1;
	}
	else if (model0->distanceSquared < model1->distanceSquared)
	{
		return 1;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	FindNewCarModelIndex
// Purpose		:	This function will return the modelindex of the next car to create.
// Parameters	:	pNewCarModelIndex - the returned car model index
//				:	availableCars - list of cars to choose from
// Returns		:	true on success, false otherwise
/////////////////////////////////////////////////////////////////////////////////
bool CPopCycle::FindNewCarModelIndex(u32* pNewCarModelIndex, CLoadedModelGroup& availableCars, const Vector3* pos, bool cargens, bool bRandomizedSelection)
{
	float aRequestedPercentages[MaxPopCycleGroups] = {0};
	u32 aTimesUsed[MaxPopCycleGroups] = {0};

	if (!HasValidCurrentPopAllocation())
		return false;

	Assertf(sm_popGroups.GetVehCount() <= MaxPopCycleGroups, "CPopCycle::FindNewCarModelIndex - popcycle.dat has more than %d popgroup columns. MaxPopCycleGroups will need to be increased in code", MaxPopCycleGroups);
	u32 popGroupCount = Min(MaxPopCycleGroups, (u32)sm_popGroups.GetVehCount());

	u32	popGroupIndex, totalModels = 0;

	// init percentages
	u32 numActiveGroups = 0;
	for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
	{
		aRequestedPercentages[popGroupIndex] = GetCurrentVehGroupPercentage(popGroupIndex) * 0.01f;
        if (aRequestedPercentages[popGroupIndex] > 0.f)
            numActiveGroups++;
	}

	sGroupPercentage *percentageStorage = Alloca(sGroupPercentage, numActiveGroups);
	atUserArray<sGroupPercentage> percentages(percentageStorage, (u16) numActiveGroups);

	// Go through all of the available vehicles and give each group that they belong to points based on the vehicle refs
	CLoadedModelGroup appropriateVehs;
	appropriateVehs.Merge(&gPopStreaming.GetAppropriateLoadedCars(), &gPopStreaming.GetInAppropriateLoadedCars(), &gPopStreaming.GetDiscardedCars(), &gPopStreaming.GetLoadedBoats());
    appropriateVehs.RemoveSuppressedAndNonAmbientCars();
	s32 numVehs = appropriateVehs.CountMembers();


	sAvailableModel *availableCarsByDistanceStorage = Alloca(sAvailableModel, numVehs);
	atUserArray<sAvailableModel> availableCarsByDistance(availableCarsByDistanceStorage, (u16) numVehs);
	availableCarsByDistance.Resize(numVehs);

	for (s32 i = 0; i < numVehs; ++i)
	{
		strLocalIndex mi = strLocalIndex(appropriateVehs.GetMember(i));

		availableCarsByDistance[i].index = mi.Get();
		availableCarsByDistance[i].distanceSquared = 0.0f;

		fwModelId modelId(mi);
		if (!modelId.IsValid())
			continue;

		CVehicleModelInfo* vehModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(modelId);
		if (!vehModelInfo)
			continue;

		if (pos)
		{
			availableCarsByDistance[i].distanceSquared = vehModelInfo->GetDistanceSqrToClosestInstance(*pos, cargens);
		}

		s32 numRefs = cargens ? vehModelInfo->GetNumVehicleModelRefsParked() : vehModelInfo->GetNumVehicleModelRefs() - vehModelInfo->GetNumVehicleModelRefsReusePool(); // don't count models in the vehicle reuse pool

		bool counted = false;
		for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
		{
            if(aRequestedPercentages[popGroupIndex] > 0.0f)
            {
				if (GetPopGroups().IsVehIndexMember(popGroupIndex, mi.Get()))
				{
					aTimesUsed[popGroupIndex] += numRefs;
					if (!counted)
					{
						totalModels += numRefs;
						counted = true;
					}
				}
            }
		}
	}

	availableCarsByDistance.QSort(0, -1, AvailableModelDistanceCompare);

	// calculate current percentages and which group has the largest deviation from requested value
	totalModels = MAX(1, totalModels);
	//float largestDeviation = -9999.f;
	//u32 wantedGroup = 0;
	for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
	{
		if(aRequestedPercentages[popGroupIndex] > 0.0f)
		{
			float percentage = (aTimesUsed[popGroupIndex] * 1.f) / totalModels;
			Assert(percentage >= 0.0f && percentage <= 1.0f);
			Assert(aRequestedPercentages[popGroupIndex] >= 0.0f);

			sGroupPercentage& gp = percentages.Append();
			gp.groupIndex = popGroupIndex;
			gp.deviation = aRequestedPercentages[popGroupIndex] - percentage;
            gp.requestedPercentage = aRequestedPercentages[popGroupIndex];
            gp.randNum = fwRandom::GetRandomNumber();
			gp.zeroInstances = gp.requestedPercentage > 0.0f && percentage < 0.0001f;
		}
	}

	percentages.QSort(0, -1, GroupPercentageCompare);

	if (!cargens)
	{
		sm_preferredSpawnGroupForVehs = -1;

		for (s32 i = 0; i < percentages.GetCount(); ++i)
		{
			if (!GetPopGroups().GetVehGroup(percentages[i].groupIndex).IsFlagSet(POPGROUP_RARE))
			{
				sm_preferredSpawnGroupForVehs = percentages[i].groupIndex;
				break;
			}
		}
	}

	u32 fallbackModel = fwModelId::MI_INVALID;
	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();

	// see if we could use a bike - don't generate a bike if we don't have any peds that can ride it
	s32 canChooseBike = -1;

	if (bRandomizedSelection && pos)
	{
		//////////////////////////////////////////////////////////////////////////
		// get a list of all candidates and randomly select one
		//
		// slower but better for populating car parks etc
		//
		atArray<u32> candidates;

		for (u32 i=0; i<numVehs; ++i)
		{
			const u32 modelIndex = availableCarsByDistance[i].index;

			// very nearby?
			if (availableCarsByDistance[i].distanceSquared==0.0f)
			{
				continue;
			}

			// group checks
			bool bPassedGroupChecks = false;
			bool bInRequestedGroupWithDisparity = false;
			bool bInPrevalentGroup = false;
			for (s32 j=0; j<percentages.GetCount(); ++j)
			{
				const bool inGroup				= CPopCycle::GetPopGroups().IsVehIndexMember(percentages[j].groupIndex, modelIndex);
				const bool bGroupRequested		= percentages[j].requestedPercentage > 0.0f;
				const bool bGroupHasDisparity	= percentages[j].deviation > 0.0f;
				const bool bNotExceededRareness	= percentages[j].requestedPercentage > 0.09f && !GetPopGroups().GetVehGroup(percentages[j].groupIndex).IsFlagSet(POPGROUP_RARE);

				if ( inGroup && bGroupRequested && (bGroupHasDisparity || percentages[j].zeroInstances || !bNotExceededRareness) )
				{
					bPassedGroupChecks = true;
					bInRequestedGroupWithDisparity |= bGroupHasDisparity;
					bInPrevalentGroup |= ( percentages[j].requestedPercentage > 0.19f );
				}
			}
			if (!bPassedGroupChecks)
			{
				continue;
			}

			// available?
			if (!availableCars.IsMemberInGroup(modelIndex))
			{
				continue;
			}

			// Don't violate percentages with big or swanky vehicles
			CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));
			const s32 handlingIndex = vmi->GetHandlingId();
			const CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(handlingIndex);
			const bool superSwanky = vmi->GetVehicleSwankness() >= SWANKNESS_5;
			if (((pHandling && (pHandling->mFlags & MF_IS_BIG)) || superSwanky) && !bInRequestedGroupWithDisparity)
			{
				continue;
			}

			// Don't spawn the same super swanky vehicle less than 300m from a duplicate instance unless
			// it's from a prevalent group
			static const float minimumDistanceToSpawnIdenticalSuperSwankyCar = square(300.0f);
			if (superSwanky && !bInPrevalentGroup && availableCarsByDistance[i].distanceSquared < minimumDistanceToSpawnIdenticalSuperSwankyCar)
			{
				continue;
			}

			if (availableCarsByDistance[i].distanceSquared >= (vmi->GetIdenticalModelSpawnDistance() * vmi->GetIdenticalModelSpawnDistance()))
			{
				candidates.Grow() = modelIndex;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// select
		if (candidates.GetCount())
		{
			const u32 selected = fwRandom::GetRandomNumberInRange(0, candidates.GetCount()-1);
			*pNewCarModelIndex = candidates[selected];
		}
	}
	else if (pos)
    {
		bool foundUsableCarFromRequestedGroup = false;
		bool foundNeededCarFromRequestedGroup = false;

        for (u32 i = 0; i < numVehs; ++i)
        {
			if(availableCarsByDistance[i].distanceSquared == 0.0f)
			{
				break;
			}

			u32 modelIndex = availableCarsByDistance[i].index;

			bool inRequestedGroup = false;
			bool inPrevalentGroup = false;
			bool inRequestedGroupWithDisparity = false;
			bool inRequestedGroupWithZeroInstances = false;

			// If we're in a requested group with zero instances, bail, we've gathered all of the information that we need
			for (s32 j = 0; j < percentages.GetCount() && !inRequestedGroupWithZeroInstances; ++j)
			{
				const bool groupRequested = percentages[j].requestedPercentage > 0.0f;
				const bool groupPrevalent = percentages[j].requestedPercentage > 0.19f;
				const bool groupHasDisparity = percentages[j].deviation > 0.0f;

				const bool inGroup = CPopCycle::GetPopGroups().IsVehIndexMember(percentages[j].groupIndex, modelIndex);
				const bool inThisRequestedGroup = inGroup && groupRequested;

				// Don't violate percentages for groups that are meant to be rare in this area
				if ((percentages[j].requestedPercentage > 0.09f && !GetPopGroups().GetVehGroup(percentages[j].groupIndex).IsFlagSet(POPGROUP_RARE))
					|| groupHasDisparity)
				{
					inRequestedGroup |= inThisRequestedGroup;
					inPrevalentGroup |= inThisRequestedGroup && groupPrevalent;
					inRequestedGroupWithDisparity |= inThisRequestedGroup && groupHasDisparity;
					inRequestedGroupWithZeroInstances = inThisRequestedGroup && percentages[j].zeroInstances;
				}
			}

			if(/**pNewCarModelIndex == fwModelId::MI_INVALID 
				|| */(inRequestedGroup && !foundUsableCarFromRequestedGroup) 
				|| (inRequestedGroupWithDisparity && !foundNeededCarFromRequestedGroup) 
				|| inRequestedGroupWithZeroInstances)
			{
                if (!availableCars.IsMemberInGroup(modelIndex))
                    continue;

				CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));

				const s32 handlingIndex = vmi->GetHandlingId();
				const CHandlingData* pHandling = CHandlingDataMgr::GetHandlingData(handlingIndex);

				const bool superSwanky = vmi->GetVehicleSwankness() >= SWANKNESS_5;

				// Don't violate percentages with big or swanky vehicles
				if (((pHandling && (pHandling->mFlags & MF_IS_BIG)) || superSwanky) && !inRequestedGroupWithDisparity)
				{
					continue;
				}

				if (availableCarsByDistance[i].distanceSquared >= (vmi->GetIdenticalModelSpawnDistance() * vmi->GetIdenticalModelSpawnDistance()))
				{
					if(vmi->GetIsBike())
					{
						if(canChooseBike == -1)
						{
							canChooseBike = gPopStreaming.IsFallbackPedAvailable(true, false);
						}

						if(canChooseBike == 0 && !CPedPopulation::FindSpecificDriverModelForCarToUse(fwModelId(strLocalIndex(modelIndex))).IsValid())
						{
							// Skipping this bike because we don't have any peds that can ride it
							continue;
						}
					}

					if (fallbackModel == fwModelId::MI_INVALID && playerVehicle && playerVehicle->GetModelIndex() == modelIndex)
					{
						fallbackModel = modelIndex;
						continue;
					}

					static const float minimumDistanceToSpawnIdenticalSuperSwankyCar = square(300.0f);

					// Don't spawn the same super swanky vehicle less than 300m from a duplicate instance unless
					// it's from a prevalent group
					if (superSwanky 
						&& !inPrevalentGroup
						&& availableCarsByDistance[i].distanceSquared < minimumDistanceToSpawnIdenticalSuperSwankyCar)
					{
						// If the player's vehicle is the fallback model, replace it with this
						if (fallbackModel == fwModelId::MI_INVALID || (playerVehicle && playerVehicle->GetModelIndex() == fallbackModel))
						{
							fallbackModel = modelIndex;
						}

						continue;
					}

					static const float minimumDistanceToOverrideUsableCar = square(100.0f);

					// Don't spawn the same vehicle less than 100 meters away from a duplicate instance unless we haven't found another usable model
					// Often, we've fulfilled percentage requirements for all other groups while waiting for models to load --
					// as soon as one model from the needy group loads, we use it for all subsequent creations until disparities level out
					if(!foundUsableCarFromRequestedGroup 
						|| availableCarsByDistance[i].distanceSquared >= minimumDistanceToOverrideUsableCar
						|| inRequestedGroupWithZeroInstances)
					{
						*pNewCarModelIndex = modelIndex;

						foundUsableCarFromRequestedGroup |= inRequestedGroup;
						foundNeededCarFromRequestedGroup |= inRequestedGroupWithDisparity;
					}

					// If we're in a requested group with zero instances, bail, we've found our top priority
					if (inRequestedGroupWithZeroInstances)
					{
						break;
					}
				}
			}
        }
    }
	else
	{
		for (s32 i = 0; i < percentages.GetCount(); ++i)
		{
			CLoadedModelGroup wantedCars = availableCars;
			wantedCars.RemoveModelsNotInGroup(percentages[i].groupIndex, false);

			*pNewCarModelIndex = wantedCars.PickRandomCarModel(cargens).Get();

			if (*pNewCarModelIndex != fwModelId::MI_INVALID)
			{
				break;
			}
		}
	}

	if (*pNewCarModelIndex == fwModelId::MI_INVALID)
		*pNewCarModelIndex = fallbackModel;

	return *pNewCarModelIndex != fwModelId::MI_INVALID;
}

#if __BANK

void CPopCycle::GetAmbientPedMultipliers(float * fWeatherMultiplier, float * fHighwayMultiplier, float * fInteriorMultiplier, float * fWantedMultiplier, float * fCombatMultiplier, float * fPedDensityMultiplier)
{
	if(fWeatherMultiplier)
		*fWeatherMultiplier = CalcWeatherMultiplier();
	if(fHighwayMultiplier)
		*fHighwayMultiplier = CalcHighWayPedMultiplier();
	if(fInteriorMultiplier)
		*fInteriorMultiplier = CPopCycle::GetInteriorPedMultiplier();
	if(fWantedMultiplier)
		*fWantedMultiplier = CPopCycle::GetWantedPedMultiplier();
	if(fCombatMultiplier)
		*fCombatMultiplier = CPopCycle::GetCombatPedMultiplier();
	if(fPedDensityMultiplier)
		*fPedDensityMultiplier = CPedPopulation::GetTotalAmbientPedDensityMultiplier();
}

void CPopCycle::GetScenarioPedMultipliers(float * fHighwayMultiplier, float * fInteriorMultiplier, float * fWantedMultiplier, float * fCombatMultiplier, float * fPedDensityMultiplier)
{
	if(fHighwayMultiplier)
		*fHighwayMultiplier = CalcHighWayPedMultiplier();
	if(fInteriorMultiplier)
		*fInteriorMultiplier = CPopCycle::GetInteriorPedMultiplier();
	if(fWantedMultiplier)
		*fWantedMultiplier = CPopCycle::GetWantedPedMultiplier();
	if(fCombatMultiplier)
		*fCombatMultiplier = CPopCycle::GetCombatPedMultiplier();
	if(fPedDensityMultiplier)
		*fPedDensityMultiplier = CPedPopulation::GetTotalAmbientPedDensityMultiplier();
}

#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// Name			:	FindNumberOfEach
// Purpose		:	This function will calculate the number of dealers, gangs, cops and others we want in this zone.
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::UpdatePercentages(void)
{
	// Work out the percentage of cops on the beat.
	// This now comes directly from popcycle.dat
	float desiredPortionCopsPeds	= GetCurrentDesiredPercentageOfCopsPeds() * 0.01f;
	float desiredPortionOtherPeds	= 1.0f - desiredPortionCopsPeds;

	// Figure out the max number of peds.
	int currentMaxNumAmbientPeds = 0; 
	int currentMaxNumScenarioPeds = 0;
	if(HasValidCurrentPopAllocation())
	{
		const float scenarioPedsMultiplier = GET_POPULATION_VALUE(ScenarioPedsMultiplier) / 100.0f;
		const float ambientPedsMultiplier  = GET_POPULATION_VALUE(AmbientPedsMultiplier) / 100.0f;

		currentMaxNumAmbientPeds = GetCurrentPopAllocation().GetMaxNumAmbientPeds();
		currentMaxNumScenarioPeds = GetCurrentPopAllocation().GetMaxNumScenarioPeds();
		currentMaxNumAmbientPeds = (int)((float)currentMaxNumAmbientPeds * CalcWeatherMultiplier() * CalcHighWayPedMultiplier() * CPopCycle::GetInteriorPedMultiplier() * CPopCycle::GetWantedPedMultiplier() * CPopCycle::GetCombatPedMultiplier() * CPedPopulation::GetTotalAmbientPedDensityMultiplier() * ambientPedsMultiplier);
		currentMaxNumScenarioPeds = (int)((float)currentMaxNumScenarioPeds * CalcHighWayPedMultiplier() * CPopCycle::GetInteriorPedMultiplier() * CPopCycle::GetWantedPedMultiplier() * CPopCycle::GetCombatPedMultiplier() * CPedPopulation::GetTotalAmbientPedDensityMultiplier() * scenarioPedsMultiplier);
	}

	currentMaxNumAmbientPeds = static_cast<s32>(CPedPopulation::GetPopCycleMaxPopulationScaler() * currentMaxNumAmbientPeds);
	currentMaxNumScenarioPeds = static_cast<s32>(CPedPopulation::GetPopCycleMaxPopulationScaler() * currentMaxNumScenarioPeds);

	float maxTotalPeds = static_cast<float>(GET_POPULATION_VALUE(MaxTotalPeds));
	float currentTotal = (float)(currentMaxNumAmbientPeds + currentMaxNumScenarioPeds);

	if(currentTotal > maxTotalPeds)
	{
		currentMaxNumAmbientPeds = (int)((currentMaxNumAmbientPeds / currentTotal) * maxTotalPeds);
		currentMaxNumScenarioPeds = (int)((currentMaxNumScenarioPeds / currentTotal) * maxTotalPeds);
	}



#if __BANK
	if (CPedPopulation::GetPopCycleOverrideNumAmbientPeds() >= 0)
	{
		currentMaxNumAmbientPeds = CPedPopulation::GetPopCycleOverrideNumAmbientPeds();
	}
	if (CPedPopulation::GetPopCycleOverrideNumScenarioPeds() >= 0)
	{
		currentMaxNumScenarioPeds = CPedPopulation::GetPopCycleOverrideNumScenarioPeds();
	}
#endif

	// Determine our total maxes for each type.
	sm_currentMaxNumCopPeds			= (s32)rage::Floorf(desiredPortionCopsPeds * currentMaxNumAmbientPeds);
	sm_currentMaxNumAmbientPeds		= (s32)(desiredPortionOtherPeds * currentMaxNumAmbientPeds);
	sm_currentMaxNumScenarioPeds	= (s32)(currentMaxNumScenarioPeds);
}

u32 CPopCycle::CountNumPedsInCombat()
{
	if(fwTimer::GetTimeInMilliseconds() - sm_lastTimePedsInCombatCounted < 2000)
		return sm_currentNumPedsInCombat;

	sm_lastTimePedsInCombatCounted = fwTimer::GetTimeInMilliseconds();
	sm_currentNumPedsInCombat = 0;

	CPed::Pool* pedPool = CPed::GetPool();
	s32 i = pedPool->GetSize();
	while(i--)
	{
		CPed* pPed = pedPool->GetSlot(i);
		if(pPed && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsInCombat))
			sm_currentNumPedsInCombat++;
	}

	return sm_currentNumPedsInCombat;
}

float CPopCycle::GetCurrentZoneVehDirtMin()
{
    float dirtMin = -1.0f;

    if(sm_pCurrZone)
    {
        dirtMin = sm_pCurrZone->m_vehDirtMin;
    }

#if __BANK
	if( sm_enableOverrideVehDirtScale )
	{
		return sm_overridevehDirtScale;
	}
#endif // __BANK

    return dirtMin;
}

float CPopCycle::GetCurrentZoneVehDirtMax()
{
    float dirtMax = -1.0f;

    if(sm_pCurrZone)
    {
        dirtMax = sm_pCurrZone->m_vehDirtMax;
    }

#if __BANK
	if( sm_enableOverrideVehDirtScale )
	{
		return sm_overridevehDirtScale;
	}
#endif // __BANK

    return dirtMax;
}

float CPopCycle::GetCurrentZonePedDirtMin()
{
	float dirtMin = -1.0f;

	if(sm_pCurrZone)
	{
		dirtMin = sm_pCurrZone->m_pedDirtMin;
	}

#if __BANK
	if( sm_enableOverridePedDirtScale )
	{
		return sm_overridePedDirtScale;
	}
#endif // __BANK

	return dirtMin;
}

float CPopCycle::GetCurrentZonePedDirtMax()
{
	float dirtMax = -1.0f;

	if(sm_pCurrZone)
	{
		dirtMax = sm_pCurrZone->m_pedDirtMax;
	}

#if __BANK
	if( sm_enableOverridePedDirtScale )
	{
		return sm_overridePedDirtScale;
	}
#endif // __BANK

	return dirtMax;
}

float CPopCycle::GetCurrentZoneDirtGrowScale()
{
    float dirtGrowScale = 1.0f;

    if(sm_pCurrZone)
    {
        dirtGrowScale = sm_pCurrZone->m_dirtGrowScale;
    }

    return dirtGrowScale;
}

Color32 CPopCycle::GetCurrentZoneDirtCol()
{
    Color32 dirtCol;

    dirtCol.Set(0);

    if(sm_pCurrZone)
    {
        dirtCol = sm_pCurrZone->m_dirtCol;
    }
    
#if __BANK
	if( sm_enableOverrideDirtColor )
	{
		return sm_overrideDirtColor;
	}
#endif // __BANK

    return dirtCol;
}

bool CPopCycle::IsCurrentZoneAirport()
{
	if (sm_pCurrZone)
	{
		return sm_pCurrZone->m_specialAttributeType == SPECIAL_AIRPORT;
	}
	return false;
}


atHashWithStringNotFinal CPopCycle::GetCurrentZoneVfxRegionHashName()
{
	atHashWithStringNotFinal vfxRegionHashName = VFXREGIONINFO_DEFAULT_HASHNAME;

	if(sm_pCurrZone)
	{
		vfxRegionHashName = sm_pCurrZone->m_vfxRegionHashName;
	}

	return vfxRegionHashName;
}

u8 CPopCycle::GetCurrentZonePlantsMgrTxdIdx()
{
    u8 txdIdx = 0x07;	// default

	if(sm_pCurrZone)
	{
		txdIdx = sm_pCurrZone->m_plantsMgrTxdIdx;
	}

	return txdIdx;
}

bool CPopCycle::VehicleBikeGroupActive()
{
	if (sm_vehicleBikeGroup == ~0U)
		return false;

	return GetCurrentVehGroupPercentage(sm_vehicleBikeGroup) > 0;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetCurrentPortionOfMaxCars
// TODO
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::GetCurrentPercentageOfMaxCars()
{
	if (!HasValidCurrentPopAllocation()) 
		return 0.0f;

	const float fPercentage = (float) GetCurrentPopAllocation().GetPercentageOfMaxCars();
	return fPercentage;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetCurrentMaxNumParkedCars
// TODO
/////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::GetCurrentMaxNumParkedCars()
{
	if (!HasValidCurrentPopAllocation()) 
		return 0; 

	s32 currentMaxParkCars = (s32)((GetCurrentPopAllocation().GetMaxNumParkedCars() * CVehiclePopulation::GetTotalParkedCarDensityMultiplier()) + (GetCurrentPopAllocation().GetMaxNumLowPrioParkedCars() * CVehiclePopulation::GetTotalLowPrioParkedCarDensityMultiplier()));

	if(currentMaxParkCars > CVehiclePopulation::ms_iParkedVehiclesUpperLimit)
	{
		currentMaxParkCars = CVehiclePopulation::ms_iParkedVehiclesUpperLimit;
	}

	return currentMaxParkCars;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetCurrentMaxNumLowPrioParkedCars
// TODO
/////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::GetCurrentMaxNumLowPrioParkedCars()
{
	if (!HasValidCurrentPopAllocation()) 
		return 0; 

	return static_cast<s32>(GetCurrentPopAllocation().GetMaxNumLowPrioParkedCars() * CVehiclePopulation::GetTotalLowPrioParkedCarDensityMultiplier());
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetCurrentDesiredPercentageOfCopsCars
// TODO
/////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::GetCurrentDesiredPercentageOfCopsCars()
{
	if (!HasValidCurrentPopAllocation()) 
		return 0; 

	return GetCurrentPopAllocation().GetPercentageCopsCars();
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetCurrentDesiredPercentageOfCopsPeds
// TODO
/////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::GetCurrentDesiredPercentageOfCopsPeds()
{
	if (!HasValidCurrentPopAllocation()) 
		return 0; 

	return GetCurrentPopAllocation().GetPercentageCopsPeds();
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CalcWeatherMultiplier
// TODO
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::CalcWeatherMultiplier()
{
//	s32 Result = 100; //sm_nPercOther[sm_nCurrentTimeIndex][sm_nCurrentTimeOfWeek][sm_nCurrentZoneType];

	 //Don't make so many peds if it is raining.
	const float fMinWeatherMultiplier=0.2f;
	float fWeatherMultiplier = 1.0f + rage::Sqrtf(g_weather.GetRain())*(fMinWeatherMultiplier-1.0f);

	 //If the player is on a taxi mission then we need plenty of peds to pick up as fares even if it is wet.
	if(CTheScripts::GetPlayerIsOnAMission())
	{
	    CPed * pPlayer = CGameWorld::FindLocalPlayer();
	    if(pPlayer)
	    {
        	if((pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))&&(pPlayer->GetMyVehicle()))
        	{
//        	    const int iModelID=pPlayer->m_pMyVehicle->GetModelIndex();
//                if( MODELID_CAR_TAXI==iModelID || MODELID_CAR_CABBIE==iModelID)
//                {
//                    fWeatherMultiplier=1.0f;
//                }
        	}
    	}
	}

	return fWeatherMultiplier;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CalcHighWayPedMultiplier
// TODO
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::CalcHighWayPedMultiplier()
{
	/*
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && pPlayer->GetPlayerInfo()->GetPlayerDataPlayerOnHighway())
	{
		return 0.4f;
	}
	*/

	return 1.0f;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	CalcHighWayCarMultiplier
// TODO
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::CalcHighWayCarMultiplier()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && pPlayer->GetPlayerInfo()->GetPlayerDataPlayerOnHighway())
	{
		return 1.2f;
	}
	return 1.0f;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetInteriorPedMultiplier
//					If we're in an interior and the flag is set for reduced population this function returns the appropriate multiplier
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::GetInteriorPedMultiplier()
{
	CInteriorInst*          pInterior = CPortalVisTracker::GetPrimaryInteriorInst();
	s32                   roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

	if(pInterior && (roomIdx > 0))
	{
		CMloModelInfo* pMloModelInfo = (CMloModelInfo*)pInterior->GetBaseModelInfo();

		Assert(pMloModelInfo->GetIsStreamType());
		{
			if (pMloModelInfo->GetRoomFlags(roomIdx) & ROOM_REDUCE_PEDS)
			{
				return 0.33f;
			}
		}
	}
	return 1.0f;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetInteriorCarMultiplier
//					If we're in an interior and the flag is set for reduced population this function returns the appropriate multiplier
// TODO
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::GetInteriorCarMultiplier()
{
	// Don't reduce vehicle population if the interior we are in is a road tunnel
	// We can identify this by the interior proxy having an ID greater than 1.
	// (index >= 0 indicates a tunnel, whilst index == 1 is reserved for the metro system)
	CInteriorProxy * pProxy = CPortalVisTracker::GetPrimaryInteriorProxy();
	if(pProxy && pProxy->GetGroupId() > 1)
		return 1.0f;

	CInteriorInst * pInterior = CPortalVisTracker::GetPrimaryInteriorInst();
	s32 roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

	if(pInterior && (roomIdx > 0))
	{
		CMloModelInfo* pMloModelInfo = (CMloModelInfo*)pInterior->GetBaseModelInfo();

		Assert (pMloModelInfo->GetIsStreamType());
		{
			if (pMloModelInfo->GetRoomFlags(roomIdx) & ROOM_REDUCE_CARS)
			{
				return 0.33f;
			}
		} 
	}
	return 1.0f;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetWantedPedMultiplier
//					If the wanted level goes up we want fewer peds around
// TODO
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::GetWantedPedMultiplier()
{
	float fMult = 1.0f;
	if (CGameWorld::FindLocalPlayerWanted())
	{
		switch(CGameWorld::FindLocalPlayerWanted()->GetWantedLevel())
		{
		case WANTED_CLEAN:// Intentional fall though.
		case WANTED_LEVEL1:
			break;
		case WANTED_LEVEL2:
			fMult = 0.9f;
			break;
		case WANTED_LEVEL3:
			fMult = 0.7f;
			break;
		case WANTED_LEVEL4:
			fMult = 0.5f;
			break;
		case WANTED_LEVEL5:
			fMult = 0.2f;
			break;
		default:
			break;
		}
	}

	if(NetworkInterface::IsGameInProgress())
	{
		static const int nDesiredPedAmount = 36;	// A sane value related to MAX_NUM_NETOBJPEDS
		const int nAmbientPeds = CPedPopulation::ms_nNumOnFootCop + CPedPopulation::ms_nNumOnFootSwat + CPedPopulation::ms_nNumOnFootAmbient;
		if (nAmbientPeds < nDesiredPedAmount)	// Don't allow too many ambient peds globally, max 50 are allowed in MP atm.
		{
			fMult = Max(fMult, sm_fMinWantedMultiplierMP);
		}
	}
	return fMult;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetWantedCarsMultiplier
//					If the wanted level goes up we want fewer cars around
// TODO
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::GetWantedCarMultiplier()
{
	float fMult = 1.0f;
	if (CGameWorld::FindLocalPlayerVehicle())
	{
		switch(CGameWorld::FindLocalPlayerWanted()->GetWantedLevel())
		{
			case WANTED_CLEAN:
			case WANTED_LEVEL1:
				break;
			case WANTED_LEVEL2:
				fMult = 0.8f;
				break;
			case WANTED_LEVEL3:
				fMult = 0.6f;
				break;
			case WANTED_LEVEL4:
				fMult = 0.6f;
				break;
			case WANTED_LEVEL5:
				fMult = 0.6f;
				break;
			default:
				break;
		}
	}
	else if (CGameWorld::FindLocalPlayerWanted())
	{
		switch(CGameWorld::FindLocalPlayerWanted()->GetWantedLevel())
		{
		case WANTED_CLEAN:
		case WANTED_LEVEL1:
			break;
		case WANTED_LEVEL2:
			fMult = 0.9f;
			break;
		case WANTED_LEVEL3:
			fMult = 0.6f;
			break;
		case WANTED_LEVEL4:
			fMult = 0.4f;
			break;
		case WANTED_LEVEL5:
			fMult = 0.2f;
			break;
		default:
			break;
		}
	}

	if(NetworkInterface::IsGameInProgress())
	{
		fMult = ScaleMultiplier_SP_to_MP(fMult);

		// Clamp
		fMult = Clamp(fMult, sm_fMinWantedMultiplierMP, 1.0f);
	}
	return fMult;
}

// Modify a multiplier values to account for the ratio between vehicle upper limits in MP & SP
float CPopCycle::ScaleMultiplier_SP_to_MP(const float fMult)
{
	Assert(NetworkInterface::IsGameInProgress());

	const float fRatio = ((float)CVehiclePopulation::ms_iAmbientVehiclesUpperLimitMP) / ((float)CVehiclePopulation::ms_iAmbientVehiclesUpperLimit);
	float fInvMult = 1.0f - fMult;
	fInvMult *= fRatio;
	return (1.0f - fInvMult);
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetCombatPedMultiplier
//					As more peds enter combat, we want fewer peds around
/////////////////////////////////////////////////////////////////////////////////

float CPopCycle::GetCombatPedMultiplier()
{
	if(sm_currentNumPedsInCombat < sm_iMinPedsInCombatForPopMultiplier)
		return 1.0f;
	else if(sm_currentNumPedsInCombat >= sm_iMaxPedsInCombatForPopMultiplier)
		return sm_fPedsInCombatMinPopMultiplier;
	
	float S = (((float)sm_currentNumPedsInCombat) / ((float)sm_iMaxPedsInCombatForPopMultiplier-sm_iMinPedsInCombatForPopMultiplier));

	S = Clamp(S, 0.0f, 1.0f);
	S *= sm_fPedsInCombatMinPopMultiplier;
	S = 1.0f - S;

	return sm_bUseCombatMultiplier ? S : 1.0f;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetCombatCarMultiplier
//					As more peds enter combat, we want fewer peds around
/////////////////////////////////////////////////////////////////////////////////
float CPopCycle::GetCombatCarMultiplier()
{
	if(sm_currentNumPedsInCombat < sm_iMinPedsInCombatForPopMultiplier)
		return 1.0f;
	else if(sm_currentNumPedsInCombat >= sm_iMaxPedsInCombatForPopMultiplier)
		return sm_fPedsInCombatMinPopMultiplier;

	float S = (((float)sm_currentNumPedsInCombat) / ((float)sm_iMaxPedsInCombatForPopMultiplier-sm_iMinPedsInCombatForPopMultiplier));

	S = Clamp(S, 0.0f, 1.0f);
	S *= sm_fPedsInCombatMinPopMultiplier;
	S = 1.0f - S;

	if(NetworkInterface::IsGameInProgress())
		S = ScaleMultiplier_SP_to_MP(S);

	return sm_bUseCombatMultiplier ? S : 1.0f;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetTemporaryMaxNumScenarioPeds
//					A cutscene can put a ceiling on the number of peds allowed
// TODO
///////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::GetTemporaryMaxNumScenarioPeds()
{
	/*//TF **REPLACE WITH NEW CUTSCENE**
	if ((JimmyCutSceneManager::GetInstance() && JimmyCutSceneManager::GetInstance()->IsRunning()) && CCutsceneManager::ms_iNumMaxPedsAllowed >= 0)
	{
		return CCutsceneManager::ms_iNumMaxPedsAllowed / 3;
	}*/
	return 9999;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetTemporaryMaxNumAmbientPeds
//					A cutscene can put a ceiling on the number of peds allowed
// TODO
/////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::GetTemporaryMaxNumAmbientPeds()
{
	//TF **REPLACE WITH NEW CUTSCENE**
	/*
	if ((JimmyCutSceneManager::GetInstance() && JimmyCutSceneManager::GetInstance()->IsRunning()) && CCutsceneManager::ms_iNumMaxPedsAllowed >= 0)
	{
		return (CCutsceneManager::ms_iNumMaxPedsAllowed * 2) / 3;
	}
	*/
	return 9999;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	GetTemporaryMaxNumCars
//					A cutscene can put a ceiling on the number of cars allowed
// TODO
/////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::GetTemporaryMaxNumCars()
{
	//TF **REPLACE WITH NEW CUTSCENE**
/*	if ((JimmyCutSceneManager::GetInstance() && JimmyCutSceneManager::GetInstance()->IsRunning()) && CCutsceneManager::ms_iNumMaxCarsAllowed >= 0)
	{
		return CCutsceneManager::ms_iNumMaxCarsAllowed;
	}
	*/
	return 9999;
}

void CPopCycle::GetNetworkPedModelsForCurrentZone(CLoadedModelGroup &ambientModelsList, CLoadedModelGroup &gangModelsList)
{
	Assertf(sm_popGroups.GetPedCount() <= MaxPopCycleGroups, "popcycle.dat has more than %d popgroup columns. MaxPopCycleGroups will need to be increased in code", MaxPopCycleGroups);
	u32 popGroupCount = Min(MaxPopCycleGroups, (u32)sm_popGroups.GetPedCount());

    CLoadedModelGroup *currentModelGroup = &ambientModelsList;

    for (u32 popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
    {
        if(GetCurrentPedGroupPercentage(popGroupIndex) > 0.0f)
        {
            if(GetPopGroups().GetPedGroup(popGroupIndex).IsFlagSet(POPGROUP_IS_GANG))
            {
                currentModelGroup = &gangModelsList;
            }
            else
            {
                currentModelGroup = &ambientModelsList;
            }

            const u32 numPedsInGroup = GetPopGroups().GetNumPeds(popGroupIndex);

            for (u32 pedIndex = 0; pedIndex < numPedsInGroup; pedIndex++)
            {
                u32 modelIndex = GetPopGroups().GetPedIndex(popGroupIndex, pedIndex);

#if __ASSERT
				fwModelId pedModelId((strLocalIndex(modelIndex)));
				Assertf(pedModelId.IsValid(), "Model index %u is not valid (found in ped group %s).", modelIndex, GetPopGroups().GetPedGroup(popGroupIndex).GetName().GetCStr());
#endif

				// Block certain models from being streamed in because their motion task is not set up to be synchronized over the network.
				if (CWildlifeManager::GetInstance().CheckWildlifeMultiplayerSpawnConditions(modelIndex))
				{
					currentModelGroup->AddMember(modelIndex);
				}
            }
        }
    }
}

void CPopCycle::GetNetworkVehicleModelsForCurrentZone(CLoadedModelGroup &commonModelsList, CLoadedModelGroup &zoneSpecificModelsList)
{
    Assertf(sm_popGroups.GetPedCount() <= MaxPopCycleGroups, "popcycle.dat has more than %d popgroup columns. MaxPopCycleGroups will need to be increased in code", MaxPopCycleGroups);
	u32 popGroupCount = Min(MaxPopCycleGroups, (u32)sm_popGroups.GetVehCount());

    unsigned numZoneSpecificModels   = 0;
    bool     overridingSpecificModel = false;

    if(HasValidCurrentPopAllocation() && GetCurrentPopSchedule().GetOverrideVehicleModel() != fwModelId::MI_INVALID)
    {
        zoneSpecificModelsList.AddMember(GetCurrentPopSchedule().GetOverrideVehicleModel());
        numZoneSpecificModels   = 1;
        overridingSpecificModel = true;
    }

    for (u32 popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
    {
        if(GetCurrentVehGroupPercentage(popGroupIndex) > 0.0f)
        {
            const u32 numVehiclesInGroup = GetPopGroups().GetNumVehs(popGroupIndex);

            for (u32 vehicleIndex = 0; vehicleIndex < numVehiclesInGroup; vehicleIndex++)
            {
                u32 modelIndex = GetPopGroups().GetVehIndex(popGroupIndex, vehicleIndex);

                if(GetPopGroups().GetVehGroup(popGroupIndex).IsFlagSet(POPGROUP_NETWORK_COMMON))
                {
                    commonModelsList.AddMember(modelIndex);
                }
                else
                {
                    if(!overridingSpecificModel && numZoneSpecificModels < MAX_NUM_IN_LOADED_MODEL_GROUP)
                    {
                        zoneSpecificModelsList.AddMember(modelIndex);
                        numZoneSpecificModels++;
                    }
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	PickMostWantingGroupOfModels
// Go through the loaded models and analyzes the groups that they belong to. The group that needs the new model
// most gets it.
// TODO
/////////////////////////////////////////////////////////////////////////////////
s32 CPopCycle::PickMostWantingGroupOfModels(CLoadedModelGroup &ExistingGroup, bool bPeds, s32 **ppGroupList)
{
	static	float aPercentages[MaxPopCycleGroups];
	static	float aRequestedPercentages[MaxPopCycleGroups];
	static	float aNeed[MaxPopCycleGroups];
	static	s32 aNeedyGroup[MaxPopCycleGroups];

	if(!HasValidCurrentPopAllocation())
		return 0;

	u32 popGroupCount=0;
	if(bPeds)
	{
		Assertf(sm_popGroups.GetPedCount() <= MaxPopCycleGroups, "CPopCycle::PickMostWantingGroupOfModels - popcycle.dat has more than %d popgroup columns. MaxPopCycleGroups will need to be increased in code", MaxPopCycleGroups);
		popGroupCount = Min(MaxPopCycleGroups, (u32)sm_popGroups.GetPedCount());
	}
	else
	{
		Assertf(sm_popGroups.GetVehCount() <= MaxPopCycleGroups, "CPopCycle::PickMostWantingGroupOfModels - popcycle.dat has more than %d popgroup columns. MaxPopCycleGroups will need to be increased in code", MaxPopCycleGroups);
		popGroupCount = Min(MaxPopCycleGroups, (u32)sm_popGroups.GetVehCount());
	}

	u32	popGroupIndex, totalModels = 0;

	for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
	{
		aPercentages[popGroupIndex] = 0.0f;
		if(bPeds)
			aRequestedPercentages[popGroupIndex] = GetCurrentPedGroupPercentage(popGroupIndex)*0.01f;//GetCurrentPopAllocation().GetPopGroupPercentage(popGroupIndex) * 0.01f;
		else
			aRequestedPercentages[popGroupIndex] = GetCurrentVehGroupPercentage(popGroupIndex)*0.01f;//GetCurrentPopAllocation().GetPopGroupPercentage(popGroupIndex) * 0.01f;
	}

	// Go through all of the loaded peds and give each group that they belong to 1 point.
	s32 numMembers = ExistingGroup.CountMembers();

	for (s32 m = 0; m < numMembers; m++)
	{
		u32 model = ExistingGroup.GetMember(m);

		// Only consider models that aren't about to be streamed out.
		fwModelId modelId((strLocalIndex(model)));
		if ( !CModelInfo::GetAssetsAreDeletable(modelId))
		{
			totalModels++;
			for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
			{
				if ( (bPeds && GetPopGroups().IsPedIndexMember(popGroupIndex, model)) || ( (!bPeds) && GetPopGroups().IsVehIndexMember(popGroupIndex, model)) )
				{
					aPercentages[popGroupIndex] += 1.0f;
				}
			}
		}
	}

	if(bPeds)
	{
		// Apply special modifier for particularly needed peds.
		for(popGroupIndex = 0; popGroupIndex < popGroupCount; ++popGroupIndex)
		{
			// If one of the gang groups needs peds and doesn't have any it gets priority.
			if(GetPopGroups().GetPedGroup(popGroupIndex).IsFlagSet(POPGROUP_IS_GANG))
			{
				if (aRequestedPercentages[popGroupIndex] > 0.0f && aPercentages[popGroupIndex] == 0.0f)
				{
					aRequestedPercentages[popGroupIndex] *= 100.0f;
				}
			}
			// If the player is in water aquatic peds get priority.
			else if (GetPopGroups().GetPedGroup(popGroupIndex).IsFlagSet(POPGROUP_AQUATIC))
			{
				if (aRequestedPercentages[popGroupIndex] > 0.0f && CWildlifeManager::GetInstance().ShouldSpawnMoreAquaticPeds())
				{
					aRequestedPercentages[popGroupIndex] *= CWildlifeManager::GetInstance().GetIncreasedAquaticSpawningFactor();
				}
			}
			else if (GetPopGroups().GetPedGroup(popGroupIndex).IsFlagSet(POPGROUP_AERIAL))
			{
				if (aRequestedPercentages[popGroupIndex] > 0.0f && CWildlifeManager::GetInstance().ShouldSpawnMoreAerialPeds())
				{
					aRequestedPercentages[popGroupIndex] *= CWildlifeManager::GetInstance().GetIncreasedGroundWildlifeSpawningFactor();
				}
			}
			else if (GetPopGroups().GetPedGroup(popGroupIndex).IsFlagSet(POPGROUP_WILDLIFE))
			{
				if (aRequestedPercentages[popGroupIndex] > 0.0f && CWildlifeManager::GetInstance().ShouldSpawnMoreGroundWildlifePeds())
				{
					aRequestedPercentages[popGroupIndex] *= CWildlifeManager::GetInstance().GetIncreasedGroundWildlifeSpawningFactor();
				}
			}
		}
	}

	u32 numWithNeed = 0;
	for (popGroupIndex = 0; popGroupIndex < popGroupCount; popGroupIndex++)
	{
		aPercentages[popGroupIndex] /= MAX(1, totalModels);
		Assert(aPercentages[popGroupIndex] >= 0.0f && aPercentages[popGroupIndex] <= 1.0f);
		Assert(aRequestedPercentages[popGroupIndex] >= 0.0f);

		// Now calculate the number that determines how much this group needs a ped.
		if (aRequestedPercentages[popGroupIndex] > 0.0f)
		{
			// if this is a rare group and we already have a model loaded from it don't add it to the list, we don't want more models
			if (aPercentages[popGroupIndex] > 0.f)
			{
				if ((bPeds && GetPopGroups().GetPedGroup(popGroupIndex).IsFlagSet(POPGROUP_RARE)) || (!bPeds && GetPopGroups().GetVehGroup(popGroupIndex).IsFlagSet(POPGROUP_RARE)))
				{
					continue;
				}
			}

			// Now build a list of all the groups that have any need. Keep track of the number of them.
			aNeed[numWithNeed] = aRequestedPercentages[popGroupIndex] - aPercentages[popGroupIndex];
			aNeedyGroup[numWithNeed] = popGroupIndex;
			numWithNeed++;
		}
	}
	//Assert(numWithNeed > 0);
	if(numWithNeed <= 0)
	{
		return 0;
	}

	// Sort the list by neediness.
	bool bChange = true;
	while (bChange)
	{
		bChange = false;

		for (popGroupIndex = 0; popGroupIndex < numWithNeed-1; popGroupIndex++)
		{
			if (aNeed[popGroupIndex] < aNeed[popGroupIndex+1])
			{
				float temp1 = aNeed[popGroupIndex];
				s32 temp2 = aNeedyGroup[popGroupIndex];
				aNeed[popGroupIndex] = aNeed[popGroupIndex+1];
				aNeedyGroup[popGroupIndex] = aNeedyGroup[popGroupIndex+1];
				aNeed[popGroupIndex+1] = temp1;
				aNeedyGroup[popGroupIndex+1] = temp2;
				bChange = true;
			}
		}
	}
	*ppGroupList = aNeedyGroup;
	return numWithNeed;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	AreTaxisRequired
// Purpose		:	If the taxis are required at all this function returns true.
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
bool CPopCycle::AreTaxisRequired()
{
	return true;		// Taxis seem required everywhere on the map in IV.
	
	//static const u32 taxiGroupNameHash = ATSTRINGHASH("TAXIS", 0x8DCB100);
	//u32 popGroupIndexOfTaxis = 0;
	//if(GetPopGroups().FindIndexFromNameHash(taxiGroupNameHash, popGroupIndexOfTaxis))
	//{
	//	return (sm_zones[sm_pCurrZone->ZonePopulationType].m_timePopInfo[sm_nCurrentTimeIndex][sm_nCurrentTimeOfWeek].m_nPercTypeGroup.GetPercentage(popGroupIndexOfTaxis) > 0);
	//}
	//else
	//{
	//	return false;
	//}
}

CPopZone *CPopCycle::GetCurrentActiveZone(void)
{
    return sm_pCurrZone;
}

/////////////////////////////////////////////////////////////////////////////////
// Name			:	ClearPopCycleInfo
// Purpose		:	To clean the sm_popSchedules and sm_popGroups arrays out.
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::ResetPopGroupsAndSchedules(void)
{
	sm_popSchedules.Reset();
	sm_popSchedulesMaxNumCars = 0;
	sm_popGroups.Reset();
}

void CPopCycle::RefreshPopGroupData()
{
	// Resize percentage arrays to be the same size as the popgroup array
	int size = sm_popGroups.GetPedCount();
	sm_currentPedPercentages.Reset();
	sm_currentPedPercentages.Reserve(size);
	sm_currentPedPercentages.Resize(size);

	size = sm_popGroups.GetVehCount();
	sm_currentVehPercentages.Reset();
	sm_currentVehPercentages.Reserve(size);
	sm_currentVehPercentages.Resize(size);

	for(int i=0; i<sm_popGroups.GetPedCount(); i++)
		sm_currentPedPercentages[i] = 0;

	for(int i=0; i<sm_popGroups.GetVehCount(); i++)
		sm_currentVehPercentages[i] = 0;

	sm_commonVehicleGroup = ~0u;
	sm_vehicleBikeGroup = ~0u;

	atHashString vehMid("VEH_MID",0xB98B07D0);
	sm_popGroups.FindVehGroupFromNameHash(vehMid, sm_commonVehicleGroup);

	atHashString vehBikes("VEH_BIKES",0x14B612EF);
	sm_popGroups.FindVehGroupFromNameHash(vehMid, sm_vehicleBikeGroup);
}

void CPopCycle::LoadPopGroupPatch(const char* file)
{
	atHashString patchFileHash = atHashString(file);

	if (sm_popGroupPatches.Find(patchFileHash) == -1)
		sm_popGroupPatches.PushAndGrow(patchFileHash);

	sm_popGroups.LoadPatch(file);
	RefreshPopGroupData();
}

void CPopCycle::UnloadPopGroupPatch(const char* file)
{
	sm_popGroupPatches.DeleteMatches(atHashString(file));
	sm_popGroups.UnloadPatch(file);
	RefreshPopGroupData();
}

void CPopCycle::LoadPopGroupsFile(const char* file)
{
	sm_popGroups.Flush();
	sm_popGroups.Reset();

	// Try to load the new file and refresh the data
	if (fwPsoStoreLoader::LoadDataIntoObject(file, META_FILE_EXT, sm_popGroups))
	{
	#if __BANK
		const char* servicePath = "Population/popgroups";
		fwPsoStoreLoadInstance inst(sm_popGroups.parser_GetPointer());

		parRestUnregisterSingleton(servicePath, inst.GetInstance());
		parRestRegisterSingleton(servicePath, sm_popGroups, file);
	#endif
	}

	RefreshPopGroupData();
}

void CPopCycle::UnloadPopGroupsFile(const char* /*file*/)
{
	// Just load via the original method
	LoadPopGroups();
}

/////////////////////////////////////////////////////////////////////////////////
// name:		LoadPopGroups
// description:	Load the pedgrp.dat file and cargrp.dat files into the
//				sm_popGroups array
// Parameters:	None
// Returns:		Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::LoadPopGroups(void)
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::POPGRP_FILE);

	if (Verifyf(DATAFILEMGR.IsValid(pData), "Pop group file %s is not available", pData->m_filename))
	{
		// Load original data
		LoadPopGroupsFile(pData->m_filename);

		// Load any active patches on top
		for (s32 i = 0; i < sm_popGroupPatches.GetCount(); i++)
		{
			CDataFileMgr::DataFile* pPopGroupPatch = DATAFILEMGR.FindDataFile(sm_popGroupPatches[i]);

			LoadPopGroupPatch(pPopGroupPatch->m_filename);
		}
	}
	else
	{
		sm_popGroups.Reset();
		RefreshPopGroupData();
	}
}

#if __DEV
//#define WRITE_FILE_OUT 		// If this is defined the game will output a formatted version of Popcycle.datnew
#endif


/////////////////////////////////////////////////////////////////////////////////
// Name			:	LoadPopSchedules
// Purpose		:	Loads (or saves) the timecycle.dat files.
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::LoadPopSchedules(void)
{
	// Load pop schedule files.
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::POPSCHED_FILE);
	if(DATAFILEMGR.IsValid(pData))
	{
		// Read in this particular file
		//char* fileName = "popcycle.dat";
		sm_popSchedules.Load(pData->m_filename);
		sm_popSchedules.PostLoad();
		//NOTFINAL_ONLY(PARSER.SaveObject(pData->m_filename, "meta", &sm_popSchedules, parManager::XML));
	}
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	LoadPopZoneToPopScheduleBindings
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPopCycle::LoadPopZoneToPopScheduleBindings()
{
	// Load pop schedule files.
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::ZONEBIND_FILE);
	if (DATAFILEMGR.IsValid(pData))
	{
		// Read in this particular file
		//char* fileName = "zonebind.dat";
		LoadPopZoneToPopScheduleBindingsDataFile_PSO(pData->m_filename);
	}
}
#if __BANK
void CPopCycle::ReloadPopZoneToPopScheduleBindings()
{
	char filename[1024];
	bool bOk = BANKMGR.OpenFile(filename, 1024, "*.pso.meta", false, "Browse for zonebind pso.meta file to load");

	if(bOk)
	{
		// Read in this particular file
		LoadPopZoneToPopScheduleBindingsDataFile_META(filename);
	}
}
#endif
/////////////////////////////////////////////////////////////////////////////////
// Name			:	LoadPopZoneToPopScheduleBindingsDataFile
// Purpose		:	TODO
// Parameters	:	None
// Returns		:	Nothing
/////////////////////////////////////////////////////////////////////////////////
const u32 NoPopScheduleHash    	= ATSTRINGHASH("NO_POP_SCHEDULE", 0x07d143edb);
#if 0
const u32 ScumminessLevel0Hash 	= ATSTRINGHASH("SL_POSH", 0x0b876d280);
const u32 ScumminessLevel1Hash 	= ATSTRINGHASH("SL_NICE", 0x0b192cf1c);
const u32 ScumminessLevel2Hash 	= ATSTRINGHASH("SL_ABOVE_AV", 0x03f9c1cde);
const u32 ScumminessLevel3Hash 	= ATSTRINGHASH("SL_BELOW_AV", 0x0fe811e7b);
const u32 ScumminessLevel4Hash 	= ATSTRINGHASH("SL_CRAP", 0x0d0dd77d4);
const u32 ScumminessLevel5Hash 	= ATSTRINGHASH("SL_SCUM", 0x0b3a5ff70);
const u32 SpecialNoneHash		= ATSTRINGHASH("SPECIAL_NONE", 0x067e4daf8);
const u32 SpecialAirportHash	= ATSTRINGHASH("SPECIAL_AIRPORT", 0x0e9b99648);
#endif

void CPopZoneData::OnPostSetCallback(parTreeNode* UNUSED_PARAM(pTreeNode), parMember* UNUSED_PARAM(pMember), void* pInstance)
{
	CPopZoneData* zoneData = static_cast<CPopZoneData*>(pInstance);
	if (zoneData)
		CPopCycle::ApplyZoneBindData(zoneData);
}

void CPopCycle::ApplyZoneBindData(const CPopZoneData* data)
{
	Assert(data);
	for (s32 i = 0; i < data->zones.GetCount(); ++i)
	{
		const CPopZoneData::sZone& zone = data->zones[i];

		CPopZone* pZone = CPopZones::GetPopZone(zone.zoneName.c_str());
		Assertf(pZone, "CPopCycle::LoadPopZoneToPopScheduleBindings - couldn't find a popzone called '%s'", zone.zoneName.c_str());
		int popZoneId = CPopZones::GetPopZoneId(pZone);

		if (popZoneId == CPopZones::INVALID_ZONEID)
			continue;

		s32 popScheduleIndexForSP = -1;
		if (zone.spName.GetHash() != NoPopScheduleHash)
		{
#if __ASSERT
			bool scheduleFoundForSP =
#endif // __ASSERT
				sm_popSchedules.GetIndexFromNameHash(zone.spName.GetHash(), popScheduleIndexForSP);
			Assertf(scheduleFoundForSP, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Unrecognized pop schedule name '%s'", zone.spName.TryGetCStr());
		}

		s32 popScheduleIndexForMP = -1;
        if (zone.mpName.GetHash() != NoPopScheduleHash)
        {
#if __ASSERT
		    bool scheduleFoundForMP =
#endif // __ASSERT
		    sm_popSchedules.GetIndexFromNameHash(zone.mpName.GetHash(), popScheduleIndexForMP);
		    Assertf(scheduleFoundForMP, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Unrecognized pop schedule name '%s'", zone.mpName.TryGetCStr());
        }

        Assertf(popScheduleIndexForSP != -1 || popScheduleIndexForMP != -1, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Zone with no SP or MP pop schedule specified!");
		CPopZones::SetPopScheduleIndices(popZoneId, popScheduleIndexForSP, popScheduleIndexForMP);

		CPopZones::SetScumminess(popZoneId, zone.scumminessLevel);

		// set zone dirt info
		Color32 dirtCol(zone.dirtRed, zone.dirtGreen, zone.dirtBlue);
		CPopZones::SetVehDirtRange(popZoneId, zone.vehDirtMin, zone.vehDirtMax);
		CPopZones::SetPedDirtRange(popZoneId, zone.pedDirtMin, zone.pedDirtMax);
		CPopZones::SetDirtGrowScale(popZoneId, zone.vehDirtGrowScale);
		CPopZones::SetDirtColor(popZoneId, dirtCol);

		CPopZones::SetVfxRegionHashName(popZoneId, zone.vfxRegion);
		CPopZones::SetPlantsMgrTxdIdx(popZoneId, zone.plantsMgrTxdIdx);

		CPopZones::SetLawResponseDelayType(popZoneId, zone.lawResponseTime);
		CPopZones::SetVehicleResponseType(popZoneId, zone.lawResponseType);

		CPopZones::SetMPGangTerritoryIndex(popZoneId, zone.mpGangTerritoryIndex);

		CPopZones::SetSpecialAttribute(popZoneId, zone.specialZoneAttribute);

		CPopZones::SetPopulationRangeScaleParameters(popZoneId, zone.popBaseRangeScale, zone.popRangeScaleStart, zone.popRangeScaleEnd, zone.popRangeMultiplier);

		CPopZones::SetAllowScenarioWeatherExits(popZoneId, zone.allowScenarioWeatherExits);
		CPopZones::SetAllowHurryAwayToLeavePavement(popZoneId, zone.allowHurryAwayToLeavePavement);
	}
}

void CPopCycle::LoadPopZoneToPopScheduleBindingsDataFile_META(const char* fileName)
{
	fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CPopZoneData>();
	fwPsoStoreLoadInstance inst;

	loader.Load(fileName, inst);

	CPopZoneData* zoneData = reinterpret_cast<CPopZoneData*>(inst.GetInstance());

	parRestRegisterSingleton("popzone/zonebind", *zoneData, fileName);

	ApplyZoneBindData(zoneData);

	loader.Unload(inst);
}

void CPopCycle::LoadPopZoneToPopScheduleBindingsDataFile_PSO(const char* fileName)
{
	// Try to load into a temporary CPopZoneData object.
	CPopZoneData zoneData;
	if (Verifyf(fwPsoStoreLoader::LoadDataIntoObject(fileName, META_FILE_EXT, zoneData), "Failed to load zonebind metadata file: %s", fileName))
	{
		ApplyZoneBindData(&zoneData);
	}
}

void CPopCycle::LoadZoneBindFile(const char* fileName)
{
	LoadPopZoneToPopScheduleBindingsDataFile_PSO(fileName);
}

void CPopCycle::UnloadZoneBindFile(const char* UNUSED_PARAM(fileName))
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::ZONEBIND_FILE);
	LoadPopZoneToPopScheduleBindingsDataFile_PSO(pData->m_filename);
}

void CPopCycle::LoadPopSchedFile(const char* fileName)
{
	CPopCycle::GetPopSchedules().Reset();
	CPopCycle::GetPopSchedules().Load(fileName);
	CPopCycle::GetPopSchedules().PostLoad();
}

void CPopCycle::UnloadPopSchedFile(const char* UNUSED_PARAM(fileName))
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::POPSCHED_FILE);
	CPopCycle::LoadPopSchedFile(pData->m_filename);
}


#if 0 // use to convert from zonebind.dat to .meta
void CPopCycle::LoadPopZoneToPopScheduleBindingsDataFile2(const char* fileName)
{
	CPopZoneData zoneData;

	// Try to open the file.
	//CFileMgr::SetDir("common:/data/");
	FileHandle	fd = CFileMgr::OpenFile(fileName, "rb");
	Assertf(CFileMgr::IsValidFileHandle(fd), "Problem opening zone binding file.");
	//CFileMgr::SetDir("");

	char line[2048];
	while(CFileMgr::ReadLine(fd, &line[0], 1024))
	{	
		// Check for empty or commented out lines...
		if(line[0] == '/' || line[0] == '\0' || line[1] == '\0' || line[2] == '\0')	// Note: ps3 returns empty lines as space,0
		{
			continue;
		}

		CPopZoneData::sZone zone;

		// Read the part of the line with the name of the zone.
		char popZoneNameBuff[256];
		const char* seps = " ,\t";
		char* pToken = NULL;
		pToken = strtok(line, seps);
		Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Badly formed line encountered.");
		strncpy(popZoneNameBuff, pToken, 256 );
		zone.zoneName = popZoneNameBuff;

		// Read the part of the line with the name of the SP pop schedule to use for that zone.
		pToken = strtok(NULL, seps);
		Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line.");

		// Get the index of the SP schedule we're interested in.
		u32 popScheduleNameHashForSP = atStringHash(pToken);
		s32 popScheduleIndexForSP = -1;
		zone.spName = pToken;

		if(popScheduleNameHashForSP != NoPopScheduleHash)
		{
#if __ASSERT
			bool scheduleFoundForSP =
#endif // __ASSERT
				sm_popSchedules.GetIndexFromNameHash(popScheduleNameHashForSP, popScheduleIndexForSP);
			Assertf(scheduleFoundForSP, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Unrecognized pop schedule name %s.", pToken);
		}

		// Read the part of the line with the name of the MP pop schedule to use for that zone.
		pToken = strtok(NULL, seps);
		Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line.");

		// Get the index of the MP schedule we're interested in.
		u32 popScheduleNameHashForMP = atStringHash(pToken);
		s32 popScheduleIndexForMP = -1;
		zone.mpName = pToken;

		if(popScheduleNameHashForMP != NoPopScheduleHash)
		{
#if __ASSERT
			bool scheduleFoundForMP =
#endif // __ASSERT
				sm_popSchedules.GetIndexFromNameHash(popScheduleNameHashForMP, popScheduleIndexForMP);
			Assertf(scheduleFoundForMP, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Unrecognized pop schedule name %s.", pToken);
		}

		Assertf(popScheduleIndexForSP != -1 || popScheduleIndexForMP != -1, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Zone with no SP or MP pop schedule specified!");

		CPopZone* pZone = CPopZones::GetPopZone(popZoneNameBuff);
		Assertf(pZone, "CPopCycle::LoadPopZoneToPopScheduleBindings - couldn't find a zone called %s in POPZONE.IPL", popZoneNameBuff);
		int popZoneId = CPopZones::GetPopZoneId(pZone);

		// Read the part of the line with scumminess level to use for that zone.
		pToken = strtok(NULL, seps);
		Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line.");
		if (popZoneId != CPopZones::INVALID_ZONEID)
		{
			int iScumminessLevel = CPopZones::GetZoneScumminessFromString(pToken);
			CPopZones::SetScumminess(popZoneId, iScumminessLevel);
			zone.scumminessLevel = (eZoneScumminess)iScumminessLevel;
		}

		// read vehicle dirt info
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			float dirtMin = (float)atof(pToken);
			zone.vehDirtMin = dirtMin;
			zone.pedDirtMin = dirtMin;

			pToken = strtok(NULL, seps);
			Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line, missing dirtMax");
			float dirtMax = pToken ? (float)atof(pToken) : -1.f;
			zone.vehDirtMax = dirtMax;
			zone.pedDirtMax = dirtMax;

			pToken = strtok(NULL, seps);
			Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line, missing dirtGrowScale");
			float dirtGrowScale = pToken ? (float)atof(pToken) : 1.0f;
			zone.vehDirtGrowScale = dirtGrowScale;

			pToken = strtok(NULL, seps);
			Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line, missing dirtRed");
			float dirtR = pToken ? (float)atof(pToken) : 0.f;
			pToken = strtok(NULL, seps);
			Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line, missing dirtGreen");
			float dirtG = pToken ? (float)atof(pToken) : 0.f;
			pToken = strtok(NULL, seps);
			Assertf(pToken, "CPopCycle::LoadPopZoneToPopScheduleBindings(), Not enough items on the line, missing dirtBlue");
			float dirtB = pToken ? (float)atof(pToken) : 0.f;

			Color32 dirtCol(dirtR, dirtG, dirtB, 1.f);
			zone.dirtRed = dirtCol.GetRed();
			zone.dirtGreen = dirtCol.GetGreen();
			zone.dirtBlue = dirtCol.GetBlue();

			if(popZoneId != CPopZones::INVALID_ZONEID)
			{
				// set zone dirt info
				CPopZones::SetVehDirtRange(popZoneId, dirtMin, dirtMax);
				CPopZones::SetPedDirtRange(popZoneId, dirtMin, dirtMax);
				CPopZones::SetDirtGrowScale(popZoneId, dirtGrowScale);
				CPopZones::SetDirtColor(popZoneId, dirtCol);
			}
		}

		// read in the vfx region data
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			if(popZoneId != CPopZones::INVALID_ZONEID)
			{
				CPopZones::SetVfxRegionHashName(popZoneId, atHashWithStringNotFinal(pToken));
			}
			zone.vfxRegion = pToken;
		}

		// read in the law response time
		int iLawResponseTime = LAW_RESPONSE_DELAY_MEDIUM;
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			iLawResponseTime = CDispatchData::GetLawResponseDelayTypeFromString(pToken);
		}
		zone.lawResponseTime = (eLawResponseTime)iLawResponseTime;

		int iLawZoneType = VEHICLE_RESPONSE_DEFAULT;
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			iLawZoneType = CDispatchData::GetVehicleResponseTypeFromString(pToken);
		}
		zone.lawResponseType = (eZoneVehicleResponseType)iLawZoneType;

		// read in the multiplayer gang territory index (-1 means zone is not part of a territory)
		pToken = strtok(NULL, seps);
		s32 territoryIndex = -1;
		if (pToken)
		{
			territoryIndex = atoi(pToken);
		}
		zone.mpGangTerritoryIndex = territoryIndex;

		// Read in the Special Zone attributes
		int iSpecialZoneAttribute = SPECIAL_NONE;
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			iSpecialZoneAttribute = CPopZones::GetSpecialZoneAttributeFromString(pToken);
		}
		zone.specialZoneAttribute = (eZoneSpecialAttributeType)iSpecialZoneAttribute;

		if(popZoneId != CPopZones::INVALID_ZONEID)
		{
			// Set the zones schedule.
			CPopZones::SetPopScheduleIndices(popZoneId, popScheduleIndexForSP, popScheduleIndexForMP);

			// Set the law response delay type
			CPopZones::SetLawResponseDelayType(popZoneId, iLawResponseTime);

			// Set the law response delay type
			CPopZones::SetVehicleResponseType(popZoneId, iLawZoneType);

			// Set the MP Gang Territory index
			CPopZones::SetMPGangTerritoryIndex(popZoneId, territoryIndex);

			// Set the special attributes
			CPopZones::SetSpecialAttribute(popZoneId, iSpecialZoneAttribute);
		}


		float fPopMultStartZ = 0.0f;
		float fPopMultEndZ = 0.0f;
		float fPopHeightMultiplier = 1.0f;

		float fPopBaseRangeScale = 1.0f;

		// Read in the low (start) Z used for multiplying population range based on height
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			fPopMultStartZ = (float)atof(pToken);
		}

		// Read in the high (end) Z used for multiplying population range based on height	
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			fPopMultEndZ = (float)atof(pToken);
		}

		// Read in the total height-based multiplier on population range when height >= fPopMultEndZ
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			fPopHeightMultiplier = (float)atof(pToken);
		}

		// Read in the total base range multiplier for this zone
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			fPopBaseRangeScale = (float)atof(pToken);
		}

		if(popZoneId != CPopZones::INVALID_ZONEID)
		{
			CPopZones::SetPopulationRangeScaleParameters(popZoneId, fPopBaseRangeScale, fPopMultStartZ, fPopMultEndZ, fPopHeightMultiplier);
		}

		zone.popRangeScaleStart = fPopMultStartZ;
		zone.popRangeScaleEnd = fPopMultEndZ;
		zone.popBaseRangeScale = fPopBaseRangeScale;
		zone.popRangeMultiplier = fPopHeightMultiplier;

		int iAllowScenarioWeatherExits = 1; // Default to allowing scenario weather exits
		// Read in the AllowScenarioWeatherExits flag
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			iAllowScenarioWeatherExits = (int)atoi(pToken);
		}
		if (popZoneId != CPopZones::INVALID_ZONEID)
		{
			CPopZones::SetAllowScenarioWeatherExits(popZoneId, iAllowScenarioWeatherExits);
		}
		zone.allowScenarioWeatherExits = iAllowScenarioWeatherExits != 0;

		int iAllowHurryAwayToLeavePavements = 0; // Default to not allow this.
		// Read in the CareAboutPavementsForShockingEvents flag
		pToken = strtok(NULL, seps);
		if (pToken)
		{
			iAllowHurryAwayToLeavePavements = (int)atoi(pToken);
		}
		zone.allowHurryAwayToLeavePavement = iAllowHurryAwayToLeavePavements != 0;
		if (popZoneId != CPopZones::INVALID_ZONEID)
		{
			CPopZones::SetAllowHurryAwayToLeavePavement(popZoneId, iAllowHurryAwayToLeavePavements);

			zoneData.zones.PushAndGrow(zone);
		}
	}

	PARSER.SaveObject("X:/zonebind", "meta", &zoneData, parManager::XML);
	// Close the zone binding file.
	CFileMgr::CloseFile(fd);
}
#endif
