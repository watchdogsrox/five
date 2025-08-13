<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CAmbientModelSets" onPostLoad="ParserPostLoad">
		<array name="m_ModelSets" type="atArray">
			<pointer type="CAmbientModelSet" policy="owner"/>
		</array>
	</structdef>

</ParserSchema>
