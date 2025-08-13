<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<!--
		player switch settings
	-->
	<structdef type="CPlayerSwitchSettings">
		<string	name="m_name" type="atHashString" />
		<float	name="m_range" />
		<u32	name="m_numJumpsMin" />
		<u32	name="m_numJumpsMax" />
		<float	name="m_panSpeed" />
		<float	name="m_ceilingHeight" />
		<float	name="m_minHeightAbove" />
		<enum   name="m_ceilingLodLevel" type="::rage::eLodType" />
		<float	name="m_heightBetweenJumps" />
		<float	name="m_skipPanRange" />
    <u32    name="m_maxTimeToStayInStartupModeOnLongRangeDescent" />
    <u32    name="m_scenarioAnimsTimeout" />
   	<float  name="m_radiusOfStreamedScenarioPedCheck" />
	</structdef>

	<const name="CPlayerSwitchInterface::SWITCH_TYPE_TOTAL" value="4"/>

  <!--
		array of settings for each type
	-->
	<structdef type="CPlayerSwitchInterface">
		<array name="m_switchSettings" type="member" size="CPlayerSwitchInterface::SWITCH_TYPE_TOTAL">
			<struct type="CPlayerSwitchSettings" />
		</array>
  </structdef>

</ParserSchema>
	

