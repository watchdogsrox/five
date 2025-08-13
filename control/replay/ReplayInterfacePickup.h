#ifndef REPLAYINTERFACEPICKUP_H
#define REPLAYINTERFACEPICKUP_H

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayInterface.h"
#include "Entity/PickupPacket.h"
#include "pickups/Pickup.h"

#define PICKUPPACKETDESCRIPTOR(id, et, type, recFunc) CPacketDescriptor<CPickup>(id, et, sizeof(type), #type, \
	&iReplayInterface::template PrintOutPacketDetails<type>,\
	recFunc,\
	NULL)

template<>
class CReplayInterfaceTraits<CPickup>
{
public:
	typedef CPacketPickupCreate	tCreatePacket;
	typedef CPacketPickupUpdate	tUpdatePacket;
	typedef CPacketPickupDelete	tDeletePacket;
	typedef CPickupInterp				tInterper;
	typedef CPickup::Pool				tPool;

	typedef deletionData<MAX_PICKUP_DELETION_SIZE>	tDeletionData;

	static const char*	strShort;
	static const char*	strLong;
	static const char*	strShortFriendly;
	static const char*	strLongFriendly;

	static const int	maxDeletionSize;
};


class CReplayInterfacePickup
	: public CReplayInterface<CPickup>
{
public:
	CReplayInterfacePickup();
	~CReplayInterfacePickup(){}

	using CReplayInterface<CPickup>::AddPacketDescriptor;

	template<typename PACKETTYPE, typename VEHICLESUBTYPE>
	void		AddPacketDescriptor(eReplayPacketId packetID, const char* packetName, s32 vehicleType);

	void		ClearWorldOfEntities();

	bool		IsRelevantUpdatePacket(eReplayPacketId packetID) const
	{
		return (packetID > PACKETID_PICKUP_CREATE && packetID < PACKETID_PICKUP_DELETE);
	}

	bool		IsEntityUpdatePacket(eReplayPacketId packetID) const	{	return packetID == PACKETID_PICKUP_UPDATE;	}

	CPickup*	CreateElement(CReplayInterfaceTraits<CPickup>::tCreatePacket const* pPacket, const CReplayState& state);
	bool		RemoveElement(CPickup* pElem, const CPacketPickupDelete* pDeletePacket, bool isRewinding = true);

	void		ResetAllEntities();
	void		ResetEntity(CPickup* pObject);

	void		PlaybackSetup(ReplayController& controller);
	void		PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist);
	void		PlayDeletePacket(ReplayController& controller);

	void		OnDelete(CPhysical* pEntity);

	bool		MatchesType(const CEntity* pEntity) const;

	void		PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);

	bool		ShouldRecordElement(const CPickup* pPickup) const;

	bool		CheckAllowedModelNameHash(u32 hash) const;

private:
	bool		LoadArchtype(strLocalIndex index, bool urgent);

	u32			GetUpdatePacketSize(CPickup* pObj) const;

	u32			GetFragDataSize(const CEntity* pEntity) const;
	u32			GetFragBoneDataSize(const CEntity* pEntity) const;

	void		RecordFragData(CEntity* pEntity, ReplayController& controller);
	void		RecordFragBoneData(CEntity* pEntity, ReplayController& controller);

	void		ExtractObjectType(CPickup* pObject, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, float interpolationValue);

};

#define INTERPER_PICKUP InterpWrapper<CReplayInterfaceTraits<CPickup>::tInterper>


#endif // GTA_REPLAY

#endif // REPLAYINTERFACEPICKUP_H
