<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="::CClothCollisionData">
	<Vec4V name="m_Rotation"/>
	<float name="m_CapsuleRadius"/>
	<float name="m_CapsuleLen"/>	
	<int name="m_BoneIndex"/>
	<string name="m_BoneName" type="atString"/>
	<bool name="m_Enabled"/>
</structdef>

<structdef type="::CPedClothCollision">
	<array name="m_CollisionData" type="atArray">
		<struct type="::CClothCollisionData"/>
	</array>
</structdef>

</ParserSchema>