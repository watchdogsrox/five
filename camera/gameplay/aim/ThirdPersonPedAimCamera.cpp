//
// camera/gameplay/aim/ThirdPersonPedAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "frontend/MobilePhone.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "task/Motion/Locomotion/TaskMotionAiming.h"
#include "task/Physics/TaskNMShot.h"
#include "task/Weapons/Gun/TaskAimGunScripted.h"
#include "task/Vehicle/TaskEnterVehicle.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonPedAimCamera,0x9742C4FA)

extern const u32 g_SelfieAimCameraHash;
extern const u32 g_ValkyrieTurretCameraHash;
extern const u32 g_InsurgentTurretCameraHash;
extern const u32 g_InsurgentTurretAimCameraHash;
extern const u32 g_TechnicalTurretCameraHash;
extern const u32 g_TechnicalTurretAimCameraHash;

#if __BANK
bool g_DebugShouldUseHighHeels						= false;
bool g_DebugShouldUseBareFeet						= false;
float g_AttachPedPelvisOffsetForHighHeels			= 0.0825f;
float g_AttachPedPelvisOffsetForBareFeet			= 0.05f;
#else
const float g_AttachPedPelvisOffsetForHighHeels		= 0.0825f;
const float g_AttachPedPelvisOffsetForBareFeet		= 0.05f;
#endif

const float	g_OffsetScalingSpringConstantForMobilePhoneSelfieMode		= 80.0f;
const float	g_OffsetScalingSpringDampingRatioForMobilePhoneSelfieMode	= 1.0f;
const float g_CustomRollOffsetSpringConstant							= 200.0f;
const float g_CustomRollOffsetSpringDampingRatio						= 1.0f;
const float g_CustomOrbitDistanceScalingSpringConstant					= 70.0f;
const float g_CustomOrbitDistanceScalingDampingRatio					= 1.0f;


#if __BANK
float	camThirdPersonPedAimCamera::ms_DebugFullShotScreenRatio			= 0.0f;
float	camThirdPersonPedAimCamera::ms_DebugOverriddenOrbitDistance		= 0.01f;
bool	camThirdPersonPedAimCamera::ms_ShouldDebugForceFullShot			= false;
bool	camThirdPersonPedAimCamera::ms_ShouldDebugOverrideOrbitDistance	= false;
#endif // __BANK

camThirdPersonPedAimCamera::camThirdPersonPedAimCamera(const camBaseObjectMetadata& metadata)
: camThirdPersonAimCamera(metadata)
, m_Metadata(static_cast<const camThirdPersonPedAimCameraMetadata&>(metadata))
, m_PreviousCinematicCameraForward(VEC3_ZERO)
, m_PreviousCinematicCameraPosition(g_InvalidPosition)
, m_LockOnTargetStunnedEnvelope(NULL)
, m_LockOnTargetStunnedEnvelopeLevel(0.0f)
, m_CachedSeatIndexForTurret(-1)
, m_OrbitHeadingLimitsRatio(0.0f)
, m_previousViewMode(-1)
, m_HasAttachPedFiredEarly(false)
, m_IsTurretCam(false)
{
	const camEnvelopeMetadata* envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(
		m_Metadata.m_LockOnTargetStunnedEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_LockOnTargetStunnedEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_LockOnTargetStunnedEnvelope,
			"A third-person ped aim camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_LockOnTargetStunnedEnvelope->SetUseGameTime(true);
		}
	}
}

camThirdPersonPedAimCamera::~camThirdPersonPedAimCamera()
{
	if(m_LockOnTargetStunnedEnvelope)
	{
		delete m_LockOnTargetStunnedEnvelope;
	}
}

bool camThirdPersonPedAimCamera::IsInAccurateMode() const
{
	bool isInAccurateMode = false;

	if(m_WeaponInfo)
	{
		//NOTE: Accurate mode is only meaningful if it actually zooms the camera.
		const float accurateModeZoomFactor	= ComputeDesiredZoomFactorForAccurateMode(*m_WeaponInfo);
		const bool isAccurateModeValid		= (accurateModeZoomFactor >= (1.0f + SMALL_FLOAT));
		if(isAccurateModeValid)
		{
			isInAccurateMode = (m_ControlHelper && m_ControlHelper->IsInAccurateMode());

			if(!isInAccurateMode && m_AttachParent && m_AttachParent->GetIsTypePed())
			{
				//Check if the attach ped has a forced zoom state, which can be set by script.
				const CPed* attachPed					= static_cast<const CPed*>(m_AttachParent.Get());
				const CPlayerInfo* attachPedPlayerInfo	= attachPed->GetPlayerInfo();
				isInAccurateMode						= attachPedPlayerInfo && attachPedPlayerInfo->GetPlayerResetFlags().IsFlagSet(
															CPlayerResetFlags::PRF_FORCED_ZOOM);
			}
		}
	}

	return isInAccurateMode;
}

bool camThirdPersonPedAimCamera::Update()
{
	if(!m_AttachParent || !m_AttachParent->GetIsTypePed()) //Safety check.
	{
		const CPed* attachPed = camInterface::FindFollowPed();
		if(!attachPed)
		{
			return false;
		}

		AttachTo(attachPed);
	}

	//Check if the attach ped has interrupted the aim intro transition and fired early, as we want to factor this in for reticule visibility.
	const CPedIntelligence* attachPedIntelligence = static_cast<const CPed*>(m_AttachParent.Get())->GetPedIntelligence();
	if(attachPedIntelligence)
	{
		const CTaskMotionAiming* motionAimingTask	= static_cast<const CTaskMotionAiming*>(attachPedIntelligence->FindTaskActiveMotionByType(
														CTaskTypes::TASK_MOTION_AIMING));
		if(motionAimingTask && motionAimingTask->HasAimIntroTimedOut())
		{
			const CQueriableInterface* pedQueriableInterface = attachPedIntelligence->GetQueriableInterface();
			if(pedQueriableInterface)
			{
				const bool isAttachPedAimingOnFoot = pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_ON_FOOT);
				if(isAttachPedAimingOnFoot)
				{
					const s32 aimState = pedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_AIM_GUN_ON_FOOT);
					if(aimState == CTaskAimGunOnFoot::State_Firing)
					{
						m_HasAttachPedFiredEarly = true;
					}
				}
			}
		}
	}

	UpdateCustomOffsetScaling();

	UpdateRelativeOrbitHeadingLimits();

	const bool hasSucceeded = camThirdPersonAimCamera::Update();

	UpdateCustomRollOffset();

	const camControlHelper* pControlHelper = GetControlHelper();
	const bool c_bViewModeChangedFromFirstPerson =	pControlHelper && pControlHelper->GetViewMode() != m_previousViewMode &&
													m_previousViewMode == camControlHelperMetadataViewMode::FIRST_PERSON;
	if( m_IsTurretCam && c_bViewModeChangedFromFirstPerson )
	{
		m_PreviousCinematicCameraForward  = VEC3_ZERO;
		m_PreviousCinematicCameraPosition = g_InvalidPosition;
	}

	if(pControlHelper)
	{
		m_previousViewMode = pControlHelper->GetViewMode();
	}

	return hasSucceeded;
}

void camThirdPersonPedAimCamera::UpdateRelativeOrbitHeadingLimits()
{
	const u32 nameHash = GetNameHash();
	if(nameHash == g_ValkyrieTurretCameraHash)
	{
		if(m_AttachParent && m_AttachParent->GetIsTypePed())
		{
			CPed* attachedPed = (CPed*)m_AttachParent.Get();
			
			const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();
			if(followVehicle) // Q: is this check necessary?
			{
				const CSeatManager* seatManager = followVehicle->GetSeatManager();
				if(seatManager) 
				{				
					const s32 iSeatIndex = seatManager->GetPedsSeatIndex(attachedPed);
					if(iSeatIndex != -1 && camGameplayDirector::IsTurretSeat(followVehicle, iSeatIndex)) 
					{
						const CVehicleWeaponMgr* weaponManager = followVehicle->GetVehicleWeaponMgr();
						if(weaponManager)
						{
							CTurret* turret = weaponManager->GetFirstTurretForSeat(iSeatIndex);
							if(turret)
							{			
								Matrix34 turretMatrixWorld;
								turret->GetTurretMatrixWorld(turretMatrixWorld, followVehicle);

								Matrix34 vehicleMatrix = MAT34V_TO_MATRIX34(followVehicle->GetMatrix());

								Vector3 turretPosition; 
								vehicleMatrix.UnTransform(turretMatrixWorld.d, turretPosition);

								const float sign = Sign(turretPosition.x);

								if(sign < 0.0f)
								{
									m_RelativeOrbitHeadingLimits = m_Metadata.m_RelativeOrbitHeadingLimits;
								}
								else
								{
									m_RelativeOrbitHeadingLimits.x = -m_Metadata.m_RelativeOrbitHeadingLimits.y;
									m_RelativeOrbitHeadingLimits.y = -m_Metadata.m_RelativeOrbitHeadingLimits.x;
								}								

								m_RelativeOrbitHeadingLimits *= DtoR;
							}
						}
					}
				}
			}		
		}		
	}
}

float camThirdPersonPedAimCamera::ComputeIdealOrbitHeadingForLimiting() const
{
	float idealHeading = camThirdPersonCamera::ComputeIdealOrbitHeadingForLimiting();

	if(m_Metadata.m_ShouldUseVehicleMatrixForOrientationLimiting)
	{
		const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();
		if(followVehicle)
		{
			const float attachParentVehicleHeading	= camFrame::ComputeHeadingFromMatrix(MAT34V_TO_MATRIX34(followVehicle->GetMatrix()));
			const float headingOffsetToApply		= m_Metadata.m_IdealHeadingOffsetForLimiting * DtoR;
			idealHeading							= attachParentVehicleHeading + headingOffsetToApply;
			idealHeading							= fwAngle::LimitRadianAngle(idealHeading);
		}
	}

	return idealHeading;
}

void camThirdPersonPedAimCamera::UpdateCustomOffsetScaling()
{
	const u32 nameHash = GetNameHash();
	if(nameHash != g_SelfieAimCameraHash)
	{
		m_CustomSideOffsetScalingSpring.Reset(1.0f);
		m_CustomVertOffsetScalingSpring.Reset(1.0f);
		return;
	}

	//Apply damping to the side offset requested by the mobile phone (script) in 'selfie' mode.
	const float desiredSideOffsetScaling	= CPhoneMgr::CamGetSelfieModeSideOffsetScaling();
	const float desiredVertOffsetScaling	= CPhoneMgr::CamGetSelfieModeVertOffsetScaling();
	const bool shouldCut					= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant				= shouldCut ? 0.0f : g_OffsetScalingSpringConstantForMobilePhoneSelfieMode;

	m_CustomSideOffsetScalingSpring.Update(desiredSideOffsetScaling, springConstant, g_OffsetScalingSpringDampingRatioForMobilePhoneSelfieMode);
	m_CustomVertOffsetScalingSpring.Update(desiredVertOffsetScaling, springConstant, g_OffsetScalingSpringDampingRatioForMobilePhoneSelfieMode);
}

void camThirdPersonPedAimCamera::UpdateCustomRollOffset()
{
	const u32 nameHash = GetNameHash();
	if(nameHash != g_SelfieAimCameraHash)
	{
		m_CustomRollOffsetSpring.Reset(0.0f);
		return;
	}

	const float desiredRollOffset = CPhoneMgr::CamGetSelfieRoll();
	const bool bCut = ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant = bCut ? 0.0f : g_CustomRollOffsetSpringConstant;

	m_CustomRollOffsetSpring.Update(desiredRollOffset, springConstant, g_CustomRollOffsetSpringDampingRatio);

	Mat34V mtxFrame(MATRIX34_TO_MAT34V(m_Frame.GetWorldMatrix()));
	Mat34VRotateLocalY(mtxFrame, ScalarV(m_CustomRollOffsetSpring.GetResult() * DtoR));
	m_Frame.SetWorldMatrix(RC_MATRIX34(mtxFrame));
}

bool camThirdPersonPedAimCamera::ComputeShouldForceAttachPedPelvisOffsetToBeConsidered() const
{
	const u32 nameHash		= GetNameHash();
	const bool shouldForce	= (nameHash == g_SelfieAimCameraHash);

	return shouldForce;
}

bool camThirdPersonPedAimCamera::GetTurretMatrixForVehicleSeat(const CVehicle& vehicle, const s32 seatIndex, Matrix34& turretMatrixOut) const
{
	const CVehicleWeaponMgr* vehicleWeaponManager = vehicle.GetVehicleWeaponMgr();
	if(vehicleWeaponManager)
	{
		const CTurret* turret = vehicleWeaponManager->GetFirstTurretForSeat(seatIndex);
		if(turret)
		{	
			if(CTaskMotionInTurret::IsSeatWithHatchProtection(seatIndex, vehicle))
			{
				const s16 boneIndex = vehicle.GetVehicleModelInfo() && vehicle.GetVehicleModelInfo()->GetModelSeatInfo() ?
					static_cast<s16>(vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(seatIndex)) : -1;
				if (boneIndex > -1)
				{
					vehicle.GetGlobalMtx(boneIndex, turretMatrixOut);
					return true;
				}
			}
			else
			{
				turret->GetTurretMatrixWorld(turretMatrixOut, &vehicle);
				return true;
			}
		}
	}

	return false;
}

void camThirdPersonPedAimCamera::UpdateBaseAttachPosition()
{
	camThirdPersonAimCamera::UpdateBaseAttachPosition();

	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	CPed* attachedPed = (CPed*)m_AttachParent.Get();

	if(m_Metadata.m_AttachBoneTag >= 0)
	{				
		const eAnimBoneTag boneTag	= (eAnimBoneTag)m_Metadata.m_AttachBoneTag;

		Vector3 bonePosition;
		const bool isBoneValid = attachedPed->GetBonePosition(bonePosition, boneTag);
		if(isBoneValid)
		{
			m_BaseAttachPosition = bonePosition;
		}
	}
	else if(m_IsTurretCam)
	{
		const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();		
		if(followVehicle)
		{		
			s32 seatIndex = -1;
			const CPedIntelligence* pedIntelligence	= attachedPed->GetPedIntelligence();
			if(pedIntelligence)
			{							
				const CTaskEnterVehicle* enterVehicleTask = static_cast<CTaskEnterVehicle*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));	
				if(enterVehicleTask)
				{					
					seatIndex = enterVehicleTask->GetTargetSeat();				
				}
				else
				{
					const CSeatManager* seatManager = followVehicle->GetSeatManager();
					if(seatManager) 
					{									
						seatIndex = seatManager->GetPedsSeatIndex(attachedPed);
					}
				}
			}
			
			Matrix34 turretMatrix;
			if(seatIndex != -1 && GetTurretMatrixForVehicleSeat(*followVehicle, seatIndex, turretMatrix))
			{									
				m_CachedSeatIndexForTurret = seatIndex;
				m_BaseAttachPosition = turretMatrix.d;			
			}
			else if(m_CachedSeatIndexForTurret != -1 && GetTurretMatrixForVehicleSeat(*followVehicle, m_CachedSeatIndexForTurret, turretMatrix))
			{
				m_BaseAttachPosition = turretMatrix.d;			
			}
		}
	}

	//Compute the ratio of the orbit heading between the limits, giving a value of 1.0f at a limit and 0.0f at the midpoint.
	float orbitHeadingLimitsRatioAbs = 0.0f;
	m_OrbitHeadingLimitsRatio = orbitHeadingLimitsRatioAbs;

	if(m_NumUpdatesPerformed > 0)
	{
		//NOTE: We must consider the heading limit ratio from the previous update.
		//TODO: Work around this properly.

		const float baseFrontHeading	= camFrame::ComputeHeadingFromFront(m_BaseFront);
		const float idealHeading		= ComputeIdealOrbitHeadingForLimiting();
		float relativeHeading			= baseFrontHeading - idealHeading;
		relativeHeading					= fwAngle::LimitRadianAngle(relativeHeading);
		const float relativeHeadingDeg	= relativeHeading * RtoD;

		m_OrbitHeadingLimitsRatio		= RampValueSafe(relativeHeadingDeg, m_Metadata.m_RelativeOrbitHeadingLimits.x,
											m_Metadata.m_RelativeOrbitHeadingLimits.y, -1.0f, 1.0f);
		orbitHeadingLimitsRatioAbs		= Abs(m_OrbitHeadingLimitsRatio);
		orbitHeadingLimitsRatioAbs		= SlowInOut(orbitHeadingLimitsRatioAbs);
	}

	Vector3 relativeAttachOffsetToApply;
	relativeAttachOffsetToApply.Lerp(orbitHeadingLimitsRatioAbs, m_Metadata.m_ParentRelativeAttachOffset,
		m_Metadata.m_ParentRelativeAttachOffsetAtOrbitHeadingLimits);

	Vector3 attachOffsetToApply;
	m_AttachParentMatrix.Transform3x3(relativeAttachOffsetToApply, attachOffsetToApply);

	m_BaseAttachPosition += attachOffsetToApply;
}

float camThirdPersonPedAimCamera::ComputeAttachPedPelvisOffsetForShoes() const
{
	//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
	const CPed* attachedPed	= static_cast<const CPed*>(m_AttachParent.Get());
	const bool hasHighHeels	= attachedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasHighHeels) BANK_ONLY(|| g_DebugShouldUseHighHeels);
	const bool hasBareFeet  = attachedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasBareFeet)  BANK_ONLY(|| g_DebugShouldUseBareFeet);
	float offset = hasHighHeels ? g_AttachPedPelvisOffsetForHighHeels : 0.0f;

	offset -= hasBareFeet ? g_AttachPedPelvisOffsetForBareFeet : 0.0f;
	return offset;
}

void camThirdPersonPedAimCamera::UpdateOrbitPitchLimits()
{
	camThirdPersonAimCamera::UpdateOrbitPitchLimits();

	//If the attach ped is running a scripted aim task, use the custom pitch limits specified for that task, as the ped will be using these.
	if(m_Metadata.m_ShouldScriptedAimTaskOverrideOrbitPitchLimits && m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed							= static_cast<const CPed*>(m_AttachParent.Get());
		const CPedIntelligence* attachPedIntelligence	= attachPed->GetPedIntelligence();
		if(attachPedIntelligence)
		{
			const CTaskAimGunScripted* scriptedAimTask	= static_cast<const CTaskAimGunScripted*>(attachPedIntelligence->FindTaskActiveByType(
															CTaskTypes::TASK_AIM_GUN_SCRIPTED));
			if(scriptedAimTask)
			{
				const float minPitchAngle = scriptedAimTask->GetMinPitchAngle();
				const float maxPitchAngle = scriptedAimTask->GetMaxPitchAngle();

				m_OrbitPitchLimits.Set(minPitchAngle, maxPitchAngle);

				return;
			}
		}
	}

	//NOTE: We ignore the pitch limits for melee weapons, as they are invalid.
	if(m_Metadata.m_ShouldAimSweepOverrideOrbitPitchLimits && m_WeaponInfo && !m_WeaponInfo->GetIsMelee())
	{
		const CAimingInfo* info = m_WeaponInfo->GetAimingInfo();
		if(info)
		{
			m_OrbitPitchLimits.Set(info->GetSweepPitchMin(), info->GetSweepPitchMax());
			m_OrbitPitchLimits.Scale(DtoR);
		}
	}
}

void camThirdPersonPedAimCamera::ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const
{
#if __BANK
	if(ms_ShouldDebugForceFullShot)
	{
		screenRatioForMinFootRoom = screenRatioForMaxFootRoom = ms_DebugFullShotScreenRatio;

		return;
	}
#endif // __BANK

	camThirdPersonAimCamera::ComputeScreenRatiosForFootRoomLimits(screenRatioForMinFootRoom, screenRatioForMaxFootRoom);
}

float camThirdPersonPedAimCamera::UpdateOrbitDistance()
{
	const float baseOrbitDistance = camThirdPersonAimCamera::UpdateOrbitDistance();

#if __BANK
	if(ms_ShouldDebugOverrideOrbitDistance)
	{
		return ms_DebugOverriddenOrbitDistance;
	}
#endif // __BANK

	float scalingToApply;

	const u32 nameHash = GetNameHash();
	if(nameHash == g_SelfieAimCameraHash)
	{
		const float desiredCustomOrbitDistanceScaling = CPhoneMgr::CamGetSelfieDistance();
		const bool bCut = ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
		const float springConstant = bCut ? 0.0f : g_CustomOrbitDistanceScalingSpringConstant;

		m_CustomOrbitDistanceScalingSpring.Update(desiredCustomOrbitDistanceScaling, springConstant, g_CustomOrbitDistanceScalingDampingRatio);	

		scalingToApply = m_LockOnOrbitDistanceScalingSpring.GetResult() * m_CustomOrbitDistanceScalingSpring.GetResult();
	}	
	else
	{
		m_CustomOrbitDistanceScalingSpring.Reset(1.0f);

		scalingToApply = m_LockOnOrbitDistanceScalingSpring.GetResult();		
	}
	
	const float orbitDistanceToApply = baseOrbitDistance * scalingToApply;

	return orbitDistanceToApply;
}

void camThirdPersonPedAimCamera::ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const
{
	camThirdPersonAimCamera::ComputeOrbitRelativePivotOffsetInternal(orbitRelativeOffset);

	const u32 nameHash = GetNameHash();
	if(nameHash == g_SelfieAimCameraHash)
	{		
		const float sideOffsetScaling	= m_CustomSideOffsetScalingSpring.GetResult();
		const float vertOffsetScaling	= m_CustomVertOffsetScalingSpring.GetResult();		

		orbitRelativeOffset.x *= sideOffsetScaling;	
		orbitRelativeOffset.z *= vertOffsetScaling;	
	}
	else if(nameHash == g_ValkyrieTurretCameraHash)
	{				
		const float baseFrontHeading	= camFrame::ComputeHeadingFromFront(m_BaseFront);
		const float idealHeading		= ComputeIdealOrbitHeadingForLimiting();
		const float relativeHeading		= fwAngle::LimitRadianAngle(baseFrontHeading - idealHeading);		
		const float relativeHeadingDeg	= relativeHeading * RtoD;					

		const float sideOffsetScaling	= RampValueSafe(relativeHeadingDeg, m_Metadata.m_RelativeOrbitHeadingLimits.x,
			m_Metadata.m_RelativeOrbitHeadingLimits.y, -1.0f, 1.0f);

		orbitRelativeOffset.x *= sideOffsetScaling;	
	}	
}

#if __BANK
float camThirdPersonPedAimCamera::ComputeBasePivotHeightScalingForFootRoom() const
{
	const float scaling = ms_ShouldDebugForceFullShot ? 1.0f : camThirdPersonAimCamera::ComputeBasePivotHeightScalingForFootRoom();

	return scaling;
}

void camThirdPersonPedAimCamera::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Third-person ped aim camera");
	{
		bank.PushGroup("Full shot");
		{
			bank.AddToggle("Enable", &ms_ShouldDebugForceFullShot);
			bank.AddSlider("Screen ratio", &ms_DebugFullShotScreenRatio, -1.0f, 0.49f, 0.05f);
		}
		bank.PopGroup(); //Full shot

		bank.PushGroup("Override orbit distance");
		{
			bank.AddToggle("Enable", &ms_ShouldDebugOverrideOrbitDistance);
			bank.AddSlider("Orbit distance", &ms_DebugOverriddenOrbitDistance, 0.001f, 10.0f, 0.001f);
		}
		bank.PopGroup(); //Override orbit distance

		bank.PushGroup("Shoes offsetting");
		{
			bank.AddToggle("Simulate high heels", &g_DebugShouldUseHighHeels);
			bank.AddSlider("High heels offset", &g_AttachPedPelvisOffsetForHighHeels, 0.0f, 1.0f, 0.01f);
			bank.AddToggle("Simulate bare feet", &g_DebugShouldUseBareFeet);
			bank.AddSlider("Bare feet offset", &g_AttachPedPelvisOffsetForBareFeet, -1.0f, 1.0f, 0.01f);
		}
		bank.PopGroup(); //Shoes offsetting
	}
	bank.PopGroup(); //Third-person ped aim camera
}
#endif // __BANK

void camThirdPersonPedAimCamera::UpdateLockOn()
{
	camThirdPersonAimCamera::UpdateLockOn();

	float desiredLockOnOrbitDistanceScaling = 1.0f;
	if(m_IsLockedOn && m_Metadata.m_LockOnOrbitDistanceSettings.m_ShouldApplyScaling)
	{
		const float lockOnDistance				= m_BasePivotPosition.Dist(m_LockOnTargetPosition);
		float desiredOrbitDistanceScalingRatio	= RampValueSafe(lockOnDistance, m_Metadata.m_LockOnOrbitDistanceSettings.m_LockOnDistanceLimits.x,
													m_Metadata.m_LockOnOrbitDistanceSettings.m_LockOnDistanceLimits.y, 0.0f, 1.0f);
		desiredOrbitDistanceScalingRatio		= SlowInOut(desiredOrbitDistanceScalingRatio);
		desiredLockOnOrbitDistanceScaling		= Lerp(desiredOrbitDistanceScalingRatio,
													m_Metadata.m_LockOnOrbitDistanceSettings.m_OrbitDistanceScalingLimits.x,
													m_Metadata.m_LockOnOrbitDistanceSettings.m_OrbitDistanceScalingLimits.y);
	}

	if((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
		m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
	{
		m_LockOnOrbitDistanceScalingSpring.Reset(desiredLockOnOrbitDistanceScaling);
	}
	else
	{
		m_LockOnOrbitDistanceScalingSpring.Update(desiredLockOnOrbitDistanceScaling,
			m_Metadata.m_LockOnOrbitDistanceSettings.m_OrbitDistanceScalingSpringConstant,
			m_Metadata.m_LockOnOrbitDistanceSettings.m_OrbitDistanceScalingSpringDampingRatio);
	}
}

void camThirdPersonPedAimCamera::UpdateLockOnEnvelopes(const CEntity* desiredLockOnTarget)
{
	camThirdPersonAimCamera::UpdateLockOnEnvelopes(desiredLockOnTarget);

	const CPed* lockOnTargetPed	= (m_IsLockedOn && desiredLockOnTarget && desiredLockOnTarget->GetIsTypePed()) ?
		static_cast<const CPed*>(desiredLockOnTarget) : NULL;

	const bool isLockedOnToStunnedPed = lockOnTargetPed && ComputeIsPedStunned(*lockOnTargetPed);
	if(m_LockOnTargetStunnedEnvelope)
	{
		m_LockOnTargetStunnedEnvelope->AutoStartStop(isLockedOnToStunnedPed);

		m_LockOnTargetStunnedEnvelopeLevel	= m_LockOnTargetStunnedEnvelope->Update();
		m_LockOnTargetStunnedEnvelopeLevel	= Clamp(m_LockOnTargetStunnedEnvelopeLevel, 0.0f, 1.0f);
		m_LockOnTargetStunnedEnvelopeLevel	= SlowInOut(m_LockOnTargetStunnedEnvelopeLevel);
	}
	else
	{
		//We don't have an envelope, so just use the state directly.
		m_LockOnTargetStunnedEnvelopeLevel	= isLockedOnToStunnedPed ? 1.0f : 0.0f;
	}
}

bool camThirdPersonPedAimCamera::ComputeIsPedStunned(const CPed& ped) const
{
	//NOTE: This logic is based on the IS_PED_BEING_STUNNED script command.

	const CPedIntelligence* pedIntelligence = ped.GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
	if(!pedQueriableInterface)
	{
		return false;
	}

	//NOTE: We explicitly exclude blend from NM, as the ped can run this task for an extended period, whilst aiming from the ground, for example.
	const bool isPedBlendingFromNm = pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_BLEND_FROM_NM);
	if(isPedBlendingFromNm)
	{
		return false;
	}

	bool isPedStunned	= pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_ELECTROCUTE) ||
		pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_DAMAGE_ELECTRIC);

	if(!isPedStunned && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_SHOT))
	{
		const CClonedNMShotInfo* taskInfo = static_cast<const CClonedNMShotInfo*>(pedQueriableInterface->GetTaskInfoForTaskType(
			CTaskTypes::TASK_NM_SHOT, PED_TASK_PRIORITY_MAX, false));
		if(taskInfo)
		{
			const u32 weaponHash = taskInfo->GetWeaponHash();
			if(weaponHash)
			{
				const CWeaponInfo* weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
				if(weaponInfo)
				{
					const eDamageType weaponDamageType = weaponInfo->GetDamageType();

					isPedStunned = (weaponDamageType == DAMAGE_TYPE_BULLET_RUBBER) || (weaponDamageType == DAMAGE_TYPE_ELECTRIC);
				}
			}
		}
	}

	return isPedStunned;
}

void camThirdPersonPedAimCamera::UpdateLockOnTargetDamping(bool hasChangedLockOnTargetThisUpdate, Vector3& desiredLockOnTargetPosition)
{
	camThirdPersonAimCamera::UpdateLockOnTargetDamping(hasChangedLockOnTargetThisUpdate, desiredLockOnTargetPosition);

	//NOTE: We only apply damping to the lock-on target position for ped targets.
	if(!m_Metadata.m_LockOnTargetDampingSettings.m_ShouldApplyDamping || !m_LockOnTarget || !m_LockOnTarget->GetIsTypePed())
	{
		return;
	}

	const Vector3 desiredLockOnTargetDelta	= desiredLockOnTargetPosition - m_BasePivotPosition;
	const float desiredLockOnTargetDistance	= desiredLockOnTargetDelta.Mag();

	Vector3 desiredLockOnTargetFront(desiredLockOnTargetDelta);
	desiredLockOnTargetFront.Normalize();

	float desiredLockOnTargetHeading;
	float desiredLockOnTargetPitch;
	camFrame::ComputeHeadingAndPitchFromFront(desiredLockOnTargetFront, desiredLockOnTargetHeading, desiredLockOnTargetPitch);

	if(hasChangedLockOnTargetThisUpdate || (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
		m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
	{
		m_LockOnTargetStunnedHeadingSpring.Reset(desiredLockOnTargetHeading);
		m_LockOnTargetStunnedPitchSpring.Reset(desiredLockOnTargetPitch);
		m_LockOnTargetStunnedDistanceSpring.Reset(desiredLockOnTargetDistance);
	}
	else
	{
		//Ensure that the heading spring caches a wrapped angle.
		float currentDampedLockOnHeading	= m_LockOnTargetStunnedHeadingSpring.GetResult();
		currentDampedLockOnHeading			= fwAngle::LimitRadianAngleSafe(currentDampedLockOnHeading);
		m_LockOnTargetStunnedHeadingSpring.OverrideResult(currentDampedLockOnHeading);

		//Ensure that we blend to the target heading over the shortest angle.
		float desiredLockOnTargetHeadingShortest	= desiredLockOnTargetHeading;
		const float desiredLockOnTargetHeadingDelta	= desiredLockOnTargetHeading - currentDampedLockOnHeading;
		if(desiredLockOnTargetHeadingDelta > PI)
		{
			desiredLockOnTargetHeadingShortest -= TWO_PI;
		}
		else if(desiredLockOnTargetHeadingDelta < -PI)
		{
			desiredLockOnTargetHeadingShortest += TWO_PI;
		}

		m_LockOnTargetStunnedHeadingSpring.Update(desiredLockOnTargetHeadingShortest,
			m_Metadata.m_LockOnTargetDampingSettings.m_StunnedHeadingSpringConstant,
			m_Metadata.m_LockOnTargetDampingSettings.m_StunnedHeadingSpringDampingRatio);

		m_LockOnTargetStunnedPitchSpring.Update(desiredLockOnTargetPitch,
			m_Metadata.m_LockOnTargetDampingSettings.m_StunnedPitchSpringConstant,
			m_Metadata.m_LockOnTargetDampingSettings.m_StunnedPitchSpringDampingRatio);

		m_LockOnTargetStunnedDistanceSpring.Update(desiredLockOnTargetDistance,
			m_Metadata.m_LockOnTargetDampingSettings.m_StunnedDistanceSpringConstant,
			m_Metadata.m_LockOnTargetDampingSettings.m_StunnedDistanceSpringDampingRatio);
	}

	if(m_LockOnTargetStunnedEnvelopeLevel < SMALL_FLOAT)
	{
		return;
	}

	const float dampedLockOnTargetPedStunnedHeading		= m_LockOnTargetStunnedHeadingSpring.GetResult();
	const float dampedLockOnTargetPedStunnedPitch		= m_LockOnTargetStunnedPitchSpring.GetResult();
	const float dampedLockOnTargetPedStunnedDistance	= m_LockOnTargetStunnedDistanceSpring.GetResult();

	Vector3 dampedLockOnTargetPedStunnedFront;
	camFrame::ComputeFrontFromHeadingAndPitch(dampedLockOnTargetPedStunnedHeading, dampedLockOnTargetPedStunnedPitch, dampedLockOnTargetPedStunnedFront);

	//Blend to the stunned-specific damped orientation and distance based upon the envelope level.

	Vector3 lockOnTargetFrontToApply;
	camFrame::SlerpOrientation(m_LockOnTargetStunnedEnvelopeLevel, desiredLockOnTargetFront, dampedLockOnTargetPedStunnedFront,
		lockOnTargetFrontToApply);

	const float lockOnTargetDistanceToApply	= Lerp(m_LockOnTargetStunnedEnvelopeLevel, desiredLockOnTargetDistance, dampedLockOnTargetPedStunnedDistance);

	desiredLockOnTargetPosition = m_BasePivotPosition + (lockOnTargetFrontToApply * lockOnTargetDistanceToApply);
}

float camThirdPersonPedAimCamera::ComputeDesiredZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const
{
	float zoomFactor = camThirdPersonAimCamera::ComputeDesiredZoomFactorForAccurateMode(weaponInfo);
	float extraZoomFactor = ComputeExtraZoomFactorForAccurateMode(weaponInfo);
	return zoomFactor * extraZoomFactor;
}

float camThirdPersonPedAimCamera::ComputeExtraZoomFactorForAccurateMode(const CWeaponInfo& weaponInfo) const
{
	float extraZoomFactor = 1.0f;

	if(m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		//Check if this weapon has a scope component in the attach ped's inventory.
		const CPed* attachPed			= static_cast<const CPed*>(m_AttachParent.Get());
		const CPedInventory* inventory	= attachPed->GetInventory();
		if(inventory)
		{
			const CWeaponItem* weaponItem = inventory->GetWeapon(weaponInfo.GetHash());
			if(weaponItem)
			{
				if(weaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *weaponItem->GetComponents();
					for(s32 i=0; i<components.GetCount(); i++)
					{
						const CWeaponComponentInfo* componentInfo = components[i].pComponentInfo;
						if(componentInfo && (componentInfo->GetClassId() == WCT_Scope))
						{
							extraZoomFactor	= static_cast<const CWeaponComponentScopeInfo*>(componentInfo)->
								GetExtraZoomFactorForAccurateMode();

							break;
						}
					}
				}
			}
		}
	}

	return extraZoomFactor;
}

void camThirdPersonPedAimCamera::UpdateFrameShakers(camFrame& frameToShake, float amplitudeScalingFactor /*= 1.0f*/)
{
    const float totalShakeAmplitude = amplitudeScalingFactor * m_Metadata.m_ShakeAmplitudeScaleFactor;
    camThirdPersonAimCamera::UpdateFrameShakers(frameToShake, totalShakeAmplitude);
}

float camThirdPersonPedAimCamera::GetFrameShakerScaleFactor() const
{
    return m_Metadata.m_ShakeAmplitudeScaleFactor;
}

bool camThirdPersonPedAimCamera::ShouldPreventBlendToItself() const
{
	return m_Metadata.m_ShouldPreventBlendToItself;
}

bool camThirdPersonPedAimCamera::ComputeShouldDisplayReticuleDuringInterpolation(const camFrameInterpolator& frameInterpolator) const
{
	const bool shouldDisplayReticule = m_HasAttachPedFiredEarly || camThirdPersonAimCamera::ComputeShouldDisplayReticuleDuringInterpolation(frameInterpolator);

	return shouldDisplayReticule;
}

void camThirdPersonPedAimCamera::UpdateDof()
{
	camThirdPersonAimCamera::UpdateDof();

	//Lock the measured focus distance for the adaptive DOF behaviour if the attach ped is reloading or performing a melee attack with a non-melee weapon.
	//Otherwise, the weapon object can influence the focus distance if it is animated in front of the reticule/focus grid.
	if(m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const CPedIntelligence* pedIntelligence = static_cast<const CPed*>(m_AttachParent.Get())->GetPedIntelligence();
		if(pedIntelligence)
		{
			const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
			if(pedQueriableInterface)
			{
				const bool isReloading				= pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_RELOAD_GUN);
				const bool isPerformingMeleeAction	= pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE_ACTION_RESULT);
				const bool isUsingMeleeWeapon		= (m_WeaponInfo && m_WeaponInfo->GetIsMelee());
				if(isReloading || (isPerformingMeleeAction && !isUsingMeleeWeapon))
				{
					m_PostEffectFrame.GetFlags().SetFlag(camFrame::Flag_ShouldLockAdaptiveDofFocusDistance);
				}
			}
		}
	}
}

void camThirdPersonPedAimCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camThirdPersonAimCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	if(CPhoneMgr::CamGetShallowDofModeState())
	{
		const camDepthOfFieldSettingsMetadata* customSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_Metadata.m_DofSettingsForMobilePhoneShallowDofMode.GetHash());
		if(customSettings)
		{
			settings = *customSettings;
		}
	}

	//NOTE: We only actually measure post-alpha pixel depth when flagged *and* when aiming at a vehicle.
	//- This allows us to avoid blurring out a vehicle when aiming through its windows without introducing more general problems with focusing on things like the windows of buildings.
	if(settings.m_ShouldMeasurePostAlphaPixelDepth)
	{
		bool isAimingAtVehicle = false;
		if(m_AttachParent && m_AttachParent->GetIsTypePed())
		{
			const CPed* attachPed			= static_cast<const CPed*>(m_AttachParent.Get());
			const CPlayerInfo* playerInfo	= attachPed->GetPlayerInfo();
			isAimingAtVehicle				= playerInfo && playerInfo->GetTargeting().IsFreeAimTargetingVehicleForCamera();
		}

		settings.m_ShouldMeasurePostAlphaPixelDepth = isAimingAtVehicle;
	}
}

void camThirdPersonPedAimCamera::CacheCinematicCameraOrientation(const camFrame& cinematicCamFrame)
{
	if( camInterface::IsDominantRenderedDirector(camInterface::GetCinematicDirector()) )
	{
		m_PreviousCinematicCameraForward	= cinematicCamFrame.GetFront();
		m_PreviousCinematicCameraPosition	= cinematicCamFrame.GetPosition();
	}
}

bool camThirdPersonPedAimCamera::ReorientCameraToPreviousAimDirection()
{
	bool bChangeOrientation = false;
	const camControlHelper* pControlHelper = GetControlHelper();
	Vector3 previousCameraForward  = camInterface::GetGameplayDirector().GetPreviousAimCameraForward();
	Vector3 previousCameraPosition = camInterface::GetGameplayDirector().GetPreviousAimCameraPosition();

	//TUNE_GROUP_BOOL(TURRET_POV_CAMERA_TUNE, CAMERA_POV_KEEP_THIRD_PERSON_UP_TO_DATE, false);
	const bool c_bViewModeChangedFromFirstPerson =	pControlHelper && pControlHelper->GetViewMode() != m_previousViewMode &&
													m_previousViewMode == camControlHelperMetadataViewMode::FIRST_PERSON;
	//const bool c_bPovTurretCameraRendering =	(CAMERA_POV_KEEP_THIRD_PERSON_UP_TO_DATE) ? 
	//												pControlHelper && 
	//												pControlHelper->GetViewMode() == camControlHelperMetadataViewMode::FIRST_PERSON &&
	//												camInterface::GetCinematicDirector().IsRendering() &&
	//												camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera() : false;
	if( m_NumUpdatesPerformed == 0 && !m_IsLockedOn &&
		!m_Metadata.m_ShouldOrbitRelativeToAttachParentOrientation &&
		!previousCameraPosition.IsClose(g_InvalidPosition, SMALL_FLOAT) )
	{
		bChangeOrientation = true;
	}
	else if( m_IsTurretCam && pControlHelper && 
			( c_bViewModeChangedFromFirstPerson /*|| c_bPovTurretCameraRendering*/ ) &&
			!m_PreviousCinematicCameraPosition.IsClose(g_InvalidPosition, SMALL_FLOAT) )
	{
		previousCameraForward  = m_PreviousCinematicCameraForward;
		previousCameraPosition = m_PreviousCinematicCameraPosition;
		bChangeOrientation = true;
	}

	if( bChangeOrientation && m_AttachParent.Get() && m_AttachParent->GetIsTypePed() )
	{
		const CPed* pAttachPed = (CPed*)m_AttachParent.Get();

		// B* 2177002: use line test so forward vector does not change. ok?
		WorldProbe::CShapeTestProbeDesc lineTest;
		WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
		lineTest.SetResultsStructure(&shapeTestResults);
		lineTest.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);
		lineTest.SetContext(WorldProbe::LOS_Camera);
		lineTest.SetIsDirected(true);
		const u32 collisionIncludeFlags =	(u32)ArchetypeFlags::GTA_ALL_TYPES_WEAPON |
											(u32)ArchetypeFlags::GTA_RAGDOLL_TYPE |
											(u32)ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
		lineTest.SetIncludeFlags(collisionIncludeFlags);
		lineTest.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);

		const Vector3 playerPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		const Vector3 previousCameraPosToPlayer = playerPosition + (previousCameraForward * (pAttachPed->GetCurrentMainMoverCapsuleRadius() + FLT_EPSILON)) - previousCameraPosition;
		float fDotProjection = Max(previousCameraPosToPlayer.Dot(previousCameraForward), 0.0f);
		const Vector3 vProbeStartPosition = previousCameraPosition + previousCameraForward*fDotProjection;

		const float maxProbeDistance = 50.0f;
		Vector3 vTargetPoint = vProbeStartPosition+previousCameraForward*maxProbeDistance;
		lineTest.SetStartAndEnd(vProbeStartPosition, vTargetPoint);
		lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		if(m_IsTurretCam && camInterface::GetGameplayDirector().GetFollowVehicle())
		{
			lineTest.AddExludeInstance(camInterface::GetGameplayDirector().GetFollowVehicle()->GetCurrentPhysicsInst());
		}

		DEV_ONLY( CEntity* pHitEntity = NULL; )
		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
		if( hasHit )
		{
			vTargetPoint = shapeTestResults[0].GetHitPosition();
		#if __DEV
			pHitEntity = shapeTestResults[0].GetHitEntity();
			if(pHitEntity)
			{
			}
		#endif
		}

		Vector3 vNewForward  = vTargetPoint - m_PivotPosition;
		vNewForward.NormalizeSafe();

		m_BaseFront = vNewForward;
		if(m_Metadata.m_ShouldOrbitRelativeToAttachParentOrientation)
		{
			//Convert the orbit orientation to be parent-relative.
			m_AttachParentMatrixToConsiderForRelativeOrbit.Transform3x3(m_BaseFront);
		}

#if __BANK && 0
		float targetHeading = camFrame::ComputeHeadingFromFront(vNewForward);
		const camBaseCamera* pPreviousCamera = camInterface::GetDominantRenderedCamera();
		if(pPreviousCamera && pPreviousCamera != this)
		{
			float currentHeading = pPreviousCamera->GetFrame().ComputeHeading();
		#if !__NO_OUTPUT
			cameraDebugf1("REORIENT -- changing heading for new camera %s, heading: %f  current heading: %f", GetName(), targetHeading, currentHeading);
		#else
			cameraDebugf1("REORIENT -- changing heading for new camera, heading: %f  current heading: %f", targetHeading, currentHeading);
		#endif
		}
#endif // __BANK

		return true;
	}

	return false;
}
