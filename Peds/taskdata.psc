<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CTaskDataInfo">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<string name="m_TaskWanderConditionalAnimsGroup" type="atHashString" description="Name of the conditional animations to use by default for CTaskWander."/>
		<float name="m_ScenarioAttractionDistance" init="10.0f"/>
		<float name="m_SurfaceSwimmingDepthOffset" init="0.9f"/>
		<float name="m_SwimmingWanderPointRange" init="10.0f"/>
		<bitset name="m_Flags" type="fixed" numBits="use_values" values="CTaskFlags::Flags"/>
  </structdef>

  <structdef type="CTaskDataInfoManager">
    <array name="m_aTaskData" type="atArray">
      <pointer type="CTaskDataInfo" policy="owner"/>
    </array>
    <pointer name="m_DefaultSet" type="CTaskDataInfo" policy="external_named" toString="CTaskDataInfoManager::GetInfoName" fromString="CTaskDataInfoManager::GetInfoFromName"/>
  </structdef>

</ParserSchema>
