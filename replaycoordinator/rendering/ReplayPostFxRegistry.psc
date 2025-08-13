<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CReplayPostFxData">
    <string name="m_name" type="atHashWithStringNotFinal"/>
    <string name="m_uiName" type="atHashWithStringNotFinal"/>
  </structdef>

  <structdef type="CReplayPostFxRegistry">
    <array name="m_fxData" type="atArray">
      <struct type="CReplayPostFxData" />
    </array>
  </structdef>

</ParserSchema>
