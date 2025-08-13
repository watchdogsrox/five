<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="CAnimScenePlayCameraAnimEvent" base="CAnimSceneEvent" onPostAddWidgets="OnPostAddWidgets">
    <struct name="m_entity" type="CAnimSceneEntityHandle" />
    <struct type="CAnimSceneClip" name="m_clip" />
    <float name="m_blendInDuration" min="0.0f" max="60.0f" init="0.0f"  onWidgetChanged="NotifyRangeUpdated"/> <!-- NotifyRangeUpdated so we can ensure the event full start is drawn at the end of the blend in.-->
    <float name="m_blendOutDuration" min="0.0f" max="60.0f" init="0.0f"  onWidgetChanged="NotifyRangeUpdated"/> <!-- NotifyRangeUpdated so we can ensure the event full start is drawn at the end of the blend in.-->
    <float name="m_startPhase" min="0.0f" max="60.0f" init="0.0f"/>
    <bool name="m_useSceneOrigin" init="true"  hideWidgets="true"/>  <!-- To be removed as the  value will be copied to m_cameraSettings flag ASCF_USE_SCENE_ORIGIN-->
    <bitset name="m_cameraSettings" type="generated" values="eCameraSettings"/>
    
    <pointer type="CAnimSceneMatrix" name="m_pSceneOffset" policy="owner" hideWidgets="true"/>
    <pad bytes="4" platform="32bit"/>
    <!-- m_data-->
    <pad bytes="8" platform="64bit"/>
    <pad bytes="4" platform="32bit"/>
    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

</ParserSchema>