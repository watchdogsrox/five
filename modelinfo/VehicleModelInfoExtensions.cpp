// VEHICLE MODEL EXTENSIONS:
// Classes describing extra information about vehicles which is not needed for all vehicles.

// Game headers.
#include "modelinfo/VehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoExtensions.h"

AUTOID_IMPL(CVehicleModelInfoDoors);
AUTOID_IMPL(CVehicleModelInfoBuoyancy);
AUTOID_IMPL(CConvertibleRoofWindowInfo);
AUTOID_IMPL(CVehicleModelInfoBumperCollision);
AUTOID_IMPL(CVehicleModelInfoRagdollActivation);

CVehicleModelInfoDoors::CVehicleModelInfoDoors()
: m_nNumDoorsWithCollisionWhenClosed(0),
m_nNumDriveableDoors(0),
m_nNumDoorStiffnessMults(0)
{

}

eHierarchyId CVehicleModelInfoDoors::ConvertExtensionIdToHierarchyId(eDoorId doorId)
{
	// Convert the enum values used in the extension class into eHierarchyId values.
	eHierarchyId hierarchyId = VEH_INVALID_ID;
	switch(doorId)
	{
	case VEH_EXT_DOOR_DSIDE_F:
		hierarchyId = VEH_DOOR_DSIDE_F;
		break;
	case VEH_EXT_DOOR_DSIDE_R:
		hierarchyId = VEH_DOOR_DSIDE_R;
		break;
	case VEH_EXT_DOOR_PSIDE_F:
		hierarchyId = VEH_DOOR_PSIDE_F;
		break;
	case VEH_EXT_DOOR_PSIDE_R:
		hierarchyId = VEH_DOOR_PSIDE_R;
		break;
	case VEH_EXT_BONNET:
		hierarchyId = VEH_BONNET;
		break;
	case VEH_EXT_BOOT:
		hierarchyId = VEH_BOOT;
		break;
	default:
		Assertf(false, "Unknown vehicle door extension enum value.");
		break;
	}

	return hierarchyId;
}

u32 CVehicleModelInfoDoors::AddDoorWithCollisionId(eHierarchyId doorId)
{
	Assert(m_nNumDoorsWithCollisionWhenClosed<MAX_NUM_DOORS);
	m_doorsWithCollisionWhenClosed[m_nNumDoorsWithCollisionWhenClosed] = doorId;

	// Increment the counter and return the number of doors with collision now stored in this extension.
	++m_nNumDoorsWithCollisionWhenClosed;
	return (u32)m_nNumDoorsWithCollisionWhenClosed;
}


bool CVehicleModelInfoDoors::ContainsThisDoorWithCollision(eHierarchyId doorId)
{
	// Search the stored array of door IDs for the given door. Return true if found.
	bool bFoundDoor = false;
	Assert(m_nNumDoorsWithCollisionWhenClosed<MAX_NUM_DOORS);
	for(u32 i=0; i < m_nNumDoorsWithCollisionWhenClosed; ++i)
	{
		if(m_doorsWithCollisionWhenClosed[i]==doorId)
		{
			bFoundDoor = true;
			break;
		}
	}

	return bFoundDoor;
}


u32 CVehicleModelInfoDoors::AddDriveableDoorId(eHierarchyId doorId)
{
    Assert(m_nNumDriveableDoors<MAX_NUM_DOORS);
    m_driveableDoors[m_nNumDriveableDoors] = doorId;

    // Increment the counter and return the number of doors which are drive-able.
    ++m_nNumDriveableDoors;
    return (u32)m_nNumDriveableDoors;
}

bool CVehicleModelInfoDoors::ContainsThisDriveableDoor(eHierarchyId doorId)
{
    // Search the stored array of door IDs for the given door. Return true if found.
    bool bFoundDoor = false;
    Assert(m_nNumDriveableDoors<MAX_NUM_DOORS);
    for(u32 i=0; i < m_nNumDriveableDoors; ++i)
    {
        if(m_driveableDoors[i]==doorId)
        {
            bFoundDoor = true;
            break;
        }
    }

    return bFoundDoor;
}


u32 CVehicleModelInfoDoors::AddStiffnessMultForThisDoorId(eHierarchyId doorId, float fStiffnessMult)
{
	Assert(m_nNumDoorStiffnessMults<MAX_NUM_DOORS);
	m_DoorWithStiffnessMults[m_nNumDoorStiffnessMults] = doorId;
	m_DoorStiffnessMults[m_nNumDoorStiffnessMults] = fStiffnessMult;

	// Increment the counter and return the number of doors which have stiffness multipliers.
	++m_nNumDoorStiffnessMults;
	return (u32)m_nNumDoorStiffnessMults;
}

float CVehicleModelInfoDoors::GetStiffnessMultForThisDoor(eHierarchyId doorId)
{
	// Search the stored array of door IDs for the given door.
	Assert(m_nNumDoorStiffnessMults<MAX_NUM_DOORS);
	for(u32 i=0; i < m_nNumDoorStiffnessMults; ++i)
	{
		if(m_DoorWithStiffnessMults[i]==doorId)
		{
			return m_DoorStiffnessMults[i];
		}
	}

	return 1.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CVehicleModelInfoBuoyancy::CVehicleModelInfoBuoyancy()
: m_vBuoyancySphereOffset(VEC3_ZERO),
m_fBuoyancySphereSizeScale(1.0f)
{

}

///////////////////////////////////////////////////////////////////////////////////////

CConvertibleRoofWindowInfo::CConvertibleRoofWindowInfo()
{

}

eHierarchyId CConvertibleRoofWindowInfo::ConvertExtensionIdToHierarchyId(eWindowId nWindowId)
{
	// Convert the enum values used in the extension class into eHierarchyId values.
	eHierarchyId hierarchyId = VEH_INVALID_ID;
	switch(nWindowId)
	{
	case VEH_EXT_WINDSCREEN:
		hierarchyId = VEH_WINDSCREEN;
		break;
	case VEH_EXT_WINDSCREEN_R:
		hierarchyId = VEH_WINDSCREEN_R;
		break;
	case VEH_EXT_WINDOW_LF:
		hierarchyId = VEH_WINDOW_LF;
		break;
	case VEH_EXT_WINDOW_RF:
		hierarchyId = VEH_WINDOW_RF;
		break;
	case VEH_EXT_WINDOW_LR:
		hierarchyId = VEH_WINDOW_LR;
		break;
	case VEH_EXT_WINDOW_RR:
		hierarchyId = VEH_WINDOW_RR;
		break;
	case VEH_EXT_WINDOW_LM:
		hierarchyId = VEH_WINDOW_LM;
		break;
	case VEH_EXT_WINDOW_RM:
		hierarchyId = VEH_WINDOW_RM;
		break;
	default:
		Assertf(false, "Unknown vehicle window extension enum value.");
		break;
	}

	return hierarchyId;
}

u32 CConvertibleRoofWindowInfo::AddWindowId(eHierarchyId nWindowId)
{
	m_WindowsAffected.PushAndGrow(nWindowId);

	return (u32)m_WindowsAffected.GetCount();
}

bool CConvertibleRoofWindowInfo::ContainsThisWindowId(eHierarchyId nWindowId) const
{
	// Search the stored array of window IDs for the given window. Return true if found.
	bool bFoundWindow = false;
	for(u32 i=0; i < m_WindowsAffected.GetCount(); ++i)
	{
		if(m_WindowsAffected[i]==nWindowId)
		{
			bFoundWindow = true;
			break;
		}
	}

	return bFoundWindow;
}
