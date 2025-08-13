<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="UIGlobalRenderData">
  <float name="m_exposure" min="0.0f" max="4.f" step="0.01f"/>
  <float name="m_dynamicBakeBoostMin" min="0.0f" max="2.f" step="0.01f"/>
  <float name="m_dynamicBakeBoostMax" min="0.0f" max="2.f" step="0.01f"/>
</structdef>

<structdef type="UILightData">
	<Vector3 name="m_position" min="-1000.0f" max="1000.0f"/>
	<float name="m_intensity" min="0.f" max="64.f" step="0.01f"/>
	<float name="m_radius" min="0.f" max="50.f" step="0.01f"/>
	<Color32 name="m_color"/>
</structdef>

<structdef type="UILightRig">
	<array name="m_lights" type="atFixedArray" size="4">
		<struct type="UILightData" name="light"/>
	</array>
	<Color32 name="m_ambientBaseCol"/>
	<float name="m_ambientBaseMult" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
	<Color32 name="m_ambientDownCol"/>
	<float name="m_ambientDownMult" min="0.0f" max="1.0f" step="0.01f" init="1.0f"/>
	<float name="m_ambientDownWrap" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
	<u32 name="m_id"/>
</structdef>

<structdef type="UIPostFXFilterData">
  <Color32 name="m_blackWhiteWeights"/>
  <Color32 name="m_tintColor"/>
  <float name="m_blend" min="0.0f" max="1.0f" step="0.01f" init="0.0f"/>
</structdef> 

<structdef type="UISceneObjectData">
	<Vector3 name="m_position" min="-1000.0f" max="1000.0f"/>
  <Vector3 name="m_position_43" min="-1000.0f" max="1000.0f"/>
  <Vector3 name="m_rotationXYZ" min="-180.0f" max="180.0f" init="0.0f"/>
  <Vector4 name="m_bgRectXYWH" min="0.0f" max="1.0f" init="0.0f"  step="0.001f"/>
  <Vector4 name="m_bgRectXYWH_43" min="0.0f" max="1.0f" init="0.0f"  step="0.001f"/>
  <Color32 name="m_bgRectColor"/>
  <Color32 name="m_blendColor" init="0xff000000"/>
  <Vector2 name="m_perspectiveShear" min="-16.0f" max="16.0f" init="0.0f" step="0.01f"/>
  <struct type="UIPostFXFilterData" name="m_postfxData"/>
  <bool name="m_enabled" init="false"/>
</structdef>

<structdef type="UIScenePreset">
    <string name="m_name" type="atHashString"/>
	<array name="m_elements" type="atFixedArray" size="5">
		<struct type="UISceneObjectData" name="object"/>
	</array>
</structdef>

<structdef type="UI3DDrawManager">
	<array name="m_scenePresets" type="atArray">
		<struct type="UIScenePreset" name="scenePreset"/>
	</array>
	<array name="m_lightRigs" type="atFixedArray" size="4">
		<struct type="UILightRig" name="rig"/>
	</array>
  <struct type="UIGlobalRenderData" name="m_globalRenderData"/> 
</structdef>

</ParserSchema>