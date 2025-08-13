<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CPageViewBase::eBackoutMode">
    <enumval name="BACKOUT_DEFAULT"/>
    <enumval name="BACKOUT_WITH_WARNING"/>
    <enumval name="BACKOUT_BLOCKED"/>    
  </enumdef>
  
	<structdef type="CPageViewBase" constructible="false">
    <array name="m_hotkeyMapping" parName="Hotkeys" type="atArray">
      <pointer type="uiPageHotkey" policy="simple_owner" />
    </array>
    <string name="m_backOutLabel" parName="BackLabel" type="atHashString" />
    <string name="m_backoutWarningLabel" parName="BackWarningLabel" type="atHashString" />
    <string name="m_acceptLabel" parName="AcceptLabel" type="atHashString" />
    <enum name="m_backoutMode" parName="BackoutMode" type="CPageViewBase::eBackoutMode" init="0" />
	</structdef>
  
</ParserSchema>
