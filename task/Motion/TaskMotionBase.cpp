
#include "task/Motion/TaskMotionBase.h"

// rage includes
#include "creature/componentextradofs.h"
#include "crskeleton/skeleton.h"
#include "math/angmath.h"
#include "physics/debugEvents.h"
#include "system/stack.h"
#include "vector\matrix34.h"

// framework includes
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/directorcomponentsyncedscene.h"

// project includes
#include "animation/debug/AnimDebug.h"
#include "modelinfo/PedModelInfo.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Objects/Door.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds/PedIKSettings.h"
#include "Physics\Physics.h"
#include "scene/entity.h"
#include "scene/Physical.h"
#include "scene/world/GameWorld.h"
#include "task/motion/Locomotion/TaskMotionPed.h"
#include "task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/System/MotionTaskData.h"
#include "Task/Weapons/TaskProjectile.h"
#include "vehicles/vehicle.h"
#include "Weapons/Info/WeaponAnimationsManager.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/GameplayDirector.h"

#include "Task/Movement/Jumping/TaskJump.h"

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

#define USE_LINEAR_INTERPOLATION 1

float CTaskMotionBase::ms_fBaseStandSpringStrength = 6.0f;
float CTaskMotionBase::ms_fBaseStandSpringDamping = -2.0f*rage::Sqrtf(ms_fBaseStandSpringStrength * 9.81f);
const fwMvClipSetId CTaskMotionBase::ms_AimingStrafingFirstPersonClipSetId("move_ped_strafing_aiming_firstperson",0xBF69108E);
dev_float CTaskMotionBase::ms_fAccelLimitStd = 30.0f;
dev_float CTaskMotionBase::ms_fAccelLimitQpd = 60.0f;
dev_float CTaskMotionBase::ms_fAccelLimitHigher = 100.0f;
dev_float CTaskMotionBase::ms_fAccelLimitDead = 0.5f;
dev_float CTaskMotionBase::ms_fAccelLimitPushedByDoor = 0.1f;

#if __BANK
bool CTaskMotionBase::ms_bLockPlayerInput		= false;
#endif //  __BANK
#if __BANK
sysPerformanceTimer * CTaskMotionBase::m_pPerfTimer = NULL;
float CTaskMotionBase::ms_fUpdateTimeRunningTotal = 0.0f;
float CTaskMotionBase::ms_fUpdateTimeForAllPeds = 0.0f;
#endif

CTaskMotionBase::CTaskMotionBase()
: m_defaultOnFootClipSet(CLIP_SET_ID_INVALID)
, m_overrideStrafingClipSet(CLIP_SET_ID_INVALID)
, m_overrideCrouchedStrafingClipSet(CLIP_SET_ID_INVALID)
, m_defaultSwimmingClipSet(CLIP_SET_ID_INVALID)
, m_defaultDivingClipSet(CLIP_SET_ID_INVALID)
, m_overrideWeaponClipSet(CLIP_SET_ID_INVALID)
, m_overrideInVehicleContext(0)
, m_requestedCloneStateChange(-1)
, m_lastRequestedCloneState(-1)
, m_onFootBlendDuration(SLOW_BLEND_DURATION)
, m_fVelocityRotation(FLT_MAX)
, m_uLastTimeVelocityRotationActive(0)
, m_uLastTimeVelocityRotationInactive(0)
, m_alternateClipsLooping(0)
, m_GaitReductionBlockedOnCollision(false)
, m_wantsToDiveUnderwater(false)
, m_isFullyInVehicle(false)
, m_isFullyAiming(false)
, m_CleanupMotionTaskNetworkOnQuit(true)
, m_UseVelocityOverride(false)
, m_VelocityOverride(Vector3::ZeroType)
, m_overrideUsesUpperBodyShadowExpression(false)
, m_isUsingInjuredMovementOverride(false)
, m_WaitingForTargetState(false)
, m_CapsuleResults(1)
{
#if __BANK
	m_bHasBeenDeleted = false;
#endif
}

CTaskMotionBase::~CTaskMotionBase()
{
#if __BANK
	m_bHasBeenDeleted = true;
#endif
}

static Vec3V_Out sRotateAroundAxis(Vec3V_In rotVecV, Vec3V_In unitAxisV, ScalarV_In rotAmountV)
{
	// What we will be doing here is the same as this, but should be faster by doing all
	// the work in the vector pipeline with no floats or memory usage.
	//	Vector3 tmp = VEC3V_TO_VECTOR3(rotVecV);
	//	Matrix34 matNew;
	//	matNew.MakeRotateUnitAxis(VEC3V_TO_VECTOR3(unitAxisV), rotAmountV.Getf());
	//	matNew.Transform3x3(tmp);

	// Set up some constants. Unfortunately there is no V_HALF_PI.
	const ScalarV halfPiV(Vec::V4VConstantSplat<FLOAT_TO_INT(0.5f*PI)>());
	const Vec4V zeroV(V_ZERO);

	// Prepare for sine/cosine computation.
	const Vec4V anglesCosSinV = MergeXY(Vec4V(halfPiV), zeroV);
	const Vec4V anglesV = rage::Add(Vec4V(rotAmountV), anglesCosSinV);

	// Compute the sine and cosine.
	const Vec4V cosSinCosSinV = Sin(anglesV);
	const ScalarV cosV = cosSinCosSinV.GetX();
	const ScalarV sinV = cosSinCosSinV.GetY();

	// What we are doing here is basically computing the axes a, b, and c
	// of a matrix for rotating around the axis, adapted from Matrix34::MakeRotateUnitAxis():
	//	float omc = 1.0f - cos;
	//	Vector3 vscaled = omc*v;
	//	Vector3 vsin = sin*v;
	//	a.x=vscaled.x*v.x + cos;
	//	a.y=vscaled.x*v.y + vsin.z;
	//	a.z=vscaled.x*v.z - vsin.y;
	//	b.x=vscaled.x*v.y - vsin.z;
	//	b.y=vscaled.y*v.y + cos;
	//	b.z=vscaled.y*v.z + vsin.x;
	//	c.x=vscaled.x*v.z + vsin.y;
	//	c.y=vscaled.y*v.z - vsin.x;
	//	c.z=vscaled.z*v.z + cos;

	const Vec3V axisTimesOneMinusCosV = SubtractScaled(unitAxisV, unitAxisV, cosV);
	const Vec3V axisTimesSinV = Scale(unitAxisV, sinV);
	const Vec3V negAxisTimesSinV = SubtractScaled(Vec3V(V_ZERO), unitAxisV, sinV);
	
	// Note: the stuff below is probably not optimal, could probably be smarter about
	// how we build the vectors. Probably not worth the effort, though.

	const Vec3V pV(axisTimesOneMinusCosV.GetY(), axisTimesOneMinusCosV.GetY(), axisTimesOneMinusCosV.GetZ());
	const Vec3V qV(unitAxisV.GetY(), unitAxisV.GetZ(), unitAxisV.GetZ());
	const Vec3V pqV = Scale(pV, qV);

	const Vec3V a1V = Scale(axisTimesOneMinusCosV.GetX(), unitAxisV);
	const Vec3V b1V(a1V.GetY(), pqV.GetX(), pqV.GetY());
	const Vec3V c1V(a1V.GetZ(), pqV.GetY(), pqV.GetZ());

	const Vec3V a2V(cosV, axisTimesSinV.GetZ(), negAxisTimesSinV.GetY());
	const Vec3V b2V(negAxisTimesSinV.GetZ(), cosV, axisTimesSinV.GetX());
	const Vec3V c2V(axisTimesSinV.GetY(), negAxisTimesSinV.GetX(), cosV);

	const Vec3V aV = Add(a1V, a2V);
	const Vec3V bV = Add(b1V, b2V);
	const Vec3V cV = Add(c1V, c2V);

	const Vec3V sumAV = Scale(aV, rotVecV.GetX());
	const Vec3V sumABV = AddScaled(sumAV, bV, rotVecV.GetY());
	const Vec3V sumABC = AddScaled(sumABV, cV, rotVecV.GetZ());

	return sumABC;
}


Vec3V_Out CTaskMotionBase::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep)
{
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fMaxSlopeAngleCos, 0.6428f, 0.0f, 1.0f, 0.0001f); // 50 degs
	TUNE_GROUP_FLOAT(PED_MOVEMENT, fIslandHeistSlopeAngle, 0.8f, 0.0f, 1.0f, 0.0001f); 


	CPed* pPed = GetPed();

#if __ASSERT

	Vector3 vPedVel = pPed->GetVelocity();
	Assertf(vPedVel == vPedVel, "Ped's existing velocity was already invalid (NaN) before CTaskMotionBase.");

	bool bLogDebug = false;
	if ( !(MagSquared(VECTOR3_TO_VEC3V(vPedVel)).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f)) )
	{
		Displayf("[0] Ped's %s current velocity Magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(VECTOR3_TO_VEC3V(vPedVel)).Getf());
		bLogDebug = true;
	}
#endif

	Vec3V extractedVelV;
	if(m_UseVelocityOverride)
	{
		extractedVelV = RCC_VEC3V(m_VelocityOverride);
	}
	else
	{
		extractedVelV = VECTOR3_TO_VEC3V(pPed->GetAnimatedVelocity());
	}

	const Vec3V mtrxAV = updatedPedMatrix.GetCol0();
	const Vec3V mtrxBV = updatedPedMatrix.GetCol1();

	const ScalarV extractedXV = extractedVelV.GetX();
	const ScalarV extractedYV = extractedVelV.GetY();
	Vec3V velocityV = Scale(mtrxAV, extractedXV);

#if __ASSERT
	if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[1] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
	{
		bLogDebug = true;
	}
#endif

	velocityV = AddScaled(velocityV, mtrxBV, extractedYV);
#if __ASSERT
	if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[2] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
	{
		bLogDebug = true;
	}
#endif
	if(pPed->GetUseExtractedZ())
	{
		const Vec3V mtrxCV = updatedPedMatrix.GetCol2();
		const ScalarV extractedZV = extractedVelV.GetZ();
		velocityV = AddScaled(velocityV, mtrxCV, extractedZV);
#if __ASSERT
		if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[3] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
		{
			bLogDebug = true;
		}
#endif
	}

	Assertf(IsFiniteAll(velocityV), "NaNs after multiplying the animated velocity by the ped's matrix.");

	const ScalarV timeStepV(fTimestep);

	Vec3V desiredAngVelV = RCC_VEC3V(pPed->GetDesiredAngularVelocity());
	if(!IsZeroAll(desiredAngVelV))
	{
		if(pPed->GetGroundPhysical())
		{
			desiredAngVelV = Subtract(desiredAngVelV, pPed->GetGroundAngularVelocity());
		}

		const ScalarV turnAmountSqV = MagSquared(desiredAngVelV);
		const ScalarV thresholdSqV(Vec::V4VConstantSplat<FLOAT_TO_INT(1e-10f)>());	// square(0.00001f);
		if(IsGreaterThanAll(turnAmountSqV, thresholdSqV))
		{
			// TODO: could potentially have used some Normalize() function here instead.
			const ScalarV turnAmountV = Sqrt(turnAmountSqV);
			const Vec3V turnAxisV = InvScale(desiredAngVelV, turnAmountV);
			const ScalarV turnAmountThisFrameV = Scale(turnAmountV, timeStepV);
			velocityV = sRotateAroundAxis(velocityV, turnAxisV, turnAmountThisFrameV);
#if __ASSERT
			if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[4] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
			{
				bLogDebug = true;
			}
#endif
		}
	}

#if FPS_MODE_SUPPORTED
	TUNE_GROUP_BOOL(PED_MOVEMENT, DO_FPS_VELOCITY_ROTATION, true);
	bool bFPSModeEnabled = pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true, true);
	if(pPed->IsPlayer() && DO_FPS_VELOCITY_ROTATION && !pPed->GetMotionData()->GetCombatRoll())
	{
		if(m_fVelocityRotation == FLT_MAX)
			m_fVelocityRotation = pPed->GetCurrentHeading();

		Vector3 v(VEC3_ZERO);

		if(bFPSModeEnabled &&
			!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride) && 
			(GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION || GetTaskType() == CTaskTypes::TASK_MOTION_AIMING) && (pPed->IsNetworkClone() || pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding)) &&
			!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableIdleExtraHeadingChange))
		{
			float fDesiredHeading = pPed->GetDesiredHeading();
			if(pPed->GetMotionData()->GetIsStrafing())
			{
				Vector3 vDesiredMBR(pPed->GetMotionData()->GetDesiredMoveBlendRatio().x, pPed->GetMotionData()->GetDesiredMoveBlendRatio().y, 0.f);
				vDesiredMBR.Normalize();
				fDesiredHeading = rage::Atan2f(-vDesiredMBR.x, vDesiredMBR.y);
				fDesiredHeading = fDesiredHeading + pPed->GetCurrentHeading();
				fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
			}

			bool bPassDesiredMoveRatioCheck = false;
			if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding) || pPed->GetIsFPSSwimming())
			{
				CControl *pControl = pPed->GetControlFromPlayer();
				if(pControl)
				{
					Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);

					float fMag = vStickInput.Mag();
					if(fMag > 0.01f)
					{
						float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPed);
						float fStickAngle = rage::Atan2f(-vStickInput.x, vStickInput.y);	
						fStickAngle = fStickAngle + fCamOrient;
						fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
						fDesiredHeading = fStickAngle;
						if (!pPed->GetIsFPSSwimming())
						{
							bPassDesiredMoveRatioCheck = true;
						}
					}
				}
			}

			if(pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2() > 0.0001f || bPassDesiredMoveRatioCheck)
			{
				TUNE_GROUP_INT(PED_MOVEMENT, FPS_VelocityRotation_BlendInTime, 1000, 0, 10000, 1);

				// Blend in velocity rotation
				u32 uTimeActive = fwTimer::GetTimeInMilliseconds() - m_uLastTimeVelocityRotationInactive;
				if(uTimeActive < FPS_VelocityRotation_BlendInTime)
				{
					const float fLerpRatio = (float)uTimeActive / (float)FPS_VelocityRotation_BlendInTime;
					m_fVelocityRotation = fwAngle::LerpTowards(m_fVelocityRotation, fDesiredHeading, fLerpRatio);
				}
				else
				{
					TUNE_GROUP_FLOAT(PED_MOVEMENT, FPS_VelocityRotation_LerpRate, 200.f, 0.f, 10000.f, 0.0001f);
					m_fVelocityRotation = fwAngle::Lerp(m_fVelocityRotation, fDesiredHeading, FPS_VelocityRotation_LerpRate * fTimestep);
				}
			}

			// FPS Swimming velocity slow down code:
			bool bIsFPSSwimStrafingUnderwater = false;
			bool bUsingLerpedSwimSlowDownVelocity = false;
			if (pPed->GetIsSwimming())
			{
				CTaskMotionBase* pPrimaryTask = pPed->GetPrimaryMotionTask();
				if( pPrimaryTask && pPrimaryTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED )
				{
					Vector3 vSwimVel(VEC3_ZERO); 
					CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(pPrimaryTask);
					bIsFPSSwimStrafingUnderwater = pTask && pTask->CheckForDiving();

					// If we're in the aiming motion task (strafing) and have no input but still have a velocity, gradually ramp it down to 0 (so we glide)
					// Fixes B*1991079 - large Z velocity pop when transitioning from diving to strafe idle
					if (pTask && pPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseAimingMotionTask))
					{
						CControl *pControl = pPed->GetControlFromPlayer();
						if (pControl)
						{
							TUNE_GROUP_FLOAT(FPS_SWIMMING, fInputThresholdToLerpDownVelocity, 0.1f, 0.0f, 1.0f, 0.01f);
							TUNE_GROUP_FLOAT(FPS_SWIMMING, fVelocityThresholdToLerpDown, 0.01f, 0.0f, 1.0f, 0.01f);

							Vector2 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
							Vector3 vCachedSwimVelocity = pTask->GetCachedSwimVelocity();
							Vector3 vCachedSwimStrafeVelocity = pTask->GetCachedSwimStrafeVelocity();

							if (vStickInput.Mag() < fInputThresholdToLerpDownVelocity && (vCachedSwimVelocity.Mag() > fVelocityThresholdToLerpDown || vCachedSwimStrafeVelocity.Mag() > fVelocityThresholdToLerpDown))
							{									
								// Dampen velocity coming from TaskInWater
								if (vCachedSwimVelocity.Mag() > fVelocityThresholdToLerpDown)
								{
									TUNE_GROUP_FLOAT(FPS_SWIMMING, fSwimSlowDownRate, 0.075f, 0.0f, 1.0f, 0.01f);
									vCachedSwimVelocity.x = Lerp(fSwimSlowDownRate, vCachedSwimVelocity.x, 0.0f);
									vCachedSwimVelocity.y = Lerp(fSwimSlowDownRate, vCachedSwimVelocity.y, 0.0f);
									if (bIsFPSSwimStrafingUnderwater)
										vCachedSwimVelocity.z = Lerp(fSwimSlowDownRate, vCachedSwimVelocity.z, 0.0f);
									else
										vCachedSwimVelocity.z = 0.0f;
									pTask->SetCachedSwimVelocity(vCachedSwimVelocity);
									pTask->SetCachedSwimStrafeVelocity(VEC3_ZERO);
									vSwimVel = vCachedSwimVelocity;

									// Don't do the velocity scaling and rotation based on heading when using a cached velocity from the swim/diving task
									v = vSwimVel;
									bUsingLerpedSwimSlowDownVelocity = true;
								}
								// Dampen velocity from TaskMotionAiming (so we don't instantly stop)
								else if (vCachedSwimStrafeVelocity.Mag() > fVelocityThresholdToLerpDown)
								{
									TUNE_GROUP_FLOAT(FPS_SWIMMING, fSwimStrafeSlowDownRate, 0.075f, 0.0f, 1.0f, 0.01f);										
									vCachedSwimStrafeVelocity.x = Lerp(fSwimStrafeSlowDownRate, vCachedSwimStrafeVelocity.x, 0.0f);
									vCachedSwimStrafeVelocity.y = Lerp(fSwimStrafeSlowDownRate, vCachedSwimStrafeVelocity.y, 0.0f);
									if (bIsFPSSwimStrafingUnderwater)
										vCachedSwimStrafeVelocity.z = Lerp(fSwimStrafeSlowDownRate, vCachedSwimStrafeVelocity.z, 0.0f);
									else
										vCachedSwimStrafeVelocity.z = 0.0f;
									pTask->SetCachedSwimStrafeVelocity(vCachedSwimStrafeVelocity);
									pTask->SetCachedSwimVelocity(VEC3_ZERO);
									vSwimVel = vCachedSwimStrafeVelocity;

									velocityV = VECTOR3_TO_VEC3V(vSwimVel);	
							#if __ASSERT
									if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[5] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
									{
										bLogDebug = true;
									}
							#endif
								}
							}
							else
							{
								pTask->SetCachedSwimVelocity(VEC3_ZERO);
								pTask->SetCachedSwimStrafeVelocity(VEC3_ZERO);
							}
						}
					}
				}
			}

			if(!bUsingLerpedSwimSlowDownVelocity)
			{
				v.SetY(Mag(velocityV).Getf());
				Matrix34 m;
				m.MakeRotateZ(m_fVelocityRotation);
				if (bIsFPSSwimStrafingUnderwater)
				{
					// Rotate local X based on camera pitch if we're swim strafing (to give us Z velocity in our movement)
					float fCamPitch = camInterface::GetPlayerControlCamPitch(*pPed);
					m.RotateLocalX(-fCamPitch);
				}
				m.Transform3x3(v);
			}

#if __BANK
			TUNE_GROUP_BOOL(FPS_SWIMMING, bEnableVelocityRender, false);
			if(bEnableVelocityRender)
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + v, 0.25f, Color_blue1, false);
			}
#endif	//__BANK

			velocityV = VECTOR3_TO_VEC3V(v);

#if __ASSERT
			if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[7] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
			{
				bLogDebug = true;
			}
#endif

			m_uLastTimeVelocityRotationActive = fwTimer::GetTimeInMilliseconds();
		}
		else
		{
			const float fCurrentHeading = pPed->GetCurrentHeading();

			if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping))
			{
				TUNE_GROUP_INT(PED_MOVEMENT, FPS_VelocityRotation_BlendOutTime, 100, 0, 10000, 1);

				// Blend out velocity rotation
				u32 uTimeActive = fwTimer::GetTimeInMilliseconds() - m_uLastTimeVelocityRotationActive;
				if(uTimeActive < FPS_VelocityRotation_BlendOutTime)
				{
					const float fLerpRatio = (float)uTimeActive / (float)FPS_VelocityRotation_BlendOutTime;
					m_fVelocityRotation = fwAngle::LerpTowards(m_fVelocityRotation, fCurrentHeading, fLerpRatio);

					v.SetY(Mag(velocityV).Getf());
					Matrix34 m;
					m.MakeRotateZ(m_fVelocityRotation);
					m.Transform3x3(v);
					velocityV = VECTOR3_TO_VEC3V(v);
#if __ASSERT
					if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[8] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
					{
						bLogDebug = true;
					}
#endif
				}
				else
				{
					m_fVelocityRotation = fCurrentHeading;
				}
			}
			else
			{
				m_fVelocityRotation = fCurrentHeading;
			}

			m_uLastTimeVelocityRotationInactive = fwTimer::GetTimeInMilliseconds();
		}
	}
#endif // FPS_MODE_SUPPORTED

	//Account for the ground.
	velocityV = Add(velocityV, pPed->GetGroundVelocityIntegrated());

#if __ASSERT
	if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[9] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
	{
		bLogDebug = true;
	}
#endif

	// account for the velocity of our synced scene (if we're in one)
	if (pPed->GetAnimDirector())
	{
		fwAnimDirectorComponentSyncedScene* pComponent = static_cast<fwAnimDirectorComponentSyncedScene*>(pPed->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeSyncedScene));
		if (pComponent && pComponent->IsPlayingSyncedScene())
		{
			Vec3V sceneVel;
			pComponent->GetSceneVelocity(sceneVel);

			velocityV = Add(velocityV, sceneVel);
#if __ASSERT
			if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[10] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
			{
				bLogDebug = true;
			}
#endif
		}
	}


	//***************************************
	// Stop peds walking up very steep slopes

	CPhysical* pPhysical = pPed->GetGroundPhysical();
	if(!pPhysical && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))	// Check reset flag to avoid the GetClimbPhysical() call in the common case.
	{
		pPhysical = pPed->GetClimbPhysical();
	}
 
	//pPed->SetPedResetFlag(CPED_RESET_FLAG_PedStoppedOnSteepSlope, false);
	if(	((pPed->IsPlayer() && (pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame() || pPed->GetMotionData()->GetForceSteepSlopeTestThisFrame()))
			|| pPed->GetPedResetFlag(CPED_RESET_FLAG_EnableSteepSlopePrevention)) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsClimbingLadder) )
	{
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

#define MAX_SNOW_DEPTH	1.0f
		static bank_float sfMaxSlopeAngleCosInSnow				= 0.85f;   // 31.7 degs
		static bank_float sfMaxSlopeDeepSnowRatio				= 0.5f;
		const float fSecondSurfaceDepthRatio = pPed->GetSecondSurfaceDepth() / MAX_SNOW_DEPTH;
		const bool bSurfaceTooSteep = pPed->GetMaxGroundNormal().z < sfMaxSlopeAngleCos && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs);
		const bool bSecondSurfaceTooSteep = pPed->GetMaxGroundNormal().z < sfMaxSlopeAngleCosInSnow && (fSecondSurfaceDepthRatio > sfMaxSlopeDeepSnowRatio);
		if(bSurfaceTooSteep || bSecondSurfaceTooSteep)
#else

		const float fMaxSlopeAngleTemp = (ThePaths.bStreamHeistIslandNodes && pPed->GetPedResetFlag(CPED_RESET_FLAG_TooSteepForPlayer)) ? fIslandHeistSlopeAngle : fMaxSlopeAngleCos;
		const bool bSurfaceTooSteep = pPed->GetMaxGroundNormal().GetZf() < fMaxSlopeAngleTemp && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs);
		if(bSurfaceTooSteep)
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
		{
			if(pPhysical && pPhysical->GetIsTypeVehicle() && 
				(((CVehicle*)pPhysical)->GetVehicleType()==VEHICLE_TYPE_BOAT || ((CVehicle*)pPhysical)->GetVehicleType()==VEHICLE_TYPE_TRAIN || ((CVehicle*)pPhysical)->GetVehicleType()==VEHICLE_TYPE_PLANE))
			{
				pPed->SetSteepSlopePos(VEC3_ZERO);
			}
			else if(pPed->GetSteepSlopePos().IsZero())
			{
				pPed->SetSteepSlopePos(pPed->GetGroundPos());
			}
			else
			{
				Vector3 vecSlopeDirn = pPed->GetGroundNormal();
				vecSlopeDirn.z = 0.0f;
				vecSlopeDirn.Normalize();
				Vector3 vecMoveDirn = pPed->GetGroundPos() - pPed->GetSteepSlopePos();
				if(vecMoveDirn.Dot(vecSlopeDirn) < -0.2f)
				{
					const ScalarV negDesiredVelIntoSlopeV = Dot(VECTOR3_TO_VEC3V(vecSlopeDirn), velocityV);
					const ScalarV zeroV(V_ZERO);
					if(IsLessThanAll(negDesiredVelIntoSlopeV, zeroV))
					{
						//pPed->SetPedResetFlag(CPED_RESET_FLAG_PedStoppedOnSteepSlope, true);
						velocityV = SubtractScaled(velocityV, VECTOR3_TO_VEC3V(vecSlopeDirn), negDesiredVelIntoSlopeV);
#if __ASSERT
						if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[11] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
						{
							bLogDebug = true;
						}
#endif
						PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*pPed->GetCurrentPhysicsInst(), negDesiredVelIntoSlopeV.Getf(), "SteepSlope_VelocityNegation"));
						PDR_ONLY(debugPlayback::RecordTaggedVectorValue(*pPed->GetCurrentPhysicsInst(), velocityV, "SteepSlope_FinalVelocity", debugPlayback::eVectorTypeVelocity));
					}
				}
			}
		}
		else
		{
			// If we're jumping CPED_CONFIG_FLAG_OnStairSlope will get set before we're actually on the ground and getting
			//   a valid max ground normal. If this happens we end up subtracting out the entire vertical portion of the velocity. 
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairSlope) && pPed->IsOnGround())
			{
				//Adjust velocity along slope
				Vec3V vNormal = pPed->GetMaxGroundNormal();
				ScalarV vDotWithNormal = Dot(velocityV, vNormal);
				Vec3V vUp(0.0f,0.0f,1.0f);
				velocityV -= vUp * vDotWithNormal;
#if __ASSERT
				if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[12] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
				{
					bLogDebug = true;
				}
#endif
			}

			pPed->SetSteepSlopePos(VEC3_ZERO);
		}
	}

	PDR_ONLY(debugPlayback::RecordPedAnimatedVelocity(pPed->GetCurrentPhysicsInst(), velocityV));

	//if gait reducing at a full stop, apply a small velocity to try and push things.	
	if (IsGaitReduced() && GetMotionData().GetGaitReducedMaxMoveBlendRatio()==MOVEBLENDRATIO_STILL && !GetMotionData().GetGaitReductionFlag(CPedMotionData::GR_HitIncline))
	{
		const ScalarV sGR_EXTRAVELO_V(V_FLT_SMALL_1);	// 0.1f
		const Vec3V fwdV = pPed->GetMatrixRef().GetCol1();
		velocityV = AddScaled(velocityV, fwdV, sGR_EXTRAVELO_V);
#if __ASSERT
		if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001), "[13] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
		{
			bLogDebug = true;
		}
#endif
	}

	const Vec3V currentPedVelocityV = VECTOR3_TO_VEC3V(pPed->GetVelocity());
#if __ASSERT
	if (!Verifyf(MagSquared(currentPedVelocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[14] Ped: %s currentPedVelocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(currentPedVelocityV).Getf()) )
	{
		bLogDebug = true;
	}
#endif

	// Now see if we are allowed this change in velocity
	Vec3V velChangeV = Subtract(velocityV, currentPedVelocityV);
#if __ASSERT
	if (!Verifyf(MagSquared(velChangeV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[15] Ped: %s velChangeV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velChangeV).Getf()) )
	{
		bLogDebug = true;
	}
#endif
    bool standingInsideVehicle = false;

    if(pPhysical)
    {
        if(pPhysical->GetIsTypeVehicle() && 
            ((CVehicle*)pPhysical)->CanPedsStandOnTop())
        {
            standingInsideVehicle = true;
        }
    }

    // don't clamp the velocity change for network clones, the network blending code prevents
    // large velocity changes in a frame but needs to know the target velocity for the ped
	TUNE_GROUP_BOOL(PED_MOVEMENT, FORCE_FPS_VEL_CLAMP, false);
	static dev_bool sbUseVelocityClamp = true;
	if (sbUseVelocityClamp && (!standingInsideVehicle || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping)) && (!pPed->IsNetworkClone() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping))
#if FPS_MODE_SUPPORTED
		&& ( !bFPSModeEnabled || (bFPSModeEnabled && (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || GetTaskType() != CTaskTypes::TASK_MOTION_AIMING || FORCE_FPS_VEL_CLAMP)))
#endif // FPS_MODE_SUPPORTED
		)
	{
		velChangeV = CalcVelChangeLimitAndClamp(*pPed, velChangeV, timeStepV, pPhysical);
#if __ASSERT
		if (!Verifyf(MagSquared(velChangeV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[16] Ped: %s velChangeV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velChangeV).Getf()) )
		{
			bLogDebug = true;
		}
#endif
	}
	else if(!pPed->GetUseExtractedZ())
	{
		velChangeV.SetZ(ScalarV(V_ZERO));
	}

#if __ASSERT
	if (!Verifyf(MagSquared(currentPedVelocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[17] Ped's current velocity Magnitude2 was invalid (%f).", 
		MagSquared(currentPedVelocityV).Getf()))
	{
		bLogDebug = true;
	}

	if (!Verifyf(pPed->IsNetworkClone() || 
		MagSquared(velChangeV).Getf() < (DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT + 0.0001f) || 
		DistSquared(VECTOR3_TO_VEC3V(pPed->GetAnimatedVelocity()), VECTOR3_TO_VEC3V(pPed->GetPreviousAnimatedVelocity())).Getf() >  DEFAULT_IMPULSE_LIMIT * DEFAULT_IMPULSE_LIMIT, "Ped's velocity change Magnitude2 was invalid (%f).  Standing inside vehicle?  %d, IndependentMoverTransition is 0? %d", 
		MagSquared(velChangeV).Getf(), standingInsideVehicle, pPed->GetMotionData()->GetIndependentMoverTransition() == 0))
	{
		bLogDebug = true;
	}

	if (!Verifyf(MagSquared(Add(currentPedVelocityV, velChangeV)).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[18]Ped's desired velocity is invalid (%f)", 
		Mag(Add(currentPedVelocityV, velChangeV)).Getf()))
	{
		bLogDebug = true;
	}

	if(bLogDebug)
	{
		Printf("currentPedVelocityV (%.2f,%.2f,%.2f):%.2f, velChangeV (%.2f,%.2f,%.2f):%.2f, desiredVelocity (%.2f,%.2f,%.2f):%.2f\n", currentPedVelocityV.GetXf(), currentPedVelocityV.GetYf(), currentPedVelocityV.GetZf(), Mag(currentPedVelocityV).Getf(), velChangeV.GetXf(), velChangeV.GetYf(), velChangeV.GetZf(), Mag(velChangeV).Getf(), pPed->GetDesiredVelocity().x, pPed->GetDesiredVelocity().y, pPed->GetDesiredVelocity().z, pPed->GetDesiredVelocity().Mag());
		Printf("Tasks:\n");
		pPed->GetPedIntelligence()->PrintTasks();
		Printf("Script task history:\n");
		pPed->GetPedIntelligence()->PrintScriptTaskHistory();
		Printf("Anims:\n");
		CAnimDebug::LogAnimations(*pPed);
	}
#endif

	velocityV = Add(currentPedVelocityV, velChangeV);

#if __ASSERT
	if (!Verifyf(MagSquared(velocityV).Getf() < (DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT + 0.0001f), "[19] Ped: %s velocityV magnitude2 was invalid (%f).", pPed->GetModelName(), MagSquared(velocityV).Getf()) )
	{
		bLogDebug = true;
	}
#endif

    NetworkInterface::OnDesiredVelocityCalculated(*pPed);
    
	return velocityV;
}

Vec3V_Out CTaskMotionBase::CalcVelChangeLimitAndClamp(const CPed& ped, Vec3V_In changeInV, ScalarV_In timestepV, const CPhysical* pGroundPhysical)
{
	Vec3V changeV = changeInV;
	if(!ped.GetUseExtractedZ())
	{
		changeV.SetZ(ScalarV(V_ZERO));
	}

	// If we're using low lod physics then don't want a vel change limit
	if(ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
	{
		return changeV;
	}

	static bank_float sfVelChangeLimitFallen				= 10.0f;
	static bank_float sfVelChangeLimitFallenOnVehicle		= 1.0f;

	const float* pChangeLimitPtr;
	// if a non-ragdoll'ing dead or dying ped is on top of a vehicle, make sure they slide off easily
	if(ped.IsInjured() && pGroundPhysical && pGroundPhysical->GetIsTypeVehicle() && 
		!static_cast<const CVehicle*>(pGroundPhysical)->CanPedsStandOnTop())
	{
		pChangeLimitPtr = &ms_fAccelLimitDead;
	}
	// if this ped is being pushed by a door that the player is pushing from the other side, make them slide out of the way easily
	else if(ped.m_PedResetFlags.GetKnockedByDoor() > 0)
	{
		pChangeLimitPtr = &ms_fAccelLimitPushedByDoor;
	}
	else if(ped.GetPedResetFlag(CPED_RESET_FLAG_FallenDown))
	{
		bool bSlideOffVehicle = false;
		if(pGroundPhysical && pGroundPhysical->GetIsTypeVehicle())
		{
			const CVehicle* pVeh = static_cast<const CVehicle*>(pGroundPhysical);
			if(!pVeh->CanPedsStandOnTop())
				bSlideOffVehicle = true;
		}
		if(bSlideOffVehicle)
			pChangeLimitPtr = &sfVelChangeLimitFallenOnVehicle;
		else
			pChangeLimitPtr = &sfVelChangeLimitFallen;
	}
	else
	{
		// limit the force the ped applies in the change direction
		// this causes inertia and restricts grip when standing on moving ground
		// Set by the AI to allow a higher limit (reset in CPed::ProcessPhysics)
		if( ped.GetPedResetFlag(CPED_RESET_FLAG_RaiseVelocityChangeLimit) )
		{
			pChangeLimitPtr = &ms_fAccelLimitHigher;
		}
		else
		{
			pChangeLimitPtr = ped.GetCapsuleInfo()->IsQuadruped() ? &ms_fAccelLimitQpd : &ms_fAccelLimitStd;
		}
	}

	const ScalarV changeLimitBeforeAnimV = LoadScalar32IntoScalarV(*pChangeLimitPtr);

	// If clip has requested a massive change then allow it
	const ScalarV timestepSqV = Scale(timestepV, timestepV);
	const ScalarV animatedChangeMagSqV = DistSquared(VECTOR3_TO_VEC3V(ped.GetAnimatedVelocity()), VECTOR3_TO_VEC3V(ped.GetPreviousAnimatedVelocity()));
	const ScalarV changeLimitBeforeAnimSqV = Scale(changeLimitBeforeAnimV, changeLimitBeforeAnimV);
	const ScalarV changeLimitTimesTimeStepBeforeAnimSqV = Scale(timestepSqV, changeLimitBeforeAnimSqV);
	const ScalarV timestepTimesLimitSqV = Max(animatedChangeMagSqV, changeLimitTimesTimeStepBeforeAnimSqV);

	const ScalarV changeMagSqV = MagSquared(changeV);
	if(IsGreaterThanAll(changeMagSqV, timestepTimesLimitSqV))
	{
		// Note: could easily have removed the branch here, but the stuff we do here is much
		// more expensive than the stuff above, and it should be a quite rare case - generally
		// only when the player does something like jumping, so branching may be better.
		const ScalarV scaleV = Sqrt(InvScale(timestepTimesLimitSqV, changeMagSqV));
		changeV = Scale(changeV, scaleV);
	
		//! If doing super jump, choose different XY scale.
		if( ped.GetPedResetFlag(CPED_RESET_FLAG_IsJumping) && !ped.GetPedResetFlag(CPED_RESET_FLAG_IsLanding))
		{
			const CTaskJump* jumpTask = static_cast<const CTaskJump*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP));
			if (jumpTask && jumpTask->GetIsDoingSuperJump())
			{
				static bank_float sfVelChangeLimitXY				= 1.0f;
				pChangeLimitPtr = &sfVelChangeLimitXY;

				const ScalarV changeLimitBeforeAnimV = LoadScalar32IntoScalarV(*pChangeLimitPtr);

				// If clip has requested a massive change then allow it
				const ScalarV changeLimitBeforeAnimSqV = Scale(changeLimitBeforeAnimV, changeLimitBeforeAnimV);
				const ScalarV changeLimitTimesTimeStepBeforeAnimSqV = Scale(timestepSqV, changeLimitBeforeAnimSqV);
				const ScalarV timestepTimesLimitSqV = Max(animatedChangeMagSqV, changeLimitTimesTimeStepBeforeAnimSqV);

				const ScalarV scaleV = Sqrt(InvScale(timestepTimesLimitSqV, changeMagSqV));
				
				Vec3V changeVXY = changeInV;
				changeVXY = Scale(changeVXY, scaleV);
				changeV.SetXf(changeVXY.GetXf());
				changeV.SetYf(changeVXY.GetYf());
			}
		}
	}

	return changeV;
}

float CTaskMotionBase::GetActualMoveSpeed(const float * fMoveSpeeds, float fMoveBlendRatio)
{
	Assert(fMoveBlendRatio >= MOVEBLENDRATIO_STILL && fMoveBlendRatio <= MOVEBLENDRATIO_SPRINT);
	fMoveBlendRatio = Clamp(fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);

	float i;
	const float fRemainderY = rage::Modf(fMoveBlendRatio, &i);

	const int index = (int)fMoveBlendRatio;

	// DLH TODO 
	// Explicitly changed this to branch here as Selectf has no modern
	// intrinsic implementation in the codebase so provided no speed up
	// and resulted in evaluating fMoveSpeeds[index+1] which is an illegal 
	// address read when fMoveBlendRatio==3.0f (max speed)
	if ((fRemainderY - 0.01f) > 0)
	{
		const float spd1 = fMoveSpeeds[index];
		const float spd2 = fMoveSpeeds[index+1];
		const float fVal1 = (1.0f - fRemainderY) * spd1;
		const float fVal2 = fRemainderY * spd2;
		return fVal1 + fVal2;
	}
	else
	{
		return fMoveSpeeds[index];
	}
}

void CTaskMotionBase::RetrieveMoveSpeeds(fwClipSet& clipSet, CMoveBlendRatioSpeeds& speeds, const fwMvClipId* moveSpeedClipNames)
{
	// Query the appropriate clips in the given set and cache the results for later use.
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY); 

	float* cachedSpeed;
	float speed;

	cachedSpeed = clipSet.GetProperties().Access(moveSpeedClipNames[0]);
	speed = 0.0f;

	if (cachedSpeed)
	{
		speeds.SetWalkSpeed(*cachedSpeed);
	}
	else
	{
		if (clipSet.IsStreamedIn_DEPRECATED())
		{
			const crClip* pClip = clipSet.GetClip(moveSpeedClipNames[0]);
			if (pClip)
			{
				speed = fwAnimHelpers::GetMoverTrackVelocity(*pClip).Mag();
			}
			clipSet.GetProperties().Insert(moveSpeedClipNames[0], speed);
			speeds.SetWalkSpeed(speed);
		}
	}

	cachedSpeed = clipSet.GetProperties().Access(moveSpeedClipNames[1]);
	speed = 0.0f;

	if (cachedSpeed)
	{
		speeds.SetRunSpeed(*cachedSpeed);
	}
	else
	{
		if (clipSet.IsStreamedIn_DEPRECATED())
		{
			const crClip* pClip = clipSet.GetClip(moveSpeedClipNames[1]);
			if (pClip)
			{
				speed = fwAnimHelpers::GetMoverTrackVelocity(*pClip).Mag();
			}
			clipSet.GetProperties().Insert(moveSpeedClipNames[1], speed);
			speeds.SetRunSpeed(speed);
		}
	}

	cachedSpeed = clipSet.GetProperties().Access(moveSpeedClipNames[2]);
	speed = 0.0f;

	if (cachedSpeed)
	{
		speeds.SetSprintSpeed(*cachedSpeed);
	}
	else
	{
		if (clipSet.IsStreamedIn_DEPRECATED())
		{
			const crClip* pClip = clipSet.GetClip(moveSpeedClipNames[2]);
			if (pClip)
			{
				speed = fwAnimHelpers::GetMoverTrackVelocity(*pClip).Mag();
			}
			clipSet.GetProperties().Insert(moveSpeedClipNames[2], speed);
			speeds.SetSprintSpeed(speed);
		}
	}
}

float CTaskMotionBase::GetCameraHeading()
{
	return GetPed()->GetTransform().GetHeading();
}

//	GetMoveBlendRatioForDesiredSpeed()
//	pMoveSpeeds : 4 floats describing the speed for idle,walk,run,sprint for the ped
//	returns the move-blend-ratio to use to get the desired speed
float CTaskMotionBase::GetMoveBlendRatioForDesiredSpeed(const float * fMoveSpeeds, float fDesiredSpeed)
{
	float maxSpeed = fMoveSpeeds[0];
	if(fDesiredSpeed < maxSpeed)
	{
		return 0.0f;
	}

	float baseSpeed = 0.0f;
	for(u32 i=0; i<3; i++)
	{
		const float minSpeed = maxSpeed;
		maxSpeed = fMoveSpeeds[i+1];
		if(fDesiredSpeed <= maxSpeed)
		{
			float speed = Range(fDesiredSpeed, minSpeed, maxSpeed);
			Assert(speed >= 0.0f && speed <= 1.0f);
			float ratio = baseSpeed + speed;
			return ratio;
		}
		baseSpeed += 1.0f;	// This is just (float)i, to avoid having to do an int->float conversion at the end.
	}

	return 3.0f;
}

float CTaskMotionBase::CalcDesiredTurnVelocityFromHeadingDelta(const float fHeadingChangeRate, const float fHeadingApproachTime, float fHeadingDelta)
{
	// Clamp desired turn velocity to a maximum of +/- 180 degrees
	fHeadingDelta = fwAngle::LimitRadianAngleFast(fHeadingDelta);

	//**********************************************************************************
	// Want some control over how quickly we approach target heading, so divide by the
	// time we want to take to reach desired heading to get speed

	float fDesiredTurnVelocity = fHeadingDelta / fHeadingApproachTime;

	if(fDesiredTurnVelocity > fHeadingChangeRate)
		fDesiredTurnVelocity = fHeadingChangeRate;
	else if(fDesiredTurnVelocity < -fHeadingChangeRate)
		fDesiredTurnVelocity = -fHeadingChangeRate;

	return fDesiredTurnVelocity;
}


void
CTaskMotionBase::ApproachCurrentTurnVelocityTowardsDesired(const float fDesiredTurnVelocity, const float fTurnRateAccel, const float fTurnRateDecel, const bool bLessenHdgChangeForSmallAngles, const bool bLessenHdgChangeBySquaring)
{
	float fScale;
	if(bLessenHdgChangeForSmallAngles)
	{
		// Expecting fDesiredTurnVelocity to be in range -PI to PI.
		// This will produce a value from 1.0 at heading delta of PI, to 0.0 at heading delta of zero
		fScale = (Abs(fDesiredTurnVelocity) / TWO_PI) + 0.5f;
		fScale = Clamp(fScale, 0.0f, 1.0f);

		if(bLessenHdgChangeBySquaring)
		{
			fScale *= fScale;    // Try squaring it ?  I really want to stop player controls feeling so jittery
			fScale = Clamp(fScale, 0.0f, 1.0f);
		}
	}
	else
	{
		fScale = 1.0f;
	}

	//************************************************************************
	//	Approach the current turn velocity towards the desired turn velocity
	//
	float fCurrentTurnVelocity = GetMotionData().GetCurrentTurnVelocity();

	// trying to accelerate heading rate in +ve direction
	if(fDesiredTurnVelocity > fCurrentTurnVelocity && fCurrentTurnVelocity > -0.01f)
	{
		fCurrentTurnVelocity += (fTurnRateAccel * fScale) * fwTimer::GetTimeStep();
		// make sure we don't overshoot
		if(fCurrentTurnVelocity > fDesiredTurnVelocity)
			fCurrentTurnVelocity = fDesiredTurnVelocity;
	}
	// trying to accelerate heading rate in -ve direction
	else if(fDesiredTurnVelocity < fCurrentTurnVelocity && fCurrentTurnVelocity < 0.01f)
	{
		fCurrentTurnVelocity -= (fTurnRateAccel * fScale) * fwTimer::GetTimeStep();
		if(fCurrentTurnVelocity < fDesiredTurnVelocity)
			fCurrentTurnVelocity = fDesiredTurnVelocity;
	}
	// trying to slow heading rate
	else if(fDesiredTurnVelocity > fCurrentTurnVelocity)
	{
		fCurrentTurnVelocity += (fTurnRateDecel * fScale) * fwTimer::GetTimeStep();
		if(fCurrentTurnVelocity > fDesiredTurnVelocity)
			fCurrentTurnVelocity = fDesiredTurnVelocity;
	}
	// also trying to slow it
	else
	{
		fCurrentTurnVelocity -= (fTurnRateDecel * fScale) * fwTimer::GetTimeStep();
		if(fCurrentTurnVelocity < fDesiredTurnVelocity)
			fCurrentTurnVelocity = fDesiredTurnVelocity;
	}

	// Apply
	GetMotionData().SetCurrentTurnVelocity(fCurrentTurnVelocity);
}

float CTaskMotionBase::CalcDesiredDirection() const
{
	const CPed * pPed = GetPed();
	const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
	const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());
	const float	fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
	return fHeadingDelta;
}

void CTaskMotionBase::ApproachCurrentMBRTowardsDesired(const float fMoveAccel, const float fMoveDecel, const float fMaxMBR)
{
	//***************************************
	//	Normal (non-strafing) movement
	//

	CPedMotionData& data = GetMotionData();

	const float fMinMBR = data.GetIsStrafing() ? -fMaxMBR : 0.0f;

	const float fDesiredMBR_Y = data.GetDesiredMbrY();
	float fCurrentMBR_Y = data.GetCurrentMbrY();
	if(fDesiredMBR_Y > fCurrentMBR_Y)
	{
		fCurrentMBR_Y += fMoveAccel * fwTimer::GetTimeStep();
		fCurrentMBR_Y = MIN(fCurrentMBR_Y, MIN(fDesiredMBR_Y, fMaxMBR));
	}
	else if(fDesiredMBR_Y < fCurrentMBR_Y)
	{
		fCurrentMBR_Y -= fMoveDecel * fwTimer::GetTimeStep();
		fCurrentMBR_Y = MAX(fCurrentMBR_Y, MAX(fDesiredMBR_Y, fMinMBR));
	}
	else
	{
		fCurrentMBR_Y = fDesiredMBR_Y;
	}

	//*************************************************
	//	If strafing, then approach the X axis as well
	//

	const float fDesiredMBR_X = data.GetDesiredMbrX();
	float fCurrentMBR_X = data.GetCurrentMbrX();
	if(data.GetIsStrafing())
	{
		if(fDesiredMBR_X > fCurrentMBR_X)
		{
			fCurrentMBR_X += fMoveAccel * fwTimer::GetTimeStep();
			fCurrentMBR_X = MIN(fCurrentMBR_X, MIN(fDesiredMBR_X, fMaxMBR));
		}
		else if(fDesiredMBR_X < fCurrentMBR_X)
		{
			fCurrentMBR_X -= fMoveDecel * fwTimer::GetTimeStep();
			fCurrentMBR_X = MAX(fCurrentMBR_X, MAX(fDesiredMBR_X, fMinMBR));
		}
		else
		{
			fCurrentMBR_X = fDesiredMBR_X;
		}
	}
	else
	{
		fCurrentMBR_X = 0.0f;
	}

	// Set the new move blend ratio
	data.SetCurrentMoveBlendRatio(fCurrentMBR_Y, fCurrentMBR_X);
}

void CTaskMotionBase::ResetGaitReduction()
{
	GetMotionData().SetGaitReduction(0);
	GetMotionData().SetGaitReductionHeading(0);	
	GetMotionData().ResetGaitReductionFlags();
}

void CTaskMotionBase::UpdateGaitReduction() 
{
	m_UseVelocityOverride=false; //clear this here?

	CPed* pPed = GetPed();	
	CPedMotionData* pMotionData = &GetMotionData();
	CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();

	if(pPed->IsNetworkClone() || pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableGaitReduction) || pMotionTask==NULL ||
		pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected)) //disabling on stairs for now, speeds don't match animation going downstairs
	{
		pMotionData->SetGaitReduction(0);
		pMotionData->SetGaitReducedMaxMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
		return; //Low LOD AI don't update their velocity so this is no good.
	}	

	float fMaxMBR = MOVEBLENDRATIO_SPRINT;
	float speed = pPed->GetVelocity().Mag();

	float clipSpeed = pPed->GetPreviousAnimatedVelocity().Mag();
	float speedRatio = (clipSpeed !=0) ? speed/clipSpeed : 1.0f;	
	
	CMoveBlendRatioSpeeds moveSpeeds;
	pMotionTask->GetMoveSpeeds(moveSpeeds);
	Vector2 vDesiredMBR;
	pMotionData->GetDesiredMoveBlendRatio(vDesiredMBR.x, vDesiredMBR.y);
	float fWorkingMBR = Max(fabs(vDesiredMBR.x), fabs(vDesiredMBR.y));

	float currHeading = pMotionData->GetCurrentHeading();
	float headingDelta = fabs(SubtractAngleShorter(pMotionData->GetDesiredHeading(), currHeading));

	bool bStrafing = pPed->IsStrafing();
	if ( bStrafing && 
		(this->GetTaskType()==CTaskTypes::TASK_MOTION_AIMING 
		 FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(true, true))) )
	{
		if(vDesiredMBR.Mag2() > 0.f)
		{
			vDesiredMBR.Normalize();
			currHeading = fwAngle::LimitRadianAngleSafe(rage::Atan2f(vDesiredMBR.y, vDesiredMBR.x)-HALF_PI);
		}
	}
	

	float fGaitReduction = pMotionData->GetGaitReduction();
	float fGaitReductionHeading = pMotionData->GetGaitReductionHeading();
	//if (pPed->IsLocalPlayer())
	//	Displayf("Desired MBR: %f, GaitReduction = %f, spd/aspd: %f, heading Delta: %f", vDesiredMBR.y, fGaitReduction, speedRatio, headingDelta );

	static float minimumRatio = 0.75f;
	static float minimumHeadingDelta = 1.0f;
	static float totalHeadingDelta = PI/8.0f;
	static u32   blockTime = 1000;
	
	bool bNoStand = false;
	bool bReduceWalkRatio = false;
	bool bDidPhysicsCheck = false;

	bool blockGaitReduction = (speedRatio>minimumRatio && fGaitReduction == 0.0f)
		|| headingDelta > minimumHeadingDelta //Current change
		|| (fGaitReduction && fabs(currHeading - fGaitReductionHeading) > totalHeadingDelta)
		|| pPed->GetPedResetFlag(CPED_RESET_FLAG_PreserveAnimatedAngularVelocity); //total change

	if (blockGaitReduction) 
	{
		if (fGaitReduction != 0.0f) //only when breaking a gait reduction in place
			pMotionData->SetGaitReductionBlockTime();
	} 
	else if((pMotionData->GetGaitReductionBlockTime() + blockTime) >= fwTimer::GetTimeInMilliseconds())
	{
		blockGaitReduction = true;
	}

	//If in a stuck state, see if there is still something in the way		
	if (!blockGaitReduction && (fGaitReduction || m_GaitReductionBlockedOnCollision))
	{
		pMotionData->SetGaitReductionFlag(CPedMotionData::GR_HitIncline, false);

		if(m_CapsuleResults.GetResultsReady())
		{
			bDidPhysicsCheck = true;

			if (m_CapsuleResults.IsEmpty())
			{
				blockGaitReduction = true;  //unlock		
				m_GaitReductionBlockedOnCollision = false;
			}
			else 
			{
				if(m_CapsuleResults[0].IsAHit())
				{
					const phInst* pHitInst = m_CapsuleResults[0].GetHitInst();
					const CEntity* pHitEntity = m_CapsuleResults[0].GetHitEntity();

					if (pHitInst && !pHitInst->GetArchetype()->GetTypeFlag(ArchetypeFlags::GTA_PED_TYPE)) //peds get full gait reduce B* 484431
					{
						if (!PHLEVEL->IsFixed(pHitInst->GetLevelIndex()) && (!pHitEntity || !pHitEntity->GetIsFixedFlagSet()))
						{ //pushing things around				
							const u32 typeFlags = CPhysics::GetLevel()->GetInstanceTypeFlags(pHitInst->GetLevelIndex());
							if ((typeFlags & ArchetypeFlags::GTA_GLASS_TYPE) == 0)
							{
								float fMassOfCollision = pHitInst->GetArchetype()->GetMass();
								//Displayf("Mass of collision: %f", fMassOfCollision);
								if (fMassOfCollision < 500.0f)
								{							
									bReduceWalkRatio = true;
								} 			
							}				
						}

						// B*2239914: Block gait reduction if we're walking into an unlocked door which isn't fully opened.
						if(pHitEntity && pHitEntity->GetIsTypeObject())
						{
							const CObject *pObject = static_cast<const CObject*>(pHitEntity);
							if (pObject && pObject->IsADoor())
							{
								const CDoor *pDoor = static_cast<const CDoor*>(pObject);
								static dev_float sfRatioEpsilon = 0.02f;
								if (pDoor && !pDoor->IsDoorFullyOpen(sfRatioEpsilon) && pDoor->GetDoorSystemData() && pDoor->GetDoorSystemData()->GetState() == DOORSTATE_UNLOCKED)
								{
									blockGaitReduction = true;
								}
							}
						}

						if (!blockGaitReduction && ! bNoStand)
						{							
							Vector3 vHitNormal = m_CapsuleResults[0].GetHitPolyNormal();
							if (vHitNormal.z > 0.4f )
							{
								pMotionData->SetGaitReductionFlag(CPedMotionData::GR_HitIncline, true);
							}

							//check angle to collision to try walking  B* 466133
							if (vHitNormal.z < 0.75f) //ignore up-bounds (B* 653880)
							{
								vHitNormal.z=0; vHitNormal.Normalize();
								Vector3 vPedNormal = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
								vPedNormal.z=0; vPedNormal.Normalize();
								if (bStrafing)
								{
									vPedNormal.RotateZ(currHeading);
								}
	
								float dot = vHitNormal.Dot(vPedNormal);
								//Displayf("Dot = %f", dot);
								static dev_float sfMIN_STAND_ANGLE = -0.7f;
								if (dot > sfMIN_STAND_ANGLE)
									bNoStand = true;
							}					
						}
					}
				}
			}
			m_CapsuleResults.Reset();
		}

		if(!m_CapsuleResults.GetWaitingOnResults())
		{
			WorldProbe::CShapeTestCapsuleDesc capsuleTest;

			Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());		
			vStart.z-=0.5f;
			Vector3 vEnd(vStart);
			Vector3 vTestDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			if (bStrafing )
			{
				vTestDirection.z=0; vTestDirection.Normalize();
				vTestDirection.RotateZ(currHeading);
			}

			static dev_float sfCapsuleLength = 0.5f;
			vEnd += vTestDirection * sfCapsuleLength;
			vStart += vTestDirection * -0.1f; //pull it back behind the ped a little for swept sphere
			static dev_float sfCapsuleRadius = 0.2f;
			// Test flags
			static const int iBoundFlags   = ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE; 
			//static const int iExcludeFlags = ArchetypeFlags::GTA_PED_TYPE; 
			capsuleTest.SetCapsule(vStart, vEnd, sfCapsuleRadius);
			capsuleTest.SetResultsStructure(&m_CapsuleResults);
			capsuleTest.SetIsDirected(true);
			capsuleTest.SetIncludeFlags(iBoundFlags);		
			capsuleTest.SetExcludeInstance(pPed->GetAnimatedInst());
			//capsuleTest.SetExcludeTypeFlags(iExcludeFlags);	

			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}
	}

	// Always allow walk when near doors
// 	if( pPed->GetPedResetFlag( CPED_RESET_FLAG_IsNearDoor ) )
// 	{
// 		bNoStand = true;
// 	}

	if (!blockGaitReduction) 
	{
		bool downgrade = false;		
		static float sprintSpeedScale = 0.9f;
		static float runSpeedScale = 0.5f;		
		static float sfWalkSpeedScale = 0.5f;		
		static float sfReducedWalkSpeedScale = 0.1f;		
		float walkSpeedScale = bReduceWalkRatio ? sfReducedWalkSpeedScale : sfWalkSpeedScale;
		if (fWorkingMBR>= MOVEBLENDRATIO_SPRINT)
		{
			bool bTooSlow = (speed < (moveSpeeds.GetSprintSpeed()*sprintSpeedScale));
			if (bTooSlow)
			{
				downgrade = true;
				fMaxMBR=MOVEBLENDRATIO_RUN;  
				if (!fGaitReduction) 
				{
					fGaitReduction = MOVEBLENDRATIO_SPRINT;
					fGaitReductionHeading = currHeading;
				}				
				//set speed override if we reduced the gait
				SetVelocityOverride(Vector3(0,moveSpeeds.GetSprintSpeed(),0));
			} else 
			{
				downgrade = false;
				fGaitReduction=0;
			}
		}
		if (downgrade || fWorkingMBR>= MOVEBLENDRATIO_RUN)
		{
			bool bTooSlow = (speed < (moveSpeeds.GetRunSpeed()*runSpeedScale));
			if (bTooSlow)
			{
				downgrade = true;
				fMaxMBR=MOVEBLENDRATIO_WALK;  
				if (!fGaitReduction) 
				{
					fGaitReduction = MOVEBLENDRATIO_RUN;
					fGaitReductionHeading = currHeading;
				}
				if (fGaitReduction==MOVEBLENDRATIO_RUN)
				{
					//set speed override if we reduced the gait
					SetVelocityOverride(Vector3(0,moveSpeeds.GetRunSpeed(),0));
				}
			} else 
			{			
				if (fGaitReduction<=MOVEBLENDRATIO_RUN)
					fGaitReduction=0;			
				downgrade = false;
			}
		}
		if (downgrade || fWorkingMBR> MOVEBLENDRATIO_STILL) //anything >0 is walk (animals)
		{
			bool bTooSlow = (speed < (moveSpeeds.GetWalkSpeed()*walkSpeedScale)); //usually want to walk at least
			if (bTooSlow && pPed->IsLocalPlayer() && !bNoStand && pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame()) //only player peds can be reduced to a stand-still B* 456890
			{
				if (!fGaitReduction)
				{
					fGaitReduction = MOVEBLENDRATIO_WALK;
					fGaitReductionHeading = currHeading;
				}				

				if (!bDidPhysicsCheck)
				{	//if we didn't do a physics check earlier, do one now to make sure we cans stand still here
					pMotionData->SetGaitReduction(fGaitReduction);
					pMotionData->SetGaitReductionHeading(fGaitReductionHeading);
					if (pMotionData->GetGaitReducedMaxMoveBlendRatio() == MOVEBLENDRATIO_STILL) 
					{
						fMaxMBR=MOVEBLENDRATIO_STILL;  
						SetVelocityOverride(VEC3_ZERO);
					}
				} 
				else 
				{
					fMaxMBR=MOVEBLENDRATIO_STILL;  
					SetVelocityOverride(VEC3_ZERO);
				}
				
			} else 
			{			
				if (fGaitReduction && downgrade)
				{
					fMaxMBR=MOVEBLENDRATIO_WALK; //Try to walk
					SetVelocityOverride(Vector3(0,moveSpeeds.GetRunSpeed(),0));						
				}
				downgrade = false;
			}
		}
		if (fGaitReduction && fWorkingMBR==MOVEBLENDRATIO_STILL) //I really want to stand
			fGaitReduction=0;
	} 
	else
	{
		fGaitReduction=0;
	}

	pMotionData->SetGaitReductionHeading(fGaitReductionHeading);
	pMotionData->SetGaitReduction(fGaitReduction);
	pMotionData->SetGaitReducedMaxMoveBlendRatio(fMaxMBR);
}

Vec3V_Out CTaskMotionBase::CalcDesiredAngularVelocity(Mat34V_ConstRef updatedPedMatrixIn, float fTimestep)
{
	const Matrix34& updatedPedMatrix = RCC_MATRIX34(updatedPedMatrixIn);

	Vector3 vAngVel = VEC3_ZERO;
	CPed* pPed = GetPed();

	const CPedMotionData& data = GetMotionData();
	const float fGameTimeStep = fwTimer::GetTimeStep();

	//*******************
	//	Apply heading

	// Heading change is primarily driven from clip
	// But also has additional components (extra heading change this frame or an independent mover transition)

	//m_fLastFrameAnimatedAngularVelocity = pPed->GetAnimatedAngularVelocity();

	float fExtractedAngularVel = pPed->GetAnimatedAngularVelocity() * pPed->m_PedResetFlags.GetAnimRotationModifier();

	// Apply the independent mover angular velocity if active
	if(data.GetIndependentMoverTransition() == 1)
	{
		float independentMoverDofWeight = GetIndependentMoverDofWeight();
		fExtractedAngularVel += ((data.GetIndependentMoverHeadingDelta()*independentMoverDofWeight)/ fGameTimeStep);
		animTaskDebugf(this, "Adding exta heading for independent mover fixup: headingDelta:%.4f, angularVel: %.4f, dofWeight: %.4f, timestep: %.4f", data.GetIndependentMoverHeadingDelta(), ((data.GetIndependentMoverHeadingDelta()*independentMoverDofWeight)/ fGameTimeStep), independentMoverDofWeight, fGameTimeStep);
	}

	float fAdditionalHeadingChange = pPed->ms_bApplyExtraHeadingChangeToPeds ? data.GetExtraHeadingChangeThisFrame() : 0.0f;
	float fAdditionalPitchChange = data.GetExtraPitchChangeThisFrame();
	
	// Extra heading change is not a velocity, so divide by time
	fAdditionalHeadingChange /= fGameTimeStep; // don't use physics timestep because then we'll get 2x the intended turn

	// If any clips are controlling the ped, then stop any extra heading change
	// and slave the desired heading to the current which is coming from the clips
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsHigherPriorityClipControllingPed))
	{
		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading))
		{
			fAdditionalHeadingChange = 0.0f;
			fAdditionalPitchChange = 0.0f;		
			float fDesiredHeading = pPed->GetCurrentHeading();

			// Add on the heading change we are about to apply so that our desired will be up to date after the physics update
			fDesiredHeading += fExtractedAngularVel*fTimestep;
			fDesiredHeading = fwAngle::LimitRadianAngleSafe(fDesiredHeading);
			
			pPed->SetDesiredHeading(fDesiredHeading);
			pPed->SetDesiredPitch(pPed->GetCurrentPitch());
		}
	}

	float fTotalHeadingVel = fAdditionalHeadingChange + fExtractedAngularVel;

	// Rotation boost: arbitrary increase in angular velocity to rotate to camera heading faster.
	// TODO: do we need this?
	////if( pPed->IsFirstPersonShooterModeEnabledForPlayer() )
	////{
	////	const bool bEnterExitVehicle = pPed->GetIsEnteringVehicle(true) || pPed->IsExitingVehicle() || pPed->IsBeingJackedFromVehicle();
	////	if(!pPed->GetMotionData()->GetIsStrafing() && !bEnterExitVehicle)
	////	{
	////		float fCurrentHeading = fwAngle::LimitRadianAngle( pPed->GetCurrentHeading() + fTotalHeadingVel * fwTimer::GetTimeStep() );
	////		float fDesiredHeading = camInterface::GetPlayerControlCamHeading(*pPed);

	////		float fHeadingDiff = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);

	////		// TODO: expose scale to metadata?
	////		fTotalHeadingVel = fHeadingDiff * 2.0f;
	////	}
	////}

	// FPS Swim: Don't modify angular velocity based on peds pitch 
	Vector3 vFPSSwimAngularVelocityScaler(0.0f, 0.0f, 1.0f);
	Vector3 fVelocityScaler = pPed->GetIsFPSSwimming() ? vFPSSwimAngularVelocityScaler : updatedPedMatrix.c;
	vAngVel += fVelocityScaler * (fTotalHeadingVel);

	//*******************
	//	Apply pitch.

	// There is currently no pitch change extracted from clip
	// So it is purely driven by the 'extra pitch change this frame'

	// Use game timestep here! not physics time step or it will be time slice dependent
	float fPitchVel = fAdditionalPitchChange / fGameTimeStep;

	// Limit pitch in similar way to heading
	// Prevent overshoot

	const float fAngDiff = fwAngle::LimitRadianAngle(pPed->GetDesiredPitch() - pPed->GetCurrentPitch());

	// First off make sure ped is turning in the correct direction altogether
	if(Sign(fAngDiff) != Sign(fPitchVel))
	{
		// Ped is turning in wrong direction to reach target
		fPitchVel = 0.0f;
	}
	else
	{
		// Ped is turning in correct direction
		// Make sure ped doesn't overshoot
		float fMaxVel = Abs(fAngDiff / fTimestep);
		fPitchVel = Min(fPitchVel,fMaxVel);
	}

	vAngVel += fPitchVel * updatedPedMatrix.a;

	return VECTOR3_TO_VEC3V(vAngVel);
}

void CTaskMotionBase::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	// By default don't allow any pitching
	fMinOut = 0.0f;
	fMaxOut = 0.0f;
}

float CTaskMotionBase::GetStoppingDistance(const float UNUSED_PARAM(fStopDirection), bool* UNUSED_PARAM(bOutIsClipActive))
{ 
	return 0.0f; 
}

// Purpose: returns the turn rates at walk, run, and sprint move-blend ratios
void CTaskMotionBase::GetTurnRates(CMoveBlendRatioTurnRates& rates)
{
	const CMotionTaskDataSet* pSet = GetPed()->GetMotionTaskDataSet();

	if (Verifyf(pSet, "Invalid motion task dataset!"))
	{
		const sMotionTaskData* pOnFoot = pSet->GetOnFootData();

		if (Verifyf(pOnFoot, "Invalid on foot motion task dataset!"))
		{
			rates.SetWalkTurnRate(pOnFoot->m_WalkTurnRate);
			rates.SetRunTurnRate(pOnFoot->m_RunTurnRate);
			rates.SetSprintTurnRate(pOnFoot->m_SprintTurnRate);
		}
	}
}

#if __DEV 
void CTaskMotionBase::SetDefaultOnFootClipSet(const fwMvClipSetId &clipSetId)
{
	animAssert(clipSetId != CLIP_SET_ID_INVALID); m_defaultOnFootClipSet = clipSetId;
}
#endif // __DEV

const crClip* CTaskMotionBase::GetBaseIdleClip()
{
	static fwMvClipId clipHash("idle",0x71C21326);
	if (m_defaultOnFootClipSet.GetHash()!=0)
	{
		fwClipSet *currentSet = fwClipSetManager::GetClipSet(m_defaultOnFootClipSet);
		if (currentSet)
		{
			// Request_DEPRECATED the clipset is streamed in
			// Surely this should have already been streamed in? (JA)
			currentSet->Request_DEPRECATED(); 

			return currentSet->GetClip(clipHash, CRO_FALLBACK | CRO_FALLBACK_IF_NOT_STREAMED);
		}
	}

	return NULL;
}

void CTaskMotionBase::SetOnFootClipSet(const fwMvClipSetId &clipSetId, const float fBlendDuration)
{
	animAssert(clipSetId != CLIP_SET_ID_INVALID);

	bool isInjured = false;
	
	if (GetEntity())
	{
		isInjured = GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured);
	}

	if (clipSetId != CLIP_SET_ID_INVALID && ((m_defaultOnFootClipSet != clipSetId) || (isInjured != m_isUsingInjuredMovementOverride)))
	{
		SetDefaultOnFootClipSet(clipSetId);
		m_isUsingInjuredMovementOverride = isInjured;
		m_onFootBlendDuration = fBlendDuration;
	}
}

void CTaskMotionBase::SetInVehicleContextHash(u32 inVehicleContext, bool bRestartState)
{
	m_overrideInVehicleContext = inVehicleContext;

	if (bRestartState)
	{
		InVehicleContextChanged(inVehicleContext);
	}
}

void CTaskMotionBase::ClearOverrideWeaponClipSet()
{ 
	m_overrideWeaponClipSet.Clear();
	m_overrideWeaponClipSetFilter.Clear();
	m_overrideUsesUpperBodyShadowExpression = false;
	
	//Note that the override weapon clip set has been cleared.
	OverrideWeaponClipSetCleared();
}

void CTaskMotionBase::ResetSwimmingClipSet(const CPed& ped)
{
	taskAssert(ped.GetMotionTaskDataSet());
	const sMotionTaskData* pInWaterData = ped.GetMotionTaskDataSet()->GetInWaterData();
	if ( pInWaterData == NULL )
	{
		SetDefaultSwimmingBaseClipSet( CLIP_SET_ID_INVALID );
		SetDefaultSwimmingClipSet( CLIP_SET_ID_INVALID );
		return; 
	}

	fwMvClipSetId baseClipSet ( pInWaterData->m_SwimmingClipSetBaseHash.GetHash() );
	SetDefaultSwimmingBaseClipSet( baseClipSet );
	fwMvClipSetId clipSet ( pInWaterData->m_SwimmingClipSetHash.GetHash() );
	SetDefaultSwimmingClipSet( clipSet );
}

void CTaskMotionBase::ResetOnFootClipSet(bool bRestartState, float fBlendDuration)
{
	fwMvClipSetId clipSet = GetModelInfoOnFootClipSet(*GetPed());

	if (bRestartState)
	{
		SetOnFootClipSet(clipSet, fBlendDuration);
	}
	else
	{
		SetDefaultOnFootClipSet(clipSet);
	}
}

void CTaskMotionBase::ResetOnFootClipSet(const CPed& ped)
{
	fwMvClipSetId clipSet = GetModelInfoOnFootClipSet(ped);
	SetDefaultOnFootClipSet(clipSet);
}

void CTaskMotionBase::ResetInVehicleContextHash(bool bRestartState)
{
	SetInVehicleContextHash(0, bRestartState);
}

void CTaskMotionBase::ResetStrafeClipSet()
{
	SetOverrideStrafingClipSet(CLIP_SET_ID_INVALID);
	SetOverrideCrouchedStrafingClipSet(CLIP_SET_ID_INVALID);
}

void CTaskMotionBase::SetDefaultClipSets(const CPed& ped)
{
	static fwMvClipSetId s_defaultDivingSet("move_ped_diving",0xA2CA7C1A);

	SetDefaultDivingClipSet(s_defaultDivingSet);

	ResetSwimmingClipSet(ped);
	ResetOnFootClipSet(ped);
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionBase::SetAlternateClip(AlternateClipType type, const sAlternateClip& data, bool bLooping)
{
	//copy the data
	m_alternateClips[type] = data;
	SetAlternateClipLooping(type, bLooping);
	
	//Note that the alternate walk clip changed.
	AlternateClipChanged(type);
}

void CTaskMotionBase::StopAlternateClip(AlternateClipType type, float fBlendDuration)
{
	sAlternateClip data;
	data.fBlendDuration = fBlendDuration;

	SetAlternateClip(type, data, false);
}

void CTaskMotionBase::SetAlternateClipLooping(AlternateClipType type, bool bLooping)
{
	if(bLooping)
	{
		m_alternateClipsLooping |= (1 << type);
	}
	else
	{
		m_alternateClipsLooping &= ~(1 << type);
	}
}

bool CTaskMotionBase::GetAlternateClipLooping(AlternateClipType type) const
{
	return (m_alternateClipsLooping & (1<<type)) != 0;
}

void CTaskMotionBase::SetIndependentMoverFrame(const float fHeadingDelta, const fwMvRequestId& requestId, const bool bActuallyApply)
{
	//taskAssertf(GetMotionData().GetIndependentMoverTransition() != 1, "Independent Mover Set when value is 1 - will cause glitch");

#if __DEV
	if(GetMotionData().GetIndependentMoverTransition() == 1)
	{
		taskDebugf3("%d: ped:0x%p, task:0x%p Independent Mover Set when value is 1 - will cause glitch", fwTimer::GetTimeInMilliseconds(), GetEntity(), this);
	}

	TUNE_GROUP_BOOL(INDEPENDENT_MOVER_TUNE, DISABLE_INDEPENDENT_MOVER, false);
	if (DISABLE_INDEPENDENT_MOVER)
	{
		return;
	}
#endif // __DEV

	if(!GetPed()->GetPedResetFlag(CPED_RESET_FLAG_DisableIndependentMoverFrame))
	{
		// Hack - so we setup the motiondata variables without paying the cost of the allocation
		// Set this up so I can send the data in PostMovement, but query if we are doing an independent mover in ProcessControl
		if(bActuallyApply)
		{
			static fwMvFrameId s_independentMoverFrame("IndependentMoverFrame",0x2CA73BFC); 

			// Fill in the independent mover frame
			Quaternion desiredRotation;
			desiredRotation.FromEulers(Vector3(0.0f, 0.0f, fHeadingDelta));

			crFrame* pMoverFrame = GetPed()->GetIndependentMoverFrame();
			Assert(pMoverFrame);

			pMoverFrame->SetQuaternion(kTrackIndependentMover, 0, QUATERNION_TO_QUATV(desiredRotation));

			GetMoveNetworkHelper()->SetFrame(pMoverFrame, s_independentMoverFrame);
			GetMoveNetworkHelper()->SendRequest(requestId);
		}

		// Track that a transition is occurring
		static dev_u32 FRAME_COUNT = 2;
		GetMotionData().SetIndependentMoverTransition(FRAME_COUNT);
		GetMotionData().SetIndependentMoverHeadingDelta(fHeadingDelta);
	}
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionBase::GetIndependentMoverDofWeight() const
{
	float independentMoverDofWeight = 0.0f;

	const CPed* pPed = GetPed();
	const crCreatureComponentExtraDofs* pExtraDofs = pPed->GetAnimDirector()->GetCreature()->FindComponent<crCreatureComponentExtraDofs>();
	if(pExtraDofs)
	{
		const crFrame& poseFrame = pExtraDofs->GetPoseFrame();
		poseFrame.GetFloat(kTrackIndependentMoverWeight, BONETAG_ROOT, independentMoverDofWeight);
	}

	return independentMoverDofWeight;
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionBase::CPersistentData::CPersistentData()
: m_onFootClipSet(u32(0))
, m_strafingClipSet(u32(0))
, m_weaponOverrideClipSet(u32(0))
, m_inVehicleOverrideContext(0)
{
	memset(m_alternateClipsLooping, 0, sizeof(m_alternateClipsLooping));
}

CTaskMotionBase::CPersistentData::~CPersistentData()
{
}

void CTaskMotionBase::CPersistentData::Init(CTaskMotionBase& oldTask)
{
	m_onFootClipSet = oldTask.GetDefaultOnFootClipSet(true);
	m_strafingClipSet = oldTask.GetOverrideStrafingClipSet();
	m_weaponOverrideClipSet = oldTask.GetOverrideWeaponClipSet();
	m_inVehicleOverrideContext = oldTask.GetOverrideInVehicleContextHash();

	for (s32 i = 0; i < ACT_MAX; i++)
	{
		const sAlternateClip& alt = oldTask.GetAlternateClip(static_cast<AlternateClipType>(i));
		if (alt.IsValid())
		{
			m_alternateClips[i] = alt;
			m_alternateClipsLooping[i] = oldTask.GetAlternateClipLooping(static_cast<AlternateClipType>(i));
		}
	}
}

void CTaskMotionBase::CPersistentData::Apply(CTaskMotionBase& newTask)
{
	if (m_onFootClipSet != CLIP_SET_ID_INVALID)
	{
		newTask.SetOnFootClipSet(m_onFootClipSet);
	}
	if (m_strafingClipSet != CLIP_SET_ID_INVALID)
	{
		newTask.SetOverrideStrafingClipSet(m_strafingClipSet);
	}
	if (m_weaponOverrideClipSet != CLIP_SET_ID_INVALID)
	{
		newTask.SetOverrideWeaponClipSet(m_weaponOverrideClipSet);
	}
	if (m_inVehicleOverrideContext != 0)
	{
		newTask.SetInVehicleContextHash(m_inVehicleOverrideContext, false);
	}

	for (s32 i = 0; i < ACT_MAX; i++)
	{
		if (m_alternateClips[i].IsValid())
		{
			newTask.SetAlternateClip(static_cast<AlternateClipType>(i), m_alternateClips[i], m_alternateClipsLooping[i]);
		}
	}
}

float CTaskMotionBase::InterpValue(float& current, float target, float interpRate, bool angular)
{
	return InterpValue(current, target, interpRate, angular, fwTimer::GetTimeStep());
}

float CTaskMotionBase::InterpValue(float& current, float target, float interpRate, bool angular, float fDt)
{
#if USE_LINEAR_INTERPOLATION
	float initialCurrent = current;

	if (angular)
	{
		float amount = SubtractAngleShorter(target, current);
		target = current+amount;
	}

	if (target<current)
	{
		current-=(interpRate*fDt);
		current = Max(current, target);
	}
	else
	{
		current+=(interpRate*fDt);
		current = Min(current, target);
	}

	return (current-initialCurrent);
#else //USE_LINEAR_INTERPOLATION
	float amount;
	if (angular)
	{
		amount = SubtractAngleShorter(target, current);
	}
	else
	{
		amount = target-current;
	}
	amount = (amount * fDt * interpRate);
	current+=amount;
	return amount;
#endif //USE_LINEAR_INTERPOLATION
}

fwMvClipSetId CTaskMotionBase::GetDefaultOnFootClipSet(const bool bReturnMemberVariable) const
{
	if(bReturnMemberVariable)
	{
		return m_defaultOnFootClipSet;
	}

	const CPed* pPed = GetPed();

	// Only use weapon clipset if m_defaultOnFootClipSet hasn't been overridden
	if (m_defaultOnFootClipSet == GetModelInfoOnFootClipSet(*pPed))
	{
		const CWeaponInfo *pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if(pWeaponInfo)
		{
			fwMvClipSetId movementWeaponOverride = pWeaponInfo->GetMovementClipSetOverride(*GetPed());
			if(movementWeaponOverride != CLIP_SET_ID_INVALID)
			{
				return movementWeaponOverride;
			}
		}
	}

	return m_isUsingInjuredMovementOverride ? (GetPed()->IsMale() ? CLIP_SET_MOVE_INJURED : CLIP_SET_MOVE_INJURED_FEMALE) : m_defaultOnFootClipSet;
}

fwMvClipSetId CTaskMotionBase::GetDefaultAimingStrafingClipSet(bool bCrouched) const
{
	static const fwMvClipSetId s_StrafingClipSetId("move_ped_strafing",0x92EC5666);
	static const fwMvClipSetId s_CrouchedStrafingClipSetId("move_ped_crouched_strafing",0x92B35B93);
	static const fwMvClipSetId s_CrouchedCoverStrafingClipSetId("cover@weapon@core",0xD8BD0F2E);
	static const fwMvClipSetId s_StealthStrafingClipSetId("move_ped_strafing_stealth",0x9EECB6A8);
	static const fwMvClipSetId s_StrafingGrenadeClipSetId("move_ped_strafing_grenade",0x8A1403C8);
#if FPS_MODE_SUPPORTED
	static const fwMvClipSetId s_StrafingFirstPersonClipSetId("move_ped_strafing_firstperson",0xbfbcbae5);
	static const fwMvClipSetId s_StealthStrafingFirstPersonClipSetId("move_ped_strafing_stealth_firstperson",0x41aecd89);
	static const fwMvClipSetId s_CloneStrafingFirstPersonClipSetId("move_ped_strafing_clone_firstperson",0x2657bf43);
#endif // FPS_MODE_SUPPORTED

	// Get the ped
	const CPed* pPed = GetPed();

	fwMvClipSetId strafeClipSet;

	if (pPed->GetIsFPSSwimming())
	{
		if (pPed->GetIsFPSSwimmingUnderwater())
		{
			strafeClipSet = CLIP_SET_FPS_DIVING;
		}
		else
		{
			strafeClipSet = CLIP_SET_FPS_SWIMMING;
		}
	}
	else if (bCrouched && pPed->GetIsInCover())
	{
		strafeClipSet = s_CrouchedCoverStrafingClipSetId;
	}
	else if (pPed->GetIsInCover() && !pPed->IsAPlayerPed())
	{
		strafeClipSet = s_StrafingClipSetId;
	}
	else if (pPed->GetMotionData()->GetUsingStealth())
	{
#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
		{
			strafeClipSet = s_StealthStrafingFirstPersonClipSetId;
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			strafeClipSet = s_StealthStrafingClipSetId;
		}
	}
	else
	{
		strafeClipSet = bCrouched ? GetOverrideCrouchedStrafingClipSet() : GetOverrideStrafingClipSet();
	}

	// For projectiles, override strafing clipset
	CTaskAimAndThrowProjectile* pProjectileTask = static_cast<CTaskAimAndThrowProjectile*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE));
	if (pProjectileTask && pProjectileTask->ShouldUseProjectileStrafeClipSet())
	{
		strafeClipSet = s_StrafingGrenadeClipSetId;		
	}

	if(strafeClipSet == CLIP_SET_ID_INVALID)
	{
		if(bCrouched)
		{
			strafeClipSet = s_CrouchedStrafingClipSetId;
		}
		else
		{
#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
			{
				bool bIsFPSIdle = pPed->GetMotionData()->GetIsFPSIdle();

				TUNE_GROUP_BOOL(PED_MOVEMENT, USE_FPS_CLONE_STRAFE_ANIMS, false);
				const bool bIsClone = pPed->IsNetworkClone();
				if(bIsClone)
				{
					bIsFPSIdle = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsInFirstPersonIdle();
				}

				const CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
				const bool bUseAimingClipSet = pWeapMgr->GetEquippedWeaponHash() != pPed->GetDefaultUnarmedWeaponHash() && pWeapMgr->GetEquippedWeaponInfo() && !pWeapMgr->GetEquippedWeaponInfo()->GetIsMeleeFist() && !bIsFPSIdle;

				if(!bUseAimingClipSet && (bIsClone || USE_FPS_CLONE_STRAFE_ANIMS) && fwClipSetManager::GetClipSet(s_CloneStrafingFirstPersonClipSetId))
				{
					strafeClipSet = s_CloneStrafingFirstPersonClipSetId;
				}
				else
				{
					strafeClipSet = fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSStrafeClipSetHash(*pPed));

					if(strafeClipSet == CLIP_SET_ID_INVALID)
					{
						if(bUseAimingClipSet)
						{
							strafeClipSet = ms_AimingStrafingFirstPersonClipSetId;
						}
						else
						{
							strafeClipSet = s_StrafingFirstPersonClipSetId;
						}
					}
				}
			}
			else
#endif // FPS_MODE_SUPPORTED
			{
				const CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
				if(pModelInfo)
				{
					fwMvClipSetId modelClipSetId = pModelInfo->GetAppropriateStrafeClipSet(pPed);
					if(modelClipSetId != CLIP_SET_ID_INVALID)
					{
						strafeClipSet = modelClipSetId;
					}
					else
					{
						strafeClipSet = s_StrafingClipSetId;
					}
				}
				else
				{
					strafeClipSet = s_StrafingClipSetId;
				}
			}
		}
	}
	return strafeClipSet;
}

float CTaskMotionBase::GetFloatPropertyFromClip(const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, const char* propertyName) const
{
	const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	if(pClip && pClip->GetProperties())
	{
		const crProperty* pProperty = pClip->GetProperties()->FindProperty(propertyName);
		if(pProperty)
		{
			const crPropertyAttribute* pPropertyAttribute = pProperty->GetAttribute(propertyName);
			if(pPropertyAttribute && pPropertyAttribute->GetType() == crPropertyAttribute::kTypeFloat)
			{
				return static_cast<const crPropertyAttributeFloat*>(pPropertyAttribute)->GetFloat();
			}
		}
	}

	// Default to 1.f
	return 1.f;
}

void CTaskMotionBase::UpdateHeadIK()
{
	if(NetworkInterface::IsGameInProgress())
	{
		bool bDoHeadIK = false;
		CPed* pPed = GetPed();

		if(pPed && pPed->IsNetworkClone() && pPed->IsPlayer())
		{
			bDoHeadIK = true;
		}

		if (bDoHeadIK)
		{
			CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPed->GetNetworkObject());

			grcViewport *pViewport = netObjPlayer?netObjPlayer->GetViewport():NULL;

			if(pViewport) 
			{
				if(!pPed->GetIKSettings().IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeBodyLook))
				{
					Matrix34 headMatrix;
					pPed->GetBoneMatrix(headMatrix, BONETAG_HEAD);

					bool bInFirstPerson = false;
					if (pPed->GetNetworkObject())
						bInFirstPerson = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsUsingFirstPersonCamera() || static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsUsingFirstPersonVehicleCamera();

					if (bInFirstPerson)
					{	
						Matrix34 camMatrix = RCC_MATRIX34(pViewport->GetCameraMtx());

						Vector3 vCamFace = -camMatrix.c; 
						Vector3 vLookAtPos = camMatrix.d + vCamFace* 2.0f; //As camera is inside players head set look at offset distance ahead of camera facing direction, 

#if __BANK
						TUNE_GROUP_BOOL(VEHICLE_HEAD_IK, bRenderHeadIKForClones, false);
						if (bRenderHeadIKForClones)
						{
							Vector3 vCameraPos = camMatrix.d;
							grcDebugDraw::Line(vCameraPos,vLookAtPos,Color_red,1);
						}
#endif

						pPed->GetIkManager().LookAt(0, NULL, 100, BONETAG_INVALID, &vLookAtPos, LF_WHILE_NOT_IN_FOV,
							500, 500, CIkManager::IK_LOOKAT_HIGH);
					}
				}
			}
		}
	}
}
