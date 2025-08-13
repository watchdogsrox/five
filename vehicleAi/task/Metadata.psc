<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CTaskVehicleDeadDriver::SteerAngleControl">
		<enumval name="SAC_Retain"/>
		<enumval name="SAC_Minimum"/>
		<enumval name="SAC_Maximum"/>
		<enumval name="SAC_Randomize"/>
	</enumdef>

	<enumdef type="CTaskVehicleDeadDriver::ThrottleControl">
		<enumval name="TC_Retain"/>
		<enumval name="TC_Minimum"/>
		<enumval name="TC_Maximum"/>
		<enumval name="TC_Randomize"/>
	</enumdef>

	<enumdef type="CTaskVehicleDeadDriver::BrakeControl">
		<enumval name="BC_Retain"/>
		<enumval name="BC_Minimum"/>
		<enumval name="BC_Maximum"/>
		<enumval name="BC_Randomize"/>
	</enumdef>

	<enumdef type="CTaskVehicleDeadDriver::HandBrakeControl">
		<enumval name="HBC_Retain"/>
		<enumval name="HBC_Minimum"/>
		<enumval name="HBC_Maximum"/>
		<enumval name="HBC_Randomize"/>
	</enumdef>

	<structdef type="CTaskVehicleDeadDriver::Tunables" base="CTuning">
		<float name="m_SwerveTime" min="0.0f" max="20.0f" step="1.0f" init="8.0f"/>
		<enum name="m_SteerAngleControl" type="CTaskVehicleDeadDriver::SteerAngleControl" init="CTaskVehicleDeadDriver::SAC_Randomize"/>
		<float name="m_MinSteerAngle" min="-1.0f" max="0.0f" step="0.1f" init="-0.5f"/>
		<float name="m_MaxSteerAngle" min="0.0f" max="1.0f" step="0.1f" init="0.5f"/>
		<enum name="m_ThrottleControl" type="CTaskVehicleDeadDriver::ThrottleControl" init="CTaskVehicleDeadDriver::TC_Randomize"/>
		<float name="m_MinThrottle" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
		<float name="m_MaxThrottle" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<enum name="m_BrakeControl" type="CTaskVehicleDeadDriver::BrakeControl" init="CTaskVehicleDeadDriver::BC_Randomize"/>
		<float name="m_MinBrake" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxBrake" min="0.0f" max="1.0f" step="0.01f" init="0.095f"/>
		<enum name="m_HandBrakeControl" type="CTaskVehicleDeadDriver::HandBrakeControl" init="CTaskVehicleDeadDriver::HBC_Retain"/>
	</structdef>

	<structdef type="CTaskVehicleGoToPointWithAvoidanceAutomobile::Tunables" base="CTuning" onPostLoad="OnPostLoad">
		<float name="m_TailgateDistanceMax" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
		<float name="m_TailgateIdealDistanceMin" min="0.0f" max="100.0f" step="1.0f" init="1.0f"/>
		<float name="m_TailgateIdealDistanceMax" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
		<float name="m_TailgateSpeedMultiplierMin" min="0.0f" max="5.0f" step="0.1f" init="0.8f"/>
		<float name="m_TailgateSpeedMultiplierMax" min="0.0f" max="5.0f" step="0.1f" init="1.2f"/>
		<float name="m_TailgateVelocityMin" min="0.0f" max="100.0f" step="1.0f" init="1.0f"/>
		<float name="m_ChanceOfPedSeeingCarFromBehind" min="0.0f" max="1.0f" step="0.01f" init="0.02f" description="Chances of a ped seeing a car coming from behind them."/>
		<float name="m_MinSpeedForAvoid" min="0.0f" max="100.0f" step="1.0f" init="2.0f" description="The speed of the car that will map to the minimum avoidance distance."/>
		<float name="m_MinDistanceForAvoid" min="0.0f" max="100.0f" step="1.0f" init="2.0f" description="The minimum distance that peds will start their avoidance maneuver."/>
		<float name="m_MaxSpeedForAvoid" min="0.0f" max="100.0f" step="1.0f" init="15.0f" description="The speed of the car that will map to the maximum avoidance distance."/>
		<float name="m_MaxDistanceForAvoid" min="0.0f" max="100.0f" step="1.0f" init="10.0f" description="The maximum distance that peds will start their avoidance maneuver."/>
		<float name="m_MinDistanceForAvoidDirected" min="0.0f" max="100.0f" step="0.1f" init="0.0f"/>
		<float name="m_MinSpeedForAvoidDirected" min="0.0f" max="100.0f" step="0.1f" init="1.0f"/>
		<float name="m_MaxDistanceForAvoidDirected" min="0.0f" max="100.0f" step="0.1f" init="1.5f"/>
		<float name="m_MaxSpeedForAvoidDirected" min="0.0f" max="100.0f" step="0.1f" init="10.0f"/>
		<float name="m_MaxAbsDotForAvoidDirected" min="0.0f" max="1.0f" step="0.01f" init="0.707f"/>
		<float name="m_MaxSpeedForBrace" min="0.0f" max="100.0f" step="1.0f" init="15.0f" description="The maximum speed at which peds will brace themselves."/>
		<float name="m_MinSpeedForDive" min="0.0f" max="100.0f" step="1.0f" init="10.0f" description="The minimum speed at which peds will dive."/>
		<float name="m_MinTimeToConsiderDangerousDriving" min="0.0f" max="60.0f" step="0.01f" init="1.0f"/>
		<float name="m_MultiplierForDangerousDriving" min="0.0f" max="5.0f" step="0.01f" init="1.2f"/>
		<float name="m_MinDistanceToSideOnPavement" min="0.0f" max="20.0f" step="0.1f" init="0.0f"/>
		<float name="m_MaxDistanceToSideOnPavement" min="0.0f" max="20.0f" step="0.1f" init="2.0f"/>
	</structdef>

	<structdef type="CTaskVehicleCrash::Tunables" base="CTuning">
		<float name="m_MinSpeedForWreck" min="0.0f" max="100.0f" step="1.0f" init="2.0f"/>
	</structdef>
	
	<structdef type="CTaskVehicleHeliProtect::Tunables" base="CTuning">
		<float name="m_minSpeedForSlowDown" min="0.0f" max="200.0f" step="1.0f" init="10.0f"/>
		<float name="m_maxSpeedForSlowDown" min="0.0f" max="200.0f" step="1.0f" init="50.0f"/>
		<float name="m_minSlowDownDistance" min="0.0f" max="200.0f" step="1.0f" init="25.0f"/>
		<float name="m_maxSlowDownDistance" min="0.0f" max="200.0f" step="1.0f" init="125.0f"/>
	</structdef>

  <structdef type="CTaskVehicleBlock::Tunables" base="CTuning">
    <float name="m_DistanceToCapSpeed" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <float name="m_DistanceToStartCappingSpeed" min="0.0f" max="50.0f" step="1.0f" init="50.0f"/>
    <float name="m_AdditionalSpeedCap" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_MaxDistanceFromTargetToForceStraightLineMode" min="0.0f" max="50.0f" step="1.0f" init="25.0f"/>
    <float name="m_TimeToLookAhead" min="0.0f" max="3.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinDistanceToLookAhead" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_MinDotTargetMovingTowardsUsToStartBackAndForth" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinDotTargetMovingTowardsOurSideToStartBackAndForth" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinDotTargetMovingTowardsUsToContinueBackAndForth" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_MinDotTargetMovingTowardsUsToStartBrakeInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinDotMovingTowardsTargetToStartBrakeInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinDotTargetMovingTowardsUsToContinueBrakeInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_MinDotMovingTowardsTargetToContinueBrakeInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_MinDotTargetMovingTowardsUsToStartCruiseInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinDotMovingAwayFromTargetToStartCruiseInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_MinDotTargetMovingTowardsUsToContinueCruiseInFront" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
  </structdef>

	<structdef type="CTaskVehicleBlockCruiseInFront::Tunables::Probes::Collision">
		<float name="m_HeightAboveGround" min="0.0f" max="3.0f" step="0.01f" init="0.5f"/>
		<float name="m_SpeedForMinLength" min="0.0f" max="50.0f" step="0.01f" init="0.0f"/>
		<float name="m_SpeedForMaxLength" min="0.0f" max="50.0f" step="0.01f" init="10.0f"/>
		<float name="m_MinLength" min="0.0f" max="50.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxLength" min="0.0f" max="50.0f" step="0.01f" init="15.0f"/>
	</structdef>

	<structdef type="CTaskVehicleBlockCruiseInFront::Tunables::Probes">
		<struct name="m_Collision" type="CTaskVehicleBlockCruiseInFront::Tunables::Probes::Collision"/>
	</structdef>

  <structdef type="CTaskVehicleBlockCruiseInFront::Tunables::Rendering">
    <bool name="m_Enabled" init="false"/>
    <bool name="m_Probe" init="true"/>
    <bool name="m_ProbeResults" init="true"/>
    <bool name="m_CollisionReflectionDirection" init="true"/>
    <bool name="m_CollisionReflectedTargetPosition" init="true"/>
  </structdef>

  <structdef type="CTaskVehicleBlockCruiseInFront::Tunables" base="CTuning">
    <struct name="m_Probes" type="CTaskVehicleBlockCruiseInFront::Tunables::Probes"/>
    <struct name="m_Rendering" type="CTaskVehicleBlockCruiseInFront::Tunables::Rendering"/>
    <float name="m_StraightLineDistance" min="0.0f" max="100.0f" step="1.0f" init="30.0f"/>
    <float name="m_TimeToLookAhead" min="0.0f" max="2.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinDistanceToLookAhead" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_MinDotForSlowdown" min="-1.0f" max="1.0f" step="0.1f" init="0.98f"/>
    <float name="m_MinDistanceForSlowdown" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <float name="m_MaxDistanceForSlowdown" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
    <float name="m_CruiseSpeedMultiplierForMinSlowdown" min="0.0f" max="5.0f" step="0.01f" init="0.75f"/>
    <float name="m_CruiseSpeedMultiplierForMaxSlowdown" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_IdealDistance" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
    <float name="m_MinDistanceToAdjustSpeed" min="0.0f" max="100.0f" step="1.0f" init="5.0f"/>
    <float name="m_MaxDistanceToAdjustSpeed" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
    <float name="m_MinCruiseSpeedMultiplier" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
    <float name="m_MaxCruiseSpeedMultiplier" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
  </structdef>

  <structdef type="CTaskVehicleBlockBrakeInFront::Tunables" base="CTuning">
    <float name="m_TimeAheadForGetInPosition" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
    <float name="m_MinOffsetForGetInPosition" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_TimeAheadForBrake" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
    <float name="m_TimeAheadForBrakeOnWideRoads" min="0.0f" max="5.0f" step="0.1f" init="2.5f"/>
    <float name="m_MaxTimeForBrake" min="0.0f" max="5.0f" step="0.1f" init="2.5f"/>
    <float name="m_FutureDistanceForMinSteerAngle" min="0.0f" max="50.0f" step="1.0f" init="1.0f"/>
    <float name="m_FutureDistanceForMaxSteerAngle" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_MaxSpeedToUseHandBrake" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
	<float name="m_MinDotToClampCruiseSpeed" min="-1.0f" max="1.0f" step="0.01f" init="0.97f"/>
    <float name="m_MaxDistanceToClampCruiseSpeed" min="0.0f" max="500.0f" step="1.0f" init="150.0f"/>
	<float name="m_MaxCruiseSpeedWhenClamped" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
  </structdef>

  <structdef type="CTaskVehicleBlockBackAndForth::Tunables" base="CTuning">
    <float name="m_ThrottleMultiplier" min="0.0f" max="1.0f" step="0.1f" init="0.15f"/>
  </structdef>

	<structdef type="CTaskVehicleGoToBoat::Tunables" base="CTuning">
		<float name="m_SlowdownDistance" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
		<float name="m_RouteArrivalDistance" min="0.0f" max="100.0f" step="1.0f" init="2.0f"/>
		<float name="m_RouteLookAheadDistance" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
	</structdef>

	<structdef type="CTaskVehicleFleeBoat::Tunables" base="CTuning">
		<float name="m_FleeDistance" min="0.0f" max="1000.0f" step="10.0f" init="50.0f"/>
	</structdef>

	<structdef type="CTaskVehicleCruiseBoat::Tunables" base="CTuning">

		<float name="m_fTimeToPickNewPoint" min="0.0f" max="1000.0f" step="10.0f" init="20.0f"/>
		<float name="m_fDistToPickNewPoint" min="0.0f" max="1000.0f" step="10.0f" init="30.0f"/>
		<float name="m_fDistSearch" min="0.0f" max="1000.0f" step="10.0f" init="150.0f"/>

	</structdef>
	


	<structdef type="CTaskVehicleGoToHelicopter::Tunables" base="CTuning">
		<float name="m_slowDistance" min="0.0f" max="1000.0f" step="1.0f" init="100.0f"/>
		<float name="m_maxCruiseSpeed" min="0.0f" max="500.0f" step="1.0f" init="50.0f"/>
		
		<float name="m_maxPitchRoll" min="0.0f" max="2.0f" step="0.05f" init="1.0f"/>
		<float name="m_maxThrottle" min="0.0f" max="2.0f" step="0.05f" init="1.0f"/>
		
		<float name="m_leanKp" min="-1.0f" max="1.0f" step="0.001f" init="0.1f"/>
		<float name="m_leanKi" min="-1.0f" max="1.0f" step="0.001f" init="0.0f"/>
		<float name="m_leanKd" min="-1.0f" max="1.0f" step="0.001f" init="0.05f"/>
		<float name="m_yawKp" min="-1.0f" max="1.0f" step="0.001f" init="1.0f"/>
		<float name="m_yawKi" min="-1.0f" max="1.0f" step="0.001f" init="0.0f"/>
		<float name="m_yawKd" min="-1.0f" max="1.0f" step="0.001f" init="0.05f"/>
		<float name="m_throttleKp" min="-10.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_throttleKi" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
		<float name="m_throttleKd" min="-10.0f" max="10.0f" step="0.01f" init="0.1f"/>
		
		<float name="m_whiskerForwardTestDistance" min="0.0f" max="100.0f" step="1.0f" init="35.0f"/>
		<float name="m_whiskerForwardSpeedScale" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
		<float name="m_whiskerLateralTestDistance" min="0.0f" max="100.0f" step="1.0f" init="35.0f"/>
		<float name="m_whiskerVerticalTestDistance" min="0.0f" max="100.0f" step="1.0f" init="25.0f"/>
		<float name="m_whiskerTestAngle" min="1.0f" max="200.0f" step="1.0f" init="40.0f"/>
		<float name="m_avoidHeadingChangeSpeed" min="0.0f" max="360.0f" step="1.0f" init="180.0f"/>
		<float name="m_avoidHeadingJump" min="0.0f" max="200.0f" step="1.0f" init="30.0f"/>
		<float name="m_avoidPitchChangeSpeed" min="0.0f" max="360.0f" step="1.0f" init="180.0f"/>
		<float name="m_avoidPitchJump" min="0.0f" max="200.0f" step="1.0f" init="60.0f"/>
		<float name="m_avoidLockDuration" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_downAvoidLockDurationMaintain" min="0.0f" max="10.0f" step="0.1f" init="0.25f"/>
    <float name="m_avoidMinFarExtension" min="0.0f" max="100.0f" step="0.1f" init="1.0f"/>
		<float name="m_avoidForwardExtraOffset" min="0.0f" max="100.0f" step="0.1f" init="2.0f"/>
    <float name="m_maintainHeightMaxZDelta" min="0.0f" max="100.0f" step="0.1f" init="10.0f"/>
    <float name="m_downHitZDeltaBuffer" min="-10.0f" max="0.0f" step="0.1f" init="-0.75f"/>
    <int name="m_numHeightmapFutureSamples" min="0" max="50" step="1" init="10"/> 
		<float name="m_futureHeightmapSampleTime" min="0.0f" max="50.0f" step="0.5f" init="0.5f"/>
		<float name="m_DistanceXYToUseHeightMapAvoidance" min="0.0f" max="500.0f" step="1.0f" init="150.0f"/>

		<float name="m_TimesliceMinDistToTarget" min="0.0f" max="1000.0f" step="1.0f" init="15.0f"/>
		<u32 name="m_TimesliceTimeAfterAvoidanceMs" min="0" max="60000" step="1000" init="5000"/>

	</structdef>

  <structdef type="CTaskVehicleGoToSubmarine::Tunables" base="CTuning">
    <float name="m_forwardProbeRadius" min="0.0f" max="5.0f" step="0.1f" init="0.5f"/>
    <float name="m_maxForwardSlowdownDist" min="0.0f" max="50.0f" step="1.0f" init="12.0f"/>
    <float name="m_minForwardSlowdownDist" min="0.0f" max="50.0f" step="1.0f" init="3.0f"/>
    <float name="m_maxReverseSpeed" min="-50.0f" max="10.0f" step="1.0f" init="-1.0f"/>
    <float name="m_wingProbeTimeDistance" min="0.0f" max="20.0f" step="0.5f" init="4.0f"/>
    <float name="m_downDiveScaler" min="-10.0f" max="10.0f" step="0.1f" init="2.5f"/>
    <float name="m_maxPitchAngle" min="0.0f" max="90.0f" step="0.5f" init="15.0f"/>
    <float name="m_slowdownDist" min="0.0f" max="100.0f" step="0.5f" init="20.0f"/>
    <float name="m_forwardProbeVelScaler" min="0.0f" max="10.0f" step="0.1f" init="0.0f"/>
  </structdef>

  <structdef type="CTaskVehicleLandPlane::Tunables" base="CTuning">

		<float name="m_SlowDownDistance" min="0.0f" max="1000.0f" step="5.0f" init="100.0f"/>
		<float name="m_TimeOnGroundToDrive" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<float name="m_HeightToStartLanding" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
		<float name="m_LandSlopeNoseUpMin" min="0.0f" max="1.0f" step="0.1f" init="0.10f"/>
		<float name="m_LandSlopeNoseUpMax" min="0.0f" max="1.0f" step="0.1f" init="0.05f"/>

	</structdef>

	<structdef type="CTaskVehiclePlaneChase::Tunables" base="CTuning">
		
		<float name="MinSpeed" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
		<float name="MaxSpeed" min="0.0f" max="1000.0f" step="1.0f" init="500.0f"/>
		
	</structdef> 

	<structdef type="CTaskVehicleGoToPlane::Tunables" base="CTuning">

		<int name="m_numFutureSamples" min="0" max="100" step="1" init="8"/>
		<float name="m_futureSampleTime" min="0.0f" max="100.0f" step="0.05f" init="0.5f"/>		
		
		<float name="m_maxDesiredAngleYawDegrees" min="0.0f" max="180.0f" step="0.5f" init="15.0f"/>
		<float name="m_maxDesiredAnglePitchDegrees" min="0.0f" max="180.0f" step="0.5f" init="15.0f"/>
		<float name="m_maxDesiredAngleRollDegrees" min="0.0f" max="180.0f" step="0.5f" init="15.0f"/>
		
		<float name="m_angleToTargetDegreesToNotUseMinRadius" min="0.0f" max="180.0f" step="0.5f" init="45.0f"/>

		<float name="m_minMinDistanceForRollComputation" min="0.0f" max="200.0f" step="5.0f" init="20.0f"/>
		<float name="m_maxMinDistanceForRollComputation" min="0.0f" max="200.0f" step="5.0f" init="100.0f"/>


		<float name="m_maxYaw" min="0.0f" max="2.0f" step="0.05f" init="1.0f"/>
		<float name="m_maxPitch" min="0.0f" max="2.0f" step="0.05f" init="1.0f"/>
		<float name="m_maxRoll" min="0.0f" max="2.0f" step="0.05f" init="1.0f"/>
		<float name="m_maxThrottle" min="0.0f" max="100.0f" step="0.05f" init="2.0f"/>

		<float name="m_yawKp" min="-1.0f" max="1.0f" step="0.001f" init="1.0f"/>
		<float name="m_yawKi" min="-1.0f" max="1.0f" step="0.001f" init="0.0f"/>
		<float name="m_yawKd" min="-1.0f" max="1.0f" step="0.001f" init="0.05f"/>		
		
		<float name="m_pitchKp" min="-1.0f" max="1.0f" step="0.001f" init="0.03f"/>
		<float name="m_pitchKi" min="-1.0f" max="1.0f" step="0.001f" init="0.0f"/>
		<float name="m_pitchKd" min="-1.0f" max="1.0f" step="0.001f" init="-0.0001f"/>
 
		<float name="m_rollKp" min="-10.0f" max="10.0f" step="0.01f" init="0.1f"/>
		<float name="m_rollKi" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
		<float name="m_rollKd" min="-10.0f" max="10.0f" step="0.01f" init="0.05f"/>		
		
		<float name="m_throttleKp" min="-10.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_throttleKi" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
		<float name="m_throttleKd" min="-10.0f" max="10.0f" step="0.01f" init="0.1f"/>

	</structdef>	

  <structdef type="CTaskVehicleParkNew::Tunables" base="CTuning">
    <float name="m_ParkingSpaceBlockedWaitTimePerAttempt" min="0.0f" max="120.0f" step="0.1f" init="4.0f" description="If tests are enabled to see if the parking space is blocked, this is the number of seconds we wait until we do the next test."/>
    <u8 name="m_ParkingSpaceBlockedMaxAttempts" min="1" max="100" step="1" init="4" description="If tests are enabled to see if the parking space is blocked, this is the number of times we wait for the parking space to become unblocked before failing the task."/>
  </structdef>

  <structdef type="CTaskVehicleRam::Tunables" base="CTuning">
    <float name="m_BackOffTimer" min="0.0f" max="10.0f" step="0.1f" init="5.0f"/>
    <float name="m_MinBackOffDistance" min="0.0f" max="50.0f" step="1.0f" init="0.0f"/>
    <float name="m_MaxBackOffDistance" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
    <float name="m_CruiseSpeedMultiplierForMinBackOffDistance" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_CruiseSpeedMultiplierForMaxBackOffDistance" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
  </structdef>

  <structdef type="CTaskVehicleSpinOut::Tunables" base="CTuning">
    <float name="m_TimeToLookAhead" min="0.0f" max="3.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinDistanceToLookAhead" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_BumperOverlapForMaxSpeed" min="-20.0f" max="5.0f" step="1.0f" init="-10.0f"/>
    <float name="m_BumperOverlapForMinSpeed" min="-20.0f" max="5.0f" step="1.0f" init="1.0f"/>
    <float name="m_CatchUpSpeed" min="0.0f" max="10.0f" step="1.0f" init="5.0f"/>
    <float name="m_BumperOverlapToBeInPosition" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_MaxSidePaddingForTurn" min="0.0f" max="5.0f" step="0.1f" init="2.5f"/>
    <float name="m_TurnTime" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_InvMassScale" min="0.0f" max="3.0f" step="0.1f" init="1.0f"/>
  </structdef>
  
	<structdef type="CTaskVehiclePursue::Tunables::Drift">
		<float name="m_MinValueForCorrection" min="0.0f" max="5.0f" step="0.01f" init="0.35f"/>
		<float name="m_MaxValueForCorrection" min="0.0f" max="5.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinRate" min="0.0f" max="1.0f" step="0.01f" init="0.15f"/>
		<float name="m_MaxRate" min="0.0f" max="1.0f" step="0.01f" init="0.35f"/>
	</structdef>

  <structdef type="CTaskVehiclePursue::Tunables" base="CTuning">
    <struct name="m_DriftX" type="CTaskVehiclePursue::Tunables::Drift"/>
    <struct name="m_DriftY" type="CTaskVehiclePursue::Tunables::Drift"/>
    <float name="m_TimeToLookBehind" min="0.0f" max="3.0f" step="0.1f" init="0.5f"/>
    <float name="m_MinDistanceToLookBehind" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_SpeedDifferenceForMinDistanceToStartMatchingSpeed" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_SpeedDifferenceForMaxDistanceToStartMatchingSpeed" min="0.0f" max="50.0f" step="1.0f" init="40.0f"/>
    <float name="m_MinDistanceToStartMatchingSpeed" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
    <float name="m_MaxDistanceToStartMatchingSpeed" min="0.0f" max="100.0f" step="1.0f" init="75.0f"/>
    <float name="m_CruiseSpeedMultiplierForBackOff" min="0.0f" max="1.0f" step="0.01f" init="0.75f"/>
    <float name="m_DotToClampSpeedToMinimum" min="-1.0f" max="1.0f" step="0.01f" init="0.707f"/>
    <float name="m_DotToClampSpeedToMaximum" min="-1.0f" max="1.0f" step="0.01f" init="0.9f"/>
    <float name="m_SpeedForMinimumDot" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
    <float name="m_TimeBetweenLineOfSightChecks" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_DistanceForStraightLineModeAlways" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
    <float name="m_DistanceForStraightLineModeIfLos" min="0.0f" max="100.0f" step="1.0f" init="50.0f"/>
  </structdef>

	<structdef type="CTaskVehicleShotTire::Tunables" base="CTuning">
		<float name="m_MaxTimeInSwerve" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
		<float name="m_MinSpeedInSwerve" min="0.0f" max="10.0f" step="0.1f" init="0.1f"/>
		<float name="m_MinSpeedToApplyTorque" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
		<float name="m_MaxDotToApplyTorque" min="-1.0f" max="1.0f" step="0.1f" init="0.85f"/>
		<float name="m_TorqueMultiplier" min="0.0f" max="1000.0f" step="1.0f" init="1.0f"/>
	</structdef>

	<structdef type="CTaskVehicleApproach::Tunables" base="CTuning">
		<float name="m_MaxDistanceAroundClosestRoadNode" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
	</structdef>

	<structdef type="CTaskVehicleFlee::Tunables" base="CTuning">
		<float name="m_ChancesForSwerve" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_MinSpeedForSwerve" min="0.0f" max="100.0f" step="1.0f" init="15.0f"/>
		<float name="m_MinTimeToSwerve" min="0.0f" max="5.0f" step="0.1f" init="0.2f"/>
		<float name="m_MaxTimeToSwerve" min="0.0f" max="5.0f" step="0.1f" init="0.3f"/>
		<float name="m_ChancesForHesitate" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
		<float name="m_MaxSpeedForHesitate" min="0.0f" max="100.0f" step="1.0f" init="5.0f"/>
		<float name="m_MinTimeToHesitate" min="0.0f" max="5.0f" step="0.1f" init="0.3f"/>
		<float name="m_MaxTimeToHesitate" min="0.0f" max="5.0f" step="0.1f" init="0.7f"/>
    <float name="m_ChancesForJitter" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_MinTimeToJitter" min="0.0f" max="5.0f" step="0.1f" init="0.4f"/>
    <float name="m_MaxTimeToJitter" min="0.0f" max="5.0f" step="0.1f" init="0.7f"/>
  </structdef>

	<structdef type="CTaskVehicleMissionBase::Tunables" base="CTuning">
		<u32 name="m_MinTimeToKeepEngineAndLightsOnWhileParked" min="0" step="1" init="10" description="Time in seconds vehicle must be at a Park scenario node to turn off its lights/engine"/>
	</structdef>

	<structdef type="CTaskVehiclePullAlongside::Tunables" base="CTuning">
		<float name="m_TimeToLookAhead" min="0.0f" max="3.0f" step="0.1f" init="0.5f"/>
		<float name="m_MinDistanceToLookAhead" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
		<float name="m_OverlapSpeedMultiplier" min="0.0f" max="10.0f" step="0.01f" init="0.75f"/>
		<float name="m_MaxSpeedDifference" min="0.0f" max="10.0f" step="0.01f" init="5.0f"/>
    <float name="m_AlongsideBuffer" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
	</structdef>

</ParserSchema>
