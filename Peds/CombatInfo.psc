<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CCombatInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <enum name="m_CombatMovement" type="CCombatData::Movement"/>
    <bitset name="m_BehaviourFlags" type="fixed" numBits="CCombatData::BehaviourFlags_NUM_ENUMS" values="CCombatData::BehaviourFlags"/>
    <enum name="m_CombatAbility" type="CCombatData::Ability"/>
    <enum name="m_AttackRanges" type="CCombatData::Range"/>
    <enum name="m_TargetLossResponse" type="CCombatData::TargetLossResponse"/>
    <enum name="m_TargetInjuredReaction" type="CCombatData::TargetInjuredReaction"/>
    <float name="m_WeaponShootRateModifier" min="0.0" max="1000.0"/>
    <string name="m_FiringPatternHash" type="atHashString"/>
    <float name="m_BlindFireChance" min="0.0" max="1.0" init="0.0"/>
    <float name="m_MaxShootingDistance" min="-1.0" max="10000.0" init="-1.0"/>
    <float name="m_TimeBetweenBurstsInCover" min="0.0" max="1000.0"/>
    <float name="m_BurstDurationInCover" min="0.0" max="1000.0"/>
    <float name="m_TimeBetweenPeeks" min="0.0" max="1000.0"/>
    <float name="m_WeaponAccuracy" min="0.0" max="1.0"/>
    <float name="m_StrafeWhenMovingChance" min="0.0" max="1.0" init="1.0"/>
    <float name="m_WalkWhenStrafingChance" min="0.0" max="1.0" init="0.0"/>
    <float name="m_HeliSpeedModifier" min="0.0" max="1.0" init="1.0"/>
    <float name="m_HeliSensesRange" min="0.0" max="200.0" init="150.0"/>
    <float name="m_AttackWindowDistanceForCover" min="-1.0" max="150.0" init="-1.0"/>
    <float name="m_TimeToInvalidateInjuredTarget" min="0.0" max="50.0" init="3.0"/>
    <float name="m_MinimumDistanceToTarget" min="0.0" max="50.0" init="7.0" description="Must have BF_MaintainMinDistanceToTarget set (for cover use only right now)"/>
    <float name="m_BulletImpactDetectionRange" min="0.0" max="50.0" init="10.0"/>
    <float name="m_AimTurnThreshold" min="0.0" max="0.5" init="0.15"/>
    <float name="m_OptimalCoverDistance" min="-1.0" max="100.0" init="-1.0"/>
    <float name="m_AutomobileSpeedModifier" min="0.0" max="1.0" init="1.0"/>
    <float name="m_SpeedToFleeInVehicle" min="0.0" max="100.0" init="40.0"/>
    <float name="m_TriggerChargeTime_Far" min="0.0" max="30.0" init="6.0" description="(seconds) Must have BF_CanCharge set to charge; far distance is combat task tunable." />
    <float name="m_TriggerChargeTime_Near" min="0.0" max="30.0" init="2.25" description="(seconds) Must have BF_CanCharge set to charge; near distance is combat task tunable." />
	<float name="m_MaxDistanceToHearEvents" min="-1.0" max="300.0" init="-1.0" description="Max distance peds can hear an event from, even if the sound is louder" />
	<float name="m_MaxDistanceToHearEventsUsingLOS" min="-1.0" max="300.0" init="3.0" description=" Max distance peds can hear an event from, even if the sound is louder if the ped is using LOS to hear events (CPED_CONFIG_FLAG_CheckLoSForSoundEvents)" />
    <float name="m_HomingRocketBreakLockAngle" min="-1.0" max="1.0" init="0.2"/>
    <float name="m_HomingRocketBreakLockAngleClose" min="-1.0" max="1.0" init="0.6"/>
    <float name="m_HomingRocketBreakLockCloseDistance" min="0.0" max="1000.0" init="20.0"/>
    <float name="m_HomingRocketTurnRateModifier" min="0.0" max="10.0" init="1.0"/>
    <float name="m_TimeBetweenAggressiveMovesDuringVehicleChase" min="-1.0" max="300.0" init="-1.0"/>
    <float name="m_MaxVehicleTurretFiringRange" min="-1.0" max="300.0" init="-1.0"/>
    <float name="m_WeaponDamageModifier" min="0.0" max="10.0" init="1.0"/>
  </structdef>

  <structdef type="CCombatInfoMgr">
     <array name="m_CombatInfos" type="atArray">
       <pointer type="CCombatInfo" policy="owner"/>
     </array>
  </structdef>

</ParserSchema>