<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="CAnimSceneForceMotionStateEvent" base="CAnimSceneEvent" onPostAddWidgets="OnPostAddWidgets" >
    <struct type="CAnimSceneEntityHandle" name="m_ped" />
    <enum name="m_motionState" type="CPedMotionStates::eMotionState" hideWidgets="true"/>
    <bool name="m_restartState" init="false" />
    <pad bytes="4" platform="32bit"/>    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

  <structdef type="CAnimSceneCreatePedEvent" base="CAnimSceneEvent" >
    <struct type="CAnimSceneEntityHandle" name="m_entity" />
    <string type="atFinalHashString" name="m_modelName" />
    <pad bytes="4" platform="32bit"/>    <!-- m_data-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

  <structdef type="CAnimSceneCreateObjectEvent" base="CAnimSceneEvent" >
    <struct type="CAnimSceneEntityHandle" name="m_entity" />
    <string type="atFinalHashString" name="m_modelName" />
    <pad bytes="4" platform="32bit"/>    <!-- m_data-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

  <structdef type="CAnimSceneCreateVehicleEvent" base="CAnimSceneEvent" >
    <struct type="CAnimSceneEntityHandle" name="m_entity" />
    <string type="atFinalHashString" name="m_modelName" />
    <pad bytes="4" platform="32bit"/>    <!-- m_data-->
    <pad bytes="8" platform="64bit"/>
  </structdef>
  
  <structdef type="CAnimScenePlaySceneEvent" base="CAnimSceneEvent" >
    <string type="atHashString" name="m_sceneName" />
    <struct type="CAnimSceneEntityHandle" name="m_trigger" description="Optional trigger value. If specified, the scene will not trigger unless the boolean entity's value is true."/>
    <bool name="m_bLoopScene" init="false" description="If true, the child scene will loop (and the parent scene will be looped internally) until the breakout trigger is hit"/>
    <struct type="CAnimSceneEntityHandle" name="m_breakOutTrigger" description="Must be specified if m_bLoopScene is true. Breaks the internal scene looping when this value becomes true."/>
    <bool name="m_bBreakOutImmediately" init="false" description="If true, the parent scene will jump to the end of this event when the breakout condition is hit"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_data-->
    <pad bytes="8" platform="64bit"/>
  </structdef>

  <structdef type="CAnimSceneInternalLoopEvent" base="CAnimSceneEvent" >
    <struct type="CAnimSceneEntityHandle" name="m_breakOutTrigger" description="Breaks the internal scene looping when this value becomes true."/>
    <bool name="m_bBreakOutImmediately" init="false" description="If true, the parent scene will jump to the end of this event when the breakout condition is hit"/>
    <pad bytes="1" /> <!-- m_bDisable -->
  </structdef>


</ParserSchema>