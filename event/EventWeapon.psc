<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	
	<structdef type="CEventGunAimedAt::Tunables"  base="CTuning">
		<float name="m_MinDelayTimer" min="0.0f" max="1.0f" step="0.01f" init="0.2f" />
		<float name="m_MaxDelayTimer" min="0.0f" max="1.0f" step="0.01f" init="0.5f" />
	</structdef>

	<structdef type="CEventGunShot::Tunables"  base="CTuning">
		<float name="m_MinDelayTimer" min="0.0f" max="1.0f" step="0.01f" init="0.2f" />
		<float name="m_MaxDelayTimer" min="0.0f" max="1.0f" step="0.01f" init="0.5f" />
		<float name="m_GunShotThresholdDistance" min="0.0f" max="5.0f" step="0.01f" init="2.0f" />
	</structdef>

	<structdef type="CEventMeleeAction::Tunables"  base="CTuning">
		<float name="m_MinDelayTimer" min="0.0f" max="1.0f" step="0.01f" init="0.2f" />
		<float name="m_MaxDelayTimer" min="0.0f" max="1.0f" step="0.01f" init="0.5f" />
	</structdef>

</ParserSchema>
