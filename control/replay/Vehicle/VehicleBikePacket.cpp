//
// name:		VehicleBikePacket.cpp
//


#include "VehicleBikePacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayExtensions.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "vehicles/Bike.h"
#include "vehicles/BMX.h"
#include "vehicles/Automobile.h"


tPacketVersion CPacketBikeUpdate_V1 = 1;
tPacketVersion CPacketBikeUpdate_V2 = 2;

template<typename PACKETTYPE, typename VEHTYPE>
void ProcessChains(const PACKETTYPE* pCurrentPacket, const CInterpEventInfo<PACKETTYPE, VEHTYPE>& info)
{
	VEHTYPE* pVehicle = info.GetEntity();

	CCustomShaderEffectVehicle* pShaderFx =	static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());

	if(pShaderFx && !(pVehicle->pHandling->mFlags & MF_HAS_TRACKS) && (pVehicle->InheritsFromBike() || pVehicle->InheritsFromBicycle()) && !fwTimer::IsGamePaused())
	{
		for(s32 i = 0; i < pVehicle->GetNumWheels(); i++)
		{			
			CWheel *pWheel = pVehicle->GetWheel(i);			
			if(pWheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				if(info.GetNextPacket())
				{
					bool animFwd = true;
					if(pCurrentPacket->GetPacketVersion() >= CPacketBikeUpdate_V2)
						animFwd = pCurrentPacket->GetAnimFwd();

					float valueX = ProcessChainValue(pCurrentPacket->GetUVAnimValueX(), info.GetNextPacket()->GetUVAnimValueX(), info.GetInterp(), animFwd);
					float valueY = ProcessChainValue(pCurrentPacket->GetUVAnimValueY(), info.GetNextPacket()->GetUVAnimValueY(), info.GetInterp(), animFwd);
					pShaderFx->TrackUVAnimSet(Vector2(valueX, valueY));
				}
			}			
		}	
	}
}


void GetChainValue(CVehicle* pVehicle, float& uvAnimValueX, float& uvAnimValueY, bool& animFwd)
{
	uvAnimValueX = uvAnimValueY = 0.0f;
	animFwd = true;

	CCustomShaderEffectVehicle* pShaderFx =	static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	if(pShaderFx && !(pVehicle->pHandling->mFlags & MF_HAS_TRACKS))
	{
		for(s32 i = 0; i < pVehicle->GetNumWheels(); i++)
		{			
			CWheel *pWheel = pVehicle->GetWheel(i);			
			if(pWheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				uvAnimValueX = pShaderFx->TrackUVAnimGet().x;
				uvAnimValueY = pShaderFx->TrackUVAnimGet().y;

				animFwd = pWheel->GetRotSpeed() >= 0.0f;
				break;
			}			
		}	
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketBikeUpdate::Store(CBike *pBike, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketBikeUpdate);
	const float SCALAR = 50.0f;

	replayAssert(pBike->GetVehicleType() == VEHICLE_TYPE_BIKE);
	CPacketVehicleUpdate::Store(PACKETID_BIKEUPDATE, sizeof(CPacketBikeUpdate), pBike, storeCreateInfo, CPacketBikeUpdate_V2);

	m_LeanAngle = (s8)(pBike->GetLeanAngle() * SCALAR);

	GetChainValue(pBike, m_uvAnimValueX, m_uvAnimValueY, m_chainAnimFwd);
}


void CPacketBikeUpdate::StoreExtensions(CBike *pBike, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketBikeUpdate); 
	replayAssert(pBike->GetVehicleType() == VEHICLE_TYPE_BIKE);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_BIKEUPDATE, sizeof(CPacketBikeUpdate), pBike, storeCreateInfo);
}


//////////////////////////////////////////////////////////////////////////
void CPacketBikeUpdate::Extract(const CInterpEventInfo<CPacketBikeUpdate, CBike>& info) const
{
	const float SCALAR = (1.0f / 50.0f);
	CBike* pBike = info.GetEntity();

	replayAssert(pBike->GetVehicleType() == VEHICLE_TYPE_BIKE);
	CPacketVehicleUpdate::Extract(info);

	pBike->m_fBikeLeanAngle =  m_LeanAngle * SCALAR;

	if(GetPacketVersion() >= CPacketBikeUpdate_V1)
	{
		ProcessChains<CPacketBikeUpdate, CBike>(this, info);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketBicycleUpdate::Store(CBmx *pBicycle, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketBicycleUpdate);
	const float SCALAR = 50.0f;

	replayAssert(pBicycle->GetVehicleType() == VEHICLE_TYPE_BICYCLE);
	CPacketVehicleUpdate::Store(PACKETID_BICYCLEUPDATE, sizeof(CPacketBicycleUpdate), pBicycle, storeCreateInfo);

	m_LeanAngle = (s8)(pBicycle->GetLeanAngle() * SCALAR);
	m_IsPedalling = false;
	m_PedallingRate = 0.0f;

	if(ReplayBicycleExtension::HasExtension(pBicycle))
	{
		m_IsPedalling = ReplayBicycleExtension::GetIsPedalling(pBicycle);
		m_PedallingRate = ReplayBicycleExtension::GetPedallingRate(pBicycle);
	}
}


void CPacketBicycleUpdate::StoreExtensions(CBmx *pBicycle, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketBicycleUpdate);
	replayAssert(pBicycle->GetVehicleType() == VEHICLE_TYPE_BICYCLE);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_BICYCLEUPDATE, sizeof(CPacketBicycleUpdate), pBicycle, storeCreateInfo);
}


//////////////////////////////////////////////////////////////////////////
void CPacketBicycleUpdate::Extract(const CInterpEventInfo<CPacketBicycleUpdate, CBmx>& info) const
{
	const float SCALAR = (1.0f / 50.0f);
	CBmx* pBicycle = info.GetEntity();

	replayAssert(pBicycle->GetVehicleType() == VEHICLE_TYPE_BICYCLE);
	CPacketVehicleUpdate::Extract(info);

	pBicycle->m_fBikeLeanAngle =  m_LeanAngle * SCALAR;

	if(!ReplayBicycleExtension::HasExtension(pBicycle))
	{
		ReplayBicycleExtension::Add(pBicycle);
	}

	if(ReplayBicycleExtension::HasExtension(pBicycle))
	{
		ReplayBicycleExtension::SetIsPedalling(pBicycle, m_IsPedalling);
		ReplayBicycleExtension::SetPedallingRate(pBicycle, m_PedallingRate);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketQuadBikeUpdate::Store(CQuadBike *pQuadbike, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketQuadBikeUpdate);
	replayAssert(pQuadbike->GetVehicleType() == VEHICLE_TYPE_QUADBIKE);
	CPacketVehicleUpdate::Store(PACKETID_QUADBIKEUPDATE, sizeof(CPacketQuadBikeUpdate), pQuadbike ,storeCreateInfo);
}


void CPacketQuadBikeUpdate::StoreExtensions(CQuadBike *pQuadbike, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketQuadBikeUpdate);
	replayAssert(pQuadbike->GetVehicleType() == VEHICLE_TYPE_QUADBIKE);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_QUADBIKEUPDATE, sizeof(CPacketQuadBikeUpdate), pQuadbike ,storeCreateInfo);
}

//////////////////////////////////////////////////////////////////////////
void CPacketQuadBikeUpdate::Extract(const CInterpEventInfo<CPacketQuadBikeUpdate, CQuadBike>& info) const
{
	replayAssert(info.GetEntity()->GetVehicleType() == VEHICLE_TYPE_QUADBIKE);
	CPacketVehicleUpdate::Extract(info);
}

#endif // GTA_REPLAY