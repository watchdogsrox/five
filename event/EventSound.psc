<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CEventFootStepHeard::Tunables" base="CTuning">
		<float name="m_MinDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="0.2f"/>
		<float name="m_MaxDelayTimer" min="0.0f" max="2.0f" step="0.1f" init="0.4f"/>
	</structdef>

	<structdef type="audFootstepEventMinMax">
		<float name="m_MinDist" min="0.0f" max="500.0f" step="0.1f"/>
		<float name="m_MaxDist" min="0.0f" max="500.0f" step="0.1f"/>
	</structdef>

	<const name="audFootstepEventTuning::kNumFootstepMinMax" value="5"/>
	
	<structdef type="audFootstepEventTuning">
		<array name="m_SinglePlayer" type="atRangeArray" size="audFootstepEventTuning::kNumFootstepMinMax">
			<struct type="audFootstepEventMinMax"/>
		</array>
		<array name="m_MultiPlayer" type="atRangeArray" size="audFootstepEventTuning::kNumFootstepMinMax">
			<struct type="audFootstepEventMinMax"/>
		</array>
	</structdef>

</ParserSchema>
