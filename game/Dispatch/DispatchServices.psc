<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
	
	<structdef type="CWantedHelicopterDispatch::Tunables"  base="CTuning" >
		<float name="m_TimeBetweenSpawnAttempts" min="0.0f" max="60.0f" step="1.0f" init="30.0f"/>
    <u32 name="m_MinSpawnTimeForPoliceHeliAfterDestroyed" min="0" max="100000" step="100" init="30000"/>
	</structdef>

	<structdef type="CPoliceBoatDispatch::Tunables"  base="CTuning" >
		<float name="m_TimeBetweenSpawnAttempts" min="0.0f" max="60.0f" step="1.0f" init="3.0f"/>
	</structdef>
	
	<structdef type="CAssassinsDispatch::Tunables"  base="CTuning" >
		<float name="m_AssassinLvl1Accuracy" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
		<float name="m_AssassinLvl1ShootRate" min="0.0f" max="100.0f" step="1.0f" init="30.0f"/>
		<float name="m_AssassinLvl1HealthMod" min="0.0f" max="60.0f" step="0.1f" init="1.0f"/>
		<string	name="m_AssassinLvl1WeaponPrimary" type="atHashString" init="WEAPONTYPE_PISTOL"/>
		<string	name="m_AssassinLvl1WeaponSecondary" type="atHashString"/>

		<float name="m_AssassinLvl2Accuracy" min="0.0f" max="100.0f" step="1.0f" init="20.0f"/>
		<float name="m_AssassinLvl2ShootRate" min="0.0f" max="100.0f" step="1.0f" init="60.0f"/>
		<float name="m_AssassinLvl2HealthMod" min="0.0f" max="60.0f" step="0.1f" init="1.2f"/>
		<string	name="m_AssassinLvl2WeaponPrimary" type="atHashString" init="WEAPONTYPE_PISTOL"/>
		<string	name="m_AssassinLvl2WeaponSecondary" type="atHashString" init="WEAPONTYPE_CARBINERIFLE"/>

		<float name="m_AssassinLvl3Accuracy" min="0.0f" max="100.0f" step="1.0f" init="30.0f"/>
		<float name="m_AssassinLvl3ShootRate" min="0.0f" max="100.0f" step="1.0f" init="100.0f"/>
		<float name="m_AssassinLvl3HealthMod" min="0.0f" max="60.0f" step="0.1f" init="1.5f"/>
		<string	name="m_AssassinLvl3WeaponPrimary" type="atHashString" init="WEAPONTYPE_DLC_SPECIALCARBINE"/>
		<string	name="m_AssassinLvl3WeaponSecondary" type="atHashString" init="WEAPONTYPE_ASSAULTSHOTGUN"/>
	</structdef>
	
</ParserSchema>
