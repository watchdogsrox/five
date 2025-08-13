// Filename   :	GrabHelper.h
// Description:	Helper class for natural motion tasks which need grab behaviour.

#ifndef INC_GRABHELPER_H_
#define INC_GRABHELPER_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/string.h"
// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "system/FileMgr.h"
#include "Animation/AnimBones.h"
#include "scene/RegdRefTypes.h"

// This class is NOT a task, it is NOT derived from CTaskNMBehaviour
// it's just a helper that can manage the grab behavior, including auto scanning for grab points.
// It does have methods for handling messages, controlling and monitoring, and aborting
// that should be passed down / called from the owner task.
class CGrabHelper
{
public:
	FW_REGISTER_CLASS_POOL(CGrabHelper);

	struct Tunables : CTuning
	{
		Tunables();

		CNmTuningSetGroup m_Sets;

		PAR_PARSABLE;
	};

	enum eGrabFlags
	{
		// when or what to grab options
		TARGET_SPECIFIC				= BIT(0),
		TARGET_AUTO_WHEN_STANDING	= BIT(1),
		TARGET_AUTO_WHEN_FALLING	= BIT(2),
		TARGET_LINE_GRAB			= BIT(4),
		TARGET_SURFACE_GRAB			= BIT(5),
		TARGET_ABORT_GRABBING		= BIT(6),
		// *** remember to update NUM_GRAB_FLAG_OPTIONS if adding any more! ***

		NUM_GRAB_FLAG_OPTIONS		= 9,

		// grab runtime flags
		GRAB_TRYING					= BIT(9),
		GRAB_SUCCEEDED_LHAND		= BIT(10),
		GRAB_SUCCEEDED_RHAND		= BIT(11),
		GRAB_MISSED					= BIT(12),
		GRAB_FAILED					= BIT(13),

		// balance runtime flags
		BALANCE_STATUS_ACTIVE		= (1<<16),//BIT(16),
		BALANCE_STATUS_SUCCEEDED	= (2<<16),
		BALANCE_STATUS_FAILED		= (3<<16),
		BALANCE_STATUS_FALLING		= (4<<16),
		BALANCE_STATUS_LYING		= (5<<16),

		BALANCE_STATUS_BITS			= (BIT(16)|BIT(17)|BIT(18)|BIT(19))
	};

	CGrabHelper(const CGrabHelper* copyFrom);
	~CGrabHelper();

	static void AppendTuningSet(CNmTuningSet* tuningSet, atHashString key)
	{
		sm_Tunables.m_Sets.Append(tuningSet,key);
	}
	static void RevertTuningSet(atHashString key)
	{
		sm_Tunables.m_Sets.Revert(key);
	}
	// explicitly set the grab target (this will set target scan flags to TARGET_SPECIFIC)
	CEntity* GetGrabTargetEntity() const {return m_pGrabEnt; }
	void SetGrabTarget(CEntity* pGrabEnt, Vector3* pGrab1, Vector3* pNormal1, Vector3* pGrab2=NULL, Vector3* pNormal2=NULL, Vector3* pGrab3=NULL, Vector3* pGrab4=NULL);
	void SetGrabTarget_BoundIndex(int iIndex);
	void SetInitialTuningSet(atHashString setHash) { m_InitialTuningSet = setHash; }
	void SetBalanceFailedTuningSet(atHashString setHash) { m_BalanceFailedTuningSet = setHash; }
	
	void Abort(CPed* pPed);

	// Draws debug information including status and grab points etc...
#if !__FINAL
	void DrawDebug(void) const;
#endif

	// remember this class is NOT a task, it is NOT derived from CTaskNMBehaviour, it's just a helper that will manage the grab behaviour
	// these functions should be called from the functions of the same name in the owner task.
	void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	void BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

	bool   GetHasAborted() {return m_bHasAborted;}
	bool   GetHasFailed() {return m_bHasFailed;}
	bool   GetHasSucceeded() {return m_bHasSucceeded;}
	u32 GetFlags() const {return m_nFlags;}
	bool   GetFlag(u32 nFlag) const {return (m_nFlags & nFlag) != 0;}
	void   SetFlag(u32 nFlag) {m_nFlags |= nFlag;}
	void   SetStatusFlag(u32 nStatusFlag) {ClearFlag(BALANCE_STATUS_BITS); SetFlag(nStatusFlag); Assert(nStatusFlag>=BALANCE_STATUS_ACTIVE && nStatusFlag<=BALANCE_STATUS_LYING);}
	void   ClearFlag(u32 nFlag) {m_nFlags &= ~nFlag;}
	void   SetFlags(u32 nFlags) {m_nFlags = nFlags;}

	// If positive, counts down and aborts the grab after that time if unsuccessful
	void SetAbortGrabAttemptTime(float fTime) { m_fAbortGrabAttemptTimer = fTime;}

	void SetAbortGrabTime(float fTime) {m_fAbortGrabTimer = fTime;}

	bool IsHoldingOn() {return (GetFlag(GRAB_SUCCEEDED_LHAND) || GetFlag(GRAB_SUCCEEDED_RHAND));}

	// these should be called within the same functions of the owner task
	void StartBehaviour(CPed* pPed);
	void ControlBehaviour(CPed* pPed);
	bool FinishConditions(CPed* pPed);

private:
	Vector3 m_vecGrab1;
	Vector3 m_vecNorm1;
	Vector3 m_vecGrab2;
	Vector3 m_vecNorm2;
	Vector3 m_vecGrab3;
	Vector3 m_vecGrab4;

	u32 m_nFlags;
	u32 m_nStartGrabTime;
	RegdEnt m_pGrabEnt;
	int m_iBoundIndex;

	float m_fAbortGrabAttemptTimer; // If positive, counts down and aborts the grab after that time if unsuccessful
	float m_fAbortGrabTimer;

	bool m_bHasAborted:1;
	bool m_bHasFailed:1;
	bool m_bHasSucceeded:1;

	atHashString m_InitialTuningSet;
	atHashString m_BalanceFailedTuningSet;

	static Tunables sm_Tunables;

#if __DEV
	static bool ms_bDisplayDebug;
#endif

	// widget tunable
public:

	//typedef struct Parameters Parameters;
	static const u32 m_eType;
	//static Parameters ms_Parameters[1];
	static Vector3 NM_grab_up;

	static atHashString TUNING_SET_DEFAULT;
};

#endif // !INC_GRABHELPER_H_
