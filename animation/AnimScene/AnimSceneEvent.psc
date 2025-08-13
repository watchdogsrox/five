<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

    <structdef type="CAnimSceneEvent" onPostAddWidgets="OnPostAddWidgets">
      <float name="m_startTime" min="0.0f" init="0.0f" onWidgetChanged="NotifyRangeUpdated"/>
      <float name="m_endTime" min="0.0f" init="0.0f" onWidgetChanged="NotifyRangeUpdated"/>
      <float name="m_preloadTime" min="0.0f" init="0.0f"/>
      <pointer name="m_pChildren" type="CAnimSceneEventList" policy="owner"  hideWidgets="true"/>
      
      <pad bytes="2"/>  <!-- m_flags -->
      <pad bytes="2"/>  <!-- m_internalFlags -->
      
      <pad bytes="4" platform="32bit"/>   <!-- m_pSection-->
      <pad bytes="8" platform="64bit"/>

      <pad bytes="4" platform="32bit"/>   <!-- m_widgets-->
      <pad bytes="8" platform="64bit"/>
    </structdef>

  <structdef type="CAnimSceneEventList" onPreAddWidgets="OnPreAddWidgets" onPostAddWidgets="OnPostAddWidgets">
    <array name="m_events" type="atArray" hideWidgets ="true">
      <pointer type="CAnimSceneEvent" policy="owner" />
    </array>
    <pad bytes="4" platform="32bit"/>   <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

</ParserSchema>