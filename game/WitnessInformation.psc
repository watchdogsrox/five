<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CWitnessPersonality::Flags">
		<enumval name="CanWitnessCrimes"/>
		<enumval name="WillCallLawEnforcement"/>
		<enumval name="WillMoveToLawEnforcement"/>
	</enumdef>

	<structdef type="CWitnessPersonality">
		<bitset name="m_Flags" type="fixed8" values="CWitnessPersonality::Flags"/>
	</structdef>

	<structdef type="CWitnessInformations">
		<map name="m_Personalities" type="atBinaryMap" key="atHashString">
			<struct type="CWitnessPersonality" />
		</map>
	</structdef>
	
</ParserSchema>
