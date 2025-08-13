// VEHICLE MODEL EXTENSIONS:
// Classes describing extra information about vehicles which is not needed for all vehicles.

#ifndef VEHICLE_MODEL_INFO_DOORS_H_
#define VEHICLE_MODEL_INFO_DOORS_H_

#if !__SPU // This file gets pulled in by the wheel SPU job.

// Rage headers.
#include "atl/array.h"
// Game headers.
#include "Vehicles/vehicle.h"

// Framework headers.
#include "entity/extensionlist.h"

// Forward declarations.
enum eDoorId;


const u8 MAX_NUM_DOORS = (u8)CVehicle::MAX_NUM_DOOR_COMPONENTS;

class CVehicleModelInfoDoors : public fwExtension
{
	friend class CVehicleModelInfo;

public:
	EXTENSIONID_DECL(CVehicleModelInfoDoors, fwExtension);

	CVehicleModelInfoDoors();
	~CVehicleModelInfoDoors() {}

	eHierarchyId ConvertExtensionIdToHierarchyId(eDoorId doorId);

	u32 AddDoorWithCollisionId(eHierarchyId doorId);
	bool ContainsThisDoorWithCollision(eHierarchyId doorId);

    u32 AddDriveableDoorId(eHierarchyId doorId);
    bool ContainsThisDriveableDoor(eHierarchyId doorId);

	u32 AddStiffnessMultForThisDoorId(eHierarchyId doorId, float fStiffnessMult);
	float GetStiffnessMultForThisDoor(eHierarchyId doorId);

private:
	eHierarchyId m_doorsWithCollisionWhenClosed[MAX_NUM_DOORS];
	u8 m_nNumDoorsWithCollisionWhenClosed;

    eHierarchyId m_driveableDoors[MAX_NUM_DOORS];
    u8 m_nNumDriveableDoors;

	eHierarchyId m_DoorWithStiffnessMults[MAX_NUM_DOORS];
	float m_DoorStiffnessMults[MAX_NUM_DOORS];
	u8 m_nNumDoorStiffnessMults;
};

class CVehicleModelInfoBuoyancy : public fwExtension
{
	friend class CVehicleModelInfo;

public:
	EXTENSIONID_DECL(CVehicleModelInfoBuoyancy, fwExtension);

	CVehicleModelInfoBuoyancy();
	~CVehicleModelInfoBuoyancy() {}

	void SetBuoyancySphereOffset(const Vector3& vOffset) { m_vBuoyancySphereOffset = vOffset; }
	const Vector3& GetBuoyancySphereOffset() const { return m_vBuoyancySphereOffset; }

	void SetBuoyancySphereSizeScale(float fValue) { m_fBuoyancySphereSizeScale = fValue; }
	float GetBuoyancySphereSizeScale() const { return m_fBuoyancySphereSizeScale; }

	void SetAdditionalVfxWaterSamples(const atArray<CAdditionalVfxWaterSample>& sampleArray) { m_additionalVfxWaterSamples = sampleArray; }
	int GetNumAdditionalVfxSamples() const { return m_additionalVfxWaterSamples.GetCount(); }
	const CAdditionalVfxWaterSample* GetAdditionalVfxSamples() const { return m_additionalVfxWaterSamples.GetElements(); }

private:
	Vector3 m_vBuoyancySphereOffset;
	float m_fBuoyancySphereSizeScale;

	atArray<CAdditionalVfxWaterSample> m_additionalVfxWaterSamples;
};

class CConvertibleRoofWindowInfo : public fwExtension
{
	friend class CVehicleModelInfo;

public:
	EXTENSIONID_DECL(CConvertibleRoofWindowInfo, fwExtension);

	CConvertibleRoofWindowInfo();
	~CConvertibleRoofWindowInfo() {}

	eHierarchyId ConvertExtensionIdToHierarchyId(eWindowId nWindowId);

	u32 AddWindowId(eHierarchyId nWindowId);
	bool ContainsThisWindowId(eHierarchyId nWindowId) const;
	atArray<eHierarchyId>& GetWindowList() { return m_WindowsAffected; }

private:
	atArray<eHierarchyId> m_WindowsAffected;
};

class CVehicleModelInfoBumperCollision : public fwExtension
{
	friend class CVehicleModelInfo;

public:
	EXTENSIONID_DECL(CVehicleModelInfoBumperCollision, fwExtension);

	CVehicleModelInfoBumperCollision() {};
	~CVehicleModelInfoBumperCollision() {}

	void SetBumpersNeedMapCollision(bool bValue) { m_bBumpersNeedToCollideWithMap = bValue; }
	bool GetBumpersNeedMapCollision() const { return m_bBumpersNeedToCollideWithMap; }

private:
	bool m_bBumpersNeedToCollideWithMap;
};

class CVehicleModelInfoRagdollActivation : public fwExtension
{
	friend class CVehicleModelInfo;

public:
	EXTENSIONID_DECL(CVehicleModelInfoRagdollActivation, fwExtension);

	CVehicleModelInfoRagdollActivation(s32 minComponent, s32 maxComponent, float modifier)
		: m_iMinComponentIndex(minComponent)
		, m_iMaxComponentIndex(maxComponent)
		, m_fActivationModifier(modifier)
		{};

	~CVehicleModelInfoRagdollActivation() {};

	float GetRagdollActivationModifier(s32 sComponent) const { return sComponent>=m_iMinComponentIndex && sComponent<= m_iMaxComponentIndex ? m_fActivationModifier : 1.0f; }

private:
	s32 m_iMinComponentIndex;
	s32 m_iMaxComponentIndex;
	float m_fActivationModifier;
};

#endif // !__SPU

#endif // VEHICLE_MODEL_INFO_DOORS_H_
