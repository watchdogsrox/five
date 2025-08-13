//
// name:		VehicleTrainPacket.h
//

#include "control/replay/Vehicle/VehicleTrainPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/ReplayInternal.h"
#include "vehicles/train.h"


//========================================================================================================================
void CPacketTrainUpdate::Store(CTrain *pTrain, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketTrainUpdate);
	replayAssert(pTrain->GetVehicleType() == VEHICLE_TYPE_TRAIN);
	
	CPacketVehicleUpdate::Store(PACKETID_TRAINUPDATE, sizeof(CPacketTrainUpdate), pTrain, storeCreateInfo);

	m_TrackForwardSpeed = pTrain->m_trackForwardSpeed;
	m_LinkedToForward = NoEntityID;
	m_LinkedToBackward = NoEntityID;
	m_TrainfFlags = pTrain->m_nTrainFlags;
	m_TrackIndex = pTrain->m_trackIndex;
	m_TrainConfigIndex = pTrain->m_trainConfigIndex;
	m_TrainConfigCarriageIndex = pTrain->m_trainConfigCarriageIndex;
	m_TrackDistFromEngineFrontForCarriageFront = pTrain->m_trackDistFromEngineFrontForCarriageFront;

	if (pTrain->GetLinkedToForward())
	{
		m_LinkedToForward = pTrain->GetLinkedToForward()->GetReplayID();
	}
	if (pTrain->GetLinkedToBackward())
	{
		m_LinkedToBackward = pTrain->GetLinkedToBackward()->GetReplayID();
	}
}


void CPacketTrainUpdate::StoreExtensions(CTrain *pTrain, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketTrainUpdate);
	replayAssert(pTrain->GetVehicleType() == VEHICLE_TYPE_TRAIN);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_TRAINUPDATE, sizeof(CPacketTrainUpdate), pTrain, storeCreateInfo);
}


//========================================================================================================================
void CPacketTrainUpdate::Extract(const CInterpEventInfo<CPacketTrainUpdate, CTrain>& info) const
{
	CTrain* pTrain = info.GetEntity();
	replayAssert(pTrain->GetVehicleType() == VEHICLE_TYPE_TRAIN);
	CPacketVehicleUpdate::Extract(info);

	pTrain->m_trackForwardSpeed = m_TrackForwardSpeed;
	pTrain->m_nTrainFlags = m_TrainfFlags;
	pTrain->m_trackIndex = m_TrackIndex;
	pTrain->m_trainConfigIndex = m_TrainConfigIndex;
	pTrain->m_trainConfigCarriageIndex = m_TrainConfigCarriageIndex;
	pTrain->m_trackDistFromEngineFrontForCarriageFront = m_TrackDistFromEngineFrontForCarriageFront;

	Vector3 worldPosNew(VEC3V_TO_VECTOR3(pTrain->GetTransform().GetPosition()));
	pTrain->UpdateLODAndFadeState(worldPosNew);
}

#endif // GTA_REPLAY

