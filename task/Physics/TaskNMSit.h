// Filename   :	TaskNMSit.h
// Description:	Natural Motion sit class (FSM version)

#ifndef INC_TASKNMSIT_H_
#define INC_TASKNMSIT_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Network/General/NetworkUtil.h"
// ------------------------------------------------------------------------------

//
// Task info for CTaskNMSit
//
class CClonedNMSitInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMSitInfo(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, const Vector3& vecLookPos, CEntity* pLookEntity, const Vector3& vecSitPos);
	CClonedNMSitInfo();
	~CClonedNMSitInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_SIT;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_ANIMATION_CLIP(serialiser, m_clipSetId, m_clipId, "Animation");

		SERIALISE_BOOL(serialiser, m_bHasLookAtEntity, "Has look at entity");

		if (m_bHasLookAtEntity || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_OBJECTID(serialiser, m_lookAtEntity, "Look at entity");
		}

		SERIALISE_POSITION(serialiser, m_vecLookPos, "Look pos");
		SERIALISE_POSITION(serialiser, m_vecSitPos, "Sit pos");
	}

private:

	CClonedNMSitInfo(const CClonedNMSitInfo &);
	CClonedNMSitInfo &operator=(const CClonedNMSitInfo &);

	fwMvClipSetId		m_clipSetId;
	fwMvClipId		m_clipId;
	Vector3			m_vecLookPos;
	ObjectId		m_lookAtEntity;
	Vector3			m_vecSitPos;
	bool			m_bHasLookAtEntity;
};

//
// Task info for CTaskNMSit
//
class CTaskNMSit : public CTaskNMBehaviour
{	
public:
	CTaskNMSit(u32 nMinTime, u32 nMaxTime, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, const Vector3& vecLookPos, CEntity* pLookEntity, const Vector3& vecSitPos);
	~CTaskNMSit();

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMSit");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMSit(m_nMinTime, m_nMaxTime, m_clipSetId, m_clipId, m_vecLooktAtPos, m_pLookAtEntity, m_vecSitPos);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_SIT;}

	Vector3		GetLookAtPos() const	{ return m_vecLooktAtPos; }
	CEntity*	GetLootAtEntity() const { return m_pLookAtEntity; }

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMSitInfo(m_clipSetId, m_clipId, m_vecLooktAtPos, m_pLookAtEntity, m_vecSitPos);
	}

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	//////////////////////////
	// Local functions and data
private:
	void UpdateLookAt(CPed* pPed, bool bStart=false);

	fwMvClipSetId m_clipSetId;
	fwMvClipId m_clipId;

	RegdEnt m_pLookAtEntity;
	Vector3	m_vecLooktAtPos;
	Vector3 m_vecSitPos;

	bool m_bActivePoseRunning;

	fwClipSetRequestHelper m_ClipSetRequestHelper;
};

#endif // ! INC_TASKNMSIT_H_
