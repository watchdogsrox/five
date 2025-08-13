#include "TaskGeneralSweep.h"
#include "Weapons/WeaponDamage.h"
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "math/angmath.h"
#include "Debug/debugscene.h"

#if !__FINAL
#define SET_DEFAULTS_ON_ERROR		// Set this to define defaults upon error rather than quitting
#endif	//!__FINAL

//////////////////////////////////////////////////////////////////////////
AI_OPTIMISATIONS()
//////////////////////////////////////////////////////////////////////////

// The 3 clip ID's (LOW/MEDIUM/HIGH)
const fwMvClipId	CTaskGeneralSweep::ms_HighClipId("HighClipId",0xCC009368);
const fwMvClipId	CTaskGeneralSweep::ms_MediumClipId("MediumClipId",0x194A33E2);
const fwMvClipId	CTaskGeneralSweep::ms_LowClipId("LowClipId",0xD3F5F2AA);
// The two control parameters (Pitch/Yaw)
const fwMvFloatId	CTaskGeneralSweep::ms_PitchId("PitchId",0xF1B8A269);
const fwMvFloatId	CTaskGeneralSweep::ms_YawId("YawId",0xA282C9F1);

//////////////////////////////////////////////////////////////////////////
// CClonedGeneralSweepInfo
//////////////////////////////////////////////////////////////////////////

CClonedGeneralSweepInfo::CClonedGeneralSweepInfo(const s32 state, const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float fDelta, float turnRate, float fBlendInDuration)
	: m_TrackMode(CTaskGeneralSweep::TRACKING_MODE_ENTITY)
	, m_pTrackEntity((CEntity*)pEntityToTrack)
	, m_TrackPoint(Vector3::ZeroType)
	, m_RunTime(runTime < 0 ? 0 : static_cast<u32>(runTime))
	, m_clipDictId(-1)
	, m_clipSetId(clipSetId)
	, m_lowClipId(lowClipId)
	, m_mediumClipId(medClipId)
	, m_highClipId(highClipId)
	, m_deltaPerSecond(fDelta)
	, m_fBlendInDuration(fBlendInDuration)
	, m_turnSpeedMultiplier(turnRate)
{
	SetStatusFromMainTaskState(state);
}

CClonedGeneralSweepInfo::CClonedGeneralSweepInfo(const s32 state, const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float fDelta, float turnRate, float fBlendInDuration)
	: m_TrackMode(CTaskGeneralSweep::TRACKING_MODE_POINT)
	, m_pTrackEntity(NULL)
	, m_TrackPoint(*pPointToTrack)
	, m_RunTime(runTime < 0 ? 0 : static_cast<u32>(runTime))
	, m_clipDictId(-1)
	, m_clipSetId(clipSetId)
	, m_lowClipId(lowClipId)
	, m_mediumClipId(medClipId)
	, m_highClipId(highClipId)
	, m_deltaPerSecond(fDelta)
	, m_fBlendInDuration(fBlendInDuration)
	, m_turnSpeedMultiplier(turnRate)
{
	SetStatusFromMainTaskState(state);
}

CClonedGeneralSweepInfo::CClonedGeneralSweepInfo(const s32 state, const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float fDelta, float turnRate, float fBlendInDuration)
	: m_TrackMode(CTaskGeneralSweep::TRACKING_MODE_ENTITY)
	, m_pTrackEntity((CEntity*)pEntityToTrack)
	, m_TrackPoint(Vector3::ZeroType)
	, m_RunTime(runTime < 0 ? 0 : static_cast<u32>(runTime))
	, m_clipDictId(clipDictId)
	, m_clipSetId(CLIP_SET_ID_INVALID)
	, m_lowClipId(lowClipId)
	, m_mediumClipId(medClipId)
	, m_highClipId(highClipId)
	, m_deltaPerSecond(fDelta)
	, m_fBlendInDuration(fBlendInDuration)
	, m_turnSpeedMultiplier(turnRate)
{
	SetStatusFromMainTaskState(state);
}

CClonedGeneralSweepInfo::CClonedGeneralSweepInfo(const s32 state, const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float fDelta, float turnRate, float fBlendInDuration)
	: m_TrackMode(CTaskGeneralSweep::TRACKING_MODE_POINT)
	, m_pTrackEntity(NULL)
	, m_TrackPoint(*pPointToTrack)
	, m_RunTime(runTime < 0 ? 0 : static_cast<u32>(runTime))
	, m_clipDictId(clipDictId)
	, m_clipSetId(CLIP_SET_ID_INVALID)
	, m_lowClipId(lowClipId)
	, m_mediumClipId(medClipId)
	, m_highClipId(highClipId)
	, m_deltaPerSecond(fDelta)
	, m_fBlendInDuration(fBlendInDuration)
	, m_turnSpeedMultiplier(turnRate)
{
	SetStatusFromMainTaskState(state);
}

CClonedGeneralSweepInfo::CClonedGeneralSweepInfo()
	: m_TrackMode(CTaskGeneralSweep::TRACKING_MODE_ENTITY)
	, m_pTrackEntity(NULL)
	, m_TrackPoint(Vector3::ZeroType)
	, m_RunTime(0)
	, m_clipDictId(-1)
	, m_clipSetId(CLIP_SET_ID_INVALID)
	, m_lowClipId(CLIP_ID_INVALID)
	, m_mediumClipId(CLIP_ID_INVALID)
	, m_highClipId(CLIP_ID_INVALID)
	, m_deltaPerSecond(SWEEP_TURN_RATE)
	, m_fBlendInDuration(SLOW_BLEND_DURATION)
	, m_turnSpeedMultiplier(1)
{
	
}

int CClonedGeneralSweepInfo::GetScriptedTaskType() const
{
	return SCRIPT_TASK_GENERAL_SWEEP;
}

CTaskFSMClone *CClonedGeneralSweepInfo::CreateCloneFSMTask()
{
	CTaskGeneralSweep* pTask = NULL;

	switch(m_TrackMode)
	{
	case CTaskGeneralSweep::TRACKING_MODE_ENTITY:
		if(m_clipSetId != CLIP_SET_ID_INVALID)
			pTask = rage_new CTaskGeneralSweep(m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, GetRunTime(), m_pTrackEntity.GetEntity(), m_turnSpeedMultiplier, m_fBlendInDuration );
		else
			pTask = rage_new CTaskGeneralSweep(m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, GetRunTime(), m_pTrackEntity.GetEntity(), m_turnSpeedMultiplier, m_fBlendInDuration);
		break;
	case CTaskGeneralSweep::TRACKING_MODE_POINT:
		if(m_clipSetId != CLIP_SET_ID_INVALID)
			pTask = rage_new CTaskGeneralSweep(m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, GetRunTime(), &m_TrackPoint, m_turnSpeedMultiplier, m_fBlendInDuration);
		else
			pTask = rage_new CTaskGeneralSweep(m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, GetRunTime(), &m_TrackPoint, m_turnSpeedMultiplier, m_fBlendInDuration);
		break;
	default:
		Assertf(0, "CClonedGeneralSweepInfo::CreateCloneFSMTask: Invalid Tracking Mode!");
		break;
	}

	if(pTask)
	{
		pTask->SetUpdateAnglePerSecond(m_deltaPerSecond);
	}

	return pTask;
}

CTaskFSMClone *CClonedGeneralSweepInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Reference by ClipSet

CTaskGeneralSweep::CTaskGeneralSweep(const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float turnRate, float fBlendInDuration )
: m_Stop(false)
, m_TrackMode(TRACKING_MODE_ENTITY)
, m_TrackEntity(pEntityToTrack)
, m_TrackPoint(Vector3::ZeroType)
, m_RunTime(runTime)
, m_clipDictId(-1)
, m_clipSetId(clipSetId)
, m_lowClipId(lowClipId)
, m_mediumClipId(medClipId)
, m_highClipId(highClipId)
, m_deltaPerSecond( SWEEP_TURN_RATE )
, m_fBlendInDuration(fBlendInDuration)
, m_turnSpeedMultiplier(turnRate)
, m_bLeaveNetworkHelper(false)
{
	SetInternalTaskType(CTaskTypes::TASK_GENERAL_SWEEP);
}

CTaskGeneralSweep::CTaskGeneralSweep(const fwMvClipSetId clipSetId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float turnRate, float fBlendInDuration )
: m_Stop(false)
, m_TrackMode(TRACKING_MODE_POINT)
, m_TrackEntity(NULL)
, m_TrackPoint(*pPointToTrack)
, m_RunTime(runTime)
, m_clipDictId(-1)
, m_clipSetId(clipSetId)
, m_lowClipId(lowClipId)
, m_mediumClipId(medClipId)
, m_highClipId(highClipId)
, m_deltaPerSecond( SWEEP_TURN_RATE )
, m_fBlendInDuration(fBlendInDuration)
, m_turnSpeedMultiplier(turnRate)
, m_bLeaveNetworkHelper(false)
{
	SetInternalTaskType(CTaskTypes::TASK_GENERAL_SWEEP);
}


//////////////////////////////////////////////////////////////////////////
// Reference by ClipDict

CTaskGeneralSweep::CTaskGeneralSweep(const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const CEntity *pEntityToTrack, float turnRate, float fBlendInDuration )
: m_Stop(false)
, m_TrackMode(TRACKING_MODE_ENTITY)
, m_TrackEntity(pEntityToTrack)
, m_TrackPoint(Vector3::ZeroType)
, m_RunTime(runTime)
, m_clipDictId(clipDictId)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_lowClipId(lowClipId)
, m_mediumClipId(medClipId)
, m_highClipId(highClipId)
, m_deltaPerSecond( SWEEP_TURN_RATE )
, m_fBlendInDuration(fBlendInDuration)
, m_turnSpeedMultiplier(turnRate)
, m_bLeaveNetworkHelper(false)
{
	SetInternalTaskType(CTaskTypes::TASK_GENERAL_SWEEP);
}

CTaskGeneralSweep::CTaskGeneralSweep(const int clipDictId, const fwMvClipId lowClipId, const fwMvClipId medClipId, const fwMvClipId highClipId, int runTime, const Vector3 *pPointToTrack, float turnRate, float fBlendInDuration )
: m_Stop(false)
, m_TrackMode(TRACKING_MODE_POINT)
, m_TrackEntity(NULL)
, m_TrackPoint(*pPointToTrack)
, m_RunTime(runTime)
, m_clipDictId(clipDictId)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_lowClipId(lowClipId)
, m_mediumClipId(medClipId)
, m_highClipId(highClipId)
, m_deltaPerSecond( SWEEP_TURN_RATE )
, m_fBlendInDuration(fBlendInDuration)
, m_turnSpeedMultiplier(turnRate)
, m_bLeaveNetworkHelper(false)
{
	SetInternalTaskType(CTaskTypes::TASK_GENERAL_SWEEP);
}

//////////////////////////////////////////////////////////////////////////

CTaskGeneralSweep::~CTaskGeneralSweep()
{

}

//////////////////////////////////////////////////////////////////////////

const crClip * CTaskGeneralSweep::GetClipFromSetOrDict( const fwMvClipId &clipId )
{
	if( m_clipSetId != CLIP_SET_ID_INVALID )
	{
		return fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, clipId);
	}
	else
	{
		return fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictId, clipId);
	}
}


//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskGeneralSweep::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter();
			FSM_OnUpdate
				return StateInitial_OnUpdate();

		FSM_State(State_DoSweep)
			FSM_OnEnter
				return StateDoSweep_OnEnter(); 
			FSM_OnUpdate
				return StateDoSweep_OnUpdate();
			
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

void	CTaskGeneralSweep::CleanUp()
{
	if(m_networkHelper.GetNetworkPlayer())
	{
		if(!m_bLeaveNetworkHelper)
			GetPed()->GetMovePed().ClearTaskNetwork(m_networkHelper, SLOW_BLEND_DURATION);

		m_networkHelper.ReleaseNetworkPlayer();
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskGeneralSweep::StateInitial_OnEnter()
{
	// request the move network
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskGeneralSweep);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskGeneralSweep::StateInitial_OnUpdate()
{
bool	clipsLoaded = false;

	if( m_clipSetId != CLIP_SET_ID_INVALID )
	{
		clipsLoaded = m_ClipSetRequestHelper.Request(m_clipSetId);
	}
	else
	{
		clipsLoaded = m_ClipRequestHelper.RequestClipsByIndex(m_clipDictId);
	}

	if (clipsLoaded && m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskGeneralSweep))
	{
		float	minYaw, maxYaw;

		// Create the network player
		m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskGeneralSweep);
//		m_networkHelper.SetClipSet(m_clipSetId);	// Don't set an clip set, it's not used now as they're set manually

		// Attach it to an empty precedence slot in fwMove
		Assert(GetPed()->GetAnimDirector());
		GetPed()->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), m_fBlendInDuration);

		// Setup the clips
		// Low Clip
		const crClip *pClip = GetClipFromSetOrDict(m_lowClipId);
		Assertf(pClip, "Anim %s does not exist in Dictionary or Set.\n", m_lowClipId.GetCStr());
		m_networkHelper.SetClip(pClip, ms_LowClipId);
		
		// Discard the yaws, we only want to use yaw from the mid clip (though would be good for error checking)
		if( !Verifyf( GetYawMinMaxAndPitch(pClip, minYaw, maxYaw, m_minPitch), "Unable find appropriate tags for Min/MaxYaw/Pitch from the sweep clip %s.\n", pClip->GetName() ) )
		{

#ifdef SET_DEFAULTS_ON_ERROR
			m_minPitch = DtoR * -45.0f;		// 45 degree's down
#else
			return FSM_Quit;
#endif //SET_DEFAULTS_ON_ERROR

		}

		//Medium Clip
		pClip = GetClipFromSetOrDict(m_mediumClipId);
		Assertf(pClip, "Anim %s does not exist in Dictionary or Set.\n", m_mediumClipId.GetCStr());
		m_networkHelper.SetClip(pClip, ms_MediumClipId);

		if( !Verifyf( GetYawMinMaxAndPitch(pClip, m_minYaw, m_maxYaw, m_medPitch), "Unable find appropriate tags for Min/MaxYaw/Pitch from the sweep clip %s.\n", pClip->GetName() ) )
		{
#ifdef SET_DEFAULTS_ON_ERROR
			m_medPitch = DtoR * 0.0f;		// straight ahead
			m_minYaw = DtoR * -90.0f;
			m_maxYaw = DtoR * 90.0f;
#else
			return FSM_Quit;
#endif //SET_DEFAULTS_ON_ERROR
		}

		//High Clip
		pClip = GetClipFromSetOrDict(m_highClipId);
		Assertf(pClip, "Anim %s does not exist in Dictionary or Set.\n", m_highClipId.GetCStr());
		m_networkHelper.SetClip(pClip, ms_HighClipId);

		// Discard the yaws, we only want to use yaw from the mid clip (though would be good for error checking)
		if( !Verifyf( GetYawMinMaxAndPitch(pClip, minYaw, maxYaw, m_maxPitch), "Unable find appropriate tags for Min/MaxYaw/Pitch from the sweep clip %s.\n", pClip->GetName() ) )
		{
#ifdef SET_DEFAULTS_ON_ERROR
			m_maxPitch = DtoR * 45.0f;		// 45 degree's up
#else
			return FSM_Quit;
#endif //SET_DEFAULTS_ON_ERROR
		}

		// Move into the enter sweep state
		SetState(State_DoSweep);

	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskGeneralSweep::StateDoSweep_OnEnter()
{
	CalculateDesiredPitchAndYaw();
	m_CurrentPitch = m_DesiredPitch;
	m_CurrentYaw = m_DesiredYaw;

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskGeneralSweep::StateDoSweep_OnUpdate()
{
	CalculateDesiredPitchAndYaw();

	// Interpolate Current Pitch/Yaw to Desired Pitch/Yaw over time here
	UpdatePitchAndYaw();

	// Get pitch and Yaw as a MoVE Param
	// I don't think we can guarantee that pitch will always be uniform, we may point up more than down. 
	//	(Must be clamped since we could have large values if the desired is outside the range)
	float pitchValue = Clamp( GetInterpolatedFloatParam(m_CurrentPitch, m_minPitch, m_medPitch, m_maxPitch ), 0.0f, 1.0f);
	float yawValue = FPRangeClamp( m_CurrentYaw, m_minYaw, m_maxYaw );

	// Reflect in MoVE network
	m_networkHelper.SetFloat(ms_PitchId, pitchValue);
	m_networkHelper.SetFloat(ms_YawId, yawValue);

	// Should we be thinking of stopping?
	if( m_RunTime > 0 )
	{
		float timeRunning = GetTimeRunning();
		if((timeRunning * 1000) > m_RunTime)
			m_Stop = true;
	}
	if( m_Stop )
		return FSM_Quit;

	return FSM_Continue;
}


bool CTaskGeneralSweep::GetYawMinMaxAndPitch( const crClip *pClipClip, float &minYaw, float &maxYaw, float &pitch )
{
	if(pClipClip->HasTags())
	{
		CClipEventTags::CTagIterator<CClipEventTags::CSweepMarkerTag> it(*pClipClip->GetTags(), CClipEventTags::SweepMarker);
		if(*it)
		{
			const CClipEventTags::CSweepMarkerTag* pTag = (*it)->GetData();

			minYaw = DtoR * pTag->GetMinYaw();
			maxYaw = DtoR * pTag->GetMaxYaw();
			pitch = DtoR * pTag->GetPitch();
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void	CTaskGeneralSweep::CalculateDesiredPitchAndYaw()
{
Vector3	trackingPoint;

	switch( m_TrackMode )
	{
	case TRACKING_MODE_ENTITY:
		if(!m_TrackEntity)
		{
			// The entity is gone, stop pointing at it
			m_Stop = true;
			return;
		}	
		else
		{
			// Get the point to track
			trackingPoint = VEC3V_TO_VECTOR3(m_TrackEntity->GetTransform().GetPosition());
		}
		break;
	case TRACKING_MODE_POINT:
		trackingPoint = m_TrackPoint;
		break;
	}

	// Calc the new desired pitch and yaw
	CPed *pPed = GetPed();

	Vector3 dirVec = trackingPoint - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	Vector3 worldSpaceChestOffset(0.f,0.0f,0.0f);
	if(pPed->GetIsInVehicle())
	{
		worldSpaceChestOffset = VEC3V_TO_VECTOR3(pPed->GetTransform().GetUp()) * 0.4f;
	}

	dirVec -= worldSpaceChestOffset;

	Vector3 invDir = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(dirVec)));

	m_DesiredYaw = -rage::Atan2f(invDir.x, invDir.y);
	float length = rage::Sqrtf( invDir.x*invDir.x + invDir.y*invDir.y );
	m_DesiredPitch = -rage::Atan2f( invDir.z, length );
}

//////////////////////////////////////////////////////////////////////////

// A basic linear lerp, change this if it's a bit "linear"
void	CTaskGeneralSweep::UpdatePitchAndYaw()
{
	// interpolate by the biggest delta and fixup the other axis
	if( abs(m_DesiredPitch - m_CurrentPitch) > abs(m_DesiredYaw - m_CurrentYaw) )
	{
		// Pitch is bigger
		float delta = UpdateAngle( m_DesiredPitch, m_CurrentPitch);
		// Fixup yaw based on delta
		m_CurrentYaw = m_CurrentYaw + ((m_DesiredYaw - m_CurrentYaw) * delta);
	}
	else
	{
		// Yaw is bigger
		float delta = UpdateAngle( m_DesiredYaw, m_CurrentYaw);
		// Fixup pitch based on delta
		m_CurrentPitch = m_CurrentPitch + ((m_DesiredPitch - m_CurrentPitch) * delta);
	}
}

float	CTaskGeneralSweep::UpdateAngle( float &desiredAngle, float &currentAngle )
{
	float	currentAngleStore = currentAngle ; 

	if( currentAngle != desiredAngle )
	{
		float	deltaAngle = desiredAngle - currentAngle;
		float	delta;
		if( deltaAngle < 0.0f )
		{
			delta = -m_deltaPerSecond;
		}
		else
		{
			delta = m_deltaPerSecond;
		}

		delta *= fwTimer::GetTimeStep() * m_turnSpeedMultiplier;

		if( abs(deltaAngle) <= abs(delta) )
		{
			currentAngle = desiredAngle;
		}
		else
		{
			currentAngle += delta;
			// Return the percent through we got (0->1)
			return FPRangeFast(abs(currentAngle - currentAngleStore), 0, abs(desiredAngle - currentAngleStore));
		}
	}
	return 1.0f;	// We've arrived
}

CTaskInfo *CTaskGeneralSweep::CreateQueriableState() const
{
	// -1 is default, if not -1 we should grab time remaining if the task has kicked off
	s32 nRunTime = m_RunTime;
	if(nRunTime != -1)
	{
		s32 nTimeRunning = static_cast<s32>(GetTimeRunning()) * 1000;
		nRunTime -= nTimeRunning;
	}

	switch(m_TrackMode)
	{
	case CTaskGeneralSweep::TRACKING_MODE_ENTITY:
		if(m_clipSetId != CLIP_SET_ID_INVALID)
			return rage_new CClonedGeneralSweepInfo(GetState(), m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, m_TrackEntity.Get(), m_deltaPerSecond, m_turnSpeedMultiplier, m_fBlendInDuration);
		else
			return rage_new CClonedGeneralSweepInfo(GetState(), m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, m_TrackEntity.Get(), m_deltaPerSecond, m_turnSpeedMultiplier, m_fBlendInDuration);
	case CTaskGeneralSweep::TRACKING_MODE_POINT:
		if(m_clipSetId != CLIP_SET_ID_INVALID)
			return rage_new CClonedGeneralSweepInfo(GetState(), m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, &m_TrackPoint, m_deltaPerSecond, m_turnSpeedMultiplier, m_fBlendInDuration);
		else
			return rage_new CClonedGeneralSweepInfo(GetState(), m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, &m_TrackPoint, m_deltaPerSecond, m_turnSpeedMultiplier, m_fBlendInDuration);
	default:
		Assertf(0, "CTaskGeneralSweep::CreateQueriableState: Invalid Tracking Mode!");
		return NULL;
	}
}

void CTaskGeneralSweep::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_GENERAL_SWEEP);
	CClonedGeneralSweepInfo* pSweepInfo = static_cast<CClonedGeneralSweepInfo*>(pTaskInfo);

	m_TrackMode			= static_cast<TRACKING_MODE>(pSweepInfo->GetTrackingMode());
	m_TrackEntity		= pSweepInfo->GetTrackEntity();
	m_TrackPoint		= pSweepInfo->GetTrackPoint();

	m_RunTime			= pSweepInfo->GetRunTime();

	m_clipDictId		= pSweepInfo->GetClipDictID();
	m_clipSetId			= pSweepInfo->GetClipSetID();
	m_lowClipId			= pSweepInfo->GetLowClipID();
	m_mediumClipId		= pSweepInfo->GetMediumClipID();
	m_highClipId		= pSweepInfo->GetHighClipID();

	m_deltaPerSecond	= pSweepInfo->GetDeltaPerSecond();
	m_turnSpeedMultiplier = pSweepInfo->GetTurnSpeedMultiplier();
	m_fBlendInDuration = pSweepInfo->GetBlendInDuration();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskGeneralSweep::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTaskFSMClone* CTaskGeneralSweep::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	// -1 is default, if not -1 we should grab time remaining if the task has kicked off
	s32 nRunTime = m_RunTime;
	if(nRunTime != -1)
	{
		s32 nTimeRunning = static_cast<s32>(GetTimeRunning()) * 1000;
		nRunTime -= nTimeRunning;
	}

	CTaskGeneralSweep* pTask = NULL; 

	switch(m_TrackMode)
	{
	case CTaskGeneralSweep::TRACKING_MODE_ENTITY:
		if(m_clipSetId != CLIP_SET_ID_INVALID)
			pTask = rage_new CTaskGeneralSweep(m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, m_TrackEntity);
		else
			pTask = rage_new CTaskGeneralSweep(m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, m_TrackEntity);
		break;
	case CTaskGeneralSweep::TRACKING_MODE_POINT:
		if(m_clipSetId != CLIP_SET_ID_INVALID)
			pTask = rage_new CTaskGeneralSweep(m_clipSetId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, &m_TrackPoint);
		else
			pTask = rage_new CTaskGeneralSweep(m_clipDictId, m_lowClipId, m_mediumClipId, m_highClipId, nRunTime, &m_TrackPoint);
		break;
	default:
		Assertf(0, "CTaskGeneralSweep::CreateTaskForClonePed: Invalid Tracking Mode!");
		break;
	}

	if(pTask)
	{
		pTask->SetUpdateAnglePerSecond(m_deltaPerSecond);
	}

	// keep the network helper
	m_bLeaveNetworkHelper = true; 

	return pTask;
}

CTaskFSMClone* CTaskGeneralSweep::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

CTask::FSM_Return CTaskGeneralSweep::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iState != State_Finish)
	{
		if(iEvent == OnUpdate)
		{
			// if the network state is telling us to finish...
			s32 iStateFromNetwork = GetStateFromNetwork();
			if(iStateFromNetwork != GetState() && iStateFromNetwork == State_Finish)
			{
				SetState(iStateFromNetwork);
			}
		}
	}

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter();
			FSM_OnUpdate
				return StateInitial_OnUpdate();

		FSM_State(State_DoSweep)
			FSM_OnEnter
				return StateDoSweep_OnEnter(); 
			FSM_OnUpdate
				return StateDoSweep_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL

const char * CTaskGeneralSweep::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Initial: return "Initial";
		case State_DoSweep: return "DoSweep";
		case State_Finish:  return "Finish";
		default: { taskAssert(false); return "Unknown"; }
	}
}

void CTaskGeneralSweep::Debug() const
{

}

#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
