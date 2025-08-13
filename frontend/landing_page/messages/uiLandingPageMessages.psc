<?xml version="1.0"?>

<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="uiLandingPage_GoToStoryMode" base="uiPageDeckMessageBase">
  </structdef>

  <structdef type="uiLandingPage_GoToOnlineMode" base="uiPageDeckMessageBase">
    <enum name="m_scriptRouterOnlineModeType" parName="ScriptRouterOnlineModeType" type="eScriptRouterContextMode"/>
    <enum name="m_scriptRouterArgType" parName="ScriptRouterArgType" type="eScriptRouterContextArgument"/>
    <string name="m_scriptRouterArg" parName="ScriptRouterArg" type="atString"/>
  </structdef>

  <structdef type="uiLandingPage_GoToEditorMode" base="uiPageDeckMessageBase">
  </structdef>
  
  <structdef type="uiLandingPage_Dismiss" base="uiPageDeckMessageBase">
  </structdef>

</ParserSchema>

