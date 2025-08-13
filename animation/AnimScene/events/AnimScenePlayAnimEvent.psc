<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">
  <structdef type="CAnimScenePlayAnimEvent" base="CAnimSceneEvent" onPostAddWidgets="OnPostAddWidgets">
    <struct name="m_entity" type="CAnimSceneEntityHandle" />
    <struct type="CAnimSceneClip" name="m_clip" />
    <bitset name="m_animFlags" type="generated" values="eScriptedAnimFlags"/>
    <bitset name="m_ragdollBlockingFlags" type="generated" values="eRagdollBlockingFlags"/>
    <bitset name="m_ikFlags" type="generated" values="eIkControlFlags"/>
    <string name="m_filter" type="atHashString" />
    <float name="m_blendInDuration" min="0.0f" max="60.0f" init="0.0f"  onWidgetChanged="NotifyRangeUpdated"/> <!-- NotifyRangeUpdated so we can ensure the event full start is drawn at the end of the blend in.-->
    <float name="m_blendOutDuration" min="0.0f" max="60.0f" init="0.0f" onWidgetChanged="NotifyRangeUpdated"/> <!-- NotifyRangeUpdated so we can ensure the event end is clamped to the start of the blend out.-->
    <float name="m_moverBlendInDuration" min="-1.0f" max="60.0f" init="-1.0" description="Mover blend in duration (for use with the USE_MOVER_EXTRACTION flag). If less than 0.0, the pose value is used instead." />
    <float name="m_startPhase" min="0.0f" max="60.0f" init="0.0f"/>
    <bool name="m_useSceneOrigin" init="true" />
    <pointer type="CAnimSceneMatrix" name="m_pSceneOffset" policy="owner" hideWidgets="true"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_data-->
    <pad bytes="8" platform="64bit"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
  </structdef>
 
</ParserSchema>