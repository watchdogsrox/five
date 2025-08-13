// Filename   :	TaskAnimatedAttach.h
// Description:	Handle attaching a ped to a CPhysical with anywhere from one limb to all four. This task
//              manages the constraints and, once attached, starts an NM task to control the ragdoll
//              beyond the simple clip.

#ifndef INC_TASKANIMATEDATTACH_H_
#define INC_TASKANIMATEDATTACH_H_

// --- Include Files ------------------------------------------------------------

#include "Network/General/NetworkUtil.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"

// ------------------------------------------------------------------------------

class CTaskAnimatedAttach : public CTaskFSMClone
{
public:

	// A structure to hold information about the attachments required between ragdoll and the attach parent.
	struct sAttachedNMComponents
	{
		Vector3 vPos;
		s32 iRagdollComponent;
		s32 iPedBoneIndex;
	};

	//A mask is set through script so these can be checked later
	enum eNMConstraintFlags
	{
		NMCF_LEFT_HAND		= BIT(0),
		NMCF_RIGHT_HAND		= BIT(1),
		NMCF_LEFT_FOOT		= BIT(2),
		NMCF_RIGHT_FOOT		= BIT(3),

		NMCF_LEFT_FOREARM	= BIT(4),
		NMCF_RIGHT_FOREARM	= BIT(5),
		NMCF_LEFT_SHIN		= BIT(6),
		NMCF_RIGHT_SHIN		= BIT(7),

		NMCF_LEFT_THIGH		= BIT(8),
		NMCF_RIGHT_THIGH	= BIT(9),

		NMCF_HANDS_AND_FEET = NMCF_LEFT_HAND | NMCF_RIGHT_HAND | NMCF_LEFT_FOOT | NMCF_RIGHT_FOOT,
		NMCF_ALL = NMCF_HANDS_AND_FEET | NMCF_LEFT_FOREARM | NMCF_RIGHT_FOREARM	| NMCF_LEFT_SHIN | NMCF_RIGHT_SHIN | NMCF_LEFT_THIGH | NMCF_RIGHT_THIGH
	};

	enum
	{
		State_Start = 0,
		State_StreamAssets,
		State_ClipatingApproach, 
		State_PosingPed,
		State_AttachingPed,
		State_PedAttached,
//		State_ReattachingPed,
		State_Finish
	};

	CTaskAnimatedAttach(CPhysical* pParent, s32 nParentBoneIndex, const Vector3 &vOffset, const Quaternion& RotationQuaternion, u32 iAttachFlags, s32 ClipDict,
		u32 ApprochClip, u32 IdleClip , bool bStreamAssets = false);
	CTaskAnimatedAttach(CPhysical* pParent, s32 nParentBoneIndex, u32 iAttachFlags, s32 ClipDict, u32 IdleClip);
	~CTaskAnimatedAttach();

	virtual aiTask* Copy() const;
	
	virtual	void CleanUp();

	void SetAttachToVehicleDoor(bool bAttachToVehicleDoor) { m_bAttachToVehicleDoor = bAttachToVehicleDoor; }

	// Clone task implementation 
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool				IsInScope(const CPed* pPed);
	virtual bool                OverridesNetworkAttachment(CPed *UNUSED_PARAM(pPed)) { return true; }
	CTaskFSMClone*				CreateTaskForClonePed(CPed *pPed);
	CTaskFSMClone*				CreateTaskForLocalPed(CPed *pPed);
	void						SetParentNetObjId(ObjectId objId) { m_parentNetObjId = objId; }
	////////////////////////////
	// CTaskFSM functions:
protected:
	//FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32 GetDefaultStateAfterAbort() const {return State_Finish;}
	int GetTaskTypeInternal() const {return CTaskTypes::TASK_ANIMATED_ATTACH;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Start&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_StreamAssets",
			"State_AnimatingApproach",
			"State_PosingPed",
			"State_AttachingPed",
			"State_PedAttached",
//			"State_ReattachingPed",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

	// Called after the skeleton update but before the Ik. Global matrices are valid in this function.
	//virtual void ProcessPreRender2(CPed *);

#if DEBUG_DRAW
public:
	virtual void Debug() const;
	void DebugVisualise(CPed *pPed);
#endif // DEBUG_DRAW

private:
	// Helper functions for FSM state management:
	void       Start_OnEnter(CPed* pPed);
	FSM_Return Start_OnUpdate(CPed* pPed);
	FSM_Return StreamAssets_OnUpdate(CPed* pPed);
	void       PosingPed_OnEnter(CPed* pPed);
	FSM_Return PosingPed_OnUpdate(CPed* pPed);
	void       PosingPed_OnExit(CPed* pPed);
	void       AttachingPed_OnEnter(CPed* pPed);
	FSM_Return AttachingPed_OnUpdate(CPed* pPed);
	void       AttachingPed_OnExit(CPed* pPed);
	void       PedAttached_OnEnter(CPed* pPed);
	FSM_Return PedAttached_OnUpdate(CPed* pPed);
	void       PedAttached_OnExit(CPed* pPed);

	void ChooseNextIdleClip(fwMvClipSetId &clipSetId, fwMvClipId &clipId);

	bool IsAttachFlagSet(const int iFlag) const {return ((m_nAttachFlags & iFlag) ? true : false);}
	void CalculateAttachPoints(atArray<sAttachedNMComponents> &Attach, CPed * pPed); 
	void DetachPed(CPed* pPed, bool bIgnoreAttachState = false);
	void ConfigureRagdollCollision(CPed* pPed, bool bCollisionEnabled);

	bool HandlesRagdoll(const CPed* pPed) const 
	{
		if (GetState()==State_AttachingPed)
			return true;
		else
			return CTask::HandlesRagdoll(pPed);
	}

private:
	// Data passed from script:
	u32 m_nAttachFlags; // Flags to denote which parts of the ragdoll are attached.
	Vector3 m_vAttachOffset;
	Quaternion m_RotationQuaternion;	// This rotation is never actually used
	s32 m_nClipDict;
	u32 m_nClip;
	u32 m_nIdleClip;
	bool m_bStreamAssets;
	bool m_bPosedFromIdle;
	bool m_bAttachToVehicleDoor;

	fwClipSetRequestHelper m_ClipSetRequestHelper;

	RegdPhy m_pAttachParent;
	bool m_bPedAttached;
	bool m_bRunShootingTask;
	u32 m_nStartTime;
	static u32 m_nFiringDelay;
	static Vector3 ms_vecTarget;
	s32 m_nBoneIndex;

	// The currently playing idle clip:
	fwMvClipSetId m_clipSetId;
	fwMvClipId m_clipId;

	// clone task members
	ObjectId m_parentNetObjId;
	bool m_bQuitCloneTask;

#if __BANK
	public:
#endif // __BANK
	static float ms_fInvMassMult;
	static bool m_bEnableDebugDraw;
};

//-------------------------------------------------------------------------
// Task info for animated attach
//-------------------------------------------------------------------------
class CClonedAnimatedAttachInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedAnimatedAttachInfo() :
	m_nParentBoneIndex(0),
	m_iAttachFlags(0),
	m_ClipDict(-1),
	m_IdleClip(0)
	{
	}

	CClonedAnimatedAttachInfo(CPhysical* pParent, s32 nParentBoneIndex, u32 iAttachFlags, s32 ClipDict, u32 IdleClip ) :
	m_parentEntity(pParent),
	m_nParentBoneIndex(static_cast<u32>(nParentBoneIndex)),
	m_iAttachFlags(iAttachFlags),
	m_ClipDict(ClipDict),
	m_IdleClip(IdleClip)
	{
	}

	~CClonedAnimatedAttachInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_ANIMATED_ATTACH;}

	virtual CTaskFSMClone *CreateCloneFSMTask()
	{
		CTaskAnimatedAttach* pTask = NULL;

		if (m_parentEntity.GetEntity())
		{
			pTask = rage_new CTaskAnimatedAttach(SafeCast(CPhysical, m_parentEntity.GetEntity()), m_nParentBoneIndex, m_iAttachFlags, m_ClipDict, m_IdleClip);
		}
		else
		{
			pTask = rage_new CTaskAnimatedAttach(NULL, m_nParentBoneIndex, m_iAttachFlags, m_ClipDict, m_IdleClip);
			pTask->SetParentNetObjId(m_parentEntity.GetEntityID());
		}

		return pTask;
	}

	virtual bool HasState() const { return false; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		ObjectId entityID = m_parentEntity.GetEntityID();
		SERIALISE_OBJECTID(serialiser, entityID, "Parent Entity");
		m_parentEntity.SetEntityID(entityID);

		u32 nParentBoneIndex = m_nParentBoneIndex;
		SERIALISE_UNSIGNED(serialiser, nParentBoneIndex, SIZEOF_BONE_INDEX,	"Parent Bone Index");
		m_nParentBoneIndex = nParentBoneIndex;

		u32 iAttachFlags = m_iAttachFlags;
		SERIALISE_UNSIGNED(serialiser, iAttachFlags, SIZEOF_ATTACH_FLAGS, "Attach Flags");
		m_iAttachFlags = iAttachFlags;

		SERIALISE_ANIMATION_CLIP(serialiser, m_ClipDict, m_IdleClip, "Attach anim");
	}

private:

	static const unsigned SIZEOF_BONE_INDEX = 8;
	static const unsigned SIZEOF_ATTACH_FLAGS = 9;

	CSyncedEntity	m_parentEntity;
	s32				m_ClipDict;
	u32				m_IdleClip;
	u32				m_nParentBoneIndex : SIZEOF_BONE_INDEX;
	u32				m_iAttachFlags : SIZEOF_ATTACH_FLAGS;
};

#endif //INC_TASKANIMATEDATTACH_H_
