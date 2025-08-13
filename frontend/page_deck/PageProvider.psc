<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CPageProvider" simple="true">

    <map name="m_entrypointMap" parName="Entrypoints" type="atMap" key="atHashString">
      <struct type="uiPageLink"/>
    </map>
    
    <map name="m_pageMap" parName="PageMap" type="atMap" key="atHashString">
      <pointer type="CPageBase" policy="owner" />
    </map>
    
    <struct name="m_defaultEntry" parName="DefaultEntry" type="uiPageLink"/>
  </structdef>

</ParserSchema>
