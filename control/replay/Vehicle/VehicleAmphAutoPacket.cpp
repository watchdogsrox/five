#include "VehicleAmphAutoPacket.h"

#if GTA_REPLAY


#include "control/replay/Misc/ReplayPacket.h"
#include "game/modelindices.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "audio/caraudioentity.h"

//========================================================================================================================
tPacketVersion CPacketAmphAutoUpdate_V1 = 1;
void CPacketAmphAutoUpdate::Store(CAmphibiousAutomobile *pCar, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketAmphAutoUpdate);
	replayAssert(pCar->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE);

	m_VehiclePacketUpdateSize = sizeof(CPacketAmphAutoUpdate);

	CPacketVehicleUpdate::Store(PACKETID_AMPHAUTOUPDATE, sizeof(CPacketAmphAutoUpdate), pCar, storeCreateInfo, CPacketAmphAutoUpdate_V1);

	if(pCar && pCar->GetVehicleAudioEntity())
	{
		m_bOnRevLimiter	= pCar->GetVehicleAudioEntity()->GetReplayRevLimiter();
	}

	//Audio data
	m_isInWater = false;
	m_IsInAmphibiousBoatMode = false;
	audCarAudioEntity* pAudioEntity= ((audCarAudioEntity*)pCar->GetVehicleAudioEntity());
	if(pAudioEntity)
	{
		m_isInWater = pAudioEntity->CalculateIsInWater();
		m_IsInAmphibiousBoatMode = pAudioEntity->IsInAmphibiousBoatMode();
	}
}


void CPacketAmphAutoUpdate::StoreExtensions(CAmphibiousAutomobile *pCar, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketAmphAutoUpdate);
	replayAssert(pCar->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_AMPHAUTOUPDATE, sizeof(CPacketAmphAutoUpdate), pCar, storeCreateInfo);
}


void CPacketAmphAutoUpdate::Extract(const CInterpEventInfo<CPacketAmphAutoUpdate, CAmphibiousAutomobile>& info) const
{
	CAmphibiousAutomobile* pCar = info.GetEntity(); 
	replayAssert(pCar->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE);

	if(pCar->GetVehicleAudioEntity())
	{			
		pCar->GetVehicleAudioEntity()->SetReplayRevLimiter(m_bOnRevLimiter);
	}

	if(GetPacketVersion() >= CPacketAmphAutoUpdate_V1)
	{
		//Audio data
		audCarAudioEntity* pAudioEntity= ((audCarAudioEntity*)pCar->GetVehicleAudioEntity());
		if(pAudioEntity)
		{
			pAudioEntity->SetIsInWater(m_isInWater);
			pAudioEntity->SetInAmphibiousBoatMode(m_IsInAmphibiousBoatMode);
		}
	}

	// Extract base packet.
	CPacketVehicleUpdate::Extract(info);
}





#endif // GTA_REPLAY
