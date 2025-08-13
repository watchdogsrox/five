<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <structdef type="CTemplateData">

    <string name="propertyId" type="atHashString"/>
    <int name="index" init="0"/>
    <int name="actionScriptEnum" init="-1"/> 
    <string name="dataId" type="atHashString"/>
    <int name="defaultValue" init="0"/>
    <bool name="visibleInMenu" init="false"/>
  </structdef>
  
<structdef type="CTextTemplateItem">

  <string name="templateId" type="atHashString"/>
  <string name="textId" type="atHashString"/>
  <int name="actionScriptEnum" init="-1"/>

  <array name="data" type="atArray">
    <struct type="CTemplateData"/>
  </array>

</structdef>

  <structdef type="CPropertyData">
    <int name="valueNumber"/>
    <string name="valueHash" type="atHashString"/>
    <string name="valueString" type="atString"/>
    <string name="textId" type="atHashString"/>
  </structdef>

  <structdef type="CPropertyItem">

    <string name="propertyId" type="atHashString"/>

    <array name="data" type="atArray">
      <struct type="CPropertyData"/>
    </array>

  </structdef>

  <structdef type="CTextTemplateXML">
      <array name="CTemplate" type="atArray">
      <struct type="CTextTemplateItem"/>
    </array>

    <array name="CProperties" type="atArray">
      <struct type="CPropertyItem"/>
    </array>

  </structdef>
  
  
  

</ParserSchema>