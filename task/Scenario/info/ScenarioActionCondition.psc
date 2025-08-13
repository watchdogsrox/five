<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="eScenarioActionType">
		<enumval name="eScenarioActionFlee"/>
		<enumval name="eScenarioActionHeadTrack"/>
		<enumval name="eScenarioActionTempCowardReaction"/>
		<enumval name="eScenarioActionCowardExit"/>
		<enumval name="eScenarioActionShockReaction"/>
		<enumval name="eScenarioActionThreatResponseExit"/>
		<enumval name="eScenarioActionCombatExit"/>
	</enumdef>
	
	
  <structdef type="CScenarioActionCondition" constructable="false">
  </structdef>

  <structdef type="CScenarioActionConditionNot" base="CScenarioActionCondition">
    <pointer name="m_Condition" type="CScenarioActionCondition" policy="owner" />
  </structdef>

  <structdef type="CScenarioActionConditionEvent" base="CScenarioActionCondition">
    <enum name="m_EventType" type="eEventType"/>
  </structdef>

  <structdef type="CScenarioActionConditionResponseTask" base="CScenarioActionCondition">
    <string name="m_TaskType" type="atHashString"/>
  </structdef>

  <structdef type="CScenarioActionConditionForceAction" base="CScenarioActionCondition">
    <enum name="m_ScenarioActionType" type="eScenarioActionType"/>
  </structdef>	
	
  <structdef type="CScenarioActionConditionInRange" base="CScenarioActionCondition">
    <float name="m_Range" min="0.0f" max="100.0f" init="0.0f" />
  </structdef>

  <structdef type="CScenarioActionConditionCloseOrRecent" base="CScenarioActionCondition">
    <float name="m_Range" min="0.0f" max="100.0f" init="0.0f" />
  </structdef>

  <structdef type="CScenarioActionConditionHasShockingReact" base="CScenarioActionCondition">
  </structdef>

  <structdef type="CScenarioActionConditionHasCowardReact" base="CScenarioActionCondition">
  </structdef>

  <structdef type="CScenarioActionConditionIsAGangPed" base="CScenarioActionCondition">
  </structdef>

  <structdef type="CScenarioActionConditionResponseType" base="CScenarioActionCondition">
    <string name="m_ResponseHash" type="atHashString"/>
  </structdef>

  <structdef type="CScenarioActionConditionCurrentlyRespondingToOtherEvent" base="CScenarioActionCondition">
  </structdef>

  <structdef type="CScenarioActionConditionCanDoQuickBlendout" base="CScenarioActionCondition">
  </structdef>

  <structdef type="CScenarioActionConditionIsACopPed" base="CScenarioActionCondition">
  </structdef>

  <structdef type="CScenarioActionConditionIsASecurityPed" base="CScenarioActionCondition">
  </structdef>
  
</ParserSchema>
