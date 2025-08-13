#ifndef WEAPON_SCRIPT_RESOURCE_H
#define WEAPON_SCRIPT_RESOURCE_H

// Game headers
#include "streaming/streamingrequest.h"

// Framework headers
#include "fwAnimation/animdefines.h"

////////////////////////////////////////////////////////////////////////////////

class CWeaponScriptResource
{
public:

	// If you add a flag and want it usable from script you need to update the matching enum in commands_weapon.sch
	enum WeaponResourceFlags
	{
		WRF_RequestBaseAnims					= BIT0,
		WRF_RequestCoverAnims					= BIT1,
		WRF_RequestMeleeAnims					= BIT2,
		WRF_RequestMotionAnims					= BIT3,
		WRF_RequestStealthAnims					= BIT4,
		WRF_RequestAllMovementVariationAnims	= BIT5,

		WRF_RequestAll							= 0xFFFFFFFF
	};

	enum ExtraWeaponComponentResourceFlags
	{
		Weapon_Component_Flash		= BIT0,
		Weapon_Component_Scope		= BIT1,
		Weapon_Component_Supp		= BIT2,
		Weapon_Component_Sclip2		= BIT3,
		Weapon_Component_Grip		= BIT4
	};

	CWeaponScriptResource( atHashWithStringNotFinal weaponHash, s32 iRequestFlags, s32 iExtraComponentFlags );
	~CWeaponScriptResource();

	u32		GetHash()			const	{ return m_WeaponHash.GetHash(); }
	bool	GetIsStreamedIn()			{ return m_assetRequester.HaveAllLoaded() && m_assetRequesterAmmo.HaveAllLoaded(); }

private:

	void	RequestAnims( s32 iRequestFlags );
	void	RequestAnimsFromClipSet( const fwMvClipSetId& clipSetId );
	
	// Streaming request helper
	static const s32 MAX_WEAPON_SCRIPT_RESOURCE_REQUESTS = 30;
	strRequestArray<MAX_WEAPON_SCRIPT_RESOURCE_REQUESTS> m_assetRequester;
	static const s32 MAX_ORDNANCE_REQUESTS = 2;
	strRequestArray<MAX_ORDNANCE_REQUESTS> m_assetRequesterAmmo;
	atHashWithStringNotFinal m_WeaponHash;

#if __BANK
	void OutputRequestArrayIfFull();
#endif	//__BANK
};

class CWeaponScriptResourceManager
{
public:

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Add a resource
	static s32 RegisterResource(atHashWithStringNotFinal weaponHash, s32 iRequestFlags, s32 iExtraComponentFlags);

	// Remove a resource
	static void UnregisterResource(atHashWithStringNotFinal weaponHash);

	// Access a resource
	static CWeaponScriptResource* GetResource(atHashWithStringNotFinal weaponHash);

private:

	// Find a resource index
	static s32 GetIndex(atHashWithStringNotFinal weaponHash);

	typedef atArray<CWeaponScriptResource*> WeaponResources;
	static WeaponResources ms_WeaponResources;
};

#endif // WEAPON_SCRIPT_RESOURCE_H
