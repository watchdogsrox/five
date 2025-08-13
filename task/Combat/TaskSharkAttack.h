#ifndef TASK_SHARK_ATTACK_H
#define TASK_SHARK_ATTACK_H

// Game headers
#include "ai/AITarget.h"
#include "Network/General/NetworkUtil.h"
#include "Peds/ped.h"
#include "Peds/QueriableInterface.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"
#include "Task/System/Tuning.h"
#include "Task/System/TaskFSMClone.h"

// Rage headers
#include "fwanimation/animdefines.h"
#include "streaming/streamingmodule.h"

//*******************************************************
//Class:  CTaskSharkCircle
//Purpose:  Special move task used to circle the target.
//*******************************************************

class CTaskSharkCircle : public CTaskMoveBase
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:  How close in XY distance the shark can be to a local circle point to advance its position.
		float		m_AdvanceDistanceSquared;

		// PURPOSE:  Override the movement rate for the shark while circling.
		float		m_MoveRateOverride;

		PAR_PARSABLE;
	};

	CTaskSharkCircle(float fMoveBlendRatio, const CAITarget& rTarget, float fRadius, float fAngularSpeed);
	~CTaskSharkCircle() {};
	
	enum
	{
		State_Initial,
		State_Circle,
		State_Finish
	};

	// Task required functions
	virtual aiTask*				Copy() const				{ return rage_new CTaskSharkCircle(GetMoveBlendRatio(), m_Target, m_fRadius, m_fAngularSpeed); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_SHARK_CIRCLE; }

	// MoveBase required functions.
	virtual bool ProcessMove(CPed* pPed);
	virtual bool HasTarget() const							{ return m_Target.GetIsValid(); }
	virtual Vector3 GetTarget() const;
	virtual Vector3 GetLookAheadTarget() const;
	virtual void SetTarget( const CPed* pPed, Vec3V_In vTarget, const bool UNUSED_PARAM(bForce));

	// Task optional functions.
	virtual	void CleanUp();


#if !__FINAL
	virtual void Debug() const;
#endif
	
protected:
	virtual const Vector3 & GetCurrentTarget();

private:

	// Helper functions.

	// Return true if the local target point is close to the shark (only in the XY plane).
	bool IsCloseToCirclePoint() const;

	// Project a point to chase, which is m_fRadius away from the victim.
	void ProjectPoint(float fDegrees);

	// Set the initial point, which is m_fRadius away from the victim and aligned with the vector between the shark and victim.
	void ProjectInitialPoint();

	// Decide on clockwise or counterclockwise circling motion.
	void PickCirclingDirection();

private:

	// Member variables.

	// Who to circle.
	CAITarget					m_Target;
	
	// The locally projected point along the circle to chase.
	Vector3						m_vCurrentTarget;

	// Where along the circle the locally projected point is to be found.
	float						m_fAngleOffset;

	// How far away from the target to circle.
	float						m_fRadius;

	// How fast to circle.
	float						m_fAngularSpeed;

	// Whether or not the initial point has been projected.
	bool						m_bSetupPattern;

	// Which way to circle.
	bool						m_bCirclePositively;

	// Task tuning.

	static Tunables				sm_Tunables;

};


//******************************************
//Class:  CTaskSharkAttack
//Purpose:  Stalking/Combat task for sharks.
//******************************************

class CTaskSharkAttack : public CTaskFSMClone
{
public:
	
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:  When surfacing, this is how far to project ahead of the shark while it rises.
		float		m_SurfaceProjectionDistance;

		// PURPOSE:  When surfacing, this is how far below the water level at target's location to aim for.
		float		m_SurfaceZOffset;

		// PURPOSE:  When surfacing, this is the minimum buffer between us and the surface before starting to circle.
		float		m_MinDepthBelowSurface;

		// PURPOSE:  How quickly to rotate around the target (in degrees per second).
		float		m_CirclingAngularSpeed;

		// PURPOSE:  How long (in seconds) the shark should circle its prey.
		float		m_TimeToCircle;

		// PURPOSE:  How far away from the target to circle.
		float		m_MinCircleRadius;
		float		m_MaxCircleRadius;

		// PURPOSE:  Move blend ratio to use while the shark is circling its prey.
		float		m_CirclingMBR;

		// PURPOSE:  When diving, this is how far to project ahead of the shark while it submerges.
		float		m_DiveProjectionDistance;

		// PURPOSE:  When diving, this is how far to go below the shark's initial position.
		float		m_DiveDepth;

		// PURPOSE:  Move blend ratio to use while the shark is diving.
		float		m_DiveMBR;

		// PURPOSE:  The min number of fake approaches the shark will do.
		s32			m_MinNumberFakeApproaches;

		// PURPOSE:  The max number of fake approaches the shark will do.
		s32			m_MaxNumberFakeApproaches;

		// PURPOSE:  How far to go past the target when doing a fake lunge.
		float		m_FakeLungeOffset;

		// PURPOSE:  How far to be behind the target when setting the lunge location.
		float		m_LungeForwardOffset;

		// PURPOSE:  What to subtract from the target's Z position when setting the lunge location.
		float		m_LungeZOffset;

		// PURPOSE:  How far away is considered a large enough distance change for the target point to change when lunging.
		float		m_LungeChangeDistance;

		// PURPOSE:  How far away can the shark be from the victim to start playing the bite animation.
		float		m_LungeTargetRadius;

		// PURPOSE:  How long to follow before giving up.
		float		m_FollowTimeout;

		// PURPOSE:  How far behind the ped to follow towards.
		float		m_FollowYOffset;

		// PURPOSE:  How far below the ped to follow towards.
		float		m_FollowZOffset;

		// PURPOSE:  How far to check in front of the shark for land.
		float		m_LandProbeLength;

		// PURPOSE:  How fast is considered moving for vehicles.
		float		m_MovingVehicleVelocityThreshold;

		// PURPOSE:  How far away to get from the target when fleeing.
		float		m_SharkFleeDist;

		PAR_PARSABLE;
	};


	enum
	{
		State_Start,
		State_Surface,
		State_Follow,
		State_Circle,
		State_Dive,
		State_FakeLunge,
		State_Lunge,
		State_Bite,
		State_Flee,
		State_Finish
	};
	
public:

	CTaskSharkAttack(CPed* pTarget);
	~CTaskSharkAttack();

	// Task required functions
	virtual aiTask*				Copy() const		{ return rage_new CTaskSharkAttack(m_pTarget); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_SHARK_ATTACK; }

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM	();
	virtual void				CleanUp			();

	// Clone task implementation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	static const Tunables& GetTunables()	{	return sm_Tunables;	}

	// Static helper functions.
	// Return true if there are any sharks currently attacking the player.
	static bool SharksArePresent()			{ return sm_NumActiveSharks > 0; }

	// Cap the Z-position of a vector based on the water height.
	static void CapHeightForWater(Vec3V_InOut vPosition);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	// FSM State Functions
	FSM_Return					Start_OnUpdate();

	void						Surface_OnEnter();
	FSM_Return					Surface_OnUpdate();

	void						Follow_OnEnter();
	FSM_Return					Follow_OnUpdate();

	void						Circle_OnEnter();
	FSM_Return					Circle_OnUpdate();

	void						Dive_OnEnter();
	FSM_Return					Dive_OnUpdate();

	void						FakeLunge_OnEnter();
	FSM_Return					FakeLunge_OnUpdate();

	void						Lunge_OnEnter();
	FSM_Return					Lunge_OnUpdate();
	void						Lunge_OnExit();

	void						Bite_OnEnter();
	FSM_Return					Bite_OnUpdate();
	void						Bite_OnExit();

	void						Flee_OnEnter();
	FSM_Return					Flee_OnUpdate();

private:

	// Helper functions.
	bool						CanTargetBeLungedAt() const;
	bool						IsTargetUnderwater() const;
	bool						ImminentCollisionWithShore();
	bool						TargetInVehicle() const;
	bool						ChasingAStationaryVehicle() const;
	bool						ShouldDoAFakeLunge() const;
	bool						CanTryToKillTarget() const;

	// Find where the shark should lunge when going towards the target for the final attack.
	Vec3V_Out					GenerateLungeLocation() const;

	// Find where the shark should try to move to when following.
	Vec3V_Out					GenerateFollowLocation() const;

	void						CreateSyncedScene();
	void						CreateSyncedSceneCamera();

	// Possibly apply ped damage based on the phase.
	void						ProcessPedDamage();

	// Possibly cause pad rumble based on the phase.
	void						ProcessRumble();

private:

	// Member variables

	// PURPOSE:  Where the shark is lunging towards, updated if the position of the ped changes significantly.
	Vec3V						m_vCachedLungeLocation;

	// PURPOSE:  When the shark flees, this is where it should swim away from.
	Vector3						m_vFleePosition;

	// PURPOSE:  The victim of the shark attack.
	RegdPed						m_pTarget;

	// PURPOSE:  Counter used to control if the shark should stop circling.
	float						m_fCirclingCounter;

	// PURPOSE:  Play back the shark attack animation.
	fwSyncedSceneId				m_SyncedSceneID;

	// PURPOSE:  Whether or not the shark has done a fake pass, we only want to do this once.
	s32							m_iNumberOfFakeApproachesLeft;

	// PURPOSE:  Whether or not ped damage has been applied.
	bool						m_bHasAppliedPedDamage;

	// PURPOSE:  Whether the shark is fleeing the shore or not.
	bool						m_bIsFleeingShore;

	// PURPOSE:  Whether or not the active shark count needs to be decremented when the task ends.
	bool						m_bSharkCountNeedsCleanup;
	
public:

	// Constants

	static const strStreamingObjectName ms_CameraAnimation;
	static const strStreamingObjectName ms_SharkBiteDict;

private:

	static s32					sm_NumActiveSharks;

	// Task tuning

	static Tunables				sm_Tunables;
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskSharkAttackInfo 
// Purpose: Task info for shark attack task
//////////////////////////////////////////////////////////////////////////

class CClonedTaskSharkAttackInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedTaskSharkAttackInfo() {};
	CClonedTaskSharkAttackInfo(s32 iState, CPed* pTargetPed);
	~CClonedTaskSharkAttackInfo() {}

	virtual s32 GetTaskInfoType() const {return INFO_TYPE_SHARK_ATTACK;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskSharkAttack::State_Finish>::COUNT;}
	virtual CEntity* GetTarget() { return m_TargetPed.GetPed(); }

	void Serialise(CSyncDataBase& serialiser)
	{
		ObjectId sourceEntityID = m_TargetPed.GetPedID();
		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_OBJECTID(serialiser, sourceEntityID, "Shark attack target");
		m_TargetPed.SetPedID(sourceEntityID);
	}

private:

	CSyncedPed m_TargetPed;
};


#endif // TASK_SHARK_ATTACK_H
