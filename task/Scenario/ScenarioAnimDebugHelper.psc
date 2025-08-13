<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="ScenarioAnimDebugInfo" cond="__BANK">
	<string name="m_ConditionName" type="atHashString"/>
	<string name="m_PropName" type="atHashString"/>
	<string name="m_ClipSetId" type="atHashString"/>
	<string name="m_ClipId" type="atHashString"/>
	<enum name="m_AnimType" type="CConditionalAnims::eAnimType"/>
	<float name="m_BasePlayTime" init="4.0f"/>
</structdef>

<structdef type="ScenarioQueueLoadSaveObject" cond="__BANK">
	<array name="m_QueueItems" type="atArray">
		<struct type="ScenarioAnimDebugInfo"/>
	</array>
</structdef>

</ParserSchema>