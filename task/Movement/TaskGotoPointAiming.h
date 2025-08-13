#ifndef TASK_GOTO_POINT_AIMING_H
#define TASK_GOTO_POINT_AIMING_H

// Game headers
#include "Peds/PedDefines.h"
#include "Peds/QueriableInterface.h"
#include "AI/AITarget.h"
#include "Network/General/NetworkUtil.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"

class CTaskGoToPointAiming : public CTaskFSMClone
{
public:

	enum ConfigFlags
	{
		CF_AimAtDestinationIfTargetLosBlocked = BIT0,
		CF_DontAimIfSprinting = BIT1,
		CF_DontAimIfTargetLosBlocked = BIT2,

		CF_NUM_FLAGS = 3
	};

	CTaskGoToPointAiming(const CAITarget& goToTarget, const CAITarget& aimAtTarget, const float fMoveBlendRatio, bool bSkipIdleTransition = false, const int iTime = -1);
	CTaskGoToPointAiming(const CTaskGoToPointAiming& other);
	virtual ~CTaskGoToPointAiming();

	virtual	void CleanUp();

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskGoToPointAiming(*this); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_GO_TO_POINT_AIMING; }

	// clone task implementation
	virtual CTaskInfo*		CreateQueriableState() const;
	virtual CTaskFSMClone*	CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*	CreateTaskForLocalPed(CPed* pPed);
	virtual FSM_Return		UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

	//
	// Settors
	//

	// Set the distance to our target that will end the task
	void SetTargetRadius(float fTargetRadius) { m_fTargetRadius = fTargetRadius; }

	// Set the distance to our target we will start slowing down at
	void SetSlowDownDistance(float fSlowDownDistance) { m_fSlowDownDistance = fSlowDownDistance; }

	// Set the firing pattern to use while aiming
	void SetFiringPattern(u32 uFiringPatternHash) { m_uFiringPatternHash = uFiringPatternHash; }

	// Set whether we should be using the navmesh for movement
	void SetUseNavmesh(bool bUseNavmesh) { m_bUseNavmesh = bUseNavmesh; }

	// Set flags passed in by script to control navigation behaviour
	void SetScriptNavFlags(u32 iFlags) { m_iScriptNavFlags = iFlags; }

	// Set flags passed in by script to control navigation behaviour
	void SetConfigFlags(u32 iFlags) { m_iConfigFlags.SetAllFlags(m_iConfigFlags.GetAllFlags()|(u8)iFlags); }

	// Sets whether or not we should be aiming at entities near the aim coord
	void SetAimAtHatedEntitiesNearAimCoord(bool bAimAtHatedNearAimCoord) { m_bAimAtHatedEntitiesNearAimCoord = bAimAtHatedNearAimCoord; }
	
	void SetInstantBlendToAim(bool bInstantBlendToAim) {m_bInstantBlendToAim = bInstantBlendToAim;}

	void SetMoveBlendRatio(float fMoveBlendRatio) { m_fMoveBlendRatio = fMoveBlendRatio; }

	//
	// Gettors
	//
	const CAITarget& GetGotoTarget() const { return m_goToTarget; }
	const CAITarget& GetAimAtTarget() const { return m_aimAtTarget; }

	float GetMoveBlendRatio() const { return m_fMoveBlendRatio; }

	static bool SpheresOfInfluenceBuilder( TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed );

#if !__FINAL
	// Debug rendering
	virtual void Debug() const;
#endif // !__FINAL

protected:
	
	enum State
	{
		State_Start = 0,
		State_GoToPointAiming,
		State_Quit,
	};

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Start; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	bool CanSetDesiredHeading() const;

private:

	//
	// Local State functions
	//

	FSM_Return StateStart_OnUpdate();
	FSM_Return StateGoToPointAiming_OnEnter(CPed* pPed);
	FSM_Return StateGoToPointAiming_OnUpdate(CPed* pPed);

	//
	// Members
	//

	CWeaponController::WeaponControllerType GetDesiredWeaponControllerType();
	CTaskGun* CreateGunTask();

	// Our target point we are moving towards
	CAITarget m_goToTarget;

	// Our target point we are aiming at
	CAITarget m_aimAtTarget;

	// Our original target to aim at (can be overridden with specific task types)
	CAITarget m_originalAimAtTarget;

	// Our move blend ratio - walk/run etc.
	float m_fMoveBlendRatio;

	// Target radius - if we get this close to the target we are done
	float m_fTargetRadius;

	// Slow down distance - we will start slowing down when we are this close to the target
	float m_fSlowDownDistance;

	// The firing pattern to use while aiming
	u32 m_uFiringPatternHash;

	// Perform a navmesh search
	bool m_bUseNavmesh;

	// Scrip flags to control navigation behaviour
	u32 m_iScriptNavFlags;

	int m_iTime;

	// Scrip flags to control general behaviour
	fwFlags8 m_iConfigFlags;

	// Skip the idle transition
	bool m_bSkipIdleTransition;

	// Were you crouched
	bool m_bWasCrouched;

	// Are we supposed to be aiming at hated entities near our target coord?
	bool m_bAimAtHatedEntitiesNearAimCoord;

	// Did our target change this frame?
	bool m_bTargetChangedThisFrame;

	// Instant blend to aim.
	bool m_bInstantBlendToAim;
};

//////////////////////////////////////////////////////////////////////////
// CTaskGoToPointAiming
//////////////////////////////////////////////////////////////////////////
class CClonedGoToPointAimingInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedGoToPointAimingInfo();
	CClonedGoToPointAimingInfo(const CAITarget& gotoTarget, 
		const CAITarget& aimTarget, 
		const float fMoveBlendRatio,
		const float fTargetRadius,
		const float fSlowDownDistance,
		const bool bIsShooting,
		const bool bUseNavmesh,
		const u32 iScriptNavFlags,
		const u32 iConfigFlags,
		const bool bAimAtHatedEntitiesNearAimCoord,
		const bool bInstantBlendToAim);

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);
	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_GO_TO_POINT_AIMING; }
	virtual int GetScriptedTaskType() const;

	void Serialise(CSyncDataBase& serialiser)
	{
		const unsigned SIZEOF_MOVE_BLEND_RATIO = 8;
		const unsigned SIZEOF_TARGET_RADIUS = 8;
		const unsigned SIZEOF_SLOW_DOWN_DISTANCE = 8;
		const float MAX_TARGET_RADIUS = 2.0f;
		const float MAX_SLOW_DOWN_DISTANCE = 6.0f;

		// only send required info for the goto target type
		bool hasGotoEntity = m_bHasGotoEntity;
		SERIALISE_BOOL(serialiser, hasGotoEntity, "Has Goto Entity");
		m_bHasGotoEntity = hasGotoEntity;
		if(m_bHasGotoEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			ObjectId targetID = m_pGotoEntity.GetEntityID();
			SERIALISE_OBJECTID(serialiser, targetID, "Goto Entity");
			m_pGotoEntity.SetEntityID(targetID);
		}

		bool hasGotoPosition = m_bHasGotoPosition;
		SERIALISE_BOOL(serialiser, hasGotoPosition, "Has Goto Position");
		m_bHasGotoPosition = hasGotoPosition;
		if(m_bHasGotoPosition || serialiser.GetIsMaximumSizeSerialiser())
			SERIALISE_POSITION(serialiser, m_vGotoPosition, "Goto Position");

		// only send required info for the Aim target type
		bool hasAimEntity = m_bHasAimEntity;
		SERIALISE_BOOL(serialiser, hasAimEntity, "Has Aim Entity");
		m_bHasAimEntity = hasAimEntity;
		if(m_bHasAimEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			ObjectId targetID = m_pAimEntity.GetEntityID();
			SERIALISE_OBJECTID(serialiser, targetID, "Aim Entity");
			m_pAimEntity.SetEntityID(targetID);
		}

		bool hasAimPosition = m_bHasAimPosition;
		SERIALISE_BOOL(serialiser, hasAimPosition, "Has Aim Position");
		m_bHasAimPosition = hasAimPosition;
		if(m_bHasAimPosition || serialiser.GetIsMaximumSizeSerialiser())
			SERIALISE_POSITION(serialiser, m_vAimPosition, "Aim Position");

		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fMoveBlendRatio, MOVEBLENDRATIO_SPRINT, SIZEOF_MOVE_BLEND_RATIO, "Move Blend Ratio");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fTargetRadius, MAX_TARGET_RADIUS, SIZEOF_TARGET_RADIUS, "Target Radius");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fSlowDownDistance, MAX_SLOW_DOWN_DISTANCE, SIZEOF_SLOW_DOWN_DISTANCE, "Slow Down Distance");

		bool isShooting = m_bIsShooting;
		SERIALISE_BOOL(serialiser, isShooting, "Is Shooting");
		m_bIsShooting = isShooting;

		bool useNavMesh = m_bUseNavmesh;
		SERIALISE_BOOL(serialiser, useNavMesh, "Uses NavMesh");
		m_bUseNavmesh = useNavMesh;

		u16 scriptNavFlags = m_iScriptNavFlags;
		SERIALISE_UNSIGNED(serialiser, scriptNavFlags, ENAV_NUM_FLAGS, "NavMesh Flags");
		m_iScriptNavFlags = scriptNavFlags;

		u8 configFlags = m_iConfigFlags;
		SERIALISE_UNSIGNED(serialiser, configFlags, CTaskGoToPointAiming::CF_NUM_FLAGS, "Task Config Flags");
		m_iConfigFlags = configFlags;

		bool aimAtHatedEntitiesNearAimCoord = m_bAimAtHatedEntitiesNearAimCoord;
		SERIALISE_BOOL(serialiser, aimAtHatedEntitiesNearAimCoord, "Aim At Hated Enemies");
		m_bAimAtHatedEntitiesNearAimCoord = aimAtHatedEntitiesNearAimCoord;

		bool instantBlendToAim = m_bInstantBlendToAim;
		SERIALISE_BOOL(serialiser, instantBlendToAim, "Instant Blend To Aim");
		m_bInstantBlendToAim = instantBlendToAim;
	}

private:

	Vector3 m_vAimPosition;
	Vector3 m_vGotoPosition;
	CSyncedEntity m_pGotoEntity;
	CSyncedEntity m_pAimEntity;
	float m_fMoveBlendRatio;
	float m_fTargetRadius;
	float m_fSlowDownDistance;
	u32 m_bHasGotoEntity :1;
	u32 m_bHasGotoPosition:1;
	u32 m_bHasAimEntity:1;
	u32 m_bHasAimPosition:1;
	u32 m_bIsShooting:1;
	u32 m_bUseNavmesh:1;
	u32 m_bAimAtHatedEntitiesNearAimCoord:1;
	u32 m_bInstantBlendToAim:1;
	u32  m_iScriptNavFlags:16;
	u32  m_iConfigFlags:8;
};

#endif // TASK_GOTO_POINT_AIMING_H
