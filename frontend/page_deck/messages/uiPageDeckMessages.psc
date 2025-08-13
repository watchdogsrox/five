<?xml version="1.0"?>

<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="uiPageDeckMessageBase" constructible="false">
    <string name="m_trackingId" parName="TrackingId" type="atHashString" />
  </structdef>

  <structdef type="uiGoToPageMessage" base="uiPageDeckMessageBase">
    <struct name="m_pageLink" parName="LinkInfo" type="uiPageLink" />
  </structdef>
  
  <structdef type="uiPageDeckAcceptMessage" base="uiPageDeckMessageBase">
  </structdef>

  <structdef type="uiPageDeckBackMessage" base="uiPageDeckMessageBase">
  </structdef>

</ParserSchema>

