#pragma once
// game headers
#include "ai\debug\system\AIDebugInfo.h"
#include "Vehicles\Metadata\VehicleEnterExitFlags.h"

#if AI_DEBUG_OUTPUT_ENABLED
#include "scene\RegdRefTypes.h"

class CPed;
class CVehicle;
typedef CVehicleEnterExitFlags::FlagsBitSet VehicleEnterExitFlags;

//base class for Vehicle Debug display and logging
class CVehicleDebugInfo : public CAIDebugInfo
{
public:
	CVehicleDebugInfo(const CVehicle* pVeh, DebugInfoPrintParams rPrintParams, s32 iNumberOfLines);
	virtual ~CVehicleDebugInfo() {};

	virtual void Print() { PrintRenderedSelection(true); }
	virtual void Visualise() { PrintRenderedSelection(false); }
	virtual void PrintRenderedSelection(bool bPrintAll = false) = 0;

	void SetVehicle(const CVehicle* pVeh) { m_Vehicle = pVeh; }
	void LoadNextVehicle(const CVehicle* pVeh, Vector3 vPos, s32 iLine) { m_Vehicle = pVeh; m_PrintParams.WorldCoords = vPos; m_NumberOfLines = iLine; }

protected:
	RegdConstVeh m_Vehicle;
};


//singleton class to hide all vehicle debug display
class CVehicleDebugVisualiser
{
public:

	enum eVehicleDebugVisMode
	{
		eOff,
		eTasks,
		eStatus,
		eTaskHistory,
		eVehicleFlags,
		eDestinations,
		eFutureNodes,
		eFollowRoute,
		eControlInfo,
		eLowlevelControl,
		eTransmission,
		eStuck,
		eNumModes
	};

	void Visualise(const CVehicle* pVeh);
	static CVehicleDebugVisualiser* GetInstance() { if(!sm_pInstance) sm_pInstance = rage_new CVehicleDebugVisualiser(); return sm_pInstance; }
	static eVehicleDebugVisMode			eVehicleDisplayDebugInfo;
	static bool							bFlagState;
	static void Process();

private:
	CVehicleDebugVisualiser();
	~CVehicleDebugVisualiser();
	static bool& GetDebugFlagFromEnum(eVehicleDebugVisMode mode);

protected:
	atArray<CVehicleDebugInfo*> debugInfos;
	static CVehicleDebugVisualiser* sm_pInstance;
};


class CVehicleStatusDebugInfo : public CVehicleDebugInfo
{
public:
	CVehicleStatusDebugInfo(const CVehicle* pVeh = 0, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams(), s32 iNumberOfLines = 0);
	virtual ~CVehicleStatusDebugInfo() {};

	virtual void PrintRenderedSelection(bool bPrintAll = false);
};

class CVehicleLockDebugInfo : public CAIDebugInfo
{
public:

	static const CVehicle* GetNearestVehicleToVisualise(const CPed* pPed);

	CVehicleLockDebugInfo(const CPed* pPed, const CVehicle* pVeh, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CVehicleLockDebugInfo() {};

	virtual void Print();
	virtual void Visualise();

	void SetVehicle(const CVehicle* pVeh) { m_Veh = pVeh; }

private:

	bool ValidateInput();
	void PrintPersonalVehicleDebug();
	void PrintDoorDebug();
	void PrintPartialDoorLockDebug();
	void PrintVehicleLockDebug();
	void PrintVehicleWeaponDebug();
	void PrintVehicleEntryConfigDebug();
	const char* GetPersonalVehicleOwnerName(CVehicle* pNonConstVeh, bool& bIsMyVehicle);

	RegdConstPed m_Ped;
	RegdConstVeh m_Veh;

};

class CEntryPointEarlyRejectionEvent : CAIDebugInfo
{
public:

	CEntryPointEarlyRejectionEvent() {};
	CEntryPointEarlyRejectionEvent(s32 iRejectedEntryPointIndex, const char* szRejectionReason, ...);
	virtual ~CEntryPointEarlyRejectionEvent() {}

	virtual void Print();
	virtual void Visualise() {};

private:

	s32 m_RejectedEntryPointIndex;
	char m_szRejectionReason[256];
};

class CEntryPointScoringEvent : CAIDebugInfo
{
public:

	CEntryPointScoringEvent() {};
	CEntryPointScoringEvent(s32 iEntryPointIndex, s32 iEntryPointPriority, float fScore, const char* szUpdateReason, ...);
	virtual ~CEntryPointScoringEvent() {}

	virtual void Print();
	virtual void Visualise() {};

private:

	s32 m_EntryPointIndex;
	s32 m_EntryPointPriority;
	float m_Score;
	char m_szUpdateReason[256];
};

class CEntryPointPriorityScoringEvent : CAIDebugInfo
{
public:

	CEntryPointPriorityScoringEvent() {}
	CEntryPointPriorityScoringEvent(s32 iScoredEntryPointIndex, s32 iNewPriorityScore, const char* szUpdateReason, ...);

	virtual void Print();
	virtual void Visualise() {};

private:

	s32 m_ScoredEntryPointPriority;
	s32 m_NewPriorityScore;
	char m_szUpdateReason[256];
};

class CRidableEntryPointEvaluationDebugInfo : public CAIDebugInfo
{
public:

	CRidableEntryPointEvaluationDebugInfo(DebugInfoPrintParams printParams = DebugInfoPrintParams());
	virtual ~CRidableEntryPointEvaluationDebugInfo() {};

	virtual void Print();
	virtual void Visualise() {};

	void Reset();
	void Init(const CPed* pEvaluatingPed, const CDynamicEntity* pRidableEnt, s32 iSeatRequest, s32 iSeatRequested, bool bWarping, VehicleEnterExitFlags iFlags);
	void AddEarlyRejectionEvent(const CEntryPointEarlyRejectionEvent& rEvent) { if (m_HasBeenInitialised) m_RejectionEvents.PushAndGrow(rEvent); }
	void AddScoringEvent(const CEntryPointScoringEvent& rEvent) { if (m_HasBeenInitialised) m_ScoringEvents.PushAndGrow(rEvent); }
	void AddPriorityScoringEvent(const CEntryPointPriorityScoringEvent& rEvent) { if (m_HasBeenInitialised) m_PriorityScoringEvents.PushAndGrow(rEvent); }
	void SetChosenEntryPointAndSeat(s32 iChosenEntryPointIndex, s32 iChosenSeatIndex) { m_ChosenEntryPointIndex = iChosenEntryPointIndex; m_ChosenSeatIndex = iChosenSeatIndex; }

private:

	bool ValidateInput();
	void PrintPedsInSeats();
	void PrintVehicleEntryExitFlags();
	void PrintEarlyRejectionEvents();
	void PrintScoringEvents();
	void PrintPriorityScoringEvents();

	bool m_HasBeenInitialised;
	bool m_Warping;
	VehicleEnterExitFlags m_VehicleEnterExitFlags;
	RegdConstPed m_EvaluatingPed;
	RegdConstDyn m_Ridable;
	atArray<CEntryPointEarlyRejectionEvent> m_RejectionEvents;
	atArray<CEntryPointScoringEvent> m_ScoringEvents;
	atArray<CEntryPointPriorityScoringEvent> m_PriorityScoringEvents;
	s32 m_SeatRequestType;
	s32 m_SeatRequested;
	s32 m_ChosenEntryPointIndex;
	s32 m_ChosenSeatIndex;

};

#endif // AI_DEBUG_OUTPUT_ENABLED