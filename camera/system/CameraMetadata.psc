<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">

  <!--  abstract base object type - all camera system object types inherit from this -->
  <structdef type="camBaseObjectMetadata" constructable="false">
    <string name="m_Name" type="atHashString" ui_key="true"/>
  </structdef>

  <!-- start of shake definitions -->
  <enumdef type="eOscillatorWaveform">
    <enumval name="OSCILLATOR_WAVEFORM_SINEWAVE"/>
    <enumval name="OSCILLATOR_WAVEFORM_DAMPED_SINEWAVE"/>
  </enumdef>

  <enumdef type="eCurveType">
    <enumval name="CURVE_TYPE_EXPONENTIAL"/>
    <enumval name="CURVE_TYPE_LINEAR"/>
    <enumval name="CURVE_TYPE_SLOW_IN"/>
    <enumval name="CURVE_TYPE_SLOW_OUT"/>
    <enumval name="CURVE_TYPE_SLOW_IN_OUT"/>
    <enumval name="CURVE_TYPE_VERY_SLOW_IN"/>
    <enumval name="CURVE_TYPE_VERY_SLOW_OUT"/>
    <enumval name="CURVE_TYPE_VERY_SLOW_IN_SLOW_OUT"/>
    <enumval name="CURVE_TYPE_SLOW_IN_VERY_SLOW_OUT"/>
    <enumval name="CURVE_TYPE_VERY_SLOW_IN_VERY_SLOW_OUT"/>
    <enumval name="CURVE_TYPE_EASE_IN"/>
    <enumval name="CURVE_TYPE_EASE_OUT"/>
    <enumval name="CURVE_TYPE_QUADRATIC_EASE_IN"/>
    <enumval name="CURVE_TYPE_QUADRATIC_EASE_OUT"/>
    <enumval name="CURVE_TYPE_QUADRATIC_EASE_IN_OUT"/>
    <enumval name="CURVE_TYPE_CUBIC_EASE_IN"/>
    <enumval name="CURVE_TYPE_CUBIC_EASE_OUT"/>
    <enumval name="CURVE_TYPE_CUBIC_EASE_IN_OUT"/>
    <enumval name="CURVE_TYPE_QUARTIC_EASE_IN"/>
    <enumval name="CURVE_TYPE_QUARTIC_EASE_OUT"/>
    <enumval name="CURVE_TYPE_QUARTIC_EASE_IN_OUT"/>
    <enumval name="CURVE_TYPE_QUINTIC_EASE_IN"/>
    <enumval name="CURVE_TYPE_QUINTIC_EASE_OUT"/>
    <enumval name="CURVE_TYPE_QUINTIC_EASE_IN_OUT"/>    
    <enumval name="CURVE_TYPE_CIRCULAR_EASE_IN"/>
    <enumval name="CURVE_TYPE_CIRCULAR_EASE_OUT"/>
    <enumval name="CURVE_TYPE_CIRCULAR_EASE_IN_OUT"/>
  </enumdef>

  <structdef type="camOscillatorMetadata" base="camBaseObjectMetadata">
    <enum name="m_Waveform" type="eOscillatorWaveform" init="OSCILLATOR_WAVEFORM_SINEWAVE"/>
    <float name="m_Amplitude" init="0.0f" min="0.0f" max="10.0f"/>
    <float name="m_Frequency" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_Phase" init="-1.0f" min="-1.0f" max="1.0f"/>
    <float name="m_DcOffset" init="0.0f" min="-1.0f" max="1.0f"/>
    <float name="m_Decay" init="1.0f" min="0.0f" max="40.0f"/>
  </structdef>

  <structdef type="camEnvelopeMetadata" base="camBaseObjectMetadata">
    <u32 name="m_PreDelayDuration" init="0" min="0" max="10000"/>
    <u32 name="m_AttackDuration" init="0" min="0" max="10000"/>
    <u32 name="m_DecayDuration" init="0" min="0" max="10000"/>
    <float name="m_SustainLevel" init="1.0f" min="0.0f" max="10.0f"/>
    <s32 name="m_HoldDuration" init="1000" min="-1" max="10000"/>
    <u32 name="m_ReleaseDuration" init="0" min="0" max="10000"/>
    <enum name="m_AttackCurveType" type="eCurveType" init="CURVE_TYPE_LINEAR"/>
    <enum name="m_ReleaseCurveType" type="eCurveType" init="CURVE_TYPE_EXPONENTIAL"/>
    <bool name="m_UseSpecialCaseEnvelopeReset" init ="false"/>
  </structdef>

  <enumdef type="eShakeFrameComponent">
    <enumval name="SHAKE_COMP_WORLD_POS_X"/>
    <enumval name="SHAKE_COMP_WORLD_POS_Y"/>
    <enumval name="SHAKE_COMP_WORLD_POS_Z"/>
    <enumval name="SHAKE_COMP_REL_POS_X"/>
    <enumval name="SHAKE_COMP_REL_POS_Y"/>
    <enumval name="SHAKE_COMP_REL_POS_Z"/>
    <enumval name="SHAKE_COMP_ROT_X"/>
    <enumval name="SHAKE_COMP_ROT_Y"/>
    <enumval name="SHAKE_COMP_ROT_Z"/>
    <enumval name="SHAKE_COMP_FOV"/>
    <enumval name="SHAKE_COMP_NEAR_DOF"/>
    <enumval name="SHAKE_COMP_FAR_DOF"/>
    <enumval name="SHAKE_COMP_MOTION_BLUR"/>
    <enumval name="SHAKE_COMP_ZOOM_FACTOR"/>
    <enumval name="SHAKE_COMP_FULL_SCREEN_BLUR"/>
  </enumdef>

  <structdef type="camShakeMetadataFrameComponent">
    <enum name="m_Component" type="eShakeFrameComponent" init="SHAKE_COMP_ROT_X"/>
    <string name="m_OscillatorRef" type="atHashString"/>
    <string name="m_EnvelopeRef" type="atHashString"/>
  </structdef>

  <structdef type="camBaseShakeMetadata" base="camBaseObjectMetadata" constructable="false">
    <string name="m_OverallEnvelopeRef" type="atHashString"/>
    <bool name="m_UseGameTime" init="false"/>
    <bool name="m_IsReversible" init="false" description="Allows this shake to be played in reverse if the camera timer reverses. Note that in addition the shake will never end on it's own (in order to allow scrubbing passed the end of the shake envelope and back into it again)"/>
  </structdef>

  <structdef type="camShakeMetadata" base="camBaseShakeMetadata">
    <array name="m_FrameComponents" type="atArray">
      <struct type="camShakeMetadataFrameComponent"/>
    </array>
    <float name="m_Vibration" init="0.0f" min="0.0f" max="1.0f"/>
  </structdef>

  <structdef type="camAnimatedShakeMetadata" base="camBaseShakeMetadata">
  </structdef>

  <!-- end of shake definitions -->

  <!-- start of helper definitions -->

  <structdef type="camSpringMountMetadata" base="camBaseObjectMetadata">
    <Vector3 name="m_AccelerationLimit" init="5.0f, 5.0f, 0.0f" min="0.0f" max="100.0f"/>
    <Vector3 name="m_AccelerationForce" init="0.025f, 0.025f, 0.0f" min="0.0f" max="100.0f"/>
    <Vector3 name="m_SpringForce" init="7.0f, 3.0f, 0.0f" min="0.0f" max="100.0f"/>
    <Vector3 name="m_DampeningForce" init="0.018f, 0.01f, 0.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <enumdef type="camControlHelperMetadataViewMode::eViewModeContext" preserveNames="true">
    <enumval name="ON_FOOT"/>
    <enumval name="IN_VEHICLE"/>
    <enumval name="ON_BIKE"/>
    <enumval name="IN_BOAT"/>
    <enumval name="IN_AIRCRAFT"/>
    <enumval name="IN_SUBMARINE"/>
    <enumval name="IN_HELI"/>
    <enumval name="IN_TURRET"/>
  </enumdef>
  <enumdef type="camControlHelperMetadataViewMode::eViewMode" generate="bitset" preserveNames="true">
    <enumval name="THIRD_PERSON_NEAR"/>
    <enumval name="THIRD_PERSON_MEDIUM"/>
    <enumval name="THIRD_PERSON_FAR"/>
    <enumval name="CINEMATIC"/>
    <enumval name="FIRST_PERSON"/>
  </enumdef>
  <structdef type="camControlHelperMetadataViewModeSettings">
    <Vector2 name="m_OrbitDistanceLimitScaling" init="1.0f, 1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camControlHelperMetaDataPrecisionAimSettings">
    <float name="m_MinAccelModifier"	init="0.1f" min="0.0f" max="5.0f"/>
    <float name="m_MaxAccelModifier"	init="1.9f" min="0.0f" max="5.0f"/>
    <float name="m_MinDeccelModifier"	init="0.5f" min="0.0f" max="5.0f"/>
    <float name="m_MaxDeccelModifier"	init="1.5f" min="0.0f" max="5.0f"/>
    <float name="m_InputMagToIncreaseDeadZoneMin"	init="0.75f" min="0.0f" max="5.0f"/>
    <float name="m_InputMagToIncreaseDeadZoneMax"	init="1.0f" min="0.0f" max="5.0f"/>
  </structdef>	
  <structdef type="camControlHelperMetadataViewModes">
    <bool name="m_ShouldUseViewModeInput" init="true"/>
    <bool name="m_ShouldToggleViewModeBetweenThirdAndFirstPerson" init="false"/>
    <enum name="m_Context" type="camControlHelperMetadataViewMode::eViewModeContext" init="camControlHelperMetadataViewMode::ON_FOOT"/>
    <bitset name="m_Flags" type="fixed8" numBits="5" values="camControlHelperMetadataViewMode::eViewMode"/>
    <array name="m_Settings" type="member" size="5">
      <struct type="camControlHelperMetadataViewModeSettings"/>
    </array>
    <string name="m_ViewModeBlendEnvelopeRef" type="atHashString"/>
  </structdef>
  <structdef type="camControlHelperMetadataLookAround">
    <float name="m_InputMagPowerFactor" init="1.0f" min="1.0f" max="10.0f"/>

    <float name="m_Acceleration" init="2.0f" min="0.0f" max="10000.0f"/>
    <float name="m_Deceleration" init="6.0f" min="0.0f" max="10000.0f"/>
    <float name="m_MaxHeadingSpeed" init="157.6f" min="-360.0f" max="360.0f"/>
    <float name="m_MaxPitchSpeed" init="100.3f" min="-360.0f" max="360.0f"/>

    <float name="m_MouseMaxHeadingSpeedMin" init="250.0f" min="-3000.0f" max="3000.0f"/>
    <float name="m_MouseMaxHeadingSpeedMax" init="1080.0f" min="-3000.0f" max="3000.0f"/>
    <float name="m_MouseMaxPitchSpeedMin" init="190.0f" min="-3000.0f" max="3000.0f"/>
    <float name="m_MouseMaxPitchSpeedMax" init="820.0f" min="-3000.0f" max="3000.0f"/>

    <float name="m_LSDeadZoneAngle" init="0.0f" min="0.0f" max="90.0f"/>
    <float name="m_LSAcceleration" init="0.0f" min="0.0f" max="10000.0f"/>
    <float name="m_LSDeceleration" init="0.0f" min="0.0f" max="10000.0f"/>

    <bool name="m_ShouldUseGameTime" init="false"/>

    <string name="m_InputEnvelopeRef" type="atHashString"/>

    <struct type="camControlHelperMetaDataPrecisionAimSettings" name="m_PrecisionAimSettings"/>
  </structdef>
  <structdef type="camControlHelperMetadataZoom">
    <bool name="m_ShouldUseZoomInput" init="false"/>
    <bool name="m_ShouldUseDiscreteZoomControl" init="false"/>
    <bool name="m_ShouldUseGameTime" init="false"/>

    <float name="m_MinFov" init="5.0f" min="1.0f" max="130.0f"/>
    <float name="m_MinFovForNetworkPlay" init="7.5f" min="1.0f" max="130.0f"/>
    <float name="m_MaxFov" init="45.0f" min="1.0f" max="130.0f"/>

    <float name="m_InputMagPowerFactor" init="1.0f" min="1.0f" max="10.0f"/>

    <float name="m_Acceleration" init="6.0f" min="-10000.0f" max="10000.0f"/>
    <float name="m_Deceleration" init="6.0f" min="-10000.0f" max="10000.0f"/>
    <float name="m_MaxSpeed" init="1.0f" min="-10.0f" max="10.0f"/>
  </structdef>

  <structdef type="camControlHelperMetadata" base="camBaseObjectMetadata">
    <struct type="camControlHelperMetadataViewModes" name="m_ViewModes"/>
    <struct type="camControlHelperMetadataLookAround" name="m_LookAround"/>
    <struct type="camControlHelperMetadataZoom" name="m_Zoom"/>

    <u32 name="m_LookBehindOutroTimeMS" init="100" min="0" max="1000"/>
    
    <bool name="m_ShouldUseLookBehindInput" init="true"/>
    <bool name="m_ShouldUseAccurateModeInput" init="false"/>
    <bool name="m_ShouldToggleAccurateModeInput" init="true"/>

    <bool name="m_ShouldApplySniperControlPref" init="false"/>
    <bool name="m_ShouldApplyAimSensitivityPref" init="false"/>
    <Vector2 name="m_AimSensitivityScalingLimits" init="0.35f, 1.65f" min="0.1f" max="10.0f"/>
    <u32 name="m_MaxDurationForMultiplayerViewModeActivation" init="500" min="0" max="10000"/>
  </structdef>

  <structdef type="camCollisionMetadataOcclusionSweep">
    <u32 name="m_NumCapsuleTests" init="6" min="2" max="100" description="The number of capsule tests that are used to find a nearby unoccluded position"/>
    <float name="m_MaxCollisionRootSpeedToForcePopIn" init="0.1f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxPreCollisionCameraSpeedToForcePopIn" init="0.1f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxSweepAngleWhenMovingTowardsLos" init="60.0f" min="0.0f" max="180.0f" description="The total angle over which to sweep the capsule tests when searching for an unoccluded position to move towards"/>
    <float name="m_MaxSweepAngleWhenAvoidingPopIn" init="90.0f" min="0.0f" max="180.0f" description="The total angle over which to sweep the capsule tests when searching for an unoccluded position that will allow us to avoid popping in"/>
    <float name="m_MinOrientationSpeedToMaintainDirection" init="0.1f" min="0.0f" max="10000.0f" description="The minimum orientation speed at which the sweep direction with be maintained between updates"/>
    <float name="m_MinCameraMoveSpeedToSweepInDirectionOfTravel" init="0.01f" min="0.0f" max="10000.0f" description="The minimum move speed at which the sweep direction with be derived from the camera's direction of travel"/>
  </structdef>
  <structdef type="camCollisionMetadataPathFinding">
    <u32 name="m_MaxCapsuleTests" init="6" min="2" max="100" description="The maximum number of capsule tests that can be used to find a clear path to the chosen unoccluded position"/>
  </structdef>
  <structdef type="camCollisionMetadataRotationTowardsLos">
    <float name="m_SpringConstant" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="0.4f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camCollisionMetadataOrbitDistanceDamping">
    <float name="m_MaxCollisionRootSpeedToPausePullBack" init="0.1f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxPreCollisionCameraSpeedToPausePullBack" init="0.1f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="0.8f" min="0.0f" max="10.0f"/>
    <float name="m_MaxDistanceErrorToIgnore" init="0.005f" min="0.0f" max="1.0f"/>
  </structdef>
  <structdef type="camCollisionMetadataClippingAvoidance">
    <u32 name="m_MaxIterations" init="4" min="1" max="10" description="The number of incremental steps that will be used to find a clear camera position when clipping is detected"/>
    <float name="m_CapsuleLengthForDetection" init="0.2f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camCollisionMetadataBuoyancySettings">
    <bool name="m_ShouldApplyBuoyancy" init="true"/>
    <bool name="m_ShouldIgnoreBuoyancyStateAndAvoidSurface" init="false"/>
    <float name="m_MinHitNormalDotWorldUpForRivers" init="0.5f" min="-1.0f" max="1.0f"/>
    <float name="m_WaterHeightSmoothRate" init="0.4f" min="0.0f" max="1.0f"/>
    <float name="m_MinHeightAboveWaterWhenBuoyant" init="0.25f" min="0.0f" max="99999.0f"/>
    <float name="m_MinDepthUnderWaterWhenNotBuoyant" init="0.25f" min="0.0f" max="99999.0f"/>
  </structdef>
  <structdef type="camCollisionMetadataPushBeyondEntitiesIfClipping">
    <float name="m_ExtraDistanceToPushAway" init="0.05f" min="-10.0f" max="10.0f"/>
    <float name="m_OrbitDistanceScalingToApplyWhenPushing" init="1.35f" min="1.0f" max="10.0f" description="A scaling factor to be applied to the orbit distance in order to push the camera further beyond a clipping entity"/>
    <float name="m_PullBackSpringConstant" init="0.0f" min="0.0f" max="1000.0f"/>
    <float name="m_PushInSpringConstant" init="5.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="0.8f" min="0.0f" max="10.0f"/>
    <bool name="m_ShouldAllowOtherCollisionToConstrainCameraIntoEntities" init="false"/>
    <bool name="m_ShouldAllowOtherCollisionToConstrainCameraIntoEntitiesForTripleHead" init="false"/>
    <bool name="m_InheritDefaultShouldAllowOtherCollisionToConstrainCameraIntoEntitiesForTripleHead" init="true"/>
  </structdef>
  <structdef type="camCollisionMetadataPullBackTowardsCollision">
    <float name="m_BlendInSpringConstant" init="0.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlendOutSpringConstant" init="15.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="0.8f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camCollisionMetadata" base="camBaseObjectMetadata">
    <bool name="m_ShouldIgnoreOcclusionWithBrokenFragments" init="false" description="if true the camera will ignore occlusion with all broken fragments"/>
    <bool name="m_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities" init="false" description="if true the camera will ignore occlusion with fragments that have broken off an ignored entity"/>
    <bool name="m_ShouldMoveTowardsLos" init="false" description="if true the camera will attempt find a path to a clear LOS and move towards it"/>
    <bool name="m_ShouldSweepToAvoidPopIn" init="false" description="if true the camera will attempt find a path to a clear LOS in the current direction of travel that will allow it to avoid popping in beyond occluders"/>
    <bool name="m_ShouldPersistPopInBehaviour" init="true" description="if true the automatic movement towards LOS will be blocked until the camera is no longer popped-in to avoid collision"/>
    <bool name="m_ShouldPullBackByCapsuleRadius" init="true" description="if true the camera will be pulled back by the capsule radius when constrained, allowing the camera to get closer to collision"/>
    <bool name="m_ShouldIgnoreOcclusionWithSelectCollision" init="true" description="if true the camera will ignore occlusion with certain types of collision, such as peds and specially flagged collision polys"/>
    <bool name="m_ShouldIgnoreOcclusionWithRagdolls" init="false" description="if true the camera will ignore occlusion with ragdolls specifically"/>
    <bool name="m_ShouldReportAsCameraTypeTest" init="true"/>
    <struct type="camCollisionMetadataOcclusionSweep" name="m_OcclusionSweep"/>
    <struct type="camCollisionMetadataPathFinding" name="m_PathFinding"/>
    <struct type="camCollisionMetadataRotationTowardsLos" name="m_RotationTowardsLos"/>
    <struct type="camCollisionMetadataOrbitDistanceDamping" name="m_OrbitDistanceDamping"/>
    <struct type="camCollisionMetadataClippingAvoidance" name="m_ClippingAvoidance"/>
    <struct type="camCollisionMetadataBuoyancySettings" name="m_BuoyancySettings"/>
    <struct type="camCollisionMetadataPushBeyondEntitiesIfClipping" name="m_PushBeyondEntitiesIfClipping"/>
    <struct type="camCollisionMetadataPullBackTowardsCollision" name="m_PullBackTowardsCollision"/>
  </structdef>

  <structdef type="camHintHelperMetadataPivotPositionAdditive">
    <float name="m_CameraRelativeSideOffsetAdditive" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_CameraRelativeVerticalOffsetAdditive" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_ParentWidthScalingForCameraRelativeSideOffsetAdditive" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_ParentHeightScalingForCameraRelativeVerticalOffsetAdditive" init="0.0f" min="-10.0f" max="10.0f"/>
  </structdef>
  
  <structdef type="camHintHelperMetadata" base="camBaseObjectMetadata">
    <float name="m_FovScalar" init="1.0f" min="0.1f" max="10.0f"/>
    <float name="m_FollowDistanceScalar" init="1.0f" min="0.1f" max="10.0f"/>
    <float name="m_BaseOrbitPitchOffset" init="-4.0f" min="-90.0f" max="90.0f"/>
    <Vector2 name="m_OrbitPitchLimits" init="-89.0f, 89.0f" min="-89.0f" max="89.0f"/>
    <float name="m_PivotOverBoundingBoxBlendLevel" init="0.0f" min="0.0f" max="1.0f"/>
    <struct type="camHintHelperMetadataPivotPositionAdditive" name="m_PivotPositionAdditive"/>
    <string name="m_LookAtVehicleDampingHelperRef" type="atHashString"/>
    <string name="m_LookAtPedDampingHelperRef" type="atHashString"/>
    <bool name="m_ShouldTerminateForPitch" init="false"/>
    <bool name="m_ShouldStopUpdatingLookAtPosition" init="false"/>
    <float name="m_MinDistanceForLockOn" init="0.5f" min="0.0f" max="99999.0f"/>
    <bool name="m_ShouldLockOn" init="false"/>
  </structdef>

  <structdef type="camInconsistentBehaviourZoomHelperBaseSettings">
    <bool name="m_ShouldDetect" init="false"/>
    <u32 name="m_ReactionTime" init="300" min="0" max="60000"/>
    <u32 name="m_MinZoomDuration" init="1000" min="0" max="60000"/>
    <u32 name="m_MaxZoomDuration" init="1400" min="0" max="60000"/>
    <float name="m_BoundsRadiusScale" init="2.0f" min="0.0f" max="1000.0f"/>
  </structdef>
  <structdef type="camInconsistentBehaviourZoomHelperDetectSuddenMovementSettings" base="camInconsistentBehaviourZoomHelperBaseSettings">
    <float name="m_MinVehicleAcceleration" init="150.0f" min="0.0f" max="99999.0f"/>
  </structdef>
  <structdef type="camInconsistentBehaviourZoomHelperDetectFastCameraTurnSettings" base="camInconsistentBehaviourZoomHelperBaseSettings">
    <float name="m_AbsMaxHeadingSpeed" init="1.5f" min="0.0f" max="99999.0f"/>
    <float name="m_AbsMaxPitchSpeed" init="1.5f" min="0.0f" max="99999.0f"/>
  </structdef>
  <structdef type="camInconsistentBehaviourZoomHelperAirborneSettings" base="camInconsistentBehaviourZoomHelperBaseSettings">
    <u32 name="m_MinTimeSinceAirborne" init="500" min="0" max="99999"/>
  </structdef>
  <structdef type="camInconsistentBehaviourZoomHelperLosSettings" base="camInconsistentBehaviourZoomHelperBaseSettings">
    <u32 name="m_MinTimeSinceLostLos" init="500" min="0" max="99999"/>
  </structdef>
  <structdef type="camInconsistentBehaviourZoomHelperMetadata" base="camBaseObjectMetadata">
    <float name="m_MinFovDifference" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxFov" init="45.0f" min="1.0f" max="130.0f"/>
    <struct type="camInconsistentBehaviourZoomHelperDetectSuddenMovementSettings" name="m_DetectSuddenMovementSettings"/>
    <struct type="camInconsistentBehaviourZoomHelperDetectFastCameraTurnSettings" name="m_DetectFastCameraTurnSettings"/>
    <struct type="camInconsistentBehaviourZoomHelperAirborneSettings" name="m_DetectAirborneSettings"/>
    <struct type="camInconsistentBehaviourZoomHelperLosSettings" name="m_DetectLineOfSightSettings"/>
  </structdef>
  
  <structdef type="camCatchUpHelperMetadata" base="camBaseObjectMetadata">
    <float name="m_MaxOrbitDistanceScalingToBlend" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxOrbitDistanceScalingForMaxValidCatchUpDistance" init="3.0f" min="0.0f" max="100.0f"/>
    <s32 name="m_BlendDuration" init="-1" min="-1" max="60000"/>
    <bool name="m_ShouldApplyCustomBlendCurveType" init="false"/>
    <enum name="m_CustomBlendCurveType" type="eCurveType" init="CURVE_TYPE_SLOW_IN_OUT"/>
  </structdef>

  <structdef type="camStickyAimHelperMetadata" base="camBaseObjectMetadata">
    <string name="m_StickyAimEnvelopeRef" type="atHashString"/>
    <float name="m_MaxStickyAimDistance" init="80.0f" min="0.0f" max="200.0f"/>
    <bool name="m_RequiresLookAroundInput" init="true"/>
    <bool name="m_RequiresPlayerMovement" init="true"/>
    <bool name="m_RequiresEitherLookAroundOrPlayerMovement" init="true"/>
    <u32 name="m_TimeToSwitchStickyTargetsMS" init="400" min="0" max="5000"/>
    <u32 name="m_LastStickyCooldownMS" init="0" min="0" max="5000"/>
    <u32 name="m_RotationNoStickTimeMS" init="500" min="0" max="2000"/>
    <float name="m_BaseRotationNear" init="0.4f" min="0.0f" max="1.0f"/>
    <float name="m_BaseRotationFar" init="0.6f" min="0.0f" max="1.0f"/>
    <float name="m_BaseRotationDistanceNear" init="2.0f" min="0.0f" max="100.0f"/>
    <float name="m_BaseRotationDistanceFar" init="6.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxHeadingRotationClamp" init="0.925f" min="0.0f" max="1.0f"/>
    <float name="m_MaxPitchRotationClamp" init="0.925f" min="0.0f" max="1.0f"/>
    <float name="m_MaxRelativeVelocity" init="4.0f" min="0.0f" max="25.0f"/>
    <float name="m_VelThresholdForPlayerMovement" init="0.1f" min="0.0f" max="10.0f"/>
    <float name="m_MaxStickyAimDistanceClose" init="5.0f" min="0.0f" max="200.0f"/>
    <float name="m_RotationDistanceModifierClose" init="0.75f" min="0.0f" max="5.0f"/>
    <float name="m_RotationDistanceModifierMin" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_RotationDistanceModifierMax" init="1.25f" min="0.0f" max="5.0f"/>
    <float name="m_RotationRelativeVelocityModifierMin" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_RotationRelativeVelocityModifierMax" init="1.25f" min="0.0f" max="5.0f"/>
    <bool name="m_EnableReticleDistanceRotationModifier" init="true"/>
    <float name="m_RotationReticleDistanceNearThreshold" init="0.075f" min="0.0f" max="5.0f"/>
    <float name="m_RotationReticleDistanceFarThreshold" init="0.025f" min="0.0f" max="5.0f"/>
    <float name="m_RotationReticleDistanceModifierMin" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_RotationReticleDistanceModifierMax_Near" init="1.25f" min="0.0f" max="5.0f"/>
    <float name="m_RotationReticleDistanceModifierMax_Far" init="1.125f" min="0.0f" max="5.0f"/>
    <bool name="m_EnableInputDirectionModifier" init="false"/>
    <float name="m_RotationInputDirectionMinDot" init="0.6f" min="-1.0f" max="1.0f"/>
    <float name="m_RotationInputDirectionModifierMin" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_RotationInputDirectionModifierMax" init="1.25f" min="0.0f" max="5.0f"/>
    <bool name="m_EnableTargetMovingIntoReticleModifier" init="true"/>
    <float name="m_TargetMovingIntoReticleModifierMin" init="0.75f" min="0.0f" max="5.0f"/>
    <float name="m_TargetMovingIntoReticleModifierMax" init="1.0f" min="0.0f" max="5.0f"/>
    <bool name="m_EnableNotLookingOrMovingModifier" init="false"/>
    <float name="m_NotLookingOrMovingModifier" init="0.75f" min="0.0f" max="1.0f"/>
    <float name="m_TargetVaultingPitchModifier" init="0.33f" min="0.0f" max="1.0f"/>
    <bool name="m_DampenRotationLerpModifiers" init="true"/>
    <float name="m_RotationLerpModifierSpringConstant" init="100.0f" min="0.0f" max="250.0f"/>
    <float name="m_RotationLerpModifierSpringDampingRatio" init="0.9f" min="0.0f" max="1.0f"/>
    <float name="m_HeadingRotationWeighting" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_PitchRotationWeighting" init="0.75f" min="0.0f" max="1.0f"/>
    <float name="m_TargetWarpToleranceNormal" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_TargetWarpToleranceBlendingOut" init="5.0f" min="0.0f" max="1000.0f"/>
  </structdef>

  <structdef type="camLookAtDampingHelperMetadata" base="camBaseObjectMetadata">
    <float name="m_HeadingSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_HeadingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_PitchSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_PitchSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_SpeedHeadingSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpeedHeadingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_SpeedPitchSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpeedPitchSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxHeadingError" init="0.6f" min="0.0f" max="1.0f"/>
    <float name="m_MaxPitchError" init="0.6f" min="0.0f" max="1.0f"/>
  </structdef>

  <structdef type="camLookAheadHelperMetadata" base="camBaseObjectMetadata">
    <float name="m_OffsetMultiplierAtMinSpeed" init="0.0f" min="0.0f" max="10.0f"/>
    <float name="m_AircraftBoatOffsetMultiplierAtMinSpeed" init="0.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinSpeed" init="0.0f" min="0.0f" max="99999.0f"/>
    <float name="m_OffsetMultiplierAtMaxForwardSpeed" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_AircraftBoatOffsetMultiplierAtMaxForwardSpeed" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxForwardSpeed" init="10.0f" min="0.0f" max="99999.0f"/>
    <bool name="m_ShouldApplyLookAheadInReverse" init="true"/>
    <float name="m_OffsetMultiplierAtMaxReverseSpeed" init="-1.0f" min="-10.0f" max="0.0f"/>
    <float name="m_AircraftBoatOffsetMultiplierAtMaxReverseSpeed" init="-1.0f" min="-10.0f" max="0.0f"/>
    <float name="m_MaxReverseSpeed" init="-10.0f" min="-99999.0f" max="0.0f"/>
    <float name="m_SpringConstant" init="1.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <!-- abstract base Switch helper type - all other Switch helper types inherit from this -->
  <structdef type="camBaseSwitchHelperMetadata" base="camBaseObjectMetadata" constructable="false">
    <u32 name="m_Duration" init="1000" min="0" max="60000"/>
    <s32 name="m_TimeToTriggerMidEffect" init="-1" min="-1" max="60000"/>
    <enum name="m_BlendCurveType" type="eCurveType" init="CURVE_TYPE_SLOW_IN_OUT"/>
    <float name="m_DesiredMotionBlurStrength" init="1.0f" min="0.0f" max="100.0f"/>
    <bool name="m_ShouldAttainDesiredMotionBlurStrengthMidDuration" init="false" description="if true the helper will attain the desired motion blur strength mid-way through the duration, rather than at the full blend level"/>
    <bool name="m_ShouldReportFinishedImmediately" init="false"/>
    <bool name="m_ShouldTriggerFpsTransitionFlash" init="false"/>
    <bool name="m_ShouldTriggerFpsTransitionFlashMp" init="false"/>
  </structdef>

  <!-- a Switch helper that swoops a parent third-person camera up to, or down from, a shot that is directly above the attach parent -->
  <structdef type="camLongSwoopSwitchHelperMetadata" base="camBaseSwitchHelperMetadata">
    <float name="m_DesiredOrbitPitch" init="-89.0f" min="-89.0f" max="89.0f"/>
    <bool name="m_ShouldAttainAttachParentHeading" init="false" description="if true the helper will blend the orbit heading towards the heading of the third-person camera's attach parent"/>
    <u32 name="m_ZoomOutDuration" init="7500" min="0" max="60000"/>
    <enum name="m_ZoomOutBlendCurveType" type="eCurveType" init="CURVE_TYPE_SLOW_OUT"/>
    <float name="m_InitialZoomOutFactor" init="1.0f" min="0.1f" max="100.0f"/>
    <float name="m_DesiredZoomOutFactor" init="0.8f" min="0.1f" max="100.0f"/>
    <u32 name="m_ZoomOut2Duration" init="0" min="0" max="60000"/>
    <enum name="m_ZoomOut2BlendCurveType" type="eCurveType" init="CURVE_TYPE_SLOW_OUT"/>
    <float name="m_DesiredZoomOut2Factor" init="0.8f" min="0.1f" max="100.0f"/>
    <float name="m_MaxOrbitDistance" init="100.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <!-- a Switch helper that rotates/orbits partially from one ped to another -->
  <structdef type="camShortRotationSwitchHelperMetadata" base="camBaseSwitchHelperMetadata">
    <float name="m_MaxAngleToRotate" init="90.0f" min="0.0f" max="180.0f"/>
  </structdef>

  <!-- a Switch helper that translates partially from one ped to another -->
  <structdef type="camShortTranslationSwitchHelperMetadata" base="camBaseSwitchHelperMetadata">
    <float name="m_MaxDistanceToTranslate" init="2.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <!-- a Switch helper that zooms in or out from one ped to another -->
  <structdef type="camShortZoomInOutSwitchHelperMetadata" base="camBaseSwitchHelperMetadata">
    <float name="m_DesiredOrbitDistanceScalingForZoomIn" init="1.0f" min="0.1f" max="100.0f"/>
    <float name="m_DesiredOrbitDistanceScalingForZoomOut" init="1.0f" min="0.1f" max="100.0f"/>
    <float name="m_DesiredZoomInFactor" init="1.0f" min="0.1f" max="100.0f"/>
    <float name="m_DesiredZoomOutFactor" init="1.0f" min="0.1f" max="100.0f"/>
    <float name="m_MinFovAfterZoom" init="5.0f" min="1.0f" max="130.0f"/>
  </structdef>

  <!-- a Switch helper that zooms in or out of the follow ped's head -->
  <structdef type="camShortZoomToHeadSwitchHelperMetadata" base="camBaseSwitchHelperMetadata">
    <bool name="m_ShouldLookAtBone" init="true"/>
    <s32 name="m_LookAtBoneTag" init="-1" min="-1" max="99999"/>
    <Vector3 name="m_BoneRelativeLookAtOffset" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_DesiredZoomFactor" init="12.0f" min="0.1f" max="100.0f"/>
    <float name="m_MinFovAfterZoom" init="5.0f" min="1.0f" max="130.0f"/>
  </structdef>

  <structdef type="camNearClipScannerMetadata" base="camBaseObjectMetadata">
    <float name="m_MaxNearClip" init="0.5f" min="0.05f" max="30.0f"/>
    <float name="m_MaxNearClipAtMinFov" init="1.5f" min="0.05f" max="30.0f"/>
    <float name="m_MinFovToConsiderForMaxNearClip" init="7.5f" min="1.0f" max="130.0f"/>
    <float name="m_MaxFovToConsiderForMaxNearClip" init="50.0f" min="1.0f" max="130.0f"/>
    <float name="m_MaxNearClipAtHighAltitude" init="0.5f" min="0.0f" max="30.0f"/>
    <float name="m_DistanceScalingForWaterTest" init="1.2f" min="0.0f" max="10.0f"/>
    <float name="m_NearClipAltitudeStartHeight" init="200.0f" min="0.05f" max="7000.0f"/>
    <float name="m_NearClipAltitudeEndHeight" init="500.0f" min="0.0f" max="8192.0f"/>
    <float name="m_HalfBoxExtentForHeightMapQuery" init="0.5f" min="0.0f" max="20.0f"/>
    <float name="m_ExtraDistanceToPullInAfterCollision" init="0.04f" min="0.0f" max="100.0f"/>
    <float name="m_ExtraDistanceToPullInAfterCollisionInFirstPerson" init="0.15f" min="0.0f" max="100.0f"/>
    <float name="m_ExtraDistanceToPullInAfterCollisionInSpecialCoverStates" init="0.45f" min="0.0f" max="100.0f"/>
    <float name="m_ExtraDistanceToPullInAfterCollisionWithFollowPed" init="0.05f" min="0.0f" max="100.0f"/>
    <float name="m_ExtraDistanceToPullInAfterCollisionWithFollowVehicle" init="0.10f" min="0.0f" max="100.0f"/>
    <float name="m_MinNearClipAfterExtraPullIn" init="0.05f" min="0.0f" max="100.0f"/>
  </structdef>

  <structdef type="camVehicleCustomSettingsMetadataDoorAlignmentSettings">
    <bool name="m_ShouldConsiderData" init="false"/>
    <bool name="m_ShouldAlignOnVehicleExit" init="true"/>
    <bool name="m_ShouldConsiderClimbDown" init="true"/>
    <float name="m_AlignmentConeOffsetTowardsVehicleFrontAngle" init="0.0f" min="0.0f" max="180.0f"/>
    <float name="m_AlignmentConeAngle" init="70.0f" min="0.0f" max="180.0f"/>
    <float name="m_AlignmentConeAngleWithTrailer" init="45.0f" min="0.0f" max="180.0f"/>
    <float name="m_MinOrientationDeltaToCut" init="360.0f" min="0.0f" max="360.0f"/>
    <float name="m_MinOrientationDeltaToCutForReverseAngle" init="360.0f" min="0.0f" max="360.0f"/>
    <float name="m_MinOrientationDeltaToCutWithTrailer" init="55.0f" min="0.0f" max="360.0f"/>
  </structdef>
  
  <structdef type="camVehicleCustomSettingsMetadataExitSeatPhaseForCameraExitSettings">
    <bool name="m_ShouldConsiderData" init="false"/>
    <float name="m_MinExitSeatPhaseForCameraExit" init="-1.0f" min="-1.0f" max="1.0f"/>  
  </structdef>
  
  <structdef type="camVehicleCustomSettingsMetadataMultiplayerPassengerCameraHashSettings">
    <bool name="m_ShouldConsiderData" init="false"/>
    <string name="m_MultiplayerPassengerCameraHash" type="atHashString"/>
    <array name="m_InvalidSeatIndices" type="atArray">
      <u32/>
    </array>
  </structdef>

  <structdef type="camVehicleCustomSettingsMetadataInvalidCinematcShotsRefsForVehicleSettings">
    <bool name="m_ShouldConsiderData" init="false"/>
    <array name="m_InvalidCinematcShot" type="atArray">
      <string type="atHashString" />
    </array>
  </structdef>

  <structdef type="camVehicleCustomSettingsMetadataAdditionalBoundScalingVehicleSettings">
    <bool name="m_ShouldConsiderData" init="false"/>
    <float name="m_HeightScaling" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camSeatSpecificCameras">
    <u32 name="m_SeatIndex" init="0"/>
    <string name="m_PovCameraHash" type="atHashString" />
    <Vector3 name="m_PovCameraOffset" init="0.0f, 0.0f, 0.0f" />
    <float name="m_ScriptedAnimHeadingOffset" init="0.0f" min="-360.0f" max="360.0f" />
    <bool name="m_DisablePovForThisSeat" init="false"/>
  </structdef>
  
  <structdef type="camVehicleCustomSettingsMetadataSeatSpecficCameras">
    <bool name="m_ShouldConsiderData" init="false"/>
    <array name="m_SeatSpecificCamera" type="atArray">
      <struct type="camSeatSpecificCameras"/>
    </array>
  </structdef>
  
  <structdef type="camVehicleCustomSettingsMetadata" base="camBaseObjectMetadata">
    <struct type="camVehicleCustomSettingsMetadataDoorAlignmentSettings" name="m_DoorAlignmentSettings"/>
    <struct type="camVehicleCustomSettingsMetadataExitSeatPhaseForCameraExitSettings" name="m_ExitSeatPhaseForCameraExitSettings"/>
    <struct type="camVehicleCustomSettingsMetadataMultiplayerPassengerCameraHashSettings" name="m_MultiplayerPassengerCameraSettings"/>
    <struct type="camVehicleCustomSettingsMetadataInvalidCinematcShotsRefsForVehicleSettings" name="m_InvalidCinematicShotRefSettings"/>
    <struct type="camVehicleCustomSettingsMetadataAdditionalBoundScalingVehicleSettings" name="m_AdditionalBoundingScaling"/>
    <struct type="camVehicleCustomSettingsMetadataSeatSpecficCameras" name="m_SeatSpecificCamerasSettings"/>
    <bool name="m_ShouldAllowCustomVehicleDriveByCameras" init="false"/>
  </structdef>
  
  <structdef type="camDepthOfFieldSettingsMetadata" base="camBaseObjectMetadata">
    <Vector2 name="m_FocusDistanceGridScaling" init="0.666666f, 0.666666f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_SubjectMagnificationPowerFactorNearFar" init="1.0f, 1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxPixelDepth" init="150.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxPixelDepthBlendLevel" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_PixelDepthPowerFactor" init="0.75f" min="0.0f" max="1.0f"/>
    <float name="m_FNumberOfLens" init="64.0f" min="0.5f" max="256.0f"/>
    <float name="m_FocalLengthMultiplier" init="1.6f" min="0.1f" max="10.0f"/>
    <float name="m_FStopsAtMaxExposure" init="0.0f" min="0.0f" max="10.0f"/>
    <float name="m_FocusDistanceBias" init="0.0f" min="-100.0f" max="100.0f"/>
    <float name="m_MaxNearInFocusDistance" init="99999.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxNearInFocusDistanceBlendLevel" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_MaxBlurRadiusAtNearInFocusLimit" init="1.0f" min="0.0f" max="30.0f"/>
    <float name="m_BlurDiskRadiusPowerFactorNear" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_FocusDistanceIncreaseSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FocusDistanceDecreaseSpringConstant" init="300.0f" min="0.0f" max="1000.0f"/>
    <bool name="m_ShouldFocusOnLookAtTarget" init="false"/>
    <bool name="m_ShouldFocusOnAttachParent" init="false"/>
    <bool name="m_ShouldKeepLookAtTargetInFocus" init="false"/>
    <bool name="m_ShouldKeepAttachParentInFocus" init="false"/>
    <bool name="m_ShouldMeasurePostAlphaPixelDepth" init="false"/>
  </structdef>

  <structdef type="camMotionBlurSettingsMetadata" base="camBaseObjectMetadata">
    <float name="m_MovementMotionBlurMinSpeed" init="3.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MovementMotionBlurMaxSpeed" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MovementMotionBlurMaxStrength" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_DamageMotionBlurMinDamage" init="0.0f" min="0.0f" max="99999.0f"/>
    <float name="m_DamageMotionBlurMaxDamage" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_DamageMotionBlurMaxStrength" init="0.025f" min="0.0f" max="1.0f"/>
    <u32 name="m_DamageMotionBlurDuration" init="500" min="0" max="60000"/>    
    <float name="m_ImpulseMotionBlurMinImpulse" init="5.0f" min="0.0f" max="99999.0f"/>
    <float name="m_ImpulseMotionBlurMaxImpulse" init="30.0f" min="0.0f" max="99999.0f"/>
    <float name="m_ImpulseMotionBlurMaxStrength" init="0.0f" min="0.0f" max="1.0f"/>
    <u32 name="m_ImpulseMotionBlurDuration" init="500" min="0" max="60000"/>
  </structdef>

  <structdef type="camInterpolatorMetadata" base="camBaseObjectMetadata">
    <float name="Duration" init="0.0f"/>
    <float name="EaseIn" init="0.0f"/>
    <float name="EaseOut" init="0.0f"/>
    <float name="Start" init="0.0f"/>
    <float name="End" init="0.0f" />
  </structdef>

  <structdef type="camVehicleRocketSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_ShakeAmplitude" init="1.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <!-- end of helper definitions -->

  <!-- abstract base camera type - all other camera types inherit from this -->
  <structdef type="camBaseCameraMetadata" base="camBaseObjectMetadata" constructable="false">
    <string name="m_ShakeRef" type="atHashString"/>
    <string name="m_CollisionRef" type="atHashString"/>
    <string name="m_DofSettings" type="atHashString"/>
    <bool name="m_CanBePaused" init="true" description="if true this camera can be paused"/>
  </structdef>

  <!-- an abstract third-person camera that attaches to an entity -->
  <structdef type="camThirdPersonCameraMetadataCustomBoundingBoxSettings">
    <float name="m_HeightScaling" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxExtraHeightForVehicleTrailers" init="1.25f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxExtraHeightForTowedVehicles" init="1.25f" min="0.0f" max="99999.0f"/>
    <float name="m_MinHeightAboveVehicleDriverSeat" init="0.75f" min="-10.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataBasePivotPositionRollSettings">
    <bool name="m_ShouldApplyAttachParentRoll" init="false"/>
    <float name="m_MinForwardSpeed" init="10.0f" min="-100.0f" max="100.0f"/>
    <float name="m_MaxForwardSpeed" init="30.0f" min="-100.0f" max="100.0f"/>
    <float name="m_AngleScalingFactor" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxAngle" init="180.0f" min="0.0f" max="180.0f"/>
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataBasePivotPosition">
    <bool name="m_ShouldUseBaseAttachPosition" init="false" description="if true the base attach position will be used (directly)"/>
    <float name="m_AttachParentHeightRatioToAttain" init="0.8f" min="-10.0f" max="10.0f"/>
    <float name="m_AttachParentHeightRatioToAttainInTightSpace" init="0.8f" min="-10.0f" max="10.0f"/>
    <bool name="m_ShouldApplyInAttachParentLocalSpace" init="false" description="if true the offset for this position will be applied in attach parent-local space, rather than in world space"/>
    <struct type="camThirdPersonCameraMetadataBasePivotPositionRollSettings" name="m_RollSettings"/>
    <Vector3 name="m_RelativeOffset" init="0.0f" min="-5000.0f" max="5000.0f"/>
    <bool name="m_ShouldLockVerticalOffset" init="false"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataPivotPosition">
    <float name="m_CameraRelativeSideOffset" init="0.3f" min="-99999.0f" max="99999.0f"/>
    <float name="m_CameraRelativeVerticalOffset" init="0.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_AttachParentWidthScalingForCameraRelativeSideOffset" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_AttachParentHeightScalingForCameraRelativeVerticalOffset" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_timeAfterAimingToApplyAlternateScalingMin" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="m_timeAfterAimingToApplyAlternateScalingMax" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="m_maxDistanceAfterAimingToApplyAlternateScalingMax" init="0.0f" min="0.0f" max="50.0f"/>
    <float name="m_timeAfterAimingToApplyDistanceBlend" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="m_AttachParentHeightScalingForCameraRelativeVerticalOffset_AfterAiming" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_AttachParentHeightScalingForCameraRelativeVerticalOffset_ForStunt" init="0.5f" min="-10.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataPivotOverBoungingBoxSettings">
    <float name="m_BlendLevel" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_ExtraCameraRelativeVerticalOffset" init="0.1f" min="-99999.0f" max="99999.0f"/>
    <float name="m_AttachParentHeightScalingForExtraCameraRelativeVerticalOffset" init="0.0f" min="-10.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataBuoyancySettings">
    <bool name="m_ShouldApplyBuoyancy" init="true"/>
    <u32 name="m_MinDelayBetweenBuoyancyStateChanges" init="500" min="0" max="60000"/>
    <u32 name="m_MinDelayOnSubmerging" init="0" min="0" max="60000"/>
    <u32 name="m_MinDelayOnSurfacing" init="0" min="0" max="60000"/>
    <u32 name="m_MinTimeSpentSwimmingToRespectMotionTask" init="1000" min="0" max="60000"/>
    <float name="m_MaxAttachParentDepthUnderWaterToRemainBuoyant" init="1.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_MaxAttachParentDepthUnderWaterToRemainBuoyantOut" init="0.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_MaxCollisionFallBackBlendLevelToForceUnderWater" init="-2.0f" min="-2.0f" max="2.0f"/>
    <bool name="m_ShouldSetBuoyantWhenAttachParentNotFullySubmerged" init="true"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataCollisionFallBackPosition">
    <float name="m_AttachParentHeightRatioToAttain" init="1.075f" min="-10.0f" max="10.0f"/>
    <float name="m_MinAttachParentHeightRatioToPushAwayFromCollision" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_MinBlendLevelToIdealPosition" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_MinBlendLevelAfterPushAwayFromCollision" init="0.0f" min="-1.0f" max="1.0f"/>
    <float name="m_MinBlendLevelAfterPushAwayFromCollisionForTripleHead" init="0.0f" min="0.0f" max="1.0f"/>
    <bool name="m_ShouldApplyMinBlendLevelAfterPushAwayFromCollisionForTripleHead" init="false" description="Applies a different minBlendLevelAfterPushAway in triple head mode"/>
    <bool name="m_ShouldApplyMinBlendLevelAfterPushAwayFromCollisionForTripleHeadWithinInteriorsOnly" init="true" description="Applies the m_MinBlendLevelAfterPushAwayFromCollisionForTripleHead value when the players is inside an interior, falling back to the m_MinBlendLevelAfterPushAwayFromCollision value outdoors"/>
    <bool name="m_ShouldApplyInAttachParentLocalSpace" init="false" description="if true the offset for this position will be applied in attach parent-local space, rather than in world space"/>
    <float name="m_SpringConstant" init="2.5f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="0.7f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataVehicleOnTopOfVehicleCollisionSettings">
    <bool name="m_ShouldApply" init="true"/>
    <u32 name="m_MaxDurationToTrackVehicles" init="1000" min="0" max="600000"/>
    <float name="m_DistanceToTestDownForVehiclesToReject" init="20.0f" min="0.0f" max="100.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataLookOverSettings">
    <float name="m_MinHeight" init="1.5f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxHeight" init="4.5f" min="0.0f" max="99999.0f"/>
    <float name="m_PitchOffsetAtMinHeight" init="0.0f" min="-90.0f" max="90.0f"/>
    <float name="m_PitchOffsetAtMaxHeight" init="0.0f" min="-90.0f" max="90.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataStealthZoomSettings">
    <bool name="m_ShouldApply" init="false"/>
    <float name="m_MaxZoomFactor" init="1.25f" min="0.1f" max="10.0f"/>
    <float name="m_SpringConstant" init="4.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonCameraMetadataQuadrupedalHeightSpring">
    <bool name="m_ShouldApply" init="false"/>
    <float name="m_SpringConstant" init="50.0f" min="0.0f" max="100.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxHeightDistance" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camThirdPersonCameraMetadataCustomOrbitDistanceSettings">
    <bool name="m_Enabled" init="false"/>
    <float name="m_ScreenRatio" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_LimitsSpringConstant" init="5.0f" min="0.0f" max="99999.0f"/>
  </structdef>

  <structdef type="camThirdPersonCameraMetadataCustomOrbitDistance">
    <struct name="m_Arresting" type="camThirdPersonCameraMetadataCustomOrbitDistanceSettings"/>
  </structdef>

  <structdef type="camThirdPersonCameraMetadata" base="camBaseCameraMetadata" constructable="false">
    <string name="m_ControlHelperRef" type="atHashString"/>
    <string name="m_HintHelperRef" type="atHashString"/>
    <string name="m_CatchUpHelperRef" type="atHashString"/>
    <string name="m_BaseAttachVelocityToIgnoreEnvelopeRef" type="atHashString"/>

    <float name="m_BaseFov" init="50.0f" min="1.0f" max="130.0f"/>
    <float name="m_BaseNearClip" init="0.1f" min="0.05f" max="0.5f"/>
    <float name="m_NearClipForTripleHead" init="0.05f" min="0.05f" max="0.5f"/>

    <bool name="m_ShouldOrbitRelativeToAttachParentOrientation" init="false"/>
    <bool name="m_ShouldPersistOrbitOrientationRelativeToAttachParent" init="false"/>

    <float name="m_AttachParentMatrixForRelativeOrbitSpringConstant" init="5.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachParentMatrixForRelativeOrbitSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>

    <float name="m_AttachParentMatrixForRelativeOrbitSpringConstant_ForStunt" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachParentMatrixForRelativeOrbitSpringDampingRatio_ForStunt" init="1.0f" min="0.0f" max="10.0f"/>
    
    <float name="m_MaxAttachParentSubmergedLevelToApplyFullAttachParentMatrixForRelativeOrbit" init="0.2f" min="0.0f" max="1.0f"/>
    <float name="m_MinAircraftGroundSpeedToApplyFullAttachParentMatrixForRelativeOrbit" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MinAircraftContactSpeedToApplyFullAttachParentMatrixForRelativeOrbit" init="5.0f" min="0.0f" max="99999.0f"/>
    <u32 name="m_MinHoldTimeToBlockFullAttachParentMatrixForRelativeOrbit" init="2000" min="0" max="60000"/>

    <float name="m_MaxAttachParentSpeedToClonePitchFromCinematicMountedCameras" init="1.0f" min="0.0f" max="99999.0f"/>
    
    <bool name="m_ShouldUseCustomFramingInTightSpace" init="false"/>
    <float name="m_MinAttachSpeedToUpdateTightSpaceLevel" init="0.25f" min="0.0f" max="1000.0f"/>
    <float name="m_TightSpaceSpringConstant" init="2.5f" min="0.0f" max="1000.0f"/>
    <float name="m_TightSpaceSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <string name="m_DofSettingsInTightSpace" type="atHashString"/>

    <bool name="m_ShouldAttachToParentCentreOfGravity" init="false"/>
    <bool name="m_ShouldUseDynamicCentreOfGravity" init="true"/>
    <bool name="m_ShouldIgnoreVelocityOfAttachParentAttachEntity" init="true"/>

    <struct type="camThirdPersonCameraMetadataCustomBoundingBoxSettings" name="m_CustomBoundingBoxSettings"/>

    <bool name="m_ShouldApplyAttachPedPelvisOffset" init="false" description="if true the offset that the leg IK applies to the attach ped's pelvis will be reflected in the camera (with damping)"/>
    <float name="m_AttachPedPelvisOffsetSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachPedPelvisOffsetSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>

    <struct type="camThirdPersonCameraMetadataBasePivotPosition" name="m_BasePivotPosition"/>
    <struct type="camThirdPersonCameraMetadataPivotPosition" name="m_PivotPosition"/>
    <struct type="camThirdPersonCameraMetadataPivotOverBoungingBoxSettings" name="m_PivotOverBoundingBoxSettings"/>

    <float name="m_ScreenRatioForMinFootRoom" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoom" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMinFootRoomInTightSpace" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoomInTightSpace" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_BasePivotHeightScalingForFootRoom" init="1.0f" min="-10.0f" max="10.0f"/>
    <bool name="m_ShouldIgnoreVerticalPivotOffsetForFootRoom" init="false"/>

    <struct type="camThirdPersonCameraMetadataCustomOrbitDistance" name="m_CustomOrbitDistanceBehaviors"/>

    <float name="m_MinSafeOrbitDistanceScalingForExtensions" init="1.2f" min="1.0f" max="2.0f"/>
    <float name="m_BaseOrbitDistanceOffset" init="0.0f" min="-10.0f" max="10.0f"/>

    <Vector2 name="m_CustomOrbitDistanceLimitsToForce" init="-1.0f, -1.0f" min="-1.0f" max="100.0f"/>

    <float name="m_OrbitDistanceLimitSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_OrbitDistanceLimitSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>

    <float name="m_OrbitDistanceScalingForCustomFirstPersonFallBack" init="0.25f" min="0.1f" max="10.0f"/>
    
    <struct type="camThirdPersonCameraMetadataBuoyancySettings" name="m_BuoyancySettings"/>

    <bool name="m_ShouldIgnoreCollisionWithAttachParent" init="true"/>
    <bool name="m_ShouldIgnoreCollisionWithFollowVehicle" init="true"/>
    <bool name="m_ShouldIgnoreFollowVehicleForCollisionOrigin" init="false"/>
    <bool name="m_ShouldIgnoreFollowVehicleForCollisionRoot" init="false"/>
    <bool name="m_ShouldPushBeyondAttachParentIfClipping" init="false"/>
    <float name="m_MaxCollisionTestRadius" init="0.24f" min="0.0f" max="10.0f"/>
    <float name="m_MinSafeRadiusReductionWithinPedMoverCapsule" init="0.075f" min="0.0f" max="100.0f"/>
    <float name="m_CollisionTestRadiusSpringConstant" init="2.5f" min="0.0f" max="1000.0f"/>
    <float name="m_CollisionTestRadiusSpringDampingRatio" init="0.7f" min="0.0f" max="10.0f"/>
    <Vector3 name="m_CustomCollisionOriginRelativePosition" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_CustomCollisionOriginRelativePositionForTripleHead" init="0.0f" min="-100.0f" max="100.0f"/>
    <bool name="m_ShouldUseCustomCollisionOrigin" init="false"/>
    <bool name="m_ShouldUseCustomCollisionOriginForTripleHead" init="false"/>
    <bool name="m_ShouldApplySideAdjustedCollisionOriginRelativePositionForTripleHead" init="false"/>
    <Vector3 name="m_ExtentsForSideAdjustedCollisionOriginRelativePositionTripleHead" init="0.0f" min="-100.0f" max="100.0f"/>
    <float name="m_SideAdjustedCollisionTestRadius" init="0.2f" min="0.0f" max="10.0f"/>    
    <struct type="camThirdPersonCameraMetadataCollisionFallBackPosition" name="m_CollisionFallBackPosition"/>
    <float name="m_CollisionRootPositionFallBackToPivotBlendValue" init="0.0f" min="0.0f" max="2.0f"/>
    <bool name="m_ShouldConstrainCollisionRootPositionAgainstClippingTypes" init="true"/>
    <float name="m_CollisionRootPositionSpringConstant" init="2.5f" min="0.0f" max="1000.0f"/>
    <float name="m_CollisionRootPositionSpringDampingRatio" init="0.7f" min="0.0f" max="10.0f"/>

    <struct type="camThirdPersonCameraMetadataVehicleOnTopOfVehicleCollisionSettings" name="m_VehicleOnTopOfVehicleCollisionSettings"/>

    <float name="m_IdealHeadingOffsetForLimiting" init="0.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_RelativeOrbitHeadingLimits" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_OrbitPitchLimits" init="-70.0f, 45.0f" min="-89.0f" max="89.0f"/>

    <float name="m_BaseOrbitPitchOffset" init="0.0f" min="-90.0f" max="90.0f"/>
    <float name="m_BaseOrbitPitchOffsetInTightSpace" init="0.0f" min="-90.0f" max="90.0f"/>

    <struct type="camThirdPersonCameraMetadataLookOverSettings" name="m_LookOverSettings"/>

    <bool name="m_ShouldIgnoreAttachParentPitchForLookBehind" init="false"/>

    <Vector2 name="m_OrbitDistanceLimitsForBasePosition" init="0.0f, 99999.0f" min="0.0f" max="99999.0f"/>

    <float name="m_PreToPostCollisionLookAtOrientationBlendValue" init="0.625f" min="0.0f" max="1.0f"/>

    <float name="m_AttachParentRollSpringConstant" init="0.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachParentRollSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <Vector2 name="m_AttachParentRollDampingPitchSoftLimits" init="0.0f, 0.0f" min="-90.0f" max="90.0f"/>
    <Vector2 name="m_AttachParentRollDampingPitchHardLimits" init="-80.0f, 80.0f" min="-90.0f" max="90.0f"/>

    <struct type="camThirdPersonCameraMetadataStealthZoomSettings" name="m_StealthZoomSettings"/>
    <struct type="camThirdPersonCameraMetadataQuadrupedalHeightSpring" name="m_QuadrupedalHeightSpringSetting" />
    
    <string name="m_MotionBlurSettings" type="atHashString"/>

    <bool name="m_ShouldPreventAnyInterpolation" init="false"/>
  </structdef>
    
  <!-- Table games camera. Attached to the ped with a pivot offset -->
  <structdef type="camTableGamesSettingsMetadata">
    <Vector3 name="m_RelativePivotOffset" init="0.0f" min="-99.0f" max="99.0f"/>
    <float name="m_OrbitDistanceScalar" init="1.0f" min="0.01f" max="5.0f"/>
    <Vector2 name="m_HeadingLimits" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_PitchLimits" init="-60.0f, 60.0f" min="-80.0f" max="80.0f"/>
    <bool name="m_ShouldApplyInitialRotation" init="false"/>
    <float name="m_InitialRelativeHeading" init="0.0f" min="-180.0f" max="180.0f"/>
    <float name="m_InitialRelativePitch" init="0.0f" min="-80.0f" max="80.0f"/>
    <int name="m_InterpolationDurationMS" init="2000" min="0" max="99999"/>
  </structdef>

  <structdef type="camThirdPersonCameraSettings" base="camThirdPersonCameraMetadata">
    <!-- empty struct to work around the fact that camThirdPersonCameraMetadata is non-constructible -->
  </structdef>

  <structdef type="camTableGamesCameraMetadata" base="camBaseObjectMetadata">
    <struct type="camThirdPersonCameraSettings" name="m_ThirdPersonCameraSettings"/>
    <struct type="camTableGamesSettingsMetadata" name="m_TableGamesSettings"/>
  </structdef>

  <!-- an abstract camera that follows an entity -->
  <structdef type="camFollowCameraMetadataFollowOrientationConing">
    <float name="m_MaxAngle" init="-1.0f" min="-1.0f" max="180.0f"/>
    <float name="m_AspectRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_HeadingOffset" init="0.0f" min="-180.0f" max="180.0f"/>
    <float name="m_PitchOffset" init="0.0f" min="-90.0f" max="90.0f"/>
    <float name="m_SmoothRate" init="0.075f" min="0.0f" max="1.0f"/>
  </structdef>
  <structdef type="camFollowCameraMetadataPullAroundSettings">
    <bool name="m_ShouldBlendOutWhenAttachParentIsInAir" init="false"/>
    <bool name="m_ShouldBlendOutWhenAttachParentIsOnGround" init="false"/>
    <bool name="m_ShouldBlendWithAttachParentMatrixForRelativeOrbitBlend" init="false"/>
    <float name="m_HeadingPullAroundMinMoveSpeed" init="0.0f" min="0.0f" max="99999.0f"/>
    <float name="m_HeadingPullAroundMaxMoveSpeed" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_HeadingPullAroundSpeedAtMaxMoveSpeed" init="80.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_HeadingPullAroundErrorScalingBlendLevel" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_HeadingPullAroundSpringConstant" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_HeadingPullAroundSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_PitchPullAroundMinMoveSpeed" init="0.0f" min="0.0f" max="99999.0f"/>
    <float name="m_PitchPullAroundMaxMoveSpeed" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_PitchPullAroundSpeedAtMaxMoveSpeed" init="0.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PitchPullAroundErrorScalingBlendLevel" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_PitchPullAroundSpringConstant" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_PitchPullAroundSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camFollowCameraMetadataRollSettings">
    <bool name="m_ShouldApplyRoll" init="false"/>
    <float name="m_RollSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_RollSpringDampRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinForwardSpeed" init="10.0f" min="-100.0f" max="100.0f"/>
    <float name="m_MaxForwardSpeed" init="20.0f" min="-100.0f" max="100.0f"/>
    <float name="m_RollAngleScale" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxRoll" init="15.0f" min="0.0f" max="89.0f"/>
  </structdef>
  <structdef type="camFollowCameraMetadataHighAltitudeZoomSettings">
    <float name="m_MinAltitudeDelta" init="50.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_MaxAltitudeDelta" init="250.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_SpringConstant" init="0.5f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.3f" min="0.0f" max="10.0f"/>
    <float name="m_MaxBaseFovScaling" init="1.4f" min="1.0f" max="2.0f"/>
  </structdef>

  <structdef type="camFollowCameraMetadata" base="camThirdPersonCameraMetadata" constructable="false">
    <string name="m_AttachParentInAirEnvelopeRef" type="atHashString"/>
    <string name="m_AttachParentUpwardSpeedScalingOnGroundEnvelopeRef" type="atHashString"/>
    <string name="m_AttachParentUpwardSpeedScalingInAirEnvelopeRef" type="atHashString"/>
    <string name="m_AimBehaviourEnvelopeRef" type="atHashString"/>
    <string name="m_WaterBobShakeRef" type="atHashString"/>

    <bool name="m_ShouldIgnoreAttachParentMovementForOrientation" init="false"/>

    <struct type="camFollowCameraMetadataPullAroundSettings" name="m_PullAroundSettings"/>
    <struct type="camFollowCameraMetadataPullAroundSettings" name="m_PullAroundSettingsForLookBehind"/>
    <bool name="m_ShouldConsiderAttachParentLocalXYVelocityForPullAround" init="true"/>
    <bool name="m_ShouldConsiderAttachParentForwardSpeedForPullAround" init="false"/>
    <bool name="m_ShouldPullAroundToAttachParentFront" init="false"/>
    <bool name="m_ShouldPullAroundToBasicAttachParentMatrix" init="false"/>
    <bool name="m_ShouldPullAroundUsingSimpleSpringDamping" init="false"/>
    <bool name="m_ShouldPullAroundWhenUsingMouse" init="true"/>

    <float name="m_MinAttachParentApproachSpeedForPitchLock" init="0.5f" min="0.0f" max="99999.0f"/>
    <bool name="m_ShouldLockHeading" init="false"/>
    <float name="m_MaxMoveSpeedForFollowOrientation" init="20.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxLookAroundMoveSpeedMultiplier" init="1.0f" min="0.0f" max="10.0f"/>
    <Vector2 name="m_SpeedLimitsForVerticalMoveSpeedScaling" init="99999.0f, 99999.0f" min="0.0f" max="99999.0f"/>
    <float name="m_VerticalMoveSpeedScaling" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_VerticalMoveSpeedScalingAtMaxSpeed" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_UpwardMoveSpeedScalingOnGround" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_UpwardMoveSpeedScalingInAir" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxMoveOrientationSpeedDuringLookAround" init="0.0f" min="0.0f" max="1000.0f"/>
    <struct type="camFollowCameraMetadataFollowOrientationConing" name="m_FollowOrientationConing"/>
   
    <struct type="camFollowCameraMetadataRollSettings" name="m_RollSettings"/>

    <struct type="camFollowCameraMetadataHighAltitudeZoomSettings" name="m_HighAltitudeZoomSettings"/>
  </structdef>

  <!-- an abstract camera that attachs to an entity and provides basic aiming functionality -->
  <structdef type="camAimCameraMetadata" base="camBaseCameraMetadata" constructable="false">
    <string name="m_ControlHelperRef" type="atHashString"/>

    <float name="m_BaseFov" init="45.0f" min="1.0f" max="130.0f"/>
    <float name="m_BaseNearClip" init="0.1f" min="0.05f" max="0.5f"/>
    
    <u32 name="m_MinUpdatesBeforeApplyingMotionBlur" init="2" min="0" max="99999"/>
    <float name="m_BaseMotionBlurStrength" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_ZoomMotionBlurMinFovDelta" init="0.1f" min="0.0f" max="100.0f"/>
    <float name="m_ZoomMotionBlurMaxFovDelta" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_ZoomMotionBlurMaxStrengthForFov" init="0.1f" min="0.0f" max="1.0f"/>

    <Vector3 name="m_AttachRelativeOffset" init="0.0f" min="-100.0f" max="100.0f"/>
    <bool name="m_ShouldApplyAttachOffsetRelativeToCamera" init="true"/>

    <float name="m_MinPitch" init="-85.0f" min="-90.0f" max="90.0f"/>
    <float name="m_MaxPitch" init="60.0f" min="-90.0f" max="90.0f"/>
    <float name="m_MinRelativeHeading" init="-180.0f" min="-180.0f" max="180.0f"/>
    <float name="m_MaxRelativeHeading" init="180.0f" min="-180.0f" max="180.0f"/>
  </structdef>

  <!-- a first-person aim camera -->
  <structdef type="camFirstPersonAimCameraMetadataHeadingCorrection">
    <float name="m_SpringConstant" min="0.0f" max="500.0f" step="0.01f" init="5.5f"/>
    <float name="m_AwayDampingRatio" min="0.0f" max="10.0f" step="0.01f" init="0.9f"/>
    <float name="m_DeltaTolerance" min="0.0f" max="1.0f" step="0.01f" init="0.001f"/>
  </structdef>

  <structdef type="camFirstPersonAimCameraMetadata" base="camAimCameraMetadata" constructable="false">
    <bool name="m_ShouldMakeAttachedEntityInvisible" init="true" description="if true the camera will make the attached entity (locally) invisible"/>
    <bool name="m_ShouldMakeAttachedPedHeadInvisible" init="false" description="if true the camera will make the attached ped's head (locally) invisible"/>
    <bool name="m_ShouldDisplayReticule" init="true" description="if true the camera will allow the HUD reticule to be displayed"/>
    <bool name="m_IsOrientationRelativeToAttached" init="false"/>
    <Vector2 name="m_ShakeFirstPersonShootingAbilityLimits" init="0.0f, 100.0f" min="0.0f" max="100.0f"/>
    <Vector2 name="m_ShakeAmplitudeScalingForShootingAbilityLimits" init="2.5f, 0.5f" min="0.0f" max="30.0f"/>
    <struct type="camFirstPersonAimCameraMetadataHeadingCorrection" name="m_AimHeadingCorrection"/>
    <float name="m_HighCoverEdgeCameraAngleAimOffsetDegrees" init="0.0f" min="-90.0f" max="90.0f" description="the amount from the cover line to correct the camera when aiming from high cover edges"/>
    <string name="m_StickyAimHelperRef" type="atHashString"/>
  </structdef>

  <!-- a first-person aim camera that is attached to a ped -->
  <structdef type="camFirstPersonPedAimCameraMetadata" base="camFirstPersonAimCameraMetadata">
    <s32 name="m_AttachBoneTag" init="31086" min="-1" max="99999"/>
    <float name="m_TripleHeadNearClip" init="0.0f" min="0.0f" max="0.5f"/>
    <float name="m_RelativeAttachPositionSmoothRate" init="0.2f" min="0.0f" max="1.0f"/>
    <bool name="m_ShouldTorsoIkLimitsOverrideOrbitPitchLimits" init="true" description="if true the attach ped's torso IK limits will be used as the orbit pitch limits"/>
    <bool name="m_ShouldUseHeliPitchLimits" init="false" description="if true custom limits will be used when inside a helicopter"/>
    <float name="m_MinPitchInHeli" init="-20.0f" min="-90.0f" max="90.0f"/>
    <float name="m_MaxPitchInHeli" init="60.0f" min="-90.0f" max="90.0f"/>
    <bool name="m_ShouldApplyFocusLock" init="false" description="if true the measured focus distance of the adaptive DOF behaviour will be locked in the presence of the focus lock control input"/>
  </structdef>

  <structdef type="camFirstPersonShooterCameraMetadataOrientationSpring">
    <Vector2 name="m_HeadingLimits" init="-45.0f, 45.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_PitchLimits" init="-10.0f, 25.0f" min="-80.0f" max="180.0f"/>
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="50.0f"/>
    <float name="m_SpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_LookBehindSpringConstant" init="500.0f" min="0.0f" max="5000.0f"/>
    <float name="m_LookBehindSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camFirstPersonShooterCameraMetadataOrientationSpringLite">
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camFirstPersonShooterCameraMetadataRelativeAttachOrientationSettings">
    <float name="m_BlendLevelWhenIdle" init="0.02f" min="0.0f" max="1.0f"/>
    <float name="m_BlendLevelWhenWalking" init="0.125f" min="0.0f" max="1.0f"/>
    <float name="m_BlendLevelWhenRunning" init="0.15f" min="0.0f" max="1.0f"/>
    <float name="m_BlendLevelWhenSprinting" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_BlendLevelWhenSwimSprinting" init="0.2f" min="0.0f" max="1.0f"/>
    <float name="m_BlendLevelWhenAiming" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_BlendLevelWhenDiving" init="0.1f" min="0.0f" max="1.0f"/>
    <bool name="m_ShouldConsiderArmedAsAiming" init="true"/>
    <float name="m_HeadingBlendLevelScalar" init="0.7f" min="0.0f" max="1.0f"/>
    <float name="m_PitchBlendLevelScalar" init="0.775f" min="0.0f" max="1.0f"/>
    <float name="m_RollBlendLevelScalar" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_BlendLevelSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlendLevelSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_LockOnBlendLevelSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_LockOnBlendLevelSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxPitchOffset" init="20.0f" min="-90.0f" max="90.0f"/>
    <Vector3 name="m_PerAxisSpringConstants" init="10.0f" min="0.0f" max="1000.0f"/>
    <Vector3 name="m_PerAxisSpringDampingRatios" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camFirstPersonShooterCameraMetadataCoverSettings">
    <float name="m_ScriptedLowCoverEdgeCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-25.0f"/>
    <float name="m_ScriptedLowCoverWallCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-25.0f"/>
    <float name="m_MinLowCoverHeightOffsetFromMover" min="-1.0f" max="2.0f" step="0.1f" init="-0.15f"/>
    <float name="m_MaxLowCoverHeightOffsetFromMover" min="-1.0f" max="2.0f" step="0.1f" init="0.05f"/>
    <float name="m_MinLowCoverPitch" min="-PI" max="PI" step="0.01f" init="0.0f"/>
    <float name="m_MaxLowCoverPitch" min="-PI" max="PI" step="0.01f" init="1.047198f"/>
    <float name="m_MinLowCoverEdgeCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-90.0f"/>
    <float name="m_MaxLowCoverEdgeCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-50.0f"/>
    <float name="m_MinLowCoverWallCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-90.0f"/>
    <float name="m_MaxLowCoverWallCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-30.0f"/>
    <float name="m_HighCoverEdgeCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-30.0f"/>
    <float name="m_HighCoverWallCameraAngleOffsetDegrees" min="-180.0f" max="180.0f" step="1.0f" init="-10.0f"/>
    <struct type="camFirstPersonAimCameraMetadataHeadingCorrection" name="m_HeadingCorrection"/>
    <int name="m_MinTimeDeltaForAutoHeadingCorrectionMS" min="0" max="20000" step="50" init="250"/>
    <int name="m_MinTimeDeltaForAutoHeadingCorrectionAimOutroMS" min="0" max="20000" step="50" init="400"/>
    <int name="m_MinTimeDeltaForAutoHeadingCorrectionAimIntroMS" min="0" max="2000" step="50" init="100"/>
    <int name="m_MinTimeDeltaForAutoHeadingCorrectionAimMS" min="0" max="2000" step="50" init="150"/>
    <int name="m_MinTimeDeltaForAutoHeadingCorrectionMoveTurnMS" min="0" max="2000" step="50" init="0"/>
    <int name="m_ExtraTimeDeltaForAutoHeadingCorrectionResetMS" min="0" max="2000" step="50" init="150"/>
    <float name="m_MaxCameraAngleFromEdgeCoverForAutoHeadingCorrectionDegreesOffset" min="-90.0f" max="90.0f" step="1.0f" init="0.0f"/>
    <float name="m_MaxCameraAngleFromWallCoverForAutoHeadingCorrectionDegreesOffset" min="-90.0f" max="90.0f" step="1.0f" init="0.0f"/>
    <u32 name="m_AimSensitivityClampWhileInCover" init="6" min="0" max="20"/>
  </structdef>

  <structdef type="camFirstPersonShooterCameraMetadataSprintBreakOutSettings">
    <float name="m_SprintBreakOutHeadingAngleMin" init="45.f" min="0.0f" max="180.0f"/>
    <float name="m_SprintBreakOutHeadingRateMin" init="0.175f" min="0.0f" max="1.0f"/>
    <float name="m_SprintBreakOutHeadingRateMax" init="0.175f" min="0.0f" max="1.0f"/>
    <float name="m_SprintBreakOutHeadingFinishAngle" init="0.01f" min="0.0f" max="180.0f"/>
    <float name="m_SprintBreakOutHoldAtEndTime" init="0.1f" min="0.0f" max="1.0f"/>
    <float name="m_SprintBreakMaxTime" init="1.f" min="0.0f" max="10.0f"/>
  </structdef>

  <enumdef type="camFirstPersonShooterCameraMetadataSprintBreakOutSettingTypes">
    <enumval name="SBOST_SPRINTING"/>
    <enumval name="SBOST_JUMPING"/>
  </enumdef>

  <structdef type="camFirstPersonShooterCameraMetadataStickyAim">
    <float name="m_TargetPositionLerpMinVelocity" init="0.1f" min="0.01f" max="1.0f"/>
    <float name="m_TargetPositionLerpMaxVelocity" init="0.1f" min="0.01f" max="1.0f"/>
    <float name="m_HeadingRateAtMinLeftStickStrength" init="0.008f" min="0.000f" max="1.0f"/>
    <float name="m_HeadingRateAtMaxLeftStickStrength" init="0.001f" min="0.000f" max="1.0f"/>
    <float name="m_LeftStickAtMinStength" init="0.5f" min="0.0f" max="5.0f"/>
    <float name="m_LeftStickAtMaxStength" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_MinDistanceRateModifier" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_MaxDistanceRateModifier" init="0.35f" min="0.0f" max="5.0f"/>
    <float name="m_MaxRelativeVelocity" init="2.0f" min="0.0f" max="5.0f"/>
    <float name="m_MinVelRateModifier" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_MaxVelRateModifier" init="0.0f" min="0.0f" max="5.0f"/>
    <float name="m_RightStickAtMinStength" init="0.2f" min="0.0f" max="5.0f"/>
    <float name="m_RightStickAtMaxStength" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_MinRightStickRateModifier" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_MaxRightStickRateModifier" init="0.5f" min="0.0f" max="5.0f"/>
    <float name="m_HeadingMagnetismLerpMin" init="0.2f" min="0.0f" max="1.0f"/>
    <float name="m_HeadingMagnetismLerpMax" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_HeadingMagnetismTimeForMinLerp" init="0.0f" min="0.01f" max="5.0f"/>
    <float name="m_HeadingMagnetismTimeForMaxLerp" init="0.5f" min="0.01f" max="5.0f"/>
    <float name="m_PitchMagnetismLerpMin" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_PitchMagnetismLerpMax" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_PitchMagnetismTimeForMinLerp" init="0.0f" min="0.01f" max="5.0f"/>
    <float name="m_PitchMagnetismTimeForMaxLerp" init="0.0f" min="0.01f" max="5.0f"/>
    <float name="m_StickyLimitsXY" init="1.0f" min="0.01f" max="5.0f"/>
    <float name="m_StickyLimitsZ" init="1.0f" min="0.01f" max="5.0f"/>
  </structdef>

  <structdef type="camFirstPersonShooterCameraMetadata" base="camFirstPersonAimCameraMetadata">
    <string name="m_HintHelperRef" type="atHashString"/>
    <Vector3 name="m_LookDownRelativeOffset" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_UnarmedLookDownRelativeOffset" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_SwimmingLookDownRelativeOffset" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_SwimmingStillRelativeOffset" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_SwimmingRunRelativeOffset" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeCollisionFallBackPosition" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_AttachRelativeOffsetStealth" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_AttachRelativeOffsetStealthTripleHead" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_AttachRelativeOffsetArmoured" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector2 name="m_AttachParentRelativeHeading" init="-110.0f, 110.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_ParachutePitchLimit" init="-10.0f, 30.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_ParachuteHeadingLimit" init="-45.0f, 45.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_PhoneToEarHeadingLimit" init="-45.0f, 85.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_SwimmingHeadingLimit" init="-60.0f, 60.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_HeadingLimitForScriptControl" init="35.0f, 38.0f" min="0.0f" max="180.0f"/>
    <string name="m_AimControlHelperRef" type="atHashString"/>
    <string name="m_LookBehindEnvelopeRef" type="atHashString"/>
    <string name="m_LockOnEnvelopeRef" type="atHashString"/>
    <string name="m_StickyAimEnvelopeRef" type="atHashString"/>
    <string name="m_ZoomInShakeRef" type="atHashString"/>
    <string name="m_BulletImpactShakeRef" type="atHashString"/>
    <string name="m_MeleeImpactShakeRef" type="atHashString"/>
    <string name="m_VehicleImpactShakeRef" type="atHashString"/>
    <string name="m_DeathShakeRef" type="atHashString"/>
    <s32 name="m_AttachBoneTag" init="31086" min="-1" max="99999"/>
    <float name="m_RelativeAttachPositionSmoothRate" init="0.7f" min="0.0f" max="1.0f"/>
    <struct type="camFirstPersonShooterCameraMetadataRelativeAttachOrientationSettings" name="m_RelativeAttachOrientationSettings"/>
    <float name="m_LookBehindAngle" init="160.0f"  min="0.0f" max="180.0f"/>
    <float name="m_MaxRollForLookBehind" init="3.0f" min="0.0f" max="30.0f"/>
    <float name="m_AngleStrafeThreshold" init="90.0f" min="0.0f" max="180.0f"/>
    <float name="m_MaxBaseFov" init="55.0f" min="0.0f" max="180.0f"/>
    <float name="m_AimFov" init="45.0f" min="0.0f" max="180.0f"/>
    <float name="m_AimFovBlend" init="0.15f" min="0.0f" max="1.0f"/>
    <float name="m_CombatRollScopeFovBlend" init="0.15f" min="0.0f" max="1.0f"/>   
    <float name="m_PhoneFov" init="27.0f" min="0.0f" max="180.0f"/>
    <float name="m_PhoneFovBlend" init="0.15f" min="0.0f" max="1.0f"/>
    <float name="m_StealthFovOffset" init="2.0f" min="-180.0f" max="180.0f"/>
    <float name="m_StealthFovBlend" init="0.15f" min="0.0f" max="1.0f"/>
    <float name="m_MinNearClip" init="0.02f" min="0.01f" max="10.0f"/>
    <float name="m_PhoneLookHeadingBlend" init="0.30f" min="0.0f" max="1.0f"/>
    <float name="m_PhoneLookPitchBlend" init="0.20f" min="0.0f" max="1.0f"/>
    <float name="m_VehicleEntryHeadingBlend" init="0.30f" min="0.0f" max="1.0f"/>
    <float name="m_VehicleEntryPitchBlend" init="0.20f" min="0.0f" max="1.0f"/>
    <float name="m_VaultClimbHeadingBlend" init="0.40f" min="0.0f" max="1.0f"/>
    <float name="m_JumpHeadingBlend" init="0.20f" min="0.0f" max="1.0f"/>
    <float name="m_ClimbPitchBlend" init="0.25f" min="0.0f" max="1.0f"/>
    <float name="m_VaultPitchBlend" init="0.30f" min="0.0f" max="1.0f"/>
    <float name="m_SprintPitchBlend" init="0.05f" min="0.0f" max="1.0f"/>
    <float name="m_SlopePitchBlend" init="0.03f" min="0.0f" max="1.0f"/>
    <float name="m_SkyDivePitch" init="-80.0f" min="-89.0f" max="89.0f"/>
    <float name="m_SkyDiveDeployPitch" init="55.0f" min="-89.0f" max="89.0f"/>
    <float name="m_SkyDivePitchBlend" init="0.05f" min="0.01f" max="1.0f"/>
    <float name="m_ParachutePitchBlendDeploy" init="0.1125f" min="0.01f" max="1.0f"/>
    <float name="m_ParachutePitchBlend" init="0.033f" min="0.01f" max="1.0f"/>
    <float name="m_DivingHeadingPitchBlend" init="0.05f" min="0.01f" max="1.0f"/>
    <float name="m_LadderPitch" init="45.0f" min="0.0f" max="85.0f"/>
    <float name="m_LookOverrideEndPitchBlend" init="0.10f" min="0.0f" max="1.0f"/>
    <float name="m_LookOverrideMeleeEndPitchBlend" init="0.15f" min="0.0f" max="1.0f"/>
    <float name="m_LookOverridePhoneEndHeadingBlend" init="0.15f" min="0.0f" max="1.0f"/>
    <float name="m_RecoilShakeAmplitudeScaling" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_SlopePitchOffset" init="2.0f" min="0.0f" max="45.0f"/>
    <float name="m_SlopePitchMinSpeed" init="1.0f" min="0.0f" max="20.0f"/>
    <float name="m_SlopePitchMinSlope" init="0.0f" min="0.0f" max="90.0f"/>
    <float name="m_SlopePitchMaxSlope" init="60.0f" min="0.0f" max="90.0f"/>
    <bool name="m_ShouldConstrainToFallBackPosition" init="true"/>
    <float name="m_MaxCollisionFallBackTestRadius" init="0.24f" min="0.01f" max="10.0f"/>
    <float name="m_MinSafeRadiusReductionWithinPedMoverCapsule" init="0.075f" min="0.0f" max="100.0f"/>
    <float name="m_CollisionFallBackBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CollisionFallBackBlendSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_MaxPedSpineBoneRotationForCollisionFallBack" init="-60.0f" min="-180.0f" max="180.0f"/>
    <u32 name="m_AimBlendDuration" init="250" min="0" max="5000"/>
    <u32 name="m_LookBehindBlendDuration" init="250" min="0" max="5000"/>
    <u32 name="m_LookAtProneMeleeTargetDelay" init="1000" min="0" max="10000"/>
    <u32 name="m_LookAtDeadMeleeTargetDelay" init="1500" min="0" max="10000"/>
    <u32 name="m_TakeOutPhoneDelay" init="1000" min="0" max="10000"/>
    <u32 name="m_PutAwayPhoneDelay" init="300" min="0" max="10000"/>
    <u32 name="m_HeadSpineBlendInDuration" init="1000" min="0" max="10000"/>
    <u32 name="m_HeadSpineRagdollBlendInDuration" init="500" min="0" max="10000"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpring" name="m_OrientationSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpring" name="m_OrientationSpringParachute"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpring" name="m_OrientationSpringSkyDive"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_OrientationSpringMelee"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleHeadingLimitSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehiclePitchLimitSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleCarJackHeadingSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleCarJackPitchSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleSeatBlendHeadingMinSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleSeatBlendPitchMinSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleSeatBlendHeadingMaxSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleSeatBlendPitchMaxSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleEnterHeadingSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleEnterPitchSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleExitHeadingSpring"/>
    <struct type="camFirstPersonShooterCameraMetadataOrientationSpringLite" name="m_VehicleExitPitchSpring"/>
    <float name="m_OrientationSpringBlend" init="0.15" min="0.0f" max="1.0f"/>
    <Vector2 name="m_HeadingSpringLimitForMobilePhoneAtEar" init="-15.0f, 15.0f" min="-180.0f" max="180.0f"/>
    <bool name="m_ShouldTorsoIkLimitsOverrideOrbitPitchLimits" init="true" description="if true the attach ped's torso IK limits will be used as the orbit pitch limits"/>
    <bool name="m_ShouldApplyFocusLock" init="false" description="if true the measured focus distance of the adaptive DOF behaviour will be locked in the presence of the focus lock control input"/>
    <string name="m_DofSettingsForAiming" type="atHashString"/>
    <string name="m_DofSettingsForMobile" type="atHashString"/>
    <string name="m_DofSettingsForParachute" type="atHashString"/>
    <float name="m_AimingBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MobileBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_ParachuteBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_HighCoverBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_HighCoverMaxNearInFocusDistance" init="3.0f" min="0.0f" max="1000.0f"/>    
    <float name="m_HeadToSpineRatio" init="0.4f" min="0.0f" max="1.0f"/>
    <float name="m_AvoidVerticalAngle" init="50.0f" min="0.0f" max="80.0f"/>
    <float name="m_AvoidVerticalRange" init="0.75f" min="0.0f" max="3.0f"/>
    <float name="m_AvoidBlendInRate" init="6.0f" min="0.1f" max="1000.0f"/>
    <float name="m_AvoidBlendOutRate" init="4.0f" min="0.1f" max="1000.0f"/>
    <bool name="m_ShouldUseStickyAiming" init="true" description=""/>
    <u32 name="m_TimeToSwitchStickyTargets" init="200" min="0" max="10000"/>
    <float name="m_MaxStickyAimDistance" init="75.0f" min="0.0f" max="200.0f"/>
    <struct type="camFirstPersonShooterCameraMetadataStickyAim" name="m_StickyAimDefault"/>
    <struct type="camFirstPersonShooterCameraMetadataStickyAim" name="m_StickyAimScope"/>
    <bool name="m_ShouldUseLockOnAiming" init="true" description=""/>
    <bool name="m_ShouldValidateLockOnTargetPosition" init="true" description=""/>
    <float name="m_MinDistanceForLockOn" init="0.5f" min="0.1f" max="10.0f"/>
    <float name="m_MinDistanceForFineAimScaling" init="0.5f" min="0.0f" max="10.0f"/>
    <float name="m_MaxDistanceForFineAimScaling" init="1.5f" min="0.0f" max="10.0f"/>
    <float name="m_MaxAngleDeltaForLockOnSwitchBlendScaling" init="90.0f" min="0.0f" max="360.0f"/>
    <float name="m_FineAimBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FineAimBlendSpringDampingRatio" init="0.95f" min="0.0f" max="10.0f"/>
    <u32 name="m_MinBlendDurationForLockOnSwitch" init="150" min="0" max="10000"/>
    <u32 name="m_MaxBlendDurationForLockOnSwitch" init="350" min="0" max="10000"/>
    <u32 name="m_MinBlendDurationForInitialLockOn" init="150" min="0" max="10000"/>
    <u32 name="m_MaxBlendDurationForInitialLockOn" init="350" min="0" max="10000"/>
    <float name="m_AttachPedPelvisOffsetSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachPedPelvisOffsetSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_ScopeOffsetBlendIn" init="0.40f" min="0.0f" max="1.0f"/>
    <float name="m_ScopeOffsetBlendOut" init="0.15f" min="0.0f" max="1.0f"/>

    <float name="m_FreeAimFocusDistanceIncreaseDampingRatio" init="0.666f" min="0.0f" max="10.0f"/>
    <float name="m_FreeAimFocusDistanceDecreaseDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>

    <Vector2 name="m_XyDistanceLimitsForHintBlendOut" init="0.5f, 1.0f" min="0.0f" max="99999.0f"/>

    <struct type="camFirstPersonShooterCameraMetadataCoverSettings" name="m_CoverSettings"/>
    <array name="m_SprintBreakOutSettingsList" type="atFixedArray" size="2">
      <struct type="camFirstPersonShooterCameraMetadataSprintBreakOutSettings"/>
    </array>

    <string name="m_MotionBlurSettings" type="atHashString"/>
  </structdef>

  <!-- a first-person head tracking aim camera -->
  <structdef type="camFirstPersonHeadTrackingAimCameraMetadata" base="camAimCameraMetadata">
    <bool name="m_ShouldDisplayReticule" init="true" description="if true the camera will allow the HUD reticule to be displayed"/>
    <s32 name="m_AttachBoneTag" init="31086" min="-1" max="99999"/>
  </structdef>
  
  <!-- a third-person aim camera -->
  <structdef type="camThirdPersonAimCameraMetadata" base="camThirdPersonCameraMetadata" constructable="false">
    <string name="m_LockOnEnvelopeRef" type="atHashString"/>
    <string name="m_StickyAimHelperRef" type="atHashString"/>
    <bool name="m_ShouldDisplayReticule" init="true" description="if true the camera will allow the HUD reticule to be displayed"/>
    <bool name="m_ShouldDisplayReticuleDuringInterpolation" init="false" description="if true the camera will allow the HUD reticule to be displayed when the aim camera is interpolating in, regardless of source camera"/>
    <bool name="m_ShouldAllowInterpolationSourceCameraToPersistReticule" init="false" description="if true the camera will defer to the source camera to determine if the reticule should persist during interpolation"/>
    <bool name="m_ShouldApplyWeaponFov" init="true"/>
    <bool name="m_ShouldUseLockOnAiming" init="true"/>
    <bool name="m_ShouldLockOnToTargetEntityPosition" init="false"/>
    <bool name="m_ShouldValidateLockOnTargetPosition" init="true"/>
    <float name="m_TripleHeadNearClip" init="0.0f" min="0.0f" max="0.5f"/>
    <float name="m_RecoilShakeAmplitudeScaling" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinDistanceForLockOn" init="0.5f" min="0.1f" max="10.0f"/>
    <float name="m_MinDistanceForFineAimScaling" init="0.5f" min="0.0f" max="10.0f"/>
    <float name="m_MaxDistanceForFineAimScaling" init="1.5f" min="0.0f" max="10.0f"/>
    <float name="m_MaxAngleDeltaForLockOnSwitchBlendScaling" init="90.0f" min="0.0f" max="360.0f"/>
    <u32 name="m_MinBlendDurationForInitialLockOn" init="150" min="0" max="10000"/>
    <u32 name="m_MaxBlendDurationForInitialLockOn" init="350" min="0" max="10000"/>
    <u32 name="m_MinBlendDurationForLockOnSwitch" init="150" min="0" max="10000"/>
    <u32 name="m_MaxBlendDurationForLockOnSwitch" init="350" min="0" max="10000"/>
    <float name="m_FineAimBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FineAimBlendSpringDampingRatio" init="0.95f" min="0.0f" max="10.0f"/>
    <float name="m_WeaponZoomFactorSpringConstant" init="300.0f" min="0.0f" max="1000.0f"/>
    <float name="m_WeaponZoomFactorSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <bool name="m_ShouldFocusOnLockOnTarget" init="true"/>
    <float name="m_BaseFovToEmulateWithFocalLengthMultiplier" init="45.0f" min="-1.0f" max="130.0f"/>
    <float name="m_FocusParentToTargetBlendLevel" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_SecondaryFocusParentToTargetBlendLevel" init="-1.0f" min="-1.0f" max="1.0f"/>
    <float name="m_MinFocusToSecondaryFocusDistance" init="0.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <!-- a third-person aim camera that is attached to a ped -->
  <structdef type="camThirdPersonPedAimCameraMetadataLockOnTargetDampingSettings">
    <bool name="m_ShouldApplyDamping" init="true" description="if true the camera will apply damping to the lock-on target position"/>
    <float name="m_StunnedHeadingSpringConstant" init="75.0f" min="0.0f" max="1000.0f"/>
    <float name="m_StunnedHeadingSpringDampingRatio" init="1.1f" min="0.0f" max="10.0f"/>
    <float name="m_StunnedPitchSpringConstant" init="75.0f" min="0.0f" max="1000.0f"/>
    <float name="m_StunnedPitchSpringDampingRatio" init="1.1f" min="0.0f" max="10.0f"/>
    <float name="m_StunnedDistanceSpringConstant" init="75.0f" min="0.0f" max="1000.0f"/>
    <float name="m_StunnedDistanceSpringDampingRatio" init="1.1f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAimCameraMetadataLockOnOrbitDistanceSettings">
    <bool name="m_ShouldApplyScaling" init="false" description="if true the camera will scale the orbit distance when locked-on to a target that is very close by"/>
    <Vector2 name="m_LockOnDistanceLimits" init="1.0f, 3.25f" min="0.0f" max="99999.0f"/>
    <Vector2 name="m_OrbitDistanceScalingLimits" init="2.0f, 1.0f" min="0.1f" max="10.0f"/>
    <float name="m_OrbitDistanceScalingSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_OrbitDistanceScalingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camThirdPersonPedAimCameraMetadata" base="camThirdPersonAimCameraMetadata">
    <string name="m_LockOnTargetStunnedEnvelopeRef" type="atHashString"/>
    <struct type="camThirdPersonPedAimCameraMetadataLockOnTargetDampingSettings" name="m_LockOnTargetDampingSettings"/>
    <struct type="camThirdPersonPedAimCameraMetadataLockOnOrbitDistanceSettings" name="m_LockOnOrbitDistanceSettings"/>
    <Vector3 name="m_ParentRelativeAttachOffset" init="0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_ParentRelativeAttachOffsetAtOrbitHeadingLimits" init="0.0f" min="-100.0f" max="100.0f"/>
    <s32 name="m_AttachBoneTag" init="-1" min="-1" max="99999"/>
    <bool name="m_ShouldScriptedAimTaskOverrideOrbitPitchLimits" init="true"/>
    <bool name="m_ShouldAimSweepOverrideOrbitPitchLimits" init="true"/>
    <string name="m_DofSettingsForMobilePhoneShallowDofMode" type="atHashString"/>
    <float name="m_ShakeAmplitudeScaleFactor" init="1.0f" min="0.0f" max="1.0f" />
    <bool name="m_ShouldPreventBlendToItself" init="false"/>
    <bool name="m_ShouldUseVehicleMatrixForOrientationLimiting" init="false"/>
  </structdef>

  <!-- a third-person aim camera that is attached to a ped that is assisted aiming -->
  <structdef type="camThirdPersonPedAssistedAimCameraShakeActivityScalingSettings">
    <float name="m_AmplitudeScale" init="2.0f" min="0.0f" max="10.0f"/>
    <float name="m_BlendInSpringConstant" init="50.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlendOutSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraRunningShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_MinAmplitude" init="0.5f" min="0.0f" max="10.0f"/>
    <float name="m_MaxAmplitude" init="1.0f" min="0.0f" max="10.0f"/>
    <bool name="m_ShouldScaleWithWalk" init="false"/>
    <bool name="m_ShouldScaleWithRun" init="true"/>
    <bool name="m_ShouldScaleWithSprint" init="false"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraPivotScalingSettings">
    <float name="m_SpringConstant" init="6.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringConstantWhenNotMoving" init="1.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxZoomFactor" init="2.25f" min="0.5f" max="10.0f"/>
    <float name="m_SpringConstantForMaxZoomFactor" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringConstantWhenNotMovingForMaxZoomFactor" init="5.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_SpringConstantSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringConstantSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_ErrorThreshold" init="0.010f" min="0.0f" max="100.0f"/>
    <float name="m_SideOffset" init="0.0f" min="0.0f" max="99999.0f"/>
    <float name="m_DesiredErrorSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_DesiredErrorSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraShootingFocusSettings">
    <bool name="m_ShouldUseExponentialCurve" init="false"/>
    <float name="m_ExponentialCurveRate" init="10.0f" min="0.1f" max="1000.0f"/>
    <float name="m_BlendInSpringConstant" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlendOutSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinHealthRatioToConsiderForNewTargets" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_ScreenRatioForMinFootRoom" init="-0.15f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoom" init="-0.15f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMinFootRoomInTightSpace" init="-0.225f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoomInTightSpace" init="-0.225f" min="-2.0f" max="0.49f"/>
    <float name="m_MaxZoomFactor" init="1.5f" min="0.5f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraLockOnAlignmentSettings">
    <string name="m_DofSettings" type="atHashString"/>
    <float name="m_ScreenWidthRatioForSafeZone" init="0.333f" min="0.0f" max="1.0f"/>
    <float name="m_ScreenHeightRatioForSafeZone" init="0.333f" min="0.0f" max="1.0f"/>
    <float name="m_ScreenHeightRatioBiasForSafeZone" init="-0.1665f" min="-1.0f" max="1.0f"/>
    <float name="m_ScreenWidthRatioBiasForSafeZone" init="0.1665f" min="0.0f" max="1.0f"/>
    <float name="m_PivotPositionRightSideOffset" init="0.2f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PivotPositionLeftSideOffset" init="-0.25f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PivotPositionVerticalOffset" init="0.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_MaxLookAroundBlendLevel" init="0.2f" min="0.0f" max="1.0f"/>
    <float name="m_LookAroundBlockedInputSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_LookAroundBlockedInputSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraPlayerFramingSettings">
    <u32 name="m_AttackDelay" init="500" min="0" max="100000"/>
    <u32 name="m_AttackDuration" init="250" min="0" max="100000"/>
    <u32 name="m_ReleaseDelay" init="1000" min="0" max="100000"/>
    <float name="m_DesiredRelativeHeading" init="45.0f" min="0.0f" max="180.0f"/>
    <float name="m_DesiredPitch" init="15.0f" min="-90.0f" max="90.0f"/>
    <float name="m_PivotPositionRightSideOffset" init="0.25f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PivotPositionLeftSideOffset" init="-0.2f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PivotPositionVerticalOffset" init="0.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_CollisionFallBackAttachParentHeightRatioToAttain" init="1.075f" min="-10.0f" max="10.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraCinematicMomentSettings">
    <float name="m_LikelihoodToTriggerCinematicCut" init="0.0f" min="0.0f" max="1.0f"/>
    <u32 name="m_DelayTimeBeforeStartingCinematicCut" init="100" min="0" max="100000"/>
    <u32 name="m_DelayTimeBeforeEndingCinematicCut" init="1000" min="0" max="100000"/>
    <float name="m_MinLikelihoodToTrigger" init="0.25f" min="0.0f" max="1.0f"/>
    <float name="m_MaxLikelihoodToTrigger" init="1.0f" min="0.0f" max="1.0f"/>
    <u32 name="m_MinTimeBetweenTriggers" init="8500" min="0" max="100000"/>
    <u32 name="m_MaxTimeBetweenTriggers" init="11500" min="0" max="100000"/>
    <float name="m_MaxTargetHealthRatio" init="0.0f" min="0.0f" max="1.0f"/>
    <u32 name="m_MinNumBulletsRemaining" init="3" min="0" max="100"/>
    <string name="m_BlendEnvelopeRef" type="atHashString"/>
    <float name="m_ZoomFactor" init="1.5f" min="0.5f" max="10.0f"/>
    <float name="m_MaxSlowTimeScaling" init="0.5f" min="0.0f" max="1.0f"/>
    <struct type="camThirdPersonPedAssistedAimCameraLockOnAlignmentSettings" name="m_LockOnAlignmentSettings"/>
    <float name="m_TargetFocusDeadPedLockOnDurationScale" init="5.0f" min="0.0f" max="100.0f"/>
    <float name="m_PlayerFocusDeadPedLockOnDurationScale" init="4.0f" min="0.0f" max="100.0f"/>
    <struct type="camThirdPersonPedAssistedAimCameraPlayerFramingSettings" name="m_PlayerFramingSettings"/>
    <float name="m_ScreenRatioForMinFootRoom" init="-0.2f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoom" init="-0.2f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMinFootRoomInTightSpace" init="-0.2f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoomInTightSpace" init="-0.2f" min="-2.0f" max="0.49f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraRecoilShakeScalingSettings">
    <bool name="m_ShouldScaleRecoilShake" init="true"/>
    <float name="m_MinRecoilShakeAmplitudeScaling" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxRecoilShakeAmplitudeScaling" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinDistanceToAttachParent" init="2.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxDistanceToAttachParent" init="10.0f" min="0.0f" max="99999.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraInCoverSettings">
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_ScreenRatioForMinFootRoom" init="-0.35f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoom" init="-0.35f" min="-2.0f" max="0.49f"/>
    <bool name="m_ShouldAllowPlayerReactionShotsIfCanShootAroundCorner" init="true"/>
    <float name="m_ShootingFocusScreenRatioForMinFootRoom" init="-0.3f" min="-2.0f" max="0.49f"/>
    <float name="m_ShootingFocusScreenRatioForMaxFootRoom" init="-0.3f" min="-2.0f" max="0.49f"/>
    <float name="m_ShootingFocusScreenRatioForMinFootRoomInTightSpace" init="-0.3f" min="-2.0f" max="0.49f"/>
    <float name="m_ShootingFocusScreenRatioForMaxFootRoomInTightSpace" init="-0.3f" min="-2.0f" max="0.49f"/>
    <float name="m_CinematicMomentScreenRatioForMinFootRoom" init="0.15f" min="-2.0f" max="0.49f"/>
    <float name="m_CinematicMomentScreenRatioForMaxFootRoom" init="0.15f" min="-2.0f" max="0.49f"/>
    <float name="m_CinematicMomentScreenRatioForMinFootRoomInTightSpace" init="0.15f" min="-2.0f" max="0.49f"/>
    <float name="m_CinematicMomentScreenRatioForMaxFootRoomInTightSpace" init="0.15f" min="-2.0f" max="0.49f"/>
    <float name="m_LockOnAlignmentPivotPositionRightSideOffset" init="0.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LockOnAlignmentPivotPositionLeftSideOffset" init="-0.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LockOnAlignmentPivotPositionVerticalOffset" init="0.4f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PlayerFramingPivotPositionRightSideOffset" init="0.475f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PlayerFramingPivotPositionLeftSideOffset" init="-0.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_PlayerFramingPivotPositionVerticalOffset" init="0.375f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LowCoverLockOnAlignmentPivotPositionRightSideOffset" init="0.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LowCoverLockOnAlignmentPivotPositionLeftSideOffset" init="-0.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LowCoverLockOnAlignmentPivotPositionVerticalOffset" init="0.325f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LowCoverPlayerFramingPivotPositionRightSideOffset" init="0.325f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LowCoverPlayerFramingPivotPositionLeftSideOffset" init="-0.325f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LowCoverPlayerFramingPivotPositionVerticalOffset" init="0.7f" min="-99999.0f" max="99999.0f"/>
    <float name="m_LowCoverPlayerFramingDesiredPitch" init="7.5f" min="-90.0f" max="90.0f"/>
  </structdef>
  <structdef type="camThirdPersonPedAssistedAimCameraMetadata" base="camThirdPersonPedAimCameraMetadata">
    <bool name="m_ShouldApplyCinematicShootingBehaviour" init="false"/>
    <string name="m_IdleShakeRef" type="atHashString"/>
    <float name="m_LosCapsuleTestRadius" init="0.1f" min="0.01f" max="10.0f"/>
    <float name="m_FovScalingSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FovScalingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_CollisionRootPositionFallBackToPivotBlendValueForAssistedAiming" init="0.0f" min="0.0f" max="2.0f"/>
    <struct type="camThirdPersonPedAssistedAimCameraShakeActivityScalingSettings" name="m_RunningShakeActivityScalingSettings"/>
    <struct type="camThirdPersonPedAssistedAimCameraRunningShakeSettings" name="m_RunningShakeSettings"/>
    <struct type="camThirdPersonPedAssistedAimCameraRunningShakeSettings" name="m_WalkingShakeSettings"/>
    <struct type="camThirdPersonPedAssistedAimCameraPivotScalingSettings" name="m_PivotScalingSettings"/>
    <struct type="camThirdPersonPedAssistedAimCameraShootingFocusSettings" name="m_ShootingFocusSettings"/>
    <struct type="camThirdPersonPedAssistedAimCameraCinematicMomentSettings" name="m_CinematicMomentSettings"/>
    <struct type="camThirdPersonPedAssistedAimCameraRecoilShakeScalingSettings" name="m_RecoilShakeScalingSettings"/>
    <struct type="camThirdPersonPedAssistedAimCameraInCoverSettings" name="m_InCoverSettings"/>
  </structdef>

  <!-- a third-person aim camera that is attached to a ped that is in cover -->
  <structdef type="camThirdPersonPedAimInCoverCameraMetadataLowCoverSettings">
    <float name="m_BlendInSpringConstant" init="40.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlendInSpringDampingRatio" init="0.85f" min="0.0f" max="10.0f"/>
    <float name="m_BlendOutSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlendOutSpringDampingRatio" init="0.85f" min="0.0f" max="10.0f"/>
    <float name="m_AttachParentHeightRatioToAttainForBasePivotPosition" init="0.52f" min="-10.0f" max="10.0f"/>
    <float name="m_AttachParentHeightRatioToAttainForCollisionFallBackPosition" init="0.75f" min="-10.0f" max="10.0f"/>
    <float name="m_ScreenRatioForMinFootRoom" init="0.15f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoom" init="0.15f" min="-2.0f" max="0.49f"/>
  </structdef>
  <structdef type="camThirdPersonPedAimInCoverCameraMetadataAimingSettings">
    <bool name="m_ShouldApply" init="true"/>
    <string name="m_DofSettings" type="atHashString"/>
    <float name="m_AimingBlendInSpringConstant" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AimingBlendInSpringDampingRatio" init="0.85f" min="0.0f" max="10.0f"/>
    <float name="m_AimingBlendOutSpringConstant" init="50.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AimingBlendOutSpringDampingRatio" init="0.95f" min="0.0f" max="10.0f"/>
    <float name="m_ScreenRatioForMinFootRoom" init="-1.125f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoom" init="-1.125f" min="-2.0f" max="0.49f"/>
  </structdef>

  <structdef type="camThirdPersonPedAimInCoverCameraMetadata" base="camThirdPersonPedAimCameraMetadata">
    <float name="m_FacingDirectionScalingSpringConstant" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FacingDirectionScalingSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_CoverToCoverFacingDirectionScalingSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CoverToCoverFacingDirectionScalingSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_ExtraBasePivotOffsetInFacingDirection" init="0.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_ExtraCollisionFallBackOffsetInFacingDirection" init="0.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_ExtraCollisionFallBackOffsetOrbitFrontDotCoverLeft" init="0.0f" min="-99999.0f" max="99999.0f"/>

    <bool name="m_ShouldApplyStepOutPosition" init="true"/>
    <float name="m_StepOutPositionBlendInSpringConstant" init="80.0f" min="0.0f" max="1000.0f"/>
    <float name="m_StepOutPositionBlendInSpringConstantForAimingHold" init="500.0f" min="0.0f" max="1000.0f"/>
    <float name="m_StepOutPositionBlendOutSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_StepOutPositionBlendSpringConstantForAiming" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_StepOutPositionBlendSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_CoverRelativeStepOutPositionSpringConstant" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CoverRelativeStepOutPositionSpringConstantForTransitionToAiming" init="200.0f" min="0.0f" max="1000.0f"/>  
    <float name="m_CoverRelativeStepOutPositionSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_BlindFiringOnHighCornerBlendInSpringConstant" init="80.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlindFiringOnHighCornerBlendOutSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlindFiringOnHighCornerBlendSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_AdditionalCameraRelativeSideOffsetForBlindFiring" init="0.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_MaxSpringSpeedToDisplayReticule" init="0.333f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxStepOutPositionBlendErrorToDisplayReticule" init="0.01f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_RelativeOrbitHeadingLimitsForHighCornerCover" init="-100.0f, 178.0f" min="-180.0f" max="180.0f"/>

    <bool name="m_ShouldAlignToCorners" init="true"/>
    <float name="m_MinMoveSpeedForDefaultAlignment" init="0.5f" min="0.0f" max="99999.0f"/>
    <float name="m_DefaultAlignmentHeadingDeltaToLookAhead" init="25.0f" min="0.0f" max="180.0f"/>
    <float name="m_DefaultAlignmentSpringConstant" init="5.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CornerAlignmentSpringConstant" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MovingAroundCornerAlignmentSpringConstant" init="40.0f" min="0.0f" max="1000.0f"/>
    <float name="m_DefaultAlignmentSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>

    <u32 name="m_MinTimeToLockFocusDistanceWhenTurningOnNarrowCover" init="750" min="0" max="100000"/>

    <struct type="camThirdPersonPedAimInCoverCameraMetadataLowCoverSettings" name="m_LowCoverSettings"/>
    <struct type="camThirdPersonPedAimInCoverCameraMetadataAimingSettings" name="m_AimingSettings"/>
  </structdef>

  <!-- a third-person aim camera that is attached to a ped that is engaged in melee combat -->
  <structdef type="camThirdPersonPedMeleeAimCameraMetadata" base="camThirdPersonPedAimCameraMetadata">
    <float name="m_LockOnDistanceSpringConstant" init="15.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_LockOnDistanceLimitsForOrbitDistance" init="1.0f, 3.25f" min="0.0f" max="99999.0f" />
    <float name="m_BaseOrbitPivotRotationBlendLevel" init="0.333f" min="0.0f" max="1.0f"/>
    <float name="m_BaseOrbitPitchScaling" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_OrbitHeadingSpringConstant" init="35.0f" min="0.0f" max="1000.0f"/>
    <float name="m_OrbitHeadingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_OrbitPitchSpringConstant" init="15.0f" min="0.0f" max="1000.0f"/>
    <float name="m_OrbitPitchSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_LockOnDistanceScalingForLookAt" init="1.0f" min="0.0f" max="1.0f"/>
  </structdef>

  <!-- a third-person aim camera that is attached to a vehicle -->
  <structdef type="camThirdPersonVehicleAimCameraMetadata" base="camThirdPersonAimCameraMetadata">
    <bool name="m_ShouldUseLockOnAimingForDriver" init="true"/>
    <bool name="m_ShouldUseLockOnAimingForPassenger" init="true"/>
    <float name="m_ExtraSideOffsetForHangingOnLeftSide" init="0.0f" min="-99999.0f" max="99999.0f"/>
    <float name="m_ExtraSideOffsetForHangingOnRightSide" init="0.0f" min="-99999.0f" max="99999.0f"/>
  </structdef>

  <structdef type="camBaseCinematicTrackingCameraMetadata" base="camBaseCameraMetadata">
  </structdef>

  <structdef type="camCinematicStuntCameraMetadata" base="camBaseCinematicTrackingCameraMetadata">
    <Vector2 name="m_FovLimits" init="14.0f, 50.0f" min="5.0f" max="60.0f" />
    <float name="m_MinFovToZoom" init="10.0f" min="0.0f" max="45.0f"/>
    <Vector2 name="m_ZoomInTimeLimits" init="1000.0f, 2000.0f" min="0.0f" max="10000.0f"/>
    <Vector2 name="m_ZoomHoldTimeLimits" init="3000.0f, 4000.0f" min="0.0f" max="10000.0f"/>
    <Vector2 name="m_ZoomOutTimeLimits" init="3000.0f, 4000.0f" min="0.0f" max="10000.0f"/>
  </structdef>

  <structdef type="camCinematicGroupCameraMetadata" base="camBaseCinematicTrackingCameraMetadata">
    <Vector2 name="m_FovLimits" init="30.0f, 65.0f" min="5.0f" max="100.0f" />
    <float name="m_MinFovToZoom" init="10.0f" min="0.0f" max="45.0f"/>
    <Vector2 name="m_ZoomTimeLimits" init="1000.0f, 4000.0f" min="0.0f" max="10000.0f"/>
    <float name="m_RadiusScalingForClippingTest" init="0.8f" min="0.0f" max="2.0f"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <u32 name="m_MaxTimeToSpendOccluded" init="1000" min="0" max="10000"/>
    <float name="m_OrbitDistanceScalar" init="20.0f" min="1.0f" max="100.0f"/>
    <float name="m_GroupCentreSpringConstant" init="3.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AverageFrontSpringConstant" init="3.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FovSpringConstant" init="3.0f" min="0.0f" max="1000.0f"/>
    <float name="m_OrbitDistanceScalarSpringConstant" init="3.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
  </structdef>


  <structdef type="camCinematicVehicleOrbitCameraInitalSettings">
    <float name="m_Heading" init="0.0f" min="0.0f" max="360.0f"/>
    <float name="m_HeadingDelta" init="0.0f" min="0.0f" max="360.0f"/>
  </structdef>
  
  
  <structdef type="camCinematicVehicleOrbitCameraMetadata" base="camBaseCameraMetadata">
    <Vector2 name="m_ShotDurationLimits" init="3000.0f, 4500.0f" min="1.0f" max="100000.0f"/>
    <Vector2 name="m_PitchLimits" init="10.0f, 14.0f" min="-89.0f" max="89.0f"/>
    <Vector2 name="m_FovLimits" init="45.0f, 45.0f" min="1.0f" max="55.0f"/>
    <float name="m_IntialAngle" init="0.0f" min="0.0f" max="360.0f"/>
    <float name="m_AngleDelta" init="45.0f" min="0.0f" max="360.0f"/>
    <float name="m_screenRatioForFootRoom" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_RadiusScalingForConstrain" init="1.0f" min="0.0f" max="2.0f"/>
    <float name="m_RadiusScalingForOcclusionTest" init="0.9f" min="0.0f" max="2.0f"/>
    <float name="m_RadiusScalingForClippingTest" init="0.8f" min="0.0f" max="2.0f"/>
    <float name="m_PercentageOfOrbitDistanceToTerminate" init="0.35f" min="0.0f" max="1.0f"/>
    <float name="m_MaxLerpDeltaBeforeTerminate" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_MaxAttachParentPitch" init="25.0f" min="0.0f" max="180.0f"/>
    <u32 name="m_MaxTimeToSpendOccluded" init="1000" min="0" max="10000"/>
    <u32 name="m_MaxTimeAttachParentPitched" init="1000" min="0" max="10000"/>
    <array name="m_InitialCameraSettingsList" type="atArray">
      <struct type="camCinematicVehicleOrbitCameraInitalSettings"/>
    </array>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
  </structdef>

  <structdef type="camSpeedRelativeShakeSettingsMetadata">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_MinForwardSpeed" init="25.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxForwardSpeed" init="45.0f" min="0.0f" max="100.0f"/>
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxShakeAmplitude" init="1.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <structdef type="camCinematicVehicleLowOrbitCameraMetadata" base="camBaseCameraMetadata">
    <Vector2 name="m_ShotDurationLimits" init="3000.0f, 4500.0f" min="1.0f" max="100000.0f"/>
    <Vector2 name="m_PitchLimits" init="10.0f, 14.0f" min="-89.0f" max="89.0f"/>
    <Vector2 name="m_FovLimits" init="45.0f, 45.0f" min="1.0f" max="55.0f"/>
    <float name="m_IntialAngle" init="0.0f" min="0.0f" max="360.0f"/>
    <float name="m_AngleDelta" init="45.0f" min="0.0f" max="360.0f"/>
    <float name="m_OrbitHeadingSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_OrbitHeadingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_ProjectedVehicleWidthToScreenRatio" init="0.54f" min="0.1f" max="1.0f"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_RadiusScalingForConstrain" init="1.0f" min="0.0f" max="2.0f"/>
    <float name="m_RadiusScalingForOcclusionTest" init="0.9f" min="0.0f" max="2.0f"/>
    <float name="m_RadiusScalingForClippingTest" init="0.8f" min="0.0f" max="2.0f"/>
    <float name="m_DistanceToTestDownForTrailersOrTowedVehiclesForClippingTest" init="4.0f" min="0.0f" max="100.0f"/>
    <float name="m_PercentageOfOrbitDistanceToTerminate" init="0.35f" min="0.0f" max="1.0f"/>
    <float name="m_MaxLerpDeltaBeforeTerminate" init="0.5f" min="0.0f" max="1.0f"/>
    <u32 name="m_MaxTimeToSpendOccluded" init="1000" min="0" max="10000"/>
    <array name="m_InitialCameraSettingsList" type="atArray">
      <struct type="camCinematicVehicleOrbitCameraInitalSettings"/>
    </array>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
    <float name="m_MaxPitchDelta" init="45.0f" min="-89.0f" max="89.0f"/>
    <float name="m_MaxPitch" init="45.0f" min="-89.0f" max="89.0f"/>
    <float name="m_MaxRoll" init="120.0f" min="-180.0f" max="180.0f"/>
    <float name="m_MaxProbeHeightForGround" init="10.0f" min="-1000.0f" max="1000.0f"/>
    <float name="m_MinProbeHeightForGround" init="-11.0f" min="-1000.0f" max="1000.0f"/>
    <float name="m_RadiusScalingForGroundTest" init="4.0f" min="0.0f" max="10.0f"/>
    <float name="m_HighGroundCollisionSpringConst" init="350.0f" min="0.0f" max="1000.0f"/>
    <float name="m_LowGroundCollisionSpringConst" init="50.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CollisionSpringConst" init="1.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CamHeightAboveGroundAsBoundBoxPercentage" init="0.5f" min="0.0f" max="100.0f"/>
    <float name="m_MinHeightAboveGroundAsBoundBoundPercentage" init="0.3f" min="0.0f" max="100.0f"/>
    <float name="m_FovRatioToApplyForLookAt" init="0.8f" min="-100.0f" max="100.0f"/>
    <float name="m_AngleRangeToApplySmoothingForAttachedTrailer" init="30.0f" min="-100.0f" max="100.0f"/>
    <struct type="camSpeedRelativeShakeSettingsMetadata" name="m_InaccuracyShakeSettings"/>
    <struct type="camSpeedRelativeShakeSettingsMetadata" name="m_HighSpeedShakeSettings"/>
  </structdef>
  
  
  <structdef type="camCinematicPositionCameraMetadata" base="camBaseCameraMetadata">
    <Vector2 name="m_PitchLimits" init="0.0f, 14.0f" min="-89.0f" max="89.0f"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_RadiusScalingForConstrain" init="1.0f" min="0.0f" max="2.0f"/>
    <float name="m_RadiusScalingForClippingTest" init="0.8f" min="0.0f" max="2.0f"/>
    <u32 name="m_MaxTimeToSpendOccluded" init="1000" min="0" max="10000"/>
    <float name="m_MinOrbitRadius" init="3.0f" min="0.0f" max="100.0f"/>
    <float name="m_OrbitDistanceScalar" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxZoomFov" init="30.0f" min="1.0f" max="130.0f"/>
    <u32 name="m_ZoomDuration" init="2000" min="0" max="10000"/>
    <float name="m_MaxDistanceToTerminate" init="60.0f" min="1.0f" max="1000.0f"/>
    <u32 name="m_NumofModes" init="0" min="0" max="100"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
    <string name="m_InVehicleLookAtDampingRef" type="atHashString"/>
    <string name="m_OnFootLookAtDampingRef" type="atHashString"/>
    <string name="m_InVehicleLookAheadRef" type="atHashString"/>
  </structdef>

  <structdef type="camCinematicVehicleTrackingCameraMetadata" base="camBaseCameraMetadata">
    <Vector3 name="m_LookAtOffset" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_PositionOffset" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <float name="m_Fov" init="50.0f" min="1.00f" max="90.0f"/>
    <Vector2 name="m_RoofLowerPositionBlends" init="0.0f, 1.0f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_RoofLowerLookAtPositionBlends" init="0.0f, 1.0f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_RoofRaisePositionBlends" init="0.0f, 1.0f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_RoofRaiseLookAtPositionBlends" init="0.0f, 1.0f" min="0.0f" max="1.0f"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_RadiusScalingForClippingTest" init="0.8f" min="0.0f" max="2.0f"/>
    <float name="m_RadiusScalingForCapsule" init="1.0f" min="0.0f" max="2.0f"/>
    <u32 name="m_HoldAtEndTime" init="250" min="0" max="10000"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
 </structdef>

  <structdef type="camCinematicWaterCrashCameraMetadata" base="camBaseCameraMetadata">
    <Vector2 name="m_PitchLimits" init="30.0f, 20.0f" min="-89.0f" max="89.0f"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_DistanceToTarget" init="3.0f" min="0.0f" max="100.0f"/>
    <float name="m_RadiusScale" init="1.5f" min="0.10f" max="10.0f"/>
    <float name="m_DropDistance" init="1.0f" min="-25.0f" max="25.0f"/>
    <float name="m_RiseDistance" init="0.5f" min="-25.0f" max="25.0f"/>
    <float name="m_ZoomFov" init="30.0f" min="1.0f" max="130.0f"/>
    <float name="m_CatchUpDistance" init="5.0f" min="0.0f" max="20.0f"/>
    <u32 name="m_MaxTimeToSpendOccluded" init="1000" min="0" max="10000"/>
    <u32 name="m_DropDuration" init="1000" min="0" max="20000"/>
    <u32 name="m_RiseDuration" init="2000" min="0" max="20000"/>
    <u32 name="m_BlendDuration" init="2000" min="0" max="30000"/>
  </structdef>

  <!-- a camera that is controlled by an animated cutscene -->
  <structdef type="camAnimatedCameraMetadata" base="camBaseCameraMetadata">
  </structdef>
  
  <!-- a cinematic camera that tracks a target from the perspective of camera man at a fixed position -->
  <structdef type="camCinematicAnimatedCameraMetadata" base="camAnimatedCameraMetadata">
  </structdef>

  <structdef type="camCinematicCameraOperatorShakeUncertaintySettings">
    <string name="m_XShakeRef" type="atHashString"/>
    <string name="m_ZShakeRef" type="atHashString"/>
    <Vector2 name="m_AbsHeadingSpeedRange" init="0.1f, 3.0f" min="0.0f" max="999.0f"/>
    <Vector2 name="m_AbsPitchSpeedRange" init="0.5f, 1.5f" min="0.0f" max="999.0f"/>
    <Vector2 name="m_AbsRollSpeedRange" init="0.0f, 0.5f" min="0.0f" max="999.0f"/>
    <Vector2 name="m_AbsFovSpeedRange" init="30.0f, 70.0f" min="0.0f" max="999.0f"/>
    <float name="m_IncreasingSpringConstant" init="50.0f" min="0.0f" max="1000.0f"/>
    <float name="m_DecreasingSpringConstant" init="15.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_ScalingFactor" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_HeadingAndPitchThreshold" init="0.1f" min="0.0f" max="10.0f"/>
    <float name="m_RatioSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_RatioSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camCinematicCameraOperatorShakeTurbulenceSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <Vector2 name="m_AbsVehicleSpeedRange" init="10.0f, 15.0f" min="0.0f" max="999.0f"/>
    <Vector2 name="m_DistanceRange" init="0.0f, 12.5f" min="0.0f" max="999.0f"/>
    <float name="m_IncreasingSpringConstant" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_DecreasingSpringConstant" init="5.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camCinematicCameraOperatorShakeSettings">
    <struct type="camCinematicCameraOperatorShakeUncertaintySettings" name="m_UncertaintySettings"/>
    <struct type="camCinematicCameraOperatorShakeTurbulenceSettings" name="m_TurbulenceSettings"/>
  </structdef>
  
  <!-- a cinematic camera that tracks a target from the perspective of camera man at a fixed position -->
  <structdef type="camCinematicCameraManCameraMetadata" base="camBaseCameraMetadata">
    <float name="m_MinDistanceToTarget" init="3.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxDistanceToTarget" init="60.0f" min="0.0f" max="99999.0f"/>
    <bool name="m_ShouldConsiderZDistanceToTarget" init="true"/>

    <float name="m_ElevationHeightOffset" init="1.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_CollisionRadius" init="0.1f" min="0.0f" max="99999.0f"/>
    <float name="m_RadiusScalingForClippingTest" init="1.0f" min="0.0f" max="5.0f"/>
    <float name="m_CollisionRadiusElevated" init="0.3f" min="0.0f" max="99999.0f"/>
    
    <float name="m_HeightOffsetAboveWater" init="0.5f" min="0.0f" max="99999.0f"/>
    <u32 name="m_MaxTimeOutOfStaticLos" init="500" min="0" max="10000"/>
    <u32 name="m_MaxTimeOutOfDynamicLos" init="1000" min="0" max="10000"/>
    <float name="m_MaxZoomFov" init="20.0f" min="1.0f" max="130.0f"/>
    <float name="m_MinZoomFov" init="55.0f" min="1.0f" max="130.0f"/>
    <float name="m_HighSpeedMaxZoomFov" init="30.0f" min="1.0f" max="130.0f"/>
    <u32 name="m_MinInitialZoomDuration" init="1000" min="0" max="60000"/>
    <u32 name="m_MaxInitialZoomDuration" init="3000" min="0" max="60000"/>
    <bool name="m_UseGroundProbe" init="false"/>
    <float name="m_ScanRadius" init="7.0f" min="0.0f" max="1000.0f"/>
    <float name="m_ScanDistance" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MinAngleBetweenShots" init="40.0f" min="0.0f" max="1000.0f"/>
    <string name="m_InVehicleLookAtDampingRef" type="atHashString"/>
    <string name="m_OnFootLookAtDampingRef" type="atHashString"/>
    <string name="m_InVehicleLookAheadRef" type="atHashString"/>
    <string name="m_InconsistentBehaviourZoomHelperRef" type="atHashString"/>
    <struct type="camCinematicCameraOperatorShakeSettings" name="m_CameraOperatorShakeSettings"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
    <float name="m_FovSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <bool name="m_ShouldScaleFovForLookAtTargetBounds" init="false"/>
    <float name="m_BoundsRadiusScaleForMinFov" init="3.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BoundsRadiusScaleForMaxFov" init="1.0f" min="0.0f" max="1000.0f"/>
    <bool name="m_ShouldExtendRelativePositionForVelocity" init="false"/>
    <float name="m_ScanRadiusScalar" init="2.0f" min="0.0f" max="1000.0f"/>
    <float name="m_ScanDistanceScalar" init="2.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MinHeadingDeltaToExtendCameraPosition" init="40.0f" min="0.0f" max="1000.0f"/>
    <bool name="m_UseRadiusCheckToComputeNavMeshPos" init="true"/>
    <float name="m_MinSpeedForHighFovScalar" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxSpeedForHighFovScalar" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxDistanceFailureVelocityScalar" init="2.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxDistanceFailureVelocityScalarForBike" init="2.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MinVelocityDistanceScalar" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxVelocityDistanceScalar" init="60.0f" min="0.0f" max="1000.0f"/>
  </structdef>

  <!-- a cinematic camera that tracks a target from the perspective of a chase helicopter -->
  <structdef type="camCinematicHeliChaseCameraMetadata" base="camBaseCameraMetadata">
    <u32 name="m_MaxDuration" init="20000" min="0" max="60000"/>
    <u32 name="m_MaxAttemptsToFindValidRoute" init="8" min="1" max="100"/>

    <u32 name="m_MaxTimeOutOfLosBeforeLock" init="1000" min="0" max="60000"/>
    <u32 name="m_MaxTargetLockDuration" init="2000" min="0" max="60000"/>

    <float name="m_MaxHorizDistanceToTarget" init="100.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxHorizDistanceToHeliTarget" init="150.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxHorizDistanceToForceCameraOut" init="5.0f" min="0.0f" max="99999.0f"/>
    <float name="m_StartDistanceBehindTarget" init="30.0f" min="0.0f" max="99999.0f"/>
    <float name="m_EndDistanceBeyondTarget" init="50.0f" min="0.0f" max="99999.0f"/>
    <float name="m_StartDistanceBehindTargetForJetpack" init="30.0f" min="0.0f" max="99999.0f"/>
    <float name="m_EndDistanceBeyondTargetForJetpack" init="50.0f" min="0.0f" max="99999.0f"/>
    <float name="m_HeightAboveTarget" init="55.0f" min="0.0f" max="99999.0f"/>
    <float name="m_HeightAboveTargetForBike" init="40.0f" min="0.0f" max="99999.0f"/>
    <float name="m_HeightAboveTargetForTrain" init="100.0f" min="0.0f" max="99999.0f"/>
    <float name="m_HeightAboveTargetForHeli" init="75.0f" min="0.0f" max="99999.0f"/>
    <float name="m_HeightAboveTargetForJetpack" init="10.0f" min="0.0f" max="9999.0f"/>
    <float name="m_PathOffsetFromSideOfVehicle" init="50.0f" min="0.0f" max="99999.0f"/>
    <float name="m_CollisionCheckRadius" init="15.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MinAngleBetweenShots" init="40.0f" min="0.0f" max="360.0f"/>
    <float name="m_MinAngleFromMaxRange" init="15.0f" min="0.0f" max="360.0f"/>
    <float name="m_MinAngleFromMinRange" init="15.0f" min="0.0f" max="360.0f"/>
    <float name="m_ClippingRadiusCheck" init="0.21f" min="0.0f" max="99999.0f"/>
    <float name="m_ClippingRadiusScalar" init="1.0f" min="0.0f" max="99999.0f"/>
    <u32 name="m_MinInitialZoomDuration" init="1000" min="0" max="60000"/>
    <u32 name="m_MaxInitialZoomDuration" init="4000" min="0" max="60000"/>
    <float name="m_MinZoomFov" init="45.0f" min="1.0f" max="130.0f"/>
    <float name="m_MaxZoomFov" init="20.0f" min="1.0f" max="130.0f"/>
    <float name="m_HighSpeedMaxZoomFov" init="30.0f" min="1.0f" max="130.0f"/>
    <float name="m_BoundsRadiusScaleForMaxFov" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_BoundsRadiusScaleForMinFov" init="3.0f" min="0.0f" max="100.0f"/>
    <float name="m_ExtraZoomFovDelta" init="7.0f" min="1.0f" max="130.0f"/>
    <float name="m_MinDistanceToTargetForExtraZoom" init="70.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxDistanceToTargetForExtraZoom" init="90.0f" min="0.0f" max="99999.0f"/>

    <float name="m_NearClip" init="0.5f" min="0.0f" max="10.0f"/>
    
    <bool name="m_ShouldApplyDesiredPathHeading" init="false"/>
    <bool name="m_ShouldConsiderZDistanceToTarget" init="false"/>
    <bool name="m_ShouldTrackVehicle" init="false"/>
    <bool name="m_UseLowOrbitMode" init="false"/>
    <float name="m_BoundBoxScaleValue" init="1.0f" min="0.0f" max="100.0f"/>
    <bool name="m_ShouldTerminateForPitch" init="false"/>
    <Vector2 name="m_PitchLimits" init="-89.0f, 89.0f" min="-89.0f" max="89.0f"/>
    <string name="m_InVehicleLookAtDampingRef" type="atHashString"/>
    <string name="m_OnFootLookAtDampingRef" type="atHashString"/>
    <string name="m_InVehicleLookAheadRef" type="atHashString"/>
    <string name="m_InconsistentBehaviourZoomHelperRef" type="atHashString"/>
    <struct type="camCinematicCameraOperatorShakeSettings" name="m_CameraOperatorShakeSettings"/>
    <float name="m_HighAccuracyTrackingScalar" init="0.8f" min="0.0f" max="1.0f"/>
    <float name="m_MediumAccuracyTrackingScalar" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_LowAccuracyTrackingScalar" init="0.3f" min="0.0f" max="1.0f"/>
    <float name="m_MinVelocityDistanceScalar" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxVelocityDistanceScalar" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxDistanceFailureVelocityScalar" init="2.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxDistanceFailureVelocityScalarForBike" init="1.3f" min="0.0f" max="1000.0f"/>
    <float name="m_InitialPositionExtensionVelocityScalar" init="2.0f" min="0.0f" max="1000.0f"/>
    <float name="m_InitialHeadingWithRespectToLookAtToExtendStartPoistion" init="130.0f" min="-180.0f" max="180.0f"/>
    <float name="m_HeightMapCheckBoundsExtension" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MinHeightAboveHeightMap" init="25.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MinSpeedForHighFovScalar" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxSpeedForHighFovScalar" init="60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FovSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
  </structdef>

  <structdef type="camCinematicIdleShots">
    <Vector2 name="m_PitchLimits" init="-25.0f, 15.0f" min="-89.0f" max="89.0f" />
    <Vector2 name="m_DistanceLimits" init="1.0f , 1.0f" min="0.0f" max="20.0f"/>
  </structdef>
  
  <!-- a camera that looks at interest events when the player is idle -->
  <structdef type="camCinematicIdleCameraMetadata" base="camBaseCameraMetadata">
    <u32 name="m_MaxShotSelectionIterations" init="4" min="0" max="8"/>
    <float name="m_MinHeadingDeltaBetweenShots" init="60.0f" min="0.0f" max="120.0f"/>
    <float name="m_MinHeadingDeltaFromActorQuadrants" init="10.0f" min="0.0f" max="45.0f"/>
    <struct type="camCinematicIdleShots" name="m_WideShot"/>
    <struct type="camCinematicIdleShots" name="m_MediumShot"/>
    <struct type="camCinematicIdleShots" name="m_CloseUpShot"/>
    <Vector2 name="m_ScreenHeightRatio" init="0.1f, 0.15f" min="-0.5f" max="0.5f"/>
    <Vector2 name="m_ShotDurationLimits" init="7.50f, 20.0f" min="1.0f" max="100.0f"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_RadiusScalingForConstrain" init="1.0f" min="0.0f" max="2.0f"/>
    <float name="m_RadiusScalingForOcclusionTest" init="0.9f" min="0.0f" max="2.0f"/>
    <Vector2 name="m_LeadingLookScreenRotation" init="0.15f , 0.3f" min="0.0f" max="0.5f"/>
    <float name="m_DistanceScalingFactorForDof" init="0.125f" min="0.0f" max="1.0f"/>
    <float name="m_MaxTimeToSpendOccluded" init="4.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <!-- a first person camera that looks at interest events when the player is idle -->
  <structdef type="camCinematicFirstPersonIdleCameraMetadata" base="camBaseCameraMetadata">
    <float name="m_ZoomFov" init="30.0f" min="5.0f" max="100.0f"/>
    <float name="m_ZoomTAFov" init="20.0f" min="-1.0f" max="90.0f"/>
    <float name="m_ZoomAngleThreshold" init="5.0" min="1.0" max="90.0"/>
    <u32 name="m_TimeToZoomIn" init="1000" min="0" max="30000"/>
    <u32 name="m_TimeToStartSearchNoTarget" init="2000" min="0" max="60000"/>
    <u32 name="m_TimeToStartSearchWithTarget" init="4000" min="0" max="60000"/>
    <u32 name="m_TimeToStartEventSearch" init="3000" min="0" max="60000"/>
    <u32 name="m_TimeToStopWatchingWhenStill" init="5000" min="0" max="60000"/>
    <u32 name="m_TimeToReachNoTargetPoint" init="10000" min="0" max="60000"/>
    <u32 name="m_TimeToSelectNewNoTargetPoint" init="10000" min="0" max="60000"/>
    <u32 name="m_MaxTimeToLoseLineOfSight" init="250" min="0" max="10000"/>
    <u32 name="m_MinTimeToWatchTarget" init="250" min="0" max="8000"/>
    <u32 name="m_TimeBetweenBirdSightings" init="30000" min="0" max="120000"/>
    <u32 name="m_TimeBetweenVehicleSightings" init="30000" min="0" max="120000"/>
    <u32 name="m_MinVehicleSwankness" init="3" min="0" max="100"/>
    <u32 name="m_MinSportsCarSwankness" init="3" min="0" max="100"/>
    <float name="m_MaxValidPedDistance" init="30.0f" min="5.0f" max="100.0f"/>
    <float name="m_MaxValidEventDistance" init="30.0f" min="5.0f" max="100.0f"/>
    <float name="m_MaxValidVehicleDistance" init="30.0f" min="5.0f" max="100.0f"/>
    <float name="m_MaxValidBirdDistance" init="30.0f" min="5.0f" max="100.0f"/>
    <float name="m_MinValidDistToTarget" init="3.0f" min="0.1f" max="25.0f"/>
    <float name="m_DistToZoomOutForVehicle" init="9.0f" min="0.0f" max="50.0f"/>
    <float name="m_InvalidPedDistance" init="50.0f" min="5.0f" max="100.0f"/>
    <float name="m_InvalidVehicleDistance" init="60.0f" min="5.0f" max="100.0f"/>
    <float name="m_PlayerHorizontalFov" init="90.0f" min="5.0f" max="180.0f"/>
    <float name="m_PlayerVerticalFov" init="45.0f" min="5.0f" max="180.0f"/>
    <float name="m_MaxAngularTurnRate" init="30.0f" min="1.0f" max="720.0f"/>
    <float name="m_MinTurnRateScalePeds" init="0.15f" min="0.0f" max="1.0f"/>
    <float name="m_MinTurnRateScaleVehicle" init="0.25f" min="0.0f" max="1.0f"/>
    <float name="m_DistanceToStartTrackHead" init="5.0f" min="3.0f" max="25.0f"/>
    <float name="m_DistanceToForceTrackHead" init="2.5f" min="2.0f" max="25.0f"/>
    <float name="m_PedOffsetInterpolation" init="0.05f" min="0.01f" max="1.0f"/>
    <float name="m_PedChestZOffset" init="0.10f" min="0.0f" max="2.0f"/>
    <float name="m_PedThighZOffset" init="-0.25f" min="-1.0f" max="1.0f"/>
    <float name="m_LookBumHalfAngle" init="75.0f" min="0.0f" max="120.0f"/>
    <float name="m_LosTestRadius" init="0.20f" min="0.05f" max="2.0f"/>
    <u32 name="m_MaxNoTargetPitch" init="15" min="0" max="60"/>
    <u32 name="m_CheckOutGirlDuration" init="2500" min="0" max="10000"/>
    <float name="m_CheckOutGirlMinDistance" init="7.5f" min="5.0f" max="20.0f"/>
    <float name="m_CheckOutGirlMaxDistance" init="12.5f" min="5.0f" max="30.0f"/>
    <float name="m_CameraZOffset" init="0.63f" min="-10.0f" max="10.0f"/>
  </structdef>

  <!-- a cinematic camera that can be mounted on a vehicle -->
  <enumdef type="eCinematicVehicleAttach">
    <enumval name="CVA_INVALID"/>
    <enumval name="CVA_WHEEL_FRONT_LEFT"/>
    <enumval name="CVA_WHEEL_FRONT_RIGHT"/>
    <enumval name="CVA_WHEEL_REAR_LEFT"/>
    <enumval name="CVA_WHEEL_REAR_RIGHT"/>
    <enumval name="CVA_WINDSCREEN"/>
    <enumval name="CVA_REARWINDOW"/>
    <enumval name="CVA_WINDOW_FRONT_LEFT"/>
    <enumval name="CVA_WINDOW_FRONT_RIGHT"/>
    <enumval name="CVA_BONNET"/>
    <enumval name="CVA_TRUNK"/>
    <enumval name="CVA_PLANE_WINGTIP_LEFT"/>
    <enumval name="CVA_PLANE_WINGTIP_RIGHT"/>
    <enumval name="CVA_PLANE_WINGLIGHT_LEFT"/>
    <enumval name="CVA_PLANE_WINGLIGHT_RIGHT"/>
    <enumval name="CVA_PLANE_RUDDER"/>
    <enumval name="CVA_PLANE_RUDDER_2"/>
    <enumval name="CVA_HELI_ROTOR_MAIN"/>
    <enumval name="CVA_HELI_ROTOR_REAR"/>
    <enumval name="CVA_HANDLEBAR_LEFT"/>
    <enumval name="CVA_HANDLEBAR_RIGHT"/>
    <enumval name="CVA_SUB_ELEVATOR_LEFT"/>
    <enumval name="CVA_SUB_ELEVATOR_RIGHT"/>
    <enumval name="CVA_SUB_RUDDER"/>
    <enumval name="CVA_SIREN_1"/>
    <enumval name="CVA_HANDLEBAR_MAIN"/>
    <enumval name="CVA_TURRET_1_BASE"/>
    <enumval name="CVA_TURRET_2_BASE"/>
    <enumval name="CVA_TURRET_3_BASE"/>
    <enumval name="CVA_TURRET_4_BASE"/>
  </enumdef>
  <enumdef type="eCinematicMountedCameraLookAtBehaviour">
    <enumval name="LOOK_FORWARD_RELATIVE_TO_ATTACH"/>
    <enumval name="LOOK_AT_ATTACH_RELATIVE_POSITION"/>
    <enumval name="LOOK_AT_FOLLOW_PED_RELATIVE_POSITION"/>
  </enumdef>
  <structdef type="camCinematicMountedCameraMetadataOrientationSpring">
    <float name="m_SpringConstant" init="4.0f" min="0.0f" max="30.0f"/>
    <float name="m_SpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
    <float name="m_MinVehicleMoveSpeedToOrientationForward" init="99999.0f" min="0.0f" max="99999.0f"/>
    <bool name="m_ShouldReversingOrientateForward" init="true"/>
    <float name="m_VehicleMoveSpeedForMinSpringConstant" init="5.0f" min="0.0f" max="99999.0f"/>
    <Vector2 name="m_MinSpeedSpringConstants" init="5.5f, 5.5f" min="0.0f" max="1000.0f"/>
    <float name="m_VehicleMoveSpeedForMaxSpringConstant" init="35.0f" min="0.0f" max="99999.0f"/>
    <Vector2 name="m_MaxSpeedSpringConstants" init="50.0f, 50.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_MaxSpeedLookAroundSpringConstants" init="5.0f, 5.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_LookBehindSpringConstants" init="50.0f, 50.0f" min="0.0f" max="1000.0f"/>
  </structdef>
  <structdef type="camCinematicMountedCameraMetadataLeadingLookSettings">
    <float name="m_AbsMinVehicleSpeed" init="99999.0f" min="0.0f" max="99999.0f"/>
    <bool name="m_ShouldApplyWhenReversing" init="false"/>
    <float name="m_MaxHeadingOffsetDegrees" init="90.0f" min="0.0f" max="180.0f"/>
    <float name="m_SpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_AbsMaxPassengerRightSpeed" init="99999.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxPassengerHeadingOffsetDegrees" init="90.0f" min="0.0f" max="180.0f"/>
    <bool name="m_ShouldReversePassengerLookDirection" init="false"/>
    <float name="m_MaxAngleToConsiderFacingForward" init="30.0f" min="0.0f" max="180.0f"/>
  </structdef>
  <structdef type="camCinematicMountedCameraMetadataRelativePitchScalingToThrottle">
    <bool name="m_ShouldScaleRelativePitchToThrottle" init="false"/>
    <float name="m_RelativePitchAtMaxThrottle" init="0.0f" min="-30.0f" max="30.0f"/>
  </structdef>
  <structdef type="camCinematicMountedCameraMetadataMovementOnAccelerationSettings">
    <bool name="m_ShouldMoveOnVehicleAcceleration" init="false"/>
    <float name="m_BlendInSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_BlendOutSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinAbsForwardAcceleration" init="0.0f" min="0.0f" max="30.0f"/>
    <float name="m_MaxAbsForwardAcceleration" init="10.0f" min="0.0f" max="30.0f"/>
    <bool name="m_ShouldApplyWhenReversing" init="true"/>
    <float name="m_MaxZoomFactor" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxDownwardOffset" init="0.0f" min="0.0f" max="10.0f"/>
    <float name="m_MaxPitchOffset" init="0.0f" min="-90.0f" max="90.0f"/>
  </structdef>
  <structdef type="camCinematicMountedCameraMetadataLookAroundSettings">
    <float name="m_LeanScale" init="0.0f" min="0.0f" max="4.0f"/>
    <float name="m_MaxLookAroundHeightOffset" init="0.0f" min="0.0f" max="2.0f"/>
    <float name="m_MinLookAroundForwardOffset" init="0.0f" min="0.0f" max="2.0f"/>
    <float name="m_MaxLookAroundForwardOffset" init="0.0f" min="0.0f" max="2.0f"/>
    <float name="m_MaxLookAroundSideOffsetLeft" init="0.0f" min="-2.0f" max="2.0f"/>
    <float name="m_MaxLookAroundSideOffsetRight" init="0.0f" min="-2.0f" max="2.0f"/>
    <float name="m_MinLookAroundAngleToBlendInHeightOffset" init="0.0f" min="0.0f" max="90.0f"/>
    <float name="m_MaxLookAroundAngleToBlendInHeightOffset" init="90.0f" min="0.0f" max="180.0f"/>
  </structdef>
  <structdef type="camCinematicMountedCameraMetadataFirstPersonRoll">
    <float name="m_OrientateUpLimit" init="0.6f" min="0.0f" max="3.14f"/>
    <float name="m_OrientateWithVehicleLimit" init="1.57f" min="0.0f" max="3.14f"/>
    <float name="m_RollDampingRatio" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_RollDampingRatioDriveBy" init="0.25f" min="0.0f" max="1.0f"/>
  </structdef>
  <structdef type="camCinematicMountedCameraMetadataFirstPersonPitchOffset">
    <bool name="m_ShouldUseAdditionalOffsetForPitch" init="false"/>
    <Vector3 name="m_MaxOffset" init="0.0f, 0.0f, 0.0f" min="-2.0f" max="2.0f"/>
    <float name="m_MinAbsPitchToBlendInOffset" init="0.25f" min="0.0f" max="3.0f"/>
    <float name="m_MaxAbsPitchToBlendInOffset" init="1.0f" min="0.0f" max="3.0f"/>
    <float name="m_OffsetBlendRate" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <enumdef type="eCinematicMountedCameraVehicleBoneOrientationFlags">
    <enumval name="VEHICLE_BONE_USE_X_AXIS_ROTATION"/>
    <enumval name="VEHICLE_BONE_USE_Y_AXIS_ROTATION"/>
    <enumval name="VEHICLE_BONE_USE_Z_AXIS_ROTATION"/>
  </enumdef>

  <structdef type="camCinematicMountedCameraMetadata" base="camBaseCameraMetadata">
    <string name="m_ControlHelperRef" type="atHashString"/>
    <string name="m_MobilePhoneCameraControlHelperRef" type="atHashString"/>
    <string name="m_IdleShakeRef" type="atHashString"/>
    <string name="m_RelativeAttachSpringConstantEnvelopeRef" type="atHashString"/>
    <string name="m_RagdollBlendEnvelopeRef" type="atHashString"/>
    <string name="m_UnarmedDrivebyBlendEnvelopeRef" type="atHashString"/>
    <string name="m_DuckingBlendEnvelopeRef" type="atHashString"/>
    <string name="m_DrivebyBlendEnvelopeRef" type="atHashString"/>
    <string name="m_SmashWindowBlendEnvelopeRef" type="atHashString"/>
    <string name="m_LookBehindEnvelopeRef" type="atHashString"/>
    <string name="m_ResetLookEnvelopeRef" type="atHashString"/>
    <string name="m_LowriderAltAnimsEnvelopeRef" type="atHashString"/>
    <string name="m_SpringMountRef" type="atHashString"/>

    <float name="m_BaseFov" init="50.0f" min="1.0f" max="130.0f"/>
    <float name="m_BaseNearClip" init="0.1f" min="0.05f" max="0.5f"/>
    <float name="m_TripleHeadNearClip" init="0.075f" min="0.0f" max="0.5f"/>

    <Vector3 name="m_RelativeAttachPosition" init="0.08f, 1.35f, 0.7f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_TripleHeadRelativeAttachPosition" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_BoneRelativeAttachOffset" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_MaxRelativeAttachOffsets" init="99999.0f, 99999.0f, 99999.0f" min="0.0f" max="99999.0f"/>
    <Vector3 name="m_RelativeAttachOffsetForUnarmedDriveby" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeAttachOffsetForDucking" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeAttachOffsetForDriveby" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeAttachOffsetForSmashingWindow" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeAttachOffsetForRappelling" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeAttachOffsetForOpeningRearHeliDoors" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeAttachOffsetForLowriderAltAnims" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector2 name="m_RelativeAttachSpringConstantLimits" init="0.1f, 60.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_RelativeAttachSpringConstantLimitsForPassengers" init="20.0f, 60.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_AttachMatrixDampingPitchSoftLimits" init="0.0f, 0.0f" min="-90.0f" max="90.0f"/>
    <Vector2 name="m_AttachMatrixDampingPitchHardLimits" init="-80.0f, 80.0f" min="-90.0f" max="90.0f"/>
    <float name="m_AttachMatrixPitchSpringConstant" init="0.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachMatrixPitchSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_AttachMatrixRollSpringConstant" init="0.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachMatrixRollSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <bool name="m_UseAttachMatrixHeadingSpring" init="false"/>
    <float name="m_AttachMatrixHeadingSpringConstant" init="0.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachMatrixHeadingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_DefaultRelativePitch" init="0.0f" min="-30.0f" max="30.0f"/>
    <float name="m_DefaultReversePitch" init="0.0f" min="-30.0f" max="30.0f"/>
    <float name="m_MaxRollForLookBehind" init="3.0f" min="0.0f" max="30.0f"/>
    <Vector2 name="m_RelativeHeadingOffsetsForLookBehind" init="-160.0f, 160.0f" min="-180.0f" max="180.0f"/>
    <float name="m_FoVOffsetForDriveby" init="0.0f" min="-20.0f" max="20.0f"/>
    <bool name="m_FirstPersonCamera" init="false"/>
    <bool name="m_ShouldLookBehindInGameplayCamera" init="false"/>
    <bool name="m_ShouldAffectAiming" init="false"/>
    <struct type="camCinematicMountedCameraMetadataRelativePitchScalingToThrottle" name="m_RelativePitchScaling"/>
    <struct type="camSpeedRelativeShakeSettingsMetadata" name="m_HighSpeedShakeSettings"/>
    <struct type="camSpeedRelativeShakeSettingsMetadata" name="m_HighSpeedVibrationShakeSettings"/>
    <struct type="camVehicleRocketSettings" name="m_RocketSettings"/>
    <bool name="m_ShouldUseAdditionalRelativeAttachOffsetWhenLookingAround" init="false"/>
    <bool name="m_ShouldUseAdditionalRelativeAttachOffsetForDriverOnlyWhenLookingAround" init="false"/>
    <bool name="m_UseAttachMatrixForAdditionalOffsetWhenLookingAround" init="false"/>
    <struct type="camCinematicMountedCameraMetadataLookAroundSettings" name="m_LookAroundSettings"/>
    <struct type="camCinematicMountedCameraMetadataLookAroundSettings" name="m_DriveByLookAroundSettings"/>
    <struct type="camCinematicMountedCameraMetadataLookAroundSettings" name="m_VehicleMeleeLookAroundSettings"/>
    <struct type="camCinematicMountedCameraMetadataFirstPersonRoll" name="m_FirstPersonRollSettings"/>
    <struct type="camCinematicMountedCameraMetadataFirstPersonPitchOffset" name="m_FirstPersonPitchOffsetSettings"/>
    <bool name="m_ShouldShakeOnImpact" init="false"/>
    <float name="m_RelativeAttachSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinSpeedForMaxRelativeAttachSpringConstant" init="0.1f" min="0.0f" max="1000.0f"/>
    <float name="m_FallBackHeightDeltaFromAttachPositionForCollision" init="0.23f" min="-2.0f" max="2.0f"/>
    <float name="m_CollisionTestRadiusOffset" init="0.0f" min="-2.0f" max="2.0f"/>
    <bool name="m_ShouldIgnoreVehicleSpecificAttachPositionOffset" init="false"/>
    <bool name="m_ShouldAttachToFollowPedHead" init="false"/>
    <bool name="m_ShouldUseCachedHeadBoneMatrix" init="false"/>
    <bool name="m_ShouldAttachToFollowPedSeat" init="true"/>
    <bool name="m_ShouldRestictToFrontSeat" init="true"/>
    <bool name="m_ShouldAttachToVehicleExitEntryPoint" init="false"/>
    <bool name="m_ShouldAttachToVehicleBone" init="false"/>
    <bool name="m_ShouldAttachToVehicleTurret" init="false"/>
    <bool name="m_ShouldAttachOrientationToVehicleBone" init="false"/>
    <bool name="m_ShouldUseVehicleOrientationForAttach" init="false"/>
    <bool name="m_ShouldForceUseofSideOffset" init="false"/>
    <bitset name="m_VehicleBoneOrientationFlags" type="fixed32" values="eCinematicMountedCameraVehicleBoneOrientationFlags"/>
    <enum name="m_VehicleAttachPart" type="eCinematicVehicleAttach" init="CVA_WHEEL_REAR_LEFT"/>
    
    <enum name="m_LookAtBehaviour" type="eCinematicMountedCameraLookAtBehaviour" init="LOOK_FORWARD_RELATIVE_TO_ATTACH"/>
    <Vector3 name="m_RelativeLookAtPosition" init="0.0f, 0.0f, 0.7f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_LookBehindRelativeAttachPosition" init="0.08f, 1.35f, 0.7f" min="-100.0f" max="100.0f"/>
    <bool name="m_ShouldUseLookBehindCustomPosition" init="false"/>
    <s32 name="m_FollowPedLookAtBoneTag" init="-1" min="-1" max="99999"/>

    <struct type="camCinematicMountedCameraMetadataOrientationSpring" name="m_OrientationSpring"/>
    <struct type="camCinematicMountedCameraMetadataLeadingLookSettings" name="m_LeadingLookSettings"/>

    <float name="m_MinPitch" init="-18.9f" min="-89.0f" max="89.0f"/>
    <float name="m_MaxPitch" init="57.3f" min="-89.0f" max="89.0f"/>
    <float name="m_MaxPitchUnarmed" init="57.3f" min="-89.0f" max="89.0f"/>
    <float name="m_PitchOffsetToApply" init="0.0f" min="-30.0f" max="30.0f"/>

    <Vector2 name="m_RelativeHeadingLimitsForMobilePhoneCamera" init="-100.0f, 100.0f" min="-180.0f" max="180.0f"/>

    <bool name="m_IsBehindVehicleGlass" init="false"/>
    <bool name="m_IsForcingMotionBlur" init="false"/>
    <bool name="m_ShouldDisplayReticule" init="false"/>
    <bool name="m_ShouldMakeFollowPedHeadInvisible" init="false"/>
    <bool name="m_ShouldMakePedInAttachSeatInvisible" init="false"/>
    <bool name="m_ShouldCopyVehicleCameraMotionBlur" init="false"/>

    <bool name="m_LimitAttachParentRelativePitchAndHeading" init="false"/>
    <bool name="m_ShouldTerminateForPitchAndHeading" init="false"/>
    <Vector2 name="m_AttachParentRelativePitch" init="-20.0f, 20.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativePitchForLookBehindDriveBy" init="-3.0f, 7.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativePitchForRappelling" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativePitchForScriptedVehAnim" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativePitchForVehicleMelee" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeading" init="-110.0f, 110.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForLookBehind" init="-30.0f, 30.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForLookBehindDriveBy" init="-30.0f, 50.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForUnarmedDriveby" init="-50.0f, 50.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForScriptedVehAnim" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForExitHeliSideDoor" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForDriveby" init="-90.0f, 90.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForDucking" init="-50.0f, 50.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForVisiblePed" init="-60.0f, 60.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeadingForVehicleMelee" init="-180.0f, 180.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_InitialRelativePitchLimits" init="-100.0f, 100.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_InitialRelativeHeadingLimits" init="-100.0f, 100.0f" min="-180.0f" max="180.0f"/>
    <bool name="m_ShouldApplyAttachParentRoll" init="false"/>
    <bool name="m_ShouldTerminateForWorldPitch" init="false"/>
    <Vector2 name="m_InitialWorldPitchLimits" init="-50.0f, 50.0f" min="-90.0f" max="90.0f"/>
    <Vector2 name="m_WorldPitchLimits" init="-70.0f, 70.0f" min="-90.0f" max="90.0f"/>
    
    <bool name="m_ShouldTerminateForOcclusion" init="false"/>
    <u32 name="m_MaxTimeToSpendOccluded" init="1000" min="0" max="10000"/>
    <bool name="m_ShouldTerminateIfOccludedByAttachParent" init="false"/>
    <u32 name="m_MaxTimeToSpendOccludedByAttachParent" init="1000" min="0" max="10000"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_RadiusScalingForOcclusionTest" init="1.0f" min="0.01f" max="10.0f"/>
    <bool name="m_ShouldTerminateForDistanceToTarget" init="false"/>
    <float name="m_DistanceToTerminate" init="60.0f" min="0.0f" max="1000.0f"/>
    <bool name="m_ShouldCalculateXYDistance" init="false"/>
    <bool name="m_ShouldTestForClipping" init="false"/>
    <u32 name="m_MaxTimeToClipIntoDynamicCollision" init="0" min="0" max="100000"/>
    <float name="m_RadiusScalingForClippingTest" init="1.0f" min="0.01f" max="10.0f"/>
    <bool name="m_DisableWaterClippingTest" init="false"/>
    <bool name="m_ForceInvalidateWhenInOcean" init="false"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <bool name="m_ShouldConsiderMinHeightAboveOrBelowWater" init="false"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
    <bool name="m_ShouldUseDifferentMinHeightAboveWaterOnFirstUpdate" init="false"/>
    <float name="m_MinHeightAboveWaterOnFirstUpdate" init="2.0f" min="0.01f" max="100.0f"/>
    <bool name="m_RestoreHeadingAndPitchAfterClippingWithWater" init="false"/>
    <string name="m_InVehicleLookAtDampingRef" type="atHashString"/>
    <string name="m_OnFootLookAtDampingRef" type="atHashString"/>
    <string name="m_InVehicleLookAheadRef" type="atHashString"/>
    <bool name="m_ShouldByPassNearClip" init="false"/>
    <bool name="m_ShouldTestForMapPenetrationFromAttachPosition" init="false"/>
    <float name="m_BaseHeading" init="0.0f" min="-180.0f" max="180.0f"/>
    <bool name="m_ShouldAttachToFollowPed" init="false"/>
    <float name="m_InVehicleLookBehindSideOffset" init="0.2f" min="-10000.0f" max="10000.0f"/>
    <float name="m_LookAroundScalingForUnarmedDriveby" init="0.5f" min="0.0f" max="10.0f"/>
    <float name="m_LookAroundScalingForArmedDriveby" init="1.0f" min="0.0f" max="10.0f"/>
  
    <struct type="camCinematicMountedCameraMetadataMovementOnAccelerationSettings" name="m_AccelerationMovementSettings"/>
    
    <float name="m_MinHeightWhenUnarmedDriveByAiming" init="0.0f" min="0.0f" max="999.0f"/>
    <Vector3 name="m_AbsMaxRelativePositionWhenSeatShuffling" init="999.0f, 999.0f, 999.0f" min="0.0f" max="999.0f"/>
    <float name="m_RelativeHeadingWhenSeatShuffling" init="0.0f" min="-180.0f" max="180.0f"/>
    <float name="m_SeatShufflingPhaseThreshold" init="0.75f" min="0.0f" max="1.0f"/>
    <float name="m_SeatShufflingSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SeatShufflingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_ScriptedVehAnimBlendLevelSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_ScriptedVehAnimBlendLevelSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_RappellingBlendLevelSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_RappellingBlendLevelSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_OpenHeliSideDoorBlendLevelSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_OpenHeliSideDoorBlendLevelSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_VehMeleeBlendLevelSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_VehMeleeBlendLevelSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>

    <float name="m_ResetLookTheshold" init="100.0f" min="0.0f" max="180"/>

    <Vector3 name="m_PutOnHelmetCameraOffset" init="0.0f, 0.0f, 0.0f" min="-1.0f" max="1.0f"/>
    <Vector3 name="m_TripleHeadPutOnHelmetCameraOffset" init="0.0f, 0.0f, 0.0f" min="-1.0f" max="1.0f"/>

    <float name="m_SideOffsetSpringConstant" init="10.0f" min="0.0f" max="100.0f"/>
    <float name="m_SideOffsetDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camCinematicMountedPartCameraMetadata" base="camBaseCameraMetadata">
    <string name="m_RelativeAttachSpringConstantEnvelopeRef" type="atHashString"/>
    <string name="m_SpringMountRef" type="atHashString"/>

    <float name="m_BaseFov" init="50.0f" min="1.0f" max="130.0f"/>
    <float name="m_BaseNearClip" init="0.1f" min="0.05f" max="0.5f"/>

    <enum name="m_AttachPart" type="eCinematicVehicleAttach" init="CVA_WHEEL_FRONT_LEFT"/>
    <Vector3 name="m_RelativeAttachOffset" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeLookAtPosition" init="0.0f, 1.0f, 0.25f" min="-100.0f" max="100.0f"/>

    <Vector2 name="m_RelativeAttachSpringConstantLimits" init="0.1f, 60.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_RelativeAttachSpringConstantLimitsForPassengers" init="20.0f, 60.0f" min="0.0f" max="1000.0f"/>
    <float name="m_RelativeAttachSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinSpeedForMaxRelativeAttachSpringConstant" init="0.1f" min="0.0f" max="1000.0f"/>

    <struct type="camSpeedRelativeShakeSettingsMetadata" name="m_HighSpeedShakeSettings"/>

    <bool name="m_IsBehindVehicleGlass" init="false"/>
    <bool name="m_IsForcingMotionBlur" init="false"/>
    <bool name="m_ShouldDisplayReticule" init="false"/>
    <bool name="m_ShouldMakeFollowPedHeadInvisible" init="false"/>
    <bool name="m_ShouldCopyVehicleCameraMotionBlur" init="false"/>

    <u32 name="m_MaxTimeToSpendOccluded" init="1000" min="0" max="10000"/>
    <float name="m_CollisionRadius" init="0.21f" min="0.01f" max="1.0f"/>
    <float name="m_RadiusScalingForOcclusionTest" init="1.0f" min="0.01f" max="10.0f"/>
    <bool name="m_ShouldTestForClipping" init="false"/>
    <float name="m_RadiusScalingForClippingTest" init="1.0f" min="0.01f" max="10.0f"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
  </structdef>

  <!-- a cinematic camera that tracks a train from one of a set of fixed positions defined in data -->
  <structdef type="camCinematicTrainTrackingCameraMetadata" base="camCinematicCameraManCameraMetadata">
    <Vector3 name="m_EngineRelativeLookAtOffset" init="0.0f, 6.0f, 1.0f" min="-100.0f" max="100.0f"/>
    <Vector2 name="m_ShotDistanceAheadOfTrain" init="25.0f, 75.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_TrackingShotSideOffset" init="3.0f, 25.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_TrackingShotVerticalOffset" init="2.0f, 10.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_FixedShotSideOffset" init="3.0f, 5.0f" min="0.0f" max="1000.0f"/>
    <Vector2 name="m_FixedShotVerticalOffset" init="2.0f, 4.0f" min="0.0f" max="1000.0f"/>
  </structdef>

  <!-- a cinematic camera that provides a close-up view of a target ped while keeping a look at ped in frame -->
  <structdef type="camCinematicTwoShotCameraMetadata" base="camBaseCameraMetadata">
    <float name="m_BaseFov" init="25.0f" min="1.0f" max="130.0f"/>
    <float name="m_BaseNearClip" init="0.1f" min="0.05f" max="0.5f"/>
    <float name="m_TimeScale" init="0.2f" min="0.0f" max="1.0f"/>

    <float name="m_CameraRelativeLeftSideOffset" init="0.25f" min="-99999.0f" max="99999.0f"/>
    <float name="m_CameraRelativeRightSideOffset" init="-0.325f" min="-99999.0f" max="99999.0f"/>
    <float name="m_CameraRelativeVerticalOffset" init="0.15f" min="-99999.0f" max="99999.0f"/>

    <float name="m_HeightAboveCameraForGroundProbe" init="0.5f" min="0.0f" max="99999.0f"/>
    <float name="m_HeightBelowCameraForGroundProbe" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MinHeightAboveGround" init="0.2f" min="0.0f" max="99999.0f"/>
    <float name="m_HeightSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_HeightSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>

    <float name="m_OrbitDistance" init="3.5f" min="-99999.0f" max="99999.0f"/>

    <s32 name="m_LookAtBoneTag" init="24818" min="-1" max="99999"/>

    <float name="m_MinDistanceForLockOn" init="0.5f" min="0.0f" max="99999.0f"/>
    <float name="m_ScreenHeightRatioBiasForSafeZone" init="-0.1665f" min="-1.0f" max="1.0f"/>
    <float name="m_ScreenWidthRatioBiasForSafeZone" init="0.1665f" min="0.0f" max="1.0f"/>

    <float name="m_LosTestRadius" init="0.18f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
    <float name="m_ClippingTestRadius" init="0.18f" min="0.0f" max="99999.0f"/>

    <bool name="m_ShouldTrackTarget" init="false"/>
    <float name="m_AttachEntityPositionSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AttachEntityPositionSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>

    <bool name="m_ShouldLockCameraPositionOnClipping" init="false"/>
    <float name="m_FovScalingSpeed" init="0.0f" min="-1000.0f" max="1000.0f"/>
  </structdef>
  
  <!-- a cinematic camera that provides a close-up view of a target ped -->
  <structdef type="camCinematicPedCloseUpCameraMetadata" base="camBaseCameraMetadata">
    <float name="m_BaseFov" init="25.0f" min="1.0f" max="130.0f"/>
    <float name="m_BaseNearClip" init="0.1f" min="0.05f" max="0.5f"/>

    <s32 name="m_AttachBoneTag" init="24818" min="-1" max="99999"/>
    <Vector3 name="m_AttachOffset" init="0.0f, 0.0f, 0.25f" min="-100.0f" max="100.0f"/>
    <bool name="m_ShouldApplyAttachOffsetInLocalSpace" init="false" description="if true the attach offset will be applied relative to the attach parent, rather than in world space"/>

    <s32 name="m_LookAtBoneTag" init="24818" min="-1" max="99999"/>
    <Vector3 name="m_LookAtOffset" init="0.0f, 0.0f, 0.25f" min="-100.0f" max="100.0f"/>
    <bool name="m_ShouldApplyLookAtOffsetInLocalSpace" init="false" description="if true the look-at offset will be applied relative to the look-at ped, rather than in world space"/>
    
    <u32 name="m_NumHeadingSweepIterations" init="4" min="2" max="10"/>
    <float name="m_MinGameplayToShotHeadingDelta" init="30.0f" min="0.0f" max="180.0f"/>
    <float name="m_MinLineOfActionToShotHeadingDelta" init="0.0f" min="0.0f" max="180.0f"/>
    <float name="m_MaxLineOfActionToShotHeadingDelta" init="180.0f" min="0.0f" max="180.0f"/>

    <float name="m_MinOrbitPitch" init="-25.0f" min="-89.0f" max="89.0f"/>
    <float name="m_MaxOrbitPitch" init="50.0f" min="-89.0f" max="89.0f"/>

    <float name="m_IdealOrbitDistance" init="3.5f" min="-99999.0f" max="99999.0f"/>
    <float name="m_MinOrbitDistance" init="1.0f" min="-99999.0f" max="99999.0f"/>
    
    <float name="m_CollisionTestRadius" init="0.2f" min="0.0f" max="99999.0f"/>
    <float name="m_LosTestRadius" init="0.18f" min="0.0f" max="99999.0f"/>
    <float name="m_ClippingTestRadius" init="0.18f" min="0.0f" max="99999.0f"/>

    <float name="m_MinAttachToLookAtDistanceForLosTest" init="0.2f" min="0.0f" max="99999.0f"/>

    <float name="m_AttachToLookAtBlendLevel" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_MaxLeadingLookHeadingOffset" init="5.0f" min="-90.0f" max="90.0f"/>
    <float name="m_LeadingLookSpringConstant" init="0.0f" min="0.0f" max="1000.0f"/>
    <float name="m_LookAtAlignmentSpringConstant" init="60.0f" min="0.0f" max="1000.0f"/>

    <u32 name="m_MaxTimeToSpendOccluded" init="100000" min="0" max="100000"/>
    <float name="m_MaxDistanceToTerminate" init="100000.0f" min="0.0f" max="100000.0f"/>
    <float name="m_MaxDistanceForWaterClippingTest" init="99999.0f" min="0.01f" max="99999.0f"/>
    <float name="m_MaxDistanceForRiverWaterClippingTest" init="10.0f" min="0.01f" max="100.0f"/>
    <float name="m_MinHeightAboveWater" init="1.0f" min="0.01f" max="100.0f"/>
  </structdef>

  <!-- a debug camera that allows free movement via control input -->
  <structdef type="camFreeCameraMetadata" base="camBaseCameraMetadata">
    <Vector3 name="m_StartPosition"  init="0.0f" min="-99999.0f" max="99999.0f"/>

    <float name="m_ForwardAcceleration" init="35.0f" min="-100.0f" max="100.0f"/>
    <float name="m_StrafeAcceleration" init="75.0f" min="-100.0f" max="100.0f"/>
    <float name="m_VerticalAcceleration" init="75.0f" min="-100.0f" max="100.0f"/>
    <float name="m_MaxTranslationSpeed" init="600.0f" min="-1000.0f" max="1000.0f"/>

    <float name="m_HeadingAcceleration" init="10000.0f" min="-10000.0f" max="10000.0f"/>
    <float name="m_PitchAcceleration" init="10000.0f" min="-10000.0f" max="10000.0f"/>
    <float name="m_RollAcceleration" init="230.0f" min="-10000.0f" max="10000.0f"/>
    <float name="m_MaxRotationSpeed" init="157.6f" min="-360.0f" max="360.0f"/>

    <float name="m_FovAcceleration" init="10.0f" min="-100.0f" max="100.0f"/>
    <float name="m_MaxFovSpeed" init="20.0f" min="-100.0f" max="100.0f"/>

    <float name="m_MaxPitch" init="89.5f" min="-90.0f" max="90.0f"/>

    <float name="m_SlowMoTranslationScalingFactor" init="0.1f" min="0.0f" max="1.0f"/>
    <float name="m_SlowMoRotationScalingFactor" init="0.3f" min="0.0f" max="1.0f"/>

    <u32 name="m_NumUpdatesToPushNearClipWhenTeleportingFollowPed" init="15" min="0" max="100"/>
  </structdef>

  <!-- a camera that follows a ped -->
  <structdef type="camFollowPedCameraMetadataCustomViewModeSettings">
    <float name="m_AttachParentHeightRatioToAttainForBasePivotPosition" init="0.8f" min="-10.0f" max="10.0f"/>
    <float name="m_ScreenRatioForMinFootRoom" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_ScreenRatioForMaxFootRoom" init="0.1f" min="-2.0f" max="0.49f"/>
    <float name="m_BaseOrbitPitchOffset" init="0.0f" min="-90.0f" max="90.0f"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataAssistedMovementAlignment">
    <bool name="m_ShouldAlign" init="false"/>
    <string name="m_AlignmentEnvelopeRef" type="atHashString"/>
    <float name="m_RouteCurvatureForMaxPullAround" init="90.0f" min="0.0f" max="360.0f"/>
    <float name="m_MinBlendLevelToForceCollisionPopIn" init="0.2f" min="0.0f" max="1.0f"/>
    <struct type="camFollowCameraMetadataPullAroundSettings" name="m_PullAroundSettings"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataLadderAlignment">
    <bool name="m_ShouldAlign" init="false"/>
    <string name="m_AlignmentEnvelopeRef" type="atHashString"/>
    <Vector2 name="m_RelativeOrbitHeadingLimits" init="-110.0f, 110.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_OrbitPitchLimits" init="-75.0f, 75.0f" min="-89.0f" max="89.0f"/>
    <float name="m_PitchLevelSmoothRate" init="0.1f" min="0.0f" max="1.0f"/>
    <float name="m_VerticalMoveSpeedScaling" init="0.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinOrbitDistanceRatioAfterCollisionForCustomPullAround" init="0.85f" min="0.0f" max="1.0f"/>
    <u32 name="m_MinTimeToBlockCustomPullAround" init="250" min="0" max="60000"/>
    <float name="m_DesiredPullAroundHeadingOffset" init="85.0f" min="0.0f" max="180.0f"/>
    <struct type="camFollowCameraMetadataPullAroundSettings" name="m_PullAroundSettings"/>
    <struct type="camFollowCameraMetadataPullAroundSettings" name="m_PullAroundSettingsForMountAndDismount"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataRappellingAlignment">
    <bool name="m_ShouldAlign" init="false"/>
    <float name="m_PitchLevelSmoothRate" init="0.1f" min="0.0f" max="1.0f"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataOrbitPitchLimitsForOverheadCollision">
    <Vector2 name="m_MinOrbitPitchLimits" init="-89.0f, -30.0f" min="-89.0f" max="89.0f"/>
    <float name="m_CollisionFallBackBlendLevelForFullLimit" init="0.7f" min="0.0f" max="1.0f"/>
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataRunningShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_MinAmplitude" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_MaxAmplitude" init="0.0f" min="0.0f" max="2.0f"/>
    <float name="m_MaxFrequencyScale" init="0.0f" min="0.0f" max="2.0f"/>
    <float name="m_RunRatioSpringConstant" init="5.0f" min="0.0f" max="1000.0f"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataSwimmingShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_MinAmplitude" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_MaxAmplitude" init="0.0f" min="0.0f" max="2.0f"/>
    <float name="m_MaxFrequencyScale" init="0.0f" min="0.0f" max="2.0f"/>
    <float name="m_UnderwaterFrequencyScale" init="0.5f" min="0.0f" max="2.0f"/>
    <float name="m_SwimRatioSpringConstant" init="5.0f" min="0.0f" max="1000.0f"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataDivingShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_MinHeight" init="5.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxHeight" init="15.0f" min="0.0f" max="100.0f"/>
    <float name="m_MinAmplitude" init="0.5f" min="0.0f" max="5.0f"/>
    <float name="m_MaxAmplitude" init="1.0f" min="0.0f" max="5.0f"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataHighFallShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_MinSpeed" init="5.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxSpeed" init="35.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxAmplitude" init="4.0f" min="0.0f" max="5.0f"/>
  </structdef>
  <structdef type="camFollowPedCameraMetadataPushBeyondNearbyVehiclesInRagdollSettings">
    <u32 name="m_MaxDurationToTrackVehicles" init="1000" min="0" max="600000"/>
    <float name="m_MaxAabbDistanceToTrackVehicles" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_MinRagdollBlendLevel" init="0.01f" min="0.0f" max="1.0f"/>
    <float name="m_MinRagdollBlendLevelDuringRelease" init="0.75f" min="0.0f" max="1.0f"/>
    <float name="m_DistanceToTestUpForVehicles" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_DetectionRadius" init="0.5f" min="0.0f" max="10.0f"/>
    <float name="m_DistanceToTestDownForVehiclesToReject" init="2.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <structdef type="camFollowPedCameraMetadata" base="camFollowCameraMetadata">
    <bool name="m_ShouldAutoLevel" init="false"/>
    <bool name="m_ShouldIgnoreInputDuringAutoLevel" init="false"/>
    <float name="m_AutoLevelSmoothRate" init="0.1f" min="0.0f" max="1.0f"/>

    <string name="m_RagdollBlendEnvelopeRef" type="atHashString"/>
    <string name="m_VaultBlendEnvelopeRef" type="atHashString"/>
    <u32 name="m_VaultSlideAttackDuration" init="100" min="0" max="5000"/>
    <float name="m_MinHandHoldHeightForClimbUpBehaviour" init="1.3f" min="0.0f" max="1000.0f"/>
    <float name="m_ClimbUpBlendSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_ClimbUpBlendSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <Vector2 name="m_RelativeOrbitHeadingLimitsForClimbUp" init="-70.0f, 70.0f" min="-180.0f" max="180.0f"/>
    <string name="m_MeleeBlendEnvelopeRef" type="atHashString"/>

    <bool name="m_ShouldUseCustomFramingForViewModes" init="false"/>
    <array name="m_CustomFramingForViewModes" type="member" size="5">
      <struct type="camFollowPedCameraMetadataCustomViewModeSettings"/>
    </array>
    <array name="m_CustomFramingForViewModesInTightSpace" type="member" size="5">
      <struct type="camFollowPedCameraMetadataCustomViewModeSettings"/>
    </array>
    <bool name="m_ShouldUseCustomFramingAfterAiming" init="false"/>
    <array name="m_CustomFramingForViewModesAfterAiming" type="member" size="5">
      <struct type="camFollowPedCameraMetadataCustomViewModeSettings"/>
    </array>
    <array name="m_CustomFramingForViewModesAfterAimingInTightSpace" type="member" size="5">
      <struct type="camFollowPedCameraMetadataCustomViewModeSettings"/>
    </array>
    <string name="m_CustomNearToMediumViewModeBlendEnvelopeRef" type="atHashString"/>
    <float name="m_MinAttachSpeedToUpdateCustomNearToMediumViewModeBlendLevel" init="0.25f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxCustomNearToMediumViewModeBlendLevel" init="1.0f" min="0.0f" max="1.0f"/>
    <float name="m_CustomNearToMediumViewModeBlendInSpringConstant" init="5.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CustomNearToMediumViewModeBlendOutSpringConstant" init="0.5f" min="0.0f" max="1000.0f"/>
    <float name="m_CustomNearToMediumViewModeBlendSpringDampingRatio" init="0.95f" min="0.0f" max="10.0f"/>
    <float name="m_MinAttachSpeedToUpdateCustomViewModeBlendForCutsceneBlendOut" init="0.25f" min="0.0f" max="1000.0f"/>
    <float name="m_CustomViewModeBlendForCutsceneBlendOutSpringConstant" init="1.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CustomViewModeBlendForCutsceneBlendOutSpringDampingRatio" init="0.95f" min="0.0f" max="10.0f"/>

    <struct type="camFollowPedCameraMetadataAssistedMovementAlignment" name="m_AssistedMovementAlignment"/>
    <struct type="camFollowPedCameraMetadataLadderAlignment" name="m_LadderAlignment"/>
    <struct type="camFollowPedCameraMetadataRappellingAlignment" name="m_RappellingAlignment"/>

    <bool name="m_ShouldUseCustomPullAroundSettingsInTightSpace" init="false"/>
    <struct type="camFollowCameraMetadataPullAroundSettings" name="m_PullAroundSettingsInTightSpace"/>

    <Vector2 name="m_OrbitPitchLimitsForRagdoll" init="-70.0f, -10.0f" min="-89.0f" max="89.0f"/>
    <Vector2 name="m_HintOrbitPitchLimitsForRagdoll" init="-70.0f, 15.0f" min="-89.0f" max="89.0f"/>
    <bool name="m_ShouldPushBeyondAttachParentIfClippingInRagdoll" init="true"/>
    <float name="m_MaxCollisionTestRadiusForRagdoll" init="0.11f" min="0.0f" max="10.0f"/>
    <float name="m_MaxCollisionTestRadiusForVault" init="0.15f" min="0.0f" max="10.0f"/>

    <bool name="m_ShouldApplyCustomOrbitPitchLimitsWhenBuoyant" init="false"/>
    <Vector2 name="m_OrbitPitchLimitsWhenBuoyant" init="-89.0f, 89.0f" min="-89.0f" max="89.0f"/>

    <struct type="camFollowPedCameraMetadataOrbitPitchLimitsForOverheadCollision" name="m_OrbitPitchLimitsForOverheadCollision"/>

    <float name="m_VehicleEntryExitPitchDegrees" init="-10.0f" min="-50.0f" max="20.0f"/>
    <float name="m_VehicleEntryExitPitchLevelSmoothRate" init="0.1f" min="0.0f" max="1.0f"/>

    <float name="m_ExtraOffsetForHatsAndHelmetsForTripleHead" init="0.0f" min="0.0f" max="1.0f"/>

    <struct type="camFollowPedCameraMetadataRunningShakeSettings" name="m_RunningShakeSettings"/>
    <struct type="camFollowPedCameraMetadataSwimmingShakeSettings" name="m_SwimmingShakeSettings"/>
    <struct type="camFollowPedCameraMetadataDivingShakeSettings" name="m_DivingShakeSettings"/>
    <struct type="camFollowPedCameraMetadataHighFallShakeSettings" name="m_HighFallShakeSettings"/>
    <struct type="camFollowPedCameraMetadataPushBeyondNearbyVehiclesInRagdollSettings" name="m_PushBeyondNearbyVehiclesInRagdollSettings"/>
  </structdef>

  <!-- a camera that follows a vehicle -->
  <structdef type="camFollowVehicleCameraMetadataHandBrakeSwingSettings">
    <string name="m_HandBrakeInputEnvelopeRef" type="atHashString"/>
    <float name="m_SpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MinLateralSkidSpeed" init="5.0f" min="0.0f" max="99999.0f"/>
    <float name="m_MaxLateralSkidSpeed" init="15.0f" min="0.0f" max="99999.0f"/>
    <float name="m_SwingSpeedAtMaxSkidSpeed" init="175.0f" min="-720.0f" max="720.0f"/>
  </structdef>
  <structdef type="camFollowVehicleCameraMetadataDuckUnderOverheadCollisionSettingsCapsuleSettings">
    <u32 name="m_NumTests" init="4" min="1" max="10"/>
    <float name="m_LengthScaling" init="1.25f" min="0.0f" max="10.0f"/>
    <Vector2 name="m_OffsetLimits" init="0.18f, 10.0f" min="0.0f" max="99999.0f"/>
  </structdef>
  <structdef type="camFollowVehicleCameraMetadataDuckUnderOverheadCollisionSettings">
    <string name="m_EnvelopeRef" type="atHashString"/>
    <bool name="m_ShouldDuck" init="false"/>
    <float name="m_OrbitPitchOffsetWhenFullyDucked" init="0.0f" min="-90.0f" max="90.0f"/>
    <float name="m_MaxDistanceToPersist" init="20.0f" min="0.0f" max="100.0f"/>
    <float name="m_SpringConstant" init="20.0f" min="0.0f" max="1000.0f"/>
    <struct type="camFollowVehicleCameraMetadataDuckUnderOverheadCollisionSettingsCapsuleSettings" name="m_CapsuleSettings"/>
  </structdef>
  <structdef type="camFollowVehicleCameraMetadataHighSpeedZoomSettings">
    <float name="m_MinForwardSpeed" init="20.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxForwardSpeed" init="35.0f" min="0.0f" max="100.0f"/>
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxBaseFovScaling" init="1.25f" min="0.1f" max="2.0f"/>
    <float name="m_CutsceneBlendSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_CutsceneBlendSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camFollowVehicleCameraMetadataHighSpeedShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <float name="m_MinForwardSpeed" init="25.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxForwardSpeed" init="45.0f" min="0.0f" max="100.0f"/>
    <float name="m_SpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
  </structdef>
  <structdef type="camFollowVehicleCameraMetadataWaterEntryShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <u32 name="m_MaxShakeInstances" init="5" min="1" max="5"/>
    <Vector2 name="m_DownwardSpeedLimits" init="2.0f, 8.0f" min="0.0f" max="99999.0f"/>
    <Vector2 name="m_AmplitudeLimits" init="0.25f, 1.0f" min="0.0f" max="10.0f"/>
  </structdef>
  <structdef type="camFollowVehicleCameraMetadataVerticalFlightModeSettings">
    <Vector2 name="m_OrbitPitchLimits" init="-45.0f, 12.5f" min="-89.0f" max="89.0f"/>
  </structdef>

  <structdef type="camFollowVehicleCameraMetadata" base="camFollowCameraMetadata">
    <struct type="camFollowVehicleCameraMetadataHandBrakeSwingSettings" name="m_HandBrakeSwingSettings"/>

    <struct type="camFollowVehicleCameraMetadataDuckUnderOverheadCollisionSettings" name="m_DuckUnderOverheadCollisionSettings"/>

    <struct type="camFollowVehicleCameraMetadataHighSpeedZoomSettings" name="m_HighSpeedZoomSettings"/>
    <struct type="camFollowVehicleCameraMetadataHighSpeedShakeSettings" name="m_HighSpeedShakeSettings"/>

    <struct type="camFollowVehicleCameraMetadataWaterEntryShakeSettings" name="m_WaterEntryShakeSettings"/>

    <struct type="camFollowVehicleCameraMetadataVerticalFlightModeSettings" name="m_VerticalFlightModeSettings"/>

    <struct type="camVehicleCustomSettingsMetadataDoorAlignmentSettings" name="m_DoorAlignmentSettings"/>

    <float name="m_VehicleEntryExitPitchLevelSmoothRate" init="0.1f" min="0.0f" max="1.0f"/>

    <float name="m_ExtraOrbitPitchOffsetForHighAngleMode" init="-5.0f" min="-90.0f" max="90.0f"/>

    <float name="m_ExtraOrbitPitchOffsetForThirdPersonFarViewMode" init="0.0f" min="-90.0f" max="90.0f"/>
    <bool name="m_ShouldForceCutToOrbitDistanceLimitsForThirdPersonFarViewMode" init="false"/>

    <string name="m_ThirdPersonVehicleAimCameraRef" type="atHashString"/>
    
    <bool name="m_IgnoreOcclusionWithNearbyBikes" init="true"/>

    <string name="m_DeployParachuteShakeRef" type="atHashString"/>

    <Vector2 name="m_OrbitPitchLimitsWhenAirborne" init="-60.0f, 45.0f" min="-89.0f" max="89.0f"/>
  </structdef>

  <!-- a camera that follows an object -->
  <structdef type="camFollowObjectCameraMetadata" base="camFollowCameraMetadata">
  </structdef>

  <!-- a camera that follows a parachute -->
  <structdef type="camFollowParachuteCameraMetadataCustomSettings">
    <string name="m_VerticalMoveSpeedEnvelopeRef" type="atHashString"/>
    <float name="m_BaseVerticalMoveSpeedScale" init="0.0f" min="0.0f" max="5.0f"/>
    <string name="m_DeployShakeRef" type="atHashString"/>
  </structdef>

  <structdef type="camFollowParachuteCameraMetadata" base="camFollowObjectCameraMetadata">
    <struct type="camFollowParachuteCameraMetadataCustomSettings" name="m_CustomSettings"/>
  </structdef>

  <!-- a marketing camera that allows free movement via control input -->
  <structdef type="camMarketingFreeCameraMetadataInputResponse">
    <float name="m_MaxInputMagInDeadZone" init="0.05f" min="0.0f" max="1.0f"/>
    <float name="m_InputMagPowerFactor" init="4.0f" min="1.0f" max="10.0f"/>

    <float name="m_Acceleration" init="2000.0f" min="0.0f" max="100000.0f"/>
    <float name="m_Deceleration" init="2000.0f" min="0.0f" max="100000.0f"/>
    <float name="m_MaxSpeed" init="90.0f" min="-10000.0f" max="10000.0f"/>
  </structdef>

  <structdef type="camMarketingFreeCameraMetadata" base="camBaseCameraMetadata">
    <struct type="camMarketingFreeCameraMetadataInputResponse" name="m_LookAroundInputResponse"/>
    <struct type="camMarketingFreeCameraMetadataInputResponse" name="m_TranslationInputResponse"/>
    <struct type="camMarketingFreeCameraMetadataInputResponse" name="m_RollInputResponse"/>

    <float name="m_ZoomMinFov" init="5.0f" min="1.0f" max="130.0f"/>
    <float name="m_ZoomMaxFov" init="100.0f" min="1.0f" max="130.0f"/>
    <float name="m_ZoomDefaultFov" init="45.0f" min="5.0f" max="100.0f"/>
    <struct type="camMarketingFreeCameraMetadataInputResponse" name="m_ZoomInputResponse"/>

    <float name="m_MinSpeedScaling" init="0.05f" min="0.0f" max="10.0f"/>
    <float name="m_MaxSpeedScaling" init="10.0f" min="0.0f" max="10.0f"/>
    <float name="m_SpeedScalingStepSize" init="0.05f" min="0.0f" max="10.0f"/>

    <float name="m_MaxPitch" init="89.5f" min="-90.0f" max="90.0f"/>
    <float name="m_MaxRoll" init="179.5f" min="0.0f" max="180.0f"/>
  </structdef>

  <!-- a marketing camera that smoothly blends between a series of keyframes -->
  <structdef type="camMarketingAToBCameraMetadata" base="camMarketingFreeCameraMetadata">
    <string name="m_SplineCameraRef" type="atHashString"/>
    <u32 name="m_MaxSplineNodes" init="10" min="0" max="255"/>
    <u32 name="m_DefaultNodeTransitionTime" init="1000" min="0" max="600000"/>
  </structdef>

  <!-- a marketing camera that allows free movement via control input and additionally can be mounted to an entity -->
  <structdef type="camMarketingMountedCameraMetadata" base="camMarketingFreeCameraMetadata">
    <float name="m_MaxDistanceToAttachParent" init="10.0f" min="0.0f" max="1000.0f"/>
    <string name="m_SpringMountRef" type="atHashString"/>
  </structdef>

  <!-- a marketing camera that allows free movement via control input and additionally can be mounted to an entity and orbit around it -->
  <structdef type="camMarketingOrbitCameraMetadata" base="camMarketingFreeCameraMetadata">
    <float name="m_MaxDistanceToAttachParent" init="10.0f" min="0.0f" max="1000.0f"/>
    <string name="m_SpringMountRef" type="atHashString"/>
  </structdef>

  <!-- a marketing camera that allows free movement via control input and additionally can be fixed and made to look at the follow entity -->
  <structdef type="camMarketingStickyCameraMetadata" base="camMarketingFreeCameraMetadata">
  </structdef>

  <!-- a basic camera that is available for script control -->
  <structdef type="camScriptedCameraMetadata" base="camBaseCameraMetadata">
    <float name="m_DefaultFov" init="70.0f" min="1.0f" max="130.0f"/>
  </structdef>

  <!-- a special in-game fly camera that is available for script control -->
  <structdef type="camScriptedFlyCameraMetadataInputResponse">
    <float name="m_InputMagPowerFactor" init="4.0f" min="1.0f" max="10.0f"/>
    <float name="m_MaxAcceleration" init="40.0f" min="0.0f" max="10000.0f"/>
    <float name="m_MaxDeceleration" init="200.0f" min="0.0f" max="10000.0f"/>
    <float name="m_MaxSpeed" init="40.0f" min="0.0f" max="10000.0f"/>
  </structdef>

  <structdef type="camScriptedFlyCameraMetadata" base="camScriptedCameraMetadata">
    <struct type="camScriptedFlyCameraMetadataInputResponse" name="m_HorizontalTranslationInputResponse"/>
    <struct type="camScriptedFlyCameraMetadataInputResponse" name="m_VerticalTranslationInputResponse"/>

    <float name="m_DefaultPitch" init="-70.0f" min="-89.0f" max="89.0f"/>
    <float name="m_HalfBoxExtentForHeightMapQuery" init="5.0f" min="0.0f" max="100.0f"/>
    <float name="m_MinDistanceAboveHeightMapToCut" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxHeight" init="500.0f" min="0.0f" max="10000.0f"/>
    <float name="m_CapsuleRadius" init="3.0f" min="0.0f" max="10.0f"/>
    <float name="m_MinHeightAboveWater" init="0.75f" min="0.0f" max="100.0f"/>
    <u32 name="m_TimeToChangeWaterSurfaceState" init="1000" min="0" max="30000"/>
  </structdef>

  <!-- a camera that varies its position and orientation according to a spline function -->
  <structdef type="camBaseSplineCameraMetadata" base="camScriptedCameraMetadata">
  </structdef>

  <structdef type="camRoundedSplineCameraMetadata" base="camBaseSplineCameraMetadata">
  </structdef>

  <structdef type="camSmoothedSplineCameraMetadata" base="camRoundedSplineCameraMetadata">
    <int name="m_NumTnSplineSmoothingPasses" init="10" min="1.0f" max="20"/>
  </structdef>

  <structdef type="camTimedSplineCameraMetadata" base="camSmoothedSplineCameraMetadata">
  </structdef>

  <structdef type="camCustomTimedSplineCameraMetadata" base="camTimedSplineCameraMetadata">
  </structdef>

  <!-- a camera that handles the Switch between player characters at different locations -->
  <structdef type="camSwitchCameraMetadata" base="camBaseCameraMetadata">
    <string name="m_EstablishingShotShakeRef" type="atHashString"/>
    <Vector3 name="m_EstablishingShotRelativeStartOffset" init="0.0f, 0.0f, 20.0f" min="-100.0f" max="100.0f"/>
    <float name="m_Fov" init="45.0f" min="1.0f" max="130.0f"/>
    <float name="m_NearClip" init="1.0f" min="0.01f" max="10.0f"/>
    <float name="m_MotionBlurStrength" init="0.0f" min="0.0f" max="100.0f"/>
    <float name="m_MotionBlurStrengthForPan" init="1.0f" min="0.0f" max="100.0f"/>
    <float name="m_MotionBlurMaxVelocityScaleForPan" init="2.0f" min="0.0f" max="100.0f"/>
    <float name="m_HalfBoxExtentForHeightMapQuery" init="5.0f" min="0.0f" max="100.0f"/>
    <float name="m_MinDistanceAboveHeightMapForFirstCut" init="15.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MaxFovScalingForCutEffect" init="0.9f" min="0.5f" max="2.0f"/>
    <float name="m_DefaultPitch" init="-89.9f" min="-89.9f" max="89.9f"/>
    <float name="m_DesiredPitchWhenLookingUp" init="10.0f" min="-89.9f" max="89.9f"/>
    <float name="m_MaxFovScalingForEstablishingShotInHold" init="1.111f" min="0.5f" max="2.0f"/>
    <float name="m_EstablishingShotShakeAmplitude" init="1.0f" min="0.0f" max="10.0f"/>
    <u32 name="m_CutEffectDuration" init="5000" min="0" max="600000"/>
    <u32 name="m_LookUpDuration" init="1000" min="0" max="600000"/>
    <u32 name="m_LookDownDuration" init="1000" min="0" max="600000"/>
    <u32 name="m_EstablishingShotInHoldDuration" init="1000" min="0" max="600000"/>
    <u32 name="m_EstablishingShotSwoopDuration" init="1000" min="0" max="600000"/>
    <u32 name="m_OutroHoldDuration" init="1000" min="0" max="600000"/>
  </structdef>

  <!-- a camera that plays back camera data recorded for replay -->
  <structdef type="camReplayBaseCameraMetadataInputResponse">
    <float name="m_InputMagPowerFactor" init="4.0f" min="1.0f" max="10.0f"/>
    <float name="m_MaxAcceleration" init="40.0f" min="0.0f" max="10000.0f"/>
    <float name="m_MaxDeceleration" init="200.0f" min="0.0f" max="10000.0f"/>
    <float name="m_MaxSpeed" init="40.0f" min="0.0f" max="10000.0f"/>
  </structdef>
  <structdef type="camReplayBaseCameraMetadataCollisionSettings">
    <float name="m_MinHeightAboveOrBelowWater" init="0.3f" min="0.0f" max="100.0f"/>
    <float name="m_WaterHeightSmoothRate" init="0.4f" min="0.0f" max="1.0f"/>
    <float name="m_MinSafeRadiusReductionWithinPedMoverCapsule" init="0.075f" min="0.0f" max="100.0f"/>
    <float name="m_TestRadiusSpringConstant" init="2.5f" min="0.0f" max="1000.0f"/>
    <float name="m_TestRadiusSpringDampingRatio" init="0.7f" min="0.0f" max="10.0f"/>
    <float name="m_RootPositionSpringConstant" init="2.5f" min="0.0f" max="1000.0f"/>
    <float name="m_RootPositionSpringDampingRatio" init="0.7f" min="0.0f" max="10.0f"/>
    <float name="m_TrackingEntityHeightRatioToAttain" init="1.075f" min="-10.0f" max="10.0f"/>
    <bool name="m_ShouldPushBeyondAttachParentIfClipping" init="false"/>
  </structdef>
  
  <structdef type="camReplayBaseCameraMetadata" base="camBaseCameraMetadata">
    <float name="m_MaxCollisionTestRadius" init="0.24f" min="0.0f" max="10.0f"/>
  </structdef>

  <structdef type="camReplayRecordedCameraMetadata" base="camReplayBaseCameraMetadata">
  </structdef>

  <structdef type="camReplayPresetCameraMetadata" base="camReplayBaseCameraMetadata">
    <Vector3 name="m_DefaultRelativeAttachPosition" init="0.0f, -1.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <Vector3 name="m_RelativeLookAtPosition" init="0.0f, 0.0f, 0.0f" min="-100.0f" max="100.0f"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_DistanceInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_RollInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_ZoomInputResponse"/>
    <float name="m_MinFov" init="5.0f" min="1.0f" max="130.0f"/>
    <float name="m_MaxFov" init="100.0f" min="1.0f" max="130.0f"/>
    <float name="m_DefaultFov" init="45.0f" min="5.0f" max="100.0f"/>
    <float name="m_MinDistanceScalar" init="0.5f" min="0.0f" max="10.0f"/>
    <float name="m_MaxDistanceScalar" init="1.5f" min="0.0f" max="10.0f"/>
    <bool name="m_ShouldAllowRollControl" init="true"/>
    <bool name="m_ShouldUseRigidAttachType" init="true"/>
    <struct type="camReplayBaseCameraMetadataCollisionSettings" name="m_CollisionSettings"/>
    <float name="m_MouseInputScale" init="3.0f" min="1.0f" max="100.0f"/>
    <u32 name="m_RagdollForceSmoothingDurationMs" init="1000" min="0" max ="10000"/>
    <u32 name="m_RagdollForceSmoothingBlendOutDurationMs" init="500" min="0" max ="10000"/>
    <u32 name="m_InVehicleForceSmoothingDurationMs" init="1000" min="0" max ="10000"/>
    <u32 name="m_InVehicleForceSmoothingBlendOutDurationMs" init="500" min="0" max ="10000"/>
    <u32 name="m_OutVehicleForceSmoothingDurationMs" init="1000" min="0" max ="10000"/>
    <u32 name="m_OutVehicleForceSmoothingBlendOutDurationMs" init="500" min="0" max ="10000"/>
  </structdef>
  
  <structdef type="camReplayFreeCameraMetadata" base="camReplayBaseCameraMetadata">
    <float name="m_NearClip" init="0.1f" min="0.05f" max="0.5f"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_HorizontalTranslationInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_VerticalTranslationInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_HeadingPitchInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_RollInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_ZoomInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_LookAtOffsetInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_HorizontalTranslationWithLookAtInputResponse"/>
    <struct type="camReplayBaseCameraMetadataInputResponse" name="m_VerticalTranslationWithLookAtInputResponse"/>
    <struct type="camInterpolatorMetadata" name="m_PhaseInterpolatorIn"/>
    <struct type="camInterpolatorMetadata" name="m_PhaseInterpolatorOut"/>
    <struct type="camInterpolatorMetadata" name="m_PhaseInterpolatorInOut"/>
    <float name="m_MaxPitch" init="89.5f" min="-90.0f" max="90.0f"/>
    <float name="m_MinFov" init="5.0f" min="1.0f" max="130.0f"/>
    <float name="m_MaxFov" init="100.0f" min="1.0f" max="130.0f"/>
    <float name="m_DefaultFov" init="45.0f" min="5.0f" max="100.0f"/>
    <float name="m_CapsuleRadius" init="0.5f" min="0.0f" max="10.0f"/>
    <float name="m_CollisionRootPositionCapsuleRadiusIncrement" init="0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_MinCollisionTestRadius" init="0.1f" min="0.0f" max="10.0f"/>
    <float name="m_TranslationWithLookAtOffsetMouseInputMultiplier" init="3.0f" min="0.10f" max="25.0f"/>
    <float name="m_LookAtOffsetMouseInputMultiplier" init="3.0f" min="0.10f" max="25.0f"/>    
    <float name="m_RotationMouseInputMultiplier" init="3.0f" min="0.10f" max="50.0f"/>    
    <Vector3 name="m_DefaultAttachPositionOffset" init="0.0f, -1.0f, 0.5f" min="-100.0f" max="100.0f"/>
    <struct type="camReplayBaseCameraMetadataCollisionSettings" name="m_CollisionSettings"/>
    <float name="m_MoverCapsuleRadiusOverrideForNonBipeds" init="0.0f" min="0.0f" max="1.0f"/>
    <float name="m_MouseInputScaleForTranslationWithLookAt" init="3.0f" min="0.1f" max="100.0f"/>
    <float name="m_MouseInputScaleForLookAtOffset" init="3.0f" min="0.1f" max="100.0f"/>
    <float name="m_MouseInputScaleForVerticalTranslation" init="3.0f" min="0.1f" max="100.0f"/>
    <float name="m_MouseInputScaleForRotation" init="3.0f" min="0.1f" max="100.0f"/>
    <u32 name="m_IgnoreRollInputAfterResetDuration" init="300" min="0" max ="3000"/>
    <float name="m_CapsuleRadiusBufferForHaltingBlend" init="5.0f" min="0.0f" max="180.0f"/>
    <float name="m_RecordedRollThresholdForRollCorrection" init="0.1f" min="0.0f" max="10.0f"/>
    <float name="m_RollCorrectionSpringConstant" init="0.5f" min="0.0f" max="99999.0f"/>
    <float name="m_RollCorrectionSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_RollCorrectionMinStickInput" init="0.85f" min="0.0f" max="1.0f"/>
    <u32 name="m_MaxStickInputTimeForRollCorrection" init="250" min="0" max ="10000"/>
    <u32 name="m_RagdollForceSmoothingDurationMs" init="1000" min="0" max ="10000"/>
    <u32 name="m_RagdollForceSmoothingBlendOutDurationMs" init="500" min="0" max ="10000"/>
    <u32 name="m_InVehicleForceSmoothingDurationMs" init="1000" min="0" max ="10000"/>
    <u32 name="m_InVehicleForceSmoothingBlendOutDurationMs" init="500" min="0" max ="10000"/>
    <u32 name="m_OutVehicleForceSmoothingDurationMs" init="1000" min="0" max ="10000"/>
    <u32 name="m_OutVehicleForceSmoothingBlendOutDurationMs" init="500" min="0" max ="10000"/>
    <float name="m_LookAtDistanceSpringConstant" init="10.0f" min="0.0f" max="99999.0f"/>
    <float name="m_LookAtDistanceSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
  </structdef>

  <!-- end of camera definitions -->

  <!-- start of shot definitions -->
  <enumdef type="camCinematicInVehicleContextFlags::Flags" generate="bitset">
    <enumval name="CAR"/>
    <enumval name="PLANE"/>
    <enumval name="TRAILER"/>
    <enumval name="QUADBIKE"/>
    <enumval name="HELI"/>
    <enumval name="AUTOGYRO"/>
    <enumval name="BIKE"/>
    <enumval name="BICYCLE"/>
    <enumval name="BOAT"/>
    <enumval name="TRAIN"/>
    <enumval name="SUBMARINE"/>
    <enumval name="ALL"/>
  </enumdef>

  <structdef type="camCinematicShotMetadata" base="camBaseObjectMetadata">
    <string name="m_CameraRef" type="atHashString"/>
    <Vector2 name="m_ShotDurationLimits" init="2000.0f, 6000.0f" min="0.0f" max="100000.0f"/>
    <bool name="m_ShouldUseDuration" init="true" description="if true the duration should be respected"/>
    <bitset name="m_VehicleTypes" type="fixed" numBits="12" values="camCinematicInVehicleContextFlags::Flags"/>
    <bool name="m_IsPlayerAttachEntity" init="false" description="is true the camer is attached relative to the player or players car"/>
    <bool name="m_IsPlayerLookAtEntity" init="true" description="is true the camer is attached relative to the player or players car"/>
    <float name="m_InitialSlowMoSpeed" init="-1.0f" description="InitialSlowMoValue"/>
    <bool name="m_ShouldTerminateAtEndOfDuration" init="false" description="Force the shot to terminate at the end of the duration"/>
    <bool name="m_IsVaildUnderWater" init="false" description="check if the shot is valid for underwater"/>
    <float name="m_MinShotTimeScalarForAbortingShots" init="1.0f" min="0.0f" max="10000.0f"/>
    <bool name="m_CanBlendOutSlowMotionOnShotTermination" init="false" description="when a shot terminates can teh slowmotion be terminated"/>
    <u32 name="m_SlowMotionBlendOutDuration" init="1000" min="0" max="100000"/>
  </structdef>
  
  <structdef type="camCinematicHeliTrackingShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_ShouldUseLookAhead" init="false"/>
  </structdef>

  <structdef type="camCinematicVehicleOrbitShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicVehicleLowOrbitShotMetadata" base="camCinematicShotMetadata">
  </structdef>
  
  <structdef type="camCinematicVehicleBonnetShotMetadata" base="camCinematicShotMetadata">
    <u32 name="m_MinTimeToCreateAfterCameraFailure" init="1000" min="0" max="100000"/>
    <u32 name="m_BlendTimeFromFirstPersonShooterCamera" init="1000" min="0" max="15000"/>
    <string name="m_HangingOnCameraRef" type="atHashString"/>
  </structdef>

  <structdef type="camCinematicCameraManShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_ShouldUseLookAhead" init="false"/>
  </structdef>
  
  <structdef type="camCinematicCraningCameraManShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_ShouldUseLookAhead" init="false"/>
    <bool name="m_ShouldUseLookAtDampingHelpers" init="false"/>
    <float name="m_MaxTotalHeightChange" init="10.0f" min="0.0f" max="1000.0f"/>
    <bool name="m_ShouldOnlyTriggerIfVehicleMovingForward" init="true"/>
    <u32 name="m_MaxTimeSinceVehicleMovedForward" init="1000" min="0" max="10000"/>
    <float name="m_ProbabilityToUseVehicleRelativeStartPosition" init="0.5f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_VehicleRelativePositionMultiplier" init="0.0f, 0.0f" min="-10.0f" max="10.0f"/>
    <float name="m_VehicleRelativePositionHeight" init="4.0f" min="0.0f" max="100.0f"/>
    <float name="m_ScanRadius" init="7.0f" min="0.0f" max="1000.0f"/>
  </structdef>

  <structdef type="camCinematicVehiclePartShotMetadata" base="camCinematicShotMetadata">
    <u32 name="m_MaxInAirTimer" init="500" min="0" max="100000"/> 
  </structdef>

  <structdef type="camCinematicTrainStationShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicTrainRoofMountedShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicTrainTrackShotMetadata" base="camCinematicShotMetadata">
  </structdef>
  
  <structdef type="camCinematicInTrainShotMetadata" base="camCinematicShotMetadata">
  </structdef>
  
  <structdef type="camCinematicTrainPassengerShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicPoliceCarMountedShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_LimitAttachParentRelativePitchAndHeading" init="false"/>
    <bool name="m_ShouldTerminateForPitchAndHeading" init="false"/>
    <Vector2 name="m_AttachParentRelativePitch" init="-20.0f, 20.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_AttachParentRelativeHeading" init="-110.0f, 110.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_InitialRelativePitchLimits" init="-100.0f, 100.0f" min="-180.0f" max="180.0f"/>
    <Vector2 name="m_InitialRelativeHeadingLimits" init="-100.0f, 100.0f" min="-180.0f" max="180.0f"/>
    <string name="m_InVehicleLookAtDampingRef" type="atHashString"/>
    <string name="m_OnFootLookAtDampingRef" type="atHashString"/>
    <bool name="m_ShouldUseLookAhead" init="false"/>
  </structdef>

  <structdef type="camCinematicPoliceHeliMountedShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_ShouldUseLookAhead" init="false"/>
  </structdef>

  <structdef type="camCinematicPoliceInCoverShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_ShouldUseLookAhead" init="false"/>
  </structdef>

  <structdef type="camCinematicPoliceExitVehicleShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_ShouldUseLookAhead" init="false"/>
  </structdef>

  <structdef type="camCinematicPoliceRoadBlockShotMetadata" base="camCinematicShotMetadata">
    <float name="m_MinDistanceToTrigger" init="3.0f" min="0.0f" max="10000.0f"/>
    <float name="m_MaxDistanceToTrigger" init="50.0f" min="0.0f" max="10000.0f"/>
    <bool name="m_ShouldUseLookAhead" init="false"/>
  </structdef>

  <structdef type="camCinematicOnFootIdleShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicOnFootFirstPersonIdleShotMetadata" base="camCinematicShotMetadata">
    <array name="m_Cameras" type="atArray">
      <string type="atHashString" />
    </array>
  </structdef>

  <structdef type="camCinematicParachuteHeliShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicParachuteCameraManShotMetadata" base="camCinematicShotMetadata">
  </structdef>
 
  <structdef type="camCinematicStuntJumpShotMetadata" base="camCinematicShotMetadata">
  </structdef>
  
  <structdef type="camCinematicOnFootMeleeShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicMeleeShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicOnFootAssistedAimingKillShotMetadata" base="camCinematicShotMetadata">
    <u32 name="m_MinTimeBetweenPedKillShots" init="6000" min="0" max="120000"/>
    <float name="m_MaxTargetHealthRatioForKillShot" init="0.15f" min="0.0f" max="1.0f"/>
  </structdef>

  <structdef type="camCinematicOnFootAssistedAimingReactionShotMetadata" base="camCinematicShotMetadata">
    <u32 name="m_MinTimeBetweenReactionShots" init="10000" min="0" max="120000"/>
  </structdef>
  
  <structdef type="camCinematicBustedShotMetadata" base="camCinematicShotMetadata">
    <float name="m_MaxArrestingPedSpeedToActivate" init="0.1f" min="0.0f" max="100.0f"/>
  </structdef>		

  <structdef type="camCinematicVehicleGroupShotMetadata" base="camCinematicShotMetadata">
    <float name="m_GroupSearchRadius" init="10.0f" min="1.0f" max="100.0f"/>
    <float name="m_DotOfEntityFrontAndPlayer" init="0.0f" min="-1.0f" max="1.0f"/>
    <u32 name="m_MinNumberForValidGroup" init="2" min="0" max="100"/>
  </structdef>

  <structdef type="camCinematicMissileKillShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicWaterCrashShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicFallFromHeliShotMetadata" base="camCinematicShotMetadata">
  </structdef>
  
  <structdef type="camCinematicOnFootSpectatingShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicVehicleConvertibleRoofShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicScriptRaceCheckPointShotMetadata" base="camCinematicShotMetadata">
  </structdef>

  <structdef type="camCinematicInVehicleCrashShotMetadata" base="camCinematicShotMetadata">
    <bool name="m_ShouldUseLookAhead" init="false"/>
    <float name="m_MinRoll" init="100.0f" min="0.0f" max="10000.0f"/>
    <u32 name="m_TimeToTerminate" init="1000" min="0" max="10000"/>
    <u32 name="m_TimeToTerminateForWheelsOnTheGround" init="250" min="0" max="10000"/>
    <float name="m_MinVelocity" init="15.0f" min="0.0f" max="10000.0f"/>
    <float name="m_MinVelocityToTerminate" init="5.0f" min="0.0f" max="10000.0f"/>
    <u32 name="m_MinTimeToCreateAfterShotFailure" init="4000" min="0" max="10000"/>
  </structdef>
  
  <!-- end of shot definitions -->

  <!-- start of context definitions -->
  <enumdef type="eShotSelectionInputType">
    <enumval name="SELECTION_INPUT_NONE"/>
    <enumval name="SELECTION_INPUT_UP"/>
    <enumval name="SELECTION_INPUT_DOWN"/>
    <enumval name="SELECTION_INPUT_LEFT"/>
    <enumval name="SELECTION_INPUT_RIGHT"/>
  </enumdef>

  <structdef type="camPreferredShotSelectionType">
    <string name="m_Shot" type="atHashString"/>
    <enum name="m_InputType" type="eShotSelectionInputType" init="SELECTION_INPUT_NONE"/>
  </structdef>

  <structdef type="camCinematicShots">
    <string name="m_Shot" type="atHashString"/>
    <u32 name="m_Priority" init="0" min="0" max="100"/>
    <float name="m_ProbabilityWeighting" init="1.0f" min="0.0f" max="10000.0f"/>
  </structdef>
  
  <structdef type="camCinematicContextMetadata" base="camBaseObjectMetadata">
    <array name="m_Shots" type="atArray">
      <struct type="camCinematicShots"/>
    </array>
    <array name="m_PreferredShotToInputSelection" type="atArray">
      <struct type="camPreferredShotSelectionType"/>
    </array>
    <bool name="m_IsValidToHaveNoCameras" init="false" description="Some contexts can run but dont need to create a camera"/>
    <bool name="m_CanAbortOtherContexts" init="false" description="Can this context abort other contexts"/>
    <bool name="m_CanSelectShotFromControlInput" init="true" description="Does this context respond to cinematic control input"/>
    <bool name="m_CanSelectSlowMotionFromControlInput" init="true" description="Does this context respond to cinematic control input"/>
    <bool name="m_CanActivateUsingQuickToggle" init="false" description="Can the context be activated using quick toggle"/>
    <bool name="m_ShouldRespectShotDurations" init="true" description="Should the context respect the shot durations"/>
    <float name="m_MinShotTimeScalarForAbortingContexts" init="1.0f" min="0.0f" max="10000.0f"/>
    <bool name="m_CanUseGameCameraAsFallBack" init="false" description="Can the context be activated using quick toggle"/>
    <u32 name="m_GameCameraFallBackDuration" init="1500" min="0" max="10000"/>
  </structdef>
  
  <structdef type="camCinematicInVehicleContextMetadata" base="camCinematicContextMetadata">  
  </structdef>

  <structdef type="camCinematicInVehicleWantedContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicOnFootIdleContextMetadata" base="camCinematicContextMetadata">
    <u32 name="m_MinIdleTimeToActivateIdleCamera" init="30000" min="0" max="120000"/>
    <float name="m_MaxPedSpeedToActivateIdleCamera" init="0.1f" min="0.0f" max="100.0f"/>
  </structdef>

  <structdef type="camCinematicStuntJumpContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicParachuteContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicOnFootMeleeContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicInVehicleFirstPersonContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicInVehicleOverriddenFirstPersonContextMetadata" base="camCinematicContextMetadata">
  </structdef>

 
  <structdef type="camCinematicOnFootAssistedAimingContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicBustedContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicMissileKillContextMetadata" base="camCinematicContextMetadata">
    <float name="MissileCollisionRadius" init="5.0f" min="0.0f" max="100.0f"/>
  </structdef>

  <structdef type="camCinematicWaterCrashContextMetadata" base="camCinematicContextMetadata">
    <u32 name="m_ContextDuration" init="5000" min="100" max="60000"/>
    <float name="m_PitchLimit" init="60.0f" min="0.0f" max="89.0f"/>
    <float name="m_fDefaultSubmergedLimit" init="0.50f" min="0.0f" max="1.0f"/>
    <float name="m_fHeliSubmergedLimit" init="0.30f" min="0.0f" max="1.0f"/>
    <float name="m_fPlaneSubmergedLimit" init="0.75f" min="0.0f" max="1.0f"/>
  </structdef>

  <structdef type="camCinematicFallFromHeliContextMetadata" base="camCinematicContextMetadata">
  </structdef>
  
  <structdef type="camCinematicOnFootSpectatingContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicInVehicleConvertibleRoofContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicScriptContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicInTrainContextMetadata" base="camCinematicInVehicleContextMetadata">
  </structdef>

  <structdef type="camCinematicInTrainAtStationContextMetadata" base="camCinematicInTrainContextMetadata">
    <u32 name="m_TimeApproachingStation" init="7000" min="0" max="100000"/>
    <u32 name="m_TimeLeavingStation" init="3000" min="0" max="100000"/>
  </structdef>

  <structdef type="camCinematicSpectatorNewsChannelContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicScriptedRaceCheckPointContextMetadata" base="camCinematicContextMetadata">
    <u32 name="m_ValidRegistartionDuration" init="1000" min="0" max="100000"/>
  </structdef>

  <structdef type="camCinematicInVehicleMultiplayerPassengerContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicScriptedMissionCreatorFailContextMetadata" base="camCinematicContextMetadata">
  </structdef>

  <structdef type="camCinematicInVehicleCrashContextMetadata" base="camCinematicContextMetadata">
  </structdef>


  <!-- end of context definitions -->
  
  <!-- start of director definitions -->

  <!--  abstract base director type - all other director types inherit from this -->
  <structdef type="camBaseDirectorMetadata" base="camBaseObjectMetadata" constructable="false">
    <bool name="m_CanBePaused" init="true" description="if true this director can be paused"/>
  </structdef>

  <!--  the director responsible for cinematic cameras -->
  <structdef type="camCinematicDirectorMetadataAssistedAimingSettings">
    
    <float name="m_DefaultSlowMotionScaling" init="0.6f" min="0.0f" max="1.0f"/>
    <float name="m_SuperSlowMotionScaling" init="0.2f" min="0.0f" max="1.0f"/>
    <float name="m_SlowMotionScalingSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <string name="m_PedKillShotCameraRef" type="atHashString"/>
  
    <u32 name="m_PedKillShotCameraDuration" init="1250" min="0" max="120000"/>

    <string name="m_ReactionShotCameraRef" type="atHashString"/>
    
    <u32 name="m_ReactionShotCameraDuration" init="1250" min="0" max="120000"/>
  </structdef>

  <structdef type="camCinematicDirectorMetadata" base="camBaseDirectorMetadata">        
    <u32 name="m_DurationBeforeTriggeringNetworkCinematicVehicleCam" init="10000" min="0" max="100000"/>

    <float name="m_SlowMotionScalingDelta" init="0.05f" min="0.0f" max="1.0f"/>
    <float name="m_MinSlowMotionScaling" init="0.2f" min="0.0f" max="1.0f"/>
    <float name="m_DefaultSlowMotionScaling" init="0.35f" min="0.0f" max="10.0f"/>
    <string name="m_SlowMoEnvelopeRef" type="atHashString"/>
    
    <float name="m_ControlStickTriggerThreshold" init="114.3f" min="0.0f" max="127.0f"/>
    <float name="m_ControlStickDeadZone" init="25.4f" min="0.0f" max="127.0f"/>
    <float name="m_MinAngleToRespectTheLineOfAction" init="20.0f" min="0.0f" max="360.0f"/>
    <u32 name="m_QuickToggleTimeThreshold" init="150" min="0" max="120000"/>
    <string name="m_VehicleImpactHeadingShakeRef" type="atHashString"/>
    <string name="m_VehicleImpactPitchShakeRef" type="atHashString"/>
    <float name="m_VehicleImpactShakeMinDamage" init="5.0f" min="0.0f" max="700.0f"/>
    <float name="m_VehicleImpactShakeMaxDamage" init="100.0f" min="10.0f" max="1000.0f"/>
    <float name="m_VehicleImpactShakeMaxAmplitude" init="1.0f" min="0.0f" max="10.0f"/>
    <array name="m_CinematicContexts" type="atArray">
      <pointer type="camCinematicContextMetadata" policy="owner"/>
    </array>
    <u32 name="m_HoldDurationOfCinematicShotForEarlyTermination" init="250" min="0" max="120000"/>
  </structdef>

  <!--  the director responsible for anim scene cameras -->
  <structdef type="camAnimSceneDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_DefaultAnimatedCameraRef" type="atHashString"/>
  </structdef>

  <!--  the director responsible for animated cutscene cameras -->
  <structdef type="camCutsceneDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_AnimatedCameraRef" type="atHashString"/>
  </structdef>

  <!--  the director responsible for debug cameras -->
  <structdef type="camDebugDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_FreeCameraRef" type="atHashString"/>
  </structdef>

  <!--  the director responsible for gameplay cameras -->
  <structdef type="camGameplayDirectorMetadataExplosionShakeSettings">
    <string name="m_ShakeRef" type="atHashString"/>
    <Vector2 name="m_DistanceLimits" init="5.0f, 200.0f" min="0.0f" max="500.0f"/>
    <u32 name="m_MaxInstances" init="2" min="0" max="10"/>
    <u32 name="m_RumbleDuration" init="300" min="0" max="10000"/>
    <u32 name="m_RumbleDurationForMp" init="400" min="0" max="10000"/>
    <float name="m_RumbleAmplitudeScaling" init="0.5f"  min="0.0f" max="1.0f"/>
    <float name="m_RumbleAmplitudeScalingForMp" init="0.75f"  min="0.0f" max="1.0f"/>
    <float name="m_MaxRumbleAmplitude" init="1.0f"  min="0.0f" max="100.0f"/>
    <float name="m_IdealFollowPedDistance" init="5.0f" min="0.0f" max="500.0f"/>
    <float name="m_ShakeAmplitudeScaling" init="1.0f"  min="0.0f" max="100.0f"/>
  </structdef>
  <structdef type="camGameplayDirectorMetadataVehicleCustomSettings">
    <string name="m_ModelName" type="atHashString"/>
    <string name="m_SettingsRef" type="atHashString"/>
  </structdef>

  <structdef type="camGameplayDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_FollowPedCameraRef" type="atHashString"/>
    <string name="m_FollowVehicleCameraRef" type="atHashString"/>
    <string name="m_FirstPersonPedAimCameraRef" type="atHashString"/>
    <string name="m_FirstPersonShooterCameraRef" type="atHashString"/>
    <string name="m_FirstPersonHeadTrackingAimCameraRef" type="atHashString"/>
    <string name="m_ThirdPersonPedAimCameraRef" type="atHashString"/>
    <string name="m_ThirdPersonVehicleAimCameraRef" type="atHashString"/>
    <string name="m_ShortRotationInSwitchHelperRef" type="atHashString"/>
    <string name="m_ShortRotationOutSwitchHelperRef" type="atHashString"/>
    <string name="m_ShortTranslationInSwitchHelperRef" type="atHashString"/>
    <string name="m_ShortTranslationOutSwitchHelperRef" type="atHashString"/>
    <string name="m_ShortZoomToHeadInSwitchHelperRef" type="atHashString"/>
    <string name="m_ShortZoomToHeadOutSwitchHelperRef" type="atHashString"/>
    <string name="m_ShortZoomInOutInSwitchHelperRef" type="atHashString"/>
    <string name="m_ShortZoomInOutOutSwitchHelperRef" type="atHashString"/>
    <string name="m_MediumInSwitchHelperRef" type="atHashString"/>
    <string name="m_MediumOutSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonMediumInSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonMediumOutSwitchHelperRef" type="atHashString"/>
    <string name="m_MediumFallBackInSwitchHelperRef" type="atHashString"/>
    <string name="m_MediumFallBackOutSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonMediumFallBackInSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonMediumFallBackOutSwitchHelperRef" type="atHashString"/>
    <string name="m_LongInSwitchHelperRef" type="atHashString"/>
    <string name="m_LongOutSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonLongInSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonLongOutSwitchHelperRef" type="atHashString"/>
    <string name="m_LongFallBackInSwitchHelperRef" type="atHashString"/>
    <string name="m_LongFallBackOutSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonLongFallBackInSwitchHelperRef" type="atHashString"/>
    <string name="m_FirstPersonLongFallBackOutSwitchHelperRef" type="atHashString"/>
    <string name="m_DeathFailIntroEffectShakeRef" type="atHashString"/>
    <string name="m_DeathFailOutroEffectShakeRef" type="atHashString"/>

    <string name="m_FollowVehicleParachuteCameraRef" type="atHashString"/>
    <u32 name="m_FollowVehicleParachuteCameraInterpolation" init="1000" min="0" max="60000"/>

    <float name="m_MaxDistanceBetweenPedsForShortRotation" init="10.0f" min="0.0f" max="100.0f"/>
    <float name="m_MinAbsHeadingDeltaBetweenPedsForShortRotation" init="135.0f" min="0.0f" max="180.0f"/>
    <float name="m_MaxAbsDeltaOldPedToPedDeltaHeadingForShortRotation" init="45.0f" min="0.0f" max="180.0f"/>
    <float name="m_MaxAbsHeadingDeltaBetweenPedsForShortTranslation" init="45.0f" min="0.0f" max="180.0f"/>
    <float name="m_MaxAbsDeltaOldPedToPedDeltaPerpHeadingForShortTranslation" init="45.0f" min="0.0f" max="90.0f"/>
    <float name="m_MaxAbsDeltaOldPedToPedDeltaPerpPitchForShortTranslation" init="45.0f" min="0.0f" max="90.0f"/>

    <float name="m_MinSafeRadiusReductionWithinPedMoverCapsuleForSwitchValidation" init="0.075f" min="0.0f" max="100.0f"/>
    <float name="m_CapsuleLengthForSwitchValidation" init="2.0f" min="0.0f" max="100.0f"/>

    <float name="m_FollowPedCrouchingBlendSpringConstant" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FollowPedCrouchingBlendSpringConstantForCoverPeekingOrFiring" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FollowPedCoverFacingDirectionScalingSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FollowPedCoverToCoverMinPhaseForFacingDirectionBlend" init="0.2f" min="0.0f" max="1.0f"/>
    <float name="m_FollowPedCoverMovementModifierBlendSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_FollowPedCoverNormalSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <u32 name="m_FollowPedCoverAimingHoldDuration" init="100" min="0" max="60000"/>

    <Vector2 name="m_ReverbSizeLimitsForTightSpaceLevel" init="0.0f, 1.0f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_TightSpaceLevelAtReverbSizeLimits" init="1.0f, 0.333f" min="-10.0f" max="10.0f"/>
    <Vector2 name="m_ReverbWetLimitsForTightSpaceLevel" init="0.333f, 1.0f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_TightSpaceLevelScalingAtReverbWetLimits" init="0.0f, 1.0f" min="-10.0f" max="10.0f"/>

    <u32 name="m_InterpolateDurationForSwitchCameraTransition" init="1000" min="0" max="60000"/>
    <u32 name="m_InterpolateDurationForMoveBlendTransition" init="1000" min="0" max="60000"/>
    <float name="m_MaxOrientationDeltaToInterpolateOnMoveBlendTransition" init="90.0f" min="0.0f" max="180.0f"/>

    <float name="m_MaxDistanceToInterpolateOnVehicleEntryExit" init="10.0f" min="0.0f" max="100.0f"/>
    <u32 name="m_FollowVehicleInterpolateDuration" init="2000" min="0" max="60000"/>
    <float name="m_MinMoveSpeedForBailOutOfVehicle" init="3.0f" min="0.0f" max="100.0f"/>
    <float name="m_MaxDistanceFromVehicleAabbForRagdollExit" init="2.0f" min="0.0f" max="100.0f"/>
    <u32 name="m_FollowVehicleInterpolateDurationForRagdollExit" init="1000" min="0" max="60000"/>
    <u32 name="m_FollowVehicleInterpolateDurationForBailOut" init="1000" min="0" max="60000"/>
    <float name="m_AngleToRotateAwayFromVehicleForClimbDownExit" init="30.0f" min="0.0f" max="180.0f"/>

    <array name="m_VehicleCustomSettingsList" type="atArray">
      <struct type="camGameplayDirectorMetadataVehicleCustomSettings"/>
    </array>
    
    <float name="m_MaxHeadingDeltaToInterpolateIntoAim" init="90.0f" min="0.0f" max="360.0f"/>
    <float name="m_MinHeadingDeltaForAimWeaponInterpolationSmoothing" init="18.0f" min="0.0f" max="360.0f"/>
    <float name="m_MaxHeadingDeltaForAimWeaponInterpolationSmoothing" init="90.0f" min="0.0f" max="360.0f"/>
    <u32 name="m_MinAimWeaponInterpolateInDuration" init="400" min="0" max="60000"/>
    <u32 name="m_MaxAimWeaponInterpolateInDuration" init="750" min="0" max="60000"/>
    <u32 name="m_MinAimWeaponInterpolateInDurationInCover" init="750" min="0" max="60000"/>
    <u32 name="m_MaxAimWeaponInterpolateInDurationInCover" init="750" min="0" max="60000"/>
    <u32 name="m_MinAimWeaponInterpolateInDurationDuringVehicleExit" init="300" min="0" max="60000"/>
    <u32 name="m_MaxAimWeaponInterpolateInDurationDuringVehicleExit" init="300" min="0" max="60000"/>
    <u32 name="m_MinAimWeaponInterpolateInDurationFreeAimNew" init="250" min="0" max="60000"/>
    <u32 name="m_MaxAimWeaponInterpolateInDurationFreeAimNew" init="300" min="0" max="60000"/>
    <u32 name="m_AimWeaponInterpolateOutDuration" init="600" min="0" max="60000"/>
    <u32 name="m_AimWeaponInterpolateOutDurationFreeAimNew" init="600" min="0" max="60000"/>
    <u32 name="m_AssistedAimingInterpolateOutDuration" init="1500" min="0" max="60000"/>
    <u32 name="m_MeleeAimingInterpolateOutDuration" init="2000" min="0" max="60000"/>
    <float name="m_AimWeaponInterpolationMotionBlurStrength" init="0.05f" min="0.0f" max="1.0f"/>
    <float name="m_AimWeaponInterpolationSourceLookAroundDecelerationScaling" init="2.0f" min="0.0f" max="1000.0f"/>

    <bool name="m_ShouldAlwaysAllowCustomVehicleDriveByCameras" init="true"/>

    <float name="m_MaxHeadingDeltaForCoverEntryAlignment" init="45.0f" min="0.0f" max="180.0f"/>

    <string name="m_KillEffectShakeRef" type="atHashString"/>
    <u32 name="m_MaxKillEffectShakeInstances" init="2" min="1" max="5"/>
    <u32 name="m_MaxRecoilShakeInstances" init="2" min="1" max="5"/>
    <Vector2 name="m_RecoilShakeFirstPersonShootingAbilityLimits" init="0.0f, 100.0f" min="0.0f" max="100.0f"/>
    <Vector2 name="m_RecoilShakeAmplitudeScalingAtFirstPersonShootingAbilityLimits" init="2.5f, 0.5f" min="0.0f" max="100.0f"/>
    <Vector2 name="m_RecoilShakeAccuracyLimits" init="0.32f, 0.64f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_RecoilShakeAmplitudeScalingAtAccuracyLimits" init="2.0f, 1.0f" min="0.0f" max="100.0f"/>
    <Vector2 name="m_AccuracyOffsetShakeAccuracyLimits" init="0.0f, 1.0f" min="0.0f" max="1.0f"/>
    <Vector2 name="m_AccuracyOffsetShakeAmplitudeScalingAtAccuracyLimits" init="1.0f, 0.0f" min="0.0f" max="100.0f"/>
    <float name="m_AccuracyOffsetAmplitudeBlendInSpringConstant" init="20.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AccuracyOffsetAmplitudeBlendOutSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_AccuracyOffsetAmplitudeSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>

    <float name="m_AimingLosProbeLengthForDimmingReticule" init="0.25f" min="0.0f" max="10.0f"/>

    <u32 name="m_VehicleAimInterpolateInDuration" init="400" min="0" max="60000"/>
    <u32 name="m_VehicleAimInterpolateOutDuration" init="800" min="0" max="60000"/>

    <string name="m_VehicleImpactShakeRef" type="atHashString"/>
    <float name="m_VehicleImpactShakeMinDamage" init="5.0f" min="0.0f" max="700.0f"/>
    <float name="m_VehicleImpactShakeMaxDamage" init="100.0f" min="10.0f" max="1000.0f"/>
    <float name="m_VehicleImpactShakeMaxAmplitude" init="1.0f" min="0.0f" max="10.0f"/>

    <string name="m_EnduranceDamageShakeRef" type="atHashString"/>
    <float name="m_EnduranceShakeMinDamage" init="2.0f" min="0.0f" max="9999.0f"/>
    <float name="m_EnduranceShakeMaxDamage" init="2.0f" min="0.0f" max="9999.0f"/>
    <float name="m_EnduranceShakeMaxAmplitude" init="1.0f" min="0.0f" max="10.0f"/>

    <float name="m_DetachTrailerBlendStartDistance" init="2.0f" min="0.0f" max="50.0f"/>
    <float name="m_DetachTrailerBlendEndDistance" init="5.0f" min="0.0f" max="75.0f"/>
    <float name="m_DetachTowedVehicleBlendStartDistance" init="0.0f" min="0.0f" max="50.0f"/>
    <float name="m_DetachTowedVehicleBlendEndDistance" init="5.0f" min="0.0f" max="75.0f"/>
    <float name="m_TowedVehicleBlendInSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_TowedVehicleBlendInSpringDampingRatio" init="0.9f" min="0.0f" max="10.0f"/>

    <float name="m_SwitchThirdToFirstPersonFovDelta" init="-3.0f" min="-25.0f" max="25.0f"/>
    <float name="m_ThirdToFirstPersonFovDelta" init="3.0f" min="-25.0f" max="25.0f"/>
    <float name="m_FirstToThirdPersonFovDelta" init="-3.0f" min="-25.0f" max="25.0f"/>
    <u32 name="m_SwitchThirdToFirstPersonDuration" init="1000" min="0" max="5000"/>
    <u32 name="m_ThirdToFirstPersonDuration" init="500" min="0" max="5000"/>
    <u32 name="m_FirstToThirdPersonDuration" init="500" min="0" max="5000"/>

    <u32 name="m_QuickToggleTimeThreshold" init="150" min="0" max="120000"/>
    <u32 name="m_MissileHintBlendInDuration" init="1500" min="0" max="120000"/>
    <u32 name="m_MissileHintBlendOutDuration" init="1500" min="0" max="120000"/>
    <u32 name="m_MissileHintKillDuration" init="1500" min="0" max="120000"/>
    <u32 name="m_MissileHintMissedDuration" init="500" min="0" max="120000"/>
    <string name="m_MissileHintHelperRef" type="atHashString"/>

    <string name="m_RagdollLookAtEnvelope" type="atHashString"/>
    
    <struct type="camGameplayDirectorMetadataExplosionShakeSettings" name="m_ExplosionShakeSettings"/>
  </structdef>

  <!--  the director responsible for marketing cameras -->
  <structdef type="camMarketingDirectorMetadataMode">
    <string name="m_CameraRef" type="atHashString"/>
    <string name="m_TextLabel" type="member" size="16"/>
  </structdef>

  <structdef type="camMarketingDirectorMetadata" base="camBaseDirectorMetadata">
    <array name="m_Modes" type="atArray">
      <struct type="camMarketingDirectorMetadataMode"/>
    </array>
  </structdef>

  <!--  the director responsible for scripted cameras -->
  <structdef type="camScriptDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_ScriptedCameraRef" type="atHashString"/>
    <string name="m_SplineCameraRef" type="atHashString"/>

    <u32 name="m_DefaultInterpolateDuration" init="1000" min="0" max="60000"/>
  </structdef>

  <!--  the director responsible for Switch cameras -->
  <structdef type="camSwitchDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_DefaultSwitchCameraRef" type="atHashString"/>
  </structdef>

  <!--  the director responsible for Code Create Synced Scne cameras -->
  <structdef type="camSyncedSceneDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_DefaultAnimatedCameraRef" type="atHashString"/>
  </structdef>
  
  <!--  the director responsible for replay cameras -->
  <structdef type="camReplayDirectorMetadata" base="camBaseDirectorMetadata">
    <string name="m_PresetFrontRef" type="atHashString"/>
    <string name="m_PresetRearRef" type="atHashString"/>
    <string name="m_PresetRightRef" type="atHashString"/>
    <string name="m_PresetLeftRef" type="atHashString"/>
    <string name="m_PresetOverheadRef" type="atHashString"/>
    <string name="m_ReplayRecordedCameraRef" type="atHashString"/>
    <string name="m_ReplayFreeCameraRef" type="atHashString"/>
    <float name="m_MaxTargetDistanceFromCenter" init="30.0f" min="5.0f" max="500.0f"/>
    <float name="m_MaxCameraDistanceFromPlayer" init="30.0f" min="0.0f" max="500.0f"/>
    <float name="m_MaxCameraDistanceReductionForEditMode" init="0.5f" min="0.0f" max="500.0f"/>
    <Vector3 name="m_ShakeIntensityToAmplitudeCurve" init="0.076f, -0.29f, 0.3f" min="-10.0f" max="10.0f"/>
    <string name="m_ReplayDofSettingsRef" type="atHashString"/>
    <float name="m_MinFNumberOfLens" init="22.0f" min="0.5f" max="256.0f"/>
    <float name="m_MaxFNumberOfLens" init="0.5f" min="0.5f" max="256.0f"/>
    <float name="m_FNumberExponent" init="1.0f" min="0.000001f" max="10000.0f"/>
    <float name="m_MinFocalLengthMultiplier" init="2.0f" min="0.1f" max="10.0f"/>
    <float name="m_MaxFocalLengthMultiplier" init="2.5f" min="0.1f" max="10.0f"/>
    <string name="m_ReplayAirTurbulenceShakeRef" type="atHashString"/>
    <string name="m_ReplayGroundVibrationShakeRef" type="atHashString"/>
    <string name="m_ReplayDrunkShakeRef" type="atHashString"/>
    <string name="m_ReplayHandheldShakeRef" type="atHashString"/>
    <string name="m_ReplayExplosionShakeRef" type="atHashString"/>
    <float name="m_ShakeAmplitudeSpringConstant" init="30.0f" min="0.0f" max="1000.0f"/>
    <float name="m_ShakeAmplitudeSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_FullyBlendedSmoothingSpringConstant" init="10.0f" min="0.0f" max="1000.0f"/>
    <float name="m_MinBlendedSmoothingSpringConstant" init="100.0f" min="0.0f" max="1000.0f"/>
    <float name="m_SmoothingSpringDampingRatio" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="m_SmoothingBlendInPhase" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_SmoothingBlendOutPhase" init="0.5f" min="0.0f" max="1.0f"/>
    <float name="m_FadeSmoothingBlendOutBegin" init="0.8f" min="0.0f" max="1.0f"/>
  </structdef>

  <!-- end of director definitions -->

  <!--  the store for all camera object metadata -->
  <structdef type="camMetadataStore">
    <array name="m_MetadataList" type="atArray">
      <pointer type="camBaseObjectMetadata" policy="owner"/>
    </array>
    <array name="m_DirectorList" type="atArray">
      <pointer type="camBaseDirectorMetadata" policy="owner"/>
    </array>
  </structdef>

  <structdef type="UnapprovedCameraLists">
    <array name="m_UnapprovedAnimatedCameras" type="atArray">
      <string type="atHashValue" />
    </array>
  </structdef>

</ParserSchema>
