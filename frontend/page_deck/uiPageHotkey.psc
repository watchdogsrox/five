<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="uiPageHotkey" simple="true">
    <pointer name="m_message" parName="Message" type="uiPageDeckMessageBase" policy="owner" />
    <string name="m_label" parName="Label" type="atHashString" />
    <enum name="m_inputType" parName="Input" type="rage::InputType" init="UNDEFINED_INPUT" />
  </structdef>
  
</ParserSchema>
