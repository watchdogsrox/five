#ifndef TASK_SEEK_ENTITY_H
#define TASK_SEEK_ENTITY_H

// Framework headers
#include "fwmaths/Angle.h"

// Game headers
#include "Control/Route.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "PathServer/PathServer.h"
#include "PedGroup/Pedgroup.h"
#include "Peds/ped.h"
#include "Peds/PedPlacement.h"
#include "scene/world/gameworld.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Movement/TaskNavMesh.h"

///////////////////////////////////
// CTaskMoveSeekEntity calculators
///////////////////////////////////

class CEntitySeekPosCalculator
{
public:

	CEntitySeekPosCalculator(){}
	virtual ~CEntitySeekPosCalculator(){}	
	
	virtual void ComputeEntitySeekPos(const CPed& UNUSED_PARAM(ped), const CEntity& UNUSED_PARAM(entity), Vector3& UNUSED_PARAM(vSeekPos)) const = 0;	
};

class CEntitySeekPosCalculatorStandard : public CEntitySeekPosCalculator
{
public:

	CEntitySeekPosCalculatorStandard(){}
	~CEntitySeekPosCalculatorStandard(){}	
	
	virtual void ComputeEntitySeekPos(const CPed& UNUSED_PARAM(ped), const CEntity& entity, Vector3& vSeekPos) const
	{
		vSeekPos=VEC3V_TO_VECTOR3(entity.GetTransform().GetPosition());
	}	
};

class CEntitySeekPosCalculatorLastNavMeshIntersection : public CEntitySeekPosCalculatorStandard
{
public:

	CEntitySeekPosCalculatorLastNavMeshIntersection(){}
	~CEntitySeekPosCalculatorLastNavMeshIntersection(){}	

	virtual void ComputeEntitySeekPos(const CPed& ped, const CEntity& entity, Vector3& vSeekPos) const
	{
		if(CLastNavMeshIntersectionHelper::Get(entity, RC_VEC3V(vSeekPos)))
		{
			return;
		}

		CEntitySeekPosCalculatorStandard::ComputeEntitySeekPos(ped, entity, vSeekPos);
	}	
};

class CEntitySeekPosCalculatorRadiusAngleOffset : public CEntitySeekPosCalculator
{
public:

	CEntitySeekPosCalculatorRadiusAngleOffset()
	: CEntitySeekPosCalculator(),
	  m_fRadius(0),
	  m_fAngle(0){}			
	CEntitySeekPosCalculatorRadiusAngleOffset(const float fRadius, const float fAngle)
	: CEntitySeekPosCalculator(),
	  m_fRadius(fRadius),
	  m_fAngle(fAngle){}	
	~CEntitySeekPosCalculatorRadiusAngleOffset(){}	
	
	virtual void ComputeEntitySeekPos(const CPed& ped, const CEntity& entity, Vector3& vSeekPos) const;

private:

	float m_fRadius;
	float m_fAngle;
};

class CEntitySeekPosCalculatorXYOffsetFixed : public CEntitySeekPosCalculator
{
public:

	CEntitySeekPosCalculatorXYOffsetFixed() : CEntitySeekPosCalculator(), m_vOffset(Vector3(0,0,0)) { }
	CEntitySeekPosCalculatorXYOffsetFixed(const Vector3& vOffset) : CEntitySeekPosCalculator(), m_vOffset(vOffset){ } 
	~CEntitySeekPosCalculatorXYOffsetFixed() { }	
	
	inline void SetOffset(const Vector3& offset) { m_vOffset = offset; }
	virtual void ComputeEntitySeekPos(const CPed& ped, const CEntity& entity, Vector3& vSeekPos) const;

private:

	Vector3 m_vOffset;
};

class CEntitySeekPosCalculatorXYOffsetRotated : public CEntitySeekPosCalculator
{
public:

	CEntitySeekPosCalculatorXYOffsetRotated() : CEntitySeekPosCalculator(), m_vOffset(Vector3(0,0,0)) { }
	CEntitySeekPosCalculatorXYOffsetRotated(const Vector3& vOffset) : CEntitySeekPosCalculator(), m_vOffset(vOffset) { } 
	~CEntitySeekPosCalculatorXYOffsetRotated() { }

	inline void SetOffset(const Vector3& offset) { m_vOffset = offset; }
	virtual void ComputeEntitySeekPos(const CPed& ped, const CEntity& entity, Vector3& vSeekPos) const;

private:

	Vector3 m_vOffset;
};

/////////////////////////////////
// CTaskMoveSeekEntity task infos
/////////////////////////////////

class CClonedSeekEntityInfoBase : public CSerialisedFSMTaskInfo
{
public:

	CClonedSeekEntityInfoBase(ObjectId entityId, int seekTime, u32 iPeriod, float fTargetRadius, float fSlowDownDistance, float fFollowNodeThresholdHeightChange, float fMoveBlendRatio, bool bFaceEntityWhenDone, bool bUseLargerSearchExtents, bool bUseAdaptiveUpdateFreq, bool bPreferPavements) 
	: m_entityId(entityId)
	, m_iSeekTime(seekTime)
	, m_iPeriod(iPeriod)
	, m_fTargetRadius(fTargetRadius)
	, m_fSlowDownDistance(fSlowDownDistance)
	, m_fFollowNodeThresholdHeightChange(fFollowNodeThresholdHeightChange)
	, m_fMoveBlendRatio(fMoveBlendRatio)
	, m_bFaceEntityWhenDone(bFaceEntityWhenDone)  
	, m_bUseLargerSearchExtents(bUseLargerSearchExtents)
	, m_bUseAdaptiveUpdateFreq(bUseAdaptiveUpdateFreq)
	, m_bPreferPavements(bPreferPavements)
	{
	}

	CClonedSeekEntityInfoBase()
	: m_entityId(0)
	, m_iSeekTime(0)
	, m_iPeriod(0)
	, m_fTargetRadius(0.0f)
	, m_fSlowDownDistance(0.0f)
	, m_fFollowNodeThresholdHeightChange(0.0f)
	, m_fMoveBlendRatio(0.0f)
	, m_bFaceEntityWhenDone(false)  
	, m_bUseLargerSearchExtents(false)
	, m_bUseAdaptiveUpdateFreq(false)
	, m_bPreferPavements(false)
	{
	}

	void Serialise(CSyncDataBase& serialiser)
	{
		const unsigned SIZEOF_SEEK_TIME = 16;
		const unsigned SIZEOF_PERIOD = 10;
		const unsigned SIZEOF_RADIUS = 8;
		const unsigned SIZEOF_SLOWDOWN_DIST = 8;
		const unsigned SIZEOF_HEIGHT_CHANGE = 8;
		const unsigned SIZEOF_MOVE_BLEND_RATIO = 8;

		const float MAX_RADIUS = 10.0f;
		const float MAX_SLOWDOWN_DIST = 10.0f;
		const float MAX_HEIGHT_CHANGE = 8;
		const float MAX_MOVE_BLEND_RATIO = 2.0f;
	
		SERIALISE_OBJECTID(serialiser, m_entityId, "Entity");
		
		// -1 indicates default, add 1 to get more +ve time
		u32 uSeekTime = static_cast<u32>(m_iSeekTime + 1);
		SERIALISE_UNSIGNED(serialiser, uSeekTime, SIZEOF_SEEK_TIME, "Seek Time");
		m_iSeekTime = static_cast<int>(uSeekTime) - 1;

		SERIALISE_UNSIGNED(serialiser, m_iPeriod, SIZEOF_PERIOD, "Period");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fTargetRadius, MAX_RADIUS, SIZEOF_RADIUS, "Target radius");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fSlowDownDistance, MAX_SLOWDOWN_DIST, SIZEOF_SLOWDOWN_DIST, "Slow Down Dist");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fFollowNodeThresholdHeightChange, MAX_HEIGHT_CHANGE, SIZEOF_HEIGHT_CHANGE, "Height Change");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fMoveBlendRatio, MAX_MOVE_BLEND_RATIO, SIZEOF_MOVE_BLEND_RATIO, "Move Blend Ratio");

		bool bFaceEntityWhenDone = m_bFaceEntityWhenDone;
		bool bUseLargerSearchExtents = m_bUseLargerSearchExtents;
		bool bUseAdaptiveUpdateFreq = m_bUseAdaptiveUpdateFreq;
		bool bPreferPavements = m_bPreferPavements;

		SERIALISE_BOOL(serialiser, bFaceEntityWhenDone, "Face Entity When Done");
		SERIALISE_BOOL(serialiser, bUseLargerSearchExtents, "Use Larger Search Extents");
		SERIALISE_BOOL(serialiser, bUseAdaptiveUpdateFreq, "Use Adaptive Update Freq");
		SERIALISE_BOOL(serialiser, bPreferPavements, "Prefer Pavements");

		m_bFaceEntityWhenDone = bFaceEntityWhenDone;
		m_bUseLargerSearchExtents = bUseLargerSearchExtents;
		m_bUseAdaptiveUpdateFreq = bUseAdaptiveUpdateFreq;
		m_bPreferPavements = bPreferPavements;
	}

protected:

	ObjectId m_entityId;
	int		 m_iSeekTime;
	u32		 m_iPeriod;
	float	 m_fTargetRadius;
	float	 m_fSlowDownDistance;
	float	 m_fFollowNodeThresholdHeightChange;
	float	 m_fMoveBlendRatio;
	bool     m_bFaceEntityWhenDone : 1;  
	bool	 m_bUseLargerSearchExtents : 1;
	bool	 m_bUseAdaptiveUpdateFreq : 1;
	bool	 m_bPreferPavements : 1;
};

class CClonedSeekEntityStandardInfo : public CClonedSeekEntityInfoBase
{
public:

	CClonedSeekEntityStandardInfo(ObjectId entityId, int seekTime, u32 iPeriod, float fTargetRadius, float fSlowDownDistance, float fFollowNodeThresholdHeightChange, float fMoveBlendRatio, bool bFaceEntityWhenDone, bool bUseLargerSearchExtents, bool bUseAdaptiveUpdateFreq, bool bPreferPavements) :
	CClonedSeekEntityInfoBase(entityId, seekTime, iPeriod, fTargetRadius, fSlowDownDistance, fFollowNodeThresholdHeightChange, fMoveBlendRatio, bFaceEntityWhenDone, bUseLargerSearchExtents, bUseAdaptiveUpdateFreq, bPreferPavements) 
	{
	}

	CClonedSeekEntityStandardInfo()
	{
	}

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_SEEK_ENTITY_STANDARD; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GO_TO_ENTITY; }

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);
};

class CClonedSeekEntityLastNavMeshIntersectionInfo : public CClonedSeekEntityInfoBase
{
public:

	  CClonedSeekEntityLastNavMeshIntersectionInfo(ObjectId entityId, int seekTime, u32 iPeriod, float fTargetRadius, float fSlowDownDistance, float fFollowNodeThresholdHeightChange, float fMoveBlendRatio, bool bFaceEntityWhenDone, bool bUseLargerSearchExtents, bool bUseAdaptiveUpdateFreq, bool bPreferPavements) :
	  CClonedSeekEntityInfoBase(entityId, seekTime, iPeriod, fTargetRadius, fSlowDownDistance, fFollowNodeThresholdHeightChange, fMoveBlendRatio, bFaceEntityWhenDone, bUseLargerSearchExtents, bUseAdaptiveUpdateFreq, bPreferPavements) 
	  {
	  }

	  CClonedSeekEntityLastNavMeshIntersectionInfo()
	  {
	  }

	  virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_SEEK_ENTITY_LAST_NAV_MESH_INTERSECTION; }
	  virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GO_TO_ENTITY; }

	  virtual CTask  *CreateLocalTask(fwEntity *pEntity);
};

class CClonedSeekEntityOffsetRotateInfo : public CClonedSeekEntityInfoBase
{
public:

	CClonedSeekEntityOffsetRotateInfo(ObjectId entityId, int seekTime, u32 iPeriod, float fTargetRadius, float fSlowDownDistance, float fFollowNodeThresholdHeightChange, float fMoveBlendRatio, bool bFaceEntityWhenDone, bool bUseLargerSearchExtents, bool bUseAdaptiveUpdateFreq, bool bPreferPavements) :
	CClonedSeekEntityInfoBase(entityId, seekTime, iPeriod, fTargetRadius, fSlowDownDistance, fFollowNodeThresholdHeightChange, fMoveBlendRatio, bFaceEntityWhenDone, bUseLargerSearchExtents, bUseAdaptiveUpdateFreq, bPreferPavements) 
	{
	}
	
	CClonedSeekEntityOffsetRotateInfo()
	{
	}

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_SEEK_ENTITY_OFFSET_ROTATE; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GOTO_ENTITY_OFFSET; }

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);
};

class CClonedSeekEntityOffsetFixedInfo : public CClonedSeekEntityInfoBase
{
public:

	CClonedSeekEntityOffsetFixedInfo(ObjectId entityId, int seekTime, u32 iPeriod, float fTargetRadius, float fSlowDownDistance, float fFollowNodeThresholdHeightChange, float fMoveBlendRatio, bool bFaceEntityWhenDone, bool bUseLargerSearchExtents, bool bUseAdaptiveUpdateFreq, bool bPreferPavements) :
	CClonedSeekEntityInfoBase(entityId, seekTime, iPeriod, fTargetRadius, fSlowDownDistance, fFollowNodeThresholdHeightChange, fMoveBlendRatio, bFaceEntityWhenDone, bUseLargerSearchExtents, bUseAdaptiveUpdateFreq, bPreferPavements) 
	{
	}

	CClonedSeekEntityOffsetFixedInfo()
	{
	}

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_SEEK_ENTITY_OFFSET_FIXED; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GOTO_ENTITY_OFFSET; }

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);
};

class CClonedSeekEntityRadiusAngleInfo : public CClonedSeekEntityInfoBase
{
public:

	CClonedSeekEntityRadiusAngleInfo(ObjectId entityId, int seekTime, u32 iPeriod, float fTargetRadius, float fSlowDownDistance, float fFollowNodeThresholdHeightChange, float fMoveBlendRatio, bool bFaceEntityWhenDone, bool bUseLargerSearchExtents, bool bUseAdaptiveUpdateFreq, bool bPreferPavements) :
    CClonedSeekEntityInfoBase(entityId, seekTime, iPeriod, fTargetRadius, fSlowDownDistance, fFollowNodeThresholdHeightChange, fMoveBlendRatio, bFaceEntityWhenDone, bUseLargerSearchExtents, bUseAdaptiveUpdateFreq, bPreferPavements) 
	{
	}

	CClonedSeekEntityRadiusAngleInfo()
	{
	}

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_SEEK_ENTITY_RADIUS_ANGLE; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_GOTO_ENTITY_OFFSET; }

	virtual CTask  *CreateLocalTask(fwEntity *pEntity);
};


////////////////////////
// CTaskMoveSeekEntity
////////////////////////

template <int _TaskType, class _CalcType, class _InfoType> class CTaskMoveSeekEntity : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_ArrivedAtDestination,
		State_WaitUntilStopped,
		State_SeekingEntity,
		State_StandStillThenQuit
	};

    static const int ms_iMaxSeekTime;
    static const int ms_iPeriod;
    static const float ms_fTargetRadius;
    static const float ms_fSlowDownDistance;
    static const float ms_fFollowNodeThresholdHeightChange;
	static const float ms_fGoToPointDistance;
	static const float ms_fPathCompletionRadius;
	static const float ms_fDefaultRunDist;
	static const float ms_fDefaultWalkDist;
    
	CTaskMoveSeekEntity(
    	const CEntity* pEntity, 
         const int iMaxSeekTime=ms_iMaxSeekTime,
         const int iPeriod=ms_iPeriod, 
         const float fTargetRadius=ms_fTargetRadius, 
         const float fSlowDownDistance=ms_fSlowDownDistance, 
         const float fFollowNodeThresholdHeightChange=ms_fFollowNodeThresholdHeightChange,
		const bool bFaceEntityWhenDone = true
	);
    virtual ~CTaskMoveSeekEntity();

	virtual aiTask* Copy() const;
    virtual int GetTaskTypeInternal() const {return _TaskType;}

#if !__FINAL
    virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_ArrivedAtDestination: return "ArrivedAtDestination";
			case State_WaitUntilStopped: return "WaitUntilStopped";
			case State_SeekingEntity : return "SeekingEntity";
			case State_StandStillThenQuit : return "StandStillThenQuit";
		};
		Assert(false);
		return NULL;
	}
#endif 

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed * pPed);
	FSM_Return ArrivedAtDestination_OnUpdate(CPed * pPed);
	FSM_Return WaitUntilStopped_OnUpdate(CPed * pPed);
	FSM_Return SeekingEntity_OnUpdate(CPed * pPed);
	FSM_Return StandStillThenQuit_OnUpdate(CPed * pPed);

    CEntity* GetSeekEntity() const {return m_pEntity;}
    
    inline void SetEntitySeekPosCalculator(const _CalcType& seekPosCalculator) { m_entitySeekPosCalculator=seekPosCalculator; }
	inline _CalcType*  GetEntitySeekPosCalculator(void) { return &m_entitySeekPosCalculator; }

	inline float CalcMbrForRange(float fRange);
    
	virtual float GetTargetRadius(void) const { return m_fTargetRadius; }
    inline void SetTargetRadius(const float fTargetRadius) 
    {
    	if(fTargetRadius!=m_fTargetRadius)
    	{
	    	//Set the radius.
   		 	m_fTargetRadius=fTargetRadius;
  		  	//Set the scan timer so that it is out of time to force an update.
    		m_scanTimer.Set(fwTimer::GetTimeInMilliseconds(),0);
    	}
	}
    
	// If a path cannot be found to get within the m_fTargetRadius of the entity, then it will be
	// sufficient to get within 'm_fPathCompletionRadius' of the target.  Defaults at 2.0f.
	inline void SetPathCompletionRadius(float fDist) { m_fPathCompletionRadius = fDist; }
    
    inline bool AchievedSeekEntity() const { return m_bAchievedSeekEntity;}
    inline float GetSlowDownDistance() { return m_fSlowDownDistance; }

    inline void SetTrackingEntity(bool b) { m_bTrackingEntity = b; }
	
	// Set the distance at which this task switches from doing a GoToPoint task and a FollowNodeRoute task
	// Defaults at 0.0f which means it will always do FollowNodeRoute, unless the distance is set otherwise
	inline void SetGoToPointDistance(float d) { m_fGoToPointDistance = d; }

	// Multiplier for the achieve heading task which may occur at the end of the task
	inline void SetAchieveHeadingRateMultiplier(float f) { m_fAchieveHeadingRateMultiplier = f; }
	inline float GetAchieveHeadingRateMultiplier() const { return m_fAchieveHeadingRateMultiplier; }

	// Whether to use the adjusted target position taken back from the navmesh task.  This is the target
	// position, moved onto the navmesh & pushed outside of any intersection objects.  The default 
	// behaviour is to *always* use this adjusted position - but there may be instances where we with to
	// override this behaviour.
	inline void SetUseAdjustedNavMeshTarget(const bool b) { m_bUseAdjustedNavMeshTarget = b; }
	inline bool GetUseAdjustedNavMeshTarget() const { return m_bUseAdjustedNavMeshTarget; }

	inline void SetDontStandStillAtEnd(const bool b) { m_bDontStandStillAtEnd = b; }
	inline bool SetDontStandStillAtEnd() const { return m_bDontStandStillAtEnd; }
	
	inline void SetNeverEnterWater(const bool b) { m_bNeverEnterWater = b; }
	inline bool GetNeverEnterWater() const { return m_bNeverEnterWater; }
	inline void SetNeverStartInWater(const bool b) { m_bNeverStartInWater = b; }
	inline bool GetNeverStartInWater() const { return m_bNeverStartInWater; }
	inline void SetAllowNavigationUnderwater(bool b) { m_bAllowNavigationUnderwater = b; }
	inline bool GetAllowNavigationUnderwater() const { return m_bAllowNavigationUnderwater; }
	inline void SetUseAdaptiveUpdateFreq(const bool b) { m_bUseAdaptiveUpdateFreq = b; }
	inline bool GetUseAdaptiveUpdateFreq() const { return m_bUseAdaptiveUpdateFreq; }
	inline void SetMaxSlopeNavigable(const float a) { Assert(a >= 0.0f && a <= PI/2.0f); m_fMaxSlopeNavigable = a; }
	inline float GetMaxSlopeNavigable() const { return m_fMaxSlopeNavigable; }
	inline void SetPreferPavements(const bool b) { m_bPreferPavements = b; }
	inline bool GetPreferPavements() const { return m_bPreferPavements; }
	inline void SetUseLargerSearchExtents(const bool b) { m_bUseLargerSearchExtents = b; }
	inline bool GetUseLargerSearchExtents() const { return m_bUseLargerSearchExtents; }
	inline void SetQuitTaskIfRouteNotFound(const bool b) { m_bQuitTaskIfRouteNotFound = b; }
	inline bool GetQuitTaskIfRouteNotFound() const { return m_bQuitTaskIfRouteNotFound; }
	inline void SetSlowToWalkInsteadOfUsingRunStops(const bool b)  { m_bSlowToWalkInsteadOfUsingRunStops = b; } 
	inline bool GetSlowToWalkInsteadOfUsingRunStops() const { return m_bSlowToWalkInsteadOfUsingRunStops; }
	inline void SetUseSpeedRanges(const bool b, const float fWalkDist=ms_fDefaultWalkDist, const float fRunDist=ms_fDefaultRunDist)
	{
		m_bUseSpeedRanges = b;
		if(m_bUseSpeedRanges)
		{
			m_fWalkDist = fWalkDist;
			m_fRunDist = fRunDist;
		}
	}

	inline bool GetUseSpeedRanges() const { return m_bUseSpeedRanges; }

	inline void SetNeverSlowDownForPathLength(bool bNeverSlowDown) { m_bNeverSlowDownForPathLength = bNeverSlowDown; }
	

	//an appropriate target point.
	virtual bool HasTarget() const { return false; }
	virtual Vector3 GetTarget(void) const { return m_vTargetPosition; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual void SetTarget			( const CPed* UNUSED_PARAM(pPed), Vec3V_In UNUSED_PARAM(vTarget), const bool UNUSED_PARAM(bForce)=false) 
	{
		Assertf(0, "Seek entity tasks must have target set as entity not vec!");
	};
	virtual void SetSlowDownDistance( const float fSlowDownDistance  ) { m_fSlowDownDistance = fSlowDownDistance; }

	virtual CTaskInfo* CreateQueriableState() const
	{
		// FIXME - This code should be converting m_pEntity to an ObjectId, but instead it was accidentally converting the pointer to a bool (ie 1 or 0), then an ObjectId, which I doubt was intended.
		return rage_new _InfoType(m_pEntity != 0, m_iMaxSeekTime, m_iPeriod, m_fTargetRadius, m_fSlowDownDistance, ms_fFollowNodeThresholdHeightChange, m_fMoveBlendRatio, m_bFaceEntityWhenDone, m_bUseLargerSearchExtents, m_bUseAdaptiveUpdateFreq, m_bPreferPavements);
	}
 
protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
	{
		// Force target pos to be updated as soon as task is reactivated
		m_scanTimer.Set(fwTimer::GetTimeInMilliseconds(), 0);
		// Base class
		CTaskMove::DoAbort(iPriority, pEvent);
	}

	float GetTargetHeading(CPed * pPed) const
	{
		Assert(m_pEntity);
		Vector3 vDir = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition()-pPed->GetTransform().GetPosition());
		vDir.Normalize();
		return fwAngle::GetRadianAngleBetweenPoints(vDir.x,vDir.y,0,0);
	}

	RegdConstEnt m_pEntity;
	Vector3	 m_vTargetPosition;
    int m_iMaxSeekTime;
    int m_iPeriod;
    float m_fTargetRadius;
    float m_fSlowDownDistance;
    float m_fGoToPointDistance;
	float m_fAchieveHeadingRateMultiplier;
	float m_fPathCompletionRadius;
	float m_fMaxSlopeNavigable;
	float m_fRunDist;
	float m_fWalkDist;
    CTaskGameTimer m_scanTimer;
    CTaskGameTimer m_seekTimer;
    _CalcType m_entitySeekPosCalculator;
      
    u32 m_bFaceEntityWhenDone				:1;
    u32 m_bAchievedSeekEntity				:1;
    u32 m_bTrackingEntity					:1;
	u32 m_bUseAdjustedNavMeshTarget			:1;
	u32 m_bNeverEnterWater					:1;
	u32 m_bNeverStartInWater				:1;
	u32 m_bUseAdaptiveUpdateFreq			:1;
	u32 m_bAllowNavigationUnderwater		:1;
	u32 m_bDontStandStillAtEnd				:1;
	u32 m_bPreferPavements					:1;
	u32	m_bUseLargerSearchExtents			:1;
	u32 m_bQuitTaskIfRouteNotFound			:1;
	u32 m_bSlowToWalkInsteadOfUsingRunStops	:1;
	u32 m_bUseSpeedRanges					:1;
	u32 m_bUseSuddenStops					:1;
	u32 m_bNeverSlowDownForPathLength		:1;
      
    aiTask* CreateSubTask(const int iSubTaskType, CPed* pPed);
    
    Vector3 ComputeSeekPos(const CPed* pPed) const;
};

template <int _TaskType, class _CalcType, class _InfoType> const int   CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_iMaxSeekTime=50000;
template <int _TaskType, class _CalcType, class _InfoType> const int   CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_iPeriod=1000;
template <int _TaskType, class _CalcType, class _InfoType> const float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_fTargetRadius=1.0f; 
template <int _TaskType, class _CalcType, class _InfoType> const float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_fSlowDownDistance=2.0f; 
template <int _TaskType, class _CalcType, class _InfoType> const float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_fGoToPointDistance=0.0f; 
template <int _TaskType, class _CalcType, class _InfoType> const float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_fFollowNodeThresholdHeightChange=2.0f;
template <int _TaskType, class _CalcType, class _InfoType> const float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_fPathCompletionRadius=2.0f;

template <int _TaskType, class _CalcType, class _InfoType> const float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_fDefaultRunDist=20.0f;
template <int _TaskType, class _CalcType, class _InfoType> const float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ms_fDefaultWalkDist=10.0f;

template <int _TaskType, class _CalcType, class _InfoType> CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::CTaskMoveSeekEntity
(const CEntity* pEntity, 
 const int iMaxSeekTime, 
 const int iPeriod, 
 const float fTargetRadius, 
 const float fSlowDownDistance, 
 const float UNUSED_PARAM(fFollowNodeThresholdHeightChange),
 const bool bFaceEntityWhenDone)
: CTaskMove(MOVEBLENDRATIO_RUN),
  m_pEntity(pEntity),
  m_vTargetPosition(0.0f, 0.0f, 0.0f),
  m_iMaxSeekTime(iMaxSeekTime),
  m_iPeriod(iPeriod),
  m_fTargetRadius(fTargetRadius),
  m_fSlowDownDistance(fSlowDownDistance),
  m_fGoToPointDistance(0.0f),
  m_fPathCompletionRadius(ms_fPathCompletionRadius),
  m_bFaceEntityWhenDone(bFaceEntityWhenDone),
  m_bDontStandStillAtEnd(false),
  m_fAchieveHeadingRateMultiplier(1.0f),
  m_bAchievedSeekEntity(false),
  m_bTrackingEntity(false),
  m_bUseAdjustedNavMeshTarget(true),
  m_bNeverEnterWater(false),
  m_bNeverStartInWater(false),
  m_bAllowNavigationUnderwater(false),
  m_bUseAdaptiveUpdateFreq(false),
  m_bPreferPavements(false),
  m_bUseLargerSearchExtents(false),
  m_bQuitTaskIfRouteNotFound(false),
  m_bSlowToWalkInsteadOfUsingRunStops(false),
  m_fMaxSlopeNavigable(0.0f),
  m_bUseSpeedRanges(false),
  m_bUseSuddenStops(false),
  m_bNeverSlowDownForPathLength(false),
  m_fRunDist(ms_fDefaultRunDist),
  m_fWalkDist(ms_fDefaultWalkDist)
{
	if(m_pEntity)
	{
		// Initialise the original target to be the entities position
		m_vTargetPosition = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition());
	}

	SetInternalTaskType((CTaskTypes::eTaskType)_TaskType);
}

template <int _TaskType, class _CalcType, class _InfoType> CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::~CTaskMoveSeekEntity()
{
}

template <int _TaskType, class _CalcType, class _InfoType> aiTask* CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::Copy() const
{
	CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>* p = rage_new CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>(
		m_pEntity,
		m_iMaxSeekTime,
		m_iPeriod,
		m_fTargetRadius,
		m_fSlowDownDistance,
		0.0f,
		m_bFaceEntityWhenDone
	);
	p->m_entitySeekPosCalculator = m_entitySeekPosCalculator;
	p->m_fMoveBlendRatio = m_fMoveBlendRatio;
	p->SetTrackingEntity( m_bTrackingEntity );
	p->SetGoToPointDistance( m_fGoToPointDistance );
	p->SetPathCompletionRadius( m_fPathCompletionRadius );
	p->SetAchieveHeadingRateMultiplier( GetAchieveHeadingRateMultiplier() );
	p->SetUseAdjustedNavMeshTarget( GetUseAdjustedNavMeshTarget() );
	p->SetNeverEnterWater( GetNeverEnterWater() );
	p->SetAllowNavigationUnderwater( GetAllowNavigationUnderwater() );
	p->SetUseAdaptiveUpdateFreq( GetUseAdaptiveUpdateFreq() );
	p->SetMaxSlopeNavigable( GetMaxSlopeNavigable() );
	p->SetDontStandStillAtEnd( m_bDontStandStillAtEnd );
	p->SetPreferPavements( GetPreferPavements() );
	p->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);
	p->SetQuitTaskIfRouteNotFound( m_bQuitTaskIfRouteNotFound );
	p->SetSlowToWalkInsteadOfUsingRunStops( GetSlowToWalkInsteadOfUsingRunStops() );
	p->SetUseSpeedRanges( m_bUseSpeedRanges, m_fWalkDist, m_fRunDist );
	p->m_bUseSuddenStops = m_bUseSuddenStops;
	p->m_bNeverSlowDownForPathLength = m_bNeverSlowDownForPathLength;

	return p;
}

#if !__FINAL
template <int _TaskType, class _CalcType, class _InfoType> void CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

template <int _TaskType, class _CalcType, class _InfoType> CTask::FSM_Return
CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return Initial_OnUpdate(pPed);
		FSM_State(State_ArrivedAtDestination)
			FSM_OnUpdate
				return ArrivedAtDestination_OnUpdate(pPed);
		FSM_State(State_WaitUntilStopped)
			FSM_OnUpdate
				return WaitUntilStopped_OnUpdate(pPed);
		FSM_State(State_SeekingEntity)
			FSM_OnUpdate
				return SeekingEntity_OnUpdate(pPed);
		FSM_State(State_StandStillThenQuit)
			FSM_OnUpdate
				return StandStillThenQuit_OnUpdate(pPed);
	FSM_End
}

template <int _TaskType, class _CalcType, class _InfoType> CTask::FSM_Return
CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::Initial_OnUpdate(CPed * pPed)
{
    //Set the scan timer to scan the target position every so often.
    m_scanTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iPeriod);
    
    //Set the max seek time if required.
    if(m_iMaxSeekTime!=-1)
    {
        //Only set the seek timer if a non-default seek time has been set.
        m_seekTimer.Set(fwTimer::GetTimeInMilliseconds(), m_iMaxSeekTime);
    }

    if(m_pEntity.Get()==NULL)
    {
    	//No seek entity so just stand still and then finish.
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_StandStillThenQuit);
    	return FSM_Continue;
    }
    else
    {
	    if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
	    {
	    	//Ped in a vehicle
			Assertf(0, "Ped told to seek entity when in car!" );

			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetState(State_StandStillThenQuit);
    		return FSM_Continue;
	    }
	    else
    	{	   
	    	m_vTargetPosition = ComputeSeekPos(pPed);
	    	
	    	Vector3 vecDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTargetPosition;
	    	vecDiff.z=0;
	    	const float f2=vecDiff.Mag2();
	    	if(f2<(m_fTargetRadius*m_fTargetRadius))
	    	{
	    		//Already there so just turn to face entity and stand still.
				m_bAchievedSeekEntity=true;	    		
				pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
			    SetNewTask( CreateSubTask(CTaskTypes::TASK_COMPLEX_MOVE_SEQUENCE, pPed) );
				SetState(State_ArrivedAtDestination);
				return FSM_Continue;
	    	}
	    	else
			{
				//**********************************************************************************************
				// Seek the entity using the navmesh
				// Unless m_fGoToPointDistance is non-zero - in which case if within the m_fGoToPointDistance
				// we do a simple goto point task (used when tracking moving entities, etc)

                if(m_fGoToPointDistance != 0.0f)
               	{
                	if(f2 > m_fGoToPointDistance*m_fGoToPointDistance)
                	{
						SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
						SetState(State_SeekingEntity);
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
						return FSM_Continue;
				    }
	               	else
	               	{
	                   	SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL, pPed) );
						SetState(State_SeekingEntity);
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
						return FSM_Continue;
	               	}
				}
				else
				{
					SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
					SetState(State_SeekingEntity);
					SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					return FSM_Continue;
				}
			}
		}
	}
}

template <int _TaskType, class _CalcType, class _InfoType> CTask::FSM_Return
CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ArrivedAtDestination_OnUpdate(CPed * pPed)
{
	if(!GetSubTask())
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_StandStillThenQuit);
    	return FSM_Continue;
	}

	//Assert(GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_MOVE_SEQUENCE || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_ACHIEVE_HEADING);

	if(m_pEntity.Get()==NULL)
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_StandStillThenQuit);
    	return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_StandStillThenQuit);
    	return FSM_Continue;
	}

	return FSM_Continue;
}

template <int _TaskType, class _CalcType, class _InfoType> CTask::FSM_Return
CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::WaitUntilStopped_OnUpdate(CPed * pPed)
{
	if (m_bFaceEntityWhenDone)
	{
		bool bOkToAchieveHeading = false;
		// Ok do this for walkstops also, since at the end of the walkstop we might have moved and therefor need to re-orient a bit
		// We should however try to nail the desired heading through an oriented walkstop though
		//if (m_fMoveBlendRatio < MOVEBLENDRATIO_RUN)
		//{
		//	bOkToAchieveHeading = true;	// Walkstops are short enough?
		//}
		//else
		{
			CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
			if (pMotionTask)
			{
				bool bIsStopClipRunning = false;
				const float fTargetHeading = GetTargetHeading(pPed);
				float fStoppingDist = pMotionTask->GetStoppingDistance(fTargetHeading, &bIsStopClipRunning);
				if (!bIsStopClipRunning || abs(fStoppingDist) < 0.1f)	// Ok either no stop is running or we only got settle left
					bOkToAchieveHeading = true;
			}
		}

		if (bOkToAchieveHeading)
		{	
			m_bAchievedSeekEntity = true;
			SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_ACHIEVE_HEADING, pPed) );
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetState(State_ArrivedAtDestination);
			return FSM_Continue;
		}
	}
	else
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_StandStillThenQuit);
		return FSM_Continue;
	}

	return FSM_Continue;
}

template <int _TaskType, class _CalcType, class _InfoType> CTask::FSM_Return
CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::SeekingEntity_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && (GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL));

	if(m_pEntity.Get()==NULL)
	{
		SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_StandStillThenQuit);
    	return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//***************************************************************************
		// Check that the ped is near the entity.  If we've just completed a navmesh
		// task, then check whether the target position was adjusted by that task.
		// If so then check against the adjusted position.
		//***************************************************************************
		if(GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			CTaskMoveFollowNavMesh * pNavMesh = (CTaskMoveFollowNavMesh*)GetSubTask();
			
			//Check if the nav-mesh was unable to find a route.
			if(pNavMesh->IsUnableToFindRoute())
			{
				//Check if we should quit the task if the route is not found.
				if(m_bQuitTaskIfRouteNotFound)
				{
					return FSM_Quit;
				}
			}

			//*************************************************************************************************
			// Under normal usage we will want to seek towards the position which the navmesh task deemed to
			// be the effective path end.  For example, if the target is standing on top of a largeish object
			// then the navmesh task will push the target position out to the edge of the object - thus
			// preventing us from try to get inside the object.  This behaviour can be overridden so that the
			// target radius is calculated from the actual target.

			if(pNavMesh->WasTargetPosAdjusted() && m_bUseAdjustedNavMeshTarget)
			{
				pNavMesh->GetAdjustedTargetPos(m_vTargetPosition);
			}
			else
			{
				m_vTargetPosition = ComputeSeekPos(pPed);
			}
		}
		else
		{
			m_vTargetPosition = ComputeSeekPos(pPed);
		}


		Vector3 vDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTargetPosition;
		vDiff.z=0;
		const float fDistSqr=vDiff.Mag2();
		if(fDistSqr > (m_fTargetRadius*m_fTargetRadius))
		{
			//********************************************************************************************************
			// Ped isn't near the entity - the entity must have moved since the last time the task scanned the
			// entity position in ControlSubTask(). Carry on seeking. If within m_fGoToPointDistance, do a GoToPoint
			//********************************************************************************************************
			if(m_fGoToPointDistance != 0.0f)
			{
				if(fDistSqr > m_fGoToPointDistance*m_fGoToPointDistance)
				{
					aiTask * pNextSubTask = CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed);
					Assert(pNextSubTask);
					if(pNextSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
					{
						((CTaskMoveFollowNavMesh*)pNextSubTask)->SetKeepMovingWhilstWaitingForPath(true);
						((CTaskMoveFollowNavMesh*)pNextSubTask)->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vTargetPosition));
						((CTaskMoveFollowNavMesh*)pNextSubTask)->SetTargetRadius(m_fTargetRadius);
					}
					SetNewTask(pNextSubTask);
				}
				else
				{
					SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL, pPed) );
				}
			}
			else
			{
				SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
			}
		}
		else
		{
			// If 'm_bDontStandStillAtEnd' is set then we just quit here.
			if(m_bDontStandStillAtEnd)
			{
				return FSM_Quit;
			}

			// Really arrived at the entity.
			if (m_bFaceEntityWhenDone)
			{
				SetState(State_WaitUntilStopped);
				return FSM_Continue;
			}
			else
			{
				m_bAchievedSeekEntity = true;
				SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				SetState(State_StandStillThenQuit);
    			return FSM_Continue;
			}   
		}
	}
	else
	{
		if(m_bUseSpeedRanges)
		{
			CTask * pSubTask = GetSubTask();
			if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
			{
				float fDist = ((CTaskMoveFollowNavMesh*)pSubTask)->GetDistanceLeftOnCurrentRouteSection(pPed);
				float fMBR = CalcMbrForRange(fDist);
				((CTaskMoveFollowNavMesh*)pSubTask)->SetMoveBlendRatio(fMBR);
			}
		}

		if (m_bFaceEntityWhenDone)
		{	
			// This should hopefully trigger proper oriented walkstops
			const float fTargetHeading = GetTargetHeading(pPed);
			pPed->SetDesiredHeading(fTargetHeading);
		}

		if(m_seekTimer.IsSet() && m_seekTimer.IsOutOfTime())
		{
			//The seek timer has been set and is out of time.
			//Give up seeking entity if the subtask can be quit.
			if(GetSubTask()->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0))
			{
				SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_STAND_STILL, pPed));
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				SetState(State_StandStillThenQuit);
		    	return FSM_Continue;
			}
		}  
		else if(m_scanTimer.IsOutOfTime())
		{
			m_scanTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iPeriod);
			m_vTargetPosition = ComputeSeekPos(pPed);

			Vector3 vecDiff = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTargetPosition;
			vecDiff.z=0;
			const float f2=vecDiff.Mag2();

			if(GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
			{
				// If now within GoToPoint distance, switch tasks
				if((m_fGoToPointDistance != 0.0f) && (f2 < (m_fGoToPointDistance-1.0f)*(m_fGoToPointDistance-1.0f)))
				{
					if(GetSubTask()->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0))
					{                
						SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL,pPed) );
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
						return FSM_Continue;
					}
				}
				else
				{
					Assert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);

					CTaskMoveFollowNavMesh* p=(CTaskMoveFollowNavMesh*)GetSubTask();

					p->SetKeepMovingWhilstWaitingForPath(true);
					p->SetTarget(pPed,VECTOR3_TO_VEC3V(m_vTargetPosition));
					p->SetTargetRadius(m_fTargetRadius);
					p->SetSlowDownDistance(m_fSlowDownDistance);
				}
			}
			else
			{
				Assert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL);

				// Only go altering target/creating node route if task didn't complete yet	            
				if(((CTaskMoveGoToPointAndStandStill*)GetSubTask())->WasTaskSuccessful())
				{

				}
				else
				{
					// If now outside of GoToPoint distance, switch tasks
					if((m_fGoToPointDistance != 0.0f) && (f2 > (m_fGoToPointDistance+1.0f)*(m_fGoToPointDistance+1.0f)))
					{
						if(GetSubTask()->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0))
						{                
							SetNewTask( CreateSubTask(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, pPed) );
							SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
							return FSM_Continue;
						}
					}
					else
					{
						CTaskMoveGoToPointAndStandStill* p=(CTaskMoveGoToPointAndStandStill*)GetSubTask();
						p->SetTarget(pPed,VECTOR3_TO_VEC3V(m_vTargetPosition));
						p->SetTargetRadius(m_fTargetRadius);
						p->SetSlowDownDistance(m_fSlowDownDistance);
					}
				}
			}
		}
	}

	//*******************************************************************************
	//	Adaptive update frequency changes the frequency of updating the target in 
	//	the subtasks, based on the distance from the ped to the entity.  The further
	//	this distance, the less frequent the update is.

	if(m_bUseAdaptiveUpdateFreq && m_pEntity)
	{
		const Vector3 vToEntity = VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
		const float fDistToEntity = vToEntity.Mag();
		static const float fMaxAdaptDist = 20.0f;
		static const float fMinAdaptDist = 8.0f;
		const s32 iMinAdaptFreq = Min(250, ms_iPeriod);
		const s32 iMaxAdaptFreq = ms_iPeriod;

		if(fDistToEntity < fMinAdaptDist)
		{
			m_iPeriod = iMinAdaptFreq;
		}
		else if(fDistToEntity > fMaxAdaptDist)
		{
			m_iPeriod = iMaxAdaptFreq;
		}
		else
		{
			const float s = (fDistToEntity - fMinAdaptDist) / (fMaxAdaptDist - fMinAdaptDist);
			const s32 iRange = (s32) (((float)(iMaxAdaptFreq - iMinAdaptFreq))*s);
			m_iPeriod = iMinAdaptFreq + iRange;
			Assert(m_iPeriod >= iMinAdaptFreq && m_iPeriod <= iMaxAdaptFreq);
		}
		if(m_scanTimer.IsSet())
		{
			m_scanTimer.ChangeDuration(m_iPeriod);
		}
	}

	return FSM_Continue;
}

template <int _TaskType, class _CalcType, class _InfoType> CTask::FSM_Return
CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::StandStillThenQuit_OnUpdate(CPed * pPed)
{
	Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_STAND_STILL);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_bUseSuddenStops)	// Reset sudden stops
		{
			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
			{
				((CTaskHumanLocomotion*)pTask)->SetPreferSuddenStops(false);
			}
		}

		return FSM_Quit;
	}

	return FSM_Continue;
}

template <int _TaskType, class _CalcType, class _InfoType> float CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::CalcMbrForRange(float fRange)
{
	Assert(m_bUseSpeedRanges);

	float fMBR;
	if(fRange < m_fWalkDist)
		fMBR = MOVEBLENDRATIO_WALK;
	else if(fRange < m_fRunDist)
		fMBR = MOVEBLENDRATIO_RUN;
	else
		fMBR = MOVEBLENDRATIO_SPRINT;

	fMBR = Min(fMBR, m_fMoveBlendRatio);
	return fMBR;
}

template <int _TaskType, class _CalcType, class _InfoType> aiTask* CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::CreateSubTask(const int iSubTaskType, CPed* pPed)
{
    aiTask* pSubTask=0;
    switch(iSubTaskType)
    {
		case CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH:
		{
			m_vTargetPosition = ComputeSeekPos(pPed);

			float fMBR = m_fMoveBlendRatio;
			if(m_bUseSpeedRanges)
			{
				Vector3 vDiff = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTargetPosition);
				fMBR = CalcMbrForRange(vDiff.Mag());
			}

            pSubTask=rage_new CTaskMoveFollowNavMesh(
            	fMBR,
            	m_vTargetPosition,
            	m_fTargetRadius,
            	m_fSlowDownDistance,
				-1,
				true,
				false,
				NULL,
				m_fPathCompletionRadius
           	);
			((CTaskMoveFollowNavMesh*)pSubTask)->SetNeverEnterWater(m_bNeverEnterWater);
			((CTaskMoveFollowNavMesh*)pSubTask)->SetNeverStartInWater(m_bNeverStartInWater);
			((CTaskMoveFollowNavMesh*)pSubTask)->SetMaxSlopeNavigable(m_fMaxSlopeNavigable);
			((CTaskMoveFollowNavMesh*)pSubTask)->SetDontStandStillAtEnd(m_bDontStandStillAtEnd);
			((CTaskMoveFollowNavMesh*)pSubTask)->SetKeepToPavements(m_bPreferPavements);
			((CTaskMoveFollowNavMesh*)pSubTask)->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);
			((CTaskMoveFollowNavMesh*)pSubTask)->SetQuitTaskIfRouteNotFound(m_bQuitTaskIfRouteNotFound);

			if (_TaskType == CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD || _TaskType == CTaskTypes::TASK_MOVE_SEEK_ENTITY_LAST_NAV_MESH_INTERSECTION)
			{
				((CTaskMoveFollowNavMesh*)pSubTask)->SetStopExactly(false);

				// Ok so, peds need some distance to stop so we prefer sudden stops if we otherwise might end up hugging our target entity
				static const float fWalkRadiusToConsider = 2.f;
				static const float fRunRadiusToConsider = 6.f;
				if (m_fTargetRadius < fWalkRadiusToConsider || (m_fMoveBlendRatio >= MBR_RUN_BOUNDARY && m_fTargetRadius < fRunRadiusToConsider))
				{
					CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
					if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
					{
						((CTaskHumanLocomotion*)pTask)->SetPreferSuddenStops(true);
						m_bUseSuddenStops = true;
					}
				}
			}

			if(m_bSlowToWalkInsteadOfUsingRunStops)
			{
				((CTaskMoveFollowNavMesh*)pSubTask)->SetStopExactly(false);
				((CTaskMoveFollowNavMesh*)pSubTask)->SetSlowDownInsteadOfUsingRunStops(true);
			}

			if (m_bNeverSlowDownForPathLength)
			{
				((CTaskMoveFollowNavMesh*)pSubTask)->SetNeverSlowDownForPathLength(true);
			}

            break;
		}
        case CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL:
		{
			m_vTargetPosition = ComputeSeekPos(pPed);
			float fMBR = m_fMoveBlendRatio;
			if(m_bUseSpeedRanges)
			{
				Vector3 vDiff = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTargetPosition);
				fMBR = CalcMbrForRange(vDiff.Mag());
			}
			pSubTask=rage_new CTaskMoveGoToPointAndStandStill(
        		fMBR,
        		m_vTargetPosition,
        		m_fTargetRadius,
        		m_fSlowDownDistance
       		);
       		((CTaskMoveGoToPointAndStandStill*)pSubTask)->SetTrackingEntity(m_bTrackingEntity);
           	
			break;
		}
		case CTaskTypes::TASK_MOVE_ACHIEVE_HEADING:
			{
				const float fTargetHeading = GetTargetHeading(pPed);
				pSubTask = rage_new CTaskMoveAchieveHeading(fTargetHeading, m_fAchieveHeadingRateMultiplier);
			}
			break;
		case CTaskTypes::TASK_MOVE_STAND_STILL:
			pSubTask=rage_new CTaskMoveStandStill(1);
			break;
        case CTaskTypes::TASK_COMPLEX_MOVE_SEQUENCE:
        	pSubTask=rage_new CTaskMoveSequence;
        	if (m_bFaceEntityWhenDone)
        	{
				const float fTargetHeading=GetTargetHeading(pPed);

	        	((CTaskMoveSequence*)pSubTask)->AddTask(rage_new CTaskMoveAchieveHeading(fTargetHeading, m_fAchieveHeadingRateMultiplier));
	        }
        	((CTaskMoveSequence*)pSubTask)->AddTask(rage_new CTaskMoveStandStill(100));
        	break;
        case CTaskTypes::TASK_FINISHED:
            pSubTask=0;
            break;
        default:
            Assert(false);
            break;
    }
    return pSubTask;
}

template <int _TaskType, class _CalcType, class _InfoType> Vector3 CTaskMoveSeekEntity<_TaskType, _CalcType, _InfoType>::ComputeSeekPos(const CPed* pPed) const
{
	Vector3 vSeekPos;
	m_entitySeekPosCalculator.ComputeEntitySeekPos(*pPed,*m_pEntity,vSeekPos);
	return vSeekPos;
}

typedef CTaskMoveSeekEntity<CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD,						CEntitySeekPosCalculatorStandard,					CClonedSeekEntityStandardInfo>					TTaskMoveSeekEntityStandard;
typedef CTaskMoveSeekEntity<CTaskTypes::TASK_MOVE_SEEK_ENTITY_LAST_NAV_MESH_INTERSECTION,	CEntitySeekPosCalculatorLastNavMeshIntersection,	CClonedSeekEntityLastNavMeshIntersectionInfo>	TTaskMoveSeekEntityLastNavMeshIntersection;
typedef CTaskMoveSeekEntity<CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE,				CEntitySeekPosCalculatorXYOffsetRotated,			CClonedSeekEntityOffsetRotateInfo>				TTaskMoveSeekEntityXYOffsetRotated;
typedef CTaskMoveSeekEntity<CTaskTypes::TASK_MOVE_SEEK_ENTITY_OFFSET_FIXED,					CEntitySeekPosCalculatorXYOffsetFixed,				CClonedSeekEntityOffsetFixedInfo>				TTaskMoveSeekEntityXYOffsetFixed;
typedef CTaskMoveSeekEntity<CTaskTypes::TASK_MOVE_SEEK_ENTITY_RADIUS_ANGLE,					CEntitySeekPosCalculatorRadiusAngleOffset,			CClonedSeekEntityRadiusAngleInfo>				TTaskMoveSeekEntityRadiusAngleOffset;


//**********************************************************************************
//	CTaskComplexFollowLeaderInFormation
//	This task is used to make a ped follow their group leader around.
//**********************************************************************************

class CTaskFollowLeaderInFormation : public CTask
{
public:

	static const float ms_fXYDiffSqrToSetNewTarget;
	static const float ms_fZDiffToSetNewTarget;
	static const float ms_fMaxDistanceToLeader;
	static const float ms_fMinSecondsBetweenCoverAttempts;
	static const s32 ms_iPossibleLookAtFreq;
	static const s32 ms_iLookAtChance;

	enum
	{
		State_Initial,
		State_EnteringVehicle,
		State_InVehicle,
		State_ExitingVehicle,
		State_SeekingLeader,
		State_StandingByLeader,
		State_InFormation,
		State_SeekCover
	};

	CTaskFollowLeaderInFormation(CPedGroup* pPedGroup, CPed* pLeader, const float fMaxDistanceToLeader=ms_fMaxDistanceToLeader, const s32 iPreferredSeat = 1);
    ~CTaskFollowLeaderInFormation();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskFollowLeaderInFormation(m_pPedGroup, m_pLeader, m_fMaxDistanceToLeader, m_iPreferredSeat);
	}

    virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_FOLLOW_LEADER_IN_FORMATION; }
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual FSM_Return ProcessPreFSM();

	inline void SetUseLargerSearchExtents(const bool b) { m_bUseLargerSearchExtents = b; }
	inline bool GetUseLargerSearchExtents() const { return m_bUseLargerSearchExtents; }

	inline void SetUseAdditionalAimTask(const bool b) { m_bUseAdditionalAimTask = b; }

	static bool IsLeaderInRangedCombat(const CPed* pLeaderPed);
	static bool IsLeaderInMeleeCombat(const CPed* pLeaderPed);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_EnteringVehicle : return "EnteringVehicle";
			case State_InVehicle : return "InVehicle";
			case State_ExitingVehicle : return "ExitingVehicle";
			case State_SeekingLeader : return "SeekingLeader";
			case State_StandingByLeader : return "StandingByLeader";
			case State_InFormation : return "InFormation";
			case State_SeekCover : return "SeekingCover";
			default: Assert(0); return NULL;
		}
	}
#endif 
    
	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateEnteringVehicle_OnEnter(CPed * pPed);
	FSM_Return StateEnteringVehicle_OnUpdate(CPed * pPed);
	FSM_Return StateInVehicle_OnEnter(CPed * pPed);
	FSM_Return StateInVehicle_OnUpdate(CPed * pPed);
	FSM_Return StateExitingVehicle_OnEnter(CPed * pPed);
	FSM_Return StateExitingVehicle_OnUpdate(CPed * pPed);
	FSM_Return StateSeekingLeader_OnEnter(CPed * pPed);
	FSM_Return StateSeekingLeader_OnUpdate(CPed * pPed);
	FSM_Return StateStandingByLeader_OnEnter(CPed * pPed);
	FSM_Return StateStandingByLeader_OnUpdate(CPed * pPed);
	FSM_Return StateInFormation_OnEnter(CPed * pPed);
	FSM_Return StateInFormation_OnUpdate(CPed * pPed);
	FSM_Return StateSeekCover_OnEnter(CPed* pPed);
	FSM_Return StateSeekCover_OnUpdate(CPed* pPed);
	FSM_Return StateSeekCover_OnExit();

    inline const CPed* GetLeader() const { return m_pLeader; }

	inline void SetMaxDistanceToLeader(float fMaxDistance) { m_fMaxDistanceToLeader = fMaxDistance; }
    
protected:
   
	Vector3 m_vLeaderPositionOnCoverStart;
	CPedGroup* m_pPedGroup;
	RegdPed m_pLeader;        
	float m_fMaxDistanceToLeader;
	float m_fSecondsSinceAttemptedSeekCover;
	float m_fTimeLeaderNotMoving;
	s32 m_iPreferredSeat;
	CTaskGameTimer m_LookAtTimer;
	u32 m_bUseLargerSearchExtents : 1;
	u32 m_bUseAdditionalAimTask : 1;

	enum eSeekCoverReason
	{
		eSCR_None = 0,
		eSCR_LeaderInCombat,
		eSCR_LeaderWanted,
		eSCR_LeaderStopped,
	};
    
	aiTask* CreateSubTask(const int iSubTaskType, const CPed* pPed);
	void AbortMoveTask(CPed * pPed);
	void ProcessLookAt(CPed * pPed);	
	bool CheckEnterVehicle(CPed * pPed);
	bool IsLeaderInCover()  const;
	bool IsLeaderWanted()   const;
	eSeekCoverReason ShouldSeekCover(const CPed* pPed) const;
	bool CanSeekCoverIfLeaderHasStopped(const CPed* pPed) const;
	float ComputeSeekTargetRadius(const CPed* pPed) const;
	bool LeaderOnSameBoat(const CPed* pPed, const float fMaxDistanceAway = 5.0f) const;

	static bool IsLeaderInCombatType(const CPed* pLeader, bool bMelee);
};



///////////////////////////
//Follow leader any means
///////////////////////////

class CTaskFollowLeaderAnyMeans : public CTask
{
public:

	enum
	{
		State_Initial,
		State_FollowingInFormation
	};

	CTaskFollowLeaderAnyMeans(CPedGroup* pPedGroup, CPed* pLeader);
    ~CTaskFollowLeaderAnyMeans();

  	virtual aiTask* Copy() const { return rage_new CTaskFollowLeaderAnyMeans(m_pPedGroup, m_pLeader); }

    virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent * pEvent);
	virtual bool IsValidForMotionTask(CTaskMotionBase& task) const;
    virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_FOLLOW_LEADER_ANY_MEANS; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }
	virtual FSM_Return ProcessPreFSM();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_FollowingInFormation: return "FollowingInFormation";
			default: Assert(0); return NULL;
		}
	}
#endif 

private:

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateFollowingInFormation_OnEnter(CPed * pPed);
	FSM_Return StateFollowingInFormation_OnUpdate(CPed * pPed);

	CPedGroup* m_pPedGroup;
	RegdPed m_pLeader;
};

class CTaskSeekEntityAiming : public CTask
{
public:

	enum
	{
		State_Initial,
		State_Seeking,
		State_StandingStill
	};

	CTaskSeekEntityAiming(const CEntity* pEntity, const float fSeekRadius, const float fAimRadius);
	 ~CTaskSeekEntityAiming();
	
   	virtual aiTask* Copy() const
	{
		CTaskSeekEntityAiming * pClone = rage_new CTaskSeekEntityAiming(m_pTargetEntity,m_fSeekRadius,m_fAimRadius);
		pClone->SetUseLargerSearchExtents( GetUseLargerSearchExtents() );
		return pClone;
	}
    virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SEEK_ENTITY_AIMING; }
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_Seeking: return "Seeking";
			case State_StandingStill: return "StandingStill";
			default: Assert(0); return NULL;
		}
	}
#endif 

	inline void SetUseLargerSearchExtents(const bool b) { m_bUseLargerSearchExtents = b; }
	inline bool GetUseLargerSearchExtents() const { return m_bUseLargerSearchExtents; }

	// when passed across network
	virtual CTaskInfo* CreateQueriableState() const;
	
private:

	FSM_Return StateInitial_OnEnter(CPed * pPed);
	FSM_Return StateInitial_OnUpdate(CPed * pPed);
	FSM_Return StateSeeking_OnEnter(CPed * pPed);
	FSM_Return StateSeeking_OnUpdate(CPed * pPed);
	FSM_Return StateStandingStill_OnEnter(CPed * pPed);
	FSM_Return StateStandingStill_OnUpdate(CPed * pPed);

	RegdConstEnt m_pTargetEntity;
	float m_fSeekRadius;
	float m_fAimRadius;
	bool m_bUseLargerSearchExtents;
};

class CClonedSeekEntityAimingInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedSeekEntityAimingInfo()
		: m_entity(NULL)
	{}

	CClonedSeekEntityAimingInfo(const CEntity* pEntity, float fSeekRadius, float fAimRadius)
		: m_entity(const_cast<CEntity*>(pEntity))
		, m_fSeekRadius(fSeekRadius)
		, m_fAimRadius(fAimRadius)
	{

	}

	virtual CTask* CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
	{
		return rage_new CTaskSeekEntityAiming(m_entity.GetEntity(), m_fSeekRadius, m_fAimRadius);
	}

	virtual s32 GetTaskInfoType() const			{ return INFO_TYPE_SEEK_ENTITY_AIMING; }
	virtual int GetScriptedTaskType() const		{ return SCRIPT_TASK_GOTO_ENTITY_AIMING; }

	virtual const CEntity*	GetEntity()	const	{ return (const CEntity*)m_entity.GetEntity(); }
	virtual CEntity*		GetEntity()			{ return (CEntity*)m_entity.GetEntity(); }
	
	float GetSeekRadius() const					{ return m_fSeekRadius; }
	float GetAimRadius() const					{ return m_fAimRadius; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		ObjectId entityID = m_entity.GetEntityID();
		SERIALISE_OBJECTID(serialiser, entityID, "Entity");
		m_entity.SetEntityID(entityID);

		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fSeekRadius, MAX_SEEK_RADIUS, NUM_SEEK_RADIUS_BITS, "Seek Radius");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fSeekRadius, MAX_AIM_RADIUS, NUM_AIM_RADIUS_BITS, "Aim Radius");
	}

private:

	CClonedSeekEntityAimingInfo(const CClonedSeekEntityAimingInfo&);
	CClonedSeekEntityAimingInfo &operator=(const CClonedSeekEntityAimingInfo&);

	static const float MAX_SEEK_RADIUS;
	static const float MAX_AIM_RADIUS;
	static const unsigned NUM_SEEK_RADIUS_BITS = 5;
	static const unsigned NUM_AIM_RADIUS_BITS = 8;

	CSyncedEntity m_entity;
	float m_fSeekRadius; 
	float m_fAimRadius; 
};

#endif
