<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CEntityEdit">
	<u32 name="m_guid"/>
	<Vector3 name="m_position"/>
	<Vector4 name="m_rotation"/>
	<float name="m_scaleXY"/>
	<float name="m_scaleZ"/>
	<float name="m_lodDistance"/>
	<float name="m_childLodDistance"/>
	<bool name="m_bForceFullMatrix"/>
</structdef>

<structdef type="CEntityDelete">
	<u32 name="m_guid"/>
</structdef>

<structdef type="CEntityAdd">
	<string name="m_name" type="atHashString"/>
	<u32 name="m_guid"/>
</structdef>

<structdef type="CEntityFlagChange">
	<u32 name="m_guid"/>
	<bool name="m_bDontRenderInShadows"/>
	<bool name="m_bOnlyRenderInShadows"/>
	<bool name="m_bDontRenderInReflections"/>
	<bool name="m_bOnlyRenderInReflections"/>
	<bool name="m_bDontRenderInWaterReflections"/>
	<bool name="m_bOnlyRenderInWaterReflections"/>
</structdef>

<structdef type="CEntityEditSet">
	<string name="m_mapSectionName" type="atHashString"/>
	<array name="m_edits" type="atArray">
		<struct type="CEntityEdit"/>
	</array>
	<array name="m_deletes" type="atArray">
		<struct type="CEntityDelete"/>
	</array>
	<array name="m_adds" type="atArray">
		<struct type="CEntityAdd"/>
	</array>
	<array name="m_flagChanges" type="atArray">
		<struct type="CEntityFlagChange"/>
	</array>

</structdef>

<structdef type="CFreeCamAdjust">
	<Vector3 name="m_vPos"/>
	<Vector3 name="m_vFront"/>
	<Vector3 name="m_vUp"/>
</structdef>

<structdef type="CEntityPickEvent">
	<u32 name="m_guid"/>
	<string name="m_mapSectionName" type="atHashString"/>
	<string name="m_modelName" type="atHashString"/>
</structdef>

<structdef type="CMapLiveEditRestInterface">
	<array name="m_incomingEditSets" type="atArray">
		<struct type="CEntityEditSet"/>
	</array>
	<array name="m_incomingCameraChanges" type="atArray">
		<struct type="CFreeCamAdjust"/>
	</array>
	<array name="m_incomingPickerSelect" type="atArray">
		<struct type="CEntityPickEvent"/>
	</array>
</structdef>

</ParserSchema>