<?xml version="1.0"?>
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

<structdef type="hrsSimTune" cond="ENABLE_HORSE">
<string name="m_HrsTypeName" type="ConstString"/>
<struct name="m_SpeedTune" type="hrsSpeedControl::Tune"/>
<struct name="m_FreshTune" type="hrsFreshnessControl::Tune"/>
<struct name="m_AgitationTune" type="hrsAgitationControl::Tune"/>
<struct name="m_BrakeTune" type="hrsBrakeControl::Tune"/>
</structdef>

<structdef type="hrsSimTuneMgr" cond="ENABLE_HORSE">
<array name="m_SimTune" type="atArray">
	<struct type="hrsSimTune"/>
</array>
</structdef>

</ParserSchema>