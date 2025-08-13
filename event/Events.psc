<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CEventRequestHelp::Tunables" base="CTuning">
		<float name="m_MaxRangeWithoutRadioForFistFights" min="0.0f" max="50.0f" step="1.0f" init="10.0f"/>
		<float name="m_MinDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="0.3f"/>
		<float name="m_MaxDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="1.0f"/>
	</structdef>

	<structdef type="CEventPedJackingMyVehicle::Tunables" base="CTuning">
		<float name="m_MinDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="0.0f"/>
		<float name="m_MaxDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="0.5f"/>
	</structdef>
	
	<structdef type="CEventAgitated::Tunables" base="CTuning">
		<u32 name="m_TimeToLive" min="0" max="10000" init="1000" description="How long (in milliseconds) this agitation event has until growing stale."/>
		<float name="m_AmbientEventLifetime" min="0.0f" max="1000.0f" step="1.0f" init="3.0f" description="How long (in seconds) to keep the ambient event in memory."/>
		<float name="m_TriggerAmbientReactionChances" min="0.0f" max="1.0f" step="0.01f" init="1.0f" description="Probability to trigger ambient reaction (1 = always)."/>
		<float name="m_MinDistanceForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="0.0f" description="Minimum distance to trigger an ambient reaction."/>
		<float name="m_MaxDistanceForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="15.0f" description="Maximum distance to trigger an ambient reaction."/>
		<float name="m_MinTimeForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="20.0f" description="Minimum time to play an ambient reaction."/>
		<float name="m_MaxTimeForAmbientReaction" min="0.0f" max="1000.0f" step="1.0f" init="21.0f" description="Maximum time to play an ambient reaction."/>
		<enum name="m_AmbientEventType" type="AmbientEventType" init="AET_Threatening" description="Which sort of ambient event this shocking event can generate."/>
	</structdef>

	<structdef type="CEventRespondedToThreat::Tunables" base="CTuning">
		<float name="m_MinDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="0.3f"/>
		<float name="m_MaxDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="0.6f"/>
	</structdef>

	<structdef type="CEventEncroachingPed::Tunables" base="CTuning">
	</structdef>

	<structdef type="CEventSuspiciousActivity::Tunables" base="CTuning">
		<float name="fMinDistanceToBeConsideredSameEvent" min="0.0f" max="2.0f" init="1.0f"/>
	</structdef>

	<structdef type="CEventCrimeCryForHelp::Tunables" base="CTuning">
    <float name="m_MaxDistance" min="0.0f" max="1000.0f" step="0.5f" init="50.0f"/>
    <float name="m_MinDelayDistance" min="0.0f" max="1000.0f" step="0.5f" init="5.0f"/>
    <float name="m_MaxDelayDistance" min="0.0f" max="1000.0f" step="0.5f" init="50.0f"/>
    <float name="m_MaxDistanceDelay" min="0.0f" max="1000.0f" step="0.5f" init="1.0f"/>
		<float name="m_MinRandomDelay" min="0.0f" max="2.0f" step="0.1f" init="0.3f"/>
		<float name="m_MaxRandomDelay" min="0.0f" max="2.0f" step="0.1f" init="0.6f"/>
  </structdef>

</ParserSchema>
