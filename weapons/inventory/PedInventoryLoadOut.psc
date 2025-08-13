<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CLoadOutItem" constructable="false">
</structdef>

<enumdef type="CLoadOutWeapon::Flags">
	<enumval name="EquipThisWeapon"/>
	<enumval name="SelectThisWeapon"/>
	<enumval name="InfiniteAmmo"/>
	<enumval name="SPOnly"/>
	<enumval name="MPOnly"/>
</enumdef>

<structdef type="CLoadOutWeapon" base="CLoadOutItem">
	<string name="m_WeaponName" type="atHashString"/>
	<u32 name="m_Ammo" init="0" min="0" max="15000"/>
	<bitset name="m_Flags" type="fixed32" numBits="32" values="CLoadOutWeapon::Flags"/>
	<array name="m_ComponentNames" type="atArray">
		<string type="atHashString"/>
	</array>
</structdef>

<structdef type="CLoadOutRandom::sRandomItem">
	<pointer name="Item" type="CLoadOutItem" policy="owner"/>
	<float name="Chance" init="1.0f"/>
</structdef>

<structdef type="CLoadOutRandom" base="CLoadOutItem" onPostLoad="PostLoad">
	<array name="m_Items" type="atArray">
		<struct type="CLoadOutRandom::sRandomItem"/>
	</array>
</structdef>

<structdef type="CPedInventoryLoadOut">
	<string name="m_Name" type="atHashString"/>
	<array name="m_Items" type="atArray">
		<pointer type="CLoadOutItem" policy="owner"/>
	</array>
</structdef>

<structdef type="CPedInventoryLoadOutManager">
	<array name="m_LoadOuts" type="atArray">
		<struct type="CPedInventoryLoadOut"/>
	</array>
</structdef>

</ParserSchema>