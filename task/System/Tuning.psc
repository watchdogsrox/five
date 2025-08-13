<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CTuning" constructable="false">
		<string name="m_Name" type="atHashString" hideWidgets="true"/>
	</structdef>

	<structdef type="CTuningFile" simple="true">
		<array name="m_Tunables" type="atArray">
			<pointer type="CTuning" policy="owner"/>
		</array>
	</structdef>

</ParserSchema>
