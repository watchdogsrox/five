<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CEventPotentialBeWalkedInto::Tunables" base="CTuning">
		<float name="m_AngleThresholdDegrees" min="0.0f" max="180.0f" step="0.1f" init="105.0f"/>
		<float name="m_ChancesToReactToRunningPedsBehindUs" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
    <float name="m_ChanceToUseCasualAvoidAgainstRunningPed" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
	</structdef>
	
	<structdef type="CEventPotentialGetRunOver::Tunables" base="CTuning">
		<float name="m_MinSpeedToDive" min="0.0f" max="1000.0f" step="0.1f" init="10.0f"/>
		<float name="m_SpeedToAlwaysDive" min="0.0f" max="1000.0f" step="0.1f" init="20.0f"/>
		<float name="m_MaxSpeedToDive" min="0.0f" max="1000.0f" step="0.1f" init="30.0f"/>
		<float name="m_ChancesToDive" min="0.0f" max="1.0f" step="0.01f" init="0.5f"/>
		<float name="m_ChancesToBeCasual" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MinDelay" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
		<float name="m_MaxDelay" min="0.0f" max="1.0f" step="0.01f" init="0.2f"/>
	</structdef>

  <structdef type="CEventPotentialBlast::Tunables" base="CTuning">
  </structdef>

</ParserSchema>
