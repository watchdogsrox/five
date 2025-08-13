/////////////////////////////////////////////////////////////////////////////////
// Title	:	TaskCombatMelee.cpp
//
// These classes enables individual peds to engage in melee combat
// with other peds.
//
// Note: The major parts of our "Melee Combat AI" are marked with the quoted
// term in this sentence.
///////////////////////////////////////////////////////////////////////////////// 
#include "task/combat/taskcombatmelee.h"

//Rage headers
#include "math/vecmath.h"
#include "math/angmath.h"
#include "vector/vector3.h"
#include "phbound/boundcomposite.h"
#include "phbound/boundcapsule.h"
#include "phbound/support.h"
#include "fragment/instance.h"
#include "fragment/typechild.h"
#include "string/string.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "vfx/channel.h"


//Game headers.
#include "AI/Ambient/ConditionalAnimManager.h"
#include "animation/AnimDefines.h"
#include "animation/animManager.h"
#include "animation/FacialData.h"
#include "animation/MovePed.h"
#include "audio/dooraudioentity.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "control/gamelogic.h"
#include "debug/debugscene.h"
#include "event/eventdamage.h"
#include "event/eventweapon.h"
#include "Event/EventReactions.h"
#include "Event/EventShocking.h"
#include "event/shockingevents.h"
#include "event/Events.h"
#include "frontend/NewHud.h"
#include "Ik/solvers/RootSlopeFixupSolver.h"
#include "modelinfo/pedmodelinfo.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/events/NetworkEventTypes.h"
#include "objects/Door.h"
#include "objects/object.h"
#include "peds/DefensiveArea.h"
#include "peds/ped.h"
#include "Peds/Action/BrawlingStyle.h"
#include "peds/pedgeometryanalyser.h"
#include "peds/PedIKSettings.h"
#include "peds/pedintelligence.h"
#include "peds/PedMoveBlend/pedmoveblend.h"
#include "peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/PlayerInfo.h"
#include "peds/PlayerSpecialAbility.h"
#include "Peds/HealthConfig.h"
#include "physics/gtaarchetype.h"
#include "physics/gtainst.h"
#include "physics/physics.h"
#include "physics/Tunable.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/PickupManager.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Stats/StatsInterface.h"
#include "Streaming/streaming.h"
#include "system/TamperActions.h"
#include "Task/Combat/CombatMeleeDebug.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/General/TaskBasic.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskMotionStrafing.h"
#include "Task/Motion/Locomotion/TaskQuadLocomotion.h"
#include "Task/Movement/taskseekentity.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#include "Task/Weapons/TaskPlayerWeapon.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Combat/TaskSharkAttack.h"
#include "Task/Animation/TaskAnims.h"
#include "vehicles/vehicle.h"
#include "vfx/systems/VfxPed.h"
#include "weapons/WeaponDamage.h"
#include "weapons/WeaponManager.h"

#if __BANK
#include "Peds/PedDebugVisualiser.h"
#endif // __BANK

#if __BANK
	#define MELEE_NETWORK_DEBUGF(fmt,...)		if(CCombatMeleeDebug::sm_bMeleeDisplayDebugEnabled) { Displayf(fmt,##__VA_ARGS__); }
#else
	#define MELEE_NETWORK_DEBUGF(fmt,...)
#endif

AI_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////
// CTaskMelee
//
// Enables individual peds to perform series of action moves in a
// smooth and convincing manner.
/////////////////////////////////////////////////////////////////////////////////
s32		CTaskMelee::sm_nMaxNumActiveCombatants								= 1;
s32		CTaskMelee::sm_nMaxNumActiveSupportCombatants						= 3;
s32		CTaskMelee::sm_nTimeInTaskAfterHitReaction							= 5000;
s32		CTaskMelee::sm_nTimeInTaskAfterStardardAttack						= 2500;
s32		CTaskMelee::ms_nCameraInterpolationDuration							= 750;

const CActionDefinition *CTaskMelee::ms_LastFoundActionDefinition			= NULL;
const CActionDefinition *CTaskMelee::ms_LastFoundActionDefinitionForNetworkDamage			= NULL;

// Tuning Constants.
float	CTaskMelee::sm_fMeleeStrafingMinDistance							= 1.15f;
float	CTaskMelee::sm_fMovingTargetStrafeTriggerRadius						= 2.5f;
float	CTaskMelee::sm_fMovingTargetStrafeBreakoutRadius					= 4.0f;
float	CTaskMelee::sm_fForcedStrafeRadius									= 4.0f;
float	CTaskMelee::sm_fBlockTimerDelayEnter								= 0.0f;
float	CTaskMelee::sm_fBlockTimerDelayExit									= 0.15f;
float	CTaskMelee::sm_fFrontTargetDotThreshold								= 0.35f;
bool	CTaskMelee::sm_bDoSpineIk											= false;
bool	CTaskMelee::sm_bDoHeadIk											= true;
bool	CTaskMelee::sm_bDoEyeIK												= true;
bool	CTaskMelee::sm_bDoLegIK												= false;
float	CTaskMelee::sm_fHeadLookMinYawDegs									= -125.0f;
float	CTaskMelee::sm_fHeadLookMaxYawDegs									= 125.0f;
float	CTaskMelee::sm_fSpineLookMinYawDegs									= -30.0f;
float	CTaskMelee::sm_fSpineLookMaxYawDegs									= 30.0f;
float	CTaskMelee::sm_fSpineLookMinPitchDegs								= -30.0f;
float	CTaskMelee::sm_fSpineLookMaxPitchDegs								= 30.0f;
float	CTaskMelee::sm_fSpineLookTorsoOffsetYawDegs							= 0.0f;
float	CTaskMelee::sm_fSpineLookMaxHeightDiff								= 0.65f;
float	CTaskMelee::sm_fSpineLookAmountPowerCurve							= 0.25f;
float	CTaskMelee::sm_fPlayerAttemptEscapeExitMultiplyer					= 4.0f;
float	CTaskMelee::sm_fPlayerMoveAwayExitMultiplyer						= 3.0f;
float	CTaskMelee::sm_fPlayerClipPlaybackSpeedMultiplier					= 1.0f;
s32		CTaskMelee::sm_nPlayerTauntFrequencyMinInMs							= 3000;
s32		CTaskMelee::sm_nPlayerTauntFrequencyMaxInMs							= 5000;
u32		CTaskMelee::sm_nPlayerImpulseInterruptDelayInMs						= 850;
s32		CTaskMelee::sm_nAiTauntHitDelayTimerInMs							= 12500;
float	CTaskMelee::sm_fAiBlockConsecutiveHitBuffIncrease					= 0.075f;
float	CTaskMelee::sm_fAiBlockActionBuffDecrease							= 0.035f;
s32		CTaskMelee::sm_fAiAttackConsecutiveHitBuffIncreaseMin				= 250;
s32		CTaskMelee::sm_fAiAttackConsecutiveHitBuffIncreaseMax				= 700;
s32		CTaskMelee::sm_nAiForceMeleeMovementAtStartTimeMs					= 1500;
float	CTaskMelee::sm_fAiTargetSeparationDesiredSelfToTargetRange			= 1.0f;
float	CTaskMelee::sm_fAiTargetSeparationDesiredSelfToGetupTargetRange		= 1.5f;
float	CTaskMelee::sm_fAiTargetSeparationPrefdDiffMinForImpetus			= 0.5f;
float	CTaskMelee::sm_fAiTargetSeparationPrefdDiffMaxForImpetus			= 1.50f;
float	CTaskMelee::sm_fAiTargetSeparationForwardImpetusPowerFactor			= 2.00f;
float	CTaskMelee::sm_fAiTargetSeparationBackwardImpetusPowerFactor		= 1.00f;
float	CTaskMelee::sm_fAiTargetSeparationMinTargetApproachDesireSize		= 0.1f;
s32		CTaskMelee::sm_nAiAvoidEntityDelayMinInMs							= 1000;
s32		CTaskMelee::sm_nAiAvoidEntityDelayMaxInMs							= 4000;
float	CTaskMelee::sm_fAiAvoidSeparationDesiredSelfToTargetRange			= 1.6f;
float	CTaskMelee::sm_fAiAvoidSeparationPrefdDiffMinForImpetus				= 1.25f;
float	CTaskMelee::sm_fAiAvoidSeparationPrefdDiffMaxForImpetus				= 3.00f;
float	CTaskMelee::sm_fAiAvoidSeparationForwardImpetusPowerFactor			= 2.00f;
float	CTaskMelee::sm_fAiAvoidSeparationBackwardImpetusPowerFactor			= 1.00f;
float	CTaskMelee::sm_fAiAvoidSeparationMinTargetApproachDesireSize		= 0.1f;
float	CTaskMelee::sm_fAiAvoidSeparationCirlclingStrength					= 0.25;
s32		CTaskMelee::sm_nAiSeekModeScanTimeMin								= 500;
s32		CTaskMelee::sm_nAiSeekModeScanTimeMax								= 3000;
float	CTaskMelee::sm_fAiSeekModeTargetHeight								= 2.5f;
float	CTaskMelee::sm_fAiSeekModeInitialPursuitBreakoutDist				= 15.0f;
float	CTaskMelee::sm_fAiSeekModeChaseTriggerOffset						= 4.0f;
float	CTaskMelee::sm_fAiSeekModeFleeTriggerOffset							= 2.5f;
float	CTaskMelee::sm_fAiSeekModeActiveCombatantRadius						= 0.25f;
float	CTaskMelee::sm_fAiSeekModeSupportCombatantRadiusMin					= 3.0f;
float	CTaskMelee::sm_fAiSeekModeSupportCombatantRadiusMax					= 5.0f;
float	CTaskMelee::sm_fAiSeekModeInactiveCombatantRadiusMin				= 10.0f;
float	CTaskMelee::sm_fAiSeekModeInactiveCombatantRadiusMax				= 15.0f;
float	CTaskMelee::sm_fAiSeekModeRetreatDistanceMin						= 3.0f;
float	CTaskMelee::sm_fAiSeekModeRetreatDistanceMax						= 5.0f;
float	CTaskMelee::sm_fAiQueuePrefdPosDiffMinForImpetus					= 3.0f;
float	CTaskMelee::sm_fAiQueuePrefdPosDiffMaxForImpetus					= 5.0f;
s32		CTaskMelee::sm_nAiLosTestFreqInMs									= 2000;
float	CTaskMelee::sm_fLookAtTargetHeadingBlendoutDegrees					= 10.0f;
float	CTaskMelee::sm_fLookAtTargetTimeout									= 3.0f;
float	CTaskMelee::sm_fLookAtTargetLooseToleranceDegrees					= 5.0f;
float	CTaskMelee::sm_fReTargetTimeIfNotStrafing							= 0.5f;

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldCheckForMeleeAmbientMove
// PURPOSE :	Determines if the player input conditions are appropriate to check for a
//				melee ambient move.
// PARAMETERS :	pPed - the ped of interest.
//				bAttemptTaunt - Whether or not the given ped is pressing the special ability button
// RETURNS :	Whether or not we should check for a melee action
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::ShouldCheckForMeleeAmbientMove( CPed* pPed, bool& bAttemptTaunt, bool bAllowHeavyAttack )
{
	if( pPed->IsLocalPlayer() && CNewHud::IsShowingHUDMenu() )
		return false;

	Assert( pPed );
	if( !pPed )
		return false;

	if( pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableAmbientMeleeMoves) )
		return false;

	//! Can't set reset flag for mobile phone as it means we have nowhere to reset it (as script can set it too). Just check explicitly here.
	if( CTaskMobilePhone::IsRunningMobilePhoneTask(*pPed) )
		return false;

	const CControl* pControl = pPed->GetControlFromPlayer();
	Assert( pControl );
	if( !pControl )
		return false;

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning))
	{		
		if (pControl->GetMeleeAttackAlternate().IsPressed() || pControl->GetMeleeAttackLight().IsPressed())
		{
			return true;
		}
		return false;
	}

	// Only even check for moves from on foot if the ambient attack button just released.
	bool bShouldCheckForMoveFromOnFoot = false;
	if( pControl->GetPedTargetIsDown() )
	{
		bool bLight = pControl->GetMeleeAttackLight().IsPressed() || pControl->GetMeleeAttackAlternate().IsPressed();
		bool bHeavy = false;

		if( bAllowHeavyAttack )
			bHeavy = pControl->GetMeleeAttackHeavy().IsPressed();

		// Only allow manual taunts when the insult special ability is equipped
		if( pControl->GetPedSpecialAbility().IsPressed() )
		{
			const CPlayerSpecialAbility* pSpecialAbility = pPed->GetSpecialAbility();
			bAttemptTaunt =	!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventAllMeleeTaunts ) && pSpecialAbility && pSpecialAbility->GetType() == SAT_RAGE && !pSpecialAbility->IsActive();
		}

		bShouldCheckForMoveFromOnFoot =	bLight || bHeavy;
	}
	else
		bShouldCheckForMoveFromOnFoot = pControl->GetMeleeAttackLight().IsPressed() || pControl->GetMeleeAttackAlternate().IsPressed();

	return bShouldCheckForMoveFromOnFoot;
}

CActionFlags::ActionTypeBitSetData CTaskMelee::GetActionTypeToLookFor(const CPed* pPed)
{
	CActionFlags::ActionTypeBitSetData typeToLookFor;
	if( pPed->IsAPlayerPed() )
	{
		if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) || pPed->GetIsFPSSwimming())
			typeToLookFor.Set( CActionFlags::AT_SWIMMING, true );
		else
			typeToLookFor.Set( CActionFlags::AT_FROM_ON_FOOT, true );
	}
	else
		typeToLookFor.Set( CActionFlags::AT_FROM_ON_FOOT, true );

	return typeToLookFor;
}

const CActionDefinition *CTaskMelee::FindSuitableActionForTarget(const CPed* pPed, const CEntity* pTargetEntity, bool bHasLockOnTarget, bool bCacheActionDefinition)
{
	CActionFlags::ActionTypeBitSetData typeToLookFor = GetActionTypeToLookFor(pPed);
	return FindSuitableActionForTargetType(pPed, pTargetEntity, bHasLockOnTarget, typeToLookFor, bCacheActionDefinition);
}

const CActionDefinition *CTaskMelee::FindSuitableActionForTargetType(const CPed* pPed, const CEntity* pTargetEntity, bool bHasLockOnTarget, const CActionFlags::ActionTypeBitSetData& typeToLookFor, bool bCacheActionDefinition)
{
	// Attempt to find a suitable action
	const CActionDefinition* pActionToDo =  ACTIONMGR.SelectSuitableAction(
		typeToLookFor,								// Only look for moves that are appropriate from on foot
		NULL,										// No impulse combo
		true,										// Test whether or not it is an entry action.
		true,										// Only find entry actions.
		pPed,										// The acting ped.
		0,											// result Id forcing us.
		-1,											// Priority
		pTargetEntity,								// The entity being attacked.
		bHasLockOnTarget,							// Do we have a lock on the target entity.
		false,										// Should we only allow impulse initiated moves.
		pPed->IsLocalPlayer(),						// Test the controls (for local players only).
		CActionManager::ShouldDebugPed( pPed ));

	if(bCacheActionDefinition)
	{
		ms_LastFoundActionDefinition = pActionToDo;
	}

	return pActionToDo;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CheckForAndGetMeleeAmbientMove
// PURPOSE :	Checks for any ambient melee moves the ped can use.
//
//				We use the action manager to test the current conditions (some of
//				which may use weapons or a complex input state such as a button
//				being released and not just pressed down) to check to see if there
//				is a request to do a pistol whip, a running attack, or a surprise
//				attack (like a sucker punch or garot move), etc.
// PARAMETERS :	pPed - the ped of interest.
//				pTargetEntity - Target entity for ped of interest (could be NULL)
//				bHasLockOnTarget - Do we have a physical lockon to the target entity?
//				bGoIntoMeleeEvenWhenNoSpecificMoveFound - Create CTaskMelee even if move isn't found
//				bPerformMeleeIntro - Whether or not to perform the melee intro animation transition
//				bAllowStrafeMode - Flag that will allow the ped to enter strafe mode when locked on
//				bCheckLastFound - Checks to see if an action definition was stored previously
// RETURNS :	A task doing that move if appropriate or NULL if none found.
/////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskMelee::CheckForAndGetMeleeAmbientMove( const CPed* pPed, CEntity* pTargetEntity, bool bHasLockOnTarget, bool bGoIntoMeleeEvenWhenNoSpecificMoveFound, bool bPerformMeleeIntro, bool bAllowStrafeMode, bool bCheckLastFound )
{
	taskAssert( pPed );
	CActionFlags::ActionTypeBitSetData typeToLookFor = GetActionTypeToLookFor(pPed);
	if( !typeToLookFor.IsSet( CActionFlags::AT_SWIMMING ) && !pPed->GetCurrentMotionTask()->IsOnFoot() )
		return NULL;	

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
	{
		return NULL;
	}

	aiTask* pTaskToDo = NULL;
	const CActionDefinition* pActionToDo = NULL;
	if( bCheckLastFound && ms_LastFoundActionDefinition )
	{
		pActionToDo = ms_LastFoundActionDefinition;
	}
	else
	{
		pActionToDo = FindSuitableActionForTargetType( pPed, pTargetEntity, bHasLockOnTarget, typeToLookFor );
	}

	// Did we find a suitable action to execute?
	if( pActionToDo )
	{
		// Try to get a pointer to the sequence via the sequence Id for the action.
		const CActionResult* pActionResult = pActionToDo->GetActionResult( pPed );
		if( pActionResult )
		{
			s32 iMeleeFlags = 0;
			if( bHasLockOnTarget )
				iMeleeFlags |= MF_HasLockOnTarget;

			// Add a new action (with the movement layer still running underneath)
			CSimpleImpulseTest::Impulse initialImpulse = CSimpleImpulseTest::ImpulseNone;

			// Record the initial impulse
			if( pPed->IsLocalPlayer() )
			{
				if( pActionToDo->GetImpulseTest() && pActionToDo->GetImpulseTest()->GetNumImpulses() > 0 )
					initialImpulse = pActionToDo->GetImpulseTest()->GetImpulseByIndex( 0 )->GetImpulse();
			}

			pTaskToDo = rage_new CTaskMelee( pActionResult, pTargetEntity, iMeleeFlags, initialImpulse );
		}
	}
	
	// Check if we should go into melee combat mode (in a melee idle stance).
	if( !pTaskToDo && pTargetEntity && bGoIntoMeleeEvenWhenNoSpecificMoveFound )
	{
		// Get the weapon; note that pWeapon may be null if there is not a weapon in hand.
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if( !pWeaponInfo || pWeaponInfo->GetIsMelee() )
		{
			// Add a new action.
			s32 iMeleeFlags = MF_ShouldBeInMeleeMode;
			if( bHasLockOnTarget )
				iMeleeFlags |= MF_HasLockOnTarget;

			if( bAllowStrafeMode )
			{
				iMeleeFlags |= MF_AllowStrafeMode;

				// Only allow the melee intro when we allow strafing
				if( bPerformMeleeIntro )
					iMeleeFlags |= MF_PerformIntroAnim;
			}
			
			pTaskToDo = rage_new CTaskMelee( NULL, pTargetEntity, iMeleeFlags );
		}
	}

	ms_LastFoundActionDefinition = NULL;

	return pTaskToDo;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CTaskMelee
// PURPOSE :	The initial action provided constructor.
//				Note: the initial action may be NULL.
// PARAMETERS :	pInitialActionResult - The action to immediately perform (may be null).
//				pTargetEntity - The melee target.
//				iFlags - Task flags like has lock on target, is doing a reaction etc
//				initialImpulse - The first impulse from an on foot attack
/////////////////////////////////////////////////////////////////////////////////
CTaskMelee::CTaskMelee( const CActionResult* pInitialActionResult, CEntity* pTargetEntity, fwFlags32 iFlags, CSimpleImpulseTest::Impulse initialImpulse, s32 nTimeInTask, u16 uUniqueNetworkID )
: // Initializer list.
m_vStealthKillWorldOffset			(VEC3_ZERO),
m_fStealthKillWorldHeading			(0.0f),
m_pNextActionResult					(pInitialActionResult),
m_bActionIsBranch					(false),
m_pTargetEntity						(pTargetEntity),
m_nCameraAnimId						(0),
m_pAvoidEntity						(NULL),
m_bStopAll							(false),
m_queueType							(QT_Invalid),
m_cloneQueueType					(QT_Invalid),
m_bLocalReaction					(false),
m_bSeekModeLosTestHasBeenRequested	(false),
m_bLosBlocked						(false),
m_impulseCombo						(initialImpulse),
m_nFlags							(iFlags),
m_fBlockDelayTimer					(0.0f),
m_fBlockBuff						(0.0f),
m_bIsBlocking						(false),
m_bUsingStealthClipClose			(false),
m_bUsingBlockClips					(false),
m_fDesiredTargetDist				(0.0f),
m_fAngleSpread						(0.0f),
m_bCanPerformCombo					(false),
m_bCanPerformDazedReaction			(false),
m_bCanBlock							(false),
m_bCanCounter						(false),
m_bLastActionWasARecoil				(false),
m_bEndsInAimingPose					(false),
#if FPS_MODE_SUPPORTED
m_bInterruptForFirstPersonCamera	(false),
#endif
m_nActionResultNetworkID			(INVALID_ACTIONRESULT_ID),
m_nUniqueNetworkMeleeActionID		(uUniqueNetworkID)
{
	SetInternalTaskType( CTaskTypes::TASK_MELEE );

	if( nTimeInTask > 0 )
		m_forceStrafeTimer.Set( fwTimer::GetTimeInMilliseconds(), (u32)nTimeInTask );
	else if( nTimeInTask < 0 )
		SetTaskFlag( MF_ForceStrafeIndefinitely );

	// Set initial lock-on
	m_bHasLockOnTargetEntity = GetIsTaskFlagSet( MF_HasLockOnTarget );

	AI_LOG_WITH_ARGS("CTaskMelee::CTaskMelee() - m_pTargetEntity = %s, task = %p \n", m_pTargetEntity ? m_pTargetEntity->GetLogName() : "Null", this);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Clone
// PURPOSE :	The cloning operation- used by the task management system.
// RETURNS :	A perfect copy of the task.
/////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskMelee::Copy() const
{
	return rage_new CTaskMelee( m_pNextActionResult, GetTargetEntity(), m_nFlags, m_impulseCombo.GetImpulseAtIndex(0), m_forceStrafeTimer.GetDuration(), m_nUniqueNetworkMeleeActionID );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldAbort
// PURPOSE :	To receive an abort request and handle it.
// PARAMETERS :	pPed - The ped of interest.
//				iPriority - How important is it to abort.
//				pEvent - What event is causing the abort (may be null).
// RETURNS :	Whether or not the task has aborted.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent )
{
	taskAssertf( GetPed(), "pPed is null in CTaskMelee::ShouldAbort." );
	DEV_BREAK_IF_FOCUS( CCombatMeleeDebug::sm_bFocusPedBreakOnTaskCombatMeleeShouldAbort, GetPed() );

	// Ignore certain events while in melee (or doing a melee hit reaction).
	if( CTask::ABORT_PRIORITY_IMMEDIATE != iPriority && pEvent )
	{
		const int nEventType = ((CEvent*)pEvent)->GetEventType();
		if( nEventType == EVENT_MELEE_ACTION )
		{
			// Always ignore melee events.
			return false;
		}
		else if( nEventType == EVENT_GUN_AIMED_AT )
		{
			// Note: If we are a player we should never get this message.
			taskAssert( !GetPed()->IsPlayer() );

			// Ignore being aimed at when we are currently doing a move, otherwise
			// listen to the event and bail out.
			// Note: If we are already in combat the CTackComplexCombat should handle
			// and filter these events for us if the event is not a physical response.
			if( IsCurrentlyDoingAMove( false, false, true ) )
				return false;
		}
		else if( nEventType == EVENT_DAMAGE )
			return CTask::ShouldAbort( iPriority, pEvent );
		// Want to completely ignore these events when fighting.
		else if( (((CEvent*)pEvent)->GetEventPriority() <= E_PRIORITY_POTENTIAL_WALK_INTO_FIRE || ((CEvent*)pEvent)->GetEventPriority() == E_PRIORITY_PED_COLLISION_WITH_PED ) )
		{
			return false;
		}
		else if (nEventType == EVENT_SCRIPT_COMMAND && GetPed()->IsDeadByMelee())
		{
#if __ASSERT
			const CEventScriptCommand* pEventScriptCommand = SafeCast(const CEventScriptCommand, pEvent);
			const aiTask* pTask = pEventScriptCommand->GetTask();
			const char* pszTaskName = pTask ? pTask->GetTaskName() : "NoTask";
			taskAssertf(false, "CTaskMelee::ShouldAbort: EVENT_SCRIPT_COMMAND (%s, %s:%d) is trying to abort a melee task, but the ped (%s) is already DeadByMelee. Rejecting.", 
				pszTaskName, 
				pEventScriptCommand->GetScriptName(),
				pEventScriptCommand->GetScriptProgramCounter(),
				AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __ASSERT
			return false;
		}
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetOverriddenGameplayCameraSettings
// PURPOSE :	Allow this task to override the active gameplay camera.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::GetOverriddenGameplayCameraSettings( tGameplayCameraSettings& settings ) const
{
	//Request an aim camera when the player ped is locked-on with a melee weapon.

	const CPed *pPed = GetPed(); //Get the ped ptr.
	if( !pPed->IsLocalPlayer() )
		return;

	//Inhibit aim cameras during long and medium Switch transitions.
	if( pPed->GetPlayerResetFlag( CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA ) ||
		( g_PlayerSwitch.IsActive() && ( g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT ) ) )
	{
		return;
	}

	const CWeaponInfo* pWeaponInfo = camGameplayDirector::ComputeWeaponInfoForPed( *pPed );
	if( taskVerifyf( pWeaponInfo, "Could not obtain a weapon info object for the ped's equipped weapon" ) )
	{
		// Special case first person scope hit reactions
		const CTask* pSubTask = GetSubTask();

		const bool bWeaponHasFirstPersonScope = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
		if( bWeaponHasFirstPersonScope && pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT )
		{
			const CTaskMeleeActionResult* pActionResultTask = static_cast<const CTaskMeleeActionResult*>( pSubTask );
			if( pActionResultTask->IsDoingReaction() )
			{
				settings.m_CameraHash = pWeaponInfo->GetRunAndGunCameraHash();
				settings.m_InterpolationDuration = ms_nCameraInterpolationDuration;
				return;
			}
		}

		bool bIsMeleeWeapon = pWeaponInfo->GetIsMelee();

		//NOTE: We render an aim camera when aim input is provided with a non-melee weapon to improve continuity with normal aiming.
		if( ( bIsMeleeWeapon && pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget() ) ||
			( !bIsMeleeWeapon && CPlayerInfo::IsAiming( false ) ) )
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			settings.m_CameraHash = pWeapon ? pWeapon->GetDefaultCameraHash() : pWeaponInfo->GetDefaultCameraHash(); //Fall-back to the default aim camera.
			settings.m_InterpolationDuration = ms_nCameraInterpolationDuration;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CleanUp
// PURPOSE :	Called when the task quits itself or is aborted, should cleanup
//				any clips, assets etc...
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::CleanUp( void )
{
	// Make sure any streaming melee clips are released.
	ReleaseStreamingClipRequests();

	// Clean up any LOS requests
	CancelPendingLosRequest();

	// Let the player on foot state know when the last time we finished a melee task
	CPed* pPed = GetPed();
	if( pPed )
	{
		bool bInFpsMode = false;
#if FPS_MODE_SUPPORTED
		bInFpsMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED

		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableActionMode, false );

// 		aiTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveMotionByType( CTaskTypes::TASK_HUMAN_LOCOMOTION );
// 		if( !pTask )
		{
			bool bWantsToActionMode = !GetIsTaskFlagSet( MF_EndsInIdlePose );
			
			// Ensure action mode enabled
			if(bWantsToActionMode)
			{
				pPed->SetUsingActionMode(true, CPed::AME_Melee, CTaskMeleeActionResult::sm_Tunables.m_ActionModeTime, false);
				pPed->UpdateMovementMode();
			}

			if( GetIsTaskFlagSet( MF_TagSyncBlendOut ) && VelocityHighEnoughForTagSyncBlendOut( pPed ) )
			{
				float fDesiredMBR = pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag();
				if( CPedMotionData::GetIsLessThanRun( fDesiredMBR ) )
					pPed->ForceMotionStateThisFrame(bInFpsMode ? CPedMotionStates::MotionState_Aiming : (pPed->GetMotionData()->GetUsingStealth() ? CPedMotionStates::MotionState_Stealth_Walk : (bWantsToActionMode ? CPedMotionStates::MotionState_ActionMode_Walk : CPedMotionStates::MotionState_Walk)));
				else
					pPed->ForceMotionStateThisFrame(bInFpsMode ? CPedMotionStates::MotionState_Aiming : (pPed->GetMotionData()->GetUsingStealth() ? CPedMotionStates::MotionState_Stealth_Run : (bWantsToActionMode ? CPedMotionStates::MotionState_ActionMode_Run : CPedMotionStates::MotionState_Run)));
			}
			// Send them back to idle to eliminate any movement anims left over from before the CTaskMelee
 			else if( GetIsTaskFlagSet( MF_QuitTaskAfterNextAction ) )
 			{
				bool bRunningSwimAimTasks = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming) && pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType()==CTaskTypes::TASK_MOTION_AIMING;
 				// Count the animal swimming tasks for players.
				if (pPed->IsAPlayerPed() && pPed->GetPedType() == PEDTYPE_ANIMAL)
				{
					CTask* pMotionTaskActiveSimplest = pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest();
					if (pMotionTaskActiveSimplest && pMotionTaskActiveSimplest->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH)
					{
						bRunningSwimAimTasks = true;
					}
				}
				if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) && !bRunningSwimAimTasks)
 					pPed->ForceMotionStateThisFrame(bInFpsMode ? CPedMotionStates::MotionState_Aiming : (pPed->GetMotionData()->GetUsingStealth() ? CPedMotionStates::MotionState_Stealth_Idle : (bWantsToActionMode ? CPedMotionStates::MotionState_ActionMode_Idle : CPedMotionStates::MotionState_Idle)));	
 			}
 			else if( GetIsTaskFlagSet( MF_AttemptMeleeControllerInterrupt ) )
			{
				// Based on the velocity force the associated motion frame
				float fVelocity = pPed->GetVelocity().Mag2();
				if( fVelocity < rage::square( CActionManager::ms_fMBRWalkBoundary ) )
					pPed->ForceMotionStateThisFrame(bInFpsMode ? CPedMotionStates::MotionState_Aiming : (pPed->GetMotionData()->GetUsingStealth() ? CPedMotionStates::MotionState_Stealth_Idle : (bWantsToActionMode ? CPedMotionStates::MotionState_ActionMode_Idle : CPedMotionStates::MotionState_Idle)), true);
				else
					pPed->ForceMotionStateThisFrame(bInFpsMode ? CPedMotionStates::MotionState_Aiming : (pPed->GetMotionData()->GetUsingStealth() ? CPedMotionStates::MotionState_Stealth_Walk : (bWantsToActionMode ? CPedMotionStates::MotionState_ActionMode_Walk : CPedMotionStates::MotionState_Walk)));
			}
		}

		if( pPed->IsPlayer() )
		{
			if( CPlayerInfo::IsAiming() && !bInFpsMode)
				pPed->SetPedResetFlag( CPED_RESET_FLAG_InstantBlendToAim, true );

			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_PLAYER_ON_FOOT );
			if( pTask )
			{
				CTaskPlayerOnFoot* pPlayerOnFoot = static_cast<CTaskPlayerOnFoot*>( pTask );
				pPlayerOnFoot->SetLastTimeInMeleeTask( fwTimer::GetTimeInMilliseconds() );
				pPlayerOnFoot->SetWasMeleeStrafing( GetIsTaskFlagSet( MF_AllowStrafeMode ) );
			}
		}
		else if( m_bEndsInAimingPose )
		{	
			pPed->SetPedResetFlag( CPED_RESET_FLAG_InstantBlendToAim, true );
		}

		pPed->m_PedConfigFlags.SetPedLegIkMode( CIkManager::GetDefaultLegIkMode(pPed) );
		pPed->GetMotionData()->SetGaitReductionBlockTime(); //block gait reduction while we ramp back up
	}

	UnRegisterMeleeOpponent();

	// Clean up the route search helper if it is still around
	if( m_RouteSearchHelper.IsSearchActive() )
		m_RouteSearchHelper.ResetSearch();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CancelPendingLosRequest
// PURPOSE :	To clear out any pathserver requests- used when shutting down the
//				task.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::CancelPendingLosRequest( void )
{
	m_seekModeLosTestHandle1.Reset();
	m_seekModeLosTestHandle2.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	StopAll
// PURPOSE :	Halt this task and its subtasks.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::StopAll( void )
{
	taskAssertf( GetSubTask(), "GetSubTask() was null in CTaskMelee::StopAll." );
	if( GetSubTask()->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT )
		m_bStopAll = true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IsCurrentlyDoingAMove
// PURPOSE :	Determine if a melee move is currently in progress.
// RETURNS :	Whether or not a  melee move is currently in progress.
///////////////////////////////////////////////////////////////////////////////// 
bool CTaskMelee::IsCurrentlyDoingAMove( bool bCountTauntAsAMove, bool bCountIntrosAsMove, bool bAllowInterrupt ) const
{
	const CTask* pSubTask = GetSubTask();
	if( !pSubTask || pSubTask->GetTaskType() != CTaskTypes::TASK_MELEE_ACTION_RESULT )
		return false;

	const CTaskMeleeActionResult* pActionResultTask = static_cast<const CTaskMeleeActionResult*>( pSubTask );
	if( !bCountTauntAsAMove && pActionResultTask->IsDoingTaunt() )
		return false;

	if( !bCountIntrosAsMove && pActionResultTask->IsDoingIntro() )
		return false;

	if( bAllowInterrupt && pActionResultTask->ShouldAllowInterrupt() )
		return false;

	return !pActionResultTask->GetIsFlagSet( aiTaskFlags::HasFinished ) &&
		   !pActionResultTask->GetIsFlagSet( aiTaskFlags::IsAborted ) &&
		    pActionResultTask->IsDoingSomething();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CheckCloneForLocalBranch
// PURPOSE :	Check if there were any local action branches we should take (i.e. dodge reactions).
// PARAMETERS :	pPed - The clone ped of interest.
// RETURNS :	If a valid branch was found.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::CheckCloneForLocalBranch( CPed* pPed )
{
	CTask* pSubTask = GetSubTask();
	if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT )
	{
		CTaskMeleeActionResult* pActionResultTask = static_cast<CTaskMeleeActionResult*>(pSubTask);
		
		//! Note: Deliberately only search for block branches atm.
		if( IsBlocking() )
		{
			CActionFlags::ActionTypeBitSetData typeToLookFor;
			typeToLookFor.Set( CActionFlags::AT_BLOCK, true );
			u32	nBranchActionId		= 0;
			if( pActionResultTask->ShouldBranchOnCurrentFrame(pPed, 
				&m_impulseCombo, 
				&nBranchActionId,	
				false,				//impulse initiated moves only
				typeToLookFor,
				0))					//nForcingResultId
			{
				u32 nActionDefnitionIdx = 0;
				const CActionDefinition* pBranchesActionDefinition = ACTIONMGR.FindActionDefinition( nActionDefnitionIdx, nBranchActionId );
				taskAssert( pBranchesActionDefinition );

				// Get the pointer to the actionResult from the action Id.
				const CActionResult* pBranchesActionResult = pBranchesActionDefinition->GetActionResult( pPed );

				// Go to local branch?
				if( pBranchesActionResult )
				{
					m_pNextActionResult = pBranchesActionResult;
					ClearTaskFlag( MF_IsDoingReaction );
					SetRunningLocally( true );
					return true;
				}
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CheckForBranch
// PURPOSE :	Check if there were any action branches we should take.
// PARAMETERS :	pPed - The ped of interest.
//				pCurrentActionResultTask - What the ped is currently doing.
//				nBranchActionId - An out val for the branch to take.
// RETURNS :	If a valid branch was found.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::CheckForBranch( CPed* pPed, CTaskMeleeActionResult* pActionResultTask, u32& nBranchActionId )
{
	taskAssertf( pPed, "pPed is null in CTaskMelee::CheckForBranch." );

	CEntity* pTargetEntity = GetTargetEntity();
	CPed* pTargetPed = ( pTargetEntity && pTargetEntity->GetIsTypePed() ) ? static_cast<CPed*>(pTargetEntity) : NULL;
	CActionFlags::ActionTypeBitSetData typeToLookFor;

	if( ShouldDaze() )
		typeToLookFor.Set( CActionFlags::AT_DAZED_HIT_REACTION, true );

	// If we do not currently have a CTaskMeleeActionResult
	if( !pActionResultTask || ( pPed->IsPlayer() && GetIsTaskFlagSet( MF_AllowNoTargetInterrupt ) ) )
	{
		if( pPed->IsPlayer() )
		{
			bool bAttemptTaunt = false;
			if( IsBlocking() )
				typeToLookFor.Set( CActionFlags::AT_BLOCK, true );
			else if( CTaskMelee::ShouldCheckForMeleeAmbientMove( pPed, bAttemptTaunt, HasLockOnTargetEntity() ) )
			{
				if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning ) )
					typeToLookFor.Set( CActionFlags::AT_SWIMMING, true );
				else 
					typeToLookFor.Set( CActionFlags::AT_MELEE, true );

				// NULL out the last found action definition in hopes that we find a new one and possibly a new target
				ms_LastFoundActionDefinition = NULL;

				CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();

				bool bTargetDeadPeds = ((pTargetEntity == NULL) || (pTargetPed && pTargetPed->IsDead())) && (rTargeting.GetLockOnTarget() == NULL);

				// See if we can grab a better target if we are button mashing
				CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
				const CWeaponInfo* pWeaponInfo = pWeaponMgr ? pWeaponMgr->GetEquippedWeaponInfo() : NULL;
				CEntity* pNewTarget = rTargeting.FindMeleeTarget( pPed, pWeaponInfo, true, bTargetDeadPeds, true, false, pTargetEntity );

				if(pNewTarget == pTargetEntity)
				{
					// do nothing. same target.
				}
				else if(pNewTarget)
				{
					if(pNewTarget->GetIsTypePed())
					{
						CPed* pNewTargetPed = static_cast<CPed*>( pNewTarget );
						pTargetPed = pNewTargetPed;
					}

					pTargetEntity = pNewTarget;
					SetTargetEntity( pTargetPed, false, true);

					// Check if we found an action to take.
					if( ms_LastFoundActionDefinition )
					{
						nBranchActionId = ms_LastFoundActionDefinition->GetID();
						return true;
					}
				}
			}
			else if( ShouldPerformIntroAnim( pPed ) )
				typeToLookFor.Set( CActionFlags::AT_INTRO, true );
			else if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventAllMeleeTaunts ) && !bAttemptTaunt && GetIsTaskFlagSet( MF_AllowStrafeMode ) && !GetIsTaskFlagSet( MF_BehindTargetPed ) )
			{
				if( m_tauntTimer.IsSet() && m_tauntTimer.IsOutOfTime() )
					typeToLookFor.Set( CActionFlags::AT_TAUNT, true );
			}
			else if( bAttemptTaunt )
				typeToLookFor.Set( CActionFlags::AT_TAUNT, true );
		}
		else
		{
			// Note: This is really one of the major parts of our "Melee Combat AI"...
			// Search for the quoted term in the line above to find the others.

			const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
			
			// Make AI attacks not be totally incessant, but make it so that the AI can
			// continue combos once started.
			if( DEV_ONLY( CCombatMeleeDebug::sm_bAllowNpcInitiatedMoves && )
				!pPed->GetPedResetFlag( CPED_RESET_FLAG_SuspendInitiatedMeleeActions ) &&
				pTargetPed &&
				taskVerifyf( pBrawlData, "CTaskMelee::CheckForBranch - Melee opponent without brawling style" ) )
			{
				// Only check for actions if we are within attack range
				if( CanPerformMeleeAttack( pPed, pTargetEntity ) )
				{
					// Check to see if we are within attack range and not queued
					if( m_nextRequiredMoveTimer.IsSet() && m_nextRequiredMoveTimer.IsOutOfTime() )
					{
						if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning ) )
							typeToLookFor.Set( CActionFlags::AT_SWIMMING, true );
						else 
							typeToLookFor.Set( CActionFlags::AT_MELEE, true );
					}
					
					// Check target to see if they are performing a move and if so check for dodging/blocking
					CQueriableInterface* pQueriableInterface = pTargetPed->GetPedIntelligence()->GetQueriableInterface();
					if( pQueriableInterface->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE_ACTION_RESULT ) )
					{
						if( pTargetPed->GetPedIntelligence()->GetTaskMeleeActionResult() && (m_bCanBlock || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMeleeCounter)) )
							typeToLookFor.Set( CActionFlags::AT_BLOCK, true );
					}
				}
				
				// If we are not performing an attack
				if( !typeToLookFor.IsSet( CActionFlags::AT_SWIMMING ) && !typeToLookFor.IsSet( CActionFlags::AT_MELEE ) )
				{
					// Always perform a taunt if we were asked to
					if( ShouldPerformIntroAnim( pPed ) )
						typeToLookFor.Set( CActionFlags::AT_INTRO, true );
					// Check taunt conditions
					else if( !pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMeleeCounter) && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventAllMeleeTaunts ) && !typeToLookFor.IsSet( CActionFlags::AT_BLOCK ) && m_tauntTimer.IsSet() && m_tauntTimer.IsOutOfTime() )
					{
						if( GetIsTaskFlagSet( MF_PlayTauntBeforeAttacking ) )
							typeToLookFor.Set( CActionFlags::AT_TAUNT, true );
						else
						{
							float fTauntProbability = IsDoingQueueForTarget() ? pBrawlData->GetTauntProbabilityQueued() : pBrawlData->GetTauntProbability();

							// AI still have a chance to not taunt based on fight proficiency
							if( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) < fTauntProbability )
							{
								if( GetQueueType() == QT_InactiveObserver )
								{
									pPed->NewSay( "FIGHT_BYSTANDER_TAUNT" );
									ResetTauntTimer();
								}
								// otherwise attempt an animated melee taunt
								else
									typeToLookFor.Set( CActionFlags::AT_TAUNT, true );
							}
						}
					}
				}
			}
		}

		// Only check if we need to
		if( typeToLookFor.AreAnySet() )
		{
			CEntity* pTargetEntity = GetTargetEntity();

			// For AI if the last action was a recoil we need to make an exception and allow 
			// non entry actions to be selected
			const bool bTestEntryAction = pPed->IsPlayer() ? true : !WasLastActionARecoil();

			// Attempt to find a suitable action
			const CActionDefinition* pActionToDo = ACTIONMGR.SelectSuitableAction(
				typeToLookFor,								// Only look for moves that are appropriate from on foot
				&m_impulseCombo,							// Impulse combo
				bTestEntryAction,							// Test whether or not it is an entry action.
				true,										// Only find entry actions.
				pPed,										// The acting ped.
				0,											// result Id forcing us.
				-1,											// Priority
				pTargetEntity,								// The entity being attacked.
				HasLockOnTargetEntity(),					// Do we have a lock on the target entity.
				false,										// Should we only allow impulse initiated moves.
				pPed->IsLocalPlayer(),						// Test the controls (for local players only).	
				CActionManager::ShouldDebugPed( pPed ) );

			// If we couldn't find an action to perform and had a valid target let us try to find a move that doesn't require a target entity
			if( pTargetEntity && !pActionToDo )
			{
				pActionToDo =  ACTIONMGR.SelectSuitableAction(
					typeToLookFor,									// Only look for moves that are appropriate from on foot
					&m_impulseCombo,								// Impulse combo
					bTestEntryAction,								// Test whether or not it is an entry action.
					true,											// Only find entry actions.
					pPed,											// The acting ped.
					0,												// result Id forcing us.
					-1,												// Priority
					NULL,											// The entity being attacked.
					false,											// Do we have a lock on the target entity.
					false,											// Should we only allow impulse initiated moves.
					pPed->IsLocalPlayer(),							// Test the controls (for local players only).	
					CActionManager::ShouldDebugPed( pPed ) );

				if( pActionToDo )
				{
					pTargetEntity = NULL;
				}
			}

			// Check if we found an action to take.
			if( pActionToDo )
			{
				SetTargetEntity( pTargetEntity, false, true );

				// Let the caller know what branch to go to.
				nBranchActionId = pActionToDo->GetID();
				return true;
			}

			// If we fell through and didn't find a taunt to play... reset the timer
			if( typeToLookFor.IsSet( CActionFlags::AT_TAUNT ) )
				ResetTauntTimer();
		}
	}
	else
	{
		// Check for player initiated actions.
		if( pPed->IsPlayer() )
		{
			if( IsBlocking() )
				typeToLookFor.Set( CActionFlags::AT_BLOCK, true );

			if( pActionResultTask->IsDoingBlock() )
				typeToLookFor.Set( CActionFlags::AT_COUNTER, true );
			else
			{
				if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning ) )
					typeToLookFor.Set( CActionFlags::AT_SWIMMING, true );
				else 
					typeToLookFor.Set( CActionFlags::AT_MELEE, true );
			}

			if( pActionResultTask->ShouldBranchOnCurrentFrame( pPed, &m_impulseCombo, &nBranchActionId, false, typeToLookFor, 0 ) )
				return true;
		}
		// Check for combo actions.
		else
		{
			// Only check for actions if we are within attack range
			if( CanPerformMeleeAttack( pPed, pTargetEntity ) )
			{
				if( (pActionResultTask->IsDoingBlock() && m_bCanCounter) || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMeleeCounter) )
					typeToLookFor.Set( CActionFlags::AT_COUNTER, true );
				// For now only allow the player to counter a hit reaction
				else if( !pActionResultTask->IsDoingReaction() && m_bCanPerformCombo )
				{
					if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning ) )
						typeToLookFor.Set( CActionFlags::AT_SWIMMING, true );
					else 
						typeToLookFor.Set( CActionFlags::AT_MELEE, true );
				}
				
				// Check to see if we should attempt a block
				if( pTargetPed )
				{
					// Check target to see if they are performing a move and if so check for dodging/blocking
					CQueriableInterface* pQueriableInterface = pTargetPed->GetPedIntelligence()->GetQueriableInterface();
					if( pQueriableInterface->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE_ACTION_RESULT ) )
					{
						if( pTargetPed->GetPedIntelligence()->GetTaskMeleeActionResult() && (m_bCanBlock || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMeleeCounter)) )
							typeToLookFor.Set( CActionFlags::AT_BLOCK, true );
					}
				}
			}

			// Either check for all available branches or only for branches that don't
			// require a move to be self initiated done... (i.e. have impulse test of none).
			//const bool bImpulseInitiatedMovesOnly = !bAttemptNpcInitiatedAction;
			if( typeToLookFor.AreAnySet() )
			{
				if( pActionResultTask->ShouldBranchOnCurrentFrame( pPed, &m_impulseCombo, &nBranchActionId, false, typeToLookFor, 0 ) )
					return true;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetTargetEntity
// PURPOSE :	Set the target of the ped.  This is used for movement, homing and
//				attacks.
// PARAMETERS :	pTargetEntity - the target.
//				bHasLockOnTargetEntity - if the ped is locked on to the entity.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::SetTargetEntity( CEntity* pTargetEntity, bool bHasLockOnTargetEntity, bool bReplaceLockOn )
{
	if(!CanChangeTarget())
	{
		return;
	}

	if(GetEntity())
	{
		CPed* pPed = GetPed();
		if(pPed)
		{
			if( bReplaceLockOn && pPed->IsLocalPlayer() )
			{
				CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();
				if( m_pTargetEntity && (rTargeting.GetLockOnTarget() == m_pTargetEntity) && (rTargeting.GetLockOnTarget() != pTargetEntity) )
				{
					rTargeting.SetLockOnTarget( pTargetEntity );
				}
			}

			// Special case for family gang members that couldn't attack the player
			if( pPed->PopTypeIsRandom() && pTargetEntity && pTargetEntity->GetIsTypePed() )
			{
				CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);
				if( pTargetPed->IsLocalPlayer() && pPed->GetPedIntelligence() )
					pPed->GetPedIntelligence()->SetFriendlyException( pTargetPed );
			}
		}

#if __BANK
		AI_LOG_WITH_ARGS("CTaskMelee::SetTargetEntity() - pPed = %s, m_pTargetEntity = %s, pTargetEntity = %s, task = %p \n",pPed ? pPed->GetLogName() : "Null", m_pTargetEntity ? m_pTargetEntity->GetLogName() : "Null", pTargetEntity ? pTargetEntity->GetLogName() : "Null", this);
		if(pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontChangeTargetFromMelee))
		{
			aiDisplayf("Frame %i, CTaskMelee::SetTargetEntity(), task = %p - callstack \n", fwTimer::GetFrameCount(), this);
			sysStack::PrintStackTrace();
		}
#endif
	}

	// Just set the has lock on flag to its current state (allowing us to
	// acquire and break locks on a given ped even when they are acquired as
	// an ambient target).
	m_pTargetEntity = pTargetEntity;
	m_bHasLockOnTargetEntity = bHasLockOnTargetEntity;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldPerformIntroAnim
// PURPOSE :	Determines whether or not we should perform the intro anim
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::ShouldPerformIntroAnim( CPed* pPed ) const
{
	if( !pPed )
		return false;

	if( !GetIsTaskFlagSet( MF_PerformIntroAnim ) )
		return false;

	if( GetIsTaskFlagSet( MF_ForceStrafe ) )
		return false;
	
	if( pPed->GetTaskData().GetIsFlagSet( CTaskFlags::DisableMeleeIntroAnimation ) )
		return false;

	// Determine whether or not we are fit to play the melee intro anim
	if( pPed->IsPlayer() || GetQueueType() == QT_ActiveCombatant )
	{
		// We don't want to play intro anim when behind our target, they could be trying to stealth kill
		CEntity* pTargetEntity = GetTargetEntity();
		if( pPed->IsPlayer() && pTargetEntity )
		{
			static dev_float sfDotThreshold = -0.15f;
			Vec3V v3PedToTarget = pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition();
			v3PedToTarget = Normalize( v3PedToTarget );

			// Check to see if we are behind the target
			float fDotCompare = Dot( v3PedToTarget, pTargetEntity->GetTransform().GetB() ).Getf();
			if( fDotCompare > sfDotThreshold )
				return false;
		}

		// We are okay to perform the intro anims
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CanPerformMeleeAttack
// PURPOSE :	Determines whether or not teh designated ped should be allowed 
//				to perform a melee attack
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::CanPerformMeleeAttack( CPed* pPed, CEntity* pTargetEntity ) const
{
	if( pPed->IsLocalPlayer() && CNewHud::IsShowingHUDMenu() )
		return false;

	if( !pPed || pPed->IsPlayer() || !pTargetEntity )
		return true;

	// Check to see if we are within attack range
	Vec3V vDelta = pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition();
	if( !IsLosBlocked() && !ShouldBeInInactiveTauntMode() && !IsDoingQueueForTarget() && Abs( vDelta.GetZf() ) < sm_fAiSeekModeTargetHeight )
	{
		const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
		taskAssertf( pBrawlData, "CTaskMelee::IncreaseMeleeBuff - Melee opponent without brawling style" );

		Vec3V vDeltaXY( vDelta.GetXf(), vDelta.GetYf(), 0.0f );
		const float fTargetDistanceSq = MagSquared( vDeltaXY ).Getf();

		return fTargetDistanceSq < rage::square( pBrawlData->GetAttackRangeMax() );
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CalculateDesiredTargetDistance
// PURPOSE :	Given the current melee queue type, this function will calculate
//				the desired target distance 
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::CalculateDesiredTargetDistance( void )
{
	if( IsLosBlocked() )
		m_fDesiredTargetDist = sm_fAiSeekModeActiveCombatantRadius;
	else
	{
		QueueType queueType = GetQueueType();
		if( queueType == QT_SupportCombatant )
			m_fDesiredTargetDist = fwRandom::GetRandomNumberInRange( sm_fAiSeekModeSupportCombatantRadiusMin, sm_fAiSeekModeSupportCombatantRadiusMax );
		else if( queueType <= QT_InactiveObserver )
			m_fDesiredTargetDist = fwRandom::GetRandomNumberInRange( sm_fAiSeekModeInactiveCombatantRadiusMin, sm_fAiSeekModeInactiveCombatantRadiusMax );
		else
			m_fDesiredTargetDist = sm_fAiSeekModeActiveCombatantRadius;
	}	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CalculateDesiredChasePosition
// PURPOSE :	Predict where the target will be given the current velocity and
//				adjust the scan time based on distance from the target
/////////////////////////////////////////////////////////////////////////////////
Vec3V_Out CTaskMelee::CalculateDesiredChasePosition( CPed* pPed )
{
	Vec3V vPredictedPosition( VEC3_ZERO );
	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
	{
		static dev_float sfPredictedTimeMin = 0.5f;
		static dev_float sfPredictedTimeMax = 1.5f;

		// based on distance from the player 
		static dev_float fMaxScaleDistance = 35.0f;

		Vec3V vPedToTarget = pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition();
		float fDistSq = MagSquared( vPedToTarget ).Getf();

		const CBrawlingStyleData* pData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
		s32 fSeekModeTimeMin = pData ? pData->GetSeekModeScanTimeMin() : sm_nAiSeekModeScanTimeMin;
		s32 fSeekModeTimeMax = pData ? pData->GetSeekModeScanTimeMax() : sm_nAiSeekModeScanTimeMax;

		float fPredictedTimeScale = 0.0f;
		if( fDistSq < rage::square( fMaxScaleDistance ) )
		{
			float fTimeRatio = fDistSq / rage::square( fMaxScaleDistance );
			fPredictedTimeScale = Lerp( fTimeRatio, sfPredictedTimeMin, sfPredictedTimeMax );
			m_seekModeScanTimer.Set( fwTimer::GetTimeInMilliseconds(), Lerp( fTimeRatio, fSeekModeTimeMin, fSeekModeTimeMax ) );
		}
		else
		{
			fPredictedTimeScale = sfPredictedTimeMax;
			m_seekModeScanTimer.Set( fwTimer::GetTimeInMilliseconds(), fSeekModeTimeMax );
		}

		// Set the desired offset distance
		vPredictedPosition.SetYf( GetDesiredTargetDistance() );

		// Rotate the desired offset in the direction of the desired heading
		vPredictedPosition = RotateAboutZAxis( vPredictedPosition, ScalarV( GetAngleSpread() ) );

		// Otherwise predict where the target will be
		vPredictedPosition += CActionManager::CalculatePredictedTargetPosition( pPed, pTargetEntity, fPredictedTimeScale, BONETAG_ROOT );
	}

	return vPredictedPosition;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetQueueType
// PURPOSE :	Changes the current queue type and recalculates the desired
//				target distance.
// PARAMETERS :	queueType - Type which you would like to change this ped to
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::SetQueueType( QueueType newQueueType ) 
{ 
	if( newQueueType != m_queueType )
	{
		m_queueType = newQueueType;

		// Audio wants this to happen whenever a new active combatant comes into scope
		if( newQueueType == QT_ActiveCombatant )
		{
			CPed* pPed = GetPed();
			if( pPed && !pPed->IsPlayer() )
				pPed->NewSay("FIGHT", 0, false, false);
		}

		// Re-calculate the distance at which we want this melee combatant to be
		CalculateDesiredTargetDistance();

		// Release this as we will most likely need to switch to a new type
		m_TauntClipSetRequestHelper.Release();
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResultId
// PURPOSE :	Get the action result Id of the action subtask that is currently
//				being enacted.
// RETURNS :	The action Id or 0 if no action is present.
/////////////////////////////////////////////////////////////////////////////////
u32 CTaskMelee::GetActionResultId( void ) const
{
	// JB: The task may now also be doing a TASK_COMPLEX_CONTROL_MOVEMENT as its subtask to seek the target ped
	const aiTask* pSubTask = GetSubTask();
	if( !pSubTask || pSubTask->GetTaskType() != CTaskTypes::TASK_MELEE_ACTION_RESULT )
		return 0;

	const CTaskMeleeActionResult* pActionResultTask = static_cast<const CTaskMeleeActionResult*>( pSubTask );
	return pActionResultTask->GetResultId();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetLocalReaction
// PURPOSE :	Force the ped to react immediately to some situation.
// PARAMETERS :	pActionResult - result to pay.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::SetLocalReaction( const CActionResult* pActionResult, bool bHitReaction, u16 uMeleeID)
{
	if(GetPed()->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF("[%d] CTaskMelee::SetLocalReaction - Player = %s State = %d", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), GetState());
	}

	SetRunningLocally( true );
	m_bLocalReaction = true;
	m_pNextActionResult = pActionResult;
	m_nUniqueNetworkMeleeActionID = uMeleeID;
	if( bHitReaction )
		SetTaskFlag( MF_IsDoingReaction );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ResetImpulseCombo
// PURPOSE :	Force the ped to reset the impulse combo (whilst being held, allows us to keep pummeling the held ped).
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::ResetImpulseCombo( void )
{
 	m_bActionIsBranch = false;
	m_impulseCombo.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	IncreaseMeleeBuff
// PURPOSE :	Increase a melee buffer to allow AI to become more likely to attack
//				and block after consecutive successful hits.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::IncreaseMeleeBuff( CPed* pPed )
{
	const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
	if( taskVerifyf( pBrawlData, "CTaskMelee::IncreaseMeleeBuff - Melee opponent without brawling style" ) )
	{
		m_fBlockBuff += sm_fAiBlockConsecutiveHitBuffIncrease;
		m_fBlockBuff = MIN( m_fBlockBuff, 1.0f - pBrawlData->GetBlockProbabilityMax() );

		// Subtract a bit of time to the next move timer... allowing for quicker attack responses
		float fFightProficiency = Clamp( pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat( kAttribFloatFightProficiency ), 0.0f, 1.0f );
		m_nextRequiredMoveTimer.SubtractTime( Lerp( fFightProficiency, sm_fAiAttackConsecutiveHitBuffIncreaseMin, sm_fAiAttackConsecutiveHitBuffIncreaseMax ) );	
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DecreaseMeleeBuff
// PURPOSE :	Decay block buffer to slowly approach original block value
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::DecreaseMeleeBuff( void )
{ 
	m_fBlockBuff -= sm_fAiBlockActionBuffDecrease;
	m_fBlockBuff = MAX( m_fBlockBuff, 0.0f );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ResetTauntTimer
// PURPOSE :	Helper function to facilitate reseting the taunt frequency
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::ResetTauntTimer( void )
{
	CPed* pPed = GetPed();
	const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
	if( !pPed->IsPlayer() && taskVerifyf( pBrawlData, "CTaskMelee::ResetTauntTimer - Melee opponent without brawling style" ) )
	{
		if( IsDoingQueueForTarget() )
			m_tauntTimer.Set( fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange( pBrawlData->GetTauntFrequencyQueuedMin(), pBrawlData->GetTauntFrequencyQueuedMax() ) );
		else 
			m_tauntTimer.Set( fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange( pBrawlData->GetTauntFrequencyMin(), pBrawlData->GetTauntFrequencyMax() ) );
	}
	else
		m_tauntTimer.Set( fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange( sm_nPlayerTauntFrequencyMinInMs, sm_nPlayerTauntFrequencyMaxInMs) );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	MakeStreamingClipRequests
// PURPOSE :	Send requests to the streaming system for clips this ped
//				needs or might likely need for melee combat.
// PARAMETERS :	pPed -  the ped we are interested in.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::MakeStreamingClipRequests( CPed* pPed )
{
	taskAssertf( pPed, "pPed is null in CTaskMelee::MakeStreamingClipRequests." );

	// Get the peds model info (since that stores the necessary streaming melee clips).
	const CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
	taskAssert( pModelInfo );

	// Make sure the melee movement clips are loaded/loading.
	if( (fwMvClipSetId)pModelInfo->GetPersonalitySettings().GetMeleeMovementDictionaryGroupId().GetHash() != CLIP_SET_ID_INVALID )
		m_MovementClipSetRequestHelper.Request( (fwMvClipSetId)pModelInfo->GetPersonalitySettings().GetMeleeMovementDictionaryGroupId().GetHash() );

	// AI only clip set logic
	if( !pPed->IsLocalPlayer() )
	{
		// Taunt and variation requests
		bool bRequestedTauntClipSet = false, bRequestedVariationClipSet = false;
		const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if( pEquippedWeaponInfo )
		{
			QueueType eQueueType = GetQueueType();
			if( eQueueType == QT_ActiveCombatant )
			{
				bRequestedTauntClipSet = bRequestedVariationClipSet = true;
				if( !m_TauntClipSetRequestHelper.IsLoaded() )
				{
					m_TauntClipSetRequestHelper.Request( pEquippedWeaponInfo->GetMeleeTauntClipSetId( *pPed ) );
				}

				if( !m_VariationClipSetRequestHelper.IsLoaded() )
				{
					m_VariationClipSetRequestHelper.Request( pEquippedWeaponInfo->GetMeleeVariationClipSetId( *pPed ) );
				}
			}
			else if( eQueueType == QT_SupportCombatant || ShouldBeInInactiveTauntMode() )
			{
				bRequestedTauntClipSet = true;
				if( !m_TauntClipSetRequestHelper.IsLoaded() )
				{
					m_TauntClipSetRequestHelper.Request( pEquippedWeaponInfo->GetMeleeSupportTauntClipSetId( *pPed ) );
				}
			}
		}

		if( !bRequestedTauntClipSet && m_TauntClipSetRequestHelper.IsLoaded() )
		{
			m_TauntClipSetRequestHelper.Release();
		}

		if( !bRequestedVariationClipSet && m_VariationClipSetRequestHelper.IsLoaded() )
		{
			m_VariationClipSetRequestHelper.Release();
		}
	}
	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ReleaseStreamingClipRequests
// PURPOSE :	Free the requests to the streaming system for clips.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::ReleaseStreamingClipRequests( void )
{
	// Release any streaming melee movement clips.
	m_MovementClipSetRequestHelper.Release();
	m_TauntClipSetRequestHelper.Release();
	m_VariationClipSetRequestHelper.Release();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateTarget
// PURPOSE :	Updates the melee target for the ped.
// PARAMETERS :	pPed - the ped we are interest in.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::UpdateTarget( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMelee::UpdateTarget." );

	CEntity* pTargetEntity = GetTargetEntity();
#if __BANK
	if( CCombatMeleeDebug::sm_bDrawTargetPos && pTargetEntity)
	{
		if( (CCombatMeleeDebug::sm_bVisualisePlayer && pPed->IsLocalPlayer()) ||
			(CCombatMeleeDebug::sm_bVisualiseNPCs && !pPed->IsLocalPlayer()) )
		{
			Vector3 vPos = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetPosition());
			static dev_float s_fZOffset = 1.25f;
			static dev_float s_fRadius = 0.15f;
			vPos.z += s_fZOffset;
			CCombatMeleeDebug::sm_debugDraw.AddSphere( VECTOR3_TO_VEC3V(vPos), s_fRadius, Color_green, 1 );
		}
	}
#endif

	// Don't update if we have already decided to perform an action
	if( m_pNextActionResult != NULL )
		return;
	
	// Don't allow switching of targets during a melee action of any type
	const CTask* pSubTask = GetSubTask();
	if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT )
		return;

	bool bIsLocalPlayer = pPed->IsLocalPlayer();

	// Update our lock on target so it is correct.
	if( bIsLocalPlayer )
	{
		CEntity* pWeaponLockOnTargetEntity = pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
		if( pWeaponLockOnTargetEntity )
			SetTargetEntity( pWeaponLockOnTargetEntity, true );
		else
			SetTargetEntity( pTargetEntity );

		// If the local player has a network object
		if( pPed->GetNetworkObject() )
		{
			// pass on the current target to the player network object
			const CEntity* pCurrentTargetEntity = GetTargetEntity();
			if( pCurrentTargetEntity )
			{
				static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->SetAimTarget(*pCurrentTargetEntity);
			}
		}

		// Check if we should search for ambient targets.
		if( !pTargetEntity )
		{
			SetQueueType( QT_ActiveCombatant );

			// Get our held weapon info to determine our targeting range.
			u32 nWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : 0;
			if( nWeaponHash != 0 )
			{
				const float fWeaponRange = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash)->GetLockOnRange();

				// Try to find an auto target target within that range.
				CPedTargetEvaluator::tHeadingLimitData headingData;
				headingData.fHeadingAngularLimit = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetAngularLimitMelee;
				headingData.fHeadingFalloffPower = 1.0f;
				CEntity* pAmbientTargetEntity = CPedTargetEvaluator::FindTarget( pPed, CPedTargetEvaluator::MELEE_TARGET_FLAGS | CPedTargetEvaluator::TS_DESIRED_DIRECTION_HEADING, fWeaponRange, fWeaponRange, NULL, NULL, headingData );

				// Update the old target and the new target.
				if( pAmbientTargetEntity )
				{
					SetTargetEntity( pAmbientTargetEntity );
					
					// if the local player has a network object
					if( pPed->GetNetworkObject() )
					{
						// pass on the current target to the player network object for syncing
						static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->SetAimTarget(*pAmbientTargetEntity);
					}
				}
			}
		}
	}
	// Since we don't currently have a target we cannot have a lock on.
	else if( !pTargetEntity )
	{
		SetQueueType( QT_InactiveObserver );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateHeadAndSpineLooks
// PURPOSE :	Updates the head and spine look IK.
// PARAMETERS :	pPed - the ped we are interest in.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::UpdateHeadAndSpineLooksAndLegIK( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMelee::UpdateHeadAndSpineLooksAndLegIK." );

	// Make sure we have and can get the target ped.
	CEntity* pTargetEntity = GetTargetEntity();
	if( !pTargetEntity || !pTargetEntity->GetIsTypePed() )
		return;

	CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );

	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();
	const float fHeightDiff = rage::Abs( vPedPosition.GetZf() - vTargetPosition.GetZf() );

	// Do leg ik for AI only
	if( !pPed->GetIKSettings().IsIKFlagDisabled( IKManagerSolverTypes::ikSolverTypeLeg ) && sm_bDoLegIK )
		pPed->m_PedConfigFlags.SetPedLegIkMode( GetQueueType() == QT_ActiveCombatant ? LEG_IK_MODE_FULL_MELEE : CIkManager::GetDefaultLegIkMode(pPed) );

	// Do the spine look to compensate for target peds that are standing on stairs, etc.
	// make we don't do it if the height difference is too great though (otherwise it just
	// looks bad).
	if( !pPed->GetIKSettings().IsIKFlagDisabled( IKManagerSolverTypes::ikSolverTypeTorso ) && sm_bDoSpineIk )
	{
		pPed->GetIkManager().SetTorsoMinYaw(( DtoR * sm_fSpineLookMinYawDegs) );
		pPed->GetIkManager().SetTorsoMaxYaw(( DtoR * sm_fSpineLookMaxYawDegs) );
		pPed->GetIkManager().SetTorsoMinPitch(( DtoR * sm_fSpineLookMinPitchDegs) );
		pPed->GetIkManager().SetTorsoMaxPitch(( DtoR * sm_fSpineLookMaxPitchDegs) );
		pPed->GetIkManager().SetTorsoOffsetYaw(( DtoR * sm_fSpineLookTorsoOffsetYawDegs) );

		float fSpineLookAmount = 0.0f;
		if( fHeightDiff > sm_fSpineLookMaxHeightDiff )
			fSpineLookAmount = 0.0f;
		else
		{
			const float p = 1.0f - ( fHeightDiff / sm_fSpineLookMaxHeightDiff );
			fSpineLookAmount = rage::Powf( p, sm_fSpineLookAmountPowerCurve );// Give the spine look a falloff curve.
		}

		if( fSpineLookAmount > 0.0f )
		{
			Matrix34 matTargetsSpine3Bone;
			pTargetPed->GetGlobalMtx( pTargetPed->GetBoneIndexFromBoneTag( BONETAG_SPINE3 ), matTargetsSpine3Bone );
			pPed->GetIkManager().PointGunAtPosition( matTargetsSpine3Bone.d, -1.0f /*fSpineLookAmount*/ );
		}
	}

	// Do the head looks to make it look like there is an actual intelligence in the ped.
	if( !pPed->GetIKSettings().IsIKFlagDisabled( IKManagerSolverTypes::ikSolverTypeHead ) && ( sm_bDoHeadIk || sm_bDoEyeIK ) )
	{	
		Vec3V vLookAtPosition = pTargetPed->GetBonePositionCachedV( BONETAG_HEAD );
		bool bAllowHeadIk = true;
		CTask* pSubTask = GetSubTask();
		if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT )
		{
			CTaskMeleeActionResult* pActionResultTask = static_cast<CTaskMeleeActionResult*>( pSubTask );
			bAllowHeadIk = !pActionResultTask->IsDoingReaction() || pActionResultTask->ShouldAllowInterrupt();

			// Only use the cached position if we are processing melee collisions
			// we do not want the ped to stop looking at the same height if we started a punch
			if( pActionResultTask->ShouldProcessMeleeCollisions() )
				vLookAtPosition = pPed->GetTransform().GetPosition() + pActionResultTask->GetCachedHeadIkOffset();
		}

		if( bAllowHeadIk )
		{
			// Should we use head ik?
			if( sm_bDoHeadIk )
			{
				CIkRequestBodyLook lookRequest( NULL, BONETAG_INVALID, vLookAtPosition, 0, CIkManager::IK_LOOKAT_HIGH);
				lookRequest.SetRefDirHead( LOOKIK_REF_DIR_FACING );
				lookRequest.SetRefDirNeck( LOOKIK_REF_DIR_FACING );
				lookRequest.SetRotLimNeck( LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE );
				lookRequest.SetRotLimNeck( LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROWEST );
				lookRequest.SetHoldTime( 100 );

				// Make the blend and turn rate as fast as possible if we are trying to sync
				if( GetIsTaskFlagSet( MF_AnimSynced ) )
				{
					lookRequest.SetTurnRate( LOOKIK_TURN_RATE_FAST );
					lookRequest.SetBlendInRate( LOOKIK_BLEND_RATE_INSTANT );
				}

				pPed->GetIkManager().Request( lookRequest );
			}
			else if( sm_bDoEyeIK && ( pPed->IsPlayer() || GetQueueType() == QT_ActiveCombatant ) )
				pPed->GetIkManager().LookAt(0, NULL, 100, BONETAG_INVALID, &RCC_VECTOR3( vLookAtPosition ), LF_USE_EYES_ONLY, GetIsTaskFlagSet( MF_AnimSynced ) ? LOOKIK_BLEND_RATE_INSTANT : 500, 500, CIkManager::IK_LOOKAT_HIGH );
		}
	}
}

static const float HIT_BLOCKING_MOMENTUM_SCALER = 1.0f;

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	MeleeMoveTaskControl
// PURPOSE :	Updates the movement task, if it is the Melee movement task &
//				not controlled by a CtrlMove task.
// PARAMETERS :	pPed - the ped we are interest in.
//				bAllowMovementInput - Whether or not to allow movement inputs through
//				bAllowStrafing - Whether or not to allow strafing 			
//				bAllowHeadingUpdate - Whether or not to allow heading
//				bAllowStealth - Whether or not we should process stealth mode
//				pAvoidEntity - an optional entity to try to move away from.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::MeleeMoveTaskControl( CPed* pPed, bool bAllowMovementInput, bool bAllowStrafing, bool bAllowHeadingUpdate, bool bAllowStealth, CEntity* pAvoidEntity )
{
	Assertf( pPed, "pPed is null in CTaskMelee::MeleeMoveTaskControl." );
	Assert( pPed->GetPedIntelligence() );

	// Get the movement task or create a new one if none exists.
	CTask* pMovementTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
	if( !pPed->IsNetworkClone() && ( !pMovementTask || pMovementTask->GetTaskType() != CTaskTypes::TASK_MOVE_MELEE_MOVEMENT ) )
	{
		// Start up the action movement task.  This handles the shuffling
		// forward/back/left/right when in melee mode.
		pPed->GetPedIntelligence()->AddTaskMovement( rage_new CTaskMoveMeleeMovement( GetTargetEntity(), HasLockOnTargetEntity(), bAllowMovementInput, bAllowStrafing, bAllowHeadingUpdate, bAllowStealth ) );

		// Update the pointer to the movement task.
		pMovementTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
	}

	Assertf( pMovementTask, "pMovementTask was not found for pPed in CTaskMelee::MeleeMoveTaskControl." );

	// Mark the movement task as still in use this frame (so it doesn't go away).
	CTaskMoveInterface* pMoveInterface = pMovementTask->GetMoveInterface();
	Assert( pMoveInterface );
	pMoveInterface->SetCheckedThisFrame( true );
#if !__FINAL
	pMoveInterface->SetOwner( this );
#endif

	if( pMovementTask->GetTaskType() == CTaskTypes::TASK_MOVE_MELEE_MOVEMENT )
	{
		CTaskMoveMeleeMovement* pMeleeMoveTask = static_cast<CTaskMoveMeleeMovement*>( pMovementTask );

		// Set whether or not to allow SimpleMeleeActionResults to use the movement
		// input values or not.
		pMeleeMoveTask->SetAllowMovementInput( bAllowMovementInput );
		pMeleeMoveTask->SetAllowStrafing( bAllowStrafing );
		pMeleeMoveTask->SetAllowHeadingUpdate( bAllowHeadingUpdate );
		pMeleeMoveTask->SetAllowStealth( bAllowStealth );
		
		bool bForcingRun = true;
		if(pPed->IsLocalPlayer() && IsCurrentlyDoingAMove(false, false, false))
		{
			//! If we are performing a move & don't have sprint/run held, then don't force run.
			const CControl* pControl = pPed->GetControlFromPlayer();
			if(pControl && !pControl->GetPedSprintIsDown())
			{
				bForcingRun = false;
			}
		}
		
		pMeleeMoveTask->SetForceRun( bForcingRun );

		// Make sure the movement task has the same target as the melee task.
		CEntity* pTargetEntity = GetTargetEntity();
		pMeleeMoveTask->SetTargetEntity( pTargetEntity, HasLockOnTargetEntity() );

		// Determine what range we would like to be at
		float fDesiredSelfToTargetRange = sm_fAiTargetSeparationDesiredSelfToTargetRange;
		if( IsDoingQueueForTarget() )
		{
			fDesiredSelfToTargetRange = GetDesiredTargetDistance();
		}
		// Check to see if the target ped is getting up and have them keep their distance to avoid clipping
		else if( pTargetEntity && pTargetEntity->GetIsTypePed() )
		{
			CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );
			if( pTargetPed->GetPedIntelligence() && pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_NM_CONTROL ) )
			{
				fDesiredSelfToTargetRange = sm_fAiTargetSeparationDesiredSelfToGetupTargetRange;
			}
		}

		// Set our desired range and impetus control vars.
		pMeleeMoveTask->SetTargetCurrentDesiredRange( fDesiredSelfToTargetRange );
		pMeleeMoveTask->SetTargetCurrentMinRangeForImpetus( IsDoingQueueForTarget() ? sm_fAiQueuePrefdPosDiffMinForImpetus : sm_fAiTargetSeparationPrefdDiffMinForImpetus );
		pMeleeMoveTask->SetTargetCurrentMaxRangeForImpetus( IsDoingQueueForTarget() ? sm_fAiQueuePrefdPosDiffMaxForImpetus : sm_fAiTargetSeparationPrefdDiffMaxForImpetus );
		
		// Make sure to avoid any other attackers.
		if( pAvoidEntity )
			pMeleeMoveTask->SetAvoidEntity( pAvoidEntity );
	}
	else
		pPed->RestoreHeadingChangeRate();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessLineOfSightTest
// PURPOSE :	Do tests to see if we should be seeking or to update our seeking
//				los tests.
// PARAMETERS :	The ped of interest.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::ProcessLineOfSightTest( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMelee::ProcessLineOfSightTest." );

	CEntity* pTargetEntity = GetTargetEntity();
	if( !pTargetEntity )
		return;

	if( !m_bSeekModeLosTestHasBeenRequested )
	{
		// JB : I added this task-timer so that navmesh requests aren't being issued every frame, for every ped in melee combat.
		// This task can also be recreated every frame, so need an initial delay time to be set also in CreateFirstSubTask
		if( IsLosBlocked() || !m_seekModeLosTestNextRequestTimer.IsSet() || m_seekModeLosTestNextRequestTimer.IsOutOfTime() )
		{
			CancelPendingLosRequest();	// (Shouldn't be required really)

			Vec3V vEntityCentre	= pPed->GetTransform().GetPosition();
			Vec3V vTargetCentre	= pTargetEntity->GetTransform().GetPosition();

			// Check to see if this ped is using a prop with the scenario
			static const s32 iNumExceptions = 2;
			const CEntity* ppExcludeEnts[iNumExceptions] = { pTargetEntity, NULL };
			if( pTargetEntity && pTargetEntity->GetIsTypePed() )
			{
				CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );
				CTask* pPedScenarioTask = pTargetPed->GetPedIntelligence() ? pTargetPed->GetPedIntelligence()->FindTaskByType( CTaskTypes::TASK_USE_SCENARIO ) : NULL;
				if(  pPedScenarioTask )
					ppExcludeEnts[1] = static_cast<CTaskScenario*>(pPedScenarioTask)->GetScenarioEntity();
			}

			// Do a high line of site test.
			float fTimeToReachEntity = 0.0f;
			int nBoneIdxCache = -1;
			Vec3V vStartChestApprox( vEntityCentre );
			Vec3V vStartChestDirection( V_ZERO );
			Vec3V vEndChestApprox( vTargetCentre );
			Vec3V vEndChestDirection( V_ZERO );

 			if( pPed->GetPedType() == PEDTYPE_ANIMAL )	
 			{
 				CActionManager::TargetTypeGetPosDirFromEntity( vStartChestApprox, vStartChestDirection, CActionFlags::TT_ENTITY_ROOT, pPed, pPed, vEntityCentre, fTimeToReachEntity, nBoneIdxCache );
 				nBoneIdxCache = -1;
 				CActionManager::TargetTypeGetPosDirFromEntity( vEndChestApprox, vEndChestDirection, CActionFlags::TT_ENTITY_ROOT, pTargetEntity, pPed, vEntityCentre, fTimeToReachEntity, nBoneIdxCache );
 			}
 			else
			{
				vStartChestApprox.SetZf( vEntityCentre.GetZf() + 0.5f );
				CActionManager::TargetTypeGetPosDirFromEntity( vStartChestApprox, vStartChestDirection, CActionFlags::TT_PED_CHEST, pPed, pPed, vEntityCentre, fTimeToReachEntity, nBoneIdxCache );

				vEndChestApprox.SetZf( vTargetCentre.GetZf() + 0.5f );
				nBoneIdxCache = -1;
				CActionManager::TargetTypeGetPosDirFromEntity( vEndChestApprox, vEndChestDirection, CActionFlags::TT_PED_CHEST, pTargetEntity, pPed, vEntityCentre, fTimeToReachEntity, nBoneIdxCache );
			}

			// Slightly scale the start/end positions to compensate for the ped capsule
			const float fCapsuleRadius = pPed->GetCapsuleInfo() ? pPed->GetCapsuleInfo()->GetProbeRadius() : CPedModelInfo::ms_fPedRadius;
			vStartChestApprox += Scale( vEndChestDirection, ScalarV( fCapsuleRadius ) );
			vEndChestApprox += Scale( Negate( vEndChestDirection ), ScalarV( fCapsuleRadius ) );
			
			WorldProbe::CShapeTestCapsuleDesc probeData1;
			probeData1.SetResultsStructure( &m_seekModeLosTestHandle1 );
			probeData1.SetCapsule( VEC3V_TO_VECTOR3( vStartChestApprox ), VEC3V_TO_VECTOR3( vEndChestApprox ), fCapsuleRadius );
			probeData1.SetContext( WorldProbe::EMelee );
			probeData1.SetIncludeFlags( ArchetypeFlags::GTA_MELEE_VISIBILTY_TYPES );
			probeData1.SetIsDirected( true );
			probeData1.SetExcludeEntities( ppExcludeEnts, iNumExceptions );
			WorldProbe::GetShapeTestManager()->SubmitTest( probeData1, WorldProbe::PERFORM_ASYNCHRONOUS_TEST );

			if( pPed->GetPedType() != PEDTYPE_ANIMAL )
			{
				// Do a low line of site test.
				nBoneIdxCache = -1;
				Vec3V vStartKneeApprox( vEntityCentre.GetXf(), vEntityCentre.GetYf(), vEntityCentre.GetZf() - 0.5f );
				Vec3V vStartKneeDriection( VEC3_ZERO );
				CActionManager::TargetTypeGetPosDirFromEntity( vStartKneeApprox, vStartKneeDriection, CActionFlags::TT_PED_KNEE_RIGHT, pPed, pPed, vEntityCentre, fTimeToReachEntity, nBoneIdxCache );

				nBoneIdxCache = -1;
				Vec3V vEndKneeApprox( vTargetCentre.GetXf(), vTargetCentre.GetYf(), vTargetCentre.GetZf() - 0.5f );
				Vec3V vEndKneeDirection( VEC3_ZERO );

				CActionManager::TargetTypeGetPosDirFromEntity( vEndKneeApprox, vEndKneeDirection, CActionFlags::TT_PED_KNEE_RIGHT, pTargetEntity, pPed, vEntityCentre, fTimeToReachEntity, nBoneIdxCache );

				// Slightly scale the start/end positions to compensate for the ped capsule
				vStartKneeApprox += Scale( vEndKneeDirection, ScalarV( fCapsuleRadius ) );
				vEndKneeApprox += Scale( Negate( vEndKneeDirection ), ScalarV( fCapsuleRadius ) );

				WorldProbe::CShapeTestCapsuleDesc probeData2;
				probeData2.SetResultsStructure( &m_seekModeLosTestHandle2 );
				probeData2.SetCapsule( VEC3V_TO_VECTOR3( vStartKneeApprox ), VEC3V_TO_VECTOR3( vEndKneeApprox ), fCapsuleRadius );
				probeData2.SetContext( WorldProbe::EMelee );
				probeData2.SetIncludeFlags( ArchetypeFlags::GTA_MELEE_VISIBILTY_TYPES );
				probeData2.SetIsDirected( true );
				probeData2.SetExcludeEntities( ppExcludeEnts, iNumExceptions );
				WorldProbe::GetShapeTestManager()->SubmitTest( probeData2, WorldProbe::PERFORM_ASYNCHRONOUS_TEST );
			}

			// Set our tracking flag. 
			m_bSeekModeLosTestHasBeenRequested = true;
		}
	}

	if( m_bSeekModeLosTestHasBeenRequested )
	{
		bool bLosReady = false;
		if( pPed->GetPedType() == PEDTYPE_ANIMAL )
			bLosReady = !m_seekModeLosTestHandle1.GetWaitingOnResults();
		else
			bLosReady = !m_seekModeLosTestHandle1.GetWaitingOnResults() && !m_seekModeLosTestHandle2.GetWaitingOnResults();
	
		if( bLosReady )
		{
			m_seekModeLosTestNextRequestTimer.Set( fwTimer::GetTimeInMilliseconds(), sm_nAiLosTestFreqInMs );
			m_bSeekModeLosTestHasBeenRequested = false;

			bool bHit1 = false;
			if( m_seekModeLosTestHandle1.GetResultsReady() )
			{
				bHit1 = m_seekModeLosTestHandle1.GetNumHits() > 0u;
				m_seekModeLosTestHandle1.Reset();
			}

			bool bHit2 = false;
			if( m_seekModeLosTestHandle2.GetResultsReady() )
			{
				bHit2 = m_seekModeLosTestHandle2.GetNumHits() > 0u;
				m_seekModeLosTestHandle2.Reset();
			}

			bool bPreviousValue = m_bLosBlocked;
			m_bLosBlocked = bHit1 || bHit2;

			if( m_bLosBlocked != bPreviousValue )
				CalculateDesiredTargetDistance();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessUnreachableNavMeshTest
// PURPOSE :	Heartbeat for the unreachable nav mesh queries
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::ProcessUnreachableNavMeshTest( CPed* pPed, CEntity* pTargetEntity )
{
	static dev_float sfArriveRadius = 2.0f;
	if( !m_RouteSearchHelper.IsSearchActive() )
	{
		if( pPed && pTargetEntity )
		{
			u32 iPathFlags = PATH_FLAG_NEVER_DROP_FROM_HEIGHT|PATH_FLAG_DONT_LIMIT_SEARCH_EXTENTS|PATH_FLAG_DONT_AVOID_DYNAMIC_OBJECTS|
							 PATH_FLAG_NEVER_ENTER_WATER|PATH_FLAG_NEVER_START_IN_WATER|PATH_FLAG_AVOID_POTENTIAL_EXPLOSIONS;

			// Find the radius that we want to use
			float fTargetRadius = sfArriveRadius;
			if( IsLosBlocked() )
			{
				fTargetRadius = sm_fAiSeekModeActiveCombatantRadius;
			}
			else
			{
				const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
				if(pBrawlData)
				{
					fTargetRadius = pBrawlData->GetTargetRadius();
				}
			}

		m_RouteSearchHelper.StartSearch( pPed, VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ), VEC3V_TO_VECTOR3(  pTargetEntity->GetTransform().GetPosition() ), fTargetRadius, iPathFlags );
			taskAssertf( m_RouteSearchHelper.IsSearchActive(), "CTaskMelee::ProcessUnreachableNavMeshTest - Inactive route search helper" );
		}
	}
	else
	{
		float fDistance = 0.0f;
		Vector3 vLastPoint(0.0f, 0.0f, 0.0f);
		SearchStatus eSearchStatus;

		// get our search results to get the status, total distance and last point
		if( m_RouteSearchHelper.GetSearchResults( eSearchStatus, fDistance, vLastPoint ) )
		{
			if( eSearchStatus == SS_SearchSuccessful )
				ClearTaskFlag( MF_CannotFindNavRoute );
			else
				SetTaskFlag( MF_CannotFindNavRoute );

			// Clear its active status
			m_RouteSearchHelper.ResetSearch();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CheckDesiredSubtaskAndUpdateBreakoutConditions
// PURPOSE :	Update and determine if we should still be running the current
//				subtask, another, or quiting.
// PARAMETERS :	pPed -  the ped we are interested in.
// RETURNS :	What the ped should be doing.
///////////////////////////////////////////////////////////////////////////////// 
CTaskMelee::MeleeState CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMelee::CheckDesiredSubtaskAndUpdateBreakoutConditions." );

	Assertf( !pPed->IsNetworkClone(), "GetDesiredStateAndUpdateBreakoutConditions does not support clone peds!" );

	// Check if someone has already requested a "stop all" or is in water
	if( m_bStopAll )
	{
		m_bStopAll = false;
		taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: m_bStopAll");
		return State_Finish;
	}

	const CPedWeaponManager* pPedWeaponManager = pPed->GetWeaponManager();
	const CObject* pHeldWeaponObject = pPedWeaponManager ? pPedWeaponManager->GetEquippedWeaponObject() : NULL;
	const CWeapon* pHeldWeapon = pHeldWeaponObject ? pHeldWeaponObject->GetWeapon() : NULL;
	if( !pHeldWeapon )
		pHeldWeapon = CWeaponManager::GetWeaponUnarmed( pPed->GetDefaultUnarmedWeaponHash() );

	const CWeaponInfo* pHeldWeaponInfo =  pHeldWeapon ? pHeldWeapon->GetWeaponInfo() : NULL;

	u32 eCurrentState = GetState();
	CEntity* pTargetEntity = GetTargetEntity();
	if( !GetIsTaskFlagSet( MF_ShouldBeInMeleeMode ) )
	{
		if( !GetIsTaskFlagSet( MF_AttemptIdleControllerInterrupt ) )
		{
			if( pHeldWeaponInfo && pHeldWeaponInfo->GetIsMelee() )
			{
				CTaskMotionBase* pPrimaryTask = pPed->GetPrimaryMotionTask();
				if( pPrimaryTask && pPrimaryTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED )
				{
					// Grab the current weapon info and see if we should allow a strafe to on foot transition
					CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(pPrimaryTask);
					Assert( pTask );
					pTask->SetTaskFlag( CTaskMotionPed::PMF_PerformStrafeToOnFootTransition );
				}
			}
		}

		taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: !MF_ShouldBeInMeleeMode");
		return State_Finish;
	}

	bool bLocalPlayerInUI = pPed->IsLocalPlayer() && CNewHud::IsShowingHUDMenu();

	if( m_pNextActionResult)
	{
		bool bOffensiveMoveInUI = m_pNextActionResult->IsOffensiveMove() && bLocalPlayerInUI;
		if(!bOffensiveMoveInUI)
		{
			return State_MeleeAction;
		}
	}

	CTask* pSubTask = GetSubTask();
	bool bRunningAction = pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT;
	CTaskMeleeActionResult* pActionResultTask = NULL;
	if( bRunningAction )
	{
		pActionResultTask = static_cast<CTaskMeleeActionResult*>( pSubTask );

		if (pSubTask->GetIsFlagSet(aiTaskFlags::HasBegun))
		{
			// Check if we should break out due to swimming		
			if( pActionResultTask->ShouldInterruptWhenOutOfWater() )
			{
				if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) && !pPed->GetIsFPSSwimming() )
				{
					taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: !CPED_CONFIG_FLAG_SwimmingTasksRunning");
					return State_Finish;
				}
			}
			else if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SwimmingTasksRunning ) || pPed->GetIsFPSSwimming() )
			{
				taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: CPED_CONFIG_FLAG_SwimmingTasksRunning");
				return State_Finish;
			}
		}		
	}

	// If we are not running an action or should allow an interrupt
	if( !bLocalPlayerInUI && (!bRunningAction || ( pActionResultTask && pActionResultTask->ShouldAllowInterrupt() ) )) 
	{
		const CPedEquippedWeapon* pEquippedWeapon = pPedWeaponManager ? pPedWeaponManager->GetPedEquippedWeapon() : NULL;
		if( !pPed->IsNetworkClone() && pEquippedWeapon && pPedWeaponManager->GetRequiresWeaponSwitch() )
		{
			const CWeaponInfo* pEquippedWeaponInfo = pEquippedWeapon->GetWeaponInfo();
			// Only attempt to switch if the weapon anims are streamed in
			if( pEquippedWeaponInfo && pEquippedWeapon->AreWeaponAnimsStreamedIn() && pEquippedWeaponInfo->GetIsMelee() )
			{
				return State_SwapWeapon;
			}
		}
	}
	
	if( pPed->IsPlayer() )
	{
		// We allow hot swapping during a melee move
		if( !pPed->IsAPlayerPed() )
		{
			taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: !pPed->IsAPlayerPed()");
			return State_Finish;
		}

		const bool bIsCurrentlyDoingMove = IsCurrentlyDoingAMove( false, false, true );
		if( !bIsCurrentlyDoingMove && !pHeldWeaponInfo )
		{
			taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: !bIsCurrentlyDoingMove && !pHeldWeaponInfo");
			return State_Finish;
		}

		if( !bIsCurrentlyDoingMove )
		{
			// Check if they are locked onto a target
			if( HasLockOnTargetEntity() )
			{
				Vector3 vPlayerToTarget = VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition() );
				if( vPlayerToTarget.Mag2() > rage::square( pHeldWeaponInfo->GetLockOnRange() ) )
				{
					taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: vPlayerToTarget.Mag2() (%.2f) > rage::square( pHeldWeaponInfo->GetLockOnRange() ) (%.2f)", vPlayerToTarget.Mag2(), rage::square( pHeldWeaponInfo->GetLockOnRange() ));
					return State_Finish;
				}
			}
			// Special case where we are forcing strafe but still want them to be able to run away
			else if( GetIsTaskFlagSet( MF_AttemptRunControllerInterrupt ) || GetIsTaskFlagSet( MF_AttemptVehicleEnterInterrupt ) )
			{
				taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: GetIsTaskFlagSet( MF_AttemptRunControllerInterrupt ) (%d) || GetIsTaskFlagSet( MF_AttemptVehicleEnterInterrupt ) (%d)", GetIsTaskFlagSet( MF_AttemptRunControllerInterrupt ), GetIsTaskFlagSet( MF_AttemptVehicleEnterInterrupt ));
				return State_Finish;
			}
		}

		// check to see if we should interrupt the current move and go back to player movement
		if( pActionResultTask && pActionResultTask->ShouldAllowInterrupt() )
		{
			// Some moves allow aim interrupt (like pistol whips)
			const bool bAimInterupt = pActionResultTask->ShouldAllowAimInterrupt() && ( CPlayerInfo::IsAiming() || CPlayerInfo::IsFiring_s() ) && !pHeldWeapon->GetHasFirstPersonScope();
			if( bAimInterupt || 
				GetIsTaskFlagSet( MF_AttemptRunControllerInterrupt ) || 
				GetIsTaskFlagSet( MF_AttemptVehicleEnterInterrupt ) || 
				GetIsTaskFlagSet( MF_AttemptDiveInterrupt ) ||
				GetIsTaskFlagSet( MF_AttemptCoverEnterInterrupt ) )
			{
				taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: bAimInterupt %d, MF_AttemptRunControllerInterrupt %d, MF_AttemptVehicleEnterInterrupt %d, MF_AttemptDiveInterrupt %d, MF_AttemptCoverEnterInterrupt %d", bAimInterupt, GetIsTaskFlagSet( MF_AttemptRunControllerInterrupt ), GetIsTaskFlagSet( MF_AttemptVehicleEnterInterrupt ), GetIsTaskFlagSet( MF_AttemptDiveInterrupt ), GetIsTaskFlagSet( MF_AttemptCoverEnterInterrupt ));
				return State_Finish;
			}

#if FPS_MODE_SUPPORTED
			bool bIsFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer( false );
			const crClip* pClip = pActionResultTask->GetClip();
#endif

			// Check to see if we have a target and only interrupt if player is clearly trying to move the other direction
			if( GetIsTaskFlagSet( MF_AttemptMeleeControllerInterrupt ) )
			{
				bool bAllowInterrupt = true;
#if FPS_MODE_SUPPORTED
				if( bIsFPSMode && pActionResultTask->IsDoingReaction() && pClip )
				{
					CTaskMotionBase* pTaskMotionBase = pPed->GetCurrentMotionTask( false );
					if( pTaskMotionBase )
					{
						TUNE_GROUP_FLOAT(CAM_FPS, fMeleeReactionInterruptPhase, 0.75f, 0.0f, 1.0f, 0.01f);
						TUNE_GROUP_FLOAT(CAM_FPS, fMeleeReactionInterruptHeading, 30.0f, 0.0f, 180.0f, 1.0f);
						float fCurrentPhase = pClip->ConvertTimeToPhase(pActionResultTask->GetCurrentClipTime());
						float fDesiredDirection = pTaskMotionBase->CalcDesiredDirection();
						// If we haven't yet reached our reaction interrupt phase and haven't yet reached our desired direction...
						if( fCurrentPhase < fMeleeReactionInterruptPhase && Abs( fDesiredDirection ) > (fMeleeReactionInterruptHeading * DtoR) )
						{
							Quaternion qRotationDiff = fwAnimHelpers::GetMoverTrackRotationDiff( *pClip, fCurrentPhase, fMeleeReactionInterruptPhase );
							float fHeadingChangeRemaining = qRotationDiff.TwistAngle( ZAXIS );

							// If there is still a decent amount of remaining animated rotation before our interrupt phase then don't allow the interrupt
							if( Abs( fHeadingChangeRemaining ) > ( fMeleeReactionInterruptHeading * DtoR ) )
							{
								bAllowInterrupt = false;
							}
						}
					}
				}
#endif

				if( HasLockOnTargetEntity() )
				{
					if( GetIsTaskFlagSet( MF_QuitTaskAfterNextAction ) )
					{
						taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: MF_QuitTaskAfterNextAction");
						return State_Finish;
					}

					return State_WaitForMeleeAction;
				}
			
				if( GetIsTaskFlagSet( MF_ReactionResetTimeInTaskAfterNextAction ) || GetIsTaskFlagSet( MF_AttackResetTimeInTaskAfterNextAction ) )
					return State_WaitForMeleeAction;
				else if( bAllowInterrupt )
				{
					taskDebugf1("Quit: CTaskMelee::GetDesiredStateAndUpdateBreakoutConditions: MF_ReactionResetTimeInTaskAfterNextAction %d, MF_AttackResetTimeInTaskAfterNextAction %d", GetIsTaskFlagSet( MF_ReactionResetTimeInTaskAfterNextAction ), GetIsTaskFlagSet( MF_AttackResetTimeInTaskAfterNextAction ));
					return State_Finish;
				}
			}

#if FPS_MODE_SUPPORTED
			if( bIsFPSMode && !IsBlocking() && !IsUsingStealthClips() && !pActionResultTask->IsDoingReaction() )
			{
				if( pClip )
				{
					const crTag* pTag = CClipEventTags::FindLastEventTag(pClip, CClipEventTags::Interruptible);
					TUNE_GROUP_FLOAT(CAM_FPS, fMeleeInterruptTime, 0.3f, 0.0f, 5.0f, 0.05f);
					// Allow some time for combos
					if( pTag && (pClip->ConvertPhaseToTime(pTag->GetStart()) + fMeleeInterruptTime) < pActionResultTask->GetCurrentClipTime() )
					{
						return State_Finish;
					}
				}
			}
#endif
		}
	}
	else 
	{
		// Note: This is really one of the major parts of our "Melee Combat AI"...
		// Search for the quoted term in the line above to find the others.
		if( eCurrentState == State_WaitForTarget || eCurrentState == State_LookAtTarget )
			return State_Invalid;

		if( IsCurrentlyDoingAMove( GetIsTaskFlagSet( MF_PlayTauntBeforeAttacking ) || ShouldBeInInactiveTauntMode(), GetIsTaskFlagSet( MF_PerformIntroAnim ), !GetIsTaskFlagSet( MF_QuitTaskAfterNextAction ) ) )
			return State_Invalid;

		if( pTargetEntity && pTargetEntity->GetIsTypePed() )
		{	
			// Possible filter the seek/flee desires if the peds movement is constrained.
			QueueType queueType = GetQueueType();
			if( queueType <= QT_InactiveObserver && eCurrentState != State_InactiveObserverIdle )
				return State_InactiveObserverIdle;

			bool bCanSeek = queueType > QT_InactiveObserver && !GetIsTaskFlagSet( MF_ForceStrafe ) && !GetIsTaskFlagSet( MF_PlayTauntBeforeAttacking );

			// Don't allow any seek/flee when ped is out side their defensive area
			const CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence() ? pPed->GetPedIntelligence()->GetDefensiveArea() : NULL;
			if( pDefensiveArea && pDefensiveArea->IsActive() && !pDefensiveArea->IsPointInDefensiveArea( VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() ) ) )
				bCanSeek = false;

			// Don't allow any seek/flee when ped is stationary
			if( pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_Stationary )
				bCanSeek = false;

			// Check to see if we should attempt 
			if( bCanSeek )
			{
				Vec3V vDelta = pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition();
				if( ShouldBeInInactiveTauntMode() )
				{
					// check to see if they are entering a vehicle
					CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );
					
					bool bWithinRetreatDistance = false;
					const float fDistanceSq = MagSquared( vDelta ).Getf();
					if( fDistanceSq > rage::square( sm_fAiSeekModeRetreatDistanceMin ) && fDistanceSq < rage::square( sm_fAiSeekModeRetreatDistanceMax ) )
						bWithinRetreatDistance = true;

					if( bWithinRetreatDistance || pTargetPed->GetVehiclePedInside() != NULL || ( pTargetPed->GetPedIntelligence() && pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE_SEAT ) ) )
						return State_WaitForTarget;
					else 
						return State_RetreatToSafeDistance;
				}
				else if( IsLosBlocked() )
					return State_ChaseAfterTarget;
				else
				{
					float fChaseThreshold =  GetDesiredTargetDistance() + sm_fAiSeekModeChaseTriggerOffset;
					Vec3V vDeltaXY( vDelta.GetXf(), vDelta.GetYf(), 0.0f );
					const float fDist2 = MagSquared( vDeltaXY ).Getf();

					// First check target height
					if( Abs( vDelta.GetZf() ) > sm_fAiSeekModeTargetHeight || fDist2 > rage::square( fChaseThreshold ) )
						return State_ChaseAfterTarget;
				}
			}
		}
	}

	// Let the caller know everything seems fine.
	return State_Invalid;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateTargetFlags
// PURPOSE :	Update the target flags for other functions to use  
// PARAMETERS :	pPed -  the ped we are interested in.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::UpdateTargetFlags( CPed* pPed )
{
	bool bIsPlayer = pPed->IsPlayer();

	// Lock on conditions
	bool bHasLockOnTarget = false;
	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity && ( !bIsPlayer || m_bHasLockOnTargetEntity ) )
		bHasLockOnTarget = true;

	if( bHasLockOnTarget )
	{
		SetTaskFlag( MF_HasLockOnTarget );
		ClearTaskFlag( MF_ForceStrafeIndefinitely );
	}
	else
		ClearTaskFlag( MF_HasLockOnTarget );	

	bool bStrafeTimer = m_forceStrafeTimer.IsEnabled() && !m_forceStrafeTimer.IsOutOfTime() FPS_MODE_SUPPORTED_ONLY(&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(false));

	// Purposely do not check whether the timer is set
	if( GetIsTaskFlagSet( MF_AnimSynced ) || GetIsTaskFlagSet( MF_ForceStrafeIndefinitely ) || pPed->GetPedResetFlag( CPED_RESET_FLAG_ForcePedToStrafe ) || ( bStrafeTimer ) )
		SetTaskFlag( MF_ForceStrafe );
	else 
		ClearTaskFlag( MF_ForceStrafe );

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && bHasLockOnTarget)
	{
		SetTaskFlag( MF_ForceStrafe );
	}
#endif

	// Should we force strafe based on movement and target distance
	Vec3V vPedToTarget( VEC3_ZERO );
	ScalarV scDistToTargetSq( 0.0f );
	if( pTargetEntity )
	{
		vPedToTarget = pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition();
		scDistToTargetSq = DistSquared( pTargetEntity->GetTransform().GetPosition(), pPed->GetTransform().GetPosition() );
	}

	// Is ped currently running or going to perform a move?
	const bool bIsCurrentlyDoingAMove = m_pNextActionResult || IsCurrentlyDoingAMove( false );

	// Determine whether or not the player is attempting to move in a direction opposite to the peds forward vector
	if( pPed->IsLocalPlayer() )
	{
		bool bAttemptIdleControllerInterrupt = false;
		bool bAttemptRunControllerInterrupt = false;
		bool bAttemptMeleeControllerInterrupt = false;
		bool bAttemptVehicleEnterInterrupt = false;
		bool bAttemptCoverEnterInterrupt = false;
		bool bAttemptDiveInterrupt = false;

		const CControl* pControl = pPed->GetControlFromPlayer();
		Assert( pControl );
		const Vector2 vStickInput( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm() );
		const float fInputMag = vStickInput.Mag2();

		// Check vehicle enter interrupt
		if( pControl->GetPedEnter().IsPressed() )
		{
			CVehicle * pTargetVehicle = CPlayerInfo::ScanForVehicleToEnter( pPed, VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() ), false );
			if( pTargetVehicle )
				bAttemptVehicleEnterInterrupt = true;
		}

		if( bAttemptVehicleEnterInterrupt )
			SetTaskFlag( MF_AttemptVehicleEnterInterrupt );
		else
			ClearTaskFlag( MF_AttemptVehicleEnterInterrupt );

		// Check cover enter interrupt
		if( pControl->GetPedCover().IsPressed() )
		{
			CTaskPlayerOnFoot* pTaskPlayerOnFoot = static_cast<CTaskPlayerOnFoot*>( pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_PLAYER_ON_FOOT ) );
			if( pTaskPlayerOnFoot && pTaskPlayerOnFoot->CheckForNearbyCover( pPed ) )
				bAttemptCoverEnterInterrupt = true;
		}

		if( bAttemptCoverEnterInterrupt )
			SetTaskFlag( MF_AttemptCoverEnterInterrupt );
		else
			ClearTaskFlag( MF_AttemptCoverEnterInterrupt );

		// Check dive interrupt
		if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) && pControl->GetPedDive().IsPressed() )
			bAttemptDiveInterrupt = true;
		if( bAttemptDiveInterrupt )
			SetTaskFlag( MF_AttemptDiveInterrupt );
		else
			ClearTaskFlag( MF_AttemptDiveInterrupt );

		static dev_float s_fMinInterruptStickValueSq = 0.5f;
		if( fInputMag > s_fMinInterruptStickValueSq )
		{
			bAttemptIdleControllerInterrupt = true;

			bool bAllowRunInterrupt = !bHasLockOnTarget;
			if (NetworkInterface::IsInCopsAndCrooks() && pTargetEntity && pTargetEntity->GetIsTypePed())
			{
				const CPed* pTargetPed = static_cast<const CPed*>(pTargetEntity);
				if (pTargetPed->GetUsingRagdoll())
				{
					bAllowRunInterrupt = true;
				}
			}

			bAttemptRunControllerInterrupt = bAllowRunInterrupt && (pControl->GetPedSprintIsDown( ) || pControl->GetPedSprintHistoryIsPressed( 300 ));

			// Do not allow the movement interrupt unless we no longer have a pending impulse or the last impulse was recorded some time ago
			if( !m_impulseCombo.HasPendingImpulse() || ( m_impulseCombo.GetLatestRecordedImpulseTime() + CTaskMelee::sm_nPlayerImpulseInterruptDelayInMs ) <= fwTimer::GetTimeInMilliseconds() )
				bAttemptMeleeControllerInterrupt = GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT && !static_cast<CTaskMeleeActionResult*>(GetSubTask())->IsUpperbodyOnly();
		}

	#if FPS_MODE_SUPPORTED
		m_bInterruptForFirstPersonCamera = false;
		TUNE_GROUP_FLOAT(CAM_FPS, c_FirstPersonTurnThresholdInterrupt, 10.0f, 0.0f, 45.0f, 0.05f);
		const camFirstPersonShooterCamera* fpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) &&
			fpsCamera != NULL &&
			Abs(fpsCamera->GetRelativeHeading(true)) > c_FirstPersonTurnThresholdInterrupt*DtoR )
		{
			m_bInterruptForFirstPersonCamera = true;
		}
	#endif

		// Special case code when we are within 4m
		if( !bIsCurrentlyDoingAMove )
		{
			float fDistToTarget = MagSquared( vPedToTarget ).Getf();
			if( fDistToTarget < rage::square( sm_fForcedStrafeRadius ) )
			{
				// Stop the force strafe timer while we are strafing
				if( bAttemptIdleControllerInterrupt )
				{
					if( m_forceStrafeTimer.IsEnabled() && !m_forceStrafeTimer.IsPaused() )
						m_forceStrafeTimer.Pause();
				}
				// If we previously stopped the strafe timer then reset to minimum time
				else if( m_forceStrafeTimer.IsEnabled() && m_forceStrafeTimer.IsPaused() )
					m_forceStrafeTimer.Set( fwTimer::GetTimeInMilliseconds(), sm_nTimeInTaskAfterStardardAttack );
			}
			else 
			{
				if( m_forceStrafeTimer.IsEnabled() && m_forceStrafeTimer.IsPaused() )
					m_forceStrafeTimer.Set( fwTimer::GetTimeInMilliseconds(), 0 );

				ClearTaskFlag( MF_ForceStrafeIndefinitely );
			}
		}

		if( bAttemptIdleControllerInterrupt )
			SetTaskFlag( MF_AttemptIdleControllerInterrupt );
		else
			ClearTaskFlag( MF_AttemptIdleControllerInterrupt );

		if( bAttemptRunControllerInterrupt )
			SetTaskFlag( MF_AttemptRunControllerInterrupt );
		else
			ClearTaskFlag( MF_AttemptRunControllerInterrupt );

		if( bAttemptMeleeControllerInterrupt )
			SetTaskFlag( MF_AttemptMeleeControllerInterrupt );
		else
			ClearTaskFlag( MF_AttemptMeleeControllerInterrupt );
	}

	bool bShouldResetTimeInTask = false;
	if( pTargetEntity && pTargetEntity->GetIsTypePed() )
	{
		bool bTargetIsUnreachable = false;
		CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );
		if( !pPed->IsLocalPlayer() )
		{
			static dev_float sfPhysicalHeightCutoff = -1.15f;
			bool bOnTopOfUnreachablePhysical = false;
			CPhysical* pPhysical = pTargetPed->GetGroundPhysical();
			if( pPhysical )
			{
				if( pPhysical->GetIsTypeVehicle() )
					bOnTopOfUnreachablePhysical = true;
				else
				{
					// Check to see if this is an object and is unreachable
					bool bIsUnreachableByMeleeNavigation = false;
					if( pPhysical->GetIsTypeObject() )
					{
						const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry( pPhysical->GetBaseModelInfo()->GetModelNameHash() );
						if( pTuning )
							bIsUnreachableByMeleeNavigation = pTuning->GetIsUnreachableByMeleeNavigation();
					}

					// Is this a hard coded object that is unreachable?
					if( bIsUnreachableByMeleeNavigation )
						bOnTopOfUnreachablePhysical = true;
					// Otherwise test the physical position difference
					else
					{					
						Vec3V vPositionDiff = pPed->GetTransform().GetPosition() - pTargetPed->GetTransform().GetPosition();
						if( vPositionDiff.GetZf() < sfPhysicalHeightCutoff )
							bOnTopOfUnreachablePhysical = true;
					}
				}
			}

			bTargetIsUnreachable = bOnTopOfUnreachablePhysical ||
								   pTargetPed->GetIsSwimming() || 
								   pTargetPed->GetPedResetFlag( CPED_RESET_FLAG_IsDiving ) || 
								   pTargetPed->GetPedResetFlag( CPED_RESET_FLAG_IsVaulting );

			bTargetIsUnreachable |= ( pTargetPed->GetPlayerWanted() && pTargetPed->GetPlayerWanted()->m_EverybodyBackOff );
			
			// in or entering vehicle
			bTargetIsUnreachable |= ( pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE_SEAT ) || pTargetPed->GetVehiclePedInside() != NULL );

			// on a ladder
			bTargetIsUnreachable |= pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_CLIMB_LADDER );
		}

		if( bTargetIsUnreachable )
		{
			SetTaskFlag( MF_TargetIsUnreachable );	
			pPed->SetPedResetFlag( CPED_RESET_FLAG_IsMeleeTargetUnreachable, true );
		}
		else
		{
			// If we were previously unreachable then lets reset the seek probe to happen now
			if( GetIsTaskFlagSet( MF_TargetIsUnreachable ) )
				m_seekModeLosTestNextRequestTimer.ChangeDuration( 0 );

			ClearTaskFlag( MF_TargetIsUnreachable );
		}

		// Let the movement task know what the status of the target is
		CTask* pMovementTask = pPed ? pPed->GetPedIntelligence()->GetGeneralMovementTask() : NULL;
		if( pMovementTask && pMovementTask->GetTaskType() == CTaskTypes::TASK_MOVE_MELEE_MOVEMENT )
		{
			CTaskMoveMeleeMovement* pMeleeMoveTask = static_cast<CTaskMoveMeleeMovement*>( pMovementTask );
			pMeleeMoveTask->SetAllowMovement( !ShouldBeInInactiveTauntMode() && !GetIsTaskFlagSet( MF_PlayTauntBeforeAttacking ) );
			pMeleeMoveTask->SetMoveWhenTargetEnteringVehicle(true);

			if( !bIsPlayer && pTargetPed->GetIsEnteringVehicle() )
			{
				ScalarV scMaxDistToStopMovementSq = LoadScalar32IntoScalarV( square( GetDesiredTargetDistance() ) );
				if( IsLessThanOrEqualAll( scDistToTargetSq, scMaxDistToStopMovementSq ) )
					pMeleeMoveTask->SetMoveWhenTargetEnteringVehicle(false);
			}
		}	

		bool bShouldAllowStrafeMode = false;
		bool bShouldAllowStealthMode = false;
		if( bIsPlayer )
		{
			bool bTargetIsInCombat = pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COMBAT );

			// Is the player behind the target ped?
			vPedToTarget = Normalize( vPedToTarget );
			float fPlayerToTargetDirection = Dot( vPedToTarget, pTargetEntity->GetTransform().GetB() ).Getf();
			if( fPlayerToTargetDirection > CTaskMelee::sm_fFrontTargetDotThreshold )
				SetTaskFlag( MF_BehindTargetPed );
			else 
				ClearTaskFlag( MF_BehindTargetPed );

			// Reset time in task under specific conditions
			bool bDontRaiseFistsFlagSet = pTargetPed->GetPedResetFlag( CPED_RESET_FLAG_DontRaiseFistsWhenLockedOn );
			if( !bIsCurrentlyDoingAMove )
			{
				if( bDontRaiseFistsFlagSet )
					bShouldResetTimeInTask = !GetIsTaskFlagSet( MF_ForceStrafe );
				else if( bHasLockOnTarget || pTargetPed->IsDead() )
					bShouldResetTimeInTask = true;
			}

			// Conditions for allowing strafe mode
			if( bTargetIsInCombat )
				bShouldAllowStrafeMode = true;
			else if( bDontRaiseFistsFlagSet )
			{
				bShouldAllowStrafeMode = ( pPed->GetMotionData()->GetUsingStealth() && GetIsTaskFlagSet( MF_BehindTargetPed ) ) || GetIsTaskFlagSet( MF_ForceStrafe );
				bShouldAllowStealthMode = GetIsTaskFlagSet( MF_BehindTargetPed );
			}
			else
			{
				bShouldAllowStealthMode = GetIsTaskFlagSet( MF_BehindTargetPed );
	
				// If the player is walking away from you and is moving then we have special conditions
				float fTargetMoveBlendRatioSq = pTargetPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
				if( fPlayerToTargetDirection > CTaskMelee::sm_fFrontTargetDotThreshold && fTargetMoveBlendRatioSq > rage::square( CActionManager::ms_fMBRWalkBoundary ) )
				{
					if ( GetIsTaskFlagSet( MF_AllowStrafeMode ) )
					{
						if( scDistToTargetSq.Getf() < rage::square( sm_fMovingTargetStrafeBreakoutRadius ) )
							bShouldAllowStrafeMode = true;
					}
					else
					{
						if( scDistToTargetSq.Getf() < rage::square( sm_fMovingTargetStrafeTriggerRadius ) )
							bShouldAllowStrafeMode = true;
					}
				}
				else if( scDistToTargetSq.Getf() < rage::square( sm_fForcedStrafeRadius ) )
					bShouldAllowStrafeMode = true;
			}

			//! Allow targeting to pick a new target if out current target is outside strafe range. This prevent initially locking
			//! onto a target that is far away, when another better target is closer.
			if( pPed->IsLocalPlayer() && (scDistToTargetSq.Getf() >= rage::square( sm_fForcedStrafeRadius )))
			{
				CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();
				if(rTargeting.GetLockOnTarget() == pTargetEntity && 
					!rTargeting.HasTargetSwitchedToLockOnTarget() &&
					rTargeting.GetTimeLockedOn() < sm_fReTargetTimeIfNotStrafing)
				{
					CEntity *pNewTargetEntity = rTargeting.FindLockOnMeleeTarget(pPed);
					if(pNewTargetEntity)
					{
						ScalarV scDistToNewTargetSq( 0.0f );
						scDistToNewTargetSq = DistSquared( pNewTargetEntity->GetTransform().GetPosition(), pPed->GetTransform().GetPosition() );
						if(scDistToNewTargetSq.Getf() < rage::square( sm_fForcedStrafeRadius ))
						{
							SetTargetEntity(pNewTargetEntity, true, true);
						}
					}
				}
			}
		}
		else
			bShouldAllowStrafeMode = ( GetQueueType() >= QT_InactiveObserver && GetIsTaskFlagSet( MF_HasPerformedInitialPursuit ) );

		if( pTargetPed->IsDead() )
			ClearTaskFlag( MF_ForceStrafeIndefinitely );

		if ( GetIsTaskFlagSet( MF_ForceStrafe ) || bShouldAllowStrafeMode )
			SetTaskFlag( MF_AllowStrafeMode );
		else
			ClearTaskFlag( MF_AllowStrafeMode );

		if ( bShouldAllowStealthMode )
			SetTaskFlag( MF_AllowStealthMode );
		else
		{
			// If we were previously allowing stealth mode there are a few cases we want to toggle it off
			if( bIsPlayer && !bIsCurrentlyDoingAMove && GetIsTaskFlagSet( MF_AllowStealthMode ) && pPed->GetMotionData()->GetUsingStealth() )
				pPed->GetMotionData()->SetUsingStealth( false );

			ClearTaskFlag( MF_AllowStealthMode );
		}
	}

	// Only reset the force strafe if it is enabled and has been evaluated to
	if( m_forceStrafeTimer.IsEnabled() && bShouldResetTimeInTask )
		m_forceStrafeTimer.Disable();

	bool bShouldBeInMeleeMode = false;
	
	if( bIsCurrentlyDoingAMove || pPed->IsNetworkClone() || GetIsTaskFlagSet( MF_ForceStrafe ) )
		bShouldBeInMeleeMode = true;
	else if( bHasLockOnTarget )
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		const bool bIsUnarmedOrHoldingMeleeWeapon = (!pWeaponInfo || pWeaponInfo->GetIsMelee() );
		if( bIsUnarmedOrHoldingMeleeWeapon )
			bShouldBeInMeleeMode = true;
	}

	if( bShouldBeInMeleeMode )
		SetTaskFlag( MF_ShouldBeInMeleeMode );
	else
		ClearTaskFlag( MF_ShouldBeInMeleeMode );		
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessPreFSM
// PURPOSE	:	Runs before the main CTask update loop (UpdateFSM)
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMelee::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	DEV_BREAK_IF_FOCUS( CCombatMeleeDebug::sm_bFocusPedBreakOnTaskCombatMeleeProcessPreFSM, pPed );

	// Make any streaming clip requests.
	MakeStreamingClipRequests( pPed );

	// Make sure the movement system knows we are doing melee.
	pPed->SetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_EnableSteepSlopePrevention, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_StreamActionModeAnimsIfDisabled, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_BlockSecondaryAnim, true );

	if( !pPed->IsNetworkClone() )
		UpdateTarget( pPed );

#if __BANK
	if(pPed->IsNetworkClone() && CCombatMeleeDebug::sm_bDrawClonePos)
	{
		//static dev_float s_fZOffset = 1.25f;
		static dev_float s_fRadius = 0.1f;
		static dev_float s_fArrowSize = 0.5f;
		Vector3 vCloneMasterPosition = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
		CCombatMeleeDebug::sm_debugDraw.AddSphere( VECTOR3_TO_VEC3V(vCloneMasterPosition), s_fRadius, Color_green, 1 );

		float fCloneHeading = NetworkInterface::GetLastHeadingReceivedOverNetwork(pPed);
		Vec3V vDesiredHeading( -rage::Sinf( fCloneHeading ), rage::Cosf( fCloneHeading ), 0.0f );

		CCombatMeleeDebug::sm_debugDraw.AddArrow( VECTOR3_TO_VEC3V(vCloneMasterPosition), ( VECTOR3_TO_VEC3V(vCloneMasterPosition) + vDesiredHeading ), s_fArrowSize, Color_green, 1 );
	}
#endif

	// Update the melee target for the ped.
	UpdateTargetFlags( pPed );
	
	// Force the ped to strafe
	if( GetIsTaskFlagSet( MF_ForceStrafe ) )
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePedToStrafe, true );

	// Set this flag every frame when in combat, so the target ped won't get switched to ragdoll by the player bumping into them.
 	CEntity* pTargetEntity = GetTargetEntity();
 	if( pTargetEntity && pTargetEntity->GetIsTypePed() && static_cast<CPed*>(pTargetEntity)->GetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning ) )
 		static_cast<CPed*>(pTargetEntity)->SetPedResetFlag( CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true );

	// Update any ik.
	// Set up the spine look IK parameters.
	UpdateHeadAndSpineLooksAndLegIK( pPed );

	if( pPed->IsPlayer() )
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		s32 nState = GetState();

		// Reset the impulse combo every frame 
		if( nState == State_WaitForMeleeAction )
			ResetImpulseCombo();

		if( nState == State_MeleeAction || 
			nState == State_WaitForMeleeAction )
		{
			m_impulseCombo.ScanForImpulses(pControl);
		}
	}
	// AI only pre-process update 
	else if( !pPed->IsNetworkClone() )
	{
		// Continually register the opponent in case a slot opens up
		RegisterMeleeOpponent( pPed, pTargetEntity );

		// Process the LOS tests to make sure we can see the target
		QueueType pedQueueType = GetQueueType();
		if( pedQueueType > QT_InactiveObserver )
			ProcessLineOfSightTest( pPed );
	}

	return FSM_Continue;
}

void CTaskMelee::ProcessPreRender2()
{
#if FPS_MODE_SUPPORTED
	CPed* pPed = GetPed();
	if( !pPed )
	{
		return;
	}

	if( pPed->IsFirstPersonShooterModeEnabledForPlayer( false ) && IsBlocking() && !fwTimer::IsGamePaused() )
	{
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
		if( pWeaponInfo && ( !pWeaponInfo->GetBlockLeftHandIKWhileAiming(*pPed) || pPed->GetMotionData()->GetUsingStealth() ) )
		{
			pPed->ProcessLeftHandGripIk( !pWeaponInfo->GetUseLeftHandIkWhenAiming(), false, CArmIkSolver::GetBlendDuration(ARMIK_BLEND_RATE_FAST), CArmIkSolver::GetBlendDuration(ARMIK_BLEND_RATE_FASTEST) );
		}
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateFSM
// PURPOSE :	Main heartbeat function for CTask
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMelee::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	FSM_Begin
		// Start
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter( pPed );
			FSM_OnUpdate
				return Start_OnUpdate( pPed );
			FSM_OnExit
				Start_OnExit();
		
		// Initial pursuit
		FSM_State(State_InitialPursuit)
			FSM_OnEnter
				InitialPursuit_OnEnter( pPed );
			FSM_OnUpdate
				return InitialPursuit_OnUpdate( pPed );
			FSM_OnExit
				InitialPursuit_OnExit();

		// Look at Target
		FSM_State(State_LookAtTarget)
			FSM_OnUpdate
				return LookAtTarget_OnUpdate( pPed );
			FSM_OnExit
				LookAtTarget_OnExit();

		// Chasing after a target outside melee range.
		FSM_State(State_ChaseAfterTarget)
			FSM_OnEnter
				ChaseAfterTarget_OnEnter( pPed );
			FSM_OnUpdate
				return ChaseAfterTarget_OnUpdate( pPed );
			FSM_OnExit
				ChaseAfterTarget_OnExit();

		FSM_State(State_RetreatToSafeDistance)
			FSM_OnEnter
				RetreatToSafeDistance_OnEnter( pPed );
			FSM_OnUpdate
				return RetreatToSafeDistance_OnUpdate( pPed );
			
		// Waiting for a target to be in a reachable location
		FSM_State(State_WaitForTarget)
			FSM_OnEnter
				WaitForTarget_OnEnter( pPed );
			FSM_OnUpdate
				return WaitForTarget_OnUpdate( pPed );
			FSM_OnExit
				WaitForTarget_OnExit();

		// Decide whether this should go into player or AI melee action detection
		FSM_State(State_WaitForMeleeActionDecide)
			FSM_OnEnter
				WaitForMeleeActionDecide_OnEnter( pPed );
			FSM_OnUpdate
				return WaitForMeleeActionDecide_OnUpdate();

		// Scanning for a new melee action
		FSM_State(State_WaitForMeleeAction)
			FSM_OnEnter
				WaitForMeleeAction_OnEnter();
			FSM_OnUpdate
				return WaitForMeleeAction_OnUpdate( pPed );
			FSM_OnExit
				WaitForMeleeAction_OnExit();

		// Main wait state for those that are deemed as an Inactive Observer
		FSM_State(State_InactiveObserverIdle)
			FSM_OnUpdate
				return InactiveObserverIdle_OnUpdate( pPed );
		
		// Running a melee action
		FSM_State(State_MeleeAction)
			FSM_OnEnter
				MeleeAction_OnEnter( pPed );
			FSM_OnUpdate
				return MeleeAction_OnUpdate( pPed );
			FSM_OnExit
				MeleeAction_OnExit( pPed );
	
		// Swap weapon
		FSM_State(State_SwapWeapon)
			FSM_OnEnter
				SwapWeapon_OnEnter( pPed );
			FSM_OnUpdate
				return SwapWeapon_OnUpdate();

		// Quit
		FSM_State(State_Finish)
			FSM_OnUpdate
				taskDebugf1("Quit: CTaskMelee::UpdateFSM: State_Finish");
				return FSM_Quit;
	FSM_End
}

bool CTaskMelee::AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed )
{
	if (!GetEntity())
		return false;

	return SetupMeleeGetup(sets, pPed, m_pTargetEntity);
}

bool CTaskMelee::SetupMeleeGetup(CNmBlendOutSetList& sets, CPed* pPed, const CEntity* pTarget)
{
	if (!pPed || !pTarget)
		return false;

	CAITarget target(pTarget);

	// set up for a melee getup
	sets.SetMoveTask(rage_new CTaskMoveFaceTarget(target));
	sets.Add(NMBS_MELEE_STANDING);
	sets.Add(NMBS_MELEE_GETUPS);
	return true;
}

bool CTaskMelee::VelocityHighEnoughForTagSyncBlendOut(const CPed* pPed)
{
	static dev_float MIN_VEL = 0.5f;
	float fVelocitySq = pPed->GetVelocity().Mag2();
	return fVelocitySq > square(MIN_VEL);
}

#if !__FINAL
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Debug
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::Debug() const
{
	const aiTask* pSubTask = GetSubTask();
	if( pSubTask ) 
		pSubTask->Debug(); 

#if __BANK
#if DEBUG_DRAW

	// Debug draw the collected information for this frame
	u32 nFrameCount = fwTimer::GetSystemFrameCount();
	if( CCombatMeleeDebug::sm_nLastDebugDrawFrame != nFrameCount )
	{
		CCombatMeleeDebug::sm_nLastDebugDrawFrame = nFrameCount;
		CCombatMeleeDebug::sm_debugDraw.Render();
	}

	if( GetPed()->IsPlayer() )
	{
		// Draw the current combo pressed
		if( GetState() != State_WaitForMeleeAction )
		{
			static const Vector2 s_vCombo1Pos( 0.85f, 0.15f );
			grcDebugDraw::Text( s_vCombo1Pos, Color_green, CImpulse::GetImpulseName( m_impulseCombo.GetImpulseAtIndex(0) ) );
			
			static const Vector2 s_vCombo2Pos( 0.85f, 0.25f );
			grcDebugDraw::Text( s_vCombo2Pos, Color_blue, CImpulse::GetImpulseName( m_impulseCombo.GetImpulseAtIndex(1) ) );
			
			static const Vector2 s_vCombo3Pos( 0.85f, 0.35f );
			grcDebugDraw::Text( s_vCombo3Pos, Color_red, CImpulse::GetImpulseName( m_impulseCombo.GetImpulseAtIndex(2) ) );
		}

		// Give an indication as to how much time is left to get the combo in (player has until first attack hits)
		if( GetState() == State_MeleeAction && pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT )
		{
			const CTaskMeleeActionResult* pMeleeActionTask = static_cast<const CTaskMeleeActionResult*>( pSubTask );
			if( pMeleeActionTask->GetCurrentPhase() > 0.0f )
				CCombatMeleeDebug::DebugDrawAttackWindow( pMeleeActionTask->GetCurrentPhase() );

			char tempString[256];
			formatf( tempString, 128, "ID %d %s\n", pMeleeActionTask->GetUniqueNetworkActionID(), pMeleeActionTask->GetActionResult() ? pMeleeActionTask->GetActionResult()->GetName() : NULL );

			static dev_float sfRootOffsetZ = 1.0f;
			Vec3V vPos = GetPed()->GetTransform().Transform3x3( Vec3V( 0.0f, 0.0f, sfRootOffsetZ ) ) + GetPed()->GetTransform().GetPosition();

			CTask::ms_debugDraw.AddText( vPos, 0, 30, tempString, Color_red, 250, atStringHash( "Unique Action ID" ) );
		}	
	}
#endif // DEBUG_DRAW
#endif // __BANK

#if __DEV
	const CPed *pPed = GetPed();
	const bool bIsPlayer = pPed->IsPlayer();

	const bool bShouldDraw = (bIsPlayer && CCombatMeleeDebug::sm_bControlSubtaskDebugDisplayForPlayer) ||
							 (!bIsPlayer && CCombatMeleeDebug::sm_bControlSubtaskDebugDisplayForNPCs);
	if( !bShouldDraw )
		return;

	static dev_float sfRootOffsetZ = 0.5f;
	Vec3V vPedPosition = pPed->GetTransform().Transform3x3( Vec3V( 0.0f, 0.0f, sfRootOffsetZ ) ) + pPed->GetTransform().GetPosition();

	Color32 debugColor( 0xc0, 0xc0, 0xff, 0xff );
	if( HasLockOnTargetEntity() )
		debugColor.Set( 0xff, 0xc0, 0xc0, 0xff );

	if( CCombatMeleeDebug::sm_bDisplayQueueType && !pPed->IsAPlayerPed() )
	{
		QueueType pedQueueType = GetQueueType();
		if( pedQueueType == QT_ActiveCombatant )
			CTask::ms_debugDraw.AddText( vPedPosition, 0, 30, "Active", Color_red, 250, atStringHash( "QueueType" ) );
		else if( pedQueueType == QT_SupportCombatant )
			CTask::ms_debugDraw.AddText( vPedPosition, 0, 30, "Support", Color_red, 250, atStringHash( "QueueType" ) );
		else if ( pedQueueType == QT_InactiveObserver )
			CTask::ms_debugDraw.AddText( vPedPosition, 0, 30, "Inactive", Color_red, 250, atStringHash( "QueueType" ) );
		else
			CTask::ms_debugDraw.AddText( vPedPosition, 0, 30, "Invalid", Color_red, 250, atStringHash( "QueueType" ) );
	}

	const CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
	{
		Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();
		CActionFlags::TargetType targetType = CActionFlags::TT_ENTITY_ROOT;
		const CPed*	pTargetPed = NULL;
		if( pTargetEntity && pTargetEntity->GetIsTypePed() )
		{
			pTargetPed = static_cast<const CPed*>( pTargetEntity );
			targetType = CActionFlags::TT_PED_NECK;
		}
	
		Vec3V v3CurrentViewedPos( VEC3_ZERO );
		Vec3V v3CurrentViewedDir( VEC3_ZERO );
		int nBoneIdxCache = -1;
		if( !CActionManager::TargetTypeGetPosDirFromEntity( v3CurrentViewedPos,
														    v3CurrentViewedDir,
														    targetType,
														    pTargetEntity,
														    pPed,
														    vPedPosition,
														    0.0f,
														    nBoneIdxCache ) )
		{
			v3CurrentViewedPos = vTargetPosition;
			v3CurrentViewedDir = NormalizeSafe( vTargetPosition - vPedPosition, pPed->GetTransform().GetB() );
		}

		Vec3V vDelta = v3CurrentViewedPos - vPedPosition;
		Vec3V vDeltaXY( vDelta.GetXf(), vDelta.GetYf(), 0.0f );
		Vec3V vMiddle = vPedPosition + Scale( vDelta, ScalarV( 0.5f ) );

		if( CCombatMeleeDebug::sm_bDrawQueueVolumes )
		{
			CTask::ms_debugDraw.AddSphere( vTargetPosition, sm_fAiSeekModeActiveCombatantRadius, Color_blue1, 250, atStringHash( "AiSeekModeActiveCombatantRadius" ) );
			CTask::ms_debugDraw.AddSphere( vTargetPosition, sm_fAiSeekModeSupportCombatantRadiusMin, Color_green1, 250, atStringHash( "AiSeekModeSupportCombatantRadiusMin" ) );
			CTask::ms_debugDraw.AddSphere( vTargetPosition, sm_fAiSeekModeSupportCombatantRadiusMax, Color_green2, 250, atStringHash( "AiSeekModeSupportCombatantRadiusMax" ) );
			CTask::ms_debugDraw.AddSphere( vTargetPosition, sm_fAiSeekModeInactiveCombatantRadiusMin, Color_red1, 250, atStringHash( "AiSeekModeInactiveCombatantRadiusMin" ) );
			CTask::ms_debugDraw.AddSphere( vTargetPosition, sm_fAiSeekModeInactiveCombatantRadiusMax, Color_red2, 250, atStringHash( "AiSeekModeInactiveCombatantRadiusMax" ) );
		}

		if( CCombatMeleeDebug::sm_bRelationshipDisplayDistanceXY && pTargetEntity )
		{
			// Draw a line to the position.
			CTask::ms_debugDraw.AddLine( vPedPosition, v3CurrentViewedPos, debugColor, 250, atStringHash( "FLAT_DIST_LINE" ) );

			// Draw the length of the line.
			char tempString[128];
			formatf( tempString, 128, "%f\n", Mag( vDeltaXY ).Getf() );
			CTask::ms_debugDraw.AddText( vMiddle, 0, 0, tempString, debugColor, 250, atStringHash( "FLAT_DIST_TEXT" ) );
		}

		if( CCombatMeleeDebug::sm_bRelationshipDisplayHeightDiff && pTargetEntity )
		{
			// Draw a line to the position.
			CTask::ms_debugDraw.AddLine( vPedPosition, v3CurrentViewedPos, debugColor, 250, atStringHash( "HEIGHT_DIFF_LINE" ) );

			// Draw the height diff of the line.
			char tempString[128];
			formatf( tempString, 128, "%f\n", vDelta.GetZf() );
			CTask::ms_debugDraw.AddText( vMiddle, 0, 0, tempString, debugColor, 250, atStringHash( "HEIGHT_DIFF_TEXT" ) );
		}

		if( CCombatMeleeDebug::sm_bRelationshipDisplayGroundHeightDiff && pTargetEntity )
		{
			static dev_float sfProbeZOffset = -10.0f;
			float fViewerGroundHeight = CActionManager::GetEntityGroundHeight( pPed, vPedPosition, sfProbeZOffset );
			float fGroundHeight = CActionManager::GetEntityGroundHeight( pTargetEntity, v3CurrentViewedPos, sfProbeZOffset );
			float fGroundHeightDiff = fGroundHeight - fViewerGroundHeight;

			// Draw a line to the position.
			CTask::ms_debugDraw.AddLine( vPedPosition, v3CurrentViewedPos, debugColor, 250, atStringHash( "GROUND_HEIGHT_DIFF_LINE" ) );

			// Draw the height diff of the line.
			char tempString[128];
			formatf( tempString, 128, "%f\n", fGroundHeightDiff );
			CTask::ms_debugDraw.AddText( vMiddle, 0, 0, tempString, debugColor, 250, atStringHash( "GROUND_HEIGHT_DIFF_TEXT" ) );
		}

		if( CCombatMeleeDebug::sm_bRelationshipDisplayFacingAngleXY && pTargetEntity )
		{
			// Draw a line to the position.
			CTask::ms_debugDraw.AddLine( vPedPosition, v3CurrentViewedPos, debugColor, 250, atStringHash( "FACING_ANGLE_LINE" ) );

			// Draw the facing angle of the line.
			const float	fViewerHeading				= fwAngle::LimitRadianAngle( pPed->GetTransform().GetHeading() );
			const float fViewerToViewedHeading		= fwAngle::LimitRadianAngle( rage::Atan2f( -vDeltaXY.GetXf(), vDeltaXY.GetYf() ) );
			const float fViewerToViewedAngleDiff	= fwAngle::LimitRadianAngle( fViewerToViewedHeading - fViewerHeading );

			char tempString[128];
			formatf( tempString, 128, "My Heading Difference (Rad)%f\n", fViewerToViewedAngleDiff );
			CTask::ms_debugDraw.AddText( vPedPosition, 0, 0, tempString, debugColor, 250, atStringHash( "FACING_ANGLE_TEXT_A" ) );

			const float	fViewedHeading				= fwAngle::LimitRadianAngle( pTargetEntity->GetTransform().GetHeading() );
			const float fViewedToViewingHeading		= fwAngle::LimitRadianAngle( rage::Atan2f( vDeltaXY.GetXf(), -vDeltaXY.GetYf() ) );
			const float fViewedToViewerAngleDiff	= fwAngle::LimitRadianAngle( fViewedToViewingHeading - fViewedHeading );
			formatf( tempString, 128, "Viewed Heading Difference (Rad))%f\n", fViewedToViewerAngleDiff );
			CTask::ms_debugDraw.AddText( v3CurrentViewedPos, 0, 0, tempString, debugColor, 250, atStringHash( "FACING_ANGLE_TEXT_B" ) );
		}

		if( CCombatMeleeDebug::sm_bRelationshipDisplayBoxAndClosestBoxPointDistXY && pTargetPed )
		{
			// Get the box which approximates the ped in the current pose.
			// We will the use the nearest point on the box as the position.
			const spdAABB boundingBox = pTargetPed->EstimateLocalBBoxFromPose();

			// Get the transformation for the box.
			const Mat34V& matBoundingBox = pTargetPed->GetMatrixRef();

			// Draw the oriented box.
			grcDebugDraw::BoxOriented( boundingBox.GetMin(), boundingBox.GetMax(), matBoundingBox, debugColor );

			// Get the closest point and normal on the box in world space.
			Vec3V vPosition = FindClosestPointOnAABB( boundingBox, vPedPosition );
			CTask::ms_debugDraw.AddLine( vPedPosition, vPosition, debugColor, 250, atStringHash( "CLOSEST_BBBOX_FLAT_DIST_LINE" ) );

			// Draw the length of the line.
			char tempString[128];
			formatf( tempString, 128, "%f\n", Mag( vDeltaXY ).Getf() );
			CTask::ms_debugDraw.AddText( vMiddle, 0, 0, tempString, debugColor, 250, atStringHash( "CLOSEST_BBBOX_FLAT_DIST_TEXT" ) );
		}

		if( CCombatMeleeDebug::sm_bRelationshipDisplayBoxAndClosestBoxPointHeightDiff && pTargetPed )
		{
			// Get the box which approximates the ped in the current pose.
			// We will the use the nearest point on the box as the position.
			const spdAABB boundingBox = pTargetPed->EstimateLocalBBoxFromPose();

			// Get the transformation for the box.
			const Mat34V& matBoundingBox = pTargetPed->GetMatrixRef();

			// Draw the oriented box.
			grcDebugDraw::BoxOriented( boundingBox.GetMin(), boundingBox.GetMax(), matBoundingBox, debugColor );

			// Get the closest point and normal on the box in world space.
			Vec3V vPosition = FindClosestPointOnAABB( boundingBox, vPedPosition );
			CTask::ms_debugDraw.AddLine( vPedPosition, vPosition, debugColor, 250, atStringHash( "CLOSEST_BBBOX_HEIGHT_DIFF_LINE" ) );

			// Draw the length of the line.
			char tempString[128];
			formatf( tempString, 128, "%f\n", vDelta.GetZf() );
			CTask::ms_debugDraw.AddText( vMiddle, 0, 0, tempString, debugColor, 250, atStringHash( "CLOSEST_BBBOX_HEIGHT_DIFF_TEXT" ) );
		}
	}
#endif //__DEV
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetStaticStateName
// PURPOSE :	Retrieve the name for the identified state
// PARAMETERS :	iState -  State index we are interested in
///////////////////////////////////////////////////////////////////////////////// 
const char * CTaskMelee::GetStaticStateName( s32 iState  )
{
	switch( iState )
	{
		case State_Start:						return "State_Start";
		case State_InitialPursuit:				return "State_InitialPursuit";
		case State_LookAtTarget:				return "State_LookAtTarget";
		case State_ChaseAfterTarget:			return "State_ChaseAfterTarget";
		case State_RetreatToSafeDistance:		return "State_RetreatToSafeDistance";
		case State_WaitForTarget:				return "State_WaitForTarget";
		case State_WaitForMeleeActionDecide:	return "State_WaitForMeleeActionDecide";
		case State_WaitForMeleeAction:			return "State_WaitForMeleeAction";
		case State_InactiveObserverIdle:		return "State_InactiveObserverIdle";
		case State_MeleeAction:					return "State_MeleeAction";
		case State_SwapWeapon:					return "State_SwapWeapon";
		case State_Finish:						return "State_Finish";	
		default: taskAssert(0);
	}

	return "State_Invalid";
}

#endif //!__FINAL

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Start_OnEnter
// PURPOSE :	Entry point of CTaskMelee which will request needed clip sets
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::Start_OnEnter( CPed* pPed )
{
	DEV_BREAK_IF_FOCUS( CCombatMeleeDebug::sm_bFocusPedBreakOnTaskCombatMeleeStateStart, pPed );
	
	if(GetPed()->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF("[%d] CTaskMelee::Start_OnEnter - Player = %s Action %p Previous State %d", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), m_pNextActionResult, GetPreviousState());
	}

	if( !pPed->IsPlayer() )
	{
		// Trigger the first LOS test as early as possible
		m_seekModeLosTestNextRequestTimer.Set( fwTimer::GetTimeInMilliseconds(), 0 ); 

		if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PreventAllMeleeTaunts ) && !GetIsTaskFlagSet( MF_HasPerformedInitialPursuit ) )
		{
			const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
			if( pBrawlData && pBrawlData->ShouldPlayTauntBeforeAttack() )
				SetTaskFlag( MF_PlayTauntBeforeAttacking );
		}			
	}

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreCombatManager))
		m_queueType = QT_ActiveCombatant;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RegisterMeleeOpponent
// PURPOSE :	Register the melee opponent with the combat manager
// PARAMETERS :	pPed -  Ped we want to register
//				pTargetEntity - Target for the given ped
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::RegisterMeleeOpponent( CPed* pPed, CEntity* pTargetEntity )
{
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreCombatManager))
	{
		return;
	}

	if( !m_Opponent.GetPed() )
		m_Opponent.Init( *pPed );

	CCombatMeleeGroup* pMeleeGroup = m_Opponent.GetMeleeGroup();
	const CEntity* pOldTargetEntity = pMeleeGroup ? pMeleeGroup->GetTarget() : NULL;

	// If the group target doesn't match my current target, switch groups
	// If the group isn't currently full and we are not properly in the group (may have been waiting for a spot to open), switch groups
	if( pTargetEntity != pOldTargetEntity || ( pMeleeGroup && !pMeleeGroup->IsMeleeGroupFull() && !pMeleeGroup->IsPedInMeleeGroup( pPed ) ) )
	{
		if( pMeleeGroup )
		{
			pMeleeGroup->Remove( m_Opponent );
			pMeleeGroup = NULL;
		}

		if( pTargetEntity )
		{
			pMeleeGroup = CCombatManager::GetMeleeCombatManager()->FindOrCreateMeleeGroup( *pTargetEntity );
			if( pMeleeGroup )
				pMeleeGroup->Insert( m_Opponent );			
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UnRegisterMeleeOpponent
// PURPOSE :	Reverse of RegisterMeleeOpponent
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::UnRegisterMeleeOpponent( void )
{
	CCombatMeleeGroup* pMeleeGroup = m_Opponent.GetMeleeGroup();
	if( pMeleeGroup )
	{
		pMeleeGroup->Remove( m_Opponent );
		pMeleeGroup = NULL;
	}

	if( m_Opponent.GetPed() )
		m_Opponent.Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Start_OnUpdate
// PURPOSE :	Heartbeat of the initial state. Decides where to direct this
//				instance.
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::Start_OnUpdate( CPed* pPed )
{
	if(GetPed()->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF("[%d] CTaskMelee::Start_OnUpdate - Player = %s Action %p", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), m_pNextActionResult);
	}

	// If the task has an initial result, go straight into a melee action
	CEntity* pTargetEntity = GetTargetEntity();
	if( m_pNextActionResult )
	{	
		if( !pPed->IsNetworkClone() )
		{
			// Calculate the animated camera that will be used for the following action
			if( pPed->IsLocalPlayer() && ( m_pNextActionResult->GetIsAStealthKill() || m_pNextActionResult->GetIsATakedown() ) )
			{
				CHoming* pHoming = m_pNextActionResult->GetHoming();
				if( pHoming && pTargetEntity && pPed->GetReferenceFrameVelocity().IsNonZero())
				{
					Vec3V vTargetDirection( VEC3_ZERO );
					int rnBoneCacheIdx = -1;
					if( !CActionManager::TargetTypeGetPosDirFromEntity( m_vStealthKillWorldOffset,
																		vTargetDirection,
																		pHoming->GetTargetType(),
																		pTargetEntity,
																		pPed,
																		pPed->GetTransform().GetPosition(),
																		0.0f,
																		rnBoneCacheIdx ) )
					{
						// Default to use the target entity position
						m_vStealthKillWorldOffset = pTargetEntity->GetTransform().GetPosition();
						vTargetDirection = m_vStealthKillWorldOffset - pPed->GetTransform().GetPosition();
					}

					Vec3V vMoverDisplacement( VEC3_ZERO );
					pHoming->CalculateHomingWorldOffset( pPed, 
														 m_vStealthKillWorldOffset,
														 vTargetDirection,
														 vMoverDisplacement, 
														 m_vStealthKillWorldOffset, 
														 m_fStealthKillWorldHeading );
				}

				m_nCameraAnimId = camCinematicAnimatedCamera::ComputeValidCameraAnimId( pTargetEntity, m_pNextActionResult ); 
				if( m_nCameraAnimId != 0 )
				{
					pPed->Teleport( VEC3V_TO_VECTOR3( GetStealthKillWorldOffset() ), GetStealthKillHeading(), true );
					SetTaskFlag( MF_SuppressMeleeActionHoming );
				}
			}
		}

		SetState( State_MeleeAction );
	}
	else 
	{
		// Players or anim synced peds go straight into melee action decide
		if( pPed->IsPlayer() || GetIsTaskFlagSet( MF_AnimSynced ) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreCombatManager) )
			SetState( State_WaitForMeleeActionDecide );
		else
		{
			QueueType eQueueType = GetQueueType();
			if( !pPed->IsNetworkClone() && eQueueType > QT_InactiveObserver && !GetIsTaskFlagSet( MF_ForceStrafe ) )
			{
				if( GetIsTaskFlagSet( MF_PlayTauntBeforeAttacking ) )
					SetState( State_LookAtTarget );
				else if( pPed->GetPedType() == PEDTYPE_ANIMAL )
					SetState( State_WaitForMeleeActionDecide );
				else
					SetState( State_InitialPursuit );
			}
			else
				SetState( State_WaitForMeleeActionDecide );
		}

		// Reset the taunt timer before moving on
		ResetTauntTimer();
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Start_OnExit
// PURPOSE :	Clean up for the start state
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::Start_OnExit( void )
{
	ClearTaskFlag( MF_AnimSynced );

	if( GetState() != State_InitialPursuit )
		SetTaskFlag( MF_HasPerformedInitialPursuit );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitialPursuit_OnEnter
// PURPOSE :	(AI ONLY) Will walk toward the target 
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::InitialPursuit_OnEnter( CPed* pPed )
{
	// Note: This is really one of the major parts of our "Melee Combat AI"...
	// Search for the quoted term in the line above to find the others.

	//Set up the nav mesh.
	CNavParams params;

	params.m_vTarget = VEC3V_TO_VECTOR3( CalculateDesiredChasePosition( pPed ) );
	params.m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	params.m_fSlowDownDistance = 0.0f;

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh( params );

	//Keep moving while waiting for a path.
	float fKeepMovingWhilePathingDistance = 0.0f;
	float fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
	if( IsLosBlocked() )
		fTargetRadius = sm_fAiSeekModeActiveCombatantRadius;
	else 
	{
		const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
		if( taskVerifyf( pBrawlData, "CTaskMelee::ChaseAfterTarget_OnEnter - Melee opponent without brawling style" ) )
		{
			fKeepMovingWhilePathingDistance = pBrawlData->GetKeepMovingWhilePathingDistance();
			fTargetRadius = pBrawlData->GetTargetRadius();
		}
	}

	pMoveTask->SetKeepMovingWhilstWaitingForPath( true, fKeepMovingWhilePathingDistance );
	pMoveTask->SetTargetRadius( fTargetRadius );
	pMoveTask->SetAvoidObjects( true );
	pMoveTask->SetNeverEnterWater( true );
	pMoveTask->SetDontStandStillAtEnd( true );

	CTask* pSubTask	= rage_new CTaskAmbientClips( CTaskAmbientClips::Flag_PlayBaseClip | CTaskAmbientClips::Flag_ForceFootClipSetRestart, CONDITIONALANIMSMGR.GetConditionalAnimsGroup( "BRAVE" ) );
	SetNewTask( rage_new CTaskComplexControlMovement( pMoveTask, pSubTask ) );

	if( ShouldAssistMotionTask() )
		ProcessMotionTaskAssistance( true );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitialPursuit_OnUpdate
// PURPOSE :	(AI ONLY) Will continue to walk towards the target until we get 
//				close enough or are told to break out
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::InitialPursuit_OnUpdate( CPed* pPed )
{
	// If we shouldn't be in melee any longer, we should quit out 
	if( !GetIsTaskFlagSet( MF_ShouldBeInMeleeMode ) )
	{
		taskDebugf1("Quit: CTaskMelee::InitialPursuit_OnUpdate: !MF_ShouldBeInMeleeMode");
		SetState( State_Finish );
		return FSM_Continue;
	}

	// Check to see somebody set a local reaction
	if( m_pNextActionResult )
	{
		SetState( State_MeleeAction );
		return FSM_Continue;
	}

	// If the task finishes, go back to searching for a new melee action
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || 
		ShouldBeInInactiveTauntMode() ||
		GetIsTaskFlagSet( MF_HasPerformedInitialPursuit ) )
	{
		ResetTauntTimer();
		SetState( State_WaitForMeleeActionDecide );
		return FSM_Continue;
	}

	CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>( pPed->GetPedIntelligence()->FindMovementTaskByType( CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH ) );
	if( pNavMeshTask )
	{
		if( pNavMeshTask->IsUnableToFindRoute() )
		{
			SetTaskFlag( MF_CannotFindNavRoute );
			return FSM_Continue;
		}
		else if( pNavMeshTask->IsFollowingNavMeshRoute() )
		{
			// Determine if the distance is too far to walk and actually bail out
			// seems strange to walk if they are extremly far away
			if( pNavMeshTask->IsTotalRouteLongerThan( sm_fAiSeekModeInitialPursuitBreakoutDist ) )
			{
				ResetTauntTimer();
				SetState( State_WaitForMeleeActionDecide );
				return FSM_Continue;
			}			
		}

		// Check to see if we should re-evaluate the seek timer
		if( m_seekModeScanTimer.IsOutOfTime() )
			pNavMeshTask->SetTarget( pPed, CalculateDesiredChasePosition( pPed ) );

		//Keep moving while waiting for a path.
		float fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
		if( IsLosBlocked() )
			fTargetRadius = sm_fAiSeekModeActiveCombatantRadius;
		else
		{
			const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
			if( taskVerifyf( pBrawlData, "CTaskMelee::InitialPursuit_OnUpdate - Melee opponent without brawling style" ) )
				fTargetRadius = pBrawlData->GetTargetRadius();
		}

		pNavMeshTask->SetTargetRadius( fTargetRadius );
	}

	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
	{
		Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();

		// Only test the stop conditions if our line of sight is not blocked
		if( !IsLosBlocked() )
		{		
			Vec3V vDelta = vTargetPosition - pPed->GetTransform().GetPosition();
			Vec3V vDeltaXY( vDelta.GetXf(), vDelta.GetYf(), 0.0f );
			float fArrivalThreshold = GetDesiredTargetDistance();			
			if( Abs( vDelta.GetZf() ) < sm_fAiSeekModeTargetHeight && MagSquared( vDeltaXY ).Getf() < rage::square( fArrivalThreshold ) )
			{
				SetState( State_WaitForMeleeActionDecide );
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitialPursuit_OnExit
// PURPOSE :	Let the task know we have performed the initial pursuit state
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::InitialPursuit_OnExit( void )
{
	SetTaskFlag( MF_HasPerformedInitialPursuit );
	
	// Reset any motion task assistance.
	ProcessMotionTaskAssistance( false );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	LookAtTarget_OnUpdate
// PURPOSE :	Test to see whether or not we are within certain look at criteria
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::LookAtTarget_OnUpdate( CPed* pPed )
{
	// Calculate the distance to the player
	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
	{
		Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();
		if ( ShouldUpdateDesiredHeading( *pPed, vTargetPosition ) )
		{
			pPed->SetDesiredHeading( VEC3V_TO_VECTOR3( vTargetPosition ) );
		}

		// We do not want to proceed until the animations are properly streamed in
		if( CEquipWeaponHelper::AreEquippedWeaponAnimsStreamedIn( *pPed ) )
		{
			// Check if the ped is reasonably close to look at their target.
			float fDesiredHeading = pPed->GetMotionData()->GetDesiredHeading();
			float fCurrentHeading = pPed->GetMotionData()->GetCurrentHeading();
			bool bCloseEnoughForBlend = fabs(SubtractAngleShorter( fDesiredHeading, fCurrentHeading )) < sm_fLookAtTargetHeadingBlendoutDegrees * DtoR;

			if (bCloseEnoughForBlend)
			{
				// Check if the ped can be seen.
				CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
				if( pPedIntelligence )
				{
					if( pPedIntelligence->GetPedPerception().ComputeFOVVisibility( pTargetEntity, NULL ) )
						SetState( State_WaitForMeleeActionDecide );
				}
				else
					SetState( State_WaitForMeleeActionDecide );

				// If looking at the target, but not in the FoV (too far away), then proceed to the next state anyway if enough time has passed.
				if (GetTimeInState() > sm_fLookAtTargetTimeout)
				{
					SetState( State_WaitForMeleeActionDecide );
				}
			}
		}
	}
	else
		SetState( State_WaitForMeleeActionDecide );

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	LookAtTarget_OnExit
// PURPOSE :	Reset anything important before moving on to the next state
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::LookAtTarget_OnExit( void )
{
	// Force an attack and taunt
	u32 nCurrentTimeInMs = fwTimer::GetTimeInMilliseconds();
	m_nextRequiredMoveTimer.Set( nCurrentTimeInMs, 0 );
	m_tauntTimer.Set( nCurrentTimeInMs, 0 );

	// Allow this ped to shout the target's position now that it has finished looking.
	GetPed()->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_DisableShoutTargetPosition);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForMeleeActionDecide_OnEnter
// PURPOSE :	Initializes special AI timers and resets the combo buffer
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::WaitForMeleeActionDecide_OnEnter( CPed* pPed )
{
	// Make sure that AI peds always do another move within a certain amount of time.
	const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
	if( !pPed->IsPlayer() && taskVerifyf( pBrawlData, "CTaskMelee::WaitForMeleeActionDecide_OnEnter - Melee opponent without brawling style" ) )
	{
		float fFightProficiency = Clamp( pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat( kAttribFloatFightProficiency ), 0.0f, 1.0f );
		if( !m_nextRequiredMoveTimer.IsSet() )
			m_nextRequiredMoveTimer.Set( fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange( Lerp( fFightProficiency, pBrawlData->GetAttackFrequencyWorstFighterMin(), pBrawlData->GetAttackFrequencyBestFighterMin() ), Lerp( fFightProficiency, pBrawlData->GetAttackFrequencyWorstFighterMax(), pBrawlData->GetAttackFrequencyBestFighterMax() ) ) );

		// Calculate probability of doing a block on the next opponent strike
		if( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) < ( Lerp( fFightProficiency, pBrawlData->GetBlockProbabilityMin(), pBrawlData->GetBlockProbabilityMax() ) + GetBlockBuff() ) )		
			m_bCanBlock = true;
		else 
			m_bCanBlock = false;
	}

	ResetImpulseCombo();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForMeleeActionDecide_OnUpdate
// PURPOSE :	Branch point which will decide to either swap weapon or send ped
//				the the main melee waiting state
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::WaitForMeleeActionDecide_OnUpdate( void )
{
	if( !GetPed()->IsPlayer() && (GetQueueType() <= QT_InactiveObserver) )
		SetState( State_InactiveObserverIdle );
	else
		SetState( State_WaitForMeleeAction );

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	MeleeAction_OnEnter
// PURPOSE :	In charge of allocating melee action instance and recalculating 
//				all important ai melee probabilities
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::MeleeAction_OnEnter( CPed* pPed )
{
	DEV_BREAK_IF_FOCUS(CCombatMeleeDebug::sm_bBreakOnFocusPedActionTransition, pPed);

	if(!pPed->IsNetworkClone())
	{
		const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
		taskAssertf( pBrawlData, "CTaskMelee::MeleeAction_OnEnter - Melee opponent without brawling style" );
		float fFightProficiency = Clamp( pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat( kAttribFloatFightProficiency ), 0.0f, 1.0f );

		Assert( m_pNextActionResult ); 

		// Disable the timers for now
		if( !pPed->IsPlayer() )
		{
			// Only stop the next move time when we are not attacking
			if( m_pNextActionResult->GetIsAStandardAttack() )
				m_nextRequiredMoveTimer.Unset();

			// Calculate whether or not we will perform a combo on our next attack
			m_bCanBlock = m_bCanCounter = m_bCanPerformCombo = false;
			if( pBrawlData )
			{
				// If we plan to execute a block, calculate whether or not we should counter attack
				if( m_pNextActionResult->GetIsABlock() )
				{
					if( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) < Lerp( fFightProficiency, pBrawlData->GetCounterProbabilityMin(), pBrawlData->GetCounterProbabilityMax() ) )
						m_bCanCounter = true;
					// Recalculate probability of doing a block on the next opponent strike
					else if( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) < ( Lerp( fFightProficiency, pBrawlData->GetBlockProbabilityMin(), pBrawlData->GetBlockProbabilityMax() ) + GetBlockBuff() ) )
						m_bCanBlock = true;	
				}
				// We do not want to allow AI combos if the target is unreachable
				else if( CanPerformMeleeAttack( pPed, GetTargetEntity() ) )
				{
					// Determine whether or not we should perform a combination
					if( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) < Lerp( fFightProficiency, pBrawlData->GetAttackProbabilityToComboMin(), pBrawlData->GetAttackProbabilityToComboMax() ) )
						m_bCanPerformCombo = true;
				}
			}

			// If we are reacting to a melee action, increase the chances to block on the next
			if( m_pNextActionResult->GetIsAHitReaction() )
				IncreaseMeleeBuff( pPed );
			// Any other action will decrease the block buffer
			else
				DecreaseMeleeBuff();
		}

		// Reset the daze reaction bool flag
		m_bCanPerformDazedReaction = false;

		// Re-evaluate the dazed reaction if allowed
		if( pBrawlData && m_pNextActionResult->AllowDazedReaction() )
		{
			if( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) < Lerp( fFightProficiency, pBrawlData->GetProbabilityToBeDazedMin(), pBrawlData->GetProbabilityToBeDazedMax() ) )
				m_bCanPerformDazedReaction = true;
		}

		if( m_pNextActionResult->GetIsAnIntro() || m_pNextActionResult->GetIsATaunt() )
		{
			m_tauntTimer.Unset();
			SetTaskFlag( MF_ResetTauntTimerAfterNextAction );

			if( ShouldBeInInactiveTauntMode() )
				pPed->NewSay( "GENERIC_INSULT_HIGH" );
			else
				pPed->NewSay( "FIGHT_BYSTANDER_TAUNT" );
		}	

		if( m_pNextActionResult->AllowNoTargetInterrupt() )
			SetTaskFlag( MF_AllowNoTargetInterrupt );

		// Time to force out the secondary task as quick as possible
		aiTask* pSecondaryTask = pPed->GetPedIntelligence() ? pPed->GetPedIntelligence()->GetTaskSecondaryActive() : NULL;
		if( pSecondaryTask && pSecondaryTask->GetTaskType() == CTaskTypes::TASK_SHOVE_PED )
			static_cast<CTaskShovePed*>(pSecondaryTask)->SetBlendOutDuration( FAST_BLEND_DURATION );
	
		// Send data immediately if a player is involved.
		if(pPed->GetNetworkObject() && GetTargetEntity() && GetTargetEntity()->GetIsTypePed())
		{
			CPed* pTargetPed = static_cast<CPed*>(GetTargetEntity());
			if(!pPed->IsNetworkClone() && (pTargetPed->IsPlayer() || pPed->IsPlayer()))
			{
				CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());
				pPedObj->ForceResendOfAllTaskData();
				pPedObj->ForceResendAllData();
			}
		}
	}

	// Pause the force strafe timer
	if( m_forceStrafeTimer.IsEnabled() )
		m_forceStrafeTimer.Pause();

	if( !pPed->IsNetworkClone() || IsRunningLocally() )
	{
		// Mark the last action as a recoil
		m_bLastActionWasARecoil = m_pNextActionResult->GetIsARecoil();

		if( m_pNextActionResult->ShouldGiveThreatResponseAfterAction() )
			SetTaskFlag( MF_GiveThreatResponseAfterNextAction );
		else 
			ClearTaskFlag( MF_GiveThreatResponseAfterNextAction );

		if( m_pNextActionResult->ShouldQuitTaskAfterAction() )
			SetTaskFlag( MF_QuitTaskAfterNextAction );
		else 
			ClearTaskFlag( MF_QuitTaskAfterNextAction );

		// Will this action end in an aiming pose?
		m_bEndsInAimingPose = m_pNextActionResult->EndsInAimingPose();

		if( m_pNextActionResult->GetIsAHitReaction() )
		{
			SetTaskFlag( MF_ResetTauntTimerAfterNextAction );
			if( m_pNextActionResult->GetShouldResetMinTimeInMelee() )
				SetTaskFlag( MF_ReactionResetTimeInTaskAfterNextAction );	
		}
		else if( m_pNextActionResult->GetShouldResetMinTimeInMelee() && GetTargetEntity() )
		{
			SetTaskFlag( MF_AttackResetTimeInTaskAfterNextAction );	
		}

		// Only set this MF_TagSyncBlendOut flag if MF_QuitTaskAfterNextAction is also set
		if( GetIsTaskFlagSet( MF_QuitTaskAfterNextAction ) && m_pNextActionResult->TagSyncBlendOut() )
			SetTaskFlag( MF_TagSyncBlendOut );
		else
			ClearTaskFlag( MF_TagSyncBlendOut );

		if( m_pNextActionResult->EndsInIdlePose() )
			SetTaskFlag( MF_EndsInIdlePose );
		else
			ClearTaskFlag( MF_EndsInIdlePose );

		//! DMKH. Cache action result ID for networking as we reset 'm_pNextActionResult', so this can't be used :(
		m_nActionResultNetworkID = m_pNextActionResult->GetID();
		m_bLocalReaction = false;
		
		// UGH! Cache off the target weapon hash 
		// This is needed because we play out an animated sequence involving a ped
		// The ped might have a weapon in their hands but will not be reliable when	
		// we process self damage.
		u32 nSelfDamageWeaponHash = pPed->GetDefaultUnarmedWeaponHash();

		eAnimBoneTag eSelfDamageAnimBoneTag = BONETAG_INVALID;
		const CDamageAndReaction* pDamageAndReaction = m_pNextActionResult->GetDamageAndReaction();
		if( pDamageAndReaction )
			eSelfDamageAnimBoneTag = pDamageAndReaction->GetForcedSelfDamageBoneTag();

		CEntity* pTargetEntity = GetTargetEntity();
		if( pTargetEntity && pTargetEntity->GetIsTypePed() )
		{
			CPed* pTargetPed = static_cast<CPed*>(pTargetEntity );
			if( pTargetPed->GetWeaponManager() )
				nSelfDamageWeaponHash = pTargetPed->GetWeaponManager()->GetEquippedWeaponHash();
		}

		CTaskMeleeActionResult* pActionResultTask = rage_new CTaskMeleeActionResult( m_pNextActionResult, pTargetEntity, HasLockOnTargetEntity(), GetIsTaskFlagSet( MF_IsDoingReaction ), GetIsTaskFlagSet( MF_SuppressMeleeActionHoming ), nSelfDamageWeaponHash, eSelfDamageAnimBoneTag, pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() );
		SetNewTask( pActionResultTask );

		// This will save a history of all the performed recent actions so we can avoid repeating moves
		CActionManager::RecordAction( m_pNextActionResult->GetID() );

		if( pActionResultTask && IsRunningLocally() )
		{
			pActionResultTask->SetRunningLocally( true );
			pActionResultTask->SetRunAsAClone( true );

			if(pPed->IsNetworkClone())
			{
				MELEE_NETWORK_DEBUGF("[%d] CTaskMelee::MeleeAction_OnEnter - Player: %s Address: %p Sub Address: %p Running Locally: %d Unique ID: %d", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), this, pActionResultTask, IsRunningLocally() ? 1 : 0, m_nUniqueNetworkMeleeActionID);
			}
		}

		pActionResultTask->SetUniqueNetworkActionID(m_nUniqueNetworkMeleeActionID);

		// If ID is non 0, indicates this melee move was triggered by another ped (e.g. a reaction). Consume ID here, so that is reset the next time we create an
		// an action.
		m_nUniqueNetworkMeleeActionID = 0;

	#if FPS_MODE_SUPPORTED
		m_bInterruptForFirstPersonCamera = false;
	#endif

		// Reset the action information
		m_pNextActionResult = NULL;
		m_bActionIsBranch = false;
		ClearTaskFlag( MF_IsDoingReaction );
	}

	if(pPed->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF( "[%d] CTaskMelee::MeleeAction_OnEnter - sequence ID = %d", CNetwork::GetNetworkTime(), GetNetSequenceID() );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	MeleeAction_OnUpdate
// PURPOSE :	Monitors the CTaskMeleeActionResult instance as well as manages
//				any combos from the current action.
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::MeleeAction_OnUpdate( CPed* pPed )
{
	// Always force ped into strafe in order to facilitate the melee_outro anim when we stop
	if( !GetIsTaskFlagSet( MF_QuitTaskAfterNextAction ) )
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePedToStrafe, true );

	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || ( GetIsTaskFlagSet( MF_AllowNoTargetInterrupt ) && !HasLockOnTargetEntity() ) )
	{
		if( GetIsTaskFlagSet( MF_ShouldStartGunTaskAfterNextAction ) )
		{
			aiTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_PLAYER_ON_FOOT );
			if( pTask )
				static_cast<CTaskPlayerOnFoot*>( pTask )->SetShouldStartGunTask( true );
			taskDebugf1("Quit: CTaskMelee::MeleeAction_OnUpdate: aiTaskFlags::SubTaskFinished (%d), MF_AllowNoTargetInterrupt (%d), !HasLockOnTargetEntity() (%d), MF_ShouldStartGunTaskAfterNextAction", GetIsFlagSet( aiTaskFlags::SubTaskFinished ), GetIsTaskFlagSet( MF_AllowNoTargetInterrupt ), !HasLockOnTargetEntity());
			return FSM_Quit;
		}

		if( (!pPed->IsNetworkClone() || IsRunningLocally()) && GetIsTaskFlagSet( MF_QuitTaskAfterNextAction ))
		{
			taskDebugf1("Quit: CTaskMelee::MeleeAction_OnUpdate: aiTaskFlags::SubTaskFinished (%d), MF_AllowNoTargetInterrupt (%d), !HasLockOnTargetEntity() (%d), (!pPed->IsNetworkClone() (%d) || IsRunningLocally() (%d)) && GetIsTaskFlagSet( MF_QuitTaskAfterNextAction )", GetIsFlagSet( aiTaskFlags::SubTaskFinished ), GetIsTaskFlagSet( MF_AllowNoTargetInterrupt ), !HasLockOnTargetEntity(), !pPed->IsNetworkClone(), IsRunningLocally());
			SetState( State_Finish );
		}
		else
		{
			SetState( State_WaitForMeleeActionDecide );
		}

		return FSM_Continue;
	}

	// Update blocking
	if( GetIsTaskFlagSet( MF_AllowStrafeMode ) && !GetIsTaskFlagSet( MF_BehindTargetPed ) )
	{
		UpdateBlocking( pPed, false );
	}

	bool bMoveAllowsBlockTargetEvaluation = IsCurrentlyDoingAMove(false, false, true);
	if(bMoveAllowsBlockTargetEvaluation)
	{
		UpdateBlockingTargetEvaluation(pPed);
	}
	
	return ManageActionSubtaskAndBranches( pPed );
}	

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	MeleeAction_OnExit
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::MeleeAction_OnExit( CPed* pPed )
{
	m_nActionResultNetworkID = INVALID_ACTIONRESULT_ID;

	if( GetIsTaskFlagSet( MF_ResetTauntTimerAfterNextAction ) )
	{
		ClearTaskFlag( MF_ResetTauntTimerAfterNextAction );
		ResetTauntTimer();
	}

	// Player only force strafe timer logic
	if( pPed->IsLocalPlayer() )
	{
		// Reset the actual force strafe timer (if necessary)
		if( GetIsTaskFlagSet( MF_ReactionResetTimeInTaskAfterNextAction ) )
		{
			ClearTaskFlag( MF_ReactionResetTimeInTaskAfterNextAction );
			m_forceStrafeTimer.Set( fwTimer::GetTimeInMilliseconds(), sm_nTimeInTaskAfterHitReaction );
		}
		else if( GetIsTaskFlagSet( MF_AttackResetTimeInTaskAfterNextAction ) )
		{
			ClearTaskFlag( MF_AttackResetTimeInTaskAfterNextAction );
			m_forceStrafeTimer.Set( fwTimer::GetTimeInMilliseconds(), sm_nTimeInTaskAfterStardardAttack );
		}
		else if( m_forceStrafeTimer.IsEnabled() && m_forceStrafeTimer.IsPaused() )
			m_forceStrafeTimer.UnPause();

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			m_forceStrafeTimer.Set(0, 0, false);
		}
#endif
	}

	if( GetIsTaskFlagSet( MF_GiveThreatResponseAfterNextAction ) && CanChangeTarget())
	{
		ClearTaskFlag( MF_GiveThreatResponseAfterNextAction );
		if( !pPed->IsPlayer() )
		{
			CEntity* pTargetEntity = GetTargetEntity();
			if( pTargetEntity && pTargetEntity->GetIsTypePed() )
			{
				CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );
				CTaskThreatResponse* pTask = rage_new CTaskThreatResponse( pTargetPed );
				pTask->SetThreatResponseOverride( CTaskThreatResponse::TR_Fight );
				pTask->GetConfigFlags().SetFlag( CTaskThreatResponse::CF_CanFightArmedPedsWhenNotArmed );
				pTask->GetConfigFlagsForCombat().SetFlag( CTaskCombat::ComF_DisableAimIntro );
				pTask->GetConfigFlagsForCombat().SetFlag( CTaskCombat::ComF_MeleeAnimPhaseSync );
				CEventGivePedTask event( PED_TASK_PRIORITY_PHYSICAL_RESPONSE, pTask, false, E_PRIORITY_DAMAGE_PAIRED_MOVE );
				pPed->GetPedIntelligence()->AddEvent( event );
				pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
			}
		}
	}

	ClearTaskFlag( MF_ForceStrafeIndefinitely );
	ClearTaskFlag( MF_PerformIntroAnim );
	ClearTaskFlag( MF_PlayTauntBeforeAttacking );
	ClearTaskFlag( MF_AllowNoTargetInterrupt );
	ClearTaskFlag( MF_SuppressMeleeActionHoming );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SwapWeapon_OnEnter
// PURPOSE :	In charge of allocating the weapon swap task
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::SwapWeapon_OnEnter( CPed* pPed )
{	
	s32 iFlags = SWAP_HOLSTER;
	const CPedWeaponManager* pPedWeaponManager = pPed->GetWeaponManager();
	if( pPedWeaponManager )
	{
		const CWeaponInfo* pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( pPedWeaponManager->GetEquippedWeaponHash() );
		if( pInfo && pInfo->GetIsCarriedInHand() )
			iFlags |= SWAP_DRAW;

		// Force release the current taunt and variation clip sets as these are based on the currently equipped weapon
		m_TauntClipSetRequestHelper.Release();
		m_VariationClipSetRequestHelper.Release();
	}

	CTask* pMovementTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
	if( pMovementTask )
		SetNewTask( rage_new CTaskComplexControlMovement( (CTask*)pMovementTask->Copy(), rage_new CTaskSwapWeapon( iFlags ), CTaskComplexControlMovement::TerminateOnSubtask ) );
	else 
		SetNewTask( rage_new CTaskSwapWeapon( iFlags ) );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SwapWeapon_OnUpdate
// PURPOSE :	Waits until previously allocated swap weapon task has finished 
//				and moves on accordingly
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::SwapWeapon_OnUpdate( void )
{
	// Set state to decide what to do next
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || !GetSubTask() )
		SetState( State_WaitForMeleeActionDecide );

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ChaseAfterTarget_OnEnter
// PURPOSE :	(AI ONLY) In charge of getting a melee ped within relative distance
//				to its target
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::ChaseAfterTarget_OnEnter( CPed* pPed )
{
	taskAssertf( !pPed->IsPlayer(), "CTaskMelee::ChaseAfterTarget_OnEnter - Non player peds not allowed!" );

	// Reset the attack timer to always allow an action from chase
	m_nextRequiredMoveTimer.Set( fwTimer::GetTimeInMilliseconds(), 0 );
	
	// Note: This is really one of the major parts of our "Melee Combat AI"...
	// Search for the quoted term in the line above to find the others.

	//Set up the nav mesh.
	CNavParams params;

	params.m_vTarget = VEC3V_TO_VECTOR3( CalculateDesiredChasePosition( pPed ) );
	if( pPed->GetPedType() == PEDTYPE_ANIMAL )
	{
		params.m_fMoveBlendRatio = MOVEBLENDRATIO_SPRINT;

		CEntity* pTargetEntity = GetTargetEntity();
		if ( pTargetEntity && pTargetEntity->GetIsTypePed() )
		{
			CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);
			if ( pTargetPed->GetTaskData().GetIsFlagSet( CTaskFlags::ForceSlowChaseInAnimalMelee ) )
			{
				params.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
			}
		}
	}
	else
	{
		params.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	}
	params.m_fSlowDownDistance = 0.0f;
	
	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh( params );

	const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
	taskAssertf( pBrawlData, "CTaskMelee::ChaseAfterTarget_OnEnter - Melee opponent without brawling style" );

	//Keep moving while waiting for a path.
	float fKeepMovingWhilePathingDistance = 2.0f;
	float fMaxDistanceMayAdjustPathEndPosition = 1.5f;
	float fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
	if( IsLosBlocked() )
	{
		fTargetRadius = sm_fAiSeekModeActiveCombatantRadius;
		fKeepMovingWhilePathingDistance = pBrawlData ? pBrawlData->GetKeepMovingWhilePathingDistance() : 2.0f;
		fMaxDistanceMayAdjustPathEndPosition = pBrawlData ? pBrawlData->GetMaxDistanceMayAdjustPathEndPosition() : 1.5f;
	}
	else if( pBrawlData )
	{
		fKeepMovingWhilePathingDistance = pBrawlData->GetKeepMovingWhilePathingDistance();
		fMaxDistanceMayAdjustPathEndPosition = pBrawlData->GetMaxDistanceMayAdjustPathEndPosition();
		fTargetRadius = pBrawlData->GetTargetRadius();
	}

	pMoveTask->SetKeepMovingWhilstWaitingForPath( true, fKeepMovingWhilePathingDistance );
	pMoveTask->SetMaxDistanceMayAdjustPathEndPosition( fMaxDistanceMayAdjustPathEndPosition );
	pMoveTask->SetTargetRadius( fTargetRadius );
	pMoveTask->SetAvoidObjects( true );
	pMoveTask->SetNeverEnterWater( true );
	pMoveTask->SetDontStandStillAtEnd( true );
	pMoveTask->SetUseLargerSearchExtents( true );
	
	SetNewTask( rage_new CTaskComplexControlMovement( pMoveTask ) );

	pPed->SetUsingActionMode( true, CPed::AME_Combat );
	pPed->SetMovementModeOverrideHash( ATSTRINGHASH("DEFAULT_ACTION", 0x076f1bf8d) );

	if( ShouldAssistMotionTask() )
		ProcessMotionTaskAssistance( true );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ChaseAfterTarget_OnUpdate
// PURPOSE :	(AI ONLY) Will monitor arrival conditions to stop the subtask also
//				make sure we are facing the target when strafing
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::ChaseAfterTarget_OnUpdate( CPed* pPed )
{
	// If the task finishes, go back to searching for a new melee action
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || ShouldBeInInactiveTauntMode() )
	{
		SetState( State_WaitForMeleeActionDecide );
		return FSM_Continue;
	}


	pPed->SetUsingActionMode( true, CPed::AME_Combat );

	CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>( pPed->GetPedIntelligence()->FindMovementTaskByType( CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH ) );
	if( pNavMeshTask )
	{
		if( pNavMeshTask->IsUnableToFindRoute() )
		{
			SetTaskFlag( MF_CannotFindNavRoute );
			return FSM_Continue;
		}

		// Check to see if we should re-evaluate the seek timer
		if( m_seekModeScanTimer.IsOutOfTime() )
			pNavMeshTask->SetTarget( pPed, CalculateDesiredChasePosition( pPed ) );
		
		//Keep moving while waiting for a path.
		float fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
		if( IsLosBlocked() )
			fTargetRadius = sm_fAiSeekModeActiveCombatantRadius;
		else
		{
			const CBrawlingStyleData* pBrawlData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );
			if( taskVerifyf( pBrawlData, "CTaskMelee::ChaseAfterTarget_OnUpdate - Melee opponent without brawling style" ) )
				fTargetRadius = pBrawlData->GetTargetRadius();
		}

		pNavMeshTask->SetTargetRadius( fTargetRadius );
	}

	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
	{
		Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();

		// Always face the player when standing still waiting
		float fMoveBlendRatioSq = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
		if( fMoveBlendRatioSq < rage::square( CActionManager::ms_fMBRWalkBoundary ) )
			pPed->SetDesiredHeading( VEC3V_TO_VECTOR3( vTargetPosition ) );

		// Only test the stop conditions if our line of sight is not blocked
		if( !IsLosBlocked() )
		{		
			Vec3V vDelta = vTargetPosition - pPed->GetTransform().GetPosition();
			Vec3V vDeltaXY( vDelta.GetXf(), vDelta.GetYf(), 0.0f );
			float fArrivalThreshold = GetDesiredTargetDistance();			
			if( Abs( vDelta.GetZf() ) < sm_fAiSeekModeTargetHeight && MagSquared( vDeltaXY ).Getf() < rage::square( fArrivalThreshold ) )
			{
				SetState( State_WaitForMeleeActionDecide );
				return FSM_Continue;
			}
		}
	}
	
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ChaseAfterTarget_OnExit
// PURPOSE :	Clean up for the ChaseAfterTarget state
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::ChaseAfterTarget_OnExit( void )
{
	// Reset the seek mode probe
	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	m_seekModeLosTestNextRequestTimer.Set( uCurrentTime, 0 );

	// Reset any motion task assistance.
	ProcessMotionTaskAssistance( false );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RetreatToSafeDistance_OnEnter
// PURPOSE :	(AI ONLY) In charge of making the ped retreat while waiting for 
//				a valid path to be processed.
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::RetreatToSafeDistance_OnEnter( CPed* pPed )
{
	taskAssertf( !pPed->IsPlayer(), "CTaskMelee::RetreatToSafeDistance_OnEnter - Player peds not allowed!" );

	CEntity* pTargetEntity = GetTargetEntity();
	if( taskVerifyf( pTargetEntity, "CTaskMelee::RetreatToSafeDistance_OnEnter - No target entity!" ) )
	{
		ProcessUnreachableNavMeshTest( pPed, pTargetEntity );

		//Set up the nav mesh.
		CNavParams params;

		// Grab a random retreat distance which is in range
		const float fDesiredRetreatDist = fwRandom::GetRandomNumberInRange( sm_fAiSeekModeRetreatDistanceMin, sm_fAiSeekModeRetreatDistanceMax );
	
		// Set the desired offset distance
		Vec3V vPredictedPosition( VEC3_ZERO );
		vPredictedPosition.SetYf( fDesiredRetreatDist );
		vPredictedPosition = RotateAboutZAxis( vPredictedPosition, ScalarV( GetAngleSpread() ) );
		vPredictedPosition += pTargetEntity->GetTransform().GetPosition();

		params.m_vTarget = VEC3V_TO_VECTOR3( vPredictedPosition );
		params.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
		params.m_fSlowDownDistance = 0.0f;

		//Create the move task.
		CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh( params );

		// Set up some additional information
		pMoveTask->SetKeepMovingWhilstWaitingForPath( false, 0.0f );
		pMoveTask->SetTargetRadius( CTaskMoveFollowNavMesh::ms_fTargetRadius );
		pMoveTask->SetAvoidObjects( true );
		pMoveTask->SetNeverEnterWater( true );
		pMoveTask->SetDontStandStillAtEnd( false );

		SetNewTask( rage_new CTaskComplexControlMovement( pMoveTask ) );

		pPed->SetUsingActionMode( true, CPed::AME_Combat );
		pPed->SetMovementModeOverrideHash( ATSTRINGHASH("DEFAULT_ACTION", 0x076f1bf8d) );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	RetreatToSafeDistance_OnUpdate
// PURPOSE :	(AI ONLY) Monitor conditions is which the retreat will move on
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::RetreatToSafeDistance_OnUpdate( CPed* pPed )
{
	// If the task finishes, go back to searching for a new melee action
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) )
	{
		SetState( State_WaitForTarget );
		return FSM_Continue;
	}

	// If we have resolved the reason why we came into this state, then re-evaluate
	if( !ShouldBeInInactiveTauntMode() )
	{
		SetState( State_WaitForMeleeActionDecide );
		m_RouteSearchHelper.ResetSearch();
		return FSM_Continue;
	}

	pPed->SetUsingActionMode( true, CPed::AME_Combat );
	
	CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>( pPed->GetPedIntelligence()->FindMovementTaskByType( CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH ) );
	if( pNavMeshTask )
	{
		if( pNavMeshTask->IsUnableToFindRoute() )
		{
			SetState( State_WaitForTarget );
			return FSM_Continue;
		}
	}

	// Make the ped try to face the target.
	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
		pPed->SetDesiredHeading( VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() ) );

	ProcessUnreachableNavMeshTest( pPed, pTargetEntity );
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForTarget_OnEnter
// PURPOSE :	(AI ONLY) In charge of waiting in the current location until a
//				reachable path has been processed
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::WaitForTarget_OnEnter( CPed* pPed )
{
	taskAssertf( !pPed->IsPlayer(), "CTaskMelee::WaitForTarget_OnEnter - Player peds not allowed!" );
	ProcessUnreachableNavMeshTest( pPed, GetTargetEntity() );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForTarget_OnUpdate
// PURPOSE :	(AI ONLY) Monitor conditions is which the wait will finish
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::WaitForTarget_OnUpdate( CPed* pPed )
{
	// If we have resolved the reason why we came into this state, then re-evaluate
	if( !ShouldBeInInactiveTauntMode() )
	{
		SetState( State_WaitForMeleeActionDecide );
		return FSM_Continue;
	}

	// Make the ped try to face the target.
	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
		pPed->SetDesiredHeading( VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() ) );

	ProcessUnreachableNavMeshTest( pPed, pTargetEntity );
	return ManageActionSubtaskAndBranches( pPed );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForTarget_OnExit
// PURPOSE :	(AI ONLY) Wait target clean up 
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::WaitForTarget_OnExit( void )
{
	if( m_RouteSearchHelper.IsSearchActive() )
		m_RouteSearchHelper.ResetSearch();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForMeleeAction_OnEnter
// PURPOSE :	Main wait state 
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::WaitForMeleeAction_OnEnter( void )
{
	ResetImpulseCombo();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForMeleeAction_OnUpdate
// PURPOSE :	Monitor impulses(player) or when ai should attack
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::WaitForMeleeAction_OnUpdate( CPed* pPed )
{
#if FPS_MODE_SUPPORTED
	bool bWasBlocking = IsBlocking();
#endif

	// Check to see if we are in stealth mode
	if( GetIsTaskFlagSet( MF_BehindTargetPed ) )
		UpdateStealth( pPed );
	else if( GetIsTaskFlagSet( MF_AllowStrafeMode ) )
		UpdateBlocking( pPed );
	else
	{
		// Abort upper body anims if we are not running either stealth nor blocking
		aiTask* pSubTask = GetSubTask();
		if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_UPPERBODY_ANIM  )
		{
			if( pSubTask->GetIsFlagSet( aiTaskFlags::HasBegun ) )
				pSubTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL );
		}
	}

#if FPS_MODE_SUPPORTED
	if( !GetNewTask() )
	{
		aiTask* pSubTask = GetSubTask();
		if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_PLAYER_WEAPON )
		{
			if( pSubTask->GetIsFlagSet( aiTaskFlags::HasBegun ) && !pPed->IsFirstPersonShooterModeEnabledForPlayer() )
			{
				pSubTask->MakeAbortable( aiTask::ABORT_PRIORITY_URGENT, NULL );
			}
		}
		else if( ( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || !pSubTask || pSubTask->GetIsFlagSet( aiTaskFlags::IsAborted) ) && pPed->IsFirstPersonShooterModeEnabledForPlayer() )
		{
			s32 iGunFlags = GF_ForceAimState;
			if( bWasBlocking && !IsBlocking() )
			{
				iGunFlags |= GF_InstantBlendToAim;
			}
			SetNewTask( rage_new CTaskPlayerWeapon( iGunFlags ) );
		}
	}
#endif

	// Quit task if we are waiting and need to switch to a non-melee related weapon
	// Unfortunately this bit of logic is somewhat duplicated in GetDesiredStateAndUpdateBreakoutConditions
	// but is necessary to get around the fact that script changes weapons without caring much.
	if( !pPed->IsNetworkClone() )
	{
		const CPedWeaponManager* pPedWeaponManager = pPed->GetWeaponManager();
		if( pPedWeaponManager && pPedWeaponManager->GetRequiresWeaponSwitch() )
		{
			const CWeaponInfo* pWeaponInfo =  pPedWeaponManager->GetEquippedWeaponInfo();
			if( pWeaponInfo && !pWeaponInfo->GetIsMelee() ) 
			{
				taskDebugf1("Quit: CTaskMelee::WaitForMeleeAction_OnUpdate: pWeaponInfo && !pWeaponInfo->GetIsMelee()");
				SetState( State_Finish );
				return FSM_Continue;
			}
		}

		//! If locked on to a friendly ped (e.g. chop) bail out of melee task (indicates we locked on after melee happened).
		if(pPed->IsLocalPlayer())
		{
			CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();
			if(rTargeting.GetLockOnTarget() && rTargeting.GetLockOnTarget()->GetIsTypePed())
			{
				CPed* pTargetPed = static_cast<CPed*>(rTargeting.GetLockOnTarget());
				if(pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowPlayerLockOnIfFriendly))
				{
					taskDebugf1("Quit: CTaskMelee::WaitForMeleeAction_OnUpdate: CPED_CONFIG_FLAG_AllowPlayerLockOnIfFriendly");
					SetState( State_Finish );
					return FSM_Continue;
				}
			}

#if FPS_MODE_SUPPORTED
			// B*1966069: If FPS swimming, stop waiting for melee action (causes ped and camera to freak out)
			if (pPed->GetIsFPSSwimming() && pPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask))
			{
				SetState( State_Finish );
				return FSM_Continue;
			}
#endif	//FPS_MODE_SUPPORTED
		}
	}

	UpdateBlockingTargetEvaluation( pPed );
		
	return ManageActionSubtaskAndBranches( pPed );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	WaitForMeleeAction_OnExit
// PURPOSE :	Clean up any previously set upperbody anim flags
// PARAMETERS :	pPed -  Ped we are interested in
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::WaitForMeleeAction_OnExit( void )
{
	m_bUsingStealthClipClose = m_bUsingBlockClips = false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InactiveObserverIdle_OnUpdate
// PURPOSE :	Main heartbeat for the Inactive Observer state 
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::InactiveObserverIdle_OnUpdate( CPed* pPed )
{
	// Make the ped try to face the target.
	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
	{
		Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();
		if ( ShouldUpdateDesiredHeading( *pPed, vTargetPosition ) )
		{
			pPed->SetDesiredHeading( VEC3V_TO_VECTOR3( vTargetPosition ) );
		}
	}

	return ManageActionSubtaskAndBranches( pPed );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CheckForActionBranch
// PURPOSE :	Clean up any previously set upperbody anim flags
// PARAMETERS :	pPed -  Ped we are interested in
//				pActionResultTask - Currently running melee action
//				newState - Reference to the new state we would like to transition to
///////////////////////////////////////////////////////////////////////////////// 
bool CTaskMelee::CheckForActionBranch( CPed* pPed, CTaskMeleeActionResult* pActionResultTask, MeleeState& newState)
{
	// Check for any branching off of the current action (including reactions).
	u32	nBranchActionId	= 0;
	if( CheckForBranch( pPed, pActionResultTask, nBranchActionId ) )
	{
		u32 nActionDefnitionIdx = 0;
		const CActionDefinition* pBranchesActionDefinition = ACTIONMGR.FindActionDefinition( nActionDefnitionIdx, nBranchActionId);
		Assert( pBranchesActionDefinition );

		// Get the pointer to the actionResult from the action Id.
		const CActionResult* pBranchesActionResult = pBranchesActionDefinition->GetActionResult( pPed );
		Assertf( pBranchesActionResult, "no suitable action result for definition (%s)\n", pBranchesActionDefinition->GetName() );

		// Check if we should fall out of melee combat or go into the branch...
		if( !pBranchesActionResult )
		{
			// Abort the action task (and possibly go into ragdoll mode).
			if( pActionResultTask && pActionResultTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
			{
				taskDebugf1("Quit: CTaskMelee::CheckForActionBranch: pActionResultTask && pActionResultTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL )");
				newState = State_Finish;
				return true;
			}
		}
		else
		{
			// Set up the reaction info
			m_pNextActionResult = pBranchesActionResult;

			// If we've just entered a held idle, we tell the melee task to reset the combo
			// we do this by saying the new action is an initial combo action
			m_bActionIsBranch = pActionResultTask ? pActionResultTask->IsDoingSomething() : false;

			// Record the type of impulse for combo 1 (combos 2 and 3 can be found in m_impulseCombo
			if( pBranchesActionDefinition && pBranchesActionDefinition->GetImpulseTest() )
			{
				if( !m_bActionIsBranch || pBranchesActionResult->GetShouldResetCombo() )
				{
					if( pBranchesActionDefinition->GetImpulseTest()->GetNumImpulses() > 0 )
						m_impulseCombo.RecordFirstImpulse( pBranchesActionDefinition->GetImpulseTest()->GetImpulseByIndex( 0 )->GetImpulse() );
				}
				else
				{
					// Mark the impulse as executed so that button presses can no longer interrupt the impulse
					const CSimpleImpulseTest* pImpulseTest = pBranchesActionDefinition->GetImpulseTest()->GetNumImpulses() > 0 ? pBranchesActionDefinition->GetImpulseTest()->GetImpulseByIndex( 0 ) : NULL;
					if( pImpulseTest )
					{
						CSimpleImpulseTest::ImpulseState impulseState = pImpulseTest->GetImpulseState();
						if( impulseState >= CSimpleImpulseTest::ImpulseStateCombo1 && impulseState <= CSimpleImpulseTest::ImpulseStateCombo3 )
						{
							s32 iIndex = impulseState - CSimpleImpulseTest::ImpulseStateCombo1;
							if( m_impulseCombo.GetImpulsePtrAtIndex( iIndex ) )
								m_impulseCombo.GetImpulsePtrAtIndex( iIndex )->SetHasExecuted( true );
						}
					}
				}
			}

			if( m_pNextActionResult->GetIsAStealthKill() )
			{
				if( pBranchesActionDefinition->GetImpulseTest() && pBranchesActionDefinition->GetImpulseTest()->GetNumImpulses() > 0 )
					m_impulseCombo.RecordFirstImpulse( pBranchesActionDefinition->GetImpulseTest()->GetImpulseByIndex( 0 )->GetImpulse() );
			}
				
			if(pPed->IsLocalPlayer() && !CNewHud::IsShowingHUDMenu())
			{
				newState = State_MeleeAction;
				return true;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ManageActionSubtaskAndBranches
// PURPOSE :	Main update function in regards to starting subtasks and branching
//				from the current subtask
// PARAMETERS :	pPed -  the ped we are interested in.
///////////////////////////////////////////////////////////////////////////////// 
CTask::FSM_Return CTaskMelee::ManageActionSubtaskAndBranches( CPed* pPed )
{
	// If clone, just take direction from host.
	if( pPed->IsNetworkClone() )
	{
		MeleeState desiredState = State_Invalid;
		if( m_bLocalReaction && m_pNextActionResult )
		{
			MELEE_NETWORK_DEBUGF("[%d] CTaskMelee::ManageActionSubtaskAndBranches - Player = %s State = %d", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), GetState());

			m_bLocalReaction = false;
			desiredState = State_MeleeAction;
		}
		else
		{
			if( CheckCloneForLocalBranch( pPed ) )
				desiredState = State_MeleeAction;
		}

		if( desiredState != State_Invalid )
		{
			if( desiredState == GetState() )
				SetFlag( aiTaskFlags::RestartCurrentState );
			else
				SetState( desiredState );
		}

		return FSM_Continue;
	}

	// Make sure we don't want to stop this task (i.e. check for times when
	// the ped should stop being in fight mode).
	MeleeState desiredState = GetDesiredStateAndUpdateBreakoutConditions( pPed );
	if( desiredState != State_Invalid )
	{
		if( desiredState == GetState() )
			SetFlag( aiTaskFlags::RestartCurrentState );
		else
			SetState( desiredState );

		return FSM_Continue;
	}

	// Get the action task.
	CTaskMeleeActionResult* pActionResultTask = NULL;
	CTask* pSubTask = GetSubTask();
	if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT )
		pActionResultTask = static_cast<CTaskMeleeActionResult*>( pSubTask );

	// Update the movement task, if it is the Melee movement task & not controlled by a CtrlMove task.
	if( pPed->IsPlayer() || (GetQueueType() > QT_InactiveObserver) )
	{
		bool bAllowMovementInput = !ShouldBeInInactiveTauntMode() && !GetIsTaskFlagSet( MF_PlayTauntBeforeAttacking );
		bool bAllowStrafing = GetIsTaskFlagSet( MF_AllowStrafeMode );
		bool bAllowHeadingUpdate = true;
		bool bAllowStealth = GetIsTaskFlagSet( MF_AllowStealthMode );

		// Get the action task.
		if( pActionResultTask )
		{
			// Make sure the simple action has the same target as the melee task.
			pActionResultTask->SetTargetEntity( GetTargetEntity(), HasLockOnTargetEntity() );

			bAllowMovementInput = pActionResultTask->AllowMovement();
			bAllowStrafing = pActionResultTask->AllowStrafing();
			bAllowHeadingUpdate = pActionResultTask->AllowHeadingUpdate();
			bAllowStealth = pActionResultTask->AllowStealthMode();
		}
		// Special case code to prevent animals from doing small heading updates
		else if( pPed->GetPedType() == PEDTYPE_ANIMAL && m_pTargetEntity )
		{
			static dev_float sfHeadingUpdateRadThreshold = 0.35f; // 20 degrees
			Vec3V vTargetPosition = m_pTargetEntity->GetTransform().GetPosition();
			Vec3V vPedPosition = pPed->GetTransform().GetPosition();
			float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints( vTargetPosition.GetXf(), vTargetPosition.GetYf(), vPedPosition.GetXf(), vPedPosition.GetYf() );
			const float fAngle = SubtractAngleShorter( pPed->GetCurrentHeading(), fDesiredHeading );
			if( fabs( fAngle ) < sfHeadingUpdateRadThreshold )
			{
				bAllowHeadingUpdate = false;
			}
		}

		MeleeMoveTaskControl( pPed, bAllowMovementInput, bAllowStrafing, bAllowHeadingUpdate, bAllowStealth, m_pAvoidEntity );
	}

	// Check for an action branch
	desiredState = State_Invalid;
	if( !m_pNextActionResult && CheckForActionBranch( pPed, pActionResultTask, desiredState ) )
	{
		taskAssertf( desiredState != State_Invalid, "Branch activated but no state returned!" );
		if( desiredState == GetState() )
			SetFlag( aiTaskFlags::RestartCurrentState );
		else
			SetState( desiredState );
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateBlocking
// PURPOSE :	(PLAYER ONLY) In charge of monitoring blocking functionality
//				including animations.
// PARAMETERS :	pPed -  the ped we are interested in.
//				bManageUpperbodyAnims - Whether or not we should manage the upperbody anims
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::UpdateBlocking( CPed* pPed, bool bManageUpperbodyAnims )
{
	// Do not allow blocking for weapons that do not support it
	const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if( !pEquippedWeaponInfo || !pEquippedWeaponInfo->GetAllowMeleeBlock() )
		return;

	bool bHasChanged = m_bUsingStealthClipClose;
	m_bUsingStealthClipClose = false;
	const CControl* pControl = pPed->GetControlFromPlayer();
	if( pControl )
	{
		// Tick block timer
		ioValue const * ioVal0 = NULL;
		ioValue const * ioVal1 = NULL;
		CSimpleImpulseTest::GetIOValuesForImpulse( pControl, CSimpleImpulseTest::ImpulseBlock, ioVal0, ioVal1 );
		if( ioVal0->IsDown() )
		{
			if( !IsBlocking() )
			{
				m_fBlockDelayTimer += fwTimer::GetTimeStep();
				if( m_fBlockDelayTimer > CTaskMelee::sm_fBlockTimerDelayEnter )
				{
					m_bIsBlocking = true; 
					m_fBlockDelayTimer = CTaskMelee::sm_fBlockTimerDelayExit;
				}
			}
			else
				m_fBlockDelayTimer = CTaskMelee::sm_fBlockTimerDelayExit;
		}
		else
		{
			if( IsBlocking() )
			{
				m_fBlockDelayTimer -= fwTimer::GetTimeStep();
				if( m_fBlockDelayTimer < 0.0f )
				{
					m_fBlockDelayTimer = 0.0;
					m_bIsBlocking = false; 
				}
			}
			else
				m_fBlockDelayTimer = 0.0;
		}
	}

	if( bManageUpperbodyAnims )
	{
		// Update blocking anims
		aiTask* pSubTask = GetSubTask();
		if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_UPPERBODY_ANIM  )
		{
			if( pSubTask->GetIsFlagSet( aiTaskFlags::HasBegun ) )
			{
				if( ( !IsBlocking() || bHasChanged ) )
				{
					pSubTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL );
				}
#if FPS_MODE_SUPPORTED
				else if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
				{
					// Need to bring in left hand IK ourselves which can only be processed later in the frame
					pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
				}
#endif
			}
		}
		else if( IsBlocking() )
		{
			const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if( pEquippedWeaponInfo )
			{
				fwMvFilterId filterId("Upperbody_filter",0x35B1D869);
				float fBlendDuration = NORMAL_BLEND_DURATION;
#if FPS_MODE_SUPPORTED
				// In first person when we start up the upper body animations that will automatically switch us from the aiming state to the
				// strafe state.  Need to ensure that we skip the strafe into anim in this case
				if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
				{
					fBlendDuration = 0.3f;
					CTaskMotionPed* pCurrentMotionTask = static_cast<CTaskMotionPed*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_PED));
					if( pCurrentMotionTask )
					{
						pCurrentMotionTask->SetTaskFlag( CTaskMotionPed::PMF_SkipStrafeIntroAnim );
					}

					// Need to bring in left hand IK ourselves which can only be processed later in the frame
					pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
				}
#endif
				//! Blocking flag can be out of sync with equipped weapon, so ensure we have anims 1st.
				if(fwAnimManager::GetClipIfExistsBySetId(pEquippedWeaponInfo->GetMeleeClipSetId(*pPed), CLIP_MELEE_BLOCK_INDICATOR))
				{
					SetNewTask( rage_new CTaskMeleeUpperbodyAnims( pEquippedWeaponInfo->GetMeleeClipSetId(*pPed), CLIP_MELEE_BLOCK_INDICATOR, filterId, fBlendDuration, CTaskMeleeUpperbodyAnims::AF_IsLooped ) );
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateBlockingTargetEvaluation
// PURPOSE :	(PLAYER ONLY) In charge of switching to a better target when we
//				attempt to block/dodge.
// PARAMETERS :	pPed -  the ped we are interested in.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::UpdateBlockingTargetEvaluation(CPed* pPed)
{
	const CControl* pControl = pPed->GetControlFromPlayer();
	if( pControl )
	{
		ioValue const * ioVal0 = NULL;
		ioValue const * ioVal1 = NULL;
		CSimpleImpulseTest::GetIOValuesForImpulse( pControl, CSimpleImpulseTest::ImpulseBlock, ioVal0, ioVal1 );
		if( ioVal0->IsPressed() )
		{
			//! We are attempting to dodge - allow ped to re-pick lock on target.
			if(pPed->IsLocalPlayer())
			{
				CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();
				if(rTargeting.GetLockOnTarget())
				{
					CEntity *pTargetEntity = rTargeting.FindLockOnMeleeTarget(pPed);
					if(pTargetEntity)
					{
						SetTargetEntity(pTargetEntity, true, true);
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateStealth
// PURPOSE :	(PLAYER ONLY) In charge of monitoring stealth functionality
//				including animations.
// PARAMETERS :	pPed -  the ped we are interested in.
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMelee::UpdateStealth( CPed* pPed )
{
	bool bHasChanged = m_bCanBlock || m_bIsBlocking;
	m_bIsBlocking = false;
	CImpulseCombo defaultStealthImpulse( CSimpleImpulseTest::ImpulseAttackLight );
	CActionFlags::ActionTypeBitSetData typeToLookFor;
	typeToLookFor.Set( CActionFlags::AT_MELEE, true );
	const CActionDefinition* pActionDef = ACTIONMGR.SelectSuitableStealthKillAction( pPed, GetTargetEntity(), true, &defaultStealthImpulse, typeToLookFor );
	if( pActionDef != NULL )
		m_bUsingStealthClipClose = true;
	else	
	{
		bHasChanged |= m_bUsingStealthClipClose;
		m_bUsingStealthClipClose = false;
	}

	// Update blocking anims
	aiTask* pSubTask = GetSubTask();
	if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE_UPPERBODY_ANIM  )
	{
		if( pSubTask->GetIsFlagSet( aiTaskFlags::HasBegun ) && bHasChanged )
			pSubTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL );
	}
	else if( m_bUsingStealthClipClose )
	{
		const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if( pEquippedWeaponInfo )
		{
			bool bAnimsStreamed = false;
			fwMvFilterId filterId( "UpperbodyFeathered_filter", 0xfcdd409 );
			fwMvClipSetId weaponClipSetId;
			strLocalIndex nAnimBlockIndex = strLocalIndex(-1);
			if( pEquippedWeaponInfo->GetIsUnarmed() || pEquippedWeaponInfo->GetIsMeleeFist())
			{
				weaponClipSetId = pEquippedWeaponInfo->GetMeleeBaseClipSetId( *pPed );
				if(weaponClipSetId == CLIP_SET_ID_INVALID)
				{
					weaponClipSetId = CLIP_SET_MELEE_UNARMED_BASE;
				}
			}
			else
			{	
				weaponClipSetId = pEquippedWeaponInfo->GetMeleeClipSetId( *pPed );
			}

			if( weaponClipSetId != CLIP_SET_ID_INVALID )
				nAnimBlockIndex = fwClipSetManager::GetClipDictionaryIndex( weaponClipSetId );

			// If we are not yet streamed in always use the base resident version
			if( nAnimBlockIndex != -1 && CStreaming::HasObjectLoaded( nAnimBlockIndex, fwAnimManager::GetStreamingModuleId() ) )
			{
				bAnimsStreamed = true;
			}

			if( bAnimsStreamed )
			{
				SetNewTask( rage_new CTaskMeleeUpperbodyAnims( weaponClipSetId, CLIP_MELEE_STEALTH_KILL_INDICATOR_CLOSE, filterId, SLOW_BLEND_DURATION, CTaskMeleeUpperbodyAnims::AF_IsLooped ) );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldAssistMotionTask
// PURPOSE :	Check the motion task and see if it needs help reaching
//				the target.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::ShouldAssistMotionTask()
{
	CPed* pPed = GetPed();
	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	return pTask && pTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_QUAD;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldUpdateDesiredHeading
// PURPOSE :	Check if the ped needs to update its heading towards the target.
//				This is done to reduce "swiveling"
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::ShouldUpdateDesiredHeading(const CPed& ped, Vec3V_In vTargetPosition)
{
	// Non dogs don't have a problem with this.
	if (!ped.GetTaskData().GetIsFlagSet(CTaskFlags::UseLooseHeadingAdjustmentsInMelee))
	{
		return true;
	}
	
	float fCurrentHeading = ped.GetCurrentHeading();

	// Compute our current heading offset.
	float fTargetHeading = fwAngle::GetRadianAngleBetweenPoints(vTargetPosition.GetXf(), vTargetPosition.GetYf(), ped.GetTransform().GetPosition().GetXf(), ped.GetTransform().GetPosition().GetYf());

	// Check if it is big enough.
	if (fabs(SubtractAngleShorter(fCurrentHeading, fTargetHeading)) > DtoR * sm_fLookAtTargetLooseToleranceDegrees)
	{
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessMotionTaskAssistance
// PURPOSE :	Set parameters to help the motion task in reaching the target.
// PARAMETERS :	bAssistance - whether assistance is being set or unset.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::ProcessMotionTaskAssistance(bool bAssitance)
{
	CPed* pPed = GetPed();
	
	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

	if (pTask &&pTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_QUAD)
	{
		CTaskQuadLocomotion* pTaskQuadLocomotion = static_cast<CTaskQuadLocomotion*>(pTask);
		pTaskQuadLocomotion->SetUsePursuitMode(bAssitance);
	}
}

bool CTaskMelee::CanChangeTarget() const
{
	if(GetEntity())
	{
		if(const CPed* pPed = GetPed())
		{
			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontChangeTargetFromMelee))
			{
				if(const CTaskCombat* pCombatTask = pPed->GetPedIntelligence()->GetTaskCombat())
				{
					if(pCombatTask->GetIsPreventChangingTargetSet() && m_pTargetEntity && m_pTargetEntity.Get() == pCombatTask->GetPrimaryTarget())
					{
						AI_LOG_WITH_ARGS("CTaskMelee::CanChangeTarget() returning false - pPed = %s, m_pTargetEntity = %s, task = %p \n",pPed->GetLogName(), m_pTargetEntity->GetLogName(), this);
						return false;
					}
					AI_LOG_WITH_ARGS("CTaskMelee::CanChangeTarget() returning true - pPed = %s, m_pTargetEntity = %s, PrimaryTarget = %s, GetIsPreventChangingTargetSet = %s, task = %p \n", pPed->GetLogName(), m_pTargetEntity ? m_pTargetEntity->GetLogName() : "Null", pCombatTask->GetPrimaryTarget() ? pCombatTask->GetPrimaryTarget()->GetLogName() : "Null", pCombatTask->GetIsPreventChangingTargetSet() ? "TRUE" : "FALSE", this);
				}
			}
		}
	}
	else
	{
		AI_LOG_WITH_ARGS("CTaskMelee::CanChangeTarget() returning true, couldn't get entity - m_pTargetEntity = %s, task = %p \n", m_pTargetEntity ? m_pTargetEntity->GetLogName() : "Null", this);
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// CTaskMeleeUpperbodyAnims
/////////////////////////////////////////////////////////////////////////////////

const fwMvClipId CTaskMeleeUpperbodyAnims::ms_UpperbodyClipId("UpperbodyClipId",0xFF6318E6);
const fwMvFilterId CTaskMeleeUpperbodyAnims::ms_UpperbodyFilterId("UpperbodyFilterId",0xABFB0DAF);
const fwMvBooleanId CTaskMeleeUpperbodyAnims::ms_IsClipLoopedId("IsClipLoopedId",0x1AD1A9D8);
const fwMvBooleanId CTaskMeleeUpperbodyAnims::ms_AnimClipFinishedId("OnClipFinished",0x88B0DC15);
const fwMvFlagId CTaskMeleeUpperbodyAnims::ms_HasGrip("HasGrip",0x1bcd14c6);
const fwMvClipSetVarId CTaskMeleeUpperbodyAnims::ms_GripClipSetId("GripClipSet", 0xcbd9a0f0);

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CTaskMeleeUpperbodyAnims ctor
// PURPOSE :	Used to define an upperbody animation which can be looped
///////////////////////////////////////////////////////////////////////////////// 
CTaskMeleeUpperbodyAnims::CTaskMeleeUpperbodyAnims( const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, const fwMvFilterId& filterId, float fBlendDuration, fwFlags32 iFlags)
: CTask()
, m_ClipSetId( clipSetId )
, m_ClipId( clipId )
, m_FilterId( filterId )
, m_pFilter( NULL )
, m_fBlendDuration( fBlendDuration )
, m_nFlags( iFlags )
{
	SetInternalTaskType(CTaskTypes::TASK_MELEE_UPPERBODY_ANIM);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CleanUp
// PURPOSE :	Main CTask clean up function in charge of shutting down anything
//				related to CTaskMeleeUpperbodyAnims
///////////////////////////////////////////////////////////////////////////////// 
void CTaskMeleeUpperbodyAnims::CleanUp( void )
{
	CPed* pPed = GetPed();
	if( pPed && m_MoveNetworkHelper.GetNetworkPlayer() )
	{
		pPed->GetMovePed().ClearTaskNetwork( m_MoveNetworkHelper, m_fBlendDuration );
		m_MoveNetworkHelper.ReleaseNetworkPlayer();
	}

	if( m_pFilter )
	{
		m_pFilter->Release();
		m_pFilter = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateFSM
// PURPOSE :	Main heartbeat function
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeUpperbodyAnims::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed* pPed = GetPed();
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate( pPed );
		FSM_State(State_PlayAnim)
			FSM_OnUpdate
				return PlayAnim_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}
	
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Start_OnUpdate
// PURPOSE :	Preps the MoVE network associated with this instance 
// PARAMETERS :	pPed - Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeUpperbodyAnims::Start_OnUpdate( CPed* pPed )
{
	if( PrepareMoveNetwork( pPed ) )
	{
		SetState( State_PlayAnim );
	}

	return FSM_Continue;
}
	
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PlayAnim_OnUpdate
// PURPOSE :	Monitors when the animation finishes (non looping) otherwise will
//				wait for an outside influence to quit.
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeUpperbodyAnims::PlayAnim_OnUpdate()
{
	taskAssert( m_MoveNetworkHelper.IsNetworkActive() );

	if( m_MoveNetworkHelper.GetBoolean( ms_AnimClipFinishedId ) )
		SetState( State_Finish );

	return FSM_Continue;
}

#if !__FINAL

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetStaticStateName
// PURPOSE:		Simply returns the state name
// PARAMETERS:	iState - State we are interested in
/////////////////////////////////////////////////////////////////////////////////
const char* CTaskMeleeUpperbodyAnims::GetStaticStateName( s32 iState  )
{
	switch( iState )
	{
		case State_Start:			return "State_Start";
		case State_PlayAnim:		return "State_PlayAnim";
		case State_Finish:			return "State_Finish";
		default: taskAssert(0);
	}

	return "State_Invalid";
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrepareMoveNetwork
// PURPOSE :	Initialize the move network associated with this upperbody animation
// PARAMETERS :	pPed - the ped of interest.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeUpperbodyAnims::PrepareMoveNetwork( CPed* pPed )
{
	// Wait until the network worker is streamed in and initialized 
	if ( !m_MoveNetworkHelper.IsNetworkActive() )
	{		
		m_MoveNetworkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkTaskMeleeUpperbodyAnims );
		if( m_MoveNetworkHelper.IsNetworkDefStreamedIn( CClipNetworkMoveInfo::ms_NetworkTaskMeleeUpperbodyAnims ) )
		{
			m_MoveNetworkHelper.CreateNetworkPlayer( pPed, CClipNetworkMoveInfo::ms_NetworkTaskMeleeUpperbodyAnims );
			pPed->GetMovePed().SetTaskNetwork( m_MoveNetworkHelper.GetNetworkPlayer(), m_fBlendDuration, CMovePed::Task_None );

			// ClipSet and Clip
			fwClipSet* pSet = fwClipSetManager::GetClipSet( m_ClipSetId );

			//! If we can't find clip (which can happen for clones if they are holding wrong weapon), fall back to a known clip set 
			//! that works. 
			if(pSet && pSet->Request_DEPRECATED())
			{
				const crClip* pClip = fwClipSetManager::GetClip( m_ClipSetId, m_ClipId );
				if(!pClip)
				{
					static const fwMvClipSetId CLIP_SET_MELEE_FALLBACK("MELEE@UNARMED@STREAMED_CORE",0xfd5a244a);
					m_ClipSetId = CLIP_SET_MELEE_FALLBACK;
					pSet = fwClipSetManager::GetClipSet( m_ClipSetId );
				}
			}

			taskAssertf( pSet, "Failed to find clip set %s", m_ClipSetId.GetCStr() );

			if( pSet && pSet->Request_DEPRECATED() )
			{
				m_MoveNetworkHelper.SetClipSet( m_ClipSetId );
				const crClip* pClip = fwClipSetManager::GetClip( m_ClipSetId, m_ClipId );
				Assertf( pClip, "Failed to find clip %s in clip set %s", m_ClipId.GetCStr(), m_ClipSetId.GetCStr() );
				if( pClip )
				{
					m_MoveNetworkHelper.SetClip( pClip, ms_UpperbodyClipId );
					m_MoveNetworkHelper.SetBoolean( ms_IsClipLoopedId, m_nFlags.IsFlagSet( AF_IsLooped ) );
				}
			}

			// Anim filter
			if( m_FilterId.GetHash() != 0 )
			{
				m_pFilter = CGtaAnimManager::FindFrameFilter( m_FilterId.GetHash(), pPed );
				if( m_pFilter )
					m_pFilter->AddRef();

				m_MoveNetworkHelper.SetFilter( m_pFilter, ms_UpperbodyFilterId );
			}

			bool bUseGrip = false;
			if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponInfo() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsMeleeFist())
			{
				fwMvClipSetId gripClipSetId = pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetMeleeClipSetId(*pPed);
				if (gripClipSetId != CLIP_SET_ID_INVALID)
				{
					fwMvClipId gripClipId("GRIP_IDLE", 0x3ec63b58);
					const crClip* pGripClip = fwClipSetManager::GetClip( gripClipSetId, gripClipId );
					if (pGripClip != NULL)
					{
						bUseGrip = true;
						m_MoveNetworkHelper.SetClipSet(gripClipSetId, ms_GripClipSetId);

						crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(gripClipSetId);
						if (pFilter)
						{
							m_MoveNetworkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
						}
					}
				}
			}

			m_MoveNetworkHelper.SetFlag( bUseGrip, ms_HasGrip );


			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// CTaskMoveMeleeMovement
//
// Enables individual peds to move around while in combat.  This
// is usually running underneath any action moves.  The action moves always
// have higher priority and override the clip being provided by this.
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CTaskMoveMeleeMovement
// PURPOSE :	The default constructor.
/////////////////////////////////////////////////////////////////////////////////
CTaskMoveMeleeMovement::CTaskMoveMeleeMovement( CEntity* pTargetEntity, bool bHasLockOnTargetEntity, bool bAllowMovement, bool bAllowStrafing, bool bAllowHeadingUpdate, bool bAllowStealth )
: // Initializer list.
CTaskMove								(MOVEBLENDRATIO_STILL),
m_bIsStrafing							(bAllowStrafing),
m_pTargetEntity							(NULL),		// Set in function body, below.
m_bHasLockOnTargetEntity				(false),	// Set in function body, below.
m_fTargetSeparationDesiredRange			(1.0f),
m_fTargetSeparationMinRangeForImpetus	(0.2f),
m_fTargetSeparationMaxRangeForImpetus	(1.5f),
m_pAvoidEntity							(NULL),
m_fAvoidEntityDesiredRange				(1.0f),
m_fAvoidEntityMinRangeForImpetus		(0.2f),
m_fAvoidEntityMaxRangeForImpetus		(1.5f),
m_vPedMoveMomentumDir					(VEC3_ZERO),
m_fPedMoveMomentumPortion				(0.0f),
m_fPedMoveMomentumTime					(0.0f),
m_bIsDoingSomething						(false),
m_bAllowMovement						(bAllowMovement),
m_bAllowMovementInput					(bAllowMovement),
m_bAllowStrafing						(bAllowStrafing),
m_bAllowHeadingUpdate					(bAllowHeadingUpdate),
m_bAllowStealth							(bAllowStealth),
m_bForceRun								(false),
m_bCanMoveWhenTargetEnteringVehicle		(true),
m_fStickAngle							(0.0f),
m_bStickAngleTweaked					(false),
m_uStealthLastPressed					(0)
{
	// Update the old target and the new target.
	SetTargetEntity(pTargetEntity, bHasLockOnTargetEntity);

	SetInternalTaskType(CTaskTypes::TASK_MOVE_MELEE_MOVEMENT);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Copy
// PURPOSE :	The cloning operation- used by the task management system.
// RETURNS :	An exact copy of the task.
/////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskMoveMeleeMovement::Copy() const
{
	return rage_new CTaskMoveMeleeMovement( GetTargetEntity(), HasLockOnTargetEntity(), m_bAllowMovement, m_bAllowStrafing, m_bAllowHeadingUpdate, m_bAllowStealth );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetTarget
// PURPOSE :	Find out what the target position currently is.
// RETURNS :	The current target position.
/////////////////////////////////////////////////////////////////////////////////
Vector3 CTaskMoveMeleeMovement::GetTarget( void ) const
{
	Assertf( false, "CTaskMoveMeleeMovement : GetTarget() was called on move task with no target.  Need to call HasTarget() on this task first to determine if GetTarget() is valid." );
	return VEC3_ZERO;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetTargetRadius
// PURPOSE :	Find out what the target radius currently is.
// RETURNS :	The current target radius.
/////////////////////////////////////////////////////////////////////////////////
float CTaskMoveMeleeMovement::GetTargetRadius( void ) const
{
	Assertf( false, "CTaskMoveMeleeMovement : GetTargetRadius() was called on move task with no target.  Need to call HasTarget() on this task first to determine if GetTargetRadius() is valid." );
	return 0.0f;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateFSM
// PURPOSE :	Main heartbeat function
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMoveMeleeMovement::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	FSM_Begin
		FSM_State(Running)
			FSM_OnEnter
				StateRunning_OnEnter( pPed );
			FSM_OnUpdate
				return StateRunning_OnUpdate( pPed );
	FSM_End
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	StateRunning_OnEnter
// PURPOSE :	Randomize phase at which we will blend into strafing
// PARAMETERS :	pPed - the ped we are interest in.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::StateRunning_OnEnter( CPed* pPed )
{
	aiTask* pMotionPedTask = pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive( PED_TASK_TREE_MOTION, CTaskTypes::TASK_MOTION_PED );
	if( pMotionPedTask )
	{
		CTaskMotionPed* pCurrentMotionTask = static_cast<CTaskMotionPed*>(pMotionPedTask );
		if( pCurrentMotionTask && !pCurrentMotionTask->IsTaskFlagSet( CTaskMotionPed::PMF_SkipStrafeIntroAnim ) )
			pCurrentMotionTask->SetInstantStrafePhaseSync( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	StateRunning_OnUpdate
// PURPOSE :	Updates the peds movement (clips and position).
// PARAMETERS :	pPed - the ped we are interest in.
// RETURNS :	Whether or not this task is done.
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMoveMeleeMovement::StateRunning_OnUpdate( CPed* pPed )
{
	Assertf( pPed, "CTaskMoveMeleeMovement::StateRunning_OnUpdate - pPed is invalid." );
	DEV_BREAK_IF_FOCUS( CCombatMeleeDebug::sm_bFocusPedBreakOnTaskMeleeMovementStateRunning, pPed );

	CEntity* pTargetEntity = GetTargetEntity();
	if( pPed->IsLocalPlayer() )
	{
		// Set the player into strafing or walkabout mode.
		// Handle changing the facing angle if strafing.
		m_bIsStrafing = ShouldAllowStrafing();
		if( m_bIsStrafing && pTargetEntity )
		{
			if( ShouldAllowMovementInput() )
				pPed->SetDesiredHeading( VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() ) );
		}

		// Handle the movement directives from the player.
		if( m_bIsStrafing )
			ProcessStrafeMove( pPed );
		else
			ProcessStdMove( pPed );

		// Only process stealth mode inputs while they are allowed
		if( ShouldAllowStealth() )
			ProcessStealthMode( pPed );
	}
	else // This is an NPC...
	{
		// Note: This is really one of the major parts of our "Melee Combat AI"...
		// Search for the quoted term in the line above to find the others.

		// Make the ped try to face the target.
		if( ShouldAllowHeadingUpdate() && pTargetEntity && CTaskMelee::ShouldUpdateDesiredHeading( *pPed, pTargetEntity->GetTransform().GetPosition() ) )
		{
			pPed->SetDesiredHeading( VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() ) );
		}
		else if ( pPed->GetTaskData().GetIsFlagSet( CTaskFlags::UseLooseHeadingAdjustmentsInMelee ) )
		{
			pPed->SetDesiredHeading( pPed->GetCurrentHeading() );
		}

		// Should we allow strafing?
		m_bIsStrafing = ShouldAllowStrafing();
		pPed->GetMotionData()->SetIsStrafing( m_bIsStrafing );
		
		if( DEV_ONLY( CCombatMeleeDebug::sm_bAllowNpcInitiatedMovement && )
			CanMoveWhenTargetEnteringVehicle() &&
			pTargetEntity &&
			ShouldAllowMovement() )
		{
			// Determine how much in each direction we want to move.
			float fForwardMoveComponent = 0.0f;
			float fRightwardMoveComponent = 0.0f;
			if( ShouldAllowMovementInput() )
			{
				Vec3V vPedPosition = pPed->GetTransform().GetPosition();
				Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();
				Vec3V vForwardHeading = pPed->GetTransform().GetB();
				Vec3V vRightwardHeading = pPed->GetTransform().GetA();
			
				// Determine how much in each direction we want to move to
				// make the ped try to maintain the desired distance to the target.
				float fForwardTargetBasedMoveComponent = 0.0f;
				float fRightwardTargetBasedMoveComponent = 0.0f;

				const CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea();
				GetPrefdDistanceMoveComponents(	fForwardTargetBasedMoveComponent,// Out
												fRightwardTargetBasedMoveComponent,// Out
												vPedPosition,
												vForwardHeading,
												vRightwardHeading,
												vTargetPosition,
												m_fTargetSeparationDesiredRange,
												false,
												m_fTargetSeparationMinRangeForImpetus,
												m_fTargetSeparationMaxRangeForImpetus,
												CTaskMelee::sm_fAiTargetSeparationForwardImpetusPowerFactor,
												CTaskMelee::sm_fAiTargetSeparationBackwardImpetusPowerFactor,
												CTaskMelee::sm_fAiTargetSeparationMinTargetApproachDesireSize,
												pDefensiveArea );

				// Determine how much in each direction we want to move to
				// make the ped try to maintain the desired distance from the avoid entity.
				float fForwardAvoidBasedMoveComponent	= 0.0f;
				float fRightwardAvoidBasedMoveComponent	= 0.0f;
				if( m_pAvoidEntity )
				{
					Vec3V vAvoidCenter = m_pAvoidEntity->GetTransform().GetPosition();
					GetPrefdDistanceMoveComponents(	fForwardAvoidBasedMoveComponent,// Out
													fRightwardAvoidBasedMoveComponent,// Out
													vPedPosition,
													vForwardHeading,
													vRightwardHeading,
													vAvoidCenter,
													CTaskMelee::sm_fAiAvoidSeparationDesiredSelfToTargetRange,
													true,
													CTaskMelee::sm_fAiAvoidSeparationPrefdDiffMinForImpetus,
													CTaskMelee::sm_fAiAvoidSeparationPrefdDiffMaxForImpetus,
													CTaskMelee::sm_fAiAvoidSeparationForwardImpetusPowerFactor,
													CTaskMelee::sm_fAiAvoidSeparationBackwardImpetusPowerFactor,
													CTaskMelee::sm_fAiAvoidSeparationMinTargetApproachDesireSize,
													pDefensiveArea );

					// Add some extra movement left or right to make us circle around the avoid entity if they are ahead of us.
					Vec3V vAvoidToSelf = vAvoidCenter - vPedPosition;
					if( MagSquared( vAvoidToSelf ).Getf() < rage::square( CTaskMelee::sm_fAiAvoidSeparationPrefdDiffMinForImpetus ) )
					{
						Vec3V vAvoidToSelf = vTargetPosition - vAvoidCenter;
						Vec3V vAvoidToTargetDir = Normalize( vAvoidToSelf );
						
						Vec3V vSelfToTarget = vTargetPosition - vPedPosition;
						Vec3V vSelfToTargetDir = Normalize( vSelfToTarget );
						
						const float fAlignment = Dot( vSelfToTargetDir, vAvoidToTargetDir ).Getf();
						Vec3V vCross = Cross( vSelfToTargetDir, vAvoidToTargetDir );
						const float fSide = Sign( vCross.GetZf() );

						// Add the circling impetus.
						fRightwardAvoidBasedMoveComponent -= ( fSide * fAlignment * CTaskMelee::sm_fAiAvoidSeparationCirlclingStrength );
					}
				}

				// Sum and clamp the different movement desires.
				fForwardMoveComponent	= Clamp( ( fForwardTargetBasedMoveComponent + fForwardAvoidBasedMoveComponent), -1.0f, 1.0f );
				fRightwardMoveComponent	= Clamp( ( fRightwardTargetBasedMoveComponent + fRightwardAvoidBasedMoveComponent), -1.0f, 1.0f );
			}

			// Mark whether we tried to move of our own accord or not.
			m_bIsDoingSomething = ( fForwardMoveComponent != 0.0f ) || ( fRightwardMoveComponent != 0.0f );

			// Set up the desired move blend:
			// (x = left right 0 to 1; y = up down 0 to 1;)
			Vector2 vDesiredMoveBlend( fRightwardMoveComponent, fForwardMoveComponent );

			const CBrawlingStyleData* pData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );

			// Fixup the desired move blend.
			if( vDesiredMoveBlend.Mag2() > 1.0f )
				vDesiredMoveBlend /= vDesiredMoveBlend.Mag();

			if( vDesiredMoveBlend.Mag2() > 0.0f )
				vDesiredMoveBlend *= pData ? pData->GetMeleeMovementMBR() : MOVEBLENDRATIO_RUN;

			// Set the desired move blend (which is smoothed internally) and force
			// the momentum (hit) based movement (which won't be smoothed internally).
			pPed->GetMotionData()->SetDesiredMoveBlendRatio( vDesiredMoveBlend.y, vDesiredMoveBlend.x );
			float fMoveBlendRatio;
			float fLateralMoveBlendRatio;
			pPed->GetMotionData()->GetDesiredMoveBlendRatio( fLateralMoveBlendRatio, fMoveBlendRatio );
			AppliedPedMoveMomentumMoveBlendTweak( pPed, &fMoveBlendRatio, &fLateralMoveBlendRatio );
			pPed->GetMotionData()->SetCurrentMoveBlendRatio( fMoveBlendRatio, fLateralMoveBlendRatio );

			Vector2 vMBR( fLateralMoveBlendRatio, fMoveBlendRatio );
			m_fMoveBlendRatio = vMBR.Mag();
		}
		else
		{
			m_bIsDoingSomething = false;
			pPed->GetMotionData()->SetDesiredMoveBlendRatio( 0.0f, 0.0f );
			pPed->GetMotionData()->SetCurrentMoveBlendRatio( 0.0f, 0.0f );
			m_fMoveBlendRatio = 0.0f;
		}
	}

	// Make sure the applied momentum decays over time.
	UpdateAppliedPedMoveMomentum();
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetTargetEntity
// PURPOSE :	Set what the target entity currently is and whether we have a
//				lock on it or not.
// PARAMETERS :	pTargetEntity - the target.
//				bHasLockOnTargetEntity - whether or not a lock on is being applied.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::SetTargetEntity( CEntity* pTargetEntity, bool bHasLockOnTargetEntity )
{
	// Just set the has lock on flag to its current state (allowing us to
	// acquire and break locks on a given ped even when they are acquired as
	// an ambient target).
	m_bHasLockOnTargetEntity = bHasLockOnTargetEntity;
	m_pTargetEntity = pTargetEntity;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetAppliedPedMoveMomentum
// PURPOSE :	Set the amount of external momentum to apply to the ped.  This is
//				usually used to simulate movement due to being hit by something.
// PARAMETERS :	dir - the direction of the desired movement.
//				momentumPortion - how much movement to apply.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::SetAppliedPedMoveMomentum( Vector3& vDir, float fPedMoveMomentumPortion )
{
	m_vPedMoveMomentumDir		= vDir;
	m_fPedMoveMomentumPortion	= fPedMoveMomentumPortion;
	m_fPedMoveMomentumTime		= m_fPedMoveMomentumPortion;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateAppliedPedMoveMomentum
// PURPOSE :	Update the affect of the external momentum applied.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::UpdateAppliedPedMoveMomentum( void )
{
	static float fDecayAmountPerSecond = 1.0f;
	m_fPedMoveMomentumTime -= ( fDecayAmountPerSecond * fwTimer::GetTimeStep() );
	if( m_fPedMoveMomentumTime <= 0.0f )
		m_fPedMoveMomentumTime = 0.0f;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AppliedPedMoveMomentumMoveBlendTweak
// PURPOSE :	Alter the move blend ratios to account for the external momentum
//				affects being applied.
// PARAMETERS :	pPed - the ped of interest.
//				fMoveBlendRatio - the forward back movement blend amount.
//				fLateralMoveBlendRatio - the side to side movement blend amount.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::AppliedPedMoveMomentumMoveBlendTweak( CPed* pPed, float* fMoveBlendRatio, float* fLateralMoveBlendRatio )
{
	Assertf( pPed, "pPed is null in CTaskMoveMeleeMovement::AppliedPedMoveMomentumMoveBlendTweak." );
	Assertf( fMoveBlendRatio, "fMoveBlendRatio is null in CTaskMoveMeleeMovement::AppliedPedMoveMomentumMoveBlendTweak." );
	Assertf( fLateralMoveBlendRatio, "fLateralMoveBlendRatio is null in CTaskMoveMeleeMovement::AppliedPedMoveMomentumMoveBlendTweak." );
	const CBrawlingStyleData* pData = CBrawlingStyleManager::GetBrawlingData( pPed->GetBrawlingStyle().GetHash() );

	// Make sure we need to apply the tweak at all.
	if( m_fPedMoveMomentumTime <= 0.0f )
		return;

	// Determine how much to tweak the move blends.
	float fPedMoveMomentumForward = 0.0f;
	float fPedMoveMomentumLateral = 0.0f;
	Vector3 vDesiredDirection( m_vPedMoveMomentumDir.x, m_vPedMoveMomentumDir.y, 0.0f );
	vDesiredDirection.Normalize();
	if( vDesiredDirection.Mag() > 0.0f )
	{
		Vector3 vPedForward = VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() );
		vPedForward.z = 0.0f;
		fPedMoveMomentumForward = -DotProduct( vDesiredDirection, vPedForward );
		fPedMoveMomentumForward *= ( m_fPedMoveMomentumPortion * ( pData ? pData->GetMeleeMovementMBR() : MOVEBLENDRATIO_RUN ) );

		Vector3 vPedRightward = VEC3V_TO_VECTOR3( pPed->GetTransform().GetA() );
		vPedRightward.z = 0.0f;
		fPedMoveMomentumLateral = DotProduct( vDesiredDirection, vPedRightward );
		fPedMoveMomentumLateral *= ( m_fPedMoveMomentumPortion *  ( pData ? pData->GetMeleeMovementMBR() : MOVEBLENDRATIO_RUN ) );
	}

	// Apply the tweak.
	// When momentum is being applied it fully controls the ped.
	(*fMoveBlendRatio) = fPedMoveMomentumForward;
	(*fLateralMoveBlendRatio) = fPedMoveMomentumLateral;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessStdMove
// PURPOSE :	Process standard (walk about) movement for the ped.
// PARAMETERS :	pPlayerPed - Player ped of interest.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::ProcessStdMove( CPed* pPlayerPed )
{
	pPlayerPed->GetMotionData()->SetIsStrafing( false );

	const CControl *pControl = pPlayerPed->GetControlFromPlayer();
	Vector2 vStickInput( 0.0f, 0.0f );
	if( ShouldAllowMovementInput() )
	{
		vStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
		vStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();
	}

	float fMoveBlendRatio = 0.0f;
	// don't let the player walk when attached
	if( !pPlayerPed->GetIsAttached() )
		fMoveBlendRatio = MIN( vStickInput.Mag(), 1.0f );

	if( fMoveBlendRatio > 0.0f )
	{
		// get the orientation of the FIRST follow ped camera, for player controls
		const float fCamOrient = camInterface::GetPlayerControlCamHeading( *pPlayerPed ); 
		float fStickAngle = rage::Atan2f( -vStickInput.x, vStickInput.y );
		fStickAngle = fStickAngle + fCamOrient;
		fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
		pPlayerPed->SetDesiredHeading( fStickAngle );
	}

	Assert( pPlayerPed->GetPlayerInfo() );
	if(	ShouldAllowMovementInput() && !pPlayerPed->GetPlayerInfo()->GetPlayerDataPlayerSprintDisabled() )
	{
		float fSprintResult = pPlayerPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT);

		if( NetworkInterface::IsGameInProgress() || NetworkInterface::IsInMPTutorial() )
		{
			// For multiplayer games the player's default movespeed is RUN, unless they are in an interior where WALK is used
			const bool bInInterior = CTaskMovePlayer::ms_bDefaultNoSprintingInInteriors && pPlayerPed->GetPortalTracker()->IsInsideInterior() && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting) && !pPlayerPed->GetPortalTracker()->IsAllowedToRunInInterior();
			if( !bInInterior )
				fSprintResult = Max( fSprintResult, 0.5f );
		}

		if( fSprintResult > 1.0f )
		{
			fMoveBlendRatio *= MOVEBLENDRATIO_SPRINT;
			fMoveBlendRatio = MIN( fMoveBlendRatio, MOVEBLENDRATIO_SPRINT );
		}
		else if( fSprintResult > 0.0f )
		{
			fMoveBlendRatio *= MOVEBLENDRATIO_RUN;
			fMoveBlendRatio = MIN( fMoveBlendRatio, MOVEBLENDRATIO_RUN );
		}
		else
		{
			fMoveBlendRatio *= MOVEBLENDRATIO_WALK;
			fMoveBlendRatio = MIN( fMoveBlendRatio, MOVEBLENDRATIO_WALK );
		}
	}
	else
	{
		fMoveBlendRatio *= MOVEBLENDRATIO_RUN;
		fMoveBlendRatio = MIN( fMoveBlendRatio, MOVEBLENDRATIO_RUN );
	}

	fMoveBlendRatio = Clamp( fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT );

	// Mark whether we tried to move of our own accord or not.
	m_bIsDoingSomething = ( fMoveBlendRatio != 0.0f );	

	// Set the desired move blend (which is smoothed internally) and force
	// the momentum (hit) based movement (which won't be smoothed internally).
	pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio( fMoveBlendRatio, 0.0f );
	float fDesiredMoveBlendRatio, fLateralMoveBlendRatio;
	pPlayerPed->GetMotionData()->GetDesiredMoveBlendRatio( fLateralMoveBlendRatio, fDesiredMoveBlendRatio );
	AppliedPedMoveMomentumMoveBlendTweak( pPlayerPed, &fDesiredMoveBlendRatio, &fLateralMoveBlendRatio );
	pPlayerPed->GetMotionData()->SetCurrentMoveBlendRatio( fDesiredMoveBlendRatio, fLateralMoveBlendRatio );
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessStrafeMove
// PURPOSE :	Process strafe (circling) movement for the ped.
// PARAMETERS :	pPlayerPed - Player ped of interest.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::ProcessStrafeMove( CPed* pPlayerPed )
{
	pPlayerPed->GetMotionData()->SetIsStrafing( true );

	Vector2 vStickInput( 0.0f, 0.0f );
	if( ShouldAllowMovementInput() )
	{
		vStickInput = ProcessStrafeInputWithRestrictions( pPlayerPed, GetTargetEntity(), CTaskMelee::sm_fMeleeStrafingMinDistance, m_fStickAngle, m_bStickAngleTweaked );
		
		if(!pPlayerPed->GetMotionData()->GetUsingStealth() && m_bForceRun)
		{
			vStickInput *= MOVEBLENDRATIO_RUN;
		}
	}

	// Mark whether we tried to move of our own accord or not.
	m_bIsDoingSomething = ( vStickInput.Mag2() > 0.0f );

	// Set the desired move blend (which is smoothed internally) and force
	// the momentum (hit) based movement (which won't be smoothed internally).
	pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio( vStickInput.y, vStickInput.x );
	float fMoveBlendRatio;
	float fLateralMoveBlendRatio;
	pPlayerPed->GetMotionData()->GetDesiredMoveBlendRatio( fLateralMoveBlendRatio, fMoveBlendRatio );
	AppliedPedMoveMomentumMoveBlendTweak( pPlayerPed, &fMoveBlendRatio, &fLateralMoveBlendRatio );
	pPlayerPed->GetMotionData()->SetCurrentMoveBlendRatio( fMoveBlendRatio, fLateralMoveBlendRatio );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessStealthMode
// PURPOSE :	Checks whether or not we should be in stealth mode
// PARAMETERS :	pPlayerPed - Player ped of interest.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::ProcessStealthMode( CPed* pPlayerPed )
{
	// We do not want to process stealth mode changes if we are using the RAGE special ability 
	const CPlayerSpecialAbility* pSpecialAbility = pPlayerPed->GetSpecialAbility();
	if( pSpecialAbility && pSpecialAbility->GetType() == SAT_RAGE && pSpecialAbility->IsActive() )
		return;

	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	if( pControl )
	{
		u32 uTimePressed = 0;
		static dev_u32 HISTORY_MS = 300;
		if( pControl->GetPedDuck().HistoryPressed( HISTORY_MS, &uTimePressed ) )
		{
			if( uTimePressed > m_uStealthLastPressed )
			{
				m_uStealthLastPressed = uTimePressed;
				if( CGameConfig::Get().AllowStealthMovement() )
				{
					if( pPlayerPed->GetMotionData()->GetUsingStealth() )
						pPlayerPed->GetMotionData()->SetUsingStealth( false );
					else if( pPlayerPed->CanPedStealth() )
						pPlayerPed->GetMotionData()->SetUsingStealth( true );
				}
				else if( pPlayerPed->GetMotionData()->GetUsingStealth() )
					pPlayerPed->GetMotionData()->SetUsingStealth( false );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPrefdDistanceMoveComponents
// PURPOSE :	TO encapsulate the AI approach and avoid behaviors.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMoveMeleeMovement::GetPrefdDistanceMoveComponents(	float& fForwardTargetBasedMoveComponentOut,
																float& fRightwardTargetBasedMoveComponentOut,
																Vec3V_In vPedPosition,
																Vec3V_In vForwardHeading,
																Vec3V_In vRightwardHeading,
																Vec3V_In vTargetPosition,
																const float fCurrentDesiredRange,
																const bool	bAvoidOnly,
																const float fCurrentMinRangeForImpetus,
																const float fCurrentMaxRangeForImpetus,
																const float fDistancingForwardImpetusPowerFactor,
																const float fDistancingBackwardImpetusPowerFactor,
																const float fDistancingMinTargetApproachDesireSize,
																const CDefensiveArea* pDefensiveArea )
{
	Vec3V vSelfToTarget = vTargetPosition - vPedPosition;
	Vec3V vSelfToTargetDir = Normalize( vSelfToTarget );

	// Determine how strongly we are attracted to the target (based on distance, etc.)
	float fTargetApproachMoveDesireSignedPortion = 0.0f;

	// Determine the unfiltered movement desire.
	const float fSelfToTargetRange = Mag( vSelfToTarget ).Getf();
	const float fDiffToDesiredRange = fCurrentDesiredRange - fSelfToTargetRange;
	const float fClampedAbsDiffToDesiredRange = Clamp( Abs( fDiffToDesiredRange ), 0.0f, fCurrentMaxRangeForImpetus );
	if( fClampedAbsDiffToDesiredRange < fCurrentMinRangeForImpetus )
		fTargetApproachMoveDesireSignedPortion = 0.0f;
	else
	{
		float fDesirePortion = ( fClampedAbsDiffToDesiredRange - fCurrentMinRangeForImpetus ) / ( fCurrentMaxRangeForImpetus - fCurrentMinRangeForImpetus );
		if( fDiffToDesiredRange < 0 )
			fDesirePortion = rage::Powf( fDesirePortion, fDistancingForwardImpetusPowerFactor );
		else
			fDesirePortion = rage::Powf( fDesirePortion, fDistancingBackwardImpetusPowerFactor );

		fTargetApproachMoveDesireSignedPortion = -( Sign( fDiffToDesiredRange ) * fDesirePortion );

		// Make sure the desire is not fleetingly small.
		if( Abs( fTargetApproachMoveDesireSignedPortion ) < fDistancingMinTargetApproachDesireSize )
			fTargetApproachMoveDesireSignedPortion = Sign( fTargetApproachMoveDesireSignedPortion ) * fDistancingMinTargetApproachDesireSize;
	}

	// Make sure to only move away if that's all we can do.
	if( bAvoidOnly && fTargetApproachMoveDesireSignedPortion > 0.0f )
		fTargetApproachMoveDesireSignedPortion = 0.0f;

	bool bCanSeek = true;
	bool bCanFlee = true;

	// Don't allow any seek/flee when ped is out side their defensive area
	if( pDefensiveArea && pDefensiveArea->IsActive() && !pDefensiveArea->IsPointInDefensiveArea( VEC3V_TO_VECTOR3( vTargetPosition ) ) )
		bCanSeek = bCanFlee = false;

	// Modify the target approach move desire according to what we are allowed to do.
	if( !bCanSeek && ( fTargetApproachMoveDesireSignedPortion > 0.0f ) )
		fTargetApproachMoveDesireSignedPortion = 0.0f;

	if( !bCanFlee && (fTargetApproachMoveDesireSignedPortion < 0.0f ) )
		fTargetApproachMoveDesireSignedPortion = 0.0f;

	// Apply the desired strength to the forward and rightward movement directions.
	fForwardTargetBasedMoveComponentOut = Dot( vSelfToTargetDir, vForwardHeading ).Getf() * fTargetApproachMoveDesireSignedPortion;
	fRightwardTargetBasedMoveComponentOut = Dot( vSelfToTargetDir, vRightwardHeading ).Getf() * fTargetApproachMoveDesireSignedPortion;
}

/////////////////////////////////////////////////////////////////////////////////
// CTaskMeleeActionResult
//
// Defines a melee action that can be anything from a strike to a taunt
/////////////////////////////////////////////////////////////////////////////////
dev_float RAGDOLL_APPLY_MELEE_FORCE_MULT = 75.0f;

const u32 SPECIAL_RESULT_SET_STOP_MOVES_HOMING_ID				= ATSTRINGHASH("SetStopMovesHoming", 0x099225e96);
const u32 SPECIAL_RESULT_SET_OPPONENT_STOP_MOVES_HOMING_ID		= ATSTRINGHASH("SetOpponentStopMovesHoming", 0x0e943d9af);
const u32 SPECIAL_RESULT_ADD_SMALL_SPECIAL_ABILITY_CHARGE_ID	= ATSTRINGHASH("AddSmallSpecialAbilityCharge", 0x0ef4d1ced);
const u32 SPECIAL_RESULT_DISABLE_INJURED_GROUND_BEHAVIOR_ID		= ATSTRINGHASH("DisableInjuredGroundBehavior", 0x01e73fec1);

static const u32	BOUND_MAX_STRIKE_CAPSULES				= 16;
static const float	BOUND_HAND_LENGTH_REDUCTION_AMOUNT		= 0.06f;
static const float	BOUND_ACCEPTABLE_DEPTH_ERROR_PORTION	= 0.3f;
static const u32	BOUND_MAX_NUM_SPHERES_ALONG_LENGTH		= 16;

const fwMvClipId CTaskMeleeActionResult::ms_ClipId("ClipId",0x4ED90D92);
const fwMvClipId CTaskMeleeActionResult::ms_AdditiveClipId("AdditiveClipId",0xDDBFA9F6);
const fwMvFilterId CTaskMeleeActionResult::ms_ClipFilterId("ClipFilterId",0x458A342);
const fwMvRequestId CTaskMeleeActionResult::ms_PlayClipRequestId("PlayClip",0x2E8FF75F);
const fwMvRequestId CTaskMeleeActionResult::ms_RestartPlayClipRequestId("RestartPlayClip",0x67fcc4c3);
const fwMvFloatId CTaskMeleeActionResult::ms_ClipPhaseId("PhaseId",0x8C064EBA);
const fwMvFloatId CTaskMeleeActionResult::ms_ClipRateId("RateId",0xF11C4C9B);
const fwMvFloatId CTaskMeleeActionResult::ms_IkShadowWeightId("IkShadowWeightId",0x1DD0C33F);
const fwMvBooleanId CTaskMeleeActionResult::ms_AnimClipFinishedId("OnClipFinished",0x88B0DC15);
const fwMvFlagId CTaskMeleeActionResult::ms_UseAdditiveClipId("UseAdditiveClip",0x24C4F88F);
phBoundComposite* CTaskMeleeActionResult::sm_pTestBound = NULL;
const fwMvBooleanId	CTaskMeleeActionResult::ms_DropWeaponOnDeath("DropWeapon",0x9312D206);
const fwMvFlagId CTaskMeleeActionResult::ms_HasGrip("HasGrip",0x1bcd14c6);
const fwMvClipSetVarId CTaskMeleeActionResult::ms_GripClipSetId("GripClipSet", 0xcbd9a0f0);

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskMeleeActionResult, 0x2a2941a3)

CTaskMeleeActionResult::Tunables CTaskMeleeActionResult::sm_Tunables;

CTaskMeleeActionResult::MeleeNetworkDamage CTaskMeleeActionResult::sm_CachedMeleeDamage[CTaskMeleeActionResult::s_MaxCachedMeleeDamagePackets];
u8 CTaskMeleeActionResult::sm_uMeleeDamageCounter = 0;

void CTaskMeleeActionResult::CacheMeleeNetworkDamage(CPed *pHitPed,
									CPed *pFiringEntity,
									const Vector3 &vWorldHitPos,
									s32 iComponent,
									float fDamage, 
									u32 flags, 
									u32 uParentMeleeActionResultID,
									u32 uForcedReactionDefinitionID,
									u16 uNetworkActionID)
{
	taskAssert(sm_uMeleeDamageCounter < s_MaxCachedMeleeDamagePackets);

	//! Remove any existing melee reacts with this ID.
	for(int i = 0; i < s_MaxCachedMeleeDamagePackets; ++i)
	{
		CTaskMeleeActionResult::MeleeNetworkDamage &CachedDamage = sm_CachedMeleeDamage[i];
		if(CachedDamage.m_uNetworkActionID==uNetworkActionID)
		{
			CachedDamage.Reset();
		}
	}

	CTaskMeleeActionResult::MeleeNetworkDamage NewCachedDamage;

	NewCachedDamage.m_pHitPed = pHitPed;
	NewCachedDamage.m_pInflictorPed = pFiringEntity;
	NewCachedDamage.m_vWorldHitPos = vWorldHitPos;
	NewCachedDamage.m_iComponent = iComponent;
	NewCachedDamage.m_fDamage = fDamage;
	NewCachedDamage.m_uFlags = flags;
	NewCachedDamage.m_uParentMeleeActionResultID = uParentMeleeActionResultID;
	NewCachedDamage.m_uForcedReactionDefinitionID = uForcedReactionDefinitionID;
	NewCachedDamage.m_uNetworkActionID = uNetworkActionID;
	NewCachedDamage.m_uNetworkTime = NetworkInterface::GetNetworkTime();

	sm_CachedMeleeDamage[sm_uMeleeDamageCounter] = NewCachedDamage;

	++sm_uMeleeDamageCounter;
	if(sm_uMeleeDamageCounter >= s_MaxCachedMeleeDamagePackets)
		sm_uMeleeDamageCounter = 0;
}

CTaskMeleeActionResult::MeleeNetworkDamage *CTaskMeleeActionResult::FindMeleeNetworkDamageForHitPed(CPed *pHitPed)
{
	for(int i = 0; i < s_MaxCachedMeleeDamagePackets; ++i)
	{
		CTaskMeleeActionResult::MeleeNetworkDamage &CachedDamage = sm_CachedMeleeDamage[i];
		if(CachedDamage.m_pHitPed == pHitPed && 
			CachedDamage.m_pInflictorPed == GetPed() && 
			CachedDamage.m_uNetworkActionID == m_nUniqueNetworkActionID )
		{
			return &sm_CachedMeleeDamage[i];
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CTaskMeleeActionResult
// PURPOSE :	The default constructor.
// PARAMETERS :	pActionResult - The action to perform (may be null).
//				pParentPed - The ped of interest.
//				pTargetEntity - The melee target.
//				bHasLockOnTargetEntity - Is a lock on the target present.
//				isDoingReaction - Is the action to perform a reaction.
/////////////////////////////////////////////////////////////////////////////////
CTaskMeleeActionResult::CTaskMeleeActionResult( const CActionResult* pActionResult, CEntity* pTargetEntity, bool bHasLockOnTargetEntity, bool bIsDoingReaction, bool bShouldSuppressHoming, u32 nSelfDamageWeaponHash, eAnimBoneTag eSelfDamageAnimBoneTag, float fMoveBlendRatioSq )
: // Initializer list.
m_pActionResult						(pActionResult),
m_pForcedReactionActionDefinition	(NULL),
m_pClipFilter						(NULL),
m_pTargetEntity						(NULL),		// Set in function body, below.
m_bHasLockOnTargetEntity			(false),	// Set in function body, below.
m_bPlaysClip						(true),
m_vCachedHomingPosition				(V_ZERO),
m_vCachedHomingDirection			(V_ZERO),
m_vCachedHeadIkOffset				(V_ZERO),
m_vMoverDisplacement				(V_ZERO),
m_vCachedHomingPedPosition			(V_ZERO),
m_vCloneCachedHomingPosition		(V_ZERO),
m_nResultId							(0),
m_nResultPriority					(0),
m_clipSetId							(CLIP_SET_ID_INVALID),
m_clipId							(CLIP_ID_INVALID),
m_bForceImmediateReaction			(false),
m_fSelfDamageAmount					(0.0f),
m_fInitialPhase						(0.0f),
m_bIsDoingIntro						(false),
m_bIsDoingTaunt						(false),
m_bIsDazed							(false),
m_bIsUpperbodyOnly					(false),
m_bIsDoingBlock						(false),
m_bDropWeapon						(false),
m_bDisarmWeapon						(false),
m_bIsDoingReaction					(bIsDoingReaction),
m_bIsNonMeleeHitReaction			(false),
m_bSelfDamageApplied				(false),
m_bPreHitResultsApplied				(false),
m_bPostHitResultsApplied			(false),
m_bAllowHoming						(true),
m_bAllowDistanceHoming				(!bShouldSuppressHoming),
m_bDidAllowDistanceHoming			(false),
m_bOwnerStoppedDistanceHoming		(false),
m_bAllowDodge						(false),
m_bHasJustStruckSomething			(false),
m_bHasTriggerMeleeSwipeAudio		(false),
m_nHitEntityComponentsCount			(0),
m_nBoneIdxCache						(-1),
m_fCriticalFrame					(0.0f),
m_bPastCriticalFrame				(false),
m_bAllowInterrupt					(false),
m_bProcessCollisions				(false),
m_bHasStartedProcessingCollisions	(false),
m_bMeleeInvulnerability				(false),
m_bFireWeapon						(false),
m_bSwitchToRagdoll					(false),
m_nActionResultNetworkID			(INVALID_ACTIONRESULT_ID),
m_bFixupActionResultFromNetwork		(false),
m_nSelfDamageWeaponHash				(nSelfDamageWeaponHash),
m_eSelfDamageAnimBoneTag			(eSelfDamageAnimBoneTag),
m_bIsStandardAttack					(false),
m_bForceDamage						(false),
m_bIgnoreArmor						(false),
m_bIgnoreStatModifiers				(false),
m_bShouldDamagePeds					(false),
m_bForceHomingArrival				(false),
m_bCloneKillAnimFinished			(false),
m_bHeadingInitialized				(false),
m_bLastKnowWeaponMatCached			(false),
m_matLastKnownWeapon				(Matrix34::IdentityType),
m_bPickupDropped					(false),
m_fCachedMoveBlendRatioSq			(fMoveBlendRatioSq),
m_bOffensiveMove					(false),
m_bInterruptWhenOutOfWater			(false),
m_bAllowScriptTaskAbort				(false),
m_bHandledLocalToRemoteSwitch		(false),
m_bInNetworkBlenderTolerance		(false),
m_nUniqueNetworkActionID			(0),
m_nCloneKillAnimFinishedTime		(0)
{
	// Update the old target and the new target.
	SetTargetEntity( pTargetEntity, bHasLockOnTargetEntity );
	SetInternalTaskType(CTaskTypes::TASK_MELEE_ACTION_RESULT);

	if(m_pActionResult)
		m_nActionResultNetworkID = m_pActionResult->GetID();

	m_nNetworkTimeTriggered = NetworkInterface::GetNetworkTime();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CacheActionResult
// PURPOSE :	Cache off the commonly used variables 
//				(might be able to optimize this out)
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::CacheActionResult( CPed* pPed )
{
	// Make sure there is an actionResult, if not try to operate in a "do nothing"
	// mode.
	if( m_pActionResult )
	{
		bool bFirstPerson = false;
#if FPS_MODE_SUPPORTED
		bFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_pActionResult->GetFirstPersonClipSetID() != CLIP_SET_ID_INVALID;
#endif

		// Set local copies of the action result data.
		// This lets us edit the action table mid-move without bad

		// things happening.
		m_nResultId						= m_pActionResult->GetID();
		m_nResultPriority				= m_pActionResult->GetPriority();
		m_clipSetId						= bFirstPerson ? (fwMvClipSetId)m_pActionResult->GetFirstPersonClipSetID() : (fwMvClipSetId)m_pActionResult->GetClipSetID();
		m_clipId						= (fwMvClipId)m_pActionResult->GetAnimID();
		m_bIsDoingIntro					= m_pActionResult->GetIsAnIntro();
		m_bIsDoingTaunt					= m_pActionResult->GetIsATaunt();
		m_bIsDazed						= m_pActionResult->GetIsADazedHitReaction();
		m_bIsUpperbodyOnly				= pPed->IsPlayer() ? m_pActionResult->GetPlayerIsUpperbodyOnly() : m_pActionResult->GetAIIsUpperbodyOnly();
		m_bIsDoingBlock					= m_pActionResult->GetIsABlock();
		m_bIsStandardAttack				= m_pActionResult->GetIsAStandardAttack();
		m_bIsNonMeleeHitReaction		= m_pActionResult->GetIsNonMeleeHitReaction();
		m_bForceDamage					= m_pActionResult->GetShouldForceDamage();
		m_bAllowAimInterrupt			= m_pActionResult->AllowAimInterrupt();
		m_bInterruptWhenOutOfWater		= m_pActionResult->ShouldInterruptWhenOutOfWater();
		m_bAllowScriptTaskAbort			= m_pActionResult->ShouldAllowScriptTaskAbort();
		m_bOffensiveMove				= m_pActionResult->IsOffensiveMove();

		const CDamageAndReaction* pDamageAndReaction = m_pActionResult->GetDamageAndReaction();
		if( pDamageAndReaction )
		{
			m_bForceImmediateReaction		= pDamageAndReaction->ShouldForceImmediateReaction();
			m_fSelfDamageAmount				= pDamageAndReaction->GetSelfDamageAmount();
			m_bDropWeapon					= pDamageAndReaction->ShouldDropWeapon();
			m_bDisarmWeapon					= pDamageAndReaction->ShouldDisarmOpponentWeapon();
			m_bIgnoreArmor					= pDamageAndReaction->ShouldIgnoreArmor();
			m_bIgnoreStatModifiers			= pDamageAndReaction->ShouldIgnoreStatModifiers();
			m_bShouldDamagePeds				= pDamageAndReaction->ShouldDamagePeds();
		}
		
		m_nSoundNameHash				= m_pActionResult->GetSoundID();
BANK_ONLY(m_pszSoundName				= m_pActionResult->GetSoundName() );
	
		// Check if this result should try to play an clip.
		m_bPlaysClip = false;
		if( m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID )
			m_bPlaysClip = true;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PrepareMoveNetwork
// PURPOSE :	Initialize the move network associated with this melee action
// PARAMETERS :	pPed - the ped of interest.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::PrepareMoveNetwork( CPed* pPed )
{
	if( !m_MoveNetworkHelper.IsNetworkActive() )
	{
		m_MoveNetworkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkTaskMeleeActionResult );
		if( m_MoveNetworkHelper.IsNetworkDefStreamedIn( CClipNetworkMoveInfo::ms_NetworkTaskMeleeActionResult ) )
			m_MoveNetworkHelper.CreateNetworkPlayer( pPed, CClipNetworkMoveInfo::ms_NetworkTaskMeleeActionResult );
	}

	if( !m_MoveNetworkHelper.IsNetworkAttached() )
	{
		pPed->GetMovePed().SetTaskNetwork( m_MoveNetworkHelper.GetNetworkPlayer(), m_pActionResult ? m_pActionResult->GetAnimBlendInRate() : NORMAL_BLEND_DURATION, CMovePed::Task_IgnoreMoverBlend );
		return true;
	}
	
	// Make sure to cover cases where it is active and attached on first pass
	return m_MoveNetworkHelper.IsNetworkActive() && m_MoveNetworkHelper.IsNetworkAttached();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CleanUp
// PURPOSE :	Main CTask cleanup function that is run once the current task 
//				has been marked to abort
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::CleanUp( void )
{
	CPed *pPed = GetPed();
	Assert( pPed );

	if(pPed->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::CleanUp [%p] sequence ID = %d m_nUniqueNetworkActionID = %d", fwTimer::GetFrameCount(), this, GetNetSequenceID(), m_nUniqueNetworkActionID );
	}

	if( m_bHasTriggerMeleeSwipeAudio )
	{
		m_bHasTriggerMeleeSwipeAudio = false;
	}

	if( m_MoveNetworkHelper.GetNetworkPlayer() )
	{
		float fBlendDuration = m_pActionResult ? m_pActionResult->GetAnimBlendOutRate() : SLOW_BLEND_DURATION;
		pPed->GetMovePed().ClearTaskNetwork( m_MoveNetworkHelper, fBlendDuration, ( m_pActionResult && m_pActionResult->TagSyncBlendOut() && CTaskMelee::VelocityHighEnoughForTagSyncBlendOut( pPed ) ) ? CMovePed::Task_TagSyncTransition : 0 );
		m_MoveNetworkHelper.ReleaseNetworkPlayer();

#if	FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSMeleeActionResult, true);
		}
#endif

		CTaskMotionBase* pPrimaryMotionTask = pPed->GetPrimaryMotionTask();
		if(pPrimaryMotionTask && pPrimaryMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
		{
			CTaskMotionPed* pPedMotionTask = static_cast<CTaskMotionPed*>(pPrimaryMotionTask);
			pPedMotionTask->SetBlockOnFootTransitionTime(fBlendDuration);
			
			// Block aiming for AI peds to avoid glitch
			if(!pPed->IsPlayer())
			{
				pPedMotionTask->SetBlockAimingTransitionIndependentMover(fBlendDuration);
			}
		}
	}

	// Clean up leg ik
	if( !pPed->GetIKSettings().IsIKFlagDisabled( IKManagerSolverTypes::ikSolverTypeLeg ) )
		pPed->m_PedConfigFlags.SetPedLegIkMode( CIkManager::GetDefaultLegIkMode(pPed) );

	if( m_pClipFilter )
	{
		m_pClipFilter->Release();
		m_pClipFilter = NULL;
	}

	// Ensure we apply network damage.
	ProcessCachedNetworkDamage(pPed);

	// In the case we exited via ragdoll collision we need to make sure we have applied self damage
	ProcessSelfDamage( pPed );	

	//! DMKH. If we tried to kill local ped (and flushed their tasks), check if they are dead. If not, give default task, otherwise they will be
	//! stuck forever.
	if(!pPed->ShouldBeDead())
	{
		if(!pPed->IsNetworkClone() && GetParent() && static_cast<CTaskMelee*>( GetParent() )->GetTaskFlags().IsFlagSet( CTaskMelee::MF_ForcedReaction ))
		{
			if(pPed->IsDeadByMelee())
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth, false);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown, false);
			}

			static_cast<CTaskMelee*>( GetParent() )->GetTaskFlags().ClearFlag( CTaskMelee::MF_ForcedReaction );
		}
	}

	pPed->SetRagdollOnCollisionIgnorePhysical( NULL );

	pPed->ClearBound();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessPreFSM
// PURPOSE	:	Runs before the main CTask update loop (UpdateFSM)
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeActionResult::ProcessPreFSM( void )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	DEV_BREAK_IF_FOCUS( CCombatMeleeDebug::sm_bFocusPedBreakOnTaskMeleeActionProcessPreFSM, pPed );

	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );
	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate( true );

	//! Clone peds may not run hand in hand with parent task, so ensure this is set here too.
	pPed->SetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bMeleeTaskRunning, true );

	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true );

#if FPS_MODE_SUPPORTED
	if(pPed->IsLocalPlayer() && pPed->GetPlayerInfo())
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}
#endif

	if( IsDoingSomething() )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);	
		pPed->SetPedResetFlag( CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, AllowHeadingUpdate() );
	}

	if( m_pActionResult && m_pActionResult->ShouldDisablePedToPedRagdoll() )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true );
	}

	if( m_pActionResult && m_pActionResult->ShouldUseKinematicPhysics() )
	{		
		pPed->SetPedResetFlag(CPED_RESET_FLAG_TaskUseKinematicPhysics, true);
	}

	// Make sure to disallow switching of weapons (otherwise all sorts of
	// weird issues come up).
	if( !pPed->IsNetworkClone() && !ShouldAllowInterrupt() ) 
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );

	//! If this is an offensive attack, and we have a target, set ped to be in action mode. 
	//! Note: We also force a delay to our auto run here.
	if(m_bOffensiveMove) 
	{
		pPed->SetUsingActionMode(true, CPed::AME_Melee, sm_Tunables.m_ActionModeTime, false);
		pPed->SetForcedRunDelay((u32)(sm_Tunables.m_ActionModeTime * 1000.0f));
	}

	//! If a clone ped is ragdolling, take ownership of it.
	if(HandlesRagdoll(pPed) && GetPed()->GetRagdollState() == RAGDOLL_STATE_PHYS)
	{
		//Notify the ped that this task is in control of the ragdoll.
		GetPed()->TickRagdollStateFromTask(*this);
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateFSM
// PURPOSE :	Main heartbeat function for CTask
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeActionResult::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Starting state
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter( pPed );
			FSM_OnUpdate
				return Start_OnUpdate( pPed );

		// Play the action result clip
		FSM_State(State_PlayClip)
			FSM_OnEnter
				PlayClip_OnEnter( pPed );
			FSM_OnUpdate
				return PlayClip_OnUpdate( pPed );
			FSM_OnExit
				PlayClip_OnExit( pPed );

		// Quit
		FSM_State(State_Finish)
			FSM_OnEnter
				Finish_OnEnter( pPed );
			FSM_OnUpdate
				taskDebugf1("Quit: CTaskMeleeActionResult::UpdateFSM: State_Finish");
				return FSM_Quit;

	FSM_End
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Copy
// PURPOSE :	The cloning operation- used by the task management system.
// RETURNS :	A perfect copy of the task.
/////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskMeleeActionResult::Copy( void ) const 
{
	return rage_new CTaskMeleeActionResult( m_pActionResult, 
		m_pTargetEntity, 
		m_bHasLockOnTargetEntity,
		m_bIsDoingReaction, 
		m_bAllowDistanceHoming, 
		m_nSelfDamageWeaponHash, 
		m_eSelfDamageAnimBoneTag, 
		m_fCachedMoveBlendRatioSq );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessPostPreRender
// PURPOSE :	Update what the ped is currently doing, but after other the
//				skeletons have been updated since the last frame, so that
//				collision detection is more reliable.
// RETURNS :	If everything went fine.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::ProcessPostPreRender( void )
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessPostPreRender." );

	if( fwTimer::IsGamePaused() )
		return true;

	// Check and handle any collisions.
	ProcessMeleeCollisions( pPed );

	// Check and handle any network damage events if we didn't process them locally.
	if(HasStartedProcessingCollisions() && (IsPastCriticalFrame() || !ShouldProcessMeleeCollisions()))
	{
		ProcessCachedNetworkDamage(pPed);
	}

	const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (pSolver != NULL && pSolver->GetBlendRate() >= 0.0f)
	{
		//! For clones, need to lift bound if we haven't received response from the owner ped as we'll slide along the floor.
		//! For url:bugstar:1596423.
		CTaskMelee* pParentMeleeTask = GetParent() ? static_cast<CTaskMelee*>( GetParent() ) : NULL;
		bool bForcedReaction = pPed->IsNetworkClone() && pParentMeleeTask && pParentMeleeTask->GetIsTaskFlagSet( CTaskMelee::MF_ForcedReaction );

		// Hack to support horizontal peds that are still playing animations
		// This allows us to avoid collisions with the ground that make the horizontal ped inch away from the attacker (dog)
		static dev_float sfAdditionalZOffset = 0.25f;
		float fOffset; 
		if(m_pActionResult && (bForcedReaction || m_pActionResult->ShouldAddOrientToSpineOffset()))
		{
			fOffset = sfAdditionalZOffset;
		}
		else
		{
			fOffset = 0.0f;
		}

		pPed->OrientBoundToSpine( NORMAL_BLEND_IN_DELTA, fOffset );
	}
	else
	{
		pPed->ClearBound();
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldAbort
// PURPOSE :	Query to determine if another task has a higher priority than this one
// PARAMETERS :	AbortPriority - Priority of abort event
//				aiEvent - Event thrown that will be tested against 
// RETURNS :	whether or not we should abort this task
// NOTES :		Might want to refer to the CTaskVault::ShouldAbort for additional abort
//				conditions.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent ) 
{
	// Cache the ped
	CPed* pPed = GetPed();
	if( iPriority == ABORT_PRIORITY_IMMEDIATE || pPed->IsFatallyInjured() )
	{
		return CTask::ShouldAbort( iPriority, pEvent );
	}
	else if( pEvent ) 
	{
		const CEvent* pGameEvent = static_cast<const CEvent*>(pEvent);
		
		//! Bail out for shark attacks.
		if(pEvent->GetEventType()==EVENT_GIVE_PED_TASK)
		{
			const CEventGivePedTask *pGivePedTask = static_cast<const CEventGivePedTask *>(pEvent);

			if(pGivePedTask->GetTask() && pGivePedTask->GetTask()->GetTaskType()==CTaskTypes::TASK_SYNCHRONIZED_SCENE)
			{
				//! Hack - compare to shark dict.
				const CTaskSynchronizedScene *pSyncScene = static_cast<const CTaskSynchronizedScene *>(pGivePedTask->GetTask());
				if(pSyncScene->GetDictionary() == CTaskSharkAttack::ms_SharkBiteDict)
				{
					return CTask::ShouldAbort( iPriority, pEvent );
				}
			}
		}

		if( pGameEvent->RequiresAbortForMelee( pPed ) ||
			pGameEvent->GetEventPriority() >= E_PRIORITY_SCRIPT_COMMAND_SP ||
			pGameEvent->GetEventType() == EVENT_ON_FIRE ||
			pGameEvent->GetEventType() == EVENT_DAMAGE ||
			pGameEvent->GetEventType() == EVENT_INCAPACITATED ||
			pGameEvent->GetEventType() == EVENT_SWITCH_2_NM_TASK ||
			( pGameEvent->GetEventType() == EVENT_SCRIPT_COMMAND && ShouldAllowScriptTaskAbort() ) )
		{
			return CTask::ShouldAbort( iPriority, pEvent );
		}
	}
	
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetCurrentClipTime
// PURPOSE :	Get the amount of time into the current clip that is being
//				played.
// RETURNS :	The clip time.
/////////////////////////////////////////////////////////////////////////////////
float CTaskMeleeActionResult::GetCurrentClipTime( void ) const
{
	float fClipTime = 0.0f;
	if( m_MoveNetworkHelper.IsNetworkActive() )
	{
		float fCurrentPhase = m_MoveNetworkHelper.GetFloat( ms_ClipPhaseId );
		const crClip* pClip = m_MoveNetworkHelper.GetClip( ms_ClipId );
		if( pClip )
			fClipTime = pClip->ConvertPhaseToTime( fCurrentPhase );
	}

	return fClipTime;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AllowMovement
// PURPOSE :	Check if the clip that is being played allows movement
//				impulses through or if it entirely controls the peds movement.
// RETURNS :	Whether or not to allow external movement impulses.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::AllowMovement( void ) const
{
	if( !m_MoveNetworkHelper.IsNetworkActive() || !m_MoveNetworkHelper.GetClip( ms_ClipId ) )
		return true;

	if( IsUpperbodyOnly() )
		return true;
	
	if( m_pActionResult && m_pActionResult->UpdateMoveBlendRatio() )
		return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AllowStrafing
// PURPOSE :	Check if the clip that is being played allows strafing
// RETURNS :	Whether or not to allow strafing movement impulses.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::AllowStrafing( void ) const
{
	if( !m_MoveNetworkHelper.IsNetworkActive() || !m_MoveNetworkHelper.GetClip( ms_ClipId ) )
		return true;

	if( IsUpperbodyOnly() )
		return true;

	if( m_pActionResult )
		return m_pActionResult->GetShouldAllowStrafing();
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AllowHeadingUpdate
// PURPOSE :	Check if the clip that is being played allows strafing
// RETURNS :	Whether or not to allow heading updates
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::AllowHeadingUpdate( void ) const
{
	if( m_pActionResult && m_pActionResult->GetIsABlock() )
		return true;

	// Allow heading update if we are not processing melee collisions
	bool bAllowDistanceHoming = ShouldAllowDistanceHoming();
	if( !bAllowDistanceHoming && ShouldProcessMeleeCollisions() )
		return false;

	if( m_pActionResult && m_pActionResult->GetIsAStandardAttack() && !ShouldProcessMeleeCollisions() )
		return true;
	
	return bAllowDistanceHoming;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AllowStealthMode
// PURPOSE :	Check if the clip that is being played allows strafing
// RETURNS :	Whether or not to allow heading updates
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::AllowStealthMode( void ) const
{
	if( m_pActionResult && m_pActionResult->GetIsAStealthKill() )
		return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetTargetEntity
// PURPOSE :	Set what the target entity currently is and whether or not we
//				have a lock on it.
//				The lock lets us know if the target is an ambient target or an
//				intended target.
// PARAMETERS :	pTargetEntity - the target.
//				bHasLockOnTargetEntity - whether or not it is a lock on target.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::SetTargetEntity( CEntity* pTargetEntity, bool bHasLockOnTargetEntity )
{
	// Just set the has lock on flag to its current state (allowing us to
	// acquire and break locks on a given ped even when they are acquired as
	// an ambient target).
	m_pTargetEntity = pTargetEntity;
	m_bHasLockOnTargetEntity = bHasLockOnTargetEntity;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldBranchOnCurrentFrame
// PURPOSE :	Determine if a new simple action should be invoked and this one
//				should be stopped.
//				This is used for beginning and continuing sequences of moves.
// PARAMETERS :	pPed - The ped of interest.
//				pImpulseCombo - History of the peds current input impulses
//				pActionIdOut - id of the action to perform
//				bImpulseInitiatedMovesOnly - Should these only be initial actions?
//				typeToLookFor - Bitfield of the types to look for
//				nForcingResultId - Id of action that we should force
// RETURNS :	Whether or not a branch should be taken.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::ShouldBranchOnCurrentFrame( const CPed* pPed, const CImpulseCombo* pImpulseCombo, u32* pActionIdOut, bool bImpulseInitiatedMovesOnly, const CActionFlags::ActionTypeBitSetData& typeToLookFor, u32 nForcingResultId )
{
#if __DEV
	if( !pPed->IsPlayer() && !CCombatMeleeDebug::sm_bAllowNpcInitiatedMoves )
		return false;
#endif // __DEV

	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ShouldBranchOnCurrentFrame." );
	Assertf( pActionIdOut, "pActionId is null in CTaskMeleeActionResult::ShouldBranchOnCurrentFrame." );

	const CActionDefinition* pActionToDo = NULL;

	CEntity *pTargetEntity = GetTargetEntity();

	// Check if we are reacting to something, currently doing nothing, or doing an idle move
	// from which any entry move could be done.
	if( !m_pActionResult || ( ShouldAllowInterrupt() && m_pActionResult->AllowInitialActionCombo() ) )
	{
		// We don't care about closest melee targets when attempting to block
		if( pPed->IsPlayer() && ( !m_pActionResult || m_pActionResult->AllowMeleeTargetEvaluation() ) )
		{
			CPed* pTargetPed = ( pTargetEntity && pTargetEntity->GetIsTypePed() ) ? static_cast<CPed*>(pTargetEntity) : NULL;
			
			CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();

			bool bTargetDeadPeds = ((pTargetEntity == NULL) || (pTargetPed && pTargetPed->IsDead())) && (rTargeting.GetLockOnTarget() == NULL);

			// First and foremost lets try and select a new target via FindMeleeTarget
			CTaskMelee::ResetLastFoundActionDefinition();
			const CWeaponInfo *pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
			CEntity* pNewBestTarget = rTargeting.FindMeleeTarget( pPed, pWeaponInfo, true, bTargetDeadPeds, false, false, pTargetEntity );

			pActionToDo = CTaskMelee::GetLastFoundActionDefinition();
			if( pNewBestTarget == pTargetEntity)
			{
				//! Assume we didn't even search for new target to keep combo moves going.
			}
			else if( pNewBestTarget )
			{
				if(pActionToDo)
				{
					// Set the parent task to use the new best target entity
					CTask* pParentTask = GetParent();
					if( pParentTask ) 
						static_cast<CTaskMelee*>( pParentTask )->SetTargetEntity( pNewBestTarget, false, true );

					// Let the caller know what branch to go to.
					*pActionIdOut = pActionToDo->GetID();
					return true;
				}

				pTargetEntity = pNewBestTarget;
			}
		}

		bool bShouldDebugPed = CActionManager::ShouldDebugPed( pPed );

		// Do the search.
		pActionToDo = ACTIONMGR.SelectSuitableAction( typeToLookFor,
													  pImpulseCombo,
													  true,							// Test whether or not it is an entry action.
													  true,							// Only find entry actions.
													  pPed,
													  nForcingResultId,				// What result was the hitter doing?
													  -1,							// Priority
													  pTargetEntity,
													  HasLockOnTargetEntity(),
													  bImpulseInitiatedMovesOnly,
													  pPed->IsLocalPlayer(),		// If this is a player then test the controls.
													  bShouldDebugPed );

		// If we couldn't find an action to perform and had a valid target let us try to find a move that doesn't require a target entity
		if( pTargetEntity && !pActionToDo )
		{
			pActionToDo = ACTIONMGR.SelectSuitableAction( typeToLookFor,			// Only look for moves that are appropriate from on foot
														  pImpulseCombo,			// No impulse combo
														  true,						// Test whether or not it is an entry action.
														  true,						// Only find entry actions.
														  pPed,						// The acting ped.
														  0,						// result Id forcing us.
														  -1,						// Priority
														  NULL,						// The entity being attacked.
														  false,					// Do we have a lock on the target entity.
														  false,					// Should we only allow impulse initiated moves.
														  pPed->IsLocalPlayer(),	// If this is a player then test the controls.
														  bShouldDebugPed );

			if( pActionToDo )
			{
				pTargetEntity = NULL;
			}
		}

		// Check if we found an action to take.
		if( pActionToDo )
		{
			// Let the caller know what branch to go to.
			*pActionIdOut = pActionToDo->GetID();

			CTask* pParentTask = GetParent();
			if( pParentTask ) 
				static_cast<CTaskMelee*>( pParentTask )->SetTargetEntity( pTargetEntity, false, true );

			return true;
		}
	}
	// Check for branches off our current move to do.
	else if( ShouldAllowInterrupt() )
	{
		// Make sure there is an clip playing.
		if ( !m_MoveNetworkHelper.IsNetworkActive() || !m_MoveNetworkHelper.GetClip( ms_ClipId ) )
			return false;

		// As a slight optimization, do not check branches unless we need to
		const bool bCheckBranches = !pPed->IsPlayer() || 
									( typeToLookFor.IsSet( CActionFlags::AT_BLOCK ) || typeToLookFor.IsSet( CActionFlags::AT_DAZED_HIT_REACTION ) ) || 
									( pImpulseCombo && pImpulseCombo->HasPendingImpulse() );
		if( bCheckBranches )
		{
			bool bAllowNoTargetFallback = false;

			// We don't care about closest melee targets when attempting to block
			if( pPed->IsPlayer() && m_pActionResult->AllowMeleeTargetEvaluation() )
			{
				bool bFindTargetMustUseDesiredDirection = false;
				
				//! Currently targeting something that isn't an entity. If that is the case, require our target test uses stick direction (so that we can't just target
				//! something that runs past).
				if(!pTargetEntity && 
					GetHoming(pPed) && 
					GetHoming(pPed)->IsSurfaceProbe())
				{
					bFindTargetMustUseDesiredDirection = true;
				}

				CPed* pTargetPed = ( pTargetEntity && pTargetEntity->GetIsTypePed() ) ? static_cast<CPed*>(pTargetEntity) : NULL;
				CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();

				bool bTargetDeadPeds = ((pTargetEntity == NULL) || (pTargetPed && pTargetPed->IsDead())) && (rTargeting.GetLockOnTarget() == NULL);

				// First and foremost lets try and select a new target via FindMeleeTarget
				CTaskMelee::ResetLastFoundActionDefinition();
				const CWeaponInfo *pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
				CEntity* pNewBestTarget = rTargeting.FindMeleeTarget( pPed, pWeaponInfo, true, bTargetDeadPeds, false, bFindTargetMustUseDesiredDirection, pTargetEntity );

				pActionToDo = CTaskMelee::GetLastFoundActionDefinition();
				if( pNewBestTarget == pTargetEntity)
				{
					//! Assume we didn't even search for new target to keep combo moves going.
				}
				else if( pNewBestTarget )
				{
					if(pActionToDo)
					{
						// Set the parent task to use the new best target entity
						CTask* pParentTask = GetParent();
						if( pParentTask ) 
							static_cast<CTaskMelee*>( pParentTask )->SetTargetEntity( pNewBestTarget, false, true );

						// Let the caller know what branch to go to.
						*pActionIdOut = pActionToDo->GetID();
						return true;
					}

					pTargetEntity = pNewBestTarget;
					bAllowNoTargetFallback = true;
				}
			}

			// Otherwise we should fallback to the ActionBranchSet
			const CActionBranchSet* pActionBranchSet = m_pActionResult->GetActionBranchSet();
			if( pActionBranchSet )
			{
				bool bShouldDebugPed = CActionManager::ShouldDebugPed( pPed );

				// Go over all the available action branches for this actionResult and
				// create a list of suitable actions to choose from.
				atFixedArray<const CActionDefinition*,MAX_SUITABLE_DEFINITIONS>	suitableActions;
				const u32 nNumBranches = pActionBranchSet->GetNumBranches();
				for( u32 nBranchIdx = 0; nBranchIdx < nNumBranches; ++nBranchIdx )
				{
					if( !CActionManager::PlayerAiFilterTypeDoesCatchPed( pActionBranchSet->GetBranchFilterType( nBranchIdx ), pPed ) )
						continue;

					const u32 nBranchActionId = pActionBranchSet->GetBranchActionID( nBranchIdx );
					const bool bIsAcceptable = ACTIONMGR.IsActionSuitable(
						nBranchActionId,
						typeToLookFor,				
						pImpulseCombo,
						false,						// This doesn't have to be an entry action (since this is a branch).
						false,						// This value doesn't matter as we aren't going to test it.
						pPed,
						0,							// There is no one forcing us, so there is no nForcingResultId.
						-1,
						pTargetEntity,
						HasLockOnTargetEntity(),
						bImpulseInitiatedMovesOnly,
						pPed->IsLocalPlayer(), 		// If this is a player then test the controls.
						bShouldDebugPed );

					if( !bIsAcceptable )
						continue;

					// Try to get the action.
					u32 nIndexFound = 0;
					const CActionDefinition* pBranchesAction = ACTIONMGR.FindActionDefinition( nIndexFound, nBranchActionId );
					Assertf( pBranchesAction, "pBranchesAction was not found for nBranchActionId in CTaskMeleeActionResult::ShouldBranchOnCurrentFrame (if you just reloaded the action table ignore this)." );
					if( !pBranchesAction )
						continue;

					// Add it to the list of acceptable actions.
					if( suitableActions.GetCount() < suitableActions.GetMaxCount() )				
						suitableActions.Append() = pBranchesAction;
				}

				// Should we check for no target actions if a target action wasn't found?
				if( suitableActions.GetCount() == 0 && pTargetEntity && (m_pActionResult->ShouldAllowNoTargetBranches() || bAllowNoTargetFallback) )
				{
					const u32 nNumBranches = pActionBranchSet->GetNumBranches();
					for( u32 nBranchIdx = 0; nBranchIdx < nNumBranches; ++nBranchIdx )
					{
						if( !CActionManager::PlayerAiFilterTypeDoesCatchPed( pActionBranchSet->GetBranchFilterType( nBranchIdx ), pPed ) )
							continue;

						const u32 nBranchActionId = pActionBranchSet->GetBranchActionID( nBranchIdx );
						const bool bIsAcceptable = ACTIONMGR.IsActionSuitable(
							nBranchActionId,
							typeToLookFor,				
							pImpulseCombo,
							false,						// This doesn't have to be an entry action (since this is a branch).
							false,						// This value doesn't matter as we aren't going to test it.
							pPed,
							0,							// There is no one forcing us, so there is no nForcingResultId.
							-1,
							NULL,
							false,
							bImpulseInitiatedMovesOnly,
							pPed->IsLocalPlayer(), 		// If this is a player then test the controls.
							bShouldDebugPed );

						if( !bIsAcceptable )
							continue;

						// Try to get the action.
						u32 nIndexFound = 0;
						const CActionDefinition* pBranchesAction = ACTIONMGR.FindActionDefinition( nIndexFound, nBranchActionId );
						Assertf( pBranchesAction, "pBranchesAction was not found for nBranchActionId in CTaskMeleeActionResult::ShouldBranchOnCurrentFrame (if you just reloaded the action table ignore this)." );
						if( !pBranchesAction )
							continue;

						// Add it to the list of acceptable actions.
						if( suitableActions.GetCount() < suitableActions.GetMaxCount() )				
							suitableActions.Append() = pBranchesAction;
					}

					// If we found an action make sure we clear out the target entity
					if( suitableActions.GetCount() > 0 )
					{
						pTargetEntity = NULL;
					}
				}

				// Try to select a single action branch to take from the list of actions
				// to choose from.
				pActionToDo = ACTIONMGR.SelectActionFromSet( &suitableActions, pPed, bShouldDebugPed );

				// Check if we found an action to take.
				if( pActionToDo )
				{
					// Let the caller know what branch to go to.
					*pActionIdOut = pActionToDo->GetID();

					CTask* pParentTask = GetParent();
					if( pParentTask ) 
						static_cast<CTaskMelee*>( pParentTask )->SetTargetEntity( pTargetEntity, false, true );

					return true;
				}
			}
		}
	}

	// We didn't find any branches to switch to.
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Shutdown
// PURPOSE :	Shutdown this task and release any referenced resources.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::Shutdown( void )
{
	if( sm_pTestBound )
	{
		sm_pTestBound->Release();
		sm_pTestBound = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HitImpulseCalculation
// PURPOSE :	Determine the physical impulse to apply for this hit based on the
//				intersection information.
// PARAMETERS :	pIntersection - Data about the intersection causing the impulse.
//				fImpulseMult - A scaling value for the impulse.
//				vImpulseDir - The direction of the impulse.
//				fImpulseMag - the strength of the impulse.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::HitImpulseCalculation( WorldProbe::CShapeTestHitPoint* pResult, float fImpulseMult, Vector3& vImpulseDir, float& fImpulseMag, CPed *hitPed )
{
	Assertf( pResult, "No valid hit-point data in CTaskMeleeActionResult::HitImpulseCalculation." );

	vImpulseDir = pResult->GetHitNormal();

	// don't want to just push ped into the ground all the time
	const float fHorizontalMag = vImpulseDir.XYMag();
	if( fHorizontalMag > 0.01f )
	{
		vImpulseDir.x /= fHorizontalMag;
		vImpulseDir.y /= fHorizontalMag;
		vImpulseDir.z = 0.0f;
	}

	// really want to get the force from the action result that hit us?
	if( fImpulseMult > 0.0f )
	{
		fImpulseMag = fImpulseMult;
	}
	else if (hitPed && hitPed->IsProne())
	{
		fImpulseMag = CTaskNMBehaviour::GetPronePedKickImpulse();
	}
	else
	{
		fImpulseMag = RAGDOLL_APPLY_MELEE_FORCE_MULT;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DoPhaseWindowsIntersect
// PURPOSE :	Helper function to determine if 2 separate time frames intersect
// PARAMETERS :	fCurrentPhase - Current clip phase
//				fNextPhase - Current phase plus the next time step
//				fStartPhase - Beginning of the time window
//				fEndPhase - End of the time window
// RETURNS :	Whether or not the time ranges intersect
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::DoPhaseWindowsIntersect( float fCurrentPhase, float fNextPhase, float fStartPhase, float fEndPhase )
{
	// Check if they intersect.
	const float fTimeIntervalDist = fCurrentPhase < fStartPhase ? ( fStartPhase - fNextPhase ) : ( fCurrentPhase - fEndPhase );
	if( fTimeIntervalDist < 0 ) 
		return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessCachedNetworkDamage
// PURPOSE :	Processes any cached network damage to sync it at correct point.
// PARAMETERS :	
// RETURNS :	
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessCachedNetworkDamage(CPed* pPed)
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessCachedNetworkDamage." );

	//! If we have pending network damage events, process them now.
	for(int i = 0; i < s_MaxCachedMeleeDamagePackets; ++i)
	{
		CTaskMeleeActionResult::MeleeNetworkDamage &CachedDamage = sm_CachedMeleeDamage[i];

		if(CachedDamage.m_uNetworkActionID == m_nUniqueNetworkActionID)
		{
			if(CachedDamage.m_pHitPed &&
				(CachedDamage.m_pInflictorPed == GetPed()) && 
				!EntityAlreadyHitByResult( CachedDamage.m_pHitPed ) )
			{
				const CDamageAndReaction* pDamageAndReaction = m_pActionResult->GetDamageAndReaction();

				// Default weapon to unarmed
				u32 uWeaponHash = pPed->GetWeaponManager() && pDamageAndReaction && !pDamageAndReaction->ShouldUseDefaultUnarmedWeapon() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : pPed->GetDefaultUnarmedWeaponHash();

				fwFlags32 flags( CachedDamage.m_uFlags );
				flags.SetFlag( CPedDamageCalculator::DF_AllowCloneMeleeDamage );

				AddToHitEntityCount(CachedDamage.m_pHitPed, CachedDamage.m_iComponent);

				CWeapon::ReceiveFireMessage(CachedDamage.m_pInflictorPed, 
					CachedDamage.m_pHitPed,
					uWeaponHash, 
					CachedDamage.m_fDamage,
					CachedDamage.m_vWorldHitPos,
					CachedDamage.m_iComponent,
					-1, 
					-1, 
					flags,
					CachedDamage.m_uParentMeleeActionResultID,
					CachedDamage.m_uNetworkActionID,
					CachedDamage.m_uForcedReactionDefinitionID);

				MELEE_NETWORK_DEBUGF("[%d] CTaskMeleeActionResult::ProcessCachedNetworkDamage - pFiringEntity = %s State = %d ID = %d Processing Collisions = %d %p", 
					fwTimer::GetFrameCount(), 
					CachedDamage.m_pInflictorPed->GetDebugName(), 
					GetState(), 
					m_nUniqueNetworkActionID, 
					m_bHasStartedProcessingCollisions ? 1 : 0,
					this);
			}

			sm_CachedMeleeDamage[i].Reset();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessSelfDamage
// PURPOSE :	Apply any damage to the ped that occurs simply by doing this move.
//				It is used to apply damaged during paired moves (where there is
//				no hit occurring) and to apply damage to one's self during moves
//				like head butts.
// PARAMETERS :	pPed - the ped of interest.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessSelfDamage( CPed* pPed, bool bForceFatalDamage )
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessSelfDamage." );

	// Make sure we haven't already applied the self damage 
	if( m_bSelfDamageApplied )
		return;

	// Make sure we have self damage to apply
	if( m_fSelfDamageAmount <= 0.0f )
		return;

	// Don't process self damage if we are quitting because clone ped didn't run task.
	if(pPed->IsNetworkClone() && IsRunningLocally() && CanCloneQuitForcedReaction(pPed))
		return;

	// Check friendly fire and skip damage if needed
	if (!NetworkInterface::IsFriendlyFireAllowed(pPed, GetTargetEntity()))
	{
		m_bSelfDamageApplied = true;
		return;
	}

	// Create a temp weapon to apply the damage.
	u32 nSelfDamageWeaponHash = GetSelfDamageWeaponHash();
	CWeapon selfDamageWeapon( nSelfDamageWeaponHash );

	// Setup the damage flags
	fwFlags32 flags( CPedDamageCalculator::DF_MeleeDamage | CPedDamageCalculator::DF_SelfDamage );
	if( ShouldForceDamage() )
		flags.SetFlag( CPedDamageCalculator::DF_ForceMeleeDamage );

	if( ShouldIgnoreArmor() )
		flags.SetFlag( CPedDamageCalculator::DF_IgnoreArmor );

	if( ShouldIgnoreStatModifiers() )
		flags.SetFlag( CPedDamageCalculator::DF_IgnoreStatModifiers );

	if (NetworkInterface::IsInCopsAndCrooks())
	{
		// Takedown moves are usually an instant kill by dealing 500 damage from the DamageAndReaction result (defined in damages.meta)
		// Override this with baseline melee damage
		if (m_pActionResult->GetDamageAndReaction() && m_pActionResult->GetDamageAndReaction()->GetSelfDamageAmount() >= pPed->GetMaxHealth())
		{
			m_fSelfDamageAmount = (float)GetMeleeEnduranceDamage();
		}

		// Melee attacks by Cops against Crooks are always non-lethal
		const CPed* pAttacker = SafeCast(const CPed, GetTargetEntity());
		if (ShouldApplyEnduranceDamageOnly(pAttacker, pPed))
		{
			flags.SetFlag(CPedDamageCalculator::DF_EnduranceDamageOnly);
		}
	}

	//! Force other ped to die if self damage is high enough.
	if(m_fSelfDamageAmount > pPed->GetHealth() && bForceFatalDamage)
	{
		flags.SetFlag( CPedDamageCalculator::DF_FatalMeleeDamage );
	}

	if(NetworkInterface::IsGameInProgress() && 
		pPed->IsNetworkClone() && 
		m_pTargetEntity && m_pTargetEntity->GetIsTypePed())
	{
		// If local player is causing this ped to inflict self damage, send damage event to them as well. This ensures they take damage, even if ped
		// didn't run melee task somehow.
		CPed* pTargetPed = static_cast<CPed*>(m_pTargetEntity.Get());

		bool fHealthMoreThanDamage = pPed && (pPed->GetMaxHealth() > m_fSelfDamageAmount);

		if(pTargetPed->IsLocalPlayer() && !pTargetPed->IsDead() && !fHealthMoreThanDamage)
		{
			eAnimBoneTag eSelfDamageAnimBoneTag = GetSelfDamageAnimBoneTag();
			s32 nSelfDamageWeaponComponent = -1;
			if( eSelfDamageAnimBoneTag != BONETAG_INVALID )
				nSelfDamageWeaponComponent = pPed->GetRagdollComponentFromBoneTag( eSelfDamageAnimBoneTag );

			CWeaponDamageEvent::Trigger(pTargetPed, 
				pPed, 
				VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ),
				nSelfDamageWeaponComponent,
				false, 
				nSelfDamageWeaponHash,
				m_fSelfDamageAmount,
				-1,
				-1,
				flags,
				0,
				m_nUniqueNetworkActionID, 
				0);
		}
	}	

	// Clones do damage on remote side.
	if( !pPed->IsNetworkClone() )
	{
		// Switch off the ragdoll on collision event
		pPed->SetActivateRagdollOnCollision( false );

		eAnimBoneTag eSelfDamageAnimBoneTag = GetSelfDamageAnimBoneTag();
		s32 nSelfDamageWeaponComponent = -1;
		if( eSelfDamageAnimBoneTag != BONETAG_INVALID )
			nSelfDamageWeaponComponent = pPed->GetRagdollComponentFromBoneTag( eSelfDamageAnimBoneTag );

		/// Apply the damage but give the credit to the opponent (if valid)
		CEntity* pTargetEntity = GetTargetEntity();
		CWeaponDamage::GeneratePedDamageEvent( pTargetEntity ? pTargetEntity : pPed, 
			pPed, 
			GetSelfDamageWeaponHash(), 
			m_fSelfDamageAmount, 
			VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ), 
			NULL, 
			flags, 
			&selfDamageWeapon, 
			nSelfDamageWeaponComponent );

		// Revert the one hit kill config flags if the event failed to kill the ped
		if( !pPed->IsInjured() ) 
		{ 
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth, false );
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown, false );
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout, false );
		} 
	}

	// The self damage has been applied so we can clear it.
	m_bSelfDamageApplied = true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessMeleeClipEvents
// PURPOSE :	Iterates through the current clip tag events and determines if
//				we have any intersecting time windows. This will drive real- time
//				clip events.
// PARAMETERS :	pPed - Operating ped
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessMeleeClipEvents( CPed* pPed )
{
	// Reset the clip tag flags
	m_bAllowInterrupt = false;
	m_bAllowDodge = false;
	m_bAllowHoming = false;
	m_bProcessCollisions = false;
	m_bMeleeInvulnerability = false;
	m_bFireWeapon = false;
	m_bSwitchToRagdoll = false;

	const crClip* pClip = fwClipSetManager::GetClip( m_clipSetId, m_clipId );
	if( pClip )
	{
		float fCurrentPhase = 0.0f;
		float fAnimRate = 1.0f;
		if( m_MoveNetworkHelper.IsNetworkActive() )
		{
			float fMoveNetworkCurrentPhase = m_MoveNetworkHelper.GetFloat( ms_ClipPhaseId );
			if( fMoveNetworkCurrentPhase != -1.0f )
				fCurrentPhase = fMoveNetworkCurrentPhase;
			float fMoveNetworkAnimRate = m_MoveNetworkHelper.GetFloat( ms_ClipRateId );
			if( fMoveNetworkAnimRate != -1.0f )
				fAnimRate = fMoveNetworkAnimRate;
		}

		float fNextPhase = MIN( fCurrentPhase + ( fAnimRate * pClip->ConvertTimeToPhase( GetTimeStep() ) ), 1.0f );

		crTag::Key uKey;
		crTagIterator it( *pClip->GetTags() );
		while(*it)
		{
			uKey = (*it)->GetKey();
			if( uKey == CClipEventTags::Interruptible.GetHash() )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					m_bAllowInterrupt = true;
			}
			else if ( uKey == CClipEventTags::ObjectVfx.GetHash() )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					ProcessVfxClipTag( pPed, pClip, *it );
			}
			else if( uKey == CClipEventTags::MeleeHoming.GetHash() )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					m_bAllowHoming = true;
			}
			else if( uKey == CClipEventTags::MeleeCollision.GetHash() )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
				{
					m_bProcessCollisions = true;
					m_bHasStartedProcessingCollisions = true;
				}
			}
			else if( uKey == CClipEventTags::MeleeInvulnerability.GetHash() )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					m_bMeleeInvulnerability = true;
			}
			else if( uKey == CClipEventTags::MeleeDodge.GetHash() )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					m_bAllowDodge = true;
			}
			else if( uKey == CClipEventTags::SwitchToRagdoll )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					m_bSwitchToRagdoll = true;
			}
			else if( uKey == CClipEventTags::MeleeFacialAnim )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					ProcessFacialClipTag( pPed, pClip, *it );
			}
			else if( uKey == CClipEventTags::CanSwitchToNm )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
				{
					if( !pPed->IsNetworkClone() && m_pActionResult && m_pActionResult->ActivateRagdollOnCollision() )
					{
						CEventSwitch2NM event( 10000, rage_new CTaskNMRelax( 1000, 10000 ) );
						pPed->SetActivateRagdollOnCollisionEvent( event );
						pPed->SetActivateRagdollOnCollision( true );
					}
				}
			}
			else if( uKey == CClipEventTags::Fire )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					m_bFireWeapon = true;
			}
			else if( uKey == CClipEventTags::CriticalFrame )
			{
				// Logic we can apply after the critical frame
				if( !IsPastCriticalFrame() && fNextPhase >= GetCriticalFrame() )
				{
					m_bPastCriticalFrame = true;

					// Add a shocking event to make people aware of the player swinging a punch.
					if( m_pActionResult && (m_pActionResult->GetIsAStandardAttack() || m_pActionResult->GetIsATakedown()) )
					{					
						CEventShockingSeenMeleeAction shockingEvent( *pPed, m_pTargetEntity );
						CShockingEventsManager::Add( shockingEvent );
						
						// Check for a player (local or clone) striking a ped target entity
						if( pPed->IsPlayer() && m_pTargetEntity && m_pTargetEntity->GetIsTypePed() )
						{
							// Get a handle to target ped
							CPed* pTargetPed = static_cast<CPed*>(m_pTargetEntity.Get());

							// If this target ped has a ped group and the config flag set to shout target on player melee
							if( pTargetPed->GetPedsGroup() && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ShoutToGroupOnPlayerMelee) )
							{
								// shout to all group members
								CEventShoutTargetPosition shoutTargetEvent(pTargetPed, pPed);
								pTargetPed->GetPedsGroup()->GiveEventToAllMembers(shoutTargetEvent, pTargetPed);
							}
						}
					}
				}
					
			}
			else if( uKey == CClipEventTags::MeleeContact )
			{
				if( DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
				{
					// Special case where we need to process the post hit results on critical frame
					if( m_pActionResult && m_pActionResult->ShouldProcessPostHitResultsOnContactFrame() )
					{
						CEntity* pTargetEntity = GetTargetEntity();
						if( pTargetEntity && pTargetEntity->GetIsTypePed() )
						{
							Vec3V vPropImpulse( 30.0f, 0.0f, 25.0f );
							float fPropImpulseMag = -1.0f;
							const crPropertyAttribute* pAttrib = (*it)->GetProperty().GetAttribute("RagdollImpulseDir");
							if( pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeVector3 )
							{
								const crPropertyAttributeVector3* pVector3Attrib = static_cast<const crPropertyAttributeVector3*>( pAttrib );
								vPropImpulse = pVector3Attrib->GetVector3();
							}

							pAttrib = (*it)->GetProperty().GetAttribute("RagdollImpulseMag");
							if( pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeFloat )
							{
								const crPropertyAttributeFloat* pFloatAttr = static_cast<const crPropertyAttributeFloat*>( pAttrib );
								fPropImpulseMag = pFloatAttr->GetFloat();
							}

							ProcessPostHitResults( pPed, static_cast<CPed*>( pTargetEntity ), true, vPropImpulse, fPropImpulseMag );
						}
					}
				}
			}
			else if( uKey == CClipEventTags::MoveEvent )
			{
				const crPropertyAttribute* pAttribute = (*it)->GetProperty().GetAttribute(CClipEventTags::MoveEvent);
				if( pAttribute != NULL && pAttribute->GetType() == crPropertyAttribute::kTypeHashString )
				{
					static const atHashString ms_BlendInRootIkSlopeFixup("BlendInRootIkSlopeFixup",0xDBFC38CA);
					const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
					if ( ( pSolver == NULL || pSolver->GetBlendRate() <= 0.0f ) &&
						ms_BlendInRootIkSlopeFixup == static_cast<const crPropertyAttributeHashString*>(pAttribute)->GetHashString() &&
						DoPhaseWindowsIntersect( fCurrentPhase, fNextPhase, (*it)->GetStart(), (*it)->GetEnd() ) )
					{
						pPed->GetIkManager().AddRootSlopeFixup(SLOW_BLEND_IN_DELTA);
					}
				}
			}

			++it;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessVfxClipTag
// PURPOSE :	Process a single Vfx tag
// PARAMETERS :	pPed - Operating ped
//				pClip - Debug use to print anim name
//				pTag - Tag that has the current vfx data
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessVfxClipTag( CPed* pPed, const crClip* ASSERT_ONLY(pClip), const crTag* pTag )
{
	const crPropertyAttribute* pAttrTrigger = pTag->GetProperty().GetAttribute( CClipEventTags::Trigger );
	if ( pAttrTrigger && aiVerifyf( pAttrTrigger->GetType() == crPropertyAttribute::kTypeBool, "Clip [%s] has a tag attribute %s that is not of the expected type [BOOL].", pClip->GetName(), CClipEventTags::Trigger.GetCStr() ) )
	{
		bool bIsTriggered = ((const crPropertyAttributeBool*)pAttrTrigger)->GetBool() ? true : false;

		const crPropertyAttribute* pAttrName = pTag->GetProperty().GetAttribute( CClipEventTags::VFXName );
		if ( pAttrName && aiVerifyf( pAttrName->GetType() == crPropertyAttribute::kTypeHashString, "Clip [%s] has a tag attribute %s that is not of the expected type [HashString].", pClip->GetName(), CClipEventTags::VFXName.GetCStr()))
		{
			const crPropertyAttributeHashString* pHashAttr = (const crPropertyAttributeHashString*)pAttrName;
			
			u32 nVfxIdx = 0;
			const CActionVfx* pVfx = ACTIONMGR.FindActionVfx( nVfxIdx, pHashAttr->GetHashString() );
			if( pVfx )
			{
				VfxMeleeTakedownInfo_s vfxInfo;

				vfxInfo.ptFxHashName = pVfx->GetVfxID();
				vfxInfo.dmgPackHashName = pVfx->GetDamagePackID();
				vfxInfo.pAttachEntity = pPed;
				vfxInfo.attachBoneTag = pVfx->GetBoneTag();
				vfxInfo.vOffsetPos = VECTOR3_TO_VEC3V( pVfx->GetOffset() );
				Quaternion tquat;
				tquat.FromEulers( pVfx->GetRotation() );
				vfxInfo.vOffsetRot.SetXf( tquat.x );
				vfxInfo.vOffsetRot.SetYf( tquat.y );
				vfxInfo.vOffsetRot.SetZf( tquat.z );
				vfxInfo.vOffsetRot.SetWf( tquat.w );
				vfxInfo.scale = pVfx->GetScale();

				if( bIsTriggered )
					g_vfxPed.TriggerPtFxMeleeTakedown( &vfxInfo );
				else
					g_vfxPed.UpdatePtFxMeleeTakedown( &vfxInfo );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessFacialClipTag
// PURPOSE :	Process a facial clip tag
// PARAMETERS :	pPed - Operating ped
//				pClip - Debug use to print anim name
//				pTag - Tag that has the current facial data
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessFacialClipTag( CPed* pPed, const crClip* ASSERT_ONLY(pClip), const crTag* pTag )
{
	// We only care if we have a facial data component
	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if( pFacialData )
	{
		const crPropertyAttribute* pAttrId = pTag->GetProperty().GetAttribute( CClipEventTags::Id );
		if ( pAttrId && aiVerifyf( pAttrId->GetType() == crPropertyAttribute::kTypeHashString, "Clip [%s] has a tag attribute %s that is not of the expected type [HASH STRING].", pClip->GetName(), CClipEventTags::Id.GetCStr() ) )
		{
			const crPropertyAttributeHashString* pHashAttr = (const crPropertyAttributeHashString*)pAttrId;

			u32 nFacialSetIdx = 0;
			const CActionFacialAnimSet* pFacialSet = ACTIONMGR.FindActionFacialAnimSet( nFacialSetIdx, pHashAttr->GetHashString() );
			if( pFacialSet && pFacialSet->GetNumFacialAnims() > 0 )
			{
				u32 nRandomIdx = fwRandom::GetRandomNumberInRange( 0, pFacialSet->GetNumFacialAnims() );
				pFacialData->PlayFacialAnim( pPed, pFacialSet->GetFacialAnimClipId( nRandomIdx ) );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessWeaponDisarmsAndDrops
// PURPOSE :	Handle any dropping or taking away of weapons that occurs during
//				this move.
// PARAMETERS :	pPed - the ped of interest.
// RETURNS :	If the melee task should quit.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::ProcessWeaponDisarmsAndDrops( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessWeaponDisarmsAndDrops." );

	if( ( !m_bDropWeapon && !m_bDisarmWeapon ) || !pPed->GetWeaponManager() )
		return false;

	// Get the weapon; note that pWeapon may be null if there is not a weapon in hand.
	const CObject* pHeldWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if( pHeldWeaponObject )
	{
		// Check if we should drop our weapon.
		if( m_bDropWeapon )
		{
			// See if we should drop the weapon in this frame.
			// Collect any events since the last frame and if there
			// was a AEF_DESTROY_OBJECT event then use it as
			// our drop weapon trigger.
			bool bDoWeaponDropThisFrame = false;
			if( m_MoveNetworkHelper.IsNetworkActive() )
			{
				const crClip* pClip = m_MoveNetworkHelper.GetClip( ms_ClipId );
				float fPhase = m_MoveNetworkHelper.GetFloat( ms_ClipPhaseId );
				if( CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeBool>( pClip, CClipEventTags::Object, CClipEventTags::Destroy, true, MAX( fPhase - 0.1f, 0.0f ), fPhase ) != NULL )
					bDoWeaponDropThisFrame = true;
			}
			else
				bDoWeaponDropThisFrame = true;

			if( bDoWeaponDropThisFrame )
				CPickupManager::CreatePickUpFromCurrentWeapon( pPed );
		}

		// See if we should grab away our opponents weapon.
		CEntity* pTargetEntity = GetTargetEntity();
		if( m_bDisarmWeapon && pTargetEntity )
		{
			bool bDoDisarmThisFrame = false;
			const crClip* pClip = m_MoveNetworkHelper.GetClip( ms_ClipId );
			if( pClip )
			{
				const CClipEventTags::CObjectEventTag* pTag = m_MoveNetworkHelper.GetEvent<CClipEventTags::CObjectEventTag>( CClipEventTags::Object );
				if( pTag && pTag->IsCreate() )
					bDoDisarmThisFrame = true;
			}
			else
				bDoDisarmThisFrame = true;

			if( bDoDisarmThisFrame )
			{
				pPed->GetWeaponManager()->DestroyEquippedWeaponObject();

				// Try to get a target ped from the opponent entity.
				CPed* pTargetPed = ( pTargetEntity && pTargetEntity->GetIsTypePed() ? static_cast<CPed*>(pTargetEntity) : NULL );
				if( pTargetPed )
				{
					// Transfer the opponents weapon to us.
					// Get the weapon; note that pWeapon may be null if there is not a weapon in hand.
					const CObject* pTargetPedsHeldWeaponObject = pTargetPed->GetWeaponManager() ? pTargetPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
					if( pTargetPedsHeldWeaponObject )
					{
						// Get a pointer to the opponents weapon.
						const CWeapon* pWeapon = pTargetPedsHeldWeaponObject->GetWeapon();
						Assert( pWeapon );

						// Give ourselves the same kind of weapon.
						pPed->GetInventory()->AddWeaponAndAmmo( pWeapon->GetWeaponHash(), pWeapon->GetAmmoTotal() );
						pPed->GetWeaponManager()->CreateEquippedWeaponObject();

						// Equip the weapon straight away
						pPed->GetWeaponManager()->EquipWeapon( pWeapon->GetWeaponHash() );

						// Make the opponent no longer have their weapon.
						pTargetPed->GetInventory()->RemoveWeapon( pWeapon->GetWeaponHash() );
						return true;
					}
				}
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessPreHitResults
// PURPOSE :	Perform the special result when the action begins
// PARAMETERS :	pPed - The owning ped
//				pTargetEntity - The current target of the ped.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessPreHitResults( CPed* pPed, CEntity* pTargetEntity )
{
	Assert( pPed );

	// Do not apply post hit results if already applied
	if( m_bPreHitResultsApplied )
		return;

	const CDamageAndReaction* pDamageAndReaction = m_pActionResult ? m_pActionResult->GetDamageAndReaction() : false;
	if( pDamageAndReaction )
	{
		if( pDamageAndReaction->ShouldStopDistanceHoming() )
		{
			CTaskMeleeActionResult* pSimpleMeleeActionResultTask = static_cast<CTaskMeleeActionResult*>(pPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT ) );
			if( pSimpleMeleeActionResultTask )
				pSimpleMeleeActionResultTask->StopDistanceHoming();
		}
		
		if( pTargetEntity && pTargetEntity->GetIsTypePed() && pDamageAndReaction->ShouldStopTargetDistanceHoming() )
		{
			CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );
			Assert( pTargetPed );
			CTaskMeleeActionResult* pSimpleMeleeActionResultTask = static_cast<CTaskMeleeActionResult*>(pTargetPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT ) );
			if( pSimpleMeleeActionResultTask)
				pSimpleMeleeActionResultTask->StopDistanceHoming();
		}
		
		if( pDamageAndReaction->ShouldAddSmallSpecialAbilityCharge() )
		{
			CPlayerSpecialAbility* pSpecialAbility = pPed->GetSpecialAbility();
			if( pSpecialAbility )
				pSpecialAbility->AddSmallCharge( false );
		}
		
		if( pDamageAndReaction->ShouldDisableInjuredGroundBehvaior() )
		{
			CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
			if( pIntelligence )
				pIntelligence->GetCombatBehaviour().DisableInjuredOnGroundBehaviour();
		}	

		if( !pDamageAndReaction->ShouldPreserveStealthMode() )
		{
			if( pPed->GetMotionData()->GetUsingStealth() )
				pPed->GetMotionData()->SetUsingStealth( false );
		}
	}
	
	// Toggle this off
	m_bPreHitResultsApplied = true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessPostHitResults
// PURPOSE :	Perform the special result (if one exists).
// PARAMETERS :	pPed - The owning ped
//				pTargetEntity - The current target of the ped.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessPostHitResults( CPed* pPed, CPed* pTargetPed, bool bForce, Vec3V_In vRagdollImpulseDir, float fRagdollImpulseMag )
{
	// Do not apply post hit results if already applied
	if( !bForce && m_bPostHitResultsApplied )
		return;

	const CDamageAndReaction* pDamageAndReaction = m_pActionResult ? m_pActionResult->GetDamageAndReaction() : nullptr;
	if( pDamageAndReaction )
	{
		// Apply controller rumble
		const CActionRumble* pActionRumble = pDamageAndReaction->GetActionRumble();
		if( pActionRumble && pPed->IsLocalPlayer() )
			CControlMgr::StartPlayerPadShakeByIntensity( pActionRumble->GetDuration(), pActionRumble->GetIntensity() );


#if FPS_MODE_SUPPORTED
		camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		if( pFpsCamera )
		{
			if( pPed && pTargetPed )
			{
				// Trigger camera shake when player is hit by melee attack in first person camera.
				// TODO: set using metadata?  Directional shakes?
				pFpsCamera->OnMeleeImpact(pPed, pTargetPed);
			}
		}
#endif // FPS_MODE_SUPPORTED

		if( pTargetPed && !pTargetPed->IsNetworkClone() && pPed->IsAllowedToDamageEntity( NULL, pTargetPed ) )
		{	
			// Should we knock off ped props? Note: Only knock props off peds we can damage.
			if( pDamageAndReaction->ShouldKnockOffProps() )
			{
				Vector3 vImpulse = VEC3V_TO_VECTOR3( Scale( vRagdollImpulseDir, ScalarV( fRagdollImpulseMag ) ) );

				// Limit the flat and up impulse of the hat to ensure that it clears the head instead of going through it. 
				Vector3 vFlatImpulse = vImpulse;
				vFlatImpulse.z = 0.0f;
				float fFlatMag = vFlatImpulse.Mag();
				TUNE_GROUP_FLOAT(MELEE,HAT_MAX_FLAT_IMPULSE, 7.0f,0.0f,100.0f,1.0f);
				vFlatImpulse *= Min(HAT_MAX_FLAT_IMPULSE,fFlatMag)*InvertSafe(fFlatMag,0.0f);

				TUNE_GROUP_FLOAT(MELEE,HAT_MIN_UP_IMPULSE,12.0f,0.0f,100.0f,1.0f);
				TUNE_GROUP_FLOAT(MELEE,HAT_MAX_UP_IMPULSE,16.0f,0.0f,100.0f,1.0f);
				Vector3 vHatImpulse(vFlatImpulse.x,vFlatImpulse.y,Clamp(vImpulse.z,HAT_MIN_UP_IMPULSE,HAT_MAX_UP_IMPULSE));

				// Assume that the head is going to move slower than the hat so roll the hat so that the top
				//   gets oriented towards the impulse vector
				TUNE_GROUP_FLOAT(MELEE,HAT_ANG_IMPULSE_SCALE,1.0f,0.0f,10.0f,0.1f);
				Vector3 vHatAngImpulse(0.0f,0.0f,1.0f);
				vHatAngImpulse.Cross(vHatImpulse);

				CPedPropsMgr::KnockOffProps( pTargetPed, true, true, true, false, &vHatImpulse, NULL, &vHatAngImpulse);
			}

			if( pDamageAndReaction->ShouldInvokeFacialPainAnimation() )
			{
				// Play facial pain animations
				CFacialDataComponent* pFacialData = pTargetPed->GetFacialData();
				if( pFacialData && !pTargetPed->IsDead() )
					pFacialData->PlayPainFacialAnim( pTargetPed );			
			}
		}

		CWeapon* pWeapon = pPed->GetWeaponManager() ?
			pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if( pWeapon )
		{
			const CWeaponInfo *pWeaponInfo = pWeapon->GetWeaponInfo();
			u32 uNextDiffuseCamoTexture = 0;

			// Check that we're using our right hand to do the attack (so we don't apply blood on kicks etc)
			// Currently don't have any left hand melee weapons to apply blood to (will need to update this if we want it for unarmed left hand attacks etc).
			bool bIsValidStrike = false;
			const CStrikeBoneSet* pStrikeBoneSet = m_pActionResult ? m_pActionResult->GetStrikeBoneSet() : NULL;
			if (pStrikeBoneSet)
			{
				const u32 nStrikeBoneCount = pStrikeBoneSet->GetNumStrikeBones();
				for( u32 i = 0; i < nStrikeBoneCount; ++i )
				{
					const eAnimBoneTag strikingBoneTag = pStrikeBoneSet->GetStrikeBoneTag( i );
					if (strikingBoneTag == BONETAG_R_HAND)
					{
						bIsValidStrike = true;
					}
				}
			}

			if( bIsValidStrike && pWeaponInfo && pWeaponInfo->GetNumCammoDiffuseTextures() > 0 && pWeaponInfo->GetNextCamoDiffuseTexIdx( *pWeapon, uNextDiffuseCamoTexture ) )
			{
				pWeapon->SetCamoDiffuseTexIdx( uNextDiffuseCamoTexture );
			}
		}
	}

	if( !m_bPostHitResultsApplied && pTargetPed )
	{
		switch (CCrime::sm_eReportCrimeMethod)
		{
		case CCrime::REPORT_CRIME_DEFAULT:
			if( pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown) )
			{
				CCrime::ReportCrime(pTargetPed->IsLawEnforcementPed() ? CRIME_KILL_COP : CRIME_KILL_PED, pTargetPed, pPed);
			}
			break;
		case CCrime::REPORT_CRIME_ARCADE_CNC:
			// NFA - ped and cop deaths are handled in script for CnC
			break;
		}
		if(pTargetPed->GetSpeechAudioEntity())
		{
			pTargetPed->GetSpeechAudioEntity()->MarkToStopSpeech();
			if( !pTargetPed->IsDead() && pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_UNALERTED ) )
			{
				// Probably want to check the last weapon hash that hit this ped and make sure it is 0
				float fInsultProbability = fwRandom::GetRandomNumberInRange( 0.0, 1.0f );
				if( fInsultProbability < 0.33f)
					pTargetPed->GetSpeechAudioEntity()->SayWhenSafe( "ATTACKED_BY_SURPRISE" );
			}
		}
	}

	// Toggle this off
	m_bPostHitResultsApplied = true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessHomingAndSeparation
// PURPOSE :	Apply the minimum separation and homing values set in the
//				action table.
//				These are used to make moves appear cleaner by helping align the
//				ped to it's target.
//				It is used to do things like making a punch land exactly on a
//				chin even when the punch can be invoked from a variety of
//				distances.
// PARAMETERS :	pPed - the ped of interest.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessHomingAndSeparation( CPed* pPed, float fTimeStep, int nTimeSlice )
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessHomingAndSeparation." );

	if( !m_pActionResult )
		return;

	CEntity* pTargetEntity = GetTargetEntity();

	// Apply homing to the ped.
	CHoming* pHoming = GetHoming(pPed);
	if( pHoming )
	{
		if(pPed->IsNetworkClone())
		{
			//! Don't allow clones to home in on probe homing types. Not really necessary - just use net blended owners position.
			if(pHoming && pHoming->IsSurfaceProbe() )
			{
				m_bDidAllowDistanceHoming = false;
				return;
			}

			//! Don't allow clones to home in on dead peds. Position is unreliable and may not match owner machine.
			if(pTargetEntity && pTargetEntity->GetIsTypePed())
			{
				CPed *pTargetPed = static_cast<CPed*>(pTargetEntity);
				if(pTargetPed->IsDead())
				{	
					m_bDidAllowDistanceHoming = false;
					return;
				}
			}

			static dev_float s_fHomingToleranceDistance = 3.25f;
			static dev_float s_fHomingToleranceKillMoveDistance = 5.0f;

			bool bKillMove = m_pActionResult->GetIsATakedown() || m_pActionResult->GetIsAStealthKill() || m_pActionResult->GetIsAKnockout();
			float fHomingTolerance = bKillMove ? s_fHomingToleranceKillMoveDistance : s_fHomingToleranceDistance;

			Vector3 vCloneTargetPosition = VEC3V_TO_VECTOR3(m_vCloneCachedHomingPosition);
			Vector3 vLocalTargetPosition = VEC3V_TO_VECTOR3(m_vCachedHomingPosition);
			Vector3 vDist = vCloneTargetPosition - vLocalTargetPosition;
			float fDistSq = vDist.Mag2();
			if(fDistSq > (fHomingTolerance*fHomingTolerance) )
			{
				m_bDidAllowDistanceHoming = false;
				return;
			}
		}

		const crClip* pClip = NULL;
		float fCurrentPhase = -1.0f;
		if( m_MoveNetworkHelper.IsNetworkActive() )
		{
			fCurrentPhase = m_MoveNetworkHelper.GetFloat( ms_ClipPhaseId );
			pClip = m_MoveNetworkHelper.GetClip( ms_ClipId );
		}

		// Unfortunately there is a frame delay despite me setting this information in Start_OnUpdate
		if( fCurrentPhase == -1.0f )
			fCurrentPhase = GetInitialAnimPhase();

		if( !pClip )
			pClip = fwClipSetManager::GetClip( m_clipSetId, m_clipId );

		bool bAllowHeadingUpdate = AllowHeadingUpdate();
		bool bUpdateDesiredHeading = false;
		if( bAllowHeadingUpdate && !m_bHeadingInitialized )
		{
			m_bHeadingInitialized = true;
			bUpdateDesiredHeading = true;
		}

		bool bAllowDistanceHoming = ShouldAllowDistanceHoming();
		
		if(!pPed->IsNetworkClone())
		{
			// CNC: stop shoulder barge homing if target is ragdolling.
			if (NetworkInterface::IsInCopsAndCrooks() && pTargetEntity && pTargetEntity->GetIsTypePed() && m_pActionResult->GetIsShoulderBarge())
			{
				const CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);
				if (pTargetPed->GetUsingRagdoll())
				{
					bAllowDistanceHoming = false;
				}
			}

			// Special distance homing considerations for standard attacks
			if( bAllowDistanceHoming && !pHoming->ShouldIgnoreSpeedMatchingCheck() &&
				m_pActionResult->GetIsAStandardAttack() && pTargetEntity && pTargetEntity->GetIsTypePed() )
			{
				CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );

				// If the target ped is running 
				float fTargetPedmoveBlendRatioSq = pTargetPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
				if( fTargetPedmoveBlendRatioSq > rage::square( MBR_RUN_BOUNDARY ) )
				{
					// Check to see if the target is attempting to run away from you
					static dev_float sfTargetDotThreshold = -0.5f;
					Vec3V vPedToTarget = pTargetEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition();
					vPedToTarget = Normalize( vPedToTarget );
					const float fTargetDirectionDot = Dot( vPedToTarget, pTargetEntity->GetTransform().GetB() ).Getf();
					if( fTargetDirectionDot > sfTargetDotThreshold )
					{
						static dev_float sfSpeedDifferenceThreshold = 0.95f;
						const float fSpeedComparision = fTargetPedmoveBlendRatioSq - GetCachedMoveBlendRatioSq();
						if( fSpeedComparision > sfSpeedDifferenceThreshold )
							bAllowDistanceHoming = false;
					}				
				}
			}
		}

		//! Don't allow distance homing if owner ped isn't.
		if(pPed->IsNetworkClone())
		{
			if(m_bOwnerStoppedDistanceHoming)
				bAllowDistanceHoming = false;
		}
		else
		{
			//! Inform clone if owner stops homing.
			if( (DidAllowDistanceHoming() || ShouldAllowDistanceHoming())  && !bAllowDistanceHoming)
			{
				m_bOwnerStoppedDistanceHoming = true;
			}
		}

		pHoming->Apply( pPed, pTargetEntity, m_vCachedHomingPosition, m_vCachedHomingDirection, m_vMoverDisplacement, m_nBoneIdxCache, pClip, fCurrentPhase, bAllowDistanceHoming, DidAllowDistanceHoming(), bAllowHeadingUpdate, HasJustStruckSomething(), bAllowHeadingUpdate && bUpdateDesiredHeading, fTimeStep, nTimeSlice, m_vCachedHomingPedPosition );

		// Cache off whether or not we allowed distance homing this time around
		m_bDidAllowDistanceHoming = bAllowDistanceHoming;

		// If we are a player who is attacking another ped and homing is allowed, set the appropriate reset flag
		if( pPed->IsAPlayerPed() &&			
			(bAllowDistanceHoming || bAllowHeadingUpdate) &&
			pTargetEntity && pTargetEntity->GetIsTypePed() )
		{
			CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );
			pTargetPed->SetPedResetFlag(CPED_RESET_FLAG_IsBeingMeleeHomedByPlayer, true);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessMeleeCollisions
// PURPOSE :	Determine if during the playback of this move the ped has struck
//				anything and if so handle the reactions to that strike.
// PARAMETERS :	pPed - the ped of interest.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ProcessMeleeCollisions( CPed* pPed )
{
#if __BANK
	if( !CCombatMeleeDebug::sm_bDoCollisions )
		return;
#endif // __BANK

	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessMeleeCollisions." );

	// Make sure there is an clip playing.
	if( !m_MoveNetworkHelper.IsNetworkActive() || !m_MoveNetworkHelper.GetClip( ms_ClipId ) )
		return;

	// Make sure the action result still exists as it can go away when reloading the action table.
	if( !m_pActionResult )
		return;

	if( !ShouldProcessMeleeCollisions() )
		return;

	// Determine what bounds to check (this will include bounds
	// that approximate weapons).
	const phBound* pTestBound = GetCustomHitTestBound( pPed );
	if( !pTestBound )
		return;

	// Make sure we can get the ragdoll data.
	if( !pPed->GetRagdollInst() )
		return;

	const u32 MAX_MELEE_COLLISION_RESULTS_PER_PROBE = 4;
	
	phBoundComposite* pCompositeBound = (phBoundComposite*) pTestBound;
	const int nBoundCount = pCompositeBound->GetNumBounds();

	if( Verifyf( (nBoundCount * MAX_MELEE_COLLISION_RESULTS_PER_PROBE) <= (BOUND_MAX_STRIKE_CAPSULES * MAX_MELEE_COLLISION_RESULTS_PER_PROBE), "CTaskMeleeActionResult::ProcessMeleeCollisions - Need to increase nMaxNumResults [NumBounds=%d NumCollisionResults=%d NumCollisionResultsPerProbe=%d]!", nBoundCount, BOUND_MAX_STRIKE_CAPSULES * MAX_MELEE_COLLISION_RESULTS_PER_PROBE, MAX_MELEE_COLLISION_RESULTS_PER_PROBE ) )
	{
		WorldProbe::CShapeTestFixedResults<BOUND_MAX_STRIKE_CAPSULES * MAX_MELEE_COLLISION_RESULTS_PER_PROBE> boundTestResults;

		Matrix34 matPed = RCC_MATRIX34( pPed->GetRagdollInst()->GetMatrix() );

		// treat the test as type ped, so it'll collide with stuff a ped will (fixes boats, want to collide with BVH not normal pBound)
		const u32 nTypeFlags = ArchetypeFlags::GTA_WEAPON_TEST;

		u32 nIncludeFlags = ArchetypeFlags::GTA_WEAPON_TYPES &~ ArchetypeFlags::GTA_RIVER_TYPE;
		if( pPed->IsPlayer() )
			nIncludeFlags |= ArchetypeFlags::GTA_BOX_VEHICLE_TYPE;

		// Fill in the descriptor for the batch test.
		WorldProbe::CShapeTestBatchDesc batchDesc;
		batchDesc.SetOptions(0);
		batchDesc.SetIncludeFlags( nIncludeFlags );
		batchDesc.SetTypeFlags( nTypeFlags );
		batchDesc.SetExcludeEntity( pPed );
		batchDesc.SetTreatPolyhedralBoundsAsPrimitives( false );
		WorldProbe::CShapeTestCapsuleDesc* pCapsuleDescriptors = Alloca(WorldProbe::CShapeTestCapsuleDesc, nBoundCount);
		batchDesc.SetCapsuleDescriptors(pCapsuleDescriptors, nBoundCount);

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetResultsStructure( &boundTestResults );
		capsuleDesc.SetMaxNumResultsToUse( MAX_MELEE_COLLISION_RESULTS_PER_PROBE );
		capsuleDesc.SetExcludeEntity( pPed );
		capsuleDesc.SetIncludeFlags( nIncludeFlags );
		capsuleDesc.SetTypeFlags( nTypeFlags );
		capsuleDesc.SetIsDirected( true );
		capsuleDesc.SetTreatPolyhedralBoundsAsPrimitives( false );

		for( int i = 0; i < nBoundCount; ++i )
		{
			Matrix34 matCapsule = RCC_MATRIX34( pCompositeBound->GetCurrentMatrix(i) );
			matCapsule.Dot( matPed );

			const phBound* pBound = pCompositeBound->GetBound(i);
			if( pBound )
			{
				const phBoundCapsule* pCapsule = static_cast<const phBoundCapsule*>( pBound );
				TUNE_GROUP_FLOAT( MELEE, fRadiusMult, 1.0f, 0.0f, 10000.0f, 0.1f );
				const float	fCapsuleRadius = pCapsule->GetRadius() * fRadiusMult;
				const Vector3 vCapsuleStart	= matCapsule * VEC3V_TO_VECTOR3( pCapsule->GetEndPointA() );
				const Vector3 vCapsuleEnd = matCapsule * VEC3V_TO_VECTOR3( pCapsule->GetEndPointB() );
	#if __BANK
				char szDebugText[128];
				formatf( szDebugText, "MELEE_COLLISION_SPHERE_START_%d", i );
				CTask::ms_debugDraw.AddSphere( RCC_VEC3V(vCapsuleStart), fCapsuleRadius, Color_green, 2, atStringHash(szDebugText), false );
				formatf( szDebugText, "MELEE_COLLISION_SPHERE_END_%d", i );
				CTask::ms_debugDraw.AddSphere( RCC_VEC3V(vCapsuleEnd), fCapsuleRadius, Color_green4, 2, atStringHash(szDebugText), false );
				formatf( szDebugText, "MELEE_COLLISION_ARROW_%d", i );
				CTask::ms_debugDraw.AddArrow( RCC_VEC3V(vCapsuleStart), RCC_VEC3V(vCapsuleEnd), fCapsuleRadius, Color_blue, 2, atStringHash(szDebugText) );
	#endif 
				capsuleDesc.SetFirstFreeResultOffset( i * MAX_MELEE_COLLISION_RESULTS_PER_PROBE );
				capsuleDesc.SetCapsule( vCapsuleStart, vCapsuleEnd, fCapsuleRadius );
				batchDesc.AddCapsuleTest( capsuleDesc );
			}
		}

		if( WorldProbe::GetShapeTestManager()->SubmitTest( batchDesc ) )
		{
			// First we need to add all valid results to the sorted result array
			atFixedArray< sCollisionInfo, BOUND_MAX_STRIKE_CAPSULES * MAX_MELEE_COLLISION_RESULTS_PER_PROBE > sortedCollisionResults;
			for( int iStrikeCapsules = 0; iStrikeCapsules < nBoundCount; iStrikeCapsules++ )
			{
				for( int iResult = 0; iResult < MAX_MELEE_COLLISION_RESULTS_PER_PROBE; iResult++ )
				{
					int iIndex = (iStrikeCapsules * MAX_MELEE_COLLISION_RESULTS_PER_PROBE) + iResult;
					// Ignore car void material
					if( PGTAMATERIALMGR->UnpackMtlId( boundTestResults[iIndex].GetHitMaterialId() ) == PGTAMATERIALMGR->g_idCarVoid )
						continue;

					phInst* pHitInst = boundTestResults[iIndex].GetHitInst();
					if( !pHitInst || !pHitInst->IsInLevel() )
						continue;

					CEntity* pHitEntity = CPhysics::GetEntityFromInst( pHitInst );
					if( !pHitEntity )
						continue;

					sCollisionInfo& result = sortedCollisionResults.Append();
					result.m_pResult = &boundTestResults[iIndex];
					result.m_vImpactDirection = pCapsuleDescriptors[iStrikeCapsules].GetEnd() - pCapsuleDescriptors[iStrikeCapsules].GetStart();
					result.m_vImpactDirection.NormalizeSafe();
				}
			}

			// Quick sort based on melee collision criteria
			sortedCollisionResults.QSort( 0, -1, CTaskMelee::CompareCollisionInfos );

			static dev_float sfPedHitDotThreshold = 0.0f;
			static dev_float sfSurfaceHitDotThreshold = 0.5f;

			Vec3V vPedPosition = pPed->GetTransform().GetPosition();
			Vec3V vDesiredDirection( V_ZERO );

			// If we have a target entity then use that direction to do the dot threshold comparison
			CEntity* pTargetEntity = GetTargetEntity();

			//! If target entity is NULL, always hit hard lock target.
			if(pPed->GetPlayerInfo() && !pTargetEntity)
			{
				CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();
				pTargetEntity = rTargeting.GetLockOnTarget();
			}

			if( pTargetEntity )
			{
				vDesiredDirection = pTargetEntity->GetTransform().GetPosition() - vPedPosition;
				vDesiredDirection.SetZf( 0.0f );
				vDesiredDirection = Normalize( vDesiredDirection );
			}
			// As a backup lets use the desired direction
			else
			{
				float fDesiredHeading = fwAngle::LimitRadianAngleSafe( pPed->GetDesiredHeading() );
				vDesiredDirection.SetXf( -rage::Sinf( fDesiredHeading ) );
				vDesiredDirection.SetYf( rage::Cosf( fDesiredHeading ) );
			}
			Vec3V vDirectionToPosition( V_ZERO );

			bool bShouldIgnoreHitDotThrehold = m_pActionResult->ShouldIgnoreHitDotThreshold();

			// Go over any impacts that were detected.
			u32 nNumSortedResults = sortedCollisionResults.GetCount();
			for( u32 i = 0; i < nNumSortedResults; ++i )
			{
				CEntity* pHitEntity = sortedCollisionResults[i].m_pResult->GetHitEntity();
				if( !pHitEntity )
					continue;

				if (NetworkInterface::AreInteractionsDisabledInMP(*pPed, *pHitEntity))
					continue;

				const bool bIsBreakable = ( IsFragInst( sortedCollisionResults[i].m_pResult->GetHitInst() ) && sortedCollisionResults[i].m_pResult->GetHitInst()->IsBreakable( NULL ) );

				// Make sure we haven't already hit this entity component with this
				// action (so we don't get double or triple hits, etc. per action).
				const bool bEntityAlreadyHitByThisAction = EntityAlreadyHitByResult( pHitEntity );

				if( bIsBreakable || !bEntityAlreadyHitByThisAction )
				{
					// Do not allow non ped collisions that are in the opposite direction of the attack
					vDirectionToPosition = sortedCollisionResults[i].m_pResult->GetHitPositionV() - vPedPosition;
					vDirectionToPosition = Normalize( vDirectionToPosition );

					if( pHitEntity->GetIsTypePed() )
					{
						// We allow a wider angle to go through on ped melee collisions 
						if( pHitEntity != pTargetEntity && !bIsBreakable && !bShouldIgnoreHitDotThrehold && Dot( vDesiredDirection, vDirectionToPosition ).Getf() < sfPedHitDotThreshold )
							continue;

						// Ignore collisions with actions that have a higher priority than the striking peds current action
						CPed* pHitPed = static_cast<CPed*>(pHitEntity);

						// Don't allow clones to hit other peds until we get notification that it is ok.
						if(pPed->IsNetworkClone())
						{
							MeleeNetworkDamage *pNetworkDamage = FindMeleeNetworkDamageForHitPed(pHitPed);
							if(!pNetworkDamage)
								continue;
						}

						// Some peds have melee disabled entirely
						if( pHitPed->GetPedResetFlag( CPED_RESET_FLAG_DisableMeleeHitReactions ) )
							continue;

						// Should we ignore friendly attacks?
						if( CActionManager::ArePedsFriendlyWithEachOther( pPed, pHitPed ) )
						{
							if( pPed->IsLocalPlayer() && pHitPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerMeleeFriendlyAttacks ) )
								continue;
							else if( !pPed->IsLocalPlayer() && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackFriendly ) )
								continue;
						}

						CTaskMeleeActionResult* pHitPedCurrentMeleeActionResultTask = pHitPed->GetPedIntelligence() ? pHitPed->GetPedIntelligence()->GetTaskMeleeActionResult() : NULL;
						if( pHitPedCurrentMeleeActionResultTask )
						{
							// Invulnerable?
							if( pHitPedCurrentMeleeActionResultTask->IsInvulnerableToMelee() )
								continue;

							// Skip if we currently do not allow interrupts and the current offensive action has a greater priority
							bool bAllowInterrupt = pHitPedCurrentMeleeActionResultTask->ShouldAllowInterrupt() || 
												   pHitPedCurrentMeleeActionResultTask->HasJustStruckSomething();
							bool bGreaterPriority = pHitPedCurrentMeleeActionResultTask->GetResultPriority() > GetResultPriority();
							if( pHitPedCurrentMeleeActionResultTask->IsAStandardAttack() && ( bGreaterPriority && !bAllowInterrupt ) )
								continue;
						}

						if(pHitPed->GetVehiclePedInside())
						{
							CVehicle *pVehicleHitPedInside = pHitPed->GetVehiclePedInside();
							
							//! This is just a rough approximation for melee'ing peds in vehicles. A more thourough shapetest may be the way to go, but we can broadly rule the ped
							//! in or out here.
							if(pVehicleHitPedInside->InheritsFromBike() || 
								pVehicleHitPedInside->InheritsFromQuadBike() || 
								pVehicleHitPedInside->InheritsFromBoat() ||
								pVehicleHitPedInside->InheritsFromAmphibiousQuadBike())
							{
								//! ok to target peds in these vehicles.
							}
							else
							{
								//! don't target peds in vehicles with roofs. Note: May want to do something about melee'ing peds if the door has been blown off, but we don't
								//! really support this, so just disable.
								if(pVehicleHitPedInside->HasRaisedRoof())
								{
									continue;
								}
							}
						}

						// Check against hands and lower arms as they do not count
						u16 iHitComponent = sortedCollisionResults[i].m_pResult->GetHitComponent();
						eAnimBoneTag nHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent( iHitComponent );
						if( nHitBoneTag==BONETAG_R_FOREARM || nHitBoneTag==BONETAG_R_HAND ||
							nHitBoneTag==BONETAG_L_FOREARM || nHitBoneTag==BONETAG_L_HAND ) 
							continue;
					}
					else
					{
						//! All network damage is done from owner side.
						if(pPed->IsNetworkClone())
							continue;

						// Tighten up the allow collision angle on all other surfaces
						if( pHitEntity != pTargetEntity && !bIsBreakable && !bShouldIgnoreHitDotThrehold && Dot( vDesiredDirection, vDirectionToPosition ).Getf() < sfSurfaceHitDotThreshold )
							continue;

						if( pHitEntity->GetIsTypeObject() )
						{
							CObject* pObject = static_cast<CObject*>(pHitEntity);
							if( pObject->IsADoor() )
							{
								audDoorAudioEntity* pDoorAudEntity = (audDoorAudioEntity *)(((CDoor *)pHitEntity)->GetDoorAudioEntity() );
								if( pDoorAudEntity && !bEntityAlreadyHitByThisAction )
									pDoorAudEntity->ProcessCollisionSound( sortedCollisionResults[i].m_pResult->GetHitTValue(), true, pPed );
							}
							// Otherwise check if we should ignore this object collision
							else
							{
								if( pObject->GetWeapon() )
									continue;

								if( pObject->m_nObjectFlags.bAmbientProp || pObject->m_nObjectFlags.bPedProp || pObject->m_nObjectFlags.bIsPickUp )
									continue;

								if( pObject->GetAttachParent() )
									continue;

								// Toggle this off and allow melee attacks apply impulses here
								if( pObject->m_nObjectFlags.bFixedForSmallCollisions )
								{	
									pObject->m_nObjectFlags.bFixedForSmallCollisions = false;
									pObject->SetFixedPhysics( false );
								}
							}
						}
					}

					// We only ever want to play the audio once, even if it is a breakable
					if( !bEntityAlreadyHitByThisAction )
						pPed->GetPedAudioEntity()->PlayMeleeCombatHit( m_nSoundNameHash, *sortedCollisionResults[i].m_pResult, m_pActionResult  BANK_ONLY(, m_pActionResult->GetName() ) );

					// Add the entity component to the list of ones already hit by this result.
					AddToHitEntityCount(pHitEntity, sortedCollisionResults[i].m_pResult->GetHitComponent());

					// Mark that a hit has occurred.
					m_bHasJustStruckSomething = true;
#if __BANK
					char szDebugText[128];
					formatf( szDebugText, "MELEE_COLLISION_SPHERE_%d", i );
					CTask::ms_debugDraw.AddSphere( sortedCollisionResults[i].m_pResult->GetHitPositionV(), 0.05f, Color_red, 2, atStringHash(szDebugText), false );
#endif
				}

				// Make the hit entity react.  If they are a ped that is already reacting to
				// a hit then they will most likely only continue that reaction.
				TriggerHitReaction( pPed, pHitEntity, sortedCollisionResults[i], bEntityAlreadyHitByThisAction, bIsBreakable );

				// If we have hit anything besides a ped, break out of the loop
				if( !pHitEntity->GetIsTypePed() )
					break;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AddToHitEntityCount
// PURPOSE :	Add entity to hit list so we know if we have already hit it.
// PARAMETERS :	pHitEntity - the entity to add.
// RETURNS :	
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::AddToHitEntityCount(CEntity* pEntity, s32 component)
{
	const CEntityComponent entityComponent( pEntity, component );

	if( m_nHitEntityComponentsCount < sm_nMaxHitEntityComponents )
	{
		m_hitEntityComponents[m_nHitEntityComponentsCount] = entityComponent;
		++m_nHitEntityComponentsCount;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	EntityAlreadyHitByResult
// PURPOSE :	Determine if this entity has already been struck by this action
//				result.
//				This is used to filter multiple hits coming back from the
//				collision detection system and to allow the hit entity to
//				properly react to the first strike.
// PARAMETERS :	pHitEntity - the entity to check.
// RETURNS :	Whether or not the have already been hit by this action.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::EntityAlreadyHitByResult( const CEntity* pHitEntity ) const
{
	// Check if the entity is already in the list (with
	// a simple linear search).
	for(u32 i = 0; i < m_nHitEntityComponentsCount; ++i)
	{
		if( pHitEntity == m_hitEntityComponents[i].m_pEntity )
			return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HasActiveMeleeCollision
// PURPOSE :	Determine if the current clip has a MeleeCollision clip tag 
//				given the current phase time.
// RETURNS :	Whether or not we have any active bone tags
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::HasActiveMeleeCollision( CPed* pPed ) const
{
	if( !pPed || !m_pActionResult )
		return false;

	// Make sure there is an clip playing.
	const crClip* pClip = m_MoveNetworkHelper.GetClip( ms_ClipId );
	if( !pClip )
		return false;

	float fPhase = m_MoveNetworkHelper.GetFloat( ms_ClipPhaseId );
	if( CClipEventTags::HasEvent( pClip, CClipEventTags::MeleeCollision, fPhase ) )
		return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetCustomHitTestBound
// PURPOSE :	Create a temporary pBound to use for determining if the ped has
//				struck anything or not.
//				We don't use the full ragdoll collisions since that would be
//				inefficient and would also create spurious collisions, such as
//				the toes of the opponents touching during a fist fight.
// PARAMETERS :	pPed - the ped to make the pBound for.
// RETURNS :	The new custom pBound.
/////////////////////////////////////////////////////////////////////////////////
phBound* CTaskMeleeActionResult::GetCustomHitTestBound( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::GetCustomHitTestBound." );

	// Make sure there is an action playing.
	if( !m_pActionResult )
		return NULL;

	// Make sure the action has an clip playing.
	if( !m_MoveNetworkHelper.GetClip( ms_ClipId ) )
		return NULL;

	// Make sure there is ragdoll data, since we use it to determine the pBound.
	if( !pPed->GetRagdollInst() || !pPed->GetRagdollInst()->GetArchetype() )
		return NULL;

	// Make sure to get the ragdolls composite pBound, which is used to get the
	// individual bone's bounds.
	const phBoundComposite*  pRagdollBoundComp = dynamic_cast<phBoundComposite*>( pPed->GetRagdollInst()->GetArchetype()->GetBound() );
	if( !pRagdollBoundComp )
		return NULL;

	// Go over all the active strikeBones for this actionResult and
	// create a list of capsules to use to approximate the moving bounds.
	u32 nNumStrikeCapsules = 0;
	StrikeCapsule strikeCapsules[BOUND_MAX_STRIKE_CAPSULES];

	const CStrikeBoneSet* pStrikeBoneSet = m_pActionResult->GetStrikeBoneSet();
	if( !pStrikeBoneSet )
		return NULL;

	const u32 nStrikeBoneCount = pStrikeBoneSet->GetNumStrikeBones();
	for( u32 i = 0; i < nStrikeBoneCount; ++i )
	{
		// Get the tag for this strikeBone (used to get the pBound from the ragdoll).
		const eAnimBoneTag strikingBoneTag = pStrikeBoneSet->GetStrikeBoneTag( i );

		// Get the ragdoll component number of this strikeBone (used to get the pBound from the ragdoll).
		const s32 nRagdollComponent = pPed->GetRagdollComponentFromBoneTag( strikingBoneTag );

		// Make sure to get the pBound for this strikeBone from the ragdoll.
		const phBound* pStrikeBoneBound = pRagdollBoundComp->GetBound( nRagdollComponent );
		if( !pStrikeBoneBound )
			continue;

		// Get the desired strike bone radius from the action table.
		const float fDesiredStrikeBoneRadius = pStrikeBoneSet->GetStrikeBoneRadius( i );

		// Make an approximation out of capsules for the volume swept though
		// space by this ragdoll pBound and if it is holding a weapon then the weapon too.
		for( u32 nBoundOrWeapon = 0; nBoundOrWeapon < 2; ++nBoundOrWeapon )
		{
			float fBoundRadius;
			Vector3 vBoundBBoxMin;
			Vector3 vBoundBBoxMax;
			Matrix34 matBoundStart;
			Matrix34 matBoundEnd;
			phMaterialMgr::Id boundMaterialId = phMaterialMgr::DEFAULT_MATERIAL_ID;

			// Check if we are trying to make an approximation for the bone pBound or
			// a held weapon pBound and get some necessary information about the pBound.
			if( nBoundOrWeapon == 0 )
			{
				// Get some information about the strike bone pBound.
				fBoundRadius = pStrikeBoneBound->GetRadiusAroundCentroid();
				vBoundBBoxMin = VEC3V_TO_VECTOR3( pStrikeBoneBound->GetBoundingBoxMin() );
				vBoundBBoxMax = VEC3V_TO_VECTOR3( pStrikeBoneBound->GetBoundingBoxMax() );
				matBoundStart = RCC_MATRIX34( pRagdollBoundComp->GetCurrentMatrix( nRagdollComponent ) );
				matBoundEnd = RCC_MATRIX34( pRagdollBoundComp->GetLastMatrix( nRagdollComponent ) );
				if( USE_GRIDS_ONLY(pStrikeBoneBound->GetType() != phBound::GRID &&) pStrikeBoneBound->GetType() != phBound::COMPOSITE )
					boundMaterialId = pStrikeBoneBound->GetMaterialId(0);				
			}
			else
			{
				// Check if this bone holds a weapon, if so we need to make an approximation
				// out of capsules for the volume swept though space by the weapons pBound.
				if( strikingBoneTag != BONETAG_R_HAND )
					break;

				// Get the weapon; note that pWeapon may be null if there is not a weapon in hand.
				const CObject* pHeldWeaponObject = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
				const CWeapon* pWeapon = pHeldWeaponObject ? pHeldWeaponObject->GetWeapon() : NULL;
				if( !pWeapon )
					break;

				// Get some information about the weapon pBound.
				fBoundRadius = pHeldWeaponObject->GetBoundRadius();  
				vBoundBBoxMin = pHeldWeaponObject->GetBoundingBoxMin();
				vBoundBBoxMax = pHeldWeaponObject->GetBoundingBoxMax();

				// get current weapon matrix, it's in world space, so transform into local ped space
				pHeldWeaponObject->GetMatrixCopy( matBoundStart );
				const Matrix34& matPed = RCC_MATRIX34(pPed->GetMatrixRef());
				matBoundStart.DotTranspose( matPed );

				if( m_bLastKnowWeaponMatCached )
					matBoundEnd = m_matLastKnownWeapon;
				else
					matBoundEnd = matBoundStart;

				// Cache off last known weapon matrix
				m_bLastKnowWeaponMatCached = true;
				m_matLastKnownWeapon = matBoundStart;

				// Set the material type.				
				const phInst* pInst = pHeldWeaponObject->GetFragInst();
				if( pInst == NULL ) 
					pInst = pHeldWeaponObject->GetCurrentPhysicsInst();

				if( pInst )
				{
					const phBound* pBound = pInst->GetArchetype()->GetBound();
					if( pBound USE_GRIDS_ONLY(&& pBound->GetType() != phBound::GRID) && pBound->GetType() != phBound::COMPOSITE )
						boundMaterialId = pBound->GetMaterialId(0);
				}
			}

			// Check if a single swept sphere approximation will be good enough
			// to approximate the pBound.
			if( fDesiredStrikeBoneRadius > fBoundRadius )
			{
				// Make a simple capsule for whole pBound based on the components animated start and
				// end positions and the radius given in the action table.
				if( nNumStrikeCapsules < BOUND_MAX_STRIKE_CAPSULES )
				{
					strikeCapsules[nNumStrikeCapsules].m_start = matBoundStart.d;
					strikeCapsules[nNumStrikeCapsules].m_end = matBoundEnd.d;
					strikeCapsules[nNumStrikeCapsules].m_radius = fDesiredStrikeBoneRadius;
					strikeCapsules[nNumStrikeCapsules].m_materialId	= boundMaterialId;
					nNumStrikeCapsules++;
				}
			}
			else
			{
				// We need to make a more accurate approximation.
				// Use multiple capsules with:
				//	their start points along the fLongestAxisLength of the longest axis of the pBound in it's start position and
				//	their end points along the fLongestAxisLength of the longest axis of the pBound in it's end position.

				// Determine which axis is the longest and get the two points
				// at the center of the cap faces for that axis.
				Vector3 vLongestAxisMin( VEC3_ZERO );
				Vector3 vLongestAxisMax( VEC3_ZERO );
				float fLongestAxisLength = 0.0f;
				
				// Determine which axis is longest.
				float xLength = Abs( vBoundBBoxMax.x - vBoundBBoxMin.x );
				float yLength = Abs( vBoundBBoxMax.y - vBoundBBoxMin.y );
				float zLength = Abs( vBoundBBoxMax.z - vBoundBBoxMin.z );
				if( (xLength > yLength) && (xLength > zLength) )
				{
					// The x Axis is longest.
					vLongestAxisMin.x = vBoundBBoxMin.x;
					vLongestAxisMax.x = vBoundBBoxMax.x;
					fLongestAxisLength = xLength;
				}
				else if( (yLength > xLength) && (yLength > zLength) )
				{
					// The y Axis is longest.
					vLongestAxisMin.y = vBoundBBoxMin.y;
					vLongestAxisMax.y = vBoundBBoxMax.y;
					fLongestAxisLength = yLength;
				}
				else
				{
					// Assume the z axis is longest (which is also good for cases like cubes, etc.).
					vLongestAxisMin.z = vBoundBBoxMin.z;
					vLongestAxisMax.z = vBoundBBoxMax.z;
					fLongestAxisLength = zLength;
				}

				// Hands need special handling since their bounds are generally too big (due
				// to accommodating the hand being open or closed).
				// We reduce the length of the pBound toward its start by about half to make
				// it smaller so that it represents a closed fist better.
				if(	( nBoundOrWeapon == 0 ) && ( ( strikingBoneTag == BONETAG_L_HAND ) || ( strikingBoneTag == BONETAG_R_HAND ) ) )
				{
					// Bring the vLongestAxisMax closer to vLongestAxisMin and
					// reduce the length.
					Vector3 vDirAlongLength( vLongestAxisMax - vLongestAxisMin );
					vDirAlongLength.Normalize();
					fLongestAxisLength -= BOUND_HAND_LENGTH_REDUCTION_AMOUNT;
					vLongestAxisMin = vLongestAxisMax - ( vDirAlongLength * fLongestAxisLength );
				}

				// Determine if one sphere will suffice for this approximation.
				if( fLongestAxisLength <= ( 2.0f * fDesiredStrikeBoneRadius ) )
				{
					// Make the one sphere into a capsule for this part of the
					// approximation of the moving pBound.
					if( nNumStrikeCapsules < BOUND_MAX_STRIKE_CAPSULES )
					{
						const Vector3 vPos = vLongestAxisMin + ( ( vLongestAxisMax - vLongestAxisMin ) * 0.5f );
						strikeCapsules[nNumStrikeCapsules].m_start = matBoundStart * vPos;
						strikeCapsules[nNumStrikeCapsules].m_end = matBoundEnd * vPos;
						strikeCapsules[nNumStrikeCapsules].m_radius = fDesiredStrikeBoneRadius;
						strikeCapsules[nNumStrikeCapsules].m_materialId	= boundMaterialId;
						nNumStrikeCapsules++;
					}// else don't make any more strike capsules.
				}
				else
				{
					// Make the multiple spheres we need into multiple capsules for
					// this part of the approximation of the moving pBound.

					// Make it so the caps of the approximation round off exactly at
					// the ends of the pBound.
					const float fLengthToUse = fLongestAxisLength - ( 2.0f * fDesiredStrikeBoneRadius );
					Vector3 vLongestAxisDir(vLongestAxisMax - vLongestAxisMin);
					vLongestAxisDir.Normalize();
					const Vector3 vCapsulesMiddle = ( vLongestAxisMin + vLongestAxisMax ) * 0.5f;
					const Vector3 vCapsulesDelta = vLongestAxisDir * fLengthToUse;
					const Vector3 vCapsulesStart = vCapsulesMiddle - ( vCapsulesDelta * 0.5f );
					//const Vector3 vCapsulesEnd = vCapsulesMiddle + ( vCapsulesDelta * 0.5f );

					// Determine how far apart we will allow the spheres in the
					// approximation to be.
					const float fMinRadiusFromAxis = fDesiredStrikeBoneRadius * (1.0f - BOUND_ACCEPTABLE_DEPTH_ERROR_PORTION);
					const float fMaxDistBetweenSphereCenters = 2.0f * rage::Sqrtf( rage::square( fDesiredStrikeBoneRadius ) - rage::square( fMinRadiusFromAxis ) );

					// Determine how many spheres we need to approximate this volume.
					u32 nNumSpheresForAproximation = u32( rage::Floorf( ( fLengthToUse / fMaxDistBetweenSphereCenters ) + 1.0f ) ) + 1;

					// Make the base points along the fLengthToUse for the approximation in local space then
					// transform them to determine the start and end positions of the capsules.
					nNumSpheresForAproximation = Min( nNumSpheresForAproximation, BOUND_MAX_NUM_SPHERES_ALONG_LENGTH );
					for( u32 i =0; i < nNumSpheresForAproximation; ++i )
					{
						const float fPortion = (float)i / (float)( nNumSpheresForAproximation - 1 );
						const Vector3 vPos = vCapsulesStart + ( vCapsulesDelta * fPortion );

						// Check if we should make a capsule for this part of the
						// approximation of the moving pBound.
						if( nNumStrikeCapsules >= BOUND_MAX_STRIKE_CAPSULES )
							break;	// Don't make any more strike capsules.
						else
						{
							// Make the capsule for this part of the approximation of
							// the moving pBound.
							strikeCapsules[nNumStrikeCapsules].m_start = matBoundStart * vPos;
							strikeCapsules[nNumStrikeCapsules].m_end = matBoundEnd * vPos;
							strikeCapsules[nNumStrikeCapsules].m_radius = fDesiredStrikeBoneRadius;
							strikeCapsules[nNumStrikeCapsules].m_materialId	= boundMaterialId;
							nNumStrikeCapsules++;
						}
					}
				}
			}
		}
	}

	// Make sure we have some strike capsules to make an approximate pBound from.
	if( nNumStrikeCapsules <= 0 )
		return NULL;

	// Build the approximate pBound from the list of capsules.
	if( !sm_pTestBound )
	{
		sm_pTestBound = rage_new phBoundComposite;
		sm_pTestBound->Init(BOUND_MAX_STRIKE_CAPSULES);
		for( unsigned int i = 0; i < BOUND_MAX_STRIKE_CAPSULES; ++i )
		{
			phBoundCapsule* pCapsule = rage_new phBoundCapsule;
			sm_pTestBound->SetBound( i, pCapsule );
			pCapsule->Release();
		}
	}

	sm_pTestBound->SetNumBounds( nNumStrikeCapsules );
	for( unsigned int i = 0; i < nNumStrikeCapsules; ++i )
	{
		phBoundCapsule* pCapsule = (phBoundCapsule*)sm_pTestBound->GetBound(i);

		// Set the material for the capsule.
		pCapsule->SetMaterial( strikeCapsules[i].m_materialId );

		// Set the capsule size/shape.
		const float fCapsuleRadius = strikeCapsules[i].m_radius;
		Vector3 vCapsule = strikeCapsules[i].m_end - strikeCapsules[i].m_start;
		pCapsule->SetCapsuleSize( fCapsuleRadius, vCapsule.Mag() );

		// Set the capsule position/orientation, but don't bother
		// orienting capsule if it's near zero length.
		Matrix34 matCapsule(Matrix34::IdentityType);
		matCapsule.d = strikeCapsules[i].m_start + 0.5f * vCapsule;
		if( vCapsule.Mag2() > 0.0001f )
		{
			vCapsule.Normalize();
			matCapsule.MakeRotateTo(YAXIS, vCapsule);
		}

		sm_pTestBound->SetCurrentMatrix( i, RCC_MAT34V( matCapsule ) );
		sm_pTestBound->SetLastMatrix( i, RCC_MAT34V( matCapsule ) );
	}

	sm_pTestBound->CalculateCompositeExtents();
	sm_pTestBound->UpdateBvh( true );
	return sm_pTestBound;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	TriggerHitReaction
// PURPOSE :	Process the effects of the ped striking something.
// PARAMETERS :	pPed - The ped causing the hit.
//				pHitEnt - The entity being struck.
//				pResult - Data about the collision.
//				bEntityAlreadyHit - Whether the hit entity has already been struck by the current action.
//				bElementAlreadyHit - Whether the hit entity's element has already been struck by the current action.
//				bIsBreakableObject - Whether the hit entity is a breakable
//				pTestBound - The pBound casing the hit.
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::TriggerHitReaction( CPed* pPed, CEntity* pHitEnt, sCollisionInfo& refResult, bool bEntityAlreadyHit, bool bBreakableObject )
{
#if __ASSERT
	bool bWasUsingRagdoll = pPed->GetUsingRagdoll();
#endif

	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::TriggerHitReaction." );
	Assertf( pHitEnt, "pHitEnt is null in CTaskMeleeActionResult::TriggerHitReaction." );
	Assertf( refResult.m_pResult->GetHitDetected(), "Invalid hit-point info in CTaskMeleeActionResult::TriggerHitReaction." );

	weaponDebugf3("CTaskMeleeActionResult::TriggerHitReaction pPed[%p][%s][%s] pHitEnt[%p][%s] bEntityAlreadyHit[%d] bBreakableObject[%d]",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",pHitEnt,pHitEnt ? pHitEnt->GetModelName() : "",bEntityAlreadyHit,bBreakableObject);

	if( !m_pActionResult )
		return;

	// Make sure there is an clip playing.
	if( !m_MoveNetworkHelper.GetClip( ms_ClipId ) )
		return;

	WorldProbe::CShapeTestResults tempResults(refResult.m_pResult, 1);
	tempResults.Update();

	// If we used a custom pBound generated from the motion of the bones / weapon
	// then we can use that information to get a normal based on the motion of the strike.
	Vector3 vImpactNormal = refResult.m_pResult->GetHitNormal();
	if( pHitEnt->GetIsTypePed() )
	{
		if( refResult.m_vImpactDirection.IsNonZero() )
		{
			// Get the direction the capsule was facing, and use that as the normal.
			vImpactNormal = refResult.m_vImpactDirection;

			// check we're not going to apply an force back towards the attacking ped
			static bank_float sfCosMaxImpactAngle = Cosf(70.0f * DtoR);
			Vector3 vPedForward( VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() ) );
			if( DotProduct( vPedForward, vImpactNormal ) < sfCosMaxImpactAngle )
				vImpactNormal = vPedForward;
		}
	}
	else if( vImpactNormal.Dot( VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() ) ) < 0.0f )
		vImpactNormal = -vImpactNormal;

	refResult.m_pResult->SetHitNormal( vImpactNormal );

	// DEBUG!! -AC, This should depend on if this is a rag-dolling ped we hit or not...
	// Make it only check if the entity has been hit- since the ragdolls are doing silly things any way...
	const bool bDoImpact = bBreakableObject || !bEntityAlreadyHit;

	// END DEBUG!!
	CPed* pTargetPed = NULL;
	if( refResult.m_pResult->GetHitDetected() && bDoImpact )
	{
		const CDamageAndReaction* pDamageAndReaction = m_pActionResult->GetDamageAndReaction();
		if( pDamageAndReaction )
		{
			// Default weapon to unarmed
			u32 uWeaponHash = pPed->GetWeaponManager() && !pDamageAndReaction->ShouldUseDefaultUnarmedWeapon() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : pPed->GetDefaultUnarmedWeaponHash();
			CWeapon tempWeapon( uWeaponHash );
			bool bCauseFatalMeleeDamage = false;

			float fStrikeDamage = pDamageAndReaction->GetOnHitDamageAmountMin();

			if (pPed->IsPlayer())
			{
				float fMaxMeleeDamage = pDamageAndReaction->GetOnHitDamageAmountMax();
				TUNE_GROUP_FLOAT(MELEE, fMaxMeleeOverrideModifier, 1.25f, -1.0f, 2.0f, 0.1f);
				if(fMaxMeleeOverrideModifier > 0.f) 
				{
					fMaxMeleeDamage = Max(fStrikeDamage, fStrikeDamage * fMaxMeleeOverrideModifier);
				}
				float fStrengthValue = Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STRENGTH.GetStatId())) / 100.0f, 0.0f, 1.0f);
				fStrikeDamage = Lerp(fStrengthValue, fStrikeDamage, fMaxMeleeDamage);
			}

			fStrikeDamage *= pPed->GetAttackStrength();

			if(tempWeapon.GetWeaponInfo())
			{
				// Apply any melee damage multipliers
				if (pHitEnt->GetIsTypePed())
				{
					CPed *pHitPed = static_cast<CPed*>(pHitEnt);

					if (NetworkInterface::IsInCopsAndCrooks() && pHitPed->IsPlayer())
					{
						fStrikeDamage = (float)GetMeleeEnduranceDamage();
					}

					if (tempWeapon.GetWeaponInfo()->GetIsMeleeFist() && tempWeapon.GetWeaponInfo()->GetMeleeRightFistTargetHealthDamageScaler() != -1 && pHitPed && !pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreMeleeFistWeaponDamageMult))
					{
						// Only apply multiplier if strike bone is right fist.
						const CStrikeBoneSet* pStrikeBoneSet = m_pActionResult ? m_pActionResult->GetStrikeBoneSet() : NULL;
						if (pStrikeBoneSet)
						{
							const u32 nStrikeBoneCount = pStrikeBoneSet->GetNumStrikeBones();
							for( u32 i = 0; i < nStrikeBoneCount; ++i )
							{
								const eAnimBoneTag strikingBoneTag = pStrikeBoneSet->GetStrikeBoneTag( i );
								if (strikingBoneTag == BONETAG_R_HAND)
								{
									float fScaledDamage = tempWeapon.GetWeaponInfo()->GetMeleeRightFistTargetHealthDamageScaler() * (pHitPed->GetMaxHealth() - pPed->GetDyingHealthThreshold());
									fStrikeDamage = fScaledDamage * sm_fMeleeRightFistMultiplier;
								}
							}
						}
					}

					if (tempWeapon.GetWeaponInfo()->GetMeleeDamageMultiplier() > 0.0f)
					{
						fStrikeDamage *= tempWeapon.GetWeaponInfo()->GetMeleeDamageMultiplier();
					}
				}
				
				// Apply the weapon damage modifier
				fStrikeDamage *= tempWeapon.GetWeaponInfo()->GetWeaponDamageModifier();
			}

			MeleeNetworkDamage *pNetworkDamage = NULL;

			bool bCanDamage = true;
			if(pHitEnt->GetIsTypePed())
			{
				if (NetworkInterface::IsGameInProgress() && !ShouldDamagePeds())
				{
					bCanDamage = false;
				}

				pTargetPed = static_cast<CPed*>(pHitEnt);

				//Increment successful counters
				if (pPed->IsLocalPlayer() && m_pActionResult->GetIsACounterAttack())
				{
					StatId stat = StatsInterface::GetStatsModelHashId("SUCCESSFUL_COUNTERS");
					if (StatsInterface::IsKeyValid(stat))
					{
						StatsInterface::IncrementStat(stat, 1.0f);
					}
				}

				// Factor in damage flags and friendliness
				bCanDamage &= !pTargetPed->m_nPhysicalFlags.bNotDamagedByAnything && 
							  ( !CActionManager::ArePedsFriendlyWithEachOther( pPed, pTargetPed ) ||
							    pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackFriendly ) );
				//@@: range CTASKMELEEACTIONRESULT_TRIGGERHITREACTION {
				#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_INVINCIBLECOP
					if(TamperActions::IsInvincibleCop(pTargetPed))
					{
						bCanDamage = false;
					}
				#endif
				//@@: } CTASKMELEEACTIONRESULT_TRIGGERHITREACTION

				// null out damage if we are attempting to hit a friendly ped.
				if( NetworkInterface::IsGameInProgress() && pPed->IsAPlayerPed() && pTargetPed->IsAPlayerPed() )
				{
					bool bRespawning = false;
					CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer *>(pTargetPed->GetNetworkObject());
					if( pNetObjPlayer && pNetObjPlayer->GetRespawnInvincibilityTimer() > 0) 
						bRespawning = true;

					// Factor in respawning
					bCanDamage &= !bRespawning;
				}

				if( bCanDamage )
				{		
					u16 iHitComponent = refResult.m_pResult->GetHitComponent();

					// Should we cause fatal damage?
					if( pDamageAndReaction->ShouldCauseFatalHeadShot() )
					{	
						eAnimBoneTag nHitBoneTag = pTargetPed->GetBoneTagFromRagdollComponent( iHitComponent );

						bool bBlockingCriticalHitsInMP = NetworkInterface::IsGameInProgress() && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NoCriticalHits); //url:bugstar:5771401
						if( nHitBoneTag == BONETAG_HEAD && !pTargetPed->m_nPhysicalFlags.bNotDamagedByMelee && !bBlockingCriticalHitsInMP
							NOTFINAL_ONLY( && !( pTargetPed->IsDebugInvincible() ) ) 
							)
						{
							bCauseFatalMeleeDamage = true;
						}
					}

					// Should we force fatal damage?
					if( pDamageAndReaction->ShouldCauseFatalShot() && 
						pTargetPed->GetPedHealthInfo() && pTargetPed->GetPedHealthInfo()->ShouldCheckForFatalMeleeCardinalAttack() &&
						!pTargetPed->PopTypeIsMission() && !pTargetPed->IsPlayer() )
					{
						bCauseFatalMeleeDamage = true;
					}

					// The iHitComponent here could have come from a low, medium or high lod ragdoll the ePhysicsLOD enum only really makes sense on 
					// iHitComponent from high lod 
					if ( pTargetPed && pTargetPed->GetRagdollInst() && pTargetPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH)
					{
						// Intentionally done after the check for causing fatal head shots.  Only want the reaction (and not the damage) to be altered by moving the hit location
						u16 iNewHitComponent = iHitComponent;
						if( pDamageAndReaction->ShouldMoveLowerArmAndHandHitsToClavicle() )
						{
							if( iHitComponent == RAGDOLL_LOWER_ARM_LEFT || iHitComponent == RAGDOLL_HAND_LEFT )
							{
								iNewHitComponent = RAGDOLL_CLAVICLE_LEFT;
							}
							else if( iHitComponent == RAGDOLL_LOWER_ARM_RIGHT || iHitComponent == RAGDOLL_HAND_RIGHT )
							{
								iNewHitComponent = RAGDOLL_CLAVICLE_RIGHT;
							}
						}
					
						if( pDamageAndReaction->ShouldMoveUpperArmAndClavicleHitsToHead() )
						{
							if( iHitComponent == RAGDOLL_CLAVICLE_LEFT || iHitComponent == RAGDOLL_CLAVICLE_RIGHT || iHitComponent == RAGDOLL_UPPER_ARM_LEFT || iHitComponent == RAGDOLL_UPPER_ARM_RIGHT )
							{
								iNewHitComponent = RAGDOLL_HEAD;
							}
						}

						if ( iNewHitComponent != iHitComponent )
						{
							refResult.m_pResult->SetHitComponent( iNewHitComponent );
							Matrix34 matNewComponent;
							if( pTargetPed->GetRagdollComponentMatrix( matNewComponent, iNewHitComponent ) )
							{
								refResult.m_pResult->SetHitPosition( matNewComponent.d );
							}
						}
					}
				}

				CTaskMeleeActionResult* pTaskMeleeActionResult = static_cast<CTaskMeleeActionResult*>(pTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MELEE_ACTION_RESULT));
				if( !pTargetPed->IsNetworkClone() && pTaskMeleeActionResult && pTaskMeleeActionResult->GetActionResult() && pTaskMeleeActionResult->GetActionResult()->ActivateRagdollOnCollision() )
				{
					CEventSwitch2NM event( 10000, rage_new CTaskNMRelax( 1000, 10000 ) );
					pTargetPed->SetActivateRagdollOnCollisionEvent( event );
					pTargetPed->SetActivateRagdollOnCollision( true );
				}

				if(GetPed()->IsNetworkClone())
					pNetworkDamage = FindMeleeNetworkDamageForHitPed(pTargetPed);
			}

			// Reset the strike damage when we cannot damage the entity
			if( !bCanDamage )
				fStrikeDamage = 0.0f;

			// Disable any forced fatal damage in Cops & Crooks
			if (NetworkInterface::IsInCopsAndCrooks())
			{
				bCauseFatalMeleeDamage = false;
			}

			// We might not want to do the same damage to objects and vehicles as we do to peds
			CWeaponDamage::sDamageModifiers damModifiers;
			// Scale damage caused to vehicles from melee attacks down a bit.
			damModifiers.fVehDamageMult = CWeaponDamage::MELEE_VEHICLE_DAMAGE_MODIFIER;
			
			// Use the normal weapon impact code.
			fwFlags32 flags( CPedDamageCalculator::DF_MeleeDamage );
			if( ShouldForceDamage() )
				flags.SetFlag( CPedDamageCalculator::DF_ForceMeleeDamage );

			if( ShouldIgnoreArmor() )
				flags.SetFlag( CPedDamageCalculator::DF_IgnoreArmor );

			if( ShouldIgnoreStatModifiers() )
				flags.SetFlag( CPedDamageCalculator::DF_IgnoreStatModifiers );

			if( bCauseFatalMeleeDamage )
				flags.SetFlag( CPedDamageCalculator::DF_FatalMeleeDamage );

			if( bEntityAlreadyHit )
				flags.SetFlag( CPedDamageCalculator::DF_SuppressImpactAudio );

			if (NetworkInterface::IsInCopsAndCrooks() && pHitEnt->GetIsTypePed())
			{
				// Melee attacks by Cops against Crooks are always non-lethal
				const CPed* pTargetPed = static_cast<const CPed*>(pHitEnt);
				if (ShouldApplyEnduranceDamageOnly(pPed, pTargetPed))
				{
					flags.SetFlag(CPedDamageCalculator::DF_EnduranceDamageOnly);
				}
			}

			CWeaponDamage::sMeleeInfo meleeInfo;
			meleeInfo.uParentMeleeActionResultID = m_nResultId;
			meleeInfo.uNetworkActionID = m_nUniqueNetworkActionID;
			meleeInfo.uForcedReactionDefinitionID = INVALID_ACTIONDEFINITION_ID;

			bool bDoDamage = false;
			if(GetPed()->IsNetworkClone())
			{
				if(pNetworkDamage)
				{
					flags = pNetworkDamage->m_uFlags;
					flags.SetFlag( CPedDamageCalculator::DF_AllowCloneMeleeDamage );
					fStrikeDamage = pNetworkDamage->m_fDamage;
					meleeInfo.uParentMeleeActionResultID = pNetworkDamage->m_uParentMeleeActionResultID;
					meleeInfo.uNetworkActionID = pNetworkDamage->m_uNetworkActionID;
					meleeInfo.uForcedReactionDefinitionID = pNetworkDamage->m_uForcedReactionDefinitionID;
					
					bDoDamage = true;

					//! NULL out if we apply damage.
					pNetworkDamage->Reset();
				}
			}
			else
			{
				bDoDamage = true;
			}

			Vector3 hitDirection = VEC3_ZERO;
			u32 nForcedReactionDefinitionID = INVALID_ACTIONDEFINITION_ID;
			if(bDoDamage)
			{
				CTaskMelee::ResetLastFoundActionDefinitionForNetworkDamage();

				CWeaponDamage::DoWeaponImpact( &tempWeapon, 
					pPed, 
					VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ), 
					tempResults, 
					fStrikeDamage, 
					flags, 
					false, 
					false, 
					&damModifiers, 
					&meleeInfo, false, 0, 1.0f, 1.0f, 1.0f, false, true, pHitEnt->GetIsTypeVehicle() ? &hitDirection: 0);

				const CActionDefinition* pMeleeActionToDo = CTaskMelee::GetLastFoundActionDefinitionForNetworkDamage();
				if(pMeleeActionToDo)
				{
					nForcedReactionDefinitionID = pMeleeActionToDo->GetID();
				}
			}

			static dev_bool bSendDamagePacket = true;

			if(NetworkInterface::IsGameInProgress() && !pPed->IsNetworkClone() && bSendDamagePacket && bDoDamage)
			{
				
				tempWeapon.SendFireMessage( pPed,
				VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ),
				tempResults,
				1, 
				true, 
				fStrikeDamage, 
				flags, 
				m_nResultId,
				m_nUniqueNetworkActionID,
				nForcedReactionDefinitionID, NULL, 1.f, 1.f, 1.f, &hitDirection);
			}
		}
	}

#if __ASSERT
	bool bUsingRagdoll = pPed->GetUsingRagdoll();
	if(pPed->IsNetworkClone() && bUsingRagdoll)
	{
		taskAssertf(bWasUsingRagdoll, "Clone has triggered ragdoll as part of simulated hit reaction. This shouldn't happen!");
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CheckForForcedReaction
// PURPOSE :	This will determine whether or not to force a reaction to the target ped
//				used for synchronized animations
// PARAMETERS :	pPed- Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::CheckForForcedReaction( CPed* pPed )
{
	// If we have a valid action, check to see if there are any immediate results to apply
	if( m_pActionResult )
	{
		// Try to get the parent ped if it is a ped.
		if( pPed )
		{
			// Determine if the action is an attack and should thus trigger some audio effects.
			if( m_pActionResult->GetIsAStandardAttack() )
			{
				const CStrikeBoneSet* pStrikeBoneSet = m_pActionResult->GetStrikeBoneSet();
				if( pStrikeBoneSet && pStrikeBoneSet->GetNumStrikeBones() > 0 && m_nSoundNameHash != 0)
				{
					pPed->GetPedAudioEntity()->PlayMeleeCombatSwipe(m_nSoundNameHash/*, pStrikeBoneSet->GetStrikeBoneTag(0) */);
					m_bHasTriggerMeleeSwipeAudio = true;
				}
			}

			CEntity* pTargetEntity = GetTargetEntity();
			CPed* pTargetPed = pTargetEntity && pTargetEntity->GetIsTypePed() ? static_cast<CPed*>(pTargetEntity) : NULL;
			if( pTargetPed && m_bForceImmediateReaction )
			{
				const CActionDefinition* pForcedReactionDefinition = NULL;
				if( GetPed()->IsNetworkClone() )
				{
					//! Don't flush tasks for a dead ped. This is bad as it can result in the dreaded "dead ped standing". If we need to play an animation whilst
					//! dead, we'll need to actively support it through dead dying task. And, at this stage, I don't think we need to.
					if( !pTargetPed->IsDead() && !pTargetPed->GetUsingRagdoll())
					{
						//! m_pForcedReactionActionDefinition is synced across network.
						pForcedReactionDefinition = m_pForcedReactionActionDefinition;
					}
				}
				else
				{
					// Check if by playing this actionResult we should force the target
					// into a specific actionResult.  If so try to force them to do it by
					// sending them a CEventGivePedTask right now.

					// Check to see if we can find an appropriate forced reaction that
					// is available for the target to do.
					CActionFlags::ActionTypeBitSetData typeToLookFor;
					typeToLookFor.Set( CActionFlags::AT_HIT_REACTION, true );

					pForcedReactionDefinition = ACTIONMGR.SelectSuitableAction(
						typeToLookFor,						// The type we want to search in.
						NULL,								// No Impulse combo
						false,								// Test whether or not it is an entry action.
						true,								// does not matter.
						pTargetPed,							// The hit ped
						m_pActionResult->GetID(),			// What result is invoking.
						m_pActionResult->GetPriority(),		// Priority
						pPed,								// the acting ped
						false,								// We don't have a lock on them.
						false,								// Should we only allow impulse initiated moves.
						pTargetPed->IsLocalPlayer(),		// If the target is a player then test the controls.
						CActionManager::ShouldDebugPed( pTargetPed ) );

					m_pForcedReactionActionDefinition = pForcedReactionDefinition;

					if( !pTargetPed->IsNetworkClone() )
					{
						Assertf( m_pForcedReactionActionDefinition, "No appropriate reaction could be found for forced reaction %s in CTaskMeleeActionResult::CTaskMeleeActionResult.", m_pActionResult->GetName() );
					}
				}

				// Check if we found an appropriate reaction, if not then do nothing.
				if( pForcedReactionDefinition )
				{
					// Try to get a pointer to the actionResult via the
					// actionResult Id for the reaction.
					const CActionResult* pReactionResult = m_pForcedReactionActionDefinition->GetActionResult( pPed );
				
					// Check if we found an appropriate reaction, if not then do nothing.
					if( pReactionResult )
					{
						s32 iMeleeFlags = CTaskMelee::MF_IsDoingReaction | CTaskMelee::MF_ForcedReaction;
						
						// Flush the target peds current task
						taskAssertf(!pTargetPed->IsDead(), "Shouldn't flush tasks for a dead ped. This will abort the dead dying task, which is bad! See DMKH");

						// Tell the ambient clips to match the blend rate. This is required otherwise there will be an animation pop
						if( CTaskAmbientClips* pAmbientClipTask = static_cast<CTaskAmbientClips *>( pTargetPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_AMBIENT_CLIPS ) ) )						
							pAmbientClipTask->SetCleanupBlendOutDelta( pReactionResult->GetAnimBlendInRate() );
						else if( CTaskComplexEvasiveStep* pEvasiveStepTask = static_cast<CTaskComplexEvasiveStep*>( pTargetPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_COMPLEX_EVASIVE_STEP ) ) )
							pEvasiveStepTask->SetBlendOutDeltaOverride( pReactionResult->GetAnimBlendInRate() );

						//! Try to stop ped moving from here on in.
						pTargetPed->SetVelocity( pTargetPed->GetReferenceFrameVelocity() );
						pTargetPed->StopAllMotion();
						pTargetPed->SetDesiredVelocity( pTargetPed->GetReferenceFrameVelocity() );
						pTargetPed->SetDesiredHeading( pTargetPed->GetCurrentHeading() );

						// Force update the clip to play immediately - saves a frame delay
						pTargetPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
						pTargetPed->SetPedResetFlag( CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, true );

						// try and make the clone ragdoll on death if possible (the master ped may not be able to and in this case a standing animated
						// death will happen)
						if (pTargetPed->IsNetworkClone())
						{
							pTargetPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceRagdollUponDeath, true);
						}

						MELEE_NETWORK_DEBUGF("[%d] CTaskMeleeActionResult::CheckForForcedReaction - Target = %s Previous State %d", fwTimer::GetFrameCount(), pTargetPed->GetDebugName(), GetPreviousState());

						// let the target ped know they are dying via stealth
						if( m_pActionResult->GetIsAStealthKill() )
						{
							pTargetPed->SetRagdollOnCollisionIgnorePhysical( pPed );

							if(!pTargetPed->IsNetworkClone())
							{
								pTargetPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth, true );
							}

							CEventShockingSeenPedKilled ev( *pPed, pTargetPed );

							// Make this event less noticeable based on the pedtype.
							float fKilledPerceptionModifier = pTargetPed->GetPedModelInfo()->GetKilledPerceptionRangeModifer();
							if (fKilledPerceptionModifier >= 0.0f)
							{
								ev.SetVisualReactionRangeOverride(fKilledPerceptionModifier);
								ev.SetAudioReactionRangeOverride(fKilledPerceptionModifier);
							}

							CShockingEventsManager::Add( ev );
						}
						else if( m_pActionResult->GetIsATakedown() )
						{
							// Takedowns can be unsuccessful
							const CDamageAndReaction* pDamageAndReaction = pReactionResult->GetDamageAndReaction();
							if( pDamageAndReaction && pDamageAndReaction->GetSelfDamageAmount() > 0 )
							{
								pTargetPed->SetRagdollOnCollisionIgnorePhysical( pPed );
								if(!pTargetPed->IsNetworkClone())
								{
									pTargetPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown, true );
								}

								CEventShockingSeenPedKilled ev( *pPed, pTargetPed );

								// Make this event less noticeable based on the pedtype.
								float fKilledPerceptionModifier = pTargetPed->GetPedModelInfo()->GetKilledPerceptionRangeModifer();
								if (fKilledPerceptionModifier >= 0.0f)
								{
									ev.SetVisualReactionRangeOverride(fKilledPerceptionModifier);
									ev.SetAudioReactionRangeOverride(fKilledPerceptionModifier);
								}

								CShockingEventsManager::Add( ev );
							}
						}

						CTaskMelee* pReactionTask = rage_new CTaskMelee( pReactionResult, pPed, iMeleeFlags );
						if( pReactionTask && pTargetPed->GetPedIntelligence()->GetTaskActive()->GetTaskType() != CTaskTypes::TASK_INCAPACITATED) // CTaskIncapacitated will not abort for this on the owner so prevent it running on the clone as well
						{
							Assert( pTargetPed->GetPedIntelligence() );
							//! kick off a local task, this keeps animations in sync throughout.
							if( pTargetPed->IsNetworkClone() )
								pTargetPed->GetPedIntelligence()->AddLocalCloneTask(pReactionTask, PED_TASK_PRIORITY_PHYSICAL_RESPONSE);
							else
							{
								CEventGivePedTask forcedReactionEvent( PED_TASK_PRIORITY_PHYSICAL_RESPONSE, pReactionTask, false, E_PRIORITY_DAMAGE_PAIRED_MOVE );
								pTargetPed->GetPedIntelligence()->AddEvent( forcedReactionEvent );
							}
						}
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CanCloneQuitForcedReaction
// PURPOSE :	Check that clone ped ran/is running forced reaction melee task.
// PARAMETERS :	pTargetPed - Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::CanCloneQuitForcedReaction(CPed *pTargetPed) const
{
	taskAssert(pTargetPed && pTargetPed->IsNetworkClone());

	bool bKillFlagSet = pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth) || pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown);

	bool bIsUsingNm = pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_NM_CONTROL );

	bool bIsIncapacitated = pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_INCAPACITATED );

	if( (bIsUsingNm || pTargetPed->IsDead() || bIsIncapacitated) && !bKillFlagSet)
	{
		return true;
	}

	return false;
};

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ComputeInitialAnimPhase
// PURPOSE :	Helper function used to determine what the best initial phase will
//				be given the clip distance tags.
// PARAMETERS :	pPed - Ped we are interested in
//				pClip - Anim clip we are about to play
/////////////////////////////////////////////////////////////////////////////////
float CTaskMeleeActionResult::ComputeInitialAnimPhase( const crClip* pClip )
{
	float fInitialPhase = 0.0f;
	const CEntity* pTargetEntity = GetTargetEntity();
	if( pClip && pClip->HasTags() && pTargetEntity )
	{		
		CPed* pPed = GetPed();
		Vector3 vPedToTarget = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() - pTargetEntity->GetTransform().GetPosition() );
		float fDistToTarget = vPedToTarget.Mag();

		// Create an iterator to find the tag with the closest distance to our actual distance from cover
		float fBestDist = -1.0f;
		float fBestPhase = -1.0f;
		float fLastKnownBestDist = 10000.0f;
		float fLastKnownBestPhase = -1.0f;
		CClipEventTags::CTagIteratorAttribute<CClipEventTags::CDistanceMarkerTag, crPropertyAttributeFloat> it( *pClip->GetTags(), CClipEventTags::DistanceMarker, CClipEventTags::Distance );
		while( *it )
		{
			const float fTagDistance = (*it)->GetData()->GetDistance();
			if( fTagDistance <= fDistToTarget )
			{
				if( fBestDist < fTagDistance )
				{
					fBestDist = fTagDistance;
					fBestPhase = (*it)->GetMid();
				}
			}
			else if( fTagDistance <= fLastKnownBestDist  )
			{
				fLastKnownBestDist = fTagDistance;
				fLastKnownBestPhase = (*it)->GetMid();
			}
			++it;
		}

		// Check if we have a better phase to use
		if( fBestPhase != -1.0f )
		{
			// Should we lerp between the second best phase and best phase?
			if( fLastKnownBestPhase != -1.0f )
				fInitialPhase = Lerp( Abs( fLastKnownBestDist - fDistToTarget ) / Abs( fLastKnownBestDist - fBestDist ), fBestPhase, fLastKnownBestPhase );
			else 
				fInitialPhase = fBestPhase;
		}
		else if( fLastKnownBestPhase != -1.0f )
			fInitialPhase = fLastKnownBestPhase;

#if __DEV
		TUNE_GROUP_BOOL( MELEE, DISPLAY_DISTANCE_TAG_DEBUG, false );
		if( DISPLAY_DISTANCE_TAG_DEBUG )
		{
			Displayf("============================================");
			Displayf("Dist To Target<%.2f>", fDistToTarget );
			if( fBestPhase != -1.0f && fLastKnownBestPhase != -1.0f )
			{
				Displayf( "Dist To Prev Phase<%.2f> / Total Dist<%.2f> = <%.2f>", Abs( fLastKnownBestDist - fDistToTarget ), Abs( fBestDist - fLastKnownBestDist ), Abs( fLastKnownBestDist - fDistToTarget ) / Abs( fBestDist - fLastKnownBestDist ) );
				Displayf( "Lerp<%.2f, %.2f> = Initial Phase<%.2f>", fBestPhase, fLastKnownBestPhase, fInitialPhase );
			}
			else
				Displayf("Initial Phase<%.2f>", fInitialPhase );
			Displayf("Best Dist<%.2f>", fBestDist );
			Displayf("============================================");
		}
#endif // __DEV
	
	}

	return fInitialPhase;
}

CHoming * CTaskMeleeActionResult::GetHoming(const CPed *pPed)
{
	if(m_pActionResult)
	{
		// Homing doesn't support combat on moving platform, disabled it for now (B*1898494)
		if(pPed && pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle())
		{
			if(pPed->GetReferenceFrameVelocity().IsNonZero())
			{
				return NULL;
			}
		}
		return m_pActionResult->GetHoming();
	}
	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DropWeaponAndCreatePickup
// PURPOSE :	Drop weapon and create pickup 
// PARAMETERS :	pPed- Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::DropWeaponAndCreatePickup(CPed* pPed)
{
	CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	if(pWeaponMgr)
	{
		bool bCanCreatePickupIfUnarmed = CPickupManager::CanCreatePickupIfUnarmed(pPed);
		const CWeaponInfo* pWeaponInfo = pWeaponMgr->GetEquippedWeaponInfo();
		if(pWeaponInfo)
		{
			const bool bHasPickup = CPickupManager::ShouldUseMPPickups(pPed) ? (pWeaponInfo->GetMPPickupHash() != 0) : (pWeaponInfo->GetPickupHash() != 0);
			CPickup* pPickup = bHasPickup ? CPickupManager::CreatePickUpFromCurrentWeapon(pPed) : NULL;

			if (pPickup)
			{
				if (pPed->IsLocalPlayer() && pPickup->GetNetworkObject())
				{
					CObject* pPlayerWeaponObject = NULL;

					if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
					{
						pPlayerWeaponObject = pWeaponMgr->DropWeapon(pWeaponMgr->GetEquippedWeaponHash(), false, false);
					}

					// pass the weapon and pickup over to the player's network object. The pickup will remain hidden while the player is dead, and the weapon will be
					// deleted once he respawns
					NetworkInterface::SetLocalDeadPlayerWeaponPickupAndObject(pPickup, pPlayerWeaponObject);
				}
				else
				{
					pWeaponMgr->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash(), -1, true);
				}
			}
			else
			{
				// Drop the object
				if(!pWeaponInfo->GetIsUnarmed())
				{
					pWeaponMgr->DropWeapon(pWeaponMgr->GetEquippedWeaponHash(), false);
				}
				else if(bCanCreatePickupIfUnarmed)
				{
					CPickupManager::CreatePickUpFromCurrentWeapon(pPed, false, true);
				}
			}
		}
		else if(bCanCreatePickupIfUnarmed)
		{
			CPickupManager::CreatePickUpFromCurrentWeapon(pPed, false, true);
		}
	}

	if(!pPed->IsLocalPlayer())
	{
		if(pPed->GetInventory())
		{
			pPed->GetInventory()->RemoveAllWeaponsExcept( pPed->GetDefaultUnarmedWeaponHash() );
		}
	}
	else
	{
		//! Don't let player's dead task create another pickup (which can happen as we don't clear inventory).
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead, true);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ShouldApplyEnduranceDamageOnly
/////////////////////////////////////////////////////////////////////////////////

bool CTaskMeleeActionResult::ShouldApplyEnduranceDamageOnly(const CPed* pAttackingPed, const CPed* pVictimPed) const
{
	if (NetworkInterface::IsInCopsAndCrooks() && pVictimPed && pAttackingPed)
	{
		bool attackerIsCop =  pAttackingPed->IsPlayer() && pAttackingPed->GetPlayerInfo()->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP;
		bool victimIsCrook = pVictimPed->IsPlayer() && pVictimPed->GetPlayerInfo()->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_CROOK;
		if (attackerIsCop && victimIsCrook)
		{
			return true;
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Start_OnEnter
// PURPOSE :	CTaskMeleeActionResult entry point for the CTask state machine
// PARAMETERS :	pPed - Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::Start_OnEnter( CPed* pPed )
{
	if(GetPed()->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF("[%d] CTaskMeleeActionResult::Start_OnEnter - Player = %s Action %p Previous State %d", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), m_pActionResult, GetPreviousState());
	}

	// Set up/cache action result.
	CacheActionResult( pPed );

	// This will determine whether or not to force a reaction to the target ped
	// used for synchronized animations
	CheckForForcedReaction( pPed );

	CEntity* pTargetEntity = GetTargetEntity();
	if( pTargetEntity )
	{
		if( pTargetEntity->GetIsTypePed() )
		{
			CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);
			m_vCachedHeadIkOffset = pTargetPed->GetBonePositionCachedV( BONETAG_HEAD ) - pPed->GetTransform().GetPosition();
		}
	}

	//! Create ID for this melee attack. Note: This needs to be unique across all machines, so we OR in player index to a static counter.
	if(m_nUniqueNetworkActionID == 0 && NetworkInterface::IsGameInProgress() && NetworkInterface::GetLocalPlayer())
	{
		++CClonedTaskMeleeResultInfo::m_nActionIDCounter;

		//! 1st 8 bits reserved for player ID. Last 8 bits reserved for counter. Note: I could probably infer player index from the
		//! ped ID on remote side.
		m_nUniqueNetworkActionID = NetworkInterface::GetLocalPhysicalPlayerIndex() << 8;
		m_nUniqueNetworkActionID |= CClonedTaskMeleeResultInfo::m_nActionIDCounter;

		MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::Start_OnEnter - Assigning unique ID = %d Address %p", fwTimer::GetFrameCount(), m_nUniqueNetworkActionID, this );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Start_OnUpdate
// PURPOSE :	Decides whether or not we plan to play an animation
// PARAMETERS :	pPed - Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeActionResult::Start_OnUpdate( CPed* pPed )
{
	if(GetPed()->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF("[%d] CTaskMeleeActionResult::Start_OnUpdate - Player = %s Action %p", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), m_pActionResult);
	}

	//! If we are a clone and are locally ragdolling, see if we can fixup. If not, just bail.
	if(pPed->GetUsingRagdoll())
	{
		bool bCanSwitchToAnimated = false; 

		if(pPed->IsNetworkClone())
		{
			//! Note: We fixup if this is an attack anim, or if ped is currently being killed.
			if(m_pActionResult)
			{
				bCanSwitchToAnimated = m_pActionResult->GetIsATakedown() || 
					m_pActionResult->GetIsAStealthKill() || 
					m_pActionResult->GetIsAKnockout() || 
					m_pActionResult->GetIsAStandardAttack() || 
					pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) ||
					pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown);
			}
		}

		if(bCanSwitchToAnimated)
		{
			pPed->SwitchToAnimated();
		}
		else
		{
			taskDebugf1("Quit: CTaskMeleeActionResult::Start_OnUpdate: !bCanSwitchToAnimated");
			SetState( State_Finish );
			return FSM_Continue;
		}
	}

	// If there is a valid action result, branch to either playing an clip or 
	// not depending on that action
	if( m_bPlaysClip )
	{
		fwClipSet* pSet = fwClipSetManager::GetClipSet( m_clipSetId );
		if( pSet )
		{
			pSet->Request_DEPRECATED();
		}

		const crClip* pClip = fwClipSetManager::GetClip( m_clipSetId, m_clipId );

		//! Remove Post V.
		//! HACK. url:bugstar:1598543. Can't patch data or anims, so hard code fallback clipset ID here.
		static const fwMvClipId MISSING_CLIP_1("HIT_COUNTER_ATTACK_L",0x5d6d28ef);
		static const fwMvClipId MISSING_CLIP_2("HIT_COUNTER_ATTACK_R",0x0d0f8831);
		if(!pClip && ((m_clipId.GetHash() == MISSING_CLIP_1) || (m_clipId.GetHash() == MISSING_CLIP_2)) )
		{
			static const fwMvClipSetId CLIP_SET_MELEE_FALLBACK("MELEE@UNARMED@STREAMED_CORE",0xfd5a244a);
			m_clipSetId = CLIP_SET_MELEE_FALLBACK;
			fwClipSet* pSet = fwClipSetManager::GetClipSet( m_clipSetId );
			if( pSet && pSet->Request_DEPRECATED() )
			{
				pClip = fwClipSetManager::GetClip( m_clipSetId, m_clipId );
			}
		}
		//! Remove Post V

		//! In network games, we may need to play a reaction for a weapon type that we haven't streamed in yet, so don't assert
		//! as we allow a small amount of time to do so.
		if(!NetworkInterface::IsNetworkOpen() && !pPed->IsNetworkClone())
		{
			if(!pClip)
			{
				taskAssertf( 0, "Failed to find clip %s in clip set %s", m_clipId.GetCStr(), m_clipSetId.GetCStr() );
				
				//! Bail out if we couldn't find clip - don't want to get stuck.
				taskDebugf1("Quit: CTaskMeleeActionResult::Start_OnUpdate: !pClip");
				SetState( State_Finish );
			}
		}

		if(NetworkInterface::IsGameInProgress() && m_pActionResult && !pClip)
		{
			bool bExecution = m_pActionResult->GetIsAStealthKill() || 
				m_pActionResult->GetIsATakedown() || 
				m_pActionResult->GetIsAKnockout() ||
				pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) ||
				pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown);

			static dev_float s_fTimeOutExecution = 2.0f;
			static dev_float s_fTimeOutOffensive = 0.5f;
			static dev_float s_fTimeOut = 0.25f;

			float fTimeOut;
			if(bExecution)
			{	
				fTimeOut = s_fTimeOutExecution;
			}
			else if(IsOffensiveMove())
			{
				fTimeOut = s_fTimeOutOffensive;
			}
			else
			{
				fTimeOut = s_fTimeOut;
			}

			if(GetTimeInState() > fTimeOut)
			{
				// Couldn't stream execution anim in time, or its something we aren't too bothered about, so 
				// just bail. This should be pretty rare.
				Displayf("CTaskMeleeActionResult::Start_OnUpdate - failed to stream clip %s in clip set %s in time!", m_clipId.TryGetCStr(), m_clipSetId.TryGetCStr());
				taskDebugf1("Quit: CTaskMeleeActionResult::Start_OnUpdate: GetTimeInState() (%.2f) > fTimeOut (%.2f)", GetTimeInState(), fTimeOut);
				SetState( State_Finish );
			}
		}
		else if( pClip && PrepareMoveNetwork( pPed ))
		{
			m_MoveNetworkHelper.SetClipSet( m_clipSetId );
			m_MoveNetworkHelper.SetClip( pClip, ms_ClipId );

			if( IsUpperbodyOnly() )
			{
				static const atHashWithStringNotFinal clipFilterId( "BONEMASK_UPPERONLY",0xD89921AE );
				if( !m_pClipFilter )
					m_pClipFilter = CGtaAnimManager::FindFrameFilter( clipFilterId.GetHash(), pPed );

				if( m_pClipFilter )
					m_pClipFilter->AddRef();

				m_MoveNetworkHelper.SetFilter( m_pClipFilter, ms_ClipFilterId );
				m_MoveNetworkHelper.SetFloat( ms_IkShadowWeightId, 1.0f );
			}
			else
			{
				m_MoveNetworkHelper.SetFilter( NULL, ms_ClipFilterId );
				m_MoveNetworkHelper.SetFloat( ms_IkShadowWeightId, 0.0f );
			}

			// Based on the ideal distance attempt to find a phase tag to enter this attack
			m_fInitialPhase = ComputeInitialAnimPhase( pClip );
			m_MoveNetworkHelper.SetFloat( ms_ClipPhaseId, m_fInitialPhase );

			// For optimization reasons we precompute the mover displacement here instead of doing every frame
			if( CClipEventTags::FindEventPhase( pClip, CClipEventTags::CriticalFrame, m_fCriticalFrame ) )
			{
				m_vMoverDisplacement = VECTOR3_TO_VEC3V( fwAnimHelpers::GetMoverTrackDisplacement( *pClip, 0.f, m_fCriticalFrame ) );
				m_bPastCriticalFrame = false;
			}
			else
				m_vMoverDisplacement = VECTOR3_TO_VEC3V( fwAnimHelpers::GetMoverTrackDisplacement( *pClip, 0.f, 1.0f ) );

			bool bUseAdditiveClip = false;
			if( m_pActionResult && m_pActionResult->AllowAdditiveAnims() )
			{
				const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
				if( pWeaponInfo && pWeaponInfo->GetMeleeClipSetId(*pPed) != CLIP_SET_ID_INVALID )
				{
					fwMvClipId additiveClipId("additive_damage_idle_fix",0xEE85260C);
					const crClip* pAdditiveClip = fwClipSetManager::GetClip( pWeaponInfo->GetMeleeClipSetId(*pPed), additiveClipId );
					if( pAdditiveClip != NULL )
					{
						bUseAdditiveClip = true;
						m_MoveNetworkHelper.SetClip( pAdditiveClip, ms_AdditiveClipId );
					}
				}
			}

			bool bUseGrip = false;
			if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponInfo() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsMeleeFist())
			{
				fwMvClipSetId gripClipSetId = pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetMeleeClipSetId(*pPed);
				if (gripClipSetId != CLIP_SET_ID_INVALID)
				{
					fwMvClipId gripClipId("GRIP_IDLE", 0x3ec63b58);
					const crClip* pGripClip = fwClipSetManager::GetClip( gripClipSetId, gripClipId );
					if (pGripClip != NULL)
					{
						bUseGrip = true;
						m_MoveNetworkHelper.SetClipSet(gripClipSetId, ms_GripClipSetId);

						crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(gripClipSetId);
						if (pFilter)
						{
							m_MoveNetworkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
						}
					}
				}
			}

			m_MoveNetworkHelper.SetFlag( bUseAdditiveClip, ms_UseAdditiveClipId );
			m_MoveNetworkHelper.SetFlag( bUseGrip, ms_HasGrip );

			SetState( State_PlayClip );
			return FSM_Continue;
		}
	}
	else
	{
		// Nothing to do
		taskDebugf1("Quit: CTaskMeleeActionResult::Start_OnUpdate: !m_bPlaysClip");
		SetState( State_Finish );
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PlayClip_OnEnter
// PURPOSE :	Setup to play the designated CTaskMeleeActionResult animation
// PARAMETERS :	pPed - Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::PlayClip_OnEnter( CPed* pPed )
{
#if __ASSERT
	if(pPed->IsNetworkClone())
	{
		MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::PlayClip_OnEnter [%p] - sequence ID = %onedd", CNetwork::GetNetworkTime(), this, GetNetSequenceID() );
	}
#endif

	taskFatalAssertf( m_pActionResult, "Valid action result expected at this point!" );

	taskAssertf( m_MoveNetworkHelper.IsNetworkActive(), "Melee Move network isn't attached. Prev State: %s (%f)",  GetStaticStateName(GetPreviousState()), GetTimeRunning()); 

	bool bLockingOn = false;
	if(pPed->GetPlayerInfo())
	{
		CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();
		bLockingOn = rTargeting.GetLockOnTarget() != NULL;
	}

	if(!bLockingOn && !pPed->IsNetworkClone())
	{
		float fDesiredHeading = 0.0f;
		if( m_pActionResult->ShouldUseCachedDesiredHeading() )
			fDesiredHeading = pPed->GetDesiredHeading();
		else
			fDesiredHeading = CActionManager::CalculateDesiredHeading( pPed );

		pPed->SetDesiredHeading(fDesiredHeading);
	}

	
	// Turn on leg ik
	if( !pPed->GetIKSettings().IsIKFlagDisabled( IKManagerSolverTypes::ikSolverTypeLeg ) && ( pPed->IsPlayer() || m_pActionResult->ShouldUseLegIk() ) )
	{
		pPed->m_PedConfigFlags.SetPedLegIkMode( LEG_IK_MODE_FULL_MELEE );
	}

	// Force update the clip to play immediately - saves a frame delay
 	pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );

	CEntity* pTargetEntity = GetTargetEntity();
	CHoming* pHoming = GetHoming(pPed);
	if( pHoming )
	{
		InitHoming(pPed, pTargetEntity);

		//! Clones need to do a test from where they are from, to the target position.
		if(pPed->IsNetworkClone() && pTargetEntity && m_pActionResult)
		{
			static dev_float SPHERE_RADIUS		= 0.2f;
			static dev_float TEST_Z_OFFSET		= 0.0f;

			Vector3 vTestPos(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
			vTestPos.z += TEST_Z_OFFSET;
			Vector3 vTargetPos = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetPosition());
			vTargetPos.z += TEST_Z_OFFSET;

			const u32			TEST_FLAGS		= ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE;

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetCapsule(vTestPos, vTargetPos, SPHERE_RADIUS);
			capsuleDesc.SetIncludeFlags(TEST_FLAGS);

			bool bKillMove = m_pActionResult->GetIsATakedown() || m_pActionResult->GetIsAStealthKill() || m_pActionResult->GetIsAKnockout();

			static dev_bool s_TryToWarpToOwnerPos = false;
			bool bWarpToOwnerPos = false;
			if(s_TryToWarpToOwnerPos)
			{
				if(!vTestPos.IsClose(NetworkInterface::GetLastPosReceivedOverNetwork(pPed), 0.1f))
				{
					if((bKillMove && WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc)))
					{
						WorldProbe::CShapeTestCapsuleDesc capsuleDesc2;
						capsuleDesc2.SetCapsule(NetworkInterface::GetLastPosReceivedOverNetwork(pPed), vTargetPos, SPHERE_RADIUS);
						capsuleDesc2.SetIncludeFlags(TEST_FLAGS);

						//! Check actual clone 'master' position is clear. If so, warp ped there and home in from that location.
						if(!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc2))
						{
							bWarpToOwnerPos = true;
						}
					}
				}
			}

			static dev_bool s_bForceWarp = false;
			if(s_bForceWarp || bWarpToOwnerPos)
			{
				//! Go to correct location.
				NetworkInterface::GoStraightToTargetPosition(pPed);

				Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

				Vector3 vDirection = vTargetPos - vPedPos;
				vDirection.NormalizeSafe();

				float fOwnerPedHeading = rage::Atan2f(-vDirection.x, vDirection.y);
				pPed->SetHeading(fOwnerPedHeading);

				//! Init homing against using new position.
				InitHoming(pPed, pTargetEntity);
			}
		}
	}
	else
	{
		StopDistanceHoming();
	}

	// Apply any special results that are defined
	ProcessPreHitResults( pPed, pTargetEntity );
}

void CTaskMeleeActionResult::InitHoming(CPed *pPed, CEntity* pTargetEntity)
{
	CHoming* pHoming = m_pActionResult->GetHoming();

	if( pTargetEntity )
	{
		int rnBoneCacheIdx = -1;
		if( !CActionManager::TargetTypeGetPosDirFromEntity( m_vCachedHomingPosition,
			m_vCachedHomingDirection,
			pHoming->GetTargetType(),
			pTargetEntity,
			pPed,
			pPed->GetTransform().GetPosition(),
			0.0f,
			rnBoneCacheIdx ) )
		{
			m_vCachedHomingPosition = pTargetEntity->GetTransform().GetPosition();
			m_vCachedHomingDirection = m_vCachedHomingPosition - pPed->GetTransform().GetPosition();
		}

		// Store the initial ped position
		m_vCachedHomingPedPosition = pPed->GetTransform().GetPosition();

		if( !pPed->IsNetworkClone() && pHoming->ShouldUseActivationDistance() )
		{
			Vec3V vPedToTarget = m_vCachedHomingPosition - pPed->GetTransform().GetPosition();
			float fDistToTarget = Mag( vPedToTarget ).Getf();
			if( fDistToTarget > pHoming->GetActivationDistance() )
			{
				//taskAssertf(0, "Stopped homing immediately - why did we pick this action!?");
				StopDistanceHoming();
			}
		}
	}
	else if( pHoming->IsSurfaceProbe() )
	{
		// Super special case
		float fRootOffsetZ = pHoming->ShouldUseLowRootZOffset() ? CActionManager::ms_fLowSurfaceRootOffsetZ : 0.0f;
		int rnBoneCacheIdx = -1;
		if( !CActionManager::TargetTypeGetPosDirFromEntity( m_vCachedHomingPosition,
			m_vCachedHomingDirection,
			pHoming->GetTargetType(),
			NULL,
			pPed,
			pPed->GetTransform().GetPosition(),
			0.0f,
			rnBoneCacheIdx, 
			pHoming->GetActivationDistance(),
			fRootOffsetZ ) )
		{
			// If we couldn't find anything they just let the move play out normally
			StopDistanceHoming();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PlayClip_OnUpdate
// PURPOSE :	Main update loop for a CTaskMeleeActionResult animation. In charge
//				of monitoring clip events and when to switch to ragdoll.
// PARAMETERS :	pPed - Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeActionResult::PlayClip_OnUpdate( CPed* pPed )
{
	taskAssert( m_MoveNetworkHelper.IsNetworkActive() );
	if( !m_MoveNetworkHelper.IsInTargetState() )
		return FSM_Continue;

	/*static dev_float s_fDamageTime = 0.2f;
	if(GetTimeInState() > s_fDamageTime && m_bOffensiveMove && pPed->IsLocalPlayer())
	{
		u32 nWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : pPed->GetDefaultUnarmedWeaponHash();

		CWeaponDamage::GeneratePedDamageEvent(m_pTargetEntity, 
			pPed, 
			nWeaponHash, 
			5000.0f, 
			VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ), 
			NULL, 
			CPedDamageCalculator::DF_MeleeDamage | CPedDamageCalculator::DF_ForceMeleeDamage | CPedDamageCalculator::DF_FatalMeleeDamage, 
			NULL, 
			-1, 
			0.0f, 
			m_nResultId, 
			false);
	}*/

	//! url:bugstar:1591188. If our target ped has gone into ragdoll, and hasn't been killed by melee, then abort. Indicates something else 
	//! has triggered ragdoll on them. (e.g. being run over by car).
	CEntity* pTargetEntity = GetTargetEntity();
	CPed* pTargetPed = pTargetEntity && pTargetEntity->GetIsTypePed() ? static_cast<CPed*>(pTargetEntity) : NULL;
	if( pTargetPed && pTargetPed->IsNetworkClone() && m_bForceImmediateReaction )
	{
		CTaskMeleeActionResult* pSimpleMeleeActionResultTask = static_cast<CTaskMeleeActionResult*>(pTargetPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT ) );
		if( !pSimpleMeleeActionResultTask && CanCloneQuitForcedReaction(pTargetPed))
		{
			MELEE_NETWORK_DEBUGF("Quit: CTaskMeleeActionResult::PlayClip_OnUpdate: !pSimpleMeleeActionResultTask && CanCloneQuitForcedReaction(pTargetPed)");
			return FSM_Quit;
		}
	}

	CTask* pParentTask = GetParent();
	CTaskMelee* pParentMeleeTask = pParentTask ? static_cast<CTaskMelee*>( GetParent() ) : NULL;

	bool bForcedReaction = pParentMeleeTask && pParentMeleeTask->GetIsTaskFlagSet( CTaskMelee::MF_ForcedReaction );

	// B*1724650 - Quit the melee task if the equipped weapon info is different to the equipped weapon info that the ped is actually using 
	// as the damage being applied will be incorrect
	if(pPed->IsLocalPlayer() && IsOffensiveMove() && !bForcedReaction )
	{
		const CWeapon* pEquippedWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		u32 uEquippedWeaponHash = pEquippedWeapon ? pEquippedWeapon->GetWeaponHash() : -1;
		u32 uSelectedWeaponHash =  pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetSelectedWeaponHash() : -1;
		if (uSelectedWeaponHash != uEquippedWeaponHash)
		{
			MELEE_NETWORK_DEBUGF("Quit: CTaskMeleeActionResult::PlayClip_OnUpdate: uSelectedWeaponHash (%d) != uEquippedWeaponHash (%d)", uSelectedWeaponHash, uEquippedWeaponHash);
			return FSM_Quit;
		}
	}

	bool bQuitEarly = false;
	if( !pPed->IsNetworkClone() && (!pPed->IsPlayer() || ShouldAllowPlayerToEarlyOut()) )
		bQuitEarly = ShouldAllowInterrupt() && pParentMeleeTask && pParentMeleeTask->GetIsTaskFlagSet( CTaskMelee::MF_QuitTaskAfterNextAction );
	if (pPed->GetUsingRagdoll() && pPed->IsNetworkClone())
		bQuitEarly = true;
	if (!m_bPastCriticalFrame && pTargetPed && pPed->IsNetworkClone() && pTargetPed->GetIsDeadOrDying() && !IsOffensiveMove())
	{
		// quit out of the melee reaction if the attacking ped has died before striking the local ped
		m_bSelfDamageApplied = true;
		bQuitEarly = true;
	}

	if( m_MoveNetworkHelper.GetBoolean( ms_AnimClipFinishedId ) || bQuitEarly )
	{
		//! If this is a network kill move, then don't let local ped exit clip until told to by the owner (or we migrate). This prevents the clone ped
		//! standing back up when dead.
		if(pPed->IsNetworkClone() && bForcedReaction)
		{
			m_bCloneKillAnimFinished = true;
			m_nCloneKillAnimFinishedTime = fwTimer::GetTimeInMilliseconds();
		}
		else
		{
			if( pParentMeleeTask && pParentMeleeTask->GetIsTaskFlagSet( CTaskMelee::MF_ShouldStartGunTaskAfterNextAction ) )
			{
				taskDebugf1("Quit: CTaskMeleeActionResult::PlayClip_OnUpdate: m_MoveNetworkHelper.GetBoolean( ms_AnimClipFinishedId ) (%d), bQuitEarly (%d), MF_ShouldStartGunTaskAfterNextAction1", m_MoveNetworkHelper.GetBoolean( ms_AnimClipFinishedId ), bQuitEarly);
				return FSM_Quit;
			}
		
			taskDebugf1("Quit: CTaskMeleeActionResult::PlayClip_OnUpdate: m_MoveNetworkHelper.GetBoolean( ms_AnimClipFinishedId ) (%d), bQuitEarly (%d), pPed->IsNetworkClone() (%d), bForcedReaction (%d)", m_MoveNetworkHelper.GetBoolean( ms_AnimClipFinishedId ), bQuitEarly, pPed->IsNetworkClone(), bForcedReaction);
			SetState( State_Finish );
			return FSM_Continue;
		}
	}	

	//! If flushed tasks, wait until both owner and clone have finished before exiting. 
	if(m_bCloneKillAnimFinished)
	{
		int nTimeout = CNetwork::DEFAULT_CONNECTION_TIMEOUT + 2000;

		bool bTimeOutExpired = fwTimer::GetTimeInMilliseconds() > (m_nCloneKillAnimFinishedTime + nTimeout);

		//! If we are waiting around for ped to respond, then don't leave this state as ped will stand back up. This can happen if ped has gone link dead.
		bool bQuit =  true;
		if(!pPed->IsDead() && !bTimeOutExpired && !pPed->GetUsingRagdoll() && !ShouldApplyEnduranceDamageOnly(pTargetPed, pPed)) 
		{
			bQuit = false;
		}

		if(bQuit)
		{
			if( pParentMeleeTask && pParentMeleeTask->GetIsTaskFlagSet( CTaskMelee::MF_ShouldStartGunTaskAfterNextAction ) )
			{
				taskDebugf1("Quit: CTaskMeleeActionResult::PlayClip_OnUpdate: MF_ShouldStartGunTaskAfterNextAction2");
				return FSM_Quit;
			}

			taskDebugf1("Quit: CTaskMeleeActionResult::PlayClip_OnUpdate: m_bCloneKillAnimFinished, bQuit");
			SetState( State_Finish );
			return FSM_Continue;
		}
	}

	// Drop weapons if killed
	if( !pPed->IsNetworkClone() && !m_bPickupDropped && m_MoveNetworkHelper.GetBoolean( ms_DropWeaponOnDeath ) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead) && ( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ) || pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) ) && pPed->IsFatallyInjured() )
	{
		DropWeaponAndCreatePickup(pPed);
		m_bPickupDropped = true;
	}

	// Process any clip clip events
	ProcessMeleeClipEvents( pPed );

	// Should we fire our weapon?
	if( ShouldFireWeapon() )
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if( pWeaponInfo && pWeaponInfo->GetIsGun() )
		{
			CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if( pWeapon )
			{
				Matrix34 matWeapon = MAT34V_TO_MATRIX34( pPed->GetMatrix() );
				CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
				if( pWeaponObject )
					matWeapon = MAT34V_TO_MATRIX34( pWeaponObject->GetMatrix() );

				// calculate the END of the firing vector using the specific position
				Vector3 vStart, vEnd;
				pWeapon->CalcFireVecFromGunOrientation( pPed, matWeapon, vStart, vEnd );

				// fire the weapon
				CWeapon::sFireParams params( pPed, matWeapon, &vStart, &vEnd );
				pWeapon->Fire( params );
			}
		}
	}

	// Process any dropping or grabbing of weapons.
	if( !pPed->IsNetworkClone() && ProcessWeaponDisarmsAndDrops( pPed ) && pParentMeleeTask )
		pParentMeleeTask->SetTaskFlag( CTaskMelee::MF_ShouldStartGunTaskAfterNextAction );

	// Check to see if somebody has sent us into ragdoll or if
	// force the ragdoll state from an anim event

	if(ShouldSwitchToRagdoll())
	{
		// Allow clones to process damage. This is essentially sending self damage over the network to the owner ped so that
		// we can ensure they die.
		if(pPed->IsNetworkClone())
		{
			ProcessSelfDamage( pPed, true );
		}

		if( !pPed->IsNetworkClone() && !pPed->GetUsingRagdoll())
		{
			if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
			{

				MELEE_NETWORK_DEBUGF("[%d] CTaskMeleeActionResult::PlayClip_OnUpdate: Creating ragdoll event for %s", fwTimer::GetFrameCount(), pPed->GetDebugName());
			
				CEventSwitch2NM event( 10000, rage_new CTaskNMRelax( 1000, 10000 ) );
				pPed->SwitchToRagdoll( event );
			}
			return FSM_Continue;	// stay in this state. The ragdoll event will take care of aborting us during the next event update
									// if we exit now, we'll immediately run the next task whilst the ped is actually in ragdoll.
		}
	}

	// Set the flag to force a process physics update

	bool bProcessPhysics = pPed->IsNetworkClone() ? IsOffensiveMove() || bForcedReaction : true;

	if(bProcessPhysics)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	
		// This is here because we had some trouble with jittering homing in some cases
		// when not double stepping the ProcessPhysics() calls, see
		// B* 1041109: "Peds wobble when melee homing when using -pedphyssinglestep".
		// For performance, it would be best if we could remove this, but that may require additional
		// work on the homing code.
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true );
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	PlayClip_OnExit
// PURPOSE :	CTaskMeleeActionResult animation clean up
// PARAMETERS :	pPed - Ped we are interested in
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::PlayClip_OnExit( CPed* pPed )
{
	// Ensure we apply network damage.
	ProcessCachedNetworkDamage(pPed);

	// Process any damage to apply to ourselves.
	ProcessSelfDamage( pPed );

	if( m_pActionResult )
	{
		// DMKH. Changed from an event to accommodate networking. This will get hit on local & clone tasks.
		CEntity* pTargetEntity = GetTargetEntity();
		if( pTargetEntity && pTargetEntity->GetIsTypePed() && ( m_pActionResult->GetIsAStealthKill() || m_pActionResult->GetIsATakedown() ) )
		{
			CPed* pTargetPed = static_cast<CPed*>( pTargetEntity );

			// A generic disturbance event which tests if nearby friendlies have seen the event
			CEventDisturbance event( pTargetPed, pPed, CEventDisturbance::ePedKilledByStealthKill );
			event.CommunicateEvent( pTargetPed );
		}
	}

	// Set the desired heading to match the current so we do not have extra turning post melee action
	if(!pPed->IsNetworkClone())
	{
		pPed->SetDesiredHeading( pPed->GetCurrentHeading() );
	}
}

void CTaskMeleeActionResult::Finish_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ProcessPhysics
// PURPOSE :	Physics update
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::ProcessPhysics( float fTimeStep, int nTimeSlice )
{
#if __DEV
	if( !CCombatMeleeDebug::sm_bDoHomingAndSeparation )
		return false;
#endif // __DEV

	if( GetState() == State_PlayClip )
	{
		CPed *pPed = GetPed(); //Get the ped ptr.

		// Check and handle any homing during this action and
		// also keep the peds a minimum distance apart.
		CHoming* pHoming = GetHoming(pPed);
		if( GetTargetEntity() || ( pHoming && pHoming->IsSurfaceProbe() ) )
			ProcessHomingAndSeparation( pPed, fTimeStep, nTimeSlice );
		else if( m_MoveNetworkHelper.IsNetworkActive() )
		{
			const crClip* pClip = m_MoveNetworkHelper.GetClip( ms_ClipId );
			if( pClip && !IsPastCriticalFrame() )
			{
				float fCurrentPhase = m_MoveNetworkHelper.GetFloat( ms_ClipPhaseId );
				const float	fHeadingDelta = SubtractAngleShorter( pPed->GetDesiredHeading(), pPed->GetCurrentHeading() );
				const float fTimeRemaining = pClip->ConvertPhaseToTime( MAX( 0.0f, m_fCriticalFrame - fCurrentPhase ) );
				if( fTimeRemaining > 0.0f )
					pPed->SetDesiredAngularVelocity( pPed->GetReferenceFrameAngularVelocity() + Vector3( 0.0f, 0.0f, fHeadingDelta / fTimeRemaining ) );
			}
		}
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	FixupKillMoveOnMigration
// PURPOSE :	When a ped migrates, if we are flushing tasks, then we need to
//				take responsibility and kill the ped.
// PARAMETERS :	
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::FixupKillMoveOnMigration()
{
	bool bForcedReaction = GetParent() && static_cast<CTaskMelee*>( GetParent() )->GetTaskFlags().IsFlagSet( CTaskMelee::MF_ForcedReaction );
	if(bForcedReaction)
	{
		//! Try and ragdoll as ped isn't going to be in a great state to animate from here.
		if(!GetPed()->GetUsingRagdoll() && CTaskNMBehaviour::CanUseRagdoll( GetPed(), RAGDOLL_TRIGGER_DIE ) )
		{
			CEventSwitch2NM event( 10000, rage_new CTaskNMRelax( 1000, 10000 ) );
			GetPed()->SwitchToRagdoll( event );
		}

		//! Kill the ped.
		m_bSelfDamageApplied=false;
		ProcessSelfDamage(GetPed(), true);
	}
}

bool CTaskMeleeActionResult::HandlesRagdoll(const CPed* pPed) const
{
	//! Allow melee to handle a clone ragdoll if we are doing an execution move. This prevents move being terminated.
	bool bForcedReaction = GetParent() && static_cast<const CTaskMelee*>( GetParent() )->GetIsTaskFlagSet( CTaskMelee::MF_ForcedReaction );
	if(pPed->IsNetworkClone() && bForcedReaction)
	{
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	InitTunables
// PURPOSE :	Initializes cloud tunables
/////////////////////////////////////////////////////////////////////////////////
float CTaskMeleeActionResult::sm_fMeleeRightFistMultiplier = 1.0f;
s32 CTaskMeleeActionResult::sm_nMeleeEnduranceDamage = -1;

void CTaskMeleeActionResult::InitTunables()
{
	sm_fMeleeRightFistMultiplier = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("KNUCKLE_DUSTER_DMG_MULT", 0x0cdb4825), sm_fMeleeRightFistMultiplier);
	sm_nMeleeEnduranceDamage = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_MELEE_DAMAGE", 0x518E4E17), -1);
}

u32 CTaskMeleeActionResult::GetMeleeEnduranceDamage()
{
	return sm_nMeleeEnduranceDamage != -1 ? sm_nMeleeEnduranceDamage : sm_Tunables.m_MeleeEnduranceDamage;
}

#if !__FINAL

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetStateName
// PURPOSE :	Returns debug state name along with some extra info
// PARAMETERS :	iState - State we are interested in
/////////////////////////////////////////////////////////////////////////////////
const char * CTaskMeleeActionResult::GetStateName( s32 iState ) const
{
#if __DEV
	static char szBuffer[256];
	if( GetState() == State_PlayClip )
	{
		if( m_pActionResult )
		{
			sprintf( szBuffer, "State_PlayClip: %s", m_pActionResult->GetName() );
			return szBuffer;
		}
	}
#endif // __DEV

	return CTask::GetStateName( iState  );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetStaticStateName
// PURPOSE :	Simply returns debug state name
// PARAMETERS :	iState - State we are interested in
/////////////////////////////////////////////////////////////////////////////////
const char* CTaskMeleeActionResult::GetStaticStateName( s32 iState  )
{
	switch( iState )
	{
		case State_Start:			return "State_Start";
		case State_PlayClip:		return "State_PlayClip";
		case State_Finish:			return "State_Finish";
		default: taskAssert(0);
	}

	return "State_Invalid";
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetCurrentPhase
// PURPOSE :	Returns the current phase based on the animation that is currently
//				being processed.
// PARAMETERS :	iState - State we are interested in
/////////////////////////////////////////////////////////////////////////////////
const float CTaskMeleeActionResult::GetCurrentPhase( void ) const
{
	float fReturnPhase = 0.0f;
	if( m_MoveNetworkHelper.IsNetworkActive() )
		fReturnPhase = m_MoveNetworkHelper.GetFloat( ms_ClipPhaseId );

	return fReturnPhase;
}
#endif //!__FINAL

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResultFromID
// PURPOSE :	Returns the CActionResult given an id
// PARAMETERS :	iResultID - Id that we would like to look up
/////////////////////////////////////////////////////////////////////////////////
const CActionResult* GetActionResultFromID( u32 iResultID )
{
	u32 nIndexFound = 0;
	return (iResultID != INVALID_ACTIONRESULT_ID) ? ACTIONMGR.FindActionResult( nIndexFound, iResultID ) : NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetActionResultFromID
// PURPOSE :	Returns the CActionResult given an id
// PARAMETERS :	iResultID - Id that we would like to look up
/////////////////////////////////////////////////////////////////////////////////
const CActionDefinition* GetActionDefinitionFromID( u32 iDefinitionID )
{
	u32 nIndexFound = 0;
	return (iDefinitionID != INVALID_ACTIONDEFINITION_ID) ? ACTIONMGR.FindActionDefinition( nIndexFound, iDefinitionID ) : NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateQueriableState
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskMelee::CreateQueriableState( void ) const
{
	// if the player weapon subtask is running, the ped is in first person and we don't want to create a melee clone task on the remote in this case
	// (we want the anims to be driven by the gun task)
	bool bInFP = (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_PLAYER_WEAPON);

	CTaskInfo* pInfo = rage_new CClonedTaskMeleeInfo( m_nActionResultNetworkID, 
		m_pTargetEntity, 
		m_nFlags, 
		GetState(), 
		m_queueType,
		IsBlocking(),
		bInFP);
	return pInfo;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ReadQueriableState
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	taskAssert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_MELEE );
	CTaskFSMClone::ReadQueriableState( pTaskInfo );

	//! Fixup melee task.
	CClonedTaskMeleeInfo *pMeleeInfo = static_cast<CClonedTaskMeleeInfo*>(pTaskInfo);

	//! Update target entity.
	m_nFlags = pMeleeInfo->m_nFlags;

	m_cloneQueueType = (QueueType)pMeleeInfo->m_nQueueType;
		
	if( m_nActionResultNetworkID != pMeleeInfo->m_nActionResultID )
		m_nActionResultNetworkID = pMeleeInfo->m_nActionResultID;

	m_bIsBlocking = pMeleeInfo->m_bBlocking;

	SetTargetEntity( pMeleeInfo->m_pTarget.GetEntity(), HasLockOnTargetEntity() );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateClonedFSM
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMelee::UpdateClonedFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed* pPed = GetPed();
	if( iEvent == OnUpdate )
	{
		//! Force queue type when ped is available.
		SetQueueType(m_cloneQueueType);

		//! If we are trying to finish, but want to trigger a local reaction, then go to a state that will process local reaction.
		if(GetState() == State_Finish && m_bLocalReaction && m_pNextActionResult)
		{
			MELEE_NETWORK_DEBUGF( "[%d] CTaskMelee::UpdateClonedFSM - delay finish for local reaction - sequence ID = %d", fwTimer::GetFrameCount(), GetNetSequenceID() );
			SetState(State_WaitForMeleeActionDecide);
			return FSM_Continue;
		}

		//! If we have tried to trigger a local melee reaction, but aren't in the correct state, attempt to force it.
		bool bForceCheckForLocalReaction = false;
		if(GetState()!=State_MeleeAction && m_bLocalReaction)
		{
			bForceCheckForLocalReaction = true;
		}

		if( !IsRunningLocally() || bForceCheckForLocalReaction)
		{
			u32 expectedCloneTaskType = 0;
			if(bForceCheckForLocalReaction)
			{
				expectedCloneTaskType = CTaskTypes::TASK_MELEE_ACTION_RESULT;
			}
			else
			{
				switch(GetStateFromNetwork())
				{
				case(State_SwapWeapon):
					expectedCloneTaskType = CTaskTypes::TASK_SWAP_WEAPON;
					break;
				default:
					expectedCloneTaskType = CTaskTypes::TASK_MELEE_ACTION_RESULT;
					break;
				}
			}

			//! Kick off networked sub task (if available).
			CTaskFSMClone *pCloneSubTask = NULL;

			bool bSubTaskRunningLocally = false;
			if( GetSubTask() && 
				GetSubTask()->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT && 
				expectedCloneTaskType == CTaskTypes::TASK_MELEE_ACTION_RESULT)
			{
				CTaskMeleeActionResult* pActionResultTask = static_cast<CTaskMeleeActionResult*>(GetSubTask());
				bSubTaskRunningLocally = pActionResultTask->IsRunningLocally();
			}

			//! If melee task isn't sitting at correct seq num, don't create sub task as we'll end up deleting it immediately.
			bool bDeferCreatingSubTask = false;
			bool bRunningMeleeTask = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE );
			if(bRunningMeleeTask)
			{
				CTaskInfo*	pMeleeTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->FindTaskInfoIfCurrentlyRunning( CTaskTypes::TASK_MELEE );
				if(pMeleeTaskInfo && (pMeleeTaskInfo->GetNetSequenceID() != GetNetSequenceID()))
				{
					MELEE_NETWORK_DEBUGF( "[%d] CTaskMelee::UpdateClonedFSM - Deferring sub task creation - sequence ID = %d new sequence ID = %d", fwTimer::GetFrameCount(), GetNetSequenceID(), pMeleeTaskInfo ? pMeleeTaskInfo->GetNetSequenceID() : 0 );
					bDeferCreatingSubTask = true;
				}
			}

			//! Don't spawn subtask whilst we have one running locally.
			if(!bSubTaskRunningLocally && !bDeferCreatingSubTask)
			{
				pCloneSubTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType( expectedCloneTaskType );
				if( pCloneSubTask )
				{
					MELEE_NETWORK_DEBUGF( "[%d] CTaskMelee::UpdateClonedFSM [Sub Task: %p] - sequence ID = %d", fwTimer::GetFrameCount(), pCloneSubTask, pCloneSubTask->GetNetSequenceID() );
					SetNewTask( pCloneSubTask );
				}
			}

			if( (GetStateFromNetwork() != -1) && ((GetStateFromNetwork() != GetState()) || pCloneSubTask))
			{
				SetFlag( aiTaskFlags::KeepCurrentSubtaskAcrossTransition );

				if( GetStateFromNetwork() == GetState() )
				{
					SetFlag( aiTaskFlags::RestartCurrentState );
				}
				else
				{
					bool bSetState = true;

					//! As local task will end before clone does, don't re-enter melee action if we don't have a sub task.
					if(GetStateFromNetwork() == State_MeleeAction && !pCloneSubTask)
					{
						bSetState = false;
					}

					//! Don't repeat start state if we have already processed it.
					if(GetState() > State_Start && GetStateFromNetwork() == State_Start)
					{
						bSetState = false;
					}

					if(bSetState)
					{
						SetState( GetStateFromNetwork() );
					}
				}

				if(pCloneSubTask)
				{
					MELEE_NETWORK_DEBUGF( "[%d] CTaskMelee::UpdateClonedFSM [%p] - changing state = %d/%d", CNetwork::GetNetworkTime(), this, GetState(), GetStateFromNetwork() ) ;
				}

				return FSM_Continue;
			}

			if( GetStateFromNetwork() == -1 && (GetSubTask() == NULL || bDeferCreatingSubTask))
			{
				MELEE_NETWORK_DEBUGF("Quit: CTaskMelee::UpdateClonedFSM: GetStateFromNetwork() == -1 && (GetSubTask() (0x%p) == NULL || bDeferCreatingSubTask (%d))", GetSubTask(), bDeferCreatingSubTask);
				return FSM_Quit;
			}
		}
		else
		{
			CPed* pTargetPed = static_cast<CPed*>(GetTargetEntity());

			// quit a locally running reaction if the target ped is not meleeing anymore
			if (pTargetPed && pTargetPed->IsAPlayerPed() && pTargetPed->IsDead())
			{
				MELEE_NETWORK_DEBUGF("Quit: CTaskMelee::UpdateClonedFSM: !pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE )");
				return FSM_Quit;
			}

			//! If we are running locally, but don't have a task to go to, then just bail.
			if(!m_pNextActionResult && (GetSubTask() == NULL) && (GetNewTask() == NULL) && GetStateFromNetwork() == -1)
			{
				MELEE_NETWORK_DEBUGF("Quit: CTaskMelee::UpdateClonedFSM: !m_pNextActionResult && (GetSubTask() == NULL) && (GetNewTask() == NULL) && GetStateFromNetwork() == -1");
				return FSM_Quit;
			}
		}
	}

	FSM_Begin

		// Start
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter( pPed );
			FSM_OnUpdate
				return Start_OnUpdate( pPed );
			FSM_OnExit
				Start_OnExit();

		// Decide whether this should go into player or AI melee action detection
		FSM_State(State_WaitForMeleeActionDecide)
			FSM_OnEnter
				WaitForMeleeActionDecide_OnEnter( pPed );
			FSM_OnUpdate
				return WaitForMeleeActionDecide_OnUpdate();

		// Running a melee action
		FSM_State(State_MeleeAction)
			FSM_OnEnter
				MeleeAction_OnEnter( pPed );
			FSM_OnUpdate
				return MeleeAction_OnUpdate( pPed );
			FSM_OnExit
				MeleeAction_OnExit( pPed );

		// Scanning for a new melee action
		FSM_State(State_WaitForMeleeAction)
			FSM_OnEnter
				WaitForMeleeAction_OnEnter();
			FSM_OnUpdate
				return WaitForMeleeAction_OnUpdate( pPed );
			FSM_OnExit
				WaitForMeleeAction_OnExit();

		// Main wait state for those that are deemed as an Inactive Observer
		FSM_State(State_InactiveObserverIdle)
			FSM_OnUpdate
				return InactiveObserverIdle_OnUpdate( pPed );

		// Quit
		FSM_State(State_Finish)
			FSM_OnUpdate
				MELEE_NETWORK_DEBUGF("Quit: CTaskMelee::UpdateClonedFSM: State_Finish");
				return FSM_Quit;

		FSM_State(State_ChaseAfterTarget)
		FSM_State(State_InitialPursuit)
		FSM_State(State_LookAtTarget)
		FSM_State(State_RetreatToSafeDistance)
		FSM_State(State_WaitForTarget)
		FSM_State(State_SwapWeapon)

	FSM_End
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	OnCloneTaskNoLongerRunningOnOwner
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
void CTaskMelee::OnCloneTaskNoLongerRunningOnOwner( void )
{
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MELEE_ACTION_RESULT)
	{
		//! Don't kill melee parent task whilst still playing melee action.
		SetStateFromNetwork(-1);
	}
	else
	{
		MELEE_NETWORK_DEBUGF("Quit: CTaskMelee::OnCloneTaskNoLongerRunningOnOwner");
		SetStateFromNetwork( State_Finish );
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	HandleLocalToRemoteSwitch
// PURPOSE :	
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::HandleLocalToRemoteSwitch(CPed* UNUSED_PARAM(pPed), CClonedFSMTaskInfo* UNUSED_PARAM(pTaskInfo))
{
	//! Don't fixup to remote if we haven't triggered action yet.
	if(GetTaskFlags().IsFlagSet( CTaskMelee::MF_ForcedReaction ) && GetTaskFlags().IsFlagSet( CTaskMelee::MF_IsDoingReaction ))
	{
		return false;
	}

	//! Don't fixup if we are trying to trigger a local melee action.
	if(m_pNextActionResult)
	{
		return false;
	}

	//! Defer to action sub task.
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MELEE_ACTION_RESULT)
	{
		if(!static_cast<CTaskMeleeActionResult*>(GetSubTask())->IsRunningLocally())
		{
			MELEE_NETWORK_DEBUGF( "[%d] CTaskMelee::HandleLocalToRemoteSwitch - Subtask not running locally", fwTimer::GetFrameCount());
			return true;
		}
		else
		{
			return false;
		}
	}

	MELEE_NETWORK_DEBUGF( "[%d] CTaskMelee::HandleLocalToRemoteSwitch - No sub task", fwTimer::GetFrameCount());
	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AllowsLocalHitReactions
// PURPOSE :	
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMelee::AllowsLocalHitReactions() const
{
	const CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->IsClonedFSMTask())
	{
		return static_cast<const CTaskFSMClone *>(pSubTask)->AllowsLocalHitReactions();
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CompareCollisionInfos
// PURPOSE :	QSORT function to compare 2 melee collisions
/////////////////////////////////////////////////////////////////////////////////
int CTaskMelee::CompareCollisionInfos( const sCollisionInfo* pA1, const sCollisionInfo* pA2 )
{
	WorldProbe::CShapeTestHitPoint* pResultA1 = pA1->m_pResult;
	WorldProbe::CShapeTestHitPoint* pResultA2 = pA2->m_pResult;
	const CEntity* pEntityA = pResultA1->GetHitEntity();
	Assert( pEntityA );

	const CEntity* pEntityB = pResultA2->GetHitEntity();
	Assert( pEntityB );

	// Check if the two impacts hit the same instance.
	if( pEntityA == pEntityB )
	{
		// Head is considered the largest component and the most important for melee
		if( pResultA1->GetHitComponent() > pResultA2->GetHitComponent() )
			return 1;
		// We should select glass above everything
		else if( pEntityA->GetIsTypeVehicle() && PGTAMATERIALMGR->GetIsGlass( pResultA1->GetMaterialId() ) )
			return 1;
	}
	// Always prioritize peds over objects
	else if( pEntityA->GetIsTypePed() && !pEntityB->GetIsTypePed() )
		return 1;

	return -1;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetUniqueNetworkMeleeActionID
// PURPOSE :	QSORT function to compare 2 melee collisions
/////////////////////////////////////////////////////////////////////////////////
u16	CTaskMelee::GetUniqueNetworkMeleeActionID() const
{
	//! Get it from sub task if available, as 'm_nUniqueNetworkMeleeActionID' is consumed when we 
	//! create action.
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MELEE_ACTION_RESULT)
	{
		return static_cast<const CTaskMeleeActionResult*>(GetSubTask())->GetUniqueNetworkActionID();
	}

	return m_nUniqueNetworkMeleeActionID;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetMeleeGripFilter
// PURPOSE :	Gets melee grip filter specified in clipsets.xml
/////////////////////////////////////////////////////////////////////////////////
crFrameFilter* CTaskMelee::GetMeleeGripFilter(fwMvClipSetId clipsetId)
{
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipsetId);
	if (pClipSet)
	{
		// Grab the requested filter from the clip item
		fwMvFilterId filter(FILTER_BOTHARMS);
		fwMvClipId clipId("GRIP_IDLE", 0x3ec63b58);
		const fwClipItem* pItem = pClipSet->GetClipItem(clipId);
		if (pItem && pItem->IsClipItemWithProps())
		{
			const fwClipItemWithProps* pItemWithProps = static_cast<const fwClipItemWithProps*>(pItem);
			filter.SetHash(pItemWithProps->GetBoneMask().GetHash());

			if (filter != FILTER_ID_INVALID)
			{
				return g_FrameFilterDictionaryStore.FindFrameFilter(filter);
			}
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// Task serialization info for CClonedTaskMeleeInfo
//////////////////////////////////////////////////////////////////////////
CClonedTaskMeleeInfo::CClonedTaskMeleeInfo( u32 iActionResultID, CEntity* pTarget, u32 iFlags, s32 iState, s32 nQueueType, bool bBlocking, bool bInFP) 
: m_nActionResultID( iActionResultID )
, m_nFlags( iFlags )
, m_nQueueType( nQueueType )
, m_bBlocking( bBlocking )
, m_bInFP( bInFP )
{
	m_pTarget.SetEntity( pTarget );
	SetStatusFromMainTaskState( iState );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CClonedTaskMeleeInfo
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CClonedTaskMeleeInfo::CClonedTaskMeleeInfo() 
: m_nActionResultID( INVALID_ACTIONRESULT_ID )
, m_nFlags( 0 )
, m_nQueueType( -1 )
, m_bBlocking( false )
, m_bInFP( false )
{
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateCloneFSMTask
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone* CClonedTaskMeleeInfo::CreateCloneFSMTask()
{
	// if the player is in first person and we don't want to create a melee clone task on the remote in this case (we want the anims to be driven by the gun task)
	if (m_bInFP)
	{
		return NULL;
	}

	CTaskMelee* pTaskMelee = rage_new CTaskMelee( GetActionResultFromID( m_nActionResultID ), m_pTarget.GetEntity(), m_nFlags );
	pTaskMelee->m_bIsBlocking = m_bBlocking;
	return pTaskMelee;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateQueriableState
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskMeleeActionResult::CreateQueriableState() const
{
	u32 nActionResultID = m_pActionResult ? m_pActionResult->GetID() : INVALID_ACTIONRESULT_ID;
	u32 nForcedReactionDefinitionID = m_pForcedReactionActionDefinition ? m_pForcedReactionActionDefinition->GetID() : INVALID_ACTIONDEFINITION_ID;

	CTaskInfo* pInfo = rage_new CClonedTaskMeleeResultInfo( nActionResultID, 
															nForcedReactionDefinitionID,
															m_pTargetEntity,
															m_bHasLockOnTargetEntity,
															GetState(), 
															m_bIsDoingReaction, 
															m_bAllowDistanceHoming,
															m_bOwnerStoppedDistanceHoming,
															m_nSelfDamageWeaponHash,
															(s32)m_eSelfDamageAnimBoneTag,
															m_nNetworkTimeTriggered,
															m_nUniqueNetworkActionID,
															VEC3V_TO_VECTOR3(m_vCachedHomingPosition));
	return pInfo;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ReadQueriableState
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	taskAssert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_MELEE_ACTION_RESULT );
	CTaskFSMClone::ReadQueriableState( pTaskInfo );

	CClonedTaskMeleeResultInfo* pMeleeInfo = static_cast<CClonedTaskMeleeResultInfo*>( pTaskInfo );
	m_bIsDoingReaction = pMeleeInfo->m_bIsDoingReaction;
	m_bAllowDistanceHoming = pMeleeInfo->m_bAllowDistanceHoming;
	m_bOwnerStoppedDistanceHoming = pMeleeInfo->m_bOwnerStoppedDistanceHoming;

	//! Update target entity.
	SetTargetEntity( pMeleeInfo->m_pTarget.GetEntity(), pMeleeInfo->m_bHasLockOnTargetEntity );
	
	//! Fix up action result. This might happen if we locally simulate a hit reaction that is different to what the ped 
	//! host created. It might just look worse to fix this, so I might disable it.
	if( m_nActionResultNetworkID != pMeleeInfo->m_uActionResultID )
	{
		m_nActionResultNetworkID = pMeleeInfo->m_uActionResultID;

#if __ASSERT
		const CActionResult *pResult = GetActionResultFromID( pMeleeInfo->m_uActionResultID );
		if( pResult != m_pActionResult )
		{
			MELEE_NETWORK_DEBUGF( "CTaskMeleeActionResult::ReadQueriableState - Action ID changed" );
		}
#endif
	}

	//! Fixup Forced Reaction.
	m_pForcedReactionActionDefinition = GetActionDefinitionFromID(pMeleeInfo->m_uForcedReactionActionDefinitionID);

	m_nNetworkTimeTriggered = pMeleeInfo->m_uNetworkTimeTriggered;

	if(m_nUniqueNetworkActionID != pMeleeInfo->m_uActionID && !IsRunningLocally())
	{
		MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::ReadQueriableState - Changing unique ID = %d:%d Address %p", fwTimer::GetFrameCount(), m_nUniqueNetworkActionID, pMeleeInfo->m_uActionID, this );
		m_nUniqueNetworkActionID = pMeleeInfo->m_uActionID;
	}

	m_vCloneCachedHomingPosition = VECTOR3_TO_VEC3V(pMeleeInfo->m_vCachedHomingPosition);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	UpdateClonedFSM
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskMeleeActionResult::UpdateClonedFSM( const s32 iState, const FSM_Event iEvent )
{
	if( (GetState() != State_Finish) && m_bFixupActionResultFromNetwork && iEvent == OnUpdate )
	{
		m_bFixupActionResultFromNetwork = false;

		m_bHeadingInitialized = false;
		m_bDidAllowDistanceHoming = false; 

		const CActionResult* pNewResult = GetActionResultFromID( m_nActionResultNetworkID );

		if( pNewResult && ( m_pActionResult != pNewResult ) )
		{
			MELEE_NETWORK_DEBUGF( "CTaskMeleeActionResult::UpdateClonedFSM - Action ID changed" );
			m_pActionResult = pNewResult;

			//! Avoid an immediate interrupt. Need to play new anim.
			m_bAllowInterrupt = false;

			//! Reset action result params. This prevents any unwanted logic running on invalid params (e.g. self damage if we have switched from 
			//! being killed to doing a kill).
			CacheActionResult(GetPed());

			if( GetState() == State_Start )
			{
				SetFlag( aiTaskFlags::RestartCurrentState );
			}
			else 
			{
				SetState( State_Start );
			}

			//! If network has started, perform a restart.
			if(m_MoveNetworkHelper.IsNetworkActive() && m_MoveNetworkHelper.IsNetworkAttached())
			{
				m_MoveNetworkHelper.SendRequest(ms_RestartPlayClipRequestId);
			}

			return FSM_Continue;
		}
	}

	if(WasLocallyStarted())
	{
		//! Detect if we played a local anim reaction on the clone, whereas the owner picked a ragdoll.
		bool bIsOwnerUsingNm = GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_NM_CONTROL );

		if((bIsOwnerUsingNm || GetPed()->GetIsInVehicle()))
		{
			MELEE_NETWORK_DEBUGF("Quit: CTaskMeleeActionResult::UpdateClonedFSM: (bIsOwnerUsingNm (%d) || GetPed()->GetIsInVehicle() (%d))", bIsOwnerUsingNm, GetPed()->GetIsInVehicle());
			return FSM_Quit;
		}

		//! If owner isn't running melee & is far enough away, interrupt.
		static dev_float uMaxTimeForLocalReact = 0.5f;
		CTaskMelee* pParentMeleeTask = GetParent() ? static_cast<CTaskMelee*>( GetParent() ) : NULL;
		bool bForcedReaction = pParentMeleeTask && pParentMeleeTask->GetIsTaskFlagSet( CTaskMelee::MF_ForcedReaction );
		bool bOwnerRunningMelee = (GetStateFromNetwork() != -1) || GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE_ACTION_RESULT );
		if(!bOwnerRunningMelee && !bForcedReaction && GetTimeInState() > uMaxTimeForLocalReact )
		{
			static dev_float s_fBreakoutDistance = 1.0f;
			Vector3 vCloneMasterPosition = NetworkInterface::GetLastPosReceivedOverNetwork(GetPed());
			Vector3 vCurrentPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
			Vector3 vDist = vCloneMasterPosition - vCurrentPosition;
			float fDistSq = vDist.Mag2();
			if(fDistSq > (s_fBreakoutDistance*s_fBreakoutDistance) )
			{
				MELEE_NETWORK_DEBUGF("Quit: CTaskMeleeActionResult::UpdateClonedFSM: fDistSq (%.2f) > (s_fBreakoutDistance*s_fBreakoutDistance (%.2f))", fDistSq, s_fBreakoutDistance*s_fBreakoutDistance);
				return FSM_Quit;
			}
		}
	}

	if( GetStateFromNetwork() == -1 && (!IsRunningLocally() && ShouldAllowInterrupt()))
	{
		MELEE_NETWORK_DEBUGF("Quit: CTaskMeleeActionResult::UpdateClonedFSM: GetStateFromNetwork() == -1 && (!IsRunningLocally() && ShouldAllowInterrupt())");
		return FSM_Quit;
	}

	//! Locally simulate action. Don't update through network 
	return UpdateFSM( iState, iEvent );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	OnCloneTaskNoLongerRunningOnOwner
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
void CTaskMeleeActionResult::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(-1);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	OnCloneTaskNoLongerRunningOnOwner
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::OverridesNetworkBlender(CPed* pPed) 
{ 
	return IsOverridingNetBlenderCommon(pPed);
}

bool CTaskMeleeActionResult::OverridesNetworkHeadingBlender(CPed* pPed)
{
	if(WasLocallyStarted() && IsDoingReaction())
	{
		return true;
	}

	return IsOverridingNetBlenderCommon(pPed);
}

bool CTaskMeleeActionResult::IsOverridingNetBlenderCommon(CPed *pPed)
{
	TUNE_GROUP_BOOL(MELEE_DEBUG, bOverrideNetBlender, true);

	if(bOverrideNetBlender)
	{
		if(WasLocallyStarted())
		{
			//! Allow a small error tolerance. If inside this, override blender.
			static dev_float s_fErrorToleranceIn = 0.5f;
			static dev_float s_fErrorToleranceOut = 0.75f;
			Vector3 vCloneMasterPosition = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
			Vector3 vCurrentPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vDist = vCloneMasterPosition - vCurrentPosition;
			float fDistSq = vDist.Mag2();

			float fTolerance = m_bInNetworkBlenderTolerance ? s_fErrorToleranceOut : s_fErrorToleranceIn;

			if(fDistSq < square(fTolerance))
			{
				m_bInNetworkBlenderTolerance = true;
				return true;
			}
			else
			{
				m_bInNetworkBlenderTolerance = false;
			}

			//! Don't let net blender run if we are running this task locally.
			if(IsRunningLocally() && !m_bHandledLocalToRemoteSwitch)
			{
				return true;
			}
		}

		//! Do homing on clone side to ensure ped is in correct position relative to ped.
		if(GetTargetEntity())
		{
			if(m_bDidAllowDistanceHoming && !m_bOwnerStoppedDistanceHoming)
			{
				return true;
			}
		}

		if(GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth) || 
			GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown) || 
			(GetParent() && static_cast<CTaskMelee*>( GetParent() )->GetTaskFlags().IsFlagSet( CTaskMelee::MF_ForcedReaction ) ) )
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	OnCloneTaskNoLongerRunningOnOwner
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::HandleLocalToRemoteSwitch(CPed* pPed, CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(!m_bFixupActionResultFromNetwork);

	taskAssert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_MELEE_ACTION_RESULT );
	CClonedTaskMeleeResultInfo* pMeleeInfo = static_cast<CClonedTaskMeleeResultInfo*>( pTaskInfo );

	m_bHandledLocalToRemoteSwitch = true;

	//! break out of reaction and do attack. TO DO - interrupt?
	bool bWasKillMove = false;
	bool bWasAttack = false;
	bool bIsKillMove = false;
	bool bIsAttack = false;

	if( !pMeleeInfo->GetIsActiveTask() )
	{
		MELEE_NETWORK_DEBUGF( "%s [Frame %d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - m_pActionResult [%s] Task Info not active ",
			pPed->GetDebugName(),
			fwTimer::GetFrameCount(),
			m_pActionResult ? m_pActionResult->GetName() : "Null");

		return false;
	}

	bool bForcingReaction = (GetParent() && static_cast<CTaskMelee*>( GetParent() )->GetTaskFlags().IsFlagSet( CTaskMelee::MF_ForcedReaction ));

	if(m_pActionResult)
	{
		 bWasKillMove = m_pActionResult->GetIsATakedown() || m_pActionResult->GetIsAStealthKill() || m_pActionResult->GetIsAKnockout();
		 bWasAttack = bWasKillMove || m_pActionResult->GetIsAStandardAttack() || m_pActionResult->GetIsACounterAttack();
	}

	const CActionResult* pNewResult = GetActionResultFromID( pMeleeInfo->m_uActionResultID );
	if(pNewResult)
	{
		bIsKillMove = pNewResult->GetIsATakedown() || pNewResult->GetIsAStealthKill() || pNewResult->GetIsAKnockout();
		bIsAttack = pNewResult->GetIsAStandardAttack() || m_pActionResult->GetIsACounterAttack();
	}

	//! If reaction is the same, handle switch. otherwise continue to play.
	u32 nCurrentID = m_pActionResult ? m_pActionResult->GetID() : 0;
	if( nCurrentID == pMeleeInfo->m_uActionResultID )
	{
		MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Same move [%s|%s] ", fwTimer::GetFrameCount(), m_pActionResult ? m_pActionResult->GetName() : "", pNewResult ? pNewResult->GetName() : "");
		return true;
	}

	//! Arbitrate kill move. A clone is attempting to trigger a kill move, but is being killed already. Check timestamps & player ID's to
	//! see who wins.
	if(bForcingReaction)
	{
		if(bIsKillMove)
		{
			const CDamageAndReaction* pDamageAndReaction = pNewResult->GetDamageAndReaction();
			if( pDamageAndReaction && 
				pDamageAndReaction->ShouldForceImmediateReaction() &&
				m_pTargetEntity &&
				m_pTargetEntity->GetIsTypePed() &&
				pMeleeInfo->m_pTarget.GetEntity() == m_pTargetEntity)
			{
				CPed* pTargetPed = static_cast<CPed*>(m_pTargetEntity.Get());
				if(!pTargetPed->IsNetworkClone())
				{
					CTaskMeleeActionResult* pSimpleMeleeActionResultTask = static_cast<CTaskMeleeActionResult*>(pTargetPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_MELEE_ACTION_RESULT ) );
					if( pSimpleMeleeActionResultTask )
					{
						MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Arbitrate kill move start [%d|%d] ", fwTimer::GetFrameCount(), pSimpleMeleeActionResultTask->GetNetworkTimeTriggered(), pMeleeInfo->m_uNetworkTimeTriggered);

						bool bFixup = false;
						if(pMeleeInfo->m_uNetworkTimeTriggered < pSimpleMeleeActionResultTask->GetNetworkTimeTriggered())
						{
							MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Clone network time won! [%d|%d] ", fwTimer::GetFrameCount(), pSimpleMeleeActionResultTask->GetNetworkTimeTriggered(), pMeleeInfo->m_uNetworkTimeTriggered);

							// clone won.
							bFixup = true;
						}
						else if( (pSimpleMeleeActionResultTask->GetNetworkTimeTriggered() == pMeleeInfo->m_uNetworkTimeTriggered) && 
							pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetPlayerOwner() && 
							pTargetPed->GetNetworkObject() && pTargetPed->GetNetworkObject()->GetPlayerOwner())
						{
							// use player owner ID's to determine who won. 
							u8 nOwnerIndex = pPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
							u8 nTargetIndex = pTargetPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();

							if(nOwnerIndex < nTargetIndex)
							{
								MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Clone ID won! [%d|%d] ", fwTimer::GetFrameCount(), nOwnerIndex, nTargetIndex);
								bFixup = true;
							}
							else
							{
								MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - local target won! [%d|%d] ", fwTimer::GetFrameCount(), nOwnerIndex, nTargetIndex);
								return false;
							}
						}	
						else
						{
							MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - local target won! [%d|%d] ", fwTimer::GetFrameCount(), pSimpleMeleeActionResultTask->GetNetworkTimeTriggered(), pMeleeInfo->m_uNetworkTimeTriggered);
							return false;
						}
						
						if(bFixup)
						{
							MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Arbitrated kill move end [%s|%s]", fwTimer::GetFrameCount(), m_pActionResult ? m_pActionResult->GetName() : "", pNewResult ? pNewResult->GetName() : "");

							m_bFixupActionResultFromNetwork = true;
							m_nActionResultNetworkID=pMeleeInfo->m_uActionResultID;
							return true;
						}
					}
				}
			}
		}
		else
		{
			//! If being killed, don't break out for any move.
			return false;
		}
	}

	//! If doing a kill move, don't accept anything else.
	if(!bWasKillMove)
	{
		//! If breaking out into a kill move, accept.
		if(bIsKillMove)
		{
			MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Special Move Switch [%s|%s]", fwTimer::GetFrameCount(), m_pActionResult ? m_pActionResult->GetName() : "", pNewResult ? pNewResult->GetName() : "");
			m_bFixupActionResultFromNetwork = true;
			m_nActionResultNetworkID=pMeleeInfo->m_uActionResultID;
			return true;
		}

		//! Allow breaking out into an attack after x secs of local reaction.
		static dev_float s_MinTimeToRun = 0.25f;
		if(!bWasAttack && bIsAttack && GetTimeRunning() > s_MinTimeToRun)
		{
			MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Attack Switch [%s|%s]", fwTimer::GetFrameCount(), m_pActionResult ? m_pActionResult->GetName() : "", pNewResult ? pNewResult->GetName() : "");
			m_bFixupActionResultFromNetwork = true;
			m_nActionResultNetworkID=pMeleeInfo->m_uActionResultID;
			return true;
		}

		//! Allow breaking out to a new attack when we hit interrupt (e.g. dodging etc).
		if(bIsAttack && pNewResult && ShouldAllowInterrupt())
		{
			MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - Breakout [%s|%s]", fwTimer::GetFrameCount(), m_pActionResult ? m_pActionResult->GetName() : "", pNewResult ? pNewResult->GetName() : "");
			m_bFixupActionResultFromNetwork = true;
			m_nActionResultNetworkID=pMeleeInfo->m_uActionResultID;
			return true;
		}
	
		//! If both moves are hit reactions, accept the switch, but don't fixup action result. This prevents a potential double reaction 
		//! animation due to seq numbers being out of sync.
		if(m_pActionResult && m_pActionResult->GetIsAHitReaction() && 
			pNewResult && pNewResult->GetIsAHitReaction())
		{
			static dev_bool s_FixupDifferentReactions = false;
			if(s_FixupDifferentReactions)
			{
				m_bFixupActionResultFromNetwork = true;
				m_nActionResultNetworkID=pMeleeInfo->m_uActionResultID;
			}
			MELEE_NETWORK_DEBUGF( "[%d] CTaskMeleeActionResult::HandleLocalToRemoteSwitch - hit reaction [%s|%s] ", fwTimer::GetFrameCount(), m_pActionResult ? m_pActionResult->GetName() : "", pNewResult ? pNewResult->GetName() : "");
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	AllowsLocalHitReactions
// PURPOSE :	
/////////////////////////////////////////////////////////////////////////////////
bool CTaskMeleeActionResult::AllowsLocalHitReactions() const
{
	static dev_bool s_bAllowLocalHitReactions = true;
	return s_bAllowLocalHitReactions;
}

//////////////////////////////////////////////////////////////////////////
// Task serialization info for CClonedTaskMeleeResultInfo
//////////////////////////////////////////////////////////////////////////

u8 CClonedTaskMeleeResultInfo::m_nActionIDCounter = 0;

CClonedTaskMeleeResultInfo::CClonedTaskMeleeResultInfo( u32 uActionResultID, 
													    u32 uForcedReactionActionDefinitionID,
													    CEntity* pTarget, 
													    bool bLockOn, 
													    s32 iState, 
													    bool bIsDoingReaction, 
													    bool bAllowDistanceHoming,
														bool bOwnerStoppedDistanceHoming,
														u32 uSelfDamageWeaponHash,
														s32 nSelfDamageAnimBoneTag,
														u32 uNetworkTimeTriggered,
														u16 uUniqueID,
														const Vector3 &vHomingPosition)
: m_uActionResultID( uActionResultID )
, m_uForcedReactionActionDefinitionID( uForcedReactionActionDefinitionID )
, m_pTarget( pTarget )
, m_bHasLockOnTargetEntity( bLockOn )
, m_bIsDoingReaction( bIsDoingReaction )
, m_bAllowDistanceHoming( bAllowDistanceHoming )
, m_bOwnerStoppedDistanceHoming( bOwnerStoppedDistanceHoming )
, m_uSelfDamageWeaponHash( uSelfDamageWeaponHash )
, m_nSelfDamageAnimBoneTag( nSelfDamageAnimBoneTag )
, m_uNetworkTimeTriggered(uNetworkTimeTriggered)
, m_uActionID(uUniqueID)
, m_vCachedHomingPosition(vHomingPosition)
{
	SetStatusFromMainTaskState( iState );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	Default Ctor
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CClonedTaskMeleeResultInfo::CClonedTaskMeleeResultInfo() 
: m_uActionResultID( INVALID_ACTIONRESULT_ID )
, m_uForcedReactionActionDefinitionID ( INVALID_ACTIONDEFINITION_ID )
, m_pTarget( NULL )
, m_bHasLockOnTargetEntity( false )
, m_bIsDoingReaction( false )
, m_bAllowDistanceHoming( false )
, m_bOwnerStoppedDistanceHoming( false )
, m_uSelfDamageWeaponHash( 0 )
, m_nSelfDamageAnimBoneTag( -1 )
, m_uActionID(0)
, m_uNetworkTimeTriggered(0)
, m_vCachedHomingPosition(Vector3::ZeroType)
{
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateCloneFSMTask
// PURPOSE :	Task Melee clone functionality
/////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone * CClonedTaskMeleeResultInfo::CreateCloneFSMTask()
{
	CTaskMeleeActionResult* pTaskMeleeResult = rage_new CTaskMeleeActionResult( GetActionResultFromID( m_uActionResultID ), 
		m_pTarget.GetEntity(), 
		m_bHasLockOnTargetEntity, 
		m_bIsDoingReaction, 
		!m_bAllowDistanceHoming,
		m_uSelfDamageWeaponHash,
		(eAnimBoneTag)m_nSelfDamageAnimBoneTag );

	pTaskMeleeResult->SetForcedReactionActionDefinition(GetActionDefinitionFromID( m_uForcedReactionActionDefinitionID ));

	return pTaskMeleeResult;
}

//////////////////////////////////////////////////////////////////////////
