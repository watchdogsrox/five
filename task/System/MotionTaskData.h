// FILE :    MotionTaskData.h
// PURPOSE : Load in motion task data set information
// CREATED : 12-8-2010

#ifndef MOTION_TASK_DATA_H
#define MOTION_TASK_DATA_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"

// Framework headers

// Game headers

enum MotionTaskType
{
	PED_ON_FOOT = 0,
	PED_IN_WATER,
	PED_ON_MOUNT,
	BIRD_ON_FOOT,
	FLIGHTLESS_BIRD_ON_FOOT,
	HORSE_ON_FOOT,
	BOAR_ON_FOOT,
	COW_ON_FOOT,
	COUGAR_ON_FOOT,
	COYOTE_ON_FOOT,
	DEER_ON_FOOT,
	PIG_ON_FOOT,
	RAT_ON_FOOT,
	RETRIEVER_ON_FOOT,
	ROTTWEILER_ON_FOOT,
	FISH_IN_WATER,
	HORSE_SWIMMING,
	CAT_ON_FOOT,
	RABBIT_ON_FOOT,

	MAX_MOTION_TASK_TYPES
};

struct sMotionTaskData
{
	MotionTaskType		m_Type;
	rage::atHashString	m_ClipSetHash;
	rage::atHashString	m_JumpClipSetHash;
	rage::ConstString	m_JumpNetworkName;
	rage::atHashString	m_FallClipSetHash;
	rage::atHashString	m_SlopeScrambleClipSetHash;
	rage::atHashString	m_SwimmingClipSetHash;
	rage::atHashString	m_SwimmingClipSetBaseHash;
	bool				m_UseQuadrupedJump;
	float				m_WalkTurnRate;
	float				m_RunTurnRate;
	float				m_SprintTurnRate;
	float				m_MotionAnimRateScale;
	float				m_QuadWalkRunTransitionTime;
	float				m_QuadRunWalkTransitionTime;
	float				m_QuadRunSprintTransitionTime;
	float				m_QuadSprintRunTransitionTime;
	float				m_QuadWalkTurnScaleFactor;
	float				m_QuadRunTurnScaleFactor;
	float				m_QuadSprintTurnScaleFactor;
	float				m_QuadWalkTurnScaleFactorPlayer;
	float				m_QuadRunTurnScaleFactorPlayer;
	float				m_QuadSprintTurnScaleFactorPlayer;
	float				m_PlayerBirdFlappingSpeedMultiplier;
	float				m_PlayerBirdGlidingSpeedMultiplier;
	bool				m_HasWalkStarts;
	bool				m_HasRunStarts;
	bool				m_HasQuickStops;
	bool				m_CanTrot;
	bool				m_CanCanter;
	bool				m_CanReverse;
	bool				m_HasDualGaits;
	bool				m_HasSlopeGaits;
	bool				m_HasCmplxTurnGaits;

	PAR_SIMPLE_PARSABLE;
};

class CMotionTaskDataSet
{
public:
	CMotionTaskDataSet() 
		: m_onFoot(NULL)
		, m_inWater(NULL)
	{}

	const rage::atHashString& GetName() const		{ return m_Name; }
	const sMotionTaskData* GetOnFootData() const	{ return m_onFoot; }
	const sMotionTaskData* GetInWaterData() const	{ return m_inWater; }
	bool HasLowLodMotionTask() const				{ return m_HasLowLodMotionTask; }

private:
	rage::atHashString	m_Name;
	sMotionTaskData*	m_onFoot;
	sMotionTaskData*	m_inWater;
	bool				m_HasLowLodMotionTask;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////
// CMotionTaskDataManager
/////////////////////////////////////

class CMotionTaskDataManager
{
public:
	// Constructor
	CMotionTaskDataManager();

	// Init
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Get the static manager
	static CMotionTaskDataManager& GetInstance() { return ms_MotionTaskDataManagerInstance; }

	// Find a data set
	static const CMotionTaskDataSet* const GetDataSet(const u32 dataSetHash);

#if __BANK
	// Add widgets
	static void InitWidgets();
#endif // __BANK

private:


#if __BANK
	// Save the data
	static void Save();
#endif // __BANK


	// Our array of motion task data sets
	rage::atArray<CMotionTaskDataSet*>	m_aMotionTaskData;

	// Our static manager/instance
	static CMotionTaskDataManager		ms_MotionTaskDataManagerInstance;

	PAR_SIMPLE_PARSABLE;
};

#endif //MOTION_TASK_DATA_H
