<?xml version="1.0"?> 
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
							xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

  <structdef type="CAnimSceneLeadInData">
    <struct type="CAnimSceneClipSet" name="m_clipSet"/>
    <string name="m_playbackList" type="atHashString"></string>
  </structdef>

  <structdef type="CAnimScenePed" base="CAnimSceneEntity" onPostAddWidgets="OnPostAddWidgets" >
    <string name="m_debugPedName" type="atHashString" hideWidgets="true"></string>
    <array name="m_leadIns" type="atArray" hideWidgets ="true">
      <struct type="CAnimSceneLeadInData" />
    </array>
    <pad bytes="4" platform="32bit"/>    <!-- m_pPed-->
    <pad bytes="8" platform="64bit"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
    <bool name="m_optional" init="false" hideWidgets ="true" description="If true, the entity is optional in the scene (and will be silently ignored if not registered by the user)"/>
  </structdef>

  <structdef type="CAnimSceneVehicle" base="CAnimSceneEntity" onPostAddWidgets="OnPostAddWidgets">
    <string name="m_debugModelName" type="atHashString" hideWidgets="true"></string>
    <pad bytes="4" platform="32bit"/>    <!-- m_pVeh-->
    <pad bytes="8" platform="64bit"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
    <bool name="m_optional" init="false" hideWidgets ="true" description="If true, the entity is optional in the scene (and will be silently ignored if not registered by the user)"/>
  </structdef>

  <structdef type="CAnimSceneObject" base="CAnimSceneEntity" onPostAddWidgets="OnPostAddWidgets">
    <string name="m_debugModelName" type="atHashString" hideWidgets="true"></string>
    <pad bytes="4" platform="32bit"/>    <!-- m_pObj-->
    <pad bytes="8" platform="64bit"/>
    <pad bytes="4" platform="32bit"/>    <!-- m_widgets-->
    <pad bytes="8" platform="64bit"/>
    <bool name="m_optional" init="false" hideWidgets ="true" description="If true, the entity is optional in the scene (and will be silently ignored if not registered by the user)"/>
  </structdef>

  <structdef type="CAnimSceneBoolean" base="CAnimSceneEntity" >
    <bool name="m_value" init="false" />
    <bool name="m_resetsAtSceneStart" init="true" />
    <pad bytes="1" />    <!-- m_originalValue-->
    <pad bytes="1" />    <!-- m_originalValueCached-->
  </structdef>

  <structdef type="CAnimSceneCamera" base="CAnimSceneEntity" onPostAddWidgets="OnPostAddWidgets" >
  </structdef>

</ParserSchema>