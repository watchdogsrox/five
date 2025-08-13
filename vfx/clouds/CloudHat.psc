<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<const name="rage::CloudHatFragContainer::sNumAnimLayers" value="3"/>
  
 <structdef type="CloudHatFragLayer">
   <string name="mFilename" type="atHashString" ui_key="true" hideWidgets="true" description="The name of the cloud hat layer drawable"/>
   <float name="mCostFactor" init="1" min="0" max="1" step="0.01f" description="Cost factor for this layer"/>
   <float name="mRotationScale" init="1" min="0" max="16" step="0.01f" description="rotation scale for this layer"/>
   <float name="mCamPositionScalerAdjust" init="1" min="0" max="1" step="0.01f" description="adjust the global camera position scaler for this layer"/>
   <float name="mTransitionInTimePercent" init="1" min="0" max="1" step="0.01f" description="normalized fade in time (it's multiplied by the transition time when the transition in is triggered)"/>
   <float name="mTransitionOutTimePercent" init="1" min="0" max="1" step="0.01f" description="normalized fade out time (it's multiplied by the transition time when the transition out is triggered)"/>
   <float name="mTransitionInDelayPercent" init="0" min="0" max="1" step="0.01f" description="normalized fade in start delay (it's multiplied by the transition time when the transition in is triggered)"/>
   <float name="mTransitionOutDelayPercent" init="0" min="0" max="1" step="0.01f" description="normalized fade out start delay (it's multiplied by the transition time when the transition out is triggered)"/>
   <float name="mHeightTigger" init="0" min="0" max="10000" step="1.0f" description="the height at which the cloud hat layer will be enabled (0 means always enabled)"/>
   <float name="mHeightFadeRange" init="0" min="1" max="500" step="1.0f" description="The disance above mHeightTigger that the layer is fully fadded in"/>
   <float name="mHeightTrigger2" init="0" min="0" max="10000" step="1.0f" description="The height at which the cloud hat layer will be disabled (0 means always enabled)"/>
   <float name="mHeightFadeRange2" init="0" min="1" max="500" step="1.0f" description="The disance above mHeightTrigger2 that the layer is fully fadded out"/>
 </structdef>

  <structdef name="CloudHatFragContainer" type="rage::CloudHatFragContainer">
	<Vec3V name="mPosition"/>
	<Vec3V name="mRotation"/>
	<Vec3V name="mScale" init="1.0f"/>
	<string name="mName" type="member" size="64"/>
	<array name="mLayers" type="atArray" description="Array of layers">
		<struct type="CloudHatFragLayer" addGroupWidget="false"/>
	</array>
	<float name="mTransitionAlphaRange"/>
	<float name="mTransitionMidPoint"/>
	<bool name="mEnabled" init="true"/>
	<Vec3V name="mAngularVelocity" noInit="true"/>
	<Vec3V name="mAnimBlendWeights" init="1.0f"/>
	<array name="mUVVelocity" type="member" size="rage::CloudHatFragContainer::sNumAnimLayers">
		<Vec2V/>
	</array>
	<array name="mAnimMode" type="member" size="rage::CloudHatFragContainer::sNumAnimLayers">
		<u8/>
	</array>
	<array name="mShowLayer" type="member" size="rage::CloudHatFragContainer::sNumAnimLayers" noInit="true">
		<bool/>
	</array>
	<bool name="mEnableAnimations" init="false"/>
</structdef>

<structdef name="CloudHatManager" type="rage::CloudHatManager">
	<array name="mCloudHatFrags" type="atArray">
		<struct type="rage::CloudHatFragContainer"/>
	</array>
	<float name="mDesiredTransitionTimeSec" type="member"/>
	<Vec3V name="mCamPositionScaler" noInit="true"/>
	<float name="mAltitudeScrollScaler" init="0.0" min="0" max="1" step="0.001f" description="the camera position scale for altitude cloud uv scrolling"/>
</structdef>

</ParserSchema>