<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="MatrixStruct" autoregister="true">
	<Matrix34 name="m_Data"/>
</structdef>

<structdef type="LawEntityEvent" base="PhysicsEvent" autoregister="true">
	<array name="m_PrimaryTaskStack" type="atArray">
		<string type="atHashString"/>
	</array>
	<array name="m_PrimaryTaskSubState" type="atArray">
		<string type="atHashString"/>
	</array>
	<pointer name="mp_VehicleMatrix" type="MatrixStruct" policy="owner"/>
	<struct name="m_TargetId" type="phInstId"/>
	<struct name="m_VehicleId" type="phInstId"/>
	<u32 name="m_FrameIndex"/>
	<bool name="m_IsClone"/>
</structdef>

<structdef type="LawUpdateEvent" base="EventBase" autoregister="true">
	<int name="m_iWantedLevel"/>
</structdef>

</ParserSchema>