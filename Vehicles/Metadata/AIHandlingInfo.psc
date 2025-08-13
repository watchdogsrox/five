<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CAICurvePoint">
		<float name="m_Angle" min="0.0" max="1000.0"/>
		<float name="m_Speed" min="0.0" max="1000.0"/>
	</structdef>

	<structdef type="CAIHandlingInfo">
    <string name="m_Name" type="atHashString" ui_key="true"/>
		<float name="m_MinBrakeDistance" min="0.0" max="1000.0"/>
		<float name="m_MaxBrakeDistance" min="0.0" max="1000.0"/>
		<float name="m_MaxSpeedAtBrakeDistance" min="0.0" max="1000.0"/>
		<float name="m_AbsoluteMinSpeed" min="0.0" max="1000.0"/>
		<array name="m_AICurvePoints" type="atArray">
			<pointer type="CAICurvePoint" policy="owner"/>
		</array>
	</structdef>

  <structdef type="CAIHandlingInfoMgr">
     <array name="m_AIHandlingInfos" type="atArray">
       <pointer type="CAIHandlingInfo" policy="owner"/>
     </array>
  </structdef>

</ParserSchema>