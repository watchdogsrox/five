
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    driverpersonality.h
// PURPOSE : Peds behave differently whilst driving.
//			 This file contains the fucntions that determone this different behaviour.
// AUTHOR :  Obbe.
// CREATED : 6/11/06
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef DRIVERPERSONALITY_H
	#define DRIVERPERSONALITY_H


class CPed;
class CVehicle;

class CDriverPersonality
{
public:
	static	float		FindDriverAbility(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		FindDriverAggressiveness(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		FindMaxAcceleratorInput(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		FindMaxCruiseSpeed(const CPed *pDriver, const CVehicle* pVehicle, const bool bIsBicycle);
	static	u32			FindDelayBeforeAcceleratingAfterLightsGoneGreen(CPed *pDriver, const CVehicle* pVehicle);
	static	u32			FindDelayBeforeAcceleratingAfterObstructionGone(const CPed *pDriver, const CVehicle* pVehicle, bool bObstructionIsPlayer=false);
	static	float		AccelerateSpeedToCatchGreenLight(CPed *pDriver, const CVehicle* pVehicle);
	static	bool		RunsAmberLights(CPed *pDriver, const CVehicle* pVehicle, u32 turnDir);
	static	bool		RunsStopSigns(const CPed *pDriver, const CVehicle* pVehicle);
	static	bool		RollsThroughStopSigns(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		GetDriverCornerSpeedModifier(const CPed *pDriver, const CVehicle* pVehicle, float fCruiseSpeed);
	static	float		GetTimeToChangeLanes(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		GetExtraStoppingDistanceForTailgating(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		GetExtraSpeedMultiplierForTailgating(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		GetStopDistanceForOtherCarsOld(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		GetDriveSlowDistanceForOtherCars(const CPed* pDriver, const CVehicle* pVehicle);
	static	float		GetStopDistanceForPeds(const CPed *pDriver, const CVehicle* pVehicle);
	static	float		GetDriveSlowDistanceForPeds(const CPed* pDriver, const CVehicle* pVehicle);
	static	float		GetStopDistanceMultiplierForOtherCars(const CPed* pDriver, const CVehicle* pVehicle);
	static	bool		WillChangeLanes(const CPed *pDriver, const CVehicle* pVehicle);
	static	bool		UsesTurnIndicatorsToTurnLeft(const CPed *pDriver, const CVehicle* pVehicle);
	static	bool		UsesTurnIndicatorsToTurnRight(const CPed *pDriver, const CVehicle* pVehicle);
	static	bool		WeavesInBetweenLanesOnBike(const CPed* pDriver, const CVehicle* pVehicle);
	static	bool		WillRespondToOtherVehiclesHonking(const CPed* pDriver, const CVehicle* pVehicle);
	static	bool		WillHonkAtStationaryVehiclesAfterGreenLight(const CPed* pDriver, const CVehicle* pVehicle);
	static	u32			FindDelayBeforeRespondingToVehicleHonk(const CPed* pDriver, const CVehicle* pVehicle);
	static	bool		IsBadAtStoppingForPeds(const CPed* pDriver, const CVehicle* pVehicle);
	static	u32			GetTimeBetweenHonkingAtPeds(const CPed* pDriver, const CVehicle* pVehicle);
};

#endif
