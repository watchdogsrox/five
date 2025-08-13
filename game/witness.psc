<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	
	<structdef type="CCrimeWitnesses::Tunables::Scoring">
		<float name="m_BonusForOnScreen" min="0.0f" max="10.0f" step="0.1f" init="5.0f"/>
		<float name="m_DistanceForMax" min="0.0f" max="100.0f" step="1.0f" init="10.0f"/>
		<float name="m_DistanceForMin" min="0.0f" max="100.0f" step="1.0f" init="40.0f"/>
		<float name="m_BonusForDistance" min="0.0f" max="10.0f" step="0.1f" init="2.0f"/>
		<float name="m_PenaltyForVehicle" min="0.0f" max="1.0f" step="0.01f" init="0.1f"/>
	</structdef>
	
	<structdef type="CCrimeWitnesses::Tunables" base="CTuning">
		<struct name="m_Scoring" type="CCrimeWitnesses::Tunables::Scoring"/>
	</structdef>
	
</ParserSchema>
