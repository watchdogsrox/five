//
// ai/ExpensiveProcess.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "ai/ExpensiveProcess.h"

// Rage headers
#include "bank/bank.h"
#include "profile/group.h"
#include "profile/page.h"

// Game headers
#include "Peds/PedDefines.h"
#include "Vehicles/VehicleDefines.h"

AI_OPTIMISATIONS()

// Performance monitors
PF_PAGE(AIDistributedProcesses, "AI Distributed Processes");
PF_GROUP(ProcessCost);
PF_LINK(AIDistributedProcesses, ProcessCost);
PF_TIMER(RedistributionCost, ProcessCost);
PF_TIMER(OverallCost, ProcessCost);
PF_TIMER(EntityScanner, ProcessCost);
PF_TIMER(AcquaintanceScanner, ProcessCost);
PF_TIMER(EncroachmentScanner, ProcessCost);
PF_TIMER(GroupScanner, ProcessCost);
PF_TIMER(VehicleCollisionScanner, ProcessCost);
PF_TIMER(PotentialCollisionScanner, ProcessCost);
PF_TIMER(FireScanner, ProcessCost);
PF_TIMER(PassengerEventScanner, ProcessCost);
PF_TIMER(TargettingLos, ProcessCost);
PF_TIMER(ShockingEventScanner, ProcessCost);

////////////////////////////////////////////////////////////////////////////////

CExpensiveProcess::CExpensiveProcess(ProcessType processType)
: m_ProcessType(processType)
, m_iAllocatedInterval(-1)
, m_bRegistered(false)
, m_bProcessedOnceInCurrentSlot(false)
, m_IndexInProcessArray(INVALID_EXPENSIVE_PROCESS_ARRAY_INDEX)
{
	Assert((int)processType == (int)m_ProcessType);	// Make sure it fit.
	CompileTimeAssert(CExpensiveProcess::MAX_PROCESS_TYPES < (1 << CExpensiveProcess::kBitsForProcessType));
}

////////////////////////////////////////////////////////////////////////////////

CExpensiveProcess::~CExpensiveProcess()
{
	if(IsRegistered())
	{
		UnregisterSlot();
	}
}

////////////////////////////////////////////////////////////////////////////////

// Static initialisation
CExpensiveProcessDistributer::ProcessArray CExpensiveProcessDistributer::ms_aProcesses[CExpensiveProcess::MAX_PROCESS_TYPES];
u16 CExpensiveProcessDistributer::ms_aProcessesCounts[CExpensiveProcess::MAX_PROCESS_TYPES];
u16 CExpensiveProcessDistributer::ms_iCurrentIntervals[CExpensiveProcess::MAX_PROCESS_TYPES];
u8 CExpensiveProcessDistributer::ms_iCurrentPhases[CExpensiveProcess::MAX_PROCESS_TYPES];
CExpensiveProcessDistributer::ProcessArray CExpensiveProcessDistributer::ms_aProcessesToSort;
s32 CExpensiveProcessDistributer::ms_iLastSortedType = 0;
CExpensiveProcessDistributer::CProcessInfo CExpensiveProcessDistributer::ms_StaticProcessInfo[CExpensiveProcess::MAX_PROCESS_TYPES] = 
{
	CProcessInfo(15, "PPT_PedScanner"),
	CProcessInfo(15, "PPT_VehicleScanner"),
	CProcessInfo(8, "PPT_DoorScanner"),
	CProcessInfo(2,  "PPT_AcquaintanceScanner"),
	CProcessInfo(10, "PPT_EncroachmentScanner"),
	CProcessInfo(10, "PPT_GroupScanner"),
	CProcessInfo(15, "PPT_VehicleCollisionScanner"),
	CProcessInfo(15, "PPT_ObjectCollisionScanner"),
	CProcessInfo(3,  "PPT_PotentialCollisionScanner"),
	CProcessInfo(15, "PPT_FireScanner"),
	CProcessInfo(15, "PPT_DriverEventScanner"),
	CProcessInfo(2, 4, "PPT_TargettingLos"),
	CProcessInfo(10, 8, "PPT_ShockingEventScanner"),
	CProcessInfo(60, "PPT_AgitationScanner"),
	CProcessInfo(3, 3, "PPT_CoverFinderDistributer"),
	CProcessInfo(60, 1, "PPT_AudioDistributer"),
	CProcessInfo(2, 4, "PPT_CombatTransitionDistributer"),
	CProcessInfo(1, 60, "PPT_ScenarioExitDistributer"),
	CProcessInfo(15, "VPT_PedScanner"),
	CProcessInfo(15, "VPT_VehicleScanner"),
	CProcessInfo(15, "VPT_ObjectScanner"),
	CProcessInfo(4, "VPT_SteeringDistributer"),
	CProcessInfo(4, "VPT_BrakingDistributer"),
	CProcessInfo(4, "VPT_LaneChangeDistributer"),
	CProcessInfo(2, "VPT_DummyAvoidanceDistributer"),
	CProcessInfo(15, "VPT_TailgateDistributer"),
	CProcessInfo(10, "VPT_NavmeshLOSDistributer"),
	CProcessInfo(2, "VPT_UpdateJunctionsDistributer"),
	CProcessInfo(7, "VPT_SirenReactionDistributer"),
	CProcessInfo(60, "VPT_AdverseConditionsDistributer"),
	CProcessInfo(30, "VPT_HeliAvoidanceDistributer")
};

////////////////////////////////////////////////////////////////////////////////

void CExpensiveProcessDistributer::Init(u32 UNUSED_PARAM(uInitMode))
{
	ms_StaticProcessInfo[CExpensiveProcess::PPT_AudioDistributer].m_iMaxFramesBetweenUpdatesForIrregularUse =
		ms_StaticProcessInfo[CExpensiveProcess::PPT_AudioDistributer].m_iDesiredInterval * 2;

	// Timesliced vehicle updates may involve calls to a number of CExpensiveProcess objects, and
	// if we are not careful, the timesliced updates are unlikely to coincide with the CExpensiveProcess intervals.
	// For this reason, we make use of the new m_iMaxFramesBetweenUpdatesForIrregularUse variable to make sure
	// we still get to update these processes during the timesliced AI updates. In most cases, just setting the
	// irregular interval to the same as the desired regular interval is probably fine.
	ms_StaticProcessInfo[CExpensiveProcess::VPT_SteeringDistributer].m_iMaxFramesBetweenUpdatesForIrregularUse = ms_StaticProcessInfo[CExpensiveProcess::VPT_SteeringDistributer].m_iDesiredInterval;
	ms_StaticProcessInfo[CExpensiveProcess::VPT_BrakingDistributer].m_iMaxFramesBetweenUpdatesForIrregularUse = ms_StaticProcessInfo[CExpensiveProcess::VPT_BrakingDistributer].m_iDesiredInterval;
	ms_StaticProcessInfo[CExpensiveProcess::VPT_DummyAvoidanceDistributer].m_iMaxFramesBetweenUpdatesForIrregularUse = ms_StaticProcessInfo[CExpensiveProcess::VPT_DummyAvoidanceDistributer].m_iDesiredInterval;
	ms_StaticProcessInfo[CExpensiveProcess::VPT_LaneChangeDistributer].m_iMaxFramesBetweenUpdatesForIrregularUse = ms_StaticProcessInfo[CExpensiveProcess::VPT_LaneChangeDistributer].m_iDesiredInterval;
	ms_StaticProcessInfo[CExpensiveProcess::VPT_UpdateJunctionsDistributer].m_iMaxFramesBetweenUpdatesForIrregularUse = ms_StaticProcessInfo[CExpensiveProcess::VPT_UpdateJunctionsDistributer].m_iDesiredInterval;
	ms_StaticProcessInfo[CExpensiveProcess::VPT_VehicleScanner].m_iMaxFramesBetweenUpdatesForIrregularUse = 20;		// Can perhaps go a bit slower than the normal 15 frames for this one?
	ms_StaticProcessInfo[CExpensiveProcess::VPT_ObjectScanner].m_iMaxFramesBetweenUpdatesForIrregularUse = 20;		// Can perhaps go a bit slower than the normal 15 frames for this one?
	ms_StaticProcessInfo[CExpensiveProcess::VPT_AdverseConditionsDistributer].m_iMaxFramesBetweenUpdatesForIrregularUse = ms_StaticProcessInfo[CExpensiveProcess::VPT_AdverseConditionsDistributer].m_iDesiredInterval;
	// TODO: Think more about if we want this for other expensive processes used during timesliced updates,
	// such as m_TailgateDistributer, VPT_AdverseConditionsDistributer, VPT_PedScanner, and VPT_ObjectScanner.
	// So far we haven't come across any situations where these seem necessary.

	for(s32 i = CExpensiveProcess::FIRST_PED_PROCESS; i < CExpensiveProcess::MAX_PED_PROCESSES; i++)
	{
		ms_aProcesses[i].Reserve(MAXNOOFPEDS);
		ms_aProcesses[i].Resize(MAXNOOFPEDS);
	}

	for(s32 i = CExpensiveProcess::FIRST_VEHICLE_PROCESS; i < CExpensiveProcess::MAX_VEHICLE_PROCESSES; i++)
	{
		ms_aProcesses[i].Reserve(VEHICLE_POOL_SIZE);
		ms_aProcesses[i].Resize(VEHICLE_POOL_SIZE);
	}

	// NULL the arrays
	for(s32 i = 0; i < CExpensiveProcess::MAX_PROCESS_TYPES; i++)
	{
		for(s32 j = 0; j < ms_aProcesses[i].GetCount(); j++)
		{
			ms_aProcesses[i][j] = NULL;
		}

		// If this fails, m_iMaxFramesBetweenUpdatesForIrregularUse has been set to a higher value than we can expect
		// to work properly. We use a very limited number of bits for the frame count so if anybody waits for too long
		// between irregular updates, the frame count will have wrapped around. For example, if 8 bits are used and you set
		// m_iMaxFramesBetweenUpdatesForIrregularUse to some value close to 1<<8, like 253, you would have a very small window
		// within which the irregular update would have to occur and it probably wouldn't work right.
		Assert(ms_StaticProcessInfo[i].m_iMaxFramesBetweenUpdatesForIrregularUse <= CExpensiveProcess::GetMaxRecommendedFramesBetweenIrregularUpdates());
	}

	for(s32 i = 0; i < CExpensiveProcess::MAX_PROCESS_TYPES; i++)
	{
		ms_aProcessesCounts[i]  = 0;
		ms_iCurrentIntervals[i] = 0;
		ms_iCurrentPhases[i] = 0;
	}

	s32 iMaxListSize = Max(MAXNOOFPEDS, VEHICLE_POOL_SIZE);
	ms_aProcessesToSort.Reserve(iMaxListSize);
}

////////////////////////////////////////////////////////////////////////////////

void CExpensiveProcessDistributer::Update()
{
	PF_FUNC(RedistributionCost);

	AdvanceCurrentInterval();
	AdvanceSortType();

	const int type = ms_iLastSortedType;
	s8 iMaxProcessesPerFrame = ms_StaticProcessInfo[type].m_iMaxProcessPerFrame;
	if( iMaxProcessesPerFrame > 0 )
	{
		unsigned int iInterval = ms_StaticProcessInfo[type].m_iOriginalDesiredInterval;
		const unsigned int iCurrentProcessCount = ms_aProcessesCounts[type];

		while((unsigned int)iMaxProcessesPerFrame < iCurrentProcessCount/iInterval)
		{
			++iInterval;
		}

		Assert(iInterval <= 0xff);
		ms_StaticProcessInfo[type].m_iDesiredInterval = (u8)iInterval;

		unsigned int phase = (ms_iCurrentIntervals[type] % iInterval);
		aiAssert(phase <= 0xff);
		ms_iCurrentPhases[type] = (u8)phase;
	}

	// Resort the current list
	ms_aProcessesToSort.Resize(0);

	// Cache the process array
	ProcessArray& aProcesses = ms_aProcesses[type];

	// Find all movable processes
	for(s32 i = 0; i < aProcesses.GetCount(); i++)
	{
		if(aProcesses[i] && aProcesses[i]->GetHasBeenProcessedInCurrentSlot())
		{
			ms_aProcessesToSort.Push(aProcesses[i]);
			aProcesses[i] = NULL;
			--ms_aProcessesCounts[type];
		}
	}

	// Re-add all processes
	for(s32 i = 0; i < ms_aProcessesToSort.GetCount(); i++)
	{
		AddProcess(ms_aProcessesToSort[i]);
	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CExpensiveProcessDistributer::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Process Distribution", false);

	for(s32 i = 0; i < CExpensiveProcess::MAX_PROCESS_TYPES; i++)
	{
		char buff[128];
#if __ASSERT
		formatf(buff, 128, "%s interval", ms_StaticProcessInfo[i].m_szName);
#else
		formatf(buff, 128, "%d interval", i);
#endif // __ASSERT

		bank.AddSlider(buff, &ms_StaticProcessInfo[i].m_iDesiredInterval, 0, 60, 1);
	}

	bank.PopGroup(); // "ProcessDistribution"
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

bool CExpensiveProcessDistributer::AddProcess(CExpensiveProcess* pProcess)
{
	aiAssert(pProcess);

	// Cache the process array
	const CExpensiveProcess::ProcessType processType = pProcess->GetType();
	ProcessArray& aProcesses = ms_aProcesses[processType];

	// Find a free slot for the process
	for(s32 i = 0; i < aProcesses.GetCount(); i++)
	{
		if(aProcesses[i] == NULL)
		{
			aProcesses[i] = pProcess;
			pProcess->SetHasBeenProcessedInCurrentSlot(false);
			pProcess->SetInterval(i % ms_StaticProcessInfo[processType].m_iDesiredInterval);
			Assert(ms_aProcessesCounts[processType] < 0xffff);
			++ms_aProcessesCounts[processType];
			Assert(i < INVALID_EXPENSIVE_PROCESS_ARRAY_INDEX);
			pProcess->SetIndexInProcessArray(i);
			return true;
		}
	}

	aiAssertf(0, "Out of slots for process type [%s]", ms_StaticProcessInfo[pProcess->GetType()].m_szName);
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CExpensiveProcessDistributer::RemoveProcess(CExpensiveProcess* pProcess)
{
	aiAssert(pProcess);

	// Cache the process array
	const CExpensiveProcess::ProcessType type = pProcess->GetType();
	ProcessArray& aProcesses = ms_aProcesses[type];

	u32 index = pProcess->GetIndexInProcessArray();
	Assert(aProcesses[index] == pProcess);

	pProcess->SetIndexInProcessArray(INVALID_EXPENSIVE_PROCESS_ARRAY_INDEX);

	aProcesses[index] = NULL;
	--ms_aProcessesCounts[type];
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CExpensiveProcessDistributer::AdvanceCurrentInterval()
{
	for(s32 i = 0; i < CExpensiveProcess::MAX_PROCESS_TYPES; i++)
	{
		unsigned int newInterval = ms_iCurrentIntervals[i] + 1;
		unsigned int newPhase;
		if(newInterval >= ms_aProcesses[i].GetCount())
		{
			newInterval = newPhase = 0;
		}
		else
		{
			newPhase = ms_iCurrentPhases[i] + 1;
			if(newPhase >= ms_StaticProcessInfo[i].m_iDesiredInterval)
			{
				newPhase = 0;
			}
		}
		aiAssert(newInterval <= 0xffff);
		aiAssert(newPhase <= 0xff);
		aiAssertf(newPhase == (newInterval % ms_StaticProcessInfo[i].m_iDesiredInterval), "%s: newPhase - %i; newInterval - %i, DesiredInterval - %i", ms_StaticProcessInfo[i].m_szName, newPhase, newInterval, ms_StaticProcessInfo[i].m_iDesiredInterval);
		ms_iCurrentIntervals[i] = (u16)newInterval;
		ms_iCurrentPhases[i] = (u8)newPhase;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CExpensiveProcessDistributer::AdvanceSortType()
{
	int newType = ms_iLastSortedType + 1;
	if(newType >= CExpensiveProcess::MAX_PROCESS_TYPES)
	{
		newType = 0;
	}
	ms_iLastSortedType = newType;
}
