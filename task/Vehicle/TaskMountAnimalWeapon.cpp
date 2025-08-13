#include "Task/Vehicle/TaskMountAnimalWeapon.h"

// Rage Headers
#include "phbound/boundcomposite.h"
#include "phbound/boundcapsule.h"
#include "phbound/support.h"

// Game headers
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "event/EventDamage.h"
#include "network/General/NetworkPendingProjectiles.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "objects/Door.h"
#include "Peds/HealthConfig.h"
#include "Peds/Ped.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/world/GameWorld.h"
#include "System/control.h"
#include "Stats/StatsInterface.h"
#include "task/Motion/Locomotion/TaskMotionPed.h"
#include "task/Movement/TaskParachute.h"
#include "task/Vehicle/TaskCarAccessories.h"
#include "task/Vehicle/TaskVehicleBase.h"
#include "task/Vehicle/TaskVehicleWeapon.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "task/Weapons/TaskProjectile.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "weapons/WeaponManager.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"

AI_OPTIMISATIONS();
AI_WEAPON_OPTIMISATIONS();

//////////////////////////////////////////////////////////////////////////
// Class CTaskMountThrowProjectile

const fwMvRequestId CTaskMountThrowProjectile::ms_StartAim("Aim",0xB01E2F36);
const fwMvRequestId CTaskMountThrowProjectile::ms_StartIntro("Intro",0xda8f5f53);
const fwMvRequestId CTaskMountThrowProjectile::ms_StartThrow("Throw",0xCDEC4431);
const fwMvRequestId CTaskMountThrowProjectile::ms_Drop("Drop", 0x111e3fa4);
const fwMvRequestId CTaskMountThrowProjectile::ms_StartOutro("Outro",0x5D40B28D);
const fwMvRequestId CTaskMountThrowProjectile::ms_StartFinish("Finish",0xcb6420e3);
const fwMvRequestId CTaskMountThrowProjectile::ms_StartBlocked("Blocked",0x568F620C);
const fwMvFlagId	CTaskMountThrowProjectile::ms_UseSimpleThrow("UseSimpleThrow",0x608058FB);
const fwMvFlagId	CTaskMountThrowProjectile::ms_UseDrop("UseDrop",0x3B93191D);
const fwMvFlagId	CTaskMountThrowProjectile::ms_UseThreeAnimThrow("UseThreeAnimThrow",0x43093CA2);
const fwMvFlagId	CTaskMountThrowProjectile::ms_UseBreathing("UseBreathing",0xC1869601);
const fwMvFlagId	CTaskMountThrowProjectile::ms_FirstPersonGrip("FirstPersonGrip",0x1d93cda9);
const fwMvFloatId	CTaskMountThrowProjectile::ms_AimPhase("AimPhase",0xEF32268B);
const fwMvFloatId	CTaskMountThrowProjectile::ms_PitchPhase("PitchPhase",0xDA36894E);
const fwMvFloatId	CTaskMountThrowProjectile::ms_ThrowPhase("ThrowPhase",0x9D46658E);
const fwMvFloatId	CTaskMountThrowProjectile::ms_IntroPhase("IntroPhase",0xE8C2B9F8);
const fwMvFloatId	CTaskMountThrowProjectile::ms_DropIntroRate("DropIntroRate",0x2b24d4ea);
const fwMvBooleanId	CTaskMountThrowProjectile::ms_ThrowCompleteEvent("ThrowComplete",0x4BFCEDBF);
const fwMvBooleanId	CTaskMountThrowProjectile::ms_OutroFinishedId("OutroFinished",0x23DD7C9D);
const fwMvBooleanId	CTaskMountThrowProjectile::ms_OnEnterThrowId("OnEnterThrow",0x931B945E);
const fwMvBooleanId	CTaskMountThrowProjectile::ms_OnEnterAimId("OnEnterAim",0xA7C82EE);
const fwMvBooleanId	CTaskMountThrowProjectile::ms_DisableGripId("DisableGrip",0xab73010b);
const fwMvClipId	CTaskMountThrowProjectile::ms_FirstPersonGripClipId("FirstPersonGripClip",0xde570bb9);
const fwMvFilterId  CTaskMountThrowProjectile::ms_FirstPersonGripFilter("FirstPersonGripFilter", 0x6173c701);

const fwMvClipId	CTaskMountThrowProjectile::ms_VehMeleeIntroClipId("VehMeleeIntroClip",0x16CC5137);
const fwMvClipId	CTaskMountThrowProjectile::ms_VehMeleeHoldClipId("VehMeleeHoldClip",0xC3332209);
const fwMvClipId	CTaskMountThrowProjectile::ms_VehMeleeHitClipId("VehMeleeHitClip",0xB130327);
const fwMvFlagId	CTaskMountThrowProjectile::ms_UseVehMelee("UseVehMelee",0x71ECE428);
const fwMvFloatId	CTaskMountThrowProjectile::ms_VehMeleeIntroToHitBlendTime("VehMeleeIntroToHitBlendTime",0x6ED70047);
const fwMvFloatId	CTaskMountThrowProjectile::ms_VehMeleeIntroToHoldBlendTime("VehMeleeIntroToHoldBlendTime",0x313D4338);
const fwMvFloatId	CTaskMountThrowProjectile::ms_VehMeleeHitToIntroBlendTime("VehMeleeHitToIntroBlendTime",0x652B7D9F);
const fwMvFloatId	CTaskMountThrowProjectile::ms_HoldPhase("HoldPhase",0x8789E6B3);
const fwMvBooleanId CTaskMountThrowProjectile::ms_InterruptVehMeleeTag("Interrupt",0x6D6BC7B7);
const fwMvBooleanId	CTaskMountThrowProjectile::ms_OnEnterIntro("OnEnterIntro",0xE312777C);

phBoundComposite* CTaskMountThrowProjectile::sm_pTestBound = NULL;

static const u32	BOUND_MAX_STRIKE_CAPSULES				= 16;
static const float	BOUND_HAND_LENGTH_REDUCTION_AMOUNT		= 0.06f;
static const float	BOUND_ACCEPTABLE_DEPTH_ERROR_PORTION	= 0.3f;
static const u32	BOUND_MAX_NUM_SPHERES_ALONG_LENGTH		= 16;
static const float	PHASE_TO_CREATE_VEH_MELEE_WEAPON		= 0.3f;
static const float	PHASE_TO_DESTROY_VEH_MELEE_WEAPON		= 0.6f;

u8 CTaskMountThrowProjectile::m_nActionIDCounter = 0;

const float	CTaskMountThrowProjectile::ms_fBikeStationarySpeedThreshold = 3.0f;
const u8 CTaskMountThrowProjectile::ms_MaxDelay = 3; //number of frames to delay
const bool CTaskMountThrowProjectile::ms_AllowDelay = true;

CTaskMountThrowProjectile::BikeMeleeNetworkDamage CTaskMountThrowProjectile::sm_CachedMeleeDamage[CTaskMountThrowProjectile::s_MaxCachedMeleeDamagePackets];
u8 CTaskMountThrowProjectile::sm_uMeleeDamageCounter = 0;

void CTaskMountThrowProjectile::CacheMeleeNetworkDamage(CPed *pHitPed,
														CPed *pFiringEntity,
														const Vector3 &vWorldHitPos,
														s32 iComponent,
														float fDamage, 
														u32 flags, 
														u32 uParentMeleeActionResultID,
														u32 uForcedReactionDefinitionID,
														u16 uNetworkActionID,
														phMaterialMgr::Id materialId)
{
	taskAssert(sm_uMeleeDamageCounter < s_MaxCachedMeleeDamagePackets);

	//! Remove any existing melee reacts with this ID.
	for(int i = 0; i < s_MaxCachedMeleeDamagePackets; ++i)
	{
		CTaskMountThrowProjectile::BikeMeleeNetworkDamage &CachedDamage = sm_CachedMeleeDamage[i];
		if(CachedDamage.m_uNetworkActionID==uNetworkActionID)
		{
			CachedDamage.Reset();
		}
	}

	CTaskMountThrowProjectile::BikeMeleeNetworkDamage NewCachedDamage;

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
	NewCachedDamage.m_materialId = materialId;

	sm_CachedMeleeDamage[sm_uMeleeDamageCounter] = NewCachedDamage;

	++sm_uMeleeDamageCounter;
	if(sm_uMeleeDamageCounter >= s_MaxCachedMeleeDamagePackets)
		sm_uMeleeDamageCounter = 0;
}

CTaskMountThrowProjectile::BikeMeleeNetworkDamage *CTaskMountThrowProjectile::FindMeleeNetworkDamageForHitPed(CPed *pHitPed)
{
	for(int i = 0; i < s_MaxCachedMeleeDamagePackets; ++i)
	{
		CTaskMountThrowProjectile::BikeMeleeNetworkDamage &CachedDamage = sm_CachedMeleeDamage[i];
		if(CachedDamage.m_pHitPed == pHitPed && 
			CachedDamage.m_pInflictorPed == GetPed() && 
			CachedDamage.m_uNetworkActionID == m_nUniqueNetworkVehMeleeActionID )
		{
			return &sm_CachedMeleeDamage[i];
		}
	}
	return NULL;
}

CTaskMountThrowProjectile::CTaskMountThrowProjectile(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const CWeaponTarget& target, bool bRightSide, bool bVehicleMelee)
: CTaskAimGun(weaponControllerType, iFlags, 0, target)
, m_pVehicleDriveByInfo(NULL)
, m_bWantsToHold(true)
, m_bWantsToFire(false)
, m_bThrownGrenade(false)
, m_bWantsToCook(false)
, m_bOverAimingClockwise(false)
, m_bOverAimingCClockwise(false)
, m_bFlipping(false)
, m_bSimpleThrow(false)
, m_bDrop(false)
, m_bWaitingForDropRelease(false)
, m_fMinYaw(0)
, m_fMaxYaw(0)
, m_fCurrentHeading(-1.0f)
, m_fDesiredHeading(0.5f)
, m_fCurrentPitch(0.5f)
, m_fDesiredPitch(0.5f)
, m_fThrowPhase(0.5f)
, m_fCreatePhase(0.5f)
, m_vTrajectory(0.0f, 0.0f, 0.0f)
, m_iDelay(0)
, m_bUsingSecondaryTaskNetwork(false) 
, m_bCloneWaitingToThrow(false)
, m_cloneWaitingToThrowTimer(0)
, m_bWindowSmashed(false)
, m_bCloneWindowSmashed(false)
, m_bCloneThrowComplete(false)
, m_bHadValidWeapon(false)
, m_bNeedsToOpenDoors(false)
, m_bDisableGripFromThrow(false)
, m_bEquippedWeaponChanged(false)
, m_bWantsToDrop(false)
, m_bVehMelee(false)
, m_nHitEntityCount(0)
, m_fIntroToThrowTransitionDuration(-1.0f)
, m_bUseUpperBodyFilter(false)
, m_matLastKnownWeapon(Matrix34::IdentityType)
, m_bLastKnowWeaponMatCached(false)
, m_bInterruptible(false)
, m_fThrowIkBlendInStartPhase(-1.0f)
, m_bVehMeleeRestarting(false)
, m_bInterruptingWithOpposite(false)
, m_bRestartingWithOppositeSide(false)
, m_bVehMeleeStartingRightSide(bRightSide)
, m_bHaveAlreadySetPlayerInfo(false)
, m_nUniqueNetworkVehMeleeActionID(0)
, m_bStartedByVehicleMelee(bVehicleMelee)
#if FPS_MODE_SUPPORTED
, m_pFirstPersonIkHelper(NULL)
#endif // FPS_MODE_SUPPORTED
{
	SetInternalTaskType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE);
}

void CTaskMountThrowProjectile::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const CPed *pPed = GetPed(); //Get the ped ptr.

	//Request an aim camera on mounts only. Vehicle aim cameras for thrown projectiles are handled specially by the gameplay director.
	if (pPed->IsLocalPlayer() && ((pPed->GetMyMount() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount )) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack)))
	{
		//Inhibit aim cameras during long and medium Switch transitions.
		if(pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA) ||
			(g_PlayerSwitch.IsActive() && (g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT)))
		{
			return;
		}

		const CControl *pControl = pPed->GetControlFromPlayer();
		if (pControl->GetPedTargetIsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD))
		{
			const CVehicleDriveByInfo* pDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
			if (pDriveByInfo)
			{
				u32 cameraHash = pDriveByInfo->GetDriveByCamera().GetHash();
				if (cameraHash)
				{
					settings.m_CameraHash = cameraHash;
				}
			}
		}
	}
}

void CTaskMountThrowProjectile::RequestSpineAdditiveBlendIn(float fBlendDuration)
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SetFlag(true, CTaskAimGunVehicleDriveBy::ms_UseSpineAdditiveFlagId);
		m_MoveNetworkHelper.SetFloat(CTaskAimGunVehicleDriveBy::ms_SpineAdditiveBlendDurationId, fBlendDuration);
	}
}

void CTaskMountThrowProjectile::RequestSpineAdditiveBlendOut(float fBlendDuration)
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SetFlag(false, CTaskAimGunVehicleDriveBy::ms_UseSpineAdditiveFlagId);
		m_MoveNetworkHelper.SetFloat(CTaskAimGunVehicleDriveBy::ms_SpineAdditiveBlendDurationId, fBlendDuration);
	}
}

void CTaskMountThrowProjectile::HitImpulseCalculation(Vector3& vImpulseDir, float& fImpulseMag, const CPed& rHitPed, const CPed& rFiringPed )
{
	const fwTransform& rHitPedTransform = rHitPed.GetTransform();
	const Vec3V& vHitPedPos = rHitPedTransform.GetPosition();

	const fwTransform& rFiringPedTransform = rFiringPed.GetTransform();
	const Vec3V& vFiringPedPos = rFiringPedTransform.GetPosition();

	// Let's hack completely the direction of the impact
	if( const CWeaponInfo* pEquippedWeapInfo = rFiringPed.GetWeaponManager()->GetEquippedWeaponInfo() )
	{
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fSpeedToApplyMaxImpulse, 10.0f, 0.0f, LARGE_FLOAT, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fSpeedToApplyMinImpulse, 0.1f, 0.0f, LARGE_FLOAT, 0.01f );

		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fImpulseX, 0.455f, 0.0f, 1.0f, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fImpulseY, 1.0f, 0.0f, 1.0f, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fImpulseZ, 0.293f, 0.0f, 1.0f, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fSidewaysImpulseX, 1.0f, 0.0f, 1.0f, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fSidewaysImpulseY, 0.408f, 0.0f, 1.0f, 0.01f );

		// For weapons that hit forward
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fMaxImpulseMagForwards, 500.0f, 0.0f, LARGE_FLOAT, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fMinImpulseMagForwards, 150.0f, 0.0f, LARGE_FLOAT, 0.01f );
		// For weapons that hit backward
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fMaxImpulseMagBackwards, 335.0f, 0.0f, LARGE_FLOAT, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fMinImpulseMagBackwards, 200.0f, 0.0f, LARGE_FLOAT, 0.01f );
		// For kicks or weapons that hit sideways
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fMaxImpulseMagSideways, 375.0f, 0.0f, LARGE_FLOAT, 0.01f );
		TUNE_GROUP_FLOAT( VEHICLE_MELEE, fMinImpulseMagSideways, 150.0f, 0.0f, LARGE_FLOAT, 0.01f );

		// Check if the target is on the right or left of the ped
		const Vec3V vFiringPedToHitPed = vHitPedPos - vFiringPedPos;
		const Vec3V vFiringPedFwd = rFiringPedTransform.GetForward();
		const bool bTargetIsOnRight = Cross(vFiringPedToHitPed, vFiringPedFwd).GetZf() >= 0.0f;

		// Change the impulse vector based on where the target is and the kind of weapon we are going to use
		// some swing back, some forwards, and some cause to kick
		Vec3V vLocalImpulseDir(bTargetIsOnRight ? fImpulseX : -fImpulseX, fImpulseY, fImpulseZ);
		
		if( pEquippedWeapInfo->GetIsUnarmed() || pEquippedWeapInfo->GetIsThrownWeapon() )
		{
			vLocalImpulseDir.SetX( bTargetIsOnRight ? fSidewaysImpulseX : -fSidewaysImpulseX );
			vLocalImpulseDir.SetY( fSidewaysImpulseY );
		}
		else if( !pEquippedWeapInfo->GetCanBeUsedInVehicleMelee() )
		{
			vLocalImpulseDir.SetY( -fImpulseY );
		}

		vLocalImpulseDir = Normalize(vLocalImpulseDir);

		// Make the impulse a global direction
		const Vec3V vGlobalImpulseDir = rFiringPedTransform.Transform3x3(vLocalImpulseDir);
		vImpulseDir = VEC3V_TO_VECTOR3(vGlobalImpulseDir);

		// Calculate the speed difference
		const Vector3& vFiringPedVel = rFiringPed.GetMyVehicle() ? rFiringPed.GetMyVehicle()->GetVelocity() : rFiringPed.GetVelocity();
		const Vector3& vHitPedVel = rHitPed.GetMyVehicle() ? rHitPed.GetMyVehicle()->GetVelocity() : rHitPed.GetVelocity();

		const float fFiringPedSqrdSpeed = vFiringPedVel.Mag2();
		const float fHitPedSqrdSpeed = vHitPedVel.Mag2();

		float fSpeedDiff = 0.0f;
		if( fFiringPedSqrdSpeed > fHitPedSqrdSpeed )
		{
			const float fFiringPedSpeed = sqrtf(fFiringPedSqrdSpeed);
			const float fHitPedSpeed = sqrtf(fHitPedSqrdSpeed);

			fSpeedDiff = fFiringPedSpeed - fHitPedSpeed;
		}

		// Normalise between 0 and 1 the speed diff
		const float fDelta = fSpeedToApplyMaxImpulse - fSpeedToApplyMinImpulse;
		const float fRelative = fSpeedDiff - fSpeedToApplyMinImpulse;
		const float fNormalizedSpeed = ( ( fDelta != 0.0f ) && ( fRelative != 0.0f ) ) ? Clamp ( fRelative / fDelta, 0.0f, 1.0f ): 0.0f;

		// Now scale the impulse magnitude
		if( pEquippedWeapInfo->GetIsUnarmed() || pEquippedWeapInfo->GetIsThrownWeapon() )
			fImpulseMag = fMinImpulseMagSideways + ((fMaxImpulseMagSideways - fMinImpulseMagSideways) * fNormalizedSpeed);
		else if( !pEquippedWeapInfo->GetCanBeUsedInVehicleMelee() )
			fImpulseMag = fMinImpulseMagBackwards + ((fMaxImpulseMagBackwards - fMinImpulseMagBackwards) * fNormalizedSpeed);
		else
			fImpulseMag = fMinImpulseMagForwards + ((fMaxImpulseMagForwards - fMinImpulseMagForwards) * fNormalizedSpeed);

		// Render stuff
#if DEBUG_DRAW
		TUNE_GROUP_BOOL( VEHICLE_MELEE_DEBUG, bDrawImpulseVector, false );
		TUNE_GROUP_BOOL( VEHICLE_MELEE_DEBUG, bDrawHitInfo, false );

		static const s32 iTimeToLiveInFrames = 150;

		const Vec3V vImpulseStart = vFiringPedPos;
		if( bDrawImpulseVector )
		{
			const Vec3V vImpulseEnd = vImpulseStart + vGlobalImpulseDir;
			grcDebugDraw::Capsule(vImpulseStart, vImpulseEnd, 0.04f, Color_red, false, iTimeToLiveInFrames);
		}

		if( bDrawHitInfo )
		{
			const s32 BUFFER_SIZE = 64;
			char text[BUFFER_SIZE];
			int iNbLines = 0;

			sprintf( text, "Speed Difference = %.3f", fSpeedDiff );
			grcDebugDraw::Text(vImpulseStart, Color_blue, 0, grcDebugDraw::GetScreenSpaceTextHeight() * iNbLines++, text, true, iTimeToLiveInFrames );
			sprintf( text, "Impulse Magnitude = %.3f", fImpulseMag );
			grcDebugDraw::Text(vImpulseStart, Color_blue, 0, grcDebugDraw::GetScreenSpaceTextHeight() * iNbLines++, text, true, iTimeToLiveInFrames );
		}
#endif
	}
}

#if !__FINAL
const char * CTaskMountThrowProjectile::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Init :			return "State_Init";
		case State_StreamingClips : return "State_StreamingAnims";				
		case State_Intro :			return "State_Intro";
		case State_OpenDoor :		return "State_OpenDoor";
		case State_SmashWindow :	return "State_SmashWindow";
		case State_Hold :			return "State_Hold";
		case State_Throw :			return "State_Throw";
		case State_DelayedThrow:	return "State_DelayedThrow";
		case State_Outro :			return "State_Outro";
		case State_Finish :			return "State_Finish";
		default: taskAssertf(0, "Missing State Name in CTaskMountThrowProjectile");
	}

	return "State_Invalid";
}
#endif

CTask::FSM_Return CTaskMountThrowProjectile::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	bool bVehicleMelee = GetIsPerformingVehicleMelee();
	if (bVehicleMelee)
	{
		if (!m_bHaveAlreadySetPlayerInfo && pPed->GetPlayerInfo())
		{
			pPed->GetPlayerInfo()->SetVehMeleePerformingRightHit(m_bVehMeleeStartingRightSide);
			m_bHaveAlreadySetPlayerInfo = true;
		}

		if (GetState() == State_Throw && m_MoveNetworkHelper.IsNetworkActive() && m_MoveNetworkHelper.GetBoolean(ms_InterruptVehMeleeTag))
			m_bInterruptible = true;

		if (pPed->IsLocalPlayer())
		{
			CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl)
			{
				pControl->DisableInput(INPUT_VEH_HANDBRAKE);

				if (pControl->GetVehMeleeLeft().IsDown())
				{
					pControl->SetInputExclusive(INPUT_VEH_MELEE_LEFT);
				}

				if (pControl->GetVehMeleeRight().IsDown())
				{
					pControl->SetInputExclusive(INPUT_VEH_MELEE_RIGHT);
				}			
			}
		}

	}

	// Check for fire being released
	eTaskInput eDesiredInput = Input_None;
	if (pPed->IsLocalPlayer())
		eDesiredInput = GetInput(pPed);

	if (eDesiredInput == Input_Fire)
	{
		m_bWantsToFire=m_bWantsToHold=m_bWantsToCook=true;	
	} 
	else if(eDesiredInput == Input_Hold)
	{
		m_bWantsToHold = true;
		m_bWantsToFire = false;
	} 
	else if(eDesiredInput == Input_Cook)
	{
		m_bWantsToHold = true;
		m_bWantsToFire = false;
		m_bWantsToCook = true;
	} 
	else
	{
		m_bWantsToFire = m_bWantsToHold = false;
	}

	UpdateTarget(pPed);
	// Ped not mounted
// 	if (!pPed->GetMyMount()) bikes now too
// 	{
// 		return FSM_Quit;
// 	}	

	
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsAiming, !bVehicleMelee );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsDoingDriveby, !bVehicleMelee );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );

	weaponAssert(pPed->GetWeaponManager());
	bool bCooking = pPed->GetWeaponManager()->GetEquippedWeapon() && pPed->GetWeaponManager()->GetEquippedWeapon()->GetIsCooking();

	if (bCooking || (GetState() == State_Throw && !m_bThrownGrenade))
	{
		// Block weapon switching while this task is firing
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
	}

	//! Block camera transition in certain states.
	switch(GetState())
	{
	case(State_Init):
	case(State_StreamingClips):
	case(State_Outro):
#if FPS_MODE_SUPPORTED
		if(pPed->IsLocalPlayer() && pPed->GetPlayerInfo())
		{
			pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		}
#endif // FPS_MODE_SUPPORTED
		break;
	default:
		break;
	}

#if FPS_MODE_SUPPORTED
	if(IsStateValidForFPSIK())
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostCamera, true );
	}

	//! Block camera transition in certain states.
	switch(GetState())
	{
	case(State_Intro):
	case(State_Hold):
	case(State_StreamingClips):
	case(State_Init):
		//! Allow transition in these states.
		break;
	default:
		if(pPed->IsLocalPlayer() && pPed->GetPlayerInfo())
		{
			pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		}
		break;
	}
#endif

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged) && GetTimeRunning() > 0.0f)
	{
		m_bEquippedWeaponChanged = true;
	}
	
	return FSM_Continue;
}

void CTaskMountThrowProjectile::UpdateTarget(CPed* pPed)
{
	if(pPed && pPed->IsPlayer())
	{
		weaponAssert(pPed->GetWeaponManager());
		if(pPed->IsNetworkClone())
		{
			if(static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->HasValidTarget())
			{
				m_target.SetPosition(pPed->GetWeaponManager()->GetWeaponAimPosition());
			}
		}
		else
		{
			m_target.Update(pPed);

			Vector3 vTargetPosition;
			if(m_target.GetIsValid() && m_target.GetPositionWithFiringOffsets(pPed, vTargetPosition))
			{
				pPed->GetWeaponManager()->SetWeaponAimPosition(vTargetPosition);
			}
		}
	}
}


CTaskMountThrowProjectile::FSM_Return CTaskMountThrowProjectile::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Init)
		FSM_OnUpdate
			return Init_OnUpdate(pPed);
	FSM_State(State_StreamingClips)
		FSM_OnUpdate
			return StreamingClips_OnUpdate();	
	FSM_State(State_Intro)
		FSM_OnEnter
			Intro_OnEnter(pPed);
		FSM_OnUpdate
			return Intro_OnUpdate(pPed);
	FSM_State(State_OpenDoor)			
		FSM_OnUpdate
			OpenDoor_OnUpdate(pPed);
	FSM_State(State_SmashWindow)
		FSM_OnEnter
			SmashWindow_OnEnter();
		FSM_OnUpdate
			SmashWindow_OnUpdate();
	FSM_State(State_Hold)	
		FSM_OnEnter
			Hold_OnEnter();	
		FSM_OnUpdate
			return Hold_OnUpdate(pPed);
	FSM_State(State_Throw)	
		FSM_OnEnter
			Throw_OnEnter();
		FSM_OnUpdate
			return Throw_OnUpdate(pPed);
		FSM_OnExit
			Throw_OnExit();
	FSM_State(State_DelayedThrow)
		FSM_OnEnter
			DelayedThrow_OnEnter();
		FSM_OnUpdate
			return DelayedThrow_OnUpdate();
	FSM_State(State_Outro)		
		FSM_OnEnter
			Outro_OnEnter();
		FSM_OnUpdate
			return Outro_OnUpdate();
	FSM_State(State_Finish)
		FSM_OnEnter
			Finish_OnEnter();
		FSM_OnUpdate
			return Finish_OnUpdate(pPed);
FSM_End
}

CTaskInfo* CTaskMountThrowProjectile::CreateQueriableState() const
{
	bool bCurrentSideRight = (GetPed() && GetPed()->GetPlayerInfo()) ? GetPed()->GetPlayerInfo()->GetVehMeleePerformingRightHit() : false;
	return rage_new CClonedMountThrowProjectileInfo(GetState(),
													m_iFlags,
													m_target,
													m_bDrop,
													m_bWindowSmashed,
													m_bRestartingWithOppositeSide,
													bCurrentSideRight,
													m_bVehMeleeStartingRightSide,
													m_nUniqueNetworkVehMeleeActionID,
													m_bStartedByVehicleMelee);
}

void CTaskMountThrowProjectile::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CClonedMountThrowProjectileInfo *pThrowProjectileInfo = static_cast<CClonedMountThrowProjectileInfo*>(pTaskInfo);

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	m_bDrop = pThrowProjectileInfo->GetDrop();
	m_bWindowSmashed = pThrowProjectileInfo->GetWindowSmashed();
	m_bRestartingWithOppositeSide = pThrowProjectileInfo->GetInteruptingHoldWithOtherSide();
	// We dont sync m_bVehMeleeStartingRightSide every frame for a reason. Only want it to sync on task creation
	m_bOwnersCurrentSideRight = pThrowProjectileInfo->GetIsCurrentSwipeSideRight();

	if (m_nUniqueNetworkVehMeleeActionID != pThrowProjectileInfo->GetUniqueNetworkVehMeleeActionID() && !IsRunningLocally())
		m_nUniqueNetworkVehMeleeActionID = pThrowProjectileInfo->GetUniqueNetworkVehMeleeActionID();

	// m_bStartedByVehicleMelee only synced in the constructor
}

aiTask::FSM_Return CTaskMountThrowProjectile::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	bool bVehMelee = GetIsPerformingVehicleMelee();

	if (iEvent==OnUpdate)
	{
		if (iState == State_DelayedThrow || iState == State_Hold || (bVehMelee && iState == State_Throw)) // these states can be interrupted. All others must complete.
		{
			SetNextStateFromNetwork();
		}
	}

	return UpdateFSM(iState, iEvent);
}

//Ensure the state is terminated on the remote if it is no longer running on the local
void CTaskMountThrowProjectile::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork( State_Finish );
}

bool CTaskMountThrowProjectile::IsInScope(const CPed* pPed)
{
	if (!pPed->GetIsInVehicle() && !pPed->GetIsOnMount() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		return false;
	}

	return CTaskFSMClone::IsInScope(pPed);
}

void CTaskMountThrowProjectile::UpdateFPSMoVESignals()
{
	//! set grip anims
	static fwMvClipId RightHandGripClip("DRIVEBY_GRIP_RIGHT",0x8a52c60d);
	static fwMvClipId LeftHandGripClip("DRIVEBY_GRIP_LEFT",0xc6d8d25b);
	
	if(m_GripClipSetId != CLIP_SET_ID_INVALID)
	{
		fwMvClipId gripClipId = m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetLeftHandedProjctiles() ? LeftHandGripClip : RightHandGripClip;
		crClip* pGripClip = fwClipSetManager::GetClip(m_GripClipSetId, gripClipId);
		if(pGripClip)
		{
			m_MoveNetworkHelper.SetClip(pGripClip, ms_FirstPersonGripClipId);
		}
	}

	if (GetIsPerformingVehicleMelee() && m_iClipSet != CLIP_SET_ID_INVALID && GetPed()->GetPlayerInfo())
	{
		static fwMvClipId VehMeleeLIntroClip("MELEE_INTRO_L",0x75B3AA8B);
		static fwMvClipId VehMeleeLHoldClip("MELEE_IDLE_L",0x6FE95F5D);
		static fwMvClipId VehMeleeLHitClip("MELEE_L",0x6F8185A2);

		static fwMvClipId VehMeleeRIntroClip("MELEE_INTRO_R",0x57506DC5);
		static fwMvClipId VehMeleeRHoldClip("MELEE_IDLE_R",0x2A804DC);
		static fwMvClipId VehMeleeRHitClip("MELEE_R",0xDE2859);

		bool bRight = GetPed()->GetPlayerInfo()->GetVehMeleePerformingRightHit();

		crClip* pClip = fwClipSetManager::GetClip(m_iClipSet, bRight ? VehMeleeRIntroClip : VehMeleeLIntroClip);
		aiAssertf(pClip, "Couldn't find vehicle melee clip %s in the clipset %s.",
											bRight ? VehMeleeRIntroClip.TryGetCStr() : VehMeleeLIntroClip.TryGetCStr(),
											m_iClipSet.TryGetCStr());
		if (pClip)
			m_MoveNetworkHelper.SetClip(pClip, ms_VehMeleeIntroClipId);

		pClip = fwClipSetManager::GetClip(m_iClipSet, bRight ? VehMeleeRHoldClip : VehMeleeLHoldClip);
		aiAssertf(pClip, "Couldn't find vehicle melee clip %s in the clipset %s.",
											bRight ? VehMeleeRHoldClip.TryGetCStr() : VehMeleeLHoldClip.TryGetCStr(),
											m_iClipSet.TryGetCStr());
		if (pClip)
			m_MoveNetworkHelper.SetClip(pClip, ms_VehMeleeHoldClipId);

		pClip = fwClipSetManager::GetClip(m_iClipSet, bRight ? VehMeleeRHitClip : VehMeleeLHitClip);
		aiAssertf(pClip, "Couldn't find vehicle melee clip %s in the clipset %s.",
											bRight ? VehMeleeRHitClip.TryGetCStr() : VehMeleeLHitClip.TryGetCStr(),
											m_iClipSet.TryGetCStr());
		if (pClip)
		{
			m_MoveNetworkHelper.SetClip(pClip, ms_VehMeleeHitClipId);

			float fStopPhase;
			CClipEventTags::FindIkTags(pClip, m_fThrowIkBlendInStartPhase, fStopPhase, bRight, true);
		}
	}

	bool bFirstPersonGrip = false;
	switch(GetState())
	{
	case(State_Intro):
	case(State_Hold):
		bFirstPersonGrip = true;
		break;
	case(State_Throw):
		bFirstPersonGrip = !m_bDisableGripFromThrow;
		break;
	default:
		break;
	}

	m_MoveNetworkHelper.SetFlag(bFirstPersonGrip, ms_FirstPersonGrip);

	static fwMvFilterId RightHandGripFilter("Grip_R_Only_NO_IK_filter",0xfdbff2aa);
	static fwMvFilterId LeftHandGripFilter("Grip_L_Only_NO_IK_filter",0xb87d64a6);

	fwMvFilterId gripFilter = m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetLeftHandedProjctiles() ? LeftHandGripFilter : RightHandGripFilter;
	crFrameFilter* pFilterGrip = g_FrameFilterDictionaryStore.FindFrameFilter(gripFilter);
	taskAssert(pFilterGrip);
	m_MoveNetworkHelper.SetFilter(pFilterGrip, ms_FirstPersonGripFilter);
}

void CTaskMountThrowProjectile::RestartMoveNetwork(bool bUseUpperBodyFilter, bool bRestartNetwork)
{
	CPed* pPed = GetPed();

	fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskMountedThrow);
	if (!m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, networkDefId);
	}
	else if (bRestartNetwork)
	{
		m_MoveNetworkHelper.ReleaseNetworkPlayer();
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, networkDefId);
	}

	bool bVehMelee = GetIsPerformingVehicleMelee();

	if (!bRestartNetwork)
	{
		m_MoveNetworkHelper.SendRequest(ms_StartIntro);

		if (bVehMelee)
			m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterIntro);
	}

	fwMvFilterId filterId = FILTER_ID_INVALID;
	if (pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		filterId = FILTER_UPPERBODY;
	}
	if (pPed->GetMyVehicle())
	{
		if (bVehMelee)
		{
			filterId = bUseUpperBodyFilter ? FILTER_UPPERBODY : BONEMASK_ALL;
		}
		else if (m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetUseSpineAdditive())
		{
			bool bIsTrike = pPed->GetMyVehicle()->IsTrike();

			if (pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
			{
				filterId = CTaskAimGunVehicleDriveBy::sm_Tunables.m_BicycleDrivebyFilterId;
			}
			else if (pPed->GetMyVehicle()->InheritsFromBike() || bIsTrike)
			{
				filterId = CTaskAimGunVehicleDriveBy::sm_Tunables.m_BikeDrivebyFilterId;
			}
		}
		else if(pPed->GetMyVehicle()->GetIsJetSki())
		{
			filterId = CTaskAimGunVehicleDriveBy::sm_Tunables.m_JetskiDrivebyFilterId;
		}
	}

	m_bUsingSecondaryTaskNetwork = (pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack));

	if (!m_MoveNetworkHelper.IsNetworkAttached())
	{
		if(!m_bUsingSecondaryTaskNetwork)
		{
			pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION, CMovePed::Task_None, filterId);
		}
		else
		{
			pPed->GetMovePed().SetSecondaryTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION, CMovePed::Secondary_None, filterId);
		}
	}

	m_MoveNetworkHelper.SetClipSet(m_iClipSet);

	if (bRestartNetwork)
	{
		m_MoveNetworkHelper.SetFlag(true, ms_UseVehMelee);

		m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterAimId);
		m_MoveNetworkHelper.SendRequest(ms_StartAim);
		m_MoveNetworkHelper.SetFloat(ms_VehMeleeIntroToHoldBlendTime, 0.0f);
	}
}

void CTaskMountThrowProjectile::UpdateMoveFilter()
{
	CPed* pPed = GetPed();

	if (pPed && GetIsPerformingVehicleMelee() && m_MoveNetworkHelper.IsNetworkActive())
	{
		bool bPreviousFilter = m_bUseUpperBodyFilter;
		float fCurrentClipPhase = m_MoveNetworkHelper.GetFloat(ms_HoldPhase);

		m_bUseUpperBodyFilter = ShouldUseUpperBodyFilter(pPed);

		if (bPreviousFilter != m_bUseUpperBodyFilter)
		{
			RestartMoveNetwork(m_bUseUpperBodyFilter, true);
			UpdateFPSMoVESignals();
			m_MoveNetworkHelper.SetFloat(ms_HoldPhase, fCurrentClipPhase);
		}
	}
}

void CTaskMountThrowProjectile::ProcessMeleeCollisions( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessMeleeCollisions." );

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

				// Scale shapetest size according to your ped's speed
				TUNE_GROUP_FLOAT( VEHICLE_MELEE, fRadiusMultMin, 1.25f, 0.0f, 10000.0f, 0.1f );
				TUNE_GROUP_FLOAT( VEHICLE_MELEE, fRadiusMultMax, 1.5f, 0.0f, 10000.0f, 0.1f );

				TUNE_GROUP_FLOAT(VEHICLE_MELEE, fShapeTestSpeedMin, 1.0f, 0.0f, 100000.0f, 1.0f);
				TUNE_GROUP_FLOAT(VEHICLE_MELEE, fShapeTestSpeedMax, 15.0f, 0.0f, 100000.0f, 1.0f);

				Vector3 vPedVel = pPed->GetVehiclePedInside() ? pPed->GetVehiclePedInside()->GetVelocity() : pPed->GetVelocity();
				float fPedVelMag = vPedVel.Mag();
				fPedVelMag = Clamp(fPedVelMag,fShapeTestSpeedMin,fShapeTestSpeedMax);

				float fOldRange = fShapeTestSpeedMax - fShapeTestSpeedMin;
				float fNewRange = fRadiusMultMax - fRadiusMultMin;

				float fRadiusMult = (((fPedVelMag - fShapeTestSpeedMin) * fNewRange) / fOldRange) + fRadiusMultMin;

				const float	fCapsuleRadius = pCapsule->GetRadius() * fRadiusMult;
				Vector3 vCapsuleStart	= matCapsule * VEC3V_TO_VECTOR3( pCapsule->GetEndPointA() );
				Vector3 vCapsuleEnd		= matCapsule * VEC3V_TO_VECTOR3( pCapsule->GetEndPointB() );

				TUNE_GROUP_BOOL(VEHICLE_MELEE, bScaleTestPos, true);
				if (bScaleTestPos && pPed->GetVehiclePedInside())
				{
					Vector3 vPedVel = pPed->GetVehiclePedInside()->GetVelocity();
					ScalarV scMag(vPedVel.Mag());
					vPedVel.NormalizeFast();

					vCapsuleStart	+= vPedVel * scMag.Getf() * fwTimer::GetTimeStep();
					vCapsuleEnd		+= vPedVel * scMag.Getf() * fwTimer::GetTimeStep();
				}

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
			CEntity* pTargetEntity = NULL; // GetTargetEntity();

			////! If target entity is NULL, always hit hard lock target.
			//if(pPed->GetPlayerInfo() && !pTargetEntity)
			//{
			//	CPlayerPedTargeting& rTargeting = pPed->GetPlayerInfo()->GetTargeting();
			//	pTargetEntity = rTargeting.GetLockOnTarget();
			//}

			//if( pTargetEntity )
			//{
			//	vDesiredDirection = pTargetEntity->GetTransform().GetPosition() - vPedPosition;
			//	vDesiredDirection.SetZf( 0.0f );
			//	vDesiredDirection = Normalize( vDesiredDirection );
			//}
			//// As a backup lets use the desired direction
			//else
			{
				float fDesiredHeading = fwAngle::LimitRadianAngleSafe( pPed->GetDesiredHeading() );
				vDesiredDirection.SetXf( -rage::Sinf( fDesiredHeading ) );
				vDesiredDirection.SetYf( rage::Cosf( fDesiredHeading ) );
			}
			Vec3V vDirectionToPosition( V_ZERO );

			bool bShouldIgnoreHitDotThrehold = true;//m_pActionResult->ShouldIgnoreHitDotThreshold();

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
							CTaskMountThrowProjectile::BikeMeleeNetworkDamage *pNetworkDamage = FindMeleeNetworkDamageForHitPed(pHitPed);
							if(!pNetworkDamage)
								continue;
						}

						// Some peds have melee disabled entirely
						if( pHitPed->GetPedResetFlag( CPED_RESET_FLAG_DisableMeleeHitReactions ) )
							continue;

						// Should we ignore friendly attacks?
						if( CActionManager::ArePedsFriendlyWithEachOther( pPed, pHitPed ) )
						{
							continue;
						}

						//CTaskMeleeActionResult* pHitPedCurrentMeleeActionResultTask = pHitPed->GetPedIntelligence() ? pHitPed->GetPedIntelligence()->GetTaskMeleeActionResult() : NULL;
						//if( pHitPedCurrentMeleeActionResultTask )
						//{
						//	// Invulnerable?
						//	if( pHitPedCurrentMeleeActionResultTask->IsInvulnerableToMelee() )
						//		continue;

						//	// Skip if we currently do not allow interrupts and the current offensive action has a greater priority
						//	bool bAllowInterrupt = pHitPedCurrentMeleeActionResultTask->ShouldAllowInterrupt() || 
						//						   pHitPedCurrentMeleeActionResultTask->HasJustStruckSomething();
						//	bool bGreaterPriority = pHitPedCurrentMeleeActionResultTask->GetResultPriority() > GetResultPriority();
						//	if( pHitPedCurrentMeleeActionResultTask->IsAStandardAttack() && ( bGreaterPriority && !bAllowInterrupt ) )
						//		continue;
						//}

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
						pPed->GetPedAudioEntity()->PlayBikeMeleeCombatHit(*sortedCollisionResults[i].m_pResult);

					// Don't register hits for clones, weapon damage code will do that
					if (!pPed->IsNetworkClone())
					{
						// Add the entity to the list of ones already hit.
						AddToHitEntityCount(pHitEntity);
					}

					// Generate a small amount of pad rumble and camera shake
					if(pPed->IsLocalPlayer())
					{
						TUNE_GROUP_INT(VEHICLE_MELEE, PAD_RUMBLE_DURATION, 100, 0, 1000, 1);
						TUNE_GROUP_FLOAT(VEHICLE_MELEE, PAD_RUMBLE_INTENSITY, 0.8f, 0.0f, 1.0f, 0.01f);
						CControlMgr::StartPlayerPadShakeByIntensity(PAD_RUMBLE_DURATION, PAD_RUMBLE_INTENSITY);

						TUNE_GROUP_FLOAT(VEHICLE_MELEE, CAMERA_SHAKE_INTENSITY, 0.25f, 0.0f, 1.0f, 0.01f);
						if (pPed->GetVehiclePedInside())
						{
							float fSpeed = pPed->GetVehiclePedInside()->GetVelocity().Mag();
							fSpeed = Clamp(fSpeed / 25.0f, 0.0f, 1.0f);
							float fIntensity = Lerp(fSpeed, 0.0f, CAMERA_SHAKE_INTENSITY);
							camInterface::GetGameplayDirector().Shake("GRENADE_EXPLOSION_SHAKE", fIntensity);
						}
					}

					// Mark that a hit has occurred.
					//m_bHasJustStruckSomething = true;
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

phBound* CTaskMountThrowProjectile::GetCustomHitTestBound( CPed* pPed )
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::GetCustomHitTestBound." );

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

	u32 nIndexFound = 0;
	const CStrikeBoneSet* pStrikeBoneSet = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() ?
		ACTIONMGR.FindStrikeBoneSet( nIndexFound, atHashString("SB_right_arm_melee_weapon",0xD8A40ADC) ) :
		ACTIONMGR.FindStrikeBoneSet( nIndexFound, atHashString("SB_left_arm_melee_weapon",0xFE136F8B) );

	if (pPed->GetWeaponManager())
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if (pWeaponInfo && (pWeaponInfo->GetIsUnarmed() || pWeaponInfo->GetIsThrownWeapon()))
		{
			pStrikeBoneSet = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() ? 
				ACTIONMGR.FindStrikeBoneSet( nIndexFound, atHashString("SB_right_leg",0xD7A07454) ) :
				ACTIONMGR.FindStrikeBoneSet( nIndexFound, atHashString("SB_left_leg",0x59AFB7A4) );
		}
	}

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
				//if( strikingBoneTag != BONETAG_R_HAND )
				//	break;

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

void CTaskMountThrowProjectile::TriggerHitReaction( CPed* pPed, CEntity* pHitEnt, sCollisionInfo& refResult, bool bEntityAlreadyHit, bool bBreakableObject )
{
#if __ASSERT
	bool bWasUsingRagdoll = pPed->GetUsingRagdoll();
#endif

	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::TriggerHitReaction." );
	Assertf( pHitEnt, "pHitEnt is null in CTaskMeleeActionResult::TriggerHitReaction." );
	Assertf( refResult.m_pResult->GetHitDetected(), "Invalid hit-point info in CTaskMeleeActionResult::TriggerHitReaction." );

	weaponDebugf3("CTaskMeleeActionResult::TriggerHitReaction pPed[%p][%s][%s] pHitEnt[%p][%s] bEntityAlreadyHit[%d] bBreakableObject[%d]",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",pHitEnt,pHitEnt ? pHitEnt->GetModelName() : "",bEntityAlreadyHit,bBreakableObject);

	
	WorldProbe::CShapeTestResults tempResults(refResult.m_pResult, 1);
	tempResults.Update();

	// If we used a custom pBound generated from the motion of the bones / weapon
	// then we can use that information to get a normal based on the motion of the strike.
	Vector3 vImpactNormal = refResult.m_pResult->GetHitNormal();
	Vector3 vHitForwardDirection( VEC3V_TO_VECTOR3(pPed->GetTransform().GetA()) );
	if (pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit())
	{
		vHitForwardDirection *= -1.0f;
	}

	if( pHitEnt->GetIsTypePed() )
	{
		if( refResult.m_vImpactDirection.IsNonZero() )
		{
			// Get the direction the capsule was facing, and use that as the normal.
			vImpactNormal = refResult.m_vImpactDirection;

			// check we're not going to apply an force back towards the attacking ped
			static bank_float sfCosMaxImpactAngle = Cosf(70.0f * DtoR);
			if( DotProduct( vHitForwardDirection, vImpactNormal ) < sfCosMaxImpactAngle )
				vImpactNormal = vHitForwardDirection;
		}
	}
	else if( vImpactNormal.Dot( vHitForwardDirection ) < 0.0f )
		vImpactNormal = -vImpactNormal;

	refResult.m_pResult->SetHitNormal( vImpactNormal );

	const bool bDoImpact = (bBreakableObject && !pHitEnt->GetIsTypeVehicle()) || !bEntityAlreadyHit;

	CPed* pTargetPed = NULL;
	if( refResult.m_pResult->GetHitDetected() && bDoImpact )
	{
		if (pHitEnt->GetIsTypePed())
		{
			pTargetPed = static_cast<CPed*>(pHitEnt);
		}

		// Default weapon to unarmed
		u32 uWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() :  pPed->GetDefaultUnarmedWeaponHash();
		CWeapon tempWeapon( uWeaponHash );
		bool bCauseFatalMeleeDamage = false;
		TUNE_GROUP_FLOAT(VEHICLE_MELEE, fDamageVsAi, 40.0f, 0.0f, 999.0f, 1.0f);
		TUNE_GROUP_FLOAT(VEHICLE_MELEE, fDamageVsPlayers, 100.0f, 0.0f, 999.0f, 1.0f)
		float fStrikeDamage = pTargetPed && pTargetPed->IsPlayer() ? fDamageVsPlayers : fDamageVsAi;

		if (pPed->IsPlayer())
		{
			TUNE_GROUP_FLOAT(VEHICLE_MELEE, fMaxExtraStrengthDamage, 15.0f, 0.0f, 999.0f, 1.0f)
			float fMaxMeleeDamage = fStrikeDamage + fMaxExtraStrengthDamage;
			float fStrengthValue = Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STRENGTH.GetStatId())) / 100.0f, 0.0f, 1.0f);
			fStrikeDamage = Lerp(fStrengthValue, fStrikeDamage, fMaxMeleeDamage);
		}

		fStrikeDamage *= pPed->GetAttackStrength();

		if(tempWeapon.GetWeaponInfo())
		{
			// Apply the weapon damage modifier
			fStrikeDamage *= tempWeapon.GetWeaponInfo()->GetWeaponDamageModifier();
		}

		CTaskMountThrowProjectile::BikeMeleeNetworkDamage *pNetworkDamage = NULL;

		bool bCanDamage = true;
		if(pTargetPed)
		{
			// Factor in damage flags and friendliness
			bCanDamage &= !pTargetPed->m_nPhysicalFlags.bNotDamagedByAnything && 
				( !CActionManager::ArePedsFriendlyWithEachOther( pPed, pTargetPed ) ||
				pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackFriendly ) );

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

				// Fatal damage if hit head
				eAnimBoneTag nHitBoneTag = pTargetPed->GetBoneTagFromRagdollComponent( iHitComponent );
				if( nHitBoneTag == BONETAG_HEAD && !pTargetPed->m_nPhysicalFlags.bNotDamagedByMelee
					NOTFINAL_ONLY( && !( pTargetPed->IsDebugInvincible() ) ) 
					)
				{
					bCauseFatalMeleeDamage = true;
				}

				// Some peds take fatal melee damage
				if (pTargetPed->GetPedHealthInfo() && pTargetPed->GetPedHealthInfo()->ShouldCheckForFatalMeleeCardinalAttack() &&
					!pTargetPed->PopTypeIsMission() && !pTargetPed->IsPlayer() )
				{
					bCauseFatalMeleeDamage = true;
				}
			}

			//CTaskMeleeActionResult* pTaskMeleeActionResult = static_cast<CTaskMeleeActionResult*>(pTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MELEE_ACTION_RESULT));
			//if( !pTargetPed->IsNetworkClone() && pTaskMeleeActionResult && pTaskMeleeActionResult->GetActionResult() && pTaskMeleeActionResult->GetActionResult()->ActivateRagdollOnCollision() )
			//{
			//	CEventSwitch2NM event( 10000, rage_new CTaskNMRelax( 1000, 10000 ) );
			//	pTargetPed->SetActivateRagdollOnCollisionEvent( event );
			//	pTargetPed->SetActivateRagdollOnCollision( true );
			//}

			if(GetPed()->IsNetworkClone())
				pNetworkDamage = FindMeleeNetworkDamageForHitPed(pTargetPed);
		}

		// Reset the strike damage when we cannot damage the entity
		if( !bCanDamage )
			fStrikeDamage = 0.0f;

		// We might not want to do the same damage to objects and vehicles as we do to peds
		CWeaponDamage::sDamageModifiers damModifiers;
		// Scale damage caused to vehicles from melee attacks down a bit.
		damModifiers.fVehDamageMult = CWeaponDamage::MELEE_VEHICLE_DAMAGE_MODIFIER;

		// Use the normal weapon impact code.
		fwFlags32 flags( CPedDamageCalculator::DF_MeleeDamage );

		flags.SetFlag( CPedDamageCalculator::DF_IgnoreArmor );

		if( bCauseFatalMeleeDamage )
			flags.SetFlag( CPedDamageCalculator::DF_FatalMeleeDamage );

		if( bEntityAlreadyHit )
			flags.SetFlag( CPedDamageCalculator::DF_SuppressImpactAudio );

		flags.SetFlag( CPedDamageCalculator::DF_ForceMeleeDamage );

		flags.SetFlag( CPedDamageCalculator::DF_VehicleMeleeHit );

		CWeaponDamage::sMeleeInfo meleeInfo;
		meleeInfo.uNetworkActionID = m_nUniqueNetworkVehMeleeActionID;

		bool bDoDamage = false;
		if(GetPed()->IsNetworkClone())
		{
			if(pNetworkDamage)
			{
				flags = pNetworkDamage->m_uFlags;
				flags.SetFlag( CPedDamageCalculator::DF_AllowCloneMeleeDamage );
				fStrikeDamage = pNetworkDamage->m_fDamage;
				meleeInfo.uNetworkActionID = pNetworkDamage->m_uNetworkActionID;

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
		//u32 nForcedReactionDefinitionID = INVALID_ACTIONDEFINITION_ID;
		if(bDoDamage)
		{
			//CTaskMelee::ResetLastFoundActionDefinitionForNetworkDamage();

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

			//const CActionDefinition* pMeleeActionToDo = CTaskMelee::GetLastFoundActionDefinitionForNetworkDamage();
			//if(pMeleeActionToDo)
			//{
			//	nForcedReactionDefinitionID = pMeleeActionToDo->GetID();
			//}
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
				0,
				m_nUniqueNetworkVehMeleeActionID,
				0, NULL, 1.f, 1.f, 1.f, &hitDirection);
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
// FUNCTION :	AddToHitEntityCount
// PURPOSE :	Add entity to hit list so we know if we have already hit it.
// PARAMETERS :	pHitEntity - the entity to add.
// RETURNS :	
/////////////////////////////////////////////////////////////////////////////////
void CTaskMountThrowProjectile::AddToHitEntityCount(CEntity* pEntity)
{
	if( m_nHitEntityCount < sm_nMaxHitEntities )
	{
		m_hitEntities[m_nHitEntityCount] = pEntity;
		++m_nHitEntityCount;
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
bool CTaskMountThrowProjectile::EntityAlreadyHitByResult( const CEntity* pHitEntity ) const
{
	// Check if the entity is already in the list (with
	// a simple linear search).
	for(u32 i = 0; i < m_nHitEntityCount; ++i)
	{
		if( pHitEntity == m_hitEntities[i] )
			return true;
	}

	return false;
}

bool CTaskMountThrowProjectile::ShouldUseUpperBodyFilter(CPed* pPed) const
{
	if (!pPed)
		return false;

	if (GetIsPerformingVehicleMelee())
	{
		// Full body for quads#
		CVehicle* pVeh = pPed->GetVehiclePedInside();
		if (pVeh)
		{
			bool bIsTrike = pVeh->IsTrike();
			if ((pVeh->InheritsFromQuadBike() || pVeh->InheritsFromAmphibiousQuadBike()) && !bIsTrike)
				return false;

			if (pVeh->GetDriver() != pPed)
				return false;
		}

		return !GetIsBikeMoving(*pPed);
	}

	return false;
}

bool CTaskMountThrowProjectile::ShouldDelayThrow() const
{
	if (!GetIsPerformingVehicleMelee())
	{
		return NetworkInterface::IsGameInProgress() && ms_AllowDelay;
	}
	else
	{
		return false;
	}
}

void CTaskMountThrowProjectile::CleanUpHitEntities()
{
	for(u32 i = 0; i < m_nHitEntityCount; ++i)
	{
		m_hitEntities[i] = NULL;
	}
	m_nHitEntityCount = 0;
}

bool CTaskMountThrowProjectile::GetIsBikeMoving(const CPed& rPed) const
{
	bool bStationary = rPed.GetVehiclePedInside() ? rPed.GetVehiclePedInside()->GetVelocity().Mag() < ms_fBikeStationarySpeedThreshold : false;
	bool bReversing = rPed.GetVehiclePedInside() ? rPed.GetVehiclePedInside()->GetThrottle() < -0.01f : false;

	return !bStationary && !bReversing;
}

bool CTaskMountThrowProjectile::ShouldCreateWeaponObject(const CPed& rPed) const
{
	if (rPed.GetWeaponManager() && rPed.GetWeaponManager()->GetEquippedWeaponInfo())
	{
		bool bEquippedThrown = rPed.GetWeaponManager()->GetEquippedWeaponInfo() && rPed.GetWeaponManager()->GetEquippedWeaponInfo()->GetIsThrownWeapon();
		bool bVehMelee = GetIsPerformingVehicleMelee();

		return !(bEquippedThrown && bVehMelee);
	}

	return true;
}

void CTaskMountThrowProjectile::ProcessCachedNetworkDamage(CPed* pPed)
{
	Assertf( pPed, "pPed is null in CTaskMeleeActionResult::ProcessCachedNetworkDamage." );

	//! If we have pending network damage events, process them now.
	for(int i = 0; i < s_MaxCachedMeleeDamagePackets; ++i)
	{
		CTaskMountThrowProjectile::BikeMeleeNetworkDamage &CachedDamage = sm_CachedMeleeDamage[i];

		if(CachedDamage.m_uNetworkActionID == m_nUniqueNetworkVehMeleeActionID)
		{
			if(CachedDamage.m_pHitPed &&
				(CachedDamage.m_pInflictorPed == GetPed()) && 
				!EntityAlreadyHitByResult( CachedDamage.m_pHitPed ) )
			{
				// url:bugstar:5524713 - Cache hit ped here for physics stuff, as CachedDamage looks to be be modified / reset
				CPed* pCachedHitPed = CachedDamage.m_pHitPed.Get();

				// Default weapon to unarmed
				u32 uWeaponHash = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponHash() : pPed->GetDefaultUnarmedWeaponHash();

				fwFlags32 flags( CachedDamage.m_uFlags );
				flags.SetFlag( CPedDamageCalculator::DF_AllowCloneMeleeDamage );

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

				WorldProbe::CShapeTestHitPoint shapeTestResult;

				phInst* pHitInst = pCachedHitPed->GetCurrentPhysicsInst();
				u16 nGenerationId = 0;
#if LEVELNEW_GENERATION_IDS
				if(pHitInst && pHitInst->IsInLevel())
				{
					nGenerationId = PHLEVEL->GetGenerationID(pHitInst->GetLevelIndex());
				}
#endif // LEVELNEW_GENERATION_IDS
				shapeTestResult.SetHitComponent((u16)CachedDamage.m_iComponent);
				shapeTestResult.SetHitInst(pHitInst->GetLevelIndex(),nGenerationId);
				shapeTestResult.SetHitPosition(CachedDamage.m_vWorldHitPos);
				shapeTestResult.SetHitMaterialId(CachedDamage.m_materialId);

				pPed->GetPedAudioEntity()->PlayBikeMeleeCombatHit(shapeTestResult);

				AddToHitEntityCount(pCachedHitPed);
			}

			sm_CachedMeleeDamage[i].Reset();
		}
	}
}

bool CTaskMountThrowProjectile::GetIsPerformingVehicleMelee() const
{
	const CPed* pPed = GetPed();

	if (pPed)
	{
		if (pPed->IsNetworkClone())
		{
			return m_bStartedByVehicleMelee;
		}
		else
		{
			return pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee);
		}
	}
	return false;
}

bool CTaskMountThrowProjectile::IsFirstPersonDriveBy() const
{
	const CPed* pPed = GetPed();

	if(pPed->IsInFirstPersonVehicleCamera())
	{
		return true;
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return true;
	}
#endif

	return false;
}

#if FPS_MODE_SUPPORTED
bool CTaskMountThrowProjectile::IsStateValidForFPSIK() const
{
	const CPed* pPed = GetPed();

	if(!IsFirstPersonDriveBy())
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if (!pWeaponInfo || (!pWeaponInfo->GetIsThrownWeapon() && !GetIsPerformingVehicleMelee()))
	{
		return false;
	}

	switch(GetState())
	{
	case(State_Intro):
	case(State_Hold):
	case(State_Outro):
		return true;
	default:
		break;
	}

	return false;
}
#endif

bool CTaskMountThrowProjectile::ProcessPostCamera()
{
#if FPS_MODE_SUPPORTED
	if (IsStateValidForFPSIK())
	{
		//! Detect a switch in IK Type, so we can restart FPS solver.
		CFirstPersonIkHelper::eFirstPersonType eIKType = GetPed()->IsInFirstPersonVehicleCamera() ? CFirstPersonIkHelper::FP_DriveBy : CFirstPersonIkHelper::FP_OnFootDriveBy;

		//! If we change types, just delete and create again. It's quite rare.
		if(m_pFirstPersonIkHelper && (m_pFirstPersonIkHelper->GetType() != eIKType) )
		{
			delete m_pFirstPersonIkHelper;
			m_pFirstPersonIkHelper = NULL;
		}

		if (m_pFirstPersonIkHelper == NULL)
		{
			m_pFirstPersonIkHelper = rage_new CFirstPersonIkHelper(eIKType);
			taskAssert(m_pFirstPersonIkHelper);

			CFirstPersonIkHelper::eArm armToIK = CFirstPersonIkHelper::FP_ArmRight;
			if(m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetLeftHandedProjctiles())
			{
				armToIK = CFirstPersonIkHelper::FP_ArmLeft;
			}

			if (GetIsPerformingVehicleMelee() && !GetPed()->GetPlayerInfo()->GetVehMeleePerformingRightHit())
			{
				armToIK = CFirstPersonIkHelper::FP_ArmLeft;
			}
			
			m_pFirstPersonIkHelper->SetArm(armToIK);
		}

		if (m_pFirstPersonIkHelper)
		{
			Vector3 vTargetOffset(Vector3::ZeroType);
			if(GetPed()->GetIsParachuting())
			{
				vTargetOffset = CTaskParachute::GetFirstPersonDrivebyIKOffset();
			}
			else if(GetPed()->GetVehiclePedInside())
			{
				CVehicle *pVehicle = GetPed()->GetVehiclePedInside();
				CVehicleModelInfo *pModelInfo = pVehicle->GetVehicleModelInfo();
				if(pModelInfo)
				{
					CFirstPersonIkHelper::eArm armToIK = CFirstPersonIkHelper::FP_ArmLeft;
					armToIK =  CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(GetPed())->GetLeftHandedFirstPersonAnims() ? CFirstPersonIkHelper::FP_ArmLeft : CFirstPersonIkHelper::FP_ArmRight;
					s32 iSeat = GetPed()->GetVehiclePedInside()->GetSeatManager()->GetPedsSeatIndex(GetPed());

					if (pVehicle->GetDriver()==GetPed())
					{
						vTargetOffset = pModelInfo->GetFirstPersonProjectileDriveByIKOffset();
					}
					else
					{
						if(armToIK == CFirstPersonIkHelper::FP_ArmRight)
						{
							vTargetOffset = pModelInfo->GetFirstPersonProjectileDriveByRearLeftIKOffset();
						}
						else if (armToIK == CFirstPersonIkHelper::FP_ArmLeft)
						{
							if (iSeat > 1)
							{
								vTargetOffset = pModelInfo->GetFirstPersonProjectileDriveByRearRightIKOffset();
							}
							else
							{
								vTargetOffset = pModelInfo->GetFirstPersonProjectileDriveByPassengerIKOffset();
							}
						}
					}					
					
#if __BANK			// Allow override of values for tweaking purposes
					TUNE_GROUP_BOOL(FIRST_PERSON_STEERING, bOverrideProjectileOffsetValues, false);
					TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fOverrideProjectileOffsetX, vTargetOffset.x, -10.0f, 10.0f, 0.01f);
					TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fOverrideProjectileOffsetY, vTargetOffset.y, -10.0f, 10.0f, 0.01f);
					TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fOverrideProjectileOffsetZ, vTargetOffset.z, -10.0f, 10.0f, 0.01f);
					if (bOverrideProjectileOffsetValues)
					{
						vTargetOffset = Vector3(fOverrideProjectileOffsetX, fOverrideProjectileOffsetY, fOverrideProjectileOffsetZ);
					}
#endif // __BANK
				}

				//! if dropping, don't allow FPS IK to pull arm around, but do lock it in place.
				if(m_bDrop)
				{
					m_pFirstPersonIkHelper->SetMinMaxDriveByYaw(-0.05f, 0.05f);

					float fDropRate = 1.0f;
					if(pVehicle->GetIsJetSki())
					{
						TUNE_GROUP_FLOAT(FIRST_PERSON_MOUNTED_THROW, fJetSkiDropAnimRate, 0.5f, 0.0f, 1.0f, 0.01f);
						TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fJetSkiProjectileOffsetY, 0.05f, 0.0f, 1.0f, 0.01f);
						fDropRate = fJetSkiDropAnimRate;
						vTargetOffset.y += fJetSkiProjectileOffsetY;
						m_pFirstPersonIkHelper->SetBlendInRate(ARMIK_BLEND_RATE_NORMAL);
					}
					else if(pVehicle->InheritsFromBike())
					{
						TUNE_GROUP_FLOAT(FIRST_PERSON_MOUNTED_THROW, fBikeDropAnimRate, 0.5f, 0.0f, 1.0f, 0.01f);
						TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fBikeProjectileOffsetY, 0.05f, 0.0f, 1.0f, 0.01f);
						m_pFirstPersonIkHelper->SetBlendInRate(ARMIK_BLEND_RATE_NORMAL);
						fDropRate = fBikeDropAnimRate;
						vTargetOffset.y += fBikeProjectileOffsetY;
					}

					m_MoveNetworkHelper.SetFloat(ms_DropIntroRate, fDropRate);
				}
			}

			m_pFirstPersonIkHelper->SetOffset(RCC_VEC3V(vTargetOffset));
			m_pFirstPersonIkHelper->Process(GetPed());
		}
	}
	else if (m_pFirstPersonIkHelper)
	{
		m_pFirstPersonIkHelper->Reset();
	}
#endif // FPS_MODE_SUPPORTED

	TUNE_GROUP_BOOL(VEHICLE_MELEE, ENABLE_FP_THROW_IK, false);
	if (ENABLE_FP_THROW_IK && GetPed()->IsLocalPlayer() && GetIsPerformingVehicleMelee() && GetState() == State_Throw && IsFirstPersonDriveBy())
	{
		bool bRightSwing = GetPed()->GetPlayerInfo()->GetVehMeleePerformingRightHit();
		crIKSolverArms::eArms eArmToIK = bRightSwing ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM;

		TUNE_GROUP_FLOAT(VEHICLE_MELEE, fZOffsetForFpMeleeWRT, 0.0f, -1000.0f, 1000.0f, 0.05f);
		const camCinematicMountedCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera();
		if(pCamera)
		{
			Vector3 vHandPos(Vector3::ZeroType);
			GetPed()->GetBonePosition(vHandPos, bRightSwing ? BONETAG_R_HAND : BONETAG_L_HAND);

			Vector3 vCamPos = pCamera->GetFrame().GetPosition();

			float fHeightDiff = vCamPos.GetZ() - vHandPos.GetZ();

			Vector3 vOffset(0.0f, 0.0f, -fHeightDiff + fZOffsetForFpMeleeWRT);

			GetPed()->GetIkManager().SetRelativeArmIKTarget( eArmToIK, 
				GetPed(),
				GetPed()->GetBoneIndexFromBoneTag(bRightSwing ? BONETAG_R_HAND : BONETAG_L_HAND),
				vOffset,
				AIK_TARGET_WRT_HANDBONE,
				PEDIK_ARMS_FADEIN_TIME_QUICK, 
				PEDIK_ARMS_FADEOUT_TIME_QUICK);
		}
	}

	return true;
}

bool CTaskMountThrowProjectile::ProcessPostPreRender()
{
	CPed* pPed = GetPed();

	// if this a network clone task running out of scope then we don't need to update here
	if(pPed && pPed->IsNetworkClone() && !IsInScope(pPed))
		return true;

	if (pPed && pPed->IsNetworkClone() && GetIsCloneInDamageWindow())
	{
		ProcessCachedNetworkDamage(pPed);
	}

	UpdateAiming(pPed, false, GetState()!=State_Intro);
	return true;
}

void CTaskMountThrowProjectile::UpdateAiming(CPed* pPed, bool popIt, bool bAllowAimTransition) {
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		// Calculate and send MoVE parameters

		//////////////////////////////////////
		// Calculate and send MoVE parameters
		//////////////////////////////////////

		// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
		Vector3 vStart, vEnd;
		if (!CalculateAimVector(pPed, vStart, vEnd))	// Based on camera direction
		{
			//Can't find my aim Vector, likely multiplayer throw
			weaponWarningf("Failed to find aim vector, see previous assert for more info");		
			m_fCurrentHeading = m_fDesiredHeading = 0.5f;
			m_MoveNetworkHelper.SetFloat(ms_AimPhase, m_fCurrentHeading);		
			m_fCurrentPitch=m_fDesiredPitch = 0.5f;
			m_MoveNetworkHelper.SetFloat(ms_PitchPhase, m_fCurrentPitch);			
			return;
		}

		Assert(vStart==vStart);
		Assert(vEnd==vEnd);

		///////////////////////////////
		//// COMPUTE AIM YAW SIGNAL ///
		///////////////////////////////

		if (GetState()!=State_Throw) //hold desired heading during throws
		{
			// Compute desired yaw angle value
			float fDesiredYaw = CTaskAimGun::CalculateDesiredYaw(pPed, vStart, vEnd);

			const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
			if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
			{
				//! Clamp animation if camera is looking a particular way past threshold.
				const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
				if(pMountedCamera)
				{
					bool bLookingLeft = pMountedCamera->IsLookingLeft();

					if(bLookingLeft && fDesiredYaw > HALF_PI)
					{
						fDesiredYaw = -PI;
					}
					else if(!bLookingLeft && fDesiredYaw < -HALF_PI)
					{
						fDesiredYaw = PI;
					}
				}
			}

			// Map the desired target heading to 0-1, depending on the range of the sweep
			float fYawSignal = CTaskAimGun::ConvertRadianToSignal(fDesiredYaw, m_fMinYaw, m_fMaxYaw, false);				

			// Cannot overshoot initially
			bool bInitialHeading = false;
			if (m_fCurrentHeading < 0.0f)
			{
				bInitialHeading = true;
			}

			// deal with having to aim at angles > or < PI without flipping direction (overshooting)
			if (m_bFlipping && bAllowAimTransition)
			{
				float fHeadingDelta = fabs(m_fDesiredHeading - m_fCurrentHeading);
				static dev_float s_fMinFlipDelta = 0.25f;
				if (fHeadingDelta < s_fMinFlipDelta)
				{
					m_MoveNetworkHelper.SendRequest(ms_StartAim);	
					UpdateFPSMoVESignals();
					m_bFlipping = false;
				}
			}

			if(!IsFirstPersonDriveBy())
			{
			bool bOverAiming = false;
			if (!bInitialHeading && ( (m_fDesiredHeading > 0.5f && fYawSignal < 0.5f) || (m_fDesiredHeading < 0.5f && fYawSignal > 0.5f) )) 
			{ //possible flip detected
				if (m_fMinYaw < -PI && fDesiredYaw >0) 
				{
					if ((m_fMinYaw+2.0f*PI) < fDesiredYaw) 
					{
						fYawSignal = CTaskAimGun::ConvertRadianToSignal(fDesiredYaw - 2.0f*PI, m_fMinYaw, m_fMaxYaw, false);
						m_bOverAimingCClockwise = true;
						m_bOverAimingClockwise = false;
						bOverAiming = true;
					}
				}				

				if (m_fMaxYaw > PI && fDesiredYaw <0) 
				{
					if ((m_fMaxYaw-2.0f*PI) > fDesiredYaw) 
					{
						fYawSignal = CTaskAimGun::ConvertRadianToSignal(fDesiredYaw + 2.0f*PI, m_fMinYaw, m_fMaxYaw, false);
						m_bOverAimingCClockwise = false;
						m_bOverAimingClockwise = true;
						bOverAiming = true;
					}
				}					
			}

			if (!bOverAiming && (GetState() == State_Hold)) 
			{
				if (m_bOverAimingClockwise)
				{
					if (fDesiredYaw < 0) 
					{	
						const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
						if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount) || (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL))
						{
							m_MoveNetworkHelper.SendRequest(ms_StartBlocked);				
							m_bFlipping = true;
						}
					}
					m_bOverAimingClockwise = false;
				} 
				else if (m_bOverAimingCClockwise)
				{
					if (fDesiredYaw > 0)
					{				
						const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
						if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount) || (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL))
						{
							m_MoveNetworkHelper.SendRequest(ms_StartBlocked);				
							m_bFlipping = true;
						}
					}
					m_bOverAimingCClockwise = false;
				} 
			}
			}

			//Clamp signal for throws (TODO Data drive this in driveby anim infos)
			if (fYawSignal >= 0.5f)
			{
				float t = (fYawSignal-0.5f)*2.0f;
				float clampRatio = m_fMaxYaw/PI;
				t = Min(clampRatio*t, 1.0f);
				fYawSignal = (t*0.5f + 0.5f); 
			} 
			else 
			{
				float t = 1.0f - fYawSignal*2.0f;
				float clampRatio = m_fMinYaw/-PI;
				t = Min(clampRatio*t, 1.0f);
				fYawSignal = (1.0f-t)*0.5f; 
			}
			m_fDesiredHeading = fYawSignal;
		}

		// The desired heading is smoothed so it doesn't jump too much in a timestep
		static float HeadingChangeRate = 2.0f; //TODO
		if (popIt)
			m_fCurrentHeading=m_fDesiredHeading;
		else
			m_fCurrentHeading = CTaskAimGun::SmoothInput(m_fCurrentHeading, m_fDesiredHeading, HeadingChangeRate); 

		// Send the heading signal
		m_MoveNetworkHelper.SetFloat(ms_AimPhase, m_fCurrentHeading);		

		//////////////////////////////////
		//// COMPUTE AIM PITCH SIGNAL ////
		//////////////////////////////////
		const float fSweepPitchMin = -QUARTER_PI; //TODO
		const float fSweepPitchMax = QUARTER_PI;

		// Compute desired pitch angle value
		float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vStart, vEnd);

		// Map the angle to the range 0.0->1.0
		m_fDesiredPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fSweepPitchMin, fSweepPitchMax, false);

		// Compute the final signal value
		static float PitchChangeRate = 1.0f; //TODO
		if (popIt)
			m_fCurrentPitch=m_fDesiredPitch;
		else
			m_fCurrentPitch = CTaskAimGun::SmoothInput(m_fCurrentPitch, m_fDesiredPitch, PitchChangeRate);

		// Send the signal
		m_MoveNetworkHelper.SetFloat(ms_PitchPhase, m_fCurrentPitch);
		//Displayf("AimPhase: %f, Desired: %f >>> PitchPhase: %f, Desired: %f", m_fCurrentHeading, fDesiredYaw*RtoD, m_fCurrentPitch, fDesiredPitch*RtoD);
	}
}

bool CTaskMountThrowProjectile::CalculateAimVector(CPed* pPed, Vector3& vStart, Vector3& vEnd)
{
	if(!pPed->IsLocalPlayer())
	{
		weaponAssert(pPed->GetWeaponManager());		
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon)
		{
			return CalculateFiringVector(pPed, vStart, vEnd);
		} 
		return false;		
	}
	else
	{
		weaponAssert(pPed->GetWeaponManager());

		float fRange = 0.0f;
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon)
		{
			fRange = pWeapon->GetRange();
		}

		CPed * pPlayer = CGameWorld::FindLocalPlayer();
		CControl *pControl = pPlayer->GetControlFromPlayer();
		if (pControl->GetPedTargetIsUp(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD))
		{
			CEntity* pLockOnTarget = pPlayer->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
			if (pLockOnTarget)
			{
				vStart = camInterface::GetPos();
				vEnd = VEC3V_TO_VECTOR3(pLockOnTarget->GetTransform().GetPosition());
				return true;
			}
		}

		const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
		vStart = aimFrame.GetPosition();
		Vector3	vecAim = aimFrame.GetFront();
		vecAim.Normalize();
		vEnd = vStart + (fRange * vecAim);
	}
	return true;
}

bool CTaskMountThrowProjectile::CheckForDrop(CPed* pPed)
{
	// no drops in 1st person - anims don't work well enough right now.
#if FPS_MODE_SUPPORTED
	TUNE_GROUP_BOOL(FIRST_PERSON_MOUNTED_THROW, bDisableDropIn1stPerson, false);
	if(pPed->IsInFirstPersonVehicleCamera() && bDisableDropIn1stPerson)
	{
		return false;
	}
#endif

	// B*2102892: Use drop timer/sweeps for player projectile drops
	TUNE_GROUP_BOOL(MOUNTED_THROW, bUseInputForDropInAltDriveby, true);
	if (bUseInputForDropInAltDriveby && pPed->IsPlayer())
	{
		//Check for drop
		if (pPed->GetIsInVehicle() && !pPed->IsNetworkClone())
		{
			if (pPed->GetMyVehicle()->GetDriver() == pPed)
			{ 
				const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();

					bool bHaveClips = (pClipSet->GetClip(fwMvClipId("Drop_Grenade_180L",0x5fb2f674)) != NULL
									&& pClipSet->GetClip(fwMvClipId("Drop_Grenade_90L",0x93b1f804)) != NULL
									&& pClipSet->GetClip(fwMvClipId("Drop_Grenade_0",0x752d7ee7)) != NULL
									&& pClipSet->GetClip(fwMvClipId("Drop_Grenade_90R",0x27481f32)) != NULL
									&& pClipSet->GetClip(fwMvClipId("Drop_Grenade_180R",0x006937e2)) != NULL);

					if (pClipSet && bHaveClips && m_bWantsToDrop)
					{
						m_bWantsToDrop = false;
						return true;
					}			
			}
		}
	}
	else
	{
		//Check for drop
		if (pPed->GetIsInVehicle() && !pPed->IsNetworkClone())
		{
			if (pPed->GetMyVehicle()->GetDriver() == pPed)
			{ 
				const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
				if ((pClipSet && pClipSet->GetClip(fwMvClipId("drop_grenade",0x13E88B52)) != NULL))
				{
					static dev_float s_fMaxDropSpeed = 6.0f * 6.0f;
					if ( pPed->GetMyVehicle()->GetVelocity().Mag2() > s_fMaxDropSpeed )
					{
						// check to see if this is placeable
						const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
						if(pWeaponInfo)
						{
							if (!pWeaponInfo->GetProjectileCanBePlaced())
								return true;					
						}					
					}
				}			
			}
		}
	}

	return false;
}

void CTaskMountThrowProjectile::SetNextStateFromNetwork()
{
	static const int MAX_TIME_TO_WAIT_FOR_PROJECTILE = 5000;

	s32 currentState = GetState();
	s32 networkState = GetStateFromNetwork();

	bool bVehMelee = GetIsPerformingVehicleMelee();

	if ((networkState != -1) && (networkState > State_Init) && (currentState != networkState))
	{
		if (m_bCloneWaitingToThrow)
		{
			m_cloneWaitingToThrowTimer += (u16)fwTimer::GetTimeStepInMilliseconds();
		}
		else
		{
			m_cloneWaitingToThrowTimer = 0;
		}

		if (networkState == State_Throw || networkState == State_DelayedThrow || m_bCloneWaitingToThrow)
		{
			m_bCloneWaitingToThrow = true;

			if (networkState == State_Finish && m_cloneWaitingToThrowTimer > MAX_TIME_TO_WAIT_FOR_PROJECTILE)
			{
				// the projectile is missing in action, quit
				SetState(State_Finish);
			}
			else if (networkState == State_Throw)
			{
				m_bCloneWaitingToThrow = false;
				SetState(State_Throw); //allow the state to be set from the network state
			}
			else if (currentState == State_Throw && networkState == State_DelayedThrow)
			{
				//Don't allow us to go straight from local throw to network delayed throw. Remote machine would have gone via hold, so force that first so we can go to throw correctly in a few frames.
				SetState(State_Hold); 
			}
		}
		else if (bVehMelee && (currentState == State_Intro && networkState != State_Throw))
		{
			// Dont allow clones to transition from intro into any other state than throw based on owner's state
		}
		else if (bVehMelee && (currentState == State_Hold || currentState == State_Throw) && networkState == State_Intro)
		{
			// Don't let owner do interrupts whenever, there's separate flag that syncs that we interrupted and clone handles that when appropriate
		}
		else if ((currentState == State_Hold) && (networkState == State_Intro))
		{
			//disregard as this can lead into a loop between hold->intro->hold on remotes...
		}
		else if ((currentState == State_Intro) && (networkState == State_OpenDoor) && !GetClipHelper())
		{
			//disregard as this can lead into a loop between intro->opendoor->intro on remotes...
		}
		else if (networkState == State_SmashWindow && m_bCloneWindowSmashed)
		{
			// don't smash window again if already done so. This can also lead to a task loop.
		}
		else if (bVehMelee && currentState == State_Hold && networkState == State_Finish)
		{
			// Owners task already finished, just finish it on clone side with a swipe too
			SetState(State_Throw);
		}
		else if (bVehMelee && (currentState == State_Intro || currentState == State_Throw) && networkState == State_Finish)
		{
			// Let the task finish playing out on teh clone if started a swipe
		}
		else
		{
			SetState(networkState); //allow the state to be set from the network state
		}
	}
}

void CTaskMountThrowProjectile::CleanUp()
{
	weaponAssert(GetPed()->GetWeaponManager());
	CPed& rPed = *GetPed();
	rPed.GetWeaponManager()->DestroyEquippedWeaponObject(true, NULL, GetIsPerformingVehicleMelee());

	// Ensure we apply network damage
	ProcessCachedNetworkDamage(&rPed);

	if (!m_bVehMeleeRestarting)
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee, false);

	if(!m_bUsingSecondaryTaskNetwork)
	{
		rPed.GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION);
	}
	else
	{
		rPed.GetMovePed().ClearSecondaryTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION); 
	}

	CNetObjPed *netObjPed  = static_cast<CNetObjPed *>(GetPed()->GetNetworkObject());
	
	if(netObjPed)
	{   //Make sure this is not left set
		netObjPed->SetTaskOverridingPropsWeapons(false);
	}

	if(m_bNeedsToOpenDoors)
	{
		// If we opened it before, set door swinging free so it can close properly
		CVehicle* pVehicle = GetPed()->GetMyVehicle();
		if (pVehicle && pVehicle->GetSeatManager())
		{
			s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
			if (pVehicle->IsSeatIndexValid(iSeat) && pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetModelSeatInfo())
			{
				s32 iDirectEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);
				if (iDirectEntryIndex > -1)
				{
					CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iDirectEntryIndex);
					if (pDoor)
					{
						pDoor->SetSwingingFree(pVehicle);
					}
				}
			}
		}
	}

	CleanUpHitEntities();

	if (sm_pTestBound)
	{
		sm_pTestBound->Release();
		sm_pTestBound = NULL;
	}

#if FPS_MODE_SUPPORTED
	if (m_pFirstPersonIkHelper)
	{
		delete m_pFirstPersonIkHelper;
		m_pFirstPersonIkHelper = NULL;
	}
#endif // FPS_MODE_SUPPORTED
}

CTask::FSM_Return CTaskMountThrowProjectile::Init_OnUpdate(CPed* pPed)
{	
	u32 uWeapon = pPed->GetWeaponManager()->GetEquippedWeaponHash();
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeapon);	
	if (!pWeaponInfo || (!pWeaponInfo->GetIsThrownWeapon() && !GetIsPerformingVehicleMelee()))
	{
		if (!pPed->IsNetworkClone())
			SetState(State_Finish);
		
		return FSM_Continue;
	}		

	weaponAssert(pPed->GetWeaponManager());
	m_pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	taskAssertf(m_pVehicleDriveByInfo, "Couldn't find driveby info for ped %s, weapon %s", pPed->GetDebugName(), pWeaponInfo->GetName());

	const CVehicleDriveByAnimInfo* pClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, uWeapon);
	taskAssert(pClipInfo);

	if (!m_pVehicleDriveByInfo || !pClipInfo)
	{
		if (!pPed->IsNetworkClone())
			SetState(State_Finish);

		return FSM_Continue;
	}

	bool bFirstPerson = IsFirstPersonDriveBy();
	if (GetIsPerformingVehicleMelee())
	{
		TUNE_GROUP_BOOL(VEHICLE_MELEE, bDisableFP_anims, true);
		if (!bFirstPerson || bDisableFP_anims)
		{
			m_iClipSet = pClipInfo->GetVehicleMeleeClipSet();
		}
		else
		{
			m_iClipSet = pClipInfo->GetFirstPersonVehicleMeleeClipSet();
		}
	}
	else if( bFirstPerson && pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) != CLIP_SET_ID_INVALID )
	{
		m_iClipSet = pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash());
	}
	else
	{
		m_iClipSet = pClipInfo->GetClipSet();
	}

	aiAssertf(m_iClipSet != CLIP_SET_ID_INVALID, "CTaskMountThrowProjectile: Could not find clipset to use. DrivebyAnimInfo - %s, Vehicle melee - %s, First person - %s",
													pClipInfo->GetName().TryGetCStr(),
													AILogging::GetBooleanAsString(GetIsPerformingVehicleMelee()),
													AILogging::GetBooleanAsString(bFirstPerson));

	m_fMinYaw = m_pVehicleDriveByInfo->GetMinAimSweepHeadingAngleDegs(pPed) * DtoR;
	m_fMaxYaw = m_pVehicleDriveByInfo->GetMaxAimSweepHeadingAngleDegs(pPed) * DtoR;

	//This may have been set by being jacked previously B* 1559968
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);

	SetState(State_StreamingClips);
	return FSM_Continue;
}

CTask::FSM_Return CTaskMountThrowProjectile::StreamingClips_OnUpdate()
{	
	if (m_ClipSetRequestHelper.Request(m_iClipSet))
	{
		CPed* pPed = GetPed();
		if (!pPed)
			return FSM_Continue;

		Vector3 vTarget(0,0,0);

		if(pPed->IsNetworkClone())
		{
			if(GetStateFromNetwork() == State_SmashWindow || m_bWindowSmashed)
			{
				SetState(State_SmashWindow);
				return FSM_Continue;		
			}
		}
		
		//Play open door clip if available
		const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
		taskAssert(pDrivebyInfo);
		if (pDrivebyInfo && pDrivebyInfo->GetNeedToOpenDoors())
		{
			m_bNeedsToOpenDoors = true;
			CVehicle* pVehicle = pPed->GetMyVehicle();
			if (pVehicle && pVehicle->GetSeatManager())
			{
				s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				if (pVehicle->IsSeatIndexValid(iSeat))
				{
					// We may not have streamed in the in vehicle anims yet, so we must wait before playing
					if (!CTaskVehicleFSM::RequestClipSetFromSeat(pVehicle, iSeat))
					{
						return FSM_Continue;
					}

					if (pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetModelSeatInfo())
					{
						s32 iDirectEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);
						if (iDirectEntryIndex > -1)
						{
							CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iDirectEntryIndex);
							if (pDoor && !pDoor->GetIsFullyOpen(0.05f))
							{
								s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
								if (taskVerifyf(iSeat != -1, "Invalid seat index"))
								{
									const CVehicleSeatAnimInfo* pSeatClipInfo = pVehicle->GetSeatAnimationInfo(iSeat);
									if (pSeatClipInfo && (pSeatClipInfo->GetSeatClipSetId().GetHash() != 0))
									{
										static const fwMvClipId OPEN_DOOR_CLIP("d_open_in",0x444B1303);
										StartClipBySetAndClip(pPed,pSeatClipInfo->GetSeatClipSetId(), OPEN_DOOR_CLIP,SLOW_BLEND_IN_DELTA);
										SetState(State_OpenDoor);
										return FSM_Continue;
									}
								}
							}
						}
					}
				}
			}
		}
		else if(CTaskVehicleGun::NeedsToSmashWindowFromTargetPos(pPed,vTarget))
		{
			SetState(State_SmashWindow);
			return FSM_Continue;
		}

		SetState(State_Intro);
	}

	return FSM_Continue;
}	

bool CTaskMountThrowProjectile::ShouldBlockHandIk(bool bRightArm) const
{
	const CPed* pPed = GetPed();

	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if (pWeaponInfo && ((pWeaponInfo->GetIsUnarmed() && !IsFirstPersonDriveBy()) || pWeaponInfo->GetIsThrownWeapon()))
		return false;

	if (bRightArm)
	{
		if (!pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit())
			return false;
	}
	else
	{
		if (pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit())
			return false;
	}

	if (GetState() == State_Throw)
	{
		if (m_fThrowIkBlendInStartPhase != -1.0f && m_MoveNetworkHelper.IsNetworkActive())
		{
			float fPhase = m_MoveNetworkHelper.GetFloat(ms_ThrowPhase);
			if (fPhase >= m_fThrowIkBlendInStartPhase)
			{
				return false;
			}
		}
	}

	return true;
}

bool CTaskMountThrowProjectile::GetIsCloneInDamageWindow() const
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		TUNE_GROUP_FLOAT(VEHICLE_MELEE, fCloneDamagePhaseThrowMin, 0.07f, 0.0f, 1.0f, 0.05f);

		if (GetState() == State_Throw)
		{
			float fThrowPhase = m_MoveNetworkHelper.GetFloat(ms_ThrowPhase);
			if (fThrowPhase >= fCloneDamagePhaseThrowMin)
			{
				return true;
			}
		}
	}

	return false;
}

void CTaskMountThrowProjectile::Intro_OnEnter(CPed* pPed)
{
	m_bUseUpperBodyFilter = ShouldUseUpperBodyFilter(pPed);
	RestartMoveNetwork(m_bUseUpperBodyFilter, false);

	static dev_float sfCreatePhase = 0.163f;	
	m_fCreatePhase = sfCreatePhase;	

	if(IsFirstPersonDriveBy())
	{
		u32 uWeapon = pPed->GetWeaponManager()->GetEquippedWeaponHash();
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeapon);
		if (pWeaponInfo)
		{
			m_GripClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
		}
	}

	UpdateFPSMoVESignals();

	//Is this a single animation behavior?
	const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
	if (GetIsPerformingVehicleMelee())
	{
		m_bVehMelee = true;

	}
	else if ((pClipSet && pClipSet->GetClip(fwMvClipId("throw_0",0xB6D0C6A8)) == NULL))
	{
		m_bSimpleThrow = true;
	}

	if (pClipSet && pClipSet->GetClip(fwMvClipId("aim_breathe_additive",0x379CBAEB)) != NULL)		
		m_MoveNetworkHelper.SetFlag(true, ms_UseBreathing);	
	else
		m_MoveNetworkHelper.SetFlag(false, ms_UseBreathing);	

	//Check for drop
	if (CheckForDrop(pPed))
		m_bDrop = true;	

	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	taskAssert(pDrivebyInfo);
	m_MoveNetworkHelper.SetFlag(m_bVehMelee, ms_UseVehMelee);	
	m_MoveNetworkHelper.SetFlag(m_bSimpleThrow, ms_UseSimpleThrow);	
	m_MoveNetworkHelper.SetFlag(m_bDrop, ms_UseDrop);	

	if(pDrivebyInfo)
	{
		if (!(pPed->GetMyVehicle() && pPed->IsInFirstPersonVehicleCamera() && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_FIVE_ANIM_THROW_FP)))
			m_MoveNetworkHelper.SetFlag(pDrivebyInfo->GetUseThreeAnimThrow(), ms_UseThreeAnimThrow);
	}

	bool bCloneVehMelee = pPed->IsNetworkClone() && m_bVehMelee;
	weaponAssert(pPed->GetWeaponManager());
	bool bCloneOrCooking = (pPed->GetWeaponManager()->GetEquippedWeapon() && pPed->GetWeaponManager()->GetEquippedWeapon()->GetIsCooking()) || bCloneVehMelee;
	if (pPed->GetWeaponManager()->GetEquippedWeapon() !=NULL && (!bCloneOrCooking || m_bInterruptingWithOpposite || m_bRestartingWithOppositeSide)) 
	{
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
	}

	// Using lowrider alt clipset, skip intro phase forwards as arm is on window, so hand is not starting on the steering wheel.
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans))
	{
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fProjectileDrivebyIntroPhase, 0.25f, 0.0f, 1.0f, 0.01f);
		m_MoveNetworkHelper.SetFloat(ms_IntroPhase, fProjectileDrivebyIntroPhase);
	}

	//NG - don't SetTaskOverridingPropsWeapons(true) here - as it interferes with changing the throwable weapons when at the ready using L1, retain for last-gen/CG - lavalley.
}

CTask::FSM_Return CTaskMountThrowProjectile::Intro_OnUpdate(CPed* pPed)
{
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		float fTimeInState = GetTimeInState();
		if (fTimeInState > 5.0f) //something went wrong, abort
		{
			SetState(State_Finish);		
			return FSM_Continue;
		}

		return( FSM_Continue );
	}
	
	// When the create object event occurs then spawn the grenade in the ped's hand 
	static const fwMvClipId s_IntroClip("IntroClip",0x4139FEC0);
	const crClip* pClip = m_MoveNetworkHelper.GetClip(s_IntroClip);
	if (pClip)
	{
		CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Create, true, m_fCreatePhase);	
	}

	//When the create clip event happens, create the projectile 
	float fPhase = m_MoveNetworkHelper.GetFloat(ms_IntroPhase);

	weaponAssert(pPed->GetWeaponManager());

	bool bVehMelee = GetIsPerformingVehicleMelee();
	if (bVehMelee)
	{
		m_fCreatePhase = PHASE_TO_CREATE_VEH_MELEE_WEAPON;

		if (m_bWantsToFire)
		{
			if (pPed->GetWeaponManager()->GetEquippedWeaponObject() == NULL && ShouldCreateWeaponObject(*pPed))
			{
				CPedEquippedWeapon::eAttachPoint eHand = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() ? CPedEquippedWeapon::AP_RightHand : CPedEquippedWeapon::AP_LeftHand;
				pPed->GetWeaponManager()->CreateEquippedWeaponObject( eHand );
				UpdateAiming(pPed, true, false); //pop the initial aim setup
			}

			// Scale down the blend rate to 0.0f if we're close to the end of the intro. But the maximum time of the blend is the default time in move - 0.25f
			TUNE_GROUP_FLOAT(VEHICLE_MELEE, fMaxIntroInterruptBlendDuration, 0.066f, 0.0f, 1.0f, 0.05f);
			float fBlendRate = 1.0f - fPhase;
			m_fIntroToThrowTransitionDuration = Clamp(fBlendRate,0.0f, fMaxIntroInterruptBlendDuration);

			SetState(ShouldDelayThrow() ? State_DelayedThrow : State_Throw);
		}
		else if (fPhase >= 1.0f)
		{
			SetState(State_Hold);
		}

		if (pPed->IsNetworkClone())
		{
			SetNextStateFromNetwork();

			if (GetStateFromNetwork() == State_Throw)
			{
				if (pPed->GetWeaponManager()->GetEquippedWeaponObject() == NULL && ShouldCreateWeaponObject(*pPed))
				{
					CPedEquippedWeapon::eAttachPoint eHand = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() ? CPedEquippedWeapon::AP_RightHand : CPedEquippedWeapon::AP_LeftHand;
					pPed->GetWeaponManager()->CreateEquippedWeaponObject( eHand );
					UpdateAiming(pPed, true, false); //pop the initial aim setup
				}
			}

			return FSM_Continue;
		}
	}

	if(fPhase >= m_fCreatePhase)
	{
		if (pPed->GetWeaponManager()->GetEquippedWeaponObject() == NULL && ShouldCreateWeaponObject(*pPed)) 
		{
			CPedEquippedWeapon::eAttachPoint eHand = CPedEquippedWeapon::AP_RightHand;
			if (GetIsPerformingVehicleMelee())
			{
				if (!pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit())
					eHand = CPedEquippedWeapon::AP_LeftHand;
			}
			else if (m_pVehicleDriveByInfo->GetLeftHandedProjctiles())
			{
				eHand = CPedEquippedWeapon::AP_LeftHand;
			}

			pPed->GetWeaponManager()->CreateEquippedWeaponObject( eHand );
			UpdateAiming(pPed, true, false); //pop the initial aim setup
		}		

		if (pPed->IsNetworkClone())
		{
			SetNextStateFromNetwork();
			return FSM_Continue;
		}
		else if (m_bSimpleThrow || m_bDrop)
		{
			m_MoveNetworkHelper.SetFloat(ms_IntroPhase, m_fCreatePhase);
			SetState(State_Hold);
		}
	}
 
	//! No early out in 1st person driveby as projectile can clip vehicle.
	if(IsFirstPersonDriveBy())
	{
		if (fPhase >= 1.0f)
		{
			SetState(State_Hold);
		}
	}
	else
	{
		// start playing the aim clip	
		TUNE_GROUP_FLOAT(PROJECTILE_THROW_TUNE, INTRO_CROSS_BLEND_PHASE, 0.9f, 0.0f, 1.0f, 0.01f);
		if (fPhase >= INTRO_CROSS_BLEND_PHASE) //TODO
		{
			if(m_bWantsToFire)
			{
				SetState(ShouldDelayThrow() ? State_DelayedThrow : State_Throw);		
			}
			else if(!m_bWantsToHold)
			{
				if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_bWantsToCook)  //if in a car just throw, B* 495583
				{
					SetState(ShouldDelayThrow() ? State_DelayedThrow : State_Throw);		
				}	
				else
				{
					SetState(State_Hold);					
				}
			}
			else
			{
				SetState(State_Hold);
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMountThrowProjectile::OpenDoor_OnUpdate(CPed* pPed)
{

	if (!GetClipHelper()) //anim done
	{
		SetState(State_Intro);
		return FSM_Continue;
	}
	
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if (pVehicle)
	{
		s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (pVehicle->IsSeatIndexValid(iSeat))
		{
			s32 iDirectEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);
			if (iDirectEntryIndex > -1)
			{
				CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iDirectEntryIndex);
				if (pDoor)
				{
					if (GetClipHelper()) 
					{
						float fPhase = GetClipHelper()->GetPhase();
						const crClip* pClip = GetClipHelper()->GetClip();
						static const float STARTOPENDOOR = 0.244f;
						static const float ENDOPENDOOR = 0.529f;	
						float fStartPhase, fEndPhase;
						bool bStartTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fStartPhase);
						bool bEndTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fEndPhase);

						if (!bStartTagFound)
							fStartPhase = STARTOPENDOOR;
						
						if (!bEndTagFound)
							fEndPhase = ENDOPENDOOR;

						if (fPhase >= fEndPhase)
						{
							pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
						}
						else if (fPhase >= fStartPhase)
						{
							float fRatio = rage::Min(1.0f, (fPhase - fStartPhase) / (fEndPhase - fStartPhase));
							fRatio = rage::Max(pDoor->GetDoorRatio(), fRatio);
							pDoor->SetTargetDoorOpenRatio(fRatio, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
						}						
					}
					else
					{
						pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
					}
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskMountThrowProjectile::SmashWindow_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetMyVehicle();
	bool smashWindowAnimFound = true;

	// Check to see if a smash window animation exists, some clipsets are missing them
	if (pVehicle)
	{
		const CVehicleSeatAnimInfo* pVehicleSeatInfo = pVehicle->GetSeatAnimationInfo(pPed);
		s32 iDictIndex = -1;
		u32 iAnimHash = 0;
		if (pVehicleSeatInfo)
		{
			bool bFirstPerson = pPed->IsInFirstPersonVehicleCamera();
			smashWindowAnimFound = pVehicleSeatInfo->FindAnim(CVehicleSeatAnimInfo::SMASH_WINDOW, iDictIndex, iAnimHash, bFirstPerson);
		}
	}

	m_bWindowSmashed = m_bCloneWindowSmashed = true;

	if (pVehicle && (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_BULLETPROOF_GLASS) || !smashWindowAnimFound)) 
	{
		// Find window
		s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (pVehicle->IsSeatIndexValid(iSeat))
		{
			int iEntryPointIndex = -1;
			iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);

			// Did we find a door?
			if(iEntryPointIndex != -1)
			{
				const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPointIndex);
				Assertf(pEntryPointInfo, "Vehicle is missing entry point");

				eHierarchyId window = pEntryPointInfo->GetWindowId();
				pVehicle->RolldownWindow(window);
			}
		}
	}
	else
		SetNewTask(rage_new CTaskSmashCarWindow(false));
}

CTask::FSM_Return CTaskMountThrowProjectile::SmashWindow_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		SetState(State_Intro);
	}

	return FSM_Continue;
}

void CTaskMountThrowProjectile::Hold_OnEnter()
{
	CPed* pPed = GetPed();

	m_bRestartingWithOppositeSide = false;

	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterAimId);
	m_MoveNetworkHelper.SendRequest(ms_StartAim);
	UpdateFPSMoVESignals();
	if (!GetIsPerformingVehicleMelee())
	{
		const fwMvFilterId frameFilterId("UpperBodyFilter",0x6F5D10AB);
		crFrameFilter* pUpperBody = g_FrameFilterDictionaryStore.FindFrameFilter(CMovePed::s_UpperBodyFilterHashId);
		if(pUpperBody)
			m_MoveNetworkHelper.SetFilter(pUpperBody, frameFilterId);
	}
	else if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() == NULL && ShouldCreateWeaponObject(*pPed))
	{
		CPedEquippedWeapon::eAttachPoint eHand = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() ? CPedEquippedWeapon::AP_RightHand : CPedEquippedWeapon::AP_LeftHand;
		pPed->GetWeaponManager()->CreateEquippedWeaponObject( eHand );
		UpdateAiming(pPed, true, false); //pop the initial aim setup
	}

	CleanUpHitEntities();
}

CTask::FSM_Return CTaskMountThrowProjectile::Hold_OnUpdate(CPed* pPed)
{
	float fTimeInState = GetTimeInState();
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		if (fTimeInState > 5.0f) //something went wrong, abort
		{
			SetState(State_Finish);		
			return FSM_Continue;
		}

		return( FSM_Continue );
	}

	if (GetIsPerformingVehicleMelee())
	{
		if (!GetIsBikeMoving(*pPed))
		{
			// If we're stopping the bike while holding unarmed kick, quit the task since kicks are disabled while stationary
			const CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
			if (pWeaponMgr && pWeaponMgr->GetEquippedWeaponInfo() && (pWeaponMgr->GetEquippedWeaponInfo()->GetIsUnarmed() || pWeaponMgr->GetEquippedWeaponInfo()->GetIsThrownWeapon()))
			{
				SetState(State_Throw);
				return FSM_Continue;
			}
		}

		if (m_bInterruptingWithOpposite || (pPed->IsNetworkClone() && m_bRestartingWithOppositeSide))
		{
			if (!pPed->IsNetworkClone())
				m_bRestartingWithOppositeSide = true;

			pPed->GetPlayerInfo()->SetVehMeleePerformingRightHit(!pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit());

			SetState(State_Init);
			return FSM_Continue;
		}

		if (pPed->IsNetworkClone() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() == NULL)
		{
			pPed->GetWeaponManager()->CreateEquippedWeaponObject( pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() ? CPedEquippedWeapon::AP_RightHand : CPedEquippedWeapon::AP_LeftHand);
		}

		TUNE_GROUP_FLOAT(VEHICLE_MELEE, fCloneTimeInHoldForSync, 0.5f, 0.0f, 10.0f, 0.05f);
		if (pPed->IsNetworkClone() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() != NULL && fTimeInState > fCloneTimeInHoldForSync)
		{
				CPedEquippedWeapon::eAttachPoint attachedHand = pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint();
				bool bRightSide = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit();
				if ((attachedHand == CPedEquippedWeapon::AP_LeftHand && bRightSide) ||
					(attachedHand == CPedEquippedWeapon::AP_RightHand && !bRightSide) ||
					(m_bOwnersCurrentSideRight && !bRightSide) ||
					(!m_bOwnersCurrentSideRight && bRightSide))
				{
					pPed->GetWeaponManager()->DestroyEquippedWeaponObject(false OUTPUT_ONLY(,"CTaskMountThrowProjectile::Hold_OnUpdate - VehMelee Clone Out Of Sync"));

					// Got out of sync, restart to the other side
					pPed->GetPlayerInfo()->SetVehMeleePerformingRightHit(!pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit());

					SetState(State_Init);
					return FSM_Continue;
				}
		}
	}

	UpdateMoveFilter();

	if(pPed->IsNetworkClone())
	{
		if (pPed->GetWeaponManager()->GetEquippedWeaponObject() ==NULL && ShouldCreateWeaponObject(*pPed)) 
		{
			pPed->GetWeaponManager()->CreateEquippedWeaponObject( (m_pVehicleDriveByInfo->GetLeftHandedProjctiles()) ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand);
		}		

		return( FSM_Continue );
	}

	weaponAssert(pPed->GetWeaponManager());
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	bool bVehMeleeWithThrown = GetIsPerformingVehicleMelee() && pPed->GetWeaponManager()->GetEquippedWeaponInfo() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsThrownWeapon();
	if (bVehMeleeWithThrown)
	{
		if(pPed->GetWeaponManager()->GetUnarmedEquippedWeapon())
			pWeapon = pPed->GetWeaponManager()->GetUnarmedEquippedWeapon();
		else 
			pWeapon = CWeaponManager::GetWeaponUnarmed();
	}

	if (!pWeapon) 
	{
		if (m_bHadValidWeapon)
		{
			SetState(State_Outro);
		}
		return FSM_Continue;
	}

	m_bHadValidWeapon = true;


	if (pPed->IsLocalPlayer() && !m_bWindowSmashed)
	{
		// Clear out partially smashed windows based on aim vector
		Vector3 vStart(VEC3_ZERO);
		Vector3 vEnd(VEC3_ZERO);
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		const Matrix34* pmWeapon;
		if (pWeaponObject)
			pmWeapon = &RCC_MATRIX34(pWeaponObject->GetMatrixRef());
		else
			pmWeapon = &RCC_MATRIX34(pPed->GetMatrixRef());
		pWeapon->CalcFireVecFromAimCamera(pPed, *pmWeapon, vStart, vEnd);

		if (CTaskVehicleGun::ClearOutPartialSmashedWindow(pPed, vEnd))
		{
			m_bWindowSmashed = true;
		}
	}

	// B*2138085: Keep door open while holding projectile if doors need to be open to throw.
	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	if (pDrivebyInfo && pDrivebyInfo->GetNeedToOpenDoors())
	{
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle && pVehicle->GetSeatManager())
		{
			s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (pVehicle->IsSeatIndexValid(iSeat))
			{
				if (pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetModelSeatInfo())
				{
					s32 iDirectEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);
					if (iDirectEntryIndex > -1)
					{
						CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iDirectEntryIndex);

						if (pDoor && !pDoor->GetIsFullyOpen(0.01f))
						{
							pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
						}
					}
				}
			}
		}
	}

	//Check for drop
	if (CheckForDrop(pPed))
		m_bDrop = true;	

	if (m_bWantsToCook && pWeapon->GetWeaponInfo()->GetCookWhileAiming())
	{
		if (!pWeapon->GetIsCooking())
			pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
	} 
	else 
	{
		if (pWeapon->GetIsCooking())
			pWeapon->CancelCookTimer();
	}
	TUNE_GROUP_FLOAT(PROJECTILE_THROW_TUNE, HOLD_CROSS_BLEND_PHASE, 0.9f, 0.0f, 1.0f, 0.01f);
	float fBlendPhase = m_MoveNetworkHelper.GetFloat(ms_IntroPhase);
	bool bThrowOk = (fBlendPhase < 0 || fBlendPhase > HOLD_CROSS_BLEND_PHASE) || m_bDrop;

	const float fFuseTime = pWeapon ? pWeapon->GetWeaponInfo()->GetProjectileFuseTime(pPed->GetIsInVehicle()) : 0.0f;
	if ( fFuseTime > 0.0f && pWeapon->GetIsCooking() && pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()) <= 0 ) 
	{			
		SetState(ShouldDelayThrow() ? State_DelayedThrow : State_Throw);		
		return FSM_Continue;			
	}
	else if(m_bWantsToFire && bThrowOk)
	{
		SetState(ShouldDelayThrow() ? State_DelayedThrow : State_Throw);		
	}
	else if(!m_bWantsToHold)
	{
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && !m_bThrownGrenade && m_bWantsToCook)  //if in a car just throw, B* 495583
		{
			if (bThrowOk)
				SetState(ShouldDelayThrow() ? State_DelayedThrow : State_Throw);		
		}
		else if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack) && !GetIsFlagSet(aiTaskFlags::TerminationRequested))
		{
			return FSM_Continue;
		}
		else
		{	
			SetState(State_Outro);				
		}
	}

	const CControl* pControl = pPed->GetControlFromPlayer();
	if(pControl && !pPed->GetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching))
	{
		if (m_bEquippedWeaponChanged)
		{
			SetState(State_Outro);		
			return FSM_Continue;
		}
	}
	
	return FSM_Continue;
}

void CTaskMountThrowProjectile::Throw_OnEnter()
{	
	static dev_float sfThrowPhase = 0.15f;	
	m_fThrowPhase = sfThrowPhase;	
	static dev_float sfCreatePhase = 0.515f;	
	m_fCreatePhase = sfCreatePhase;	
	m_bThrownGrenade=false;
	m_bCloneThrowComplete=false;
	m_bDisableGripFromThrow = false;

	CPed* pPed = GetPed();

	m_bRestartingWithOppositeSide = false;

	if (pPed->IsNetworkClone())
	{
		if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() == NULL && ShouldCreateWeaponObject(*pPed))
		{
			CPedEquippedWeapon::eAttachPoint eHand = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit() ? CPedEquippedWeapon::AP_RightHand : CPedEquippedWeapon::AP_LeftHand;
			pPed->GetWeaponManager()->CreateEquippedWeaponObject( eHand );
			UpdateAiming(pPed, true, false); //pop the initial aim setup
		}
	}

	if (GetIsPerformingVehicleMelee())
	{
		pPed->GetPedAudioEntity()->PlayMeleeCombatBikeSwipe();

		//! Create ID for this melee attack. Note: This needs to be unique across all machines, so we OR in player index to a static counter.
		if(m_nUniqueNetworkVehMeleeActionID == 0 && NetworkInterface::IsGameInProgress() && NetworkInterface::GetLocalPlayer())
		{
			++CTaskMountThrowProjectile::m_nActionIDCounter;

			//! 1st 8 bits reserved for player ID. Last 8 bits reserved for counter. Note: I could probably infer player index from the
			//! ped ID on remote side.
			m_nUniqueNetworkVehMeleeActionID = NetworkInterface::GetLocalPhysicalPlayerIndex() << 8;
			m_nUniqueNetworkVehMeleeActionID |= CTaskMountThrowProjectile::m_nActionIDCounter;
		}
	}

	if (m_bDrop)
	{
		m_MoveNetworkHelper.SendRequest(ms_Drop);
	}
	else
	{
		if (m_fIntroToThrowTransitionDuration != -1.0f)
		{
			m_MoveNetworkHelper.SetFloat(ms_VehMeleeIntroToHitBlendTime, m_fIntroToThrowTransitionDuration);
		}

		m_MoveNetworkHelper.SendRequest(ms_StartThrow);
	}
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterThrowId);

	UpdateFPSMoVESignals();
}

CTask::FSM_Return CTaskMountThrowProjectile::Throw_OnUpdate(CPed* pPed)
{
	m_fIntroToThrowTransitionDuration = -1.0f;

	float fTimeInState = GetTimeInState();
	if (fTimeInState > 5.0f) //something went wrong, abort
	{
		SetState(State_Finish);		
		return FSM_Continue;
	}

	if (!m_MoveNetworkHelper.IsInTargetState())
		return( FSM_Continue );

	// Doing this in Throw_Enter is always a frame late, (MoVE)
	static const fwMvClipId s_ThrowClip("ThrowClip",0xC4C387F);
	const crClip* pClip = m_MoveNetworkHelper.GetClip(s_ThrowClip);
	if (pClip)
	{
		//HACK - B*1864258 - If the drop_grenade animation doesn't have a fire tag for some reason, default to 0.4f (other throws already default to 0.15f)
		if (!CClipEventTags::FindEventPhase(pClip, CClipEventTags::Fire, m_fThrowPhase) && m_bDrop)
		{
			m_fThrowPhase = 0.4f;
		}
	}

	bool bVehMelee = GetIsPerformingVehicleMelee();
	if (bVehMelee)
	{
		ProcessMeleeCollisions(GetPed());
	}

	weaponAssert(pPed->GetWeaponManager());
	CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	float fPhase = m_MoveNetworkHelper.GetFloat(ms_ThrowPhase);

	if (bVehMelee && pPed->IsNetworkClone() && AreNearlyEqual(fPhase, 1.0f) && GetStateFromNetwork() == State_Finish)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (!bVehMelee && pPed->IsNetworkClone() && !m_bThrownGrenade)
	{
		//Clones check and wait here for the synced projectile and throw as soon as they see it
		if(CNetworkPendingProjectiles::GetProjectile(GetPed(), GetNetSequenceID(), false))
		{
			//Clones throw as soon as they see a projectile synced
			m_bThrownGrenade = ThrowProjectileImmediately(pPed);
		}
		else
		{
			if(GetStateFromNetwork()==State_Finish)
			{
				SetState(State_Finish);
			}
			return FSM_Continue;
		}
	}

	// Somehow we've got to this point without smashing / clearing a window, so let's do it again
	if (!m_bWindowSmashed)
	{
		Vector3 vTarget(0,0,0);
		if (CTaskVehicleGun::ClearOutPartialSmashedWindow(pPed, vTarget))
		{
			m_bWindowSmashed = true;
		}
	}

	CalculateTrajectory(pPed);
	//When the fire clip event happens, throw the projectile 
	if(!m_bThrownGrenade && fPhase >= m_fThrowPhase)
	{
		// Time to throw the projectile
		m_bThrownGrenade = ThrowProjectileImmediately(pPed);
	}

	// When the create object event occurs then spawn the (new) grenade in the ped's hand 
	if (pClip)
	{
		CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Create, true, m_fCreatePhase);
	}

	bool bDisableGrip = m_MoveNetworkHelper.GetBoolean(ms_DisableGripId);
	if(bDisableGrip != m_bDisableGripFromThrow)
	{
		m_bDisableGripFromThrow = bDisableGrip;
		UpdateFPSMoVESignals();
	}

	if (fTimeInState > 0.5f && fPhase < 0)
	{	//Something went wrong, finish it
		SetState(State_Finish);		
		return FSM_Continue;
	}

	bool bWantsToThrowAgain = m_bWantsToHold && !m_bSimpleThrow && !CheckForDrop(pPed);

	if (GetIsPerformingVehicleMelee() && !GetIsBikeMoving(*pPed))
	{
		// If we're stopping the bike while holding unarmed kick, quit the task since kicks are disabled while stationary
		if (pWeaponMgr->GetEquippedWeaponInfo() && (pWeaponMgr->GetEquippedWeaponInfo()->GetIsUnarmed() || pWeaponMgr->GetEquippedWeaponInfo()->GetIsThrownWeapon()))
		{
			bWantsToThrowAgain = false;
		}
	}

	bool bCloneInterupting = pPed->IsNetworkClone() && m_bRestartingWithOppositeSide;
	if (bVehMelee && ((bWantsToThrowAgain && m_bInterruptible) || bCloneInterupting))
	{
		// Reduce blend time interrupting with opposite side
		if (m_bInterruptingWithOpposite || bCloneInterupting)
		{
			if (!pPed->IsNetworkClone())
				m_bRestartingWithOppositeSide = true;

			TUNE_GROUP_FLOAT(VEHICLE_MELEE, fThrowToHoldOppositeInterruptBlendRate, 0.1f, 0.0f, 1.0f, 0.05f);
			m_MoveNetworkHelper.SetFloat(ms_VehMeleeHitToIntroBlendTime, fThrowToHoldOppositeInterruptBlendRate);

			pPed->GetPlayerInfo()->SetVehMeleePerformingRightHit(!pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit());

			SetState(State_Init);
			return FSM_Continue;
		}

		SetState(State_Hold);
		return FSM_Continue;
	}

	//When the create clip event happens, create the projectile 
	if(fPhase >= m_fCreatePhase  && !m_bDrop && bWantsToThrowAgain)
	{
		if (pWeaponMgr->GetEquippedWeaponObject() ==NULL && ShouldCreateWeaponObject(*pPed)) 
		{
			pWeaponMgr->CreateEquippedWeaponObject( (m_pVehicleDriveByInfo->GetLeftHandedProjctiles()) ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand);
		}	

		m_bDisableGripFromThrow=false;
	}

	if (bVehMelee && (!bWantsToThrowAgain || pPed->IsNetworkClone()) && fPhase >= PHASE_TO_DESTROY_VEH_MELEE_WEAPON && pWeaponMgr && pWeaponMgr->GetEquippedWeaponObject() != NULL)
	{
		pWeaponMgr->DestroyEquippedWeaponObject(false OUTPUT_ONLY(,"CTaskMountThrowProjectile::Throw_OnUpdate - Vehicle Melee"));
	}
	
	if (m_MoveNetworkHelper.GetBoolean( ms_ThrowCompleteEvent) || m_bCloneThrowComplete)
	{
		if (pPed->IsNetworkClone())
		{
			m_bCloneThrowComplete = true;

			SetNextStateFromNetwork();
			return FSM_Continue;
		}
		else if (m_bDrop)
		{
			SetState(State_Finish);
		}
		else
		{
			if (bWantsToThrowAgain && pWeaponMgr->GetEquippedWeaponInfo()->GetIsThrownWeapon())
			{
				SetState(IsFirstPersonDriveBy() ? State_Hold : State_Intro);
				m_bWantsToCook = false; //reset cook
				m_bThrownGrenade = false;
			} 
			else 
			{
				SetState(State_Finish);
			}
			return FSM_Continue;
		}
	}	

	//wait for ped to release fire input
	if (m_bWaitingForDropRelease && !m_bWantsToHold && !pPed->IsNetworkClone())
	{
		SetState(State_Finish);
	}

	// Player wants to change weapons
	if(m_bThrownGrenade)
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if(pControl && !pPed->GetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching))
		{
			if (m_bEquippedWeaponChanged)
			{
				SetState(State_Outro);		
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

void CTaskMountThrowProjectile::Throw_OnExit()
{
	m_bInterruptible = false;
}

void CTaskMountThrowProjectile::DelayedThrow_OnEnter()
{
	m_iDelay = 0;
}

CTask::FSM_Return CTaskMountThrowProjectile::DelayedThrow_OnUpdate()
{
	m_iDelay++;

	if (m_iDelay >= ms_MaxDelay || (GetIsPerformingVehicleMelee() && IsFirstPersonDriveBy()))
	{
		SetState(State_Throw);
		m_iDelay = 0;
	}
	
	return FSM_Continue;
}

void CTaskMountThrowProjectile::Outro_OnEnter()
{	
}

aiTask::FSM_Return CTaskMountThrowProjectile::Outro_OnUpdate()
{
	if( (GetTimeInState() < 2.0f) && m_MoveNetworkHelper.IsNetworkActive() && m_MoveNetworkHelper.IsNetworkAttached() ) //added the 2 second check - as I saw this hang in this sate on a local player - this will ensure it finishes. lavalley.
	{
		const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
		if (pClipSet && pClipSet->GetClip(fwMvClipId("outro_0",0xba7e4fe6)) != NULL)
		{
			m_MoveNetworkHelper.SendRequest(ms_StartOutro);
			if (m_MoveNetworkHelper.GetBoolean( ms_OutroFinishedId))
			{
				SetState(State_Finish);
			}
		}
		else
		{
			SetState(State_Finish);
		}
	}
	else 
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}


void CTaskMountThrowProjectile::Finish_OnEnter()
{	
	if (m_MoveNetworkHelper.IsNetworkActive())
		m_MoveNetworkHelper.SendRequest(ms_StartFinish);
}

bool CTaskMountThrowProjectile::ThrowProjectileImmediately(CPed* pPed) 
{
	bool bRet = false;

	if(pPed->IsNetworkClone())
	{
		if (!GetIsPerformingVehicleMelee())
		{
			CProjectileManager::FireOrPlacePendingProjectile(pPed, GetNetSequenceID());
			// Ensure object is removed
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject();		

			bRet=true;
		}
	}
	else
	{
		weaponAssert(pPed->GetWeaponManager());
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon && pWeapon->GetWeaponInfo()->GetIsThrownWeapon() && !GetIsPerformingVehicleMelee()) 
		{
			// Time to throw the projectile	
			CObject* pProjOverride = pPed->GetWeaponManager()->GetEquippedWeaponObject();

			// Calculate the launch position
			const s32 nBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
			Assert(nBoneIndex != BONETAG_INVALID);

			Matrix34 weaponMat;
			if( pProjOverride )
				weaponMat = MAT34V_TO_MATRIX34(pProjOverride->GetMatrix());
			else
				pPed->GetGlobalMtx(nBoneIndex, weaponMat);

			const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();

			Vector3 vStart = aimFrame.GetPosition();
			Vector3 vShot  = aimFrame.GetFront();
			Vector3 vEnd(Vector3::ZeroType);

			//If this is a drop..
			if( m_bDrop )
			{
				Vector3 trajectory;		
				trajectory = VEC3V_TO_VECTOR3(pPed->GetTransform().GetRight());
				trajectory.Negate(); 
				vEnd = weaponMat.d + trajectory;
				vStart = weaponMat.d;
			}
			else
			{
				// Move source of gunshot vector along to the gun muzzle so can't hit anything behind the gun.
				f32 fShotDot = vShot.Dot(weaponMat.d - vStart);
				vStart += fShotDot * vShot;

				bool bValidTargetPosition = false;
				if( pPed->IsLocalPlayer() && pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->AreControlsDisabled() )
				{
					// First check for the more precise reading, the camera aim at position								
					if( pProjOverride )
					{
						Vector3 vStartTemp( Vector3::ZeroType );
						bValidTargetPosition = pWeapon->CalcFireVecFromAimCamera( pPed->GetIsInVehicle() ? static_cast<CEntity*>(pPed->GetMyVehicle()) : pPed, weaponMat, vStartTemp, vEnd );
					}
				}

				// Fallback
				if( !bValidTargetPosition && m_target.GetIsValid() )
					bValidTargetPosition = m_target.GetPositionWithFiringOffsets( pPed, vEnd );
			}

			// TODO: Limit the throw distance while in cover. Possibly add a WeaponRangeInCover to weapons.meta

			float fOverrideLifeTime = -1.0f;	
			if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
				fOverrideLifeTime = Max(0.0f, pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()));

			// Throw the projectile
			CWeapon::sFireParams params(pPed, weaponMat, &vStart, &vEnd);
			params.fOverrideLifeTime = fOverrideLifeTime;
			if (m_bDrop)
				params.iFireFlags.SetFlag(CWeapon::FF_DropProjectile);

			if(pWeapon->Fire(params))
			{
				// Ensure object is removed
				pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
				bRet=true;
			}

			// Check ammo
			s32 iAmmo = pPed->GetInventory()->GetWeaponAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo());
			if( iAmmo == 0 )
			{
				pPed->GetWeaponManager()->EquipBestWeapon();
				SetState(State_Outro);
			}
		}
	}
	return bRet;
}

void CTaskMountThrowProjectile::CalculateTrajectory( CPed* pPed )
{
	Vector3 vTargetPos;
	// Continually recalculate trajectory if player
	if(pPed->IsLocalPlayer())
	{
		const CPlayerPedTargeting & targeting = pPed->GetPlayerInfo()->GetTargeting();

		if(targeting.GetLockOnTarget())
		{
			const CEntity* pTarget = targeting.GetLockOnTarget();

			Vector3 vTargetVelocity(0.0f, 0.0f, 0.0f);
			if(pTarget->GetIsPhysical())
			{
				vTargetVelocity = static_cast<const CPhysical*>(pTarget)->GetVelocity();
			}

			vTargetPos = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());

			Assert(rage::FPIsFinite(vTargetPos.x));
			Assert(rage::FPIsFinite(vTargetPos.y));
			Assert(rage::FPIsFinite(vTargetPos.z));

			Vector3 vStart = CThrowProjectileHelper::GetPedThrowPosition(pPed, GetClipHelper() ? GetClipHelper()->GetClipId() : CLIP_ID_INVALID);

			CThrowProjectileHelper::CalculateAimTrajectory(pPed, vStart, vTargetPos, vTargetVelocity, m_vTrajectory);
		}
		else
		{
			const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
			m_vTrajectory = aimFrame.GetFront() + aimFrame.GetUp();
			m_vTrajectory.Normalize();

			if(m_vTrajectory.z < 0.0f)
			{
				vTargetPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (m_vTrajectory * 2.0f);
			}
			else
			{
				vTargetPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (m_vTrajectory * 20.0f);
			}

			Assert(rage::FPIsFinite(vTargetPos.x));
			Assert(rage::FPIsFinite(vTargetPos.y));
			Assert(rage::FPIsFinite(vTargetPos.z));

			m_vTrajectory.Scale(CThrowProjectileHelper::GetProjectileVelocity(pPed));
		}
	}
	else if(!pPed->IsNetworkClone())
	{
		Vector3 vStart = CThrowProjectileHelper::GetPedThrowPosition(pPed, GetClipHelper() ? GetClipHelper()->GetClipId() : CLIP_ID_INVALID);
		Vector3 vTargetVelocity(0.0f, 0.0f, 0.0f);
		Vector3 vNewTrajectory;
		if(CThrowProjectileHelper::CalculateAimTrajectory(pPed, vStart, vTargetPos, vTargetVelocity, vNewTrajectory))
		{
			m_vTrajectory = vNewTrajectory;
		}
	}
}

CTaskMountThrowProjectile::eTaskInput CTaskMountThrowProjectile::GetInput(CPed* pPed)
{
	m_bInterruptingWithOpposite = false;

	bool isOnMountorIsPassenger = pPed->GetMyMount()!=NULL || (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && (!pPed->GetMyVehicle()->IsDriver(pPed)));

	eTaskInput input = Input_None;
	if(taskVerifyf(pPed->IsLocalPlayer(),"Task in player mode for non-player ped"))
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		if(aiVerifyf(pControl,"NULL CControl from a player CPed"))
		{
			if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack) || pPed->GetIsParachuting())
			{
				if(pControl->GetPedAttack().IsReleased())
				{
					input = Input_Fire;
				}
				else if( pControl->GetPedTargetIsDown() || pControl->GetPedAttack().IsDown())
				{ 
					input = Input_Hold;
				}
			}
			else if (GetIsPerformingVehicleMelee() && pPed->IsLocalPlayer())
			{
				bool bPerformingRightSide = pPed->GetPlayerInfo()->GetVehMeleePerformingRightHit();
				bool bIgnoreVehMeleeHoldCheck = false;
#if RSG_PC
				// Allow vehicle melee to continue being held once letting go of Hold key on PC, it's in a really awkward default place
				if (pControl->WasKeyboardMouseLastKnownSource())
				{
					bIgnoreVehMeleeHoldCheck = true;
				}
#endif // RSG_PC

				if (pControl->GetVehMeleeHold().IsDown() || bIgnoreVehMeleeHoldCheck)
				{
					bool bLeftReleased = pControl->GetVehMeleeLeft().IsReleased();
					bool bRightReleased = pControl->GetVehMeleeRight().IsReleased();
					bool bLeftHeld = pControl->GetVehMeleeLeft().IsDown();
					bool bRightHeld = pControl->GetVehMeleeRight().IsDown();

					if ( (bLeftReleased && !bRightHeld) || (bRightReleased && !bLeftHeld) )
					{
						if ((bPerformingRightSide && bLeftReleased) || (!bPerformingRightSide && bRightReleased))
						{
							m_bInterruptingWithOpposite = true;
						}

						input = Input_Fire;
					}
					else if ( (bLeftHeld && !bPerformingRightSide) || (bRightHeld && bPerformingRightSide))
					{
						input = Input_Cook;
					}
					else if ((bPerformingRightSide && bLeftHeld) || (!bPerformingRightSide && bRightHeld))
					{
						m_bInterruptingWithOpposite = true;

						input = Input_Cook;
					}

				}
			}
			else
			{
				bool bCanDrop = pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetDriver() == pPed;

				bool bVehicleAttackDown = pControl->IsVehicleAttackInputDown() || (bCanDrop && pControl->GetVehicleDropProjectile().IsDown());
				bool bVehicleAttackReleased = pControl->IsVehicleAttackInputReleased();

				if( isOnMountorIsPassenger ? (pControl->GetPedAttack().IsReleased() || bVehicleAttackReleased) : bVehicleAttackReleased)
				{
					input = Input_Fire;
				}
				else if( isOnMountorIsPassenger ? (pControl->GetPedAttack().IsDown() || bVehicleAttackDown) : bVehicleAttackDown)
				{
					input = Input_Cook;
				}
				else if( isOnMountorIsPassenger ? pControl->GetPedTargetIsDown() || pControl->GetVehicleAim().IsDown() : pControl->GetVehicleAim().IsDown())
				{
					input = Input_Hold;
				}
#if RSG_PC
				else if (!isOnMountorIsPassenger && pControl->WasKeyboardMouseLastKnownSource() && pControl->GetVehicleTargetDown())
				{
					input = Input_Hold;
				}
#endif
				
				// B*2102892/2149628: Flag us to drop projectile if we press the drop input (and only if using alt driveby controls)
				if (bCanDrop)
				{
					if (pControl->GetVehicleDropProjectile().IsReleased())
					{
						input = Input_Fire;
						m_bWantsToDrop = true;
					}
				}
			}
		}
	}			

	return input;
}

//////////////////////////////////////////////////////////////////////////
// CClonedMountThrowProjectileInfo
//////////////////////////////////////////////////////////////////////////
CTaskFSMClone * CClonedMountThrowProjectileInfo::CreateCloneFSMTask()
{	
	CTaskMountThrowProjectile* pThrowProjectileTask = rage_new CTaskMountThrowProjectile(CWeaponController::WCT_Player, m_iFlags, m_target, m_bVehMeleeStartingRightSide, m_bStartedByVehicleMelee);
	
	pThrowProjectileTask->SetDrop(m_bDrop);
	pThrowProjectileTask->SetWindowSmashed(m_bWindowSmashed);

	return pThrowProjectileTask;
}
