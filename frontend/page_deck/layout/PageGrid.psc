<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="PageGridSizingPacket" >
    <float name="m_size" parName="Size" init="1" />
    <float name="m_minSize" parName="Min" init="0" />
    <float name="m_maxSize" parName="Max" init="32678" />
    <enum name="m_type" type="uiPageDeck::eGridSizingType" init="1" />
  </structdef>

  <structdef type="PageGridCol" base="PageGridSizingPacket" >
  </structdef>

  <structdef type="PageGridRow" base="PageGridSizingPacket" >
  </structdef>

	<structdef type="CPageGrid" base="CPageLayoutBase" constructible="false">
	</structdef>

</ParserSchema>
