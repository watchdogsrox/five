#ifndef PED_EQUIPPED_WEAPON_H
#define PED_EQUIPPED_WEAPON_H

// Game headers
#include "Objects/Object.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Weapon.h"

//////////////////////////////////////////////////////////////////////////
// CPedEquippedWeapon
//////////////////////////////////////////////////////////////////////////

class CPedEquippedWeapon
{
public:

	enum eAttachPoint { AP_RightHand = 0, AP_LeftHand, };

	// Constructor
	CPedEquippedWeapon();
	~CPedEquippedWeapon();

	// Init
	void Init(CPed* pPed);

	// Processing
	void Process();

	// Do we need to instantiate the equipped weapon?
	bool GetRequiresSwitch() const;

	// Set the equipped weapon hash
	void SetWeaponHash(u32 uWeaponHash);

	// Get the equipped weapon hash
	u32 GetWeaponHash() const;

	// Set the equipped vehicle weapon index
	void SetVehicleWeaponInfo(const CWeaponInfo* pWeaponInfo);

	// Get the equipped weapon info
	const CWeaponInfo* GetWeaponInfo() const;
	CWeaponInfo* GetWeaponInfo();
	const CWeaponInfo* GetVehicleWeaponInfo() const;
	CWeaponInfo* GetVehicleWeaponInfo();

	// Get the instantiated object
	const CObject* GetObject() const;
	CObject* GetObject();

	const CWeapon* GetUnarmedEquippedWeapon() const;
	CWeapon* GetUnarmedEquippedWeapon();

	const CWeapon* GetUnarmedKungFuEquippedWeapon() const;
	CWeapon* GetUnarmedKungFuEquippedWeapon();

	const CWeapon* GetWeaponNoModel() const;
	CWeapon* GetWeaponNoModel();

	// Get the object hash
	u32 GetObjectHash() const;

	// Get weapon model index
	u32 GetModelIndex() const;

	// Retrieve the projectile ordnance if the equipped weapon has one
	const CProjectile* GetProjectileOrdnance() const;
	CProjectile* GetProjectileOrdnance();
	static const CProjectile* GetProjectileOrdnance(const CObject* pWeaponObject);
	static CProjectile* GetProjectileOrdnance(CObject* pWeaponObject);

	// Used for automatically reloading water weapons (ie harpoon) after a specified time
	void SetIsReloadInterrupted(bool bInterrupted) { m_bHasInterruptedReload = bInterrupted; }

	//
	// Weapon object processing
	//

	// Instantiate the object from the selected item
	bool CreateObject(eAttachPoint attach);
	void CreateUnarmedWeaponObject(u32 nWeaponHash);
	void CreateUnarmedKungFuWeaponObject(u32 nWeaponHash);

	// Attach an object that already exists in the world, to be used with this equipped weapon
	bool UseExistingObject(CObject& obj, eAttachPoint attach);

	// Instantiate the ordnance for the selected item
	void CreateProjectileOrdnance();

	// Destroy the object
	void DestroyObject();

	// Release the object ownership
	CObject* ReleaseObject();

	// Weapon anim interface
	void RequestWeaponAnims();
	void ReleaseWeaponAnims();

	// Attach the object to the ped
	void AttachObject(eAttachPoint attach, bool bUseSecondObject = false);

	// Get the current attach point
	eAttachPoint GetAttachPoint() const { return m_AttachPoint; }
	void SetAttachPoint(eAttachPoint attach) { m_AttachPoint = attach; }

	//
	// Helper functions
	//

	static CObject* CreateObject(u32 uWeaponNameHash, s32 iAmmo, const Matrix34& mat, const bool bCreateDefaultComponents = false, CEntity* pOwner = NULL, const u32 uModelHashOverride = 0, const fwInteriorLocation* pInteriorLocation = NULL, float fScale = 1.0f, bool bCreatedFromScript = false, u8 tintIndex = 0);
	static CObject* CreateObject(const CWeaponInfo* pWeaponInfo, s32 iAmmo, const Matrix34& mat, const bool bCreateDefaultComponents = false, CEntity* pOwner = NULL, const u32 uModelHashOverride = 0, const fwInteriorLocation* pInteriorLocation = NULL, float fScale = 1.0f, bool bCreatedFromScript = false, u8 tintIndex = 0);
	static void DestroyObject(CObject* pObject, CEntity* pOwner);
	static void AttachObjects(CPhysical* pParent, CPhysical* pChild, s32 iAttachBoneParent, s32 iOffsetBoneChild);
	// Weapon component functions
	static bool CreateWeaponComponent(u32 uComponentNameHash, CObject* pWeaponObject, const bool bDoReload = true, u8 iTintIndex = 0);
	static bool CreateWeaponComponent(u32 uComponentNameHash, CObject* pWeaponObject, eHierarchyId attachPoint, const bool bDoReload = true, u8 iTintIndex = 0);
	static bool CreateWeaponComponent(const CWeaponComponentInfo* pComponentInfo, CObject* pWeaponObject, eHierarchyId attachPoint, const bool bDoReload = true, u8 iTintIndex = 0);
	static void DestroyWeaponComponent(CObject* pWeaponObject, CWeaponComponent* pComponentToDestroy);
	static void ReleaseWeaponComponent(CObject* pWeaponObject, CWeaponComponent* pComponentToRelease);
	// Ordnance
	static void CreateProjectileOrdnance(CObject* pWeaponObject, CEntity* pOwner, bool bReloading = true, bool bCreatedFromScript = false);
	// Setup as weapon object
	static bool SetupAsWeapon(CObject* pWeaponObject, const CWeaponInfo* pWeaponInfo, s32 iAmmo, const bool bCreateDefaultComponents = false, CEntity* pOwner = NULL, CWeapon* pWeapon = NULL, bool bCreatedFromScript = false, u8 tintIndex = 0);

	// are we waiting to stream in clips?
	bool HaveWeaponAnimsBeenRequested() const { return m_bWeaponAnimsRequested; }
	bool AreWeaponAnimsStreamedIn() const { return m_bWeaponAnimsStreamed; };

	// Is the current m_pEquippedWeaponInfo armed
	inline bool GetIsArmed() const { return m_bArmed; }

private:

	//
	// Private helper functions
	//

	static CObject* CreateObject(const CWeaponInfo* pWeaponInfo, const fwModelId& modelId, const Matrix34& mat, CEntity* pOwner = NULL, const fwInteriorLocation* pInteriorLocation = NULL, float fScale = 1.0f, bool bCreatedFromScript = false);

	//
	// Members
	//

	// Owner ped - not a RegdPed as it is owned by CPed (indirectly via CPedWeaponManager)
	CPed* m_pPed;

	// Flag to avoid the AI from requesting the animations multiple times
	bool m_bWeaponAnimsRequested;
	bool m_bWeaponAnimsStreamed;

	// The equipped weapon info - this is what the m_pObject is instantiated from
	const CWeaponInfo* m_pEquippedWeaponInfo;

	// The vehicle weapon info, if we are in a vehicle
	const CWeaponInfo* m_pEquippedVehicleWeaponInfo;

	// Pointer to instantiated object
	RegdObj m_pObject;
	RegdObj m_pSecondObject;
	RegdWeapon m_pUnarmedWeapon;
	RegdWeapon m_pUnarmedKungFuWeapon;
	RegdWeapon m_pWeaponNoModel;

	// Store which point the weapon is attached to
	eAttachPoint m_AttachPoint;

	enum RequestHelperType
	{
		RHT_Weapon,
		RHT_Stealth,
		RHT_Scope,
		RHT_HiCover,

		// Max
		RHT_Count,
	};

	// Streaming
	fwClipSetRequestHelper m_ClipRequestHelpers[RHT_Count][FPS_StreamCount];
	fwClipSetRequestHelper m_WeaponMeleeStealthClipRequestHelper;

	// Cache if the m_pEquippedWeaponInfo is armed
	bool m_bArmed;

	// Auto-reload timer
	float m_fAutoReloadTimer;
	// Used to check if ped has interrupted reload on the gun
	bool m_bHasInterruptedReload;
};

////////////////////////////////////////////////////////////////////////////////

inline u32 CPedEquippedWeapon::GetWeaponHash() const
{
	return m_pEquippedWeaponInfo ? m_pEquippedWeaponInfo->GetHash() : 0;
}

////////////////////////////////////////////////////////////////////////////////

inline void CPedEquippedWeapon::SetVehicleWeaponInfo(const CWeaponInfo* pVehicleWeaponInfo)
{
	ReleaseWeaponAnims();
	m_pEquippedVehicleWeaponInfo = pVehicleWeaponInfo;
	m_pEquippedWeaponInfo = NULL;
	m_bArmed = false;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponInfo* CPedEquippedWeapon::GetWeaponInfo() const
{
	if(m_pEquippedWeaponInfo)
	{
		return m_pEquippedWeaponInfo;
	}
	else
	{
		return m_pEquippedVehicleWeaponInfo;
	}
}

////////////////////////////////////////////////////////////////////////////////

inline CWeaponInfo* CPedEquippedWeapon::GetWeaponInfo()
{
	if(m_pEquippedWeaponInfo)
	{
		return const_cast<CWeaponInfo*>(m_pEquippedWeaponInfo);
	}
	else
	{
		return const_cast<CWeaponInfo*>(m_pEquippedVehicleWeaponInfo);
	}
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeaponInfo* CPedEquippedWeapon::GetVehicleWeaponInfo() const
{
	return m_pEquippedVehicleWeaponInfo;
}

////////////////////////////////////////////////////////////////////////////////

inline CWeaponInfo* CPedEquippedWeapon::GetVehicleWeaponInfo()
{
	return const_cast<CWeaponInfo*>(m_pEquippedVehicleWeaponInfo);
}

////////////////////////////////////////////////////////////////////////////////

inline const CObject* CPedEquippedWeapon::GetObject() const
{
	return m_pObject;
}

////////////////////////////////////////////////////////////////////////////////

inline CObject* CPedEquippedWeapon::GetObject()
{
	return m_pObject;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeapon* CPedEquippedWeapon::GetUnarmedEquippedWeapon() const
{
	return m_pUnarmedWeapon;
}

////////////////////////////////////////////////////////////////////////////////

inline CWeapon* CPedEquippedWeapon::GetUnarmedEquippedWeapon()
{
	return m_pUnarmedWeapon;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeapon* CPedEquippedWeapon::GetUnarmedKungFuEquippedWeapon() const
{
	return m_pUnarmedKungFuWeapon;
}

////////////////////////////////////////////////////////////////////////////////

inline CWeapon* CPedEquippedWeapon::GetUnarmedKungFuEquippedWeapon()
{
	return m_pUnarmedKungFuWeapon;
}

////////////////////////////////////////////////////////////////////////////////

inline const CWeapon* CPedEquippedWeapon::GetWeaponNoModel() const
{
	return m_pWeaponNoModel;
}

////////////////////////////////////////////////////////////////////////////////

inline CWeapon* CPedEquippedWeapon::GetWeaponNoModel()
{
	return m_pWeaponNoModel;
}

////////////////////////////////////////////////////////////////////////////////

inline u32 CPedEquippedWeapon::GetObjectHash() const
{
	return m_pObject && m_pObject->GetWeapon() ? m_pObject->GetWeapon()->GetWeaponHash() : 0;
}

////////////////////////////////////////////////////////////////////////////////

#endif // PED_EQUIPPED_WEAPON_H
