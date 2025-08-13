<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CPageItemBase" constructible="false">
    <string name="m_id" parName="ID" type="atHashString" />
    <string name="m_title" parName="Title" type="atHashString" />
    <string name="m_tooltip" parName="Tooltip" type="atHashString" />

    <pointer name="m_onSelectMessage" parName="OnSelect" type="uiPageDeckMessageBase" policy="owner" />
	</structdef>
  
</ParserSchema>
