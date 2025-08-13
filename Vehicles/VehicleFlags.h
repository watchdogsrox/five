// 21/07/2015:	DO NOT ADD NEW VEHICLE FLAGS!!! ADDING THEM WILL CAUSE THE REPLAY VEHICLE FLAGS TO GET OUT OF SYNC. PLEASE ADD ELSEWHERE.

#ifndef _VEH_FLAGS_H_
#define _VEH_FLAGS_H_

#include "fragment/type.h"

class CVehicle;

class CVehicleFlags
{
#if __BANK
	friend class CVehicleIntelligence;
	friend class CVehicleFlagDebugInfo;
#endif

public:
	void Init();
	void Reset();

	inline bool GetIsSirenOn() const { return (bHasSiren&&bSiren&&!bRandomlyDelaySiren); }
	inline void SetSirenOn( bool bOn, bool bRandomDelay = true ) { if(bHasSiren||!bOn) { bRandomlyDelaySiren = !bSiren&&bOn&&bRandomDelay; bSiren = bOn; } }
	inline void SetSirenUsesEarlyFade(bool b) { bSirenUsesEarlyFade = b; }
	inline bool GetSirenUsesEarlyFade() const { return bSirenUsesEarlyFade; }
	inline void SetSirenUseNoSpec(bool b) { bSirenUseNoSpec = b; }
	inline bool GetSirenUseNoSpec() const { return bSirenUseNoSpec; }
	inline void SetSirenUseNoLights(bool b) { bSirenUseNoLights = b; }
	inline bool GetSirenUseNoLights() const { return bSirenUseNoLights; }
	inline void SetSirenUsesDayNightFade(bool b) { bSirenUsesDayNightFade = b; }
	inline bool GetSirenUsesDayNightFade() const { return bSirenUsesDayNightFade; }
	bool GetRandomlyDelaySiren() const { return bRandomlyDelaySiren; }
	void SetRandomlyDelaySiren(u8 val) { bRandomlyDelaySiren = val; }

	// Siren mute flag now used by script...  If you need a code side, please work it in either by
	// another flag or through the audio entity.
	void SetSirenMutedByScript(bool val) { bSirenMutedByScript = val; }
	void SetSirenMutedByCode(bool val) { bSirenMutedByCode = val; }
	bool GetIsSirenAudioOn() const { return (!bSirenMutedByScript && !bSirenMutedByCode && (bHasSiren&&bSiren&&!bRandomlyDelaySiren)); }

public:
	u8	bDisplayHighDetail				: 1;	// want to display the high detail model associated with this model (not the actual model)
	//  - MUST BE FIRST FOR SPU (ugh)

//////////////////////////////////////////////////////////////////////////
// config state
public:
	u8	bCreatedByFactory			: 1;	// Set true if car has been created using a vehicle factory
	u8	bNeverUseSmallerRemovalRange: 1;	// Some vehicles (like planes) we don't want to remove just behind the camera.
	u8	bCreatedUsingCheat			: 1;	// Player has created this vehicle using a cheat. (Should be tidied away next time cheat is used)
	u8	bGangVeh					: 1;	// This is a gang car and gang member should be created inside (gangPopGroup obtained from CVehicleModelInfo)
	u8  bMercVeh					: 1;	// This is a vehicle belonging to a mercanary

	u8	bIsVan						: 1;	// Is this vehicle a van (doors at back of vehicle)
	u8	bIsBus						: 1;	// Is this vehicle a bus
	u8	bIsBig						: 1;	// Is this vehicle big
	u8	bLowVehicle					: 1;	// Need this for sporty type cars to use low getting-in/out anims
	u8	bHasDummyBound				: 1;	// Does this vehicle have a dummy bound

	u8	bWasCreatedByDispatch		: 1;	// Tells us if this vehicle was created by a dispatch system
	u8	bIsLawEnforcementVehicle	: 1;	// Same result as old IsLawEnforcementVehicle() function but faster.
	u8	bIsLawEnforcementCar		: 1;	// Same result as old IsLawEnforcementCar() function but faster.
	u8  bReportCrimeIfStandingOn	: 1;	// Crime reporting for standing on
	u8	bIsThisAParkedCar			: 1;	// Same result as old IsThisAParkedCar() function but faster.
	u8	bIsThisAStationaryCar		: 1;	// Same result as old IsThisAStationaryCar() function but faster. Note: NOT an accurate indicator of 
											// the vehicle's velocity (moving vehicles CAN have this flag set to true in certain situations)

	u8	bIsThisADriveableCar		: 1;	// Same result as old GetVehicleDamage()->GetIsDriveable() function but faster.
	u8	bForceOtherVehiclesToStopForThisVehicle	: 1;	// Makes sure normal traffic always stops rather than trying to steer round (reset every frame)
	u8	bForceOtherVehiclesToOvertakeThisVehicle : 1;	//Makes sure normal traffic always tries to overtake this car rather than wait behind
	u8	bBecomeStationaryQuicker	: 1;	//Become stationary more quickly after losing the driver.


	u8	bHasDynamicNavMesh			: 1;	// If this vehicle has its own navmesh
	u8	bMadDriver					: 1;	// This vehicle is driving like a lunatic
	u8	bSlowChillinDriver			: 1;	// This vehicle is driving slow, windows down, music up
	u8	bHasGlowingBrakes			: 1;
	u8	bHasSiren					: 1;	// Vehicle has a siren.
	u8	bUseDeformation				: 1;	// turn deformation off for certain vehicle types (bikes for example)

	// script
	u8	bFreebies					: 1;	// Any freebies left in this vehicle ?
	u8	bTakeLessDamage				: 1;	// This vehicle is stronger (takes about 1/4 of damage)
	u8	bTyresDontBurst				: 1;	// If this is set the tyres are invincible
	u8	bWheelsDontBreak			: 1;	// If set the wheels cannot be broken off
	u8	bVehicleCanBeTargetted 		: 1;	// The ped driving this vehicle can be targetted, (for Torenos plane mission).  If no ped then the vehicle is targeted
	u8	bIsATargetPriority 			: 1;	// If set to targetable then this vehicle is higher priority target than normal
	u8	bTargettableWithNoLos		: 1;	// If set to targetable then this vehicle can be targeted with no line of sight
	u8	bDoesProvideCover			: 1;	// If this is false this particular vehicle can not be used to take cover behind.
	u8	bConsideredByPlayer			: 1;	// This vehicle is considered by the player to enter
	u8	bCanBeVisiblyDamaged		: 1;
	u8	bMissionVehToughAxles		: 1;	// tougher axles for mission vehicles so they don't skew / jam so easily
	u8	bShouldNotChangeColour		: 1;	// Some scripted cars shouldn't change colour in respray shop
	u8	bExplodesOnHighExplosionDamage : 1;	// Script can stop cars exploding instantly when hit by explosion
	u8  bExplodesOnNextImpact		: 1;	// Script can set vehicles to explode on the next impact
	u8	bPlaneResistToExplosion		: 1;	// (MP Only) Script can set a plane to survive from 2 to 3 explosion impacts
	u8  bDisableExplodeOnContact	: 1;	// Script can set this to override the CVehicleModelInfoFlags::FLAG_EXPLODE_ON_CONTACT flag
	u8  bCanEngineMissFire			: 1;	// Script can enable engines to missfire when damaged
	u8	bCanLeakPetrol				: 1;	// script can enable cars to leak petrol from damaged petrol tanks
	u8	bCanLeakOil					: 1;	// script can enable cars to leak oil from damaged engines
    u8	bCanPlayerCarLeakPetrol		: 1;	// script can enable player cars to leak petrol from damaged petrol tanks
    u8	bCanPlayerCarLeakOil		: 1;	// script can enable player cars to leak oil from damaged engines
	u8	bDisablePetrolTankFires		: 1;	// script can disable petrol tank fires from starting
	u8	bDisablePetrolTankDamage	: 1;	// script can disable petrol tank damage
	u8	bDisableEngineFires			: 1;	// script can disable engine fires from starting
	u8	bLimitSpeedWhenPlayerInactive:1;	// script can disable the limiting of speed when the player driver is inactive
	u8	bStopInstantlyWhenPlayerInactive:1;	// script can allow vehicle stop naturally when the player driver is inactive
	u8	bUnfreezeWhenCleaningUp		: 1;	// decides whether to call SetFixedPhysics(false) in CPhysical::CleanupMissionState
	u8	bDontTryToEnterThisVehicleIfLockedForPlayer : 1; // (MP Only) Player won't even attempt to enter this vehicle if its locked for the player, also displays help text
	u8  bDisablePretendOccupants	: 1;	// script can disable the vehicle attempting to add pretend occupants to the vehicle.
	u8  bUnbreakableLights			: 1;	// nothing will break the lights, unless they actually get detached from the vehicle.
    u8  bRespectLocksWhenHasDriver  : 1;    // vehicle locks are ignored for vehicles with drivers by default - this flag ensures locks are always checked
	u8	bForceEngineDamageByBullet: 1;	// Apply engine damage when vehicle gets hit by bullet at any position
	u8	bGeneratesEngineShockingEvents	: 1;	// Script can disable whether this vehicle generates shocking events tied to the engine like CEventShockingPlaneFlyBy
	u8	bCanEngineDegrade				: 1;	// Script can enable/disable engine degrading
	u8	bCanPlayerAircraftEngineDegrade	: 1;	// Script can enable/disable engine degrading for aircraft that's driven by player
	u8	bCanDeformWheels			: 1;		// Script can enable/disable the wheel deformation
	u8	bCanBreakOffWheelsWhenBlowUp: 1;		// Script can enable/disable breaking off wheels when car blows up
	u8	bIsStolen					: 1;	// mark the vehicle as stolen
	u8	bForceEnemyPedsFalloff		: 1;	// force peds that are standing on this vehicle to fall off regardless of how slow the vehicle is moving
	u8	bBlowUpWhenMissingAllWheels : 1;	// force this vehicle to blow up when it is missing all of its wheels.
	u8	bRearDoorsHaveBeenBlownOffByStickybomb	: 1;	// True if vehicle rear doors have been blown off by stickybomb
	u8	bDisableSuperDummy : 1;		// B*1814876: Prevent a vehicle from switching to superdummy mode.

//////////////////////////////////////////////////////////////////////////
// ai state
public:
	// controls
	u8	bInfluenceWantedLevel		: 1;	// Does this vehicle affect the wanted level if it's considered the "victim" of a crime?

	// visual state
	u8	bLightsOn					: 1;	// Are the lights switched on ?
	u8	bHeadlightsFullBeamOn		: 1;	// Are the extra brights headlights active
	u8	bLeftIndicator				: 1;	// If true the left indicator light should be on.
	u8	bRightIndicator				: 1;	// If true the right indicator light should be on.
	u8	bHazardLights				: 1;	// Hazard lights make all indicators flash.
	u8	bIndicatorLights			: 1;	// Turn indicator forced on/off
	u8	bForceBrakeLightOn			: 1;	// If this is true the brake light is rendered 'on' regardless of the actual braking applied.
	u8	bSuppressBrakeLight			: 1;	// If this is true the brake light is rendered 'off' regardless of the actual braking applied.
	u8	bInteriorLightOn			: 1;	// Force interior lights to be on.
	u8	bLightsButtonHeldDown		: 1;	// Is the headlights button held down?
	u8	bLightsAllowFullBeamSwitch	: 1;	// If swapping between dipped/full beam, don't allow another swap until bLightsButtonHeldDown becomes false
	u8	bRepairWhenSafe				: 1;	// Calls Fix() on the vehicle during ProcessControl(). This is set when we can't immediately fix the vehicle, usually during the physics update.
	u8	bPlayerHasTurnedOffLowBeamsAtNight : 1;	//If the player toggles lights twice, cycle through off-low-high
	u8  bRoofLowered				: 1;	// If the car is a convertible and has it's roof lowered

	// audio state (engine, horn etc)
	u8	bEngineOn					: 1;	// Cars can only drive when their engine is on.
	u8	bEngineStarting				: 1;	// Before the engine is actually on the engine will be starting first. Depending on the temperature of the engine this may take longer.
	u8	bEngineWontStart			: 1;	// Is the engine going to fail to start this time?
	u8	bSkipEngineStartup			: 1;	// should skip audible ignition sequence
	u8	bBlipThrottleRandomly		: 1;	// script can set this
	u8	bHasCoolingFan				: 1;
	u8	bIsCoolingFanOn				: 1;
	u8	bAudioBackfired				: 1;	// this flag is set when the audio thread plays a backfire sound - we need to trigger an fx but can't from this thread
	u8	bAudioBackfiredBanger		: 1;	// this flag is set when the audio thread plays a banger backfire sound - we need to trigger an fx but can't from this thread

	u8	bHornOnNetwork				: 1;

	// population
	u8	bIsAmbulanceOnDuty			: 1;	// Ambulance trying to get to an accident
	u8	bIsFireTruckOnDuty			: 1;	// Firetruck trying to get to a fire
	u8	bCannotRemoveFromWorld		: 1;	// Is this guy locked by the script (cannot be removed)
	u8	bHasBeenOwnedByPlayer		: 1;	// To work out whether stealing it is a crime
	u8	bCarNeedsToBeHotwired		: 1;	// To work out whether stealing it is a crime
	u8	bCountAsParkedCarForPopulation: 1;	// For population purposes this car is counted as a parked car (So it won't come out of the budget for moving cars)
	u8	bDontStoreAsPersistent		: 1;	// Some vehicles (like mission vehicles) should not be stored as persistent.
	u8	bScheduledForCreation		: 1;	// This vehicle is scheduled for creation
	u8	bAlertWhenEntered			: 1;	// This car will send a shocking event for being stolen when the player enters it.
	u8	bIsParkedAtPetrolStation	: 1;	// This car is parked in a petrol stations and is waiting for a scenario to be applied to it.
	u8	bCantTrafficJam				: 1;	// This car will not be considered part of a traffic jam (for population control purposes).
	u8	bVehicleIsOnscreen			: 1;	// If this car can be seen on screen.
	u8	bCreatingAsInactive			: 1;	// The vehicle is being created in inactive state (i.e. super-dummy with physics "disabled")
	u8	bTryToRemoveAggressively	: 1;	// If set, try to delete this vehicle when the player isn't looking. Used for scenario vehicles within blocking areas.
	u8	bCanBeDeletedWithAlivePedsInIt : 1;
	u8  bIsInCluster				: 1;	// Was this created at a clustered scenario point
	u8	bIsInClusterBeingSpawned	: 1;	// True while we are in the process of spawning as a part of a scenario cluster
	u8	bCanBeUsedByFleeingPeds		: 1;	// This vehicle can be used by fleeing peds
	u8	bCreatedByLowPrioCargen		: 1;	// A low prio cargen has created this vehicle
	u8	bRemoveWithEmptyCopOrWreckedVehs : 1;
	u8	bCreatedByCarGen			: 1;	// Vehicle was created on a cargen
	u8	bHasBeenDriven				: 1;	// Vehicle has been driven

	// car ai
	u8	bApproachingJunctionWithSiren : 1;	// This car has its siren on and is within a certain distance of a junction
	u8	bPreviousApproachingJunctionWithSiren :1;	// Same as above but for previous frame. (Can be used to find out whether we have just approached junction)
	u8	bIsParkedParallelly			: 1;	// This car has parked itself in a parallel parking spot
	u8	bIsParkedPerpendicularly	: 1;	// This car has parked itself in a perpendicular parking spot
	u8	bShouldBeRemovedByAmbientPed: 1;	// This car is a nuisance and should be removed by an ambient ped
	u8	bPoliceFocusWillTrackCar	: 1;	// If this is set the vehicle will always have the police focus circle locked onto it.
	u8	bShouldBeBeepedAt			: 1;	// This car is badly driven by player and should be beeped at
	u8	bPlayerDrivingAgainstTraffic		: 1;
	u8	bDisableTowing				: 1;	// Don't allow this vehicle to tow another (set on the tow truck)
	u8	bIsWaitingToTurnLeft		: 1;	// This car is waiting to turn left at a junction
	u8	bIsRacing					: 1;	// This vehicle is racing and should use different params for avoidance/etc
	u8	bHasGeneratedCover			: 1;	// Whether the vehicle has generated cover
	u8	bIsInRecording				: 1;	// Is the vehicle being controlled by a vehicle recording
	u8	bIsCargoVehicle				: 1;
	u8	bHasWheelOnPavement			: 1;
	u8	bWillTurnLeftAgainstOncomingTraffic : 1;	//we think we will set bIsWaitingToTurnLeft when we get to the middle of a junction
	u8	bTurningLeftAtJunction		: 1;	//Are we in the middle of a left turn?
	u8	bToldToGetOutOfTheWay		: 1;	//Another vehicle has hurried us
	u8	bIsInRoadBlock				: 1;	// This car is in a road block
	u8  bShouldLerpFromAiToFullRecording	: 1;	//This car is flagged to use bLerpToFullRecording when switching from an AI recording to a full recording
	u8	bIsDoingHandBrakeTurn		: 1;
	u8	bDisableAvoidanceProjection : 1;
	u8	bForceEntityScansAtHighSpeeds : 1;
	u8	bIsRacingConservatively		: 1;

	// hints for ped ai
	u8	bMoveAwayFromPlayer			: 1;	// Used to tell helis to fly away. (When wanted level has dropped)
	//u8	bCopperBlockedCouldLeaveCar	: 1;	// Used to tell Phils ai that this cop car was blocked or had to avoid another cop car/ped and the cop should consider leaving the car.

	// LOD (dummy state and other info)
	u8	bCanMakeIntoDummyVehicle	: 1;
	u8	bUsingPretendOccupants		: 1;
	u8	bSuperDummyProcessed		: 1;	// Was the super-dummy update processed this frame
	u8	bLodFarFromPopCenter		: 1;	// Is this vehicle far from the pop center this frame? (LOD info independent of dummy state.  Useful for LODing features that have small visual impact.)
	u8	bLodSuperDummyWheelCompressionSet	: 1; // Have the wheel compressions been set since this vehicle became a super-dummy? 
	u8	bLodCanUseTimeslicing		: 1;	// This is meant to get set to true by vehicle tasks that support timeslicing
	u8	bLodCanUseTimeslicingAfterVisibilityChecks : 1;	// Set by the LOD manager based on bLodCanUseTimeslicing.
	u8	bLodCanUseTimeslicingIfTooManyUpdates : 1;		// Set by the LOD manager based on bLodCanUseTimeslicing.
	u8	bLodShouldUseTimeslicing	: 1;	// Like bLodCanUseTimeslicing, but more aggressive, bypassing checks for superdummy and visibility.
	u8	bLodShouldSkipUpdatesInTimeslicedLod : 1;
	u8	bLodForceUpdateThisTimeslice : 1;	// Used for AI timeslicing to force an immediate update on the first frame in timeslicing LOD.
	u8	bLodForceUpdateUntilNextAiUpdate : 1; // Similar to bLodForceUpdateThisTimeslice, but ensures that we get an actual AI update before the flag gets reset.
	u8	bLodStrictRoadProximityTest	: 1;	// If a vehicle is coming from navigation other than using road nodes, then be more strict when testing to see if it can convert to dummy
	u8	bIsDeactivatedByPlayback : 1;		// If a vehicle physics gets deactivated by the playback, either forcibly or as an optimization
	u8	bMayBecomeParkedSuperDummy : 1;		// If this vehicle is parked, should we possibly allow it to become a superdummy?
	u8	bMoreAccurateVirtualRoadProbes : 1;	// If set, do some extra calculations to see if we might want to use virtual road probes on all wheels when in superdummy, due to an uneven surface.
	u8	bWasOffroadWithConstraint : 1;
	u8	bCheckForMisalignmentOnDeactivation : 1;	// True if we should check for misalignment with the road on the next OnDeactivate() call

	// misc
	u8  bWheelsDisabled				: 1;
	u8  bWheelsDisabledOnDeactivation : 1;	// Wheels were disabled when this vehicle was deactivated.
	u8	bIsDrowning					: 1;	// is vehicle occupants taking damage in water (i.e. vehicle is dead in water)
	u8	bDisableParticles			: 1;	// Disable particles from this car. Used in garage.
	u8	bWanted						: 1;	// If true this car is wanted by the police, if a player is in a wanted car he is easier to see.
	u8	bInSlownessZone				: 1;	// This is true if this vehicle is in an artists specified slowness zone and needs to have extra air friction applied.	
	u8	bWheelShapetest				: 1;	// Is wheel shapetest enabled (more expensive wheel processing)
	u8	bPlaceOnRoadQueued			: 1;	// Waiting for collision to load to place on road properly
	u8	bAutomaticallyAttaches		: 1;	// Indicates whether tow trucks or trucks automatically attach to this vehicle
	u8	bScansWithNonPlayerDriver	: 1;	// Indicates whether trucks without player drivers search and attach to trailers
	u8  bCarAgainstCarSideCollision	: 1;    // Indicates whether a car has run into the side of this car
	u8  bCarHitByHeavyVehicle		: 1;    // Indicates whether a car has been hit by a heavy vehicle
	u8	bCarHitWhileInRoadBlock		: 1;	// Indicates whether a car has been hit side on while part of a road block
	u8	bCarBrushAgainstCarSideCollision : 1;	// Indicates whether a car side has brushing against another car side in high speed
	u8  bCarHitBySuperHeavyVehicle	: 1;    // Indicates whether a car has been hit by a super heavy vehicle
	u8  bCarCrushingBike			: 1;    // Indicates whether a car is sitting on top of a bike
	u8	bShouldFixIfNoCollision		: 1;	// Fixes the car if no collision is around it
	u8  bPetrolTankEmpty			: 1;	// True if the petrol tank is empty (set by vehicledamage code)
	u8  bDrivingOnVehicle			: 1;	// True it the vehicle is driving on another vehicle
	u8	bProducingSlipStream		: 1;	// This vehicle is producing slip stream for another vehicle, i.e. another vehicle is behind this vehicle and speeding up because of it.
	u8  bUpdateProducingSlipStream  : 1;	// The vehicle was producing slip stream
	u8  bSlipStreamDisabledByTimeOut    : 1;	// After being in slip stream for a certain amount of time we deactivate slipstream for a bit.
	u8  bDisableHeightMapAvoidance	: 1;	// Setting this to true will disable height map avoidance on planes and helicopters
	u8	bDrivenDangerouslyThisUpdate : 1;
	u8	bDrivenDangerouslyExtreme	: 1;
	u8	bIsInPopulationCache		: 1;
	u8	bPartsBrokenOff				: 1;
	u8	bHasBeenRepainted			: 1;
	u8	bForceHd					: 1;
	u8	bScriptForceHd				: 1;
	u8	bIsBeingTowed				: 1;
	u8	bCanSaveInGarage			: 1;
	u8	bRestoreVelAfterCollLoads	: 1;	// Setting to true will restore velocity on vehicles that was set before they were fixed waiting for collision
	u8  bRestorePlaybackVelAfterCollLoads : 1; // Setting to true will restore velocity on vehicles that was set by a recording after they are un-fixed from waiting for collision
	u8	bMayHavePedDrivingDoorFlagSet : 1;	// Set if PED_DRIVING_DOOR is currently set on any of the doors
	u8	bIsAsleep					: 1;
	u8	bCanBeStolen				: 1;	// if false it won't be flagged as stolen when player enters
	u8	bVehicleInaccesible			: 1;	// Used to identify when a ped can't enter a vehicle, e.g. a bike gets stuck under something low.
	u8	bAllowBoundsToBeRaised		: 1;	// This vehicle can have its bounds raised and lowered to allow vehicles to go over high curbs without disturbance from collision.
	u8	bTowedVehBoundsCanBeRaised	: 1;	// This towed vehicle can have its bounds raised and lowered to allow vehicles to go over high curbs without disturbance from collision.
	u8	bIsNitrousBoostActive		: 1;	// Set when the engine is being given a boost due to nitrous 
	u8	bIsKERSBoostActive			: 1;	// Set when the engine is being given a boost due to KERS 
	u8  bAICanUseExclusiveSeats		: 1;	// AI are allowed to use driver seats marked as exclusive
	u8	bAreHydraulicsAllRaised		: 1;	// Set when the hydraulics are set to their top position.
	u8  bPlayerModifiedHydraulics   : 1;	// Set when hydraulics are being modified by the player. Synced across network.
	u8	bIsUnderMagnetControl		: 1;	// Set when a vehicle is being pulled in by a magnet gadget. Prevents the owner of the magnet from losing control of the vehicle in network games.
	u8  bDisableRoadExtentsForAvoidance   : 1;	// Set to disable road extent calculation for vehicle avoidance

	// script
	u8	bHasBeenResprayed			: 1;	// Has been resprayed in a respray garage. Reset after it has been checked.
	u8	bHideInCutscene				: 1;    // all (mission) vehicles are hidden in cutscene as default
	u8	bNotDriveable				: 1;	// Script can prevent us from being able to start the engine (can still get in though)
    u8	bUseAlternateHandling		: 1;	// Signifies whether the car uses alternate handling or not.
	u8	bStealable					: 1;	// Disables a vehicle from being stealable by ambient peds.
	u8	bLockDoorsOnCleanup			: 1;	// Locks all doors when the vehicle converts from a script vehicle to an ambient one
	u8	bForceInactiveDuringPlayback: 1;	// Force the vehicle staying inactive during the playback recording.
	u8	bForceActiveDuringPlayback: 1;		// Force the vehicle staying active during the playback recording.
	u8	bAllowKnockOffVehicleForLargeVertVel : 1;
	u8  bScriptSetHandbrake			: 1;	// Script is forcing the handbrake on
	u8	bUsedForPilotSchool			: 1;	// Is used for pilot school
	u8	bActiveForPedNavigation		: 1;	// Peds will consider this vehicle when navigation
	u8	bKeepEngineOnWhenAbandoned	: 1;	// The engine will remain on after driver exit vehicle
	u8	bUsesLargeRearRamp			: 1;	// Script can force aircraft doors to use the VEH_BOOT tunings for other doors (used for Cargoplane)
	u8	bForceAfterburnerVFX		: 1;	// Script can force vehicles after burner on. This is a fix for GTAV B*1739284

	// Weapon
	u8	bAllowHomingMissleLockon	: 1;	// Special case where scripters can disable homing missiles from targeting individual vehicles;
	u8  bAllowNoPassengersLockOn	: 1;	// Allow lock on, no occupants.
	
	// animation
	u8 bAnimateWheels				: 1; 
	u8 bAnimatePropellers			: 1; 
	u8 bForcePostCameraAiSecondaryTaskUpdate		: 1;
	u8 bForcePostCameraAnimUpdate	: 1;
	u8 bForcePostCameraAnimUpdateUseZeroTimeStep	: 1;
    u8 bDriveMusclesToAnim          : 1;
	u8 bAnimateJoints				: 1;		//Don't sync the skeleton when joints are being animated	
	u8 bUseCutsceneWheelCompression	: 1;

	u8 bCountsTowardsAmbientPopulation : 1;

	// rendering
	u8 bIsFarDrawEnabled			: 1;	// vehicle has it's fade dist extended
	u8 bRequestDrawHD						: 1;	// issues necessary requests and draw this vehicle as HD
	u8 bRefreshVisibility			: 1;

	// Damage Tracking
	u8 bDamageTrackingLocked        : 1; // When this is set the vehicle is locked for future damage tracking.

	// networking
	u8 bSyncPopTypeOnMigrate		: 1;
	u8 bBlownUp						: 1;

private:
	u8 	bSiren						: 1;	// Set to TRUE if siren active, else FALSE
	u8 	bRandomlyDelaySiren			: 1;	// Set to TRUE if siren active, else FALSE
	u8  bSirenUsesEarlyFade			: 1;	// Set to TRUE to reduce sirens fade in distance.
	u8	bSirenUseNoSpec				: 1;	// Set to TRUE to force no specular on siren lights.
	u8	bSirenUseNoLights			: 1;	// Set to TRUE to force sirens to use no lights.
	u8  bSirenUsesDayNightFade		: 1;	// Set to TRUE to use Daylight fade on sirens.
	u8 	bSirenMutedByScript			: 1;	// Set to TRUE if the siren is muted (still flashing), else FALSE
	u8	bSirenMutedByCode			: 1;

//////////////////////////////////////////////////////////////////////////
// reset state
public:
	u8	bWarnedPeds					: 1;	// Has scan and warn peds of danger been processed?
	u8	bVehicleColProcessed 		: 1;	// Has ProcessEntityCollision been processed for this car?
	u8  bWakeUpNextUpdate           : 1;    // 
	u8	bRestingOnPhysical 			: 1;	// Don't go static cause car is sitting on a physical object that might get removed
	u8  bDontSleepOnThisPhysical    : 1;	
	u8	bPreventBeingDummyThisFrame	: 1;	// Disallow this vehicle from becoming a dummy this frame
	u8	bPreventBeingSuperDummyThisFrame : 1;	// Disallow SuperDummy updates but allow Dummy
	u8	bPreventBeingDummyUnlessCollisionLoadedThisFrame : 1;
	u8	bTasksAllowDummying			: 1;	// The tasks set this flag then they permit the vehicle to become a dummy
	u8	bNoDummyPathAvailable		: 1;	// This vehicle does not have a path that could be used to become a dummy.
	u8	bDummyWheelImpactsSampled	: 1;	// Whether the dummy/superdummy virtual wheel impacts have already been sampled this frame
	u8  bDoorRatioHasChanged		: 1;	// Doors have opened/closed, navigation bounds may need updating
	u8	bActAsIfHighSpeedForFragSmashing : 1;	// When processing collisions with frags, pretend we have a really high speed in order to smash stuff
	u8  bMayHaveWheelGroundForcesToApply : 1;   // The wheels are touching something movable
	u8	bHasParentVehicle			: 1;	//Is this a trailer attached to a cab, or a car on the back of a trailer?
	u8	bHasProcessedAIThisFrame	: 1;
	u8	uAllowedDummyConversion		: 3;	// If we do a GetCanMakeIntoDummy check, store what dummy mode we're allowed (seee VehicleDummyMode) so we don't have to do it again
	u8	bIsOvertakingStationaryCar	: 1;
	u8	bOtherCarsShouldCheckTrajectoryBeforeSpawning : 1;
	u8	bAvoidanceDirtyFlag			: 1;
	u8	bInMotionFromExplosion		: 1;	// Set while we're in motion due to an explosion
	u8	bActAsIfHasSirenOn			: 1;	// Allow non-sirened cars to get other vehs to act as if they do
	u8	bUnbreakableLandingGear		: 1;	// Make landing gear invincible for one frame
	u8	bIsInTunnel					: 1;
	u8  bSwitchToAiRecordingThisFrame : 1;
	u8	bLerpToFullRecording	 : 1;
	u8	bTellOthersToHurry			: 1;
	u8  bDisableVehicleMapCollision : 1;	// Reset flag.  Used by synced scenes and scripted animations.
	u8	bUseScriptedCeilingHeight	: 1;	// Reset flag.	Vehicle uses the ceiling height that's set from script
	u8	bWasStoppedForDoor			: 1;
	u8	bIsHesitating				: 1;
#if ENABLE_FRAG_OPTIMIZATION
	u8	bHasFragCacheEntry			: 1;
	u8	bLockFragCacheEntry			: 1;
#endif

//////////////////////////////////////////////////////////////////////////
// want to remove
	u8		bPartOfConvoy 				: 1;
	u8		bFailedToResetPretendOccupants : 1;

//////////////////////////////////////////////////////////////////////////
// visibility
	u8 bNonAmbientVehicleMayBeUsedByGoToPointAnyMeans : 1;	// Level designers want peds to take scripted vehicles created by the mission editor
	u8 bHasBeenSeen	: 1;

//////////////////////////////////////////////////////
// reuse
	u8 bIsInVehicleReusePool : 1;
#if __BANK
	u8 bWasReused : 1;
#endif // __BANK
//////////////////////////////////////////////////////////////////////////
// Cleanup readiness
	u8 bReadyForCleanup : 1;

	u8	bCheckForEnoughRoomToFitPed : 1;	// script can force the check to ensure there's enough room to fit a ped during vehicle entry in case of attached props
};



#endif	//#ifndef _VEH_FLAGS_H_



