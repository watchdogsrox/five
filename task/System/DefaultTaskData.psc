<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="DefaultTaskType">
	<enumval name="WANDER"/>
	<enumval name="DO_NOTHING"/>
	<enumval name="MAX_DEFAULT_TASK_TYPES"/>
</enumdef>

<structdef type="sDefaultTaskData">
	<enum name="m_Type" type="DefaultTaskType"/>
</structdef>

<structdef type="CDefaultTaskDataSet">
	<string name="m_Name" type="atHashString"/>
	<pointer name="m_onFoot" type="sDefaultTaskData" policy="owner"/>
</structdef>

<structdef type="CDefaultTaskDataManager">
	<array name="m_aDefaultTaskData" type="atArray">
		<pointer type="CDefaultTaskDataSet" policy="owner"/>
	</array>
	<pointer name="m_DefaultSet" type="CDefaultTaskDataSet" policy="external_named" toString="CDefaultTaskDataManager::GetInfoName" fromString="CDefaultTaskDataManager::GetInfoFromName"/>
</structdef>

</ParserSchema>