
#include "Vehicles\VehicleFlags.h"


void CVehicleFlags::Init()
{
	bInfluenceWantedLevel = true;

	bIsAmbulanceOnDuty = false;
	bIsFireTruckOnDuty = false;
	bCannotRemoveFromWorld = false;
	bEngineOn = false;			// Engine off initially.
	bEngineStarting = false;
	bEngineWontStart = false;
	bLightsOn = false;
	bFreebies = true;

	bIsVan = false;
	bIsBig = false;
	bIsBus = false;
	bLowVehicle = false;
	bWarnedPeds = false;
	bHasDummyBound = false;

	bTakeLessDamage = false;
	bHasBeenOwnedByPlayer = false;
	bCarNeedsToBeHotwired = true;

	bVehicleColProcessed = false;
	bPartOfConvoy = false;
	bIsDrowning = false;

	bTyresDontBurst = false;
	bWheelsDontBreak = false;
	bRestingOnPhysical = false;
	bDontSleepOnThisPhysical = false;
	bWakeUpNextUpdate = false;
	bPreventBeingDummyThisFrame = false;
	bPreventBeingSuperDummyThisFrame = false;
	bPreventBeingDummyUnlessCollisionLoadedThisFrame = false;
	bTasksAllowDummying = false;
	bNoDummyPathAvailable = false;
	bDummyWheelImpactsSampled = false;
	bNeverUseSmallerRemovalRange = false;
	bActAsIfHighSpeedForFragSmashing = false;
	bMayHaveWheelGroundForcesToApply = false;
	bHasParentVehicle = false;
	bHasProcessedAIThisFrame = false;
	uAllowedDummyConversion = 0;
	bCarCrushingBike = false; 

	bVehicleCanBeTargetted = false;
	bIsATargetPriority = false;
	bTargettableWithNoLos = false;

	SetSirenOn(false);
	SetRandomlyDelaySiren(false);
	SetSirenMutedByScript(false);
	SetSirenMutedByCode(false);

	bApproachingJunctionWithSiren = false;
	bPreviousApproachingJunctionWithSiren = false;
	bDoesProvideCover = true;
	bMadDriver = false;
	bSlowChillinDriver = false;
	bConsideredByPlayer = true;
	bDisableParticles = false;
	bHasBeenResprayed = false;
	bUseAlternateHandling = false;
	bStealable = true;
	bLockDoorsOnCleanup = false;
	bForceInactiveDuringPlayback = false;
	bForceActiveDuringPlayback = false;

	bCarAgainstCarSideCollision = false;
	bCarBrushAgainstCarSideCollision = false;
	bCarHitByHeavyVehicle = false;
	bCarHitBySuperHeavyVehicle = false;

	bShouldFixIfNoCollision = false;
	bPetrolTankEmpty = false;

	bIsAsleep = false;
	bLeftIndicator = false;
	bRightIndicator = false;
	bHazardLights = false;
	bIndicatorLights = false;
	bInteriorLightOn = false;
	bForceBrakeLightOn = false;
	bSuppressBrakeLight = false;
	bWanted			= false;
	bHasDynamicNavMesh = false;
	bIsParkedParallelly = false;
	bIsParkedPerpendicularly = false;
	bShouldBeRemovedByAmbientPed = false;
	bCountAsParkedCarForPopulation = false;
	bDontStoreAsPersistent = false;
	bIsParkedAtPetrolStation = false;
	bCantTrafficJam = false;
	bVehicleIsOnscreen = true;
	bTryToRemoveAggressively = false;
	bCanBeDeletedWithAlivePedsInIt = false;
	bIsInCluster = false;
	bIsInClusterBeingSpawned = false;
	bCanBeUsedByFleeingPeds = true;
	bCreatedByLowPrioCargen = false;
	bRemoveWithEmptyCopOrWreckedVehs = false;
	bCreatedByCarGen = false;
	bHasBeenDriven = false;
	bDisableTowing = false;
	bIsWaitingToTurnLeft = false;
	bRepairWhenSafe = false;
	bWillTurnLeftAgainstOncomingTraffic = false;
	bTurningLeftAtJunction = false;
	bCanMakeIntoDummyVehicle = true;
	bUsingPretendOccupants = false;
	bIsCargoVehicle = false;
	bSuperDummyProcessed = false;
	bLodFarFromPopCenter = false;
	bLodSuperDummyWheelCompressionSet = false;
	bLodForceUpdateThisTimeslice = false;
	bLodForceUpdateUntilNextAiUpdate = false;
	bLodCanUseTimeslicing = false;
	bLodShouldSkipUpdatesInTimeslicedLod = false;
	bLodShouldUseTimeslicing = false;
	bLodStrictRoadProximityTest = false;
	bIsDeactivatedByPlayback = false;
	bMayBecomeParkedSuperDummy = true;
	bIsBeingTowed = false;
	bMoreAccurateVirtualRoadProbes = false;
	bWasOffroadWithConstraint = false;
	bCheckForMisalignmentOnDeactivation = false;

	bPoliceFocusWillTrackCar = false;
	bScheduledForCreation = false;
	bAlertWhenEntered = false;
	//bCreatedInMechanicCargen = false;

	bHasCoolingFan = false;
	bIsCoolingFanOn = false;
	bBlipThrottleRandomly = false;
	bMoveAwayFromPlayer = false;
	//bCopperBlockedCouldLeaveCar = false;
	bHideInCutscene = true;  // all (mission) vehicles are hidden in cutscene as default
	bNotDriveable = false;
	bAllowHomingMissleLockon = true; // all vehicles can be targetted by default
	bAllowNoPassengersLockOn = false; // all vehicles can don't allow lock on with no passengers by default.
	bForcePostCameraAiSecondaryTaskUpdate = false;
	bForcePostCameraAnimUpdate = false;
	bForcePostCameraAnimUpdateUseZeroTimeStep = false;
	bAnimateWheels = false; 
	bAnimatePropellers = false; 
    bDriveMusclesToAnim = false;
	bAnimateJoints = false; 
	bUseCutsceneWheelCompression = false;
	bCountsTowardsAmbientPopulation = false;
	bWasCreatedByDispatch = false;
	bIsLawEnforcementVehicle = false;
	bIsLawEnforcementCar = false;
	bReportCrimeIfStandingOn = false;
	bIsThisAParkedCar = false;
	bIsThisAStationaryCar = false;
	bIsThisADriveableCar = true;
	bForceOtherVehiclesToStopForThisVehicle = false;
	bForceOtherVehiclesToOvertakeThisVehicle = false;
	bIsOvertakingStationaryCar = false;
	bBecomeStationaryQuicker = false;
	bDrivingOnVehicle = false;
	bProducingSlipStream = false;
	bUpdateProducingSlipStream = false;
	bSlipStreamDisabledByTimeOut = false;
	bRestoreVelAfterCollLoads = false;
	bRestorePlaybackVelAfterCollLoads = false;
	bDisableHeightMapAvoidance = false;
	bMayHavePedDrivingDoorFlagSet = false;

	bUseDeformation = false;
	bCanBeVisiblyDamaged = true;
	bMissionVehToughAxles = false;
	bGangVeh = false;
	bMercVeh = false;
	bInSlownessZone = false;

	bDisplayHighDetail = false;
	bShouldBeBeepedAt = false;
	bPlayerDrivingAgainstTraffic = false;

	bLightsButtonHeldDown = false;
	bLightsAllowFullBeamSwitch = true;

	bAudioBackfired = false;
	bAudioBackfiredBanger = false;
	bHeadlightsFullBeamOn = false;
	bCreatedUsingCheat = false;
	bPlayerHasTurnedOffLowBeamsAtNight = false;
	bRoofLowered = false;

	bSkipEngineStartup = false;
	bShouldNotChangeColour = false;

	bExplodesOnHighExplosionDamage = true;
	bExplodesOnNextImpact = false;
	bPlaneResistToExplosion = false;
	bDisableExplodeOnContact = false;
	bInMotionFromExplosion = false;
	bActAsIfHasSirenOn = false;
	bUnbreakableLandingGear = false;

	bCanEngineMissFire = true;
	bCanLeakPetrol = false;
	bCanLeakOil = false;
    bCanPlayerCarLeakPetrol = true;
    bCanPlayerCarLeakOil = true;
	bDisablePetrolTankFires = false;
	bDisablePetrolTankDamage = false;
	bDisableEngineFires = false;
	bLimitSpeedWhenPlayerInactive = true;
	bStopInstantlyWhenPlayerInactive = true;
	bUnfreezeWhenCleaningUp = true;
	bDontTryToEnterThisVehicleIfLockedForPlayer = false;
	bDisablePretendOccupants = false;
	bUnbreakableLights = false;
    bRespectLocksWhenHasDriver = false;
	bForceEngineDamageByBullet = false;
	bGeneratesEngineShockingEvents = true;
	bCanEngineDegrade = true;
	bCanPlayerAircraftEngineDegrade = true;
	bCanDeformWheels = true;
	bCanBreakOffWheelsWhenBlowUp = true;
	bIsStolen = false;
	bCanBeStolen = true;
	bVehicleInaccesible = false;
	bUsedForPilotSchool = false;
	bActiveForPedNavigation = true;
	bKeepEngineOnWhenAbandoned = false;
	bUseScriptedCeilingHeight = false;
	bUsesLargeRearRamp = false;
	bForceAfterburnerVFX = false;

	bWheelShapetest = false;
	bPlaceOnRoadQueued = false;
    bAutomaticallyAttaches = true;
	bScansWithNonPlayerDriver = false;

	bFailedToResetPretendOccupants = false;

	bIsFarDrawEnabled = false;
	bRequestDrawHD = false;
	bRefreshVisibility = false;

	bOtherCarsShouldCheckTrajectoryBeforeSpawning = false;
	bAvoidanceDirtyFlag = false;
	bNonAmbientVehicleMayBeUsedByGoToPointAnyMeans = false;
	bIsRacing = false;
	bIsInRecording = false;
	bHasWheelOnPavement = false;
	bIsInTunnel = false;
	bScriptSetHandbrake = false;
	bSwitchToAiRecordingThisFrame = false;
	bLerpToFullRecording = false;
	bShouldLerpFromAiToFullRecording = false;
	bIsDoingHandBrakeTurn = false;
	bDisableAvoidanceProjection = false;
	bForceEntityScansAtHighSpeeds = false;
	bIsRacingConservatively = false;
	bToldToGetOutOfTheWay = false;
	bTellOthersToHurry = false;
	bDisableVehicleMapCollision = false;
	bIsInRoadBlock = false;
	bHasBeenSeen = false;
	bIsInVehicleReusePool = false;
	bIsHesitating = false;
	BANK_ONLY(bWasReused = false;)

	bDamageTrackingLocked = false;

	bSyncPopTypeOnMigrate = false;
	bBlownUp = false;

	bAllowBoundsToBeRaised = false;
	bTowedVehBoundsCanBeRaised = false;
	bIsNitrousBoostActive = false;
	bIsKERSBoostActive = false;
	bAICanUseExclusiveSeats = false;
	bAreHydraulicsAllRaised = false;
	bPlayerModifiedHydraulics = false;
	bForceEnemyPedsFalloff = false;
	bBlowUpWhenMissingAllWheels = false;
	bWasStoppedForDoor = false;
	bRearDoorsHaveBeenBlownOffByStickybomb = false;
	bDisableSuperDummy = false;
	bCheckForEnoughRoomToFitPed = false;
	bIsUnderMagnetControl = false;
	bDisableRoadExtentsForAvoidance = false;
	bReadyForCleanup = false;

#if ENABLE_FRAG_OPTIMIZATION
	bHasFragCacheEntry = false;
	bLockFragCacheEntry = false;
#endif
}

void Reset(CVehicle* UNUSED_PARAM(pVehicle))
{

}




