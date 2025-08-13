//
// ai/ExpensiveProcess.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef EXPENSIVE_PROCESS_H
#define EXPENSIVE_PROCESS_H

// Rage headers
#include "atl/array.h"
#include "profile/element.h"

// Framework headers
#include "ai/aichannel.h"
#include "fwsys/timer.h"

////////////////////////////////////////////////////////////////////////////////

#define INVALID_EXPENSIVE_PROCESS_ARRAY_INDEX		(0x1ff)

class CExpensiveProcess
{
public:

	// Process types
	enum ProcessType
	{
		// Ped processes
		FIRST_PED_PROCESS = 0,

		PPT_PedScanner = FIRST_PED_PROCESS,
		PPT_VehicleScanner,
		PPT_DoorScanner,
		PPT_AcquaintanceScanner,
		PPT_EncroachmentScanner,
		PPT_GroupScanner,
		PPT_VehicleCollisionScanner,
		PPT_ObjectCollisionScanner,
		PPT_PotentialCollisionScanner,
		PPT_FireScanner,
		PPT_PassengerEventScanner,
		PPT_TargettingLos,
		PPT_ShockingEventScanner,
		PPT_AgitationScanner,
		PPT_CoverFinderDistributer,
		PPT_AudioDistributer,
		PPT_CombatTransitionDistributer,
		PPT_ScenarioExitDistributer,

		// Last ped process
		MAX_PED_PROCESSES,

		// Vehicle processes
		FIRST_VEHICLE_PROCESS = MAX_PED_PROCESSES, 

		VPT_PedScanner = FIRST_VEHICLE_PROCESS,
		VPT_VehicleScanner,
		VPT_ObjectScanner,
		VPT_SteeringDistributer,
		VPT_BrakingDistributer,
		VPT_LaneChangeDistributer,
		VPT_DummyAvoidanceDistributer,
		VPT_TailgateDistributer,
		VPT_NavmeshLOSDistributer,
		VPT_UpdateJunctionsDistributer,
		VPT_AdverseConditionsDistributer,
		VPT_HeliAvoidanceDistributer,
		VPT_SirenReactionDistributer,
		
		// Last vehicle process
		MAX_VEHICLE_PROCESSES,

		MAX_PROCESS_TYPES = MAX_VEHICLE_PROCESSES,
	};

	CExpensiveProcess(ProcessType processType);
	~CExpensiveProcess();

	// Register
	void RegisterSlot();

	// Unregister
	void UnregisterSlot();

	// Check if registered
	bool IsRegistered() const;

	// Should be processed?
	bool ShouldBeProcessedThisFrame();

	// Like ShouldBeProcessedThisFrame(), but read-only and not supporting irregular use.
	bool ShouldBeProcessedThisFrameSimple() const;

	// 
	void StartProcess();
	void StopProcess();

	// Get type
	ProcessType GetType() const;

	// Get allocated interval
	s32 GetInterval() const;

	// Set the interval
	void SetInterval(s32 iInterval);

	// Get has been processed
	bool GetHasBeenProcessedInCurrentSlot() const;
	
	//  Set has been processed
	void SetHasBeenProcessedInCurrentSlot(bool bTrue);

	// Get the number of frames since ShouldBeProcessedThisFrame() was last called and returned true
	unsigned int GetFramesSinceLastProcess() const;

	// Reset the frame count used internally for irregular updates
	void ResetFramesSinceLastProcess();

	void SetIndexInProcessArray(u32 index) { m_IndexInProcessArray = index; }
	u32 GetIndexInProcessArray() const { return m_IndexInProcessArray; }

#if __ASSERT
	static const unsigned int GetMaxRecommendedFramesBetweenIrregularUpdates()
	{	return (1 << kBitsForFrameCount)/2;	}
#endif	// __ASSERT

private:

	// Number of bits used for the frame count
	static const unsigned int kBitsForFrameCount = 8;

	// Mask used for frame count comparisons
	static const u32 kFrameCountMask = (1 << kBitsForFrameCount) - 1;

	// Number of bits used for the process type
	static const unsigned int kBitsForProcessType = 5;

	//
	// Members
	//

	// Frame counter used for irregular updates
	u32 m_FrameCount : kBitsForFrameCount;

	// Index in process array
	u32 m_IndexInProcessArray : 9;

	// Type of process
	u32 m_ProcessType : kBitsForProcessType;

	// Interval
	s32 m_iAllocatedInterval : 8;

	// Flags
	u32 m_bRegistered : 1;
	u32 m_bProcessedOnceInCurrentSlot : 1;
};

////////////////////////////////////////////////////////////////////////////////

class CExpensiveProcessDistributer
{
	friend CExpensiveProcess;
public:

	// Initialisation
	static void Init(u32 uInitMode);

	// Processing
	static void Update();

#if __BANK
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Add new process
	static bool AddProcess(CExpensiveProcess* pProcess);

	// Remove process
	static bool RemoveProcess(CExpensiveProcess* pProcess);

	// Check if should be processed
	static bool ShouldBeProcessedThisFrame(CExpensiveProcess* pProcess);
	static bool ShouldBeProcessedThisFrameSimple(const CExpensiveProcess* pProcess);

	// Advance the intervals
	static void AdvanceCurrentInterval();

	// Advance the type to sort
	static void AdvanceSortType();

	// Cannot be constructed
	CExpensiveProcessDistributer();
	~CExpensiveProcessDistributer();

	//
	// Members
	//

	// Process lists
	typedef atArray<CExpensiveProcess*> ProcessArray;
	static ProcessArray ms_aProcesses[CExpensiveProcess::MAX_PROCESS_TYPES];

	// Current non NULL processes
	static u16 ms_aProcessesCounts[CExpensiveProcess::MAX_PROCESS_TYPES];

	// Current intervals
	static u16 ms_iCurrentIntervals[CExpensiveProcess::MAX_PROCESS_TYPES];

	// PURPOSE:	Each element of this is basically currentInterval % desiredInterval,
	//			stored off so we don't have to recompute that for each user.
	static u8 ms_iCurrentPhases[CExpensiveProcess::MAX_PROCESS_TYPES];

	// List of processes to sort
	static ProcessArray ms_aProcessesToSort;

	// Last sorted process type
	static s32 ms_iLastSortedType;

	// Process info
	class CProcessInfo
	{
	public:
		CProcessInfo(unsigned int iDesiredInterval, const char* BANK_ONLY(szName)) 
			: m_iDesiredInterval((u8)iDesiredInterval)
			, m_iOriginalDesiredInterval((u8)iDesiredInterval)
			, m_iMaxProcessPerFrame(-1)
			, m_iMaxFramesBetweenUpdatesForIrregularUse(0)
#if __BANK
			, m_szName(szName)
#endif // __BANK
		{
			aiAssert(iDesiredInterval <= 0xff);
		}

		CProcessInfo(unsigned int iDesiredInterval, s8 iMaxProcessPerFrame, const char* BANK_ONLY(szName)) 
			: m_iDesiredInterval((u8)iDesiredInterval)
			, m_iOriginalDesiredInterval((u8)iDesiredInterval)
			, m_iMaxProcessPerFrame(iMaxProcessPerFrame)
			, m_iMaxFramesBetweenUpdatesForIrregularUse(0)
#if __BANK
			, m_szName(szName)
#endif // __BANK
		{
			aiAssert(iDesiredInterval <= 0xff);
		}

		~CProcessInfo()
		{
		}

		// Interval
		u8 m_iDesiredInterval;
		u8 m_iOriginalDesiredInterval;

		// Max desired processes per frame (to be considered "initialized" it needs to be > 0
		s8 m_iMaxProcessPerFrame;

		// If non-zero, this controls after how many frames a process gets to update outside of
		// its regular slot. This can be used when we don't attempt to update the process on each
		// frame, for example due to higher level timeslicing.
		u8 m_iMaxFramesBetweenUpdatesForIrregularUse;

#if __BANK
		// Name for debug
		const char* m_szName;
#endif // __BANK
	};

	// Per type info
	static CProcessInfo ms_StaticProcessInfo[CExpensiveProcess::MAX_PROCESS_TYPES];
};

////////////////////////////////////////////////////////////////////////////////

inline void CExpensiveProcess::RegisterSlot()
{
	aiAssert(!IsRegistered());
	if(CExpensiveProcessDistributer::AddProcess(this))
	{
		m_bRegistered = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CExpensiveProcess::UnregisterSlot()
{
	aiAssert(IsRegistered());
	if(CExpensiveProcessDistributer::RemoveProcess(this))
	{
		m_bRegistered = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

inline bool CExpensiveProcess::IsRegistered() const
{ 
	return m_bRegistered; 
}

////////////////////////////////////////////////////////////////////////////////

inline bool CExpensiveProcess::ShouldBeProcessedThisFrame()
{
	aiAssert(IsRegistered());
	return CExpensiveProcessDistributer::ShouldBeProcessedThisFrame(this);
}

////////////////////////////////////////////////////////////////////////////////

inline bool CExpensiveProcess::ShouldBeProcessedThisFrameSimple() const
{
	aiAssert(IsRegistered());
	return CExpensiveProcessDistributer::ShouldBeProcessedThisFrameSimple(this);
}

////////////////////////////////////////////////////////////////////////////////

EXT_PF_TIMER(OverallCost);

inline void CExpensiveProcess::StartProcess()
{
	m_bProcessedOnceInCurrentSlot = true;
	PF_START(OverallCost);
}

////////////////////////////////////////////////////////////////////////////////

inline void CExpensiveProcess::StopProcess()
{
	PF_STOP(OverallCost);
}

////////////////////////////////////////////////////////////////////////////////

inline CExpensiveProcess::ProcessType CExpensiveProcess::GetType() const
{
	return (ProcessType)m_ProcessType;
}

////////////////////////////////////////////////////////////////////////////////

inline s32 CExpensiveProcess::GetInterval() const
{
	return m_iAllocatedInterval;
}

////////////////////////////////////////////////////////////////////////////////

inline void CExpensiveProcess::SetInterval(s32 iInterval)
{
	Assert(iInterval >= -0x80 && iInterval <= 0x7f);
	m_iAllocatedInterval = iInterval;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CExpensiveProcess::GetHasBeenProcessedInCurrentSlot() const
{
	return m_bProcessedOnceInCurrentSlot;
}

////////////////////////////////////////////////////////////////////////////////

inline void CExpensiveProcess::SetHasBeenProcessedInCurrentSlot(bool bTrue)
{
	m_bProcessedOnceInCurrentSlot = bTrue;
}

////////////////////////////////////////////////////////////////////////////////

inline unsigned int CExpensiveProcess::GetFramesSinceLastProcess() const
{
	return (fwTimer::GetFrameCount() - m_FrameCount) & kFrameCountMask;
}

////////////////////////////////////////////////////////////////////////////////

inline void CExpensiveProcess::ResetFramesSinceLastProcess()
{
	m_FrameCount = fwTimer::GetFrameCount();
}

////////////////////////////////////////////////////////////////////////////////

inline bool CExpensiveProcessDistributer::ShouldBeProcessedThisFrame(CExpensiveProcess* pProcess)
{
	aiAssert(pProcess);
	const CExpensiveProcess::ProcessType type = pProcess->GetType();
	const CProcessInfo& info = ms_StaticProcessInfo[type];
	bool ret = false;
	aiAssert((ms_iCurrentIntervals[type] % info.m_iDesiredInterval) == ms_iCurrentPhases[type]);
	if(ms_iCurrentPhases[type] == (unsigned int)pProcess->GetInterval())
	{
		ret = true;
	}
	else if(info.m_iMaxFramesBetweenUpdatesForIrregularUse)
	{
		u32 diff = pProcess->GetFramesSinceLastProcess();
		if(diff >= info.m_iMaxFramesBetweenUpdatesForIrregularUse)
		{
			ret = true;
		}
	}

	if(ret)
	{
		pProcess->ResetFramesSinceLastProcess();
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////////

inline bool CExpensiveProcessDistributer::ShouldBeProcessedThisFrameSimple(const CExpensiveProcess* pProcess)
{
	aiAssert(pProcess);
	const CExpensiveProcess::ProcessType type = pProcess->GetType();
	aiAssert((ms_iCurrentIntervals[type] % ms_StaticProcessInfo[type].m_iDesiredInterval) == ms_iCurrentPhases[pProcess->GetType()]);
	return ms_iCurrentPhases[type] == pProcess->GetInterval();
}

#endif // EXPENSIVE_PROCESS_H
