<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CPedMotivationInfo">
	<string name="m_Name" type="atHashString"/>
	<float name="m_Happy" init="0.5" min="0" max="1"/>
	<float name="m_Anger" init="0" min="0" max="1"/>
	<float name="m_Fear" init="0" min="0" max="1"/>
	<float name="m_Hunger" init="0" min="0" max="1"/>
	<float name="m_Tired" init="0" min="0" max="1"/>
	<float name="m_HappyRate" init="0.5" min="0" max="100"/>
	<float name="m_AngerRate" init="0" min="0" max="100"/>
	<float name="m_FearRate" init="0" min="0" max="100"/>
	<float name="m_HungerRate" init="0" min="0" max="100"/>
	<float name="m_TiredRate" init="0" min="0" max="100"/>
</structdef>

<structdef type="CPedMotivationInfoManager">
	<array name="m_aPedMotivation" type="atArray">
		<struct type="CPedMotivationInfo"/>
	</array>
</structdef>

</ParserSchema>