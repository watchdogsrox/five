<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CConditionalAnimsGroup">
		<string name="m_Name" type="atHashString" ui_key="true"/>
		<array name="m_ConditionalAnims" type="atArray">
			<struct type="CConditionalAnims"/>
		</array>
	</structdef>

	<structdef type="CConditionalAnimManager">
    <array name="m_ConditionalAnimsGroup" type="atArray">      
		<pointer type="CConditionalAnimsGroup" policy="owner"/>
    </array>
  </structdef>

</ParserSchema>
