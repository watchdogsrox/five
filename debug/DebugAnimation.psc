<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="MoveTransitionStringCollection" base="SetTaggedDataCollectionEvent">
	</structdef>

<structdef type="MoveSetValueStringCollection" base="SetTaggedDataCollectionEvent">
  </structdef>

<structdef type="MoveConditionStringCollection" base="SetTaggedDataCollectionEvent">
  </structdef>

<structdef type="AnimMotionTreeTracker" base="PhysicsEvent">
  <pointer name="m_DumpNetwork" type="fwMoveDumpNetwork" policy="owner"/>
  <string name="m_Label" type="atHashString"/>
</structdef>

<structdef type="MoveGetValue" base="SetTaggedFloatEvent">
	</structdef>

<structdef type="MoveSetValue" base="SetTaggedFloatEvent">
	</structdef>

<structdef type="StoreSkeleton::f16Vector" simple="true">
  <Float16 name="m_X"/>
  <Float16 name="m_Y"/>
  <Float16 name="m_Z"/>
</structdef>

  <structdef type="StoreSkeleton::u32Quaternion" simple="true">
    <u32 name="m_uPackedData"/>
  </structdef>


  <structdef type="StoreSkeleton" base="PhysicsEvent" constructable="false">
		<string name="m_SkeletonName" type="atHashString"/>
		<u32 name="m_DataSignature"/>
    <u32 name="m_iColor"/>
		<array name="m_BonePositions" type="atArray">
			<struct type="StoreSkeleton::f16Vector"/>
		</array>
    <array name="m_BoneRotations" type="atArray">
      <struct type="StoreSkeleton::u32Quaternion"/>
    </array>

  </structdef>

<structdef type="StoreSkeletonPreUpdate" base="StoreSkeleton">
</structdef>

<structdef type="StoreSkeletonPostUpdate" base="StoreSkeleton">
</structdef>

  <structdef type="StoredSkelData::BoneLink" simple="true">
		<u16 name="m_iBone0"/>
		<u16 name="m_iBone1"/>
	</structdef>

<structdef type="StoredSkelData" simple="true">
		<array name="m_BoneLinks" type="atArray">
			<struct type="StoredSkelData::BoneLink"/>
		</array>
    <array name="m_BoneIndexToFilteredIndex" type="atArray">
      <u16/>
    </array>
   <array name="m_FilteredIndexToBoneIndex" type="atArray">
     <u16/>
    </array>
  <u32 name="m_SkelSignature"/>
</structdef>

</ParserSchema>