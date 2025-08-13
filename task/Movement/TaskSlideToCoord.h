#ifndef TASK_SLIDE_TO_COORD_H
#define TASK_SLIDE_TO_COORD_H

#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskMove.h"

class CEntity;
class CPed;

//*********************************
// Slide to coord
//
class CTaskSlideToCoord : public CTask
{
public:

	enum
	{
		State_AchieveHeading,
		State_PlayClip
	};

    CTaskSlideToCoord(const Vector3& vPos, const float fHeading, const float fSpeed);
    CTaskSlideToCoord(const Vector3& vPos, const float fHeading, const float fSpeed, const char* pClipName, const strStreamingObjectName pClipDictName, const s32 flags, const float fBlendDelta, const bool bRunInSequence, const int iTime=-1);
	CTaskSlideToCoord(const Vector3& vPos, const float fHeading, const float fSpeed, fwMvClipId clipId, fwMvClipSetId clipSetId, const float fBlendDelta=NORMAL_BLEND_IN_DELTA, const s32 flags=0, const bool bRunInSequence=false, const int iTime=-1 );
	~CTaskSlideToCoord();

	inline void SetHeadingIncrement(float fInc)
	{
		Assertf(m_bFirstTime, "Call this once task is created, but before it is first run!");
		m_fHeadingIncrement = fInc;
	}

  	virtual aiTask* Copy() const;
		
    virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SLIDE_TO_COORD; }

#if !__FINAL
    virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_AchieveHeading: return "AchieveHeading";
			case State_PlayClip: return "PlayClip";
		}
		Assert(false);
		return NULL;
	}
#endif 
    
	virtual s32 GetDefaultStateAfterAbort() const { return State_AchieveHeading; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StateAchieveHeading_OnUpdate(CPed * pPed);
	FSM_Return StatePlayClip_OnEnter(CPed * pPed);
	FSM_Return StatePlayClip_OnUpdate(CPed * pPed);

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	static bool ProcessSlidePhysics(const Vector3 & vTargetPos, CPed* pPed, const float fTimeStep, const float fDesiredSpeed, const float fDesiredAccuracy);
	static void RemoveShuffleStopClip(CPed * pPed);

	inline void SetTargetAndHeading( Vector3& vTarget, float fHeading ) { m_vPos = vTarget; m_fHeading = fHeading; }

	inline void SetMaxTime(const int iTime) { m_iClipTime = iTime; }
	inline void SetPosAccuracy(const float f) { m_fPosAccuracy = f; }
	inline float GetPosAccuracy() const { return m_fPosAccuracy; }
	inline void SetUseZeroVelocityIfGroundPhsyicalIsMoving( bool bUseZeroVelocityIfGroundPhsyicalIsMoving ) { m_bUseZeroVelocityIfGroundPhsyicalIsMoving = bUseZeroVelocityIfGroundPhsyicalIsMoving; }
	inline bool GetUseZeroVelocityIfGroundPhsyicalIsMoving( void ) { return m_bUseZeroVelocityIfGroundPhsyicalIsMoving; }
	virtual void CleanUp();

	const CClipHelper* GetSubTaskClip();

	// Function to create cloned task information
	virtual CTaskInfo* CreateQueriableState() const;

protected:

private:


    Vector3 m_vPos;
	Vector2 m_vToTarget;
    float m_fHeading;
	float m_fHeadingIncrement;
    float m_fSpeed;
	float m_fPosAccuracy;
    CTaskGameTimer m_TaskTimer;

	fwMvClipId m_ClipId;
	fwMvClipSetId m_ClipSetId;
	s32 m_iClipFlags;
	float m_fClipBlendInDelta;
	s32 m_iClipTime;
	bool m_bClipRunInSequence;

    u32 m_bFirstTime				:1;    
    u32 m_bRunningClip				:1;
	u32 m_bClipExtractingVelocity	:1;
	bool m_bUseZeroVelocityIfGroundPhsyicalIsMoving :1;

	
	CTask * CreateClipTask();

public:
	static float ms_fPosAccuracy;
	static float ms_fHeadingAccuracy;
	static float ms_fSpeedMin;
	static float ms_fSpeedMax;
};

//***********************************************************
//	Slide to coord info
//

class CClonedSlideToCoordInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedSlideToCoordInfo(const Vector3& position, float heading, float speed, const fwMvClipSetId clipSetId, const fwMvClipId clipId)
		: m_Position(position)
		, m_Heading(heading)
		, m_Speed(speed)
		, m_clipSetId(clipSetId)
		, m_clipId(clipId)
	{
		m_bHasAnimation = (m_clipId.GetHash() != 0 && m_clipSetId.GetHash() != 0);
	}

	CClonedSlideToCoordInfo()
		: m_Position(Vector3(VEC3_ZERO))
		, m_Heading(0.0f)
		, m_Speed(0.0f)
		, m_bHasAnimation(false)
	{
		m_clipId.Clear();
		m_clipSetId.Clear();
	}

	virtual CTask *CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
	{
		if(m_bHasAnimation)
			return rage_new CTaskSlideToCoord(m_Position, m_Heading, m_Speed, m_clipId, m_clipSetId);
		else
			return rage_new CTaskSlideToCoord(m_Position, m_Heading, m_Speed);
	}

	virtual s32 GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_SLIDE_TO_COORD; }
	virtual int GetScriptedTaskType() const	{ return SCRIPT_TASK_PED_SLIDE_TO_COORD; }

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_POSITION(serialiser, m_Position, "Position");
		SERIALISE_PACKED_FLOAT(serialiser, m_Heading, PI, SIZEOF_HEADING, "Heading");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_Speed, 1.0f, SIZEOF_SPEED, "Speed");

		SERIALISE_BOOL(serialiser, m_bHasAnimation, "Has Animation");
		if(m_bHasAnimation || serialiser.GetIsMaximumSizeSerialiser())
			SERIALISE_ANIMATION_CLIP(serialiser, m_clipSetId, m_clipId, "Animation");
	}

private:

	static const unsigned SIZEOF_HEADING = 8;
	static const unsigned SIZEOF_SPEED = 8;

	Vector3	m_Position;
	float	m_Heading;
	float	m_Speed;

	bool			m_bHasAnimation;
	fwMvClipSetId	m_clipSetId;
	fwMvClipId		m_clipId;
};

//***********************************************************
//	Slide to coord - a cut-down movement task version.
//

class CTaskMoveSlideToCoord : public CTaskMove
{
public:

	CTaskMoveSlideToCoord(const Vector3 & vPos, const float fHeading, const float fSpeed, const float fHeadingInc, const int iTime);
	~CTaskMoveSlideToCoord();

	enum // State
	{
		Running
	};

	virtual aiTask* Copy() const;

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_SLIDE_TO_COORD; }

	inline void SetTargetAndHeading( Vector3& vTarget, float fHeading ) { m_vPos = vTarget; m_fHeading = fHeading; }

#if !__FINAL
	virtual void Debug() const;
	virtual atString GetName() const { return atString("MoveSlideToCoord"); }
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "none";  };
#endif 

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget() const { Assertf(0, "CTaskMoveSlideToCoord : GetTarget called() on move task with no move target" ); return m_vPos;}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { Assertf(0, "CTaskMoveSlideToCoord : GetTargetRadius() called on move task no move target" ); return 0.0f;}

	//*******************************************
	// Virtual members inherited From CTask
	//*******************************************
	virtual s32 GetDefaultStateAfterAbort() const { return Running; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	FSM_Return StateRunning_OnUpdate(CPed * pPed);

	inline void SetPosAccuracy(const float f) { m_fPosAccuracy = f; }
	inline float GetPosAccuracy() const { return m_fPosAccuracy; }

	inline void SetMaxTime(const int iTime) { m_iTimer = fwTimer::GetTimeInMilliseconds()+iTime; }
	
	virtual void CleanUp();

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);


private:


	Vector3 m_vPos;
	Vector2 m_vToTarget;
	float m_fHeading;
	float m_fHeadingIncrement;
	float m_fPosAccuracy;
	float m_fSpeed;
	int m_iTimer;
	
	u32 m_bFirstTime					:1;
};



#endif // TASK_SLIDE_TO_COORD_H

