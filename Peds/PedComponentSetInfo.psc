<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CPedComponentSetInfo">
	<string name="m_Name" type="atHashString" ui_key="true"/>
	<bool name="m_HasFacial" init="true"/>
	<bool name="m_HasRagdollConstraints" init="true"/>
	<bool name="m_HasInventory" init="true"/>
	<bool name="m_HasVfx" init="true"/>
	<bool name="m_HasSpeech" init="true"/>
	<bool name="m_HasPhone" init="true"/>
	<bool name="m_HasHelmet" init="true"/>
	<bool name="m_HasShockingEventReponse" init="true"/>
	<bool name="m_HasReins" init="false"/>
	<bool name="m_IsRidable" init="false"/>
</structdef>

<structdef type="CPedComponentSetManager">
	<array name="m_Infos" type="atArray">
    <pointer type="CPedComponentSetInfo" policy="owner"/>
	</array>
	<pointer name="m_DefaultSet" type="CPedComponentSetInfo" policy="external_named" toString="CPedComponentSetManager::GetInfoName" fromString="CPedComponentSetManager::GetInfoFromName"/>
</structdef>

<structdef type="CPedComponentClothInfo">
  <string name="m_Name" type="atHashString" ui_key="true"/>
  <int name="m_ComponentID"/>
  <int name="m_DrawableID"/>
  <int name="m_ComponentTargetID"/>
  <int name="m_ClothSetID"/>  
</structdef>

<structdef type="CPedComponentClothManager">
  <array name="m_Infos" type="atArray">
    <struct type="CPedComponentClothInfo"/>
  </array>
</structdef>

</ParserSchema>