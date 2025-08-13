<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CScenarioAction" constructable="false">
  </structdef>

  <structdef type="CScenarioActionTrigger">
    <array name="m_Conditions" type="atArray">
      <pointer type="CScenarioActionCondition" policy="owner"/>
    </array>
    <pointer name="m_Action" type="CScenarioAction" policy="owner"/>
    <float name="m_Probability" min="0.0f" max="1.0f" init="0.0f" />
  </structdef>

  <structdef type="CScenarioActionTriggers">
    <array name="m_Triggers" type="atArray">
      <pointer type="CScenarioActionTrigger" policy="owner"/>
    </array>
  </structdef>

  <structdef type="CScenarioActionFlee" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionHeadTrack" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionShockReaction" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionThreatResponseExit" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionCombatExit" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionCowardExitThenRespondToEvent" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionImmediateExit" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionNormalExitThenRespondToEvent" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionNormalExit" base="CScenarioAction">
  </structdef>

  <structdef type="CScenarioActionScriptExit" base="CScenarioAction">
  </structdef>

  
</ParserSchema>
