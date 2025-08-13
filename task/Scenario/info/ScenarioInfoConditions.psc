<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CScenarioCondition" constructable="false" onPreAddWidgets="PreAddWidgets" onPostAddWidgets="PostAddWidgets">
    <enum name="m_Result" type="CScenarioCondition::Result"/>
  </structdef>

  <enumdef type="AmbientEventType">
    <enumval name="AET_No_Type"           value="0"/>
    <enumval name="AET_Interesting"       value="1"/>
    <enumval name="AET_Threatening"       value="2"/>
    <enumval name="AET_Threatened"        value="3"/>
    <enumval name="AET_In_Place"          value="4"/>
    <enumval name="AET_Directed_In_Place" value="5"/>
    <enumval name="AET_Flinch"            value="6"/>
  </enumdef>

  <enumdef type="RoleInSyncedScene">
    <enumval name="Invalid"/>
    <enumval name="Master"/>
    <enumval name="Slave"/>
  </enumdef>

  <enumdef type="MovementModeType">
    <enumval name="Invalid"/>
    <enumval name="Action"/>
    <enumval name="Stealth"/>
    <enumval name="Any"/>
  </enumdef>

  <enumdef type="CScenarioCondition::Result">
    <enumval name="True"/>
    <enumval name="False"/>
  </enumdef>

  <enumdef type="CScenarioCondition::Comparison">
    <enumval name="Equal"/>
    <enumval name="LessThan"/>
    <enumval name="GreaterThan"/>
  </enumdef>

  <enumdef type="CScenarioCondition::RadioGenrePar">
    <enumval name="GENRE_GENERIC"/>
    <enumval name="GENRE_COUNTRY"/>
    <enumval name="GENRE_HIPHOP"/>
    <enumval name="GENRE_MEXICAN"/>
    <enumval name="GENRE_MOTOWN"/>
    <enumval name="GENRE_PUNK"/>
  </enumdef>

  <structdef type="CScenarioConditionSet" base="CScenarioCondition">
    <array name="m_Conditions" type="atArray">
      <pointer type="CScenarioCondition" policy="owner"/>
    </array>
  </structdef>

	<structdef type="CScenarioConditionSetOr" base="CScenarioConditionSet">
	</structdef>

  <structdef type="CScenarioConditionWorld" base="CScenarioCondition" constructable="false">
  </structdef>

  <structdef type="CScenarioConditionPopulation" base="CScenarioCondition" constructable="false">
  </structdef>

  <structdef type="CScenarioConditionWorldSet" base="CScenarioConditionWorld">
    <array name="m_Conditions" type="atArray">
      <pointer type="CScenarioConditionWorld" policy="owner"/>
    </array>
  </structdef>
  
  <structdef type="CScenarioConditionPlayingAnim" base="CScenarioCondition">
    <string name="m_ClipSet" type="atHashString"/>
    <string name="m_Anim" type="atHashString"/>
  </structdef>

	<structdef type="CScenarioConditionHasProp" base="CScenarioCondition">
		<string name="m_PropSet" type="atHashString"/>
	</structdef>

	<structdef type="CScenarioConditionModel" base="CScenarioConditionPopulation">
    <pointer name="m_ModelSet" type="CAmbientModelSet" policy="external_named" toString="CAmbientModelSetManager::GetNameFromModelSet" fromString="CAmbientModelSetManager::GetPedModelSetFromNameCB"/>
  </structdef>

  <structdef type="CScenarioConditionSpeed" base="CScenarioCondition">
    <enum name="m_Speed" type="CScenarioConditionSpeed::Speed"/>
  </structdef>

  <enumdef type="CScenarioConditionSpeed::Speed">
    <enumval name="Stand"/>
    <enumval name="Walk"/>
    <enumval name="Run"/>
    <enumval name="Sprint"/>
  </enumdef>

  <structdef type="CScenarioConditionHealth" base="CScenarioCondition">
    <float name="m_Health" init="0"/>
    <enum name="m_Comparison" type="CScenarioCondition::Comparison"/>
  </structdef>

  <structdef type="CScenarioConditionEquippedWeapon" base="CScenarioCondition">
    <string name="m_Weapon" type="atHashString"/>
  </structdef>

  <structdef type="CScenarioConditionDistanceToPlayer" base="CScenarioCondition">
    <float name="m_Range" init="0"/>
    <enum name="m_Comparison" type="CScenarioCondition::Comparison"/>
  </structdef>

  <enumdef type="CScenarioConditionTime::Time" ui_numerical="true">
    <enumval name="0"/>
    <enumval name="1"/>
    <enumval name="2"/>
    <enumval name="3"/>
    <enumval name="4"/>
    <enumval name="5"/>
    <enumval name="6"/>
    <enumval name="7"/>
    <enumval name="8"/>
    <enumval name="9"/>
    <enumval name="10"/>
    <enumval name="11"/>
    <enumval name="12"/>
    <enumval name="13"/>
    <enumval name="14"/>
    <enumval name="15"/>
    <enumval name="16"/>
    <enumval name="17"/>
    <enumval name="18"/>
    <enumval name="19"/>
    <enumval name="20"/>
    <enumval name="21"/>
    <enumval name="22"/>
    <enumval name="23"/>
  </enumdef>

  <structdef type="CScenarioConditionTime" base="CScenarioConditionWorld">
    <bitset name="m_Hours" type="fixed" numBits="24" values="CScenarioConditionTime::Time"/>
  </structdef>

	<enumdef type="CVehicleConditionRoofState::RoofState">
		<enumval name="NoRoof"/>
		<enumval name="HasRoof"/>
		<enumval name="RoofIsDown"/>
		<enumval name="RoofIsUp"/>
	</enumdef>

	<structdef type="CVehicleConditionRoofState" base="CScenarioCondition">
		<bitset name="m_RoofState" type="fixed" numBits="4" values="CVehicleConditionRoofState::RoofState"/>
	</structdef>

	<structdef type="CScenarioConditionArePedConfigFlagsSet" base="CScenarioCondition">
		<bitset name="m_ConfigFlags" type="fixed" numBits="use_values" values="ePedConfigFlags"/>
	</structdef>

	<structdef type="CScenarioConditionArePedConfigFlagsSetOnOtherPed" base="CScenarioCondition">
		<bitset name="m_ConfigFlags" type="fixed" numBits="use_values" values="ePedConfigFlags"/>
	</structdef>

	<structdef type="CScenarioConditionCrouched" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionInCover" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionWet" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionOutOfBreath" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionJustGotUp" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionAlert" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionIsPlayer" base="CScenarioCondition"/>
	<structdef type="CScenarioConditionIsPlayerInMultiplayerGame" base="CScenarioCondition"/>
	<structdef type="CScenarioConditionIsPlayerTired" base="CScenarioCondition"/>
	
  <structdef type="CScenarioConditionRaining" base="CScenarioConditionWorld"/>
  <structdef type="CScenarioConditionSunny" base="CScenarioConditionWorld"/>
  <structdef type="CScenarioConditionSnowing" base="CScenarioConditionWorld"/>
  <structdef type="CScenarioConditionWindy" base="CScenarioConditionWorld"/>
 
	<structdef type="CScenarioConditionInInterior" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionIsMale" base="CScenarioConditionPopulation"/>
  <structdef type="CScenarioConditionHasNoProp" base="CScenarioCondition"/>
  <structdef type="CScenarioConditionIsPanicking" base="CScenarioCondition"/>
	<structdef type="CVehicleConditionEntryPointHasOpenableDoor" base="CScenarioCondition"/>
	<structdef type="CScenarioConditionIsMultiplayerGame" base="CScenarioCondition"/>

  <structdef type="CScenarioConditionOnStraightPath" base="CScenarioCondition">
    <float name="m_MinDist" init="1.0f"/>
  </structdef>

	<structdef type="CScenarioConditionHasParachute" base="CScenarioCondition"/>

  <structdef type="CScenarioConditionUsingJetpack" base="CScenarioCondition" cond="ENABLE_JETPACK"/>

  <structdef type="CScenarioConditionIsSwat" base="CScenarioCondition"/>

  <structdef type="CScenarioConditionAffluence" base="CScenarioConditionPopulation">
    <enum name="m_Affluence" type="Affluence"/>
  </structdef>

  <structdef type="CScenarioConditionTechSavvy" base="CScenarioConditionPopulation">
    <enum name="m_TechSavvy" type="TechSavvy"/>
  </structdef>
  
  <structdef type="CScenarioConditionAmbientEventTypeCheck" base="CScenarioCondition">
    <enum name="m_Type" type="AmbientEventType"/>
  </structdef>

  <structdef type="CScenarioConditionAmbientEventDirection" base="CScenarioCondition">
    <Vector3 name="m_Direction"/>
    <float name="m_Threshold" init="1.0f"/>
    <bool name="m_FlattenZ" init="true"/>
  </structdef>

  <structdef type="CScenarioConditionRoleInSyncedScene" base="CScenarioCondition">
    <enum name="m_Role" type="RoleInSyncedScene"/>
  </structdef>

  <structdef type="CScenarioConditionInVehicleOfType" base="CScenarioCondition">
    <pointer name="m_VehicleModelSet" type="CAmbientModelSet" policy="external_named" toString="CAmbientModelSetManager::GetNameFromModelSet" fromString="CAmbientModelSetManager::GetVehicleModelSetFromNameCB"/>
  </structdef>

  <structdef type="CScenarioConditionIsRadioPlaying" base="CScenarioCondition">
  </structdef>

	<structdef type="CScenarioConditionIsRadioPlayingMusic" base="CScenarioCondition">
	</structdef>
	
  <structdef type="CScenarioConditionInVehicleSeat" base="CScenarioCondition">
    <s8 name="m_SeatIndex"/>
  </structdef>

  <structdef type="CScenarioConditionAttachedToPropOfType" base="CScenarioCondition">
    <pointer name="m_PropModelSet" type="CAmbientModelSet" policy="external_named" toString="CAmbientModelSetManager::GetNameFromModelSet" fromString="CAmbientModelSetManager::GetPropModelSetFromNameCB"/>
  </structdef>

  <structdef type="CScenarioConditionFullyInIdle" base="CScenarioCondition">
  </structdef>

  <structdef type="CScenarioConditionPedHeading" base="CScenarioCondition">
    <float name="m_TurnAngleDegrees"/>
    <float name="m_ThresholdDegrees" init="45.0f"/>
  </structdef>

  <structdef type="CScenarioConditionInStationaryVehicleScenario" base="CScenarioCondition">
    <float name="m_MinimumWaitTime" init="5.0f"/>
  </structdef>

  <structdef type="CScenarioConditionPhoneConversationAvailable" base="CScenarioCondition"/>

  <structdef type="CScenarioPhoneConversationInProgress" base="CScenarioCondition"/>

  <structdef type="CScenarioConditionCanStartNewPhoneConversation" base="CScenarioCondition"/>
  
  <structdef type="CScenarioConditionPhoneConversationStarting" base="CScenarioCondition"/>

  <structdef type="CScenarioConditionMovementModeType" base="CScenarioCondition">
    <enum name="m_MovementModeType" type="MovementModeType"/>
    <string name="m_MovementModeIdle" type="atHashString"/>
    <bool name="m_HighEnergy" init="false"/>
  </structdef>

  <structdef type="CScenarioConditionHasComponentWithFlag" base="CScenarioCondition">
    <enum name="m_PedComponentFlag" type="ePedCompFlags"/>
  </structdef>

	<structdef type="CScenarioConditionPlayerHasSpaceForIdle" base="CScenarioCondition"/>
	<structdef type="CScenarioConditionCanPlayInCarIdle" base="CScenarioCondition"/>

  <structdef type="CScenarioConditionBraveryFlagSet" base="CScenarioCondition">
    <enum name="m_Flag" type="eBraveryFlags"/>
  </structdef>
  <structdef type="CScenarioConditionOnFootClipSet" base="CScenarioCondition">
    <string name="m_ClipSet" type="atHashString"/>
  </structdef>
  
  <structdef type="CScenarioConditionWorldPosWithinSphere" base="CScenarioCondition">
    <struct name="m_Sphere" type="rage::spdSphere" />
  </structdef>

  <structdef type="CScenarioConditionHasHighHeels" base="CScenarioCondition">
  </structdef>

  <structdef type="CScenarioConditionIsReaction" base="CScenarioCondition">
  </structdef>

  <structdef type="CScenarioConditionIsHeadbobbingToRadioMusicEnabled" base="CScenarioCondition">
  </structdef>

  <structdef type="CScenarioConditionHeadbobMusicGenre" base="CScenarioCondition">
    <enum name="m_RadioGenre" type="CScenarioCondition::RadioGenrePar"/>
  </structdef>

	<structdef type="CScenarioConditionIsTwoHandedWeaponEquipped" base="CScenarioCondition">
	</structdef>

  <structdef type="CScenarioConditionUsingLRAltClipset" base="CScenarioCondition">
  </structdef>

</ParserSchema>
