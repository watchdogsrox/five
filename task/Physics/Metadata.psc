<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <const name="RAGDOLL_NUM_COMPONENTS" value="21"/>
  <const name="RAGDOLL_NUM_JOINTS" value="20"/>

  <!-- NM parameter tuning system -->

  <structdef type="CNmParameter">
    <string name="m_Name" type="atFinalHashString"></string>
  </structdef>

  <structdef type="CNmParameterResetMessage" base="CNmParameter">
  </structdef>

  <structdef type="CNmParameterFloat" base="CNmParameter">
    <float name="m_Value" />
  </structdef>

  <structdef type="CNmParameterRandomFloat" base="CNmParameter">
    <float name="m_Min" />
    <float name="m_Max" />
  </structdef>

  <structdef type="CNmParameterInt" base="CNmParameter">
    <int name="m_Value" />
  </structdef>

  <structdef type="CNmParameterRandomInt" base="CNmParameter">
    <int name="m_Min" />
    <int name="m_Max" />
  </structdef>

  <structdef type="CNmParameterBool" base="CNmParameter">
    <bool name="m_Value" />
  </structdef>

  <structdef type="CNmParameterString" base="CNmParameter">
    <string name="m_Value" type="atFinalHashString"></string>
  </structdef>

  <structdef type="CNmParameterVector" base="CNmParameter">
    <Vector3 name="m_Value" />
  </structdef>

  <structdef type="CNmMessage">
    <string name="m_Name" type="atFinalHashString"></string>
    <array name="m_Params" type="atArray">
      <pointer type="CNmParameter" policy="owner" />
    </array>
    <bool name="m_ForceNewMessage" init="false" />
    <bool name="m_TaskMessage" init="false" />
  </structdef>

  <structdef type="CNmTuningSet" onPreAddWidgets="AddWidgets">
    <string name="m_Id" type="atHashString" hideWidgets="true"></string>
    <int name="m_Priority" min="0" max="255" init="0" hideWidgets="true"/>
    <bool name="m_Enabled" init="true" description="Enables / disables this tuning group" hideWidgets="true"/>
    <array name="m_Messages" type="atArray" hideWidgets="true">
      <pointer type="CNmMessage" policy="owner" />
    </array>
  </structdef>

  <structdef type="CNmTuningSetGroup" onPreAddWidgets="AddParamWidgets">
    <map name="m_sets" type="atBinaryMap" key="atHashString" hideWidgets="true" >
      <struct type="CNmTuningSet"/>
    </map>
  </structdef>

  <!-- Individual task tuning -->

  <structdef type="CTaskNMBrace::Tunables::VelocityLimits">
    <bool name="m_Apply" init="false"/>
    <Vector3 name="m_Constant" init="0.02" min="0.0f" max="100.0f" description="Constant damping amount" />
    <Vector3 name="m_Velocity" init="0.02" min="0.0f" max="100.0f" description="Damping that scales linearly with velocity" />
    <Vector3 name="m_Velocity2" init="0.02" min="0.0f" max="100.0f" description="Damping that scales with velocity squared" />
    <float name="m_Max" init="12.0f" min="0.0f" max="100000.0f" description="Maximum value (clamp)" />
    <int name="m_Delay" init="500" min="0" max="100000" description="Delay (in ms) before applying the velocity limits" />
  </structdef>

  <structdef type="CTaskNMBrace::Tunables::InitialForce">
    <float name="m_VelocityMin" init="0.0" min="0.0f" max="3600.0f" description="ForceAtMinVel will be applied at this velocity (or lower)" />
    <float name="m_VelocityMax" init="10.0" min="0.0f" max="3600.0f" description="ForceAtMaxVel will be applied at this velocity (or higher)" />
    <float name="m_ForceAtMinVelocity" init="0.0" min="0.0f" max="100000.0f" description="The minimum force to apply" />
    <float name="m_ForceAtMaxVelocity" init="10.0" min="0.0f" max="100000.0f" description="The maximum force to apply" />
    <bool name="m_ScaleWithUpright" init="true"/>
  </structdef>

  <structdef type="CTaskNMBrace::Tunables::ApplyForce">
    <bool name="m_Apply" init="false" description="Enables and disables the force"/>
   
    <bool name="m_ScaleWithVelocity" init="false" description="Scales the force with the velocity of the vehicle"/>
    <float name="m_MinVelThreshold" init="0.0" min="0.0f" max="100000.0f" description="The velocity (or lower) at which the min force multiplier will be applied" />
    <float name="m_MaxVelThreshold" init="100000.0f" min="0.0f" max="100000.0f" description="The velocity (or higher) at which the max force multiplier will be applied" />
    <float name="m_MinVelMag" init="0.0" min="0.0f" max="100000.0f" description="The minimum force multiplier to apply" />
    <float name="m_MaxVelMag" init="100000.0f" min="0.0f" max="100000.0f" description="The maximum force multiplier to apply" />

    <bool name="m_ScaleWithUpright" init="false" description="Scales the force with the uprightness o the hit character"/>
    <bool name="m_ScaleWithMass" init="false" description="Scales the force with the mass of the vehicle"/>
    <bool name="m_ReduceWithPedVelocity" init="false" description="Scales the force down by the magnitude of the peds linear velocity (may improve stability)"/>
    <bool name="m_ReduceWithPedAngularVelocity" init="false" description="Scales the force down by the magnitude of the peds angular velocity (may improve stability)"/>
    <bool name="m_OnlyInContact" init="false" description="Applies the force only whilst still in contact with the vehicle"/>
    <bool name="m_OnlyNotInContact" init="false" description="Applies the force only whilst not in contact with the vehicle"/>
    <float name="m_ForceMag" init="70.0f" min="0.0f" max="10000000.0f" description="the magnitude of the force" />
    <float name="m_MinMag" init="0.0f" min="0.0f" max="10000000.0f" description="the minimum impulse magnitude allowed (after scaling / timestep etc)" />
    <float name="m_MaxMag" init="4500.f" min="0.0f" max="10000000.0f" description="the maximum impulse magnitude allowed (after scaling / timestep etc)" />
    <int name="m_Duration" init="200" min="0" max="100000" description="The duration to apply the force over" />
  </structdef>

  <structdef type="CTaskNMBrace::Tunables::VehicleTypeOverrides">
    <string name="m_Id" type="atHashString"></string>

    <bool name="m_OverrideInverseMassScales" init="false" description="Should this vehicle override the inverse mass scales"/>
    <struct name="m_InverseMassScales" type="CTaskNMBehaviour::Tunables::InverseMassScales" description="Adjusts the overall effect of the collision force on the ped and the vehicle."/>

    <bool name="m_OverrideReactionType" init="false" description="Should this vehicle override the reaction type"/>
    <bool name="m_ForceUnderVehicle" init="false"/>
    <bool name="m_ForceOverVehicle" init="false"/>
    <float name="m_VehicleCentreZOffset" init="0.0f" min="-100.0f" max="100.0f" description="Adjusts the height of the vehicle bounding box centre of mass when determining which reaction type to use." />

    <bool name="m_OverrideRootLiftForce" init="false" description="Should this vehicle override the reaction type"/>
    <struct name="m_RootLiftForce" type="CTaskNMBrace::Tunables::ApplyForce" description="An upward force applied to the peds root when the task starts"/>

    <bool name="m_OverrideFlipForce" init="false" description="Should this vehicle override the reaction type"/>
    <struct name="m_FlipForce" type="CTaskNMBrace::Tunables::ApplyForce" description="A force applied into the car on the chest and away from the car on the feet, to make the ped flip"/>

    <bool name="m_OverrideInitialForce" init="false" description="Should this vehicle override the reaction type"/>
    <struct name="m_InitialForce" type="CTaskNMBrace::Tunables::InitialForce" description="Applies an additional force at the beginning of the task"/>

    <bool name="m_OverrideElasticity" init="false" description="Should this vehicle override the reaction type"/>
    <float name="m_VehicleCollisionElasticityMult" init="10.0f" min="0.0f" max="1000.0f" description="The elasticity of any contacts between ped ragdolls and vehicles will be multiplied by this value." />

    <bool name="m_OverrideFriction" init="false" description="Should this vehicle override the reaction type"/>
    <float name="m_VehicleCollisionFrictionMult" init="0.25f" min="0.0f" max="1000.0f" description="The friction of any contacts between ped ragdolls and vehicles will be multiplied by this value." />

    <struct name="m_LateralForce" type="CTaskNMBrace::Tunables::ApplyForce" description="Apply a sideways force to the ped based on the side of the vehicle they're on."/>

    <bool name="m_OverrideStuckOnVehicleSets" init="false" description="Should this vehicle override the stuck on vehicle nm parameters"/>
    <bool name="m_AddToBaseStuckOnVehicleSets" init="false" description="IF true, send the base stuck on vehicle sets as well as the overriden one"/>
    <struct name="m_StuckOnVehicle" type="CTaskNMBrace::Tunables::StuckOnVehicle" description="Tuning for the stuck on vehicle mode"/>
  </structdef>

  <structdef type="CTaskNMBrace::Tunables::StuckOnVehicle">
    <int name="m_InitialDelay" init="500" min="0" max="100000" description="Delay (in ms) before the stuck on vehicle tuning set can be triggered" />
    <int name="m_UnderVehicleInitialDelay" init="500" min="0" max="100000" description="Delay (in ms) before the stuck under vehicle tuning set can be triggered" />
    <float name="m_VelocityThreshold" init="1.0f" min="0.0f" max="3600.0f" description="If the ped is in contact with a vehicle, and the peds velocity is greater than this, the stuck on vehicle tuning bank will be sent" />
    <float name="m_ContinuousContactTime" init="0.5f" min="0.0f" max="3600.0f" description="If the ped is continuously in contact with a vehicle for this amount of time or longer (in seconds) switch to stuck on vehicle mode." />
    <float name="m_UnderVehicleVelocityThreshold" init="0.0f" min="0.0f" max="3600.0f" description="If the ped is in contact with a vehicle, and the peds velocity is greater than this, the stuck on vehicle tuning bank will be sent" />
    <float name="m_UnderVehicleContinuousContactTime" init="0.1f" min="0.0f" max="3600.0f" description="If the ped is continuously in contact with a vehicle for this amount of time or longer (in seconds) and is under the vehicle, switch to stuck under vehicle mode." />
    <float name="m_UnderCarMaxVelocity" init="1.0f" min="0.0f" max="3600.0f" description="If the ped is in contact with and under the vehicle, the peds velocity must be lower than this to enter stuck on vehicle mode" />

    <struct name="m_StuckOnVehicle" type="CNmTuningSet" description="Sent when the task detects that the ped is stuck on a vehicle"/>
    <struct name="m_EndStuckOnVehicle" type="CNmTuningSet" description="Sent when the task detects that the ped is no longer stuck on a vehicle"/>
    <struct name="m_UpdateOnVehicle" type="CNmTuningSet" description="Sent every frame whilst the ped is on the vehicle"/>

    <struct name="m_StuckUnderVehicle" type="CNmTuningSet" description="Sent when the task detects that the ped is stuck under a vehicle"/>
    <struct name="m_EndStuckUnderVehicle" type="CNmTuningSet" description="Sent when the task detects that the ped is no longer stuck under a vehicle"/>

    <struct name="m_StuckOnVehiclePlayer" type="CNmTuningSet" description="Sent when the task detects that the ped is stuck on a vehicle"/>
    <struct name="m_EndStuckOnVehiclePlayer" type="CNmTuningSet" description="Sent when the task detects that the ped is no longer stuck on a vehicle"/>
    <struct name="m_UpdateOnVehiclePlayer" type="CNmTuningSet" description="Sent every frame whilst the ped is on the vehicle"/>
    
    <struct name="m_StuckUnderVehiclePlayer" type="CNmTuningSet" description="Sent when the task detects that the ped is stuck under a vehicle"/>
    <struct name="m_EndStuckUnderVehiclePlayer" type="CNmTuningSet" description="Sent when the task detects that the ped is no longer stuck under a vehicle"/>
  </structdef>
  
  <structdef type="CTaskNMBrace::Tunables::VehicleTypeTunables">
    <array name="m_sets" type="atArray" >
      <struct type="CTaskNMBrace::Tunables::VehicleTypeOverrides"/>
    </array>
  </structdef>

  <structdef type="CTaskNMBrace::Tunables" base="CTuning">
    <struct name="m_VehicleOverrides" type="CTaskNMBrace::Tunables::VehicleTypeTunables" description="Per vehicle type tunable overrides."/>

    <struct name="m_InverseMassScales" type="CTaskNMBehaviour::Tunables::InverseMassScales" description="Adjusts the overall effect of the collision force on the ped and the vehicle."/>
    <struct name="m_AngularVelocityLimits" type="CTaskNMBrace::Tunables::VelocityLimits" description="Limit the angular velocity of the character while the behaviour is running"/>
    <struct name="m_InitialForce" type="CTaskNMBrace::Tunables::InitialForce" description="Applies an additional force at the beginning of the task"/>
    <struct name="m_ChestForce" type="CTaskNMBrace::Tunables::ApplyForce" description="A force applied to the chest at the beginning of the task (in the direction of movement of the vehicle)"/>
    <struct name="m_FeetLiftForce" type="CTaskNMBrace::Tunables::ApplyForce" description="An upward force applied to the feet at the start of the task"/>
    <struct name="m_RootLiftForce" type="CTaskNMBrace::Tunables::ApplyForce" description="An upward force applied to the peds root when the task starts"/>
    <struct name="m_FlipForce" type="CTaskNMBrace::Tunables::ApplyForce" description="A force applied into the car on the chest and away from the car on the feet, to make the ped flip"/>
    <struct name="m_CapsuleHitForce" type="CTaskNMBrace::Tunables::ApplyForce" description="An additional force applied to the ped when the vehicle hit is against the ped capsule rather than the ragdoll bounds"/>
    <struct name="m_SideSwipeForce" type="CTaskNMBrace::Tunables::ApplyForce" description="An additional angular force applied to the ped when the ped is activated from a side swipe"/>
    <bool name="m_ForceUnderVehicle" init="false"/>
    <bool name="m_ForceOverVehicle" init="false"/>
    <float name="m_ChestForcePitch" init="0.0f" min="-3.14f" max="3.14f" description="Pitches the force applied to the chast based on the direction of movement of the hit vehicle" />
    <bool name="m_AllowWarningActivations" init="false"/>
    <float name="m_LowVelocityReactionThreshold" init="20.0f" min="0.0f" max="3600.0f" description="Velocities lower than this will use the low velocity reaction. Higher will use the high velocity one." />
    <float name="m_FallingSpeedForHighFall" init="4.0f" min="0.0f" max="3600.0f" description="If the ped is falling downwards faster than this speed, transitions to a high fall task" />
    <float name="m_VehicleCollisionElasticityMult" init="10.0f" min="0.0f" max="1000.0f" description="The elasticity of any contacts between ped ragdolls and vehicles will be multiplied by this value." />
    <float name="m_VehicleCollisionFrictionMult" init="0.25f" min="0.0f" max="1000.0f" description="The friction of any contacts between ped ragdolls and vehicles will be multiplied by this value." />
    <float name="m_VehicleCollisionNormalPitchOverVehicle" init="0.0f" min="-3.14" max="3.14" description="Pitch the collision normal on the ped by this amount when hit by a moving vehicle." />
    <float name="m_VehicleCollisionNormalPitchUnderVehicle" init="0.0f" min="-3.14" max="3.14" description="Pitch the collision normal on the ped by this amount when hit by a moving vehicle." />

    <int name="m_AiClearedVehicleDelay" init="100" min="0" max="100000" description="Amount of time (in ms) over which the ped has to not touch the car to be considered clear of it." />
    <int name="m_AiClearedVehicleSmartFallDelay" init="1500" min="0" max="100000" description="Amount of time (in ms) over which the ped has to not touch the car to transition into smart fall." />
    <int name="m_PlayerClearedVehicleDelay" init="100" min="0" max="100000" description="Amount of time (in ms) over which the player ped has to not touch the car to be considered clear of it." />
    <int name="m_PlayerClearedVehicleSmartFallDelay" init="300" min="0" max="100000" description="Amount of time (in ms) over which the player ped has to not touch the car to transition into smart fall." />

    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start of the task"/>
    <struct name="m_OnStairs" type="CNmTuningSet" description="Sent once at the start of the task if the ped is on stairs"/>
    <struct name="m_Weak" type="CNmTuningSet" description="Sent once at the start of the task if the ped is weak"/>
    <struct name="m_OnBalanceFailed" type="CNmTuningSet" description="Sent once at the point where the balancer fails"/>
    <struct name="m_OnBalanceFailedStairs" type="CNmTuningSet" description="Sent if the balancer fails whilst the ped is on stairs"/>
    <struct name="m_HighVelocity" type="CNmTuningSet" description="Sent if the vehicle velocity is above the low velocity threshold"/>

    <float name="m_StuckUnderVehicleMaxUpright" init="0.6f" min="0.0f" max="1.0f" description="The ped must be less 'upright' than this to be considered stuck under the vehicle. 1.0f is completely vertical (upright or upside down), 0.0f is completely horizontal (lying down absolutely flat)" />
    <struct name="m_StuckOnVehicle" type="CTaskNMBrace::Tunables::StuckOnVehicle" description="Tuning for the stuck on vehicle mode"/>

    <struct name="m_Update" type="CNmTuningSet" description="Sent instead of the above if the vehicle velocity is above the low velocity threshold"/>
    <struct name="m_Dead" type="CNmTuningSet" description="Sent instead of the above if the vehicle velocity is above the low velocity threshold"/>

    <struct name="m_OverVehicle" type="CNmTuningSet" description="Sent when attempting to push the ped under the hit vehicle"/>
    <struct name="m_UnderVehicle" type="CNmTuningSet" description="Sent when attempting to push the ped over the hit vehicle"/>
    <struct name="m_ClearedVehicle" type="CNmTuningSet" description="Sent when the ped is no longer in contact with the vehicle"/>

    <struct name="m_HighVelocityBlendOut" type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds"/>
    
  </structdef>

  <structdef type="CTaskNMBalance::Tunables" base="CTuning">
    <enum name="m_InitialBumpComponent" type="RagdollComponent">RAGDOLL_SPINE3</enum>
    <struct name="m_InitialBumpForce" type="CTaskNMBehaviour::Tunables::TunableForce" description="Tunable force to apply to the bumped ped"/>
    <Vector3 name="m_InitialBumpOffset" init="0.0" min="-10.0" max="10.0" description="Local offset from the component to apply the impulse"/>
    
    <bool name="m_ScaleStayUprightWithVel" init="true"/>
    <float name="m_StayUprightAtMaxVel" init="0.0" min="0.0" max="16.f" description="The stayupright force to use at max velocity"/>
    <float name="m_MaxVel" init="0.0" min="0.0" max="10000.0" description="Max velocity"/>
    <float name="m_StayUprightAtMinVel" init="0.0" min="0.0" max="16.f" description="The stayupright force to use at min velocity"/>
    <float name="m_MinVel" init="0.0" min="0.0" max="10000.0" description="Min velocity"/>

    <float name="m_lookAtVelProbIfNoBumpTarget" init="0.125" min="0.0" max="1.0" description="Probability of looking at velocity if there's no head look bump target"/>
    <float name="m_fMaxTargetDistToUpdateFlinch" init="0.7" min="0.0" max="10.0" />
    <float name="m_fMaxTargetDistToUpdateFlinchOnGround" init="0.7" min="0.0" max="10.0" />
    <float name="m_fFlinchTargetZOffset" init="0.0" min="-10.0" max="10.0" />
    <float name="m_fFlinchTargetZOffsetOnGround" init="0.0" min="-10.0" max="10.0" />
    <float name="m_fMinForwardVectorToFlinch" init="0.2" min="0.0" max="1.0" />
    <float name="m_fMinForwardVectorToFlinchOnGround" init="0.2" min="-1.0" max="1.0" />
    <float name="m_fHeadLookZOffset" init="0.0" min="-10.0" max="10.0" />
    <float name="m_fHeadLookZOffsetOnGround" init="0.0" min="-10.0" max="10.0" />
    <int name="m_MaxSteps" init="3" min="0" max="100" description="Max steps allowed when bumped by a ped"/>
    <int name="m_timeToCatchfallMS" init="3000" min="0" max="20000" description="Time in rolling fall before switching to catch fall (to slow down)"/>
    <struct name="m_StartWeak" type="CNmTuningSet" description="Sent once at the start of the task for weak peds"/>
    <struct name="m_StartAggressive" type="CNmTuningSet" description="Sent once at the start of the task fior aggressive peds"/>
    <struct name="m_StartDefault" type="CNmTuningSet" description="Sent once at the start of the task for 'default' peds"/>
    <struct name="m_BumpedByPed" type="CNmTuningSet" description="Sent once at the start of the task if bumped by a ped"/>
    <struct name="m_OnStairs" type="CNmTuningSet" description="Sent once at the start of the task if on stairs"/>
    <struct name="m_OnSteepSlope" type="CNmTuningSet" description="Sent once at the start of the task if on a steep slope"/>
    <struct name="m_OnMovingGround" type="CNmTuningSet" description="Sent once at the start of the task when on moving ground"/>
    <struct name="m_LostBalanceAndGrabbing" type="CNmTuningSet" description="On update - pedal if grabbing something"/>
    <struct name="m_Teeter" type="CNmTuningSet" description="teeter params when the opportunity arises (detected in code)"/>
    <struct name="m_FallOffAMovingCar" type="CNmTuningSet" description="A special faked teeter that is used when on a moving vehicle to force a fall"/>
    <struct name="m_RollingFall" type="CNmTuningSet" description="Used when the balancer fails"/>
    <struct name="m_CatchFall" type="CNmTuningSet" description="Called after doing a rolling fall for a period of time.  This brings the fall to a halt."/>
    <struct name="m_OnBalanceFailed" type="CNmTuningSet" description="Called once at the point where the balancer fails."/>

    <int name="m_NotBeingPushedDelayMS" init="100" min="0" max="100000" description="Amount of time (in milliseconds) the ped needs to be out of contact with the brace entity before sending the out of contact nm tuning set"/>
    <int name="m_NotBeingPushedOnGroundDelayMS" init="100" min="0" max="100000" description="Amount of time (in milliseconds) the ped needs to be out of contact with the brace entity while on the ground before sending the out of contact nm tuning set"/>
    <int name="m_BeingPushedOnGroundTooLongMS" init="2000" min="0" max="100000" description="Amount of time (in milliseconds) before the m_OnBeingPushedOnGroundTooLong tuning set is used."/>

    <struct name="m_OnBeingPushed" type="CNmTuningSet" description="Called when entering the in contact state (ped is being pushed)."/>
    <struct name="m_OnBeingPushedOnGround" type="CNmTuningSet" description="Called when entering the in contact state (ped is being pushed) while on the ground."/>
    <struct name="m_OnNotBeingPushed" type="CNmTuningSet" description="Called when leaving the in contact state (once ped is no longer being pushed)."/>
    <struct name="m_OnBeingPushedOnGroundTooLong" type="CNmTuningSet" description="Called when the ped has been pushed longer than m_BeingPushedOnGroundTooLongMS milliseconds."/>

    <struct name="m_PushedThresholdOnGround" type="CTaskNMBehaviour::Tunables::BlendOutThreshold" description="Threshold for considering a ped as being pushed while on the ground."/>
  </structdef>

  <structdef type="CTaskNMRiverRapids::Tunables::BodyWrithe">
    <bool name="m_bControlledByPlayerSprintInput" init="false" description="Whether the body writhe can be affected by the player mashing the sprint button"/>
    <float name="m_fMinArmAmplitude" init="1.5" min="0.0" max="3.0" description="The minimum amount of arm amplitude to apply"/>
    <float name="m_fMaxArmAmplitude" init="1.5" min="0.0" max="3.0" description="The maximum amount of arm amplitude to apply"/>
    <float name="m_fMinArmStiffness" init="13.0" min="6.0" max="16.0" description="The minimum amount of arm stiffness to apply"/>
    <float name="m_fMaxArmStiffness" init="13.0" min="6.0" max="16.0" description="The maximum amount of arm stiffness to apply"/>
    <float name="m_fMinArmPeriod" init="1.5" min="0.01" max="4.0" description="The minimum arm period to apply"/>
    <float name="m_fMaxArmPeriod" init="1.5" min="0.01" max="4.0" description="The maximum arm period to apply"/>
    <float name="m_fMinStroke" init="0.0" min="-1000.0" max="1000.0" description="The minimum stroke to apply"/>
    <float name="m_fMaxStroke" init="0.0" min="-1000.0" max="1000.0" description="The maximum stroke to apply"/>
    <float name="m_fMinBuoyancy" init="1.0" min="0.0" description="The minimum buoyancy factor to apply"/>
    <float name="m_fMaxBuoyancy" init="1.0" min="0.0" description="The maximum buoyancy factor to apply"/>
  </structdef>

  <structdef type="CTaskNMBuoyancy::Tunables" base="CTuning">
    <struct name="m_BlendOutThreshold" type="CTaskNMBehaviour::Tunables::BlendOutThreshold" description="Custom blend out threshold for nm buoyancy task."/>
  </structdef>

  <structdef type="CTaskNMRiverRapids::Tunables" base="CTuning" onPreAddWidgets="PreAddWidgets">
    <float name="m_fMinRiverFlowForRapids" init="3.0" min="0.0" max="1000.0" description="Minimum speed of river flow to be considered rapids"/>
    <float name="m_fMinRiverGroundClearanceForRapids" init="0.6" min="0.0" max="10.0" description="Minimum ground clearance to be considered rapids"/>
    <bool name="m_bHorizontalRighting" init="true" description="Allow the character to right themselves horizontally"/>
    <float name="m_fHorizontalRightingStrength" init="25.0" min="0.0" description="The strength with which the character rights themselves horizontally"/>
    <float name="m_fHorizontalRightingTime" init="1.0" min="0.0" description="The amount of time to wait before strating to right the character horizontally"/>
    <bool name="m_bVerticalRighting" init="false" description="Allow the character to right themselves vertically"/>
    <float name="m_fVerticalRightingStrength" init="25.0" min="0.0" description="The strength with which the character rights themselves vertically"/>
    <float name="m_fVerticalRightingTime" init="1.0" min="0.0" description="The amount of time to wait before strating to right the character vertically"/>
    <array name="m_fRagdollComponentBuoyancy" type="member" size="RAGDOLL_NUM_COMPONENTS" hideWidgets="true">
      <float />
    </array>
    <struct name="m_BodyWrithe" type="CTaskNMRiverRapids::Tunables::BodyWrithe" description="Parameters that control the amount of body writhe"/>
    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start of the behavior"/>
    <struct name="m_Update" type="CNmTuningSet" description="Sent each update of the behavior"/>
  </structdef>

  <structdef type="CTaskNMFlinch::Tunables" base="CTuning">
    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start of the task"/>
    <struct name="m_Passive" type="CNmTuningSet" description="Sent once at the start of the task an passive peds"/>
    <struct name="m_WaterCannon" type="CNmTuningSet" description="Sent once at the start of the task for 'default' peds"/>
   
    <struct name="m_Armoured" type="CNmTuningSet" description="Sent once at the start of the task on armoured peds"/>
    <struct name="m_OnStairs" type="CNmTuningSet" description="Sent once at the start of the task if on stairs"/>
    <struct name="m_BoundAnkles" type="CNmTuningSet" description="Sent once at the start of the task if the peds ankles are bound"/>
    <struct name="m_FatallyInjured" type="CNmTuningSet" description="Sent once at the start of the task if the ped is fatally injured"/>
    <struct name="m_PlayerDeath" type="CNmTuningSet" description="Sent once at the start of the task if the ped is a dead player"/>
    <struct name="m_HoldingTwoHandedWeapon" type="CNmTuningSet" description="teeter params when the opportunity arises (detected in code)"/>
    <struct name="m_HoldingSingleHandedWeapon" type="CNmTuningSet" description="A special faked teeter that is used when on a moving vehicle to force a fall"/>

    <struct name="m_Update" type="CNmTuningSet" description="Called every frame while the task updates"/>
    <struct name="m_OnBalanceFailed" type="CNmTuningSet" description="Called once when the balancer fails"/>
    <struct name="m_OnBalanceFailedStairs" type="CNmTuningSet" description="Called in addition to the OnBalanceFailed bank when the balancer fails on stairs"/>

    <struct name="m_WeaponSets" type="CNmTuningSetGroup"/>
    <struct name="m_ActionSets" type="CNmTuningSetGroup"/>

    <bool name="m_RandomiseLeadingHand" init="true"/>
    <int name="m_MinLeanInDirectionTime" init="200" min="0" max="100000" description="Minimum time (in MS) to use lean in direction for"/>
    <int name="m_MaxLeanInDirectionTime" init="900" min="0" max="100000" description="Maximum time (in MS) to use lean in direction for"/>

    <float name="m_fImpulseReductionScaleMax" init="1.0f" min="0.0f" max="1.0f" description="Amount of impulse to allow to be reduced (1.0 means all impulse can be reduced and 0.0 means no amount of impulse can be reduced)"/>
    <float name="m_fSpecialAbilityRageKickImpulseModifier" init="1.15f" min="0.0f" max="100.0f" description="Amount to modify the impulse when kicking while using the rage special ability"/>
    <float name="m_fCounterImpulseScale" init="1.0f" min="0.0f" max="1.0f" description="Amount of the counter impulse to apply"/>
  </structdef>
  
  <structdef type="CTaskNMDrunk::Tunables" base="CTuning" cond="ENABLE_DRUNK">
    <float name="m_fMinHeadingDeltaToFixTurn" init="2.3561" min="0.00" max="3.1415"/>
    <float name="m_fHeadingRandomizationRange" init="0.4" min="0.0" max="3.1415"/>
    <int name="m_iHeadingRandomizationTimeMin" init="300" min="0" max="10000"/>
    <int name="m_iHeadingRandomizationTimeMax" init="1000" min="0" max="10000"/>
    <float name="m_fForceLeanInDirectionAmountMin" init="0.75" min="0.0" max="16.0"/>
    <float name="m_fForceLeanInDirectionAmountMax" init="0.75" min="0.0" max="16.0"/>
    <float name="m_fForceRampMinSpeed" init="0.0" min="0.0" max="16.0"/>
    <float name="m_fForceRampMaxSpeed" init="1.0" min="0.0" max="16.0"/>
    <float name="m_fHeadLookHeadingRandomizationRange" init="0.75" min="0.0" max="3.1415"/>
    <float name="m_fHeadLookPitchRandomizationRange" init="0.5" min="0.00" max="3.1415"/>
    <int name="m_iHeadLookRandomizationTimeMin" init="3000" min="0" max="10000"/>
    <int name="m_iHeadlookRandomizationTimeMax" init="10000" min="0" max="100000"/>
    <float name="m_fMinHeadingDeltaToIdleTurn" init="0.5235" min="0.00" max="3.1415"/>
    <int name="m_iRunningTimeForVelocityBasedStayupright" init="6000" min="0" max="20000"/>
    <float name="m_fStayUprightForceNonVelocityBased" init="4.0" min="0.0" max="16.0"/>
    <float name="m_fStayUprightForceMoving" init="6.0" min="0.00" max="16.0"/>
    <float name="m_fStayUprightForceIdle" init="3.0" min="0.00" max="16.0"/>
    <float name="m_fFallingSpeedForHighFall" init="4.0" min="0.00" max="1000.0"/>
    <bool name="m_bUseStayUpright" init="true"/>
    <bool name="m_bDrawIdleHeadLookTarget" init="false"/>
    <struct name="m_Start" type="CNmTuningSet" description="Sent once when the task starts" />
    <struct name="m_Base" type="CNmTuningSet" description="Sent every frame" />
    <struct name="m_Moving" type="CNmTuningSet" description="Sent every frame while the ped is standing idle" />
    <struct name="m_Idle" type="CNmTuningSet" description="Sent every frame while the ped is trying to move" />
  </structdef>

  <structdef type="CTaskNMExplosion::Tunables::Explosion">
    <float name="m_NMBodyScale" min="0.0f" max="50.0f" init="4.5f" description="Amount to scale force applied to each of the body parts of an NM agent" />
    <float name="m_HumanBodyScale" min="0.0f" max="50.0f" init="3.0f" description="Amount to scale force applied to each of the body parts of a human using a rage ragdoll" />
    <float name="m_HumanPelvisScale" min="0.0f" max="50.0f" init="1.0f" description="Amount to scale force applied to pelvis of a human using a rage ragdoll" />
    <float name="m_HumanSpine0Scale" min="0.0f" max="50.0f" init="0.8f" description="Amount to scale force applied to spine0 of a human using a rage ragdoll" />
    <float name="m_HumanSpine1Scale" min="0.0f" max="50.0f" init="0.6f" description="Amount to scale force applied to spine1 of a human using a rage ragdoll" />
    <float name="m_AnimalBodyScale" min="0.0f" max="50.0f" init="1.5f" description="Amount to scale force applied to each of the body parts of an animal using a rage ragdoll" />
    <float name="m_AnimalPelvisScale" min="0.0f" max="50.0f" init="0.5f" description="Amount to scale force applied to pelvis of an animal using a rage ragdoll" />

    <float name="m_StrongBlastMagnitude" min="0.0f" max="50.0f" init="20.0f" description="The strength that a blast needs to be above to reset the ped's velocities before applying the new explosion. This is done so the ragdoll doesn't end up with too large a velocity and doesn't hit a spread eagle pose." />
    <float name="m_FastMovingPedSpeed" min="0.0f" max="50.0f" init="3.0f" description="The speed (in the direction of the explosion) that a ped needs to going above to reset the ped's velocities before applying the new explosion. This is done so the ragdoll doesn't end up with too large a velocity and doesn't hit a spread eagle pose." />

    <float name="m_MaxDistanceAbovePedPositionToClampPitch" min="0.0f" max="50.0f" init="1.5f" description="The distance that an explosion can be above a ped that an explosion will still have its pitch clamped to give a nice arc" />
    <float name="m_PitchClampMin" min="0.0f" max="180.0f" init="20.0f" description="Minimum pitch to apply force to give a nice arc" />
    <float name="m_PitchClampMax" min="0.0f" max="180.0f" init="60.0f" description="Maximum pitch to apply force to give a nice arc" />

    <float name="m_MagnitudeClamp" min="0.0f" max="1000.0f" init="100.0f" description="The maximum amount of force to allow" />

    <float name="m_SideScale" min="0.0f" max="10.0f" init="2.2f" description="Amount of extra force to apply perpendicularly to the blast force (provided the blast force was behind or to the side of the ped)" />
    <float name="m_PitchSideAngle" min="0.0f" max="90.0f" init="72.542397" description="The angle at which the extra perpendicular force starts to fall-off" />
    <float name="m_PitchTorqueMin" min="0.0f" max="50.0f" init="5.0f" description="Minimum amount of extra force to apply perpendicularly to the blast force" />
    <float name="m_PitchTorqueMax" min="0.0f" max="50.0f" init="8.0f" description="Maximum amount of extra force to apply perpendicularly to the blast force" />

    <float name="m_BlanketForceScale" min="0.0f" max="10.0f" init="0.1f" description="Scale the amount of force that gets applied to each of the body parts of an NM agent" />

    <float name="m_ExtraTorqueTwistMax" min="0.0f" max="100.0f" init="15.0f" description="Maximum extra twist torque to apply" />

    <float name="m_DisableInjuredBehaviorImpulseLimit" min="0.0f" max="1000.0f" init="105.0f" description="The maximum impulse that can be applied before the injured behavior is disabled" />
    <float name="m_DisableInjuredBehaviorDistLimit" min="0.0f" max="100.0f" init="2.0f" description="The minimum distance from the explosion before the injured behavior is disabled" />
  </structdef>

  <structdef type="CTaskNMExplosion::Tunables" base="CTuning">
    <int name="m_MinStunnedTime" min="0" max="100000" init="5000" />
    <int name="m_MaxStunnedTime" min="0" max="100000" init="10000" />
    <bool name="m_AllowPlayerStunned" init="false"/>   
    <bool name="m_UseRelaxBehaviour" init="false"/>
    <float name="m_RollUpHeightThreshold" min="0.0f" max="50.0f" init="5.0f"/>
    <float name="m_CatchFallHeightThresholdRollUp" min="0.0f" max="50.0f" init="3.0f"/>
    <float name="m_CatchFallHeightThresholdWindmill" min="0.0f" max="50.0f" init="1.0f"/>
    <float name="m_CatchFallHeightThresholdClipPose" min="0.0f" max="50.0f" init="10.0f"/>
    <u32 name="m_TimeToStartCatchFall" min="0" init="2000"/>
    <u32 name="m_TimeToStartCatchFallPlayer" min="0" init="500"/>
    <bool name="m_DoCatchFallRelax" init="false"/>   
    <float name="m_CatchFallRelaxHeight" min="0.0f" max="50.0f" init="0.4f"/>
    <float name="m_HeightToStartWrithe" min="0.0f" max="50.0f" init="1.0f"/>
    <int name="m_MinTimeForInitialState" min="0" max="100000" init="500" />
    <int name="m_MaxTimeForInitialState" min="0" max="100000" init="501" />
    <int name="m_MinWritheTime" min="0" max="100000" init="1000" />
    <int name="m_MaxWritheTime" min="0" max="100000" init="3000" />
    <bool name="m_ForceRollUp" init="true"/>
    <bool name="m_ForceWindmill" init="false"/>
    <struct name="m_StartWindmill" type="CNmTuningSet" description="Sent once at the start of the windmill behavior"/>
    <struct name="m_StartCatchFall" type="CNmTuningSet" description="Sent once at the start of the catchfall behavior"/>
    <struct name="m_StartRollDownStairs" type="CNmTuningSet" description="Sent once at the start of the roll-down-stairs behavior"/>
    <struct name="m_Update" type="CNmTuningSet" description="Sent every time the task updates"/>
    <struct name="m_Explosion" type="CTaskNMExplosion::Tunables::Explosion" description="Tunables used for explosions hitting peds"/>
  </structdef>

  <structdef type="CTaskNMInjuredOnGround::Tunables" autoregister="true" base="CTuning">
    <float name="m_fDoInjuredOnGroundChance" min="0.0f" max="1.0f" init="0.0f" />
    <float name="m_fFallingSpeedThreshold" min="-20.0f" max="20.0f" init="-4.0f" />
    <int name="m_iRandomDurationMin" min="0" max="65535" init="10000" />
    <int name="m_iRandomDurationMax" min="0" max="65535" init="35000" />
    <int name="m_iMaxNumInjuredOnGroundAgents" min="0" max="50" init="1" />
  </structdef>

  <structdef type="CTaskNMJumpRollFromRoadVehicle::Tunables" autoregister="true" base="CTuning">
    <float name="m_GravityScale" min="0.0f" max="1000.0f" init="100.0f"/>
    <float name="m_StartForceDownHeight" min="0.0f" max="100.0f" init="0.0f"/>
    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start"/>
    <struct name="m_EntryPointSets" type="CNmTuningSetGroup"/>
    <struct name="m_BlendOut" type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds" description="Blend out thresholds"/>
    <struct name="m_QuickBlendOut" type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds" description="Fast blend out thresholds (for player jacks)"/>
  </structdef>
  
	<structdef type="CTaskNMDraggingToSafety::Tunables::Stiffness">
		<float name="m_Relaxation" min="0.0f" max="100.0f" init="45.0f" />
		<float name="m_HeadAndNeck" min="0.0f" max="1.0f" init="0.8f" />
		<float name="m_AnkleAndWrist" min="0.0f" max="1.0f" init="0.8f" />
	</structdef>

	<structdef type="CTaskNMDraggingToSafety::Tunables::DraggerArmIk">
		<bool name="m_Enabled" init="true" />
		<Vector3 name="m_LeftBoneOffset" />
		<Vector3 name="m_RightBoneOffset" />
	</structdef>

  <structdef type="CTaskNMDraggingToSafety::Tunables::Forces">
    <bool name="m_Enabled" init="false" />
    <Vector3 name="m_LeftHandOffset" />
    <Vector3 name="m_RightHandOffset" />
  </structdef>

  <structdef type="CTaskNMDraggingToSafety::Tunables::Constraints">
		<float name="m_MaxDistance" min="0.0f" max="1.0f" init="0.05f" />
	</structdef>
	
	<structdef type="CTaskNMDraggingToSafety::Tunables" base="CTuning">
		<struct name="m_Stiffness" type="CTaskNMDraggingToSafety::Tunables::Stiffness" />
		<struct name="m_DraggerArmIk" type="CTaskNMDraggingToSafety::Tunables::DraggerArmIk" />
		<struct name="m_Constraints" type="CTaskNMDraggingToSafety::Tunables::Constraints" />
        <struct name="m_Forces" type="CTaskNMDraggingToSafety::Tunables::Forces" />      
	</structdef>

  <enumdef type="RagdollComponent">
    <enumval name="RAGDOLL_BUTTOCKS"/>
    <enumval name="RAGDOLL_THIGH_LEFT"/>
    <enumval name="RAGDOLL_SHIN_LEFT"/>
    <enumval name="RAGDOLL_FOOT_LEFT"/>
    <enumval name="RAGDOLL_THIGH_RIGHT"/>
    <enumval name="RAGDOLL_SHIN_RIGHT"/>
    <enumval name="RAGDOLL_FOOT_RIGHT"/>
    <enumval name="RAGDOLL_SPINE0"/>
    <enumval name="RAGDOLL_SPINE1"/>
    <enumval name="RAGDOLL_SPINE2"/>
    <enumval name="RAGDOLL_SPINE3"/>
    <enumval name="RAGDOLL_CLAVICLE_LEFT"/>
    <enumval name="RAGDOLL_UPPER_ARM_LEFT"/>
    <enumval name="RAGDOLL_LOWER_ARM_LEFT"/>
    <enumval name="RAGDOLL_HAND_LEFT"/>
    <enumval name="RAGDOLL_CLAVICLE_RIGHT"/>
    <enumval name="RAGDOLL_UPPER_ARM_RIGHT"/>
    <enumval name="RAGDOLL_LOWER_ARM_RIGHT"/>
    <enumval name="RAGDOLL_HAND_RIGHT"/>
    <enumval name="RAGDOLL_NECK"/>
    <enumval name="RAGDOLL_HEAD"/>
  </enumdef>

  <structdef type="CTaskNMShot::Tunables::ShotAgainstWall">
    <float name="m_HealthRatioLimit" min="0.0" max="1.0" init="0.3" description="shot against wall will only be used on peds whose health ratio is below this range" />
    <float name="m_WallProbeRadius" min="0.0" max="10.0" init="0.2" description="Radius of the swept sphere wall probe"  />
    <float name="m_WallProbeDistance" min="0.0" max="10.0" init="0.8" description="The distance of the swept sphere wall probe back from the pelvis" />
    <float name="m_ProbeHeightAbovePelvis" min="0.0" max="10.0" init="0.3" description="Height of the wall probe above the pelvis bone"  />
    <float name="m_ImpulseMult" min="0.0" max="10.0" init="1.75" description="Impulse multiplier" />
    <float name="m_MaxWallAngle" min="0.0" max="90.0" init="90.0" description="Maximum allowed angle between shot impulse and wall normal (in degrees)" />
  </structdef>

  <structdef type="CTaskNMShot::Tunables::Impulses::SniperImpulses">
    <float name="m_MaxHealthImpulseMult" min="0.0" max="100000" init="1.0" description="The impulse multiplier that will be used when the ped is at maximum health (but has no armour). This scales linearly with health up to m_MinHealthImpulseMult at the injured health threshold"/>
    <float name="m_MinHealthImpulseMult" min="0.0" max="100000" init="1.75" description="The impulse multiplier that will be used when the ped is at the injured health threashold. This scales linearly with health down to m_MaxHealthImpulseMult at full health"/>

    <float name="m_MaxDamageTakenImpulseMult" min="0.0" max="100000" init="1.0" description="The impulse multiplier that will be used when the ped is at maximum health (but has no armour). This scales linearly with health up to m_MinHealthImpulseMult at the injured health threshold"/>
    <float name="m_MinDamageTakenImpulseMult" min="0.0" max="100000" init="1.0" description="The impulse multiplier that will be used when the ped is at maximum health (but has no armour). This scales linearly with health up to m_MinHealthImpulseMult at the injured health threshold"/>
    <float name="m_MaxDamageTakenThreshold" min="0.0" max="100000" init="100" description="The amount of total damage taken (or higher) at which the max impulse multiplier will be used (must be higher than m_MinDamageTakenThreshold)"/>
    <float name="m_MinDamageTakenThreshold" min="0.0" max="100000" init="0.0" description="The amount of total damage taken (or lower) at which the min impulse multiplier will be used "/>

    <float name="m_DefaultKillShotImpulseMult" min="0.0" max="100000" init="1.75" description="The impulse multiplier applied on a killshot for single shot weapons (overrides health and armour impulse multipliers above)."/>
    <float name="m_DefaultMPKillShotImpulseMult" min="0.0" max="100" init="1.75" description="The impulse multiplier applied on a killshot for single shot weapons in multiplayer (overrides health and armour impulse multipliers above)."/>
  </structdef>

  <structdef type="CTaskNMShot::Tunables::Impulses">
    <float name="m_MaxArmourImpulseMult" min="0.0" max="100000" init="0.25" description="The impulse multiplier that will be used when the ped is at maximum armour. This scales linearly with armour down to m_MinArmourImpulseMult at 0.0 armour remaining" />
    <float name="m_MinArmourImpulseMult" min="0.0" max="100000" init="1.0" description="The impulse multiplier that will be used when the ped is at minimum armour. This scales linearly with armour up to m_MaxArmourImpulseMult at full armour" />

    <float name="m_MaxHealthImpulseMult" min="0.0" max="100000" init="1.0" description="The impulse multiplier that will be used when the ped is at maximum health (but has no armour). This scales linearly with health up to m_MinHealthImpulseMult at the injured health threshold"/>
    <float name="m_MinHealthImpulseMult" min="0.0" max="100000" init="1.75" description="The impulse multiplier that will be used when the ped is at the injured health threashold. This scales linearly with health down to m_MaxHealthImpulseMult at full health"/>

    <float name="m_MaxDamageTakenImpulseMult" min="0.0" max="100000" init="1.0" description="The impulse multiplier that will be used when the ped is at maximum health (but has no armour). This scales linearly with health up to m_MinHealthImpulseMult at the injured health threshold"/>
    <float name="m_MinDamageTakenImpulseMult" min="0.0" max="100000" init="1.0" description="The impulse multiplier that will be used when the ped is at maximum health (but has no armour). This scales linearly with health up to m_MinHealthImpulseMult at the injured health threshold"/>
    <float name="m_MaxDamageTakenThreshold" min="0.0" max="100000" init="100" description="The amount of total damage taken (or higher) at which the max impulse multiplier will be used (must be higher than m_MinDamageTakenThreshold)"/>
    <float name="m_MinDamageTakenThreshold" min="0.0" max="100000" init="0.0" description="The amount of total damage taken (or lower) at which the min impulse multiplier will be used "/>

    <float name="m_DefaultKillShotImpulseMult" min="0.0" max="100000" init="1.75" description="The impulse multiplier applied on a killshot for single shot weapons (overrides health and armour impulse multipliers above)."/>
    <float name="m_DefaultRapidFireKillShotImpulseMult" min="0.0" max="100000" init="1.25" description="The impulse multiplier applied on a killshot for rapid fire weapons(overrides health and armour impulse multipliers above)."/>

    <float name="m_DefaultMPKillShotImpulseMult" min="0.0" max="100" init="1.75" description="The impulse multiplier applied on a killshot for single shot weapons in multiplayer (overrides health and armour impulse multipliers above)."/>
    <float name="m_DefaultMPRapidFireKillShotImpulseMult" min="0.0" max="100" init="1.25" description="The impulse multiplier applied on a killshot for rapid fire weapons in multiplayer(overrides health and armour impulse multipliers above)."/>

    <float name="m_ShotgunMaxSpeedForLiftImpulse" min="0.0" max="100000" init="5.0" description="The maximum movement speed of the target ped at which the shotgun lift will be applied"/>
    <float name="m_ShotgunMaxLiftImpulse" min="0.0" max="100000" init="12.0" description="The maximum lifting impulse that can be applied by the shotgun"/>
    <float name="m_ShotgunLiftNearThreshold" min="0.0" max="100000" init="60.0" description="The maxumum range of the firing ped at which the lifting impule will be applied. Lift impulse scales down from the max over this distance"/>
    <float name="m_ShotgunChanceToMoveSpine3ImpulseToSpine2" min="0.0" max="1.0" init="0.5" description="The chance that a spine 3 impulse will be moved to spine 2. Increasing this reduces the chance of peds being flipped over by high shotgun hits"/>
    <float name="m_ShotgunChanceToMoveNeckImpulseToSpine2" min="0.0" max="1.0" init="0.6" description="The chance that a neck impulse will be moved to spine 2. Increasing this reduces the chance of peds being flipped over by high shotgun hits"/>
    <float name="m_ShotgunChanceToMoveHeadImpulseToSpine2" min="0.0" max="1.0" init="0.7" description="The chance that a head impulse will be moved to spine 2. Increasing this reduces the chance of peds being flipped over by high shotgun hits"/>

    <float name="m_RapidFireBoostShotImpulseMult" min="0.0" max="100000" init="1.2" description="The impulse will be multiplied by this value every few shots when rapid firing"/>
    <int name="m_RapidFireBoostShotMinRandom" min="1" max="50" init="2" description="The minimum of the random range used to pick the next rapid fire boost shot (maximum=m_RapidFireBoostShotMaxRandom)"/>
    <int name="m_RapidFireBoostShotMaxRandom" min="1" max="50" init="4" description="The maximum of the random range used to pick the next rapid fire boost shot (minimum=m_RapidFireBoostShotMinRandom)"/>
    
    <float name="m_EqualizeAmount" min="0.0" max="1.0" init="0.0" description="The equalize amount to apply to the impulse. A value of 0.0 means the impulse is applied directly. a value of 1.0f means the impulse is fully mass relative (the impulse will be multiplied by the mass in nm)."/>

    <float name="m_COMImpulseScale" min="0.0" max="100000.0" init="0.0" description="The scale of an additional impulse applied to the COM position in the same direction as the normal shot impulse"/>
    <bitset name="m_COMImpulseComponent" type="fixed32" values="RagdollComponent" description="Only hits to these components will apply an additional COM impulse"/>
    <float name="m_COMImpulseMaxRootVelocityMagnitude" min="0.0" max="100.0" init="100.0" description="The maximum root velocity for a ped for which a COM impulse will be applied"/>
    <bool name="m_COMImpulseOnlyWhileBalancing" init="false" description="Should COM velocity be applied only while balancing"/>

    <float name="m_HeadShotImpulseMultiplier" min="0.0" max="100.0" init="1.2" description="Multiplies the head shot impulse in singleplayer (does not include shotgun pelletr)."/>
    <float name="m_HeadShotMPImpulseMultiplier" min="0.0" max="100.0" init="1.2" description="Multiplies the head shot impulse in multiplayer(does not include shotgun pelletr)."/>
    <bool name="m_ScaleHeadShotImpulseWithSpineOrientation" init="false" description="Should the task redue the head shot magnitute as the ped falls over"/>
    <float name="m_MinHeadShotImpulseMultiplier" min="0.0" max="3.0" init="0.5" description="The minimum head shot scale value to be applied when reducing the head shot multiplier as the ped falls over."/>

    <float name="m_AutomaticInitialSnapMult" min="0.0" max="10.0" init="1.2" description="Multiplies the snap magnitude of the first shot from weapons using the Automatic weapon tuning group."/>
    <float name="m_BurstFireInitialSnapMult" min="0.0" max="10.0" init="1.2" description="Multiplies the snap magnitude of the first shot from weapons using the BurstFire weapon tuning group."/>

    <float name="m_FinalShotImpulseClampMax" min="0.0" init="1000.0" description="Clamp the maximum final shot impulse to this value"/>

    <float name="m_RunningAgainstBulletImpulseMult" min="0.0" max="2.0" init="1.0" description="Multiplies the impulse based on how much the ped is running into the direction of the bullet impulse."/>
    <float name="m_RunningAgainstBulletImpulseMultMax" min="0.0" max="4.0" init="2.0" description="Maximum clamp on the multiplier based on how much the ped is running into the direction of the bullet impulse."/>
    <float name="m_RunningWithBulletImpulseMult" min="0.0" max="2.0" init="0.3" description="Multiplies the impulse based on how much the ped is running away from the direction of the bullet impulse."/>

    <float name="m_LegShotFallRootImpulseMinUpright" min="-1.0" max="1.0f" init="0.6" description="Ped must be at least this upright for the extra leg impulse to be applied (-1.0 is upside down, 0.0 is horizontal and 1.0 is vertical)"/>
    <float name="m_LegShotFallRootImpulseMult" min="0.0" max="100.0" init="0.375" description="Applies an aditional impulse with this percentage of the bullet impulse when a ped is shot in the leg"/>

    <struct name="m_SniperImpulses" type="CTaskNMShot::Tunables::Impulses::SniperImpulses" description="Sniper-specific impulse tuning" />
  </structdef>

  <structdef type="CTaskNMShot::Tunables::ArmShot">
    <int name="m_MinLookAtArmWoundTime" min="0" max="100000" init="25" description="The minimum time (in milliseconds) to look at the arm wound"/>
    <int name="m_MaxLookAtArmWoundTime" min="0" max="100000" init="35" description="The maximum time (in milliseconds) to look at the arm wound"/>
    <float name="m_UpperArmImpulseCap" min="0.0" max="500.0" init="50.0" description="The maximum impulse to be applied to the upper arm. The remainder of the impulse will be projected through the arm and applied to any non arm part behind it."/>
    <float name="m_LowerArmImpulseCap" min="0.0" max="500.0" init="25.0" description="The maximum impulse to be applied to the forearms / hands. The remainder of the impulse will be projected through the arm and applied to any non arm part behind it."/>
    <float name="m_ClavicleImpulseScale" min="0.0" max="10.0" init="0.7" description="The amount to scale the bullet impulse remaining after the clamp before applying it to the clavicle as well" />
    <float name="m_UpperArmNoTorsoHitImpulseCap" min="0.0" max="500.0" init="50.0" description="The maximum impulse to be applied to the upper arm. The remainder of the impulse will be projected through the arm and applied to any non arm part behind it."/>
    <float name="m_LowerArmNoTorseHitImpulseCap" min="0.0" max="500.0" init="25.0" description="The maximum impulse to be applied to the forearms / hands. The remainder of the impulse will be projected through the arm and applied to any non arm part behind it."/>
  </structdef>

  <structdef type="CTaskNMShot::Tunables::HitPointRandomisation">
    <bool name="m_Enable" init="false" description="Enables and disable hit point randomisation"/>
    <float name="m_TopSpread" min="0.0" max="1.0" init="0.2" description="The width around the top component that defines the top extents of the randomisation."/>
    <float name="m_BottomSpread" min="0.0" max="1.0" init="0.1" description="The width around the bottom component that defines the bottom extents of the randomisation"/>
    <float name="m_Blend" min="0.0" max="1.0" init="1.0" description="Blends between the actual hit point and the randomised target."/>
    <enum name="m_TopComponent" type="RagdollComponent">RAGDOLL_SPINE3</enum>
    <enum name="m_BottomComponent" type="RagdollComponent">RAGDOLL_BUTTOCKS</enum>
    <float name="m_BiasSide" min="-1.0" max="1.0" init="1.0" description="Alternatively bias the left and right sides by this amount."/>
    <u32 name="m_BiasSideTime" min="0" max="100000" init="0" description="The amount of time (in milliseconds) to bias a given side."/>
  </structdef>

  <structdef type="CTaskNMShot::Tunables::StayUpright">
    <float name="m_HoldingWeaponBonus" min="0.0" max="16.0" init="2.0" description="A bonus to apply to the stayUpright forceStrength parameter if the shot ped is holding a weapon."/>
    <float name="m_UnarmedBonus" min="0.0" max="16.0" init="0.0" description="A bonus to apply to the stayUpright forceStrength parameter if the shot ped is unarmed."/>
    <float name="m_ArmouredBonus" min="0.0" max="16.0" init="0.0" description="A bonus to apply to the stayUpright forceStrength parameter if the shot ped is armoured."/>
    <float name="m_MovingMultiplierBonus" min="0.0" max="1.0" init="0.0" description="Multiplied with the velocity magnitude and added as a bonus to the stayUpright forceStrength/torqueStrength parameters."/>
    <float name="m_HealthMultiplierBonus" min="0.0" max="1.0" init="0.0" description="The amount of the health reduction to apply. 0.0 means ped will have 50% stayUpright forceStrength/torqueStrength at 50% health. 1.0 means ped will still have 100% stayUpright forceStrength/torqueStrength at 50% health."/>
  </structdef>
 
  <structdef type="CTaskNMShot::Tunables::ParamSets">
    <struct name="m_Base" type="CNmTuningSet" description="Sent on every shot" />
    
    <struct name="m_Melee" type="CNmTuningSet" description="Sent on every melee hit"/>
    <struct name="m_Electrocute" type="CNmTuningSet" description="Sent on every stun gun hit"/>
    
    <struct name="m_SprintingLegShot" type="CNmTuningSet" description="Sent when hit in the leg whilst sprinting"/>
    <struct name="m_SprintingDeath" type="CNmTuningSet" description="Sent when killed whilst sprinting"/>
    <struct name="m_Sprinting" type="CNmTuningSet" description="Sent when hit whilst sprinting"/>
    <struct name="m_AutomaticHeadShot" type="CNmTuningSet" description="Sent when hit in the head with automatic weapon fire"/>
    <struct name="m_HeadShot" type="CNmTuningSet" description="Sent when hit in the head"/>
    <struct name="m_AutomaticNeckShot" type="CNmTuningSet" description="Sent when hit in the neck with automatic weapon fire"/>
    <struct name="m_NeckShot" type="CNmTuningSet" description="Sent when hit in the neck"/>
    <struct name="m_SniperLegShot" type="CNmTuningSet" description="Sent when shot in the leg with a sniper rifle"/>    
    <struct name="m_LegShot" type="CNmTuningSet" description="Sent when shot in the leg"/>
    <struct name="m_ArmShot" type="CNmTuningSet" description="Sent when shot in the arm"/>
    <struct name="m_BackShot" type="CNmTuningSet" description="Sent when shot in the back"/>

    <struct name="m_Underwater" type="CNmTuningSet" description="Sent on hit when shot underwater"/>
    <struct name="m_UnderwaterRelax" type="CNmTuningSet" description="Sent on hit when shot underwater and need to relax (shot 5+ times or in head)"/>
    <struct name="m_Armoured" type="CNmTuningSet" description="Sent on hit when the target is armoured"/>
    <struct name="m_BoundAnkles" type="CNmTuningSet" description="Sent on hit when the peds ankles are bound"/>
    <struct name="m_FatallyInjured" type="CNmTuningSet" description="Sent on hit when the ped is fatally injured"/>
    <struct name="m_PlayerDeathSP" type="CNmTuningSet" description="Sent on hit when the player is killed in single-player"/>
    <struct name="m_PlayerDeathMP" type="CNmTuningSet" description="Sent on hit when the player is killed in multi-player"/>
    <struct name="m_OnStairs" type="CNmTuningSet" description="Sent on hit when the ped is on stairs"/>
    
    <struct name="m_ShotAgainstWall" type="CNmTuningSet" description="Sent on hit when against a wall"/>
    <struct name="m_LastStand" type="CNmTuningSet" description="Sent on hit whilst doing last stand"/>
    <struct name="m_LastStandArmoured" type="CNmTuningSet" description="Sent on hit whilst doing last stand and armoured"/>
    <struct name="m_HeadLook" type="CNmTuningSet" description="Parameters for head look"/>
    <struct name="m_FallToKnees" type="CNmTuningSet" description="Parameters for falling to knees (can be triggered at various times)"/>
    <struct name="m_StaggerFall" type="CNmTuningSet" description="Parameters for stagger fall(can be triggered at various times)"/>
    <struct name="m_CatchFall" type="CNmTuningSet" description="Parameters for catch fall(can be triggered at various times)"/>
    <struct name="m_SetFallingReactionHealthy" type="CNmTuningSet" description="Falling reaction parameters for healthy peds( called whe applying bulet impulses)"/>
    <struct name="m_SetFallingReactionInjured" type="CNmTuningSet" description="Falling reaction parameters for injured peds( called whe applying bulet impulses)"/>
    <struct name="m_SetFallingReactionFallOverWall" type="CNmTuningSet" description="Falling reaction parameters when trying to fall over a wall"/>
    <struct name="m_SetFallingReactionFallOverVehicle" type="CNmTuningSet" description="Falling reaction parameters when trying to fall over a vehicle"/>
    <struct name="m_RubberBulletKnockdown" type="CNmTuningSet" description="Called when knocking down peds with a rubber bullet gun"/>
    <struct name="m_Teeter" type="CNmTuningSet" description="teeter params when the opportunity arises (detected in code)"/>

    <struct name="m_HoldingTwoHandedWeapon" type="CNmTuningSet" description="Sent on hit when ped is holding a two handed weapon"/>
    <struct name="m_HoldingSingleHandedWeapon" type="CNmTuningSet" description="Sent on hit when ped is holding a single handed weapon"/>

    <struct name="m_CrouchedOrLowCover" type="CNmTuningSet" description="Sent on hit when ped is crouched or in low cover"/>

    <struct name="m_Female" type="CNmTuningSet" description="Only sent on female peds"/>
  </structdef>

  <structdef type="CTaskNMShot::Tunables" base="CTuning">
    <int name="m_MinimumShotReactionTimePlayerMS" min="0" max="100000" init="450" description="The minimum time used for shot reactions players"/>
    <int name="m_MinimumShotReactionTimeAIMS" min="0" max="100000" init="450" description="The minimum time used for shot reactions for non-players"/>
    <bool name="m_bUseClipPoseHelper" init="false" />
    <bool name="m_bEnableDebugDraw" init="false" />
    <float name="m_fImpactConeAngleFront" min="0.0f" max="180.0" init="45.0f" />
		<float name="m_fImpactConeAngleBack" min="0.0f" max="180.0" init="45.0f" />
    <enum name="m_eImpactConeRagdollComponent" type="RagdollComponent">RAGDOLL_BUTTOCKS</enum>
    <int name="m_iShotMinTimeBeforeGunThreaten" min="0" max="100000" init="0" description="Threatened helper?"/>
    <int name="m_iShotMaxTimeBeforeGunThreaten" min="0" max="100000" init="500" description="Threatened helper?"/>
    <int name="m_iShotMinTimeBetweenFireGun" min="0" max="100000" init="100" description="Threatened helper?"/>
    <int name="m_iShotMaxTimeBetweenFireGun" min="0" max="100000" init="500" description="Threatened helper?"/>
    <int name="m_iShotMaxBlindFireTimeL" min="0" max="100000" init="400" description="The minimum time (in milliseconds) to blind fire for"/>
    <int name="m_iShotMaxBlindFireTimeH" min="0" max="100000" init="1000" description="The maximum time (in milliseconds) to blind fire for"/>
    <int name="m_BlendOutDelayStanding" min="0" max="100000" init="500" description="The time (in milliseconds) The ped has to have successfully balanced for to blend out."/>
    <int name="m_BlendOutDelayBalanceFailed" min="0" max="100000" init="500" description="The time (in milliseconds) The ped has to have settled for to blend out after falling over."/>
    <float name="m_fShotBlindFireProbability" min="0.0" max="1.0" init="0.1" description="Chance to blind fire"/>
    <float name="m_fShotWeaponAngleToFireGun" min="-1.0" max="1.0" init="0.9" description="Min angle of weapon muzzle vector to the target to fire the weapon when pointing gun"/>
    <float name="m_fShotHeadAngleToFireGun" min="-1.0" max="1.0" init="0.2" description="Min angle of wthe peds head forward vector to the target to fire the weapon when pointing gun"/>
    <float name="m_fFireWeaponStrengthForceMultiplier" min="0.0" max="10000.0" init="10.0" description="The amount the forcehitped from weapondata is multiplied by to set the recoil strength when firing a gun"/>
    <float name="m_fMinFinisherShotgunTotalImpulseNormal" min="0.0" max="10000.0" init="200.0" description="The total amount of impulse to ensure a ped receives from a shotgun blast - regardless of the number of pellets that hit"/>
    <float name="m_fMinFinisherShotgunTotalImpulseBraced" min="0.0" max="10000.0" init="300.0" description="The total amount of impulse to ensure a braced ped receives from a shotgun blast - regardless of the number of pellets that hit"/>
    <float name="m_fFinisherShotgunBonusArmedSpeedModifier" min="0.0" max="100.0" init="1.5" description="The impulse modifier for armed peds that are moving towards the shooter"/>
    <bool name="m_ScaleSnapWithSpineOrientation" init="true" description="Should the task redue the snap magnitute as the ped falls over"/>
    <float name="m_MinSnap" min="0.0" max="3.0" init="0.35" description="The minimum snap scale value to be applied when reducing the snap value as the ped falls over."/>
    <struct name="m_ShotAgainstWall" type="CTaskNMShot::Tunables::ShotAgainstWall" />
    <float name="m_BCRExclusionZone" min="0.0" max="100000" init="0.2" description="Exclusion zone for the balancer collision reaction"/>
    <struct name="m_Impulses" type="CTaskNMShot::Tunables::Impulses" />
    <struct name="m_HitRandomisation" type="CTaskNMShot::Tunables::HitPointRandomisation" />
    <struct name="m_HitRandomisationAutomatic" type="CTaskNMShot::Tunables::HitPointRandomisation" />
    <struct name="m_StayUpright" type="CTaskNMShot::Tunables::StayUpright" />
    <struct name="m_ArmShot" type="CTaskNMShot::Tunables::ArmShot" />
    <float name="m_FallingSpeedForHighFall" min="0.0" max="100.0" init="4.0" description="At this falling speed the ped will switch to a high fall behaviour"/>
    <bool name="m_ReduceDownedTimeByPerformanceTime" init="true" description="If true, the downed time will be reduced by the performance time, to a minimum of m_MinimumDownedTime."/>
    <int name="m_MinimumDownedTime" min="0" max="100000" init="2000" description="The minimum downed time (in milliseconds) to be used for rubber bullets and stun gun"/>
    <float name="m_ChanceOfFallToKneesOnCollapse" min="0.0" max="1.0" init="0.2" description="Probability that the ped will send the fall to knees message when collapsing. If fall to knees is not used, the staggerfall message will be used instead"/>
    <float name="m_ChanceOfFallToKneesAfterLastStand" min="0.0" max="1.0" init="0.0" description="Probability that the ped will send the fall to knees message when collapsing. If fall to knees is not used, the staggerfall message will be used instead"/>
	<float name="m_ChanceForGutShotKnockdown" min="0.0" max="1.0" init="0.3" description="Probability that the ped will be knocked down (run the StaggerFall parameter set) when hit in spine0 or spine1 from the front"/>
	  
    <float name="m_LastStandMaxTotalTime" min="0.0" init="0.6" description="The maximum amount of time to let the last stand behaviour run" />
    <float name="m_LastStandMaxArmouredTotalTime" min="0.0" init="0.6" description="The maximum amount of time to let the last stand behaviour run for an armoured ped" />

    <int name="m_RapidHitCount" min="0" init="3" description="The number of rapid fire hits before a ped is considered to be taking rapid fire"/>
    <int name="m_ArmouredRapidHitCount" min="0" init="3" description="The number of rapid fire hits before an armoured ped is considered to be taking rapid fire"/>

    <bool name="m_AllowArmouredLegShot" init="true" description="Allow armoured peds to use the leg shot parameter sets" />
    <bool name="m_AllowArmouredKnockdown" init="true" description="Allow armoured peds to be knocked down" />

    <bool name="m_DisableReachForWoundOnHeadShot" init="true" description="If true, reach for wound will be disabled after a head shot."/>
    <int name="m_DisableReachForWoundOnHeadShotMinDelay" min="0" max="100000" init="250" description="The minimum time (in milliseconds) to delay before disabling reach for wound when shot in the head"/>
    <int name="m_DisableReachForWoundOnHeadShotMaxDelay" min="0" max="100000" init="300" description="The maximum time (in milliseconds) to delay before disabling reach for wound when shot in the head"/>
    <bool name="m_DisableReachForWoundOnNeckShot" init="true" description="If true, reach for wound will be disabled after a neck shot."/>
    <int name="m_DisableReachForWoundOnNeckShotMinDelay" min="0" max="100000" init="250" description="The minimum time (in milliseconds) to delay before disabling reach for wound when shot in the neck"/>
    <int name="m_DisableReachForWoundOnNeckShotMaxDelay" min="0" max="100000" init="300" description="The maximum time (in milliseconds) to delay before disabling reach for wound when shot in the neck"/>
    <struct name="m_ParamSets" type="CTaskNMShot::Tunables::ParamSets"/>
    <struct name="m_WeaponSets" type="CNmTuningSetGroup"/>
    <struct name="m_BlendOutThreshold" type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds"/>
    <struct name="m_SubmergedBlendOutThreshold" type="CTaskNMBehaviour::Tunables::BlendOutThreshold"/>
  </structdef>

  <structdef type="CTaskNMControl::Tunables::DriveToGetup">
    <bool name="m_AllowDriveToGetup" init="true" description="Enable / disable the drive to getup functionality."/>
    <bool name="m_OnlyAllowForShot" init="false" description="Only use the drive to getup whilst shot is running."/>
    <bool name="m_AllowWhenBalanced" init="false" description="If true, the ped will be able top drive to the blend out animation whilst balancing"/>
    <float name="m_MinHealth" min="0.0" max="500.0" init="0.0" description="Ped's health must be ablove this threshold to perform the drive to getup"/>
    <float name="m_MaxSpeed" min="0.0" max="10.0" init="0.75" description="The root speed must be lower than this threshold to perform the drive to getup"/>
    <float name="m_MaxUprightRatio" min="0.0" max="100.0" init="1.0" description="How 'upright' the ped can be for driving to the getup" />
    <u32 name="m_MatchTimer" min="0" max="10000" init="100" description="How long to wait before trying to perform a new pose match for drive to getup" />
  </structdef>

  <structdef type="CTaskNMControl::Tunables" base="CTuning">
    <struct name="m_DriveToGetup" type="CTaskNMControl::Tunables::DriveToGetup" />
    <struct name="m_OnEnableDriveToGetup" type="CNmTuningSet" description="Sent when enabling drive to getup"/>
    <struct name="m_OnDisableDriveToGetup" type="CNmTuningSet" description="Sent when disabling drive to getup"/>
  </structdef>

  <structdef type="CTaskNMElectrocute::Tunables" base="CTuning">
    <enum name="m_InitialForceComponent" type="RagdollComponent">RAGDOLL_SPINE1</enum>
    <struct name="m_InitialForce" type="CTaskNMBehaviour::Tunables::TunableForce" description="Tunable force to apply to the bumped ped"/>
    <Vector3 name="m_InitialForceOffset" init="0.0" min="-10.0" max="10.0" description="Local offset from the component to apply the impulse"/>

    <float name="m_FallingSpeedForHighFall" init="4.0f" min="0.0f" max="3600.0f" description="If the ped is falling downwards faster than this speed, and we're no longer being electrocuted, transition to a high fall task" />
    
    <struct name="m_Start" type="CNmTuningSet" description="Sent when the behaviour starts"/>
    <struct name="m_Walking" type="CNmTuningSet" description="Sent at the start of the behaviour when the ped is walking"/>
    <struct name="m_Running" type="CNmTuningSet" description="Sent at the start of the behaviour when the ped is running"/>
    <struct name="m_Sprinting" type="CNmTuningSet" description="Sent at the start of the behaviour when the ped is sprinting"/>

    <struct name="m_OnBalanceFailed" type="CNmTuningSet" description="Sent once when the balancer fails"/>
    <struct name="m_OnCatchFallSuccess" type="CNmTuningSet" description="Sent once when catch fall succeeds"/>
    <struct name="m_OnElectrocuteFinished" type="CNmTuningSet" description="Sent when the electrocute timer runs out."/>
  </structdef>


  <structdef type="CTaskNMOnFire::Tunables" base="CTuning">
    <struct name="m_Start" type="CNmTuningSet" description="Sent once when the on fire task starts"/>
    <struct name="m_Weak" type="CNmTuningSet" description="Sent along with the start set when the on fire tasks starts on weak peds"/>
    <struct name="m_Update" type="CNmTuningSet" description="Sent every frame"/>
    <struct name="m_OnBalanceFailed" type="CNmTuningSet" description="Sent once when the peds balance fails"/>
  </structdef>
  
  <structdef type="CTaskNMBehaviour::Tunables::ActivationLimitModifiers">
    <float name="m_BumpedByCar" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when hit by a car"/>
    <float name="m_BumpedByCarFriendly" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when hit by a car driven by a friendly ped (after the BumpedByCar limit has been applied)"/>
    <float name="m_PlayerBumpedByCar" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when the player is hit by a car (after the BumpedByCar and friendly limits have been applied)" />
    <float name="m_MinVehicleWarning" min="0.0" max="500.0" init="1.0" description="The minimum required 'impulse' to activate the brace behaviour using the ai scanner (also checks the other hit by car limits)"/>
    <float name="m_BumpedByPedMinVel" min="0.0" max="10.0" init="1.5" description="the minimum speed the player must be moving at to activate nm on another ped"/>
    <float name="m_BumpedByPedMinDotVel" min="-1.0" max="1.0" init="0.6" description="determines how directly the ped must bump into another ped to activate nm"/>
    <float name="m_BumpedByPed" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when bumped by a ped"/>
    <float name="m_BumpedByPlayerRagdoll" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when bumped by a player ped in ragdoll"/>
    <float name="m_BumpedByPedRagdoll" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when bumped by a ped in ragdoll"/>
    <float name="m_BumpedByPedFriendly" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when bumped by a friendly ped" />
    <float name="m_BumpedByPedIsQuadruped" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when the bumped ped is a quadruped" />
    <float name="m_BumpedByObject" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when bumped by an object" />
    <float name="m_Walking" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when walking" />
    <float name="m_Running" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when running" />
    <float name="m_Sprinting" min="0.0" max="500.0" init="1.0" description="Multiplies the base activation limit when sprinting" />
    <float name="m_MaxPlayerActivationLimit" min="0.0" max="100000.0f" init="1000.0f" description="The maximum activation limit after all of the above modifiers have been applied" />
    <float name="m_MaxAiActivationLimit" min="0.0" max="100000.0f" init="1000.0f" description="The maximum activation limit after all of the above modifiers have been applied" />
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::TunableForce">
    <bool name="m_Enable" init="false" description="Enables and disables the force"/>

    <float name="m_Mag" init="0.0" min="0.0f" max="100000.0f" description="The force to apply" />

    <bool name="m_ScaleWithVelocity" init="false" description="Scales the force with the passed in velocity"/>
    <float name="m_VelocityMin" init="0.0" min="0.0f" max="3600.0f" description="ForceAtMinVel will be applied at this velocity (or lower)" />
    <float name="m_VelocityMax" init="10.0" min="0.0f" max="3600.0f" description="ForceAtMaxVel will be applied at this velocity (or higher)" />
    <float name="m_ForceAtMinVelocity" init="0.0" min="0.0f" max="1.0f" description="The proporton of the force to apply at min vel" />
    <float name="m_ForceAtMaxVelocity" init="1.0" min="0.0f" max="1.0f" description="The proporton of the force to apply at max vel" />

    <bool name="m_ClampImpulse" init="true" description="Scales the force with the passed in velocity"/>
    <float name="m_MinImpulse" init="0.0f" min="0.0f" max="10000000.0f" description="the minimum impulse magnitude allowed (after scaling / timestep etc)" />
    <float name="m_MaxImpulse" init="4500.f" min="0.0f" max="10000000.0f" description="the maximum impulse magnitude allowed (after scaling / timestep etc)" />

    <int name="m_Delay" init="200" min="0" max="100000" description="The initial delay on applying the force" />
    <int name="m_Duration" init="200" min="0" max="100000" description="The duration to apply the force over" />
    
    <bool name="m_ScaleWithUpright" init="true"/>
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::PedCapsuleVehicleImpactTuning">
    <bool name="m_EnableActivationsFromCapsuleImpacts" init="false" description="Enables and disables brace activations from capsule impacts"/>
    <float name="m_VehicleVelToImpactNormalMinDot" init="0.15" min="0.0f" max="100000.0f" description="The min dot product of the vehicle velocity against the impact normal to activate the ragdoll on direct capsule hits" />
    <bool name="m_EnableSideSwipeActivations" init="false" description="Enables and disables brace activations vehicle side swipes"/>
    <bool name="m_EnableSideSwipeActivationsFirstPerson" init="false" description="Enables and disables brace activations vehicle side swipes when in first person camera mode"/>
    <float name="m_MinSideNormalForSideSwipe" init="0.8f" min="0.0f" max="100000.0f" description="The absolute min dot product of the vehicle side vector against the impact normal to consider the impact valid for side swipe activation" />
    <float name="m_MinVelThroughNormalForSideSwipe" init="0.5f" min="0.0f" max="100000.0f" description="The min dot product of the vehicle relative velocity against the impact normal to consider the impact valid for side swipe activation" />
    <float name="m_MinAccumulatedImpactForSideSwipe" init="5.0f" min="0.0f" max="100000.0f" description="The total impact amount we have to accumulate before activating the side swipe" />
    <float name="m_MinVehVelMagForSideSwipe" init="6.0f" min="0.0f" max="100000.0f" description="The minimum velocity magnitude of the vehicle to allow sideswipe activations." />
    <float name="m_MinVehVelMagForBicycleSideSwipe" init="6.0f" min="0.0f" max="100000.0f" description="The minimum velocity magnitude of bicycles to allow sideswipe activations." />
    <float name="m_MinAccumulatedImpactForSideSwipeCNC" init="5.0f" min="0.0f" max="100000.0f" description="The total impact amount we have to accumulate before activating the side swipe (in CNC mode)." />
    <float name="m_MinVehVelMagForSideSwipeCNC" init="6.0f" min="0.0f" max="100000.0f" description="The minimum velocity magnitude of the vehicle to allow sideswipe activations (in CNC mode)." />
    <float name="m_MinVehVelMagForBicycleSideSwipeCNC" init="6.0f" min="0.0f" max="100000.0f" description="The minimum velocity magnitude of bicycles to allow sideswipe activations (in CNC mode)." />
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::InverseMassScales">
    <bool name="m_ApplyVehicleScale" init="false" description="Apply the vehicle inverse mass scaling modifier"/>
    <float name="m_VehicleScale" init="1.0" min="0.0f" max="2.0f" description="The inverse mass scale to apply" />
    <bool name="m_ApplyPedScale" init="false" description="Apply the ped inverse mass scaling modifier"/>
    <float name="m_PedScale" init="1.0" min="0.0f" max="2.0f" description="The inverse mass scale to apply" />
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::BoundWeight">
    <enum name="m_Bound" type="RagdollComponent">RAGDOLL_SPINE0</enum>
    <float name="m_Weight" min="0.0" max="500.0" init="1.0" description="the weight for this particular ragdoll bound"/>
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::RagdollUnderWheelTuning">
    <float name="m_fImpulseMultLimbs" min="0.0" max="60.0" init="5.0" description="The upwards impulse applied to the limbs when a wheel presses down on a ped"/>
    <float name="m_fImpulseMultSpine" min="-60.0" max="0.0" init="-5.0" description="The downwards impulse applied to the spine when a wheel presses down on a ped"/>
    <float name="m_fMinSpeedForPush" min="0.0" max="30.0" init="5.0" description="The minimum car speed needed to push a ped instead of compressing it"/>
    <float name="m_fFastCarPushImpulseMult" min="0.0" max="60.0" init="8.0" description="The impulse multiplier a fast car uses to push a ped"/>
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::KickOnGroundTuning">
    <float name="m_fPronePedKickImpulse" min="0.0" max="500.0" init="200.0" description="The impulse applied when kicking a prone ped"/>
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::BlendOutThreshold">
    <float name="m_MaxLinearVelocity" min="0.0" max="500.0" init="0.2" description="The ragdolls overall linear velocity must lower than this to blend out"/>
    <float name="m_MaxAngularVelocity" min="0.0" max="500.0" init="1.0" description="The ragdolls overall angular velocity must lower than this to blend out"/>
    <int name="m_SettledTimeMS" init="100" min="0" max="100000" description="The amount of time the ragdoll must remain continuously below the thresholds before blending back to animation. If randomising, this is the max time" />
    <bool name="m_RandomiseSettledTime" init="false" description="Randomise the settled time"/>
    <int name="m_SettledTimeMinMS" init="50" min="0" max="100000" description="Only used when randomising the settle time. Defines the minimum of the random range" />
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds">
    <struct name="m_Ai" type="CTaskNMBehaviour::Tunables::BlendOutThreshold" description="Thresholds for standard ai blend outs" />
    <struct name="m_Player" type="CTaskNMBehaviour::Tunables::BlendOutThreshold" description="Thresholds for standard player blend outs" />
    <struct name="m_PlayerMp" type="CTaskNMBehaviour::Tunables::BlendOutThreshold" description="Thresholds for standard multiplayer player blend outs" />
  </structdef>

  <structdef type="CGrabHelper::Tunables" base="CTuning">
    <struct name="m_Sets" type="CNmTuningSetGroup" description="Grab helper tuning set parameters."/>
  </structdef>

  <structdef type="CTaskNMBehaviour::Tunables" base="CTuning" onPostLoad="ParserPostLoad">

    <bool name="m_EnableRagdollPooling" init="true" description="Manage ragdoll usage using the ragdoll pooling system in single player"/>
    <int name="m_MaxGameplayNmAgents" init="3" min="0" max="10" description="The maximum number of nm agents that in game can activate at once" onWidgetChanged="OnMaxGameplayAgentsChanged" />
    <int name="m_MaxRageRagdolls" init="3" min="0" max="20" description="The maximum number of simultaneous allowed rage ragdolls" onWidgetChanged="OnMaxRageRagdollsChanged"/>
    <bool name="m_ReserveLocalPlayerNmAgent" init="true" description="Always allow the player to activate an nm agent on the local player" onWidgetChanged="OnMaxPlayerAgentsChanged" />

    <bool name="m_EnableRagdollPoolingMp" init="false" description="Manage ragdoll usage using the ragdoll pooling system in multi player"/>
    <int name="m_MaxGameplayNmAgentsMp" init="10" min="0" max="10" description="The maximum number of nm agents that in game can activate at once" onWidgetChanged="OnMaxGameplayAgentsChanged" />
    <int name="m_MaxRageRagdollsMp" init="20" min="0" max="20" description="The maximum number of simultaneous allowed rage ragdolls" onWidgetChanged="OnMaxRageRagdollsChanged"/>
    <bool name="m_ReserveLocalPlayerNmAgentMp" init="true" description="Always allow the player to activate an nm agent on the local player" onWidgetChanged="OnMaxPlayerAgentsChanged" />

    <bool name="m_BlockOffscreenShotReactions" init="false" description="Block ragdoll activation from shot reactions on offscreen peds"/>

    <bool name="m_UsePreEmptiveEdgeActivation" init="false" description="Triggers the ragdoll when the player is about to walk or run off of an edge"/>
    <bool name="m_UsePreEmptiveEdgeActivationMp" init="false" description="Triggers the ragdoll when the player is about to walk or run off of an edge in multiplayer"/>
    <bool name="m_UseBalanceForEdgeActivation" init="true" description="Uses the balance task (instead of the smart fall) when activating because we're about to run off an edge"/>
    <float name="m_PreEmptiveEdgeActivationMaxVel" min="0.0" max="100.0" init="2.0f" description="Don't use pre-emptive edge activation when moving faster than this speed" />
    <float name="m_PreEmptiveEdgeActivationMaxHeadingDiff" min="0.0" max="100.0" init="0.5f" description="Don't use pre-emptive edge activation when the desired heading is more than this amount away from the edge" />
    <float name="m_PreEmptiveEdgeActivationMinDotVel" min="-100.0" max="100.0" init="0.4f" description="Don't use pre-emptive edge activation when the dot product of the velocity vector against the vector to the edge is less than this value" />
    <float name="m_PreEmptiveEdgeActivationMaxDistance" min="0.0" max="100.0" init="1.0f" description="Don't use pre-emptive edge activation when the distance to the edge is greater than this value" />
    <float name="m_PreEmptiveEdgeActivationMinDesiredMBR2" min="0.0" max="100.0" init="1.0f" description="Don't use pre-emptive edge activation when the desired move blend ratio (squared) of the ped is less than this value" />

    <struct name="m_StandardBlendOutThresholds" type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds" description="Thresholds used for most nm blend outs" />
    <array name="m_CamAttachPositionWeights" type="atArray">
      <struct type="CTaskNMBehaviour::Tunables::BoundWeight" description="Ragdoll component bound weights for calculating camera attachg positions in ragdoll" />
    </array>
    <struct name="m_SpActivationModifiers" type="CTaskNMBehaviour::Tunables::ActivationLimitModifiers" />
    <struct name="m_MpActivationModifiers" type="CTaskNMBehaviour::Tunables::ActivationLimitModifiers" />
    <float name="m_PlayerBumpedByCloneCarActivationModifier" min="0.0" max="100.0" init="1.0" description="The activation limit is multiplied by this when a player ped is hit by a clone vehicle in a mp game" />
    <float name="m_ClonePlayerBumpedByCarActivationModifier" min="0.0" max="100.0" init="1.0" description="The activation limit is multiplied by this when a clone player ped is hit by a vehicle in a mp game" />
    <float name="m_ClonePedBumpedByCarActivationModifier" min="0.0" max="100.0" init="1.0" description="The activation limit is multiplied by this when a clone ped is hit by a vehicle in a mp game" />
    <float name="m_MaxVehicleCapsulePushTimeForRagdollActivation" init="1.0" min="0.0" max="100.0" description="The amount of time (in seconds) the ped capsule can be continuously pushed along by a car before activating the ragdoll" />
    <float name="m_MaxVehicleCapsulePushTimeForPlayerRagdollActivation" init="0.5" min="0.0" max="100.0" description="The amount of time (in seconds) the player ped capsule can be continuously pushed along by a car before activating the ragdoll" />
    <float name="m_VehicleMinSpeedForContinuousPushActivation" min="0.0" max="500.0" init="1.5" description="The minimum vehicle movement speed to activate a ped ragdoll from continuously pushing them" />
    <float name="m_MinContactDepthForContinuousPushActivation" min="-10.0" max="10.0" init="0.0" description="The minimum impact depth the ped must experience every frame to be considered pushed by th vehicle" />
    <float name="m_DurationRampDownCapsulePushedByVehicle" min="0.0" max="10.0" init="0.2" description="The amount of time without a vehicle impact before the timer indicating the ped is being pushed by a vehicle is reset" />
    <float name="m_VehicleMinSpeedForAiActivation" min="0.0" max="500.0" init="0.5" description="The minimum vehicle movement speed to activate an ai ped ragdoll" />
    <float name="m_VehicleMinSpeedForStationaryAiActivation" min="0.0" max="500.0" init="0.2" description="The minimum vehicle movement speed to activate an ai ped ragdoll when the ped is stationary" />
    <float name="m_VehicleMinSpeedForPlayerActivation" min="0.0" max="500.0" init="4.0" description="The minimum vehicle movement speed to activate a player ped ragdoll" />
    <float name="m_VehicleMinSpeedForStationaryPlayerActivation" min="0.0" max="500.0" init="0.5" description="The minimum vehicle movement speed to activate an player ped ragdoll when the ped is stationary" />
    <float name="m_VehicleMinSpeedForWarningActivation" min="0.0" max="500.0" init="10.0" description="The minimum vehicle movement speed to activate a player ped ragdoll" />
    <float name="m_VehicleFallingSpeedWeight" min="0.0" max="10.0" init="1.0" description="An amount used to weight the z-direction speed of the vehicle" />
    <float name="m_VehicleActivationForceMultiplierDefault" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from vehicles versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierBicycle" min="0.0" max="10000.0" init="0.02" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from bycicles versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierBike" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from motorbikes versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierBoat" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from boats versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierPlane" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from planes versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierQuadBike" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from quad bikes versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierHeli" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from helicopters versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierTrain" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from trains versus from objects and peds" />
    <float name="m_VehicleActivationForceMultiplierRC" min="0.0" max="10000.0" init="0.008" description="Multiplies the velocity of the vehicle in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from RC cars versus from objects and peds" />

    <bool name="m_ExcludePedBumpAngleFromPushCalculation" init="false" description="If true, the direction to the other ped will not be taken into accound when calculating bump activation forces."/>
    <float name="m_PedActivationForceMultiplier" min="0.0" max="10000.0" init="1.0" description="Multiplies the velocity of the player in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from ped hits versus from objects and vehicles" />
   
    <float name="m_ObjectMinSpeedForActivation" min="0.0" max="10000.0" init="0.5" description="The minimum speed an object must be moving at to activate a peds ragdoll" />
    <float name="m_ObjectActivationForceMultiplier" min="0.0" max="10000.0" init="1.0" description="Multiplies the velocity of the object in the direction of the ped to give the activation push force. Use to tune the general ease of nm activations from object hits versus from vehicles and peds" />

    <int name="m_MaxPlayerCapsulePushTimeForRagdollActivation" init="1000" min="0" max="100000" description="The amount of time the ped capsule can be continuously pushed against by the player capsule before activating the ragdoll" />
    <float name="m_PlayerCapsuleMinSpeedForContinuousPushActivation" min="0.0" max="500.0" init="1.5" description="The minimum player ped capsule movement speed (desired) to activate a ped ragdoll from continuously pushing them" />

    <float name="m_StuckOnVehicleMaxTime" min="0.0f" max="100.0f" init="3.0f" description="If the ped is stuck on a vehicle for more than this amount of time then they are no longer considered stuck on the vehicle and we allow them to stand up" />
    <struct name="m_StuckOnVehicleBlendOutThresholds" type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds" description="Blend out thresholds used for peds stuck on vehicles" />   
    
    <struct name="m_Start" type="CNmTuningSet" description="Sent once whenever a new nm behaviour task starts"/>

    <struct name="m_TeeterControl" type="CNmTuningSet" description="Sent while teeter is running on players."/>

    <struct name="m_animPoseDefault" type="CNmTuningSet" description="Default anim pose set"/>
    <struct name="m_animPoseAttachDefault" type="CNmTuningSet" description="Default attach anim pose set"/>
    <struct name="m_animPoseAttachToVehicle" type="CNmTuningSet" description="Attach to vehicle anim pose"/>
    <struct name="m_animPoseHandsCuffed" type="CNmTuningSet" description="Hands cuffed anim pose"/>
    <struct name="m_forceFall" type="CNmTuningSet" description="Used to force the ped to fall down"/>
    <struct name="m_RagdollUnderWheelTuning" type="CTaskNMBehaviour::Tunables::RagdollUnderWheelTuning" description="Impulse tuning for ragdolls acted upon by wheels" />
    <struct name="m_KickOnGroundTuning" type="CTaskNMBehaviour::Tunables::KickOnGroundTuning" description="Tuning related to kicking prone peds" />
    <struct name="m_CapsuleVehicleHitTuning" type="CTaskNMBehaviour::Tunables::PedCapsuleVehicleImpactTuning" description="Tuning related to activating ped ragdolls from vehicle capsule hits" />
  </structdef>
  
  <structdef type="CTaskNMPrototype::Tunables::TimedTuning">
    <float name="m_TimeInSeconds" init="5.0f" min="0.0f" max="3600.0f" description="Time after the start of the task that this tuning set will be sent." />
    <bool name="m_Periodic" init="false" description="If true, the tuning set will be sent repeatedly, with a delay of TimeInSeconds between each run"/>
    <struct name="m_Messages" type="CNmTuningSet" description="The messages to send"/>
  </structdef>

  <structdef type="CTaskNMPrototype::Tunables" base="CTuning" onPreAddWidgets="AddPrototypeWidgets">
    <bool name="m_RunForever" init="false" description="If true, the prototype task will continue to run indefinitely"/>
    <int name="m_SimulationTimeInMs" min="0" max="60000" init="2000" description="The minimum time the task will run for (assuming RunForever is false)"/>
    <bool name="m_CheckForMovingGround" init="true" description="If true, the prototype task will check whether it's on moving ground and, if so, send the appropriate configure balance parameter."/>
    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start of the task"/>
    <struct name="m_Update" type="CNmTuningSet" description="Sent every time the task updates"/>
    <struct name="m_OnBalanceFailed" type="CNmTuningSet" description="Sent once after the balance failed feedback message is recieved"/>
    <array name="m_TimedMessages" type="atArray">
      <struct type="CTaskNMPrototype::Tunables::TimedTuning" description="Timed tuning messages" />
    </array>
    <struct name="m_DynamicSet1" type="CNmTuningSet" description="Sent on demand from RAG"/>
    <struct name="m_DynamicSet2" type="CNmTuningSet" description="Sent on demand from RAG"/>
    <struct name="m_DynamicSet3" type="CNmTuningSet" description="Sent on demand from RAG"/>
  </structdef>

  <structdef type="CTaskNMHighFall::Tunables" base="CTuning" onPreAddWidgets="PreAddWidgets">
  
    <struct name="m_PitchInDirectionForce" type="CTaskNMBehaviour::Tunables::TunableForce" description="Tunable force to apply to the bumped ped"/>
    <int name="m_PitchInDirectionComponent" min="-1" max="21" init="10" description="Component to apply the initial force."/>

    <struct name="m_StuntJumpPitchInDirectionForce" type="CTaskNMBehaviour::Tunables::TunableForce" description="Tunable force to apply to the bumped ped"/>
    <int name="m_StuntJumpPitchInDirectionComponent" min="-1" max="21" init="10" description="Component to apply the initial force."/>

    <int name="m_HighFallTimeToBlockInjuredOnGround" min="0" max="60000" init="3500" description="If the behaviour runs for longer than this, the injured on ground behaviour when dead is disabled."/>

    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start of the task"/>

    <struct name="m_InAir" type="CNmTuningSet" description="Sent when starting the task in the in air entry state"/>
    <struct name="m_Vault" type="CNmTuningSet" description="Sent when starting the task in the vault entry state"/>
    <struct name="m_FromCarHit" type="CNmTuningSet" description="Sent when starting the task in the from car hit entry state"/>
    <struct name="m_SlopeSlide" type="CNmTuningSet" description="Sent when starting the task in the slope slide entry state"/>
    <struct name="m_TeeterEdge" type="CNmTuningSet" description="Sent when starting the task in the teeter edge entry state"/>
    <struct name="m_SprintExhausted" type="CNmTuningSet" description="Sent when starting the task in the sprint exhausted entry state"/>
    <struct name="m_JumpCollision" type="CNmTuningSet" description="Sent when starting the task in the jump collision entry state"/>
    <struct name="m_StuntJump" type="CNmTuningSet" description="Sent when starting the task in the stunt jump state"/>
    <struct name="m_OnBalanceFailedSprintExhausted" type="CNmTuningSet" description="Sent when the peds balance fails whilst using the spring exhausted behaviour"/>
    <bool name="m_DisableStartMessageForSprintExhausted" init="true" description="If true, the start tuning set won't be sent when using sprint exhausted entry state"/>

    <struct name="m_Update" type="CNmTuningSet" description="Sent every time the task updates"/>
  
    <struct name="m_BlendOut" type="CTaskNMBehaviour::Tunables::StandardBlendOutThresholds"/>

    <struct name="m_PlayerQuickBlendOut" type="CTaskNMBehaviour::Tunables::BlendOutThreshold"/>
    <struct name="m_MpPlayerQuickBlendOut" type="CTaskNMBehaviour::Tunables::BlendOutThreshold"/>
    <float name="m_MaxHealthLossForQuickGetup" init="10.0f" min="0.0f" max="500.0f" description="the maximum amount of health the ped can lose during the fall and still get up quickly" />
    <float name="m_MinHealthForQuickGetup" init="100.0f" min="0.0f" max="500.0f" description="the minimum amount of health the ped must have in order to perform a quick getup" />

    <float name="m_MpMaxHealthLossForQuickGetup" init="10.0f" min="0.0f" max="500.0f" description="the maximum amount of health the ped can lose during the fall and still get up quickly" />
    <float name="m_MpMinHealthForQuickGetup" init="100.0f" min="0.0f" max="500.0f" description="the minimum amount of health the ped must have in order to perform a quick getup" />

    <bool name="m_UseRemainingMinTimeForGroundWrithe" init="true" description="If true, set the min time of the ground writhe task to the remaining min time of this task"/>
    <int name="m_MinTimeRemainingForGroundWrithe" min="0" max="60000" init="500" description="If we've hit our blend out threshold, but the behaviour min time remaining is set longer than this, switch to ground writhe to handle the remaining time."/>
    <int name="m_MinTimeElapsedForGroundWrithe" min="0" max="60000" init="800" description="The high fall task must have been running for at least this length of time before transitioning to the ground writhe"/>

    <struct name="m_HighHighFallStart" type="CNmTuningSet" description="Sent when the player is sufficiently high and falling sufficiently fast"/>
    <struct name="m_SuperHighFallStart" type="CNmTuningSet" description="Sent when the player is sufficiently high and falling sufficiently fast"/>
    <struct name="m_HighHighFallEnd" type="CNmTuningSet" description="Sent when the player is no longer sufficiently high or no longer falling sufficiently fast"/>

    <u8 name="m_AirResistanceOption" min="0" max="4" init="2" description= "Change the way the new air resistance (AR) forces are applied: 0 - No change, 1 - Fake the health so even dead characters act 'alive', 2 - Don't apply (AR) forces to dead peds, 3 - Don't apply (AR) forces to AI, 4 - Scale (AR) forces based on health"/>
    
    <float name="m_DistanceZThresholdForHighHighFall" min="0.0f" max="10000.0f" init="20.0f" description="We're considered to be falling from a high height if more than this high above the ground"/>
    <float name="m_VelocityZThresholdForHighHighFall" min="-1000.0f" max="1000.0f" init="-6.0f" description="We're considered to be falling from a high height if falling faster than this speed"/>
    <float name="m_VelocityZThresholdForSuperHighFall" min="-1000.0f" max="1000.0f" init="-15.0f" description="We're considered to be falling from a super high height if falling faster than this speed"/>
    <array name="m_RagdollComponentAirResistanceForce" type="member" size="RAGDOLL_NUM_COMPONENTS" hideWidgets="true">
      <float />
    </array>
    <array name="m_RagdollComponentAirResistanceMinStiffness" type="member" size="RAGDOLL_NUM_JOINTS" hideWidgets="true">
      <float />
    </array>
  </structdef>

  <structdef type="CTaskNMThroughWindscreen::Tunables" autoregister="true" base="CTuning">
    <float name="m_GravityScale" min="0.0f" max="1000.0f" init="100.0f"/>
    <float name="m_StartForceDownHeight" min="0.0f" max="100.0f" init="0.0f"/>

    <float name="m_KnockOffBikeForwardMinComponent" min="0.0f" max="100.0f" init="0.1f"/>
    <float name="m_KnockOffBikeForwardMaxComponent" min="0.0f" max="100.0f" init="0.1f"/>
    <float name="m_KnockOffBikeUpMinComponent" min="0.0f" max="100.0f" init="0.05f"/>
    <float name="m_KnockOffBikeUpMaxComponent" min="0.0f" max="100.0f" init="0.05f"/>
    <float name="m_KnockOffBikePitchMinComponent" min="-1000.0f" max="1000.0f" init="10.0f"/>
    <float name="m_KnockOffBikePitchMaxComponent" min="-1000.0f" max="1000.0f" init="10.0f"/>
    <float name="m_KnockOffBikeMinSpeed" min="0.0f" max="100.0f" init="10.0f"/>
    <float name="m_KnockOffBikeMaxSpeed" min="0.0f" max="100.0f" init="50.0f"/>
    <float name="m_KnockOffBikeMinUpright" min="-1.0f" max="1.0f" init="0.2f"/>
    <float name="m_KnockOffBikeMaxUpright" min="-1.0f" max="1.0f" init="0.8f"/>
    <float name="m_KnockOffBikeEjectMaxImpactDepth" min="0.0f" max="100.0f" init="0.1f"/>
    <float name="m_KnockOffBikeEjectImpactFriction" min="0.0f" max="100.0f" init="1.0f"/>

    <int name="m_ClearVehicleTimeMS" min="0" max="60000" init="1000" description="The amount of time (in millisecs) to disable downward contacts with the leg and override inverse mass scales on impacts with the vehicle we're being ejected from."/>

    <struct name="m_DefaultInverseMassScales" type="CTaskNMBehaviour::Tunables::InverseMassScales" description="Overrides default mass scales in brace on the vehicle you are being ejected from."/>
    <struct name="m_BicycleInverseMassScales" type="CTaskNMBehaviour::Tunables::InverseMassScales" description="Overrides default mass scales in brace on the vehicle you are being ejected from."/>
    <struct name="m_BikeInverseMassScales" type="CTaskNMBehaviour::Tunables::InverseMassScales" description="Overrides default mass scales in brace on the vehicle you are being ejected from."/>

    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start of the task"/>
    <struct name="m_Update" type="CNmTuningSet" description="Sent every time the task updates"/>
  </structdef>

  <structdef type="CTaskRageRagdoll::Tunables::WritheStrengthTuning">
    <float name="m_fInitialDelay" min="0.0f" max="5.0f" init="0.0f" description="Time before location-specific strength tuning kicks in" />
    <float name="m_fStartStrength" min="0.0f" max="500.0f" init="5.0f" description="Strength at the beginning" />
    <float name="m_fMidStrength" min="0.0f" max="500.0f" init="5.0f" description="Strength in the middle" />
    <float name="m_fEndStrength" min="0.0f" max="500.0f" init="5.0f" description="Strength at the end" />    
    <float name="m_fDurationStage1" min="0.0f" max="5.0f" init="1.0f" description="Duration of lerp between start and mid strength" />
    <float name="m_fDurationStage2" min="0.0f" max="5.0f" init="1.0f" description="Duration of lerp between mid and end strength" />
  </structdef>

  <structdef type="CTaskRageRagdoll::Tunables::RageRagdollImpulseTuning">
    <float name="m_fImpulseReductionPerShot" min="0.0f" max="1.0f" init="0.2f" description="Percent (normalzied) that the ipulse is reduced per shot" />
    <float name="m_fImpulseRecoveryPerSecond" min="0.0f" max="5.0f" init="1.0f" description="How much (normalized) of the impulse is recovered per second" />
    <float name="m_fMaxImpulseModifier" min="0.0f" max="5.0f" init="1.0f" description="Max possible rapid-fire impulse modifier due to reduction and recovery" />
    <float name="m_fMinImpulseModifier" min="0.0f" max="1.0f" init="0.5f" description="Min possible rapid-fire impulse modifier due to reduction and recovery" />    

    <float name="m_fCounterImpulseRatio" min="0.0f" max="5.0f" init="0.5f" description="Amount of the impulse to initially pull towards the shooter" />    
    <float name="m_fTempInitialStiffnessWhenShot" min="0.01f" max="0.99f" init="0.1f" description="Temporary stifness of the body when an impulse comes in" />    
    <float name="m_fAnimalMassMult" min="0.0f" max="1.0f" init="0.07f" description="Impulse multiplier used for animals" />    
    <float name="m_fAnimalImpulseMultMin" min="0.0f" max="5.0f" init="0.5f" description="Min possible impulse modifier to animals" />    
    <float name="m_fAnimalImpulseMultMax" min="0.0f" max="5.0f" init="2.0f" description="Max possible impulse modifier to animals" />  
    <float name="m_fInitialHitImpulseMult" min="0.0f" max="5.0f" init="2.0f" description="Impulse multipliers for initial hits to rage ragdolls" />    

  </structdef>

  <structdef type="CTaskRageRagdoll::Tunables" base="CTuning">
    <struct name="m_SpineStrengthTuning" type="CTaskRageRagdoll::Tunables::WritheStrengthTuning" description="Spine-specific tuning"/>
    <struct name="m_NeckStrengthTuning" type="CTaskRageRagdoll::Tunables::WritheStrengthTuning" description="Neck-specific tuning"/>
    <struct name="m_LimbStrengthTuning" type="CTaskRageRagdoll::Tunables::WritheStrengthTuning" description="Limb-specific tuning"/>
    <struct name="m_RageRagdollImpulseTuning" type="CTaskRageRagdoll::Tunables::RageRagdollImpulseTuning" description="Impulse tuning for rage ragdolls including animals and humans"/>
    <float name="m_fMuscleAngleStrengthRampDownRate" min="0.0f" max="10000.0f" init="0.5f" description="The rate at which muscle angle strength (strength) is ramped down" />
    <float name="m_fMuscleSpeedStrengthRampDownRate" min="0.0f" max="10000.0f" init="0.5f" description="The rate at which muscle speed strength (damping) is ramped down" />
  </structdef>
  

  <structdef type="CTaskNMSimple::Tunables::Tuning" autoregister="true">
    <int name="m_iMinTime" min="0" max="65535" init="2000" />
    <int name="m_iMaxTime" min="0" max="65535" init="10000" />
    <float name="m_fRagdollScore" min="-1.0f" max="1.0f" init="-1.0f" />
    <struct name="m_Start" type="CNmTuningSet" description="Sent once at the start of the task" />
    <struct name="m_Update" type="CNmTuningSet" description="Sent every time the task updates" />
    <struct name="m_OnBalanceFailure" type="CNmTuningSet" description="Sent once when the balancer fails" />
    <struct name="m_BlendOutThreshold" type="CTaskNMBehaviour::Tunables::BlendOutThreshold" description="Blend out thresholds" />
  </structdef>

  <structdef type="CTaskNMSimple::Tunables" autoregister="true" base="CTuning" onPreAddWidgets="PreAddWidgets">
    <map name="m_Tuning" type="atBinaryMap" key="atHashString" hideWidgets="true">
      <struct type="CTaskNMSimple::Tunables::Tuning" />
    </map>
  </structdef>
 
</ParserSchema>
