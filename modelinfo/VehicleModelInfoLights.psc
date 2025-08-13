<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="vehicleLight">
      <float name="intensity"/>
      <float name="falloffMax"/>
      <float name="falloffExponent" init="8.0f"/>
      <float name="innerConeAngle"/>
      <float name="outerConeAngle"/>
      <bool name="emmissiveBoost" init="false"/>
      <Color32 name="color"/>
      <string name="textureName" type="atHashWithStringNotFinal" init=""/>
      <bool name="mirrorTexture" init="true"/>
	</structdef>

	<structdef type="vehicleCorona">
      <float name="size"/>
      <float name="size_far"/>
      <float name="intensity"/>
      <float name="intensity_far"/>
      <Color32 name="color"/>
      <u8 name="numCoronas" init="1"/>
      <u8 name="distBetweenCoronas" init="128"/>
      <u8 name="distBetweenCoronas_far" init="255"/>
      <float name="xRotation" init="0.0"/>
      <float name="yRotation" init="0.0"/>
      <float name="zRotation" init="0.0"/>
      <float name="zBias" init="0.08"/>
      <bool name="pullCoronaIn" init="false"/>
	</structdef>

	<structdef type="vehicleLightSettings">
      <u8 name="id" init="0xFF"/>   
      <struct type="vehicleLight" name="indicator"/>
      <struct type="vehicleCorona" name="rearIndicatorCorona"/>
      <struct type="vehicleCorona" name="frontIndicatorCorona"/>
      <struct type="vehicleLight" name="tailLight"/>
      <struct type="vehicleCorona" name="tailLightCorona"/>
      <struct type="vehicleCorona" name="tailLightMiddleCorona"/>
      <struct type="vehicleLight" name="headLight"/>
      <struct type="vehicleCorona" name="headLightCorona"/>
      <struct type="vehicleLight" name="reversingLight"/>
      <struct type="vehicleCorona" name="reversingLightCorona"/>
      <string name="name" type="pointer" ui_key="true"/>
	</structdef>

</ParserSchema>

						
