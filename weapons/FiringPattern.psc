<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CFiringPatternInfo">
	<string name="m_Name" type="atHashString" ui_key="true"/>
	<s16 name="m_NumberOfBurstsMin" init="-1" min="-1" max="100"/>
	<s16 name="m_NumberOfBurstsMax" init="-1" min="-1" max="100"/>
	<s16 name="m_NumberOfShotsPerBurstMin" init="-1" min="-1" max="100"/>
	<s16 name="m_NumberOfShotsPerBurstMax" init="-1" min="-1" max="100"/>
	<float name="m_TimeBetweenShotsMin" init="-1" min="-1" max="300"/>
	<float name="m_TimeBetweenShotsMax" init="-1" min="-1" max="300"/>
	<float name="m_TimeBetweenShotsAbsoluteMin" init="0" min="0" max="300"/>
	<float name="m_TimeBetweenBurstsMin" init="-1" min="-1" max="300"/>
	<float name="m_TimeBetweenBurstsMax" init="-1" min="-1" max="300"/>
	<float name="m_TimeBetweenBurstsAbsoluteMin" init="0" min="0" max="300"/>
	<float name="m_TimeBeforeFiringMin" init="-1" min="-1" max="300"/>
	<float name="m_TimeBeforeFiringMax" init="-1" min="-1" max="300"/>
	<bool name="m_BurstCooldownOnUnset" init="false"/>
</structdef>

<structdef type="CFiringPatternInfoManager">
	<array name="m_Infos" type="atArray">
		<pointer type="CFiringPatternInfo" policy="owner"/>
	</array>
</structdef>

</ParserSchema>