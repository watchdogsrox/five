<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="eAnimatedPostFXMode">
  <enumval name="POSTFX_IN_HOLD_OUT"/>
  <enumval name="POSTFX_EASE_IN_HOLD_EASE_OUT"/>
  <enumval name="POSTFX_EASE_IN"/>
  <enumval name="POSTFX_EASE_OUT"/>
</enumdef>

<enumdef type="eAnimatedPostFXLoopMode">
  <enumval name="POSTFX_LOOP_ALL"/>
  <enumval name="POSTFX_LOOP_HOLD_ONLY"/>
  <enumval name="POSTFX_LOOP_NONE"/>
</enumdef>

<enumdef type="AnimPostFXEventType">
  <enumval name="ANIMPOSTFX_EVENT_INVALID" value="-1"/>
  <enumval name="ANIMPOSTFX_EVENT_ON_START"/>
  <enumval name="ANIMPOSTFX_EVENT_ON_STOP"/>
  <enumval name="ANIMPOSTFX_EVENT_ON_FRAME"/>
  <enumval name="ANIMPOSTFX_EVENT_ON_START_ON_STOP"/>
  <enumval name="ANIMPOSTFX_EVENT_ON_START_ON_STOP_ON_FRAME"/>
  <enumval name="ANIMPOSTFX_EVENT_ON_START_ON_FRAME"/>
  <enumval name="ANIMPOSTFX_EVENT_ON_STOP_ON_FRAME"/>
</enumdef>

<structdef type="AnimatedPostFX">
  <string name="m_ModifierName" type="atHashString" ui_key="true"/>
  <enum name="m_AnimMode" type="eAnimatedPostFXMode" init="POSTFX_IN_HOLD_OUT"/>
  <u32 name="m_StartDelayDuration" min="0" max="100000" step="1"/>  
  <u32 name="m_InDuration" min="0" max="100000" step="1"/>
  <u32 name="m_HoldDuration" min="0" max="100000" step="1"/>
  <u32 name="m_OutDuration" min="0" max="100000" step="1"/>
  <bool name="m_Disabled" init="false"/>
  <enum name="m_LoopMode" type="eAnimatedPostFXLoopMode" init="POSTFX_LOOP_ALL"/>
  <bool name="m_CanBePaused" init="false"/>
</structdef>

<structdef type="LayerBlendModifier">
  <string name="m_LayerA" type="atHashString" init=""/>
  <string name="m_LayerB" type="atHashString" init=""/>
  <bool name="m_Disabled" init="true"/>
  <float name="m_FrequencyNoise" min="0.0f" max ="1.0f" init="0.0f"/>
  <float name="m_AmplitudeNoise" min="0.0f" max ="1.0f" init="0.0f"/>
  <float name="m_Frequency" min="0.0f" max ="100.0f" init="1.0f"/>
  <float name="m_Bias" min="0.0f" max ="1.0f" init="0.5f"/>
</structdef>

<structdef type="AnimatedPostFXStack" onPostLoad="PostLoadCallback">
  <struct name="m_LayerBlend" type="LayerBlendModifier"/> 
  <array name="m_Layers" type="atFixedArray" size="6">
    <struct type="AnimatedPostFX"/>
  </array>
  <s32 name="m_GroupId" init="-1"/>
  <u32 name="m_EventTimeMS" init="0"/>
  <u32 name="m_EventUserTag" init="0"/>
  <enum name="m_EventType" type="AnimPostFXEventType" init="ANIMPOSTFX_EVENT_INVALID"/> 
</structdef>

<structdef type="RegisteredAnimPostFXStack">
  <string name="m_Name" type="atHashString" ui_key="true"/>
  <struct name="m_FXStack" type="AnimatedPostFXStack"/>
  <u8 name="m_Priority" min="0" max="255" init="0"/>
</structdef>

<structdef type="AnimPostFXManager">
  <array name="m_RegisteredStacks" type="atArray">
    <struct type="RegisteredAnimPostFXStack"/>
  </array>
</structdef>

<structdef type="PauseMenuPostFXManager">
  <float name="m_FadeInCheckTreshold" min="0.0f" max ="1.0f" init="0.0f"/>
  <float name="m_FadeOutCheckTreshold" min="0.0f" max ="1.0f" init="0.0f"/>
</structdef>
  
</ParserSchema>
