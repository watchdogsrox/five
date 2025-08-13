#ifndef INC_THROWNWEAPONINFO_H_
#define INC_THROWNWEAPONINFO_H_

// Rage headers
#include "Vector/Matrix34.h"

// Game headers
#include "ModelInfo/BaseModelInfo.h"
#include "scene/RegdRefTypes.h"

// Forward declarations
class CObject;

class CThrownWeaponInfo
{
public:

	static void Init();
	static void Shutdown();

	static void LoadThrownWeaponInfo();

#if __BANK
	static void InitWidgets();
#endif

	//
	// Query CThrownWeaponInfo objects
	//

	// Get the thrown weapon info from the object hash key
	static CThrownWeaponInfo* GetThrownWeaponInfo(const char* pName) { return GetThrownWeaponInfo(atStringHash(pName)); }
	static CThrownWeaponInfo* GetThrownWeaponInfo(u32 uNameHash);

	// Get the thrown weapon info from the model index
	static CThrownWeaponInfo* GetThrownWeaponInfoFromModelIndex(strLocalIndex iModelIndex);

	// Get the mass for the projectile
	static float GetThrownWeaponMass() { return ms_fDefaultProjectileMass; }

	//
	// CThrownWeaponInfo functions
	//

	CThrownWeaponInfo();
	~CThrownWeaponInfo() {}

	// Get the thrown weapon offset matrix
	void GetOffsetMatrix(const CObject* pObj, Matrix34& rv) const;

	bool operator< (const CThrownWeaponInfo& other) const { return m_uNameHash <  other.m_uNameHash; }
	bool operator==(const CThrownWeaponInfo& other) const { return m_uNameHash == other.m_uNameHash; }

	//
	// CThrownWeaponInfo accessors
	//

	bool IsDisabled() const { return m_bIsDisabled; }

private:

	// Parse in the ThrownWeaponInfo.xml file
	static void LoadThrownWeaponInfoXMLFile(const char* szFileName);

	// Function to sort the array so we can use a binary search
	static int PairSort(const CThrownWeaponInfo* a, const CThrownWeaponInfo* b) { return (a->m_uNameHash < b->m_uNameHash ? -1 : 1); }

#if __BANK

	// Widget callbacks
	static void GiveThrownWeaponObject();
	static void UpdateObjectPosOffset();
	static void UpdateObjectRotOffset();

	// Debug cleanup
	static void DestroySelectedObject();
#endif

	// The mass thrown projectiles are set too
	static bank_float ms_fDefaultProjectileMass;

	// Name hash
	u32 m_uNameHash;

	// Transform applied to object to produce better looking results
	Matrix34 m_matOffset;

	// Disallow this object to be picked up
	bool m_bIsDisabled;

#if __BANK

	// Debug vars
	strStreamingObjectName m_name;

	// Widget vars
	static int ms_iSelectedObjectIndex;
	static RegdObj ms_pSelectedObject;
	static Vector3 ms_vObjectPosOffset;
	static Vector3 ms_vObjectRotOffset;
#endif
};

#endif // INC_THROWNWEAPONINFO_H_
