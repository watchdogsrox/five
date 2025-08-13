
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    driverpersonality.cpp
// PURPOSE : Peds behave differently whilst driving.
//			 This file contains the fucntions that determone this different behaviour.
// AUTHOR :  Obbe.
// CREATED : 6/11/06
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef DRIVERPERSONALITY_H
	#include "vehicleAi/driverpersonality.h"
#endif

// rage includes


// ped includes
#include "peds/ped.h"
#include "ModelInfo/PedModelInfo.h"
#include "peds/PedIntelligence.h"
#include "vehicleAi/VehicleIntelligence.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindDriverAbility
// PURPOSE :  How brave is this driver scaled from 0.0 to 1.0.
/////////////////////////////////////////////////////////////////////////////////

float CDriverPersonality::FindDriverAbility(const CPed *pDriver, const CVehicle* pVehicle)
{
	Assert(pVehicle);

	if(!pDriver)
	{
		if (pVehicle->GetIntelligence()->GetPretendOccupantAbility() >= 0.0f)
		{
			return pVehicle->GetIntelligence()->GetPretendOccupantAbility();
		}
		else
		{
			mthRandom rnd(pVehicle->GetRandomSeed());
			return rnd.GetRanged(0.1f, 0.9f);
		}
	}
	else if (pDriver->GetPedIntelligence()->GetDriverAbilityOverride() >= 0.0f)
	{
		return pDriver->GetPedIntelligence()->GetDriverAbilityOverride();
	}
	else
	{
		static const float fScale = 10.0f;
		static const float fInvScale = 1.0f / fScale;
		const u8 iDrivingAbility = pDriver->GetPedModelInfo()->GetPersonalitySettings().GetDrivingAbility(pDriver->GetRandomSeed());
		aiAssertf(iDrivingAbility <= fScale, "Driving Ability %d outside of expected range [0.0f, %4.1f], may need to be retuned", iDrivingAbility, fScale);
		return (float)iDrivingAbility * fInvScale;
	}
}

float CDriverPersonality::FindDriverAggressiveness(const CPed *pDriver, const CVehicle* pVehicle)
{
	if(!pDriver)
	{
		if (pVehicle->GetIntelligence()->GetPretendOccupantAggressiveness() >= 0.0f)
		{
			return pVehicle->GetIntelligence()->GetPretendOccupantAggressiveness();
		}
		else
		{
			mthRandom rnd(pVehicle->GetRandomSeed());
			return rnd.GetRanged(0.1f, 0.9f);
		}
	}
	else if (pDriver->GetPedIntelligence()->GetDriverAggressivenessOverride() >= 0.0f)
	{
		return pDriver->GetPedIntelligence()->GetDriverAggressivenessOverride();
	}
	else
	{
		static const float fScale = 10.0f;
		static const float fInvScale = 1.0f / fScale;
		const u8 iDrivingAggressiveness = pDriver->GetPedModelInfo()->GetPersonalitySettings().GetDrivingAggressiveness(pDriver->GetRandomSeed());
		aiAssertf(iDrivingAggressiveness <= fScale, "Driving Aggressiveness %d outside of expected range [0.0f, %4.1f], may need to be retuned", iDrivingAggressiveness, fScale);
		return (float)iDrivingAggressiveness * fInvScale;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindMaxAcceleratorInput
// PURPOSE :  How much will this driver apply the accelerator under normal conditions.
/////////////////////////////////////////////////////////////////////////////////

float CDriverPersonality::FindMaxAcceleratorInput(const CPed *pDriver, const CVehicle* pVehicle)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, ACCELERATION_MIN, 0.4f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, ACCELERATION_MAX, 1.0f, 0.0f, 1.0f, 0.01f);
	const float Bravery = FindDriverAggressiveness(pDriver, pVehicle);
	return ACCELERATION_MIN + (Bravery * (ACCELERATION_MAX-ACCELERATION_MIN));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindMaxCruiseSpeed
// PURPOSE :  How fast will this driver go under normal conditions.
/////////////////////////////////////////////////////////////////////////////////

float CDriverPersonality::FindMaxCruiseSpeed(const CPed *pDriver, const CVehicle* pVehicle, const bool bIsBicycle)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CRUISE_SPEED_MIN, 10.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CRUISE_SPEED_MAX, 20.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CRUISE_SPEED_MIN_BICYCLE, 5.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CRUISE_SPEED_MAX_BICYCLE, 8.0f, 0.0f, 100.0f, 0.01f);

	//guys will go fast if they are aggressive OR good drivers
	const float fDriverAggressiveness = FindDriverAggressiveness(pDriver, pVehicle);
	const float Bravery = fDriverAggressiveness;

	float fCruiseSpeed = 10.0f;
	if (!bIsBicycle)
	{
		fCruiseSpeed = CRUISE_SPEED_MIN + (Bravery * (CRUISE_SPEED_MAX-CRUISE_SPEED_MIN));
	}
	else
	{
		fCruiseSpeed = CRUISE_SPEED_MIN_BICYCLE + (Bravery * (CRUISE_SPEED_MAX_BICYCLE-CRUISE_SPEED_MIN_BICYCLE));
	}
	
	//aggressive drivers get an extra 20% speed boost
	if (AreNearlyEqual(fDriverAggressiveness, 1.0f))
	{
		fCruiseSpeed *= 1.2f;
	}

	//sunday drivers should really go slow
	if (pVehicle && pVehicle->m_nVehicleFlags.bSlowChillinDriver)
	{
		fCruiseSpeed *= 0.6f;
	}

	return fCruiseSpeed;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindDelayBeforeAcceleratingAfterLightsGoneGreen
// PURPOSE :  How many milliseconds will this guy wait before pulling away from a light
/////////////////////////////////////////////////////////////////////////////////

u32 CDriverPersonality::FindDelayBeforeAcceleratingAfterLightsGoneGreen(CPed *pDriver, const CVehicle* pVehicle)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_GREEN_LIGHT_MIN, -1.5f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_GREEN_LIGHT_MAX, 1.5f, -10.0f, 10.0f, 0.01f);

	float Bravery = FindDriverAggressiveness(pDriver, pVehicle);
	float invBravery = 1.0f - Bravery;

	float fWait = DELAY_AFTER_GREEN_LIGHT_MIN + (invBravery * (DELAY_AFTER_GREEN_LIGHT_MAX - DELAY_AFTER_GREEN_LIGHT_MIN));
	u32	Wait = (u32)rage::Max((fWait * 1000.0f), 0.0f);

	// Granny drivers will sometimes wait 3 full seconds to get going.
	if (Bravery < 0.1f && ((fwRandom::GetRandomNumber() & 3) == 0) )
	{
		Wait = 3000;
	}

	return Wait;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindDelayBeforeAcceleratingAfterObstructionGone
// PURPOSE :  How many milliseconds will this guy wait before pulling away after
//				a car or ped has gotten out of his way
/////////////////////////////////////////////////////////////////////////////////

u32 CDriverPersonality::FindDelayBeforeAcceleratingAfterObstructionGone(const CPed *pDriver, const CVehicle* pVehicle, bool bObstructionIsPlayer)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_OBSTRUCTION_GONE_MIN, -1.0f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_OBSTRUCTION_GONE_MAX, 1.0f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_PLAYER_OBSTRUCTION_GONE_MIN, 3.5f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_PLAYER_OBSTRUCTION_GONE_MAX, 5.25f, -10.0f, 10.0f, 0.01f);

	float Bravery = FindDriverAggressiveness(pDriver, pVehicle);
	float invBravery = 1.0f - Bravery;

	float fWait = bObstructionIsPlayer ? DELAY_AFTER_PLAYER_OBSTRUCTION_GONE_MIN + (invBravery * (DELAY_AFTER_PLAYER_OBSTRUCTION_GONE_MAX - DELAY_AFTER_PLAYER_OBSTRUCTION_GONE_MIN))
		: DELAY_AFTER_OBSTRUCTION_GONE_MIN + (invBravery * (DELAY_AFTER_OBSTRUCTION_GONE_MAX - DELAY_AFTER_OBSTRUCTION_GONE_MIN));
	u32	Wait = (u32)rage::Max((fWait * 1000.0f), 0.0f);

	// Granny drivers will sometimes wait 3 full seconds to get going.
	if (Bravery < 0.1f && ((fwRandom::GetRandomNumber() & 3) == 0) )
	{
		Wait = rage::Max((u32)3000, Wait);
	}

	return Wait;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AccelerateSpeedToCatchGreenLight
// PURPOSE :  Some drivers will accelerate to catch an amber light.
/////////////////////////////////////////////////////////////////////////////////

float CDriverPersonality::AccelerateSpeedToCatchGreenLight(CPed *pDriver, const CVehicle* pVehicle)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, ACCELERATION_TO_CATCH_GREEN_LIGHT_MIN, -10.0f, -100.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, ACCELERATION_TO_CATCH_GREEN_LIGHT_MAX, 25.0f, -100.0f, 100.0f, 0.01f);
	float Bravery = FindDriverAggressiveness(pDriver, pVehicle);

	float	Accel = ACCELERATION_TO_CATCH_GREEN_LIGHT_MIN + (Bravery * ACCELERATION_TO_CATCH_GREEN_LIGHT_MAX);

// 	if (Accel > 0.0f)
// 	{
// 		grcDebugDraw::Sphere(pDriver->GetTransform().GetPosition(), 1.5f, Color_orange, false);
// 	}

	Accel = rage::Max(0.0f, Accel);

	return (Accel);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RunsAmberLights
// PURPOSE :  Some drivers will run straight through amber lights.
/////////////////////////////////////////////////////////////////////////////////

bool CDriverPersonality::RunsAmberLights(CPed *pDriver, const CVehicle* pVehicle, u32 turnDir)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, RUNS_AMBER_LIGHTS_STRAIGHT, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, RUNS_AMBER_LIGHTS_RIGHT, 0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, RUNS_AMBER_LIGHTS_LEFT, 0.1f, 0.0f, 1.0f, 0.01f);
	float Bravery = FindDriverAggressiveness(pDriver, pVehicle);

	float fRunsAmberLightsThreshold = RUNS_AMBER_LIGHTS_STRAIGHT;
	if (turnDir == BIT_TURN_LEFT)
	{
		fRunsAmberLightsThreshold = RUNS_AMBER_LIGHTS_LEFT;
	}
	else if (turnDir == BIT_TURN_RIGHT)
	{
		fRunsAmberLightsThreshold = RUNS_AMBER_LIGHTS_RIGHT;
	}

	return (Bravery >= fRunsAmberLightsThreshold);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RunsStopSigns
// PURPOSE :  Some drivers will run straight through stop signs.
/////////////////////////////////////////////////////////////////////////////////

bool CDriverPersonality::RunsStopSigns(const CPed *pDriver, const CVehicle* pVehicle)
{
	//TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, RUNS_STOP_SIGNS, 0.9f, 0.0f, 1.0f, 0.01f);
	//float Bravery = FindDriverAggressiveness(pDriver);

	//return (Bravery >= RUNS_STOP_SIGNS);

	return pDriver && pVehicle && pVehicle->m_nVehicleFlags.bMadDriver ? true : false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RollsThroughStopSigns
// PURPOSE :  Some drivers will roll through stop signs without completely stopping.  "California Roll"
/////////////////////////////////////////////////////////////////////////////////

bool CDriverPersonality::RollsThroughStopSigns(const CPed *pDriver, const CVehicle* pVehicle)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, ROLLING_STOP_FOR_STOP_SIGNS, 0.6f, 0.0f, 1.0f, 0.01f);
	//float Bravery = FindDriverAggressiveness(pDriver, pVehicle);
	const float fAbility = FindDriverAbility(pDriver, pVehicle);
	const float fAggressiveness = FindDriverAggressiveness(pDriver, pVehicle);

	return (fAggressiveness >= ROLLING_STOP_FOR_STOP_SIGNS && fAbility < ROLLING_STOP_FOR_STOP_SIGNS);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetDriverCornerSpeedModifier
// PURPOSE :  Returns a scale based on the driver profile to adjust cornering speeds
/////////////////////////////////////////////////////////////////////////////////
float CDriverPersonality::GetDriverCornerSpeedModifier(const CPed* /*pDriver*/, const CVehicle* /*pVehicle*/, float fCruiseSpeed)
{
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CORNER_SPEED_MIN, 0.6f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CORNER_SPEED_MAX, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CRUISE_TO_AGGRESSION_MIN, 16.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, CRUISE_TO_AGGRESSION_MAX, 25.0f, 0.0f, 100.0f, 0.01f);
	float fAggresiveness = Clamp((fCruiseSpeed - CRUISE_TO_AGGRESSION_MIN)/(CRUISE_TO_AGGRESSION_MAX-CRUISE_TO_AGGRESSION_MIN), 0.0f, 1.0f); // Hacked until I can rework the calculations
	//float fAggresiveness = FindDriverAggressiveness(pDriver);
	return CORNER_SPEED_MIN + (fAggresiveness * (CORNER_SPEED_MAX - CORNER_SPEED_MIN));
}

float CDriverPersonality::GetTimeToChangeLanes(const CPed *pDriver, const CVehicle* pVehicle)
{
	const float Aggression = FindDriverAggressiveness(pDriver, pVehicle);

	//TODO: maybe use some kind of gaussian distribution here?

	if (Aggression > 0.75f)
	{
		return 6.0f;
	}
	else if (Aggression > 0.35f)
	{
		return 7.0f;
	}
	else
	{
		return 8.0f;
	}
}

float CDriverPersonality::GetExtraStoppingDistanceForTailgating(const CPed* pDriver, const CVehicle* pVehicle)
{
	const float Aggression = FindDriverAggressiveness(pDriver, pVehicle);

	u32 nRandomSeed = 0;
	//intentionally check vehicle before driver so we dont' change
	//results when pretend occupants stream in/out
	if (pVehicle)
	{
		nRandomSeed = pVehicle->GetRandomSeed();
	}
	else if (pDriver)
	{
		nRandomSeed = pDriver->GetRandomSeed();
	}

	mthRandom rnd(nRandomSeed);
	const float fNoise = rnd.GetGaussian(0.0f, 0.5f);

	if (Aggression > 0.75f)
	{
		//no noise for this case
		return 0.0f;
	}
	else if (Aggression > 0.35f)
	{
		return 1.5f + fNoise;
	}
	else if (Aggression > 0.1f)
	{
		return 3.0f + fNoise;
	}
	else
	{
		return 6.0f + fNoise;
	}
}

float CDriverPersonality::GetExtraSpeedMultiplierForTailgating(const CPed *pDriver, const CVehicle* pVehicle)
{
	const float Ability = FindDriverAbility(pDriver, pVehicle);

	u32 nRandomSeed = 0;
	//intentionally check vehicle before driver so we dont' change
	//results when pretend occupants stream in/out
	if (pVehicle)
	{
		nRandomSeed = pVehicle->GetRandomSeed();
	}
	else if (pDriver)
	{
		nRandomSeed = pDriver->GetRandomSeed();
	}

	mthRandom rnd(nRandomSeed);
	const float fNoise = rnd.GetGaussian(0.0f, 0.05f);

	if (Ability > 0.75f)
	{
		return 0.4f + fNoise;
	}
	else if (Ability > 0.35f)
	{
		return 0.2f + fNoise;
	}
	else
	{
		return 0.0f;
	}
}

float CDriverPersonality::GetStopDistanceForOtherCarsOld(const CPed* pDriver, const CVehicle* pVehicle)
{
	//TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, StopDistForOtherCars, 1.5f, 0.0f, 100.0f, 0.1f);

	float Bravery = FindDriverAggressiveness(pDriver, pVehicle);
	if (Bravery > 0.6f)
	{
		return 1.5f;
	}
	else if (Bravery > 0.3f)
	{
		return 2.0f;
	}
	else if (Bravery > 0.1f)
	{
		return 3.0f;
	}
	else
	{
		//granny drivers
		return 4.0f;
	}
}

float CDriverPersonality::GetDriveSlowDistanceForOtherCars(const CPed* pDriver, const CVehicle* pVehicle)
{
	//TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, SlowDistForOtherCars, 4.5f, 0.0f, 100.0f, 0.1f);

	float Bravery = FindDriverAggressiveness(pDriver, pVehicle);
	if (Bravery > 0.6f)
	{
		return 4.5f;
	}
	else if (Bravery > 0.3f)
	{
		return 6.5f;
	}
	else
	{
		return 7.5f;
	}
}

float CDriverPersonality::GetStopDistanceForPeds(const CPed* pDriver, const CVehicle* pVehicle)
{
	//TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, StopDistForPeds, 2.5f, 0.0f, 100.0f, 0.1f);

	//guys will stop short if they are really bad drivers, or really timid
	float Bravery = Min(FindDriverAggressiveness(pDriver, pVehicle), FindDriverAbility(pDriver, pVehicle));
	if (Bravery > 0.6f)
	{
		return 2.5f;
	}
	else if (Bravery > 0.3f)
	{
		return 3.0f;
	}
	else if (Bravery > 0.1f)
	{
		return 4.5f;
	}
	else
	{
		//granny drivers
		return 5.5f;
	}
}

float CDriverPersonality::GetDriveSlowDistanceForPeds(const CPed* pDriver, const CVehicle* pVehicle)
{
	//TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, SlowDistForPeds, 4.0f, 0.0f, 10.0f, 0.1f);

	//guys will stop short if they are really bad drivers, or really timid
	float Bravery = Min(FindDriverAggressiveness(pDriver, pVehicle), FindDriverAbility(pDriver, pVehicle));
	if (Bravery > 0.6f)
	{
		return 4.0f;
	}
	else if (Bravery > 0.3f)
	{
		return 5.0f;
	}
	else
	{
		return 7.5f;
	}
}

//we might try and stop for a ped but not do it in time
bool CDriverPersonality::IsBadAtStoppingForPeds(const CPed* pDriver, const CVehicle* pVehicle)
{
	const float fAbility = FindDriverAbility(pDriver, pVehicle);

	return fAbility < 0.25f;
}

float CDriverPersonality::GetStopDistanceMultiplierForOtherCars(const CPed* pDriver, const CVehicle* pVehicle)
{
	u32 nRandomSeed = 0;
	//intentionally check vehicle before driver so we dont' change
	//results when pretend occupants stream in/out
	if (pVehicle)
	{
		nRandomSeed = pVehicle->GetRandomSeed();
	}
	else if (pDriver)
	{
		nRandomSeed = pDriver->GetRandomSeed();
	}

	float fMultiplier = 1.0f;

	mthRandom rnd(nRandomSeed);

	float Bravery = FindDriverAggressiveness(pDriver, pVehicle);
	if (Bravery >= 0.9f)
	{
		//super badass drivers
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_VERY_AGGRESSIVE_MIN, 0.2f, 0.0f, 2.0f, 0.05f);
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_VERY_AGGRESSIVE_MAX, 0.4f, 0.0f, 2.0f, 0.05f);
		fMultiplier = MULTIPLIER_VERY_AGGRESSIVE_MIN + (rnd.GetFloat() * (MULTIPLIER_VERY_AGGRESSIVE_MAX - MULTIPLIER_VERY_AGGRESSIVE_MIN));
	}
	else if (Bravery > 0.65f)
	{
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_AGGRESSIVE_MIN, 0.6f, 0.0f, 2.0f, 0.05f);
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_AGGRESSIVE_MAX, 0.8f, 0.0f, 2.0f, 0.05f);
		fMultiplier = MULTIPLIER_AGGRESSIVE_MIN + (rnd.GetFloat() * (MULTIPLIER_AGGRESSIVE_MAX - MULTIPLIER_AGGRESSIVE_MIN));
	}
	else if (Bravery > 0.4f)
	{
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_NORMAL_MIN, 0.8f, 0.0f, 2.0f, 0.05f);
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_NORMAL_MAX, 1.3f, 0.0f, 2.0f, 0.05f);
		fMultiplier = MULTIPLIER_NORMAL_MIN + (rnd.GetFloat() * (MULTIPLIER_NORMAL_MAX - MULTIPLIER_NORMAL_MIN));
	}
	else if (Bravery > 0.1f)
	{
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_CAUTIOUS_MIN, 1.3f, 0.0f, 2.0f, 0.05f);
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_CAUTIOUS_MAX, 1.9f, 0.0f, 2.0f, 0.05f);
		fMultiplier = MULTIPLIER_CAUTIOUS_MIN + (rnd.GetFloat() * (MULTIPLIER_CAUTIOUS_MAX - MULTIPLIER_CAUTIOUS_MIN));
	}
	else
	{
		//granny drivers
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_VERY_CAUTIOUS_MIN, 1.9f, 0.0f, 2.0f, 0.05f);
		TUNE_GROUP_FLOAT(DP_STOP_DISTANCE_FOR_OTHER_CARS, MULTIPLIER_VERY_CAUTIOUS_MAX, 2.1f, 0.0f, 2.0f, 0.05f);
		fMultiplier = MULTIPLIER_VERY_CAUTIOUS_MIN + (rnd.GetFloat() * (MULTIPLIER_VERY_CAUTIOUS_MAX - MULTIPLIER_VERY_CAUTIOUS_MIN));
	}

	return fMultiplier;
}

bool CDriverPersonality::WillChangeLanes(const CPed *pDriver, const CVehicle* pVehicle)
{
	float Aggressiveness = FindDriverAggressiveness(pDriver, pVehicle);

	return Aggressiveness > 0.75f;
}

bool CDriverPersonality::UsesTurnIndicatorsToTurnLeft(const CPed *pDriver, const CVehicle* pVehicle)
{
	float fAbility = FindDriverAbility(pDriver, pVehicle);

	return fAbility > 0.25f;
}

bool CDriverPersonality::UsesTurnIndicatorsToTurnRight(const CPed *pDriver, const CVehicle* pVehicle)
{
	float fAbility = FindDriverAbility(pDriver, pVehicle);

	return fAbility > 0.5f;
}

bool CDriverPersonality::WeavesInBetweenLanesOnBike(const CPed* pDriver, const CVehicle* pVehicle)
{
	float fAggressiveness = FindDriverAggressiveness(pDriver, pVehicle);

	//TODO: re-enable this
	return fAggressiveness > 1.0f; //0.25f;
}

bool CDriverPersonality::WillRespondToOtherVehiclesHonking(const CPed* pDriver, const CVehicle* pVehicle)
{
	float fBravery = Max(FindDriverAggressiveness(pDriver, pVehicle), FindDriverAbility(pDriver, pVehicle));

	return fBravery < 0.5f;
}

bool CDriverPersonality::WillHonkAtStationaryVehiclesAfterGreenLight(const CPed* pDriver, const CVehicle* pVehicle)
{
	const float fAbility = FindDriverAbility(pDriver, pVehicle);
	const float fAggressiveness = FindDriverAggressiveness(pDriver, pVehicle);

	return fAggressiveness > 0.5f && fAbility < 0.5f;
}

u32 CDriverPersonality::FindDelayBeforeRespondingToVehicleHonk(const CPed* pDriver, const CVehicle* pVehicle)
{
	float fAbility = FindDriverAbility(pDriver, pVehicle);

	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_HONKED_AT_MIN, -1.5f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, DELAY_AFTER_HONKED_AT_MAX, 1.5f, -10.0f, 10.0f, 0.01f);

	float invAbility = 1.0f - fAbility;

	float fWait = DELAY_AFTER_HONKED_AT_MIN + (invAbility * (DELAY_AFTER_HONKED_AT_MAX - DELAY_AFTER_HONKED_AT_MIN));
	u32	Wait = (u32)rage::Max((fWait * 1000.0f), 0.0f);

	// Granny drivers will sometimes wait 2 full seconds to get going.
	if (fAbility < 0.1f && ((fwRandom::GetRandomNumber() & 3) == 0) )
	{
		Wait = 2000;
	}

	return Wait;
}

u32 CDriverPersonality::GetTimeBetweenHonkingAtPeds(const CPed* pDriver, const CVehicle* pVehicle)
{
	const float fAggressiveness = FindDriverAggressiveness(pDriver, pVehicle);

	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, TIME_BTWN_HONKS_AT_PEDS_MIN, 3.0f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DRIVER_PERSONALITY, TIME_BTWN_HONKS_AT_PEDS_MAX, 6.0f, -10.0f, 10.0f, 0.01f);

	const float fIntAggression = 1.0f - fAggressiveness;

	const float fWait = TIME_BTWN_HONKS_AT_PEDS_MIN + (fIntAggression * (TIME_BTWN_HONKS_AT_PEDS_MAX - TIME_BTWN_HONKS_AT_PEDS_MIN));
	const u32	Wait = (u32)rage::Max((fWait * 1000.0f), 0.0f);

	return Wait;
}