<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="ScriptedGlow">
    <Color32 name="color"/>
    <float	name="intensity"/>
    <float	name="range"/>
  </structdef>

  <structdef type="ScriptedGlowList">
    <array name="m_data" type="atArray">
      <struct type="ScriptedGlow"/>
    </array>
  </structdef>
  
</ParserSchema>
