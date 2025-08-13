// Filename   :	TaskCrawl.h
// Description:	Task to handle crawl from underneath of a vehicle or object to a safe position for getting up
//

#ifndef INC_TASKCRAWL_H_
#define INC_TASKCRAWL_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Game headers
#include "peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/TaskHelpers.h"
#include "network/General/NetworkUtil.h"

// ------------------------------------------------------------------------------

class CTaskCrawl : public CTaskFSMClone
{
public:

	enum
	{
		State_Start = 0,
		State_Crawl,
		State_Finish
	};

	// Pass in the safe position to crawl towards
	CTaskCrawl(const Vector3& vSafePosition);

	~CTaskCrawl();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskCrawl(m_vSafePosition);
	}

	// Clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual void ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

protected:
	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32 GetDefaultStateAfterAbort() const { return State_Finish; }

	void CleanUp();

	// Determine whether or not the task should abort.
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting.
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState)
	{
		Assert(iState >= State_Start && iState <= State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_Crawl",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

private:
	//////////////////////////////////////////////
	// Helper functions for FSM state management:
	FSM_Return Start_OnEnter(CPed* pPed);
	FSM_Return Start_OnUpdate(CPed* pPed);

	FSM_Return Crawl_OnEnter(CPed* pPed);
	FSM_Return Crawl_OnUpdate(CPed* pPed);
	void Crawl_OnExit(CPed* pPed);

	FSM_Return Finish_OnUpdate();

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_CRAWL; }

	// Helper functions:
	void StartMoveNetwork();

	const crClip* GetClip() const;

	bool CalculateDesiredHeadingAndPlaybackRate(CPed* pPed);

public:

	void SetRunningAsMotionTask(bool bIsMotionTask) { m_bRunningAsMotionTask = bIsMotionTask; }
	void OrientBounds(CPed* pPed);
	void SetSafePosition(const Vector3& vSafePosition);
	const Vector3& GetSafePosition() const { return m_vSafePosition; }

public:

	bool m_bTagSyncBlend : 1;
	bool m_bRunningAsMotionTask : 1;

	fwMvClipId m_clipId;

	float m_fBlendDuration;	
	float m_fBlendOutRootIkSlopeFixupPhase;

	CMoveNetworkHelper m_networkHelper;

	Vector3 m_vSafePosition;

	// Streams in the crawl clipset
	fwClipSetRequestHelper m_CrawlRequestHelper;

	//////////////////////////////////////////////////////////////////////////
	// MoVE network signals
	//////////////////////////////////////////////////////////////////////////

	static fwMvRequestId ms_GetUpClipRequest;

	static fwMvBooleanId ms_OnEnterGetUpClip;

	static fwMvBooleanId ms_GetUpClipFinished;

	static fwMvFloatId ms_BlendDuration;
	static fwMvRequestId ms_TagSyncBlend;

	static fwMvClipId ms_GetUpClip;
	static fwMvFloatId ms_GetUpPhase0;
	static fwMvFloatId ms_GetUpRate;
	static fwMvFloatId ms_GetUpClip0PhaseOut;
	static fwMvBooleanId ms_GetUpLooped;

	static fwMvClipId ms_CrawlForwardClipId;
	static fwMvClipId ms_CrawlBackwardClipId;
	static fwMvClipSetId ms_CrawlClipSetId;
};

//-------------------------------------------------------------------------
// Task info for crawling
//-------------------------------------------------------------------------
class CClonedCrawlInfo : public CSerialisedFSMTaskInfo
{
public:
	
	CClonedCrawlInfo(u32 uState, const Vector3& vSafePosition);
	CClonedCrawlInfo();

	~CClonedCrawlInfo() {}

	virtual s32 GetTaskInfoType( ) const { return INFO_TYPE_CRAWL; }

    virtual bool HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskCrawl::State_Finish>::COUNT; }
    virtual const char*	GetStatusName(u32) const { return "Status"; }

	const Vector3& GetSafePosition() const { return m_vSafePosition; }

	virtual CTaskFSMClone* CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_POSITION(serialiser, m_vSafePosition, "Safe Position");

		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedCrawlInfo(const CClonedCrawlInfo&);
	CClonedCrawlInfo &operator=(const CClonedCrawlInfo&);

	Vector3 m_vSafePosition;
};

#endif // ! INC_TASKCRAWL_H_
