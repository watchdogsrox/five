<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CTaskShockingEvent::Tunables"  base="CTuning">
    <float name="m_MinRemainingRotationForScaling" min="0.0f" max="3.14f" step="0.01f" init="0.01f"/>
    <float name="m_MinAngularVelocityScaleFactor" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxAngularVelocityScaleFactor" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
  </structdef>

	<structdef type="CTaskShockingEventGoto::Tunables"  base="CTuning">
		<float name="m_DistSquaredThresholdAtCrowdRoundPos" min="0.0f" max="1000.0f" step="0.01f" init="0.5625f"/>
		<float name="m_DistSquaredThresholdMovingToCrowdRoundPos" min="0.0f" max="1000.0f" step="0.01f" init="0.25f"/>
		<float name="m_DistVicinityOfCrowd" min="0.0f" max="100.0f" step="0.1f" init="5.0f"/>
		<float name="m_ExtraDistForGoto" min="0.0f" max="100.0f" step="0.1f" init="1.0f"/>
		<float name="m_MinDistFromOtherPeds" min="0.0f" max="100.0f" step="0.1f" init="1.3f"/>
		<float name="m_MoveBlendRatioForFarGoto" min="0.0f" max="3.0f" step="0.1f" init="1.66f"/>
		<float name="m_TargetRadiusForCloseNavMeshTask" min="0.0f" max="100.0f" step="0.1f" init="0.5f"/>
    	<float name="m_ExtraToleranceForStopWatchDistance" min="0.0f" max="100.0f" step="0.1f" init="1.0f"/>
	</structdef>

	<structdef type="CTaskShockingEventHurryAway::Tunables"  base="CTuning">
		<float name="m_LookAheadDistanceForBackAway" min="0.0f" max="50.0f" init="6.0f"/>
		<float name="m_ChancesToCallPolice" min="0.0f" max="1.0f" init="0.3f"/>
		<float name="m_MinTimeToCallPolice" min="0.0f" max="30.0f" init="1.0f"/>
		<float name="m_MaxTimeToCallPolice" min="0.0f" max="30.0f" init="5.0f"/>
		<float name="m_ChancePlayingInitalTurnAnimSmallReact" min="0.0f" max="1.0f" init="0.5f"/>
		<float name="m_ChancePlayingCustomBackAwayAnimSmallReact" min="0.0f" max="1.0f" init="0.5f"/>
		<float name="m_ChancePlayingInitalTurnAnimBigReact" min="0.0f" max="1.0f" init="1.0f"/>
		<float name="m_ChancePlayingCustomBackAwayAnimBigReact" min="0.0f" max="1.0f" init="1.0f"/>
		<float name="m_ShouldFleeDistance"				min="0.0f" max="100.0f" init="3.0f" description="Distance to use when checking ShouldFleeSource/OtherEntity flag"/>
		<float name="m_ShouldFleeVehicleDistance"	min="0.0f" max="100.0f" init="5.0f"	description="Distance to use when checking ShouldFleeSource/OtherEntity flag, if other entity is a vehicle"/>
		<float name="m_ShouldFleeFilmingDistance"	min="0.0f" max="100.0f" init="6.0f"	description="Distance to use when checking ShouldFleeSource/OtherEntity flag, if this ped is currently filming the shocking event"/>
		<u32 name="m_EvasionThreshold" init="5000" description="How long ago in ms is recent wrt evading something."/>
		<float name="m_ClosePlayerSpeedupDistanceSquaredThreshold" min="0.0f" max="400.0f" init="25.0f" description="Distance considered close to the local player for speedups."/>
		<float name="m_ClosePlayerSpeedupTimeThreshold" min="0.0f" max="40.0f" init="6.0f" description="Time threshold to speedup when the local player is close."/>
		<float name="m_MinDistanceFromPlayerToDeleteHurriedPed" min="0.0f" max="100.0f" init="20.0f" description="Distance to delete a hurrying ped (must also be offscreen)."/>
		<float name="m_TimeUntilDeletionWhenHurrying" min="0.0f" max="100.0f" init="10.0f" description ="Time to delete a hurrying ped when offscreen and far away."/>
  </structdef>

	<structdef type="CTaskShockingEventWatch::Tunables"  base="CTuning">
		<float name="m_MaxTargetAngularMovementForWatch" min="0.0f" max="10.0f" step="0.001f" init="0.349f"/>
		<float name="m_ThresholdWatchAfterFace" min="0.0f" max="1.0f" step="0.01f" init="0.9659f"/>
		<float name="m_ThresholdWatchStop" min="0.0f" max="100.0f" step="0.01f" init="0.9063f"/>
		<float name="m_MinDistanceBetweenFilmingPeds"  min="0.0f" max="1000.0f" step="0.1f" init="2.0f" description="Minimum distance away from any other filming ped we must be to start filming a shocking event ourselves."/>
		<float name="m_MinDistanceFromOtherPedsToFilm" min="0.0f" max="1000.0f" step="0.1f" init="1.0f" description="Minimum distance away from any other ped we must be to start filming a shocking event."/>
		<float name="m_MinDistanceAwayForFilming"      min="0.0f" max="1000.0f" step="0.1f" init="5.0f" description="Minimum distance away from the event to start filming."/>
	</structdef>

  <structdef type="CTaskShockingEventReactToAircraft::Tunables"  base="CTuning">
    <float name="m_ThresholdWatch" min="0.0f" max="500.0f" step="1.0f" init="400.0f"/>
    <float name="m_ThresholdRun" min="0.0f" max="200.0f" step="1.0f" init="20.0f"/>
  </structdef>

  <structdef type="CTaskShockingEventReact::Tunables"  base="CTuning">
    <float name="m_TurningTolerance" min="0.0f" max="1.0f" step="0.05f" init="0.800f"/>
    <float name="m_TurningRate" min="0.0f" max="5.0f" step="0.1f" init="1.000f"/>
    <float name="m_TurningEnergyUpperThreshold" min="0.0f" max="10.0f" step="0.16f" init="3.0f"/>
    <float name="m_TurningEnergyLowerThreshold" min="0.0f" max="10.0f" step="0.1f" init="0.01f"/>
    <float name="m_TimeBetweenReactionIdlesMin" min="0.0f" max="10.0f" step="0.1f" init="5.0f"/>
    <float name="m_TimeBetweenReactionIdlesMax" min="0.0f" max="10.0f" step="0.1f" init="6.0f"/>
    <float name="m_BlendoutPhase" min="0.0f" max="1.0f" step="0.01f" init="0.85f"/>
  </structdef>

  <structdef type="CTaskShockingEventBackAway::Tunables"  base="CTuning">
    <float name="m_MaxHeadingAdjustmentRate" min="0.0f" max="1000.0f" init="1.0f"/>
    <float name="m_MinHeadingAlignmentCosThreshold" min="0.0f" max="1.0f" init="0.5f"/>
    <float name="m_MaxHeadingAlignmentCosThreshold" min="0.0f" max="1.0f" init="0.866f"/>
    <float name="m_MoveNetworkBlendoutDuration" min="0.0f" max="2.0f" step="0.01f" init="0.3f"/>
    <float name="m_DefaultBackwardsProjectionRange" min="0.0f" max="20.0f" step="0.1f" init="10.0f"/>
    <float name="m_AxesFacingTolerance" min="0.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinDistanceForBackAway" min="0.0f" max="10.0f" init="2.5f" step="0.1f"/>
    <float name="m_MaxDistanceForBackAway" min="0.0f" max="10.0f" init="1.5f" step="0.1f"/>
    <float name="m_MaxWptAngle" min="0.0f" max="30.0f" init="15.0f" step="0.1f"/>
    <float name="m_BlendOutPhase" min="0.0f" max="1.0f" step="0.01f" init="0.85f"/>
  </structdef>

  <structdef type="CTaskShockingPoliceInvestigate::Tunables"  base="CTuning">
    <float name="m_ExtraDistForGoto" min="0.0f" max="100.0f" step="0.1f" init="1.0f"/>
    <float name="m_MoveBlendRatioForFarGoto" min="0.0f" max="3.0f" step="0.1f" init="1.66f"/>
	<float name="m_MinDistFromPlayerToDeleteOffscreen" init="20.0f" min="0.0f" max="500.0f" description="How far ped must be from the player to delete if standing forever"/>
	<u32 name="m_DeleteOffscreenTimeMS_MIN" init="3000" description="How long the ped must be offscreen for deletion minimum (milliseconds)"/>
	<u32 name="m_DeleteOffscreenTimeMS_MAX" init="8000" description="How long the ped must be offscreen for deletion maximum (milliseconds)"/>
  </structdef>

	<structdef type="CTaskShockingEventStopAndStare::Tunables"  base="CTuning">
		<float name="m_BringVehicleToHaltDistance" min="0.0f" max="20.0f" step="0.1f" init="5.0f"/>
	</structdef>

	<structdef type="CTaskAgitated::Tunables::Rendering">
		<bool name="m_Enabled" init="false"/>
		<bool name="m_Info" init="true"/>
		<bool name="m_Hashes" init="true"/>
		<bool name="m_History" init="false"/>
	</structdef>
	
	<structdef type="CTaskAgitated::Tunables"  base="CTuning">
		<struct name="m_Rendering" type="CTaskAgitated::Tunables::Rendering"/>
		<float name="m_TimeBetweenLookAts" min="0.0f" max="10.0f" step="1.0f" init="3.0f"/>
		<float name="m_MovingAwayVelocityMSThreshold" min="0.0f" max="10.0f" step="0.01f" init="0.01f"/>
	</structdef>

	<structdef type="CTaskConfront::Tunables"  base="CTuning">
		<float name="m_IdealDistanceIfUnarmed" min="0.0f" max="10.0f" step="0.1f" init="0.75f"/>
		<float name="m_IdealDistanceIfArmed" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
		<float name="m_MinDistanceToMove" min="0.0f" max="10.0f" step="0.1f" init="1.25f"/>
		<float name="m_MaxRadius" min="0.0f" max="15.0f" step="0.1f" init="7.5f"/>
		<float name="m_ChancesToIntimidateArmedTarget" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_ChancesToIntimidateUnarmedTarget" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
	</structdef>

	<structdef type="CTaskIntimidate::Tunables"  base="CTuning">
		<bool name="m_UseArmsOutForCops" init="false"/>
	</structdef>

	<structdef type="CTaskShove::Tunables::Rendering">
		<bool name="m_Enabled" init="false" />
		<bool name="m_Contact" init="true" />
	</structdef>

	<structdef type="CTaskShove::Tunables" base="CTuning">
		<struct name="m_Rendering" type="CTaskShove::Tunables::Rendering"/>
		<float name="m_MaxDistance" min="0.0f" max="3.0f" step="0.01f" init="0.75f"/>
		<float name="m_MinDot" min="-1.0f" max="1.0f" step="0.01f" init="0.95f"/>
		<float name="m_RadiusForContact" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
	</structdef>

	<structdef type="CTaskShoved::Tunables" base="CTuning">
	</structdef>

	<structdef type="CTaskReactToExplosion::Tunables"  base="CTuning">
		<float name="m_MaxShellShockedDistance" min="0.0f" max="100.0f" step="0.5f" init="10.0f"/>
		<float name="m_MaxFlinchDistance" min="0.0f" max="100.0f" step="0.5f" init="15.0f"/>
		<float name="m_MaxLookAtDistance" min="0.0f" max="100.0f" step="0.5f" init="30.0f"/>
	</structdef>
	
	<structdef type="CTaskReactToImminentExplosion::Tunables"  base="CTuning">
		<float name="m_MaxEscapeDistance" min="0.0f" max="25.0f" step="1.0f" init="1.0f"/>
		<float name="m_MaxFlinchDistance" min="0.0f" max="25.0f" step="1.0f" init="10.0f"/>
	</structdef>

	<structdef type="CTaskReactAndFlee::Tunables"  base="CTuning">
		<float name="m_MinFleeMoveBlendRatio" min="0.0f" max="3.0f" step="0.1f" init="1.8f"/>
		<float name="m_MaxFleeMoveBlendRatio" min="0.0f" max="3.0f" step="0.1f" init="2.2f"/>
		<bool name="m_OverrideDirections" init="false"/>
		<float name="m_OverrideReactDirection" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_OverrideFleeDirection" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxReactionTime" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinRate" min="0.0f" max="2.0f" step="0.01f" init="0.9f"/>
		<float name="m_MaxRate" min="0.0f" max="2.0f" step="0.01f" init="1.1f"/>
		<float name="m_HeadingChangeRate" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
		<float name="m_MinTimeToRepeatLastAnimation" min="0.0f" max="5.0f" step="0.01f" init="3.0f"/>
    	<float name="m_SPMinFleeStopDistance" min="-1.0f" max="100000.0f" step="1.0f" init="500.0f"/>
    	<float name="m_SPMaxFleeStopDistance" min="-1.0f" max="100000.0f" step="1.0f" init="500.0f"/>
    	<float name="m_MPMinFleeStopDistance" min="-1.0f" max="100000.0f" step="1.0f" init="100.0f"/>
    	<float name="m_MPMaxFleeStopDistance" min="-1.0f" max="100000.0f" step="1.0f" init="150.0f"/>
      <float name="m_TargetInvalidatedMinFleeStopDistance" min="0.0f" max="1000.0f" step="0.5f" init="15.0f" />
      <float name="m_TargetInvalidatedMaxFleeStopDistance" min="0.0f" max="1000.0f" step="0.5f" init="25.0f" />
    	<float name="m_IgnorePreferPavementsTimeWhenInCrowd" min="-1.0f" max="1000.0f" step="0.1f" init="1.0f" />
  </structdef>
	
	<structdef type="CTaskSmartFlee::Tunables"  base="CTuning">
		<bool name="m_ExitVehicleDueToRoute" init="false"/>
		<bool name="m_UseRouteInterceptionCheckToExitVehicle" init="true"/>
		<float name="m_RouteInterceptionCheckMinTime" min="0.0f" max="60.0f" step="1.0f" init="1.5f"/>
		<float name="m_RouteInterceptionCheckMaxTime" min="0.0f" max="60.0f" step="1.0f" init="3.0f"/>
		<float name="m_RouteInterceptionCheckDefaultMaxSideDistance" min="0.0f" max="200.0f" step="1.0f" init="1.5f"/>
		<float name="m_RouteInterceptionCheckDefaultMaxForwardDistance" min="0.0f" max="200.0f" step="1.0f" init="6.0f"/>
		<float name="m_RouteInterceptionCheckVehicleMaxSideDistance" min="0.0f" max="200.0f" step="1.0f" init="2.25f"/>
		<float name="m_RouteInterceptionCheckVehicleMaxForwardDistance" min="0.0f" max="200.0f" step="1.0f" init="6.0f"/>
    <float name="m_EmergencyStopMinSpeedToUseDefault" min="0.0f" max="10000.0f" step="0.1f" init="10.0f"/>
    <float name="m_EmergencyStopMaxSpeedToUseDefault" min="0.0f" max="10000.0f" step="0.1f" init="20.0f"/>
    <float name="m_EmergencyStopMaxSpeedToUseOnHighWays" min="0.0f" max="10000.0f" step="0.1f" init="15.0f"/>
    <float name="m_EmergencyStopMinTimeBetweenStops" min="0.0f" max="10000.0f" step="0.1f" init="30.0f"/>
    <float name="m_EmergencyStopTimeBetweenChecks" min="0.0f" max="10000.0f" step="0.1f" init="0.1f"/>
		<float name="m_EmergencyStopInterceptionMinTime" min="0.0f" max="60.0f" step="1.0f" init="0.5f"/>
		<float name="m_EmergencyStopInterceptionMaxTime" min="0.0f" max="60.0f" step="1.0f" init="3.5f"/>
		<float name="m_EmergencyStopInterceptionMaxSideDistance" min="0.0f" max="200.0f" step="1.0f" init="1.8f"/>
		<float name="m_EmergencyStopInterceptionMaxForwardDistance" min="0.0f" max="200.0f" step="1.0f" init="10.0f"/>
    <float name="m_EmergencyStopTimeToUseIfIsDrivingParallelToTarget" min="0.0f" max="200.0f" step="1.0f" init="5.0f"/>
    <float name="m_DrivingParallelToThreatMaxDistance" min="0.0f" max="200.0f" step="1.0f" init="8.0f"/>
    <float name="m_DrivingParallelToThreatMinVelocityAlignment" min="-1.0f" max="1.0f" step="0.1f" init="0.866f"/>
    <float name="m_DrivingParallelToThreatMinFacing" min="-1.0f" max="1.0f" step="0.1f" init="-0.5f"/>
    <float name="m_ExitVehicleMaxDistance" min="0.0f" max="200.0f" step="1.0f" init="50.0f"/>
		<float name="m_ExitVehicleRouteMinDistance" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
		<float name="m_TimeBetweenHandsUpChecks" min="0.0f" max="5.0f" step="1.0f" init="1.0f"/>
		<float name="m_TimeBetweenExitVehicleDueToRouteChecks" min="0.0f" max="5.0f" step="1.0f" init="1.0f"/>
		<float name="m_TimeToCower" min="0.0f" max="1000.0f" step="1.0f" init="20.0f" description="Time to cower, if using stationary reactions from a scenario."/>
		<float name="m_MinTimeForHandsUp" min="0.0f" max="10.0f" step="0.1f" init="1.5f" />
		<float name="m_MaxTimeForHandsUp" min="0.0f" max="10.0f" step="0.1f" init="2.5f" />
		<float name="m_MinDelayTimeForExitVehicle" min="0.0f" max="2.0f" step="0.1f" init="0.0f" />
		<float name="m_MaxDelayTimeForExitVehicle" min="0.0f" max="2.0f" step="0.1f" init="0.5f" />
        <float name="m_ChanceToDeleteOnExitVehicle" min="0.0f" max="1.0f" step="0.01f" init="0.5f" />
        <float name="m_MinDistFromPlayerToDeleteOnExitVehicle" min="0.0f" max="10000.0f" step="1.0f" init="10.0f"/>
		<float name="m_MaxRouteLengthForCower" min="0.0f" max="100.0f" step="1.0f" init="15.0f" />
		<float name="m_MinDistFromTargetWhenCoweringToCheckForExit" min="0.0f" max="100.0f" step="1.0f" init="10.0f" />
		<float name="m_FleeTargetTooCloseDistance" min="0.0f" max="20.0f" step="0.1f" init="4.0f" />
		<float name="m_MinFleeOnBikeDistance" min="0.0f" max="20.0f" step="0.1f" init="5.0f" />
		<float name="m_TimeOnBikeWithoutFleeingBeforeExitVehicle" min="0.0f" max="120.0f" step="0.1f" init="10.0f" />
		<int name="m_MaxRouteSizeForCower" min="0" max="24" step="1" init="14" />
		<bool name="m_ForceCower" init="false" />
		<float name="m_ChancesToAccelerateTimid" min="0.0f" max="1.0f" step="0.01f" init="0.4f" />
		<float name="m_ChancesToAccelerateAggressive" min="0.0f" max="1.0f" step="0.01f" init="0.7f" />
		<float name="m_TargetObstructionSizeModifier" min="0.0f" max="10.0f" step="0.1f" init="3.0f" />
		<float name="m_TimeToReverseCarsBehind" min="0.0f" max="10.0f" step="0.05f" init="0.0f" />
		<float name="m_TimeToReverseInoffensiveTarget" min="0.0f" max="10.0f" step="0.1f" init="0.4f" />
		<float name="m_TimeToReverseDefaultMin" min="0.0f" max="10.0f" step="0.05f" init="1.4f" />
		<float name="m_TimeToReverseDefaultMax" min="0.0f" max="10.0f" step="0.05f" init="2.1f" />
		<float name="m_TimeToExitCarDueToVehicleStuck" min="0.0f" max="1000.0f" step="0.5f" init="5.0f" />
		<float name="m_RoutePassNearTargetThreshold" min="0.0f" max="1000.0f" step="0.5f" init="7.5f" />
		<float name="m_RoutePassNearTargetCareThreshold" min="0.0f" max="1000.0f" step="0.5f" init="30.0f" />
    <bool name="m_CanAccelerateAgainstInoffensiveThreat" init="false" />
		<float name="m_TargetInvalidatedMaxStopDistanceLeftMinVal" min="0.0f" max="1000.0f" step="0.5f" init="5.0f" />
		<float name="m_TargetInvalidatedMaxStopDistanceLeftMaxVal" min="0.0f" max="1000.0f" step="0.5f" init="10.0f" />
		<float name="m_MinSpeedToJumpOutOfVehicleOnFire" min="0.0f" max="1000.0f" step="0.5f" init="15.0f" />
</structdef>

  <structdef type="CTaskScenarioFlee::Tunables"  base="CTuning">
    <float name="m_fFleeProjectRange" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <float name="m_fInitialSearchRadius" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
    <float name="m_fSearchScaler" min="1.0f" max="5.0f" step="0.1f" init="2.0f"/>
    <float name="m_fSearchRangeMax" min="0.0f" max="200.0" step="1.0f" init="200.0f" />
    <float name="m_fFleeRange" min="0.0f" max="50.0f" step="1.0f" init="20.0f" />
    <float name="m_fFleeRangeExtended" min="0.0f" max="999.0f" step="1.0f" init="40.0f"/>
    <float name="m_fTargetScenarioRadius" min="0.0f" max="10.0f" step="0.1f" init="0.1f" />
    <float name="m_fProbeLength" min="1.0f" max="20.0f" step="0.1f" init="5.0f" />
    <u32 name="m_uAvoidanceProbeInterval" init="250" />
  </structdef>


  <structdef type="CTaskExhaustedFlee::Tunables" base="CTuning">
    <float name="m_StartingEnergy" min="0.0f" max="1000.0f" step="0.1f" init="100.0f"/>
    <float name="m_EnergyLostPerSecond" min="0.0f" max="1000.0f" step="0.1f" init="20.0f"/>
    <float name="m_OuterDistanceThreshold" min="0.0f" max="1000.0f" step="0.1f" init="10.0f"/>
    <float name="m_InnerDistanceThreshold" min="0.0f" max="1000.0f" step="0.1f" init="25.0f"/>
  </structdef>

  <structdef type="CTaskWalkAway::Tunables" base="CTuning">
    <float name="m_SafeDistance" min="0.0f" max="50.0f" init="5.0f" step="0.1f"/>
    <float name="m_TimeBetweenRouteAdjustments" min="0.0f" max="50.0f" init="3.0f" step="0.1f"/>
  </structdef>

  <structdef type="CTaskGrowlAndFlee::Tunables" base="CTuning">
    <float name="m_FleeMBR" min="0.0f" max="3.0f" step="0.1f" init="2.0f"/>
  </structdef>

	<structdef type="CTaskReactInDirection::Tunables"  base="CTuning">
	</structdef>

</ParserSchema>
