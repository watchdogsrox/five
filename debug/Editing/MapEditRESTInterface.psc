<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CLodDebugEntityDelete">
	<u32 name="m_hash"/>
	<Vec3V name="m_posn"/>
	<u32 name="m_guid"/>
	<string name="m_imapName" type="atHashString"/>
  <string name="m_modelName" type="atHashString"/>
</structdef>

<structdef type="CLodDebugLodOverride">
	<u32 name="m_hash"/>
	<Vec3V name="m_posn"/>
	<float name="m_distance"/>
	<u32 name="m_guid"/>
	<string name="m_imapName" type="atString"/>
  <string name="m_modelName" type="atHashString"/>
</structdef>

<structdef type="CAttributeDebugAttribOverride">
  <u32 name="m_hash"/>
  <Vec3V name="m_posn"/>
  <bool	name="m_bDontCastShadows"/>
  <bool	name="m_bDontRenderInShadows"/>
  <bool	name="m_bDontRenderInReflections"/>
  <bool	name="m_bOnlyRenderInReflections"/>
  <bool	name="m_bDontRenderInWaterReflections"/>
  <bool	name="m_bOnlyRenderInWaterReflections"/>
  <bool	name="m_bDontRenderInMirrorReflections"/>
  <bool	name="m_bOnlyRenderInMirrorReflections"/>
  <bool	name="m_bStreamingPriorityLow"/>
  <int name="m_iPriority"/>
  <u32 name="m_guid"/>
  <string name="m_imapName" type="atString"/>
  <string name="m_modelName" type="atHashString"/>
</structdef>

<structdef type="CEntityMatrixOverride">
	<u32 name="m_hash"/>
	<u32 name="m_guid"/>
	<string name="m_imapName" type="atString"/>
  <string name="m_modelName" type="atHashString"/>
	<Mat34V name="m_matrix" />
</structdef>

<structdef type="COutputGameCameraState">
	<Vec3V name="m_vPos"/>
	<Vec3V name="m_vFront" />
	<Vec3V name="m_vUp" />
	<float name="m_fov" />
</structdef>

<structdef type="CMapEditRESTInterface">
	<array name="m_distanceOverride" type="atArray">
		<struct type="CLodDebugLodOverride"/>
	</array>
	<array name="m_childDistanceOverride" type="atArray">
		<struct type="CLodDebugLodOverride"/>
	</array>
	<array name="m_mapEntityDeleteList" type="atArray">
		<struct type="CLodDebugEntityDelete"/>
	</array>
	<array name="m_attributeOverride" type="atArray">
	  <struct type="CAttributeDebugAttribOverride"/>
	</array>
	<array name="m_entityMatrixOverride" type="atArray">
		<struct type="CEntityMatrixOverride"/>
	</array>
	<struct name="m_gameCamera" type="COutputGameCameraState" />
</structdef>

</ParserSchema>