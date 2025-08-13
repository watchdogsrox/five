<?xml version="1.0"?>
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

<structdef type="::rage::CReinsAttachData" cond="ENABLE_HORSE">
	<Vec3V name="m_Offset"/>
	<int name="m_BoneIndex"/>
	<int name="m_VertexIndex"/>
	<string name="m_BoneName" type="atString"/>
</structdef>

<structdef type="::rage::CReinsCollisionData" cond="ENABLE_HORSE">
	<ScalarV name="m_Rotation"/>
	<float name="m_CapsuleRadius"/>
	<float name="m_CapsuleLen"/>
	<int name="m_BoneIndex"/>
	<int name="m_BoundIndex"/>
	<bool name="m_Enabled"/>
	<string name="m_BoneName" type="atString"/>
</structdef>

<structdef type="::rage::CReins" cond="ENABLE_HORSE">	
	<float name="m_Length"/>
	<int name="m_NumSections"/>
	<bool name="m_UseSoftPinning"/>
	<array name="m_AttachData" type="atArray">
		<struct type="::rage::CReinsAttachData"/>
	</array>
	<array name="m_CollisionData" type="atArray">
		<struct type="::rage::CReinsCollisionData"/>
	</array>
	<array name="m_BindPos" type="atArray">
		<Vec3V/>
	</array>
	<array name="m_BindPos2" type="atArray">
		<Vec3V/>
	</array>
	<array name="m_SoftPinRadiuses" type="atArray">
		<float/>
	</array>	
</structdef>

</ParserSchema>