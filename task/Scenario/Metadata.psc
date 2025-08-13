<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CTaskCoupleScenario::Tunables" base="CTuning">
		<float name="m_ResumeDistSq" min="0.0f" max="100.0f" step="0.1f" init="4.0f" description="Leader resume distance, squared."/>
		<float name="m_StopDistSq" min="0.0f" max="100.0f" step="0.1f" init="9.0f" description="Leader stop distance, squared."/>
		<float name="m_TargetDistance" min="0.0f" max="100.0f" step="0.1f" init="0.0f" description="Min distance before coupled ped is allowed to stand still."/>
	</structdef>

	<structdef type="CTaskCowerScenario::Tunables" base="CTuning">
		<u32 name="m_EventDecayTimeMS" init="5000" description="How long in milliseconds before this ped can switch to directed after a gunshot."/>
		<float name="m_ReturnToNormalDistanceSq" init="400.0f" description="How far away the threat has to be for this ped to resume life as normal."/>
		<float name="m_BackHeadingInterpRate" init="5.0f" description=" Rate for interpolating between back left and back right when the target is moving behind the ped."/>
		<float name="m_EventlessSwitchStateTimeRequirement" init="3.0f" min="0.0f" max="10.0f" description="How long the ped has to be in a state (in s) before a switch of cower types due to distance."/>
		<u32 name="m_EventlessSwitchInactivityTimeRequirement" init="3000" description="How long it has to be (in ms) with nothing interesting going on before a switch of cower type due to distance."/>
		<float name="m_EventlessSwitchDistanceRequirement" init="3.0f" min="0.0f" max="30.0f" description="How far away the player can be to trigger a cower type switch without an event."/>
		<float name="m_MinDistFromPlayerToDeleteCoweringForever" init="20.0f" min="0.0f" max="500.0f" description="How far ped must be from the player to delete if cowering forever"/>
    <float name="m_MinDistFromPlayerToDeleteCoweringForeverRecordingReplay" init="100.0f" min="0.0f" max="500.0f" description="How far ped must be from the player to delete if cowering forever when recording a replay."/>
    <u32 name="m_CoweringForeverDeleteOffscreenTimeMS_MIN" init="3000" description="How long the ped must be offscreen for deletion minimum (milliseconds)"/>
		<u32 name="m_CoweringForeverDeleteOffscreenTimeMS_MAX" init="8000" description="How long the ped must be offscreen for deletion maximum (milliseconds)"/>
		<u32 name="m_FlinchDecayTime" init="750" description="How long the ped has after an event to flinch (ms)."/>
		<u32 name="m_MinTimeBetweenFlinches" init="200" description="The min time in milliseconds between consecutive flinches."/>
	</structdef>

	<structdef type="CTaskUseScenario::Tunables" base="CTuning">
		<float name="m_AdvanceUseTimeRandomMaxProportion" min="0.0f" max="1.0f" step="0.1f" init="0.67f" description="When spawning ambient peds in place, this controls how much extra randomness there is in the time before they leave the scenario."/>
		<float name="m_BreakAttachmentMoveSpeedThreshold" min="0.0f" max="100.0f" step="0.01f" init="0.25f" description="If attached to an entity, this is the threshold its velocity must exceed to break the attach status."/>
		<float name="m_BreakAttachmentOrientationThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.9f" description="If attached to an entity, this is the threshold its up vector's z component must fall below to break the attach status."/>
		<float name="m_ExitAttachmentMoveSpeedThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.1f" description="If attached to an entity, this is the threshold its velocity must exceed to exit normally."/>
		<float name="m_RouteLengthThresholdForFinalApproach" min="0.0f" max="25.0f" step="0.01" init="6.25" description="While moving towards the scenario point, this is a threshold against which to compare the route length to re-evaluate the path."/>
		<float name="m_ZThresholdForApproachOffset" min="0.0f" max="10.0f" step="0.1f" init="2.0f" description="The Z-threshold for the difference of the ped and scenario's z position when offsetting the goto target."/>
		<float name="m_DetachExitDefaultPhaseThreshold" min="0.0f" max="1.0f" step="0.01" init="0.5f" description="Default phase value for detaching from a scenario when exiting to a fall (used when clip is not tagged)."/>
		<float name="m_FastExitDefaultPhaseThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.6f" description="Default phase value for when the ped is doing a normal exit in a stressed mode (combat or to cower)."/>
		<float name="m_RouteLengthThresholdForApproachOffset" min="0.0f" max="10.0f" step="0.1f" init="6.25" description="When moving towards the scenario point, this is the route length threshold to determine if the ped is close enough to offset the goto target."/>
		<float name="m_ExtraFleeDistance" min="0.0f" max="100.0f" step="1.0f" init="20.0f" description="How much to add to the distance between the ped and aggressor when determining flee safe distance."/>
		<float name="m_FindPropInEnvironmentDist" min="0.0f" max="100.0f" step="0.1f" init="3.0f" description="Max distance at which to locate props in the environment to use from the scenario."/>
		<float name="m_MinRateToPlayCowerReaction" min="0.6f"  max="1.0f" step="0.05f" init="0.8f" description="Min rate to play cower reaction"/>
		<float name="m_MaxRateToPlayCowerReaction" min="1.0f"  max="1.4f" step="0.05f" init="1.3f" description="Max rate to play cower reaction"/>
		<float name="m_MinDifferenceBetweenCowerReactionRates" min="0.0f" max="2.0f" init="0.3f" description="The minimum amount two subsequent cower reaction rates can differ by."/>
		<float name="m_ReactAndFleeBlendOutPhase" min="0.0f" max="1.0f" step="0.01f" init="0.95f" description="When playing a panic exit animation to transition to CTaskReactAndFlee, end this task when less than this amount of time remains of the exit clip's duration."/>
		<float name="m_RegularExitDefaultPhaseThreshold" min="0.0f" max="1.0f" step="0.01f" init="0.95f" description="When playing a normal exit animation to transition into some other task, end the exit clip when the exit clip phase passes this mark."/>
		<float name="m_TimeOfDayRandomnessHours" min="0.0f" max="1.0f" step="0.05f" init="0.25f" description="To make it look more natural when the time of day limit has been reached, this is the phase threshold to begin blending out."/>
		<float name="m_TimeToLeaveMinBetweenAnybody" min="0.0f" max="20.0f" step="0.1f" init="0.5f" description="Minimum time in seconds that must have elapsed since any ped naturally left a scenario before another one can do it, used to desynchronize animations."/>
		<float name="m_TimeToLeaveRandomAmount" min="0.0f" max="10.0f" step="0.1f" init="0.5f" description="Time in seconds for how much the time until leaving can vary randomly. If m_TimeToLeaveRandomFraction yields a larger time, that will be used instead."/>
		<float name="m_TimeToLeaveRandomFraction" min="0.0f" max="1.0f" step="0.01f" init="0.2f" description="Specifies how much the time until leaving can vary randomly, as a fraction of the total use time. If this comes out to a smaller time than m_TimeToLeaveRandomAmount, m_TimeToLeaveRandomAmount will be used instead."/>
		<float name="m_PavementFloodFillSearchRadius" min="0.0f" max="50.0f" step="0.1f" init="10.0f" description="Defines a search radius when peds look for nearby pavement."/>
		<float name="m_DelayBetweenPavementFloodFillSearches" min="1.0f" max="5.0f" init="1.f" description="Time in seconds between retrying a flood fill to search for nearby pavement."/>
		<float name="m_FleeMBRMin" min="0.0f" max="3.0f" step="0.1f" init="1.8f" description="When starting an early navmesh flee, this is the lower bound for the initial MBR."/>
		<float name="m_FleeMBRMax" min="0.0f" max="3.0f" step="0.1f" init="2.2f" description="When starting an early navmesh flee, this is the uppoer bound for the initial MBR."/>
		<float name="m_MinPathLengthForValidExit" min="0.0f" max="100.0f" step="0.1f" init="10.0f" description="When playin a panic exit animation from a scenario, this is the min route length the ped can have to consider this exit point as valid."/>
		<float name="m_MaxDistanceNavmeshMayAdjustPath" min="0.0f" max="10.0f" step="0.01f" init="0.5f" description="The min distance the pathserver can snap the path from this point if the scenario entity exists and is not fixed."/>
		<float name="m_TimeBetweenChecksToLeaveCowering" min="0.0f" max="100.0f" step="0.1f" init="10.0f" description="When cowering, this is how long in seconds between attempts to leave the point."/>
		<float name="m_SkipGotoXYDist" min="0.0f" max="3.0f" step="0.1f" init="0.5f" description="When initially heading to the scenario the XY distance that means they will just skip the locomotion and blend into the intro."/>
		<float name="m_SkipGotoZDist" min="0.0f" max="3.0f" step="0.1f" init="1.5f" description="When initially heading to the scenario the Z distance that means they will just skip the locomotion and blend into the intro"/>
		<float name="m_SkipGotoHeadingDeltaDegrees" min="0.0f" max="90.0f" init="15.0f" description="When initially heading to the scenario the heading delta acceptable to skip the locomotion and blend into the intro."/>
		<s32 name="m_MinExtraMoney" min="0" max="100" step="1" init="30" description="Minimum amount of extra money granted when Ped uses a scenario that grants money."/>
		<s32 name="m_MaxExtraMoney" min="101" max="1000" step="1" init="200" description="Maximum amount of extra money granted when Ped uses a scenario that grants money."/>
    <s8 name="m_UpdatesBeforeShiftingBounds" init="15" description="How many updates before changing the ped bounds after a warp (was causing sinking when set to a hardcoded 5 so making it tunable)"/>
  </structdef>

	<structdef type="CScenarioClipHelper::Tunables" base="CTuning">
	</structdef>

	<structdef type="CTaskUseVehicleScenario::Tunables" base="CTuning">
		<float name="m_BringVehicleToHaltDistance" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
		<float name="m_IdleTimeRandomFactor" min="0.0f" max="1.0f" step="0.05f" init="0.2f"/>
		<float name="m_SlowDownDist" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
		<float name="m_SlowDownSpeed" min="0.0f" max="20.0f" step="0.1f" init="6.0f"/>
		<float name="m_SwitchToStraightLineDist" min="0.0f" max="50.0f" step="0.1f" init="3.0f"/>
		<float name="m_TargetArriveDist" min="0.0f" max="50.0f" step="0.1f" init="3.0f"/>
		<float name="m_PlaneTargetArriveDist" min="0.0f" max="100.0f" step="0.5f" init="10.0f"/>
		<float name="m_HeliTargetArriveDist" min="0.0f" max="100.0f" step="0.5f" init="5.0f"/>
		<float name="m_BoatTargetArriveDist" min="0.0f" max="100.0f" step="0.5f" init="0.0f"/>
		<float name="m_PlaneTargetArriveDistTaxiOnGround" min="0.0001f" max="100.0f" step="0.5f" init="12.0f"/>
		<float name="m_PlaneDrivingSubtaskArrivalDist" min="0.0001f" max="100.0f" step="0.5f" init="5.0f"/>
    	<float name="m_BoatMaxAvoidanceAngle" min="0.0f" max="360.0f" step="0.0001f" init="7.0f"/>
		<u16 name="m_MaxSearchDistance" min="0" max="10000" step="5" init="500" description="Max search distance for pathfinding on roads, for CTaskVehicleGoToAutomobileNew/CPathNodeRouteSearchHelper. A lower value helps to limit the risk of performance problem in case of pathfinding failure"/>
	</structdef>

	<structdef type="CTaskWanderingScenario::Tunables" base="CTuning">
		<float name="m_MaxTimeWaitingForBlockingArea" min="0.0f" max="100.0f" step="0.1f" init="20.0f"/>
		<float name="m_SwitchToNextPointDistWalking" min="0.0f" max="100.0f" step="0.1f" init="0.5f"/>
		<float name="m_SwitchToNextPointDistJogging" min="0.0f" max="100.0f" step="0.1f" init="1.5f"/>
		<float name="m_PreferNearWaterSurfaceArrivalRadius" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<u32 name="m_TimeBetweenBlockingAreaChecksMS" min="0" max="60000" step="500" init="2000"/>
	</structdef>

</ParserSchema>
