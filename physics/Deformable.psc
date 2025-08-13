<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CDeformableBoneData" onPostLoad="OnPostLoad">
	<string name="m_boneName" type="atString" />
	<float name="m_strength" init="1.0" min="0.0" max="1.0" />
	<float name="m_radius" init="0.5" min="0.0" />
	<float name="m_damageVelThreshold" init="1.5" min="0.0" />
</structdef>

<structdef type="CDeformableObjectEntry" onPostLoad="OnPostLoad">
	<string name="m_objectName" type="atHashString" />
	<float name="m_objectStrength" init="1.0" min="0.0" />
	<array name="m_DeformableBones" type="atArray">
		<struct type="CDeformableBoneData" />
	</array>
</structdef>

<structdef type="CDeformableObjectManager" onPostLoad="OnPostLoad">
	<map name="m_DeformableObjects" type="atBinaryMap" key="atHashString">
		<struct type="CDeformableObjectEntry" />
	</map>
</structdef>

</ParserSchema>
