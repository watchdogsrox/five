<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="sLevelData">
	<string name="m_cFriendlyName" type="atString" ui_key="true"/>
	<string name="m_cTitle" type="atString"/>
	<string name="m_cFilename" type="atString"/>
	<string name="m_cBugstarName" type="atString"/>
</structdef>
  
<structdef type="CLevelData">
	<array type="atArray" name="m_aLevelsData">
		<struct type="sLevelData"/>
	</array>
</structdef>
 
</ParserSchema>