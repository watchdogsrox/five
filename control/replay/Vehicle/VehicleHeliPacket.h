//
// name:		VehicleHeliPacket.h
// description:	Derived from CVehicleAutoPacket, this class handles all helicopter-related replay packets.
//				See VehicleAutomobilePacket.h for a full description of all the methods.
// written by:	Al-Karim Hemraj (R* Toronto)
//

#ifndef VEHHELIPACKET_H
#define VEHHELIPACKET_H

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"

class CHeli;
class CAutogyro;
class CBlimp;

// Global Variables
extern float g_MinHealthForHeliWarning;

//=====================================================================================================
class CPacketHeliUpdate : public CPacketVehicleUpdate
{
public:

	void		Store(CHeli *pHeli, bool storeCreateInfo);
	void		StoreExtensions(CHeli *pHeli, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketHeliUpdate, CHeli>& info) const;

	float		GetAngularVelocity() const			{ return m_WheelAngularVelocity; }
	float		GetThrottleControl() const			{ return m_ThrottleControl; }
	float		GetHealth() const					{ return m_Health; }
	float		GetRotorAngle() const				{ return m_fMainRotorAngle; }

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_HELIUPDATE, "Validation of CPacketHeliUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketHeliUpdate), "Validation of CPacketHeliUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_HELIUPDATE;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	enum// eHeliHealth
	{
		HELI_HEALTH_INVALID	= 0,
		HELI_HEALTH_BROKEN	= 1,
		HELI_HEALTH_LOW		= 2,
		HELI_HEALTH_NORMAL	= 3
	};

	u8			GetHealthState( f32 health ) const;
	f32			GetHealthFromState( u8 healthState ) const;

	void		ExtractInterpolateableValues(CHeli* pHeli, CPacketHeliUpdate const* pNextPacket, float fInterp) const;

	//-------------------------------------------------------------------------------------------------
#if !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	float		m_WheelAngularVelocity;	// See CPacketVehicleUpdate.
#endif // !CPACKETVEHICLEUPDATE_READ_PS4_PACKETS_ON_X64
	float		m_ThrottleControl;	//m_fThrottleControl // TODO: Compress
	float		m_MainRotorSpeed;
	float		m_Health;			//m_fHealth m_Vehicle->GetHealth(); // TODO: Compress
	float		m_fMainRotorAngle;	// TODO: compress

	// Helicopter Health
	//  - The vehicle audio entity checks for 3 helicopter health states:
	//		1. Broken	- 0		>= Health
	//		2. Low		- 200	>= Health > 0
	//		3. Normal	- Max	>= Health > 200
	u8			m_MainRotorHealth		: 2; // 1st bit
	u8			m_RearRotorHealth		: 2;
	u8			m_TailBoomHealth		: 2;

	u8			m_HeliState				: 2; // 8th bit
	
	u8			m_IsAltitudeWarningOn	: 1; // 1st bit
	u8			m_LightOn				: 1;
	u8			m_LightForceOn			: 1;
	u8			m_LightHasPriority		: 1;
	u8			/* Padding */			: 4; // 8th bit

	u8			m_MinigunSpinSpeed;

	u8			m_HeliAudioState;

	CReplayID	m_lightTarget;

	float		m_fRopeLengthA;
	float		m_fRopeLengthB;
	float		m_fRopeLengthRateA;
	float		m_fRopeLengthRateB;
	DECLARE_PACKET_EXTENSION(CPacketHeliUpdate);

	float		m_jetpackThrusterThrottle;
	float		m_jetpackStrafeForceScale;
};

//stub packets, these need filling in at some point
//=====================================================================================================
class CPacketAutoGyroUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CAutogyro *pAutoGyro, bool storeCreateInfo);
	void		StoreExtensions(CAutogyro *pAutoGyro, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketAutoGyroUpdate, CAutogyro>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_AUTOGYROUPDATE, "Validation of CPacketAutoGyroUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketAutoGyroUpdate), "Validation of CPacketAutoGyroUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_AUTOGYROUPDATE;
	}
	DECLARE_PACKET_EXTENSION(CPacketAutoGyroUpdate);
};

//=====================================================================================================
class CPacketBlimpUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CBlimp *pBlimp, bool storeCreateInfo);
	void		StoreExtensions(CBlimp *pBlimp, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketBlimpUpdate, CBlimp>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_BLIMPUDPATE, "Validation of CPacketBlimpUpdate Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_BLIMPUDPATE;
	}
	DECLARE_PACKET_EXTENSION(CPacketBlimpUpdate);
};

#	endif // GTA_REPLAY

#endif  // VEHHELIPACKET_H
