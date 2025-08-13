<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CPlayerInfo::sSprintControlData">
    <float name="m_TapAdd" min="0.0f" max="50.0f" step="0.1f" init="4.0f"/>
    <float name="m_HoldSub" min="0.0f" max="50.0f" step="1.0f" init="35.0f"/>
    <float name="m_ReleaseSub" min="0.0f" max="50.0f" step="1.0f" init="15.0f"/>
    <float name="m_Threshhold" min="0.0f" max="20.0f" step="0.1f" init="5.0f"/>
    <float name="m_MaxLimit" min="0.0f" max="20.0f" step="0.1f" init="10.0f"/>
    <float name="m_ResultMult" min="0.0f" max="1.0f" step="0.1f" init="0.3f"/>
  </structdef>

  <structdef type="CPlayerInfo::sPlayerStatInfo" onPreAddWidgets="PreAddWidgets" onPostAddWidgets="PostAddWidgets">
    <string name="m_Name" type="atHashString" hideWidgets="true" ui_key="true"/>
    <float name="m_MinStaminaDuration" min="0.0f" max="200.0f" step="0.1f" init="10.0f"/>
    <float name="m_MaxStaminaDuration" min="0.0f" max="200.0f" step="0.1f" init="90.0f"/>
    <float name="m_MinHoldBreathDuration" min="0.0f" max="200.0f" step="0.1f" init="10.0f"/>
    <float name="m_MaxHoldBreathDuration" min="0.0f" max="200.0f" step="0.1f" init="90.0f"/>
    <float name="m_MinWheelieAbility" min="0.0f" max="200.0f" step="0.1f" init="10.0f"/>
    <float name="m_MaxWheelieAbility" min="0.0f" max="200.0f" step="0.1f" init="90.0f"/>
    <float name="m_MinPlaneControlAbility" min="0.0f" max="200.0f" step="0.1f" init="10.0f"/>
    <float name="m_MaxPlaneControlAbility" min="0.0f" max="200.0f" step="0.1f" init="90.0f"/>
    <float name="m_MinPlaneDamping" min="0.0f" max="200.0f" step="0.1f" init="10.0f"/>
    <float name="m_MaxPlaneDamping" min="0.0f" max="200.0f" step="0.1f" init="90.0f"/>
    <float name="m_MinHeliDamping" min="0.0f" max="200.0f" step="0.1f" init="10.0f"/>
    <float name="m_MaxHeliDamping" min="0.0f" max="200.0f" step="0.1f" init="90.0f"/>
    <float name="m_MinFallHeight" min="0.0f" max="200.0f" step="0.1f" init="5.0f"/>
    <float name="m_MaxFallHeight" min="0.0f" max="200.0f" step="0.1f" init="8.5f"/>
    <float name="m_MinDiveHeight" min="0.0f" max="9999.0f" step="0.1f" init="5.0f"/>
    <float name="m_MaxDiveHeight" min="0.0f" max="9999.0f" step="0.1f" init="150.f"/>
    <float name="m_DiveRampPow" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
  </structdef>
  
  <structdef type="CPlayerInfo::Tunables::EnemyCharging">
    <float name="m_fChargeGoalBehindCoverCentralOffset" min="0.0f" max="10.0f" step="0.1f" init="1.2f"/>
    <float name="m_fChargeGoalLateralOffset" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
    <float name="m_fChargeGoalRearOffset" min="0.0f" max="10.0f" step="0.1f" init="1.0f"/>
    <float name="m_fChargeGoalMaxAdjustRadius" min="0.0f" max="10.0f" step="0.1f" init="1.00f"/>
    <float name="m_fPlayerMoveDistToResetChargeGoals" min="0.0f" max="10.0f" step="0.1f" init="1.2f"/>
  </structdef>
  
  <structdef type="CPlayerInfo::Tunables::CombatLoitering">
    <float name="m_fPlayerMoveDistToResetLoiterPosition" min="0.0f" max="20.0f" step="0.1f" init="5.0f"/>
    <u32 name="m_uDistanceCheckPeriodMS" min="0" max="30000" step="500" init="1000"/>
  </structdef>
  
  <structdef type="CPlayerInfo::Tunables::EnduranceManagerSettings">
    <bool name="m_CanEnduranceIncapacitate" init="true"/>
    <u32 name="m_MaxRegenTime" min="0" max="100000" step="100" init="10000"/>
    <u32 name="m_RegenDamageCooldown" min="0" max="100000" step="100" init="10000"/>
  </structdef>

  <const name="CPlayerInfo::SPRINT_TYPE_MAX" value="4"/>
  <const name="CPlayerInfo::PT_MAX" value="8"/>
  
  <structdef type="CPlayerInfo::Tunables"  base="CTuning">
    <struct name="m_EnemyCharging" type="CPlayerInfo::Tunables::EnemyCharging"/>
    <struct name="m_CombatLoitering" type="CPlayerInfo::Tunables::CombatLoitering"/>
    <struct name="m_EnduranceManagerSettings" type="CPlayerInfo::Tunables::EnduranceManagerSettings"/>
    <float name="m_MinVehicleCollisionDamageScale" min="0.0f" max="2.0f" step="0.1f" init="0.9f"/>
    <float name="m_MaxVehicleCollisionDamageScale" min="0.0f" max="2.0f" step="0.1f" init="1.0f"/>
    <float name="m_MaxAngleConsidered" min="0.0f" max="6.28f" step="0.01f" init="6.28f"/>
    <float name="m_MinDotToConsiderVehicleValid" min="0.0f" max="1.0f" step="0.1f" init="0.5f"/>
    <float name="m_MaxDistToConsiderVehicleValid" min="0.0f" max="15.0f" step="0.1f" init="5.0f"/>
    <float name="m_SprintReplenishFinishedPercentage" min="0.0f" max="5.0f" step="0.1f" init="0.2f"/>
    <float name="m_SprintReplenishFinishedPercentageBicycle" min="0.0f" max="5.0f" step="0.1f" init="0.05f"/>
    <float name="m_SprintReplenishRateMultiplier" min="0.0f" max="5.0f" step="0.1f" init="0.5f"/>
    <float name="m_SprintReplenishRateMultiplierBike" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_MaxWorldLimitsPlayerX" init="10000.0f"/>
    <float name="m_MaxWorldLimitsPlayerY" init="10000.0f"/>
    <float name="m_MinWorldLimitsPlayerX" init="-10000.0f"/>
    <float name="m_MinWorldLimitsPlayerY" init="-10000.0f"/>
    <float name="m_MaxTimeToTrespassWhileSwimmingBeforeDeath" init="60.0f"/>
    <float name="m_MovementAwayWeighting" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_DistanceWeighting" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_HeadingWeighting" min="0.0f" max="5.0f" step="0.1f" init="3.5f"/>
    <float name="m_CameraWeighting" min="0.0f" max="5.0f" step="0.1f" init="1.5f"/>
    <float name="m_DistanceWeightingNoStick" min="0.0f" max="5.0f" step="0.1f" init="2.0f"/>
    <float name="m_HeadingWeightingNoStick" min="0.0f" max="5.0f" step="0.1f" init="1.0f"/>
    <float name="m_OnFireWeightingMult" min="0.0f" max="5.0f" step="0.1f" init="0.5f"/>
    <float name="m_BikeMaxRestoreDuration" min="0.0f" max="200.0f" step="0.1f" init="100.0f"/>
    <float name="m_BikeMinRestoreDuration" min="0.0f" max="100.0f" step="0.1f" init="12.5f"/>
    <float name="m_BicycleDepletionMinMult" min="-20.0f" max="20.0f" step="0.1f" init="0.5f"/>
    <float name="m_BicycleDepletionMidMult" min="-20.0f" max="20.0f" step="0.1f" init="1.0f"/>
    <float name="m_BicycleDepletionMaxMult" min="-20.0f" max="20.0f" step="0.1f" init="1.5f"/>
    <float name="m_BicycleMinDepletionLimit" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
    <float name="m_BicycleMidDepletionLimit" min="0.0f" max="10.0f" step="0.1f" init="3.0f"/>
    <float name="m_BicycleMaxDepletionLimit" min="0.0f" max="10.0f" step="0.1f" init="5.0f"/>
    <u32 name="m_TimeBetweenSwitchToClearTasks" min="0" max="6000" step="1" init="2000"/>
    <u32 name="m_TimeBetweenShoutTargetPosition" min="0" max="10000" step="50" init="800"/>
    <string name="m_TrespassGuardModelName" type="atHashString" init="A_C_SharkTiger"/>
    <bool name="m_GuardWorldExtents" init="true"/>
    <array name="m_SprintControlData" type="atFixedArray" size="CPlayerInfo::SPRINT_TYPE_MAX">
      <struct type="CPlayerInfo::sSprintControlData"/>
    </array>
    <array name="m_PlayerStatInfos" type="atFixedArray" size="CPlayerInfo::PT_MAX">
      <struct type="CPlayerInfo::sPlayerStatInfo"/>
    </array>
    <float name="m_ScanNearbyMountsDistance" min="0.0f" max="20.0f" step="0.5f" init="4.0f"/>
    <float name="m_MountPromptDistance" min="0.0f" max="20.0f" step="0.5f" init="3.0f"/>
    <bool name="m_bUseAmmoCaching" init="false"/>
  </structdef>

  <structdef type="CAnimSpeedUps::Tunables"  base="CTuning">
    <float name="m_MultiplayerClimbStandRateModifier" min="1.0f" max="2.0f" init="1.2f" description="Speed increase for all stand climb anims in MP"/>
    <float name="m_MultiplayerClimbRunningRateModifier" min="1.0f" max="2.0f" init="1.1f" description="Speed increase for all running climb anims in MP"/>
    <float name="m_MultiplayerClimbClamberRateModifier" min="1.0f" max="2.0f" init="1.1f" description="Speed increase for all clamber anims in MP"/>
    <float name="m_MultiplayerEnterExitJackVehicleRateModifier" min="0.0f" max="4.0f" step="0.1f" init="1.15f" description="Speed increase for all enter/exit/jacks in MP"/>
    <float name="m_MultiplayerLadderRateModifier" min="0.0f" max="2.0f" step="0.1f" init="1.2f" description="Speed increase for all ladder anims in MP"/>
    <float name="m_MultiplayerReloadRateModifier" min="0.0f" max="4.0f" step="0.1f" init="1.1f" description="Speed increase for all reloads in MP"/>
    <float name="m_MultiplayerCoverIntroRateModifier" min="0.1f" max="4.0f" step="0.1f" init="1.1f" description="Speed multiplier for spinning out from cover in MP"/>
    <float name="m_MultiplayerIdleTurnRateModifier" min="0.1f" max="4.0f" step="0.1f" init="1.1f" description="Speed multiplier for idle turns in MP"/>
    <bool name="m_ForceMPAnimRatesInSP" init="false" description="Forces MP anim rates in SP"/>
  </structdef>

</ParserSchema>
