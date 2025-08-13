#include "control/replay/Vehicle/VehicleSubPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "audio/boataudioentity.h"
#include "Vehicles/Submarine.h"


//////////////////////////////////////////////////////////////////////////
void CPacketSubUpdate::Store(CSubmarine* pSub, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketSubUpdate);
	CPacketVehicleUpdate::Store(PACKETID_SUBUPDATE, sizeof(CPacketSubUpdate), pSub, storeCreateInfo);

	for(int i = 0; i < pSub->GetSubHandling()->GetNumPropellors(); ++i)
	{
		m_subPropellorSpeeds[i] = pSub->GetSubHandling()->GetPropellorSpeed(i);
	}

	m_rudderAngle = pSub->GetSubHandling()->GetRudderAngle();
	m_elevatorAngle = pSub->GetSubHandling()->GetElevatorAngle();

	//Audio data
	m_ReplayVehiclePhysicsInWater = false;
	m_SubAngularVelocityMag = 0.0f;
	audVehicleAudioEntity* pAudioEntity= pSub->GetVehicleAudioEntity();
	if(pAudioEntity)
	{
		m_SubAngularVelocityMag = pAudioEntity->GetSubAngularVelocityMag();
		m_ReplayVehiclePhysicsInWater = pSub->GetIsInWater();
	}
}


void CPacketSubUpdate::StoreExtensions(CSubmarine* pSub, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketSubUpdate); 
	CPacketVehicleUpdate::StoreExtensions(PACKETID_SUBUPDATE, sizeof(CPacketSubUpdate), pSub, storeCreateInfo);
}


//////////////////////////////////////////////////////////////////////////
void CPacketSubUpdate::Extract(const CInterpEventInfo<CPacketSubUpdate, CSubmarine>& info) const
{
	CSubmarine* pSub = info.GetEntity();
	replayAssert(pSub->GetVehicleType() == VEHICLE_TYPE_SUBMARINE);

	// Set propellor, rudder and elevator details here as the prerender is called in the vehicle extract function
	for(int i = 0; i < pSub->GetSubHandling()->GetNumPropellors(); ++i)
	{
		pSub->GetSubHandling()->SetPropellorSpeed(i, m_subPropellorSpeeds[i]);
	}

	pSub->GetSubHandling()->SetRudderAngle(m_rudderAngle);
	pSub->GetSubHandling()->SetElevatorAngle(m_elevatorAngle);


	//Audio data
	audVehicleAudioEntity* pAudioEntity= pSub->GetVehicleAudioEntity();
	if(pAudioEntity)
	{
		pAudioEntity->SetVehiclePhysicsInWater(m_ReplayVehiclePhysicsInWater);
		pAudioEntity->SetSubAngularVelocityMag(m_SubAngularVelocityMag);
	}

	CPacketVehicleUpdate::Extract(info);

	pSub->GetSubHandling()->ProcessCrushDepth(pSub);
}



//////////////////////////////////////////////////////////////////////////
void CPacketSubCarUpdate::Store(CSubmarineCar* pSub, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketSubCarUpdate);
	CPacketVehicleUpdate::Store(PACKETID_SUBCARUPDATE, sizeof(CPacketSubCarUpdate), pSub, storeCreateInfo);

	for(int i = 0; i < pSub->GetSubHandling()->GetNumPropellors(); ++i)
	{
		m_subPropellorSpeeds[i] = pSub->GetSubHandling()->GetPropellorSpeed(i);
	}

	m_rudderAngle = pSub->GetSubHandling()->GetRudderAngle();
	m_elevatorAngle = pSub->GetSubHandling()->GetElevatorAngle();

	//Audio data
	m_ReplayVehiclePhysicsInWater = false;
	m_SubAngularVelocityMag = 0.0f;
	audVehicleAudioEntity* pAudioEntity= pSub->GetVehicleAudioEntity();
	if(pAudioEntity)
	{
		m_SubAngularVelocityMag = pAudioEntity->GetSubAngularVelocityMag();
		m_ReplayVehiclePhysicsInWater = pSub->GetIsInWater();
	}
}


void CPacketSubCarUpdate::StoreExtensions(CSubmarineCar* pSub, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketSubCarUpdate); 
	CPacketVehicleUpdate::StoreExtensions(PACKETID_SUBCARUPDATE, sizeof(CPacketSubCarUpdate), pSub, storeCreateInfo);
}


//////////////////////////////////////////////////////////////////////////
void CPacketSubCarUpdate::Extract(const CInterpEventInfo<CPacketSubCarUpdate, CSubmarineCar>& info) const
{
	CSubmarineCar* pSub = info.GetEntity();
	replayAssert(pSub->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR);

	// Set propellor, rudder and elevator details here as the prerender is called in the vehicle extract function
	for(int i = 0; i < pSub->GetSubHandling()->GetNumPropellors(); ++i)
	{
		pSub->GetSubHandling()->SetPropellorSpeed(i, m_subPropellorSpeeds[i]);
	}

	pSub->GetSubHandling()->SetRudderAngle(m_rudderAngle);
	pSub->GetSubHandling()->SetElevatorAngle(m_elevatorAngle);


	//Audio data
	audVehicleAudioEntity* pAudioEntity= pSub->GetVehicleAudioEntity();
	if(pAudioEntity)
	{
		pAudioEntity->SetVehiclePhysicsInWater(m_ReplayVehiclePhysicsInWater);
		pAudioEntity->SetSubAngularVelocityMag(m_SubAngularVelocityMag);
	}

	CPacketVehicleUpdate::Extract(info);

	pSub->GetSubHandling()->ProcessCrushDepth(pSub);
}



#endif // GTA_REPLAY
