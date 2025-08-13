<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CPageBase" constructible="false">
    <pointer name="m_view" parName="View" type="CPageViewBase" policy="owner" />
    <string name="m_title" parName="Title" type="atHashString" />
	</structdef>

  <structdef type="CCategorylessPageBase" base="CPageBase" constructible="false">
  </structdef>
  
</ParserSchema>
