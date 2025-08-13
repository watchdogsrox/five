// Filename   :	TaskNMBalance.h
// Description:	Natural Motion balance class (FSM version)
//
// (TODO RA: Give examples of where this task is used in the codebase.)

#ifndef INC_TASKNMBALANCE_H_
#define INC_TASKNMBALANCE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/GrabHelper.h"
#include "system/FileMgr.h"
#include "Animation/AnimBones.h"
#include "scene/RegdRefTypes.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMBalance
//
class CClonedNMBalanceInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMBalanceInfo(const CEntity* pLookAt, u32 nGrabFlags, const Vector3* pVecLeanDir, float fLeanAmount, u32 balanceType);
	CClonedNMBalanceInfo();
	~CClonedNMBalanceInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_BALANCE;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		bool bHasLookAt			= m_bHasLookAt;
		bool bHasGrabFlags		= m_nGrabFlags != 0;
		bool bHasLeanDir		= m_bHasLeanDir;
		bool bHasBalanceType	= m_bHasBalanceType;

		SERIALISE_BOOL(serialiser, bHasLookAt, "Has look at");

		if (bHasLookAt || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_OBJECTID(serialiser, m_lookAtEntity, "Look at entity");
		}

		SERIALISE_BOOL(serialiser, bHasGrabFlags);

		if (bHasGrabFlags || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 grabFlags = (u32)m_nGrabFlags;
			SERIALISE_UNSIGNED(serialiser, grabFlags, SIZEOF_GRAB_FLAGS, "Grab flags");
			m_nGrabFlags = (u16) grabFlags;
		}
		else
		{
			m_nGrabFlags = 0;
		}

		SERIALISE_BOOL(serialiser, bHasLeanDir);

		if (bHasLeanDir || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_VECTOR(serialiser, m_vecLeanDir, 1.0f, SIZEOF_LEAN_DIR, "Lean direction");

			u32 leanAmount = (u32)m_leanAmount;
			SERIALISE_UNSIGNED(serialiser, leanAmount, SIZEOF_LEAN_AMOUNT, "Lean amount");
			m_leanAmount = (u8)leanAmount;

			m_vecLeanDir.Normalize(); 
		}

		SERIALISE_BOOL(serialiser, bHasBalanceType);

		if (bHasBalanceType || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 balanceType = (u32)m_balanceType;
			SERIALISE_UNSIGNED(serialiser, balanceType, SIZEOF_BALANCE_TYPE, "Balance type");
			m_balanceType = (u8)balanceType;
		}

		m_bHasLookAt		= bHasLookAt;
		m_bHasLeanDir		= bHasLeanDir;
		m_bHasBalanceType	= bHasBalanceType;
	}

private:

	CClonedNMBalanceInfo(const CClonedNMBalanceInfo &);
	CClonedNMBalanceInfo &operator=(const CClonedNMBalanceInfo &);

	static const unsigned int SIZEOF_LEAN_DIR		= 16;
	static const unsigned int SIZEOF_LEAN_AMOUNT	= 8;
	static const unsigned int SIZEOF_BALANCE_TYPE	= 3;
	static const unsigned int SIZEOF_GRAB_FLAGS		= CGrabHelper::NUM_GRAB_FLAG_OPTIONS;

	Vector3		m_vecLeanDir; 
	ObjectId	m_lookAtEntity;
	u16		    m_nGrabFlags; 
	u8			m_leanAmount; 
	u8			m_balanceType;
	bool		m_bHasLookAt : 1;
	bool		m_bHasLeanDir : 1;
	bool		m_bHasBalanceType : 1;
};

// balance (including grab/brace while balancing)
// triggers:	bumped by player ped
//				standing on moving vehicle
//				any grab while standing
class CTaskNMBalance : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		RagdollComponent m_InitialBumpComponent;
		CTaskNMBehaviour::Tunables::TunableForce m_InitialBumpForce;
		Vector3			 m_InitialBumpOffset;

		bool m_ScaleStayUprightWithVel;
		float m_StayUprightAtMaxVel;
		float m_MaxVel;
		float m_StayUprightAtMinVel;
		float m_MinVel;

		float m_lookAtVelProbIfNoBumpTarget;
		float m_fMaxTargetDistToUpdateFlinch;
		float m_fMaxTargetDistToUpdateFlinchOnGround;
		float m_fFlinchTargetZOffset;
		float m_fFlinchTargetZOffsetOnGround;
		float m_fMinForwardVectorToFlinch;
		float m_fMinForwardVectorToFlinchOnGround;
		float m_fHeadLookZOffset;
		float m_fHeadLookZOffsetOnGround;
		int m_MaxSteps;
		int m_timeToCatchfallMS;

		CNmTuningSet m_StartWeak;
		CNmTuningSet m_StartAggressive;
		CNmTuningSet m_StartDefault;

		CNmTuningSet m_FallOffAMovingCar;
		CNmTuningSet m_BumpedByPed;
		CNmTuningSet m_OnStairs;
		CNmTuningSet m_OnSteepSlope;
		CNmTuningSet m_OnMovingGround;

		CNmTuningSet m_LostBalanceAndGrabbing;
		CNmTuningSet m_Teeter;
		CNmTuningSet m_RollingFall;
		CNmTuningSet m_CatchFall;

		CNmTuningSet m_OnBalanceFailed;

		u32 m_NotBeingPushedDelayMS;
		u32 m_NotBeingPushedOnGroundDelayMS;
		u32 m_BeingPushedOnGroundTooLongMS;
		CNmTuningSet m_OnBeingPushed;
		CNmTuningSet m_OnBeingPushedOnGround;
		CNmTuningSet m_OnNotBeingPushed;
		CNmTuningSet m_OnBeingPushedOnGroundTooLong;

		CTaskNMBehaviour::Tunables::BlendOutThreshold m_PushedThresholdOnGround;
		
		PAR_PARSABLE;
	};

	enum eBalanceType {
		BALANCE_DEFAULT,
		BALANCE_WEAK,
		BALANCE_AGGRESSIVE,
		BALANCE_FORCE_FALL_FROM_MOVING_CAR,
		NUM_BALANCE_TYPES
	};

	CTaskNMBalance(u32 nMinTime, u32 nMaxTime, CEntity* pLookAt, u32 nGrabFlags, const Vector3* pVecLeanDir=NULL, float fLeanAmount=0.0f, const CGrabHelper* pGrabHelper=NULL, eBalanceType eType = BALANCE_DEFAULT);
	~CTaskNMBalance();

	void SetType(eBalanceType eType) { m_eType = eType; };
	eBalanceType GetType() const { return m_eType; }
	eBalanceType EvaluateAndSetType(const CPed* const pPed);

	void SetLookAtPosition(const Vector3& vLookAt);

	bool HasBalanceFailed() const { return m_bBalanceFailed; }

	virtual bool HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId);

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}

	virtual void Debug() const;
#endif
	virtual aiTask* Copy() const;
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_BALANCE;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMBalanceInfo(m_pLookAt.Get(), GetGrabHelper() ? GetGrabHelper()->GetFlags() : 0, &m_vecLean, m_fLeanAmount, static_cast<u32>(m_eType));
	}

	///////////////////////////
	// CTaskNMBehaviour functions
public:
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourStart(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	//////////////////////////
	// Local functions and data
public:
	void SetAbortLeanTime(float fTime) { m_fAbortLeanTimer = fTime;}
	CGrabHelper* GetGrabHelper() const {return m_pGrabHelper;}
	static bool ProbeForSignificantDrop(CPed* pPed, Vector3& vEdgeLeft, Vector3& vEdgeRight, bool onMovingGround, bool &firstTeeterProbeDetectedDrop,
		WorldProbe::CShapeTestResults &probeResult);

	CEntity* GetLookAtEntity() { return m_pLookAt; }

private:
	enum eStates // for internal state machine.
	{
		STANDING,
		STAGGERING,
		TEETERING,
		FALLING,
		ROLLING_FALL,
		ON_GROUND
	};

	u32 GetConfigureBalanceEnum(eBalanceType state);
	u32 GetPedalEnum(eBalanceType state);
	void ProbeForStairsOrSlope(CPed* pPed, bool &onStairs, bool &onSlope);

private:

	WorldProbe::CShapeTestResults m_probeResult;

	// Vectors first
	Vector3 m_vecLean;
	Vector3 m_vLookAt;
	// Used to define an edge for the teeter message.
	Vector3 m_vEdgeLeft, m_vEdgeRight;

	eStates m_eState;
	RegdEnt m_pLookAt;
	float m_fLeanAmount;
	float m_fAbortLeanTimer;
	CGrabHelper* m_pGrabHelper;
	eBalanceType m_eType;
	u32 m_rollStartTime;
	bool m_FirstTeeterProbeDetectedDrop;

	u32 m_PushStartTime;

	// Flags
	bool m_bBalanceFailed : 1;
	bool m_bRollingDownStairs : 1;
	bool m_bCatchFallInitd : 1;
	bool m_bRollingFallInitd : 1;
	bool m_bPedalling : 1;
	bool m_bLookAtPosition : 1;
	bool m_bTeeterInitd : 1;
	bool m_bBeingPushed : 1;
	bool m_bPushedTooLong : 1;

public:

	static const char* ms_TypeToString[NUM_BALANCE_TYPES];

	static Tunables	sm_Tunables;

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL
};

#endif // ! INC_TASKNMBALANCE_H_
