
#ifndef HRS_SPEED_H
#define HRS_SPEED_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "Peds/Horse/horseControl.h"
#include "rmptfx/ptxkeyframe.h"
#include "parser/manager.h"
#include "phcore/materialmgr.h"

class CPed;
class hrsBrakeControl;

extern const float HORSE_MBR_STILL;
extern const float HORSE_MBR_WALK;
extern const float HORSE_MBR_TROT;
extern const float HORSE_MBR_RUN;
extern const float HORSE_MBR_GALLOP;

namespace rage
{
	class Vector2;
	class bkBank;
}

////////////////////////////////////////////////////////////////////////////////
// class hrsSpeedControl
// 
// PURPOSE: Encapsulate all speed-related tuning, state, and logic.  This
// class is used by the horse sim to determine a mount's current speed,
// based on various factors (such as current freshness and rider spurring).
//
class hrsSpeedControl : public hrsControl
{
public:
	hrsSpeedControl();

	enum hrsSpeedFlags
	{
		
		HSF_SpurredThisFrame			= BIT0,
		HSF_MaintainSpeed				= BIT1,
		HSF_Aiming						= BIT2,
		HSF_AllowSlowdownAndSpeedGain	= BIT3,
		HSF_Sliding						= BIT4,
		HSF_NoBoost						= BIT5,	// e.g. while following a target
	};

	// PURPOSE: Reset the internal state.
	void Reset();

	// PURPOSE: Initialize our internal state on mount
	// PARAMETERS:	
	//	fCurrMvrSpdMS - The horse's current speed in m/s.
	//	bZeroSpeed - Don't apply any initial speed on mounting
	void OnMount(float fCurrMvrSpdMS, const bool bZeroSpeed);

	// PURPOSE: Update the current speed and acceleration.
	// PARAMETERS:
	//	fCurrFreshness - The current freshness, normalized 0-1
	//	rMvr - The horse's mover component.
	//	fMaxSpdNorm - The maximum speed, normalized 0-1
	//	HSF_SpurredThisFrame - True if a spur happened this frame, false otherwise
	//	HSF_MaintainSpeed - True if the horse should maintain current speed
	//	vStickThrottle - The magnitude input stick
	//	rBrakeCtrl - The brake controller (should already be updated)
	//	HSF_Aiming - True if the rider is aiming, false otherwise.
	//	fDTime - Delta time
	//	speedMatchSpeed - When the player is matching speed with an AI companion, this
	//					  is the normalized target speed to maintain. Otherwise it will
	//					  be negative.
	void Update(const u32 horseSpeedFlags, const float fCurrFreshness, float fCurrMvrSpdMS,
		const float fMaxSpdNorm, const float fMBR,
		const float fTimeSinceSpur, float fWaterSpeedMax,
		const Vector2 & vStickThrottle,	const hrsBrakeControl &rBrakeCtrl, const float fDTime,
		float speedMatchSpeed);

	// PURPOSE:	Update the m_MvrSpeed value, from a current speed in m/s.
	// PARAMS:	fCurrSpdMS				- Current movement speed, in m/s
	//			alsoUpdateCurrentSpeed	- If true, m_CurrSpeed is also set to the same value as m_MvrSpeed
	// NOTES:	This is called by Updated(), but was separated out so that
	//			we can call it by itself for AI actors. /FF
	void UpdateMoverSpeed(float fCurrSpdMS, bool alsoUpdateCurrentSpeed);

	// PURPOSE: Update the animated sprint speed, which is used to calculate
	//	the normalized values.
	// PARAMETERS:
	//	fSprintSpeedMPS - the animated sprint speed, in meters per second.
	void UpdateSprintSpeed(float fSprintSpeedMPS);

	// PURPOSE: Override the maximum speed the horse is allowed while the rider aims, for GTA B* 662243.
	// PARAMETERS:
	//	spd, from 0 (stand) ->1.0 (full gallop)
	void SetMaxSpeedWhileAiming(float spd){ m_State.m_MaxSpeedWhileAiming = spd;}

	// PURPOSE:	Update the current speed and acceleration values based
	//			on the desired target speed.
	// PARAMS:	fFresh	- The current freshness, normalized 0-1
	//			fMaxSpdNorm - Max speed, normalized 0-1
	//			fDTime	- Delta time
	//			fMaxAccel - Upper limit for the acceleration
	// NOTES:	This is currently called by Update(), but was separated out so that
	//			we can call it by itself for AI actors. /FF
	void PerformAcceleration(float fFresh, const float fMaxSpdNorm, float fDTime, float fMaxAccel);

	// PURPOSE: Update the material speed multiplier (e.g., off path, in water)
	// RETURNS: The new material speed multiplier
	float UpdateMaterialSpeedMult(float fDTime, phMaterialMgr::Id groundMaterial);

	// PURPOSE: Update the speed boost multiplier (stick speed boost)
	// RETURNS: The new speed boost multiplier
	float UpdateSpeedBoostMult(float fInputSpeedBoostMult, const float fDTime);

	// PURPOSE: Update the per-horse gait speed scaling and turn rate
	// RETURNS: The gait speed scalar
	float UpdateGaitSpeedScaleAndTurnRate(float fMBR, float& fTurnRate);

	// RETURNS: The factor to apply to the target speed, based on the material
	//	the horse is on (water, or off path) -- [0 - 1]
	float GetMaterialSpeedMult() const { return m_State.m_fCurrMtlFactor; }

	// PURPOSE:	Explicitly set the current target speed value.
	// PARAMS:	fTgtSpd		- The current target speed, normalized 0-1
	void SetTgtSpeed(float fTgtSpd) { m_State.m_TgtSpeed = fTgtSpd; }

	// PURPOSE:	Added onto the normal speed boost mult (stick input etc)
	//			allows AI to catch up with player is sprinting
	// PARAMS:	fSpdBoost - 0.0f == none, 0.2 == 20% increase
	void SetTmpFollowSpeedBoost(float fSpdBoost) { m_State.m_fFollowSpeedOffset = fSpdBoost; }

	// PURPOSE: Override the tuned top speed.
	// PARAMETERS:
	//	fTopSpeedOverride - the new top speed.
	void SetTopSpeedOverride(const float fTopSpeedOverride) { m_State.m_fTopSpeedOverride=fTopSpeedOverride; }

	// PUROSE: Clear the top speed override
	void ClearTopSpeedOverride() { m_State.m_fTopSpeedOverride=-1.0f; }

	// PUROSE: Supress the spurring for a bit
	void SupressSpurringRequirement(float fTimeToSupress) { m_State.m_fSupressedSpurringRequirementDuration=fTimeToSupress; }

	void SetAutopilotSpeedOverride(const float fAutopilotSpeedOverride)
	{	m_State.m_fAutopilotTopSpeed = fAutopilotSpeedOverride;	}

	void ClearAutopilotSpeedOverride()	{ m_State.m_fAutopilotTopSpeed = -1.0f;	}

	// PURPOSE: React to a collision with a fixed bound
	// PARAMS:
	//	collisionMsg - the incoming collision message
	void HandleCollision();

	// RETURNS: The current mover speed [0-1]
	float GetCurrMoverSpeedNorm() const { return m_State.m_MvrSpeed; }
	// RETURNS: The current speed [0-1]
	float GetCurrSpeed() const { return m_State.m_CurrSpeed; }
	// RETURNS: The target speed (may differ from curr speed) [0-1]
	float GetTgtSpeed() const { return m_State.m_TgtSpeed; }
	// RETURNS: The current *adjusted* speed [0-(TopSpeed/SprintSpeed)]
	float GetCurrAdjustedSpeed() const { return GetCurrSpeed()*GetSpeedFactor(); }
	// RETURNS: The current acceleration
	float GetCurrAccel() const { return m_State.m_CurrAccel; }
	// RETURNS: The top speed, used in normalizing actual speed.
	float GetTopSpeed() const;
	// RETURNS: The normalized [0-1] speed, given a raw speed.
	float ComputeNormalizedSpeed(const float fRawSpeed) const;
	// RETURNS: True if the last spur was bad, and false otherwise.
	bool WasLastSpurBad() const { return m_State.m_bLastSpurWasBad; }
	// RETURNS: True if horse is sliding.
	bool IsSliding() const { return m_State.m_bSliding; }	
	// RETURNS: The number of "bad" spurs in the horse's short-term memory
	int GetNumBadSpurs() const { return m_State.m_NumBadSpurs; }

	void SetCurrSpeed(float s) { m_State.m_CurrSpeed = s; }

	bool HasTuning() const { return m_pTune!=NULL; }
	void AddWidgets(bkBank &b);

	// RETURNS: The factor beyond [0..1] that speed can reach
	float GetSpeedFactor() const
	{
		// Protect against nonsensical results, e.g. turn-in-place
		if( m_State.m_SprintSpeed >= 1.0f )
			return m_State.m_fSpeedFactor * (GetTopSpeed() / m_State.m_SprintSpeed);
		return 1.0f;
	}

	void SetSpeedFactor(float speedFactor)
	{
		m_State.m_fSpeedFactor = speedFactor;
	}

	float GetGaitSpeedScale() const
	{
		return m_State.m_fGaitSpeedScale;
	}

	void GetGaitRateClampRange(float& fMinRateScale, float& fMaxRateScale) const
	{
		fMinRateScale = m_State.m_fMinGaitRateClamp; 
		fMaxRateScale = m_State.m_fMaxGaitRateClamp; 
	}


	float GetSpeedBoostFactor() const
	{
		return m_State.m_fCurrSpeedBoostFactor;
	}

	float GetTurnRate() const { return m_State.m_fTurnRate;}

protected:

	struct hrsState
	{
		hrsState();
		void Reset();
		void AddWidgets(bkBank &b);

		struct KeyedByFreshness
		{
			float m_SpeedBoostPerSpur;
			float m_AccelMult;
		};
		struct KeyedBySpeed
		{
			float m_Accel;
			float m_SpeedDecay;
			//float m_SpeedGainFactor;
		};
		struct KeyedByStickMag
		{
			float m_MinSpeed;
		};
		struct KeyedByStickY
		{
			float m_MinSpeed;
			float m_SpeedDecayMult;
			float m_SpeedBoost;
		};

		float m_fTopSpeedOverride;
		float m_fAutopilotTopSpeed;
		float m_TgtSpeed;
		float m_CurrSpeed;
		float m_MvrSpeed;
		float m_CurrAccel;
		float m_fDecayDelayTimer;
		float m_fMaintainSpeedDelayTimer;
		float m_fSupressedSpurringRequirementDuration;
		float m_SprintSpeed;
		bool m_bLastSpurWasBad;
		int m_NumBadSpurs;
		float m_fSpeedFactor;
		float m_fGaitSpeedScale;
		float m_fMinGaitRateClamp;
		float m_fMaxGaitRateClamp;
		float m_fTurnRate;
		float m_fCurrMtlFactor;
		float m_fCurrSpeedBoostFactor;	
		float m_fFollowSpeedOffset;
		float m_MaxSpeedWhileAiming;
		bool m_bCollidedThisFrame;
		bool m_bCollidedFixedThisFrame;
		bool m_bSliding;
	};

public:

	struct Tune
	{
		Tune();

		// PURPOSE: Register self and internal (private) structures
		static void RegisterParser()
		{
			static bool bInitialized = false;
			if( !bInitialized )
			{
				REGISTER_PARSABLE_CLASS(hrsSpeedControl::Tune);
				bInitialized = true;
			}
		}

		void AddWidgets(bkBank &b);
		float ComputeNormalizedSpeed(const float fRawSpeed, const float fTopSpeedOverride=-1.0f) const;

		void GetKeyframeData(const float fCurrFreshness, hrsSpeedControl::hrsState::KeyedByFreshness &kbf) const;
		void GetKeyframeData(const float fCurrSpeed, hrsSpeedControl::hrsState::KeyedBySpeed &kbs) const;
		void GetKeyframeData(const float fCurrStickMag, hrsSpeedControl::hrsState::KeyedByStickMag &kbx) const;
		void GetKeyframeData(const float fCurrStickY, hrsSpeedControl::hrsState::KeyedByStickY &kby) const;

		float GetMtlWaterMult(bool isAI) const { return (isAI ? m_fMtlWaterMult_AI : m_fMtlWaterMult_Human); }
		float GetMtlOffPathMult(bool isAI) const { return (isAI ? m_fMtlOffPathMult_AI : m_fMtlOffPathMult_Human); }

		// The top speed, used in normalizing operations:
		float m_TopSpeed;
		// If true, all acceleration occurs as quickly as possible.
		bool m_bInstantaneousAccel;
		// Deceleration rate due to collisions
		float m_fCollisionDecelRate;
		// Deceleration rate due to collisions with fixed objects
		float m_fCollisionDecelRateFixed;
		// Delay after spurring (in seconds) before speed starts to decay
		float m_fDecayDelay;		
		// The speed factor when aiming
		float m_AimSpeedFactor;
		// The time delay after spurring (in seconds) before speed starts to increase
		float m_fSpeedGainFactorDelay;

		//0-1 range, caps how fast a horse can move while sliding
		float m_fMaxSlidingSpeed;

		// The approach rate when the speed boost is increasing
		float m_fSpeedBoostIncApproachRate;
		// The approach rate when the speed boost is decreasing
		float m_fSpeedBoostDecApproachRate;
		//How long must maintain speed be help before MinMaintain kicks in
		float m_fMaintainSpeedDelay;
		//At speed greater than this, speed will decay without spurs when trying to maintain speed
		float m_fMaxMaintainSpeed;
		//Horse will approach at least this speed if ped is tryign to maintain speed
		float m_fMinMaintainSpeed;

		// Material tuning
		float m_fMtlWaterMult_Human;
		float m_fMtlWaterMult_AI;
		float m_fMtlWaterApproachRate;
		float m_fMtlOffPathMult_Human;
		float m_fMtlOffPathMult_AI;
		float m_fMtlOffPathApproachRate;
		float m_fMtlRecoveryApproachRate;

		//Per gait speed tuning
		float m_ReverseSpeedScale;
		float m_WalkSpeedScale;
		float m_TrotSpeedScale;
		float m_RunSpeedScale;
		float m_GallopSpeedScale;

		float m_WalkMinSpeedRateScale;
		float m_TrotMinSpeedRateScale;
		float m_RunMinSpeedRateScale;
		float m_GallopMinSpeedRateScale;
		float m_WalkMaxSpeedRateScale;
		float m_TrotMaxSpeedRateScale;
		float m_RunMaxSpeedRateScale;
		float m_GallopMaxSpeedRateScale;

		float m_WalkTurnRate;
		float m_TrotTurnRate;
		float m_RunTurnRate;
		float m_GallopTurnRate;

		// Keyed by freshness:
		//	- SpeedBoostPerSpur
		//	- AccelMult
		ptxKeyframe m_KFByFreshness;

		// Keyed by current speed:
		//	- Accel
		//	- SpeedDecay
		ptxKeyframe m_KFBySpeed;

		// Keyed by stick magnitude:
		//	- MinSpeed
		ptxKeyframe m_KFByStickMag;

		// Keyed by stick Y:
		//	- MinSpeed
		//	- SpeedDecayMult
		ptxKeyframe m_KFByStickY;

		PAR_SIMPLE_PARSABLE;
	};

	void AttachTuning(const Tune & t) { m_pTune = &t; }
	void DetachTuning() { m_pTune = 0; }
	const Tune & GetCurrTune() const { FastAssert(m_pTune); return *m_pTune; }

protected:

	const Tune * m_pTune;
	hrsState m_State;

#if __BANK
	bool m_bSimulateOffPath;
	bool m_bSimulateInWater;
#endif
};

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE

#endif
