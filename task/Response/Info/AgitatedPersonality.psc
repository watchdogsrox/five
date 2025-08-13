<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<enumdef type="CAgitatedPersonality::Flags">
		<enumval name="IsAggressive" />
	</enumdef>
	
	<structdef type="CAgitatedPersonality">
		<bitset name="m_Flags" type="fixed8" values="CAgitatedPersonality::Flags" />
		<map name="m_Contexts" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedContext" />
		</map>
	</structdef>
	
	<structdef type="CAgitatedPersonalities">
		<map name="m_Personalities" type="atBinaryMap" key="atHashString">
			<struct type="CAgitatedPersonality" />
		</map>
	</structdef>
  
</ParserSchema>
