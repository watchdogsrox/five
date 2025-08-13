#ifndef _TASK_GENERAL_SWEEP_H_
#define _TASK_GENERAL_SWEEP_H_

#include "Peds/ped.h"
#include "task/system/Task.h"
#include "task/Motion/TaskMotionBase.h"
#include "animation/Move.h"
#include "animation/MovePed.h"

#define SWEEP_TURN_RATE	(PI * 0.5f)	// 90 degrees per second

class CTaskGeneralSweep : public CTaskFSMClone
{
	friend class CClonedGeneralSweepInfo;

public:

	// These will be the final 
	CTaskGeneralSweep(const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float turnRate = SWEEP_TURN_RATE, float fBlendInDuration = SLOW_BLEND_DURATION);
	CTaskGeneralSweep(const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float turnRate = SWEEP_TURN_RATE, float fBlendInDuration = SLOW_BLEND_DURATION );

	CTaskGeneralSweep(const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float turnRate = SWEEP_TURN_RATE, float fBlendInDuration = SLOW_BLEND_DURATION );
	CTaskGeneralSweep(const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float turnRate = SWEEP_TURN_RATE, float fBlendInDuration = SLOW_BLEND_DURATION );

	virtual ~CTaskGeneralSweep();

	/////////////////////////////////////////////////////////////////////
	// Access Functions
	/////////////////////////////////////////////////////////////////////

	// Track an entity
	void	SetTrackEntity(const CEntity *pEntity)
	{
		m_TrackEntity = pEntity;
		m_TrackMode = TRACKING_MODE_ENTITY;
	}

	// Track a point
	void	SetTrackPosition(const Vector3 *pPosition)
	{
		m_TrackPoint = *pPosition;
		m_TrackMode = TRACKING_MODE_POINT;
	}

	// Sets the update rate in radians per second
	void SetUpdateAnglePerSecond( float angle ) { m_deltaPerSecond = angle;	}

	// Are we pointing at the point specified?
	bool IsAtDestination() { return ((m_DesiredPitch == m_CurrentPitch) && (m_DesiredYaw == m_CurrentYaw)); }

	// Quit the task
	void Stop() { m_Stop = true; }

	/////////////////////////////////////////////////////////////////////

	enum
	{
		State_Initial,
		State_DoSweep,
		State_Finish
	};

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32			GetDefaultStateAfterAbort() const { return State_Initial; }

	// TODO: needs to support the two tracking modes
	virtual aiTask*		Copy() const 
	{
		if( m_clipSetId != CLIP_SET_ID_INVALID )
		{
			// By Clip Set
			if( m_TrackMode == TRACKING_MODE_ENTITY )
			{
				// TRACKING_MODE_ENTITY
				return rage_new CTaskGeneralSweep(m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, m_RunTime, m_TrackEntity, m_turnSpeedMultiplier,m_fBlendInDuration);
			}
			else
			{
				// TRACKING_MODE_POINT
				return rage_new CTaskGeneralSweep(m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, m_RunTime, &m_TrackPoint, m_turnSpeedMultiplier,m_fBlendInDuration);
			}
		}
		else
		{
			// By Clip Dict
			if( m_TrackMode == TRACKING_MODE_ENTITY )
			{
				// TRACKING_MODE_ENTITY
				return rage_new CTaskGeneralSweep(m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, m_RunTime, m_TrackEntity, m_turnSpeedMultiplier,m_fBlendInDuration);
			}
			else
			{
				// TRACKING_MODE_POINT
				return rage_new CTaskGeneralSweep(m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, m_RunTime, &m_TrackPoint, m_turnSpeedMultiplier,m_fBlendInDuration);
			}
		}
	}

	virtual s32			GetTaskTypeInternal() const { return CTaskTypes::TASK_GENERAL_SWEEP; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32 );
	void Debug() const;
#endif //!__FINAL

	// FSM optional functions
	virtual	void		CleanUp();

	// clone task implementation
	virtual CTaskInfo*		CreateQueriableState() const;
	virtual void			ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void			OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskFSMClone*	CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*	CreateTaskForLocalPed(CPed* pPed);
	virtual FSM_Return		UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

protected:

private:

	typedef	enum
	{
		TRACKING_MODE_ENTITY = 0,
		TRACKING_MODE_POINT,
	}	TRACKING_MODE;

	const crClip *GetClipFromSetOrDict(const fwMvClipId &clipId);

	FSM_Return			StateInitial_OnEnter();
	FSM_Return			StateInitial_OnUpdate();
	FSM_Return			StateDoSweep_OnEnter();
	FSM_Return			StateDoSweep_OnUpdate();

	// Updates desired pitch and yaw to the set
	void				CalculateDesiredPitchAndYaw();
	// Does the interpolation
	void				UpdatePitchAndYaw();
	// Returns the delta of the change in the range 0->1
	float				UpdateAngle( float &desiredAngle, float &currentAngle );

	// Extract the MinYaw/MaxYaw from the clip data passed (returns success or failure)
	bool				GetYawMinMaxAndPitch( const crClip *pClipClip, float &minYaw, float &maxYaw, float &pitch );

	// Converts the input to a float between 0.0 -> 1.0 
	//	(within the range specified, though could go beyond the bounds if the input is beyond the bounds)
	//	Assumes the following constants:-
	//	min = 0.0
	//	mid = 0.5
	//	max = 1.0
	float				GetInterpolatedFloatParam( float in, float min, float mid, float max)
	{
		float range;

		in < mid ? range = mid-min : range = max-mid;
		return 0.5f + (in - mid)*(0.5f/range);
	}

	//	Controlling vars

	// Goes true when the task should stop
	bool				m_Stop;				

	TRACKING_MODE		m_TrackMode;
	RegdConstEnt		m_TrackEntity;
	Vector3				m_TrackPoint;

	// The time, in mseconds that the task should run (-1 means forever)
	int					m_RunTime;

	fwClipSetRequestHelper	m_ClipSetRequestHelper;
	CClipRequestHelper		m_ClipRequestHelper;

	// Clip Set/Dict and Clip ID data
	int					m_clipDictId;				// For reference via Dictionary
	fwMvClipSetId		m_clipSetId;				// For Reference via Clip Set
	fwMvClipId			m_lowClipId;
	fwMvClipId			m_mediumClipId;
	fwMvClipId			m_highClipId;

	// The rate at which we update pitch/yaw (per second)
	float				m_deltaPerSecond;
	// The time it takes for the move network to blend in 
	float				m_fBlendInDuration;


	//	Modify the time it takes to turn with this multiplier
	float				m_turnSpeedMultiplier;

	// The network Helper
	CMoveNetworkHelper	m_networkHelper;
	bool				m_bLeaveNetworkHelper;

	float				m_DesiredPitch;
	float				m_DesiredYaw;
	float				m_CurrentPitch;
	float				m_CurrentYaw;

	// Min/Max yaw from data within the clip (we use the "mid" clip ranges, assuming the others are the same)
	float				m_minYaw;
	float				m_maxYaw;
	// The pitch of each clip
	float				m_minPitch;
	float				m_medPitch;
	float				m_maxPitch;

	//	MoVE control parameters
	static	const fwMvClipId	ms_HighClipId;
	static	const fwMvClipId	ms_MediumClipId;
	static	const fwMvClipId	ms_LowClipId;
	static	const fwMvFloatId	ms_PitchId;
	static	const fwMvFloatId	ms_YawId;
};

class CClonedGeneralSweepInfo : public CSerialisedFSMTaskInfo
{
public:

	 CClonedGeneralSweepInfo(const s32 state, const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float fDelta, float turnRate, float fBlendInDuration);
	 CClonedGeneralSweepInfo(const s32 state, const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float fDelta, float turnRate, float fBlendInDuration);
	 CClonedGeneralSweepInfo(const s32 state, const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float fDelta, float turnRate, float fBlendInDuration);
	 CClonedGeneralSweepInfo(const s32 state, const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float fDelta, float turnRate, float fBlendInDuration);
	 CClonedGeneralSweepInfo();
	~CClonedGeneralSweepInfo() {}

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_GENERAL_SWEEP; }
	virtual int GetScriptedTaskType() const;

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTaskFSMClone *CreateLocalTask(fwEntity* pEntity);

	virtual bool			HasState() const			{ return true; }
	virtual u32				GetSizeOfState() const		{ return datBitsNeeded<CTaskGeneralSweep::State_Finish>::COUNT;}

	inline u32				GetTrackingMode() const				{ return m_TrackMode;					}
	inline const CEntity*	GetTrackEntity() const				{ return m_pTrackEntity.GetEntity();	}
	inline CEntity*			GetTrackEntity()					{ return m_pTrackEntity.GetEntity();	}
	inline Vector3 const&	GetTrackPoint() const				{ return m_TrackPoint;					}

	inline s32				GetRunTime() const					{ return (m_RunTime == 0) ? -1 : static_cast<s32>(m_RunTime); }

	inline int				GetClipDictID() const				{ return m_clipDictId;					}
	inline fwMvClipSetId	GetClipSetID() const				{ return m_clipSetId;					}
	inline fwMvClipId		GetLowClipID() const				{ return m_lowClipId;					}
	inline fwMvClipId		GetMediumClipID() const				{ return m_mediumClipId;				}
	inline fwMvClipId		GetHighClipID() const				{ return m_highClipId;					}

	inline float			GetDeltaPerSecond() const			{ return m_deltaPerSecond;				}
	inline float			GetTurnSpeedMultiplier() const		{ return m_turnSpeedMultiplier;			}
	inline float            GetBlendInDuration() const			{ return m_fBlendInDuration;			}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		u32 nTrackMode = m_TrackMode;
		SERIALISE_UNSIGNED(serialiser, nTrackMode, SIZEOF_TRACK_MODE, "Track Mode");
		m_TrackMode = nTrackMode;

		// only send required info for this tracking mode
		if(m_TrackMode == CTaskGeneralSweep::TRACKING_MODE_POINT || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_POSITION(serialiser, m_TrackPoint, "Track Point");
		}
		else if(m_TrackMode == CTaskGeneralSweep::TRACKING_MODE_ENTITY)
		{
			ObjectId targetID = m_pTrackEntity.GetEntityID();
			SERIALISE_OBJECTID(serialiser, targetID, "Track Entity");
			m_pTrackEntity.SetEntityID(targetID);
		}

		bool bSyncTime = (m_RunTime > 0);
		SERIALISE_BOOL(serialiser, bSyncTime, "Has Run Time");
		if(bSyncTime || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 nRunTime = m_RunTime;
			SERIALISE_UNSIGNED(serialiser, nRunTime, SIZEOF_RUN_TIME, "Run Time");
			m_RunTime = nRunTime;
		}
		else
			m_RunTime = 0;

		// 
		bool bFromClipSet = (m_clipSetId != CLIP_SET_ID_INVALID);
		SERIALISE_BOOL(serialiser, bFromClipSet, "Is Clip Set");
		if(bFromClipSet || serialiser.GetIsMaximumSizeSerialiser())
		{
			u32 clipSetIdHash = m_clipSetId.GetHash();
			SERIALISE_UNSIGNED(serialiser, clipSetIdHash, SIZEOF_CLIP_SET_ID, "Clip Set ID");
			m_clipSetId.SetHash(clipSetIdHash);
		}
		else
		{
			u32 clipDictId = m_clipDictId;
			SERIALISE_INTEGER(serialiser, clipDictId, SIZEOF_ANIMSLOTINDEX, "Clip Dictionary ID");
			m_clipDictId = clipDictId;
		}

		u32 animIdHash;

		animIdHash = m_lowClipId.GetHash();
		SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID, "Low Clip");
		m_lowClipId.SetHash(animIdHash);

		animIdHash = m_mediumClipId.GetHash();
		SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID, "Medium Clip");
		m_mediumClipId.SetHash(animIdHash);

		animIdHash = m_highClipId.GetHash();
		SERIALISE_UNSIGNED(serialiser, animIdHash, SIZEOF_ANIM_ID, "High Clip");
		m_highClipId.SetHash(animIdHash);

		bool bHasDelta = ABS(m_deltaPerSecond - SWEEP_TURN_RATE) < VERY_SMALL_FLOAT;
		SERIALISE_BOOL(serialiser, bHasDelta, "Has Delta");
		if(bHasDelta || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_deltaPerSecond, 2 * PI, SIZEOF_DELTA, "Delta Per Second");
			SERIALISE_PACKED_FLOAT(serialiser, m_turnSpeedMultiplier, 100.0f, SIZEOF_DELTA, "Turn Rate");
		}
	}

private:

	static const unsigned SIZEOF_TRACK_MODE		= 1;
	static const unsigned SIZEOF_RUN_TIME		= 14;
	static const unsigned SIZEOF_CLIP_SET_ID	= 32;
	static const unsigned SIZEOF_ANIM_ID		= 32;
	static const unsigned SIZEOF_ANIMSLOTINDEX	= 14;
	static const unsigned SIZEOF_PITCHYAW		= 12;
	static const unsigned SIZEOF_DELTA			= 12;

	CClonedGeneralSweepInfo(const CClonedGeneralSweepInfo &);
	CClonedGeneralSweepInfo &operator=(const CClonedGeneralSweepInfo &);

	// What we're tracking
	Vector3				m_TrackPoint;
	CSyncedEntity		m_pTrackEntity;

	// The rate at which we update pitch/yaw (per second)
	float				m_deltaPerSecond;
	// The time it takes for the move network to blend in 
	float				m_fBlendInDuration;

	// Clip Set/Dictionary and Clip ID data
	fwMvClipId			m_lowClipId;
	fwMvClipId			m_mediumClipId;
	fwMvClipId			m_highClipId;
	fwMvClipSetId		m_clipSetId;							// For Reference via Clip Set
	int					m_clipDictId : SIZEOF_ANIMSLOTINDEX;	// For reference via Dictionary

	// The time, in ms that the task should run (0 means forever)
	u32					m_RunTime : SIZEOF_RUN_TIME;
	u32					m_TrackMode : SIZEOF_TRACK_MODE;
	//	Modify the time it takes to turn with this multiplier
	float				m_turnSpeedMultiplier;
};

#endif	//_TASK_GENERAL_SWEEP_H_