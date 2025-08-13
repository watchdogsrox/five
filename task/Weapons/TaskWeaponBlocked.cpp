// Class header
#include "Task/Weapons/TaskWeaponBlocked.h"

#include "grcore/debugdraw.h"

// Game headers
#include "camera/gameplay/GameplayDirector.h"
#include "Debug/DebugScene.h"
#include "ik/IkManager.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Objects/Door.h"
#include "Peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PlayerInfo.h"
#include "phbound/boundcomposite.h"
#include "Physics/archetype.h"
#include "Physics/Physics.h"
#include "renderer/Mirrors.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "task/Combat/TaskCombatMelee.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/WeaponController.h"
#include "Vehicles/Vehicle.h"
#include "Weapons/Weapon.h"
#include "Objects/DoorTuning.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

////////////////////
//Main task weapon blocked info
////////////////////

CClonedWeaponBlockedInfo::CClonedWeaponBlockedInfo( CClonedWeaponBlockedInfo::InitParams const& initParams ) 
:
	m_weaponControllerType(initParams.m_type)
{
	SetStatusFromMainTaskState(initParams.m_state);	
}

CClonedWeaponBlockedInfo::CClonedWeaponBlockedInfo()
{}

CClonedWeaponBlockedInfo::~CClonedWeaponBlockedInfo()
{}

CTaskFSMClone *CClonedWeaponBlockedInfo::CreateCloneFSMTask()
{
    return rage_new CTaskWeaponBlocked(GetType());
}

CTask*	CClonedWeaponBlockedInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return rage_new CTaskWeaponBlocked(GetType());
}

//////////////////////////////////////////////////////////////////////////
// CTaskWeaponBlocked
//////////////////////////////////////////////////////////////////////////

const fwMvClipId CTaskWeaponBlocked::ms_WeaponBlockedClipId(ATSTRINGHASH("wall_block_idle", 0x0f9b74953));
const fwMvClipId CTaskWeaponBlocked::ms_WeaponBlockedNewClipId(ATSTRINGHASH("wall_block", 0x0ea90630e));
const fwMvNetworkDefId CTaskWeaponBlocked::ms_NetworkWeaponBlockedFPS(ATSTRINGHASH("TaskWeaponBlockedFPs", 0x893F2E7B));
const fwMvFloatId CTaskWeaponBlocked::ms_PitchId("Pitch",0x3F4BB8CC);

dev_float fInFrontMirrorToleranceWhenOpen = 0.1f;
dev_float fInFrontMirrorToleranceWhenBlocked = 0.4f;
dev_float fBehindMirrorTolerance = -0.1f;
dev_float fMirrorVerticalNormalTolerance = 0.25f;
dev_float fMirrorExtentWhenBlocked = 1.0f;
dev_float fMirrorExtentWhenOpen = 0.5f;
CTaskWeaponBlocked::CTaskWeaponBlocked(const CWeaponController::WeaponControllerType weaponControllerType)
: m_weaponControllerType(weaponControllerType)
{
#if FPS_MODE_SUPPORTED
	m_bUsingLeftHandGripIKFPS = false;
#endif
	SetInternalTaskType(CTaskTypes::TASK_WEAPON_BLOCKED);
}

bool CTaskWeaponBlocked::IsWeaponBlocked(const CPed* pPed, Vector3& vTargetPos, Vector3* pvOverridenStart, float fProbeLengthMultiplier, float fRadiusMultiplier, bool bCheckAlternativePos, bool bWantsToFire, bool bOverrideProbeLength, float fOverridenProbeLength)
{
	if (CTaskAimGunFromCoverIntro::ms_Tunables.m_DisableWeaponBlocking)
	{
		return false;
	}

	Assertf(vTargetPos.IsNonZero(), "IsWeaponBlocked: Invalid target parameter - is zero.\n");
	weaponAssert(pPed->GetWeaponManager());
	if(!pPed->GetWeaponManager()->GetEquippedWeapon())
	{
		// Early out - no weapon
		Assertf(false, "IsWeaponBlocked: Invalid call - no weapon equipped.\n");
		return false;
	}

	// Cache the weapon info
	const CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if( !pEquippedWeapon )
		return false;

	const CWeaponInfo* pWeaponInfo = pEquippedWeapon->GetWeaponInfo();
	if( !pWeaponInfo )
		return false;

	bool bPedInCover = pPed->GetIsInCover();
	if(bPedInCover)
	{
		//If we are coming out of cover then don't update as probe position won't be correct.
		CTaskAimGunFromCoverIntro* pCoverIntro = static_cast<CTaskAimGunFromCoverIntro*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_FROM_COVER_INTRO));
		if(pCoverIntro)
		{
			return false;
		}	
	}

	if(pPed->IsLocalPlayer())
	{
		//When exiting a vehicle disable blocked until the player is out of the vehicle fully
		static dev_s32 sfDisableBlockedWhilstExitingVehicleTimer = 250;
		if(pPed->IsExitingVehicle() || (pPed->GetPlayerInfo()->GetTimeExitVehicleTaskFinished() + sfDisableBlockedWhilstExitingVehicleTimer) > fwTimer::GetTimeInMilliseconds() )
		{
			return false;
		}
	}

	// Check if local peds are within an air defence zone (don't trigger this for unarmed)
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere) && !pWeaponInfo->GetIsUnarmed())
	{
		return true;
	}

	float fPitchRatio = 0.5f;
	const CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask(false);
	if (pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
		fPitchRatio = static_cast<const CTaskMotionAiming*>(pMotionTask)->GetCurrentPitch();	

	Vector3 vAimOffset = pWeaponInfo->GetAimOffset( fPitchRatio FPS_MODE_SUPPORTED_ONLY(, pPed->IsFirstPersonShooterModeEnabledForPlayer(false) ? (CPedMotionData::eFPSState)(pPed->GetMotionData()->GetFPSState()) : CPedMotionData::FPS_MAX) );

	bool bInHighCover = false;
	bool bIsFPS = false;
#if FPS_MODE_SUPPORTED
	bIsFPS = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED

	const bool bUsingLongWeapon = pWeaponInfo->GetIsLongWeapon();
	if (bPedInCover)
	{
#if FPS_MODE_SUPPORTED
		bool bUseAimDirectlyOffsetForWeaponsInHighCover = bIsFPS;
		if (bUseAimDirectlyOffsetForWeaponsInHighCover)
		{
			const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
			bUseAimDirectlyOffsetForWeaponsInHighCover = pCoverTask ? pCoverTask->IsCoverFlagSet(CTaskCover::CF_AimDirectly) : false;
		}
#endif // FPS_MODE_SUPPORTED
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_ApplyCoverWeaponBlockingOffsets) FPS_MODE_SUPPORTED_ONLY(|| bUseAimDirectlyOffsetForWeaponsInHighCover))
		{
			bool bInHighCover = pPed->GetCoverPoint() && pPed->GetCoverPoint()->GetHeight() == CCoverPoint::COVHEIGHT_TOOHIGH;
#if FPS_MODE_SUPPORTED
			if (bInHighCover && bUseAimDirectlyOffsetForWeaponsInHighCover)
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_COVER_TUNE, WEAPON_FPS_OFFSET_AIM_DIRECTLY, 0.0f, -1.0f, 1.0f, 0.01f);
				vAimOffset.x = WEAPON_FPS_OFFSET_AIM_DIRECTLY;
			} else
#endif // FPS_MODE_SUPPORTED
			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft))
			{
				if(bUsingLongWeapon)
				{
					vAimOffset.x += bIsFPS ? CTaskInCover::ms_Tunables.m_WeaponLongBlockingOffsetInLeftCoverFPS : CTaskInCover::ms_Tunables.m_WeaponLongBlockingOffsetInLeftCover;
				}
				else
				{
					vAimOffset.x += bIsFPS ? CTaskInCover::ms_Tunables.m_WeaponBlockingOffsetInLeftCoverFPS : CTaskInCover::ms_Tunables.m_WeaponBlockingOffsetInLeftCover;
				}
			}
			else
			{
				vAimOffset.x += bIsFPS ? CTaskInCover::ms_Tunables.m_WeaponBlockingOffsetInRightCoverFPS : CTaskInCover::ms_Tunables.m_WeaponBlockingOffsetInRightCover;
			}
		}
	}

	//Using scoped animation
	bool bUsingScopedAnimation = false;
	const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
	if(appropriateWeaponClipSetId == pWeaponInfo->GetScopeWeaponClipSetId(*pPed) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock) )
	{
		bUsingScopedAnimation = true;
	}

	if((bCheckAlternativePos && !bUsingScopedAnimation) || (bUsingScopedAnimation && !bCheckAlternativePos) )
	{
		vAimOffset.z += CTaskAimGunOnFoot::GetTunables().m_AlternativeAnimBlockedHeight; 

		//Whilst in blocked and using a scoped animation we don't want to scale the radius of the probe
		CTaskWeaponBlocked* pBlockedTask = static_cast<CTaskWeaponBlocked*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WEAPON_BLOCKED));
		if(!pBlockedTask && bUsingScopedAnimation)
		{
			fRadiusMultiplier = 1.0f;
		}
	}

	// Construct the point we are aiming from
	Vector3 vStart( Vector3::ZeroType );
	if( pvOverridenStart )
		vStart = *pvOverridenStart;
	else
	{
		Vec3V vAimOffsetV( pPed->GetTransform().Transform3x3( VECTOR3_TO_VEC3V( vAimOffset ) ) );

		TUNE_GROUP_BOOL(AIMING_DEBUG, DISABLE_TORSO_IK_FOR_WEAPONBLOCK, false);
		if (pWeaponInfo->GetTorsoIKForWeaponBlock() && !DISABLE_TORSO_IK_FOR_WEAPONBLOCK)
		{
			QuatV qTorsoRotation( QuatVFromAxisAngle( Vec3V(V_UP_AXIS_WZERO), ScalarV(pPed->GetIkManager().GetTorsoYawDelta()) ) );
			vAimOffsetV = Transform(qTorsoRotation, vAimOffsetV);
		}

		vStart = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() + vAimOffsetV );
#if FPS_MODE_SUPPORTED
		float fPelvisOffset = pPed->ComputeExternallyDrivenPelvisOffsetForFPSStealth();
		vStart.z += fPelvisOffset;
#endif // FPS_MODE_SUPPORTED
	}

	// Aiming vector
	Vector3 vAimDirection = vTargetPos - vStart;
	// Slightly alter the offset to compensate for oscillation
	float fProbeLength = Lerp( fPitchRatio, pWeaponInfo->GetAimProbeLengthMin(), pWeaponInfo->GetAimProbeLengthMax() );

#if FPS_MODE_SUPPORTED
	// Override the end position with the aim offset end positions in weapon info.
	// Don't override it when reloading.
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading))
	{
		//Always use the idle end offset for probe direction for throwable projectiles 
		bool bThrownWeapon = pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsThrownWeapon();

		Vector3 vAimOffsetEndPos(Vector3::ZeroType);

		if(pPed->GetMotionData()->GetIsFPSLT())
		{
			vAimOffsetEndPos = pWeaponInfo->GetAimOffsetEndPosFPSLT(fPitchRatio);
		}
		
		if(pPed->GetMotionData()->GetIsFPSIdle() || (bThrownWeapon && vAimOffsetEndPos.Mag2() == 0.0f))
		{
			vAimOffsetEndPos = pWeaponInfo->GetAimOffsetEndPosFPSIdle(fPitchRatio);
		}

		if(vAimOffsetEndPos.Mag2() > 0.0f)
		{
			if((pWeaponInfo->GetIsGun2Handed() || pWeaponInfo->GetUseLeftHandIkWhenAiming()) && pPed->GetMotionData()->GetUsingStealth())
			{
				TUNE_GROUP_FLOAT(WEAPON_BLOCK, fFPSStealthZOffset2Handed, -0.30f, -1.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(WEAPON_BLOCK, fFPSStealthXOffset2Handed, 0.10f, -1.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(WEAPON_BLOCK, fFPSStealthYOffset2Handed, 0.12f, -1.0f, 1.0f, 0.01f);
				vAimOffsetEndPos.z += fFPSStealthZOffset2Handed;
				vAimOffsetEndPos.x += fFPSStealthXOffset2Handed;
				vAimOffsetEndPos.y += fFPSStealthYOffset2Handed;
			}
			Vec3V vEndV = pPed->GetTransform().Transform( VECTOR3_TO_VEC3V( vAimOffsetEndPos ) );
			float fPelvisOffset = pPed->ComputeExternallyDrivenPelvisOffsetForFPSStealth();
			vEndV.SetZ(vEndV.GetZf() + fPelvisOffset);
			vAimDirection = VEC3V_TO_VECTOR3(vEndV) - vStart;
			fProbeLength = vAimDirection.Mag()/2.0f;	// the probe length is doubled late in code. It looks like a bug, but changing it now will break third person.
		}
	}
#endif // FPS_MODE_SUPPORTED

	// Work out the weapon pitch and heading from the aiming vector
	float fDesiredPitch = rage::Atan2f(vAimDirection.z, vAimDirection.XYMag());
	float fDesiredHeading = rage::Atan2f(-vAimDirection.x, vAimDirection.y);

	// Aim-grenade thows from cover: use a probe length of 0.5m
	if (bOverrideProbeLength)
	{
		fProbeLength = fOverridenProbeLength;
	}

	static dev_float s_fCoverExtraDoorPushLength = 0.25f;	//extra extension applied to probe used only for opening the door, not aim blocking
	if (bPedInCover)
	{
		if (bInHighCover && bUsingLongWeapon)
		{
			fProbeLength += bIsFPS ? CTaskInCover::ms_Tunables.m_WeaponBlockingLengthLongWeaponOffsetFPS : CTaskInCover::ms_Tunables.m_WeaponBlockingLengthLongWeaponOffset;
		}
		else
		{
			fProbeLength += bIsFPS ? CTaskInCover::ms_Tunables.m_WeaponBlockingLengthOffsetFPS	: CTaskInCover::ms_Tunables.m_WeaponBlockingLengthOffset;
		}
		fProbeLength += s_fCoverExtraDoorPushLength; 
	}

	fProbeLength += (fProbeLength * fProbeLengthMultiplier);

#if FPS_MODE_SUPPORTED
	// Add suppressor length in FPS
	if(!bOverrideProbeLength && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pEquippedWeapon->GetSuppressorComponent())
	{
		const CDynamicEntity* pSuppressor = pEquippedWeapon->GetSuppressorComponent()->GetDrawable();
		if(pSuppressor)
		{
			float fSuppressorLength = pSuppressor->GetBoundingBoxMax().x - pSuppressor->GetBoundingBoxMin().x;
			fProbeLength += fSuppressorLength;
		}
	}
#endif // FPS_MODE_SUPPORTED

	Vector3 vEnd( 0.0f, fProbeLength, 0.0f );
	vEnd.RotateX( fDesiredPitch );
	vEnd.RotateZ( fDesiredHeading );

	// Local to world space
	vEnd += vStart;

	const s32 iCollisionFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_GLASS_TYPE; // Switched PED->RAGDOLL some peds may be down low and the ped wants to fire over them. PED bounds block, ragdoll don't B* 343089 [2/10/2012 musson]
	s32 iIgnoreFlags    = 0;

	if(bWantsToFire)
	{
		iIgnoreFlags = WorldProbe::LOS_IGNORE_SHOOT_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU_FX;
	}

	static dev_float MIN_CAPSULE_RADIUS = 0.06f;
	float fCapsuleRadius = MIN_CAPSULE_RADIUS;
	const CObject* pObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if( pObject )
	{
		const Vector3 boundingBoxMax = pObject->GetBoundingBoxMax();
		const Vector3 boundingBoxMin = pObject->GetBoundingBoxMin();

		// For some odd reason all the weapons are modeled with x as forward
		fCapsuleRadius = abs( boundingBoxMax.y - boundingBoxMin.y ) * 0.5f;

		// Unfortunately not all weapon capsule sizes mesh well with cover (wall on right side)
		// The idle animation has a lot of lateral movement which will cause oscillation between
		// the weapon blocked and unblocked states. (maybe ask animation to tone down the lateral movement?)
		fCapsuleRadius = MAX( fCapsuleRadius, MIN_CAPSULE_RADIUS );
	}
	
	fCapsuleRadius *= fRadiusMultiplier;
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		float fAimProbeRadiusOverride = pWeaponInfo->GetAimProbeRadiusOverride((CPedMotionData::eFPSState)(pPed->GetMotionData()->GetFPSState()), pPed->GetMotionData()->GetUsingStealth());
		if(fAimProbeRadiusOverride > 0.0f)
		{
			fCapsuleRadius = fAimProbeRadiusOverride;
		}
		else if(fAimProbeRadiusOverride < 0.0f)	// cancel probe if the radius override value is less than zero
		{
			return false;
		}
	}
#endif	// FPS_MODE_SUPPORTED

#if DEBUG_DRAW
	TUNE_GROUP_BOOL(AIMING_DEBUG, RENDER_BLOCKING_CAPSULE, false);
#endif
	const s32 iNumIntersections = 16;
	WorldProbe::CShapeTestHitPoint capsuleIsects[iNumIntersections];
	WorldProbe::CShapeTestResults capsuleResults(capsuleIsects, iNumIntersections);
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	capsuleDesc.SetResultsStructure(&capsuleResults);
	capsuleDesc.SetCapsule(vStart, vEnd, fCapsuleRadius);
	capsuleDesc.SetIncludeFlags(iCollisionFlags);
	capsuleDesc.SetExcludeEntity(pPed);
	capsuleDesc.SetOptions(iIgnoreFlags);
	capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetDoInitialSphereCheck(true);

	// Make sure we don't get blocked by entities with no collision
	// since shapetests don't take this into account
	bool bIsBlocked = false;
	bool bApplyingDoorImpulse = false;
	WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

	bool bWasSniperBlocked = false;
	if(pEquippedWeapon->GetHasFirstPersonScope())
	{
		CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
		bWasSniperBlocked = pAimGunTask && pAimGunTask->GetIsWeaponBlocked();
	}

	// Do weapon blocking in FPS mode even if we have a scope attachment
	bool bFPSModeNotAiming = false;
#if FPS_MODE_SUPPORTED
	bFPSModeNotAiming = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !(pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsAiming(false));
#endif	//FPS_MODE_SUPPORTED

	for(WorldProbe::ResultIterator it = capsuleResults.begin(); it < capsuleResults.last_result() && !bIsBlocked; ++it)
	{
		bApplyingDoorImpulse = false;
		Assert(it->GetHitDetected());
		if(it->GetHitInst() && it->GetHitInst() != pPed->GetCurrentPhysicsInst())
		{
			CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());

			// Could still hit ped capsule if shape test takes the frag inst as exception over phys inst.
			if(pHitEntity)
			{
				// don't allow ghosted entities in MP to block our weapon
				if (NetworkInterface::IsGameInProgress() && NetworkInterface::AreInteractionsDisabledInMP(*pPed, *pHitEntity))
				{
					continue;
				}

				if (pEquippedWeapon->GetHasFirstPersonScope() && !bFPSModeNotAiming)
				{
					// 		// the mirror check is not accurate, and there are building glass walls also considered as mirrors. Only enable the mirror check when player
					// 		// is inside building.
					if(pHitEntity->GetIsTypeBuilding() && pPed->GetIsInInterior())
					{
						const Vec3V* mirrorCorners = CMirrors::GetMirrorCorners();
						const Vec4V  mirrorPlane   = CMirrors::GetMirrorPlane();
						float fDistanceToMirror = PlaneDistanceTo(mirrorPlane, it->GetHitPositionV()).Getf();
						if(fabs(mirrorPlane.GetZf()) < fMirrorVerticalNormalTolerance && fDistanceToMirror > fBehindMirrorTolerance 
							&& fDistanceToMirror < (bWasSniperBlocked ? fInFrontMirrorToleranceWhenBlocked : fInFrontMirrorToleranceWhenOpen))
						{
							Vec3V vHitPosOnMirror = PlaneProject(mirrorPlane, it->GetHitPositionV());
							spdAABB mirrorBoundingBox(mirrorCorners[0], mirrorCorners[0]);
							mirrorBoundingBox.GrowPoint(mirrorCorners[1]);
							mirrorBoundingBox.GrowPoint(mirrorCorners[2]);
							mirrorBoundingBox.GrowPoint(mirrorCorners[3]);
							mirrorBoundingBox.GrowUniform(ScalarV(bWasSniperBlocked ? fMirrorExtentWhenBlocked : fMirrorExtentWhenOpen));
							if(!mirrorBoundingBox.ContainsPoint(vHitPosOnMirror))
							{
								// Skip the wall block check for sniper when it's not pointing into a mirror
								continue;
							}
#if DEBUG_DRAW
							else
							{
								if(RENDER_BLOCKING_CAPSULE)
								{
									ms_debugDraw.AddOBB(mirrorBoundingBox.GetMin(), mirrorBoundingBox.GetMax(), Mat34V(V_IDENTITY), Color_red, 1);
									ms_debugDraw.AddArrow(mirrorBoundingBox.GetCenter(), mirrorBoundingBox.GetCenter() + mirrorPlane.GetXYZ(), 0.25f, Color_red, 1);
								}
							}
#endif
						}
						else
						{
							// Skip the wall block check for sniper when it's not pointing into a mirror
							continue;
						}
					}
					else
					{
						// Skip the wall block check for sniper when it's not pointing into a mirror
						continue;
					}
				}

				if( pHitEntity->GetIsTypeObject() && ((CObject*)pHitEntity)->IsADoor() )
				{
					CDoor* pDoor = static_cast<CDoor*>( pHitEntity );
					const CDoorTuning* pDoorTuning = pDoor->GetTuning();
					
					// Make sure the door is unlocked and is of a particular type
					int nDoorType = pDoor->GetDoorType();
					if( !pDoor->IsBaseFlagSet( fwEntity::IS_FIXED ) && 
						( nDoorType == CDoor::DOOR_TYPE_STD || nDoorType == CDoor::DOOR_TYPE_STD_SC ) &&
						( !pDoorTuning || !pDoorTuning->m_Flags.IsFlagSet( CDoorTuning::DontCloseWhenTouched ) ) )
					{
						float fCurrentMoveBlendRatioSq = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
						static dev_float fOpenDoorThreshold = 0.9f;
						static dev_float fMoveBlendRatioThreshold = 0.145f;  // Looks like the min movement ratio is 0.15
						if((bPedInCover || fCurrentMoveBlendRatioSq > rage::square( fMoveBlendRatioThreshold )) && 
							fabs( pDoor->GetDoorOpenRatio() ) < fOpenDoorThreshold )
						{
							// Determine if we haven't already set the target ratio
							float fTargetDoorRatio = CTaskOpenDoor::IsPedInFrontOfDoor( pPed, pDoor ) ? -1.0f : 1.0f;
							if( pDoor->GetTargetDoorRatio() != fTargetDoorRatio )
							{
								static dev_float fPushingForce = 175.0f;
								float fImpulseForce = MAX( fCurrentMoveBlendRatioSq, MOVEBLENDRATIO_WALK ) * fPushingForce;
								Vec3V vHitImpulse = Scale( pDoor->GetTransform().GetB(), ScalarV( fTargetDoorRatio * fImpulseForce ) );

								Vec3V vHingeVector = pDoor->GetTransform().GetA(); 	
								if( Abs( pDoor->GetBoundingBoxMin().x ) > Abs( pDoor->GetBoundingBoxMax().x ) )
								{
									vHingeVector = Negate( vHingeVector );
									vHitImpulse = Negate( vHitImpulse );
								}
								
								float fDoorWidth = Abs( pDoor->GetBoundingBoxMax().x - pDoor->GetBoundingBoxMin().x );
								Vec3V vDoorEdge = Scale( vHingeVector, ScalarV( fDoorWidth ) );
								pDoor->ApplyImpulse( VEC3V_TO_VECTOR3( vHitImpulse ), VEC3V_TO_VECTOR3( vDoorEdge ), it->GetHitComponent() );

								if (!bPedInCover)
									pDoor->SetCloseDelay( CDoor::ms_fPlayerDoorCloseDelay );
								pDoor->SetTargetDoorRatio( fTargetDoorRatio, false );
								
								// Make sure the door doesn't latch shut
								pDoor->SetDoorControlFlag( CDoor::DOOR_CONTROL_FLAGS_STUCK, true );
								pDoor->SetDoorControlFlag( CDoor::DOOR_CONTROL_FLAGS_LATCHED_SHUT, false );

								if(pDoor->GetDoorAudioEntity())
								{
									pDoor->GetDoorAudioEntity()->EntityWantsToOpenDoor(it->GetHitPosition(), pPed->GetVelocity().Mag());
									pDoor->GetDoorAudioEntity()->TriggerDoorImpact(pPed, it->GetHitPositionV(), true);
								}

								static dev_u32 siWeaponBlockDelay = 500;
								const_cast<CPed*>(pPed)->SetWeaponBlockDelay( fwTimer::GetTimeInMilliseconds() + siWeaponBlockDelay );
							}
						}
					}
				}

				//Handle peds on bikes/quads as they have collision turned off
				bool bBikePed = false;
				if( pHitEntity->GetIsTypePed() )
				{
					CPed* pPed = static_cast<CPed*>(pHitEntity);
					CVehicle* pVehicle = pPed->GetVehiclePedInside();
					if(pVehicle)
					{
						VehicleType vehType = pVehicle->GetVehicleType();
						if(vehType == VEHICLE_TYPE_BIKE ||  vehType == VEHICLE_TYPE_QUADBIKE || vehType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
						{
							bBikePed = true;
						}
					}
				}

				//B*1698454: Special-case check for jetski steering handlebars
				bool bJetskiHandlebars = false;
				if (pHitEntity->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pHitEntity);
					if (pVehicle && pVehicle->GetIsJetSki())
					{
						int iHitComp = it->GetHitComponent();
						const int iSteeringColumnIndex = 5;
						if (iHitComp == iSteeringColumnIndex)
						{
							bJetskiHandlebars = true;
						}
					}
				}

				if( pPed->GetWeaponBlockDelay() < fwTimer::GetTimeInMilliseconds() && (pHitEntity->IsCollisionEnabled() || bBikePed) && !bJetskiHandlebars)
				{
					bool bDoPedBlocking = true;
					if(pHitEntity->GetIsTypePed())
					{
						if (pWeaponInfo->GetDisablePlayerBlockingInMP() && bWantsToFire)
						{
							//! If allowing damage against other ped, don't block.
							if(pPed->IsAllowedToDamageEntity(pEquippedWeapon->GetWeaponInfo(), pHitEntity))
							{
								bDoPedBlocking = false;
							}
						}

						//B* 1729304 - Hacked solution; don't set as blocked if we have a height difference of greater than 0.82f
						float fHeightDifference = Abs(pPed->GetTransform().GetPosition().GetZf() - pHitEntity->GetTransform().GetPosition().GetZf());
						TUNE_GROUP_FLOAT(WEAPON_BLOCK, fWeaponBlockHeight, 0.82f, 0.0f, 5.0f, 0.01f);
						if (fHeightDifference > fWeaponBlockHeight)
						{
							bDoPedBlocking = false;
						}
					}

					if (!pHitEntity->GetIsTypePed() || bDoPedBlocking)
					{
						if (bPedInCover)
						{
							float depth = it->GetHitDepth();
							if (depth > s_fCoverExtraDoorPushLength)
								bIsBlocked = true;
						}
						else
						{
							bIsBlocked = true;
						}
					}					
				}

				//Don't allow the weapon to block against foliage, in this instance a sprinkler
				if (it->GetHitInst()->GetArchetype() && it->GetHitInst()->GetArchetype()->GetTypeFlags() & ArchetypeFlags::GTA_FOLIAGE_TYPE)
				{
					if (it->GetHitInst()->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE)
					{
						phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(it->GetHitInst()->GetArchetype()->GetBound());
						if(pBoundComposite->GetTypeAndIncludeFlags() && pBoundComposite->GetTypeFlags((it->GetHitComponent())&ArchetypeFlags::GTA_FOLIAGE_TYPE))
						{
							bIsBlocked = false;
						}
					}
				}
			}
		}
	}

#if DEBUG_DRAW
	if(RENDER_BLOCKING_CAPSULE)
	{
		Color32 color = bIsBlocked ? Color_red : Color_green;
		ms_debugDraw.AddSphere(RCC_VEC3V(vStart), fCapsuleRadius, color, 1, 0, false);
		ms_debugDraw.AddSphere(RCC_VEC3V(vEnd), fCapsuleRadius, color, 1, 0, false);
		ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd),color, 1);

		static float t = 0.0f;
		static float fRate = 1.0f;
		t += fwTimer::GetTimeStep() * fRate;
		t = fmodf(t, 1.0f);
		Vector3 vSweptSphere(VEC3_ZERO); 
		vSweptSphere.x = Lerp(t, vStart.x,  vEnd.x);
		vSweptSphere.y = Lerp(t, vStart.y,  vEnd.y);
		vSweptSphere.z = Lerp(t, vStart.z,  vEnd.z);
		ms_debugDraw.AddSphere(RCC_VEC3V(vSweptSphere), fCapsuleRadius, color, 1, 0, false);
	}
#endif // DEBUG_DRAW

	// Return if we are blocked
	return bIsBlocked;
}

CTaskWeaponBlocked::FSM_Return CTaskWeaponBlocked::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	weaponAssert(pPed->GetWeaponManager());
	if(!pPed->GetWeaponManager()->GetEquippedWeapon())
	{
		//Debug log
		weaponDebugf2("CTaskWeaponBlocked::ProcessPreFSM Quit, ped[%s]",pPed->GetModelName());
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskWeaponBlocked::FSM_Return CTaskWeaponBlocked::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Blocked)
			FSM_OnEnter
				return StateBlockedOnEnter(pPed);
			FSM_OnUpdate
				return StateBlockedOnUpdate(pPed);
		FSM_State(State_Blocked_FPS)
			FSM_OnEnter
				return StateBlockedFPSOnEnter(pPed);
			FSM_OnUpdate
				return StateBlockedFPSOnUpdate(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskWeaponBlocked::CleanUp()
{
	CPed* pPed = GetPed();
	if(pPed != NULL)
	{
		float fBlendOutDelta = SLOW_BLEND_DURATION;
		bool bShouldSetBlockFiringTime = false;

		// Blend out the blocked clip
		if( GetClipHelper() )
		{
			fBlendOutDelta = pPed->IsLocalPlayer() ? SLOW_BLEND_OUT_DELTA : NORMAL_BLEND_OUT_DELTA;
			StopClip(fBlendOutDelta);

			fBlendOutDelta = 1.0f / Abs(fBlendOutDelta);
			bShouldSetBlockFiringTime = true;
		}

		// Blend out of move network for FPS mode
		if( m_MoveWeaponBlockedFPSNetworkHelper.GetNetworkPlayer() )
		{
			fBlendOutDelta = NORMAL_BLEND_DURATION;
			pPed->GetMovePed().ClearTaskNetwork(m_MoveWeaponBlockedFPSNetworkHelper, fBlendOutDelta);
			m_MoveWeaponBlockedFPSNetworkHelper.ReleaseNetworkPlayer();

			bShouldSetBlockFiringTime = true;
		}

		// Set the minimum delay to allow to fire after blocking the gun
		// This will prevent issues like firing the rocket launcher when it's still not in the firing position
		if(bShouldSetBlockFiringTime)
		{
			CTaskGun* pGunTask = static_cast<CTaskGun*>( pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN) );
			if(pGunTask)
			{
				pGunTask->SetBlockFiringTime( fwTimer::GetTimeInMilliseconds() + (u32)(fBlendOutDelta * 1000.0f) );
			}
		}
	}	
}

#if !__FINAL
const char * CTaskWeaponBlocked::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Blocked",
		"Blocked_FPS",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTaskWeaponBlocked::FSM_Return CTaskWeaponBlocked::StateBlockedOnEnter(CPed* pPed)
{
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	Assert(pWeapon);

	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	Assert(pWeaponInfo);

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		CArmIkSolver* pArmSolver = static_cast<CArmIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeArm));
		if (pArmSolver && pArmSolver->GetArmBlend(crIKSolverArms::LEFT_ARM) == 1.0f)
		{
			m_bUsingLeftHandGripIKFPS = true;
		}
	}
#endif	// FPS_MODE_SUPPORTED

	bool bInFirstPersonIdle = false;

	// don't bother with FP idle blocks on the clone side
	if (pPed->IsNetworkClone() && NetworkInterface::IsRemotePlayerInFirstPersonMode(*pPed, &bInFirstPersonIdle) && bInFirstPersonIdle)
	{
		return FSM_Quit;
	}

	if (!pWeaponInfo)
	{
		//Debug log
		weaponDebugf2("CTaskWeaponBlocked::StateBlockedOnEnter !pWeaponInfo, ped[%s]",pPed->GetModelName());
		return FSM_Quit;
	}

	if(m_MoveWeaponBlockedFPSNetworkHelper.IsNetworkActive())
	{
		pPed->GetMovePed().ClearTaskNetwork(m_MoveWeaponBlockedFPSNetworkHelper, NORMAL_BLEND_DURATION);
		m_MoveWeaponBlockedFPSNetworkHelper.ReleaseNetworkPlayer();
	}
	Assert(!m_MoveWeaponBlockedFPSNetworkHelper.IsNetworkActive());

	fwMvClipId wallBlockClipId = CLIP_ID_INVALID;
	fwMvClipSetId appropriateWeaponClipSetId = CLIP_SET_ID_INVALID;
	if(GetWallBlockClip(pPed, pWeaponInfo, appropriateWeaponClipSetId, wallBlockClipId)) 
	{
		const float fBlendInDelta = pPed->IsLocalPlayer() ? SLOW_BLEND_IN_DELTA : NORMAL_BLEND_IN_DELTA;
		StartClipBySetAndClip(pPed, appropriateWeaponClipSetId, wallBlockClipId, fBlendInDelta, CClipHelper::TerminationType_OnDelete);
		Assertf(GetClipHelper(), "Blocked weapon clip failed to start");
		return FSM_Continue;
	}
	else
	{
		if(!pPed->IsNetworkClone())
		{
			Assertf(0, "Missing weapon blocked clip for : %s : clip set Id %d : clip set name %s. Ped name: %s. Parent Task: %s", pWeaponInfo->GetName(), appropriateWeaponClipSetId.GetHash(), appropriateWeaponClipSetId.GetCStr(), GetPed() ? GetPed()->GetDebugName() : "NULL", GetParent() ? GetParent()->GetTaskName() : "NULL");
		}
		return FSM_Quit;
	}
}

bool CTaskWeaponBlocked::GetWallBlockClip(const CPed* pPed, const CWeaponInfo* pWeaponInfo, fwMvClipSetId& appropriateWeaponClipSetId_Out, fwMvClipId& wallBlockClipId_Out)
{
	wallBlockClipId_Out = CLIP_ID_INVALID;

	if (pWeaponInfo)
	{
		const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
		appropriateWeaponClipSetId_Out = appropriateWeaponClipSetId;

		if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedNewClipId))
		{
			wallBlockClipId_Out = ms_WeaponBlockedNewClipId;
		}
		else if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedClipId))
		{
			wallBlockClipId_Out = ms_WeaponBlockedClipId;
		}
	}

	return wallBlockClipId_Out != CLIP_ID_INVALID;
}

CTaskWeaponBlocked::FSM_Return CTaskWeaponBlocked::StateBlockedOnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::AnimFinished))
	{
		//Debug log
		weaponDebugf2("CTaskWeaponBlocked::StateBlockedOnUpdate AnimFinished, ped[%s]",pPed->GetModelName());
		return FSM_Quit;
	}

	switch(CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, false))
	{
	case WCS_Aim:
#if FPS_MODE_SUPPORTED
		// allow weapon swapping in FPS Idle state
		if(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !pPed->GetMotionData() || !pPed->GetMotionData()->GetIsFPSIdle())
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
		}
		break;
#endif	// FPS_MODE_SUPPORTED
	case WCS_Fire:
	case WCS_FireHeld:
		//no weapon switch allowed
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
		break;

	default:
		{
			//Debug log
			weaponDebugf2("CTaskWeaponBlocked::StateBlockedOnUpdate GetController != WCS_Aim || WCS_Fire || WCS_FireHeld, State:%i ped[%s]",CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, false),pPed->GetModelName());
			// No longer aiming or firing, so quit
			return FSM_Quit;
		}
	}
	
#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		SetState(State_Blocked_FPS);
	}
#endif	// FPS_MODE_SUPPORTED

	return FSM_Continue;
}

CTaskWeaponBlocked::FSM_Return CTaskWeaponBlocked::StateBlockedOnUpdateClone(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::AnimFinished))
	{
		// only leave the blocked task if the master ped is not running it
		if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WEAPON_BLOCKED))
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

CTaskWeaponBlocked::FSM_Return CTaskWeaponBlocked::StateBlockedFPSOnEnter(CPed* UNUSED_PARAM(pPed))
{
	// Blend out third person anim
	if(GetClipHelper())
	{
		StopClip();
	}

#if FPS_MODE_SUPPORTED
	if(!m_MoveWeaponBlockedFPSNetworkHelper.IsNetworkActive())
	{		
		SetUpFPSMoveNetwork();
	}
#endif	// FPS_MODE_SUPPORTED

	return FSM_Continue;
}

CTaskWeaponBlocked::FSM_Return CTaskWeaponBlocked::StateBlockedFPSOnUpdate(CPed* pPed)
{
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	Assert(pWeapon);

	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	Assert(pWeaponInfo);

	if (!pWeaponInfo)
	{
		//Debug log
		weaponDebugf2("CTaskWeaponBlocked::StateBlockedOnEnter !pWeaponInfo, ped[%s]",pPed->GetModelName());
		return FSM_Quit;
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if(!m_MoveWeaponBlockedFPSNetworkHelper.IsNetworkActive())
		{
			SetUpFPSMoveNetwork();
		}
		else
		{
			float fPitchRatio = 0.5f;
			const CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask(false);
			if (pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
			{
				fPitchRatio = static_cast<const CTaskMotionAiming*>(pMotionTask)->GetCurrentPitch();	
			}

			m_MoveWeaponBlockedFPSNetworkHelper.SetFloat(ms_PitchId, fPitchRatio);
		}
	}
	else
#endif	// FPS_MODE_SUPPORTED
	{
		SetState(State_Blocked);
	}

	return FSM_Continue;
}

void CTaskWeaponBlocked::SetUpFPSMoveNetwork()
{
	const CPed* pPed = GetPed();
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

	if(pWeaponInfo)
	{
		m_MoveWeaponBlockedFPSNetworkHelper.RequestNetworkDef(ms_NetworkWeaponBlockedFPS);

		if(m_MoveWeaponBlockedFPSNetworkHelper.IsNetworkDefStreamedIn(ms_NetworkWeaponBlockedFPS))
		{
			m_MoveWeaponBlockedFPSNetworkHelper.CreateNetworkPlayer(pPed, ms_NetworkWeaponBlockedFPS);
			pPed->GetMovePed().SetTaskNetwork(m_MoveWeaponBlockedFPSNetworkHelper, NORMAL_BLEND_DURATION);	
			const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
			m_MoveWeaponBlockedFPSNetworkHelper.SetClipSet(appropriateWeaponClipSetId);

			if (pWeaponInfo->GetIsMeleeFist())
			{
				crFrameFilter *pFilter = CTaskMelee::GetMeleeGripFilter(appropriateWeaponClipSetId);
				if (pFilter)
				{
					m_MoveWeaponBlockedFPSNetworkHelper.SetFilter(pFilter, CTaskHumanLocomotion::ms_MeleeGripFilterId);
				}
			}
		}
	}
}

CTaskInfo*	CTaskWeaponBlocked::CreateQueriableState() const
{
	CClonedWeaponBlockedInfo::InitParams initParams(GetState(), m_weaponControllerType);

	return rage_new CClonedWeaponBlockedInfo(initParams);
}

void CTaskWeaponBlocked::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_WEAPON_BLOCKED );
    CClonedWeaponBlockedInfo *weaponBlockedInfo = static_cast<CClonedWeaponBlockedInfo*>(pTaskInfo);

	m_weaponControllerType = weaponBlockedInfo->GetType();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

CTaskWeaponBlocked::FSM_Return	CTaskWeaponBlocked::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (iEvent == OnUpdate && GetStateFromNetwork() == State_Quit)
	{
		SetState(State_Quit);
	}

	FSM_Begin
		FSM_State(State_Blocked)
			FSM_OnEnter
				return StateBlockedOnEnter(pPed);
			FSM_OnUpdate
				return StateBlockedOnUpdateClone(pPed);
		FSM_State(State_Blocked_FPS)
			FSM_OnEnter
				return StateBlockedOnEnter(pPed);
			FSM_OnUpdate
				return StateBlockedOnUpdateClone(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskWeaponBlocked::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Quit);
}

CTaskFSMClone*	CTaskWeaponBlocked::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskWeaponBlocked(m_weaponControllerType);
}

CTaskFSMClone*	CTaskWeaponBlocked::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskWeaponBlocked(m_weaponControllerType);
}
