<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="CAnimSceneSection" >
    <struct type="CAnimSceneEventList" name="m_events" hideWidgets="true"/>
    <pad bytes="4"/>    <!-- m_duration-->
    <pad bytes="4"/>    <!-- m_sceneStartTime-->
  </structdef>

  <structdef type="CAnimScenePlaybackList" >
    <array name="m_sections" type="atArray">
      <string type="atHashString"></string>
    </array>
  </structdef>

  <structdef type="CAnimScene" onPostAddWidgets="OnPostAddWidgets" onPreAddWidgets="OnPreAddWidgets">
    <float name="m_rate" min="-20.0f" max="20.0f" init="1.0f"/>
    <struct type="CAnimSceneMatrix" name="m_sceneOrigin"/>
    <map name="m_entities" type="atBinaryMap" key="atHashString" hideWidgets="true">
      <pointer type="CAnimSceneEntity" policy="owner" />
    </map>
    <map name="m_sections" type="atBinaryMap" key="atHashString" hideWidgets="true">
      <pointer type="CAnimSceneSection" policy="owner" />
    </map>
    <map name="m_playbackLists" type="atBinaryMap" key="atHashString" hideWidgets="true">
      <pointer type="CAnimScenePlaybackList" policy="owner" />
    </map>
    <bitset name="m_flags" type="generated" values="eAnimSceneFlags"/>
    <u32 name="m_version" init="0" hideWidgets="true"/>
    
    <pad bytes="4"/>  <!-- m_refs-->
    
    <pad bytes="4" platform="32bit"/> <!-- m_data-->
    <pad bytes="8" platform="64bit"/>
    
    <pad bytes="4" platform="32bit"/> <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>

  </structdef>
  
</ParserSchema>
