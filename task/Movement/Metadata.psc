<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint::Bonus">
		<float name="m_Current" min="1.0f" max="5.0f" step="0.01f" init="3.0f"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint::Penalty::BadRoute">
		<float name="m_ValueForMin" min="0.0f" max="10.0f" step="0.01f" init="0.05f"/>
		<float name="m_ValueForMax" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_Min" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_Max" min="0.0f" max="1.0f" step="0.01f" init="0.05f"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint::Penalty">
		<struct name="m_BadRoute" type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint::Penalty::BadRoute"/>
		<float name="m_Arc" min="0.0f" max="1.0f" step="0.01f" init="0.05f"/>
		<float name="m_LineOfSight" min="0.0f" max="1.0f" step="0.01f" init="0.01f"/>
		<float name="m_Nearby" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint">
		<float name="m_Base" min="0.0f" max="5.0f" step="0.01f" init="2.0f"/>
		<struct name="m_Bonus" type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint::Bonus"/>
		<struct name="m_Penalty" type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint::Penalty"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint::Bonus">
		<float name="m_Current" min="1.0f" max="5.0f" step="0.01f" init="3.0f"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint::Penalty::BadRoute">
		<float name="m_ValueForMin" min="0.0f" max="10.0f" step="0.01f" init="0.05f"/>
		<float name="m_ValueForMax" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
		<float name="m_Min" min="0.0f" max="1.0f" step="0.01f" init="0.25f"/>
		<float name="m_Max" min="0.0f" max="1.0f" step="0.01f" init="0.05f"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint::Penalty">
		<struct name="m_BadRoute" type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint::Penalty::BadRoute"/>
		<float name="m_LineOfSight" min="0.0f" max="1.0f" step="0.01f" init="0.01f"/>
		<float name="m_Nearby" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint">
		<float name="m_Base" min="0.0f" max="5.0f" step="0.01f" init="1.0f"/>
		<struct name="m_Bonus" type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint::Bonus"/>
		<struct name="m_Penalty" type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint::Penalty"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring::Position">
		<float name="m_MaxDistanceFromPed" min="0.0f" max="100.0f" step="1.0f" init="50.0f"/>
		<float name="m_ValueForMaxDistanceFromPed" min="0.0f" max="1.0f" step="0.1f" init="0.5f"/>
		<float name="m_ValueForMinDistanceFromPed" min="0.0f" max="1.0f" step="0.1f" init="1.0f"/>
		<float name="m_MaxDistanceFromOptimal" min="0.0f" max="50.0f" step="1.0f" init="20.0f"/>
		<float name="m_ValueForMaxDistanceFromOptimal" min="0.0f" max="1.0f" step="0.1f" init="0.5f"/>
		<float name="m_ValueForMinDistanceFromOptimal" min="0.0f" max="1.0f" step="0.1f" init="1.0f"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables::Scoring">
		<struct name="m_CoverPoint" type="CTaskMoveToTacticalPoint::Tunables::Scoring::CoverPoint"/>
		<struct name="m_NavMeshPoint" type="CTaskMoveToTacticalPoint::Tunables::Scoring::NavMeshPoint"/>
		<struct name="m_Position" type="CTaskMoveToTacticalPoint::Tunables::Scoring::Position"/>
	</structdef>

	<structdef type="CTaskMoveToTacticalPoint::Tunables" base="CTuning">
		<struct name="m_Scoring" type="CTaskMoveToTacticalPoint::Tunables::Scoring"/>
		<float name="m_TargetRadiusForMoveToPosition" min="0.0f" max="3.0f" step="0.1f" init="0.5f"/>
		<float name="m_TimeUntilRelease" min="0.0f" max="15.0f" step="1.0f" init="8.0f"/>
		<float name="m_MaxDistanceToConsiderCloseToPositionToMoveTo" min="0.0f" max="10.0f" step="1.0f" init="3.0f"/>
		<float name="m_TimeBetweenInfluenceSphereChecks" min="0.0f" max="10.0f" step="1.0f" init="0.5f"/>
	</structdef>
	
  <structdef type="CTaskRappel::Tunables"  base="CTuning">
    <float name="m_fJumpDescendRate" min="0.0f" max="10.0f" init="3.4f" description="The speed at which the jump anim descends"/>
    <float name="m_fLongJumpDescendRate" min="0.0f" max="15.0f" init="6.0f" description="The speed at which the jump anim descends when using long jump"/>
    <float name="m_fJumpToSmashWindowPhaseChange" min="0.0f" max="1.0f" init="0.65f" description="The amount we change our jump phase to blend into our smash window"/>
    <float name="m_fMinJumpPhaseAllowDescend" min="0.0f" max="1.0f" init=".07f" description="The min phase needed in order to descend while jumping"/>
    <float name="m_fMaxJumpPhaseAllowDescend" min="0.0f" max="1.0f" init=".76f" description="The max phase needed in order to descend while jumping"/>
    <float name="m_fMinJumpPhaseAllowSmashWindow" min="0.0f" max="1.0f" init="0.0f" description="The min phase needed in order to smash window while jumping"/>
    <float name="m_fMaxJumpPhaseAllowSmashWindow" min="0.0f" max="1.0f" init="0.65f" description="The max phase needed in order to smash window while jumping"/>
    <float name="m_fMinSmashWindowPhase" min="0.0f" max="1.0f" init=".325f" description="The min phase needed in order to smash any found window"/>
    <float name="m_fGlassBreakRadius" min="0.0f" max="10.0f" init=".75f" description="The radius to use when smashing glass during the smash window state"/>
    <float name="m_fGlassDamage" min="0.0f" max="10000.0f" init="800.0f" description="The damage to use when smashing glass during the smash window state"/>
    <float name="m_fMinDistanceToBreakWindow" min="0.0f" max="2.0f" init="0.425f" description="How close the ped needs to be in order to smash the window"/>
    <float name="m_fMinStickValueAllowDescend" min="-1.0f" max="0.0f" init="-.1f" description="How much the player has to press the stick to descend"/>
    <bool name="m_bAllowSmashDuringJump" init="true"/>
  </structdef>
  
  <structdef type="CTaskHeliPassengerRappel::Tunables"  base="CTuning">
    <float name="m_fDefaultRopeLength" min="0.0f" max="100.0f" init="40.0f" description="The length of the rope if we don't hit ground with our probe"/>
    <float name="m_fExtraRopeLength" min="0.0f" max="20.0f" init="5.0f" description="The length of rope we add onto the distance from heli to ground"/>
    <float name="m_fExitDescendRate" min="0.0f" max="10.0f" init="0.8f" description="How much we move the rope when playing the exit anims"/>
    <float name="m_fDefaultDescendRate" min="0.0f" max="15.0f" init="5.5f" description="The rope descend rate for peds"/>
    <float name="m_fStartDescendingDistToTargetSq" min="0.0f" max="1000.0f" init="6.25f" description="How close we want to be to the target before starting the descend"/>
    <float name="m_fRopeUnwindRate" min="0.0f" max="1000.0f" init="12.5f"/>
    <float name="m_fMinHeightToRappel" min="0.0f" max="1000.0f" init="1.75f"/>
    <float name="m_fMaxHeliSpeedForRappel" min="0.0f" max="1000.0f" init="1.5f"/>
  </structdef>

  <structdef type="CTaskParachute::Tunables::ChangeRatesForSkydiving" >
    <float name="m_Pitch" min="0.0f" max="3.14f" init="2.0f" />
    <float name="m_Roll" min="0.0f" max="3.14f" init="1.5f" />
    <float name="m_Yaw" min="0.0f" max="3.14f" init="1.0f" />
    <float name="m_Heading" min="0.0f" max="3.14f" init="2.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ChangeRatesForParachuting" >
    <float name="m_Pitch" min="0.0f" max="3.14f" init="1.5f" />
    <float name="m_Roll" min="0.0f" max="3.14f" init="1.5f" />
    <float name="m_Yaw" min="0.0f" max="3.14f" init="1.0f" />
    <float name="m_Brake" min="0.0f" max="5.0f" init="2.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::FlightAngleLimitsForSkydiving" >
    <float name="m_MinPitch" min="-1.0f" max="0.0f" init="-0.75f" />
    <float name="m_InflectionPitch" min="0.0f" max="1.0f" init="0.15f" />
    <float name="m_MaxPitch" min="0.0f" max="1.0f" init="0.75f" />
    <float name="m_MaxRoll" min="0.0f" max="1.0f" init="0.85f" />
    <float name="m_MaxYaw" min="0.0f" max="1.0f" init="0.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::FlightAngleLimitsForParachuting" >
    <float name="m_MinPitch" min="-1.0f" max="1.0f" init="-0.75f" />
    <float name="m_MaxPitch" min="-1.0f" max="1.0f" init="0.75f" />
    <float name="m_MaxRollFromStick" min="0.0f" max="1.0f" init="0.5f" />
    <float name="m_MaxRollFromBrake" min="0.0f" max="1.0f" init="1.0f" />
    <float name="m_MaxRoll" min="0.0f" max="1.0f" init="1.0f" />
    <float name="m_MaxYawFromStick" min="0.0f" max="1.0f" init="0.0f" />
    <float name="m_MaxYawFromRoll" min="0.0f" max="1.0f" init="0.0f" />
    <float name="m_RollForMinYaw" min="0.0f" max="1.0f" init="0.0f" />
    <float name="m_RollForMaxYaw" min="0.0f" max="1.0f" init="0.0f" />
    <float name="m_MaxYaw" min="0.0f" max="1.0f" init="0.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::PedAngleLimitsForSkydiving" >
    <float name="m_MinPitch" min="-3.14f" max="0.0f" init="-1.5f" />
    <float name="m_MaxPitch" min="0.0f" max="3.14f" init="0.0f" />
  </structdef>

	<structdef type="CTaskParachute::Tunables::MoveParameters::Parachuting::InterpRates" >
		<float name="m_StickX" min="0.0f" max="5.0f" init="1.0f" />
		<float name="m_StickY" min="0.0f" max="5.0f" init="1.0f" />
		<float name="m_TotalStickInput" min="0.0f" max="5.0f" init="1.0f" />
		<float name="m_CurrentHeading" min="0.0f" max="5.0f" init="1.0f" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::MoveParameters::Parachuting" >
		<struct name="m_InterpRates" type="CTaskParachute::Tunables::MoveParameters::Parachuting::InterpRates" />
		<float name="m_MinParachutePitch" min="-1.0f" max="1.0f" init="-0.75f" />
		<float name="m_MaxParachutePitch" min="-1.0f" max="1.0f" init="0.75f" />
		<float name="m_MaxParachuteRoll" min="0.0f" max="1.0f" init="1.0f" />
	</structdef>
	
	<structdef type="CTaskParachute::Tunables::MoveParameters" >
		<struct name="m_Parachuting" type="CTaskParachute::Tunables::MoveParameters::Parachuting" />
	</structdef>

  <structdef type="CTaskParachute::Tunables::ForcesForSkydiving" >
    <float name="m_MaxThrust" min="0.0f" max="10.0f" init="3.0f" />
    <float name="m_MaxLift" min="-10.0f" max="0.0f" init="-5.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ParachutingAi::Target" >
    <float name="m_MinDistanceToAdjust" min="0.0f" max="100.0f" init="20.0f" />
    <float name="m_Adjustment" min="0.0f" max="100.0f" init="5.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ParachutingAi::Brake" >
    <float name="m_MaxDistance" min="0.0f" max="250.0f" init="100.0f" />
    <float name="m_DistanceToStart" min="0.0f" max="100.0f" init="10.0f" />
    <float name="m_DistanceForFull" min="0.0f" max="100.0f" init="5.0f" />
    <float name="m_AngleForMin" min="-3.14f" max="3.14f" init="-0.5f" />
    <float name="m_AngleForMax" min="-3.14f" max="3.14f" init="-0.75f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ParachutingAi::Roll" >
    <float name="m_AngleDifferenceForMin" min="0.0f" max="6.28f" init="0.05f" />
    <float name="m_AngleDifferenceForMax" min="0.0f" max="6.28f" init="0.75f" />
    <float name="m_StickValueForMin" min="0.0f" max="1.0f" init="0.25f" />
    <float name="m_StickValueForMax" min="0.0f" max="1.0f" init="1.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ParachutingAi::Pitch" >
		<float name="m_DesiredTimeToResolveAngleDifference" min="0.0f" max="5.0f" init="2.0f" />
		<float name="m_DeltaForMaxStickChange" min="0.0f" max="1.0f" init="0.25f" />
		<float name="m_MaxStickChangePerSecond" min="0.0f" max="5.0f" init="2.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ParachutingAi::Drop" >
    <float name="m_MinDistance" min="0.0f" max="25.0f" init="1.0f" />
    <float name="m_MaxDistance" min="0.0f" max="25.0f" init="5.0f" />
    <float name="m_MinHeight" min="0.0f" max="25.0f" init="0.5f" />
    <float name="m_MaxHeight" min="0.0f" max="25.0f" init="5.0f" />
    <float name="m_MaxDot" min="-1.0f" max="1.0f" init="0.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ParachutingAi" >
    <struct name="m_Target" type="CTaskParachute::Tunables::ParachutingAi::Target" />
    <struct name="m_Brake" type="CTaskParachute::Tunables::ParachutingAi::Brake" />
    <struct name="m_RollForNormal" type="CTaskParachute::Tunables::ParachutingAi::Roll" />
    <struct name="m_RollForBraking" type="CTaskParachute::Tunables::ParachutingAi::Roll" />
    <struct name="m_PitchForNormal" type="CTaskParachute::Tunables::ParachutingAi::Pitch" />
    <struct name="m_PitchForBraking" type="CTaskParachute::Tunables::ParachutingAi::Pitch" />
    <struct name="m_Drop" type="CTaskParachute::Tunables::ParachutingAi::Drop" />
  </structdef>

	<structdef type="CTaskParachute::Tunables::Landing::NormalThreshold" >
		<float name="m_Forward" min="0.0f" max="1.0f" init="1.0f" />
		<float name="m_Collision" min="0.0f" max="1.0f" init="1.0f" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::Landing::NormalThresholds" >
		<struct name="m_Normal" type="CTaskParachute::Tunables::Landing::NormalThreshold" />
		<struct name="m_Braking" type="CTaskParachute::Tunables::Landing::NormalThreshold" />
	</structdef>

  <structdef type="CTaskParachute::Tunables::Landing" >
    <struct name="m_NormalThresholds" type="CTaskParachute::Tunables::Landing::NormalThresholds" />
    <float name="m_MaxVelocityForSlow" min="0.0f" max="20.0f" init="9.0f" />
    <float name="m_MinVelocityForFast" min="0.0f" max="20.0f" init="13.0f" />
    <float name="m_ParachuteProbeRadius" min="0.0f" max="1.0f" init="0.25f" />
    <float name="m_MinStickMagnitudeForEarlyOutMovement" min="0.0f" max="1.0f" init="0.2f" />
    <float name="m_FramesToLookAheadForProbe" min="0.0f" max="5.0f" init="1.5f" />
    <float name="m_BlendDurationForEarlyOut" min="0.0f" max="10.0f" init="0.125f" />
    <float name="m_AngleForRunway" min="0.0f" max="1.57f" init="0.0f" />
    <float name="m_LookAheadForRunway" min="0.0f" max="10.0f" init="5.0f" />
    <float name="m_DropForRunway" min="0.0f" max="10.0f" init="5.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::CrashLanding" >
    <float name="m_NoParachuteTimeForMinCollisionNormalThreshold" min="0.0f" max="10.0f" init="4.0f" />
    <float name="m_NoParachuteMaxCollisionNormalThreshold" min="-1.0f" max="1.0f" init="0.707f" />
    <float name="m_NoParachuteMinCollisionNormalThreshold" min="-1.0f" max="1.0f" init="0.0f" />
    <float name="m_NoParachuteMaxPitch" min="-3.14f" max="3.14f" init="-0.79f" />
    <float name="m_ParachuteProbeRadius" min="0.0f" max="1.0f" init="0.25f" />
    <float name="m_ParachuteUpThreshold" min="-1.0f" max="1.0f" init="0.75f" />
    <float name="m_FramesToLookAheadForProbe" min="0.0f" max="5.0f" init="1.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::Allow" >
    <float name="m_MinClearDistanceBelow" min="0.0f" max="100.0f" init="15.0f" />
    <float name="m_MinFallingSpeedInRagdoll" min="-30.0f" max="0.0f" init="-10.0f" />
    <float name="m_MinTimeInRagdoll" min="0.0f" max="10.0f" init="1.0f" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::CameraSettings" >
    <string name="m_SkyDivingCamera" type="atHashString" />
    <string name="m_ParachuteCamera" type="atHashString" />
    <string name="m_ParachuteCloseUpCamera" type="atHashString" />
  </structdef>

  <structdef type="CTaskParachute::Tunables::ParachutePhysics" >
    <float name="m_ParachuteInitialVelocityY" min="0.0f" max="50.0f" init="5.0f" />
    <float name="m_ParachuteInitialVelocityZ" min="-50.0f" max="0.0f" init="-5.0f" />
  </structdef>

	<structdef type="CTaskParachute::Tunables::ExtraForces::Parachuting::Normal" >
		<struct name="m_TurnFromStick" type="CTaskParachute::Tunables::ExtraForces::FromStick" />
		<struct name="m_TurnFromRoll" type="CTaskParachute::Tunables::ExtraForces::FromValue" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::ExtraForces::Parachuting::Braking" >
		<struct name="m_TurnFromStick" type="CTaskParachute::Tunables::ExtraForces::FromStick" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::ExtraForces::Value" >
		<float name="m_X" min="-100.0f" max="100.0f" init="0.0f" />
		<float name="m_Y" min="-100.0f" max="100.0f" init="0.0f" />
		<float name="m_Z" min="-100.0f" max="100.0f" init="0.0f" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::ExtraForces::FromValue" >
		<float name="m_ValueForMin" min="-1.0f" max="1.0f" init="0.0f" />
		<float name="m_ValueForMax" min="-1.0f" max="1.0f" init="0.0f" />
		<struct name="m_MinValue" type="CTaskParachute::Tunables::ExtraForces::Value" />
		<struct name="m_ZeroValue" type="CTaskParachute::Tunables::ExtraForces::Value" />
		<struct name="m_MaxValue" type="CTaskParachute::Tunables::ExtraForces::Value" />
		<bool name="m_IsLocal" init="false" />
		<bool name="m_ClearZ" init="false" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::ExtraForces::FromStick" >
		<struct name="m_FromValue" type="CTaskParachute::Tunables::ExtraForces::FromValue" />
		<bool name="m_UseVerticalAxis" init="false" />
	</structdef>
	
	<structdef type="CTaskParachute::Tunables::ExtraForces::Parachuting" >
		<struct name="m_Normal" type="CTaskParachute::Tunables::ExtraForces::Parachuting::Normal" />
		<struct name="m_Braking" type="CTaskParachute::Tunables::ExtraForces::Parachuting::Braking" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::ExtraForces" >
		<struct name="m_Parachuting" type="CTaskParachute::Tunables::ExtraForces::Parachuting" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::Rendering" >
		<bool name="m_Enabled" init="false" />
		<bool name="m_RunwayProbes" init="true" />
		<bool name="m_ValidityProbes" init="true" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::LowLod" >
		<float name="m_MinDistance" min="0.0f" max="500.0f" init="75.0f" />
		<bool name="m_AlwaysUse" init="false" />
		<bool name="m_NeverUse" init="false" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::ParachuteBones::Attachment" >
		<float name="m_X" min="-1.0f" max="1.0f" init="0.0f" />
		<float name="m_Y" min="-1.0f" max="1.0f" init="0.0f" />
		<float name="m_Z" min="-1.0f" max="1.0f" init="0.0f" />
		<bool name="m_UseOrientationFromParachuteBone" init="false" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::ParachuteBones" >
		<struct name="m_LeftGrip" type="CTaskParachute::Tunables::ParachuteBones::Attachment"/>
		<struct name="m_RightGrip" type="CTaskParachute::Tunables::ParachuteBones::Attachment"/>
		<struct name="m_LeftWire" type="CTaskParachute::Tunables::ParachuteBones::Attachment"/>
		<struct name="m_RightWire" type="CTaskParachute::Tunables::ParachuteBones::Attachment"/>
	</structdef>

	<structdef type="CTaskParachute::Tunables::Aiming" >
		<bool name="m_Disabled" init="false"/>
	</structdef>

	<structdef type="CTaskParachute::Tunables::PadShake::Falling" >
		<u32 name="m_Duration" min="0" max="5000" init="0" />
		<float name="m_PitchForMinIntensity" min="-1.0f" max="1.0f" init="-1.0f" />
		<float name="m_PitchForMaxIntensity" min="-1.0f" max="1.0f" init="1.0f" />
		<float name="m_MinIntensity" min="0.0f" max="1.0f" init="0.0f" />
		<float name="m_MaxIntensity" min="0.0f" max="1.0f" init="0.0f" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::PadShake::Deploy" >
		<u32 name="m_Duration" min="0" max="5000" init="0" />
		<float name="m_Intensity" min="0.0f" max="1.0f" init="0.0f" />
	</structdef>

	<structdef type="CTaskParachute::Tunables::PadShake" >
		<struct name="m_Falling" type="CTaskParachute::Tunables::PadShake::Falling"/>
		<struct name="m_Deploy" type="CTaskParachute::Tunables::PadShake::Deploy"/>
	</structdef>

  <structdef type="CTaskParachute::PedVariation"  >
    <enum name="m_Component" type="ePedVarComp" init="PV_COMP_INVALID"/>
    <u32 name="m_DrawableId" min="0" max="100" step="1" init="0"/>
	  <u32 name="m_DrawableAltId" min="0" max="100" step="1" init="0"/>
    <u32 name="m_TexId" min="0" max="100" step="1" init="0"/>
  </structdef>

	<structdef type="CTaskParachute::PedVariationSet"  >
		<enum name="m_Component" type="ePedVarComp" init="PV_COMP_INVALID"/>
		<array name="m_DrawableIds" type="atArray">
			<u32 min="0" max="100" step="1" init="0" />
		</array>
	</structdef>

	<structdef type="CTaskParachute::ParachutePackVariation"  >
		<array name="m_Wearing" type="atArray">
			<struct type="CTaskParachute::PedVariationSet"/>
		</array>
		<struct name="m_ParachutePack" type="CTaskParachute::PedVariation"/>
	</structdef>

	<structdef type="CTaskParachute::ParachutePackVariations"  >
		<string name="m_ModelName" type="atHashString"/>
		<array name="m_Variations" type="atArray">
			<struct type="CTaskParachute::ParachutePackVariation"/>
		</array>
	</structdef>

  <structdef type="CTaskParachute::Tunables"  base="CTuning">
    <struct name="m_ChangeRatesForSkydiving" type="CTaskParachute::Tunables::ChangeRatesForSkydiving" />
    <struct name="m_ChangeRatesForParachuting" type="CTaskParachute::Tunables::ChangeRatesForParachuting" />
    <struct name="m_FlightAngleLimitsForSkydiving" type="CTaskParachute::Tunables::FlightAngleLimitsForSkydiving" />
    <struct name="m_FlightAngleLimitsForParachutingNormal" type="CTaskParachute::Tunables::FlightAngleLimitsForParachuting" />
    <struct name="m_FlightAngleLimitsForParachutingBraking" type="CTaskParachute::Tunables::FlightAngleLimitsForParachuting" />
    <struct name="m_PedAngleLimitsForSkydiving" type="CTaskParachute::Tunables::PedAngleLimitsForSkydiving" />
    <struct name="m_MoveParameters" type="CTaskParachute::Tunables::MoveParameters" />
    <struct name="m_ForcesForSkydiving" type="CTaskParachute::Tunables::ForcesForSkydiving" />
    <struct name="m_ParachutingAi" type="CTaskParachute::Tunables::ParachutingAi" />
    <struct name="m_Landing" type="CTaskParachute::Tunables::Landing" />
    <struct name="m_CrashLanding" type="CTaskParachute::Tunables::CrashLanding" />
    <struct name="m_Allow" type="CTaskParachute::Tunables::Allow" />
    <struct name="m_CameraSettings" type="CTaskParachute::Tunables::CameraSettings" />
    <struct name="m_ParachutePhysics" type="CTaskParachute::Tunables::ParachutePhysics" />
    <struct name="m_ExtraForces" type="CTaskParachute::Tunables::ExtraForces" />
    <struct name="m_Rendering" type="CTaskParachute::Tunables::Rendering" />
    <struct name="m_LowLod" type="CTaskParachute::Tunables::LowLod" />
    <struct name="m_ParachuteBones" type="CTaskParachute::Tunables::ParachuteBones" />
    <struct name="m_Aiming" type="CTaskParachute::Tunables::Aiming" />
    <struct name="m_PadShake" type="CTaskParachute::Tunables::PadShake" />
    <float name="m_BrakingDifferenceForLinearVZMin" min="0.0f" max="1.0f" init="0.75f" />
    <float name="m_BrakingDifferenceForLinearVZMax" min="0.0f" max="1.0f" init="1.0f" />
    <float name="m_LinearVZForBrakingDifferenceMin" min="0.0f" max="500000.0f" init="0.0f" />
    <float name="m_LinearVZForBrakingDifferenceMax" min="0.0f" max="500000.0f" init="50000.0f" />
    <float name="m_PitchRatioForLinearVZMin" min="0.0f" max="1.0f" init="0.75f" />
    <float name="m_PitchRatioForLinearVZMax" min="0.0f" max="1.0f" init="1.0f" />
    <float name="m_LinearVZForPitchRatioMin" min="0.0f" max="500000.0f" init="0.0f" />
    <float name="m_LinearVZForPitchRatioMax" min="0.0f" max="500000.0f" init="50000.0f" />
    <float name="m_MinBrakeForCloseUpCamera" min="0.0f" max="1.0f" init="0.9f" />
    <float name="m_ParachuteMass" min="0.0f" max="50.0f" init="20.0f" />
    <float name="m_ParachuteMassReduced" min="0.0f" max="50.0f" init="1.0f" />
    <float name="m_MaxTimeToLookAheadForFutureTargetPosition" min="0.0f" max="50.0f" init="3.0f" />
    <float name="m_MaxDifferenceToAverageBrakes" min="0.0f" max="1.0f" init="0.25f" />
    <string name="m_ModelForParachuteInSP" type="atHashString" />
    <string name="m_ModelForParachuteInMP" type="atHashString" />
	<array name="m_ParachutePackVariations" type="atArray">
		  <struct type="CTaskParachute::ParachutePackVariations" />
	</array>
	<Vector3 name="m_FirstPersonDriveByIKOffset" init="0.0f, 0.0f, 0.0f" />
  </structdef>

  <structdef type="CTaskParachuteObject::Tunables"  base="CTuning">
    <float name="m_PhaseDuringDeployToConsiderOut" min="0.0f" max="1.0f" init="0.5f" />
  </structdef>

  <structdef type="CTaskMoveWithinAttackWindow::Tunables"  base="CTuning">
    <float name="m_fMaxAngleOffset" min="0.0f" max="1.0f" step="0.05f" init="0.3927f" description="The max left/right we'll offset our target position from our target"/>
    <float name="m_fMinAlliesForMaxAngleOffset" min="1.0f" max="10.0f" step="0.1f" init="4.0f" description="Less allies gives us a smaller angle offset"/>
    <float name="m_fMaxAllyDistance" min="0.0f" max="100.0f" step="0.1f" init="10.0f" description="The max distance an ally must be in order to consider them for our offset"/>
    <float name="m_fMaxRandomAdditionalOffset" min="0.0f" max="0.25f" step="0.01f" init="0.05f" description="Additional random offset so our peds are not evenly spaced"/>
    <float name="m_fMaxRouteDistanceModifier" min="0.0f" max="5.0f" step="0.1f" init="1.5f" description="The modifier we apply to our ped->target distance when comparing against route"/>
    <float name="m_fMinTimeToWait" min="0.0f" max="5.0f" step="0.1f" init="1.75f" description="The min time we want to wait before checking again for a position to move to"/>
    <float name="m_fMaxTimeToWait" min="0.0f" max="5.0f" step="0.1f" init="2.25f" description="The max time we want to wait before checking again for a position to move to"/>
  </structdef>

  <structdef type="CTaskFlyToPoint::Tunables"  base="CTuning">
    <float name="m_HeightMapDelta" min="0.0f" max="50.0f" step="0.5f" init="2.0f" description="The min distance to stay above the heightmap."/>
    <float name="m_HeightMapLookAheadDist" min="0.0f" max="50.0f" step="0.1f" init="5.0f" description="How far to look ahead of the bird on the heightmap when checking for desired flight heights."/>
    <float name="m_InitialTerrainAvoidanceAngleD" min="0.0f" max="90.0f" step="0.1f" init="45.0f" description="When turning to avoid terrain, this is the initial separation from the target."/>
    <float name="m_ProgressiveTerrainAvoidanceAngleD" min="0.0f" max="45.0f" step="0.1f" init="15.0f" description="How much to increase the avoidance angle over time if you are still in danger of collision."/>
    <float name="m_TimeBetweenIncreasingAvoidanceAngle" min="0.0f" max="10.0f" step="0.1f" init="5.0f" description="How long between increasing the avoidance angle."/>
  </structdef>

  <structdef type="CTaskMoveCrossRoadAtTrafficLights::WaitingOffset"  >
    <Vector3 name="m_Pos"/>
  </structdef>

  <structdef type="CTaskMoveCrossRoadAtTrafficLights::Tunables"  base="CTuning">
    <bool name="m_bTrafficLightPositioning" init="false"/>
    <array name="m_WaitingOffsets" type="atArray">
       <struct type="CTaskMoveCrossRoadAtTrafficLights::WaitingOffset"/>
    </array>
    <u32 name="m_iMaxPedsAtTrafficLights" min="0" max="100" step="1" init="4"/>
    <float name="m_fMinDistanceBetweenPeds" min="-10.0f" max="10.0f" step="0.05f" init="1.5f" description=""/>
	<float name="m_fDecideToRunChance" min="0.0f" max="100.0f" step="0.01f" init="100.0f" description=""/>
	<float name="m_fPlayerObstructionCheckRadius" min="0.0f" max="10.0f" step="0.1f" init="2.25f" description="Distance to goal to checking for player obstruction and stop."/>
	<float name="m_fPlayerObstructionRadius" min="0.0f" max="10.0f" step="0.01f" init="1.0f" description="Local player obstructs goals within this radius."/>
    <bool name="m_bDebugRender" init="false"/>
  </structdef>
 
  <structdef type="CTaskFall::Tunables"  base="CTuning">
    <float name="m_ImmediateHighFallSpeedPlayer" min="-100.0" max="0.0" init="-3.0" description="Falling speed to kick the player ped immediately into high fall, assuming the ground probe returns false"/>
    <float name="m_ImmediateHighFallSpeedAi" min="-100.0" max="0.0" init="-3.0" description="Falling speed to kick the ai ped immediately into high fall, assuming the ground probe returns false"/>
    <float name="m_HighFallProbeLength" min="0.0f" max="100.0" init="6.0" description="ground probe length for switch to high fall (if no ground is found by this probe, high fall will be activated"/>
    <float name="m_ContinuousGapHighFallSpeed" min="-10.0" max="10.0f" init="0.0f" description=""/>
    <float name="m_ContinuousGapHighFallTime" min="0.0f" max="10.0" init="0.25" description="Time the gap must exist before starting high fall"/>
    <float name="m_DeferFallBlockTestAngle" min="0.0f" max="90.0" init="60.0f" description=""/>
    <float name="m_DeferFallBlockTestDistance" min="0.0f" max="20.0" init="2.0f" description=""/>
    <float name="m_DeferFallBlockTestRadius" min="0.0f" max="5.0" init="0.25f" description=""/>
    <float name="m_DeferHighFallTime" min="0.0f" max="10.0" init="0.33f" description="Time the gap must exist before starting high fall"/>
    <float name="m_InAirHeadingRate" min="0.0f" max="100.0" init="3.0f" description="In Air heading update rate"/>
    <float name="m_InAirMovementRate" min="0.0f" max="100.0" init="4.0f" description="Max in air velocity"/>
    <float name="m_InAirMovementApproachRate" min="0.0f" max="100.0" init="5.0f" description="Time to match stick velocity"/>
    <float name="m_LandHeadingModifier" min="0.0f" max="100.0" init="3.5f" description="Land heading update rate"/>
    <float name="m_StandingLandHeadingModifier" min="0.0f" max="100.0" init="2.0f" description="Land heading update rate"/>
    <float name="m_FallLandThreshold" min="0.0f" max="100.0" init="0.2f" description="Threshold to begin landing"/>
    <float name="m_ReenterFallLandThreshold" min="0.0f" max="100.0" init="0.5f" description="Time to match stick velocity"/>
    <float name="m_PadShakeMinIntensity" min="0.0f" max="1.0" init="0.1f" description="Time to match stick velocity"/>
    <float name="m_PadShakeMaxIntensity" min="0.0f" max="1.0" init="0.6f" description="Time to match stick velocity"/>
    <float name="m_PadShakeMinHeight" min="0.0f" max="100.0" init="0.25f" description="Time to match stick velocity"/>
    <float name="m_PadShakeMaxHeight" min="0.0f" max="100.0" init="8.0f" description="Time to match stick velocity"/>
    <u32 name="m_PadShakeMinDuration" min="0" max="1000" step="1" init="125"/>
    <u32 name="m_PadShakeMaxDuration" min="0" max="1000" step="1" init="200"/>
	<float name="m_SuperJumpIntensityMult" min="0.0f" max="100.0" init="1.25f" description="Time to match stick velocity"/>
	<float name="m_SuperJumpDurationMult" min="0.0f" max="100.0" init="1.25f" description="Time to match stick velocity"/>
    <float name="m_VaultFallTestAngle" min="0.0f" max="45.0" init="10.0f" description="Angle to do vault fall test"/>
    <float name="m_JumpFallTestAngle" min="0.0f" max="45.0" init="10.0f" description="Angle to do just fall test"/>
    <float name="m_FallTestAngleBlendOutTime" min="0.0f" max="45.0" init="0.5f" description="Angle to do just fall parachute test"/>
    <float name="m_DiveControlMaxFallDistance" min="0.0f" max="500.0f" init="100.0f" description="max fall distance for dive control blend"/>
    <float name="m_DiveControlExtraDistanceForDiveFromVehicle" min="0.0f" max="100.0" init="20.0f" description="extra distance for dive control blend"/>
    <float name="m_DiveControlExtraDistanceBlendOutSpeed" min="0.0f" max="100.0" init="10.0f" description="time to blend out extra distance"/>
    <float name="m_DiveWaterOffsetToHitFullyInControlWeight" min="0.0f" max="100.0" init="0.5f" description="z offset from water to hit fully in control dive"/>
    <float name="m_LandRollHeightFromJump" min="0.0f" max="20.0" init="2.75f" description="Fall height to do a land roll from jump"/>
    <float name="m_LandRollHeightFromVault" min="0.0f" max="20.0" init="3.0f" description="Fall height to do a land roll from vault"/>
    <float name="m_LandRollHeight" min="0.0f" max="20.0" init="3.0f" description="Fall height to do a land roll"/>
	<float name="m_LandRollSlopeThreshold" min="0.0f" max="20.0" init="0.55f" description="Fall slope z to do a land roll"/>
	<float name="m_HighFallWaterDepthMin" min="0.0" max="10.0" init="1.0f" description=""/>
	<float name="m_HighFallWaterDepthMax" min="0.0" max="10.0" init="3.0f" description=""/>
	<float name="m_HighFallExtraHeightWaterDepthMin" min="0.0" max="100.0f" init="1.25f" description=""/>
	<float name="m_HighFallExtraHeightWaterDepthMax" min="0.0" max="100.0f" init="5.0f" description=""/>
  </structdef>

	<structdef type="CTaskTakeOffPedVariation::Tunables"  base="CTuning">
	</structdef>

  <structdef type="CTaskVault::Tunables"  base="CTuning">
    <float name="m_AngledClimbTheshold" min="0.0f" max="1000.0f" init="5.0f" description="Angle to trigger angled climb"/>
    <float name="m_MinAngleForScaleVelocityExtension" min="0.0f" max="1000.0f" init="5.0f" description="Min Angle to extend scale velocity phase"/>
    <float name="m_MaxAngleForScaleVelocityExtension" min="0.0f" max="1000.0f" init="40.0f" description="Max Angle to extend scale velocity phase"/>
    <float name="m_AngledClimbScaleVelocityExtensionMax" min="0.0f" max="1000.0f" init="0.8f" description="Angle to trigger angled climb"/>
    <float name="m_DisableVaultForwardDot" min="-1.0f" max="1.0f" init="0.5f" description="Forward test direction of stick to disable vaulting in preference to standing up"/>
    <float name="m_SlideWalkAnimRate" min="0.0f" max="2.0f" init="0.9f" description="Anim rate of slide from walk"/>
  </structdef>

  <structdef type="CTaskJump::Tunables"  base="CTuning">
    <float name="m_MinSuperJumpInitialVelocity" min="0.0f" max="1000.0f" init="18.0f" description="Super jump velocity - TO DO convert to height if need be"/>
    <float name="m_MaxSuperJumpInitialVelocity" min="0.0f" max="1000.0f" init="30.0f" description="Super jump velocity - TO DO convert to height if need be"/>
	<float name="m_MinBeastJumpInitialVelocity" min="0.0f" max="1000.0f" init="22.0f" description="Beast jump velocity - TO DO convert to height if need be"/>
	<float name="m_MaxBeastJumpInitialVelocity" min="0.0f" max="1000.0f" init="38.0f" description="Beast jump velocity - TO DO convert to height if need be"/>
    <float name="m_HighJumpMinAngleForVelScale" min="0.0f" max="1000.0f" init="12.5f" description=""/>
    <float name="m_HighJumpMaxAngleForVelScale" min="0.0f" max="1000.0f" init="30.0f" description=""/>
    <float name="m_HighJumpMinVelScale" min="0.0f" max="1000.0f" init="0.0f" description=""/>
    <float name="m_HighJumpMaxVelScale" min="0.0f" max="1000.0f" init="10.0f" description=""/>
    <bool name="m_DisableJumpOnSteepStairs" init="true"/>
    <float name="m_MaxStairsJumpAngle" min="0.0f" max="90.0f" init="20.0f" description="Stair angle to disable jumps at"/>
    <bool name="m_bEnableJumpCollisions" init="true"/>
    <bool name="m_bEnableJumpCollisionsMp" init="false"/>
    <bool name="m_bBlockJumpCollisionAgainstRagdollBlocked" init="true"/>
    <float name="m_PredictiveProbeZOffset" min="-10.0f" max="10.0f" init="0.0f" description="The amount to z offset the start of the jump probe from the peds position"/>
    <float name="m_PredictiveBraceStartDelay" min="0.0f" max="10.0f" init="0.15f" description="Time into the jump task that the predictive brace can be triggered (when a collision is predicted)"/>
    <float name="m_PredictiveBraceProbeLength" min="0.0f" max="10.0f" init="1.2f" description="Distance to probe when searching for walls when looking to pre-emptively activate the ragdoll"/>
    <float name="m_PredictiveBraceBlendInDuration" min="0.0f" max="1.0f" init="0.125f" description="Blend in duration of the brace anim"/>
    <float name="m_PredictiveBraceBlendOutDuration" min="0.0f" max="1.0f" init="0.125f" description="Blend out duration of the brace anim"/>
    <float name="m_PredictiveBraceMaxUpDotSlope" min="-1.0f" max="1.0f" init="0.5f" description="When on slopes, ignore contacts where the poly normal is too vertical"/>
    <float name="m_PredictiveRagdollIntersectionDot" min="-1.0f" max="1.0f" init="-0.5f" description="Intersection dot to dissalow ragdolling when angle of incidence is too severe"/>
    <float name="m_PredictiveRagdollStartDelay" min="0.0f" max="10.0f" init="0.25f" description="Time into the jump task that the predictive ragdoll can be triggered (when a wall collision is predicted)"/>
    <float name="m_PredictiveRagdollProbeLength" min="0.0f" max="10.0f" init="1.5f" description="Distance to probe when searching for walls when looking to pre-emptively activate the ragdoll"/>
    <float name="m_PredictiveRagdollProbeRadius" min="0.0f" max="10.0f" init="0.25f" description="With of the probe when searching for walls when looking to pre-emptively activate the ragdoll"/>
    <float name="m_PredictiveRagdollRequiredVelocityMag" min="0.0f" max="10.0f" init="4.0f" description="Velocity required of ped to pre-emptively activate ragdoll"/>
  </structdef>

  <structdef type="CTaskGetUp::Tunables"  base="CTuning">
    <float name="m_fPreferInjuredGetupPlayerHealthThreshold" min="0.0f" max="1000.0f" init="115.0f" description="If the players health is less than this, always prefer injured getups over the standard player ones."/>
    <float name="m_fInjuredGetupImpulseMag2" min="0.0f" max="1000000.0f" init="20.0f" description="If the ragdoll takes an impact with a magnitude greater than this, prefer an injured getup when blending out."/>
	  <float name="m_fMinTimeInGetUpToAllowCover" min="0.0f" max="2.0f" init="0.5f" description="The minimum time in the get up task to cache the cover input to force the player into cover asap."/>
    <bool name="m_AllowNonPlayerHighFallAbort" init="false" description="Allow fallnig ragdoll abort on non players"/>
    <bool name="m_AllowOffScreenHighFallAbort" init="false" description="Allow falling ragdoll abort when offscreen"/>
    <int name="m_FallTimeBeforeHighFallAbort" min="0" max="10000" init="500" description="If the ped falls for more than this amount of time (ms) whilst performing the getup, switch to an nm high fall behaviour (if possible)"/>
    <float name="m_MinFallSpeedForHighFallAbort" min="-100.0f" max="0.0f" init="-1.0f" description="The rate at which the ped must be falling to abort the getup into high fall."/>
    <float name="m_MinHeightAboveGroundForHighFallAbort" min="0.0f" max="10.0f" init="0.2f" description="The minimum height above the ground the ped has to be to do a high fall abort."/>
    <float name="m_PlayerMoverFixupMaxExtraHeadingChange" min="0.0f" max="200.0f" init="1.0f" description="The rate at which the extra heading change lerps towards the desired."/>
    <float name="m_AiMoverFixupMaxExtraHeadingChange" min="0.0f" max="200.0f" init="20.0f" description="The rate at which the extra heading change lerps towards the desired."/>
    <int name="m_StartClipWaitTimePlayer" min="0" max="10000" init="300" description="The time for the player to wait between getup attempts when stuck"/>
    <int name="m_StartClipWaitTime" min="0" max="10000" init="1000" description="The time for the ai to wait between getup attempts when stuck"/>
    <int name="m_StuckWaitTime" min="0" max="10000" init="4000" description="The maximum time for players to remain stuck before warping"/>
    <int name="m_StuckWaitTimeMp" min="0" max="10000" init="3000" description="The maximum time for mp players to remain stuck before warping"/>
  </structdef>


  <structdef type="CTaskMoveFollowNavMesh::Tunables" base="CTuning">
		<u8 name="uRepeatedAttemptsBeforeTeleportToLeader" init="3"/>
	</structdef>

	<structdef type="CTaskGoToScenario::Tunables" base="CTuning">
		<float name="m_ClosePointDistanceSquared" min="0.0f" max="100.0f" init="1.5625" description="How far away from the point in 2D space is considered close for error checking."/>
		<float name="m_ClosePointCounterMax" min="0.0f" max="100.0f" init="9.0f" description="How long (in seconds) to stay close to a point while going towards it before calling this task a failure."/>
		<float name="m_HeadingDiffStartBlendDegrees" min="0.0f" max="180.0f" init="40.0f" description="How far away the ped's heading can be away from the target heading (in degrees) before this ped can blend out."/>
		<float name="m_PositionDiffStartBlend" min="0.0f" max="1.0f" init="0.5f" description="How far away the ped's position can be from the target position before this ped can blend out."/>
		<float name="m_ExactStopTargetRadius" min="0.0f" max="2.0f" init="0.1f" description="Target radius when trying to precisely achieve a scenario point on land."/>
		<float name="m_PreferNearWaterSurfaceArrivalRadius" min="0.0f" max="100.0f" init="2.0f" description="Target radius when trying to achieve a scenario point when the PREFER_NEAR_WATER_SURFACE nav capability is set."/>
		<float name="m_TimeBetweenBrokenPointChecks" min="0.0f" max="100.0f" init="1.0f" description="Time between checks to see if the scenario point has broken up or been uprooted."/>
  </structdef>

	<structdef type="CTaskComplexEvasiveStep::Tunables"  base="CTuning">
		<float name="m_BlendOutDelta" min="-16.0f" max="0.0f" step="0.1f" init="-4.0f"/>
	</structdef>

  <structdef type="CTaskJetpack::Tunables::HoverControlParams" cond="ENABLE_JETPACK">
    <bool name="m_bGravityDamping" init="true" description=""/>
    <bool name="m_bDoRelativeToCam" init="false" description=""/>
    <bool name="m_bAntiGravityThrusters" init="true" description=""/>
    <bool name="m_bAutoLand" init="true" description=""/>
    <bool name="m_bSyncHeadingToCamera" init="false" description=""/>
    <bool name="m_bSyncHeadingToDesired" init="true" description=""/>
    <float name="m_fGravityDamping" min="-10.0f" max="10.0f" init="-0.1f" />
    <float name="m_fGravityDecelerationModifier" min="-10.0f" max="10.0f" init="-5.5f" />
    <float name="m_fHorThrustModifier" min="0.0f" max="100.0f" init="15.0f" />
    <float name="m_fForThrustModifier" min="0.0f" max="100.0f" init="15.0f" />
    <float name="m_fVerThrustModifier" min="0.0f" max="100.0f" init="20.0f" />
    <float name="m_fCameraHeadingModifier" min="0.0" max="50.0f" init="10.0f" />
    <float name="m_fDesiredHeadingModifier" min="0.0" max="50.0f" init="1.0f" />
    <float name="m_fDesiredHeadingModifierBoost" min="0.0" max="50.0f" init="0.75f" />
    <float name="m_fMinPitchForBoost" min="0.0" max="1.0f" init="0.5f" />
    <float name="m_fThrustLerpRate" min="0.0f" max="100.0f" init="0.05f" />
    <float name="m_fPitchLerpRate" min="0.0f" max="100.0f" init="0.035f" />
	  <float name="m_fRollLerpRate" min="0.0f" max="100.0f" init="0.1f" />
    <float name="m_fThrustLerpOutRate" min="0.0f" max="100.0f" init="0.100f" />
    <float name="m_fPitchLerpOutRate" min="0.0f" max="100.0f" init="0.070f" />
    <float name="m_fRollLerpOutRate" min="0.0f" max="100.0f" init="0.150f" />
    <float name="m_fAngVelocityLerpRate" min="0.0f" max="1.0f" init="0.1f" />
    <float name="m_fBoostApproachRateIn" min="0.0f" max="100.0f" init="1.0f" />
    <float name="m_fBoostApproachRateOut" min="0.0f" max="100.0f" init="1.0f" />
    <float name="m_MaxIdleAnimVelocity" min="0.0f" max="100.0f" init="0.5f" />
    <float name="m_fMinPitchForHeadingChange" min="-1.0f" max="1.0f" init="-0.3f" />
    <float name="m_fMinYawForHeadingChange" min="-1.0f" max="1.0f" init="0.1f" />
    <Vector3 name="m_vMinStrafeSpeedMinThrust" init="6.0f,6.0f,4.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMaxStrafeSpeedMinThrust" init="6.0f,10.0f,4.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMinStrafeSpeedMaxThrust" init="10.0f,10.0f,10.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMaxStrafeSpeedMaxThrust" init="10.0f,15.0f,10.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMinStrafeSpeedMinThrustBoost" init="10.0f, 10.0f,10.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMaxStrafeSpeedMinThrustBoost" init="10.0f,50.0f,10.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMinStrafeSpeedMaxThrustBoost" init="10.0f, 20.0f,20.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMaxStrafeSpeedMaxThrustBoost" init="10.0f,100.0f,20.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMaxStrafeAccel" init="35.0f,45.0f,35.0f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_vMinStrafeAccel" init="35.0f,45.0f,35.0f" min="0.0f" max="100.0f"/>
    <float name="m_fLandHeightThreshold" min="0.0f" max="100.0f" init="0.2f" />
    <float name="m_fTimeNearGroundToLand" min="0.0f" max="100.0f" init="0.25f" />
    <float name="m_fMaxAngularVelocityZ" min="0.0f" max="100.0f" init="5.0f" />
    <float name="m_fMaxHolsterTimer" min="0.0f" max="5.0f" init="0.75f" />
    <bool name="m_bUseKillSwitch" init="false"/>
  </structdef>

  <structdef type="CTaskJetpack::Tunables::FlightControlParams" cond="ENABLE_JETPACK">
    <bool name="m_bThrustersAlwaysOn" init="false" description=""/>
    <bool name="m_bEnabled" init="false" description=""/>
    <float name="m_fThrustLerpRate" min="0.0f" max="100.0f" init="0.5f" />
    <float name="m_fPitchLerpRate" min="0.0f" max="100.0f" init="0.075f" />
    <float name="m_fRollLerpRate" min="0.0f" max="100.0f" init="0.15f" />
    <float name="m_fYawLerpRate" min="0.0f" max="100.0f" init="0.1f" />
    <float name="m_fAngVelocityLerpRate" min="0.0f" max="1.0f" init="1.0f" />
    <float name="m_fThrustModifier" min="0.0f" max="1000.0f" init="20.0f" />
    <float name="m_fTimeToFlattenVelocity" min="0.0f" max="10.0f" init="2.0f" />
    <float name="m_fMaxSpeedForMoVE" min="0.0f" max="200.0f" init="50.0f" />
    <float name="m_fMaxAngularVelocityX" min="0.0f" max="100.0f" init="1.0f" />
    <float name="m_fMaxAngularVelocityY" min="0.0f" max="100.0f" init="1.5f" />
    <float name="m_fMaxAngularVelocityZ" min="0.0f" max="100.0f" init="1.0f" />
  </structdef>

  <structdef type="CTaskJetpack::Tunables::AvoidanceSettings" cond="ENABLE_JETPACK">
    <float name="m_fAvoidanceRadius" min="0.0f" max="100.0f" init="2.0f" />
    <float name="m_fVerticalBuffer" min="0.0f" max="100.0f" init="1.5" />
    <float name="m_fArrivalTolerance" min="0.0f" max="100.0f" init="5.0f" />
    <float name="m_fAvoidanceMultiplier" min="0.0f" max="10.0f" init="0.75f" />
    <float name="m_fMinTurnAngle" min="0.0f" max="360.0f" init="20.0f" />
  </structdef>

  <structdef type="CTaskJetpack::Tunables::AimSettings" cond="ENABLE_JETPACK">
    <bool name="m_bEnabled" init="true" description=""/>
  </structdef>

  <structdef type="CTaskJetpack::Tunables::CameraSettings" cond="ENABLE_JETPACK"> 
    <string name="m_HoverCamera" type="atHashString" init="0"/>
    <string name="m_FlightCamera" type="atHashString" init="0"/>
  </structdef>

  <structdef type="CTaskJetpack::Tunables::JetpackModelData" cond="ENABLE_JETPACK">
    <string name="m_JetpackModelName" type="atHashString" init="agt_Prop_SPA_Jetpack"/>
    <Vector3 name="m_JetpackAttachOffset" init="0.02f, -0.185f, 0.0f" min="-100.0f" max="100.0f"/>
  </structdef>

  <structdef type="CTaskJetpack::Tunables::JetpackClipsets" cond="ENABLE_JETPACK">
    <string name="m_HoverClipSetHash" type="atHashString" init="jetpack_hover"/>
    <string name="m_FlightClipSetHash" type="atHashString" init="jetpack_flight"/>
  </structdef>
  
  <structdef type="CTaskJetpack::Tunables"  base="CTuning" cond="ENABLE_JETPACK">
    <struct name="m_HoverControlParams" type="CTaskJetpack::Tunables::HoverControlParams" />
    <struct name="m_FlightControlParams" type="CTaskJetpack::Tunables::FlightControlParams" />
    <struct name="m_AimSettings" type="CTaskJetpack::Tunables::AimSettings" />
    <struct name="m_CameraSettings" type="CTaskJetpack::Tunables::CameraSettings" />
    <struct name="m_JetpackModelData" type="CTaskJetpack::Tunables::JetpackModelData"/>
    <struct name="m_JetpackClipsets" type="CTaskJetpack::Tunables::JetpackClipsets"/>
    <struct name="m_AvoidanceSettings" type="CTaskJetpack::Tunables::AvoidanceSettings"/>
  </structdef>

</ParserSchema>
