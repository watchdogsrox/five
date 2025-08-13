#include "TaskVehicleAnimation.h"

// Game headers
#include "animation/MoveVehicle.h"
#include "modelinfo/VehicleModelInfoExtensions.h"
#include "Vehicles/vehicle.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"
#include "VehicleAi/task/TaskVehicleGoto.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/task/TaskVehiclePark.h"
#include "VehicleAi/task/TaskVehicleTempAction.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "peds/ped.h"
#include "peds/PlayerPed.h"
#include "physics/gtaInst.h"
#include "pharticulated/articulatedcollider.h"
#include "streaming/streaming.h"		// For CStreaming::LoadAllRequestedObjects().
#include "system/control.h"
#include "phbound/boundcomposite.h"
#include "text/messages.h"
#include "audio/caraudioentity.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "vehicles/Submarine.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////////////

CTaskVehicleAnimation::CTaskVehicleAnimation(	const strStreamingObjectName pAnimDictName, 
												const char* pAnimName,
												float playBackRate,
                                                float playBackPhase,
                                                bool holdAtEndOfAnimation,
												bool useExtractedVelocity ) :
CTaskVehicleMissionBase(),
m_animDictIndex(-1),
m_animHashKey(0),
m_holdAtEndOfAnimation(holdAtEndOfAnimation),
m_playBackRate(playBackRate),
m_playBackPhase(playBackPhase),
m_useExtractedVelocity(useExtractedVelocity)
{
	Assertf(pAnimDictName.IsNotNull(), "Null anim dictionary name\n");
	Assertf(pAnimName, "Null anim name\n");

	if (pAnimDictName.IsNotNull())
	{
		m_animDictIndex = fwAnimManager::FindSlot(pAnimDictName).Get();
	}

	if (pAnimName)
	{
		Assertf(strlen(pAnimName)<ANIM_NAMELEN, "Animation name is too long : %s ", pAnimName);
		m_animHashKey = atStringHash(pAnimName);
	}

#if __DEV
	if (pAnimDictName.IsNotNull())
	{
		m_animDictName = pAnimDictName;
	}

	if (pAnimName)
	{
		strncpy(m_animName, pAnimName, ANIM_NAMELEN);
	}

	if (pAnimDictName.IsNotNull() && pAnimName)
	{
		Assertf(fwAnimManager::GetClipIfExistsByDictIndex(m_animDictIndex, m_animHashKey), "Missing anim (anim dict name, anim name): %s, %s\n", pAnimDictName.GetCStr(), pAnimName);
	}
#endif //__DEV

	Assert((m_animDictIndex != -1) && (m_animHashKey != 0));
	Assertf(fwAnimManager::GetClipIfExistsByDictIndex(m_animDictIndex, m_animHashKey), "Missing anim (anim dict index, anim hash key): %d, %d\n", m_animDictIndex, m_animHashKey);
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_ANIMATION);

}

///////////////////////////////////////////////////////////////////////////////////////

CTaskVehicleAnimation::CTaskVehicleAnimation(	u32 animDictIndex, 
                                                u32 animHashKey,
                                                float playBackRate,
                                                float playBackPhase,
                                                bool holdAtEndOfAnimation,
                                                bool useExtractedVelocity ) :
m_playBackPhase(playBackPhase),
m_playBackRate(playBackRate),
m_holdAtEndOfAnimation(holdAtEndOfAnimation),
m_useExtractedVelocity(useExtractedVelocity)
{
    m_animHashKey = animHashKey;
    m_animDictIndex = animDictIndex;
    Assert((m_animDictIndex != -1) && (m_animHashKey != 0));

#if __DEV
    const strStreamingObjectName pAnimDictName = fwAnimManager::GetName(strLocalIndex(m_animDictIndex));
    if (pAnimDictName.IsNotNull())
    {
        m_animDictName = pAnimDictName;
    }

    const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_animDictIndex, m_animHashKey);
    if (pClip && pClip->GetName())
    {
        strcpy(m_animName, pClip->GetName());
    }
#endif // __DEV
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_ANIMATION);
}

CTaskVehicleAnimation::~CTaskVehicleAnimation()
{
	if (GetVehicle() && GetVehicle()->GetAnimDirector())
	{
		CGenericClipHelper& helper = GetVehicle()->GetMoveVehicle().GetClipHelper();
		helper.BlendOutClip(INSTANT_BLEND_OUT_DELTA);
	}
}


///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleAnimation::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)

	FSM_Begin
		// Entry point
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		// Play an animation
		FSM_State(State_PlayingAnim)
			FSM_OnEnter
				PlayingAnim_OnEnter(pVehicle);
			FSM_OnUpdate
				return PlayingAnim_OnUpdate(pVehicle);
			FSM_OnExit
				PlayingAnim_OnExit(pVehicle);

		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleAnimation::Start_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if(Verifyf(fwAnimManager::GetClipIfExistsByDictIndex(m_animDictIndex, m_animHashKey), "Animation does not exist"))
	{
		SetState(State_PlayingAnim);
		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAnimation::PlayingAnim_OnEnter( CVehicle* pVehicle )
{
    pVehicle->m_nVehicleFlags.bAnimateJoints = true;

	StartAnim(pVehicle);
}

///////////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleAnimation::IsAnimFinished() const
{ 
    if( GetState() == State_PlayingAnim || GetState() == State_Finish)
    {
        const CGenericClipHelper& helper = GetVehicle()->GetMoveVehicle().GetClipHelper();
		
		if (helper.GetLooped()==false)
        {
            float currentPhase = helper.GetPhase();
            if(m_playBackPhase > 0.0f)
            {
                if ( currentPhase <= 0.01f )
                {
                    return true;
                }
            }
            else
            {
                if ( currentPhase >= 0.99f )
                {
                    return true;
                }
            }
        }
    }

    return false;
}


///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleAnimation::PlayingAnim_OnUpdate( CVehicle* pVehicle )
{
	CGenericClipHelper& helper = pVehicle->GetMoveVehicle().GetClipHelper();

	// If there is no anim, the task will have finished
	if(helper.GetClip()==NULL)
	{
		SetState(State_Finish);
	}
    else
    {
        if(!m_holdAtEndOfAnimation)
        {
            if(IsAnimFinished())
            {
                SetState(State_Finish);
            }
        } 
        
        if(m_useExtractedVelocity)
        {	
            if (GetClipHelper()->GetClip())
            {
                const crClip* pClip = GetClipHelper()->GetClip();

                static bool s_bEnableVehicleMoverExtraction = true;
				if (s_bEnableVehicleMoverExtraction && fwAnimHelpers::HasMoverTranslation(*pClip))
                {
					fwDynamicEntityComponent *vehDynComp = pVehicle->CreateDynamicComponentIfMissing();

                    const Vector3 vecExtractelVel(vehDynComp->GetAnimatedVelocity());
                    float fExtractedAngularVel = vehDynComp->GetAnimatedAngularVelocity();

                    pVehicle->SetVelocity(vecExtractelVel);

                    float fDesiredHeading = fwAngle::LimitRadianAngleSafe(fExtractedAngularVel);

                    Vector3 vAngVel = VEC3_ZERO;
                    vAngVel += VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC()) * (fDesiredHeading);

                    pVehicle->SetAngVelocity(vAngVel);
                }
            }
        }
    }

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAnimation::PlayingAnim_OnExit(CVehicle* pVehicle)
{
    pVehicle->m_nVehicleFlags.bAnimateJoints = false;
	CGenericClipHelper& helper = pVehicle->GetMoveVehicle().GetClipHelper();
	helper.BlendOutClip(INSTANT_BLEND_OUT_DELTA);
}


///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleAnimation::StartAnim(CVehicle* pVehicle)
{
	pVehicle->InitAnimLazy();
	Assert(pVehicle->GetAnimDirector());

	CGenericClipHelper& helper = pVehicle->GetMoveVehicle().GetClipHelper();

	helper.BlendInClipByDictAndHash(pVehicle, m_animDictIndex, m_animHashKey, INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, false, m_holdAtEndOfAnimation );

	//Stick to the last frame once the anim has finished.
	if ( vehicleVerifyf(helper.GetClip()!=NULL, "Animation failed to start") )
    {    
        helper.SetPhase(m_playBackPhase);
        helper.SetRate(m_playBackRate);
	}
}

////////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleAnimation::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_PlayingAnim",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif

///////////////////////////////////////////////////////////////////////////////////////
float	CTaskVehicleConvertibleRoof::ms_fMaximumSpeedToOpenRoofSq = 10.0f * 10.0f;
u32		CTaskVehicleConvertibleRoof::ms_uTimeToHoldButtonDown = 250;

static const s32 MAX_ROOF_PARTS = 3;
eHierarchyId sRoofIds[MAX_ROOF_PARTS] = 
{      
    VEH_MISC_A,
    VEH_ROOF,
    VEH_ROOF2,
};

CTaskVehicleConvertibleRoof::CTaskVehicleConvertibleRoof( bool bInstantlyMoveRoof, bool bRoofLowered ) :   
#if DISPLAY_HELP_TEXT
m_HelpMessageDisplayed(false),
m_MessageTimeDisplayedFor(0),
#endif
m_bInstantlyMoveRoof(bInstantlyMoveRoof),
m_DesiredStateAfterClosingBoot(State_Invalid),
m_nStuckCounter(0),
m_bOkToDecrementCounter(true),
m_bWaitingOnDoorClosing(false),
m_bMechanismShouldJam(false),
m_bHasButtonBeenUp(false)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_CONVERTIBLE_ROOF);

	if (bRoofLowered)
		SetState(State_Roof_Lowered);
}

CTaskVehicleConvertibleRoof::~CTaskVehicleConvertibleRoof()
{

}

///////////////////////////////////////////////////////////////////////////////////////

CTaskVehicleConvertibleRoof::eConvertibleStates  CTaskVehicleConvertibleRoof::GetRoofState()
{
	CVehicle* pVehicle	= GetVehicle();
	CCarDoor *pDoor		= pVehicle->GetDoorFromId( VEH_BOOT );

    switch(GetState())
    {
    case State_Roof_Raised:
        return STATE_RAISED;
    case State_Lower_Roof:
        return STATE_LOWERING;
    case State_Roof_Lowered:
		if( pDoor != NULL && 
			( !pDoor->GetIsLatched( pVehicle ) || pDoor->GetEverKnockedOpen() || pVehicle->IsUpsideDown() ) )
		{
			return STATE_ROOF_STUCK_LOWERED;
		}
		return STATE_LOWERED;
    case State_Raise_Roof:
        return STATE_RAISING;
	case State_Closing_Boot:
		return STATE_CLOSING_BOOT;
	case State_Roof_Stuck_Raised:
		return STATE_ROOF_STUCK_RAISED;
	case State_Roof_Stuck_Lowered:
		return STATE_ROOF_STUCK_LOWERED;
    default:
        vehicleAssertf(0, "Invalid convertible roof state");
		break;
    }

    //Default state
    return STATE_RAISED;
}

///////////////////////////////////////////////////////////////////////////////////////

float CTaskVehicleConvertibleRoof::GetRoofProgress()
{
	switch(GetState())
    {
	case State_Lower_Roof:
	case State_Raise_Roof:
	case State_Closing_Boot:
		{
			CVehicle *pVehicle = GetVehicle();
			CMoveVehicle& move = pVehicle->GetMoveVehicle();
			return move.GetMechanismPhase();
		}
		break;
	case State_Roof_Raised:
	case State_Roof_Stuck_Raised:
		return 0.0f;
		break;
    case State_Roof_Lowered:
	case State_Roof_Stuck_Lowered:
		return 1.0f;
		break;
	default:
        vehicleAssertf(0, "Invalid convertible roof state");
		return 0.0f;
		break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleConvertibleRoof::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
/*
	TUNE_BOOL(DEBUG_CONVERTIBLE_FSM, false);
	if(DEBUG_CONVERTIBLE_FSM)
	{
		grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), Color_white, GetStaticStateName(iState), true);
	}
*/
	FSM_Begin

        FSM_State(State_Roof_Raised)
            FSM_OnEnter
                RoofRaised_OnEnter(pVehicle);
            FSM_OnUpdate
                return RoofRaised_OnUpdate(pVehicle);

		FSM_State(State_Lower_Roof)
			FSM_OnEnter
				LowerRoof_OnEnter(pVehicle);
			FSM_OnUpdate
				return LowerRoof_OnUpdate(pVehicle);

        FSM_State(State_Roof_Lowered)
            FSM_OnEnter
                RoofLowered_OnEnter(pVehicle);
            FSM_OnUpdate
                return RoofLowered_OnUpdate(pVehicle);

		FSM_State(State_Raise_Roof)
			FSM_OnEnter
				RaiseRoof_OnEnter(pVehicle);
			FSM_OnUpdate
				return RaiseRoof_OnUpdate(pVehicle);

		FSM_State(State_Closing_Boot)
			FSM_OnEnter
				ClosingBoot_OnEnter(pVehicle);
			FSM_OnUpdate
				return ClosingBoot_OnUpdate(pVehicle);
			FSM_OnExit
				ClosingBoot_OnExit(pVehicle);

		FSM_State(State_Roof_Stuck_Raised)
			FSM_OnEnter
				RoofStuckRaised_OnEnter(pVehicle);
			FSM_OnUpdate
				return RoofStuckRaised_OnUpdate(pVehicle);

		FSM_State(State_Roof_Stuck_Lowered)
			FSM_OnEnter
				RoofStuckLowered_OnEnter(pVehicle);
			FSM_OnUpdate
				return RoofStuckLowered_OnUpdate(pVehicle);


		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::CleanUp() 
{
	// Roof animation can continue after abort, so make sure vehicle flag is set to eventual position
	CVehicle* pVehicle = GetVehicle();

	if (GetState() == State_Lower_Roof)
	{
		pVehicle->m_nVehicleFlags.bRoofLowered = true;
	}
	else if (GetState() == State_Raise_Roof)
	{
		pVehicle->m_nVehicleFlags.bRoofLowered = false;
	}
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::UnLatchRoof(CVehicle* pVehicle)
{
	ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
    fragInstGta* pFragInst = pVehicle->GetVehicleFragInst();
    Assert(pFragInst);

    pFragInst->GetCacheEntry()->LazyArticulatedInit();

    for(s32 i = 0; i < MAX_ROOF_PARTS; i++)
    {
        s32 iBoneIndex = pVehicle->GetBoneIndex(sRoofIds[i]);
        s8 nFragGroup = (s8)pVehicle->GetVehicleFragInst()->GetGroupFromBoneIndex(iBoneIndex);

        if(nFragGroup > -1 && GetIsLatched(pVehicle, nFragGroup))
        {
            s32 nGroup = pFragInst->GetType()->GetPhysics(0)->GetAllChildren()[nFragGroup]->GetOwnerGroupPointerIndex();
            if(pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(nGroup))
            {
                pFragInst->OpenLatchAbove(nFragGroup);

                phBoundComposite* pThisBoundComposite = static_cast<phBoundComposite*>(pVehicle->GetVehicleFragInst()->GetArchetype()->GetBound());

                // turn on collision with world when open
                if(pThisBoundComposite->GetTypeAndIncludeFlags())
                    pThisBoundComposite->SetIncludeFlags(nFragGroup, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::LatchRoof(CVehicle* pVehicle)
{
	ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)

    for(s32 i = MAX_ROOF_PARTS-1; i >= 0; i--)
    {
        s32 iBoneIndex = pVehicle->GetBoneIndex(sRoofIds[i]);
        s8 nFragGroup = (s8)pVehicle->GetVehicleFragInst()->GetGroupFromBoneIndex(iBoneIndex);
		s32 nComponent = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndex);
        if(nFragGroup > -1 && !GetIsLatched(pVehicle, nFragGroup) && pVehicle->IsColliderArticulated())
		{
			if(pVehicle->GetVehicleFragInst()->GetCached())
			{
				const phArticulatedCollider& articulatedCollider = *pVehicle->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider;

				// Find out to what link this fragTypeChild maps.
				int linkIndex = articulatedCollider.GetLinkFromComponent(nComponent);
				if(linkIndex > 0) // Only set latch if the component is part of the articulated body
				{
					SetLatch(pVehicle, nFragGroup);
				}
			}			
		}
    }
}

//////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::InitAnimation	(CVehicle* pVehicle)
{	
	if (!pVehicle->GetAnimDirector())
	{
		pVehicle->InitAnimLazy();
	}
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::RoofRaised_OnEnter			(CVehicle* pVehicle)
{
#if DISPLAY_HELP_TEXT
    CPed *pPed = pVehicle->GetDriver();
    if( pPed && pPed->IsLocalPlayer() )
    {
        CHelpMessage::SetMessageText(TheText.Get("L_ROOF"));//if we have a roof and the hand brakes pressed, lower the roof.
        m_MessageTimeDisplayedFor = 0;
        m_HelpMessageDisplayed = true;
    }
#endif

    LatchRoof(pVehicle);

	pVehicle->GetVehicleAudioEntity()->StopAllRoofLoops();
	pVehicle->ToggleRoofCollision(true);

	// Mark any windows which get rolled up as part of the animation as "up".
	CConvertibleRoofWindowInfo* pWindowExtension = pVehicle->GetBaseModelInfo()->GetExtension<CConvertibleRoofWindowInfo>();
	if(pWindowExtension)
	{
		atArray<eHierarchyId>& affectedWindowList = pWindowExtension->GetWindowList();
		for(int i = 0; i < affectedWindowList.GetCount(); ++i)
		{
			// Flag this window as having been rolled up.
			pVehicle->RollWindowUp(affectedWindowList[i]);
		}
	}

	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
}

///////////////////////////////////////////////////////////////////////////////////////
#if DISPLAY_HELP_TEXT
static dev_s32 siHelpMessageDisplayTime = 3000;
#endif

CTask::FSM_Return CTaskVehicleConvertibleRoof::RoofRaised_OnUpdate			(CVehicle* pVehicle)
{
    CPed *pPed = pVehicle->GetDriver();
    if( pPed && pPed->IsLocalPlayer() )
    {
#if DISPLAY_HELP_TEXT
        if(m_HelpMessageDisplayed)
        {
            u32 SystemTimeStep = fwTimer::GetTimeStepInMilliseconds();
            m_MessageTimeDisplayedFor += SystemTimeStep;

            if (m_MessageTimeDisplayedFor >= siHelpMessageDisplayTime)
            {
                CHelpMessage::Clear();
                m_HelpMessageDisplayed = false;
            }
        }
#endif

        CControl *pControl = pPed->GetControlFromPlayer();
		m_bHasButtonBeenUp |= pControl->GetVehicleRoof().IsUp();
        if(m_bHasButtonBeenUp && pControl->GetVehicleRoof().HistoryHeldDown(ms_uTimeToHoldButtonDown))
        {

            if(pVehicle->GetVelocity().Mag2() < ms_fMaximumSpeedToOpenRoofSq && pVehicle->m_nVehicleFlags.bEngineOn
				&& (pVehicle->GetTransform().GetC().GetZf()>0.0f))
            {
#if DISPLAY_HELP_TEXT
                CHelpMessage::Clear();
                m_HelpMessageDisplayed = false;
#endif

				// If the boot is part of the animation and it isn't latched, it will screw up. Need to close
				// the boot before switching to the raising state.
				CCarDoor *pDoor = pVehicle->GetDoorFromId(VEH_BOOT);
				if(pDoor == NULL || (pDoor->GetIsLatched(pVehicle) && !pDoor->GetEverKnockedOpen() && !pVehicle->IsUpsideDown()))
				{
					SetState(State_Lower_Roof);
				}
				else
				{
					m_DesiredStateAfterClosingBoot = State_Lower_Roof;
					SetState(State_Closing_Boot);
				}
            }
        }
    }

    return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::LowerRoof_OnEnter			(CVehicle* pVehicle)
{
	m_animRequester.Release();

	if (!AssertVerify(pVehicle))
		return;

	if (!AssertVerify(pVehicle->GetVehicleModelInfo()))
		return;

	InitAnimation(pVehicle);

	// smash any partially broken windows if required
	g_vehicleGlassMan.ConvertibleRoofAnimStart(pVehicle);

	// Find the animation
    // These anims need renaming and adding to animid's instead of loaded in from vehicles.ide
    u32 iAnimHashKey = pVehicle->GetVehicleModelInfo()->GetConvertibleRoofAnimNameHash();

	// Stream the anim dictionary
    strLocalIndex iSlot = pVehicle->GetVehicleModelInfo()->GetClipDictionaryIndex();
  
	if( vehicleVerifyf(iSlot.IsValid(), "No animation dictionary found for vehicle. Check vehicles.ide") )
	{
		if(!m_animRequester.Request(iSlot,fwAnimManager::GetStreamingModuleId(),STRFLAG_PRIORITY_LOAD))
		{
			// Force load
			CStreaming::LoadAllRequestedObjects(true);
		}
		Assert(m_animRequester.HasLoaded());
	}

    UnLatchRoof(pVehicle);

    float aRoofStrength[MAX_ROOF_PARTS] = 
    {
        400.0f,
        800.0f,
        1400.0f,
    };

	if (!AssertVerify(pVehicle->GetFragInst()))
		return;

	if (!AssertVerify(pVehicle->GetFragInst()->GetCacheEntry()))
		return;

	if (!AssertVerify(pVehicle->GetFragInst()->GetType()))
		return;

    for(int i = 0; i < MAX_ROOF_PARTS; i++)
    {  
        fragHierarchyInst* pHierInst = pVehicle->GetFragInst()->GetCacheEntry()->GetHierInst();

        int iBoneIndex = pVehicle->GetVehicleModelInfo()->GetBoneIndex(sRoofIds[i]);

        int iComponentIndex = pVehicle->GetFragInst()->GetType()->GetComponentFromBoneIndex(0,iBoneIndex);

        if(iBoneIndex > -1 && iComponentIndex > -1)
        {
			if (!AssertVerify(pHierInst))
				return;

            phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;

			if (!AssertVerify(pArticulatedCollider))
				return;

            if(pVehicle->GetFragInst() && pVehicle->GetFragInst()->GetCacheEntry())
                pVehicle->GetFragInst()->GetCacheEntry()->LazyArticulatedInit();

			if(!Verifyf(iComponentIndex < pArticulatedCollider->GetNumComponents(),"iComponentIndex[%d] !< pArticulatedCollider->GetNumComponents()[%d] for vehicle[%s] : %s",iComponentIndex,pArticulatedCollider->GetNumComponents(),pVehicle->GetDebugName(), (pVehicle->GetCurrentPhysicsInst() && pVehicle->GetCurrentPhysicsInst()->GetArchetype() ? pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetFilename(): " noname ") ))
				return;

            int linkIndex = pArticulatedCollider->GetLinkFromComponent(iComponentIndex);
            if(linkIndex > 0)
            {
				if (!AssertVerify(pHierInst->body))
					return;

                phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
                if (pJoint)
                {
                    pJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE);
                    switch (pJoint->GetJointType())
                    {
                    case phJoint::JNT_1DOF:
                        {
                            phJoint1Dof& joint1Dof = *static_cast<phJoint1Dof*>(pJoint);
                            joint1Dof.SetMuscleAngleStrength(aRoofStrength[i]);
                            joint1Dof.SetAngleLimits(-2.0f*PI, 2.0f*PI);//remove angle limits, wont be necessary once model is setup properly
                        }
                    }
                }
            }
        }
    }

    if(m_bInstantlyMoveRoof)
    {
        pVehicle->m_nVehicleFlags.bForcePostCameraAnimUpdate = 1;
    }

    CMoveVehicle& move = pVehicle->GetMoveVehicle();

    float playbackRate = 1.0f;
    float playbackPhase = 0.0f;

	if(!move.IsMechanismActive())
	{
		if(m_bInstantlyMoveRoof)
		{
			m_bInstantlyMoveRoof = false;
			playbackPhase = 1.0f;
		}

		move.StartMechanism(fwAnimManager::GetClipIfExistsByDictIndex(iSlot.Get(), iAnimHashKey), 0.0f, playbackPhase, playbackRate);
	}
	else
	{
		move.OpenMechanism(playbackRate);
		if(m_bInstantlyMoveRoof)
		{
			m_bInstantlyMoveRoof = false;
			move.SetMechanismPhase(1.0f);
		}
	}
	
	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleConvertibleRoof::LowerRoof_OnUpdate			(CVehicle* pVehicle)
{
	static float sfConversionRevertMinPhase = 0.40f;

	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	if (move.IsMechanismFullyOpen())
	{
		pVehicle->m_nVehicleFlags.bRoofLowered = true;
		SetState(State_Roof_Lowered);
	}


	if(pVehicle->GetVelocity().Mag2() < ms_fMaximumSpeedToOpenRoofSq)
	{
		CPed *pPed = pVehicle->GetDriver();
		if( pPed && pPed->IsLocalPlayer() )
		{
			CControl *pControl = pPed->GetControlFromPlayer();
			m_bHasButtonBeenUp |= pControl->GetVehicleRoof().IsUp();
			if(m_bHasButtonBeenUp && pControl->GetVehicleRoof().HistoryHeldDown(ms_uTimeToHoldButtonDown))
			{
				SetState(State_Raise_Roof);
			}
		}
	}
	else
	{
		float fPhase = move.GetMechanismPhase();
		// If vehicle is speed up, revert the convert process if it hasn't reached the half way
		if(fPhase > 0.0f && fPhase < sfConversionRevertMinPhase)
		{
			SetState(State_Raise_Roof);
		}
	}


	return FSM_Continue;
}


///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::RoofLowered_OnEnter			(CVehicle* pVehicle)
{
    CPed *pPed = pVehicle->GetDriver();
    if( pPed && pPed->IsLocalPlayer() )
    {
#if DISPLAY_HELP_TEXT
        CHelpMessage::SetMessageText(TheText.Get("R_ROOF"));//if we have a roof and the hand brakes pressed, lower the roof.
        m_MessageTimeDisplayedFor = 0;
        m_HelpMessageDisplayed = true;
#endif
    }

    //This latches the roof in the closed poisition, so i'm not doing it for now.
    //LatchRoof(pVehicle);

	pVehicle->GetVehicleAudioEntity()->StopAllRoofLoops();
	pVehicle->ToggleRoofCollision(false);

	// Mark any windows which get rolled down as part of the animation as "down".
	CConvertibleRoofWindowInfo* pWindowExtension = pVehicle->GetBaseModelInfo()->GetExtension<CConvertibleRoofWindowInfo>();
	if(pWindowExtension)
	{
		atArray<eHierarchyId>& affectedWindowList = pWindowExtension->GetWindowList();
		for(int i = 0; i < affectedWindowList.GetCount(); ++i)
		{
			// Flag this window as having been rolled down.
			pVehicle->RolldownWindow(affectedWindowList[i]);
		}
	}

	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleConvertibleRoof::RoofLowered_OnUpdate			(CVehicle* pVehicle)
{
    CPed *pPed = pVehicle->GetDriver();
    if( pPed && pPed->IsLocalPlayer() )
    {
#if DISPLAY_HELP_TEXT
        if(m_HelpMessageDisplayed)
        {
            u32 SystemTimeStep = fwTimer::GetTimeStepInMilliseconds();
            m_MessageTimeDisplayedFor += SystemTimeStep;

            if (m_MessageTimeDisplayedFor >= siHelpMessageDisplayTime)
            {
                CHelpMessage::Clear();
                m_HelpMessageDisplayed = false;
            }
        }
#endif

        CControl *pControl = pPed->GetControlFromPlayer();
		m_bHasButtonBeenUp |= pControl->GetVehicleRoof().IsUp();
        if(m_bHasButtonBeenUp && pControl->GetVehicleRoof().HistoryHeldDown(ms_uTimeToHoldButtonDown))
        { 

            if(pVehicle->GetVelocity().Mag2() < ms_fMaximumSpeedToOpenRoofSq && pVehicle->m_nVehicleFlags.bEngineOn
				&& (pVehicle->GetTransform().GetC().GetZf()>0.0f))
            {
#if DISPLAY_HELP_TEXT
                CHelpMessage::Clear();
                m_HelpMessageDisplayed = false;
#endif

				// If the boot is part of the animation and it isn't latched, it will screw up. Need to close
				// the boot before switching to the raising state.
				CCarDoor *pDoor = pVehicle->GetDoorFromId(VEH_BOOT);
				if(pDoor == NULL || (pDoor->GetIsLatched(pVehicle) && !pDoor->GetEverKnockedOpen() && !pVehicle->IsUpsideDown()))
				{
					SetState(State_Raise_Roof);
				}
				else
				{
					m_DesiredStateAfterClosingBoot = State_Raise_Roof;
					SetState(State_Closing_Boot);
				}
            }
        }
    }

    return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::RaiseRoof_OnEnter			(CVehicle* pVehicle)
{
    m_animRequester.Release();

	InitAnimation(pVehicle);

    Assert(pVehicle->GetVehicleModelInfo());

    // Find the animation
    // These anims need renaming and adding to animid's instead of loaded in from vehicles.ide
    u32 iAnimHashKey = pVehicle->GetVehicleModelInfo()->GetConvertibleRoofAnimNameHash();

    // Stream the anim dictionary
    strLocalIndex iSlot = pVehicle->GetVehicleModelInfo()->GetClipDictionaryIndex();

    if( vehicleVerifyf(iSlot.IsValid(), "No animation dictionary found for vehicle. Check vehicles.ide") )
    {
        if(!m_animRequester.Request(iSlot,fwAnimManager::GetStreamingModuleId(),STRFLAG_PRIORITY_LOAD))
        {
            // Force load
            CStreaming::LoadAllRequestedObjects(true);
        }
        Assert(m_animRequester.HasLoaded());
    }

    //Not required as I don't latch the roof when the roof is open.
    //UnLatchRoof(pVehicle);

    if(m_bInstantlyMoveRoof)
    {
        pVehicle->m_nVehicleFlags.bForcePostCameraAnimUpdate = 1;
    }

    float playbackRate = -1.0f;
    float playbackPhase = 1.0f;
	CMoveVehicle& move = pVehicle->GetMoveVehicle();
	if(!move.IsMechanismActive())
	{    
		//if we want to instantly move the roof, set the playback phase to 0.0 i.e already completed
		if(m_bInstantlyMoveRoof)
		{
			m_bInstantlyMoveRoof = false;
			playbackPhase = 0.0f;
		}

		move.StartMechanism(fwAnimManager::GetClipIfExistsByDictIndex(iSlot.Get(), iAnimHashKey), 0.0f, playbackPhase, playbackRate);
	}
	else
	{
		move.CloseMechanism(playbackRate);
		if(m_bInstantlyMoveRoof)
		{
			m_bInstantlyMoveRoof = false;
			move.SetMechanismPhase(0.0f);
		}
	}

	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
	
	//This will allow rolled down windows (set when spawned) to roll up again
	pVehicle->RollupWindows(false);
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleConvertibleRoof::RaiseRoof_OnUpdate		(CVehicle* pVehicle)
{
	static float sfConversionRevertMaxPhase = 0.50f;

	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	if(move.IsMechanismFullyClosed())
	{
		pVehicle->m_nVehicleFlags.bRoofLowered = false;
		SetState(State_Roof_Raised);
	}

	if(pVehicle->GetVelocity().Mag2() < ms_fMaximumSpeedToOpenRoofSq)
	{
		CPed *pPed = pVehicle->GetDriver();
		if( pPed && pPed->IsLocalPlayer() )
		{
			CControl *pControl = pPed->GetControlFromPlayer();
			m_bHasButtonBeenUp |= pControl->GetVehicleRoof().IsUp();
			if(m_bHasButtonBeenUp && pControl->GetVehicleRoof().HistoryHeldDown(ms_uTimeToHoldButtonDown))
			{
				SetState(State_Lower_Roof);
			}
		}
	}
	else
	{
		// If vehicle is speed up, revert the convert process if it hasn't reached the half way
		if(move.GetMechanismPhase() > sfConversionRevertMaxPhase)
		{
			SetState(State_Lower_Roof);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::ClosingBoot_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{

}

////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleConvertibleRoof::ClosingBoot_OnUpdate(CVehicle* pVehicle)
{
	CCarDoor* pBoot = pVehicle->GetDoorFromId(VEH_BOOT);

	// Door can't be closed if it's broken off. In this case, stutter through the first few
	// frames of the roof animation to give the user some visual clue that the roof is going
	// nowhere.

	if(!pBoot || (pBoot && (pBoot->GetFlag(CCarDoor::IS_BROKEN_OFF) || pBoot->GetEverKnockedOpen() || pVehicle->IsUpsideDown())))
	{
		m_bMechanismShouldJam = true;
		m_bWaitingOnDoorClosing = false;
	}
	else
	{
		m_bMechanismShouldJam = false;
	}

	if(pBoot && !pBoot->GetIsClosed() && !m_bMechanismShouldJam)
	{
		if( !m_bWaitingOnDoorClosing )
		{
			pBoot->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
			m_bWaitingOnDoorClosing = true;
		}
	}
	else
	{
		m_bMechanismShouldJam = true;

		if( pBoot &&
			!pBoot->GetFlag(CCarDoor::IS_BROKEN_OFF) )
		{
			pBoot->SetShutAndLatched(pVehicle);
		}

		Assert(m_DesiredStateAfterClosingBoot > State_Invalid);
		switch(m_DesiredStateAfterClosingBoot)
		{
		case State_Lower_Roof:
		case State_Roof_Stuck_Raised:
			{
				pVehicle->m_nVehicleFlags.bRoofLowered = false;

				if(m_bMechanismShouldJam)
					SetState(State_Roof_Stuck_Raised);
				else
					SetState(State_Roof_Raised);
			}
			break;
		case State_Raise_Roof:
		case State_Roof_Stuck_Lowered:
			{
				pVehicle->m_nVehicleFlags.bRoofLowered = true;

				if(m_bWaitingOnDoorClosing)
				{
					SetState(State_Raise_Roof);
				}
				else if(m_bMechanismShouldJam)
				{
					SetState(State_Roof_Stuck_Lowered);
				}
				else
				{
					SetState(State_Roof_Lowered);
				}
			}
			break;
		default:
			Assertf(false, "Unexpected desired state: %s.", GetStaticStateName(m_DesiredStateAfterClosingBoot));
		}
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::ClosingBoot_OnExit(CVehicle* UNUSED_PARAM(pVehicle))
{
	m_bWaitingOnDoorClosing = false;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::RoofStuckRaised_OnEnter(CVehicle* pVehicle)
{
	// Set the number of times we want to try lowering the roof before giving up.
	m_nStuckCounter = 3;
	m_bOkToDecrementCounter = true;

	// Make sure that the lowering animation is streamed in and started.
	LowerRoof_OnEnter(pVehicle);
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleConvertibleRoof::RoofStuckRaised_OnUpdate(CVehicle* pVehicle)
{
	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	static float sfRaisingAnimationMinPhase = 0.10f;
	static float sfStuckLoweringPlaybackRate = 2.0f;

    CCarDoor *pDoor = pVehicle->GetDoorFromId(VEH_BOOT);

	// If car gets healed by a cheat or if we have tried enough times to raise the roof, we
	// don't want to get stuck in this state.
	if(pDoor == NULL || (pDoor && !(pDoor->GetFlag(CCarDoor::IS_BROKEN_OFF) || pDoor->GetEverKnockedOpen() || pVehicle->IsUpsideDown())) || !m_nStuckCounter)
	{
		pVehicle->m_nVehicleFlags.bRoofLowered = false;
		SetState(State_Roof_Raised);
	}

	// If the boot is part of the animation and it isn't latched, it will screw up. Need to close
	// the boot before switching to the raising state.
	if( pDoor != NULL && !pDoor->GetFlag(CCarDoor::IS_BROKEN_OFF) &&
		(!pDoor->GetIsLatched(pVehicle) || !pDoor->GetIsClosed()))
	{
		m_DesiredStateAfterClosingBoot = State_Roof_Stuck_Raised;
		SetState(State_Closing_Boot);
		return FSM_Continue;
	}

	if(move.GetMechanismPhase() > sfRaisingAnimationMinPhase)
	{
		move.CloseMechanism(-sfStuckLoweringPlaybackRate);
		if(m_bOkToDecrementCounter)
		{	
			--m_nStuckCounter;
			m_bOkToDecrementCounter = false;

			if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				static_cast<audCarAudioEntity*>(pVehicle->GetVehicleAudioEntity())->TriggerRoofStuckSound();
			}
		}
	}
	else if(move.IsMechanismFullyClosed())
	{
		move.OpenMechanism(sfStuckLoweringPlaybackRate);
		m_bOkToDecrementCounter = true;
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::RoofStuckLowered_OnEnter(CVehicle* pVehicle)
{
	// Set the number of times we want to try raising the roof before giving up.
	m_nStuckCounter = 3;
	m_bOkToDecrementCounter = true;

	// Make sure that the raising animation is streamed in and started.
	RaiseRoof_OnEnter(pVehicle);
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleConvertibleRoof::RoofStuckLowered_OnUpdate(CVehicle* pVehicle)
{
	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	static float sfLoweringAnimationMaxPhase = 0.90f;
	static float sfStuckRaisingPlaybackRate = 2.0f;

    CCarDoor *pDoor = pVehicle->GetDoorFromId(VEH_BOOT);

	// If car gets healed by a cheat or if we have tried enough times to lower the roof, we
	// don't want to get stuck in this state.
    if(pDoor == NULL || (pDoor && !(pDoor->GetFlag(CCarDoor::IS_BROKEN_OFF) || pDoor->GetEverKnockedOpen() || pVehicle->IsUpsideDown())) || !m_nStuckCounter)
	{
		pVehicle->m_nVehicleFlags.bRoofLowered = true;
		SetState(State_Roof_Lowered);
	}

	// If the boot is part of the animation and it isn't latched, it will screw up. Need to close
	// the boot before switching to the raising state.
	if(pDoor != NULL && !pDoor->GetFlag(CCarDoor::IS_BROKEN_OFF) &&
		(!pDoor->GetIsLatched(pVehicle) || !pDoor->GetIsClosed()))
	{
		m_DesiredStateAfterClosingBoot = State_Roof_Stuck_Lowered;
		SetState(State_Closing_Boot);
		return FSM_Continue;
	}

	if(move.GetMechanismPhase() < sfLoweringAnimationMaxPhase)
	{
		move.OpenMechanism(sfStuckRaisingPlaybackRate);
		if(m_bOkToDecrementCounter)
		{
			--m_nStuckCounter;
			m_bOkToDecrementCounter = false;

			if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				static_cast<audCarAudioEntity*>(pVehicle->GetVehicleAudioEntity())->TriggerRoofStuckSound();
			}
		}
	}
	else if(move.IsMechanismFullyOpen())
	{
		move.CloseMechanism(-sfStuckRaisingPlaybackRate);
		m_bOkToDecrementCounter = true;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleConvertibleRoof::GetIsLatched(CVehicle* pParent, s32 roofFragGroup)
{
	if(roofFragGroup < 0)
		return false;

	if(pParent->GetVehicleFragInst()->GetCached())
	{
		fragHierarchyInst* pHierInst = pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst();

		if(pHierInst->latchedJoints != NULL && pHierInst->latchedJoints->IsSet(roofFragGroup))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleConvertibleRoof::SetLatch(CVehicle* pParent, s32 roofFragGroup)
{
	if(!pParent || !pParent->GetVehicleFragInst())
		return;

	const fragType* pFragType = pParent->GetVehicleFragInst()->GetType();

	Assert(!GetIsLatched(pParent, roofFragGroup));
	if(roofFragGroup > 0
		&& pParent->GetVehicleFragInst()->GetCacheEntry()
		&& pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()
		&& pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->latchedJoints)
	{
		pParent->GetVehicleFragInst()->CloseLatchAbove(roofFragGroup);

		phBoundComposite* pThisBoundComposite = static_cast<phBoundComposite*>(pParent->GetVehicleFragInst()->GetArchetype()->GetBound());
		phBoundComposite* pTemplateBoundComposite = pFragType->GetPhysics(0)->GetCompositeBounds();

		int nParentGroup = pFragType->GetPhysics(0)->GetAllChildren()[roofFragGroup]->GetOwnerGroupPointerIndex();
		fragTypeGroup* pParentGroup = pFragType->GetPhysics(0)->GetAllGroups()[nParentGroup];
		for (int childIndex = 0; childIndex < pParentGroup->GetNumChildren(); ++childIndex)
		{
			int nChild = pParentGroup->GetChildFragmentIndex() + childIndex;
			pThisBoundComposite->SetCurrentMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));
			pThisBoundComposite->SetLastMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));
		}

		for(int groupIndex = 0; groupIndex < pParentGroup->GetNumChildGroups(); ++groupIndex)
		{
			fragTypeGroup* pGroup = pFragType->GetPhysics(0)->GetAllGroups()[pParentGroup->GetChildGroupsPointersIndex() + groupIndex];
			for (int childIndex = 0; childIndex < pGroup->GetNumChildren(); ++childIndex)
			{
				int nChild = pGroup->GetChildFragmentIndex() + childIndex;
				pThisBoundComposite->SetCurrentMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));
				pThisBoundComposite->SetLastMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));
			}
		}

		// turn off collision with roof and the rest of the world when it's shut
		if(pThisBoundComposite->GetTypeAndIncludeFlags())
			pThisBoundComposite->SetIncludeFlags(roofFragGroup, ArchetypeFlags::GTA_CORE_SHAPETEST_TYPES);
	}
}

////////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleConvertibleRoof::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Roof_Raised&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
	    "State_Roof_Raised",
		"State_Lower_Roof",
        "State_Roof_Lowered",
		"State_Raise_Roof",
		"State_Closing_Boot",
		"State_Roof_Stuck_Raised",
		"State_Roof_Stuck_Lowered",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif


///////////////////////////////////////////////////////////////////////////////////////

CTaskVehicleTransformToSubmarine::CTaskVehicleTransformToSubmarine( bool bInstantlyTransform ) :
m_bInstantlyTransform( bInstantlyTransform ),
m_bHasButtonBeenUp( true ),
m_bUseAlternateInput( false ),
m_fTransformRateForNextAnimation( -1.0f )
{
	SetInternalTaskType( CTaskTypes::TASK_VEHICLE_TRANSFORM_TO_SUBMARINE );
}

///////////////////////////////////////////////////////////////////////////////////////

CTaskVehicleTransformToSubmarine::eTransformStates  CTaskVehicleTransformToSubmarine::GetTransformState()
{
	switch( GetState() )
	{
	case State_Car:
		return STATE_CAR;
	case State_Transform_To_Submarine:
		return STATE_TRANSFORMING_TO_SUBMARINE;
	case State_Submarine:
		return STATE_SUBMARINE;
	case State_Transform_To_Car:
		return STATE_TRANSFORMING_TO_CAR;
	default:
		vehicleAssertf(0, "Invalid vehicle transform state");
		break;
	}

	//Default state
	return STATE_CAR;
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleTransformToSubmarine::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin

        FSM_State( State_Car )
            FSM_OnEnter
                Car_OnEnter( pVehicle );
            FSM_OnUpdate
                return Car_OnUpdate( pVehicle );

		FSM_State( State_Transform_To_Submarine )
			FSM_OnEnter
				TransformToSubmarine_OnEnter( pVehicle );
			FSM_OnUpdate
				return TransformToSubmarine_OnUpdate( pVehicle );

        FSM_State( State_Submarine )
            FSM_OnEnter
                Submarine_OnEnter( pVehicle );
            FSM_OnUpdate
                return Submarine_OnUpdate( pVehicle );

		FSM_State( State_Transform_To_Car )
			FSM_OnEnter
				TransformToCar_OnEnter(pVehicle);
			FSM_OnUpdate
				return TransformToCar_OnUpdate(pVehicle);

		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleTransformToSubmarine::Car_OnEnter( CVehicle* pVehicle )
{
	// TODO: DO WE NEED TO STOP ANY AUDIO
	//pVehicle->GetVehicleAudioEntity()->StopAllRoofLoops();
	
	// TODO: PROBABLY NEED TO TOGGLE COLLISION ON THE BITS THAT STICK OUT WHEN IN SUBMARINE MODE
	//pVehicle->ToggleRoofCollision(true);

	CSubmarineCar* pSubmarineCar = static_cast< CSubmarineCar* >( pVehicle );
	pSubmarineCar->SetTransformingComplete();

	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleTransformToSubmarine::Car_OnUpdate( CVehicle* pVehicle )
{
	CPed *pPed = pVehicle->GetDriver();

	if( pPed && pPed->IsLocalPlayer() )
	{
		CControl *pControl = pPed->GetControlFromPlayer();
			
		m_bHasButtonBeenUp |= pControl->GetVehicleRoof().IsUp();
		bool beginTransform = m_bHasButtonBeenUp && pControl->GetVehicleRoof().HistoryHeldDown( CTaskVehicleConvertibleRoof::ms_uTimeToHoldButtonDown );

		if( m_bUseAlternateInput )
		{
			beginTransform = pControl->GetVehicleTransform().IsReleased() && !pControl->GetVehicleTransform().IsReleasedAfterHistoryHeldDown( CTaskVehicleConvertibleRoof::ms_uTimeToHoldButtonDown );
		}

		if( beginTransform &&
			!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableSpycarTransformation) )
		{
			if( pVehicle->m_nVehicleFlags.bEngineOn )
			{
				SetState( State_Transform_To_Submarine );
			}
		}
	}


	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleTransformToSubmarine::TransformToSubmarine_OnEnter( CVehicle* pVehicle )
{
	float playbackRate = 1.5f;
	float playbackPhase = 0.0f;
	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	if (m_fTransformRateForNextAnimation > 0.0f)
	{
		playbackRate = m_fTransformRateForNextAnimation;
		m_fTransformRateForNextAnimation = -1.0f;
	}

	CSubmarineCar* pSubmarineCar = static_cast< CSubmarineCar* >( pVehicle );
	pSubmarineCar->SetTransformingToSubmarine();

	if(!move.IsMechanismActive())
	{
		strLocalIndex iSlot	= pVehicle->GetVehicleModelInfo()->GetClipDictionaryIndex();
		u32 iAnimHashKey	= pVehicle->GetVehicleModelInfo()->GetConvertibleRoofAnimNameHash();

		if( m_bInstantlyTransform )
		{
			m_bInstantlyTransform = false;
			playbackPhase = 1.0f;
		}

		move.StartMechanism( fwAnimManager::GetClipIfExistsByDictIndex( iSlot.Get(), iAnimHashKey ), 0.0f, playbackPhase, playbackRate );
	}
	else
	{
		move.OpenMechanism(playbackRate);

		if( m_bInstantlyTransform )
		{
			m_bInstantlyTransform = false;
			move.SetMechanismPhase( 1.0f );
		}
	}

	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
}

///////////////////////////////////////////////////////////////////////////////////////
static dev_float sfEnterSubmarineModeAnimationPhase = 0.35f;

CTask::FSM_Return CTaskVehicleTransformToSubmarine::TransformToSubmarine_OnUpdate( CVehicle* pVehicle )
{
	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	if( move.IsMechanismFullyOpen() )
	{
		CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );
		submarineCar->SetSubmarineMode( true, true );
		SetState( State_Submarine );
	}
	else 
	{
		if( move.GetMechanismPhase() > sfEnterSubmarineModeAnimationPhase )
		{
			// Set the vehicle to be in submarine mode
			CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );
			submarineCar->SetSubmarineMode( true, false );
		}
	}

	if( !pVehicle->GetIsVisibleInSomeViewportThisFrame() )
	{
		pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleTransformToSubmarine::Submarine_OnEnter( CVehicle* pVehicle )
{
	CSubmarineCar* pSubmarineCar = static_cast< CSubmarineCar* >( pVehicle );
	pSubmarineCar->SetTransformingComplete();

	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleTransformToSubmarine::Submarine_OnUpdate( CVehicle* pVehicle )
{
	CPed *pPed = pVehicle->GetDriver();

	if( pPed && 
	    pPed->IsLocalPlayer() )
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		m_bHasButtonBeenUp |= pControl->GetVehicleRoof().IsUp();

		bool beginTransform = m_bHasButtonBeenUp && pControl->GetVehicleRoof().HistoryHeldDown( CTaskVehicleConvertibleRoof::ms_uTimeToHoldButtonDown );

		if( m_bUseAlternateInput )
		{
			beginTransform = pControl->GetVehicleTransform().IsReleased() && !pControl->GetVehicleTransform().IsReleasedAfterHistoryHeldDown( CTaskVehicleConvertibleRoof::ms_uTimeToHoldButtonDown );
		}

		if( beginTransform && 
			!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableSpycarTransformation) )
		{ 
			if( pVehicle->m_nVehicleFlags.bEngineOn )
			{
				CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );
				submarineCar->SetSubmarineMode( true, false );

				SetState( State_Transform_To_Car );
			}
		}
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleTransformToSubmarine::TransformToCar_OnEnter( CVehicle* pVehicle )
{
	float playbackRate = -1.5f;
	float playbackPhase = 1.0f;
	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	if (m_fTransformRateForNextAnimation > 0.0f)
	{
		playbackRate = -m_fTransformRateForNextAnimation;
		m_fTransformRateForNextAnimation = -1.0f;
	}

	CSubmarineCar* pSubmarineCar = static_cast< CSubmarineCar* >( pVehicle );
	pSubmarineCar->SetTransformingToCar(m_bInstantlyTransform);

	if(!move.IsMechanismActive())
	{    
		strLocalIndex iSlot			= pVehicle->GetVehicleModelInfo()->GetClipDictionaryIndex();
		u32 iAnimHashKey	= pVehicle->GetVehicleModelInfo()->GetConvertibleRoofAnimNameHash();
		
		if( m_bInstantlyTransform )
		{
			m_bInstantlyTransform = false;
			playbackPhase = 0.0f;
		}

		move.StartMechanism( fwAnimManager::GetClipIfExistsByDictIndex( iSlot.Get(), iAnimHashKey ), 0.0f, playbackPhase, playbackRate );
	}
	else
	{
		move.CloseMechanism(playbackRate);

		if( m_bInstantlyTransform )
		{
			m_bInstantlyTransform = false;
			move.SetMechanismPhase( 0.0f );
		}
	}

	//Note that the button hasn't been up.
	m_bHasButtonBeenUp = false;
}

///////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleTransformToSubmarine::TransformToCar_OnUpdate( CVehicle* pVehicle )
{
	CMoveVehicle& move = pVehicle->GetMoveVehicle();

	if( move.IsMechanismFullyClosed() )
	{
		CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );
		submarineCar->SetSubmarineMode( false, true );
		SetState( State_Car );
	}
	else 
	{
		CMoveVehicle& move = pVehicle->GetMoveVehicle();
		static float sfTransformToCarPhaseMin = 0.9f;

		if( move.GetMechanismPhase() < sfTransformToCarPhaseMin )
		{
			// Set back to car mode as soon as we start transforming so that the suspension works correctly
			CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pVehicle );
			submarineCar->SetSubmarineMode( false, false );
		}
	}

	if( !pVehicle->GetIsVisibleInSomeViewportThisFrame() )
	{
		pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////


#if !__FINAL
const char * CTaskVehicleTransformToSubmarine::GetStaticStateName( s32 iState  )
{
	Assert( iState >= State_Car && iState <= State_Finish );
	static const char* aStateNames[] = 
	{
		"State_Car",
		"State_Transform_To_Submarine",
		"State_Submarine",
		"State_Transform_To_Car",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif

