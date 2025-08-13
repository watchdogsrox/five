#ifndef REPLAYINTERFACEVEH_H
#define REPLAYINTERFACEVEH_H

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayInterface.h"

#include "misc/ReplayPacket.h"
#include "Vehicle/VehiclePacket.h"
#include "vehicle/VehicleAutomobilePacket.h"
#include "Vehicle/VehicleBikePacket.h"
#include "Vehicle/VehicleBoatPacket.h"
#include "vehicle/VehicleHeliPacket.h"
#include "vehicle/VehiclePlanePacket.h"
#include "vehicle/VehicleSubPacket.h"
#include "vehicle/VehicleTrailerPacket.h"
#include "vehicle/VehicleTrainPacket.h"
#include "vehicle/VehicleAmphAutoPacket.h"
#include "vehicle/VehicleAmphQuadPacket.h"
#include "Vehicles/vehiclestorage.h"
#include "entity/FragmentPacket.h"


#define VEHPACKETDESCRIPTOR(id, et, type, recFunc, extFunc) CPacketDescriptor<CVehicle>(id, et, sizeof(type), #type, \
	&iReplayInterface::template PrintOutPacketDetails<type>,\
	recFunc,\
	extFunc)

template<>
class CReplayInterfaceTraits<CVehicle>
{
public:
	typedef CPacketVehicleCreate	tCreatePacket;
	typedef CPacketVehicleUpdate	tUpdatePacket;
	typedef CPacketVehicleDelete	tDeletePacket;
	typedef CVehInterp			tInterper;
	typedef CVehicle::Pool		tPool;

	typedef deletionData<MAX_VEHICLE_DELETION_SIZE>	tDeletionData;

	static const char*	strShort;
	static const char*	strLong;
	static const char*	strShortFriendly;
	static const char*	strLongFriendly;

	static const int	maxDeletionSize;
};


class CReplayInterfaceVeh
	: public CReplayInterface<CVehicle>
{
public:
	CReplayInterfaceVeh();
	~CReplayInterfaceVeh(){}

	using CReplayInterface<CVehicle>::AddPacketDescriptor;

	template<typename PACKETTYPE, typename VEHICLESUBTYPE>
	void		AddPacketDescriptor(eReplayPacketId packetID, const char* packetName, s32 vehicleType);

	void		ClearWorldOfEntities();

	void		DisplayEstimatedMemoryUsageBreakdown();

	bool		IsRelevantUpdatePacket(eReplayPacketId packetID) const
	{
		return (packetID > PACKETID_VEHICLEDELETE && packetID < PACKETID_WHEELUPDATE_OLD) || packetID == PACKETID_AMPHAUTOUPDATE || packetID == PACKETID_AMPHQUADUPDATE || packetID == PACKETID_SUBCARUPDATE;
	}

	void		Process();

	bool		IsEntityUpdatePacket(eReplayPacketId packetID) const	{	return (packetID >= PACKETID_CARUPDATE && packetID <= PACKETID_SUBUPDATE) || packetID == PACKETID_AMPHAUTOUPDATE || packetID == PACKETID_AMPHQUADUPDATE || packetID == PACKETID_SUBCARUPDATE;	}

	void		PreRecordFrame(CReplayRecorder&, CReplayRecorder&);

	bool		ShouldRecordElement(const CVehicle* pVehicle) const;
	bool		RecordElement(CVehicle* pVehicle, CReplayRecorder& recorder, CReplayRecorder& createPacketRecorder, CReplayRecorder& fullCreatePacketRecorder, u16 sessionBlockIndex);

#if REPLAY_DELAYS_QUANTIZATION
	void		PostRecordOptimize(CPacketBase* pPacket) const;
	void		ResetPostRecordOptimizations() const;
#endif // REPLAY_DELAYS_QUANTIZATION

#if REPLAY_VEHICLES_USE_SKELETON_BONE_DATA_PACKET
	bool		IsRelevantOptimizablePacket(eReplayPacketId packetID) const	{	return packetID == CPacketVehicleUpdate::fragBoneDataPacketId || packetID == CPacketVehicleUpdate::fragBoneDataPacketIdHQ;	}
#endif

	ePlayPktResult PlayPackets(ReplayController& controller, bool entityMayNotExist);
	void		JumpPackets(ReplayController& controller);
	
	void		PlaybackSetup(ReplayController& controller);
	void		ApplyFadesSecondPass();
	CVehicle*	CreateElement(CReplayInterfaceTraits<CVehicle>::tCreatePacket const* pPacket, const CReplayState& state);

	void		ResetEntity(CVehicle* pVehicle);
	void		StopEntitySounds(CVehicle* pVehicle);

	bool		MatchesType(const CEntity* pEntity) const	{	return pEntity->GetType() == ENTITY_TYPE_VEHICLE; }
	virtual u8	GetPreferredRecordingThreadIndex()			{ return 2; }

	void		PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);

	void		SetVehicleVariationData(CVehicle* pVehicle);

	bool		WaitingOnLoading();
	bool		IsWaitingOnDamageToBeApplied();

	bool		GetBlockDetails(char* pString, bool& err, bool recording) const;

	static s32	NumDoorsToRecord(const CVehicle* pVehicle);
	static bool ShouldRecordDoor(const CVehicle* pVehicle, s32 doorIdx);

#if __BANK
	virtual	bkGroup* AddDebugWidgets();
	bool	IsRecordingOldStyleWheels() { return m_recordOldStyleWheels; }
#endif // __BANK

	bool	GetPacketGroup(eReplayPacketId packetID, u32& packetGroup) const
	{
		if((packetID >= m_relevantPacketMin && packetID <= m_relevantPacketMax) || packetID == PACKETID_AMPHAUTOUPDATE || packetID == PACKETID_AMPHQUADUPDATE || packetID == PACKETID_SUBCARUPDATE)
		{
			packetGroup = m_packetGroup;
			return true;
		}

		return false;
	}

protected:
	u32			GetUpdatePacketSize(CVehicle* pVehicle) const;
	void		RequestDial(CVehicle* pVehicle);


private:

	void		HandleCreatePacket(const tCreatePacket* pCreatePacket, bool notDelete, bool firstFrame, const CReplayState& state);
	CVehicle*	CreateVehicle(s32 modelIndex, u8 vehicleType, s32 replayID);

	void		PreUpdatePacket(ReplayController& controller);
	void		PreUpdatePacket(const CPacketVehicleUpdate* pPacket, const CPacketVehicleUpdate* pNextPacket, float interpValue, const CReplayState& flags, bool canDefer = true);
	void		PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist);
	void		JumpUpdatePacket(ReplayController& controller);
	
	s32 		GetFragDataSize(const CEntity* pEntity) const;
	s32 		GetFragBoneDataSize(const CEntity* pEntity) const;
	
	bool		RemoveElement(CVehicle* pVehicle, const CPacketVehicleDelete* pDeletePacket, bool isRewinding = true);
	int			RecordWheels(CVehicle* pVehicle, CReplayRecorder& recorder, u16 sessionBlockIndex);
	int			RecordDoors(CVehicle* pVehicle, CReplayRecorder& recorder);

	int 		RecordFragData(CEntity* pEntity, CReplayRecorder& recorder);
	int 		RecordFragBoneData(CEntity* pEntity, CReplayRecorder& recorder);

	void		ExtractVehicleType(CVehicle* pVehicle, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, const ReplayController& controller);

	void		ApplyUpdateFadeSpecial(CEntityData& entityData, const CPacketVehicleUpdate& packet);

	bool		WaitingForHDVehicles();

	bool		m_recordDoors;
	bool		m_recordWheels;
	bool		m_recordFrags;
	bool		m_recordFragBones;
	bool		m_recordOnlyPlayerCars;
	bool		m_recordOldStyleWheels;

	atArray<CVehicle*>	m_HDVehicles;
	CVehicle*			m_LastDialVehicle;

	struct trainInfo
	{
		CTrain*		pTrain;
		CReplayID	linkageBack;
		CReplayID	linkageFore;
	};
	atMap<CReplayID, trainInfo>	m_trainLinkInfos;
	atArray<CReplayID>	m_trainEngines;

	// Estimated memory usage breakdown.
	mutable u32			m_basePackets; // For Car, Boat update packets.
	mutable u32			m_doorPackets;
	mutable u32			m_noOfWheels;
	mutable u32			m_wheelPackets;
	mutable u32			m_fragPackets;
};

#define INTERPER_VEH InterpWrapper<CReplayInterfaceTraits<CVehicle>::tInterper>

#endif // GTA_REPLAY

#endif // REPLAYINTERFACEVEH_H
