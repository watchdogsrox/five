<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <structdef type="CLookAtHistory::Tunables"  base="CTuning">
    <float name="m_HistoryCosineThreshold" min="0.0f" max="1.0f" step="0.001" init="0.996" description="Used to test the lookat angle of a new entity compared to the history entity."/>
    <u32 name="m_MemoryDuration" min="0" max="100000" init="15000" description="How long (in milliseconds) an entity should stay in memory."/>
  </structdef>

  <enumdef type="LookIkTurnRate">
    <enumval name="LOOKIK_TURN_RATE_SLOW"/>
    <enumval name="LOOKIK_TURN_RATE_NORMAL"/>
    <enumval name="LOOKIK_TURN_RATE_FAST"/>
  </enumdef>

  <enumdef type="LookIkBlendRate">
    <enumval name="LOOKIK_BLEND_RATE_SLOWEST"/>
    <enumval name="LOOKIK_BLEND_RATE_SLOW"/>
    <enumval name="LOOKIK_BLEND_RATE_NORMAL"/>
    <enumval name="LOOKIK_BLEND_RATE_FAST"/>
    <enumval name="LOOKIK_BLEND_RATE_FASTEST"/>
    <enumval name="LOOKIK_BLEND_RATE_INSTANT"/>
  </enumdef>
  
  <enumdef type="LookIkRotationLimit">
    <enumval name="LOOKIK_ROT_LIM_OFF"/>
    <enumval name="LOOKIK_ROT_LIM_NARROWEST"/>
    <enumval name="LOOKIK_ROT_LIM_NARROW"/>
    <enumval name="LOOKIK_ROT_LIM_WIDE"/>
    <enumval name="LOOKIK_ROT_LIM_WIDEST"/>
  </enumdef>
  
  <structdef type="CAmbientLookAt::Tunables"  base="CTuning">
    <float name="m_DefaultLookAtThreshold" min="0.0f" max="1000.0f" step="0.1f" init="3.0f" description="What the lookat score needs to exceed to be considered a valid lookat target."/>
    <float name="m_DefaultDistanceFromWorldCenter" min="0.0f" max="100.0f" step="0.25f" init="30.0f" description="Standard distance from the world center ped needs to be within to do lookats."/>
    <float name="m_ExtendedDistanceFromWorldCenter" min="0.0f" max="100.0f" step="0.25f" init="60.0f" description="Extended distance from the world center ped needs to be within to do lookats."/>
    <float name="m_MaxDistanceToScanLookAts" min="0.0f" max="100.0f" step="1.0f" init="15.0f" description="How far around the ped entities can be to be considered valid lookat targets."/>
    <float name="m_BaseTimeToLook" min="0.0f" max="10.0f" step="0.1f" init="0.8f" description="The base time in seconds to perform a lookat for."/>
    <float name="m_AITimeBetweenLookAtsFailureMin" min="0.0f" max="100.0f" step="0.1f" init="1.0f" description="The min range (s) for how long until the next lookat for AI peds."/>
    <float name="m_AITimeBetweenLookAtsFailureMax" min="0.0f" max="100.0f" step="0.1f" init="3.0f" description="The max range (s) for how long until the next lookat for AI peds."/>
    <float name="m_PlayerTimeBetweenLookAtsMin" min="0.0f" max="100.0f" step="0.1f" init="2.0f" description="The min range (s) for how long until the next lookat for player peds."/>
    <float name="m_PlayerTimeBetweenLookAtsMax" min="0.0f" max="100.0f" step="0.1f" init="4.0f" description="The max range (s) for how long until the next lookat for player peds."/>
    <float name="m_PlayerTimeMyVehicleLookAtsMin" min="0.0f" max="100.0f" step="0.1f" init="5.0" description="The min range (s) for how long to look at my vehicle for player peds."/>
    <float name="m_PlayerTimeMyVehicleLookAtsMax" min="0.0f" max="100.0f" step="0.1f" init="5.0f" description="The max range (s) for how long to look at my vehicle for player peds."/>
    <float name="m_TimeBetweenScenarioScans" min="0.0f" max="100.0f" step="0.5f" init="10.0f" description="The time (s) between scans for a nearby lookat scenario point."/>
    <float name="m_ScenarioScanOffsetDistance" min="0.0f" max="100.0f" step="0.1f" init="2.0f" description="How far away from the ped the scenario scan should take place."/>
    <float name="m_ScenarioScanRadius" min="0.0f" max="100.0f" step="1.0f" init="20.0f" description="How far the scenario finder should look when searching for a scenario lookat point."/>
    <float name="m_MaxPlayerScore" min="0.0f" max="100.0f" step="0.1f" init="30.0f" description="The max ambient score for player peds."/>
    <float name="m_BasicPedScore" min="0.0f" max="1000.0f" step="0.1f" init="1.0f" description="Initial value for a basic ped in the lookat scoring system."/>
    <float name="m_BasicVehicleScore" min="0.0f" max="1000.0f" step="0.1f" init="1.0f" description="Initial value for a basic vehicle in the lookat scoring system."/>
    <float name="m_BasicObjectScore" min="0.0f" max="1000.0f" step="0.1f" init="0.5f" description="Initial value for a basic object in the lookat scoring system."/>
    <float name="m_BehindPedModifier" min="0.0f" max="1000.0f" step="0.1f" init="0.75f" description="Factor for objects behind ped doing the scoring."/>
    <float name="m_PlayerPedModifier" min="0.0f" max="1000.0f" step="0.1f" init="1.0f" description="Factor for peds to score the player."/>
    <float name="m_WalkingRoundPedModifier" min="0.0f" max="1000.0f" step="0.1f" init="5.0f" description="Factor for peds who are walking around the ped doing the scoring."/>
    <float name="m_RunningPedModifier" min="0.0f" max="1000.0f" step="0.1f" init="5.0f" description="Factor for peds who are running."/>
    <float name="m_ClimbingOrJumpingPedModifier" min="0.0f" max="1000.0f" step="0.1f" init="5.0f" description="Factor for peds who are climbing or jumping."/>
    <float name="m_FightingModifier" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Factor for peds that are fighting."/>
    <float name="m_JackingModifier" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Factor for peds who are stealing a car."/>
    <float name="m_HangingAroundVehicleModifier" min="0.0f" max="1000.0f" step="0.1f" init="8.0f" description="Factor for peds who are hanging around our vehicle."/>
    <float name="m_ScenarioToScenarioPedModifier" min="0.0f" max="1000.0f" step="0.1f" init="3.0f" description="Factor for scenario peds looking at other scenario peds."/>
    <float name="m_GangScenarioPedToPlayerModifier" min="0.0f" max="1000.0f" step="0.1f" init="15.0f" description="Factor used to make gang members using a scenario more likely to look at the player."/>
    <float name="m_ApproachingPlayerModifier" min="0.0f" max="1000.0f" step="0.1f" init="2.0f" description="Factor used to scale up the weight of a player approaching this ped."/>
    <float name="m_ClosePlayerModifier" min="0.0f" max="1000.0f" step="0.1f" init="2.0f" description="Factor used to scale up the weight of a player extremely close to this ped."/>
    <float name="m_InRangePlayerModifier" min="0.0f" max="1000.0f" step="0.1f" init="2.0f" description="Factor used to scale up the weight of a player close to this ped."/>
    <float name="m_InRangeDrivingPlayerModifier" min="0.0f" max="1000.0f" step="0.1f" init="2.0f" description="Factor used to scale up the weight of a player extremely close to this ped while driving a car."/>
    <float name="m_HoldingWeaponPlayerModifier" min="0.0f" max="1000.0f" step="0.1f" init="2.0f" description="Factor used to scale up the weight of a player holding a weapon."/>
    <float name="m_CoveredInBloodPlayerModifier" min="0.0f" max="1000.0f" step="0.1f" init="2.0f" description="Factor used to scale up the weight of a player covered in blood."/>
    <float name="m_RagdollingModifier" min="0.0f" max="1000.0f" step="0.1f" init="8.0f" description="Factor for peds ragdolling."/>
    <float name="m_PickingUpBikeModifier" min="0.0f" max="1000.0f" step="0.1f" init="4.0f" description="Factor for peds picking up a on ground bike."/>
    <float name="m_RecklessCarModifier" min="0.0f" max="1000.0f" step="0.1f" init="5.0f" description="Factor for cars that are being recklessly driven."/>
    <float name="m_RecentlyLookedAtPlayerModifier" min="0.0f" max="10.0f" step="0.01f" init="0.9f" description="Factor used to scale down recently looked at players."/>
    <float name="m_RecentlyLookedAtEntityModifier" min="0.0f" max="10.0f" step="0.01f" init="0.5f" description="Factor used to scale down recently looked at entities."/>
    <float name="m_HighImportanceModifier" min="1.0f" max="10.0f" step="0.1f" init="2.0f" description="Factor used for high-importance lookat scenarios."/>
    <float name="m_MediumImportanceModifier" min="0.5f" max="2.0f" step="0.1f" init="1.0f" description="Factor used for medium-importance lookat scenarios."/>
    <float name="m_LowImportanceModifier" min="0.0f" max="1.0f" step="0.1f" init="0.5f" description="Factor used for low-importance lookat scenarios."/>
    <array name="m_ModelNamesToConsiderPlayersForScoringPurposes" type="atArray">
      <string type="atHashString"/>
    </array>
    <float name="m_RecklessCarSpeedMin" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Scaling constant used in increasing the value of recklessly driven cars."/>
    <float name="m_RecklessCarSpeedMax" min="0.0f" max="1000.0f" step="0.1f" init="16.666f" description="Scaling constant used in increasing the value of recklessly driven cars."/>
    <float name="m_CarSirenModifier" min="0.0f" max="1000.0f" step="0.1f" init="5.0f" description="Factor for cars that have sirens activated."/>
    <float name="m_PlayerCopModifier" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Factor used for players to make them more likely to look at cops."/>
    <float name="m_PlayerSexyPedModifier" min="0.0f" max="1000.0f" step="0.1f" init="20.0f" description="Factor used for players to make them more likely to look at sexy peds."/>
    <float name="m_PlayerSwankyCarModifier" min="0.0f" max="1000.0f" step="0.1f" init="20.0f" description="Factor used for players to make them more likely to look at swanky cars."/>
    <float name="m_PlayerCopCarModifier" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Factor used for players to make them more likely to look at cop cars."/>
    <float name="m_PlayerHasslingModifier" min="0.0f" max="1000.0f" step="0.1f" init="100.0f" description="Factor used for players to make them more likely to look at peds hassling them."/>
    <float name="m_HotPedMinDistance" min="0.0f" max="50.0f" step="0.1f" init="10.0f" description="Min distance to the hot ped from the jeering ped"/>
    <float name="m_HotPedMaxDistance" min="0.0f" max="50.0f" step="0.1f" init="20.0f" description="Max distance to the hot ped from the jeering ped"/>
    <float name="m_HotPedMinDotAngle" min="-1.0f" max="1.0f" step="0.05f" init="-0.1f" description="Min dot product angle to the hot ped from the jeering ped"/>
    <float name="m_HotPedMaxDotAngle" min="-1.0f" max="1.0f" step="0.05f" init="0.3f" description="Max dot product angle to the hot ped from the jeering ped"/>
    <float name="m_HotPedMaxHeightDifference" min="0.0f" max="10.0f" step="0.1f" init="4.0f" description="Max height difference between hot ped and jeering ped"/>
    <float name="m_InRangePlayerDistanceThreshold" min="0.0f" max="10.0f" step="0.1f" init="5.0f" description="Try to look at the player within this range even if the ped is looking at some non-player target."/>
    <float name="m_InRangePlayerInRaceDistanceThreshold" min="0.0f" max="50.0f" step="0.1f" init="15.0f" description="Try to look at the player within this range even in race mode if the ped is looking at some non-player target."/>
    <float name="m_ClosePlayerDistanceThreshold" min="0.0f" max="10.0f" step="0.1f" init="2.0f" description="Min distance to consider the player as extremely close."/>
    <float name="m_ApproachingPlayerDistanceThreshold" min="0.0f" max="10.0f" step="0.1f" init="4.0f" description="Min distance to consider the player as approaching."/>
    <float name="m_ApproachingPlayerCosineThreshold" min="0.0f" max="1.0f" step="0.001f" init="0.707" description="Threshold for the dot product between the straight line to the player and the player's forward vector."/>
    <float name="m_RagdollPlayerDistanceThreshold" min="0.0f" max="50.0f" step="0.1f" init="15.0f" description="Threshold distance to try to look at player if he is ragdolling"/>
    <float name="m_LookingInRangePlayerMaxDotAngle" min="-1.0f" max="1.0f" step="0.001f" init="-0.707" description="Max dot product angle to the in-range player to ignore the IK FOV."/>
    <float name="m_MaxVelocityForVehicleLookAtSqr" min="0.0f" max="1000.0f" step="1.0f" init="25.0f" description="The maximum velocity a vehicle can travel to be valid for lookats."/>
    <u8 name="m_PlayerSwankyCarMin" min="0" max="5" step="1" init="5" description="The minimum swankiness of a car that the player is interested in."/>
    <u8 name="m_PlayerSwankyCarMax" min="0" max="5" step="1" init="5" description="The maximum swankiness of a car that the player is interested in."/>
    <bool name="m_HotPedRenderDebug" init="false"/>
    <bool name="m_HotPedDisableSexinessFlagChecks" init="false"/>
    <float name="m_MinTimeBeforeSwitchLookAt" min="0.0f" max="10.0f" step="0.1f" init="2.0f" description="The minimum time before switching look at target."/>
    <float name="m_MaxLookBackAngle" min="0.0f" max="180.0f" step="1.0f" init="135.0f" description="The maximum angle the player can look where camera is pointing."/>
    <float name="m_MinTurnSpeedMotionOverPOI" min="0.0f" max="20.0f" step="0.1f" init="0.65f" description="The minimum turn speed to use motion direction over POI."/>
   
    <float name="m_SpeedForNarrowestAnglePickPOI" min="0.0f" max="100.0f" step="1.0f" init="20.0f" description="Any speed bigger than this will use the narrowest angle in POI pick."/>
    <float name="m_MaxAnglePickPOI" min="0.0f" max="180.0f" step="1.0f" init="90.0f" description="The maximum angle used in POI pick."/>
    <float name="m_MinAnglePickPOI" min="0.0f" max="180.0f" step="1.0f" init="15.0f" description="The minimum angle used in POI pick."/>
    <float name="m_MaxPitchingAnglePickPOI" min="0.0f" max="180.0f" step="1.0f" init="45.0f" description="The maximum pitching angle used in POI pick."/>
    
    <bool name="m_PlayerLookAtDebugDraw" init="false"/>
    <enum name="m_CameraLookAtTurnRate" type="LookIkTurnRate" init="LOOKIK_TURN_RATE_SLOW"/>
    <enum name="m_POILookAtTurnRate" type="LookIkTurnRate" init="LOOKIK_TURN_RATE_SLOW"/>
    <enum name="m_MotionLookAtTurnRate" type="LookIkTurnRate" init="LOOKIK_TURN_RATE_SLOW"/>
    <enum name="m_VehicleJumpLookAtTurnRate" type="LookIkTurnRate" init="LOOKIK_TURN_RATE_FAST"/>
    <enum name="m_CameraLookAtBlendRate" type="LookIkBlendRate" init="LOOKIK_BLEND_RATE_NORMAL"/>
    <enum name="m_POILookAtBlendRate" type="LookIkBlendRate" init="LOOKIK_BLEND_RATE_NORMAL"/>
    <enum name="m_MotionLookAtBlendRate" type="LookIkBlendRate" init="LOOKIK_BLEND_RATE_NORMAL"/>
    <enum name="m_VehicleJumpLookAtBlendRate" type="LookIkBlendRate" init="LOOKIK_BLEND_RATE_FAST"/>
    <enum name="m_CameraLookAtRotationLimit" type="LookIkRotationLimit" init="LOOKIK_ROT_LIM_WIDE"/>
    <enum name="m_POILookAtRotationLimit" type="LookIkRotationLimit" init="LOOKIK_ROT_LIM_WIDE"/>
    <enum name="m_MotionLookAtRotationLimit" type="LookIkRotationLimit" init="LOOKIK_ROT_LIM_WIDE"/>

    <float name="m_AITimeWaitingToCrossRoadMin" min="0.0f" max="15.0f" step="0.1f" init="2.5f" description="The minimum time to do the waiting to cross road look at."/>
    <float name="m_AITimeWaitingToCrossRoadMax" min="0.0f" max="15.0f" step="0.1f" init="3.5f" description="The maximum time to do the waiting to cross road look at."/>

    <float name="m_fAIGreetingDistanceMin" min="0.0f" max="100.0f" step="1.0f" init="10.0f" description="The minimum distance to the target ped to greet."/>
    <float name="m_fAIGreetingDistanceMax" min="0.0f" max="100.0f" step="1.0f" init="25.0f" description="The maximum distance to the target ped to greet."/>
    <u32 name="m_uAITimeBetweenGreeting" min="0" max="100000" step="1" init="10000" description="The ms time between greetings"/>
    <float name="m_fAIGreetingPedModifier" min="0.0f" max="50.0f" step="1.0f" init="30.0f" description="Factor used for AI peds to make them more likely to look at the ped to greet."/>

    <u32 name="m_uTimeBetweenLookBacks" min="0" max="50000" step="1" init="8000" description="The ms time between two look backs."/>
    <u32 name="m_uTimeToLookBack" min="0" max="50000" step="1" init="3000" description="The ms time to look back when the look back button is pressed."/>

    <u32 name="m_uAimToIdleLookAtTime" min="0" max="10000" step="1" init="1500" description="The maximum time to look at camera direction during aim to idle transition."/>
    <float name="m_fAimToIdleBreakOutAngle" min="0.0f" max="180.0f" step="1.0f" init="150.0f" description="Used to break out the aim to idle look at if the angle between the camera direction and spine3 forward is greater than it."/>
    <float name="m_fAimToIdleAngleLimitLeft" min="0.0f" max="180.0f" step="1.0f" init="45.0f" description="The maximum look at left angle during aim to idle between the camera direction and facing direction"/>
    <float name="m_fAimToIdleAngleLimitRight" min="0.0f" max="180.0f" step="1.0f" init="90.0f" description="The maximum look at left angle during aim to idle between the camera direction and facing direction"/>

    <u32 name="m_uExitingCoverLookAtTime" min="0" max="10000" step="1" init="1500" description="The maximum time to look at camera direction during exiting cover."/>

  </structdef>

  <structdef type="CTaskAmbientClips::Tunables"  base="CTuning">
  <string name="m_LowLodBaseClipSetId" type="atHashString" description="Clip set containing fallback animations to be used instead of base animations that are not streamed in on time."/>
    <float name="m_DefaultChanceOfStandingWhileMoving" min="0.0f" max="1.0f" step="0.05f" init="0.5f" description="If we don't get the chance of standing from conditional animations, this value will be used as a probability between 0..1."/>
    <float name="m_DefaultTimeBetweenIdles" min="0.0f" max="120.0f" step="0.1f" init="4.0f"  description="If we don't get the time between idles from conditional animations, this value (in seconds) will be used."/>
    <float name="m_TimeAfterGunshotToPlayIdles" min="0.0f" max="600.0f" step="1.0f" init="40.0f"  description="When blocking idle animations after gunshots, this is the number of seconds to wait."/>
    <float name="m_TimeAfterGunshotForPlayerToPlayIdles" min="0.0f" max="600.0f" step="1.0f" init="20.0f"  description="Like m_TimeAfterGunshotToPlayIdles, but used any time for the player, not just when wandering."/>
    <float name="m_playerNearToHangoutDistanceInMetersSquared" min="1.0f" max="100.0f" init="25.0f" description="If the player is within this distance (squared) to a scenario with the BreakForNearbyPlayer flag, we start a timer to force the ped to do something else."/>
    <float name="m_minSecondsNearPlayerUntilHangoutQuit" min="0.0f" max="120.0f" step="0.1f" init="7.0f" description="Minimum time in seconds the player must be within the above distance until the Ped is forced into a conversation or different scenario."/>
    <float name="m_maxSecondsNearPlayerUntilHangoutQuit" min="0.0f" max="120.0f" step="0.1f" init="12.0f" description="Maximum time in seconds the player must be within the above distance until the Ped is forced into a conversation or different scenario."/>
    <float name="m_maxHangoutChatDistSq" min="0.0f" max="100.0f" step="0.1f" init="4.0f" description="Maximum distance (squared) peds can be and still perform hangout-style chats."/>
    <float name="m_VFXCullRangeScaleNotVisible" min="0.0f" max="1.0f" step="0.1f" init="0.5f" description="Scaling factor to apply to VFX cull ranges for peds that are not visible."/>
    <float name="m_SecondsSinceInWaterThatCountsAsWet" min="0.0f" max="20.0f" step="0.1f" init="10.0f" description="How long a ped is allowed to be wet after leaving the water."/>
    <float name="m_MaxVehicleVelocityForAmbientIdles" min="0.0f" max="40.0f" step="0.1f" init="10.0f" description="The maximum velocity in meters per second the vehicle we're in can travel at before we stop ambient idles from playing."/>
    <float name="m_MaxBikeVelocityForAmbientIdles" min="0.0f" max="40.0f" step="0.1f" init="0.5f" description="The maximum velocity in meters per second the bike we're in can travel at before we stop ambient idles from playing."/>
    <float name="m_MaxSteeringAngleForAmbientIdles" min="0.0f" max="1.0f" step="0.01f" init="0.1f" description="The max absolute steering angle before we stop ambient idles from playing."/>
    <u32 name="m_MaxTimeSinceGetUpForAmbientIdles" min="0" max="100000" step="1" init="5000" description="The ms time since getting up that a dust off idle could occur"/>
    <float name="m_fArgumentDistanceMinSq" min="0.0f" max="900.0f" step="0.10f" init="100.0f" description="Minimum distance from the player in meters (squared) to start an argument between peds if they choose to have one." />
    <float name="m_fArgumentDistanceMaxSq" min="0.0f" max="900.0f" step="0.10f" init="225.0f" description="Maximum distance from the player in meters (squared) to start an argument between peds if they choose to have one." />
    <float name="m_fArgumentProbability" min="0.0f" max="1.0f"  step="0.05f" init="0.10f" description="Chance that two peds near each other with AllowConversations will get into an argument."/>
  </structdef>

  <structdef type="CTaskChat::Tunables"  base="CTuning">
    <float name="m_HeadingToleranceDegrees" min="0.0f" max="180.0f" step="1.0f" init="5.0f" description="Max allowed angle away from the desired heading for going to State_Chatting, in degrees."/>
    <float name="m_MaxWaitTime" min="0.0f" max="1000.0f" step="1.0f" init="30.0f" description="Max time to wait for the other ped (or proper heading) in State_WaitingToChat, in seconds."/>
  </structdef>

  <structdef type="CTaskIncapacitated::Tunables" base="CTuning" cond="CNC_MODE_ENABLED">
    <u32 name="m_TimeSinceRecoveryToIgnoreEnduranceDamage" min="0" max="100000" step="100" init="3000" description="The ms duration since recovering from Incapacitation to ignore Endurance damage."/>
    <u32 name="m_IncapacitateTime" min="0" max="100000" step="100" init="16000" description="The ms duration for Incapacitation to run until the Ped recovers."/>
    <u32 name="m_GetUpFasterModifier" min="0" max="1000" step="1" init="200" description="The ms time to reduce Incapacitation recovery by per button press."/>
    <u32 name="m_GetUpFasterLimit" min="0" max="100000" step="100" init="8000" description="The maximum ms time that Incapacitation recovery can be reduced by."/>
  </structdef>

  <structdef type="CTaskPlayerOnFoot::Tunables::ParachutePack::VelocityInheritance">
    <bool name="m_Enabled" init="false"/>
    <float name="m_X" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
    <float name="m_Y" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
    <float name="m_Z" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
  </structdef>

  <structdef type="CTaskPlayerOnFoot::Tunables::ParachutePack">
    <struct name="m_VelocityInheritance" type="CTaskPlayerOnFoot::Tunables::ParachutePack::VelocityInheritance"/>
    <float name="m_AttachOffsetX" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_AttachOffsetY" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_AttachOffsetZ" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_AttachOrientationX" min="-180.0f" max="180.0f" step="1.0f" init="0.0f"/>
    <float name="m_AttachOrientationY" min="-180.0f" max="180.0f" step="1.0f" init="0.0f"/>
    <float name="m_AttachOrientationZ" min="-180.0f" max="180.0f" step="1.0f" init="0.0f"/>
    <float name="m_BlendInDeltaForPed" min="0.1f" max="1000.0f" step="0.1f" init="8.0f"/>
    <float name="m_BlendInDeltaForProp" min="0.1f" max="1000.0f" step="0.1f" init="8.0f"/>
    <float name="m_PhaseToBlendOut" min="-1.0f" max="1.0f" step="0.01f" init="0.85f"/>
    <float name="m_BlendOutDelta" min="-100.0f" max="-1.0f" step="0.01f" init="-4.0f"/>
  </structdef>

  <structdef type="CTaskPlayerOnFoot::Tunables::ScubaGear::VelocityInheritance">
    <bool name="m_Enabled" init="false"/>
    <float name="m_X" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
    <float name="m_Y" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
    <float name="m_Z" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
  </structdef>

  <structdef type="CTaskPlayerOnFoot::Tunables::ScubaGear">
    <struct name="m_VelocityInheritance" type="CTaskPlayerOnFoot::Tunables::ScubaGear::VelocityInheritance"/>
    <float name="m_AttachOffsetX" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_AttachOffsetY" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_AttachOffsetZ" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_AttachOrientationX" min="-180.0f" max="180.0f" step="1.0f" init="0.0f"/>
    <float name="m_AttachOrientationY" min="-180.0f" max="180.0f" step="1.0f" init="0.0f"/>
    <float name="m_AttachOrientationZ" min="-180.0f" max="180.0f" step="1.0f" init="0.0f"/>
    <float name="m_PhaseToBlendOut" min="-1.0f" max="1.0f" step="0.01f" init="0.85f"/>
    <float name="m_BlendOutDelta" min="-100.0f" max="-1.0f" step="0.01f" init="-4.0f"/>
  </structdef>

  <structdef type="CTaskPlayerOnFoot::Tunables::JetpackData::VelocityInheritance">
    <bool name="m_Enabled" init="true"/>
    <float name="m_X" min="-10.0f" max="10.0f" step="0.01f" init="0.0f"/>
    <float name="m_Y" min="-10.0f" max="10.0f" step="0.01f" init="-2.0f"/>
    <float name="m_Z" min="-10.0f" max="10.0f" step="0.01f" init="-2.0f"/>
  </structdef>

  <structdef type="CTaskPlayerOnFoot::Tunables::JetpackData">
    <struct name="m_VelocityInheritance" type="CTaskPlayerOnFoot::Tunables::JetpackData::VelocityInheritance"/>
    <float name="m_PhaseToBlendOut" min="-1.0f" max="1.0f" step="0.01f" init="0.85f"/>
    <float name="m_BlendOutDelta" min="-100.0f" max="-1.0f" step="0.01f" init="-4.0f"/>
    <float name="m_ForceToApplyAfterInterrupt" min="0.0f" max="1000.0f" step="1.0f" init="300.0f"/>
    <string name="m_ClipSetHash" type="atHashString" init="skydive@parachute@pack"/>
    <string name="m_PedClipHash" type="atHashString" init="Chute_Off"/>
    <string name="m_PropClipHash" type="atHashString" init="Chute_Off_Bag"/>
  </structdef>

  <structdef type="CTaskPlayerOnFoot::Tunables"  base="CTuning">
    <struct name="m_ParachutePack" type="CTaskPlayerOnFoot::Tunables::ParachutePack"/>
    <struct name="m_ScubaGear" type="CTaskPlayerOnFoot::Tunables::ScubaGear"/>
    <struct name="m_JetpackData" type="CTaskPlayerOnFoot::Tunables::JetpackData"/>
    <bool name="m_EvaluateThreatFromCoverPoints" init="true"/>
    <bool name="m_UseThreatWeighting" init="false"/>
    <bool name="m_UseThreatWeightingFPS" init="false"/>
    <bool name="m_CanMountFromInAir" init="false"/>
    <float name="m_ArrestDistance" min="0.1f" max="10.0f" step="0.1f" init="6.0f"/>
    <float name="m_ArrestDot" min="0.0f" max="1.0f" step="0.05f" init="0.5f"/>
    <float name="m_ArrestVehicleSpeed" min="0.1f" max="30.0f" step="0.1f" init="24.0f"/>
    <float name="m_MaxEncumberedClimbHeight" min="0.0f" max="3.0f" step="0.05f" init="0.8f"/>
    <float name="m_MaxTrainClimbHeight" min="0.0f" max="3.5f" step="0.05f" init="3.2f"/>
    <float name="m_TakeCustodyDistance" min="0.1f" max="100.0f" step="0.1f" init="10.0f"/>
    <float name="m_UncuffDistance" min="0.1f" max="10.0f" step="0.1f" init="2.0f"/>
    <float name="m_MaxDistanceToTalk" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
    <float name="m_MinDotToTalk" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <float name="m_TimeBetweenPlayerEvents" min="0.0f" max="10.0f" step="1.0f" init="3.0f"/>
    <float name="m_DistanceBetweenAiPedsCoverAndPlayersCover" min="0.0f" max="5.0f" step="0.1f" init="0.6f"/>
    <float name="m_MaxDistanceAiPedFromTheirCoverToAbortPlayerEnterCover" min="0.0f" max="10.0f" step="0.1f" init="1.5f"/>
    <float name="m_SmallCapsuleCoverPenalty" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_SmallCapsuleCoverRadius" min="0.0f" max="10.0f" step="0.01f" init="0.3f"/>
    <float name="m_PriorityCoverWeight" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
    <float name="m_EdgeCoverWeight" min="0.0f" max="10.0f" step="0.01f" init="0.5f"/>
    <float name="m_DistToCoverWeightThreat" min="0.0f" max="10.0f" step="0.01f" init="6.0f"/>
    <float name="m_DistToCoverWeight" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_DistToCoverWeightNoStickBonus" min="0.0f" max="10.0f" step="0.01f" init="2.0f"/>
    <float name="m_VeryCloseToCoverDist" min="0.0f" max="2.0f" step="0.01f" init="0.75f"/>
    <float name="m_VeryCloseToCoverWeight" min="0.0f" max="10.0f" step="0.01f" init="4.0f"/>
    <float name="m_DesiredDirToCoverWeight" min="0.0f" max="10.0f" step="0.01f" init="6.0f"/>
    <float name="m_DesiredDirToCoverAimingWeight" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_ThreatDirWeight" min="0.0f" max="20.0f" step="0.01f" init="6.0f"/>
    <float name="m_ThreatEngageDirWeight" min="0.0f" max="20.0f" step="0.01f" init="2.0f"/>
    <float name="m_CoverDirToCameraWeightMin" min="0.0f" max="10.0f" step="0.01f" init="1.0f"/>
    <float name="m_CoverDirToCameraWeightMax" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_CoverDirToCameraWeightMaxAimGun" min="0.0f" max="10.0f" step="0.01f" init="6.0f"/>
    <float name="m_CoverDirToCameraWeightMaxScaleDist" min="0.0f" max="10.0f" step="0.01f" init="3.0f"/>
    <float name="m_DesiredDirToCoverMinDot" min="-1.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_CameraDirToCoverMinDot" min="-1.0f" max="1.0f" step="0.01f" init="0.5f"/>
    <float name="m_StaticLosTest1Offset" min="-1.0f" max="1.0f" step="0.01f" init="0.4f"/>
    <float name="m_StaticLosTest2Offset" min="-1.0f" max="1.0f" step="0.01f" init="-0.4f"/>
    <float name="m_CollisionLosHeightOffset" min="-1.0f" max="1.0f" step="0.01f" init="-0.15f"/>
    <float name="m_VeryCloseIgnoreDesAndCamToleranceDist" min="0.0f" max="3.0f" step="0.01f" init="1.75f"/>
    <float name="m_VeryCloseIgnoreDesAndCamToleranceDistAimGun" min="0.0f" max="3.0f" step="0.01f" init="3.0f"/>
    <float name="m_DeadZoneStickNorm" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
    <float name="m_SearchThreatMaxDot" min="-1.0f" max="1.0f" step="0.01f" init="0.0f"/>
    <string name="m_BirdWeaponHash" type="atHashString" init="WEAPON_BIRD_CRAP"/>
    <bool name="m_HideBirdProjectile" init="true"/>
    <bool name="m_AllowFPSAnalogStickRunInInteriors" init="true"/>
  </structdef>

  <structdef type="CTaskUnalerted::Tunables"  base="CTuning">
    <float name="m_ScenarioDelayAfterFailureMin" min="0.0f" max="1000.0f" step="0.1f" init="5.0f" description="Minimum time after an unsuccessful scenario search before we try again."/>
    <float name="m_ScenarioDelayAfterFailureMax" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Maximum time after an unsuccessful scenario search before we try again."/>
    <float name="m_ScenarioDelayAfterFailureWhenStationary" min="0.0f" max="1000.0f" step="0.1f" init="20.0f" description="Time after an unsuccessful scenario search before trying again, if we're not moving (when in roam mode)"/>
    <float name="m_ScenarioDelayAfterNotAbleToSearch" min="0.0f" max="1000.0f" step="0.1f" init="1.0f" description="Time after not being allowed to search for scenarios, until we try again."/>
    <float name="m_ScenarioDelayAfterSuccessMin" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Minimum time after successfully using a scenario, before we try to search for scenarios again."/>
    <float name="m_ScenarioDelayAfterSuccessMax" min="0.0f" max="1000.0f" step="0.1f" init="15.0f" description="Maximum time after successfully using a scenario, before we try to search for scenarios again."/>
    <float name="m_ScenarioDelayInitialMin" min="0.0f" max="1000.0f" step="0.1f" init="0.0f" description="Minimum time after the task starts, before we try to search for a scenario to use."/>
    <float name="m_ScenarioDelayInitialMax" min="0.0f" max="1000.0f" step="0.1f" init="10.0f" description="Maximum time after the task starts, before we try to search for a scenario to use."/>
    <float name="m_TimeBeforeDriverAnimCheck" min="0.0f" max="100.0f" step="0.5f" init="1.0f" description="Minimum time, in seconds, before a passenger should check the driver's conditional anim group."/>
    <float name="m_TimeBetweenSearchesForNextScenarioInChain" min="0.0f" max="100.0f" step="0.5f" init="0.5f" description="Minimum time, in seconds, between searches for the next scenario point in a chain."/>
    <float name="m_TimeMinBeforeLastPoint" min="0.0f" max="10000.0f" step="1.0f" init="30.0f" description="Minimum time, in seconds, that must have passed before considering reusing the last point."/>
    <float name="m_TimeMinBeforeLastPointType" min="0.0f" max="10000.0f" step="1.0f" init="30.0f" description="Minimum time, in seconds, that must have passed before considering reusing the last point type."/>
    <float name="m_PavementFloodFillSearchRadius" init="10.0f" description="Defines a radius to perform a flood fill search for nearby pavement to wander on."/>
    <float name="m_WaitTimeAfterFailedVehExit" init="0.5f" description="Defines a wait time between attempts to exit a vehicle"/>
    <float name="m_MaxDistanceToReturnToLastUsedVehicle" init="50.0f" description="Max distance to return to our last used vehicle"/>
  </structdef>

  <structdef type="CTaskFlyingWander::Tunables"  base="CTuning">
    <float name="m_RangeOffset" min="0.0f" max="1000.0f" step="1.0f" init="50.0f" description="How far to project out the new target point for flying."/>
    <float name="m_HeadingWanderChange" min="0.0f" max="1.587285f" step="0.01f" init="0.524283f" description="The maximum value to change heading by (in radians) when wandering."/>
    <float name="m_TargetRadius" min="0.0f" max="100.0f" step="0.1f" init="0.1f" description="Target radius used in FlyToPoint subtask."/>
  </structdef>

  <structdef type="CTaskSwimmingWander::Tunables"  base="CTuning">
    <float name="m_SurfaceSkimmerDepth" min="0.0f" max="100.0f" step="0.1f" init="2.0f" description="How deep below the surface a close to the surface swimming ped should be."/>
    <float name="m_NormalPreferredDepth" min="0.0f" max="100.0f" step="0.1f" init="3.0f" description="How deep below the surface a normal swimming ped should be."/>
    <float name="m_AvoidanceProbeLength" min="0.0f" max="20.0f" step="0.1f" init="15.0f" description="How long the avoidance probe should be."/>
    <float name="m_AvoidanceProbePullback" min="0.0f" max="20.0f" step="0.1f" init="7.0f" description="How far by default to pullback from the swimmer when launching a probe."/>
    <u32 name="m_AvoidanceProbeInterval" min="0" max="10000" init="3000" description="How long between avoidance probe checks (ms)."/>
    <float name="m_AvoidanceSteerAngleDegrees" min="0.0f" max="180.0f" init="10.0f" description="How much to change a fish's swim angle by in degrees if you discovered that you will hit something in the near future."/>
    <u32 name="m_InstantProbeDurationMin" min="0" max="1000" init="250" description="Min time for the duration to be when resetting it to 0 during instant probe checks when the swimmer hits a wall."/>
  </structdef>

  <structdef type="CTaskWander::Tunables" base="CTuning">
    <u32    name="m_uNumPedsToTransitionToRainPerPeriod"  min="0" max="100" init="8" description="How many peds can transition to their rain wanders per m_SecondsInRainTransitionPeriod"/>
    <float  name="m_fSecondsInRainTransitionPeriod"       min="0.0f" max="60.0f" init="1.0f"/>
  </structdef>
  
  <structdef type="CTaskWanderInArea::Tunables" base="CTuning">
    <float name="m_MinWaitTime" min="0" max="10" step="0.1" init="3" description="Minimum wait time between wanders, in seconds."/>
    <float name="m_MaxWaitTime" min="0" max="10" step="0.1" init="6" description="Maximum wait time between wanders, in seconds."/>
  </structdef>
</ParserSchema>
