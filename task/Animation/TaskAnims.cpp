// Class header
#include "Task/Animation/TaskAnims.h"

// Rage headers
#include "cranimation/animtolerance.h"
#include "crskeleton/skeleton.h"
#include "crmetadata/properties.h"
#include "crmetadata/property.h"
#include "parser/manager.h"
#include "pharticulated/articulatedcollider.h"

// Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/animhelpers.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentcreature.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwmaths/Angle.h"
#include "fwmaths/vector.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/framefilterdictionarystore.h"

// Game headers
#include "Animation/AnimBones.h"
#include "Animation/AnimDefines.h"
#include "Animation/debug/AnimViewer.h"
#include "animation/MoveObject.h"
#include "animation/MovePed.h"
#include "animation/MoveVehicle.h"
#include "animation/EventTags.h"
#include "Debug/DebugScene.h"
#include "event/EventDamage.h"
#include "network/general/NetworkSynchronisedScene.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "physics/gtaInst.h"
#include "scene/world/GameWorld.h"
#include "script/commands_ped.h"
#include "Streaming/Streaming.h"
#include "Task/General/TaskBasic.h"
#include "Task/Physics/TaskNmBalance.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "vehicleAI/VehicleAILodManager.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/train.h"

AI_OPTIMISATIONS()
ANIM_OPTIMISATIONS()

#if __BANK
#define DEBUG_NET_SYNCED_SCENE(s, ...) if(NetworkInterface::IsGameInProgress()) { gnetDebug1(s, __VA_ARGS__); }
#else
#define DEBUG_NET_SYNCED_SCENE(s, ...)
#endif // __BANK

//#include "system/findsize.h"
//FindSize(CClonedScriptClipInfo); //48

//-------------------------------------------------------------------------------------------------
// 
// CTaskClip
// 
//-------------------------------------------------------------------------------------------------
CTaskClip::CTaskClip()
: m_iTaskFlags(0),
m_fBlendDelta(NORMAL_BLEND_IN_DELTA),
m_fBlendOutDelta(NORMAL_BLEND_OUT_DELTA),
m_fRate(1.0f),
m_fStartPhase(0.0f),
//m_iQuitEvent(0),
m_iDuration(-1),
m_iClipFlags(0),
m_iPriority(AP_MEDIUM),
m_iBoneMask(BONEMASK_ALL),
m_clipSetId(CLIP_SET_ID_INVALID),
m_clipId(CLIP_ID_INVALID),
m_clipDictIndex(-1),
m_clipHashKey(atHashWithStringNotFinal::Null()),
m_physicsSettings(0),
m_initialPosition(VEC3_ZERO),
m_initialOrientation(Quaternion::sm_I),
m_pClip(NULL)
{
}

CTaskClip::~CTaskClip()
{
	// Ensure the callback is cleared before deleting
	if(GetClipHelper())
	{
		GetClipHelper()->SetFlag(APF_ISBLENDAUTOREMOVE);
		StopClip(m_fBlendOutDelta, GetStopClipFlags());
	}

	SetReferenceClip(NULL);
}

void CTaskClip::PrintDebugInfo()
{
	if (fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, m_clipId))
	{
		taskAssertf(0, "Clip not found %s\n", PrintClipInfo(m_clipSetId, m_clipId));
	}
	else
	{
#if __ASSERT
		const char *clipName = m_clipHashKey.TryGetCStr() ? m_clipHashKey.TryGetCStr() : "(null)";
#endif

		if (m_clipDictIndex!= -1)
		{
			const strStreamingObjectName pClipDictName = fwAnimManager::GetName(strLocalIndex(m_clipDictIndex));
			if (pClipDictName.IsNotNull())
			{
				taskAssertf(0, "Missing clip (clip dict name, clip dict index, clip hash key): %s, %d, %s %u\n", pClipDictName.GetCStr(), m_clipDictIndex, clipName, m_clipHashKey.GetHash());
			}
			else
			{
				taskAssertf(0, "Missing clip (clip dict index, clip hash key): %d, %s %u\n", m_clipDictIndex, clipName, m_clipHashKey.GetHash());
			}
		}
		else
		{
			taskAssertf(0, "Missing clip (clip dict index, clip hash key): %d, %s %u\n", m_clipDictIndex, clipName, m_clipHashKey.GetHash());
		}
	}
}


bool CTaskClip::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	// special clip that has been set by the script, and can only be aborted by the script
	// or by itself (or directly by another task, so not any events is what I mean, ok.)
	if(!(CTask::ABORT_PRIORITY_IMMEDIATE==iPriority) 
		&& IsTaskFlagSet(ATF_DONT_INTERRUPT) 
		&& pEvent 
		&& !((CEvent*)pEvent)->RequiresAbortForRagdoll() 
		&& ((CEvent*)pEvent)->GetEventType()!= EVENT_SCRIPT_COMMAND
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_GIVE_PED_TASK) 
		// stealth kills use the give ped task, this ensures peds who are playing clips will abort correctly
	{
		return false;
	}

	if (GetSubTask() && !GetSubTask()->MakeAbortable(iPriority, pEvent))
	{
		return false;
	}

	if(CTask::ABORT_PRIORITY_URGENT==iPriority || CTask::ABORT_PRIORITY_IMMEDIATE==iPriority || (m_iClipFlags & APF_ISBLENDAUTOREMOVE))
	{
		// Base class
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}

void CTaskClip::CleanUp()
{
	if ( CPhysical * pPhysical = GetPhysical() )
	{
		RestorePhysicsState(pPhysical);
	}

	StopClip(m_fBlendOutDelta, GetStopClipFlags());
}

void CTaskClip::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPhysical *pPhysical = GetPhysical(); //Get the physical ptr.

	// Clear the callback to make the task abortable.
	if(GetClipHelper()) 
	{
		float fBlendOutDelta = IsTaskFlagSet(ATF_FAST_BLEND_OUT_ON_ABORT) ? FAST_BLEND_OUT_DELTA : SLOW_BLEND_OUT_DELTA;
		if(CTask::ABORT_PRIORITY_IMMEDIATE==iPriority)
		{
			fBlendOutDelta = INSTANT_BLEND_OUT_DELTA;
		}
		GetClipHelper()->UnsetFlag(APF_ISFINISHAUTOREMOVE);
		GetClipHelper()->SetFlag(APF_ISBLENDAUTOREMOVE);
		StopClip(fBlendOutDelta, GetStopClipFlags());
		SetTaskFlag(ATF_TASK_FINISHED);
	}

	RestorePhysicsState(pPhysical);

	// Base class
	CTask::DoAbort(iPriority, pEvent);
}

void CTaskClip::ApplyMoverTrackFixup(CPhysical * pPhysical)
{
	float phase = 0.0f;
	
	if (IsTaskFlagSet(ATF_FIRST_UPDATE))
	{
		phase = m_fStartPhase;
	}
	else if (GetClipHelper())
	{
		phase = GetClipHelper()->GetPhase();
	}

	TUNE_GROUP_BOOL(CLIP_ERROR, bApplyMoverTrackFixup, true);
	if (bApplyMoverTrackFixup)
	{
		Matrix34 deltaMatrix(M34_IDENTITY);
		Matrix34 startMatrix(M34_IDENTITY);

		if (GetClipHelper())
		{
			// Get the calculated mover track offset
			if(GetClipHelper()->GetClip())
			{
				fwAnimHelpers::GetMoverTrackMatrixDelta(*GetClipHelper()->GetClip(), 0.f, phase, deltaMatrix);
			}

			Vector3 initialPosition = m_initialPosition;
			Quaternion initialOrientation = m_initialOrientation;
			if (IsTaskFlagSet(ATF_EXTRACT_INITIAL_OFFSET))
			{
				if(GetClipHelper()->GetClip())
				{
					fwAnimHelpers::ApplyInitialOffsetFromClip(*GetClipHelper()->GetClip(), initialPosition, initialOrientation);
				}
			}

			// Get the initial transform
			startMatrix.d = initialPosition;
			startMatrix.FromQuaternion(initialOrientation);
		
			// Transform original transform by mover offset
			startMatrix.DotFromLeft(deltaMatrix);
			
			if (IsTaskFlagSet(ATF_FIRST_UPDATE))
			{	
				pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
				pPhysical->GetPortalTracker()->Update(startMatrix.d);
			}
			// apply to entity
			pPhysical->SetMatrix(startMatrix);
		}
	}
};

bool CTaskClip::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
{
	CPed * pPed = GetPhysical()->GetIsTypePed() ? static_cast<CPed*>(GetPhysical()) : NULL;

	if(IsTaskFlagSet(ATF_OVERRIDE_PHYSICS) && pPed)
	{
		pPed->SetDesiredVelocity(VEC3_ZERO);
		pPed->SetDesiredAngularVelocity(VEC3_ZERO);
		return false; 
	}
	return true; 
}

void CTaskClip::UpdateClipRootTransform(const Matrix34 &transform_)
{
	Matrix34 transform = transform_;

	m_initialPosition = transform.d;
	m_initialOrientation.FromMatrix34(transform);
};

void CTaskClip::ApplyPhysicsStateChanges(CPhysical* pPhysical, const crClip* pClip )
{
	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;
	if(pPhysical->GetIsAttached())
	{
		// Don't effect the entity if it is attached
		// Shouldn't be running an clip with these flags on attached entities
		taskAssertf(!IsTaskFlagSet(ATF_OVERRIDE_PHYSICS), "CTaskClip: Do not run with OVERRIDE_PHYSICS on attached entities!");			
		taskAssertf(!IsTaskFlagSet(ATF_TURN_OFF_COLLISION), "CTaskClip: Do not run with TURN_OFF_COLLISION on attached entities!");			
		taskAssertf(!IsTaskFlagSet(ATF_IGNORE_GRAVITY), "CTaskClip: Do not run with IGNORE_GRAVITY on attached entities!");

		UnsetTaskFlag(ATF_OVERRIDE_PHYSICS);
		UnsetTaskFlag(ATF_TURN_OFF_COLLISION);
		UnsetTaskFlag(ATF_IGNORE_GRAVITY);
	}

	// Store the current physics settings for the entity 
	// and apply any changes requested by the task flags

	if (IsTaskFlagSet(ATF_OVERRIDE_PHYSICS) ||
		IsTaskFlagSet(ATF_TURN_OFF_COLLISION) ||
		IsTaskFlagSet(ATF_IGNORE_GRAVITY)			
		)
	{
		//store the current state
		if (pPed && pPed->GetUseExtractedZ())
		{
			m_physicsSettings |= PS_USE_EXTRACTED_Z;
		}
		if (pPhysical->IsCollisionEnabled())
		{
			m_physicsSettings |= PS_USES_COLLISION;
		}

		//Disable gravity if requested
		if (IsTaskFlagSet(ATF_IGNORE_GRAVITY))
		{
			if (Verifyf(pPed, "ATF_IGNORE_GRAVITY on non ped entity"))
				pPed->SetUseExtractedZ(true);
		}

		//Disable collision detection if requested
		if (IsTaskFlagSet(ATF_TURN_OFF_COLLISION))
		{
			pPhysical->DisableCollision();
		}

		//Disable physics if requested
		if (IsTaskFlagSet(ATF_OVERRIDE_PHYSICS))
		{
			//if a start position has not been provided by TASK_PLAY_CLIP_ADVANCED, get the current position
			if ( !(m_physicsSettings & PS_APPLY_REPOSITION)) 
			{
				m_initialPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());
			}
			//if a start orientation has not been provided by TASK_PLAY_CLIP_ADVANCED, get the current orientation
			if ( !(m_physicsSettings & PS_APPLY_REORIENTATION)) 
			{
				m_initialOrientation.FromMatrix34(MAT34V_TO_MATRIX34(pPhysical->GetMatrix()));
			}
		}
	}
	else
	{
		// Force a physics update through to make sure ped ground position is up to date on start
		if (pPed)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true); 
		}
	}

	//reposition and reorient the entity if requested by the task constructor
	if (m_physicsSettings & PS_APPLY_REPOSITION || m_physicsSettings & PS_APPLY_REORIENTATION)
	{
		Matrix34 entityMatrix(M34_IDENTITY);

		Vector3 initialPosition = m_initialPosition;
		Quaternion initialOrientation = m_initialOrientation;

		if (IsTaskFlagSet(ATF_EXTRACT_INITIAL_OFFSET))
		{
			// apply the authored offset from the clip
			if(pClip)
			{
				fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, initialPosition, initialOrientation);
			}
		}

		if ( m_physicsSettings & PS_APPLY_REPOSITION )
		{
			entityMatrix.d = initialPosition;
		}
		if ( m_physicsSettings & PS_APPLY_REORIENTATION )
		{
			entityMatrix.FromQuaternion(initialOrientation);
		}

		pPhysical->SetMatrix(entityMatrix);
		pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
	}
}

void CTaskClip::RestorePhysicsState(CPhysical* pPhysical)
{
	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;

	//Restore the peds stored physics state
	if (IsTaskFlagSet(ATF_OVERRIDE_PHYSICS) ||  //?? Can these flags be set as the task is running? If so we could get into trouble here...
		IsTaskFlagSet(ATF_TURN_OFF_COLLISION) ||
		IsTaskFlagSet(ATF_IGNORE_GRAVITY)			
		)
	{
		if (m_physicsSettings & PS_USE_EXTRACTED_Z)
		{
			if (Verifyf(pPed, "PS_USE_EXTRACTED_Z not supported on dynamicentity clip"))
				pPed->SetUseExtractedZ(true);
		}
		else
		{
			if (pPed)
				pPed->SetUseExtractedZ(false);
		}

		if (m_physicsSettings & PS_USES_COLLISION)
		{
			pPhysical->EnableCollision();
		}
		else
		{
			pPhysical->DisableCollision();
		}

		if (IsTaskFlagSet(ATF_OVERRIDE_PHYSICS) && (!pPed || pPed->GetRagdollState()!= RAGDOLL_STATE_PHYS))
		{
			//we need to update the heading of the entity to stop it rotating needlessly at the end of the clip
			Vector3 eulerRotation;
			Matrix34 m = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			m.ToEulersXYZ(eulerRotation);
			
			pPhysical->SetHeading(eulerRotation.z);
			if (pPed)
			{
				pPed->SetDesiredHeading(eulerRotation.z);

				pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, false);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskClip::SetReferenceClip(const crClip* pClip)
{
	if (pClip)
	{
		pClip->AddRef();
	}

	if (m_pClip)
	{
		m_pClip->Release();
		m_pClip = NULL;
	}

	m_pClip = pClip;
}

//////////////////////////////////////////////////////////////////////////

int CTaskClip::GetRemainingDuration()
{
	// -1 is default, if not -1 we should grab time remaining if the task has kicked off
	s32 iDuration = m_iDuration;
	if(iDuration != -1)
	{
		if(m_timer.IsSet())
			iDuration = m_timer.GetTimeLeft();
		else
			iDuration = m_iDuration;
	}

	return iDuration;
}

// Returns the flags needed to stop the clip.
u32 CTaskClip::GetStopClipFlags() const
{
	u32 uFlags = 0;

	if (IsTaskFlagSet(ATF_IGNORE_MOVER_BLEND_ROTATION))
	{
		uFlags |= CMovePed::Task_IgnoreMoverBlend;
	}

	return uFlags;
}

///////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
// 
// CTaskRunClip
// 
//-------------------------------------------------------------------------------------------------

CTaskRunClip::CTaskRunClip(
						   const fwMvClipSetId &clipSetId,
						   const fwMvClipId &clipId,
						   const float fBlendDelta,
						   const float fBlendOutDelta,
						   const int iDuration,
						   const s32 iTaskFlags,
						   const CTaskTypes::eTaskType taskType)
{
	m_clipSetId = clipSetId;
	m_clipId = clipId;
	taskAssert(m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID);
	taskAssertf(fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, m_clipId), "Missing clip %s\n", PrintClipInfo(m_clipSetId, m_clipId));

	m_clipHashKey = clipId; //fwClipSetManager::GetClipId(clipSetId, clipId);
	m_clipDictIndex = fwClipSetManager::GetClipDictionaryIndex(clipSetId, clipId);
	taskAssertf((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()), "Invalid Clip Dict for clip set '%s'", clipSetId.GetCStr());
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey);
	SetReferenceClip(pClip);
	taskAssertf(pClip, "Missing clip (clip dict index, clip hash key): %d, %s %u\n", m_clipDictIndex, m_clipHashKey.TryGetCStr() ? m_clipHashKey.TryGetCStr() : "(null)", m_clipHashKey.GetHash());

	m_iBoneMask = BONEMASK_ALL;

	const fwClipItem *pClipItem = fwClipSetManager::GetClipItem(m_clipSetId, m_clipId);
	taskAssertf(pClipItem, "Clip Item not found in clipset %s\n", PrintClipInfo(m_clipSetId, m_clipId));
	if (pClipItem)
	{
		m_iClipFlags = GetClipItemFlags(pClipItem);
		m_iBoneMask = GetClipItemBoneMask(pClipItem).GetHash();
		m_iPriority = GetClipItemPriority(pClipItem);
	}

	m_eTaskType = taskType;
	m_fBlendDelta = fBlendDelta;
	m_fBlendOutDelta = fBlendOutDelta;
	m_iTaskFlags = iTaskFlags;
	m_iDuration = iDuration;
	m_physicsSettings = 0;
	SetInternalTaskType(CTaskTypes::TASK_RUN_CLIP);
}

CTaskRunClip::CTaskRunClip(const int clipDictIndex, 
						   const u32 clipHashKey, 
						   const float fBlendDelta, 
						   const s32 animPriority, 
						   const s32 clipFlags,
						   const u32 boneMask,
						   const int iDuration,
						   const s32 iTaskFlags,
						   const CTaskTypes::eTaskType taskType)
						   : CTaskClip()
{
	m_clipDictIndex = clipDictIndex;
	m_clipHashKey = clipHashKey;
	taskAssert((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()));
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey);
	SetReferenceClip(pClip);
	taskAssertf(pClip, "Missing clip (clip dict index, clip hash key): %d, %s %u\n", m_clipDictIndex, m_clipHashKey.TryGetCStr() ? m_clipHashKey.TryGetCStr() : "(null)", m_clipHashKey.GetHash());

	m_fBlendDelta = fBlendDelta;
	m_iPriority = animPriority;
	m_iClipFlags = clipFlags;
	m_iBoneMask= boneMask;
	m_iTaskFlags = iTaskFlags;
	m_iDuration = iDuration;
	m_eTaskType = taskType;
	m_physicsSettings = 0;
	SetInternalTaskType(CTaskTypes::TASK_RUN_CLIP);
}

aiTask* CTaskRunClip::Copy() const
{
	CTaskRunClip* pTask = rage_new CTaskRunClip(
		m_clipDictIndex, 
		m_clipHashKey, 
		m_fBlendDelta, 
		m_iPriority, 
		m_iClipFlags,
		m_iBoneMask,
		m_iDuration,
		m_iTaskFlags);

	pTask->m_eTaskType = m_eTaskType;
	pTask->m_fBlendOutDelta = m_fBlendOutDelta;
	pTask->m_fRate = m_fRate;
	pTask->m_fStartPhase = m_fStartPhase;
	//pTask->m_iQuitEvent = m_iQuitEvent; 
	pTask->m_timer = m_timer;
	pTask->m_clipSetId = m_clipSetId;
	pTask->m_clipId = m_clipId;
	//pTask->m_iQuitEvent = m_iQuitEvent;

	pTask->m_physicsSettings = m_physicsSettings;
	
	return pTask;
}

#if !__FINAL
const char * CTaskRunClip::GetStaticStateName( s32 iState )
{
	taskAssert(iState>=0&&iState<=State_Finish);
	static const char* aRunClipStateNames[] = 
	{
		"State_Start",
		"State_PlayingAnim",
		"State_Finish"
	};

	return aRunClipStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskRunClip::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPhysical *pPhysical = GetPhysical(); //Get the entity ptr.

	FSM_Begin
		// Entry point
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPhysical);

		// Play an clip
		FSM_State(State_PlayingClip)
			FSM_OnEnter
				// Try and start the clip
				StartClip(pPhysical);
			FSM_OnUpdate
				return PlayingClip_OnUpdate(pPhysical);
			FSM_OnExit
				PlayingClip_OnExit(pPhysical);

		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskRunClip::Start_OnUpdate( CPhysical* UNUSED_PARAM(pPhysical) )
{
	const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey);
	if(pClip)
	{
		SetState(State_PlayingClip);
		return FSM_Continue;
	}
	else
	{
		PrintDebugInfo();
		return FSM_Quit;
	}
}

CTask::FSM_Return CTaskClip::ProcessPreFSM()
{
	if (GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions, true);
	}

	if (GetPhysical() && IsTaskFlagSet(ATF_USE_KINEMATIC_PHYSICS))
	{
		GetPhysical()->SetShouldUseKinematicPhysics(true);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskRunClip::PlayingClip_OnUpdate( CPhysical* UNUSED_PARAM(pPhysical) )
{
	// Has the clip terminated?
	if(GetIsFlagSet(aiTaskFlags::AnimFinished))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Is the timer being used if so has the timer finished?
	if(m_timer.IsSet() && m_timer.IsOutOfTime())
	{
		SetState(State_Finish);
		StopClip(m_fBlendOutDelta, GetStopClipFlags());
		SetTaskFlag(ATF_TASK_FINISHED);
		return FSM_Continue;
	}

	if (IsTaskFlagSet(ATF_OVERRIDE_PHYSICS) && GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
	}

	// Is there a quit event set up ?
	// And is the clip valid ?
	// And has the event has just occurred ?
	//if( m_iQuitEvent != 0
	//	&& GetClipHelper()
	//	&& GetClipHelper()->IsEvent(m_iQuitEvent) )
	//{
	//	// Stop the clip
	//	StopClip(m_fBlendOutDelta);
	//	SetState(State_Finish);
	//	return FSM_Continue;
	//}
	return FSM_Continue;
}

void CTaskRunClip::PlayingClip_OnExit(CPhysical* pPhysical)
{
	RestorePhysicsState(pPhysical);
	StopClip(m_fBlendOutDelta, GetStopClipFlags());
}

void CTaskRunClip::StartClip(CPhysical* pPhysical)
{
	// Is this clip running for a specific length of time?
	if(m_iDuration>=0)
	{
		m_timer.Set(fwTimer::GetTimeInMilliseconds(),m_iDuration);
	}

	// If this is a secondary clip, use the secondary task blend helper
	if (IsTaskFlagSet(ATF_SECONDARY))
	{
		m_iClipFlags |= APF_USE_SECONDARY_SLOT;
	}

	// APCB_DELETE = callback when the phase has finished and the clip has blended out
	// APCB_FINISH = callback when the phase has finished
	CClipHelper::eTerminationType terminationType = (m_iClipFlags & APF_ISFINISHAUTOREMOVE) ? CClipHelper::TerminationType_OnFinish: CClipHelper::TerminationType_OnDelete;

	// If m_clipDictIndex and m_clipHashKey are not defined 
	// m_clipSetId and m_clipId must be defined
	if (m_clipDictIndex == -1 || m_clipHashKey.IsNull())
	{
		taskAssert((m_clipSetId != CLIP_SET_ID_INVALID) && (m_clipId != CLIP_ID_INVALID));

		m_clipDictIndex = fwClipSetManager::GetClipDictionaryIndex(m_clipSetId);
		m_clipHashKey = m_clipId; //fwClipSetManager::GetClipId(m_clipSetId, m_clipId);

		const fwClipItem *pClipItem = fwClipSetManager::GetClipItem(m_clipSetId, m_clipId);
		taskAssertf(pClipItem, "Clip Item not found in clipset %s\n", PrintClipInfo(m_clipSetId, m_clipId));
		if (pClipItem)
		{
			m_iClipFlags = GetClipItemFlags(pClipItem);
			m_iPriority = GetClipItemPriority(pClipItem);
		}
	}
	taskAssert((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()));

	// Do I have a valid clipSetId and clipId
	if ((m_clipSetId != CLIP_SET_ID_INVALID) && (m_clipId != CLIP_ID_INVALID))
	{
		// Start the clip using the clipSetId set id and clipId
		CTask::StartClipBySetAndClip(pPhysical, m_clipSetId, m_clipId, m_iClipFlags, m_iBoneMask, m_iPriority, m_fBlendDelta, terminationType);
	}
	else
	{
		// Start the clip using the clip dictionary index and clip hash
		CTask::StartClipByDictAndHash(pPhysical, m_clipDictIndex, m_clipHashKey, m_iClipFlags, m_iBoneMask, m_iPriority, m_fBlendDelta, terminationType);
	}

	taskAssert(GetClipHelper());
	if(GetClipHelper())
	{	
		// Apply any requested changes to the physics state
		ApplyPhysicsStateChanges(pPhysical, GetClipHelper()->GetClip());

		// Override the rate and phase
		GetClipHelper()->SetRate(m_fRate);
		GetClipHelper()->SetPhase(m_fStartPhase);
	}
	else
	{
		SetTaskFlag(ATF_TASK_FINISHED);
	}
}

CTaskInfo *CTaskRunClip::CreateQueriableState() const 
{
	// this needs to be defined but we don't want to implement this since this task is not synced for now
	return CTask::CreateQueriableState();
}

void CTaskRunClip::ReadQueriableState(CClonedFSMTaskInfo *pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SCRIPT_ANIM );

    CClonedScriptClipInfo *pClipInfo = static_cast<CClonedScriptClipInfo*>(pTaskInfo);

	m_fBlendDelta = pClipInfo->GetClipBlendDelta();
	m_iClipFlags = pClipInfo->GetClipFlags();

    CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskRunClip::FSM_Return CTaskRunClip::UpdateClonedFSM(s32 iState, FSM_Event iEvent)
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

    CPhysical *pPhysical = GetPhysical(); //Get the entity ptr.

	FSM_Begin
		// Entry point
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPhysical);

		// Play an clip
		FSM_State(State_PlayingClip)
			FSM_OnEnter
				// Try and start the clip
				StartClip(pPhysical);
			FSM_OnUpdate
				return PlayingClip_OnUpdate(pPhysical);
			FSM_OnExit
				PlayingClip_OnExit(pPhysical);
			
		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskRunClip::OnCloneTaskNoLongerRunningOnOwner()
{
    SetStateFromNetwork(State_Finish);
}

//-------------------------------------------------------------------------------------------------
// 
// CTaskRunNamedClip
// 
//-------------------------------------------------------------------------------------------------
CTaskRunNamedClip::CTaskRunNamedClip(
	const strStreamingObjectName pClipDictName, 
	const char* pClipName, 
	const u32 iPriority, 
	const s32 iClipFlags, 
	const u32 iBoneMask, 
	const float fBlendDelta, 
	const int iDuration, 
	const s32 iTaskFlags,
	const float fStartPhase)
	: CTaskClip()
{
	taskAssertf(pClipDictName.IsNotNull(), "Null clip dictionary name\n");
	taskAssertf(pClipName, "Null clip name\n");
	
	if (pClipDictName.IsNotNull())
	{
		m_clipDictIndex = fwAnimManager::FindSlot(pClipDictName).Get();
	}

	if (pClipName)
	{
		m_clipHashKey = atHashWithStringNotFinal(pClipName);
	}

	taskAssert((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()));
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey.GetHash());
	SetReferenceClip(pClip);
	taskAssertf(pClip, "Missing clip (clip dict index, clip hash key): %d, %s %u\n", m_clipDictIndex, m_clipHashKey.TryGetCStr() ? m_clipHashKey.TryGetCStr() : "(null)", m_clipHashKey.GetHash());

	m_eTaskType = CTaskTypes::TASK_RUN_NAMED_CLIP;
	m_iTaskFlags = iTaskFlags;
	m_iClipFlags = iClipFlags;
	m_iBoneMask = iBoneMask;
	m_iPriority = iPriority,
	m_fBlendDelta = fBlendDelta;
	m_fStartPhase = fStartPhase;
	m_iDuration = iDuration;
	m_iTerminationBehavior = TB_DO_NOTHING;
	m_physicsSettings = 0;

#if !__FINAL
	m_eTaskType = CTaskTypes::TASK_RUN_NAMED_CLIP;
#endif
	SetInternalTaskType(CTaskTypes::TASK_RUN_NAMED_CLIP);
}

CTaskRunNamedClip::CTaskRunNamedClip(
	s32 clipDictIndex,
	u32 clipHashKey,
	const u32 iPriority, 
	const s32 iClipFlags, 
	const u32 iBoneMask, 
	const float fBlendDelta, 
	const s32 iDuration, 
	const s32 iTaskFlags,
	const float fStartPhase,
    const bool  ASSERT_ONLY(bIsCloneTask))
	: CTaskClip()
{
	m_clipDictIndex = clipDictIndex;
	m_clipHashKey = clipHashKey;
	taskAssert((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()));
    // network clone tasks stream in the scripted clips they are using, local task require the clip to already be streamed in
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey.GetHash());
	SetReferenceClip(pClip);

#if __ASSERT
	if(!g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictIndex)))
	{
		const char *clipDictionaryName = "(invalid)";
		if (g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictIndex)))
		{
			clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictIndex));
		}

		taskAssertf(bIsCloneTask || pClip, "Missing clip (clip dict index, clip hash key): %d \"%s\", %s %u\n", m_clipDictIndex, clipDictionaryName, m_clipHashKey.TryGetCStr() ? m_clipHashKey.TryGetCStr() : "(null)", m_clipHashKey.GetHash());
	}
#endif // __ASSERT

	m_iPriority = iPriority,
	m_iClipFlags = iClipFlags;
	m_iBoneMask= iBoneMask;
	m_fBlendDelta = fBlendDelta;
	m_iDuration = iDuration;
	m_iTaskFlags = iTaskFlags;
	m_fStartPhase = fStartPhase;
	m_eTaskType = CTaskTypes::TASK_RUN_NAMED_CLIP;
	m_iTerminationBehavior = TB_DO_NOTHING;
	m_physicsSettings = 0;

#if !__FINAL
	m_eTaskType = CTaskTypes::TASK_RUN_NAMED_CLIP;
#endif
	SetInternalTaskType(CTaskTypes::TASK_RUN_NAMED_CLIP);
}

CTaskRunNamedClip::CTaskRunNamedClip(
				const strStreamingObjectName pClipDictName, 
				const char* pClipName, 
				const u32 iPriority, 
				const s32 iClipFlags, 
				const u32 iBoneMask, 
				const float fBlendDelta,
				const s32 iDuration, 
				const s32 iTaskFlags,				
				const float fStartPhase,
				const Vector3 &vInitialPosition,
				const Quaternion &InitialOrientationQuaternion)
				  : CTaskClip()
{
	taskAssertf(pClipDictName.IsNotNull(), "Null clip dictionary name\n");
	taskAssertf(pClipName, "Null clip name\n");

	if (pClipDictName.IsNotNull())
	{
		m_clipDictIndex = fwAnimManager::FindSlot(pClipDictName).Get();
	}

	if (pClipName)
	{
		m_clipHashKey = atHashWithStringNotFinal(pClipName);
	}

	taskAssert((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()));
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey.GetHash());
	SetReferenceClip(pClip);
	taskAssertf(pClip, "Missing clip (clip dict index, clip hash key): %d, %s %u\n", m_clipDictIndex, m_clipHashKey.TryGetCStr() ? m_clipHashKey.TryGetCStr() : "(null)", m_clipHashKey.GetHash());

	m_eTaskType = CTaskTypes::TASK_RUN_NAMED_CLIP;
	m_iTaskFlags = iTaskFlags;
	m_iClipFlags = iClipFlags;
	m_iBoneMask= iBoneMask;
	m_iPriority = iPriority,
	m_fBlendDelta = fBlendDelta;
	m_iDuration = iDuration;
	m_iTerminationBehavior = TB_DO_NOTHING;
	m_fStartPhase = fStartPhase;
	m_physicsSettings = 0;

	//settings for repositioning and reorienting the entity before the clip starts
	m_initialOrientation = InitialOrientationQuaternion;
	m_physicsSettings |= PS_APPLY_REORIENTATION;
	m_initialPosition = vInitialPosition;
	m_physicsSettings |= PS_APPLY_REPOSITION;

#if !__FINAL
	m_eTaskType = CTaskTypes::TASK_RUN_NAMED_CLIP;
#endif
	SetInternalTaskType(CTaskTypes::TASK_RUN_NAMED_CLIP);
}

void CTaskRunNamedClip::CleanUp()
{
	CEntity *pEntity = static_cast< CEntity * >(GetEntity());
	if(pEntity->GetIsTypeVehicle() || pEntity->GetIsTypeObject())
	{
		CDynamicEntity *pDynamicEntity = static_cast< CDynamicEntity * >(pEntity);
		pDynamicEntity->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
	}

	CTaskClip::CleanUp();
}

aiTask* CTaskRunNamedClip::Copy() const
{
	CTaskRunNamedClip* pTask = rage_new CTaskRunNamedClip(
		m_clipDictIndex,
		m_clipHashKey,
		m_iPriority,
		m_iClipFlags,
		m_iBoneMask,
		m_fBlendDelta,
		m_iDuration,
		m_iTaskFlags,
		m_fStartPhase);

	pTask->m_eTaskType = m_eTaskType;
	pTask->m_fBlendOutDelta = m_fBlendOutDelta;
	pTask->m_fRate = m_fRate;
	pTask->m_timer = m_timer;
	pTask->m_clipSetId = m_clipSetId;
	pTask->m_clipId = m_clipId;
	//pTask->m_iQuitEvent = m_iQuitEvent;
	pTask->m_initialPosition = m_initialPosition;
	pTask->m_initialOrientation = m_initialOrientation;
	pTask->m_physicsSettings = m_physicsSettings;
	pTask->m_iTerminationBehavior = m_iTerminationBehavior;

	return pTask;
}


#if !__FINAL
const char * CTaskRunNamedClip::GetStaticStateName( s32 iState )
{
	taskAssert(iState>=0&&iState<=State_Finish);
	static const char* aRunNamedClipStateNames[] = 
	{
		"State_Start",
		"State_PlayingAnim",
		"State_Finish"
	};

	return aRunNamedClipStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskRunNamedClip::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPhysical *pPhysical = GetPhysical(); //Get the entity ptr.

	FSM_Begin
		// Entry point
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPhysical);

		// Play an clip
		FSM_State(State_PlayingClip)
			FSM_OnEnter
				PlayingClip_OnEnter(pPhysical);
			FSM_OnUpdate
				return PlayingClip_OnUpdate(pPhysical);
			FSM_OnExit
				PlayingClip_OnExit(pPhysical);

		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskRunNamedClip::ProcessPreFSM()
{
	CPed * pPed = GetPhysical()->GetIsTypePed() ? static_cast<CPed*>(GetPhysical()) : NULL;
	if(pPed)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);
	}

	return CTaskClip::ProcessPreFSM();
}

CTask::FSM_Return CTaskRunNamedClip::Start_OnUpdate( CPhysical* UNUSED_PARAM(pPhysical))
{
	const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey);
	if(pClip)
	{
		SetState(State_PlayingClip);
		return FSM_Continue;
	}
	else
	{
		PrintDebugInfo();
		return FSM_Quit;
	}
}

void CTaskRunNamedClip::Start_OnEnterClone( CPhysical *UNUSED_PARAM(pPhysical) )
{
    m_clipRequest.RequestClipsByIndex(m_clipDictIndex);
}

CTask::FSM_Return CTaskRunNamedClip::Start_OnUpdateClone( CPhysical *UNUSED_PARAM(pPhysical) )
{
    if(!m_clipRequest.HaveClipsBeenLoaded())
    {
        return FSM_Continue;
    }

	const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey);
	if(pClip)
	{
		SetState(State_PlayingClip);
		return FSM_Continue;
	}
	else
	{
		PrintDebugInfo();
		return FSM_Quit;
	}
}

void CTaskRunNamedClip::PlayingClip_OnEnter( CPhysical* pPhysical )
{
	if (pPhysical->GetIsTypeObject())
	{
		// Physics bounds update
		CPhysical * pPhysical = GetPhysical();
		fragInst* pFragInst = pPhysical->GetFragInst();
		if(pFragInst && pFragInst->GetSkeleton())
		{
			if (pFragInst->GetCached())
			{
				if(IsTaskFlagSet(ATF_DRIVE_TO_POSE))
				{
					// Make sure objects using drive-to-pose are updating their physics every frame
					pFragInst->GetCacheEntry()->LazyArticulatedInit();
					pPhysical->ClearBaseFlag(fwEntity::IS_FIXED);			// fixed objects will not be active and will not update bounds when clipating with drive to pose
					pPhysical->ActivatePhysics();

					//Initialize the frame used for pose matching
					m_PoseMatchFrame.InitCreateBoneAndMoverDofs(pFragInst->GetSkeleton()->GetSkeletonData(), false);
					m_PoseMatchFrame.IdentityFromSkel(pFragInst->GetSkeleton()->GetSkeletonData());
				}
			}
		}
	}
	
	// Start the clip
	if(!GetClipHelper())
	{
		StartClip(pPhysical);
	}

	// This is the first call to the update function
	SetTaskFlag(ATF_FIRST_UPDATE);
}

CTask::FSM_Return CTaskRunNamedClip::PlayingClip_OnUpdate( CPhysical* pPhysical )
{
	TUNE_BOOL(bDisableDriveToPose, false);

	CEntity *pEntity = static_cast< CEntity * >(GetEntity());
	if(pEntity->GetIsTypeVehicle() || pEntity->GetIsTypeObject())
	{
		CDynamicEntity *pDynamicEntity = static_cast< CDynamicEntity * >(pEntity);
		pDynamicEntity->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
	}

	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;

	if(IsTaskFlagSet(ATF_TASK_FINISHED))
	{
		SetState(State_Finish);

		// No longer the first update
		UnsetTaskFlag(ATF_FIRST_UPDATE);
		return FSM_Continue;
	}

	if (IsTaskFlagSet(ATF_OVERRIDE_PHYSICS))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
	}

	// don't want non-script events clogging up the event queue
	if(IsTaskFlagSet(ATF_DONT_INTERRUPT) && (!IsClipFlagSet(APF_ISFINISHAUTOREMOVE) || IsClipFlagSet(APF_ISLOOPED)) )
	{
		if (pPed)
		{
			pPed->GetPedIntelligence()->RemoveInvalidEvents(true, true);

			// Set the dying state, if the ped is injured, as cos this clip is non-interruptible they'll stay in this state
			if( pPed->IsFatallyInjured() )
			{
				pPed->SetDeathState(DeathState_Dying);
			}
		}
	}

	// Is the timer being used if so has the timer finished?
	if(m_timer.IsSet() && m_timer.IsOutOfTime())
	{
		StopClip(m_fBlendOutDelta, GetStopClipFlags());
		SetTaskFlag(ATF_TASK_FINISHED);
		SetState(State_Finish);

		// No longer the first update
		UnsetTaskFlag(ATF_FIRST_UPDATE);
		return FSM_Continue;
	}

	// If there is no clip, the task will have finished
	if(!GetClipHelper())
	{
		SetTaskFlag(ATF_TASK_FINISHED);
		SetState(State_Finish);
	}

	//hack for cumulative error in extracting mover information
	//Manually calculate the correct offset for the entity based on the stored starting matrix
	if (IsTaskFlagSet(ATF_OVERRIDE_PHYSICS))
	{
		if (pPed)
			pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
		ApplyMoverTrackFixup(pPhysical);
	}

	if (pPhysical->GetIsTypeObject())
	{
		// Physics bounds update
		CPhysical * pPhysical = GetPhysical();
		fragInst* pFragInst = pPhysical->GetFragInst();
		if(pFragInst && pFragInst->GetSkeleton())
		{
			if (pFragInst->GetCached())
			{
				if(IsTaskFlagSet(ATF_DRIVE_TO_POSE) && pFragInst->GetCacheEntry()->GetHierInst() && pFragInst->GetCacheEntry()->GetHierInst()->body && !bDisableDriveToPose)
				{
					// Tell the system to do drive to pose
					pPhysical->GetAnimDirector()->SetUseCurrentSkeletonForActivePose(true);

					if (Verifyf(pFragInst->GetCacheEntry()->GetHierInst()->articulatedCollider, "Missing articulation data on object set to drive to pose"))
						// make sure the objects physics doesn't sleep (needs to be awake so the articulation is updated)
						pFragInst->GetCacheEntry()->GetHierInst()->articulatedCollider->GetSleep()->Reset();
					pPhysical->ActivatePhysics();
				}
				else
					// just pose the bounds based on the animated skeleton (non drive to pose version)
					pFragInst->PoseBoundsFromSkeleton(true, true);
			}
		}
	}

	// No longer the first update
	UnsetTaskFlag(ATF_FIRST_UPDATE);
	return FSM_Continue;
}

void CTaskRunNamedClip::PlayingClip_OnExit(CPhysical* pPhysical)
{
	//if (GetClipHelperStatus() == CClipHelper::CLIP_TERMINATED)
	//{
		if (m_iTerminationBehavior & TB_OFFSET_PED)
		{
			OffsetEntityPosition(pPhysical);
		}

		RestorePhysicsState(pPhysical);

		StopClip(m_fBlendOutDelta, GetStopClipFlags());
	//}

	// sync the desired heading to the current
		if (pPhysical->GetIsTypePed())
		{
			CPed* pPed = GetPed();
			Vector3 eulerRotation;
			Matrix34 m = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			m.ToEulersXYZ(eulerRotation);

			pPed->SetDesiredHeading(eulerRotation.z);
		}

	//clean up pose matching frame memory
	if(IsTaskFlagSet(ATF_DRIVE_TO_POSE))
		m_PoseMatchFrame.Shutdown();
}


void CTaskRunNamedClip::StartClip(CPhysical* pPhysical)
{
	// Is this clip running for a specific length of time?
	if(m_iDuration>=0)
	{
		m_timer.Set(fwTimer::GetTimeInMilliseconds(),m_iDuration);
	}

	// Isn't the clip to be removed when its phase finishes?
	if(!(m_iClipFlags & APF_ISFINISHAUTOREMOVE))
	{
		// Set the clip to be removed when its blend out finishes
		// (it will start to blend out when its phase finishes)
		m_iClipFlags |= APF_ISBLENDAUTOREMOVE;
	}

	// If this is a secondary clip, use the secondary task blend helper
	if (IsTaskFlagSet(ATF_SECONDARY))
	{
		m_iClipFlags |= APF_USE_SECONDARY_SLOT;
	}

	// Figure out if the task is the last in the sequence
	bool isLastTaskInSequence = false;

	// Go to the root of the task tree
	//aiTask* pParentTask = GetParent();
	//while (pParentTask->GetParent())
	//{
	//	pParentTask = pParentTask->GetParent();
	//}
	//bool isInSequence = pParentTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_SEQUENCE;

	// Is the parent task a TASK_USE_SEQUENCE task?
	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_USE_SEQUENCE)
	{
		CTaskUseSequence* pTaskComplexUseSequence = static_cast<CTaskUseSequence*>(GetParent());
		if (pTaskComplexUseSequence)
		{
			taskAssert(pTaskComplexUseSequence->GetSequenceID() < CTaskSequences::MAX_NUM_SEQUENCE_TASKS);

			// Get the task from the array of sequence
			const CTaskSequenceList* pSequenceList = pTaskComplexUseSequence->GetTaskSequenceList();
			if (Verifyf(pSequenceList,"Invalid sequence list"))
			{
				// Count the number of tasks in the sequence
				u32 taskIndex = 0;
				const aiTask* pTaskInSequence = pSequenceList->GetTask(taskIndex);
				while (pTaskInSequence)
				{
					taskIndex++;
					pTaskInSequence = pSequenceList->GetTask(taskIndex);
				}

				// Is the current task the last in the sequence?
				isLastTaskInSequence = pTaskComplexUseSequence->GetPrimarySequenceProgress() + 1 == taskIndex;
			}
		}
	}

	// Blend in the clip
	//CClipNetworkMoveInfo moveInfo(CClipNetworkMoveInfo::NETWORK_TASK, this->GetTaskType(), CTask::GetPool()->GetIndex(this));
	//CClipPlayer* pClipPlayer = pPed->GetClipBlender()->BlendClip(m_clipDictIndex, m_clipHashKey, m_iClipFlags, m_iBoneMask, m_iPriority, m_fBlendDelta, moveInfo);
	CTask::StartClipByDictAndHash(GetPhysical(), m_clipDictIndex, m_clipHashKey, m_iClipFlags, m_iBoneMask, m_iPriority, m_fBlendDelta, CClipHelper::TerminationType_OnFinish);
	CClipHelper* pClipPlayer = GetClipHelper();
	taskAssert(pClipPlayer);
	if (pClipPlayer)
	{
		// Override the phase
		pClipPlayer->SetRate(m_fRate); 
		pClipPlayer->SetPhase(m_fStartPhase);

		// Is the task going to delay blending out the clip by one frame?
		if (IsTaskFlagSet(ATF_DELAY_BLEND_WHEN_FINISHED))
		{
			m_iTerminationBehavior |= TB_DELAY_BLEND_OUT_ONE_FRAME;
		}

		// Apply any requested changes to the physics state
		ApplyPhysicsStateChanges(pPhysical, pClipPlayer->GetClip());

		// Is the clip going to offset the entity?
		if(IsTaskFlagSet(ATF_OFFSET_PED))
		{
			// When the phase finishes offset the entity
			m_iTerminationBehavior |= TB_OFFSET_PED;

			// When the clip phase is 1
			// Call the callback and blend out the clip
			pClipPlayer->SetFlag(APF_ISFINISHAUTOREMOVE);
			pClipPlayer->UnsetFlag(APF_ISBLENDAUTOREMOVE);
		}
		else if(IsTaskFlagSet(ATF_RUN_IN_SEQUENCE) && !isLastTaskInSequence)
		{
			// The task is in a sequence and is not the last task in a sequence
			if (IsTaskFlagSet(ATF_HOLD_LAST_FRAME))
			{
				// Is this clip running as a secondary task?
				if (IsTaskFlagSet(ATF_SECONDARY))
				{
					// Secondary clip in a sequence holding the last frame

					// Is this clip running for a specific length of time?
					if(m_iDuration>=0)
					{
						// When the clip phase is 1 pause the clip at phase 1
						// Call the callback when the clip blends out
						pClipPlayer->UnsetFlag(APF_ISFINISHAUTOREMOVE);
						pClipPlayer->SetFlag(APF_ISBLENDAUTOREMOVE);
					}
					else
					{
						// When the clip phase is 1
						// Call the callback and blend out the clip
						//terminationType = CClipHelper::TerminationType_OnFinish;
						//pClipPlayer->SetFlag(APF_ISFINISHAUTOREMOVE);
						//pClipPlayer->UnsetFlag(APF_ISBLENDAUTOREMOVE);
					}
				}
				else
				{	
					// Primary clip in a sequence holding the last frame

					// Is this clip running for a specific length of time?
					if(m_iDuration>=0)
					{
						// When the clip phase is 1 pause the clip at phase 1
						// Call the callback when the clip blends out
						pClipPlayer->UnsetFlag(APF_ISFINISHAUTOREMOVE);
						pClipPlayer->SetFlag(APF_ISBLENDAUTOREMOVE);
					}
					else
					{
						// When the clip phase is 1
						// Call the callback and blend out the clip 
						// but delay the blend out by one frame
						m_iTerminationBehavior |= TB_DELAY_BLEND_OUT_ONE_FRAME;
						pClipPlayer->SetFlag(APF_ISFINISHAUTOREMOVE);
						pClipPlayer->UnsetFlag(APF_ISBLENDAUTOREMOVE);
					}
				}
			}
			else
			{
				// Primary clip in a sequence not holding the last frame

				// Is the clip to be removed when its phase finishes?
				if (IsClipFlagSet(APF_ISFINISHAUTOREMOVE))
				{
					// When the clip phase is 1
					// Call the callback and blend out the clip
					pClipPlayer->SetFlag(APF_ISFINISHAUTOREMOVE);
					pClipPlayer->UnsetFlag(APF_ISBLENDAUTOREMOVE);
				}
				else
				{
					// When the clip phase is 1 pause the clip at phase 1
					// Call the callback when the clip blends out
					pClipPlayer->UnsetFlag(APF_ISFINISHAUTOREMOVE);
					pClipPlayer->SetFlag(APF_ISBLENDAUTOREMOVE);
				}
			}
		}
		else
		{
			// The task is not in a sequence or it is the last task in a sequence

			if (IsTaskFlagSet(ATF_HOLD_LAST_FRAME))
			{
				// When the clip phase is 1 pause the clip at phase 1
				// Call the callback when the clip blends out
				pClipPlayer->UnsetFlag(APF_ISFINISHAUTOREMOVE);
				pClipPlayer->SetFlag(APF_ISBLENDAUTOREMOVE);
			}
			else
			{
				// Is the clip to be removed when its phase finishes?
				if (IsClipFlagSet(APF_ISFINISHAUTOREMOVE))
				{

					pClipPlayer->SetFlag(APF_ISFINISHAUTOREMOVE);
					pClipPlayer->UnsetFlag(APF_ISBLENDAUTOREMOVE);
				}
				else
				{
					// When the clip phase is 1 pause the clip at phase 1
					// Call the callback when the clip blends out
					pClipPlayer->UnsetFlag(APF_ISFINISHAUTOREMOVE);
					pClipPlayer->SetFlag(APF_ISBLENDAUTOREMOVE);
				}
			}
		}

		taskAssert(GetClipHelper());
	}
	else
	{
		SetTaskFlag(ATF_TASK_FINISHED);
	}

	if (m_iTerminationBehavior & TB_DELAY_BLEND_OUT_ONE_FRAME)
	{
		DelayBlendOut(pPhysical);
	}
}

//
// DelayBlendOut
// When the clip has finished delay the blend out for one frame
void CTaskRunNamedClip::DelayBlendOut(CPhysical* ASSERT_ONLY(pPhysical))
{
	// Assumes there is only one instance of the clip playing
	taskAssert((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()));
	taskAssert(pPhysical && pPhysical->GetAnimDirector());

	CClipHelper* pClipPlayer = GetClipHelper();
	if (pClipPlayer)
	{
		pClipPlayer->SetFlag(APF_SKIP_NEXT_UPDATE_BLEND);
	}
}


//
// OffsetEntityPosition
// When the clip has finished offset the entity
void CTaskRunNamedClip::OffsetEntityPosition(CPhysical* pPhysical)
{
	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;

	taskAssert((m_clipDictIndex != -1) && (m_clipHashKey.IsNotNull()));
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_clipDictIndex, m_clipHashKey);
	if (!pClip)
	{
		taskAssertf(0, "clip not loaded!" );
		return;
	}

	float endTime = pClip->GetDuration();

	crFrameDataFixedDofs<4> frameData;
	frameData.AddDof(kTrackBoneTranslation, 0, kFormatTypeVector3);
	frameData.AddDof(kTrackBoneRotation, 0, kFormatTypeQuaternion);
	frameData.AddDof(kTrackMoverTranslation, 0, kFormatTypeVector3);
	frameData.AddDof(kTrackMoverRotation, 0, kFormatTypeQuaternion);

	crFrameFixedDofs<4> frame(frameData);
	frame.SetAccelerator(fwAnimDirectorComponentCreature::GetAccelerator());
	pClip->Composite(frame, endTime);

	TransformV moverSituation(V_IDENTITY);
	frame.GetMoverSituation(0, moverSituation);

	TransformV rootSituation(V_IDENTITY);
	frame.GetBoneSituation(0, rootSituation);

	Matrix34 moverEndMat(M34_IDENTITY);
	Mat34VFromTransformV(RC_MAT34V(moverEndMat), moverSituation);

	Matrix34 rootEndMat(M34_IDENTITY);
	Mat34VFromTransformV(RC_MAT34V(rootEndMat), rootSituation);

	// transform by the peds matrix to get the world space heading and position
	const Matrix34 pedMat = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
	Vector3 translation = moverEndMat.d;
	pedMat.Transform(translation);

	// Set the position of the peds matrix
	pPhysical->SetPosition(translation, false, false, false);

	// Calculate the peds new matrix
	// Only use the z component from the mover track
	// (apply the x and y components to the root bone later)
	Quaternion rotation(QUATV_TO_QUATERNION(moverSituation.GetRotation()));
	Vector3 eulers;
	rotation.ToEulers(eulers);
	eulers.x = 0.0f;
	eulers.y = 0.0f;
	rotation.FromEulers(eulers);

	Matrix34 mat;
	mat.Identity();
	mat.FromQuaternion(rotation);

	// rotation is in local space transform by the peds matrix to get the world position
	mat.Dot(pedMat);

	// Set the heading of the peds matrix?
	if (IsTaskFlagSet(ATF_ORIENT_PED))
	{
		float fTheta = fwAngle::GetRadianAngleBetweenPoints(mat.b.x, mat.b.y, 0.0f, 0.0f);
		fTheta = fwAngle::LimitRadianAngle(fTheta);
		if (pPed)
			pPed->SetDesiredHeading(fTheta);
		pPhysical->SetHeading(fTheta);
	}

	// Get the local space matrix of the root bone
	Matrix34 &localRootBoneMat = pPhysical->GetLocalMtxNonConst(0);

	// Set the rotation of the root bone matrix?
	if (IsTaskFlagSet(ATF_ORIENT_PED))
	{
		// The root bone (in local space) is the x and the y components of the mover track rotation
		// (we applied the z component to the peds matrix earlier)
		localRootBoneMat.Identity();
		rotation = QUATV_TO_QUATERNION(moverSituation.GetRotation());
		rotation.ToEulers(eulers);
		eulers.z = 0.0f;
		rotation.FromEulers(eulers);
		localRootBoneMat.FromQuaternion(rotation);
		localRootBoneMat.Dot3x3(rootEndMat);
	}

	// Set the position of the root bone matrix
	localRootBoneMat.d = rootEndMat.d;//Vector3(0.0f, 0.0f, 0.0f);

	// If we are attached to anything detach now
	if (pPhysical->GetAttachParent())
	{
		pPhysical->DetachFromParent(0);
	}

	// In case the entity has moved into another sector
	//pPed->RemoveAndAdd();
	pPhysical->SetPosition(translation, false, false, false);
}

CTaskInfo *CTaskRunNamedClip::CreateQueriableState() const
{
	u8 iPriority = static_cast<u8>(m_iPriority);
	return rage_new CClonedScriptClipInfo(GetState(), m_clipDictIndex, m_clipHashKey, m_fBlendDelta, iPriority, m_iClipFlags);
}

void CTaskRunNamedClip::ReadQueriableState(CClonedFSMTaskInfo *pTaskInfo)
{
    taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SCRIPT_ANIM );

	CClonedScriptClipInfo *pClipInfo = static_cast<CClonedScriptClipInfo*>(pTaskInfo);

	m_fBlendDelta = pClipInfo->GetClipBlendDelta();
	m_iClipFlags = pClipInfo->GetClipFlags();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskRunClip::FSM_Return CTaskRunNamedClip::UpdateClonedFSM(s32 iState, FSM_Event iEvent)
{
    CPhysical *pPhysical = GetPhysical(); //Get the entity ptr.

	FSM_Begin
		// Entry point
		FSM_State(State_Start)
            FSM_OnEnter
				Start_OnEnterClone(pPhysical);
			FSM_OnUpdate
				return Start_OnUpdateClone(pPhysical);

		// Play an clip
		FSM_State(State_PlayingClip)
			FSM_OnEnter
				PlayingClip_OnEnter(pPhysical);
			FSM_OnUpdate
				return PlayingClip_OnUpdate(pPhysical);
			FSM_OnExit
				PlayingClip_OnExit(pPhysical);

		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskRunNamedClip::OnCloneTaskNoLongerRunningOnOwner()
{
    SetStateFromNetwork(State_Finish);
}

bool CTaskRunNamedClip::ProcessPostMovement()
{
	if(GetClipHelper() && IsTaskFlagSet(ATF_DRIVE_TO_POSE))
	{
		const fragInst* pFragInst = GetPhysical()->GetFragInst();
		if(pFragInst && pFragInst->GetSkeleton())
		{
			if (pFragInst->GetCached())
			{
				const fragCacheEntry* entry = pFragInst->GetCacheEntry();
				const fragHierarchyInst* hierInst = entry->GetHierInst();
				if(hierInst && hierInst->articulatedCollider)
				{
					float bestTime = ComputeMatchingPose(*pFragInst, *hierInst->articulatedCollider);
					float currentTime = GetClipHelper()->GetTime();
					float duration = GetClipHelper()->GetDuration();

					//make sure the times aren't looped
					float error = abs(bestTime - currentTime);
					if(error > duration/2)
					{
						if(bestTime > currentTime)
							currentTime += duration;
						else
							bestTime += duration;

						error = abs(bestTime - currentTime);
					}

					//if the error between the animation and the instance is too great set the animation to the instance pose
					static float maxError = 0.05f;
					if(error > maxError)
					{
						bestTime += (bestTime < currentTime ? maxError: -maxError);
						if(bestTime < 0)
							bestTime += duration;
						else if(bestTime > duration)
							bestTime -= duration;
						GetClipHelper()->SetTime(bestTime);
					}
				}
			}
		}
	}
	return true;
}

float CTaskRunNamedClip::ComputeMatchingPose(const fragInst& frag, const phArticulatedCollider& collider)
{
	static unsigned maxSamples = 3;
	static float maxError = 0.002f;
	static float spread = 0.03f;
	static float expectedLag = 0.01f;

	float expectedTime = GetClipHelper()->GetTime() - expectedLag;

	// check the where it is expected that the body is first
	float errorMid = ComputePoseError(expectedTime, frag, collider);
	if(errorMid < maxError)
	{
		return expectedTime;
	}
	else
	{
		float errorLeft = ComputePoseError(expectedTime - spread, frag, collider);
		float errorRight = ComputePoseError(expectedTime + spread, frag, collider);

		float error1;
		float error2 = errorMid;
		float error3;
		float direction;

		// select the path of least error
		if(errorLeft < errorRight)
		{
			error1 = errorRight;
			error3 = errorLeft;
			direction = -1;
		}
		else
		{
			error1 = errorLeft;
			error3 = errorRight;
			direction = 1;
		}

		// sample in the chosen direction until an the error is increasing
		unsigned sample = 1;
		while(error3 <= error2)
		{
			++sample;
			error1 = error2;
			error2 = error3;
			error3 = ComputePoseError(expectedTime + direction*spread*sample, frag, collider);
		}

		// choose whichever of the end samples is closer
		if(error3 < error1)
		{
			return ComputeMatchingPoseHelper(expectedTime + direction*spread*sample, expectedTime + direction*spread*(sample-1), error3, error2, maxSamples, frag, collider);
		}
		else
		{
			return ComputeMatchingPoseHelper(expectedTime + direction*spread*(sample-2), expectedTime + direction*spread*(sample-1), error1, error2, maxSamples, frag, collider);
		}
	}
}
float CTaskRunNamedClip::ComputeMatchingPoseHelper(float timeBegin, float timeEnd, float errorBegin, float errorEnd, unsigned maxSamples, const fragInst& frag, const phArticulatedCollider& collider)
{
	float sampleTime = (timeEnd-timeBegin)/static_cast<float>(maxSamples);

	float smallestError = FLT_MAX;
	float bestSampleErrorLeft = FLT_MAX;
	float bestSampleErrorRight = FLT_MAX;
	unsigned bestSample = 0;

	float errorLeft = errorBegin;
	float errorRight;

	// go through each pair of samples between the two times
	for(unsigned sample = 1; sample <= maxSamples; ++sample)
	{
		errorRight = (sample == maxSamples) ? ComputePoseError(timeEnd + sampleTime*sample, frag, collider) : errorEnd;
		float error = errorLeft + errorRight;
		if(error < smallestError)
		{
			smallestError = error;
			bestSampleErrorLeft = errorLeft;
			bestSampleErrorRight = errorRight;
			bestSample = sample;
		}
		errorLeft = errorRight;
	}

	float bestTimeBegin = timeBegin + sampleTime*(bestSample-1);
	float bestTimeEnd = timeBegin + sampleTime*bestSample;
	return (bestSampleErrorRight*bestTimeBegin + bestSampleErrorLeft*bestTimeEnd)/(smallestError);
}


float CTaskRunNamedClip::ComputePoseError(float poseTime, const fragInst& frag, const phArticulatedCollider& collider)
{
	// make sure poseTime is between 0 and the animation duration
	if(poseTime > GetClipHelper()->GetDuration())
		poseTime -= GetClipHelper()->GetDuration();
	else if(poseTime < 0)
		poseTime += GetClipHelper()->GetDuration();

	// pose the frame based on the time
	GetClipHelper()->GetClip()->Composite(m_PoseMatchFrame, poseTime);
	const crSkeleton* skeleton = frag.GetSkeleton();
	const crSkeletonData& skeletonData = skeleton->GetSkeletonData();
	const fragPhysicsLOD* physics = frag.GetTypePhysics();

	float error = 0;

	// loop through all links
	for (int componentIndex = 0; componentIndex < physics->GetNumChildren(); ++componentIndex)
	{ 
		if(collider.GetLinkFromComponent(componentIndex) > 0)
		{
			u16 boneId = physics->GetAllChildren()[componentIndex]->GetBoneID();
			int boneIndex = -1; skeletonData.ConvertBoneIdToIndex(boneId, boneIndex);

			// the physical rotation has already been put in the object matrix post-physics
			const Mat34V& physicalRotation = skeleton->GetObjectMtx(boneIndex);

			// the graphical rotation must be calculated by multiplying backwards through the skeleton tree
			Mat34V graphicalRotation;
			m_PoseMatchFrame.GetBoneMatrix3x3(boneId,graphicalRotation);
			for(int parentBoneIndex = skeletonData.GetParentIndex(boneIndex);parentBoneIndex > 0; parentBoneIndex = skeletonData.GetParentIndex(parentBoneIndex))
			{
				u16 parentBoneId; skeletonData.ConvertBoneIndexToId(parentBoneIndex, parentBoneId);
				Mat34V parentRotation;
				m_PoseMatchFrame.GetBoneMatrix3x3(parentBoneId, parentRotation);
				Transform3x3(graphicalRotation, parentRotation, graphicalRotation);
			}

			// the error is the distance squared between each basis vectors in each rotation
			Mat34V errorMatrix = (physicalRotation-graphicalRotation);
			error += (MagSquared(errorMatrix.GetCol0()) + MagSquared(errorMatrix.GetCol1()) + MagSquared(errorMatrix.GetCol2())).Getf();
		}
	}

	return error;
}

//-------------------------------------------------------------------------
// Task info for running a script clip
//-------------------------------------------------------------------------
CClonedScriptClipInfo::CClonedScriptClipInfo()
: m_nBlendDelta(SCI_BLEND_INSTANT)
, m_iPriority(0)
, m_iClipFlags(0)
, m_slotIndex(0)
, m_hashKey(0)
{

}

CClonedScriptClipInfo::CClonedScriptClipInfo(s32 nState, s32 nDictID, u32 nClipHash, float fBlendIn, u8 iPriority, u32 iFlags)
: m_iPriority(iPriority)
, m_iClipFlags(iFlags)
, m_slotIndex(nDictID)
, m_hashKey(nClipHash)
{
	m_nBlendDelta = PackBlendDelta(fBlendIn);
	SetStatusFromMainTaskState(nState);
}

CTaskFSMClone *CClonedScriptClipInfo::CreateCloneFSMTask()
{
    CTaskFSMClone *cloneTask = 0;

    if(m_hashKey != 0)
    {
        u32 boneMask = BONEMASK_ALL;

        if((m_iClipFlags & APF_UPPERBODYONLY) != 0)
        {
            boneMask = (u32)BONEMASK_UPPERONLY;
        }

        cloneTask = rage_new CTaskRunNamedClip(m_slotIndex, m_hashKey, m_iPriority, m_iClipFlags, boneMask, UnpackBlendDelta(m_nBlendDelta), -1, 0, 0.0f, true);
    }

    return cloneTask;
}

CTaskFSMClone *CClonedScriptClipInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}

u8 CClonedScriptClipInfo::PackBlendDelta(float fBlendDelta)
{
    u8 iBlendDelta = 0;

	if (fBlendDelta > 16.0f || fBlendDelta == 0.0f)
		iBlendDelta = SCI_BLEND_INSTANT; // instant blend in
	else if (fBlendDelta > 8.0f)
		iBlendDelta = SCI_BLEND_16;
	else if (fBlendDelta > 4.0f)
		iBlendDelta = SCI_BLEND_8;
	else 
		iBlendDelta = SCI_BLEND_4;

	return iBlendDelta;
}

float CClonedScriptClipInfo::UnpackBlendDelta(u8 iBlendDelta)
{
    float fBlendDelta = 0.0f;

	switch (iBlendDelta)
	{
	case SCI_BLEND_4:
		fBlendDelta = 4.0f;
		break;
	case SCI_BLEND_8:
		fBlendDelta = 8.0f;
		break;
	case SCI_BLEND_16:
		fBlendDelta = 16.0f;
		break;
	case SCI_BLEND_INSTANT:
		fBlendDelta = 1000.0f;
		break;
	default:
		taskAssert(0);
	}

	return fBlendDelta;
}
#if __BANK

const char * CMoveParameterWidget::ms_widgetTypeNames[CMoveParameterWidget::kParameterWidgetNum] = 
{
	"Real",
	"Bool",
	"Flag",
	"Request",
	"Event",
	"Clip",
	"Expression",
	"Filter"
};

CompileTimeAssert(NELEM(CMoveParameterWidget::ms_widgetTypeNames) == CMoveParameterWidget::kParameterWidgetNum);

void CMoveParameterWidgetReal::Save()
{
	if (m_controlValue)
	{
		float inputValue = 0.0f;

		switch(m_controlValue)
		{
		case 1:
			inputValue = ((float)CControlMgr::GetDebugPad().GetLeftStickX() + 128.0f) / 256.0f ;
			break;
		case 2:
			inputValue = (-(float)CControlMgr::GetDebugPad().GetLeftStickY() + 128.0f) / 256.0f;
			break;
		case 3:
			inputValue = (float)CControlMgr::GetDebugPad().GetLeftShoulder2() / 255.0f;
			break;
		case 4:
			inputValue = ((float)CControlMgr::GetDebugPad().GetRightStickX() + 128.0f) / 256.0f;
			break;
		case 5:
			inputValue = (-(float)CControlMgr::GetDebugPad().GetRightStickY() + 128.0f) / 256.0f;
			break;
		case 6:
			inputValue = (float)CControlMgr::GetDebugPad().GetRightShoulder2() / 255.0f;
			break;
		default:
			break;
		}
		//translate the axis' -1.0f to 1.0f range into the range of the control parameter
		m_value = m_min +(inputValue*(m_max - m_min));
	}

	m_network->SetFloat( fwMvFloatId( GetName() ), m_value);
}

void CMoveParameterWidgetFlag::Save()
{
	if (m_controlValue)
	{
		switch(m_controlValue)
		{
		case 1:
			m_value = CControlMgr::GetDebugPad().LeftShoulder1JustDown();
			break;
		case 2:
			m_value = CControlMgr::GetDebugPad().RightShoulder1JustDown();
			break;
		case 3:
			m_value = CControlMgr::GetDebugPad().ButtonCrossJustDown();
			break;
		case 4:
			m_value = CControlMgr::GetDebugPad().ButtonCircleJustDown();
			break;
		case 5:
			m_value = CControlMgr::GetDebugPad().ButtonTriangleJustDown();
			break;
		case 6:
			m_value = CControlMgr::GetDebugPad().ButtonSquareJustDown();
			break;
		default:
			break;
		}
	}
	m_network->SetFlag( fwMvFlagId( GetName() ), m_value);
}

void CMoveParameterWidgetBool::Save()
{
	if (m_controlValue)
	{
		switch(m_controlValue)
		{
		case 1:
			m_value = CControlMgr::GetDebugPad().LeftShoulder1JustDown();
			break;
		case 2:
			m_value = CControlMgr::GetDebugPad().RightShoulder1JustDown();
			break;
		case 3:
			m_value = CControlMgr::GetDebugPad().ButtonCrossJustDown();
			break;
		case 4:
			m_value = CControlMgr::GetDebugPad().ButtonCircleJustDown();
			break;
		case 5:
			m_value = CControlMgr::GetDebugPad().ButtonTriangleJustDown();
			break;
		case 6:
			m_value = CControlMgr::GetDebugPad().ButtonSquareJustDown();
			break;
		default:
			break;
		}
	}

	m_network->SetBoolean( fwMvBooleanId( GetName() ), m_value);
}

void CMoveParameterWidgetRequest::Save()
{
	if (m_controlValue)
	{
		switch(m_controlValue)
		{
		case 1:
			m_value = CControlMgr::GetDebugPad().LeftShoulder1JustDown();
			break;
		case 2:
			m_value = CControlMgr::GetDebugPad().RightShoulder1JustDown();
			break;
		case 3:
			m_value = CControlMgr::GetDebugPad().ButtonCrossJustDown();
			break;
		case 4:
			m_value = CControlMgr::GetDebugPad().ButtonCircleJustDown();
			break;
		case 5:
			m_value = CControlMgr::GetDebugPad().ButtonTriangleJustDown();
			break;
		case 6:
			m_value = CControlMgr::GetDebugPad().ButtonSquareJustDown();
			break;
		default:
			break;
		}
	}

	if(m_requiresSave)
	{
		m_value = true; 
	}

	// TODO - implement request triggering through CMove
	if(m_value)
	{
		m_network->BroadcastRequest( fwMvRequestId( GetName() ));
	}
}

void CMoveParameterWidgetEvent::Save()
{
	if (m_controlValue)
	{
		switch(m_controlValue)
		{
		case 1:
			m_value = CControlMgr::GetDebugPad().LeftShoulder1JustDown();
			break;
		case 2:
			m_value = CControlMgr::GetDebugPad().RightShoulder1JustDown();
			break;
		case 3:
			m_value = CControlMgr::GetDebugPad().ButtonCrossJustDown();
			break;
		case 4:
			m_value = CControlMgr::GetDebugPad().ButtonCircleJustDown();
			break;
		case 5:
			m_value = CControlMgr::GetDebugPad().ButtonTriangleJustDown();
			break;
		case 6:
			m_value = CControlMgr::GetDebugPad().ButtonSquareJustDown();
			break;
		default:
			break;
		}
	}

	if(m_requiresSave)
	{
		m_value = true; 
	}

	if (m_value)
	{
		m_network->SetBoolean( fwMvBooleanId( GetName() ), m_value);
	}
}

const char *CMoveParameterWidgetClip::ms_sources[] =
{
	"Variable Clipset",
	"Absolute Clipset",
	"Clip Dictionary",
	"File Path"
};

int Compare(const char * const *lhs, const char * const *rhs)
{
	return stricmp(*lhs, *rhs);
}

CMoveParameterWidgetClip::CMoveParameterWidgetClip( fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, bool bUpdateEveryFrame /* = false */)
{
	m_type = kParameterWidgetClip;
	m_name = paramName;
	m_move = move;
	m_network = handle;
	m_perFrameUpdate = bUpdateEveryFrame;

	m_clip = NULL;
	strcpy(m_clipTextString, "(None Selected)");
	m_clipText = NULL;

	m_sourceIndex = eVariableClipset;

	strcpy(m_variableClipset_Clipset, "$default");
	strcpy(m_variableClipset_Clip, "");

	m_absoluteClipset_ClipsetIndex = 0;
	m_absoluteClipset_ClipsetCount = fwClipSetManager::GetClipSetCount();
	m_absoluteClipset_Clipsets = new const char *[m_absoluteClipset_ClipsetCount];
	atArray< const char * > clipsets; clipsets.ResizeGrow(m_absoluteClipset_ClipsetCount);
	for(int i = 0; i < m_absoluteClipset_ClipsetCount; i ++)
	{
		clipsets[i] = fwClipSetManager::GetClipSetIdByIndex(i).GetCStr();
	}
	clipsets.QSort(0, m_absoluteClipset_ClipsetCount, Compare);
	for(int i = 0; i < m_absoluteClipset_ClipsetCount; i ++)
	{
		m_absoluteClipset_Clipsets[i] = clipsets[i];
	}
	m_absoluteClipset_ClipIndex = 0;
	m_absoluteClipset_ClipCount = 0;
	m_absoluteClipset_Clips = NULL;

	m_clipDictionary_ClipDictionaryIndex = 0;
	m_clipDictionary_ClipDictionaryCount = g_ClipDictionaryStore.GetNumUsedSlots();
	m_clipDictionary_ClipDictionaries = new const char *[m_clipDictionary_ClipDictionaryCount];
	atArray< const char * > clipDictionaries; clipDictionaries.ResizeGrow(m_clipDictionary_ClipDictionaryCount);
	for(int i = 0, j = 0; j < g_ClipDictionaryStore.GetCount(); j ++)
	{
		if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(j)))
		{
			clipDictionaries[i ++] = g_ClipDictionaryStore.GetName(strLocalIndex(j));
		}
	}
	clipDictionaries.QSort(0, m_clipDictionary_ClipDictionaryCount, Compare);
	for(int i = 0; i < m_clipDictionary_ClipDictionaryCount; i ++)
	{
		m_clipDictionary_ClipDictionaries[i] = clipDictionaries[i];
	}
	m_clipDictionary_ClipIndex = 0;
	m_clipDictionary_ClipCount = 0;
	m_clipDictionary_Clips = NULL;

	strcpy(m_filePath_Clip, "");
}

CMoveParameterWidgetClip::~CMoveParameterWidgetClip()
{
	delete[] m_absoluteClipset_Clipsets; m_absoluteClipset_Clipsets = NULL;
	delete[] m_absoluteClipset_Clips; m_absoluteClipset_Clips = NULL;

	delete[] m_clipDictionary_ClipDictionaries; m_clipDictionary_ClipDictionaries = NULL;
	delete[] m_clipDictionary_Clips; m_clipDictionary_Clips = NULL;
}

void CMoveParameterWidgetClip::BuildClipMap(atMap< atHashValue, atHashValue > &clipMap, const fwMvClipSetId &clipSetId)
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(clipSet)
	{
		if(clipSet->GetClipItemCount() > 0)
		{
			for(int clipIndex = 0, clipCount = clipSet->GetClipItemCount(); clipIndex < clipCount; clipIndex ++)
			{
				atHashValue clipNameHash = clipSet->GetClipItemIdByIndex(clipIndex).GetHash();

				if(!clipMap.Access(clipNameHash))
				{
					clipMap.Insert(clipNameHash, clipNameHash);
				}
			}
		}
		else
		{
			strLocalIndex clipDictionaryIndex = g_ClipDictionaryStore.FindSlot(clipSet->GetClipDictionaryName().GetCStr());
			if(clipDictionaryIndex != -1)
			{
				crClipDictionary *clipDictionary = g_ClipDictionaryStore.Get(clipDictionaryIndex);
				if(clipDictionary)
				{
					for(int clipIndex = 0, clipCount = clipDictionary->GetNumClips(); clipIndex < clipCount; clipIndex ++)
					{
						crClip *clip = clipDictionary->FindClipByIndex(clipIndex);
						if(clip)
						{
							atString clipName(clip->GetName());
							atArray< atString > clipNameSplit;
							clipName.Split(clipNameSplit, "/");
							clipName = clipNameSplit[1];
							clipName.Truncate(clipName.GetLength() - 5);

							atHashValue clipNameHash = atHashString(clipName).GetHash();

							if(!clipMap.Access(clipNameHash))
							{
								clipMap.Insert(clipNameHash, clipNameHash);
							}
						}
					}
				}
			}
		}

		fwMvClipSetId fallbackSetId = clipSet->GetFallbackId();
		if(fallbackSetId != CLIP_SET_ID_INVALID)
		{
			BuildClipMap(clipMap, fallbackSetId);
		}
	}
}

void CMoveParameterWidgetClip::GetClipSetClips(int &clipCount, const char **&clips, const fwMvClipSetId &clipSetId)
{
	atMap< atHashValue, atHashValue > clipMap;

	BuildClipMap(clipMap, clipSetId);

	clipCount = clipMap.GetNumUsed();
	if(clipCount > 0)
	{
		clips = new const char *[clipCount];

		atArray< const char * >clipNames; clipNames.ResizeGrow(clipCount);

		atMap< atHashValue, atHashValue>::Iterator iterator = clipMap.CreateIterator();
		int clipIndex = 0;
		for(iterator.Start(); !iterator.AtEnd(); iterator.Next())
		{
			atHashValue clipNameHash = iterator.GetKey();

			clipNames[clipIndex ++] = atHashString(clipNameHash).GetCStr();
		}
		taskAssert(clipIndex == clipCount);

		clipNames.QSort(0, clipCount, Compare);

		for(int clipIndex = 0; clipIndex < clipCount; clipIndex ++)
		{
			clips[clipIndex] = clipNames[clipIndex];
		}
	}
}

void CMoveParameterWidgetClip::AddWidgets(bkBank* bank)
{
	m_group = (bkWidget *)bank->PushGroup(m_name, true);
	{
		// Add source type specific widgets
		switch(static_cast< SOURCE >(m_sourceIndex))
		{
		case eVariableClipset:
			{
				AddVariableClipsetWidgets(bank);
				OnVariableClipsetChanged();
			} break;
		case eAbsoluteClipset:
			{
				AddAbsoluteClipsetWidgets(bank);
				OnAbsoluteClipsetChanged();
			} break;
		case eClipDictionary:
			{
				AddClipDictionaryWidgets(bank);
				OnClipDictionaryChanged();
			} break;
		case eFilePath:
			{
				AddFilePathWidgets(bank);
				OnFilePathChanged();
			} break;
		default:
			{
			} break;
		}
	}
	bank->PopGroup();
}

void CMoveParameterWidgetClip::AddVariableClipsetWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetClip::OnSourceChanged), (datBase *)this));

	// Add variable clipset widgets
	bank->AddText("Variable Clipset:", m_variableClipset_Clipset, 256, datCallback(MFA(CMoveParameterWidgetClip::OnVariableClipsetChanged), (datBase *)this));
	bank->AddText("Clip Name:", m_variableClipset_Clip, 256, datCallback(MFA(CMoveParameterWidgetClip::OnVariableClipsetChanged), (datBase *)this));

	// Add expressions widget
	m_clipText = bank->AddText("Clip:", m_clipTextString, 256, true);
}

void CMoveParameterWidgetClip::AddAbsoluteClipsetWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetClip::OnSourceChanged), (datBase *)this));

	// Add absolute clipset widgets
	bank->AddCombo("Absolute Clipset:", &m_absoluteClipset_ClipsetIndex, m_absoluteClipset_ClipsetCount, m_absoluteClipset_Clipsets, datCallback(MFA(CMoveParameterWidgetClip::OnAbsoluteClipset_ClipsetChanged), (datBase *)this));
	m_absoluteClipset_ClipCount = 0;
	delete[] m_absoluteClipset_Clips; m_absoluteClipset_Clips = NULL;
	GetClipSetClips(m_absoluteClipset_ClipCount, m_absoluteClipset_Clips, fwMvClipSetId(m_absoluteClipset_Clipsets[m_absoluteClipset_ClipsetIndex]));
	bank->AddCombo("Clip Name:", &m_absoluteClipset_ClipIndex, m_absoluteClipset_ClipCount, m_absoluteClipset_Clips, datCallback(MFA(CMoveParameterWidgetClip::OnAbsoluteClipset_ClipChanged), (datBase *)this));

	// Add clip widget
	m_clipText = bank->AddText("Clip:", m_clipTextString, 256, true);
}

void CMoveParameterWidgetClip::AddClipDictionaryWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetClip::OnSourceChanged), (datBase *)this));

	// Add clip dictionary widgets
	bank->AddCombo("Clip Dictionary:", &m_clipDictionary_ClipDictionaryIndex, m_clipDictionary_ClipDictionaryCount, m_clipDictionary_ClipDictionaries, datCallback(MFA(CMoveParameterWidgetClip::OnClipDictionary_ClipDictionaryChanged), (datBase *)this));
	m_clipDictionary_ClipCount = 0;
	delete[] m_clipDictionary_Clips; m_clipDictionary_Clips = NULL;
	strLocalIndex clipDictionaryIndex = g_ClipDictionaryStore.FindSlot(m_clipDictionary_ClipDictionaries[m_clipDictionary_ClipDictionaryIndex]);
	crClipDictionary *clipDictionary = clipDictionaryIndex != -1 ? g_ClipDictionaryStore.Get(clipDictionaryIndex) : NULL;
	if(clipDictionary && clipDictionary->GetNumClips() > 0)
	{
		m_clipDictionary_ClipCount = clipDictionary->GetNumClips();
		m_clipDictionary_Clips = new const char *[m_clipDictionary_ClipCount];
		atArray< const char * > clips; clips.ResizeGrow(m_clipDictionary_ClipCount);
		for(int i = 0; i < m_clipDictionary_ClipCount; i ++)
		{
			clips[i] = clipDictionary->FindClipByIndex(i)->GetName();
		}
		clips.QSort(0, m_clipDictionary_ClipCount, Compare);
		for(int i = 0; i < m_clipDictionary_ClipCount; i ++)
		{
			m_clipDictionary_Clips[i] = clips[i];
		}
	}
	bank->AddCombo("Clip Name:", &m_clipDictionary_ClipIndex, m_clipDictionary_ClipCount, m_clipDictionary_Clips, datCallback(MFA(CMoveParameterWidgetClip::OnClipDictionary_ClipChanged), (datBase *)this));

	// Add clip widget
	m_clipText = bank->AddText("Clip:", m_clipTextString, 256, true);
}

void CMoveParameterWidgetClip::AddFilePathWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetClip::OnSourceChanged), (datBase *)this));

	// Add file path widgets
	bank->AddText("File Path:", m_filePath_Clip, 256, datCallback(MFA(CMoveParameterWidgetClip::OnFilePathChanged), (datBase *)this));

	// Add clip widget
	m_clipText = bank->AddText("Clip:", m_clipTextString, 256, true);
}

void CMoveParameterWidgetClip::Save()
{
	if(m_clipText)
	{
		if(m_clip)
		{
			strcpy(m_clipTextString, m_clip->GetName());
			m_clipText->SetString(m_clipTextString);
		}
		else
		{
			strcpy(m_clipTextString, "(None Selected)");
			m_clipText->SetString(m_clipTextString);
		}
	}
	fwMvClipId clipId = fwMvClipId( GetName() );
	m_network->SetClip( clipId, m_clip);
}

void CMoveParameterWidgetClip::OnSourceChanged()
{
	bkBank *bank = static_cast< bkBank * >(m_group);

	// Clear clip
	m_clip = NULL;

	// Add source type specific widgets
	switch(static_cast< SOURCE >(m_sourceIndex))
	{
	case eVariableClipset:
		{
			AddVariableClipsetWidgets(bank);
			OnVariableClipsetChanged();
		} break;
	case eAbsoluteClipset:
		{
			AddAbsoluteClipsetWidgets(bank);
			OnAbsoluteClipsetChanged();
		} break;
	case eClipDictionary:
		{
			AddClipDictionaryWidgets(bank);
			OnClipDictionaryChanged();
		} break;
	case eFilePath:
		{
			AddFilePathWidgets(bank);
			OnFilePathChanged();
		} break;
	default:
		{
		} break;
	}
}

void CMoveParameterWidgetClip::OnVariableClipsetChanged()
{
	fwClipSet *clipSet = m_network->GetClipSet(fwMvClipSetVarId(m_variableClipset_Clipset));
	if(clipSet)
	{
		m_clip = clipSet->GetClip(fwMvClipId(m_variableClipset_Clip));
	}
	else
	{
		m_clip = NULL;
	}
	Save();
}

void CMoveParameterWidgetClip::OnAbsoluteClipset_ClipsetChanged()
{
	m_absoluteClipset_ClipIndex = 0;

	bkBank *bank = static_cast< bkBank * >(m_group);
	AddAbsoluteClipsetWidgets(bank);

	OnAbsoluteClipsetChanged();
}

void CMoveParameterWidgetClip::OnAbsoluteClipset_ClipChanged()
{
	OnAbsoluteClipsetChanged();
}

void CMoveParameterWidgetClip::OnAbsoluteClipsetChanged()
{
	fwClipSet *clipSet = fwClipSetManager::GetClipSet(fwMvClipSetId(m_absoluteClipset_Clipsets[m_absoluteClipset_ClipsetIndex]));
	if(clipSet && m_absoluteClipset_ClipIndex < m_absoluteClipset_ClipCount)
	{
		m_clip = clipSet->GetClip(fwMvClipId(m_absoluteClipset_Clips[m_absoluteClipset_ClipIndex]));
	}
	else
	{
		m_clip = NULL;
	}
	Save();
}

void CMoveParameterWidgetClip::OnClipDictionary_ClipDictionaryChanged()
{
	m_clipDictionary_ClipIndex = 0;

	bkBank *bank = static_cast< bkBank * >(m_group);
	AddClipDictionaryWidgets(bank);

	OnClipDictionaryChanged();
}

void CMoveParameterWidgetClip::OnClipDictionary_ClipChanged()
{
	OnClipDictionaryChanged();
}

void CMoveParameterWidgetClip::OnClipDictionaryChanged()
{
	crClipDictionary *clipDictionary = NULL;
	strLocalIndex clipDictionaryIndex = g_ClipDictionaryStore.FindSlot(m_clipDictionary_ClipDictionaries[m_clipDictionary_ClipDictionaryIndex]);
	if(clipDictionaryIndex != -1)
	{
		clipDictionary = g_ClipDictionaryStore.Get(clipDictionaryIndex);
	}
	m_clip = NULL;
	if(clipDictionary && m_clipDictionary_ClipIndex < m_clipDictionary_ClipCount)
	{
		m_clip = clipDictionary->GetClip(m_clipDictionary_Clips[m_clipDictionary_ClipIndex]);
	}
	Save();
}

void CMoveParameterWidgetClip::OnFilePathChanged()
{
	m_clip = NULL;

	// Tokenize into filename + extension
	atString clipNameString(m_filePath_Clip);
	atArray< atString > clipNameTokens;
	clipNameString.Split(clipNameTokens, ".");
	if(clipNameTokens.GetCount() == 2)
	{
		bool assetExists = ASSET.Exists(clipNameTokens[0], clipNameTokens[1]);
		if (assetExists)
		{
			// lossless compression on hot-loaded clips - keep compression techniques cheap
			crAnimToleranceSimple tolerance(SMALL_FLOAT, SMALL_FLOAT, SMALL_FLOAT, SMALL_FLOAT, crAnimTolerance::kDecompressionCostDefault, crAnimTolerance::kCompressionCostLow);

			// load clip directly, compress on load
			crAnimLoaderCompress loader;
			loader.Init(tolerance);

			// WARNING: This is known to be slow and dynamic memory intense.
			// Do NOT profile or bug performance of this call, it is DEV only.
			m_clip = crClip::AllocateAndLoad(m_filePath_Clip, &loader);
		}
	}

	Save();
}

const char *CMoveParameterWidgetExpression::ms_sources[] =
{
	"Expression Dictionary",
	"File Path"
};

CMoveParameterWidgetExpression::CMoveParameterWidgetExpression( fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, bool bUpdateEveryFrame /* = false */)
{
	m_type = kParameterWidgetExpression;
	m_name = paramName;
	m_move = move;
	m_network = handle;
	m_perFrameUpdate = bUpdateEveryFrame;

	m_expressions = NULL;
	strcpy(m_expressionsTextString, "(None Selected)");
	m_expressionsText = NULL;

	m_sourceIndex = eExpressionDictionary;

	m_expressionDictionary_ExpressionDictionaryIndex = 0;
	m_expressionDictionary_ExpressionDictionaryCount = g_ExpressionDictionaryStore.GetNumUsedSlots();
	m_expressionDictionary_ExpressionDictionaries = new const char *[m_expressionDictionary_ExpressionDictionaryCount];
	atArray< const char * > expressionDictionaries; expressionDictionaries.ResizeGrow(m_expressionDictionary_ExpressionDictionaryCount);
	for(int i = 0, j = 0; j < g_ExpressionDictionaryStore.GetCount(); j ++)
	{
		if(g_ExpressionDictionaryStore.IsValidSlot(strLocalIndex(j)))
		{
			expressionDictionaries[i ++] = g_ExpressionDictionaryStore.GetName(strLocalIndex(j));
		}
	}
	expressionDictionaries.QSort(0, m_expressionDictionary_ExpressionDictionaryCount, Compare);
	for(int i = 0; i < m_expressionDictionary_ExpressionDictionaryCount; i ++)
	{
		m_expressionDictionary_ExpressionDictionaries[i] = expressionDictionaries[i];
	}
	m_expressionDictionary_ExpressionsIndex = 0;
	m_expressionDictionary_ExpressionsCount = 0;
	m_expressionDictionary_Expressionss = NULL;

	strcpy(m_filePath_Expression, "");
}

CMoveParameterWidgetExpression::~CMoveParameterWidgetExpression()
{
	delete[] m_expressionDictionary_ExpressionDictionaries; m_expressionDictionary_ExpressionDictionaries = NULL;
	delete[] m_expressionDictionary_Expressionss; m_expressionDictionary_Expressionss = NULL;
}

void CMoveParameterWidgetExpression::AddWidgets(bkBank* bank)
{
	m_group = (bkWidget *)bank->PushGroup(m_name, true);
	{
		// Add source type specific widgets
		switch(static_cast< SOURCE >(m_sourceIndex))
		{
		case eExpressionDictionary:
			{
				AddExpressionDictionaryWidgets(bank);
				OnExpressionDictionaryChanged();
			} break;
		case eFilePath:
			{
				AddFilePathWidgets(bank);
				OnFilePathChanged();
			} break;
		default:
			{
			} break;
		}
	}
	bank->PopGroup();
}

void CMoveParameterWidgetExpression::AddExpressionDictionaryWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetExpression::OnSourceChanged), (datBase *)this));

	// Add expression dictionary widgets
	bank->AddCombo("Expression Dictionary:", &m_expressionDictionary_ExpressionDictionaryIndex, m_expressionDictionary_ExpressionDictionaryCount, m_expressionDictionary_ExpressionDictionaries, datCallback(MFA(CMoveParameterWidgetExpression::OnExpressionDictionary_ExpressionDictionaryChanged), (datBase *)this));
	m_expressionDictionary_ExpressionsCount = 0;
	delete[] m_expressionDictionary_Expressionss; m_expressionDictionary_Expressionss = NULL;
	strLocalIndex expressionDictionaryIndex = g_ExpressionDictionaryStore.FindSlot(m_expressionDictionary_ExpressionDictionaries[m_expressionDictionary_ExpressionDictionaryIndex]);
	
	fwExpressionsDictionaryLoader loader(expressionDictionaryIndex.Get());
	if(expressionDictionaryIndex != -1)
	{
		taskAssertf(loader.IsLoaded(), "Expression dictionary : %s has failed to load\n", m_expressionDictionary_ExpressionDictionaries[m_expressionDictionary_ExpressionDictionaryIndex]);
	}

	crExpressionsDictionary *expressionDictionary = expressionDictionaryIndex != -1 ? g_ExpressionDictionaryStore.Get(expressionDictionaryIndex) : NULL;
	if(expressionDictionary && expressionDictionary->GetCount() > 0)
	{
		m_expressionDictionary_ExpressionsCount = expressionDictionary->GetCount();
		m_expressionDictionary_Expressionss = new const char *[m_expressionDictionary_ExpressionsCount];
		atArray< const char * > expressions; expressions.ResizeGrow(m_expressionDictionary_ExpressionsCount);
		for(int i = 0; i < m_expressionDictionary_ExpressionsCount; i ++)
		{
			expressions[i] = expressionDictionary->GetEntry(i)->GetName();
		}
		expressions.QSort(0, m_expressionDictionary_ExpressionsCount, Compare);
		for(int i = 0; i < m_expressionDictionary_ExpressionsCount; i ++)
		{
			m_expressionDictionary_Expressionss[i] = expressions[i];
		}
	}
	bank->AddCombo("Expressions Name:", &m_expressionDictionary_ExpressionsIndex, m_expressionDictionary_ExpressionsCount, m_expressionDictionary_Expressionss, datCallback(MFA(CMoveParameterWidgetExpression::OnExpressionDictionary_ExpressionChanged), (datBase *)this));

	// Add expression widget
	m_expressionsText = bank->AddText("Expressions:", m_expressionsTextString, 256, true);
}

void CMoveParameterWidgetExpression::AddFilePathWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetExpression::OnSourceChanged), (datBase *)this));

	// Add file path widgets
	bank->AddText("File Path:", m_filePath_Expression, 256, datCallback(MFA(CMoveParameterWidgetExpression::OnFilePathChanged), (datBase *)this));

	// Add expression widget
	m_expressionsText = bank->AddText("Expressions:", m_expressionsTextString, 256, true);
}

void CMoveParameterWidgetExpression::Save()
{
	if(m_expressionsText)
	{
		if(m_expressions)
		{
			strcpy(m_expressionsTextString, m_expressions->GetName());
			m_expressionsText->SetString(m_expressionsTextString);
		}
		else
		{
			strcpy(m_expressionsTextString, "(None Selected)");
			m_expressionsText->SetString(m_expressionsTextString);
		}
	}
	fwMvExpressionsId expressionsId = fwMvExpressionsId( GetName() );
	m_network->SetExpressions( expressionsId, m_expressions);
}

void CMoveParameterWidgetExpression::OnSourceChanged()
{
	bkBank *bank = static_cast< bkBank * >(m_group);

	// Clear expression
	if(m_expressions)
	{
		m_expressions->Release(); m_expressions = NULL;
	}

	// Add source type specific widgets
	switch(static_cast< SOURCE >(m_sourceIndex))
	{
	case eExpressionDictionary:
		{
			AddExpressionDictionaryWidgets(bank);
			OnExpressionDictionaryChanged();
		} break;
	case eFilePath:
		{
			AddFilePathWidgets(bank);
			OnFilePathChanged();
		} break;
	default:
		{
		} break;
	}
}

void CMoveParameterWidgetExpression::OnExpressionDictionary_ExpressionDictionaryChanged()
{
	m_expressionDictionary_ExpressionsIndex = 0;

	bkBank *bank = static_cast< bkBank * >(m_group);
	AddExpressionDictionaryWidgets(bank);

	OnExpressionDictionaryChanged();
}

void CMoveParameterWidgetExpression::OnExpressionDictionary_ExpressionChanged()
{
	OnExpressionDictionaryChanged();
}

void CMoveParameterWidgetExpression::OnExpressionDictionaryChanged()
{
	crExpressionsDictionary *expressionDictionary = NULL;
	strLocalIndex expressionDictionaryIndex = g_ExpressionDictionaryStore.FindSlot(m_expressionDictionary_ExpressionDictionaries[m_expressionDictionary_ExpressionDictionaryIndex]);
	if(expressionDictionaryIndex != -1)
	{
		expressionDictionary = g_ExpressionDictionaryStore.Get(expressionDictionaryIndex);
	}

	// Clear expression
	if(m_expressions)
	{
		m_expressions->Release(); m_expressions = NULL;
	}

	if(expressionDictionary && m_expressionDictionary_ExpressionsIndex < m_expressionDictionary_ExpressionsCount)
	{
		for(int i = 0; i < m_expressionDictionary_ExpressionsCount; i ++)
		{
			if(expressionDictionary->GetEntry(i)->GetName() == m_expressionDictionary_Expressionss[m_expressionDictionary_ExpressionsIndex])
			{
				m_expressions = expressionDictionary->GetEntry(i);
				m_expressions->AddRef();

				break;
			}
		}
	}
	Save();
}

void CMoveParameterWidgetExpression::OnFilePathChanged()
{
	// Clear expression
	if(m_expressions)
	{
		m_expressions->Release(); m_expressions = NULL;
	}

	//see if the file exists
	atString name(m_filePath_Expression);

	atArray<atString> splitName;
	name.Split(splitName, '.');

	if (splitName.GetCount()>1)
	{
		if (ASSET.Exists(splitName[0], splitName[1]))
		{
			//if so, allocate and load
			m_expressions = crExpressions::AllocateAndLoad(m_filePath_Expression, true);
			m_expressions->AddRef();
		}
	}

	Save();
}

const char *CMoveParameterWidgetFilter::ms_sources[] =
{
	"Bone Filter Manager",
	"File Path"
};

CMoveParameterWidgetFilter::CMoveParameterWidgetFilter( fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, bool bUpdateEveryFrame /* = false */)
{
	m_type = kParameterWidgetFilter;
	m_name = paramName;
	m_move = move;
	m_network = handle;
	m_perFrameUpdate = bUpdateEveryFrame;

	m_filter = NULL;
	strcpy(m_filterTextString, "(None Selected)");
	m_filterText = NULL;

	m_sourceIndex = eBoneFilterManager;

	crFrameFilterDictionary *frameFilterDictionary = NULL;

	// Find filter set slot
	strLocalIndex filterSetSlot = g_FrameFilterDictionaryStore.FindSlotFromHashKey(FILTER_SET_PLAYER);

	fwFrameFilterDictionaryLoader loader(filterSetSlot.Get(), false);
	if(loader.IsLoaded() || loader.Load(true))
	{
		g_FrameFilterDictionaryStore.AddRef(filterSetSlot, REF_OTHER);
	}
	frameFilterDictionary = loader.GetDictionary();

	m_boneFilterManager_FilterIndex = 0;
	if(frameFilterDictionary)
	{
		m_boneFilterManager_FilterCount = frameFilterDictionary->GetCount();
		m_boneFilterManager_Filters = new const char *[m_boneFilterManager_FilterCount];
		atArray< const char * > filterDictionaries; filterDictionaries.ResizeGrow(m_boneFilterManager_FilterCount);
		for(int i = 0; i < m_boneFilterManager_FilterCount; i ++)
		{
			filterDictionaries[i] = atHashString(frameFilterDictionary->GetCode(i)).GetCStr();
		}
		filterDictionaries.QSort(0, m_boneFilterManager_FilterCount, Compare);
		for(int i = 0; i < m_boneFilterManager_FilterCount; i ++)
		{
			m_boneFilterManager_Filters[i] = filterDictionaries[i];
		}
	}
	else
	{
		taskAssert(false);

		m_boneFilterManager_FilterCount = 0;
		m_boneFilterManager_Filters = NULL;
	}

	strcpy(m_filePath_Filter, "");
}

CMoveParameterWidgetFilter::~CMoveParameterWidgetFilter()
{
	delete[] m_boneFilterManager_Filters; m_boneFilterManager_Filters = NULL;
}

void CMoveParameterWidgetFilter::AddWidgets(bkBank* bank)
{
	m_group = (bkWidget *)bank->PushGroup(m_name, true);
	{
		// Add source type specific widgets
		switch(static_cast< SOURCE >(m_sourceIndex))
		{
		case eBoneFilterManager:
			{
				AddBoneFilterManagerWidgets(bank);
				OnBoneFilterManagerChanged();
			} break;
		case eFilePath:
			{
				AddFilePathWidgets(bank);
				OnFilePathChanged();
			} break;
		default:
			{
			} break;
		}
	}
	bank->PopGroup();
}

void CMoveParameterWidgetFilter::AddBoneFilterManagerWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	//bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetFilter::OnSourceChanged), (datBase *)this));

	// Add filter dictionary widgets
	bank->AddCombo("Filter Name:", &m_boneFilterManager_FilterIndex, m_boneFilterManager_FilterCount, m_boneFilterManager_Filters, datCallback(MFA(CMoveParameterWidgetFilter::OnBoneFilterManagerChanged), (datBase *)this));

	// Add filter widget
	m_filterText = bank->AddText("Filter:", m_filterTextString, 256, true);
}

void CMoveParameterWidgetFilter::AddFilePathWidgets(bkBank *bank)
{
	// Remove existing widgets
	while(m_group->GetChild())
	{
		m_group->GetChild()->Destroy();
	}

	// Add source widget
	//bank->AddCombo("Source", &m_sourceIndex, NUM_SOURCES, ms_sources, datCallback(MFA(CMoveParameterWidgetFilter::OnSourceChanged), (datBase *)this));

	// Add file path widgets
	bank->AddText("File Path:", m_filePath_Filter, 256, datCallback(MFA(CMoveParameterWidgetFilter::OnFilePathChanged), (datBase *)this));

	// Add filter widget
	m_filterText = bank->AddText("Filter:", m_filterTextString, 256, true);
}

void CMoveParameterWidgetFilter::Save()
{
	if(m_filterText)
	{
		if(m_filter)
		{
			const char *filterName = g_FrameFilterDictionaryStore.FindFrameFilterNameSlow(m_filter);
			strcpy(m_filterTextString, filterName);
			m_filterText->SetString(m_filterTextString);
		}
		else
		{
			strcpy(m_filterTextString, "(None Selected)");
			m_filterText->SetString(m_filterTextString);
		}
	}
	fwMvFilterId filterId = fwMvFilterId( GetName() );
	m_network->SetFilter( filterId, m_filter);
}

void CMoveParameterWidgetFilter::OnSourceChanged()
{
	bkBank *bank = static_cast< bkBank * >(m_group);

	// Clear filter
	m_filter = NULL;

	// Add source type specific widgets
	switch(static_cast< SOURCE >(m_sourceIndex))
	{
	case eBoneFilterManager:
		{
			AddBoneFilterManagerWidgets(bank);
			OnBoneFilterManagerChanged();
		} break;
	case eFilePath:
		{
			AddFilePathWidgets(bank);
			OnFilePathChanged();
		} break;
	default:
		{
		} break;
	}
}

void CMoveParameterWidgetFilter::OnBoneFilterManagerChanged()
{
	m_filter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(m_boneFilterManager_Filters[m_boneFilterManager_FilterIndex]));
	Save();
}

void CMoveParameterWidgetFilter::OnFilePathChanged()
{
	m_filter = NULL;

	//see if the file exists
	atString name(m_filePath_Filter);

	atArray<atString> splitName;
	name.Split(splitName, '.');

	if (splitName.GetCount()>1)
	{
		if (ASSET.Exists(splitName[0], splitName[1]))
		{
			//if so, allocate and load
		}
	}

	Save();
}

float CTaskPreviewMoveNetwork::sm_blendDuration = 0.0f;

CDebugFilterSelector CTaskPreviewMoveNetwork::sm_filterSelector;

bool CTaskPreviewMoveNetwork::sm_secondary = false;

bool  CTaskPreviewMoveNetwork::sm_Task_TagSyncContinuous = false;
bool  CTaskPreviewMoveNetwork::sm_Task_TagSyncTransition = false;
bool  CTaskPreviewMoveNetwork::sm_Task_DontDisableParamUpdate = false;
bool  CTaskPreviewMoveNetwork::sm_Task_IgnoreMoverBlendRotation = false;
bool  CTaskPreviewMoveNetwork::sm_Task_IgnoreMoverBlend = false;

bool  CTaskPreviewMoveNetwork::sm_Secondary_TagSyncContinuous = false;
bool  CTaskPreviewMoveNetwork::sm_Secondary_TagSyncTransition = false;

bool  CTaskPreviewMoveNetwork::sm_BlockMovementTasks = false;

CTask::FSM_Return CTaskPreviewMoveNetwork::ProcessPreFSM()
{
	if (sm_BlockMovementTasks && GetEntity()->GetType() == ENTITY_TYPE_PED)
	{
		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_IsHigherPriorityClipControllingPed, true );
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskPreviewMoveNetwork::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		// Entry point
	FSM_State(State_Start)
		FSM_OnUpdate
			return Start_OnUpdate();

	// Play an clip
	FSM_State(State_PreviewNetwork)
		FSM_OnEnter
			PreviewNetwork_OnEnter();
		FSM_OnUpdate
			return PreviewNetwork_OnUpdate();
		FSM_OnExit
			PreviewNetwork_OnExit();

	FSM_State(State_Exit)
		FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}

const char * CTaskPreviewMoveNetwork::GetStaticStateName( s32 iState )
{
	taskAssert(iState>=0&&iState<=State_Exit);
	static const char* aPreviewMoveNetworkStateNames[] = 
	{
		"State_Start",
		"State_PreviewNetwork",
		"State_Exit"
	};

	return aPreviewMoveNetworkStateNames[iState];
}

CTaskPreviewMoveNetwork::CTaskPreviewMoveNetwork(const char * fileName)
	: m_animateJoints(false)
{
	m_networkDef = m_moveNetworkHelper.LoadNetworkDefFromDisk(fileName);

	m_networkFileName = fileName;
	SetInternalTaskType(CTaskTypes::TASK_PREVIEW_MOVE_NETWORK);
}

CTaskPreviewMoveNetwork::CTaskPreviewMoveNetwork(const mvNetworkDef* networkDef)
	: m_animateJoints(false)
{
	m_networkDef = networkDef;
	SetInternalTaskType(CTaskTypes::TASK_PREVIEW_MOVE_NETWORK);
}

CTaskPreviewMoveNetwork::~CTaskPreviewMoveNetwork()
{
	m_networkFileName.Clear();
}

#if !__FINAL

int GetChildInteger(parTreeNode* node, const char * name)
{
	parTreeNode* childNode = node->FindChildWithName(name);
	
	if (childNode)
	{
		return childNode->ReadStdLeafInt();
	}
	
	return 0;
}
float GetChildFloat(parTreeNode* node, const char * name)
{
	parTreeNode* childNode = node->FindChildWithName(name);

	if (childNode)
	{
		return childNode->ReadStdLeafFloat();
	}

	return 0.0f;
}
bool GetChildBool(parTreeNode* node, const char * name)
{
	parTreeNode* childNode = node->FindChildWithName(name);

	if (childNode)
	{
		return childNode->ReadStdLeafBool();
	}
	return false;
}
const char * GetChildString(parTreeNode* node, const char * name)
{
	parTreeNode* childNode = node->FindChildWithName(name);

	if (childNode)
	{
		return childNode->GetData();
	}
	return NULL;
}

void CTaskPreviewMoveNetwork::LoadParamWidgets()
{
	if(m_networkFileName.c_str() == NULL)
	{
		return; 
	}
	
	parTree *paramTree = PARSER.LoadTree(m_networkFileName.c_str(), "xml");

	if (paramTree)
	{
		parTreeNode* rootNode = paramTree->GetRoot();
	
		parTreeNode* paramNode = rootNode->GetChild();

		while( paramNode )
		{
			int paramType = GetChildInteger(paramNode, "type");
			CMoveParameterWidget* newParam = NULL;

			fwMove* pMove = &(GetPed()->GetAnimDirector()->GetMove());
			fwMoveNetworkPlayer* handle = m_moveNetworkHelper.GetNetworkPlayer();

			const char * name = GetChildString(paramNode, "name");
			bool perFrame = GetChildBool(paramNode, "perframe");
			float min = GetChildFloat(paramNode, "min");
			float max = GetChildFloat(paramNode, "max");
			bool output = GetChildBool(paramNode, "output");
			int controller = GetChildInteger(paramNode, "controller");

			switch ((CMoveParameterWidget::ParameterWidgetType)paramType)
			{
			case CMoveParameterWidget::kParameterWidgetReal:
				newParam = rage_new CMoveParameterWidgetReal( pMove, handle, name, perFrame, min, max, output, controller);
				break;
			case CMoveParameterWidget::kParameterWidgetBool:
				newParam = rage_new CMoveParameterWidgetBool(  pMove, handle, name, perFrame, output, controller);
				break;
			case CMoveParameterWidget::kParameterWidgetFlag:
				newParam = rage_new CMoveParameterWidgetFlag(  pMove, handle, name, controller);
				break;
			case CMoveParameterWidget::kParameterWidgetRequest:
				newParam = rage_new CMoveParameterWidgetRequest( pMove, handle, name, controller);
				break;
			case CMoveParameterWidget::kParameterWidgetClip:
				newParam = rage_new CMoveParameterWidgetClip( pMove, handle, name, perFrame);
				break;
			case CMoveParameterWidget::kParameterWidgetExpression:
				newParam = rage_new CMoveParameterWidgetExpression( pMove, handle, name, perFrame);
				break;
			case CMoveParameterWidget::kParameterWidgetFilter:
				newParam = rage_new CMoveParameterWidgetFilter( pMove, handle, name, perFrame);
				break;
			case CMoveParameterWidget::kParameterWidgetEvent:
				newParam = rage_new CMoveParameterWidgetEvent( pMove, handle, name, controller);
				break; 
			default:
				break;
			}

			AddParameterHookup(newParam);

			paramNode = paramNode->GetSibling();
		}

		delete paramTree;
	}
}

void CTaskPreviewMoveNetwork::SaveParamWidgets()
{
	parTree *paramTree = rage_new parTree();

	if (paramTree)
	{
		parTreeNode* rootNode =  rage_new parTreeNode("ParameterWidgets");
		paramTree->SetRoot(rootNode);

		for (int i=0; i<m_widgetList.GetCount(); i++)
		{
			CMoveParameterWidget* widget = m_widgetList[i];

			parTreeNode* paramNode = rage_new parTreeNode("parameter");

			parTreeNode* nameNode = rage_new parTreeNode("name");
			nameNode->SetData(widget->GetName(), ustrlen(widget->GetName())+1);
			nameNode->AppendAsChildOf(paramNode);

			paramNode->AppendStdLeafChild<s64>("type",widget->GetType());

			switch(widget->GetType())
			{
			case CMoveParameterWidget::kParameterWidgetReal:
				{
					CMoveParameterWidgetReal* pReal = static_cast<CMoveParameterWidgetReal*>(widget);
					paramNode->AppendStdLeafChild<bool>("perframe", pReal->m_perFrameUpdate );
					paramNode->AppendStdLeafChild<bool>("output", pReal->m_outputMode);
					paramNode->AppendStdLeafChild<s64>("controller", pReal->m_controlValue);
					paramNode->AppendStdLeafChild<float>("min", pReal->m_min);
					paramNode->AppendStdLeafChild<float>("max", pReal->m_max);
				}
			break;

			case CMoveParameterWidget::kParameterWidgetEvent:
				{
					CMoveParameterWidgetEvent* pEvent = static_cast<CMoveParameterWidgetEvent*>(widget);
					paramNode->AppendStdLeafChild<bool>("perframe", widget->m_perFrameUpdate );
					paramNode->AppendStdLeafChild<bool>("output", widget->m_outputMode);
					paramNode->AppendStdLeafChild<s64>("controller", pEvent->m_controlValue);
				}
			break;

			case CMoveParameterWidget::kParameterWidgetBool:
				{
					CMoveParameterWidgetBool* pBool = static_cast<CMoveParameterWidgetBool*>(widget);
					paramNode->AppendStdLeafChild<bool>("perframe", widget->m_perFrameUpdate );
					paramNode->AppendStdLeafChild<bool>("output", widget->m_outputMode);
					paramNode->AppendStdLeafChild<s64>("controller", pBool->m_controlValue);
				}			
				break;
			case CMoveParameterWidget::kParameterWidgetRequest:
				{
					CMoveParameterWidgetRequest* pReq = static_cast<CMoveParameterWidgetRequest*>(widget);
					paramNode->AppendStdLeafChild<s64>("controller", pReq->m_controlValue);
				}
				break; 
			case CMoveParameterWidget::kParameterWidgetFlag:
				{
					CMoveParameterWidgetFlag* pReq = static_cast<CMoveParameterWidgetFlag*>(widget);
					paramNode->AppendStdLeafChild<s64>("controller", pReq->m_controlValue);
				}
				break; 
			case CMoveParameterWidget::kParameterWidgetClip:
				paramNode->AppendStdLeafChild<bool>("perframe", widget->m_perFrameUpdate );
				break;
			case CMoveParameterWidget::kParameterWidgetExpression:
				paramNode->AppendStdLeafChild<bool>("perframe", widget->m_perFrameUpdate );
				break;
			case CMoveParameterWidget::kParameterWidgetFilter:
				paramNode->AppendStdLeafChild<bool>("perframe", widget->m_perFrameUpdate );
				break;
			
			default:
				break;
			}

			paramNode->AppendAsChildOf(rootNode);
		}

		PARSER.SaveTree(m_networkFileName.c_str(), "xml", paramTree);

		delete paramTree;
	}
}

#endif //__FINAL

void CTaskPreviewMoveNetwork::CleanUp()
{
	if (m_moveNetworkHelper.IsNetworkAttached())
	{
		if (GetEntity()->GetType() == ENTITY_TYPE_PED)
		{
			CPed* pPed = GetPed();
			pPed->GetMovePed().ClearSecondaryTaskNetwork(m_moveNetworkHelper, sm_blendDuration, sm_Secondary_TagSyncTransition ? CMovePed::Secondary_TagSyncTransition : CMovePed::Secondary_None);
			pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, sm_blendDuration, sm_Task_TagSyncTransition ? CMovePed::Task_TagSyncTransition : CMovePed::Task_None);
		}
		else if (GetEntity()->GetType() == ENTITY_TYPE_OBJECT)
		{
			CObject* pObj = GetObject();
			if (pObj->GetAnimDirector())
			{
				pObj->GetMoveObject().ClearNetwork(m_moveNetworkHelper, 0.0f);
			}
		}
		else if (GetEntity()->GetType() == ENTITY_TYPE_VEHICLE)
		{
			CVehicle* pVeh = GetVehicle();
			if (pVeh->GetAnimDirector())
			{
				pVeh->GetMoveVehicle().ClearNetwork(m_moveNetworkHelper, 0.0f);
			}

			pVeh->m_nVehicleFlags.bAnimateJoints = m_animateJoints;
		}
	}

	if (m_networkDef)
	{
		if(m_networkFileName.c_str() != NULL)
		{
			delete m_networkDef;
		}
		
		m_networkDef = NULL;
	}

	//Clean up the parameter widgets
	for (int i=0; i<m_widgetList.GetCount(); ++i)
	{
		m_widgetList[i]->DeleteWidgets();
		delete m_widgetList[i];
		m_widgetList[i] = NULL;
	}

	if (m_widgetList.GetCount()>0) m_widgetList.clear();
}

CTask::FSM_Return CTaskPreviewMoveNetwork::Start_OnUpdate()
{
	taskAssert(GetEntity()->GetType() == ENTITY_TYPE_PED || GetEntity()->GetType() == ENTITY_TYPE_OBJECT || GetEntity()->GetType() == ENTITY_TYPE_VEHICLE);
	if (!m_networkDef)
	{
		taskAssertf(m_networkDef, "Error previewing MoVE network: network def is not loaded.");
		SetState(State_Exit);
	}

	if (CAnimViewer::m_moveNetworkClipSet)
	{
		fwMvClipSetId clipSetId(fwClipSetManager::GetClipSetNames()[CAnimViewer::m_moveNetworkClipSet]);
		fwClipSetManager::StreamIn_DEPRECATED(clipSetId);
	}

	//also assert we have a network def here
	SetState(State_PreviewNetwork);

	return FSM_Continue;
}

void CTaskPreviewMoveNetwork::PreviewNetwork_OnEnter()
{
	if (GetEntity()->GetType() == ENTITY_TYPE_PED)
	{
		CPed* pPed = GetPed();

		// Add the network to the ped's fwMove
		taskAssert(pPed);
		taskAssert(pPed->GetAnimDirector());
	
		m_moveNetworkHelper.CreateNetworkPlayer(pPed, m_networkDef);

		if (sm_secondary)
		{
			u32 secondaryFlags = CMovePed::Secondary_None;

			if (sm_Secondary_TagSyncContinuous)
				secondaryFlags|=CMovePed::Secondary_TagSyncContinuous;
			if (sm_Secondary_TagSyncTransition)
				secondaryFlags|=CMovePed::Secondary_TagSyncTransition;

			pPed->GetMovePed().SetSecondaryTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), sm_blendDuration, secondaryFlags, sm_filterSelector.GetSelectedFilterId());

		}
		else
		{
			u32 taskFlags = CMovePed::Task_None;

			if (sm_Task_IgnoreMoverBlend)
				taskFlags|=CMovePed::Task_IgnoreMoverBlend;
			if (sm_Task_IgnoreMoverBlendRotation)
				taskFlags|=CMovePed::Task_IgnoreMoverBlendRotation;
			if (sm_Task_TagSyncContinuous)
				taskFlags|=CMovePed::Task_TagSyncContinuous;
			if (sm_Task_TagSyncTransition)
				taskFlags|=CMovePed::Task_TagSyncTransition;

			pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), sm_blendDuration, taskFlags, sm_filterSelector.GetSelectedFilterId());
		}
	}
	else if (GetEntity()->GetType() == ENTITY_TYPE_OBJECT)
	{
		CObject* pObj = GetObject();
		m_moveNetworkHelper.CreateNetworkPlayer(pObj, m_networkDef);

		pObj->GetMoveObject().SetNetwork(m_moveNetworkHelper.GetNetworkPlayer(), sm_blendDuration);
	}
	else if (GetEntity()->GetType() == ENTITY_TYPE_VEHICLE)
	{
		CVehicle* pVeh = GetVehicle();
		m_moveNetworkHelper.CreateNetworkPlayer(pVeh, m_networkDef);

		pVeh->GetMoveVehicle().SetNetwork(m_moveNetworkHelper.GetNetworkPlayer(), sm_blendDuration);
		m_animateJoints = pVeh->m_nVehicleFlags.bAnimateJoints;
	}

	fwMvClipSetId clipSetId(fwClipSetManager::GetClipSetNames()[CAnimViewer::m_moveNetworkClipSet]);
	m_moveNetworkHelper.SetClipSet(clipSetId);

#if __DEV
	if(CAnimViewer::m_previewNetworkGroup)
	{
		if (GetEntity()->GetType() == ENTITY_TYPE_PED || GetEntity()->GetType() == ENTITY_TYPE_OBJECT || GetEntity()->GetType() == ENTITY_TYPE_VEHICLE)
		{
			LoadParamWidgets();
			CAnimViewer::AddParamWidgetsFromEntity(static_cast< CDynamicEntity * >(GetEntity()));
		}
	}
#endif
}

CTask::FSM_Return CTaskPreviewMoveNetwork::PreviewNetwork_OnUpdate()
{
	// Update any parameters, etc here
	for (int i=0; i<m_widgetList.GetCount(); ++i)
	{
		m_widgetList[i]->Update();
	}
	
	return FSM_Continue;
}

void CTaskPreviewMoveNetwork::PreviewNetwork_OnExit()
{
	taskAssert(GetEntity()->GetType() == ENTITY_TYPE_PED || GetEntity()->GetType() == ENTITY_TYPE_OBJECT || GetEntity()->GetType() == ENTITY_TYPE_VEHICLE);
}

void CTaskPreviewMoveNetwork::AddWidgets(bkBank* pBank)
{
	for (int i=0; i<m_widgetList.GetCount(); ++i)
	{
		m_widgetList[i]->AddWidgets(pBank);
	}
}

void CTaskPreviewMoveNetwork::RemoveWidgets()
{
	for (int i=0; i<m_widgetList.GetCount(); ++i)
	{
		m_widgetList[i]->DeleteWidgets();
	}
}

void CTaskPreviewMoveNetwork::ClearParamWidgets()
{
	for (int i=0; i<m_widgetList.GetCount(); i++)
	{
		m_widgetList[i]->DeleteWidgets();
		delete m_widgetList[i];
		m_widgetList[i] = NULL;
	}
	m_widgetList.clear();
}

void CTaskPreviewMoveNetwork::ApplyClipSet(const fwMvClipSetId &clipSetId, const fwMvClipSetVarId &clipSetVarId /* = CLIP_SET_VAR_ID_DEFAULT */)
{
	fwClipSetManager::StreamIn_DEPRECATED(clipSetId);
	m_moveNetworkHelper.SetClipSet(clipSetId, clipSetVarId);
}

void CTaskPreviewMoveNetwork::AddParameterHookup(CMoveParameterWidget* pParam)
{
	m_widgetList.PushAndGrow(pParam);
}

aiTask* CTaskPreviewMoveNetwork::Copy() const
{ 
	CTaskPreviewMoveNetwork* pTask = rage_new CTaskPreviewMoveNetwork(m_networkDef); 

	for (int i=0; i<m_widgetList.GetCount(); ++i)
	{
		pTask->AddParameterHookup(m_widgetList[i]);
	}

	return pTask;
}

//////////////////////////////////////////////////////////////////////////
//	TaskTestMoveNetwork
//	A dev only task for testing more complex issues related to MoVE 
//	network and the a.i. system. Modify this of you need to create
//	a straightforward repro for a MoVE related bug.
//////////////////////////////////////////////////////////////////////////

const fwMvFlagId CTaskTestMoveNetwork::ms_FlagId("flag",0x3BA81545);
const fwMvFloatId CTaskTestMoveNetwork::ms_PhaseId("phase",0xA27F482B);
const fwMvRequestId CTaskTestMoveNetwork::ms_RequestId("request",0x1B69EB10);
const fwMvFloatId CTaskTestMoveNetwork::ms_DurationId("duration",0xDB0B92D5);
const fwMvNetworkId CTaskTestMoveNetwork::ms_Subnetwork0Id("Subnetwork0",0x51D03B60);

float CTaskTestMoveNetwork::ms_ReferencePhase = 0.5f;
float CTaskTestMoveNetwork::ms_TransitionDuration = 0.25f;

CTask::FSM_Return CTaskTestMoveNetwork::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		// Entry point
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_RunningTest)
		FSM_OnUpdate
		return RunningTest_OnUpdate();

	FSM_State(State_Exit)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskTestMoveNetwork::GetStaticStateName( s32 iState )
{
	taskAssert(iState>=0&&iState<=State_Exit);
	static const char* aPreviewMoveNetworkStateNames[] = 
	{
		"State_Start",
		"State_RunningTest",
		"State_Exit"
	};

	return aPreviewMoveNetworkStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////////

void CTaskTestMoveNetwork::CleanUp()
{
	GetPed()->GetMovePed().ClearTaskNetwork(m_parentNetwork);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskTestMoveNetwork::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	mvNetworkDef* pDef = m_parentNetwork.LoadNetworkDefFromDisk("assets:/clip/move/networks/Preview/TagSyncCompare.mrf");
	
	m_parentNetwork.CreateNetworkPlayer(GetPed(), pDef);

	CMovePed& move = pPed->GetMovePed();

	move.SetMotionTaskNetwork(m_parentNetwork, 0.0f);

	SetState(State_RunningTest);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskTestMoveNetwork::RunningTest_OnUpdate()
{

	float phase = m_parentNetwork.GetFloat(ms_PhaseId);
	if (ms_ReferencePhase>= m_previousPhase && ms_ReferencePhase<phase)
	{
		m_parentNetwork.SendRequest(ms_RequestId);
		m_parentNetwork.SetFloat(ms_DurationId, ms_TransitionDuration);
	}
	m_previousPhase = phase;

	return FSM_Continue;
}


#endif //__BANK

const char *CTaskSynchronizedScene::ms_facialClipSuffix = "_facial";
const eSyncedSceneFlagsBitSet CTaskSynchronizedScene::SYNCED_SCENE_NONE;
const eRagdollBlockingFlagsBitSet CTaskSynchronizedScene::RBF_NONE;

//////////////////////////////////////////////////////////////////////////
//	Task Synchronized Scene
//	Plays a synchronized scene on a ped
//////////////////////////////////////////////////////////////////////////

CTaskSynchronizedScene::CTaskSynchronizedScene
( 
	fwSyncedSceneId	_scene, 
	u32								_clipPartialHash, 
	strStreamingObjectName			_dictionary, 
	float							_blendIn , 
	float							_blendOut,
	eSyncedSceneFlagsBitSet			_flags,
	eRagdollBlockingFlagsBitSet		_ragdollBlockingFlags,
	float							_moverBlendIn,
    int                             networkSceneID,
	eIkControlFlagsBitSet			_ikFlags
					   )
	: m_startPosition(VEC3_ZERO)
	, m_startVelocity(VEC3_ZERO)
	, m_startOrientation(Quaternion::sm_I)
	, m_StartSituationValid(false)
	, m_FrameMoverLastProcessed(0)
{
	m_scene = _scene;
	m_clipPartialHash = _clipPartialHash;
	m_dictionary = _dictionary;
	m_blendInDelta = _blendIn;
	m_blendOutDelta = _blendOut;
	m_moverBlendRate = _moverBlendIn;
    m_NetworkSceneID = networkSceneID;
	m_fVehicleLightingScalar = 0.0f;

	m_DisabledMapCollision = false;
	m_DisabledGravity = false;
	m_ExitScene = false;
	m_NetworkCoordinateScenePhase = false;
    m_EstimatedCompletionTime = 0;

	m_flags = _flags;
	m_ragdollBlockingFlags = _ragdollBlockingFlags;
	m_ikFlags = _ikFlags;

#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED

	//Assert that the flags are valid.
	taskAssertf(IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS) || !IsSceneFlagSet(SYNCED_SCENE_PRESERVE_VELOCITY), "Unable to preserve velocity if physics is not used.");

#if __DEV
	static bool s_ForceNotInterruptable = false;
	if (s_ForceNotInterruptable)
	{
		m_flags.BitSet().Set(SYNCED_SCENE_DONT_INTERRUPT, true);
	}
#endif //__DEV

	// have to add an extra ref in the task constructor because it's possible for the task to be
	// deleted before it is ever run. This can result in scripts leaking synchronised scenes
	// (because the scene never has a ref added to it, and as a result will never be released either).
	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_scene))
	{
		fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(m_scene);

		//find the clip
		int dictionaryIndex = fwAnimManager::FindSlotFromHashKey(m_dictionary.GetHash()).Get();
		atHashString clip = atFinalizeHash(m_clipPartialHash);
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex( dictionaryIndex, clip.GetHash());
		if (taskVerifyf(pClip, "Could not find clip (dict %s %u, clip %s %u) for Network Scene ID: %d!", m_dictionary.TryGetCStr(), m_dictionary.GetHash(), clip.TryGetCStr(), clip.GetHash(), networkSceneID))
		{
			// if we have a valid clip and the scene duration has not been intialized, set it here
			// (have to do this here or the scene will not progress whilst in the homing state)
			if(pClip)
			{
				if(pClip->GetDuration() > fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(m_scene))
				{
					fwAnimDirectorComponentSyncedScene::SetSyncedSceneDuration(m_scene, pClip->GetDuration());
				}
			}
		}
	}
	SetInternalTaskType(CTaskTypes::TASK_SYNCHRONIZED_SCENE);
}

CTaskSynchronizedScene::CTaskSynchronizedScene(const CTaskSynchronizedScene &taskToCopy)
{
    m_scene                  = taskToCopy.m_scene;
	m_clipPartialHash        = taskToCopy.m_clipPartialHash;
	m_dictionary             = taskToCopy.m_dictionary;
	m_blendInDelta           = taskToCopy.m_blendInDelta;
	m_blendOutDelta          = taskToCopy.m_blendOutDelta;
    m_moverBlend             = taskToCopy.m_moverBlend;
	m_moverBlendRate         = taskToCopy.m_moverBlendRate;
	m_flags                  = taskToCopy.m_flags;
	m_ragdollBlockingFlags   = taskToCopy.m_ragdollBlockingFlags;
	m_ikFlags			     = taskToCopy.m_ikFlags;
	m_startPosition          = taskToCopy.m_startPosition;
    m_startOrientation       = taskToCopy.m_startOrientation;
    m_startVelocity          = taskToCopy.m_startVelocity;
	m_StartSituationValid	 = taskToCopy.m_StartSituationValid;
    m_startMoveBlendRatio    = taskToCopy.m_startMoveBlendRatio;
    m_NetworkSceneID         = taskToCopy.m_NetworkSceneID;
	m_fVehicleLightingScalar = taskToCopy.m_fVehicleLightingScalar;
	m_FrameMoverLastProcessed = taskToCopy.m_FrameMoverLastProcessed;

	m_NetworkCoordinateScenePhase = taskToCopy.m_NetworkCoordinateScenePhase;
    m_EstimatedCompletionTime = 0;

#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED

	m_DisabledMapCollision = taskToCopy.m_DisabledMapCollision;
	m_DisabledGravity = taskToCopy.m_DisabledGravity;
	m_ExitScene = taskToCopy.m_ExitScene;

	//Assert that the flags are valid.
	taskAssertf(IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS) || !IsSceneFlagSet(SYNCED_SCENE_PRESERVE_VELOCITY), "Unable to preserve velocity if physics is not used.");

    if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_scene))
	{
		fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(m_scene);
    }

    SetStateFromNetwork(taskToCopy.GetState());
    SetIsMigrating(true);
	SetInternalTaskType(CTaskTypes::TASK_SYNCHRONIZED_SCENE);
}

CTaskSynchronizedScene::~CTaskSynchronizedScene()
{	
	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_scene))
	{
		fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_scene);
	}

#if FPS_MODE_SUPPORTED
	for (int helper = 0; helper < 2; ++helper)
	{
		if (m_apFirstPersonIkHelper[helper])
		{
			delete m_apFirstPersonIkHelper[helper];
			m_apFirstPersonIkHelper[helper] = NULL;
		}
	}
#endif // FPS_MODE_SUPPORTED
}

CTaskSynchronizedScene::FSM_Return CTaskSynchronizedScene::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		// Entry point
	FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_Homing)
		FSM_OnEnter
		Homing_OnEnter();
	FSM_OnUpdate
		return Homing_OnUpdate();

	// Play an clip
	FSM_State(State_RunningScene)
		FSM_OnEnter
			RunningScene_OnEnter();
		FSM_OnUpdate
			return RunningScene_OnUpdate();
		FSM_OnExit
			RunningScene_OnExit();

	FSM_State(State_Exit)
		FSM_OnUpdate
		return QuitScene_OnUpdate();
	FSM_End
}

//////////////////////////////////////////////////////////////////////////
bool CTaskSynchronizedScene::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
{
	CPed* pPed = TryGetPed();
	if (pPed)
	{
		Vector3 desiredVel = pPed->GetAnimatedVelocity();
		float desiredAngVel = pPed->GetAnimatedAngularVelocity();

		desiredVel.RotateZ(pPed->GetCurrentHeading()+(desiredAngVel*fwTimer::GetTimeStep()));

		pPed->SetDesiredVelocity(desiredVel);
		pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, desiredAngVel));
	}

	return false; 
}

bool CTaskSynchronizedScene::ProcessPostMovement()
{
	CPhysical* pPhys = GetPhysical();

	if (pPhys->GetIsTypePed())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_AllowUpdateIfNoCollisionLoaded, true);
	}
	
	if (pPhys && IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
	{
		// need to do this here as it is reset in postphysics,
		// and we may need to reference it whilst processing events
		pPhys->SetShouldUseKinematicPhysics(true);
	}

	if (m_moverBlend < 1.0f)
	{
		InterpolateMover();
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskSynchronizedScene::ProcessPreFSM()
{
	CPhysical* pPhys  = GetPhysical();

	if (pPhys)
	{
		if (m_StartSituationValid && m_FrameMoverLastProcessed!=fwTimer::GetFrameCount())
		{
			float deltaTime = fwTimer::GetTimeStep();
			static bool s_ApplyVelocityChange = true;
			if (s_ApplyVelocityChange)
			{
				// update the supporting data
				m_startPosition += (m_startVelocity*deltaTime);
			}
			m_moverBlend += deltaTime*m_moverBlendRate;
			m_moverBlend = Clamp(m_moverBlend, 0.0f, 1.0f);
			m_FrameMoverLastProcessed=fwTimer::GetFrameCount();
		}

		fwAnimDirectorComponentSyncedScene *animDirectorComponentSyncedScene = pPhys->GetAnimDirector()?pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>():NULL;
		if(GetState()==State_RunningScene && !animDirectorComponentSyncedScene)
		{
			animTaskDebugf(this, "%s", "Exiting task, synced scene component has been removed");
			return FSM_Quit;
		}

		// Exit if the scene id is no longer valid - synced scene may have been forcibly stopped due to the pool becoming full
		if (animDirectorComponentSyncedScene && !fwAnimDirectorComponentSyncedScene::IsValidSceneId(animDirectorComponentSyncedScene->GetSyncedSceneId()))
		{
			animTaskDebugf(this, "Exiting scene - sceneId %d is no longer valid. Synced scene may have been forcibly stopped due to the synced scene pool being full.", animDirectorComponentSyncedScene->GetSyncedSceneId());
			return FSM_Quit;
		}

		if ( pPhys->GetIsTypePed())
		{
			CPed* pPed = GetPed();
			pPed->SetPedResetFlag(CPED_RESET_FLAG_AllowUpdateIfNoCollisionLoaded, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleMapCollision, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

			// Allow sleeping ragdolls to wake up from explosions.
			if(!IsRagdollBlockingFlagSet(RBF_EXPLOSION))
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceExplosionCollisions, true);
			}

			pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockRagdollFromVehicleFallOff, true);

			if (NetworkInterface::IsInCopsAndCrooks())
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableActionMode, true);
			}

			CTaskScriptedAnimation::ProcessIkControlFlags(*pPed, m_ikFlags);

			// don't allow ai timeslice lodding during synced scenes.
			pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
		}
		else if (pPhys->GetIsTypeObject())
		{
			// Reset flag, similar to ped's CPED_RESET_FLAG_DisablePedCapsuleMapCollision.
			if (!IsSceneFlagSet(SYNCED_SCENE_BLOCK_MOVER_UPDATE))
			{
				CObject* pObj = GetObject();
				pObj->SetDisableObjectMapCollision(true);
			}
		}
		else if (pPhys->GetIsTypeVehicle())
		{
			// Reset flag, similar to ped's CPED_RESET_FLAG_DisablePedCapsuleMapCollision.
			CVehicle* pVeh = GetVehicle();

			if (!IsSceneFlagSet(SYNCED_SCENE_BLOCK_MOVER_UPDATE))
			{
				pVeh->m_nVehicleFlags.bDisableVehicleMapCollision = true;
			}

			// don't allow ai timeslice lodding during synced scenes.
			CVehicleAILodManager::DisableTimeslicingImmediately(*pVeh);
		}
	}
	
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////
#if !__FINAL

EXTERN_PARSER_ENUM(eSyncedSceneFlags);
EXTERN_PARSER_ENUM(eRagdollBlockingFlags);
EXTERN_PARSER_ENUM(eIkControlFlags);

extern const char* parser_eSyncedSceneFlags_Strings[];
extern const char* parser_eRagdollBlockingFlags_Strings[];
extern const char* parser_eIkControlFlags_Strings[];

void CTaskSynchronizedScene::Debug() const
{
#if DEBUG_DRAW
	// Draw a line between the entity and root position
	Matrix34 root(M34_IDENTITY);

	bool sceneValid = fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_scene);
	
	GetPhysical()->GetGlobalMtx(BONETAG_ROOT, root);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPhysical()->GetTransform().GetPosition());
	grcDebugDraw::Line(vPedPosition, root.d, Color_red);

	//print out some text with the clip, dictionary and scene info
	char text[256];
	
	float fScenePhase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_scene);

	if(NetworkInterface::IsGameInProgress())
	{
		sprintf(text, "m_NetworkSceneID %d, Scene phase %.3f", m_NetworkSceneID, fScenePhase );
		grcDebugDraw::Text(root.d, Color_moccasin, 0, -20, text);
	}

	sprintf(text, "Scene index : %i (%s) ", m_scene, sceneValid ? "running" : "Not running");
	grcDebugDraw::Text(root.d, Color_yellow, 0, 0, text);
	atHashString clip(atFinalizeHash(m_clipPartialHash));
	atHashString facialClip(atFinalizeHash(atPartialStringHash(ms_facialClipSuffix, m_clipPartialHash)));
	sprintf(text, "Dict: \"%s\" (hash: %u), Clip: \"%s\" (hash: %u), FacialClip: \"%s\" (hash: %u)",
		m_dictionary.TryGetCStr() ? m_dictionary.TryGetCStr() : "(Unknown)", m_dictionary.GetHash(),
		clip.TryGetCStr() ? clip.TryGetCStr() : "(Unknown)", clip.GetHash(),
		facialClip.TryGetCStr() ? facialClip.TryGetCStr() : "(Unknown)", facialClip.GetHash());
	grcDebugDraw::Text(root.d, Color_yellow, 0, 10, text);

	if (sceneValid)
	{	
		Matrix34 sceneRoot(M34_IDENTITY);
		fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(m_scene, sceneRoot);

		Vector3 rootEulers(VEC3_ZERO);
		sceneRoot.ToEulersXYZ(rootEulers);

		//print out some scene info
		sprintf(text, "Root pos : x=%.3f, y=%.3f, z=%.3f", sceneRoot.d.x, sceneRoot.d.y, sceneRoot.d.z);
		grcDebugDraw::Text(root.d, Color_yellow, 0, 20, text);
		sprintf(text, "Root rot : x=%.3f, y=%.3f, z=%.3f", RtoD*rootEulers.x, RtoD*rootEulers.y, RtoD*rootEulers.z);
		grcDebugDraw::Text(root.d, Color_yellow, 0, 30, text);
		sprintf(text, "phase : %.3f, %s, %s", fScenePhase, fwAnimDirectorComponentSyncedScene::IsSyncedSceneLooped(m_scene) ? "Looping" : "Hold at end", fwAnimDirectorComponentSyncedScene::IsSyncedScenePaused(m_scene) ? "paused" : "playing");
		grcDebugDraw::Text(root.d, Color_yellow, 0, 40, text);

		grcDebugDraw::Line(sceneRoot.d, vPedPosition, Color_green);
		sprintf(text, "Scene %i root", m_scene );
		grcDebugDraw::Text(sceneRoot.d, Color_green, 0, 50, text);
	}

	int ypos = 70;
	// render the flags being used
	grcDebugDraw::Text(root.d, Color_yellow, -100, ypos, "Scene flags:");
	ypos+=10;
	// Add a toggle for each bit
	for(int i = 0; i < PARSER_ENUM(eSyncedSceneFlags).m_NumEnums; i++)
	{
		if (m_flags.BitSet().IsSet(i))
		{
			grcDebugDraw::Text(root.d, Color_yellow, -100, ypos,  parser_eSyncedSceneFlags_Strings[i]);
			ypos+=10;
		}
	}

	// render the ragdoll blocking flags being used
	ypos = 70;
	grcDebugDraw::Text(root.d, Color_yellow, 200, ypos, "Ragdoll blocking flags:");
	ypos+=10;
	// Add a toggle for each bit
	for(int i = 0; i < PARSER_ENUM(eRagdollBlockingFlags).m_NumEnums; i++)
	{
		if (m_ragdollBlockingFlags.BitSet().IsSet(i))
		{
			grcDebugDraw::Text(root.d, Color_yellow, 200, ypos,  parser_eRagdollBlockingFlags_Strings[i]);
			ypos+=10;
		}
	}

	// render the ragdoll blocking flags being used
	ypos = 70;
	grcDebugDraw::Text(root.d, Color_yellow, -200, ypos, "Ik control flags:");
	ypos+=10;
	// Add a toggle for each bit
	for(int i = 0; i < PARSER_ENUM(eIkControlFlags).m_NumEnums; i++)
	{
		if (m_ikFlags.BitSet().IsSet(i))
		{
			grcDebugDraw::Text(root.d, Color_yellow, -200, ypos,  parser_eIkControlFlags_Strings[i]);
			ypos+=10;
		}
	}


	if (m_moverBlend<1.0f)
	{
		int dictionaryIndex = fwAnimManager::FindSlotFromHashKey(m_dictionary.GetHash()).Get();
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex( dictionaryIndex, clip.GetHash());

		Matrix34 originalMat(M34_IDENTITY);
		originalMat.FromQuaternion(m_startOrientation);
		originalMat.d = m_startPosition;

		Matrix34 sceneMatrix(M34_IDENTITY);
		fwAnimDirectorComponentSyncedScene::CalcEntityMatrix(m_scene, pClip, IsSceneFlagSet(SYNCED_SCENE_LOOP_WITHIN_SCENE), sceneMatrix);

		grcDebugDraw::Axis(originalMat, 0.5f, true);
		grcDebugDraw::Text(originalMat.d, Color_white, 0, 0, "Initial");
		atVarString velString ("Vel(%.2f,%.2f,%.2f)(=%.2f)", m_startVelocity.x, m_startVelocity.y, m_startVelocity.z, m_startVelocity.Mag());
		grcDebugDraw::Text( originalMat.d, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight(), velString.c_str());


		grcDebugDraw::Axis(sceneMatrix, 0.5f, true);
		grcDebugDraw::Text(sceneMatrix.d, Color_white, 0, 0, "Target");

		grcDebugDraw::Line(originalMat.d, originalMat.d+m_startVelocity, Color_white);
	}

	

#endif	// DEBUG_DRAW
}

const char * CTaskSynchronizedScene::GetStaticStateName( s32 iState )
{
	taskAssert(iState>=0&&iState<=State_Exit);
	static const char* aPreviewMoveNetworkStateNames[] = 
	{
		"State_Start",
		"State_Homing",
		"State_RunningScene",
		"State_Exit"
	};

	return aPreviewMoveNetworkStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::CleanUp()
{
    // if this task is being cleaned up due to the ped migrating a new task is
    // going to be created to replace this task, so don't try and restore the state here
    if(IsMigrating() && (GetState() > State_Start))
    {
        // an exception where we have to restore the state is when the ped is dead,
        // in which case this task will be replaced by a dead dying task rather than a copy of this task
        bool bFatallyInjuredPed = false;

        CPed *pPed = TryGetPed();

        if(pPed && pPed->IsFatallyInjured())
        {
            bFatallyInjuredPed = true;
        }

        if(!bFatallyInjuredPed)
        {
            DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene - Skipping Cleanup() early due to task migrating!", pPed ? pPed->GetDebugName() : "Unknown");
            return;
        }
    }

	//restore physics state changes and stop clip director's synchronized scene
	if (TryGetPed())
	{
		RestorePhysicsState(GetPed());
	}
	else
	{
		CPhysical* pPhys = GetPhysical();
		
		if (pPhys)
		{
			if (!IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
			{
				DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene(0x%p) - Not using kinematic physics. Unfixing.", pPhys ? pPhys->GetDebugName() : "Unknown", this);
				pPhys->SetFixedPhysics(false);
			}
			else
			{
				if (!IsSceneFlagSet(SYNCED_SCENE_BLOCK_MOVER_UPDATE) && pPhys->GetCurrentPhysicsInst() && (pPhys->GetIsTypeObject() || pPhys->GetIsTypeVehicle()))
				{
					pPhys->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
				}
			}
		}
	}

	CPhysical* pPhys = GetPhysical();
	if(pPhys)
	{
		//Check if we are using physics.
		if(IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
		{
			//Switch off kinematic physics.
			pPhys->SetShouldUseKinematicPhysics(false);
			
			//Check if we should preserve the velocity.
			if(IsSceneFlagSet(SYNCED_SCENE_PRESERVE_VELOCITY))
			{
				//Preserve the velocity from kinematic physics.
				pPhys->m_nPhysicalFlags.bPreserveVelocityFromKinematicPhysics = true;
			}
		}
		
		if(pPhys->GetAnimDirector() && pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>())
		{
            DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene - (cleanup) Stopping synced scene playback!", pPhys ? pPhys->GetDebugName() : "Unknown");
			pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>()->StopSyncedScenePlayback( m_blendOutDelta, IsSceneFlagSet(SYNCED_SCENE_TAG_SYNC_OUT));
			pPhys->GetAnimDirector()->RemoveAllComponentsByType(fwAnimDirectorComponent::kComponentTypeSyncedScene);
			pPhys->RemoveSceneUpdateFlags(CGameWorld::SU_AFTER_ALL_MOVEMENT);
		}
	}

	switch (pPhys->GetType())
	{

	case ENTITY_TYPE_PED:
		{
			CPed* pPed = GetPed();
			if (pPed)
			{
				// Show Weapon
				if (IsSceneFlagSet(SYNCED_SCENE_HIDE_WEAPON))
				{
					CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
					if (pWeapon)
					{
						pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
					}
				}

			}
		}
		break;

	case ENTITY_TYPE_VEHICLE:
		{
			CVehicle* pVeh = GetVehicle();

			if(pVeh)
			{
				if(pVeh->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					CTrain* pTrain = static_cast<CTrain*>(pVeh);
					pTrain->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
					pTrain->m_nTrainFlags.bIsSyncedSceneControlled = false;
				}

				pVeh->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
				pVeh->m_nVehicleFlags.bAnimateWheels = false; 
				pVeh->m_nVehicleFlags.bAnimatePropellers = false; 
				pVeh->m_nVehicleFlags.bAnimateJoints = false;
				pVeh->SetDriveMusclesToAnimation(false);
			}

		}
		break;

	case ENTITY_TYPE_OBJECT:
		{
			CObject *pObject = GetObject();
			if(pObject)
			{
				pObject->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
			}
		}
		break;

	}

}

//////////////////////////////////////////////////////////////////////////
void CTaskSynchronizedScene::ProcessPreRender2()
{
	if (GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		CTaskScriptedAnimation::ProcessIkControlFlagsPreRender(*GetPed(), m_ikFlags);

	#if FPS_MODE_SUPPORTED
		if (GetState() == State_RunningScene)
		{
			ProcessFirstPersonIk(m_ikFlags);
		}
	#endif // FPS_MODE_SUPPORTED
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskSynchronizedScene::Start_OnUpdate()
{
	taskAssert(GetPhysical());
	taskAssert(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_scene));
	ENABLE_FRAG_OPTIMIZATION_ONLY(animAssert(GetPhysical() && (!GetPhysical()->GetIsTypeVehicle() || GetVehicle()->GetHasFragCacheEntry())));

    if(IsMigrating())
    {
        DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene - rehooking up task after migration!", GetPhysical() ? GetPhysical()->GetDebugName() : "Unknown");
        SetState(State_RunningScene);
        SetIsMigrating(false);
        return FSM_Continue;
    }

	//find the clip
	strLocalIndex dictionaryIndex = fwAnimManager::FindSlotFromHashKey(m_dictionary.GetHash());
	atHashString clip(atFinalizeHash(m_clipPartialHash));
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex( dictionaryIndex.Get(), clip.GetHash());

	if (pClip)
	{
		// do any entity specific setup here
		CPhysical *pPhys = GetPhysical();
		pPhys->GetPortalTracker()->RequestRescanNextUpdate();
		switch (pPhys->GetType())
		{

		case ENTITY_TYPE_OBJECT:
			{
				CObject * pObj = GetObject();

				pObj->InitAnimLazy();

				if (!IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
				{
					pObj->SetFixedPhysics(true);
				}
				else
				{
					if (!IsSceneFlagSet(SYNCED_SCENE_BLOCK_MOVER_UPDATE) && pObj->GetCurrentPhysicsInst())
					{
						pObj->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
						m_DisabledGravity = true;
					}
				}

				pObj->GetAnimDirector()->RequestUpdatePostCamera();
			}
			break;

		case ENTITY_TYPE_PED:
			{
				CPed* pPed = GetPed();

				ApplyRagdollBlockingFlags(pPed);

				if (IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS) && !pPed->GetUseExtractedZ())
				{
					// need to disable gravity here too, or the ped will build up significant downward 
					// velocity due to gravity potentially causing issues on scene exit
					pPed->SetUseExtractedZ(true);
					m_DisabledGravity = true;
				}

				pPed->ResetStandData();
				if (IsSceneFlagSet(SYNCED_SCENE_EXPAND_PED_CAPSULE_FROM_SKELETON))
				{
					pPed->SetPedResetFlag(CPED_RESET_FLAG_ExpandPedCapsuleFromSkeleton, true);
				}

				if (pPed->GetIsInVehicle() && pPed->GetMyVehicle() && IsSceneFlagSet(SYNCED_SCENE_SET_PED_OUT_OF_VEHICLE_AT_START))
				{
					pPed->SetPedOutOfVehicle(CPed::PVF_IgnoreSafetyPositionCheck | CPed::PVF_Warp);
				}
			}
			break;

		case ENTITY_TYPE_VEHICLE:
			{
				CVehicle* pVeh = GetVehicle();
				
				pVeh->InitAnimLazy();

				if (!IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
				{
					pVeh->SetFixedPhysics(true);
				}
				else
				{
					if (!IsSceneFlagSet(SYNCED_SCENE_BLOCK_MOVER_UPDATE) && pVeh->GetCurrentPhysicsInst())
					{
						pVeh->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
						m_DisabledGravity = true;
					}
				}

				pVeh->m_nVehicleFlags.bAnimateWheels = true; 
				pVeh->m_nVehicleFlags.bAnimatePropellers = true; 
				pVeh->m_nVehicleFlags.bAnimateJoints = true; 

				if(pVeh->GetVehicleFragInst()->GetCached() && pVeh->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->body!=NULL)
				{
					pVeh->SetDriveMusclesToAnimation(true);
				}

				pVeh->GetAnimDirector()->RequestUpdatePostCamera();
			}
			break;
		}

		SetState(State_Homing);
		return FSM_Continue;
	}
	else
	{
		taskAssertf(dictionaryIndex != -1, "Clip dictionary \"%s\" (hash: %u) has an invalid slot -1!", m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash());
		taskAssertf(g_ClipDictionaryStore.IsValidSlot(dictionaryIndex), "Clip dictionary \"%s\" (hash: %u) has an invalid slot %i!", m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash(), dictionaryIndex.Get());
		taskAssertf(g_ClipDictionaryStore.HasObjectLoaded(dictionaryIndex), "Clip dictionary \"%s\" (hash: %u) at index %i is not loaded!", m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash(), dictionaryIndex.Get());
		taskAssertf(0, "Task synchronized scene: Couldn't find clip \"%s\" (hash: %u) - Clip dictionary \"%s\" (hash: %u)", clip.GetCStr() ? clip.GetCStr() : "(null)", clip.GetHash(), m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash());
		return FSM_Quit;
	}
}

//////////////////////////////////////////////////////////////////////////
		
void CTaskSynchronizedScene::Homing_OnEnter()
{
	// start the timer before transitioning to the running scene state
	CPhysical* pPhys = GetPhysical();
	Matrix34 entityMat(M34_IDENTITY);
	pPhys->GetMatrixCopy(entityMat);
	m_startOrientation.FromMatrix34(entityMat);
	m_startPosition = entityMat.d;
	m_startVelocity = pPhys->GetAnimatedVelocity();
	m_StartSituationValid = true;

	entityMat.d.Zero();
	entityMat.Transform(m_startVelocity); //transform the initial velocity into global space
	
	m_moverBlend = (m_moverBlendRate==INSTANT_BLEND_IN_DELTA ? 1.0f : 0.0f);

	if (pPhys->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pPhys);
		pPed->GetMotionData()->GetCurrentMoveBlendRatio(m_startMoveBlendRatio.x, m_startMoveBlendRatio.y);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskSynchronizedScene::Homing_OnUpdate()
{

	if (m_ExitScene)
	{
		animTaskDebugf(this, "Exiting scene - ExitSceneNextUpdate called externally");
		SetState(State_Exit);
		return FSM_Continue;
	}

	float moverBlendTimeLeft = m_moverBlendRate>0.0f ? (1.0f - m_moverBlend)/m_moverBlendRate : 0.0f;
	float poseBlendTime = m_blendInDelta > 0.0f ? (1.0f/m_blendInDelta) : 1000.0f;

	if (m_moverBlend>=1.0f ||((moverBlendTimeLeft - poseBlendTime)<=0.0f))
	{
		SetState(State_RunningScene);
	}
	else
	{
		CPhysical* pPhys = GetPhysical();

		InterpolateMover();

		if (IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
		{
			pPhys->SetShouldUseKinematicPhysics(true);
		}

		if (static_cast<CEntity*>(pPhys)->GetIsTypePed())
		{
			CPed* pPed = GetPed();
			// Stop the physics world from affecting clip playback on the mover

			pPed->SetPedResetFlag(CPED_RESET_FLAG_UsingMoverExtraction, true);

			if (!IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
			{
				// Force low lod physics by default (this removes collision and 
				pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
			}
			else
			{
				// B* 1485998: Avoid pops due to out of date impulses applied by ped lodding code 
				// pushing peds up in the air whilst lerping into synced scenes.
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPedStandingUpdateEachSimStep, true);
			}

			if (IsSceneFlagSet(SYNCED_SCENE_DONT_INTERRUPT) && !IsSceneFlagSet(SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE))
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions, true);
			}

			if (IsSceneFlagSet(SYNCED_SCENE_EXPAND_PED_CAPSULE_FROM_SKELETON))
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ExpandPedCapsuleFromSkeleton, true);
			}
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::RunningScene_OnEnter()
{
    if(IsMigrating())
    {
        // peds can only migrate when in the running scene state so all of this code should have already run
        return;
    }

	//
	// Ped likely warps on the first frame so reset the physical motions
	//
	CPhysical* pPhys = GetPhysical();
	if (pPhys->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pPhys);
		if (pPed)
		{
			crCreature* creature = pPed->GetCreature();
			if(creature)
			{
				crCreatureComponentPhysical* physicalComp = creature->FindComponent<crCreatureComponentPhysical>();
				if(physicalComp)
				{
					physicalComp->ResetMotions();
				}
			}
			
			// Ensure the ragdoll frame blends out when we need it to!
			pPed->GetMovePed().SwitchToAnimated(m_blendInDelta == INSTANT_BLEND_IN_DELTA ? INSTANT_BLEND_DURATION : 1.0f / m_blendInDelta);

			// Hide Weapon
			if (IsSceneFlagSet(SYNCED_SCENE_HIDE_WEAPON))
			{
				CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
				if (pWeapon)
				{
					pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
				}
			}
		}
	}

	// find the clip
	strLocalIndex dictionaryIndex = fwAnimManager::FindSlotFromHashKey(m_dictionary.GetHash());
	atHashString clip(atFinalizeHash(m_clipPartialHash));
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex( dictionaryIndex.Get(), clip.GetHash());

	if (pClip)
	{
		//find the facial clip
		u32 facialClipHash = atFinalizeHash(atPartialStringHash(ms_facialClipSuffix, m_clipPartialHash));
		const crClip* pFacialClip = fwAnimManager::GetClipIfExistsByDictIndex( dictionaryIndex.Get(), facialClipHash);

		fwAnimDirectorComponentSyncedScene* componentSyncedScene = pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>();
		if(!componentSyncedScene)
		{
			componentSyncedScene = pPhys->GetAnimDirector()->CreateComponentSyncedScene();
			pPhys->AddSceneUpdateFlags(CGameWorld::SU_AFTER_ALL_MOVEMENT);
		}
		componentSyncedScene->StartSyncedScenePlayback(dictionaryIndex.Get(), pClip, pFacialClip, m_scene, CClipNetworkMoveInfo::ms_NetworkSyncedScene, m_blendInDelta, false, IsSceneFlagSet(SYNCED_SCENE_LOOP_WITHIN_SCENE), (m_moverBlend < 1.0f) || IsSceneFlagSet(SYNCED_SCENE_BLOCK_MOVER_UPDATE));

		if(m_flags.BitSet().IsSet(SYNCED_SCENE_PROCESS_ATTACHMENTS_ON_START))
		{
			pPhys->ProcessAllAttachments();
		}
	
		if (pPhys->GetIsTypePed() && IsSceneFlagSet(SYNCED_SCENE_ACTIVATE_RAGDOLL_ON_COLLISION))
		{
			CTask* pTaskNM = NULL;

			TUNE_GROUP_FLOAT(TASK_SYNCHRONIZED_SCENE, fRagdollOnCollisionAllowedSlope, -0.999f, -10.0f, 10.0f, 0.0001f);
			TUNE_GROUP_FLOAT(TASK_SYNCHRONIZED_SCENE, fRagdollOnCollisionAllowedPenetration, 0.100f, 0.0f, 100.0f, 0.0001f);
			
			pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 10000, false);
			
			CEventSwitch2NM event(10000, pTaskNM);
			GetPed()->SetActivateRagdollOnCollisionEvent(event);
			GetPed()->SetRagdollOnCollisionAllowedSlope(fRagdollOnCollisionAllowedSlope);
			GetPed()->SetRagdollOnCollisionAllowedPenetration(fRagdollOnCollisionAllowedPenetration);
		}
	}
	else
	{
		taskAssertf(dictionaryIndex != -1, "Clip dictionary \"%s\" (hash: %u) has an invalid slot -1!", m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash());
		taskAssertf(g_ClipDictionaryStore.IsValidSlot(dictionaryIndex), "Clip dictionary \"%s\" (hash: %u) has an invalid slot %i!", m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash(), dictionaryIndex.Get());
		taskAssertf(g_ClipDictionaryStore.HasObjectLoaded(dictionaryIndex), "Clip dictionary \"%s\" (hash: %u) at index %i is not loaded!", m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash(), dictionaryIndex.Get());
		taskAssertf(0, "Task synchronized scene: Couldn't find clip \"%s\" (hash: %u) - Clip dictionary \"%s\" (hash: %u)", clip.GetCStr() ? clip.GetCStr() : "(null)", clip.GetHash(), m_dictionary.GetCStr() ? m_dictionary.GetCStr() : "(null)", m_dictionary.GetHash());
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskSynchronizedScene::RunningScene_OnUpdate()
{

	if (m_ExitScene)
	{
		animTaskDebugf(this, "Exiting scene - ExitSceneNextUpdate called externally");
		SetState(State_Exit);
		return FSM_Continue;
	}

	// we need to update the desired heading of the ped to stop the underlying moveblender from trying to turn unnecessarily
	// (This can cause delays in movement when exiting the task
	Vector3 eulerRotation(VEC3_ZERO);
	CPhysical* pPhys = GetPhysical();

	bool blockMoverUpdate = (m_moverBlend < 1.0f) || IsSceneFlagSet(SYNCED_SCENE_BLOCK_MOVER_UPDATE);
	if(pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>())
	{
		pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>()->SetBlockMoverUpdate(blockMoverUpdate);
	}

	fwAnimDirectorComponentSyncedScene *animDirectorComponentSyncedScene = pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>();
	if(animDirectorComponentSyncedScene)
	{
		pPhys->AddSceneUpdateFlags(CGameWorld::SU_AFTER_ALL_MOVEMENT);

		float sceneDuration = animDirectorComponentSyncedScene->GetSyncedSceneDuration(m_scene);
		float scenePhase = animDirectorComponentSyncedScene->GetSyncedScenePhase(m_scene);

		int dictionaryIndex = fwAnimManager::FindSlotFromHashKey(m_dictionary.GetHash()).Get();
		atHashString clip(atFinalizeHash(m_clipPartialHash));
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(dictionaryIndex, clip.GetHash());
		float clipDuration = pClip ? pClip->GetDuration() : 0.0f;

		float fSyncedSceneTime = animDirectorComponentSyncedScene->GetSyncedSceneTime(m_scene);
		float fTimeRemaining = clipDuration - fSyncedSceneTime;

		float clipPhase = pClip ? pClip->ConvertTimeToPhase(fSyncedSceneTime) : 1.0f;

		const float fTolerance = 1.0e-5f;
		bool shortClip = !IsClose(clipDuration, sceneDuration, fTolerance) && clipDuration < sceneDuration && !IsSceneFlagSet(SYNCED_SCENE_LOOP_WITHIN_SCENE);

		bool holdLastFrame = animDirectorComponentSyncedScene->IsSyncedSceneHoldLastFrame(m_scene);
		bool abort = animDirectorComponentSyncedScene->IsSyncedSceneAbort(m_scene);

		static const float NET_MAX_MIGRATE_TIME_BEFORE_END  = 1.0f;  // don't allow migration if the ped is not holding last frame and has less than this much time left 
		const unsigned TIME_TO_DELAY_PED_MIGRATION = unsigned(NET_MAX_MIGRATE_TIME_BEFORE_END*1000.0f);

		bool bNetworkPreventControlPassing = (fTimeRemaining < NET_MAX_MIGRATE_TIME_BEFORE_END) && !holdLastFrame && pPhys->GetIsTypePed();

		if (bNetworkPreventControlPassing)
		{
			NetworkInterface::DelayMigrationForTime(pPhys, TIME_TO_DELAY_PED_MIGRATION);
		}

		if((shortClip && clipPhase >= 1.0f) || (scenePhase >= 1.0f && !holdLastFrame) || abort)
		{
            DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene - (running scene) Stopping synced scene playback!", pPhys ? pPhys->GetDebugName() : "Unknown");
			animDirectorComponentSyncedScene->StopSyncedScenePlayback(m_blendOutDelta, IsSceneFlagSet(SYNCED_SCENE_TAG_SYNC_OUT));
			pPhys->GetAnimDirector()->RemoveAllComponentsByType(fwAnimDirectorComponent::kComponentTypeSyncedScene);
			pPhys->RemoveSceneUpdateFlags(CGameWorld::SU_AFTER_ALL_MOVEMENT);

			SetState(State_Exit);
			animTaskDebugf(this, "%s", "Exiting task, anim has ended");
			return FSM_Continue;
		}

		// Blending to vehicle lighting
		if (pClip)
		{
			const crTags* pTags = pClip->GetTags();
			if (pTags)
			{
				for (int i=0; i < pTags->GetNumTags(); i++)
				{
					const crTag* pTag = pTags->GetTag(i);
					if (pTag)
					{
						if (pTag->GetKey() == CClipEventTags::BlendToVehicleLighting.GetHash())
						{
							const float fStart = pTag->GetStart();
							const float fEnd = pTag->GetEnd();
							bool bGettingIntoVehicle = true;

							const crProperty& rProperty = pTag->GetProperty();
							const crPropertyAttribute* pAttrib = rProperty.GetAttribute(CClipEventTags::IsGettingIntoVehicle.GetHash());
							if (pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeBool)
							{
								const crPropertyAttributeBool* pTagBlendInBool = static_cast<const crPropertyAttributeBool*>(pAttrib);
								{
									bGettingIntoVehicle = pTagBlendInBool->GetBool();
								}
							}

							// Calculate vehicle lighting scalar
							if (clipPhase <= fStart)
							{
								m_fVehicleLightingScalar = 0.0f;
							}
							else if (clipPhase >= fEnd)
							{
								m_fVehicleLightingScalar = 1.0f;
							}
							else
							{
								const float fDuration = pTag->GetDuration();
								if (fDuration >= 0.01f)
								{
									m_fVehicleLightingScalar = (clipPhase - fStart) / fDuration;
								}
								else
								{
									m_fVehicleLightingScalar = 1.0f;
								}
							}

							// Invert if we're blending out/getting out of vehicle.
							if (!bGettingIntoVehicle)
							{
								m_fVehicleLightingScalar = 1.0f - m_fVehicleLightingScalar;
							}
						}

						static const crProperty::Key enablePhysicsDrivenJointKey = crProperty::CalcKey("EnablePhysicsDrivenJoint", 0x78c21fd4);
						if (pTag->GetKey() == enablePhysicsDrivenJointKey)
						{
							if (clipPhase >= pTag->GetStart() && clipPhase <= pTag->GetEnd())
							{
								if (IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
								{
									CVehicle *pVehicle = pPhys->GetIsTypeVehicle() ? static_cast< CVehicle * >(pPhys) : NULL;
									if (pVehicle)
									{
										for (int j = 0; j < pTag->GetNumAttributes(); j ++)
										{
											const crPropertyAttribute *pPropertyAttribute = pTag->GetAttributeByIndex(j);
											if (pPropertyAttribute && pPropertyAttribute->GetType() == crPropertyAttribute::kTypeInt)
											{
												const crPropertyAttributeInt *pPropertyAttributeInt = static_cast< const crPropertyAttributeInt * >(pPropertyAttribute);

												CCarDoor *pDoor = pVehicle->GetDoorFromId(static_cast< eHierarchyId >(pPropertyAttributeInt->GetValue()));
												if (Verifyf(pDoor, "Couldn't find door specified by a EnablePhysicsDrivenJoint tag"))
												{
													pDoor->SetFlag(CCarDoor::DRIVEN_BY_PHYSICS);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (m_moverBlend < 1.0f)
	{
		InterpolateMover();
	}

	if (pPhys->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pPhys);
		if(!m_ikFlags.BitSet().IsSet(AIK_USE_LEG_ALLOW_TAGS) && !m_ikFlags.BitSet().IsSet(AIK_USE_LEG_BLOCK_TAGS))
		{
			// if no leg ik flags are set, disable leg ik
			pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
		}

		pPed->SetPedResetFlag(CPED_RESET_FLAG_UsingMoverExtraction, true);

		// Early out if we've entered ragdoll for any reason
		// the entity may have been shot / hit by a car / etc
		// Ragdoll reactions can be disabled seperately by using the
		// block ragdoll reaction reset flags
		// Also need to exit if the ped has died
		if (pPed->GetUsingRagdoll() || pPed->IsDead() || (pPed->ShouldBeDead() && IsSceneFlagSet(SYNCED_SCENE_ABORT_ON_DEATH)))
		{
			m_ExitScene = true;
			SetState(State_Exit);
			animTaskDebugf(this, "Exiting task, ped in ragdoll(%s) or dead(%s)", pPed->GetUsingRagdoll() ? "yes" : "no", pPed->IsDead() ? "yes" : "no");
			return FSM_Continue;
		}

		Matrix34 m = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		m.ToEulersXYZ(eulerRotation);

		pPed->SetDesiredHeading(eulerRotation.z);

		if (!IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
		{
			// Force low lod physics by default (this removes collision etc)
			pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
		}

		if (IsSceneFlagSet(SYNCED_SCENE_DONT_INTERRUPT) && !IsSceneFlagSet(SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions, true);
		}

		if (IsSceneFlagSet(SYNCED_SCENE_EXPAND_PED_CAPSULE_FROM_SKELETON))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ExpandPedCapsuleFromSkeleton, true);
		}

		if (IsSceneFlagSet(SYNCED_SCENE_ACTIVATE_RAGDOLL_ON_COLLISION))
		{
			pPed->SetActivateRagdollOnCollision(true);
		}

		// Disable the constraint, otherwise the ped capsule orientation gets changed during the
		// physics update, potentiallky leading to odd collisions during attached scenes.
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedConstraints, true);

		UpdateNetworkPedRateCompensate( pPed, animDirectorComponentSyncedScene );

	}
	else if (pPhys->GetIsTypeObject() || pPhys->GetIsTypeVehicle())
	{
		pPhys->m_nDEflags.bForcePrePhysicsAnimUpdate = true;

		if (IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
		{
			Vector3 animVel(VEC3_ZERO);
			Vector3 animAngVel(VEC3_ZERO);
			
			CalculateAnimatedVelocities(animVel, animAngVel);
#if __ASSERT
			pPhys->SetIgnoreVelocityCheck(true);
#endif
			pPhys->SetVelocity(animVel);
			pPhys->SetAngVelocity(animAngVel);
#if __ASSERT
			pPhys->SetIgnoreVelocityCheck(false);
#endif
		}
	}

	// Stop the physics world from affecting clip playback on the mover
	if (IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
	{
		pPhys->SetShouldUseKinematicPhysics(true);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::CalculateAnimatedVelocities(Vector3& velocityOut, Vector3& angularVelocityOut)
{
	velocityOut.Zero();
	angularVelocityOut.Zero();

	CPhysical* pPhys = GetPhysical();

	// Get the synced scene component
	fwAnimDirectorComponentSyncedScene *animDirectorComponentSyncedScene = pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>();
	if(animDirectorComponentSyncedScene)
	{
		// Get the clip
		int dictionaryIndex = fwAnimManager::FindSlotFromHashKey(m_dictionary.GetHash()).Get();
		atHashString clip(atFinalizeHash(m_clipPartialHash));
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(dictionaryIndex, clip.GetHash());

		if (pClip)
		{
			// Get the start and end clip phases
			float fStartTime = animDirectorComponentSyncedScene->GetSyncedScenePreviousTime(m_scene);
			float fEndTime = animDirectorComponentSyncedScene->GetSyncedSceneTime(m_scene);

			if ((fStartTime!=fEndTime) && (fwTimer::GetTimeStep()!=0.0f))
			{

				Matrix34 startMat(M34_IDENTITY);
				Matrix34 endMat(M34_IDENTITY);

				fwAnimDirectorComponentSyncedScene::CalcEntityMatrix(m_scene, pClip, IsSceneFlagSet(SYNCED_SCENE_LOOP_WITHIN_SCENE), startMat, fStartTime);
				fwAnimDirectorComponentSyncedScene::CalcEntityMatrix(m_scene, pClip, IsSceneFlagSet(SYNCED_SCENE_LOOP_WITHIN_SCENE), endMat, fEndTime);

				velocityOut = endMat.d - startMat.d;
				velocityOut /= fwTimer::GetTimeStep();

				Quaternion startRot(Quaternion::sm_I);
				Quaternion endRot(Quaternion::sm_I);

				startMat.ToQuaternion(startRot);
				endMat.ToQuaternion(endRot);

				Quaternion rotDiff(Quaternion::sm_I);
				rotDiff.MultiplyInverse(endRot, startRot);
				rotDiff.ToEulers(angularVelocityOut);
				angularVelocityOut/= fwTimer::GetTimeStep();
			}			
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::UpdateNetworkPedRateCompensate(CPed* pPed, fwAnimDirectorComponentSyncedScene *animDirectorComponentSyncedScene )
{
	//Network checks peds in long duration scenes for differences to remote and compensates rate of the clones scene
	if( NetworkInterface::IsGameInProgress() && 
		animDirectorComponentSyncedScene &&
		pPed &&
		pPed->GetNetworkObject())
	{
		float scenePhase	= animDirectorComponentSyncedScene->GetSyncedScenePhase(m_scene);
		float sceneDuration = animDirectorComponentSyncedScene->GetSyncedSceneDuration(m_scene);
        float sceneRate     = fwAnimDirectorComponentSyncedScene::GetSyncedSceneRate(m_scene);
        bool  isSceneLooped = fwAnimDirectorComponentSyncedScene::IsSyncedSceneLooped(m_scene);

        static const float MIN_SCENE_DURATION_TO_CORRECT     = 10.0f; // we don't correct shorter duration scenes
		static const float MAX_TIME_DIFF_SECS_BEFORE_CORRECT = 0.15f; //If we see that time difference in seconds is longer than this period we compensate rate
		static const float MIN_TIME_DIFF_SECS_BEFORE_RESET   = 0.05f; //If we see that time difference in seconds is shorter than this period we set back to default

        if(sceneDuration >= MIN_SCENE_DURATION_TO_CORRECT)
        {
		    if(!pPed->IsNetworkClone())
		    {
                if(sceneRate > 0.0f)
                {
                    float timeRemainingInMs   = (((1.0f - scenePhase) * sceneDuration) / sceneRate)* 1000.0f;
                    unsigned estimatedCompletionTime = NetworkInterface::GetNetworkTime() + static_cast<unsigned>(timeRemainingInMs);

                    // don't update the value for small delta to prevent the task data being constantly sent out
                    const unsigned ADJUST_THRESHOLD = 5;

                    int delta = m_EstimatedCompletionTime - estimatedCompletionTime;

                    if(rage::Abs(delta) >= ADJUST_THRESHOLD)
                    {
                        m_EstimatedCompletionTime = estimatedCompletionTime;

                        gnetDebug3("%s UpdateNetworkPedRateCompensate (Network Scene ID: %d) - Estimated local completion time is (%d) which is in %dms time", pPed->GetDebugName(), m_NetworkSceneID, m_EstimatedCompletionTime, m_EstimatedCompletionTime - NetworkInterface::GetNetworkTime());
                    }
                }
                else
                {
                    m_EstimatedCompletionTime = 0;
                }
		    }
		    else
		    {
                if(m_EstimatedCompletionTime != 0)
                {
                    if(sceneRate > 0.0f)
                    {
                        float originalRate = 0.0f;
                        if(CNetworkSynchronisedScenes::GetSceneRate(m_NetworkSceneID, originalRate))
                        {
                            unsigned networkTime                  = NetworkInterface::GetNetworkTime();
                            float    timeRemainingInMsAtStartRate = (((1.0f - scenePhase) * sceneDuration) / originalRate) * 1000.0f;
                            unsigned localEstimatedCompletionTime = networkTime + static_cast<unsigned>(timeRemainingInMsAtStartRate);

                            float fTimeDiff = (static_cast<int>(localEstimatedCompletionTime - m_EstimatedCompletionTime)) / 1000.0f;

                            // if we are looped check if we can catch up quicker by accelerating back around rather than slowing down
                            if(isSceneLooped)
                            {
                                if(originalRate > 0.0f && ((fabs(fTimeDiff) / originalRate) > (sceneDuration / 2.0f)))
                                {
                                    if(fTimeDiff >= 0.0f)
                                    {
                                        // clone is running more than half the scene duration ahead, so correct so it is running less
                                        // than half the duration behind
                                        fTimeDiff = fTimeDiff - (sceneDuration / originalRate);
                                    }
                                    else
                                    {
                                        // clone is running more than half the scene duration behind, so correct so it is running less
                                        // than half the duration behind
                                        fTimeDiff = fTimeDiff + (sceneDuration / originalRate);
                                    }
                                }
                            }

                            bool canAdjustRate = false;

                            if(!IsClose(sceneRate, originalRate, 0.01f))
                            {
                                if(fabs(fTimeDiff) <= MIN_TIME_DIFF_SECS_BEFORE_RESET)
                                {
                                    gnetDebug3("%s UpdateNetworkPedRateCompensate - Resetting rate back to original value (%.2f)", pPed->GetDebugName(), originalRate);
                                    animDirectorComponentSyncedScene->SetSyncedSceneRate(m_scene, originalRate);
                                }
                                else
                                {
                                    canAdjustRate = true;
                                }
                            }
                            else if(fabs(fTimeDiff) >= MAX_TIME_DIFF_SECS_BEFORE_CORRECT)
                            {
                                gnetDebug3("%s UpdateNetworkPedRateCompensate - Starting rate adjustment due to too large time diff (%.2f)", pPed->GetDebugName(), fTimeDiff);
                                canAdjustRate = true;
                            }

                            if(canAdjustRate)
                            {
                                float targetRateAdjustment = fTimeDiff;
                                targetRateAdjustment       = rage::Max(targetRateAdjustment, -0.1f);
                                targetRateAdjustment       = rage::Min(targetRateAdjustment,  0.1f);

                                if(!IsClose(sceneRate, originalRate + targetRateAdjustment, 0.01f))
                                {
                                    animDirectorComponentSyncedScene->SetSyncedSceneRate(m_scene, originalRate + targetRateAdjustment);
                                    gnetDebug3("%s UpdateNetworkPedRateCompensate - Changing rate  from %.2f to %.2f due to time diff of (%.2f)", pPed->GetDebugName(), sceneRate, originalRate + targetRateAdjustment, fTimeDiff);
                                }

                                // used for more detailed debugging, but too spammy to have on all of the time
                                /*else
                                {
                                    gnetDebug3("%s UpdateNetworkPedRateCompensate - Time diff is (%.2f)", pPed->GetDebugName(), fTimeDiff);
                                }*/
                            }

                            // used for more detailed debugging, but too spammy to have on all of the time
                            //gnetDebug3("%s UpdateNetworkPedRateCompensate (Network Scene ID: %d) - Estimated local completion time is (%d) which is in %dms time, Estimated remote completion time is (%d) which is in %dms time", pPed->GetDebugName(), m_NetworkSceneID, localEstimatedCompletionTime, localEstimatedCompletionTime - networkTime, m_EstimatedCompletionTime, m_EstimatedCompletionTime - networkTime);
                        }
                    }
                }
		    }
        }
	}
}

//////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED
void CTaskSynchronizedScene::ProcessFirstPersonIk(eIkControlFlagsBitSet& flags)
{
	if (!flags.BitSet().IsSet(AIK_USE_FP_ARM_LEFT) && !flags.BitSet().IsSet(AIK_USE_FP_ARM_RIGHT))
	{
		return;
	}

	CPed* pPed = TryGetPed();

	if (pPed == NULL)
	{
		return;
	}

	if (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		for (int helper = 0; helper < 2; ++helper)
		{
			if (m_apFirstPersonIkHelper[helper])
			{
				m_apFirstPersonIkHelper[helper]->Reset();
			}
		}

		return;
	}

	for (int helper = 0; helper < 2; ++helper)
	{
		eIkControlFlags controlFlag = (helper == 0) ? AIK_USE_FP_ARM_LEFT : AIK_USE_FP_ARM_RIGHT;

		if (flags.BitSet().IsSet(controlFlag))
		{
			if (m_apFirstPersonIkHelper[helper] == NULL)
			{
				m_apFirstPersonIkHelper[helper] = rage_new CFirstPersonIkHelper();
				taskAssert(m_apFirstPersonIkHelper[helper]);

				m_apFirstPersonIkHelper[helper]->SetArm((helper == 0) ? CFirstPersonIkHelper::FP_ArmLeft : CFirstPersonIkHelper::FP_ArmRight);
			}

			if (m_apFirstPersonIkHelper[helper])
			{
				m_apFirstPersonIkHelper[helper]->Process(pPed);
			}
		}
	}
}
#endif // FPS_MODE_SUPPORTED

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::RunningScene_OnExit()
{
	taskAssert(GetPhysical());
	if (GetPhysical()->GetIsTypePed() && IsSceneFlagSet(SYNCED_SCENE_ACTIVATE_RAGDOLL_ON_COLLISION))
	{
		CPed* pPed = GetPed();
		pPed->SetActivateRagdollOnCollision(false);
		pPed->ClearActivateRagdollOnCollisionEvent();
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskSynchronizedScene::QuitScene_OnUpdate()
{
	CPed* pPed = TryGetPed();

	if (m_ExitScene && IsSceneFlagSet(SYNCED_SCENE_ON_ABORT_STOP_SCENE))
	{
		CPhysical *pPhys = GetPhysical();
		if(pPhys)
		{
			fwAnimDirectorComponentSyncedScene *animDirectorComponentSyncedScene = pPhys->GetAnimDirector()?pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>():NULL;
			if(animDirectorComponentSyncedScene)
			{
				animDirectorComponentSyncedScene->SetSyncedSceneAbort(animDirectorComponentSyncedScene->GetSyncedSceneId(), true);
			}
		}
	}

	if (pPed)
	{
		if (IsSceneFlagSet(SYNCED_SCENE_EXPAND_PED_CAPSULE_FROM_SKELETON))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ExpandPedCapsuleFromSkeleton, true);
		}
	}

	animTaskDebugf(this, "%s", "Quitting task");
	return FSM_Quit;
}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::RestorePhysicsState(CPed* pPed)
{
	pPed->ClearRagdollBlockingFlags(m_ragdollBlockingFlags);

	if (m_DisabledGravity)
	{
		pPed->SetUseExtractedZ(false);
	}

	if (pPed->GetRagdollState()!= RAGDOLL_STATE_PHYS && pPed->GetAnimatedInst() && pPed->GetAnimatedInst()->IsInLevel() && !pPed->IsDead())
	{
		//we need to update the desired heading of the ped to stop it rotating needlessly at the end of the clip
		Vector3 eulerRotation(VEC3_ZERO);
		Matrix34 m = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		m.ToEulersXYZ(eulerRotation);
		
		pPed->SetDesiredHeading(eulerRotation.z);

		if (!IsSceneFlagSet(SYNCED_SCENE_USE_KINEMATIC_PHYSICS))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, false);
			pPed->ResetStandData();
		}
	}

}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::InterpolateMover()
{
	fwEntity* pEnt = GetEntity();

	if (pEnt && m_StartSituationValid)
	{
		int dictionaryIndex = fwAnimManager::FindSlotFromHashKey(m_dictionary.GetHash()).Get();
		atHashString clip(atFinalizeHash(m_clipPartialHash));
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex( dictionaryIndex, clip.GetHash());

		// interpolate the mover from the stored version to the scene version.
		//calc the original matrix and lerp
		Matrix34 originalMat(M34_IDENTITY);
		originalMat.FromQuaternion(m_startOrientation);
		originalMat.d = m_startPosition;

		Matrix34 sceneMatrix(M34_IDENTITY);
		fwAnimDirectorComponentSyncedScene::CalcEntityMatrix(m_scene, pClip, IsSceneFlagSet(SYNCED_SCENE_LOOP_WITHIN_SCENE), sceneMatrix);

		static bool s_InterpUsingCurve = false;
		float t = 0.0f;
		if (s_InterpUsingCurve)
		{
			t = Sinf(m_moverBlend*PI*0.5f);
		}
		else
		{
			t = m_moverBlend;
		}

		originalMat.Interpolate(originalMat, sceneMatrix, t);
		pEnt->SetMatrix(originalMat);

		if (static_cast<CEntity*>(pEnt)->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pEnt);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(m_startMoveBlendRatio.y, m_startMoveBlendRatio.x);
		}
	}	
}

//////////////////////////////////////////////////////////////////////////

bool CTaskSynchronizedScene::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(pEvent && IsSceneFlagSet(SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE))
	{
		switch (static_cast< const CEvent * >(pEvent)->GetEventType())
		{
		case EVENT_DAMAGE:
		case EVENT_ON_FIRE:
			{
				animTaskDebugf(this, "Aborting for weapon damage - event: %s", pEvent ? ((CEvent*)pEvent)->GetName() : "none");
				return true;
			}
			break;
		}
	}

	bool bAbortForDeath = false;
	if (GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		CPed* pPed = GetPed();
		if (pPed->ShouldBeDead() && IsSceneFlagSet(SYNCED_SCENE_ABORT_ON_DEATH))
			bAbortForDeath = true;
	}
	

	// special clip that has been set by the script, and can only be aborted by the script
	// or by itself (or directly by another task, so not any events is what I mean, ok.)
	if(	!(CTask::ABORT_PRIORITY_IMMEDIATE==iPriority) 
		&& IsSceneFlagSet(SYNCED_SCENE_DONT_INTERRUPT) 
		&& pEvent 
		&& !((CEvent*)pEvent)->RequiresAbortForRagdoll() 
		&& ((CEvent*)pEvent)->GetEventType()!= EVENT_SCRIPT_COMMAND
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_GIVE_PED_TASK // stealth kills use the give ped task, this ensures peds who are playing clips will abort correctly
		&& ((CEvent*)pEvent)->GetEventPriority() != E_PRIORITY_DAMAGE_BY_MELEE
		&& !bAbortForDeath
		)
	{
		if(pEvent->GetEventType() == EVENT_DAMAGE)
		{
			const_cast< aiEvent * >(pEvent)->Tick();
		}
		return false;
	}

	if (GetSubTask() && !GetSubTask()->MakeAbortable(iPriority, pEvent))
	{
		return false;
	}

	if(CTask::ABORT_PRIORITY_URGENT==iPriority || CTask::ABORT_PRIORITY_IMMEDIATE==iPriority )
	{
		// Base class
		animTaskDebugf(this, "Aborting for event: %s", pEvent ? ((CEvent*)pEvent)->GetName() : "none");
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
    // if this task is being cleaned up due to the ped migrating a new task is
    // going to be created to replace this task, so don't try and restore the state here
    if(IsMigrating() && (GetState() > State_Start))
    {
        // an exception where we have to restore the state is when the ped is dead,
        // in which case this task will be replaced by a dead dying task rather than a copy of this task
        bool bFatallyInjuredPed = false;

        CPed *pPed = TryGetPed();

        if(pPed && pPed->IsFatallyInjured())
        {
            bFatallyInjuredPed = true;
        }

        if(!bFatallyInjuredPed)
        {
            DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene - Skipping DoAbort() early due to task migrating!", pPed ? pPed->GetDebugName() : "Unknown");
            return;
        }
    }

    if(CTask::ABORT_PRIORITY_IMMEDIATE == iPriority || IsSceneFlagSet(SYNCED_SCENE_ON_ABORT_STOP_SCENE))
    {
	    CPhysical *pPhys = GetPhysical();
	    if(pPhys)
	    {
		    fwAnimDirectorComponentSyncedScene *animDirectorComponentSyncedScene = pPhys->GetAnimDirector()?pPhys->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>():NULL;
		    if(animDirectorComponentSyncedScene)
		    {
			    /* Check we have a synced scene to stop, it may have already been stopped and cleaned up. */
			    if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(animDirectorComponentSyncedScene->GetSyncedSceneId()))
			    {
				    if(IsSceneFlagSet(SYNCED_SCENE_ON_ABORT_STOP_SCENE))
				    {
					    animDirectorComponentSyncedScene->SetSyncedSceneAbort(animDirectorComponentSyncedScene->GetSyncedSceneId(), true);
				    }

					if (CTask::ABORT_PRIORITY_IMMEDIATE == iPriority)
					{
                        DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene - (immediate abort) Stopping synced scene playback!", pPhys ? pPhys->GetDebugName() : "Unknown");
						animDirectorComponentSyncedScene->StopSyncedScenePlayback();
					}
					else
					{
                        DEBUG_NET_SYNCED_SCENE("%s - CTaskSynchronizedScene - (non-immediate abort) Stopping synced scene playback!", pPhys ? pPhys->GetDebugName() : "Unknown");
						animDirectorComponentSyncedScene->StopSyncedScenePlayback(m_blendOutDelta, IsSceneFlagSet(SYNCED_SCENE_TAG_SYNC_OUT));
					}
				    pPhys->GetAnimDirector()->RemoveAllComponentsByType(fwAnimDirectorComponent::kComponentTypeSyncedScene);
				    pPhys->RemoveSceneUpdateFlags(CGameWorld::SU_AFTER_ALL_MOVEMENT);
			    }
		    }
            else if(NetworkInterface::IsGameInProgress() && m_NetworkSceneID != -1)
            {
                CPed *pPed = TryGetPed();

                if(pPed)
                {
                    CNetworkSynchronisedScenes::OnPedAbortingSyncedSceneEarly(pPed, m_NetworkSceneID);
                }
            }
	    }
    }

    // inform the network synced scene system when a ped aborts their synced scene task
    if(NetworkInterface::IsGameInProgress() && m_NetworkSceneID != -1)
    {
        CPed *pPed = TryGetPed();

        if(pPed)
        {
            atString eventName("");

#if !__NO_OUTPUT
            eventName = pEvent ? pEvent->GetName() : "None";
#endif // !__NO_OUTPUT

            CNetworkSynchronisedScenes::OnPedAbortingSyncedScene(pPed, m_NetworkSceneID, eventName);
        }
    }

	// Base class
	CTask::DoAbort(iPriority, pEvent);
}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::ApplyRagdollBlockingFlags(CPed* pPed)
{
	// remove any ragdoll blocking flags that are already turned on.
	// this is so they don't get removed when the scene ends.
	m_ragdollBlockingFlags.BitSet().IntersectNegate(pPed->GetRagdollBlockingFlags().BitSet());
	
	pPed->ApplyRagdollBlockingFlags(m_ragdollBlockingFlags);
}

//////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskSynchronizedScene::CreateQueriableState() const
{
	CClonedSynchronisedSceneInfo *pSyncedSceneInfo = rage_new CClonedSynchronisedSceneInfo(m_NetworkSceneID, m_EstimatedCompletionTime);

    return pSyncedSceneInfo;
}

//////////////////////////////////////////////////////////////////////////

void CTaskSynchronizedScene::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SYNCHRONISED_SCENE);

    CClonedSynchronisedSceneInfo *pSyncedSceneInfo = static_cast<CClonedSynchronisedSceneInfo*>(pTaskInfo);

	if(m_NetworkSceneID != pSyncedSceneInfo->GetNetworkSceneID())
	{
		m_NetworkSceneID = pSyncedSceneInfo->GetNetworkSceneID();
	}

    m_EstimatedCompletionTime = pSyncedSceneInfo->GetEstimatedCompletionTime();

    CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

//////////////////////////////////////////////////////////////////////////

bool CTaskSynchronizedScene::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	return GetState() >= State_RunningScene;
}


//////////////////////////////////////////////////////////////////////////

bool CTaskSynchronizedScene::IgnoresCloneTaskPriorities() const
{
    if(m_flags.BitSet().IsSet(SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE))
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

CTaskSynchronizedScene::FSM_Return CTaskSynchronizedScene::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM( iState,  iEvent);
}

//////////////////////////////////////////////////////////////////////////

CTaskFSMClone *CTaskSynchronizedScene::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
    CTaskSynchronizedScene *newTask = rage_new CTaskSynchronizedScene(*this);

    SetIsMigrating(true);

    return newTask;
}

//////////////////////////////////////////////////////////////////////////

CTaskFSMClone *CTaskSynchronizedScene::CreateTaskForLocalPed(CPed* pPed)
{
	CTaskSynchronizedScene *newTask = static_cast<CTaskSynchronizedScene*>(CreateTaskForClonePed(pPed));

	if(m_NetworkCoordinateScenePhase)
	{
		//If this ped is now local reset the scene update rate to original
		float fOriginalRate;
		CNetworkSynchronisedScenes::GetSceneRate(m_NetworkSceneID, fOriginalRate);
		fwAnimDirectorComponentSyncedScene::SetSyncedSceneRate(m_scene, fOriginalRate);
	}

    return newTask;
}



