<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="debugLocation">
	<string name="name" type="atHashString"/>
	<Vector3 name="playerPos"/>
	<Matrix34 name="cameraMtx"/>
	<u32 name="hour" min="0" max="24"/>
</structdef>

<structdef type="debugLocationList">
	<array name="locationList" type="atArray">
		<pointer type="debugLocation" policy="owner"/>
	</array>
</structdef>

</ParserSchema>