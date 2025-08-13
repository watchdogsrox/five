<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CEventExplosionHeard::Tunables"  base="CTuning">
		<float name="m_MaxCombineDistThresholdSquared" min="0.0f" max="10000.0f" step="1.0f" init="64.0f" description="Square of the max distance between two explosion events for combining them."/>
	</structdef>

</ParserSchema>
