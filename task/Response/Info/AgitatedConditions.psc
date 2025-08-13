<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CAgitatedCondition" constructable="false">
  </structdef>

  <structdef type="CAgitatedConditionMulti" base="CAgitatedCondition" constructable="false">
    <array name="m_Conditions" type="atArray">
      <pointer type="CAgitatedCondition" policy="owner"/>
    </array>
  </structdef>

  <structdef type="CAgitatedConditionAnd" base="CAgitatedConditionMulti">
  </structdef>

  <structdef type="CAgitatedConditionOr" base="CAgitatedConditionMulti">
  </structdef>

  <structdef type="CAgitatedConditionNot" base="CAgitatedCondition">
    <pointer name="m_Condition" type="CAgitatedCondition" policy="owner"/>
  </structdef>

	<structdef type="CAgitatedConditionTimeout" base="CAgitatedCondition">
		<float name="m_Time" init="0.0f"/>
		<float name="m_MinTime" init="0.0f"/>
		<float name="m_MaxTime" init="0.0f"/>
	</structdef>

	<structdef type="CAgitatedConditionCanCallPolice" base="CAgitatedCondition">
	</structdef>
	
	<structdef type="CAgitatedConditionCanFight" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionCanHurryAway" base="CAgitatedCondition">
	</structdef>
	
	<structdef type="CAgitatedConditionCanStepOutOfVehicle" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionCanWalkAway" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionCheckBraveryFlags" base="CAgitatedCondition">
		<bitset name="m_Flags" type="fixed" numBits="32" values="eBraveryFlags"/>
	</structdef>

	<structdef type="CAgitatedConditionHasBeenHostileFor" base="CAgitatedCondition">
		<float name="m_MinTime" init="0.0f"/>
		<float name="m_MaxTime" init="0.0f"/>
	</structdef>

	<structdef type="CAgitatedConditionHasContext" base="CAgitatedCondition">
		<string name="m_Context" type="atPartialHashValue"/>
	</structdef>

	<structdef type="CAgitatedConditionHasFriendsNearby" base="CAgitatedCondition">
		<u8 name="m_Min" init="0"/>
	</structdef>

	<structdef type="CAgitatedConditionHasLeader" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionHasLeaderBeenFightingFor" base="CAgitatedCondition">
		<float name="m_MinTime" init="0.0f"/>
		<float name="m_MaxTime" init="0.0f"/>
	</structdef>

	<structdef type="CAgitatedConditionHasVehicle" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsAGunPulled" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsAgitatorArmed" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsAgitatorEnteringVehicle" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsAgitatorInOurTerritory" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsAgitatorInVehicle" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsAgitatorInjured" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsAgitatorMovingAway" base="CAgitatedCondition">
		<float name="m_Dot" init="0.0f"/>
	</structdef>

  <structdef type="CAgitatedConditionIsAngry" base="CAgitatedCondition">
    <float name="m_Threshold"/>
  </structdef>

  <structdef type="CAgitatedConditionIsArgumentative" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsAvoiding" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsBecomingArmed" base="CAgitatedCondition">
	</structdef>
	
  <structdef type="CAgitatedConditionIsBumped" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsBumpedByVehicle" base="CAgitatedCondition">
  </structdef>
  
  <structdef type="CAgitatedConditionIsBumpedInVehicle" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsConfrontational" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsConfronting" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsContext" base="CAgitatedCondition">
    <string name="m_Context" type="atHashString" />
  </structdef>

	<structdef type="CAgitatedConditionIsDodged" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsDodgedVehicle" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsDrivingVehicle" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsExitingScenario" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsFacing" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsFearful" base="CAgitatedCondition">
    <float name="m_Threshold"/>
  </structdef>

	<structdef type="CAgitatedConditionIsFighting" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsFollowing" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsFleeing" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsFlippingOff" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsFriendlyTalking" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsGettingUp" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsGriefing" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsGunAimedAt" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsHarassed" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsHash" base="CAgitatedCondition">
		<string name="m_Value" type="atHashString" />
	</structdef>

  <structdef type="CAgitatedConditionIsHostile" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsHurryingAway" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsInjured" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsInsulted" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsIntervene" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsIntimidate" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsInVehicle" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsLastAgitationApplied" base="CAgitatedCondition">
		<enum name="m_Type" type="AgitatedType" />
	</structdef>

	<structdef type="CAgitatedConditionIsLawEnforcement" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsLeaderAgitated" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsLeaderFighting" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsLeaderInState" base="CAgitatedCondition">
		<string name="m_State" type="atHashString" />
	</structdef>

	<structdef type="CAgitatedConditionIsLeaderStill" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsLeaderTalking" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsLeaderUsingResponse" base="CAgitatedCondition">
		<string name="m_Name" type="atHashString" />
	</structdef>

  <structdef type="CAgitatedConditionIsLoitering" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsMale" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsOutsideClosestDistance" base="CAgitatedCondition">
    <float name="m_Distance"/>
  </structdef>

  <structdef type="CAgitatedConditionIsOutsideDistance" base="CAgitatedCondition">
    <float name="m_Distance"/>
  </structdef>

  <structdef type="CAgitatedConditionIsProvoked" base="CAgitatedCondition">
	  <bool name="m_IgnoreHostility" init="false"/>
  </structdef>

	<structdef type="CAgitatedConditionIsRanting" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsSirenOn" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsStanding" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsTalking" base="CAgitatedCondition">
	</structdef>

  <structdef type="CAgitatedConditionIsWandering" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsUsingScenario" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsTerritoryIntruded" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIntruderLeft" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsUsingTerritoryScenario" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsPlayingAmbientsInScenario" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsReadyForScenarioResponse" base="CAgitatedCondition">
  </structdef>

  <structdef type="CAgitatedConditionIsTargetDoingAMeleeMove" base="CAgitatedCondition">
  </structdef>

	<structdef type="CAgitatedConditionIsUsingRagdoll" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionRandom" base="CAgitatedCondition">
		<float name="m_Chances"/>
	</structdef>

	<structdef type="CAgitatedConditionWasLeaderHit" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionWasRecentlyBumpedWhenStill" base="CAgitatedCondition">
		<float name="m_MaxTime" init="5.0f"/>
	</structdef>

	<structdef type="CAgitatedConditionWasUsingTerritorialScenario" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionHasPavement" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsCallingPolice" base="CAgitatedCondition">
	</structdef>

	<structdef type="CAgitatedConditionIsSwimming" base="CAgitatedCondition">
	</structdef>
	
</ParserSchema>
