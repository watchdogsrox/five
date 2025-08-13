<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="ProfiledObjectLODData">
	<int name="lod"/>
	<float name="min"/>
	<float name="max"/>
	<float name="average"/>
	<float name="std"/>
</structdef>

<structdef type="ProfiledObjectData">
	<string name="objectName" type="atString"/>
	<array name="objectLODData" type="atArray">
		<struct type="ProfiledObjectLODData"/>
	</array>
</structdef>

<structdef type="ObjectProfilerResults">
	<string name="buildVersion" type="atString"/>
	<array name="objectData" type="atArray">
		<struct type="ProfiledObjectData"/>
	</array>
</structdef>
							
</ParserSchema>