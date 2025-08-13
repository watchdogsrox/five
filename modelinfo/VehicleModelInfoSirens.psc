<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	<structdef type="sirenCorona">
		<float name="intensity"/>
		<float name="size"/>
		<float name="pull"/>
		<bool name="faceCamera"/>
	</structdef>

	<structdef type="sirenRotation" onPostLoad="PostLoad">
		<float name="delta"/>
		<float name="start"/>
		<float name="speed"/>
    <u32 name="sequencer"/>
    <u8 name="multiples"/>
		<bool name="direction"/>
		<bool name="syncToBpm"/>
	</structdef>

	<structdef type="sirenLight">
		<struct type="sirenRotation" name="rotation"/>
    <struct type="sirenRotation" name="flashiness"/>
    <struct type="sirenCorona" name="corona"/>
		<Color32 name="color"/>
		<float name="intensity" init="1.0"/>
		<u8 name="lightGroup"/>
		<bool name="rotate"/>
    <bool name="scale"/>
    <u8 name="scaleFactor" init="0"/>
    <bool name="flash"/>
    <bool name="light"/>
	<bool name="spotLight"/>
	<bool name="castShadows"/>
	</structdef>

  <structdef type="sirenSettings::sequencerData">
    <u32 name="sequencer"/>
  </structdef>

  
  <structdef type="sirenSettings" onPostLoad="PostLoad">
    <u8 name="id" init ="0xFF"/>
		<string name="name" type="pointer" ui_key="true"/>
		
		<float name="timeMultiplier"/>
		<float name="lightFalloffMax"/>
		<float name="lightFalloffExponent"/>
		<float name="lightInnerConeAngle"/>
		<float name="lightOuterConeAngle"/>
		<float name="lightOffset"/>
    <string name="textureName" type="atHashWithStringNotFinal" init=""/>

    <u32 name="sequencerBpm"/>
    <struct type="sirenSettings::sequencerData" name="leftHeadLight"/>
    <struct type="sirenSettings::sequencerData" name="rightHeadLight"/>
    <struct type="sirenSettings::sequencerData" name="leftTailLight"/>
    <struct type="sirenSettings::sequencerData" name="rightTailLight"/>
    
    <u8 name="leftHeadLightMultiples"/>
    <u8 name="rightHeadLightMultiples"/>
    <u8 name="leftTailLightMultiples"/>
    <u8 name="rightTailLightMultiples"/>

    <bool name="useRealLights"/>

		<array name="sirens" type="atFixedArray" size="20">
			<struct type="sirenLight"/>
		</array>
	</structdef>

</ParserSchema>