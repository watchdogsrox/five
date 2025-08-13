		//
// name:        PlaneSyncNodes.cpp
// description: Network sync nodes used by CNetObjPlane
// written by:    
//

#include "network/objects/synchronisation/syncnodes/PlaneSyncNodes.h"

#include "vehicles/LandingGear.h"
#include "vehicles/Planes.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IPlaneNodeDataAccessor);

CompileTimeAssert(NUM_PLANE_DAMAGE_SECTIONS == CAircraftDamage::NUM_DAMAGE_SECTIONS);

void CPlaneGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_LANDING_GEAR_PUBLIC_STATES = datBitsNeeded<CLandingGear::NUM_PUBLIC_STATES>::COUNT;
	static const unsigned SIZEOF_SECTION_DAMAGE = 10;
	static const unsigned SIZEOF_SECTION_DAMAGE_SCALE = 7;
	static const unsigned SIZEOF_SECTION_FLAGS = NUM_PLANE_DAMAGE_SECTIONS;
	static const unsigned SIZEOF_LOCKON_STATE = 2;
	static const unsigned SIZEOF_PROPELLERS = PLANE_NUM_PROPELLERS;
	static const unsigned int SIZEOF_LOD_DISTANCE = 13;

	bool bHasDamagedSections	= m_DamagedSections != 0;
	bool bHasBrokenSections		= m_BrokenSections  != 0;
	bool bHasBrokenRotor		= m_RotorBroken     != 0;
	bool bHasLockOnTarget		= m_LockOnTarget    != NETWORK_INVALID_OBJECT_ID;
	bool bHasEnabledPropellers	= m_IndividualPropellerFlags != 0;

	SERIALISE_UNSIGNED(serialiser, m_LandingGearPublicState, SIZEOF_LANDING_GEAR_PUBLIC_STATES, "Landing Gear Public State");

	SERIALISE_BOOL(serialiser, bHasDamagedSections);
	SERIALISE_BOOL(serialiser, bHasBrokenSections);
	SERIALISE_BOOL(serialiser, bHasBrokenRotor);
	SERIALISE_BOOL(serialiser, bHasEnabledPropellers);

	SERIALISE_PACKED_FLOAT(serialiser, m_EngineDamageScale, 1.0f, SIZEOF_SECTION_DAMAGE_SCALE, "Engine damage scale");

	SERIALISE_BOOL(serialiser, m_HasCustomSectionDamageScale);
	if (m_HasCustomSectionDamageScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		for (int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
		{
#if ENABLE_NETWORK_LOGGING
			char name[100];
			sprintf(name, "Section %d damage scale", i);
#endif
			SERIALISE_PACKED_FLOAT(serialiser, m_SectionDamageScale[i], 1.0f, SIZEOF_SECTION_DAMAGE_SCALE, name);
		}
	}

	SERIALISE_BOOL(serialiser, m_HasCustomLandingGearSectionDamageScale);
	if (m_HasCustomLandingGearSectionDamageScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		for (int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
		{
#if ENABLE_NETWORK_LOGGING
			char name[100];
			sprintf(name, "Landing gear section %d damage scale", i);
#endif
			SERIALISE_PACKED_FLOAT(serialiser, m_LandingGearSectionDamageScale[i], 1.0f, SIZEOF_SECTION_DAMAGE_SCALE, name);
		}
	}

	if (bHasDamagedSections || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_DamagedSections, SIZEOF_SECTION_FLAGS, "Damaged Sections");

		for (int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
		{
			if ((m_DamagedSections & (1<<i)) || serialiser.GetIsMaximumSizeSerialiser())
			{
#if ENABLE_NETWORK_LOGGING
				char name[100];
				sprintf(name, "Section %d damage", i);
#endif
				SERIALISE_PACKED_FLOAT(serialiser, m_SectionDamage[i], 1.0f, SIZEOF_SECTION_DAMAGE, name);
			}
		}
	}
	else
	{
		m_DamagedSections = 0;
	}

	if (bHasBrokenSections || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_BrokenSections, SIZEOF_SECTION_FLAGS, "Broken Sections");
	}
	else
	{
		m_BrokenSections = 0;
	}

	if (bHasBrokenRotor || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_RotorBroken, CNetObjPlane::MAX_NUM_SYNCED_PROPELLERS, "Broken Rotor");
	}
	else
	{
		m_RotorBroken = 0;
	}

	if (bHasEnabledPropellers || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_IndividualPropellerFlags, SIZEOF_PROPELLERS, "Propellers");
	}
	else
	{
		m_IndividualPropellerFlags = 0;
	}

	SERIALISE_BOOL(serialiser, bHasLockOnTarget);
	SERIALISE_BOOL(serialiser, m_AIIgnoresBrokenPartsForHandling, "Damage affects AI handling");
	SERIALISE_BOOL(serialiser, m_ControlSectionsBreakOffFromExplosions, "Control Sections Break Off From Explosions");
	
	if(bHasLockOnTarget || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_LockOnTarget);
		SERIALISE_UNSIGNED(serialiser, m_LockOnState, SIZEOF_LOCKON_STATE, "Lock-on State");
	}
	else
	{
		m_LockOnTarget = NETWORK_INVALID_OBJECT_ID;
		m_LockOnState  = 0;
	}

	bool hasLODdistance = m_LODdistance != 0;
	SERIALISE_BOOL(serialiser, hasLODdistance, "Has Modified LOD Distance");
	if(hasLODdistance || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_LODdistance, SIZEOF_LOD_DISTANCE, "LOD Distance");
	}
	else
	{
		m_LODdistance = 0;
	}
	SERIALISE_BOOL(serialiser, m_disableExpFromBodyDamage, "Disable Explosion from body damage");
	SERIALISE_BOOL(serialiser, m_disableExlodeFromBodyDamageOnCollision, "Disable Explosion from body damage on collision");

	SERIALISE_BOOL(serialiser, m_AllowRollAndYawWhenCrashing, "Allow Roll And Yaw When Crashing");
	SERIALISE_BOOL(serialiser, m_dipStraightDownWhenCrashing, "Dip Down When Crashing");
}

bool CPlaneGameStateDataNode::HasDefaultState() const
{ 
	return (m_LandingGearPublicState == 0 &&
		m_DamagedSections        == 0 &&
		m_BrokenSections         == 0 &&
		m_RotorBroken			 == 0 &&
		m_LockOnTarget           == NETWORK_INVALID_OBJECT_ID &&
		m_LockOnState            == 0 &&
		m_EngineDamageScale      == 1.0f);
}

void CPlaneGameStateDataNode::SetDefaultState() 
{
	m_LandingGearPublicState = m_DamagedSections = m_BrokenSections = m_RotorBroken = m_LockOnState = 0;
	m_LockOnTarget           = NETWORK_INVALID_OBJECT_ID;
	m_EngineDamageScale      = 1.0f;
}

void CPlaneControlDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_ANGCONTROL   = 8;
	static const unsigned int SIZEOF_THROTTLE     = 8;

	SERIALISE_PACKED_FLOAT(serialiser, m_yawControl,   1.0f, SIZEOF_ANGCONTROL, "Yaw Control");
	SERIALISE_PACKED_FLOAT(serialiser, m_pitchControl, 1.0f, SIZEOF_ANGCONTROL, "Pitch Control");
	SERIALISE_PACKED_FLOAT(serialiser, m_rollControl,  1.0f, SIZEOF_ANGCONTROL, "Roll Control");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_throttleControl, 2.0f, SIZEOF_THROTTLE, "Throttle Control");

	SERIALISE_BOOL(serialiser, m_hasActiveAITask,		"Has Active Task");
	
	bool hasBrake = !IsClose(m_brake, 0.0f, SMALL_FLOAT);
	SERIALISE_BOOL(serialiser, hasBrake, "Has Break Control");
	if(hasBrake || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_brake, 1.0f, SIZEOF_ANGCONTROL, "Brake Control");
	}
	else
	{
		m_brake = 0.0f;
	}

	bool hasVerticalFlightMode = !IsClose(m_verticalFlightMode, 0.0f, SMALL_FLOAT);
	SERIALISE_BOOL(serialiser, hasVerticalFlightMode,	"Has Vertical Flight Mode");
	if(hasVerticalFlightMode || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_verticalFlightMode, 1.0f, 8, "Vertical Flight Mode");
	}
	else
	{
		m_verticalFlightMode = 0.0f;
	}
}

bool CPlaneControlDataNode::HasDefaultState() const 
{ 
	return (m_yawControl == 0.0f && m_pitchControl == 0.0f && m_rollControl == 0.0f && m_throttleControl == 0.0f && IsClose(m_brake, 0.0f, SMALL_FLOAT)); 
}

void CPlaneControlDataNode::SetDefaultState() 
{
	m_yawControl = m_pitchControl = m_rollControl = m_throttleControl = m_brake = 0.0f; 
}

