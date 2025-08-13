<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="PickupFlagsEnum">
	<enumval name="CollectableOnFoot"/>
	<enumval name="CollectableInCar"/>
	<enumval name="CollectableInBoat"/>
	<enumval name="CollectableInPlane"/>
	<enumval name="CollectableOnShot"/>
	<enumval name="LowPriority"/>
	<enumval name="AlwaysFixed"/>
	<enumval name="ManualPickUp"/>
	<enumval name="InvisibleWhenCarried"/>
	<enumval name="RequiresButtonPressToPickup"/>
	<enumval name="CanBeDamaged"/>
	<enumval name="Rotates"/>
	<enumval name="FacePlayer"/>
	<enumval name="ShareWithPassengers"/>
	<enumval name="IgnoreAmmoCheck"/>
	<enumval name="DoesntCollideWithRagdolls"/>
	<enumval name="DoesntCollideWithPeds"/>
	<enumval name="DoesntCollideWithVehicles"/>
	<enumval name="OrientateUpright"/>
	<enumval name="AirbornePickup"/>
	<enumval name="CollectableInCarByPassengers"/>
	<enumval name="AutoCollectIfInInventory"/>
</enumdef>

<structdef type="CPickupData">
	<string name="m_Name" type="atHashString" ui_key="true"/>
	
	<string name="Model" type="atHashString"/>
  <float name="GenerationRange"/>
  <float name="RegenerationTime"/>
  <float name="CollectionRadius"/>
  <float name="CollectionRadiusFirstPerson"/>
  <float name="Scale"/>
	<string name="LoopingAudioRef" type="atHashString"/>
	<float name="GlowRed"/>
	<float name="GlowGreen"/>
	<float name="GlowBlue"/>
	<float name="GlowIntensity"/>
	<float name="GlowRange"/>
  <float name="DarkGlowIntensity" init="-1.0f"/>
  <float name="MPGlowIntensity" init="-1.0f"/>
  <float name="MPDarkGlowIntensity" init="-1.0f"/>
  <enum name="AttachmentBone" type="eAnimBoneTag"/>
	<Vector3 name="AttachmentOffset"/>
	<Vector3 name="AttachmentRotation"/>

	<bitset name="PickupFlags" type="fixed32" numBits="32" values="PickupFlagsEnum"/>

	 <array name="OnFootPickupActions" type="atFixedArray" size="10">
		<string type="atHashString"/>
	</array>
	 <array name="InCarPickupActions" type="atFixedArray" size="10">
		<string type="atHashString"/>
	</array>
	<array name="OnShotPickupActions" type="atFixedArray" size="10">
		<string type="atHashString"/>
	</array>
	<array name="Rewards" type="atFixedArray" size="10">
		<string type="atHashString"/>
	</array>

	<enum name="ExplosionTag" type="eExplosionTag" init="EXP_TAG_DONTCARE"/>
</structdef>

<structdef type="CPickupActionData">
	<string name="m_Name" type="atHashString" ui_key="true"/>
</structdef>

<structdef type="CPickupActionAudio" base="CPickupActionData">
	<string name="AudioRef" type="atHashString"/>
</structdef>

<structdef type="CPickupActionPadShake" base="CPickupActionData">
	<float name="Intensity"/>
	<s32 name="Duration"/>
</structdef>

<structdef type="CPickupActionVfx" base="CPickupActionData">
	<string name="Vfx" type="member" size="32"/>
</structdef>

<structdef type="CPickupActionGroup" base="CPickupActionData">
	<array name="Actions" type="atFixedArray" size="10">
		<string type="atHashString"/>
	</array>
</structdef>

<structdef type="CPickupRewardData">
	<string name="m_Name" type="atHashString" ui_key="true"/>
</structdef>

<structdef type="CPickupRewardAmmo" base="CPickupRewardData">
	<string name="AmmoRef" type="atHashString"/>
	<s32 name="Amount"/>
</structdef>

<structdef type="CPickupRewardBulletMP" base="CPickupRewardData">
</structdef>

<structdef type="CPickupRewardMissileMP" base="CPickupRewardData">
</structdef>

<structdef type="CPickupRewardFireworkMP" base="CPickupRewardData">
</structdef>
  
<structdef type="CPickupRewardGrenadeLauncherMP" base="CPickupRewardData">
</structdef>
  
<structdef type="CPickupRewardArmour" base="CPickupRewardData">
	<s32 name="Armour"/>
</structdef>

<structdef type="CPickupRewardHealth" base="CPickupRewardData">
	<s32 name="Health"/>
</structdef>

<structdef type="CPickupRewardHealthVariable" base="CPickupRewardData">
</structdef>
  
<structdef type="CPickupRewardMoneyVariable" base="CPickupRewardData">
</structdef>

<structdef type="CPickupRewardMoneyFixed" base="CPickupRewardData">
	<s32 name="Money"/>
</structdef>

<structdef type="CPickupRewardStat" base="CPickupRewardData">
	<string name="Stat" type="member" size="32"/>
	<float name="Amount"/>
</structdef>

<structdef type="CPickupRewardStatVariable" base="CPickupRewardData">
  <string name="Stat" type="member" size="32"/>
</structdef>

<structdef type="CPickupRewardWeapon" base="CPickupRewardData">
	<string name="WeaponRef" type="atHashString"/>
	<bool name="Equip" init="false"/>
</structdef>

<structdef type="CPickupRewardVehicleFix" base="CPickupRewardData">
</structdef>

<structdef type="CPickupDataManager">
	<array name="m_pickupData" type="atArray">
		<pointer type="CPickupData" policy="owner"/>
	</array>
	<array name="m_actionData" type="atArray">
		<pointer type="CPickupActionData" policy="owner"/>
	</array>
	<array name="m_rewardData" type="atArray">
		<pointer type="CPickupRewardData" policy="owner"/>
	</array>
</structdef>

</ParserSchema>