<?xml version="1.0"?>

<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="psoChecks">	

<structdef type="CGrassScalingParams" layoutHint="novptr" simple="true">
	<float name="distanceMultiplier"/>
	<float name="densityMultiplier"/>
</structdef>

<structdef type="CGrassScalingParams_AllLODs" layoutHint="novptr" simple="true">
    <array name="m_AllLods" type="member" size="3">
      <struct type="CGrassScalingParams" />
    </array>
</structdef>


<structdef type="CGrassShadowParams" layoutHint="novptr" simple="true">
	<float name="fadeNear"/>
	<float name="fadeFar"/>
</structdef>

<structdef type="CGrassShadowParams_AllLODs" layoutHint="novptr" simple="true">
    <array name="m_AllLods" type="member" size="3">
      <struct type="CGrassShadowParams" />
    </array>
</structdef>


</ParserSchema>
