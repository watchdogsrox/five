#ifndef REPLAYINTERFACEPED_H
#define REPLAYINTERFACEPED_H

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayInterface.h"

#include "Peds/ped.h"	
#include "peds/PopCycle.h"
#include "peds/PedFactory.h"
#include "peds/pedpopulation.h"
#include "Ped/PedPacket.h"

#include "atl/map.h"

class CReplayInterfaceVeh;

template<>
class CReplayInterfaceTraits<CPed>
{
public:
	typedef CPacketPedCreate	tCreatePacket;
	typedef CPacketPedUpdate	tUpdatePacket;
	typedef CPacketPedDelete	tDeletePacket;
	typedef CPedInterp			tInterper;
	typedef CPed::Pool			tPool;

	typedef deletionData<MAX_PED_DELETION_SIZE>	tDeletionData;

	static const char*	strShort;
	static const char*	strLong;
	static const char*	strShortFriendly;
	static const char*	strLongFriendly;

	static const int	maxDeletionSize;
};

const static int MAX_PREV_PLAYERS = 16;

class CReplayInterfacePed
	: public CReplayInterface<CPed>
{
public:
	CReplayInterfacePed(CReplayInterfaceVeh& vehInterface);
	~CReplayInterfacePed(){}

	void		PreClearWorld();
	void		ClearWorldOfEntities();

	void		ResetEntity(CPed* pPed);

	bool		ShouldRecordElement(const CPed* pPed) const;

	bool		MatchesType(const CEntity* pEntity) const	{	return pEntity->GetType() == ENTITY_TYPE_PED; }
	virtual u8	GetPreferredRecordingThreadIndex()			{ return 1; }

	void		OnDelete(CPhysical* pEntity);
	void		PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);

	bool		TryPreloadPacket(const CPacketBase* pPacket, bool& preloadSuccess, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason));

	bool		WaitingOnLoading();

#if REPLAY_DELAYS_QUANTIZATION
	void		PostRecordOptimize(CPacketBase* pPacket) const;
	void		ResetPostRecordOptimizations() const;
	bool		IsRelevantOptimizablePacket(eReplayPacketId packetID) const	{	return packetID == PACKETID_PEDUPDATE;	}
#endif
private:

	void		PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist);

	CPed*		CreateElement(CReplayInterfaceTraits<CPed>::tCreatePacket const* pPacket, const CReplayState& state);
	bool		RemoveElement(CPed* pElem, const CPacketPedDelete* pDeletePacket, bool isRewinding = true);

	CReplayInterfaceVeh&	m_vehInterface;
	bool					m_suppressOldPlayerDeletion;

	atFixedArray<CReplayID, MAX_PREV_PLAYERS>	m_playerList;
};




#endif // GTA_REPLAY

#endif // REPLAYINTERFACEPED_H
