<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
>

	<structdef type="sWeaponInfoList" onPostLoad="::CWeaponInfoManager::UpdateInfoPtrs">
		<array name="Infos" type="atArray">
			<pointer type="CItemInfo" policy="owner"/>
		</array>
	</structdef>

  <structdef type="CWeaponInfoBlob::SlotEntry">
    <int name="m_OrderNumber"/>
    <string name="m_Entry" type="atHashString" ui_key="true" />
  </structdef>

  <structdef type="CWeaponInfoBlob::sWeaponSlotList">
    <array name="WeaponSlots" type="atArray">
      <struct type="CWeaponInfoBlob::SlotEntry"/>
    </array>
  </structdef>

  <structdef type="CWeaponInfoBlob::sWeaponGroupArmouredGlassDamage">
    <string name="GroupHash" type="atHashString"/>
    <float name="Damage" init="10.0f"/>
  </structdef>

  <const name="CWeaponInfoBlob::WS_Max" value="2"/>
  <const name="CWeaponInfoBlob::IT_Max" value="4"/>
  
  <structdef type="CWeaponInfoBlob" onPostLoad="Validate">
    <array name="m_SlotNavigateOrder" type="atFixedArray" size="CWeaponInfoBlob::WS_Max">
      <struct type="CWeaponInfoBlob::sWeaponSlotList"/>
		</array>
		<struct name="m_SlotBestOrder" type="CWeaponInfoBlob::sWeaponSlotList"/>
    <array name="m_TintSpecValues" type="atArray">
        <struct type="CWeaponTintSpecValues"/>
    </array>
    <array name="m_FiringPatternAliases" type="atArray">
        <struct type="CWeaponFiringPatternAliases"/>
    </array>
    <array name="m_UpperBodyFixupExpressionData" type="atArray">
      <struct type="CWeaponUpperBodyFixupExpressionData"/>
    </array>
    <array name="m_AimingInfos" type="atArray">
			<pointer type="CAimingInfo" policy="owner"/>
		</array>
		<array name="m_Infos" type="member" size="CWeaponInfoBlob::IT_Max">
			<struct type="sWeaponInfoList"/>
		</array>
		<array name="m_VehicleWeaponInfos" type="atArray">
			<pointer type="CVehicleWeaponInfo" policy="owner"/>
		</array>
    <array name="m_WeaponGroupDamageForArmouredVehicleGlass" type="atArray">
      <struct type="CWeaponInfoBlob::sWeaponGroupArmouredGlassDamage"/>
    </array>
    <string name="m_Name" type="ConstString"/>
	</structdef>

</ParserSchema>