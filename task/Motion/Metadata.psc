<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CTaskHumanLocomotion::Tunables::sMovingVars">
    <float name="MovingDirectionSmoothingAngleMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingAngleMax" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingRateMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingRateMaxWalk" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingRateMaxRun" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingRateAccelerationMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingRateAccelerationMax" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingForwardAngleWalk" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingForwardAngleRun" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingForwardRateMod" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingDirectionSmoothingForwardRateAccelerationMod" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingExtraHeadingChangeAngleMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingExtraHeadingChangeAngleMax" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingExtraHeadingChangeRateWalk" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingExtraHeadingChangeRateRun" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingExtraHeadingChangeRateAccelerationMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="MovingExtraHeadingChangeRateAccelerationMax" init="1.0f" min="0.0f" max="100.0f"/>
    <bool name="UseExtraHeading" init="true"/>
    <bool name="UseMovingDirectionDiff" init="true"/>
  </structdef>

  <structdef type="CTaskHumanLocomotion::Tunables::sMovingVarsSet">
    <string name="Name" type="atHashString" />
    <struct name="Standard" type="CTaskHumanLocomotion::Tunables::sMovingVars"/>
    <struct name="StandardAI" type="CTaskHumanLocomotion::Tunables::sMovingVars"/>
    <struct name="TighterTurn" type="CTaskHumanLocomotion::Tunables::sMovingVars"/>
  </structdef>

  <const name="CTaskHumanLocomotion::Tunables::MVST_Max" value="3"/>
  
  <structdef type="CTaskHumanLocomotion::Tunables" base="CTuning">
    <float name="Player_MBRAcceleration" init="4.0f" min="0.0f" max="20.0f"/>
    <float name="Player_MBRDeceleration" init="4.0f" min="0.0f" max="20.0f"/>
    <float name="AI_MBRAcceleration" init="2.0f" min="0.0f" max="20.0f"/>
    <float name="AI_MBRDeceleration" init="2.0f" min="0.0f" max="20.0f"/>
    <float name="FromStrafeAccelerationMod" init="1.0f" min="0.0f" max="20.0f"/>
    <float name="FastWalkRateMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="FastWalkRateMax" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="SlowRunRateMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="SlowRunRateMax" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="FastRunRateMin" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="FastRunRateMax" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="Turn180ActivationAngle" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="Turn180ConsistentAngleTolerance" init="0.1f" min="0.0f" max="100.0f"/>
    <float name="IdleHeadingLerpRate" init="7.5f" min="0.0f" max="20.0f"/>
    <float name="Player_IdleTurnRate" init="1.0f" min="0.0f" max="20.0f"/>
    <float name="AI_IdleTurnRate" init="1.0f" min="0.0f" max="20.0f"/>
    <float name="FromStrafe_WeightRate" init="1.3f" min="0.0f" max="20.0f"/>
    <float name="FromStrafe_MovingBlendOutTime" init="0.2f" min="0.0f" max="20.0f"/>
    <float name="IdleTransitionBlendTimeFromActionMode" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="IdleTransitionBlendTimeFromStealth" init="0.5f" min="0.0f" max="1.0f"/>
    <array name="MovingVarsSet" type="member" size="CTaskHumanLocomotion::Tunables::MVST_Max">
      <struct type="CTaskHumanLocomotion::Tunables::sMovingVarsSet"/>
    </array>
  </structdef>

  <structdef type="CTaskMotionBasicLocomotionLowLod::Tunables" base="CTuning">
    <float name="MovingExtraHeadingChangeRate" init="0.78f" min="0.0f" max="20.0f"/>
    <float name="MovingExtraHeadingChangeRateAcceleration" init="1.0f" min="0.0f" max="20.0f"/>
    <float name="m_ForceUpdatesWhenTurningStartThresholdRadians" init="0.14f" min="0.0f" max="3.14f" description="Heading delta at which we should start to force full updates (disable timeslicing), in radians."/>
    <float name="m_ForceUpdatesWhenTurningStopThresholdRadians" init="0.035f" min="0.0f" max="3.14f" description="Heading delta at which we should stop to force full updates (disable timeslicing), in radians."/>
  </structdef>

  <structdef type="CTaskHorseLocomotion::Tunables" base="CTuning" cond="ENABLE_HORSE">    

    <float name="m_InMotionAlignmentVelocityTolerance" init="2.0f" min="0.0f" max="20.0f" step="0.01f" description="When scaling animated velocity to improve path accuracy, this is the minimum turn speed that must be present in the animation before it can be scaled."/>
    <float name="m_InMotionTighterTurnsVelocityTolerance" init="0.01f" min="0.0f" max="20.0f" step="0.01f" description="When scaling animated velocity to improve path accuraacy, this is the minimum speed that must be present in the animation when galloping at tighter turning."/>    
    
    <float name="m_ProcessPhysicsApproachRate" min="0.0f" max="30.0f" step="0.01f" init="8.0f" description="Lerp rate in process physics when achieving tighter turns."/>

    <float name="m_DisableTimeslicingHeadingThresholdD" init="15.0f" min="0.0f" max="90.0f" step="0.1f" description="If the heading delta is greater than this value (in degrees), force a timeslice update so we don't run the risk of leaving our path."/>
    <float name="m_LowLodExtraHeadingAdjustmentRate" init="1.0f" min="0.0f" max="30.0f" step="0.1f" description="Lerp rate for adjusting extra heading changes per frame while in low LoD."/>

    <float name="m_PursuitModeGallopRateFactor" init="1.2f" min="0.0f" max="1.5f" step="0.01f" description="Rate to play the gallop anims when pursuing a target."/>
    <float name="m_PursuitModeExtraHeadingRate" init="4.0f" min="0.0f" max="10.0f" step="0.01f" description="Lerp rate for extra heading adjustments while pursuing a target."/>

    <float name="m_StoppingDistanceWalkMBR" init="0.3f" min="0.0f" max="10.0f" step="0.01f" description="Hacked stopping distance for QPeds using quickstops while walking."/>
    <float name="m_StoppingDistanceRunMBR" init="0.3f" min="0.0f" max="10.0f" step="0.01f" description="Hacked stopping distance for QPeds using quickstops while cantering."/>
    <float name="m_StoppingDistanceGallopMBR" init="0.3f" min="0.0f" max="10.0f" step="0.01f" description="Hacked stopping distance for QPeds using quickstops while galloping."/>
    <float name="m_StoppingGotoPointRemainingDist" init="0.5f" min="0.0f" max="20.0f" step="0.1f" description="How far the stop animation takes the ped."/>
  </structdef>

  <structdef type="CTaskRiderRearUp::Tunables" base="CTuning" cond="ENABLE_HORSE">
    <float name="m_ThrownRiderNMTaskTimeMin" init="1.5f" min="0.0f" max="10.0f" step="0.1f" description="Minimum amount of time (seconds) the natural mation task of the thrown rider will be active."/>
    <float name="m_ThrownRiderNMTaskTimeMax" init="6.0f" min="0.0f" max="10.0f" step="0.1f" description="Maximum amount of time (seconds) the natural mation task of the thrown rider will be active."/>
  </structdef>

  <structdef type="CTaskQuadLocomotion::Tunables" base="CTuning">
		<float name="m_StartAnimatedTurnsD" init="5.0f" min="0.0f" max="90.0f" step="0.01f" description="If the angle difference (in degrees) on the heading change is greater than this value while idling, start animated turns."/>
		<float name="m_StopAnimatedTurnsD" init="5.0f" min="0.0f" max="90.0f" step="0.01f" description="If the angle difference (in degrees) on the change in headings is smaller than this value while in TurnL/TurnR, stop animated turns."/>
		<float name="m_TurnTransitionDelay" init="0.1f" min="0.0f" max="20.0f" step="0.01f" description="How long to wait in the Idle state before transitioning to TurnInPlace."/>
		<float name="m_TurnToIdleTransitionDelay" init="0.5f" min="0.0f" max="30.0f" step="0.01f" description="How long to wait in the TurnInPlace state before transitioning to Idle."/>
		<float name="m_SteepSlopeStartAnimatedTurnsD" init="0.0f" min="0.0f" max="90.0f" step="0.01f" description="If the angle difference (in degrees) on the heading change is greater than this value while idling on a slope, start animated turns."/>
		<float name="m_SteepSlopeStopAnimatedTurnsD" init="0.0f" min="0.0f" max="90.0f" step="0.01f" description="If the angle difference (in degrees) on the change in headings is smaller than this value while in TurnL/TurnR, stop animated turns."/>
		<float name="m_SteepSlopeThresholdD" init="0.0f" min="0.0f" max="90.0f" step="0.01f" description="How steep is too steep to limit tight turning (in degrees)."/>
    
    
		<float name="m_InMotionAlignmentVelocityTolerance" init="2.0f" min="0.0f" max="20.0f" step="0.01f" description="When scaling animated velocity to improve path accuracy, this is the minimum turn speed that must be present in the animation before it can be scaled."/>
		<float name="m_InMotionTighterTurnsVelocityTolerance" init="0.01f" min="0.0f" max="20.0f" step="0.01f" description="When scaling animated velocity to improve path accuraacy, this is the minimum speed that must be present in the animation when galloping at tighter turning."/>
		<float name="m_InPlaceAlignmentVelocityTolerance" init="0.01f" min="0.0f" max="20.0f" step="0.01f" description="When checking if the idle turn blended out, this is the minimum turn angular speed that can be present in the anim for it to count as turning."/>

		<float name="m_TurnSpeedMBRThreshold" init="1.6f" min="0.0f" max="3.0f" step="0.1f" description="When changing direction, this is the separator between fast and slow turn blending."/>
		<float name="m_SlowMinTurnApproachRate" init="0.0f" min="0.0f" max="3.0f" step="0.1f" description="When changing direction, this is what the turn approach rate resets to when moving slowly."/>
		<float name="m_FastMinTurnApproachRate" init="0.0f" min="0.0f" max="3.0f" step="0.1f" description="When changing direction, this is what the turn approach rate resets to when moving quickly."/>
		<float name="m_SlowTurnApproachRate" init="2.0f" min="0.0f" max="10.0f" step="0.1f" description="Lerp rate to approach the desired turn phase while moving slowly."/>
		<float name="m_FastTurnApproachRate" init="2.0f" min="0.0f" max="10.0f" step="0.1f" description="Lerp rate to approach the desired turn phase while moving quickly."/>
		<float name="m_SlowTurnAcceleration" min="0.0f" max="10.0f" step="0.1f" init="3.0f" description="Rate of lerping towards the ideal heading change lerp rate when moving slow."/>
		<float name="m_FastTurnAcceleration" min="0.0f" max="10.0f" step="0.1f" init="1.0f" description="Rate of lerping towards the ideal heading change lerp rate when moving fast."/>
		<float name="m_TurnResetThresholdD" min="0.0f" max="50.0f" step="0.01f" init="0.05f" description="How big of an angle change is necessary to reset the heading lerp rate."/>
		<float name="m_ProcessPhysicsApproachRate" min="0.0f" max="30.0f" step="0.01f" init="8.0f" description="Lerp rate in process physics when achieving tighter turns."/>
    
		<float name="m_DisableTimeslicingHeadingThresholdD" init="15.0f" min="0.0f" max="90.0f" step="0.1f" description="If the heading delta is greater than this value (in degrees), force a timeslice update so we don't run the risk of leaving our path."/>
		<float name="m_LowLodExtraHeadingAdjustmentRate" init="1.0f" min="0.0f" max="30.0f" step="0.1f" description="Lerp rate for adjusting extra heading changes per frame while in low LoD."/>
		
		<float name="m_StartLocomotionBlendoutThreshold" init="0.85f" min="0.0f" max="1.0f" step="0.01f" description="How far along the intro anim can play for before it starts to blend out." />
		<float name="m_StartLocomotionHeadingDeltaBlendoutThreshold" init="0.78539" min="0.0f" max="3.1459" description="Degree threshold in radians between the initial and current desired headings for blending out of walkstarts."/>
		<float name="m_StartLocomotionDefaultBlendDuration" init="0.125f" min="0.0f" max="1.0f" description="The blend duration from idle to start locomotion if the ped isn't playing a special clip."/>
		<float name="m_StartLocomotionDefaultBlendOutDuration" init="0.375f" min="0.0f" max="1.0f" description="How long to blend between StartLocomotion and Locomotion if not early exiting."/>
		<float name="m_StartLocomotionEarlyOutBlendOutDuration" init="0.5f" min="0.0f" max="1.0f" description="How long ot blend between StartLocomotion and Locomotion if early exiting."/>
		<float name="m_StartLocomotionWalkRunBoundary" init="1.5f" min="0.0f" max="3.0f" description="Separator for run and walk starts."/>
		<float name="m_StartToIdleDirectlyPhaseThreshold" init="0.25f" min="0.0f" max="1.0f" description="Phase cutoff for blending from start locomotion directly to idle."/>
		<float name="m_MovementAcceleration" init="2.0f" min="0.0f" max="20.0f" step="0.1f" description="Rate to change the move blend ratio."/>

		<float name="m_MinMBRToStop" init="0.5f" min="0.0f" max="3.0f" step="0.01f" description="What MBR to blend in idle / the stopping animations."/>

		<float name="m_PursuitModeGallopRateFactor" init="1.2f" min="0.0f" max="1.5f" step="0.01f" description="Rate to play the gallop anims when pursuing a target."/>
		<float name="m_PursuitModeExtraHeadingRate" init="4.0f" min="0.0f" max="10.0f" step="0.01f" description="Lerp rate for extra heading adjustments while pursuing a target."/>

		<float name="m_StoppingDistanceWalkMBR" init="0.3f" min="0.0f" max="10.0f" step="0.01f" description="Hacked stopping distance for QPeds using quickstops while walking."/>
		<float name="m_StoppingDistanceRunMBR" init="0.3f" min="0.0f" max="10.0f" step="0.01f" description="Hacked stopping distance for QPeds using quickstops while cantering."/>
		<float name="m_StoppingDistanceGallopMBR" init="0.3f" min="0.0f" max="10.0f" step="0.01f" description="Hacked stopping distance for QPeds using quickstops while galloping."/>
		<float name="m_StoppingGotoPointRemainingDist" init="0.5f" min="0.0f" max="20.0f" step="0.1f" description="How far the stop animation takes the ped."/>
		<float name="m_StopPhaseThreshold" init="0.85f" min="0.0f" max="1.0f" step="0.01f" description="When to blend out of the quickstop animations and transition to idle."/>
		<float name="m_MinStopPhaseToResumeMovement" init="0.0f" max="1.0f" step="0.01f" description="Minimum phase stop locomotion can transition to start locomotion."/>
		<float name="m_MaxStopPhaseToResumeMovement" init="0.4f" max="1.0f" step="0.01f" description="Maximum phase stop locomotion can transition to start locomotion."/>
		<string name="m_PlayerControlCamera" type="atHashString" init="DEFAULT_FOLLOW_PED_CAMERA" />
		<float name="m_ReversingHeadingChangeDegreesForBreakout" init="30.0f" min="0.0f" max="180.0f"/>
		<float name="m_ReversingTimeBeforeAllowingBreakout" init="1.0f" min="0.0f" max="300.0f"/>
  </structdef>

	<structdef type="CTaskMotionAiming::Tunables::Turn">
		<float name="m_MaxVariationForCurrentPitch" init="0.03f" min="0.0f" max="0.5f" step="0.01f"/>
		<float name="m_MaxVariationForDesiredPitch" init="0.03f" min="0.0f" max="0.5f" step="0.01f"/>
	</structdef>

	<structdef type="CTaskMotionAiming::Tunables" base="CTuning">
    <float name="m_PlayerMoveAccel" init="6.0f" min="0.01f" max="20.0f"/>
    <float name="m_PlayerMoveDecel" init="6.0f" min="0.01f" max="20.0f"/>
    <float name="m_PedMoveAccel" init="2.0f" min="0.01f" max="20.0f"/>
    <float name="m_PedMoveDecel" init="8.0f" min="0.01f" max="20.0f"/>
    <float name="m_FromOnFootAccelerationMod" init="1.0f" min="0.0f" max="20.0f" step="0.01f"/>
    <float name="m_WalkAngAccel" init="3.0f" min="0.01f" max="20.0f"/>
    <float name="m_RunAngAccel" init="5.0f" min="0.01f" max="20.0f"/>
    <float name="m_WalkAngAccelDirectBlend" init="7.5f" min="0.01f" max="20.0f"/>
    <float name="m_RunAngAccelDirectBlend" init="7.5f" min="0.01f" max="20.0f"/>
    <float name="m_WalkAngAccelLookBehindTransition" init="15.0f" min="0.01f" max="20.0f"/>
    <float name="m_RunAngAccelLookBehindTransition" init="15.0f" min="0.01f" max="20.0f"/>
    <float name="Turn180ActivationAngle" init="2.74f" min="0.01f" max="20.0f"/>
    <float name="Turn180ConsistentAngleTolerance" init="0.1f" min="0.01f" max="20.0f"/>
    <struct name="m_Turn" type="CTaskMotionAiming::Tunables::Turn"/>
    <float name="m_PitchChangeRate" init="0.75f" min="0.0f" max="5.0f" step="0.01f"/>
    <float name="m_PitchChangeRateAcceleration" init="1.0f" min="0.0f" max="5.0f" step="0.01f"/>
    <float name="m_OverwriteMaxPitch" init="0.01f" min="0.001f" max="0.5f"/>
    <float name="m_AimIntroMaxAngleChangeRate" init="10.0f" min="0.001f" max="100.0f"/>
    <float name="m_AimIntroMinPhaseChangeRate" init="1.5f" min="0.001f" max="100.0f"/>
    <float name="m_AimIntroMaxPhaseChangeRate" init="3.0f" min="0.001f" max="100.0f"/>
    <float name="m_AimIntroMaxTimedOutPhaseChangeRate" init="1.0f" min="0.001f" max="100.0f"/>
    <float name="m_PlayerIdleIntroAnimRate" init="1.0f" min="0.001f" max="100.0f"/>
    <float name="m_MovingWalkAnimRateMin" init="1.0f" min="0.001f" max="100.0f"/>
    <float name="m_MovingWalkAnimRateMax" init="1.0f" min="0.001f" max="100.0f"/>
    <float name="m_MovingWalkAnimRateAcceleration" init="1.0f" min="0.001f" max="100.0f"/>
    <bool name="m_DoPostCameraClipUpdateForPlayer" init="true"/>
    <bool name="m_EnableIkForAI" init="false"/>
    <bool name="m_FPSForceUseStarts" init="false"/>
    <bool name="m_FPSForceUseStops" init="false"/>
    <float name="m_FPSForceUseStopsMinTimeInStartState" init="0.0f" min="0.001f" max="100.0f"/>
    <bool name="m_FPSForceUse180s" init="false"/>
  </structdef>

  <structdef type="CTaskBirdLocomotion::Tunables" base="CTuning">
    <u32 name="m_MinWaitTimeBetweenTakeOffsMS" init="250"/>
    <u32 name="m_MaxWaitTimeBetweenTakeOffsMS" init="500"/>
    <float name="m_MinTakeOffRate" init="0.9f"/>
    <float name="m_MaxTakeOffRate" init="1.1f"/>
    <float name="m_MinTakeOffHeadingChangeRate" init="-0.785f"/>
    <float name="m_MaxTakeOffHeadingChangeRate" init="0.785f"/>
    <float name="m_DefaultTakeoffBlendoutPhase" init="0.9f"/>
    <float name="m_TimeToFlapMin" init="2.5f" min="0.0f" max="10.0f"/>
    <float name="m_TimeToFlapMax" init="5.5f" min="0.0f" max="10.0f"/>
    <float name="m_NoAnimTimeslicingShadowRange" init="55.0f" min="0.0f" max="1000.0" description="Distance to the shadow segment at which we check the shadow visibility to disable timeslicing."/>
    <float name="m_ForceNoTimeslicingHeadingDiff" init="5.0f" min="0.0f" max="90.0f"/>
    <float name="m_MinDistanceFromPlayerToDeleteStuckBird" init="10.0f" min="0.0f" max="30.0f"/>
    <float name="m_TimeUntilDeletionWhenStuckOffscreen" init="3.0f" min="0.0f" max="30.0f"/>
    <float name="m_TimeWhenStuckToIgnoreBird" init="2.0f" min="0.0f" max="30.0f"/>
    <float name="m_HighLodWalkHeadingLerpRate" init="1.0f" min="0.0f" max="30.0f"/>
    <float name="m_LowLodWalkHeadingLerpRate" init="5.0f" min="0.0f" max="30.0f"/>
    <float name="m_PlayerWalkCapsuleRadius" init="0.5f" min="0.0f" max="100.0f"/>
    <float name="m_AIBirdTurnApproachRate" init="4.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerGlideIdealTurnRate" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerGlideTurnAcceleration" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerHeadingDeadZoneThresholdDegrees" init="2.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerFlappingTurnAcceleration" init="3.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerFlappingIdealTurnRate" init="4.0f" min="0.0f" max="100.0f"/>
    <float name="m_AIPitchTurnApproachRate" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerPitchIdealRate" init="1.0f"/>
    <float name="m_PlayerPitchAcceleration" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerTiltedDownToleranceDegrees" init="-45.0f" min="-180.0f" max="180.0f"/>
    <float name="m_PlayerTiltedDownSpeedBoost" init="45.0f" min="0.0f" max="1000.0f"/>
    <float name="m_PlayerTiltedSpeedCap" init="24.0f" min="0.0f" max="9000.0f" />
    <string name="m_PlayerControlCamera" type="atHashString" init="DEFAULT_FOLLOW_PED_CAMERA" />
  </structdef>

	<structdef type="CTaskMotionSwimming::PedVariation"  >
		<enum name="m_Component" type="ePedVarComp" init="PV_COMP_INVALID"/>
		<u32 name="m_DrawableId" min="0" max="255" step="1" init="0"/>
		<u32 name="m_DrawableAltId" min="0" max="255" step="1" init="0"/>
	</structdef>

	<structdef type="CTaskMotionSwimming::PedVariationSet"  >
		<enum name="m_Component" type="ePedVarComp" init="PV_COMP_INVALID"/>
		<array name="m_DrawableIds" type="atArray">
			<u32 min="0" max="255" step="1" init="0" />
		</array>
	</structdef>

	<structdef type="CTaskMotionSwimming::ScubaGearVariation"  >
		<array name="m_Wearing" type="atArray">
			<struct type="CTaskMotionSwimming::PedVariationSet"/>
		</array>
		<struct name="m_ScubaGearWithLightsOn" type="CTaskMotionSwimming::PedVariation"/>
		<struct name="m_ScubaGearWithLightsOff" type="CTaskMotionSwimming::PedVariation"/>
	</structdef>

	<structdef type="CTaskMotionSwimming::ScubaGearVariations"  >
		<string name="m_ModelName" type="atHashString"/>
		<array name="m_Variations" type="atArray">
			<struct type="CTaskMotionSwimming::ScubaGearVariation"/>
		</array>
	</structdef>
	
	<structdef type="CTaskMotionSwimming::Tunables::ScubaMaskProp"  >
		<string name="m_ModelName" type="atHashString" />
		<s32 name="m_Index" min="-1" max="127" step="1" init="-1"/>
	</structdef>

	<structdef type="CTaskMotionSwimming::Tunables" base="CTuning">
    	<int name="m_MinStruggleTime" min="0" init="1500" description="Minimum time before struggling when in rapids"/>
    	<int name="m_MaxStruggleTime" min="0" init="4000" description="Maximum time before struggling when in rapids"/>
		<array name="m_ScubaGearVariations" type="atArray">
			<struct type="CTaskMotionSwimming::ScubaGearVariations" />
		</array>
		<array name="m_ScubaMaskProps" type="atArray">
			<struct type="CTaskMotionSwimming::Tunables::ScubaMaskProp"/>
		</array>
	</structdef>

  <structdef type="CTaskFishLocomotion::Tunables" base="CTuning">
    <float name="m_StartTurnThresholdDegrees" init="15.0f" min="0.0f" max="90.0f"/>
    <float name="m_StopTurnThresholdDegrees" init="10.0f" min="0.0f" max="90.0f"/>
    <float name="m_MinTurnApproachRate" init="1.0f" min="0.0f" max="20.0f"/>
    <float name="m_IdealTurnApproachRate" init="1.0f" min="0.0f" max="20.0f"/>
    <float name="m_IdealTurnApproachRateSlow" init="2.0f" min="0.0f" max="20.0f"/>
    <float name="m_TurnAcceleration" init="3.0f" min="0.0f" max="5.0f"/>
    <float name="m_TurnAccelerationSlow" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_PitchAcceleration" init="1.0f" min="0.0f" max="20.0f" />
    <float name="m_PlayerPitchAcceleration" init="2.0f" min="0.0f" max="20.0f"/>
    <float name="m_HighLodPhysicsPitchIdealApproachRate" init="1.0f" min="0.0f" max="20.0f" />
    <float name="m_LowLodPhysicsPitchIdealApproachRate" init="1.0f" min="0.0f" max="20.0f" />
    <float name="m_FastPitchingApproachRate" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_FishOutOfWaterDelay" init="0.5f"/>
    <float name="m_PlayerOutOfWaterThreshold" init="0.2f"/>
    <float name="m_AssistanceAngle" init="15.0f"/>
    <float name="m_ExtraHeadingRate" init="1.0f"/>
    <float name="m_SurfaceProbeHead" init="1.0f"/>
    <float name="m_SurfaceProbeTail" init="1.0f"/>
    <float name="m_SurfacePitchLerpRate" init="1.0f"/>
    <float name="m_SurfaceHeightFallingLerpRate" init="1.0f"/>
    <float name="m_SurfaceHeightRisingLerpRate" init="1.0f"/>
    <float name="m_SurfaceHeightFollowingTriggerRange" init="1.0f"/>
    <float name="m_LongStateTransitionBlendTime" init="1.0f" />
    <float name="m_ShortStateTransitionBlendTime" init="0.25f" />
    <string name="m_PlayerControlCamera" type="atHashString" init="DEFAULT_FOLLOW_PED_CAMERA" />
    <float name="m_GaitlessRateBoost" init="0.5f" />
  </structdef>

  <structdef type="CTaskMotionTennis::Tunables" base="CTuning">
	<float name="m_StrafeDirectionLerpRateMinAI" init="6.0f" min="0.0f" max="50.0f"/>
	<float name="m_StrafeDirectionLerpRateMaxAI" init="12.0f" min="0.0f" max="50.0f"/>
	<float name="m_StrafeDirectionLerpRateMinPlayer" init="6.0f" min="0.0f" max="50.0f"/>
	<float name="m_StrafeDirectionLerpRateMaxPlayer" init="12.0f" min="0.0f" max="50.0f"/>
  </structdef>

	<structdef type="CTaskMotionInTurret::Tunables" base="CTuning" cond="ENABLE_MOTION_IN_TURRET_TASK">
		<float name="m_MinAnimPitch" init="-0.477f" min="-3.14f" max="3.14f" step="0.001f"/>
		<float name="m_MaxAnimPitch" init="0.446" min="-3.14f" max="3.14f" step="0.001f"/>
		<float name="m_DeltaHeadingDirectionChange" init="0.01f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_DeltaHeadingTurnThreshold" init="0.05f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_MinHeadingDeltaTurnSpeed" init="0.0f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_MaxHeadingDeltaTurnSpeed" init="0.12f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_PitchApproachRate" init="2.0f" min="0.0f" max="20.0f" step="0.001f"/>
		<float name="m_TurnSpeedApproachRate" init="0.75f" min="0.0f" max="20.0f" step="0.001f"/>
		<float name="m_MinTimeInTurnState" init="0.1f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_MinTimeInIdleState" init="0.1f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_MinTimeInTurnStateAiOrRemote" init="0.2f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_MinTimeInIdleStateAiOrRemote" init="0.2f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_MinHeadingDeltaToAdjust" init="0.1f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_PlayerTurnControlThreshold" init="0.1f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_RemoteOrAiTurnControlThreshold" init="0.01f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_RemoteOrAiPedTurnDelta" init="0.01f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_RemoteOrAiPedStuckThreshold" init="0.001f" min="0.0f" max="1.0f" step="0.001f"/>
		<float name="m_TurnVelocityThreshold" init="3.0f" min="0.0f" max="30.0f" step="0.1f"/>
		<float name="m_TurnDecelerationThreshold" init="9.0f" min="0.0f" max="30.0f" step="0.1f"/>
		<float name="m_NoTurnControlThreshold" init="0.1f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_MinTimeTurningForWobble" init="0.5f" min="0.0f" max="3.0f" step="0.001f"/>
		<float name="m_SweepHeadingChangeRate" init="4.0f" min="0.0f" max="10.0f" step="0.01f"/>
		<float name="m_MinMvPitchToPlayAimUpShunt" init="0.7f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_MaxMvPitchToPlayAimDownShunt" init="0.3f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_MaxAngleToUseDesiredAngle" init="0.5f" min="0.0f" max="3.14f" step="0.01f"/>
		<float name="m_EnterPitchSpeedModifier" init="0.5f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_EnterHeadingSpeedModifier" init="0.2f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_ExitPitchSpeedModifier" init="0.05f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_DelayMovementDuration" init="0.2f" min="-1.0f" max="10.0f" step="0.01f"/>
		<float name="m_InitialAdjustFinishedHeadingTolerance" init="0.8f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_InitialAdjustFinishedPitchTolerance" init="0.1f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_LookBehindSpeedModifier" init="0.25f" min="0.0f" max="1.0f" step="0.01f"/>
		<float name="m_RestartIdleBlendDuration" init="0.15f" min="0.0f" max="1.0f" step="0.01f"/>
		<string name="m_AdjustStep0LClipId" type="atHashString" init="Adjust_Step_0L"/>
		<string name="m_AdjustStep90LClipId" type="atHashString" init="Adjust_Step_90L"/>
		<string name="m_AdjustStep180LClipId" type="atHashString" init="Adjust_Step_180L"/>
		<string name="m_AdjustStep0RClipId" type="atHashString" init="Adjust_Step_0R"/>
		<string name="m_AdjustStep90RClipId" type="atHashString" init="Adjust_Step_90R"/>
		<string name="m_AdjustStep180RClipId" type="atHashString" init="Adjust_Step_180R"/>
		<string name="m_TurnLeftSlowDownClipId" type="atHashString" init="turn_left_slow_aim_down"/>
		<string name="m_TurnLeftFastDownClipId" type="atHashString" init="turn_left_fast_aim_down"/>
		<string name="m_TurnLeftSlowClipId" type="atHashString" init="turn_left_slow"/>
		<string name="m_TurnLeftFastClipId" type="atHashString" init="turn_left_fast"/>
		<string name="m_TurnLeftSlowUpClipId" type="atHashString" init="turn_left_slow_aim_up"/>
		<string name="m_TurnLeftFastUpClipId" type="atHashString" init="turn_left_fast_aim_up"/>
		<string name="m_TurnRightSlowDownClipId" type="atHashString" init="turn_right_slow_aim_down"/>
		<string name="m_TurnRightFastDownClipId" type="atHashString" init="turn_right_fast_aim_down"/>
		<string name="m_TurnRightSlowClipId" type="atHashString" init="turn_right_slow"/>
		<string name="m_TurnRightFastClipId" type="atHashString" init="turn_right_fast"/>
		<string name="m_TurnRightSlowUpClipId" type="atHashString" init="turn_right_slow_aim_up"/>
		<string name="m_TurnRightFastUpClipId" type="atHashString" init="turn_right_fast_aim_up"/>
		<string name="m_ShuntLeftClipId" type="atHashString" init="shunt_from_left"/>
		<string name="m_ShuntRightClipId" type="atHashString" init="shunt_from_right"/>
		<string name="m_ShuntFrontClipId" type="atHashString" init="shunt_from_front"/>
		<string name="m_ShuntBackClipId" type="atHashString" init="shunt_from_back"/>
		<string name="m_ShuntUpClipId" type="atHashString" init="shunt_aim_up"/>
		<string name="m_ShuntDownClipId" type="atHashString" init="shunt_aim_down"/>
		<string name="m_TurretFireClipId" type="atHashString" init="gun_fire"/>
	</structdef>

</ParserSchema>
