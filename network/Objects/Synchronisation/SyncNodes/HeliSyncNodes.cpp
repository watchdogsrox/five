//
// name:        HeliSyncNodes.cpp
// description: Network sync nodes used by CNetObjHelis
// written by:    John Gurney
//

#include "network/objects/synchronisation/syncnodes/HeliSyncNodes.h"
#include "vehicles/LandingGear.h"

NETWORK_OPTIMISATIONS()

DATA_ACCESSOR_ID_IMPL(IHeliNodeDataAccessor);

void CHeliHealthDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_HELI_HEALTH  = 17;

	SERIALISE_UNSIGNED(serialiser, m_mainRotorHealth, SIZEOF_HELI_HEALTH, "Main Rotor Health");
	SERIALISE_UNSIGNED(serialiser, m_rearRotorHealth, SIZEOF_HELI_HEALTH, "Rear Rotor Health");
	SERIALISE_BOOL(serialiser, m_boomBroken, "Boom Broken");
	SERIALISE_BOOL(serialiser, m_canBoomBreak, "Can Boom Break");
	
	SERIALISE_BOOL(serialiser, m_hasCustomHealth, "Heli Has Custom Health");

	if(m_hasCustomHealth || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_bodyHealth, SIZEOF_HELI_HEALTH, "Heli Body Health");
		SERIALISE_UNSIGNED(serialiser, m_gasTankHealth, SIZEOF_HELI_HEALTH, "Heli Gas Tank Health");
		SERIALISE_UNSIGNED(serialiser, m_engineHealth, SIZEOF_HELI_HEALTH, "Heli Engine Health");
	}

	static const unsigned int SIZE_OF_SCALE = 11;

	SERIALISE_PACKED_FLOAT(serialiser, m_mainRotorDamageScale, 100.0f, SIZE_OF_SCALE, "Main Rotor Damage Scale");
	SERIALISE_PACKED_FLOAT(serialiser, m_rearRotorDamageScale, 100.0f, SIZE_OF_SCALE, "Rear Rotor Damage Scale");
	SERIALISE_PACKED_FLOAT(serialiser, m_tailBoomDamageScale, 100.0f, SIZE_OF_SCALE, "Tail Boom Damage Scale");

	SERIALISE_BOOL(serialiser, m_disableExpFromBodyDamage, "Disable Explosion from Body Damage");
}

bool CHeliHealthDataNode::HasDefaultState() const
{
	return (m_mainRotorHealth == (u32)VEH_DAMAGE_HEALTH_STD && m_rearRotorHealth == (u32)VEH_DAMAGE_HEALTH_STD && !m_boomBroken && m_canBoomBreak && m_mainRotorDamageScale == 1.0f && m_rearRotorDamageScale == 1.0f && m_tailBoomDamageScale == 1.0f);
}

void CHeliHealthDataNode::SetDefaultState()
{
	m_mainRotorHealth = m_rearRotorHealth = (u32)VEH_DAMAGE_HEALTH_STD;
	m_canBoomBreak = true;
	m_boomBroken   = false;
	m_mainRotorDamageScale = 1.0f;
	m_rearRotorDamageScale = 1.0f;
	m_tailBoomDamageScale  = 1.0f;
}

void CHeliControlDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_LANDING_GEAR_PUBLIC_STATES = datBitsNeeded<CLandingGear::NUM_PUBLIC_STATES>::COUNT;
	static const unsigned int SIZEOF_ANGCONTROL   = 8;
	static const unsigned int SIZEOF_THROTTLE     = 8;
	static const unsigned int SIZEOF_JETPACK_STRAFE_FORCE_SCALE = 9;

	SERIALISE_PACKED_FLOAT(serialiser, m_yawControl,   1.0f, SIZEOF_ANGCONTROL, "Yaw Control");
	SERIALISE_PACKED_FLOAT(serialiser, m_pitchControl, 1.0f, SIZEOF_ANGCONTROL, "Pitch Control");
	SERIALISE_PACKED_FLOAT(serialiser, m_rollControl,  1.0f, SIZEOF_ANGCONTROL, "Roll Control");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_throttleControl, 2.0f, SIZEOF_THROTTLE, "Throttle Control");
	SERIALISE_BOOL(serialiser, m_mainRotorStopped, "Main Rotor Stopped");
	
	SERIALISE_BOOL(serialiser, m_bHasLandingGear, "Has Landing Gear");
	if (m_bHasLandingGear || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_landingGearState, SIZEOF_LANDING_GEAR_PUBLIC_STATES, "Landing Gear State");
	}

	SERIALISE_BOOL(serialiser, m_hasJetpackStrafeForceScale, "Has Jetpack Strafe Force Scale");
	if (m_hasJetpackStrafeForceScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_jetPackStrafeForceScale, 1.0f, SIZEOF_JETPACK_STRAFE_FORCE_SCALE, "Jetpack Strafe Force Scale");
		SERIALISE_PACKED_FLOAT(serialiser, m_jetPackThrusterThrottle, 1.0f, SIZEOF_JETPACK_STRAFE_FORCE_SCALE, "Jetpack Strafe Force Scale");
	}
	else
	{
		m_jetPackStrafeForceScale = 0.0f;
		m_jetPackThrusterThrottle = 0.0f;
	}

	SERIALISE_BOOL(serialiser, m_hasActiveAITask, "Has Active Task");
	SERIALISE_BOOL(serialiser, m_lockedToXY, "Locked to XY");
}

bool CHeliControlDataNode::HasDefaultState() const
{
	return (m_yawControl == 0.0f && m_pitchControl ==  0.0f && m_rollControl ==  0.0f && m_throttleControl == 0.0f && m_jetPackStrafeForceScale == 0.0f && m_jetPackThrusterThrottle == 0.0f && m_lockedToXY == false);
}

void CHeliControlDataNode::SetDefaultState()
{
	m_yawControl = m_pitchControl = m_rollControl = m_throttleControl = m_jetPackStrafeForceScale = m_jetPackThrusterThrottle = 0.0f;
	m_lockedToXY = false;
}

