//
// name:		VehicleTrainPacket.h
// description:	Derived from CVehiclePacket, this class handles all train-related replay packets.
//				See VehiclePacket.h for a full description of all the methods.
// written by:	Al-Karim Hemraj (R* Toronto)
//

#ifndef VEHTRAINPACKET_H
#define VEHTRAINPACKET_H

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Vehicle/VehiclePacket.h"
#include "Vehicles/train.h"

//=====================================================================================================
class CPacketTrainUpdate : public CPacketVehicleUpdate
{
public:
	void		Store(CTrain *pTrain, bool storeCreateInfo);
	void		StoreExtensions(CTrain *pTrain, bool storeCreateInfo);
	void		Extract(const CInterpEventInfo<CPacketTrainUpdate, CTrain>& info) const;

	bool		ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRAINUPDATE, "Validation of CPacketTrainUpdate Failed!, 0x%08X", GetPacketID());
		replayAssertf(VALIDATE_PACKET_EXTENSION(CPacketTrainUpdate), "Validation of CPacketTrainUpdate extensions Failed!, 0x%08X", GetPacketID());
		return CPacketVehicleUpdate::ValidatePacket() && GetPacketID() == PACKETID_TRAINUPDATE;
	}

	void		PrintXML(FileHandle handle) const
	{
		CPacketVehicleUpdate::PrintXML(handle);

		char str[1024];
		snprintf(str, 1024, "\t\t<m_LinkedToForward>0x%08X</m_LinkedToForward>\n", m_LinkedToForward.ToInt());
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<m_LinkedToBackward>0x%08X</m_LinkedToBackward>\n", m_LinkedToBackward.ToInt());
		CFileMgr::Write(handle, str, istrlen(str));
	}


	const CReplayID&	GetLinkedToForward() const	{	return m_LinkedToForward;	}
	const CReplayID&	GetLinkedToBackward() const	{	return m_LinkedToBackward;	}
private:
	//-------------------------------------------------------------------------------------------------
	float	m_TrackForwardSpeed;
	float	m_TrackDistFromEngineFrontForCarriageFront;
	s8		m_TrackIndex;
	s8		m_TrainConfigIndex;
	s8		m_TrainConfigCarriageIndex;
	CReplayID		m_LinkedToForward;
	CReplayID		m_LinkedToBackward;
	CTrain::CTrainFlags m_TrainfFlags;
	DECLARE_PACKET_EXTENSION(CPacketTrainUpdate);
};

#endif // GTA_REPLAY

#endif  // VEHTRAINPACKET_H
