#include "Peds/Horse/horseSpeed.h"

#if ENABLE_HORSE

#include "horseSpeed_parser.h"

#include "Peds/Horse/horseBrake.h"
#include "Peds/Ped.h"
#include "math/amath.h"
#include "physics/gtaMaterialManager.h"

#if __BANK
#include "bank/bank.h"
#endif

AI_OPTIMISATIONS()

const float HORSE_MBR_STILL				= 0.0f;
const float HORSE_MBR_WALK				= 0.25f;
const float HORSE_MBR_TROT				= 0.5f;
const float HORSE_MBR_RUN				= 0.75f;
const float HORSE_MBR_GALLOP			= 1.0f;

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// hrsSpeedControl ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsSpeedControl::hrsSpeedControl()
{
	Reset();
#if __BANK
	m_bSimulateInWater = false;
	m_bSimulateOffPath = false;
#endif
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::Reset()
{
	m_State.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::OnMount(float fCurrMvrSpdMS, const bool bZeroSpeed)
{
	// Set the current and target speed to either the quickmount speed or the 
	// current mover speed (whichever is faster).

	float fNewSpeedNorm = 0;

	UpdateMoverSpeed(fCurrMvrSpdMS, false);
	fNewSpeedNorm = Max(fNewSpeedNorm, m_State.m_MvrSpeed);

	if( !bZeroSpeed )
	{
		m_State.m_TgtSpeed = fNewSpeedNorm;
		m_State.m_CurrSpeed = fNewSpeedNorm;
	}
	else
	{
		m_State.m_TgtSpeed = 0.0f;
		m_State.m_CurrSpeed = 0.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////
void hrsSpeedControl::Update(const u32 horseSpeedFlags, const float fCurrFreshness, float fCurrMvrSpdMS,
	const float fMaxSpdNorm, const float fMBR, const float fTimeSinceSpur, float fWaterSpeedMax,
	const Vector2 & vStickThrottle, const hrsBrakeControl &rBrakeCtrl,
	const float fDTime, const float speedMatchSpeed)
{
	if(!HasTuning())
	{
		return;
	}

	// Make sure freshness is normalized:
	float fFresh = rage::Clamp(fCurrFreshness, 0.0f, 1.0f);

	UpdateMoverSpeed(fCurrMvrSpdMS, false);

	const hrsSpeedControl::Tune &rTune = GetCurrTune();
	hrsState::KeyedByStickY kby;
	rTune.GetKeyframeData(Max(vStickThrottle.y, 0.0f), kby);

	UpdateSpeedBoostMult(1.0f + ((horseSpeedFlags&HSF_NoBoost) ? 0.f : kby.m_SpeedBoost), fDTime);
	float fMtlSpeedMult = m_State.m_fCurrMtlFactor; 
	m_State.m_fGaitSpeedScale = UpdateGaitSpeedScaleAndTurnRate(fMBR, m_State.m_fTurnRate);

	if ((horseSpeedFlags & HSF_AllowSlowdownAndSpeedGain) == 0)
	{
		fMtlSpeedMult = 1.0f;
	}
	if( rBrakeCtrl.GetInstantStop() )
	{
		m_State.m_TgtSpeed = 0.0f;
		m_State.m_CurrSpeed = 0.0f;
	}
	else if( rBrakeCtrl.GetHardBrake() || rBrakeCtrl.GetSoftBrake() )
	{
		m_State.m_TgtSpeed -= rBrakeCtrl.GetDeceleration() * fDTime;
		m_State.m_TgtSpeed = rage::Clamp(m_State.m_TgtSpeed, 0.0f, fMaxSpdNorm);
		m_State.m_CurrSpeed = rage::Clamp(m_State.m_CurrSpeed, 0.0f, rage::Min(fMaxSpdNorm, m_State.m_TgtSpeed));
	}
	else
	{
		// Compute the interpolated values for the current freshness and normalized speed:
		// TODO: Keep the results as part of our state?  It might be helpful to
		// graph these values in the EKG.
		hrsState::KeyedByFreshness kbf;
		hrsState::KeyedBySpeed kbs;
		hrsState::KeyedByStickMag kbmag;
		rTune.GetKeyframeData(fFresh, kbf);
		rTune.GetKeyframeData(m_State.m_CurrSpeed/*m_State.m_MvrSpeed*/, kbs);
		rTune.GetKeyframeData(Max(vStickThrottle.Mag(), 0.0f), kbmag);

		// Update the speed factor:
		if( (horseSpeedFlags&HSF_Aiming) && speedMatchSpeed < 0.0f )
			m_State.m_fSpeedFactor = GetCurrTune().m_AimSpeedFactor;
		else
			m_State.m_fSpeedFactor = 1.0f;

		m_State.m_fSpeedFactor *= fMtlSpeedMult;

		// Compute the new target speed:
		float fTgtSpd = m_State.m_TgtSpeed;
		if( (horseSpeedFlags&HSF_SpurredThisFrame) )
		{
			fTgtSpd += kbf.m_SpeedBoostPerSpur;
			if( kbf.m_SpeedBoostPerSpur <= 0.0f )
			{
				m_State.m_bLastSpurWasBad = true;
				++m_State.m_NumBadSpurs;
			}
			else
			{
				m_State.m_fDecayDelayTimer = GetCurrTune().m_fDecayDelay;
				m_State.m_bLastSpurWasBad = false;
				m_State.m_NumBadSpurs = 0;
			}
		}
		else if (m_State.m_fSupressedSpurringRequirementDuration > 0.0f)
		{
			m_State.m_fDecayDelayTimer = GetCurrTune().m_fDecayDelay;
			m_State.m_fSupressedSpurringRequirementDuration -= fDTime;
		}
		else if ((horseSpeedFlags&HSF_MaintainSpeed) && (speedMatchSpeed >= 0.0f || GetCurrTune().m_fMinMaintainSpeed >= m_State.m_CurrSpeed || m_State.m_CurrSpeed <= GetCurrTune().m_fMaxMaintainSpeed))
		{
			if( speedMatchSpeed >= 0.0f )
			{
				m_State.m_fDecayDelayTimer = GetCurrTune().m_fDecayDelay;
				fTgtSpd = speedMatchSpeed;
			}
			else if((horseSpeedFlags&HSF_AllowSlowdownAndSpeedGain) && fTimeSinceSpur >= GetCurrTune().m_fSpeedGainFactorDelay )
			{
				m_State.m_fDecayDelayTimer = GetCurrTune().m_fDecayDelay;
				//fTgtSpd += kbs.m_SpeedGainFactor;

				if (m_State.m_fMaintainSpeedDelayTimer <=0)
				{
					if (fTgtSpd < GetCurrTune().m_fMinMaintainSpeed  && ((horseSpeedFlags&HSF_Aiming) == 0))
						fTgtSpd = GetCurrTune().m_fMinMaintainSpeed;
				} 
				else
				{
					m_State.m_fMaintainSpeedDelayTimer -= fDTime;
				}				
			}
		}
		else
		{
			m_State.m_fMaintainSpeedDelayTimer = GetCurrTune().m_fMaintainSpeedDelay;
		}

		if( m_State.m_fDecayDelayTimer > 0.0f )
		{
			// Decrement the timer
			m_State.m_fDecayDelayTimer = rage::Max(0.0f, m_State.m_fDecayDelayTimer - fDTime);
		}
		else
		{
			// Apply speed decay
			fTgtSpd -= (kbs.m_SpeedDecay * kby.m_SpeedDecayMult * fDTime);
			if( fTgtSpd<0.0f )
				fTgtSpd = 0.0f;
		}

		float fMinSpd = Max(kby.m_MinSpeed, kbmag.m_MinSpeed);
		if( m_State.m_bCollidedThisFrame )
		{
			// Apply collision penalty
			if( m_State.m_bCollidedFixedThisFrame )
				Approach(fTgtSpd, 0.0f, rTune.m_fCollisionDecelRateFixed, fDTime);
			else if( fTgtSpd > m_State.m_MvrSpeed )
			{
				Approach(fTgtSpd, m_State.m_MvrSpeed, rTune.m_fCollisionDecelRate, fDTime);
			}

			// Clear the collided flag
			m_State.m_bCollidedThisFrame = false;
			m_State.m_bCollidedFixedThisFrame = false;

			// If we're colliding, the min speed must be zero to avoid jitter:
			fMinSpd = 0.0f;
		}

		fTgtSpd = Max(fTgtSpd, fMinSpd);

		fTgtSpd = Min(fTgtSpd, fWaterSpeedMax);

		if (horseSpeedFlags&HSF_Aiming)
			fTgtSpd = Min(m_State.m_MaxSpeedWhileAiming, fTgtSpd);

		if (m_State.m_fAutopilotTopSpeed >= 0.0f)
		{
			const float autopilotSpeedNormalized = m_State.m_fAutopilotTopSpeed / GetCurrTune().m_TopSpeed;
			m_State.m_TgtSpeed = Min(autopilotSpeedNormalized, fTgtSpd);
		}
		else
		{
			m_State.m_TgtSpeed = fTgtSpd;
		}

		m_State.m_bSliding = (horseSpeedFlags&HSF_Sliding)!=0;
		if (m_State.m_bSliding) {			
			m_State.m_TgtSpeed = Min(rTune.m_fMaxSlidingSpeed, m_State.m_TgtSpeed);  			  
		}

		PerformAcceleration(fFresh, fMaxSpdNorm, fDTime, FLT_MAX);
	}
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::UpdateMoverSpeed(const float fCurrSpdMS, bool alsoUpdateCurrentSpeed)
{
	// Convert the raw speed to a normalized value:
	const float spd = rage::Clamp( ComputeNormalizedSpeed(fCurrSpdMS), 0.0f, 1.0f);
	m_State.m_MvrSpeed = spd;
	if(alsoUpdateCurrentSpeed)
	{
		m_State.m_CurrSpeed = spd;
	}
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::UpdateSprintSpeed(float fSprintSpeedMPS)
{
	if( m_State.m_SprintSpeed != fSprintSpeedMPS )
		m_State.m_SprintSpeed = fSprintSpeedMPS;
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::PerformAcceleration(const float fFresh,
		const float fMaxSpdNorm, const float fDTime, float fMaxAccel)
{
	m_State.m_TgtSpeed = Min(m_State.m_TgtSpeed, fMaxSpdNorm);

	const hrsSpeedControl::Tune &rTune = GetCurrTune();

	if( rTune.m_bInstantaneousAccel )
	{
		// Set the current speed to the target speed instantly:
		m_State.m_CurrSpeed = m_State.m_TgtSpeed;
		// Accel is not used; just set it to zero:
		m_State.m_CurrAccel = 0.0f;
	}
	else
	{
		hrsState::KeyedByFreshness kbf;
		hrsState::KeyedBySpeed kbs;
		rTune.GetKeyframeData(fFresh, kbf);
		rTune.GetKeyframeData(m_State.m_MvrSpeed, kbs);

		// Approach the target speed based on acceleration:
		const float fAccelBeforeLimit = (kbs.m_Accel * kbf.m_AccelMult);
		const float fAccel = Min(fAccelBeforeLimit, fMaxAccel);

		m_State.m_CurrAccel = fAccel;

		rage::Approach(m_State.m_CurrSpeed, m_State.m_TgtSpeed, m_State.m_CurrAccel, fDTime);
	}
}

////////////////////////////////////////////////////////////////////////////////

float hrsSpeedControl::UpdateMaterialSpeedMult(float fDTime, phMaterialMgr::Id groundMaterial)
{
	const hrsSpeedControl::Tune &rTune = GetCurrTune();
	float fTargetMult = 1.0f;
	float fTargetRate = rTune.m_fMtlRecoveryApproachRate;
	bool bIsAI = false;

// 	if( ??? )
// 	{
//	
// 		fTargetMult = GetCurrTune().GetMtlWaterMult(bIsAI);
// 		const float noMovementHeight = mvr.GetStandingHeight()*mvr.GetWaterNoMovementHeightScalar();
// 		float liquidDepth = mvr.GetLiquidDepth();
// 
// 		static float kPlayerInWaterMaxDecel = 0.9f;
// 		static float kAIInWaterMaxDecel = 0.5f;
// 
// 		fTargetMult *= 1.f - Clamp(liquidDepth/noMovementHeight, 0.f, IsMountedByPlayer() ? kPlayerInWaterMaxDecel : kAIInWaterMaxDecel);
// 			
// 
// 		fTargetRate = rTune.m_fMtlWaterApproachRate;
// 	} else
	if( !PGTAMATERIALMGR->GetIsRoad(groundMaterial) )
	{
		fTargetMult = GetCurrTune().GetMtlOffPathMult(bIsAI);
		fTargetRate = rTune.m_fMtlOffPathApproachRate;
	}	
	Approach(m_State.m_fCurrMtlFactor, fTargetMult, fTargetRate, fDTime);

	return m_State.m_fCurrMtlFactor;
}

////////////////////////////////////////////////////////////////////////////////

float hrsSpeedControl::UpdateSpeedBoostMult(float fInputSpeedBoostMult, const float fDTime)
{
	const hrsSpeedControl::Tune &rTune = GetCurrTune();
	float fApproachRate = 0.0f;
	fInputSpeedBoostMult += m_State.m_fFollowSpeedOffset;
	m_State.m_fFollowSpeedOffset = 0.0f;
	if( fInputSpeedBoostMult < m_State.m_fCurrSpeedBoostFactor )
		fApproachRate = rTune.m_fSpeedBoostDecApproachRate;
	else
		fApproachRate = rTune.m_fSpeedBoostIncApproachRate;
	 Approach(m_State.m_fCurrSpeedBoostFactor, fInputSpeedBoostMult, fApproachRate, fDTime);

	 return m_State.m_fCurrSpeedBoostFactor;
}

float hrsSpeedControl::UpdateGaitSpeedScaleAndTurnRate(float fMBR, float& fTurnRate)
{
	const hrsSpeedControl::Tune &rTune = GetCurrTune();
	if (fMBR < HORSE_MBR_STILL)
	{
		fTurnRate = rTune.m_WalkTurnRate * DtoR;
		m_State.m_fMinGaitRateClamp = rTune.m_WalkMinSpeedRateScale;
		m_State.m_fMaxGaitRateClamp = rTune.m_WalkMaxSpeedRateScale;
		return rTune.m_ReverseSpeedScale;
	}
	else if (fMBR <= HORSE_MBR_WALK)
	{
		fTurnRate = rTune.m_WalkTurnRate * DtoR;
		m_State.m_fMinGaitRateClamp = rTune.m_WalkMinSpeedRateScale;
		m_State.m_fMaxGaitRateClamp = rTune.m_WalkMaxSpeedRateScale;
		return rTune.m_WalkSpeedScale;
	}
	else if (fMBR <= HORSE_MBR_TROT) 
	{
		float t = (fMBR-HORSE_MBR_WALK)/(HORSE_MBR_TROT-HORSE_MBR_WALK);
		fTurnRate = Lerp( t, rTune.m_WalkTurnRate, rTune.m_TrotTurnRate) * DtoR;
		m_State.m_fMinGaitRateClamp = rTune.m_TrotMinSpeedRateScale;
		m_State.m_fMaxGaitRateClamp = rTune.m_TrotMaxSpeedRateScale;
		return Lerp( t, rTune.m_WalkSpeedScale, rTune.m_TrotSpeedScale);
	}
	else if (fMBR <= HORSE_MBR_RUN)
	{
		float t = (fMBR-HORSE_MBR_TROT)/(HORSE_MBR_RUN-HORSE_MBR_TROT);
		fTurnRate = Lerp( t, rTune.m_TrotTurnRate, rTune.m_RunTurnRate) * DtoR;
		m_State.m_fMinGaitRateClamp = rTune.m_RunMinSpeedRateScale;
		m_State.m_fMaxGaitRateClamp = rTune.m_RunMaxSpeedRateScale;
		return Lerp( t, rTune.m_TrotSpeedScale, rTune.m_RunSpeedScale);
	}
	else 
	{
		float t = (fMBR-HORSE_MBR_RUN)/(HORSE_MBR_GALLOP-HORSE_MBR_RUN);
		fTurnRate = Lerp( t, rTune.m_RunTurnRate, rTune.m_GallopTurnRate) * DtoR;
		m_State.m_fMinGaitRateClamp = rTune.m_GallopMinSpeedRateScale;
		m_State.m_fMaxGaitRateClamp = rTune.m_GallopMaxSpeedRateScale;
		return Lerp( t, rTune.m_RunSpeedScale, rTune.m_GallopSpeedScale);
	}
}


////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::HandleCollision()
{
	m_State.m_bCollidedThisFrame = true;
//TODO
// 	int iLvlIdx = (collisionMsg.otherInst ? collisionMsg.otherInst->GetLevelIndex() : -1);
// 	m_State.m_bCollidedFixedThisFrame = m_State.m_bCollidedFixedThisFrame ||
// 		PHLEVEL->IsFixed(iLvlIdx);
}

////////////////////////////////////////////////////////////////////////////////

float hrsSpeedControl::GetTopSpeed() const
{
	if( m_State.m_fTopSpeedOverride>=0.0f )
		return m_State.m_fTopSpeedOverride;
	return GetCurrTune().m_TopSpeed;
}

////////////////////////////////////////////////////////////////////////////////

float hrsSpeedControl::ComputeNormalizedSpeed(const float fRawSpeed) const
{
	return GetCurrTune().ComputeNormalizedSpeed(fRawSpeed, m_State.m_fTopSpeedOverride);
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("SpeedControl", false);
	b.AddToggle("Simulate Off Path", &m_bSimulateOffPath);
	b.AddToggle("Simulate In Water", &m_bSimulateInWater);
	hrsControl::AddWidgets(b);
	m_State.AddWidgets(b);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Tune /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsSpeedControl::Tune::Tune()
{
	m_TopSpeed = 20.0f;
	m_bInstantaneousAccel = true;
	m_fCollisionDecelRate = 1.0f;
	m_fCollisionDecelRateFixed = 1.0f;
	m_fMaxSlidingSpeed = 0.5f;
	m_fDecayDelay = 0.0f;	
	m_AimSpeedFactor = 0.75f;
	m_fSpeedGainFactorDelay = 0.0f;
	m_fSpeedBoostIncApproachRate = 0.1f;
	m_fSpeedBoostDecApproachRate = 0.1f;
	m_fMtlWaterMult_Human = 1.0f;
	m_fMtlWaterMult_AI = 1.0f;
	m_fMtlWaterApproachRate = 1.0f;
	m_fMtlOffPathMult_Human = 1.0f;
	m_fMtlOffPathMult_AI = 1.0f;
	m_fMtlOffPathApproachRate = 1.0f;
	m_fMtlRecoveryApproachRate = 1.0f;

	m_ReverseSpeedScale = 1.0f;
	m_WalkSpeedScale = 1.0f;
	m_TrotSpeedScale = 1.0f;
	m_RunSpeedScale = 1.0f;
	m_GallopSpeedScale = 1.0f;

	m_WalkTurnRate = 45.0f;
	m_TrotTurnRate = 110.0f;
	m_RunTurnRate = 100.0f;
	m_GallopTurnRate = 80.0f;

}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::Tune::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("Speed Tuning", false);
	{

		b.AddToggle("InstantaneousAccel", &m_bInstantaneousAccel);
		b.AddSlider("TopSpeed", &m_TopSpeed, 0.0f, 25.0f, 0.01f);
		b.AddSlider("CollisionDecelRate", &m_fCollisionDecelRate, 0.0f, 100.0f, 0.01f);
		b.AddSlider("CollisionDecelRateFixed", &m_fCollisionDecelRateFixed, 0.0f, 100.0f, 0.01f);
		b.AddSlider("DecayDelay", &m_fDecayDelay, 0.0f, 100.0f, 0.01f);		
		b.AddSlider("AimSpeedFactor", &m_AimSpeedFactor, 0.0f, 1.0f, 0.01f);
		b.AddSlider("SpeedGainFactorDelay", &m_fSpeedGainFactorDelay, 0.0f, 100.0f, 0.01f);

		b.AddSlider("MaxSlidingSpeed", &m_fMaxSlidingSpeed, 0.0f, 1.0f, 0.01f);		

		b.PushGroup("SpeedBoost", false);
		b.AddSlider("Increase Approach Rate", &m_fSpeedBoostIncApproachRate, 0.0f, 5.0f, 0.1f);
		b.AddSlider("Decrease Approach Rate", &m_fSpeedBoostDecApproachRate, 0.0f, 5.0f, 0.1f);
		b.PopGroup();

		b.AddSlider("MaxMaintainSpeed", &m_fMaxMaintainSpeed, 0.0f, 1.0f, 0.01f);
		b.AddSlider("MinMaintainSpeed", &m_fMinMaintainSpeed, 0.0f, 1.0f, 0.01f);

		b.AddSlider("MtlWaterMult_Human", &m_fMtlWaterMult_Human, 0.0f, 1.0f, 0.1f);
		b.AddSlider("MtlWaterMult_AI", &m_fMtlWaterMult_AI, 0.0f, 1.0f, 0.1f);
		b.AddSlider("MtlWaterApproachRate", &m_fMtlWaterApproachRate, 0.0f, 10.0f, 0.1f);
		b.AddSlider("MtlOffPathMult_Human", &m_fMtlOffPathMult_Human, 0.0f, 1.0f, 0.1f);
		b.AddSlider("MtlOffPathMult_AI", &m_fMtlOffPathMult_AI, 0.0f, 1.0f, 0.1f);
		b.AddSlider("MtlOffPathApproachRate", &m_fMtlOffPathApproachRate, 0.0f, 10.0f, 0.1f);
		b.AddSlider("MtlRecoveryApproachRate", &m_fMtlRecoveryApproachRate, 0.0f, 10.0f, 0.1f);

		b.AddSlider("ReverseSpeedScale", &m_ReverseSpeedScale, 0.0f, 5.0f, 0.01f);
		b.AddSlider("WalkSpeedScale", &m_WalkSpeedScale, 0.0f, 5.0f, 0.01f);
		b.AddSlider("WalkMinSpeedRateScale", &m_WalkMinSpeedRateScale, 0.0f, 10.0f, 0.1f);
		b.AddSlider("WalkMaxSpeedRateScale", &m_WalkMaxSpeedRateScale, 0.0f, 10.0f, 0.1f);
		b.AddSlider("TrotSpeedScale", &m_TrotSpeedScale, 0.0f, 5.0f, 0.01f);
		b.AddSlider("TrotMinSpeedRateScale", &m_TrotMinSpeedRateScale, 0.0f, 10.0f, 0.1f);
		b.AddSlider("TrotMaxSpeedRateScale", &m_TrotMaxSpeedRateScale, 0.0f, 10.0f, 0.1f);
		b.AddSlider("RunSpeedScale", &m_RunSpeedScale, 0.0f, 5.0f, 0.01f);
		b.AddSlider("RunMinSpeedRateScale", &m_RunMinSpeedRateScale, 0.0f, 10.0f, 0.1f);
		b.AddSlider("RunMaxSpeedRateScale", &m_RunMaxSpeedRateScale, 0.0f, 10.0f, 0.1f);
		b.AddSlider("GallopSpeedScale", &m_GallopSpeedScale, 0.0f, 5.0f, 0.01f);
		b.AddSlider("GallopMinSpeedRateScale", &m_GallopMinSpeedRateScale, 0.0f, 10.0f, 0.1f);
		b.AddSlider("GallopMaxSpeedRateScale", &m_GallopMaxSpeedRateScale, 0.0f, 10.0f, 0.1f);
		
		b.AddSlider("WalkTurnRate", &m_WalkTurnRate, 0.0f, 180.0f, 0.1f);		
		b.AddSlider("TrotTurnRate", &m_TrotTurnRate, 0.0f, 180.0f, 0.1f);		
		b.AddSlider("RunTurnRate", &m_RunTurnRate, 0.0f, 180.0f, 0.1f);		
		b.AddSlider("GallopTurnRate", &m_GallopTurnRate, 0.0f, 180.0f, 0.1f);	

		static const float sMin = 0.0f;
		static const float sMax = 1.0f;
		static const float sDelta = 0.01f;		

		Vec3V vMinMaxDeltaX = Vec3V(sMin, sMax, sDelta);
		Vec3V vMinMaxDeltaY = Vec3V(sMin, sMax, sDelta);

		Vec4V vDefaultValue = Vec4V(V_ZERO);

		static ptxKeyframeDefn s_WidgetKBF("Keyed by Freshness", 0, PTXKEYFRAMETYPE_FLOAT2, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "SpeedBoostPerSpur", "AccelMult", NULL, NULL, false);
		static ptxKeyframeDefn s_WidgetKBS("Keyed by Speed", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "Accel", "SpeedDecay", "SpeedGainFactor", NULL, false);
		static ptxKeyframeDefn s_WidgetKBSM("Keyed by Stick Mag", 0, PTXKEYFRAMETYPE_FLOAT, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "MinSpeed", NULL, NULL, NULL, false);
		static ptxKeyframeDefn s_WidgetKBY("Keyed by Stick Y", 0, PTXKEYFRAMETYPE_FLOAT3, vDefaultValue, vMinMaxDeltaX, vMinMaxDeltaY, "MinSpeed", "SpeedDecayMult", "SpeedBoost", NULL, false);		

		b.PushGroup("Keyed by Freshness", false);
		m_KFByFreshness.SetDefn(&s_WidgetKBF);
		m_KFByFreshness.AddWidgets(b);
		b.PopGroup();

		b.PushGroup("Keyed by Speed", false);
		m_KFBySpeed.SetDefn(&s_WidgetKBS);
		m_KFBySpeed.AddWidgets(b);
		b.PopGroup();

		b.PushGroup("Keyed by Stick Mag", false);
		m_KFByStickMag.SetDefn(&s_WidgetKBSM);
		m_KFByStickMag.AddWidgets(b);
		b.PopGroup();

		b.PushGroup("Keyed by Stick Y", false);
		m_KFByStickY.SetDefn(&s_WidgetKBY);
		m_KFByStickY.AddWidgets(b);
		b.PopGroup();
	}
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////

float hrsSpeedControl::Tune::ComputeNormalizedSpeed(const float fRawSpeed, const float fTopSpeedOverride) const
{
	float fTopSpeed = (fTopSpeedOverride>=0.0f ? fTopSpeedOverride : m_TopSpeed);
	return rage::Max(0.0f, rage::ClampRange(fRawSpeed, 0.0f, fTopSpeed));
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::Tune::GetKeyframeData(const float fCurrFreshness, hrsSpeedControl::hrsState::KeyedByFreshness &kbf) const
{
	
	Vec4V vKBF = m_KFByFreshness.Query(ScalarV(fCurrFreshness));
	// "SpeedBoostPerSpur", "AccelMult"
	kbf.m_SpeedBoostPerSpur = vKBF.GetXf();
	kbf.m_AccelMult = vKBF.GetYf();
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::Tune::GetKeyframeData(const float fCurrSpeed, hrsSpeedControl::hrsState::KeyedBySpeed &kbs) const
{	
	Vec4V vKBS = m_KFBySpeed.Query(ScalarV(fCurrSpeed));
	// "Accel", "SpeedDecay" m_SpeedGainFactor
	kbs.m_Accel = vKBS.GetXf();
	kbs.m_SpeedDecay = vKBS.GetYf();
	//kbs.m_SpeedGainFactor = vKBS.GetZf();
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::Tune::GetKeyframeData(const float fCurrStickMag, hrsSpeedControl::hrsState::KeyedByStickMag &kbmag) const
{
	Vec4V vKBMag = m_KFByStickMag.Query(ScalarV(fCurrStickMag));
	//	"MinSpeed"
	kbmag.m_MinSpeed = vKBMag.GetXf();
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::Tune::GetKeyframeData(const float fCurrStickY, hrsSpeedControl::hrsState::KeyedByStickY &kby) const
{	
	Vec4V vKBY = m_KFByStickY.Query(ScalarV(fCurrStickY));
	//	"MinSpeed", "SpeedDecayMult", "SpeedBoost"
	kby.m_MinSpeed = vKBY.GetXf();
	kby.m_SpeedDecayMult = vKBY.GetYf();
	kby.m_SpeedBoost = vKBY.GetZf();
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// hrsState ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsSpeedControl::hrsState::hrsState()
: m_fTopSpeedOverride(-1.0f)
, m_fAutopilotTopSpeed(-1.0f)
, m_SprintSpeed(-1.0f)
, m_fSupressedSpurringRequirementDuration(-1.0f)
{}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::hrsState::Reset()
{
	// NOTE: do not clear top speed override here, because Reset() gets
	// called when you mount a horse.
	// m_fTopSpeedOverride	= -1.0f;
	m_fAutopilotTopSpeed = -1.0f;
	m_TgtSpeed = 0.0f;
	m_CurrSpeed = 0.0f;
	m_MvrSpeed = 0.0f;
	m_CurrAccel = 0.0f;
	m_fDecayDelayTimer = 0.0f;
	m_fMaintainSpeedDelayTimer = 0.0f;
	m_NumBadSpurs = 0;
	m_fSpeedFactor = 1.0f;
	m_fGaitSpeedScale = 1.0f;
	m_fMinGaitRateClamp = 0.8f;
	m_fMaxGaitRateClamp = 1.2f;
	m_fTurnRate = 0.0f;
	m_fCurrMtlFactor = 1.0f;
	m_fCurrSpeedBoostFactor = 1.0f;	
	m_fFollowSpeedOffset = 0.0f;
	m_bCollidedThisFrame = false;
	m_bCollidedFixedThisFrame = false;
	m_SprintSpeed = 15.0f;
	m_MaxSpeedWhileAiming = 1.0f;
	m_fSupressedSpurringRequirementDuration = -1.0f;
	m_bSliding = false;
}

////////////////////////////////////////////////////////////////////////////////

void hrsSpeedControl::hrsState::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("hrsState", false);
	b.AddSlider("Top Speed Override", &m_fTopSpeedOverride, -1.0f, 100.0f, 1.0f);
	b.AddSlider("Sprint Speed", &m_SprintSpeed, 0.0f, 0.0f, 0.0f);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE