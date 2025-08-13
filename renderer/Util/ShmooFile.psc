<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
  <structdef type="ShmooItem" layoutHint="novptr" simple="true">
    <string name="m_TechniqueName" type="atString"/>
    <int name="m_Pass"/>
    <int name="m_RegisterCount"/>
  </structdef>
  <structdef type="ShmooFile" layoutHint="novptr" simple="true">
    <array name="m_ShmooItems" type="atArray">
      <struct type="ShmooItem" />
    </array>
  </structdef>
</ParserSchema>