<?xml version="1.0" encoding="utf-8"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CVehicleScenarioManager::AttractorTuning::Tunables" base="CTuning">
  <float name="m_ForwardDirectionThresholdCosSquared" min="-3.0f" max="3.0f" init="0.25" />
  <float name="m_MaxDistToPathDefault" init="6.0f" min="0.0f" max="30.0f" />
  <float name="m_MaxDistToVehicle" init="50.0f" min="0.0f" max="200.0f" />
  <float name="m_MinDistToVehicle" init="10.0f" min="0.0f" max="200.0f" />
  <int name="m_NumToUpdatePerFrame" init="2" min="1" max="4" />
  <u32 name="m_TimeAfterAttractionMs" init="120000" min="0" max="150000" />
  <u32 name="m_TimeAfterChainTestFailedMs" init="60000" min="30000" max="200000" />
  <u32 name="m_TimeAfterFailedConditionsMs" init="2000" min="500" max="10000" />
  <u32 name="m_TimeAfterNoBoundsMs" init="5000" min="1000" max="15000" />
  <u16 name="m_MinPassengersForAttraction" init="0" min="0" max="4" />
  <u16 name="m_MaxPassengersForAttraction" init="0" min="0" max="8" />
</structdef>

</ParserSchema>
