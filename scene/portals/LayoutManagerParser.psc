<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">


<structdef type="CShopData">
    <string  name="m_Name" type="atString"/>
    <float  name="m_Price" init="0.0"/>
    <string  name="m_Description" type="atString"/>
</structdef>

<structdef type="CRsRef">
     <string  name="m_Name" type="atString"/>
     <Vector3 name="m_Translation"/>
     <Vector3 name="m_Rotation"/>
     <struct name="m_ShopData" type="CShopData"/>
     <array name="m_LayoutNodeList" type="atArray">
       <pointer type="CLayoutNode" policy="owner"/>
     </array>
 </structdef>

<structdef type="CGroup">
  <int name="m_Id"/>
  <string name="m_Name" type="atString"/>
  <array name="m_RefList" type="atArray">
    <pointer type="CRsRef" policy="owner"/>
  </array>
</structdef>

<structdef type="CLayoutNode">
    <string  name="m_Name" type="atString"/>
    <Vector3 name="m_Translation"/>
    <Vector3 name="m_Rotation"/>
    <bool name="m_Purchasable"/>
    <array name="m_GroupList" type="atArray">
      <pointer type="CGroup" policy="owner"/>
    </array>
</structdef>
  
<structdef type="CMiloRoom">
	<string name="m_Name" type ="atString"/>
	<array name="m_LayoutNodeList" type="atArray">
		<struct type="CLayoutNode"/>
	</array>
</structdef>

<structdef type="CMiloInterior">
    <string name="m_Name" type ="atString"/>
    <string name="m_File" type="atString"/>
    <array name="m_RoomList" type="atArray">
      <struct type="CMiloRoom"/>
    </array>
</structdef>
  
<structdef type="CLayoutMasterList" simple="true">
	<array name="m_Files" type="atArray">
		<string type="atString"/>
	</array>
</structdef>

</ParserSchema>