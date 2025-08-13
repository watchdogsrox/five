//
// camera/cinematic/CinematicFirstPersonIdleCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/CamInterface.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/Frame.h"
#include "camera/system/CameraMetadata.h"
#include "camera/camera_channel.h"
#include "event/Event.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedCapsule.h"
#include "physics/WorldProbe/worldprobe.h"
#include "task/System/TaskTypes.h"
#include "task/Motion/Locomotion/TaskBirdLocomotion.h"
#include "Weapons/WeaponManager.h"
#include "Weapons/Weapon.h"

#include "string/stringhash.h"
#include "fwanimation/animdefines.h"

#if SUPPORT_MULTI_MONITOR
#include "grcore/device.h"
#include "grcore/monitor.h"
#endif //SUPPORT_MULTI_MONITOR

#if __BANK
#include "vectormath/vec3v.h"
#include "grcore/debugdraw.h"
#endif

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicFirstPersonIdleCamera,0xCAADD2A3)

#define TURN_PLAYER_TO_CAMERA_HEADING		0

static const u16 FPI_PEDTYPE_MICHAEL  = (u16)PEDTYPE_PLAYER_0;
static const u16 FPI_PEDTYPE_FRANKLIN = (u16)PEDTYPE_PLAYER_1;
static const u16 FPI_PEDTYPE_TREVOR   = (u16)PEDTYPE_PLAYER_2;

static const u32 c_HashYoungToughWoman = ATSTRINGHASH("YoungAverageToughWoman", 0x0c568cee4);
static const u32 c_HashYoungWeakWoman = ATSTRINGHASH("YoungAverageWeakWoman", 0x30152ce1);
static const u32 c_HashYoungRichWoman = ATSTRINGHASH("YoungRichWoman", 0x015ec5425);
static const u32 c_HashMediumRichWoman = ATSTRINGHASH("MediumRichWoman", 0x40C629AE);
static const u32 c_HashFitnessFemale = ATSTRINGHASH("FitnessFemale", 0x01903f09e);
static const u32 c_HashMuscleWoman = ATSTRINGHASH("MuscleWoman", 0xeacaf858);

// TODO: move to AnimDefines.h/.cpp
static const fwMvClipSetId CLIP_SET_MOVE_F_SEXY("move_f@sexy@a", 0x89555CFA);
static const fwMvClipSetId CLIP_SET_MOVE_F_FAT_NO_ADD("move_f@fat@a_no_add", 0xafd0cf25);
static const fwMvClipSetId CLIP_SET_MOVE_F_FAT("move_f@fat@a", 0x1f299bf1);
static const fwMvClipSetId CLIP_SET_MOVE_F_CHUBBY("move_f@chubby@a", 0xf987d93e);
static const fwMvClipSetId CLIP_SET_GESTURE_F_FAT("ANIM_GROUP_GESTURE_F_FAT", 0x1c4a8715);

static const u32 c_PedCollisionFlags = ((ArchetypeFlags::GTA_ALL_TYPES_WEAPON | ArchetypeFlags::GTA_RAGDOLL_TYPE) & ~(ArchetypeFlags::GTA_GLASS_TYPE));
static const u32 c_VehCollisionFlags = (ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE);
static const u32 c_VehExtraCollisionFlags = (ArchetypeFlags::GTA_PED_INCLUDE_TYPES & ~(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_GLASS_TYPE));

// TODO: add vehicle type scale to metadata?
float GetVehicleTypeScale(const CVehicle* pVehicle)
{
	float fReturn = 1.0f;
	switch (pVehicle->GetVehicleType())
	{
		case VEHICLE_TYPE_PLANE:
			fReturn = 4.0f;
			break;

		case VEHICLE_TYPE_HELI:
			fReturn = 2.0f;
			break;

		case VEHICLE_TYPE_BOAT:
			fReturn = 3.0f;
			break;

		case VEHICLE_TYPE_SUBMARINE:
			fReturn = 5.0f;
			break;

		case VEHICLE_TYPE_TRAIN:
			fReturn = 4.0f;
			break;

		default:
			break;
	}

	if(pVehicle->GetIsInWater() && (pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE))
	{
		fReturn = 3.0f;
	}

	return fReturn;
}

camCinematicFirstPersonIdleCamera::camCinematicFirstPersonIdleCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicFirstPersonIdleCameraMetadata&>(metadata))
, m_fTurnRate(0.0f)
, m_fZoomStartFov(g_DefaultFov)
, m_LookatZOffset(0.0f)
, m_PrevChestZOffset(-FLT_MAX)
, m_PrevHeadZOffset(-FLT_MAX)
, m_StartAngleToTarget(-1.0f)
, m_MinTurnRateScale(0.0f)
#if USE_SIMPLE_DOF
, m_DistanceToTarget(0.0f)
#endif
, m_TimeSinceTargetLastUpdated(0)
, m_TimeSinceLastTargetSearch(0)
, m_TimeSinceLastEventSearch(0)
, m_LastTimeLosWasClear(0)
, m_TimeSinceTargetLastMoved(0)
, m_TimeOfLastNoTargetPosUpdate(0)
, m_ZoomInStartTime(0)
, m_ZoomOutStartTime(0)
, m_CheckOutGirlStartTime(0)
, m_LastTimeBirdWasTargetted(0)
, m_LastTimeVehicleWasTargetted(0)
, m_RandomTimeToSelectNewNoTargetPoint(m_Metadata.m_TimeToSelectNewNoTargetPoint/2)
, m_TimeSinceTargetRotationEnded(0)
, m_PedType(FPI_PEDTYPE_FRANKLIN)	// default to Franklin, should match default in camCinematicOnFootFirstPersonIdleShotMetadata metadata.
, m_LookingAtEntity(false)
, m_IsActive(false)
, m_BlendChestZOffset(false)
, m_BlendHeadZOffset(false)
, m_ZoomedInTA(false)
, m_HasCheckedOutGirl(false)
, m_ForceTargetPointDown(false)
, m_PerformNoTargetInterpolation(false)
, m_LockedOnTarget(false)
{
	m_LookatPosition.Zero();
	m_SavedOrientation.Identity();
	m_DesiredOrientation.Identity();

	for (int i = 0; i < FPI_MAX_TARGET_HISTORY; i++)
	{
		m_LastValidTarget[i] = NULL;
	}

	m_LookAtTarget = NULL;
}

camCinematicFirstPersonIdleCamera::~camCinematicFirstPersonIdleCamera()
{
	//const CPed* player = NULL;
	//if (m_AttachParent.Get() && m_AttachParent.Get()->GetIsTypePed())
	//{
	//	player = static_cast<const CPed*>(m_AttachParent.Get());
	//}
}

void camCinematicFirstPersonIdleCamera::Init(const camFrame& UNUSED_PARAM(frame), const CEntity* target)
{
	if(target == NULL)
	{
		return; 
	}

	if(!m_IsActive)
	{
		camBaseCamera::AttachTo(target); 
		m_IsActive = true;

		Matrix34 matPlayer;
		m_AttachParent.Get()->GetMatrixCopy(matPlayer);
		matPlayer.d.z += 0.80f;

		InitFrame(matPlayer);
	}
}

bool camCinematicFirstPersonIdleCamera::IsValid()
{
	return m_IsActive; 
}

void camCinematicFirstPersonIdleCamera::RotateDirectionAboutAxis(Vector3& vDirection, const Vector3& vAxis, float fAngleToRotate)
{
	// TODO: add to math library?  (is this what audBeamPyramid::RotateVectorAboutCustomAxis() is trying to do?)
	Quaternion qDir(vDirection.x, vDirection.y, vDirection.z, 0.0f);
	Quaternion qRotation;
	qRotation.FromRotation(vAxis, fAngleToRotate);

	Quaternion qResult = qRotation;
	qResult.Multiply(qDir);
	qResult.MultiplyInverse(qRotation);

	vDirection.Set(qResult.x, qResult.y, qResult.z);
	vDirection.NormalizeSafe();
}

bool camCinematicFirstPersonIdleCamera::Update()
{
	if(m_AttachParent.Get() == NULL ) //Safety check.
	{
		return false;
	}

	const CPed* player = NULL;
	Matrix34 matPlayer;
	if (m_AttachParent.Get()->GetIsTypePed())
	{
		player = static_cast<const CPed*>(m_AttachParent.Get());
		////player->GetBoneMatrix(matPlayer, BONETAG_HEAD);
		m_AttachParent.Get()->GetMatrixCopy(matPlayer);
		matPlayer.d.z += m_Metadata.m_CameraZOffset;
		m_PedType = (u16)player->GetPedType();
	}
	else
	{
		m_AttachParent.Get()->GetMatrixCopy(matPlayer);
		matPlayer.d.z += m_Metadata.m_CameraZOffset;
	}

	if (m_LookingAtEntity && m_LookAtTarget.Get() == NULL)
	{
		m_LookingAtEntity = false;
	}

	float fDistanceToTarget = 0.0f;

	// Handle turning towards target point.
	float fAngleToTarget = -1.0f;
	UpdateOrientation(player, matPlayer, fDistanceToTarget, fAngleToTarget);
	UpdateOrientationForNoTarget(player, matPlayer);

#if USE_SIMPLE_DOF
	// Update simple dof.
	if (m_Metadata.m_SimpleDofDistance > 0.0f)
	{
		float farInFocusDistance = m_DistanceToTarget + m_Metadata.m_SimpleDofDistance;
		float nearInFocusDistance = 0.0f;
		m_Frame.SetNearInFocusDofPlane(nearInFocusDistance);
		m_Frame.SetFarInFocusDofPlane(farInFocusDistance);
		m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldUseSimpleDof);
	}
	else
	{
		m_Frame.GetFlags().ClearFlag(camFrame::Flag_ShouldUseSimpleDof);
	}
#endif // USE_SIMPLE_DOF

	// Handle zooming.
	UpdateZoom(player, fDistanceToTarget, fAngleToTarget);

	Vector3 vForward = m_LookatPosition - matPlayer.d;
	vForward.NormalizeSafe();

	m_Frame.SetWorldMatrixFromFront(vForward);
	m_Frame.SetPosition(matPlayer.d);

	if (m_LookingAtEntity)
	{
		m_Frame.GetWorldMatrix().ToQuaternion(m_SavedOrientation);
	}

#if TURN_PLAYER_TO_CAMERA_HEADING
	UpdatePlayerHeading(const_cast<CPed*>(player));
#endif // TURN_PLAYER_TO_CAMERA_HEADING

#if SUPPORT_MULTI_MONITOR
	if (GRCDEVICE.GetMonitorConfig().isMultihead()) 
	{
		//if using multi head resolution (eye finity), reduce the near clip to 0.1 to avoid clipping into objects/walls
		m_Frame.SetNearClip(0.1f);
	}
#endif //SUPPORT_MULTI_MONITOR

	return m_IsActive;
}


void camCinematicFirstPersonIdleCamera::UpdateOrientation(const CPed* player, const Matrix34& matPlayer, float& fDistanceToTarget, float& fAngleToTarget)
{
	bool bTurningToTarget = false;
	Vector3 vTargetPosition;
	UpdateTargetPosition(player, vTargetPosition);

	/*bool isTargetValid =*/ ValidateLookatTarget(matPlayer.d);

	const CEntity* previousTarget = m_LookAtTarget.Get();
	u32 durationToStartNextSearch = (m_LookingAtEntity) ? m_Metadata.m_TimeToStartSearchWithTarget : m_Metadata.m_TimeToStartSearchNoTarget;
	if ((!m_LookingAtEntity || m_LockedOnTarget) && m_TimeSinceLastTargetSearch + durationToStartNextSearch < fwTimer::GetCamTimeInMilliseconds())
	{
		if (m_TimeSinceTargetRotationEnded + m_Metadata.m_MinTimeToWatchTarget < fwTimer::GetCamTimeInMilliseconds())
		{
			SearchForLookatTarget();
			m_TimeSinceLastTargetSearch = fwTimer::GetCamTimeInMilliseconds();
			m_TimeSinceLastEventSearch = m_TimeSinceLastTargetSearch;
		}
	}
	else if (m_TimeSinceLastEventSearch + m_Metadata.m_TimeToStartEventSearch < fwTimer::GetCamTimeInMilliseconds())
	{
		SearchForLookatEvent();
		m_TimeSinceLastEventSearch = fwTimer::GetCamTimeInMilliseconds();
	}

	// Turn towards target, respecting turn rate velocity.
	if (m_LookingAtEntity && m_LookAtTarget.Get())
	{
		if (previousTarget != m_LookAtTarget.Get())
		{
			ResetForNewTarget();
			UpdateTargetPosition(player, vTargetPosition);
			m_TimeSinceTargetRotationEnded  = fwTimer::GetCamTimeInMilliseconds();

			// When setting a new target, randomize time to look around after losing target.
			const u32 uMinTimeToSelectNewPoint = m_Metadata.m_TimeToSelectNewNoTargetPoint/3500;
			const u32 uMaxTimeToSelectNewPoint = m_Metadata.m_TimeToSelectNewNoTargetPoint/1500;
			m_RandomTimeToSelectNewNoTargetPoint = (u32)fwRandom::GetRandomNumberInRange((int)uMinTimeToSelectNewPoint, (int)uMaxTimeToSelectNewPoint);
			m_RandomTimeToSelectNewNoTargetPoint *= 1000;
		}

		// Recalculate target position.
		Vector3 vCamToNewTargetNorm = vTargetPosition - m_Frame.GetPosition();
		fDistanceToTarget = vCamToNewTargetNorm.Mag();
		if (fDistanceToTarget > 0.0f)
		{
		#if USE_SIMPLE_DOF
			m_DistanceToTarget = fDistanceToTarget;
		#endif
			Vector3 vCamForward = m_Frame.GetFront();
			vCamToNewTargetNorm.Scale(1.0f/fDistanceToTarget);

			float fAngle = rage::AcosfSafe(vCamForward.Dot(vCamToNewTargetNorm));
			if (fAngle > PI*0.50f)
			{
				// HACK: Target is not in front of us, so do a 2d rotation until it is in "view".
				vCamForward.z = 0.0f;
				vCamToNewTargetNorm.z = 0.0f;
				vCamForward.NormalizeSafe();
				vCamToNewTargetNorm.NormalizeSafe();

				fAngle = rage::AcosfSafe(vCamForward.Dot(vCamToNewTargetNorm));
			}
			fAngleToTarget = fAngle;

			const float c_fSmallAngleThresholdRadians = m_Metadata.m_ZoomAngleThreshold*DtoR*0.50f;
			if (previousTarget != m_LookAtTarget.Get())
			{
				m_StartAngleToTarget = fAngleToTarget;
				if (fAngleToTarget <= c_fSmallAngleThresholdRadians)
				{
					// For small angles, don't bother ramping up.
					m_StartAngleToTarget = fAngleToTarget * 2.0f;
				}
			}

			if (!IsNearZero(fAngle, SMALL_FLOAT))
			{
				bTurningToTarget = true;

				m_fTurnRate = m_Metadata.m_MaxAngularTurnRate;
				if (!m_LockedOnTarget)
				{
					float fInterp = (m_StartAngleToTarget > SMALL_FLOAT) ? (fAngle / m_StartAngleToTarget) : 0.0f;
					fInterp = Max(fInterp, 0.0f);

					float fMaxAngularTurnRate = m_Metadata.m_MaxAngularTurnRate;
					float fMinTurnRateScale = m_Metadata.m_MinTurnRateScalePeds;
					bool bIsTargetVehicle = false;
					if (!m_LockedOnTarget && m_StartAngleToTarget < m_Metadata.m_ZoomAngleThreshold*DtoR)
					{
						float fInitialMaxAngularTurnRateScaler = 2.0f;
						if (m_LookAtTarget.Get()->GetIsTypeVehicle())
						{
							fInitialMaxAngularTurnRateScaler = 5.0f;
						}
						if (m_StartAngleToTarget <= c_fSmallAngleThresholdRadians)
						{
							fInitialMaxAngularTurnRateScaler = 3.5f;
						}
						float fMaxAngularTurnRateScaler = RampValue(fInterp, 1.0f, 2.0f, fInitialMaxAngularTurnRateScaler, 1000.0f);
						fMaxAngularTurnRate = Min(m_StartAngleToTarget * RtoD * fMaxAngularTurnRateScaler,
												  m_Metadata.m_MaxAngularTurnRate);
						fMinTurnRateScale = 1.0f;
					}
					else if (m_LookAtTarget.Get()->GetIsTypeVehicle())
					{
						fMinTurnRateScale = m_Metadata.m_MinTurnRateScaleVehicle;
						bIsTargetVehicle = true;
					}
					else if (IsBirdTarget(m_LookAtTarget.Get()))
					{
						fMinTurnRateScale = Min(m_Metadata.m_MinTurnRateScalePeds + 0.07f, 1.0f);	// TODO: metadata?
					}

					if (fInterp > 1.0f)
					{
						// If we need to catch up to target, set a minimum scale until we are caught up.
						m_MinTurnRateScale = (!bIsTargetVehicle) ? 
												Max(m_Metadata.m_MinTurnRateScalePeds, 0.25f) :
												Max(m_Metadata.m_MinTurnRateScaleVehicle, 0.50f);
						fInterp = Min(fInterp - 1.0f, 0.50f);
					}
					fInterp = BellInOut(fInterp);
					if (fInterp < m_MinTurnRateScale)
					{
						fInterp = m_MinTurnRateScale;
					}
					else if (m_MinTurnRateScale > 0.0f)
					{
						// We have caught up, so clear the minimum so we can slow down once again as we approach.
						m_MinTurnRateScale = 0.0f;
					}
					float fScale = fMinTurnRateScale + (fInterp) * (1.0f - fMinTurnRateScale);
					m_fTurnRate = fMaxAngularTurnRate * fScale;
				}
				else
				{
					if (fDistanceToTarget <= m_Metadata.m_DistanceToForceTrackHead)
					{
						m_fTurnRate = Min(m_fTurnRate*2.0f, m_Metadata.m_MaxAngularTurnRate);
					}
					else if (fDistanceToTarget <= m_Metadata.m_DistanceToStartTrackHead)
					{
						float fInterp = (fDistanceToTarget-m_Metadata.m_DistanceToForceTrackHead) /
										(m_Metadata.m_DistanceToStartTrackHead-m_Metadata.m_DistanceToForceTrackHead);
						fInterp = Clamp(fInterp, 0.0f, 1.0f);
						float fScale = Lerp(fInterp, 1.0f, 2.0f);
						m_fTurnRate = Min(m_fTurnRate*fScale, m_Metadata.m_MaxAngularTurnRate);
					}
				}

				float fMaxAngleThisUpdate = m_fTurnRate * DtoR * fwTimer::GetCamTimeStep();
				if (!m_LockedOnTarget && fAngle > fMaxAngleThisUpdate)
				{
					// Need to limit look at position to turn rate.
					Vector3 vAxis;
					vAxis.Cross(vCamForward, vCamToNewTargetNorm);
					vAxis.NormalizeSafe();

					Vector3 rotatedCameraForward = m_Frame.GetFront();
					RotateDirectionAboutAxis(rotatedCameraForward, vAxis, fMaxAngleThisUpdate);
					m_LookatPosition = m_Frame.GetPosition() + (rotatedCameraForward * fDistanceToTarget);
				}
				else
				{
					m_LookatPosition = vTargetPosition;
					m_LockedOnTarget = true;
					m_TimeSinceTargetRotationEnded  = fwTimer::GetCamTimeInMilliseconds();
				}

				if (m_ZoomInStartTime == 0 && fAngle <= m_Metadata.m_ZoomAngleThreshold*DtoR)
				{
					m_ZoomInStartTime = fwTimer::GetCamTimeInMilliseconds();
					m_fZoomStartFov = m_Frame.GetFov();
				}
			}
		}
		else
		{
			ClearTarget();
		}

		// Keep setting timer while we have no target, so when we detach, we wait a bit before looking around.
		m_TimeOfLastNoTargetPosUpdate = fwTimer::GetCamTimeInMilliseconds();

		// Disable until we get a chance to pick new position.
		m_PerformNoTargetInterpolation = false;
	}
}

void camCinematicFirstPersonIdleCamera::UpdateOrientationForNoTarget(const CPed* UNUSED_PARAM(player), const Matrix34& UNUSED_PARAM(matPlayer))
{
	const float fDistanceToTest = 40.0f;	// meters
	if (!m_LookingAtEntity)
	{
		bool bSelectNewTargetPoint = m_TimeOfLastNoTargetPosUpdate + m_RandomTimeToSelectNewNoTargetPoint < fwTimer::GetCamTimeInMilliseconds();
		bool bForceTargetPointDown = false;
		float fCurrentPitch = m_Frame.ComputePitch();
		if (!bSelectNewTargetPoint && !m_ForceTargetPointDown && fCurrentPitch > ((float)m_Metadata.m_MaxNoTargetPitch * DtoR))
		{
			m_ForceTargetPointDown = m_TimeOfLastNoTargetPosUpdate + m_Metadata.m_TimeToSelectNewNoTargetPoint/16 < fwTimer::GetCamTimeInMilliseconds();
			bForceTargetPointDown = m_ForceTargetPointDown;
		}

		// Protect against bad data.
		u32 uTimeToReachNoTargetPoint = m_Metadata.m_TimeToReachNoTargetPoint;
		if (uTimeToReachNoTargetPoint >= m_Metadata.m_TimeToSelectNewNoTargetPoint)
		{
			uTimeToReachNoTargetPoint = m_Metadata.m_TimeToSelectNewNoTargetPoint/2;
		}

		if (bSelectNewTargetPoint || bForceTargetPointDown)
		{
			// Save current look at orientation for the interpolation.
			m_Frame.GetWorldMatrix().ToQuaternion(m_SavedOrientation);

			Vector3 vLookatPointToTest;
			Vector3 vPossibleLookatDirection;
			Vector3 vBestLookatPoint = m_LookatPosition;
			float   fFarthestLookatPointDistanceSq = 0.0f;
			float   fCurrentPointDistSq;

			const s32 sMaxTestAngles = 8;
			const float fTestAngles[sMaxTestAngles] = { 120.0f, -30.0f, -150.0f, 60.0f, 30.0f, -120.0f, -60.0f, 150.0f };

			// Make the direction 2d, so if we were looking up, we will come back down.
			Vector3 vBaseDirection = m_Frame.GetFront();
			vBaseDirection.z = 0.0f;
			vBaseDirection.NormalizeSafe();

			// Random pitch for no target look at direction.
			{
				const s32 c_RoundTo = 3;
				s32 pitch = fwRandom::GetRandomNumberInRange((int)0, (int)(m_Metadata.m_MaxNoTargetPitch/c_RoundTo));
				if (pitch > 0)
				{
					// Set pitch in c_RoundTo degree increments.
					pitch *= c_RoundTo;
					float fPitch = (float)pitch;
					Vector3 vRightAxis;
					vRightAxis.Cross(vBaseDirection, ZAXIS);
					RotateDirectionAboutAxis(vBaseDirection, vRightAxis, fPitch * DtoR);
				}
			}

			if (!bForceTargetPointDown)
			{
				// Randomize starting angle so, if all points pass collision test, we don't always pick the same direction.
				s32 index = fwRandom::GetRandomNumberInRange((int)0, (int)sMaxTestAngles-1);
				for (s32 counter = 0; counter < sMaxTestAngles; counter++)
				{
					// Rotate to get direction for test point.
					vPossibleLookatDirection = vBaseDirection;
					RotateDirectionAboutAxis(vPossibleLookatDirection, ZAXIS, fTestAngles[index] * DtoR);

					// Test against collision.
					vLookatPointToTest = m_Frame.GetPosition() + vPossibleLookatDirection * fDistanceToTest;
					if (TrimPointToCollision( vLookatPointToTest, (ArchetypeFlags::GTA_ALL_MAP_TYPES) ))
					{
						// Had collision, so have to calculate distance.
						fCurrentPointDistSq = (vLookatPointToTest - m_Frame.GetPosition()).Mag2();
					}
					else
					{
						fCurrentPointDistSq = fDistanceToTest*fDistanceToTest;
					}

					// Set best look at point based on distance.
					if (fCurrentPointDistSq > fFarthestLookatPointDistanceSq)
					{
						vBestLookatPoint = vLookatPointToTest;
						fFarthestLookatPointDistanceSq = fCurrentPointDistSq;
					}

					index++;
					if (index >= sMaxTestAngles)
					{
						index = 0;
					}
				}
			}
			else
			{
				vBestLookatPoint = m_Frame.GetPosition() + vBaseDirection * fDistanceToTest;
			}

			m_TimeOfLastNoTargetPosUpdate = fwTimer::GetCamTimeInMilliseconds();
			const u32 uMinTimeToSelectNewPoint = Max(uTimeToReachNoTargetPoint+1000, m_Metadata.m_TimeToSelectNewNoTargetPoint-5000)/1000;
			const u32 uMaxTimeToSelectNewPoint = m_Metadata.m_TimeToSelectNewNoTargetPoint/1000;
			m_RandomTimeToSelectNewNoTargetPoint = (u32)fwRandom::GetRandomNumberInRange((int)uMinTimeToSelectNewPoint, (int)uMaxTimeToSelectNewPoint);
			m_RandomTimeToSelectNewNoTargetPoint *= 1000;

			Vector3 vNewForward = vBestLookatPoint - m_Frame.GetPosition();
			vNewForward.Normalize();

			Matrix34 matNewWorld;
			camFrame::ComputeWorldMatrixFromFront(vNewForward, matNewWorld);
			matNewWorld.ToQuaternion(m_DesiredOrientation);
			m_PerformNoTargetInterpolation = true;
		}

		if (m_ForceTargetPointDown)
		{
			uTimeToReachNoTargetPoint = m_Metadata.m_TimeToReachNoTargetPoint/16;			// TODO: metadata?
		}

		// Drift to desired look at position.
		float fInterp = 0.0f;
		if (m_PerformNoTargetInterpolation)
		{
			fInterp = (float)(fwTimer::GetCamTimeInMilliseconds() - m_TimeOfLastNoTargetPosUpdate) /
					  (float)(m_Metadata.m_TimeToReachNoTargetPoint);
			fInterp = Clamp(fInterp, 0.0f, 1.0f);
			fInterp = SlowInOut(fInterp);
			Quaternion qResult;
			qResult.SlerpNear(fInterp, m_SavedOrientation, m_DesiredOrientation);

			Matrix34 matResult;
			matResult.FromQuaternion(qResult);
			m_LookatPosition = m_Frame.GetPosition() + matResult.b * 20.0f;					// TODO: metadata?
		}

		if (fInterp == 1.0f)
		{
			m_ForceTargetPointDown = false;
			m_DesiredOrientation = m_SavedOrientation;
			m_PerformNoTargetInterpolation = false;
		}

	#if USE_SIMPLE_DOF
		m_DistanceToTarget = Lerp(0.10f, m_DistanceToTarget, 0.0f);
	#endif
	}
	else
	{
		m_TimeOfLastNoTargetPosUpdate = fwTimer::GetCamTimeInMilliseconds();
	}
}

void camCinematicFirstPersonIdleCamera::InitFrame(const Matrix34& matPlayer)
{
	if (m_NumUpdatesPerformed == 0)
	{
	#if TURN_PLAYER_TO_CAMERA_HEADING
		// Init camera direction to player's facing direction. (use root matrix for now)
		Vector3 vForward2d = matPlayer.b;
	#else
		// Use previous camera's heading, esp. since player is no longer visible.
		Vector3 vForward2d = camInterface::GetFrame().GetFront();
	#endif // TURN_PLAYER_TO_CAMERA_HEADING
		vForward2d.z = 0.0f;
		vForward2d.NormalizeSafe();
		m_LookatPosition = matPlayer.d + vForward2d * 5.0f;

		//m_TimeSinceTargetLastUpdated = fwTimer::GetCamTimeInMilliseconds();
		m_TimeOfLastNoTargetPosUpdate = fwTimer::GetCamTimeInMilliseconds();
		m_Frame.SetWorldMatrixFromFront(vForward2d);
		m_Frame.SetPosition(matPlayer.d);

		m_Frame.GetWorldMatrix().ToQuaternion(m_SavedOrientation);
		m_DesiredOrientation = m_SavedOrientation;

	#if FPS_MODE_SUPPORTED
		const camBaseCamera* pRendereredCamera = camInterface::GetDominantRenderedCamera();
		if( pRendereredCamera && pRendereredCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) )
		{
			const camFrame& frame = pRendereredCamera->GetFrame();
			m_Frame.CloneFrom( frame );

			m_fZoomStartFov = m_Frame.GetFov();
			m_ZoomOutStartTime = fwTimer::GetCamTimeInMilliseconds();
		}
	#endif
	}
}


void camCinematicFirstPersonIdleCamera::UpdateZoom(const CPed* player, float fDistanceToTarget, float fAngleToTarget)
{
	float fZoomedInFov = m_Metadata.m_ZoomFov;
	bool bAngleValid = fAngleToTarget >= 0.0f && m_StartAngleToTarget > 1.0f*DtoR;
	if (m_LookingAtEntity && m_LookAtTarget.Get())
	{
		// Handle zooming in/out from closer zoom for female body parts. (T&A)  If zoom fov is negative, this feature is disabled.
		if (m_Metadata.m_ZoomTAFov > 0.0f && player && m_LookingAtEntity && m_LookAtTarget.Get()->GetIsTypePed())
		{
			const CPed* pTarget = static_cast<const CPed*>(m_LookAtTarget.Get());
			const bool bIsOppositeGender = player->IsMale() && player->IsMale() != pTarget->IsMale();
			if (bIsOppositeGender)
			{
				fZoomedInFov = m_Metadata.m_ZoomTAFov;
				if (fDistanceToTarget > m_Metadata.m_DistanceToStartTrackHead)
				{
					// If far enough away, zoom in on body parts, if not already zooming in.
					if (m_ZoomInStartTime + m_Metadata.m_TimeToZoomIn < fwTimer::GetCamTimeInMilliseconds() &&
						bAngleValid && m_LockedOnTarget)
					{
						m_ZoomInStartTime = fwTimer::GetCamTimeInMilliseconds();
						m_fZoomStartFov = m_Frame.GetFov();
					}
					m_ZoomedInTA = true;
				}
				else if (m_Frame.GetFov() < fZoomedInFov && m_ZoomedInTA)
				{
					// If zoomed in too far, zoom out from body parts, if we are zoomed in.
					m_ZoomInStartTime = fwTimer::GetCamTimeInMilliseconds();
					m_fZoomStartFov = m_Frame.GetFov();
					m_ZoomedInTA = false;
				}
			}
		}
	}

	if (m_LookingAtEntity && m_LookAtTarget.Get())
	{
		if (m_LookAtTarget.Get()->GetIsTypeVehicle())
		{
			const CVehicle* pTargetVehicle = static_cast<const CVehicle*>(m_LookAtTarget.Get());
			float fVehicleTypeScale = GetVehicleTypeScale(pTargetVehicle);
			float fMaxVehicleZoomOutDist = fVehicleTypeScale * m_Metadata.m_DistToZoomOutForVehicle;
			float fMinVehicleZoomOutDist = fVehicleTypeScale * m_Metadata.m_MinValidDistToTarget;

			if (fDistanceToTarget <= fMaxVehicleZoomOutDist)
			{
				// For vehicles, zoom out when too close.
				float fInterp = RampValueSafe(fDistanceToTarget, fMinVehicleZoomOutDist, fMaxVehicleZoomOutDist, 1.0f, 0.0f);
				fInterp = SlowInOut(fInterp);
				fZoomedInFov = Lerp(fInterp, fZoomedInFov, g_DefaultFov);
			}
		}
		else if (m_LookAtTarget.Get()->GetIsTypePed())
		{
			if (fDistanceToTarget <= m_Metadata.m_DistanceToStartTrackHead)
			{
				// For peds, zoom out when too close.
				const float c_fMinPedZoomDist = Max(2.0f, m_Metadata.m_DistanceToForceTrackHead*0.75f);	// TODO: metadata?
				float fInterp = RampValueSafe(fDistanceToTarget, c_fMinPedZoomDist, m_Metadata.m_DistanceToStartTrackHead, 1.0f, 0.0f);
				fInterp = SlowInOut(fInterp);
				fZoomedInFov = Lerp(fInterp, fZoomedInFov, g_DefaultFov);
			}
		}
	}

	// Zoom in on target.
	bAngleValid = fAngleToTarget >= 0.0f && m_StartAngleToTarget > m_Metadata.m_ZoomAngleThreshold*DtoR;
	if (!m_LookingAtEntity || m_ZoomInStartTime == 0)
	{
		m_Frame.SetFov(g_DefaultFov);
	}
	else if (!bAngleValid && m_ZoomInStartTime + m_Metadata.m_TimeToZoomIn < fwTimer::GetCamTimeInMilliseconds())
	{
		m_Frame.SetFov(fZoomedInFov);
	}
	else
	{
		// zoom in over time
		float fInterp = 0.0f;
		if (bAngleValid)
		{
			if (!m_LockedOnTarget)
			{
				float fAngleToStartZoom = Min(m_StartAngleToTarget, m_Metadata.m_ZoomAngleThreshold*DtoR);
				fInterp =	(fAngleToStartZoom - fAngleToTarget) / (fAngleToStartZoom - 1.0f*DtoR);
			}
			else
			{
				fInterp = 1.0f;
			}
		}
		else
		{
			fInterp =	(m_Metadata.m_TimeToZoomIn > 0) ? 
							(float)(fwTimer::GetCamTimeInMilliseconds() - m_ZoomInStartTime) /
							(float)m_Metadata.m_TimeToZoomIn :
							1.0f;
		}

		fInterp = Clamp( fInterp, 0.0f, 1.0f );
		fInterp = SlowInOut(fInterp);
		float finalFov = Lerp(fInterp, m_fZoomStartFov, fZoomedInFov);
		m_Frame.SetFov(finalFov);
	}

	// Zoom out when target lost.
	// TODO: separate zoom out duration in metadata?
	if (m_ZoomOutStartTime != 0)
	{
		if (m_ZoomOutStartTime + m_Metadata.m_TimeToZoomIn < fwTimer::GetCamTimeInMilliseconds())
		{
			m_Frame.SetFov(g_DefaultFov);
			m_ZoomOutStartTime = 0;
		}
		else
		{
			// zoom out over time
			float fInterp =	(m_Metadata.m_TimeToZoomIn > 0) ? 
								(float)(fwTimer::GetCamTimeInMilliseconds() - m_ZoomOutStartTime) /
								(float)m_Metadata.m_TimeToZoomIn :
								1.0f;
			fInterp = Clamp( fInterp, 0.0f, 1.0f );
			fInterp = SlowInOut(fInterp);
			float finalFov = Lerp(fInterp, m_fZoomStartFov, g_DefaultFov);
			m_Frame.SetFov(finalFov);
		}
	}
}

void camCinematicFirstPersonIdleCamera::UpdatePlayerHeading(CPed* player)
{
	if (player && !player->GetUsingRagdoll())
	{
		// Force player's body to face where camera is looking, so player will face that direction when camera is done.
		// (also avoids seeing player shoulder when only head is invisible)
		// If we need to make the player completely visible, look in CTaskAimGunOnFoot::UpdateVisibility(CPed& ped)
		float fCameraHeading = m_Frame.ComputeHeading();
		player->SetHeading(fCameraHeading);
		player->SetDesiredHeading(fCameraHeading);

		// Changing the player kicks us out of idle camera, so we need to force idle state here.
		player->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
	}
}

void camCinematicFirstPersonIdleCamera::UpdateTargetPosition(const CPed* player, Vector3& vTargetPosition)
{
	// Logic for determining what part of target to look at or what part of environment to look at.
	if (m_LookingAtEntity && m_LookAtTarget.Get())
	{
		// Recalculate target position.
		m_LookatPosition = VEC3V_TO_VECTOR3(m_LookAtTarget.Get()->GetTransform().GetPosition());
		if (m_LookAtTarget->GetIsTypePed())
		{
			Matrix34 matTarget;
			const CPed* pTarget = static_cast<const CPed*>(m_LookAtTarget.Get());

			// Default to look at target's chest.
			pTarget->GetBoneMatrix(matTarget, BONETAG_SPINE3);
			float fChestZOffset = (matTarget.d.z - m_LookatPosition.z) + m_Metadata.m_PedChestZOffset;

			const bool bIsStanding     = pTarget->GetIsStanding() && !pTarget->GetIsSitting() && !IsScenarioPedProne(pTarget);
			const bool bIsMoving       = pTarget->GetVelocity().Mag2() > SMALL_FLOAT;
			const bool bIsTargetStandingStill = !bIsMoving && bIsStanding;
			const bool bIsOppositeGender = player->IsMale() != pTarget->IsMale();

			// B* 2188999 - disable this for now
			// For opposite gender, look at bum if target is facing away from player (unless target is sitting)
//			if (player && bIsOppositeGender && !pTarget->GetIsSitting())
//			{
//				Vector3 vCameraFront2d = m_Frame.GetFront();
//				Vector3 vTargetForward2d = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetB());
//				vCameraFront2d.z = 0.0f;
//				vTargetForward2d.z = 0.0f;
//				vCameraFront2d.NormalizeSafe();
//				vTargetForward2d.NormalizeSafe();
//
//				float dot = vCameraFront2d.Dot(vTargetForward2d);
//				const bool bIsTargetFacingAwayFromPlayer = (dot >= rage::Cosf(m_Metadata.m_LookBumHalfAngle * DtoR));
//				if (bIsTargetFacingAwayFromPlayer)
//				{
//					if (m_Metadata.m_ZoomTAFov > 0.0f)
//					{
//						pTarget->GetBoneMatrix(matTarget, BONETAG_PELVIS);
//						fChestZOffset = (matTarget.d.z - m_LookatPosition.z);
//					}
//					else if (bIsStanding)
//					{
//						pTarget->GetBoneMatrix(matTarget, BONETAG_SPINE1);
// 						fChestZOffset = (matTarget.d.z - m_LookatPosition.z);
//					}
//				}
//			}

			if (m_BlendChestZOffset)
			{
				fChestZOffset = Lerp(m_Metadata.m_PedOffsetInterpolation,
									m_PrevChestZOffset, fChestZOffset);
			}
			m_PrevChestZOffset  = fChestZOffset;
			m_BlendChestZOffset = true;

			pTarget->GetBoneMatrix(matTarget, BONETAG_HEAD);
			float fHeadZOffset = (matTarget.d.z - m_LookatPosition.z);
			if (m_BlendHeadZOffset)
			{
				fHeadZOffset = Lerp(m_Metadata.m_PedOffsetInterpolation,
									m_PrevHeadZOffset, fHeadZOffset);
			}
			m_PrevHeadZOffset  = fHeadZOffset;
			m_BlendHeadZOffset = true;

			m_LookatZOffset = fChestZOffset;

			Vector3 vTargetPosition = VEC3V_TO_VECTOR3(m_LookAtTarget.Get()->GetTransform().GetPosition());
			Vector3 vCamToNewTargetNorm = vTargetPosition - m_Frame.GetPosition();
			float fDistanceToTarget = vCamToNewTargetNorm.Mag();
			const u32 uCheckOutGirlDuration = m_Metadata.m_CheckOutGirlDuration;
			if (uCheckOutGirlDuration > 0)
			{
				const bool bIsHot = pTarget->CheckSexinessFlags(SF_HOT_PERSON) ||
									( pTarget->GetPedModelInfo() &&
									 (pTarget->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashFitnessFemale ||
									  pTarget->GetPedModelInfo()->GetMovementClipSet() == CLIP_SET_MOVE_F_SEXY) );
				if (bIsOppositeGender && bIsHot && m_Metadata.m_CheckOutGirlDuration > 0.0f)
				{
					if (!m_HasCheckedOutGirl && 
						m_TimeSinceTargetLastUpdated + 3500 < fwTimer::GetCamTimeInMilliseconds() &&		// TODO: metadata?
						(bIsTargetStandingStill ||
						 (fDistanceToTarget > m_Metadata.m_CheckOutGirlMinDistance && fDistanceToTarget < m_Metadata.m_CheckOutGirlMaxDistance)))
					{
						if (bIsTargetStandingStill)
						{
							// If woman is facing player and is far enough away, start checking her out.
							m_CheckOutGirlStartTime = fwTimer::GetCamTimeInMilliseconds();
							m_HasCheckedOutGirl = true;
						}
						else if (bIsMoving)
						{
							Vec3V vTargetToPlayer = player->GetTransform().GetPosition() - pTarget->GetTransform().GetPosition();
							vTargetToPlayer = Normalize(vTargetToPlayer);
							Vec3V vTargetForward = pTarget->GetTransform().GetB();
							ScalarV dot = Dot(vTargetToPlayer, vTargetForward);
							if (dot.Getf() > rage::Cosf(80.0f * DtoR))						// TODO: metadata?
							{
								// If woman is facing player and is far enough away, start checking her out.
								m_CheckOutGirlStartTime = fwTimer::GetCamTimeInMilliseconds();
								m_HasCheckedOutGirl = true;
							}
						}
					}

					if (m_CheckOutGirlStartTime > 0)
					{
						if (m_CheckOutGirlStartTime + uCheckOutGirlDuration < fwTimer::GetCamTimeInMilliseconds())
						{
							m_CheckOutGirlStartTime = 0;
						}
						else
						{
							// Check out girl.
							float fInterp = (float)(fwTimer::GetCamTimeInMilliseconds() - m_CheckOutGirlStartTime) /
											(float)uCheckOutGirlDuration;
							fInterp = Clamp(fInterp, 0.0f, 1.0f);
							fInterp = BellInOut(fInterp);
							m_LookatZOffset = Lerp(fInterp, m_LookatZOffset, m_Metadata.m_PedThighZOffset);
						}
					}
				}
			}

			if (fDistanceToTarget <= m_Metadata.m_DistanceToForceTrackHead)
			{
				m_LookatZOffset = fHeadZOffset;
			}
			else if (fDistanceToTarget <= m_Metadata.m_DistanceToStartTrackHead)
			{
				float fInterp = (fDistanceToTarget-m_Metadata.m_DistanceToForceTrackHead) /
								(m_Metadata.m_DistanceToStartTrackHead-m_Metadata.m_DistanceToForceTrackHead);
				fInterp = Clamp(fInterp, 0.0f, 1.0f);
				fInterp = SlowInOut(fInterp);
				m_LookatZOffset = Lerp(fInterp, fHeadZOffset, m_LookatZOffset);
			}

			m_LookatPosition.z += m_LookatZOffset;
		}
		else if (m_LookAtTarget->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtTarget.Get());
			const CBaseModelInfo* vehicleModelInfo = pVehicle->GetBaseModelInfo();
			if(vehicleModelInfo)
			{
				const Vector3& vehicleBoundingBoxMin = vehicleModelInfo->GetBoundingBoxMin();
				const Vector3& vehicleBoundingBoxMax = vehicleModelInfo->GetBoundingBoxMax();

				float fHalfLength = Min(Abs(vehicleBoundingBoxMax.y), Abs(vehicleBoundingBoxMin.y));
				Vector3 vehicleForward = VEC3V_TO_VECTOR3(m_LookAtTarget.Get()->GetTransform().GetB());
				const float c_fMinLengthForOffset = 2.5f;
				const float c_fMaxLengthForOffset = 8.0f;
				const float c_fMinLengthPercentage = 0.10f;
				const float c_fMaxLengthPercentage = 0.60f;
				float fLengthPercentage = RampValue(fHalfLength, c_fMinLengthForOffset, c_fMaxLengthForOffset, c_fMinLengthPercentage, c_fMaxLengthPercentage);

				Vector3 vOffset = vehicleForward * (fHalfLength * fLengthPercentage);		// % of distance from center to front
				m_LookatPosition += vOffset;
			}
		}
	}
	else
	{
		////m_LookatPosition = m_Frame.GetPosition() + m_Frame.GetFront() * 5.0f;
		m_LookatZOffset = 0.0f;
	}

	vTargetPosition = m_LookatPosition;
}

bool camCinematicFirstPersonIdleCamera::ValidateLookatTarget(const Vector3& playerPosition)
{
	if (m_LookingAtEntity && m_LookAtTarget.Get())
	{
		bool bRejectTarget = false;
		float fDistanceSquared = (m_LookatPosition - playerPosition).Mag2();
		float fDistanceToStopTracking = m_Metadata.m_InvalidPedDistance;
		if (m_LookAtTarget->GetIsTypeVehicle())
		{
			fDistanceToStopTracking = m_Metadata.m_InvalidVehicleDistance;
		}
		else if (IsBirdTarget(m_LookAtTarget.Get()))
		{
			fDistanceToStopTracking = m_Metadata.m_MaxValidBirdDistance + 10.0f;		// TODO: metadata?
		}

		if ( fDistanceSquared > (fDistanceToStopTracking * fDistanceToStopTracking) )
		{
			bRejectTarget = true;
		}

		// HACK: sometimes, peds just walk back and forth near player, so should stop watching eventually.
		const u32 c_uMaxTimeToWatchTarget = 40000;										// TODO: metadata?
		if (m_TimeSinceTargetLastUpdated + c_uMaxTimeToWatchTarget < fwTimer::GetCamTimeInMilliseconds())
		{
			bRejectTarget = true;
		}
		else if (m_TimeSinceTargetLastUpdated + m_Metadata.m_MinTimeToWatchTarget < fwTimer::GetCamTimeInMilliseconds())
		{
			u32 lineOfSightFlags = (!m_LookAtTarget->GetIsTypeVehicle()) ? c_PedCollisionFlags : c_VehCollisionFlags;
			if (TestLineOfSight( m_LookatPosition, NULL, lineOfSightFlags ))
			{
				// Test if occluded by the world.
				if (m_LastTimeLosWasClear + m_Metadata.m_MaxTimeToLoseLineOfSight < fwTimer::GetCamTimeInMilliseconds())
				{
					// Time elapsed without regaining LOS, reject target.
					bRejectTarget = true;
				}
			}
			else if (m_LookAtTarget->GetIsTypeVehicle() && TestLineOfSight( m_LookatPosition, NULL, c_VehExtraCollisionFlags ))
			{
				// Test if occluded by a object that can move.
				// TODO: set a separate los timeout due to dynamic collision in metadata?
				if (m_LastTimeLosWasClear + m_Metadata.m_MaxTimeToLoseLineOfSight*3 < fwTimer::GetCamTimeInMilliseconds())
				{
					// Time elapsed without regaining LOS, reject target.
					bRejectTarget = true;
				}
			}
			else
			{
				m_LastTimeLosWasClear = fwTimer::GetCamTimeInMilliseconds();
			}
		}
		else
		{
			m_LastTimeLosWasClear = fwTimer::GetCamTimeInMilliseconds();
		}

		float fSpeedSq = 0.0f;
		if (m_LookAtTarget->GetIsTypeVehicle() || m_LookAtTarget->GetIsTypePed())
		{
			if (m_LookAtTarget->GetIsTypeVehicle())
			{
				fSpeedSq = ((CVehicle*)m_LookAtTarget.Get())->GetVelocity().Mag2();

				if (((CVehicle*)m_LookAtTarget.Get())->GetIsInReusePool())
				{
					// B* 1441116: Target is in reuse pool, reject.
					bRejectTarget = true;
				}
			}
			else
			{
				fSpeedSq = ((CPed*)m_LookAtTarget.Get())->GetVelocity().Mag2();
				if ( static_cast<const CPed*>(m_LookAtTarget.Get())->IsMale() &&
					!IsBirdTarget(m_LookAtTarget.Get()))
				{
					// HACK: treat human males as standing still, so we do not watch them for too long.
					fSpeedSq = 0.0f;
				}

				if (static_cast<const CPed*>(m_LookAtTarget.Get())->GetIsSwimming())
				{
					// If target is swimming, ignore them since peds don't swim well and
					// they move with the waves. so they register as moving even though they may be still.
					bRejectTarget = true;
				}

				if (static_cast<const CPed*>(m_LookAtTarget.Get())->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
				{
					// B* 1441116: Target is in reuse pool, reject.
					bRejectTarget = true;
				}

				if (NetworkInterface::IsGameInProgress() && static_cast<const CPed*>(m_LookAtTarget.Get())->IsPlayer())
				{
					// Ignore players in network games, as they can warp around.
					bRejectTarget = true;
				}
			}
			
			// SMALL_FLOAT is too small
			if (fSpeedSq > 0.01f*0.01f)
			{
				m_TimeSinceTargetLastMoved = fwTimer::GetCamTimeInMilliseconds();
			}
			
			if (m_TimeSinceTargetLastMoved + m_Metadata.m_TimeToStopWatchingWhenStill < fwTimer::GetCamTimeInMilliseconds())
			{
				// If target is not moving, then stop watching as it is boring.
				bRejectTarget = true;
			}
		}

		if (IsBirdTarget(m_LookAtTarget.Get()) && !IsValidBirdTarget(m_LookAtTarget.Get()))
		{
			// Bird target no longer valid.
			bRejectTarget = true;
		}

		if (!m_LookAtTarget.Get()->GetIsVisible())
		{
			// Target is invisible, so stop targetting.
			bRejectTarget = true;
		}

		if (bRejectTarget)
		{
			bool bLastTargetWasMale = m_LookAtTarget.Get() && m_LookAtTarget->GetIsTypePed() && ((CPed*)m_LookAtTarget.Get())->IsMale();
			ClearTarget();

			const u32 c_uTimeToGetNewTargetAfterLoss = 1000;	// msec		TODO: metadata?
			if (!bLastTargetWasMale && m_Metadata.m_TimeToStartSearchNoTarget > c_uTimeToGetNewTargetAfterLoss)
			{
				// When target becomes invalid, modify the search target timer so we will try to find another target after one second.
				m_TimeSinceLastTargetSearch = fwTimer::GetCamTimeInMilliseconds() - (m_Metadata.m_TimeToStartSearchNoTarget - c_uTimeToGetNewTargetAfterLoss);
			}
			return false;
		}
	}

	return true;
}

void camCinematicFirstPersonIdleCamera::SearchForLookatTarget()
{
	bool bFoundNewTarget = false;
	if (m_LookingAtEntity && m_LookAtTarget.Get())
	{
		// Do not allow changing of targets during movement to look at the existing target.
		return;
	}

	if (m_AttachParent.Get() && m_AttachParent.Get()->GetIsTypePed())
	{
		const CPed* player = static_cast<const CPed*>(m_AttachParent.Get());
		if (player->GetPedIntelligence())
		{
			const CEntity* pBestValidPed = (m_LookAtTarget.Get() != NULL && m_LookAtTarget->GetIsTypePed()) ? m_LookAtTarget.Get() : NULL;
			const CEntity* pBestValidEventPed = NULL;
			const CEntity* pBestValidVehicle = (m_LookAtTarget.Get() != NULL && m_LookAtTarget->GetIsTypeVehicle()) ? m_LookAtTarget.Get() : NULL;
			const CEntity* pBestValidBird = (IsBirdTarget(m_LookAtTarget.Get())) ? m_LookAtTarget.Get() : NULL;
			float    fBestValidPedDistanceSq = FLT_MAX;
			float    fBestValidEventPedDistanceSq = FLT_MAX;
			float    fBestValidVehicleDistanceSq = FLT_MAX;
			float    fBestValidBirdDistanceSq = FLT_MAX;

			if (pBestValidPed)
			{
				Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pBestValidPed->GetTransform().GetPosition());
				Vector3 vCamToTarget = vTargetPosition - m_Frame.GetPosition();
				fBestValidPedDistanceSq = vCamToTarget.Mag2();
			}

			if (pBestValidVehicle)
			{
				Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pBestValidVehicle->GetTransform().GetPosition());
				Vector3 vCamToTarget = vTargetPosition - m_Frame.GetPosition();
				fBestValidVehicleDistanceSq = vCamToTarget.Mag2();
			}

			if (pBestValidBird)
			{
				Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pBestValidBird->GetTransform().GetPosition());
				Vector3 vCamToTarget = vTargetPosition - m_Frame.GetPosition();
				fBestValidBirdDistanceSq = vCamToTarget.Mag2();
			}

			Vector3  vCameraFront2d = m_Frame.GetFront();
			float fCosMaxHorizontalAngleToSearch = rage::Cosf(m_Metadata.m_PlayerHorizontalFov * DtoR);
			float fCosMaxVerticalAngleToSearch   = rage::Cosf(m_Metadata.m_PlayerVerticalFov * DtoR);
			vCameraFront2d.z = 0.0f;
			vCameraFront2d.NormalizeSafe();

			if (player->GetPedIntelligence()->GetPedScanner())
			{
				CPedScanner& pedScanner = *player->GetPedIntelligence()->GetPedScanner();

				// Look at nearby peds and find a valid target.
				CEntityScannerIterator entityList = pedScanner.GetIterator();
				for (CEntity* pEnt = static_cast<CPed*>(entityList.GetFirst()); pEnt; pEnt = entityList.GetNext())
				{
					// TODO: when scanning for events, should we ignore recent targets?
					CPed* pNearbyPed = static_cast<CPed*>(pEnt);
					if (pNearbyPed)// && IsInHistoryList(pNearbyPed) < 0)
					{
						if (!pNearbyPed->ShouldBeDead() && !pNearbyPed->GetIsInVehicle())
						{
							// Look for nearby events.
							TestEventTarget(pNearbyPed, pBestValidEventPed, fBestValidEventPedDistanceSq, vCameraFront2d);

							if ( IsInHistoryList(pNearbyPed) < 0 )
							{
								// Look for valid peds.
								TestPedTarget(player, pNearbyPed, pBestValidPed, fBestValidPedDistanceSq,
											  pBestValidBird, fBestValidBirdDistanceSq,
											  vCameraFront2d, fCosMaxHorizontalAngleToSearch, fCosMaxVerticalAngleToSearch);
							}
						}
					}
				}
			}

			// Don't look for vehicles too often.
			if (player->GetPedIntelligence()->GetVehicleScanner() &&
				m_LastTimeVehicleWasTargetted + m_Metadata.m_TimeBetweenVehicleSightings < fwTimer::GetCamTimeInMilliseconds())
			{
				CVehicleScanner& vehScanner = *player->GetPedIntelligence()->GetVehicleScanner();

				// Look at nearby vehicles and find a valid target.
				CEntityScannerIterator entityList = vehScanner.GetIterator();
				for (CEntity* pEnt = static_cast<CVehicle*>(entityList.GetFirst()); pEnt; pEnt = entityList.GetNext())
				{
					CVehicle* pNearbyVeh = static_cast<CVehicle*>(pEnt);

					if ( IsInHistoryList(pNearbyVeh) < 0 )
					{
						TestVehicleTarget(pNearbyVeh, pBestValidVehicle, fBestValidVehicleDistanceSq,
										  vCameraFront2d, fCosMaxHorizontalAngleToSearch, fCosMaxVerticalAngleToSearch);
					}
				}
			}

			if (pBestValidEventPed || pBestValidPed || pBestValidVehicle || pBestValidBird)
			{
				const CEntity* prevLookAtTarget = m_LookAtTarget.Get();
				if (pBestValidEventPed)
				{
					m_LookAtTarget = pBestValidEventPed;
				}
				else
				{
					if (fBestValidPedDistanceSq < fBestValidVehicleDistanceSq)
					{
						m_LookAtTarget = pBestValidPed;
					}
					else
					{
						m_LookAtTarget = pBestValidVehicle;
					}

					if (m_LookAtTarget.Get() == NULL && pBestValidBird)
					{
						// No other targets, so if there is a bird, look at it.
						m_LookAtTarget = pBestValidBird;
					}
				}

				if (prevLookAtTarget != m_LookAtTarget.Get())
				{
					OnTargetChange(prevLookAtTarget);
					m_TimeSinceTargetLastUpdated = fwTimer::GetCamTimeInMilliseconds();
				}
				m_LookingAtEntity = true;
				bFoundNewTarget = true;
			}
		}
	}
}

void camCinematicFirstPersonIdleCamera::SearchForLookatEvent()
{
	if (m_AttachParent.Get() && m_AttachParent.Get()->GetIsTypePed())
	{
		const CPed* player = static_cast<const CPed*>(m_AttachParent.Get());
		if (player->GetPedIntelligence())
		{
			const CEntity* pBestValidEventPed = NULL;
			float    fBestValidEventPedDistanceSq = FLT_MAX;

			Vector3  vCameraFront2d = m_Frame.GetFront();
			vCameraFront2d.z = 0.0f;
			vCameraFront2d.NormalizeSafe();

			if (player->GetPedIntelligence()->GetPedScanner())
			{
				CPedScanner& pedScanner = *player->GetPedIntelligence()->GetPedScanner();

				// Look at nearby peds and find a valid event.
				CEntityScannerIterator entityList = pedScanner.GetIterator();
				for (CEntity* pEnt = static_cast<CPed*>(entityList.GetFirst()); pEnt; pEnt = entityList.GetNext())
				{
					// TODO: when scanning for events, should we ignore recent targets?
					CPed* pNearbyPed = static_cast<CPed*>(pEnt);
					if (pNearbyPed) // && IsInHistoryList(pNearbyPed) < 0)
					{
						if (!pNearbyPed->ShouldBeDead() && !pNearbyPed->GetIsInVehicle())
						{
							TestEventTarget(pNearbyPed, pBestValidEventPed, fBestValidEventPedDistanceSq, vCameraFront2d);
						}
					}
				}
			}

			if (pBestValidEventPed)
			{
				m_LookAtTarget = pBestValidEventPed;
				m_TimeSinceTargetLastUpdated = fwTimer::GetCamTimeInMilliseconds();
				m_LookingAtEntity = true;
			}
		}
	}
}

void camCinematicFirstPersonIdleCamera::TestEventTarget(const CPed* pNearbyPed,
														const CEntity*& pBestValidEventPed, float& fBestValidEventPedDistanceSq,
														const Vector3& vCameraFront2d)
{
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());
	Vector3 vCamToTarget2d = vTargetPosition - m_Frame.GetPosition();;
	//Vector3 vCamToTarget = vCamToTarget2d;
	vCamToTarget2d.z = 0.0;
	//vCamToTarget.NormalizeSafe();
	vCamToTarget2d.NormalizeSafe();

	Vector3 vCross;
	float fCosHeadingDiff = Clamp(vCameraFront2d.Dot(vCamToTarget2d), -1.0f, 1.0f);
	vCross.Cross(vCamToTarget2d, vCameraFront2d);
	RotateDirectionAboutAxis(vCamToTarget2d, (vCross.z >= 0.0f) ? ZAXIS : -ZAXIS, rage::Acosf(fCosHeadingDiff));

	const CEntity* pPossibleValidEventTarget = NULL;
	if (pNearbyPed->GetPedIntelligence() && pNearbyPed->GetIsVisible())
	{
		const CEvent* pEvent = GetShockingEvent(pNearbyPed);
		if (pEvent)
		{
			pPossibleValidEventTarget = (pEvent->GetSourcePed()) ? pEvent->GetSourcePed() : pEvent->GetTargetPed();
			if (pPossibleValidEventTarget && pPossibleValidEventTarget != m_AttachParent.Get())
			{
				if (!TestLineOfSight( vTargetPosition, pPossibleValidEventTarget, c_PedCollisionFlags ))
				{
					Vector3 vEventPedPosition = VEC3V_TO_VECTOR3(pPossibleValidEventTarget->GetTransform().GetPosition());
					Vector3 vCamToEventPed = vEventPedPosition - m_Frame.GetPosition();
					float fDistanceSquaredToEventPed = vCamToEventPed.Mag2();
					if (fDistanceSquaredToEventPed < (m_Metadata.m_MaxValidEventDistance * m_Metadata.m_MaxValidEventDistance))
					{
						pBestValidEventPed = pPossibleValidEventTarget;
						fBestValidEventPedDistanceSq = fDistanceSquaredToEventPed;
					}
				}
			}
		}
	}
}

void camCinematicFirstPersonIdleCamera::TestPedTarget(const CPed* player, const CPed* pNearbyPed,
													  const CEntity*& pBestValidPed, float& fBestValidPedDistanceSq,
													  const CEntity*& pBestValidBird, float& fBestValidBirdDistanceSq,
													  const Vector3& vCameraFront2d,
													  float fCosMaxHorizontalAngleToSearch, float fCosMaxVerticalAngleToSearch)
{
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());
	if (!IsBirdTarget(pNearbyPed))
	{
		vTargetPosition.z += 0.25f;
	}
	Vector3 vCamToTarget = vTargetPosition - m_Frame.GetPosition();
	Vector3 vCamToTarget2d = vCamToTarget;
	float fDistanceSquaredToPed = vCamToTarget.Mag2();
	vCamToTarget2d.z = 0.0;
	vCamToTarget.NormalizeSafe();
	vCamToTarget2d.NormalizeSafe();

	Vector3 vCross;
	float fCosHeadingDiff = Clamp(vCameraFront2d.Dot(vCamToTarget2d), -1.0f, 1.0f);
	vCross.Cross(vCamToTarget2d, vCameraFront2d);
	RotateDirectionAboutAxis(vCamToTarget, (vCross.z >= 0.0f) ? ZAXIS : -ZAXIS, rage::Acosf(fCosHeadingDiff));
	float fCosPitchDiff = vCamToTarget.Dot(vCameraFront2d);

	const CPed* pPossibleValidTarget = NULL;
	const CPed* pPossibleValidBirdTarget = NULL;

	// Reject targets that are too far.
	// TODO: ignore restriction for shocking events?  Or have a different limit?
	float fMaxDistance = Max(m_Metadata.m_MaxValidPedDistance, m_Metadata.m_MaxValidBirdDistance);
	const bool bIsWithinHorizontalFov = (fCosHeadingDiff >= fCosMaxHorizontalAngleToSearch);
	if (fDistanceSquaredToPed < (fMaxDistance * fMaxDistance) &&
		 pNearbyPed->GetIsVisible() &&
		!pNearbyPed->GetIsSwimming() &&											// Ignore swimming peds
		!pNearbyPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool) &&		// B* 1441116
		!(NetworkInterface::IsGameInProgress() && pNearbyPed->IsPlayer()) &&	// Ignore players in network games, as they can warp around.
		fCosPitchDiff   >= fCosMaxVerticalAngleToSearch)
	{
		const bool bIsSexyOppositeGender = player->IsMale() != pNearbyPed->IsMale() && pNearbyPed->CheckSexinessFlags(SF_HOT_PERSON);
		bool bLookAtFitnessWomen = false;
		bool bLookAtRichYoungWomen = false;
		bool bLookAtToughYoungWomen = false;
		bool bLookAtWeakYoungWomen = false;
		bool bLookAtMediumRichWomen = false;
		bool bLookAtMuscleWomen = false;
		bool bLookAtSpecialPed = false;
		bool bUsingSexyFemaleAnims = false;
		bool bUsingFatFemaleAnims = false;
		if (pNearbyPed->GetPedIntelligence() && 
			pNearbyPed->GetPedIntelligence()->GetQueriableInterface())
		{
			// Peds running scripted animation are interesting to look at. (i.e. Impotent Rage, Vinewood Zombie)
			bLookAtSpecialPed = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SCRIPTED_ANIMATION);
		}

		// Choose targets based on criteria.
		if (bIsWithinHorizontalFov)
		{
			if (pNearbyPed->GetPedModelInfo() && player->IsMale())
			{
				bLookAtFitnessWomen    = pNearbyPed->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashFitnessFemale;
				bLookAtRichYoungWomen  = pNearbyPed->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashYoungRichWoman;
				bUsingSexyFemaleAnims  = pNearbyPed->GetPedModelInfo()->GetMovementClipSet() == CLIP_SET_MOVE_F_SEXY;

				bLookAtToughYoungWomen = (m_PedType == FPI_PEDTYPE_TREVOR || m_PedType == FPI_PEDTYPE_FRANKLIN) &&
										 pNearbyPed->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashYoungToughWoman;
				bLookAtWeakYoungWomen  = (m_PedType == FPI_PEDTYPE_MICHAEL || m_PedType == FPI_PEDTYPE_FRANKLIN) &&
										 pNearbyPed->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashYoungWeakWoman;
				bLookAtMediumRichWomen = (m_PedType == FPI_PEDTYPE_TREVOR) &&
										 pNearbyPed->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashMediumRichWoman;
				bLookAtMuscleWomen     = (m_PedType == FPI_PEDTYPE_TREVOR) &&
										 pNearbyPed->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashMuscleWoman;

				bUsingFatFemaleAnims   = pNearbyPed->GetPedModelInfo()->GetMovementClipSet() == CLIP_SET_MOVE_F_FAT ||
										 pNearbyPed->GetPedModelInfo()->GetMovementClipSet() == CLIP_SET_MOVE_F_CHUBBY ||
										 (m_PedType != FPI_PEDTYPE_TREVOR &&
										  (pNearbyPed->GetPedModelInfo()->GetDefaultGestureClipSet() == CLIP_SET_GESTURE_F_FAT ||
										   pNearbyPed->GetPedModelInfo()->GetMovementClipSet() == CLIP_SET_MOVE_F_FAT_NO_ADD));
			}

			if ((bIsSexyOppositeGender  || bLookAtRichYoungWomen  || bLookAtFitnessWomen || bUsingSexyFemaleAnims || bLookAtWeakYoungWomen ||
				 bLookAtToughYoungWomen || bLookAtMediumRichWomen || bLookAtMuscleWomen  || bLookAtSpecialPed) &&
				 fDistanceSquaredToPed < (m_Metadata.m_MaxValidPedDistance * m_Metadata.m_MaxValidPedDistance))
			{
				if (!bUsingFatFemaleAnims || bLookAtSpecialPed)
				{
					// Look at hot women.
					pPossibleValidTarget = pNearbyPed;
				}
			}

			if (m_PedType == FPI_PEDTYPE_TREVOR)
			{
				const bool bIsAgressiveMale = pNearbyPed->IsMale() && 
											(pNearbyPed->CheckBraveryFlags(BF_CONFRONTATIONAL|BF_INTERVENE_ON_MELEE_ACTION) ||								// cops, gangs, army
											 pNearbyPed->CheckBraveryFlags(BF_DONT_SAY_PANIC_ON_FLEE|BF_PURSUE_WHEN_HIT_BY_CAR|BF_BOOST_BRAVERY_IN_GROUP));	// bold personality
				if (bIsAgressiveMale &&
					fDistanceSquaredToPed < (m_Metadata.m_MaxValidPedDistance * m_Metadata.m_MaxValidPedDistance))
				{
					// Target ped should be facing player, at least when first targetted.
					// TODO: only do for Bold personality peds?
					Vec3V vPlayerToTarget = pNearbyPed->GetTransform().GetPosition() - player->GetTransform().GetPosition();
					vPlayerToTarget = Normalize(vPlayerToTarget);
					Vec3V vTargetForward = pNearbyPed->GetTransform().GetB();
					ScalarV dot = Dot(vPlayerToTarget, vTargetForward);
					if (dot.Getf() < -rage::Cosf(m_Metadata.m_LookBumHalfAngle * DtoR))		// TODO: metadata? use separate angle?
					{
						// Trevor looks for guys to fight with.
						pPossibleValidTarget = pNearbyPed;
					}
				}
			}
			else if (!pBestValidPed)
			{
				// Michael and Franklin will look at cops if no other valid ped.
				const bool bIsAgressiveMale = pNearbyPed->IsMale() && 
											(pNearbyPed->CheckBraveryFlags(BF_CONFRONTATIONAL|BF_INTERVENE_ON_MELEE_ACTION));								// cops, gangs, army
				if (bIsAgressiveMale &&
					fDistanceSquaredToPed < (m_Metadata.m_MaxValidPedDistance * m_Metadata.m_MaxValidPedDistance))
				{
					// Target ped should be facing player, at least when first targetted.
					Vec3V vPlayerToTarget = pNearbyPed->GetTransform().GetPosition() - player->GetTransform().GetPosition();
					vPlayerToTarget = Normalize(vPlayerToTarget);
					Vec3V vTargetForward = pNearbyPed->GetTransform().GetB();
					ScalarV dot = Dot(vPlayerToTarget, vTargetForward);
					if (dot.Getf() < -rage::Cosf(m_Metadata.m_LookBumHalfAngle * DtoR))		// TODO: metadata? use separate angle?
					{
						pPossibleValidTarget = pNearbyPed;
					}
				}
			}
		}

		// Don't restrict birds to horizontal fov.
		// Don't look at birds too often, ignore timer if we are re-acquiring the same target.
		if (m_Metadata.m_TimeBetweenBirdSightings > 0 &&
			(m_LastTimeBirdWasTargetted + m_Metadata.m_TimeBetweenBirdSightings < fwTimer::GetCamTimeInMilliseconds() ||
			 pNearbyPed == m_LookAtTarget.Get()))
		{
			// Michael can look at birds, ignore ones that are not flying.
			const bool isFlying = pNearbyPed->GetMotionData() && pNearbyPed->GetMotionData()->GetIsFlying();
			if ( IsBirdTarget(pNearbyPed) && IsValidBirdTarget(pNearbyPed) &&
				 fDistanceSquaredToPed < (m_Metadata.m_MaxValidBirdDistance * m_Metadata.m_MaxValidBirdDistance) &&
				 //pNearbyPed->GetVelocity().Mag2() > 0.0f &&	// If bird is flying, it should not be still.
				 isFlying )
			{
				pPossibleValidBirdTarget = pNearbyPed;
			}
		}

		// Validate that this target is closer than current best target.
		if (pPossibleValidTarget)
		{
			if (!TestLineOfSight( vTargetPosition, pPossibleValidTarget, c_PedCollisionFlags ))
			{
				if (pBestValidPed)
				{
					const CPed* pPrevTarget = static_cast<const CPed*>(pBestValidPed);
					const bool bIsPrevSexyOppositeGender = player->IsMale() != pPrevTarget->IsMale() && pPrevTarget->CheckSexinessFlags(SF_HOT_PERSON);
					const bool bIsPrevFitnessWomen = pPrevTarget->GetPedModelInfo() &&
													pPrevTarget->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashFitnessFemale;
					const bool bIsPrevYoungRichWomen = pPrevTarget->GetPedModelInfo() &&
													pPrevTarget->GetPedModelInfo()->GetPersonalitySettings().GetPersonalityNameHash() == c_HashYoungRichWoman;
					const bool bIsPrevSpecialPed = (pPrevTarget->GetPedIntelligence() && 
													pPrevTarget->GetPedIntelligence()->GetQueriableInterface() &&
													pPrevTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SCRIPTED_ANIMATION));
					const bool bPrevUsingSexyFemaleAnims = pPrevTarget->GetPedModelInfo() &&
													pPrevTarget->GetPedModelInfo()->GetMovementClipSet() == CLIP_SET_MOVE_F_SEXY;

					const bool bIsPreviousHighPriority = (bIsPrevSexyOppositeGender || bIsPrevFitnessWomen || bIsPrevSpecialPed);
					const bool bIsNewTargetHighPriority = (bIsSexyOppositeGender || bLookAtFitnessWomen || bLookAtSpecialPed);

					const bool bIsPreviousMediumPriority = (bIsPrevYoungRichWomen || bPrevUsingSexyFemaleAnims);
					const bool bIsNewTargetMediumPriority = (bLookAtRichYoungWomen || bUsingSexyFemaleAnims);

					const bool bIsPreviousLowPriority = player->IsMale() != pPrevTarget->IsMale() && !bIsPreviousHighPriority && !bIsPreviousMediumPriority;
					const bool bIsNewTargetLowPriority = player->IsMale() != pPossibleValidTarget->IsMale() && !bIsNewTargetHighPriority && !bIsNewTargetMediumPriority;

					// Do not change targets unless highER priority target is found.
					if ( (bIsNewTargetHighPriority && !bIsPreviousHighPriority) ||
						 (bIsNewTargetMediumPriority && !bIsPreviousHighPriority && !bIsPreviousMediumPriority) ||
						 (bIsNewTargetLowPriority && !bIsPreviousHighPriority && !bIsPreviousMediumPriority) ||
						 (!bIsPreviousHighPriority && !bIsPreviousMediumPriority && !bIsPreviousLowPriority) )
					{
						Vector3 vCamToBestTarget = VEC3V_TO_VECTOR3(pBestValidPed->GetTransform().GetPosition()) - m_Frame.GetPosition();
						vCamToBestTarget.z = 0.0f;
						if (vCamToBestTarget.Mag2() >= fDistanceSquaredToPed)
						{
							pBestValidPed = pPossibleValidTarget;
							fBestValidPedDistanceSq = fDistanceSquaredToPed;
						}
					}
				}
				else
				{
					pBestValidPed = pPossibleValidTarget;
					fBestValidPedDistanceSq = fDistanceSquaredToPed;
				}
			}
		}
		else if (pPossibleValidBirdTarget)
		{
			if (!TestLineOfSight( vTargetPosition, pPossibleValidBirdTarget, c_PedCollisionFlags ))
			{
				if (pBestValidBird)
				{
					Vector3 vCamToBestTarget = VEC3V_TO_VECTOR3(pBestValidBird->GetTransform().GetPosition()) - m_Frame.GetPosition();
					vCamToBestTarget.z = 0.0f;
					if (vCamToBestTarget.Mag2() >= fDistanceSquaredToPed)
					{
						pBestValidBird = pPossibleValidBirdTarget;
						fBestValidBirdDistanceSq = fDistanceSquaredToPed;
					}
				}
				else
				{
					pBestValidBird = pPossibleValidBirdTarget;
					fBestValidBirdDistanceSq = fDistanceSquaredToPed;
				}
			}
		}
	}
}

void camCinematicFirstPersonIdleCamera::TestVehicleTarget(const CVehicle* pNearbyVeh,
														  const CEntity*& pBestValidVehicle, float& fBestValidVehicleDistanceSq,
														  const Vector3& vCameraFront2d,
														  float fCosMaxHorizontalAngleToSearch, float fCosMaxVerticalAngleToSearch)
{
	const CEntity* pPossibleValidTarget = NULL;
	if (pNearbyVeh)
	{
		const bool bDrivable =	 pNearbyVeh->GetStatus() != STATUS_WRECKED &&
								!pNearbyVeh->m_nVehicleFlags.bIsDrowning && 
								 pNearbyVeh->m_nVehicleFlags.bIsThisADriveableCar &&
								 pNearbyVeh->GetIsVisible();
		const bool bIsOnFire = pNearbyVeh->m_nFlags.bIsOnFire;

		// Do not look at wrecked cars, unless they are burning.
		if (!const_cast<CVehicle*>(pNearbyVeh)->GetIsInReusePool() && (bDrivable || bIsOnFire))
		{
			Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pNearbyVeh->GetTransform().GetPosition());
			Vector3 vCamToTarget = vTargetPosition - m_Frame.GetPosition();
			Vector3 vCamToTarget2d = vCamToTarget;
			float fDistanceSquaredToVeh = vCamToTarget.Mag2();
			vCamToTarget2d.z = 0.0;
			vCamToTarget.NormalizeSafe();
			vCamToTarget2d.NormalizeSafe();

			Vector3 vCross;
			float fCosHeadingDiff = Clamp(vCameraFront2d.Dot(vCamToTarget2d), -1.0f, 1.0f);
			vCross.Cross(vCamToTarget2d, vCameraFront2d);
			RotateDirectionAboutAxis(vCamToTarget, (vCross.z >= 0.0f) ? ZAXIS : -ZAXIS, rage::Acosf(fCosHeadingDiff));
			float fCosPitchDiff = vCamToTarget.Dot(vCameraFront2d);

			float fVehicleTypeScale = GetVehicleTypeScale(pNearbyVeh);
			float fMinValidDistance = fVehicleTypeScale * m_Metadata.m_MinValidDistToTarget;

			// Clamp to 2.0f so if the scale is 1.0f, we don't reduce the max distance.
			const float c_fVehicleMaxDistanceModifier = 0.50f;
			fVehicleTypeScale = Min(1.0f/c_fVehicleMaxDistanceModifier, fVehicleTypeScale);
			float fMaxValidDistance = Min(m_Metadata.m_InvalidVehicleDistance - 5.0f,
								fVehicleTypeScale * c_fVehicleMaxDistanceModifier * m_Metadata.m_MaxValidVehicleDistance);

			// Reject targets that are too far.
			// TODO: ignore restriction for shocking events?  Or have a different limit?
			if (fDistanceSquaredToVeh > (fMinValidDistance * fMinValidDistance) &&
				fDistanceSquaredToVeh < (fMaxValidDistance * fMaxValidDistance) &&
				fCosHeadingDiff >= fCosMaxHorizontalAngleToSearch &&
				fCosPitchDiff   >= fCosMaxVerticalAngleToSearch)
			{
				CVehicleModelInfo* pVehicleInfo = pNearbyVeh->GetVehicleModelInfo();
				u32 vehicleSwankness  = (pVehicleInfo) ? pVehicleInfo->GetVehicleSwankness() : 0;
				bool isLawEnforcement = (pVehicleInfo) ? pVehicleInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT) : false;
				bool isSportsCar      = (pVehicleInfo) ? pVehicleInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS) : false;
				if ( vehicleSwankness >= m_Metadata.m_MinVehicleSwankness ||
					(isSportsCar && vehicleSwankness >= m_Metadata.m_MinSportsCarSwankness) ||
					 isLawEnforcement ||
					 bIsOnFire )
				{
					// Look at nice cars or burning cars or police cars.
					pPossibleValidTarget = pNearbyVeh;
				}

				// Validate that this target is closer than current best target.
				if (pPossibleValidTarget)
				{
					const bool bIsMoving = pNearbyVeh->GetVelocity().Mag2() > SMALL_FLOAT;
					u32 lineOfSightFlags = c_VehCollisionFlags;
					if (!bIsMoving)
					{
						// Do extra testing for non-moving vehicles.
						lineOfSightFlags |= c_VehExtraCollisionFlags;
					}

					if (!TestLineOfSight( vTargetPosition, pPossibleValidTarget, lineOfSightFlags ))
					{
						if (pBestValidVehicle)
						{
							const CVehicle* pPrevTarget = static_cast<const CVehicle*>(pBestValidVehicle);
							CVehicleModelInfo* pPrevVehicleInfo = pPrevTarget->GetVehicleModelInfo();
							u32 prevSwankness  = (pPrevVehicleInfo) ? pPrevVehicleInfo->GetVehicleSwankness() : 0;
							////bool prevLawEnforcement = (pPrevVehicleInfo) ? pPrevVehicleInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT) : false;
							const bool bPrevIsOnFire = pPrevTarget->m_nFlags.bIsOnFire;

							const bool bIsPreviousHighPriority = (prevSwankness >= 5);
							const bool bIsNewTargetHighPriority = (vehicleSwankness >= 5);
							const bool bIsPreviousMediumPriority = (prevSwankness == 4 || bPrevIsOnFire);
							const bool bIsNewTargetMediumPriority = (vehicleSwankness == 4 || bIsOnFire);

							// Do not change targets unless highER priority target is found.
							if ( (bIsNewTargetHighPriority && !bIsPreviousHighPriority) ||
								 (bIsNewTargetMediumPriority && !bIsPreviousHighPriority && !bIsPreviousMediumPriority) ||
								 (!bIsPreviousHighPriority && !bIsPreviousMediumPriority) )
							{
								Vector3 vCamToBestTarget = VEC3V_TO_VECTOR3(pBestValidVehicle->GetTransform().GetPosition()) - m_Frame.GetPosition();
								vCamToBestTarget.z = 0.0f;
								if (vCamToBestTarget.Mag2() >= fDistanceSquaredToVeh)
								{
									pBestValidVehicle = pPossibleValidTarget;
									fBestValidVehicleDistanceSq = fDistanceSquaredToVeh;
								}
							}
						}
						else
						{
							pBestValidVehicle = pPossibleValidTarget;
							fBestValidVehicleDistanceSq = fDistanceSquaredToVeh;
						}
					}
				}
			}
		}
	}
}

bool camCinematicFirstPersonIdleCamera::TestLineOfSight(const Vector3& targetPosition, const CEntity* pTargetToTest, const u32 flags) const
{
	const CEntity* aExcludeEntityList[2] = { m_AttachParent.Get(), (pTargetToTest) ? pTargetToTest : m_LookAtTarget.Get() };

	//Now perform a directed capsule test between the camera and target positions.
	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	capsuleTest.SetIncludeFlags(flags);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetExcludeEntities(aExcludeEntityList, 2, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	capsuleTest.SetCapsule(targetPosition, m_Frame.GetPosition(), m_Metadata.m_LosTestRadius);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);

	return hasHit; 
}

bool camCinematicFirstPersonIdleCamera::TrimPointToCollision(Vector3& testPosition, const u32 flags) const
{
	const CEntity* aExcludeEntityList[2] = { m_AttachParent.Get(), m_LookAtTarget.Get() };

	//Now perform a directed capsule test between the camera and target positions.
	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestHitPoint capsuleIsect;
	WorldProbe::CShapeTestResults capsuleResult(capsuleIsect);
	capsuleTest.SetResultsStructure(&capsuleResult);
	capsuleTest.SetIncludeFlags(flags);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetExcludeEntities(aExcludeEntityList, (m_LookAtTarget.Get()) ? 2 : 1, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	capsuleTest.SetCapsule(testPosition, m_Frame.GetPosition(), m_Metadata.m_LosTestRadius);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	if (hasHit)
	{
		testPosition = capsuleResult[0].GetHitPosition();
	}

	return hasHit; 
}

#define CHECK_FOR_EVENT(x)	if (!pEvent) { pEvent = pNearbyPed->GetPedIntelligence()->GetEventOfType(x); if (pEvent) { return pEvent; } }
const CEvent* camCinematicFirstPersonIdleCamera::GetShockingEvent(const CPed* pNearbyPed)
{
	// List of possible shocking events.
	//EVENT_SHOCKING_CAR_CRASH,
	//EVENT_SHOCKING_CAR_PILE_UP,
	//EVENT_SHOCKING_CAR_ON_CAR,
	//EVENT_SHOCKING_DEAD_BODY,
	//EVENT_SHOCKING_DRIVING_ON_PAVEMENT,
	//EVENT_SHOCKING_EXPLOSION,
	//EVENT_SHOCKING_FIRE,
	//EVENT_SHOCKING_GUN_FIGHT,
	//EVENT_SHOCKING_GUNSHOT_FIRED,
	//EVENT_SHOCKING_PED_RUN_OVER,
	//EVENT_SHOCKING_PED_SHOT,
	//EVENT_SHOCKING_SEEN_GANG_FIGHT,
	//EVENT_SHOCKING_SEEN_MELEE_ACTION,
	//EVENT_SHOCKING_SEEN_PED_KILLED,
	//EVENT_SHOCKING_SEEN_WEAPON_THREAT,
	//EVENT_SHOCKING_SEEN_WEIRD_PED,
	//EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING,

	// TODO: filter out some events based on who the player is.
	CEvent* pEvent = NULL;
	CHECK_FOR_EVENT(EVENT_SHOCKING_CAR_CRASH);
	CHECK_FOR_EVENT(EVENT_SHOCKING_CAR_PILE_UP);
	CHECK_FOR_EVENT(EVENT_SHOCKING_GUN_FIGHT);
	CHECK_FOR_EVENT(EVENT_SHOCKING_GUNSHOT_FIRED);
	CHECK_FOR_EVENT(EVENT_SHOCKING_PED_SHOT);
	CHECK_FOR_EVENT(EVENT_SHOT_FIRED);
	CHECK_FOR_EVENT(EVENT_AGITATED_ACTION);
	CHECK_FOR_EVENT(EVENT_SHOUT_TARGET_POSITION);
	CHECK_FOR_EVENT(EVENT_SHOCKING_SEEN_GANG_FIGHT);

	CHECK_FOR_EVENT(EVENT_SHOCKING_EXPLOSION);
	CHECK_FOR_EVENT(EVENT_EXPLOSION);
	CHECK_FOR_EVENT(EVENT_SHOCKING_FIRE);

	CHECK_FOR_EVENT(EVENT_SHOCKING_PED_RUN_OVER);
	CHECK_FOR_EVENT(EVENT_SHOCKING_MAD_DRIVER_EXTREME);
	CHECK_FOR_EVENT(EVENT_SHOCKING_DRIVING_ON_PAVEMENT);

	CHECK_FOR_EVENT(EVENT_SHOCKING_RUNNING_STAMPEDE);
	CHECK_FOR_EVENT(EVENT_SHOCKING_RUNNING_PED);

	CHECK_FOR_EVENT(EVENT_SHOCKING_PARACHUTER_OVERHEAD);

	if (m_PedType == FPI_PEDTYPE_MICHAEL || m_PedType == FPI_PEDTYPE_FRANKLIN)
	{
		CHECK_FOR_EVENT(EVENT_CRIME_CRY_FOR_HELP);
		CHECK_FOR_EVENT(EVENT_INJURED_CRY_FOR_HELP);
	}

	//if (m_PedType == FPI_PEDTYPE_MICHAEL)
	//{
	//}

	//if (m_PedType == FPI_PEDTYPE_FRANKLIN)
	//{
	//}

	if (m_PedType == FPI_PEDTYPE_TREVOR)
	{
		CHECK_FOR_EVENT(EVENT_COMBAT_TAUNT);
		CHECK_FOR_EVENT(EVENT_SHOCKING_SEEN_WEIRD_PED);				// Note: Trevor is a weird ped.
		CHECK_FOR_EVENT(EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING);
	}

	return NULL;
}			
#undef CHECK_FOR_EVENT

const CPed* camCinematicFirstPersonIdleCamera::GetPedToMakeInvisible() const
{
	if (m_AttachParent.Get() && m_AttachParent->GetIsTypePed())
	{
		return static_cast<const CPed*>(m_AttachParent.Get());
	}

	return NULL;
}

void camCinematicFirstPersonIdleCamera::ResetForNewTarget()
{
	m_TimeSinceTargetLastMoved = fwTimer::GetCamTimeInMilliseconds();
	m_TimeOfLastNoTargetPosUpdate = fwTimer::GetCamTimeInMilliseconds();
	m_ZoomInStartTime = 0;
	m_ZoomOutStartTime = 0;
	m_CheckOutGirlStartTime = 0;
	m_fTurnRate = 0.0f;
	m_ZoomedInTA = false;
	m_HasCheckedOutGirl = false;
	m_LockedOnTarget = false;
	m_StartAngleToTarget = -1.0f;
	m_MinTurnRateScale = 0.0f;

	m_PrevChestZOffset  = -FLT_MAX;
	m_PrevHeadZOffset   = -FLT_MAX;
	m_BlendChestZOffset = false;
	m_BlendHeadZOffset  = false;
}

void camCinematicFirstPersonIdleCamera::OnTargetChange(const CEntity* pOldTarget)
{
	// Once we are done looking at the target, add to history.
	if (pOldTarget)
	{
		AddTargetToHistory(pOldTarget);
	}

	if (m_LookAtTarget.Get())
	{
		if (IsBirdTarget(m_LookAtTarget.Get()))
		{
			m_LastTimeBirdWasTargetted = fwTimer::GetCamTimeInMilliseconds();
		}
		else if (m_LookAtTarget->GetIsTypeVehicle())
		{
			m_LastTimeVehicleWasTargetted = fwTimer::GetCamTimeInMilliseconds();
		}
	}
}

void camCinematicFirstPersonIdleCamera::ClearTarget()
{
	OnTargetChange(m_LookAtTarget.Get());

	m_LookingAtEntity = false;
	m_LookAtTarget = NULL;
	if (m_ZoomOutStartTime == 0)
	{
		m_ZoomOutStartTime = fwTimer::GetCamTimeInMilliseconds();
		m_fZoomStartFov = m_Frame.GetFov();
	}
}

s32 camCinematicFirstPersonIdleCamera::IsInHistoryList(const CEntity* pTarget)
{
	s32 targetIndex = -1;
	for (s32 i = 0; i < FPI_MAX_TARGET_HISTORY; i++)
	{
		if (m_LastValidTarget[i] == pTarget)
		{
			targetIndex = i;
			break;
		}
	}

	return targetIndex;
}

void camCinematicFirstPersonIdleCamera::AddTargetToHistory(const CEntity* pTarget)
{
	if (pTarget == NULL)
	{
		return;
	}

	// If target already in list, do not re-add.
	s32 indexOfMatch = IsInHistoryList(pTarget);
	if (indexOfMatch >= 0)
	{
		if (indexOfMatch != FPI_MAX_TARGET_HISTORY-1)
		{
			// Move up later entries so this target can be moved to the end of the list.
			for (s32 i = indexOfMatch; i < FPI_MAX_TARGET_HISTORY-1; i++)
			{
				m_LastValidTarget[i] = m_LastValidTarget[i+1];
			}
			m_LastValidTarget[FPI_MAX_TARGET_HISTORY-1] = pTarget;
		}
		return;
	}

	// Adding a new target, so shift previous history targets up.
	for (s32 i = 0; i < FPI_MAX_TARGET_HISTORY-1; i++)
	{
		m_LastValidTarget[i] = m_LastValidTarget[i+1];
	}

	m_LastValidTarget[FPI_MAX_TARGET_HISTORY-1] = pTarget;
}

bool camCinematicFirstPersonIdleCamera::IsBirdTarget(const CEntity* pTarget)
{
	if (pTarget && pTarget->GetIsTypePed())
	{
		const CBaseCapsuleInfo* pCapsule = ((CPed*)pTarget)->GetCapsuleInfo();
		if (pCapsule && pCapsule->IsBird())
		{
			return true;
		}
	}

	return false;
}

bool camCinematicFirstPersonIdleCamera::IsValidBirdTarget(const CEntity* UNUSED_PARAM(pTarget))
{
	//Disable the targeting of birds for now, as it highlights a number of issues with the smoothness of the orientation and zoom changes.
	return false;

// 	cameraAssertf(IsBirdTarget(pTarget), "Need to validate that target is a bird before calling this");
// 
// 	const CPed* pBird = static_cast<const CPed*>(pTarget);
// 	{
// 		CTaskMotionBase* pMotionTask = pBird->GetPrimaryMotionTask();
// 		if (pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_BIRD)
// 		{
// 			const bool bReturn = !static_cast<CTaskBirdLocomotion*>(pMotionTask)->ShouldIgnoreDueToContinuousCollisions();
// 			return bReturn;
// 		}
// 	}
// 
// 	return true;
}

// Copied from CPed::IsProne() which doesn't work for the case I need as there is an early out if pPedTarget->GetCollider() is NULL.
bool camCinematicFirstPersonIdleCamera::IsScenarioPedProne(const CPed* pPedTarget) const
{
	if (pPedTarget && pPedTarget->GetPedResetFlag(CPED_RESET_FLAG_IsInStationaryScenario))
	{
		// A detailed check for high LOD rage ragdolls
		if (pPedTarget->GetRagdollInst() != NULL &&
			pPedTarget->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH &&
			pPedTarget->GetRagdollInst()->GetCacheEntry())
		{
			// Check that the pelvis and spine3 have similar heights
			phBoundComposite* bound = pPedTarget->GetRagdollInst()->GetCacheEntry()->GetBound();
			if(bound && bound->GetNumBounds() > 0)
			{
				const Mat34V& ragdollMat = pPedTarget->GetRagdollInst()->GetMatrix();
				Mat34V boundMat;

				Transform(boundMat, pPedTarget->GetRagdollInst()->GetMatrix(), bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS));
				float pelvisHeight = boundMat.GetCol3().GetZf();
				
				//! Some ped variations (e.g. hens, fish etc) don't have some bounds, so check beforehand.

				if( (RAGDOLL_NECK < bound->GetNumBounds()) && (RAGDOLL_FOOT_LEFT < bound->GetNumBounds()) && (RAGDOLL_FOOT_RIGHT < bound->GetNumBounds()))
				{
					Transform(boundMat, ragdollMat, bound->GetCurrentMatrix(RAGDOLL_NECK));
					float neckHeight = boundMat.GetCol3().GetZf();
				
					Transform(boundMat, ragdollMat, bound->GetCurrentMatrix(RAGDOLL_FOOT_LEFT));
					float leftFootHeight = boundMat.GetCol3().GetZf();

					Transform(boundMat, ragdollMat, bound->GetCurrentMatrix(RAGDOLL_FOOT_RIGHT));
					float rightFootHeight = boundMat.GetCol3().GetZf();
				
					// This check is only really good for slopes less than ~40 degrees.  Would like to get more precise, but without a probe.
					const float upperBodyRange = 0.12f;
					const float lowerBodyRange = 0.14f;
					float fNeckPelvisDifference = neckHeight - pelvisHeight;
					if (Abs(fNeckPelvisDifference) < upperBodyRange && (Abs(pelvisHeight-leftFootHeight) < lowerBodyRange || Abs(pelvisHeight-rightFootHeight) < lowerBodyRange))
					{
						return true;
					}

					// For beach pose, both neck and feet are higher than pelvis.
					const float beachUpperBodyRange = 0.265f;
					if (fNeckPelvisDifference > 0.0f && fNeckPelvisDifference < beachUpperBodyRange &&
						((pelvisHeight-leftFootHeight) < lowerBodyRange || (pelvisHeight-rightFootHeight) < lowerBodyRange))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

#if __BANK
void camCinematicFirstPersonIdleCamera::DebugRender()
{
	if (m_LookingAtEntity && m_LookAtTarget.Get())
	{
		//Vec3V vTargetPosition = m_LookAtTarget.Get()->GetTransform().GetPosition();

		//grcDebugDraw::Capsule(vTargetPosition - Vec3V(0.0f, 0.0f, 0.75f),
		//					  vTargetPosition + Vec3V(0.0f, 0.0f, 0.75f),
		//					  0.35f,
		//					  Color_green);

		if (m_LookAtTarget->GetIsTypePed())
		{
			////grcDebugDraw::Line(m_LookatPosition, m_Frame.GetPosition(), Color_green);
			Matrix34 matTemp;
			m_LookAtTarget.Get()->GetMatrixCopy(matTemp);
			matTemp.d = m_LookatPosition;
			grcDebugDraw::Axis(matTemp, 1.0f, false);
		}
		else
		{
			grcDebugDraw::Sphere(m_LookatPosition, 0.55f, Color_green);
		}
	}
	else
	{
		grcDebugDraw::Sphere(m_LookatPosition, 0.30f, Color_cyan);
	}
	
	for (int i = FPI_MAX_TARGET_HISTORY-1; i >= 0; i--)
	{
		if (m_LastValidTarget[i] && m_LastValidTarget[i] != m_LookAtTarget.Get())
		{
			Vec3V vTargetPosition = m_LastValidTarget[i]->GetTransform().GetPosition();
			if (m_LastValidTarget[i]->GetIsTypePed())
			{
				////grcDebugDraw::Line(m_LookatPosition, m_Frame.GetPosition(), Color_green);
				Matrix34 matTemp;
				m_LastValidTarget[i]->GetMatrixCopy(matTemp);
				grcDebugDraw::Axis(matTemp, 0.65f - (0.05f * (float)i), true);
			}
			else
			{
				////grcDebugDraw::Capsule(vTargetPosition - Vec3V(0.0f, 0.0f, 0.85f),
				////					  vTargetPosition + Vec3V(0.0f, 0.0f, 0.85f),
				////					  0.53f,
				////					  Color_red);
				grcDebugDraw::Sphere(vTargetPosition, 0.60f, Color_red);
			}
		}
	}
}
#endif //__BANK
