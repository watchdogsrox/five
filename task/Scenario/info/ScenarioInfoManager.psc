<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CScenarioTypeGroupEntry">
    <pointer name="m_ScenarioType" type="CScenarioInfo" policy="external_named" toString="CScenarioInfoManager::ToStringFromScenarioInfoCB" fromString="CScenarioInfoManager::FromStringToScenarioInfoCB"/>
    <float name="m_ProbabilityWeight"/>
  </structdef>

  <structdef type="CScenarioTypeGroup">
    <string name="m_Name" type="atHashString" ui_key="true"/>
    <array name="m_Types" type="atArray">
      <struct type="CScenarioTypeGroupEntry"/>
    </array>
  </structdef>

  <structdef type="CScenarioInfoManager" onPostLoad="ParserPostLoad">
    <array name="m_Scenarios" type="atArray">
      <pointer type="CScenarioInfo" policy="owner"/>
    </array>
    <array name="m_ScenarioTypeGroups" type="atArray">      
	  <pointer type="CScenarioTypeGroup" policy="owner"/>
    </array>
  </structdef>
</ParserSchema>
