#ifndef REPLAYINTERFACEGAME_H
#define REPLAYINTERFACEGAME_H

#include "Control/Replay/ReplaySettings.h"

#if GTA_REPLAY

#include "Control/Replay/ReplayInterface.h"
#include "Control/Replay/ReplayEnums.h"
#include "control/Replay/ReplayInteriorManager.h"
#include "../Misc/WaterPacket.h"
#include "../Misc/ClothPacket.h"


//////////////////////////////////////////////////////////////////////////

#define NUM_LINKED_PACKET_TYPES 2

#define  WATER_LINKED_PACKET_INDEX 0
#define  CLOTH_LINKED_PACKET_INDEX 1


class CReplayInterfaceGame
	: public iReplayInterface
{
private:
	typedef struct PacketLinkingMaintainedInfo
	{
		CPacketLinkedBase*			m_PreviousCubicPatchWaterFrame;
		CPacketLinkedBase*			m_LastFoundCubicPatch;
		u16						m_PreviousCubicPatchSessionBlock;
		u32						m_PreviousCubicPatchPosition;
	} PacketLinkingMaintainedInfo;

public:
	CReplayInterfaceGame(float& timeScale, bool& overrideTime, bool& overrideWeather);
	~CReplayInterfaceGame();

	const char*		GetShortStr() const									{	return "Game";		}
	const char*		GetLongFriendlyStr() const							{	return "ReplayInterface-Game";		}

	void			Initialise()
	{
		ResetPacketLinking();
		ResetWaterTracking();

		m_PrevGameTime = 0;
		m_GameTime = 0;
		m_FrameStep = 0;

		m_CubicPatchWaterFrameT0 = NULL;
		m_CubicPatchWaterFrameTPlus1 = NULL;
		m_interpWaterFrame = 0.0f;
	}

	void			Reset()												
	{ 
		iReplayInterface::Reset();		
		Initialise();
	}

	void			Clear()
	{
		iReplayInterface::Clear();
		Initialise();
	}

	void			EnableRecording()
	{
		iReplayInterface::EnableRecording(); 
		Initialise();

		ReplayInteriorManager::OnRecordStart();
		m_InteriorManager.ResetRoomRequests();
	}

	void ResetEntityCreationFrames()
	{
		ResetPacketLinking();
	}

	void ResetWaterTracking()
	{
		m_PacketLinkingInfo[WATER_LINKED_PACKET_INDEX].m_PreviousCubicPatchWaterFrame = NULL;
		m_PacketLinkingInfo[WATER_LINKED_PACKET_INDEX].m_PreviousCubicPatchSessionBlock = 0;
		m_PacketLinkingInfo[WATER_LINKED_PACKET_INDEX].m_PreviousCubicPatchPosition = 0;

		m_PreviousSimPacket = NULL;
		m_recordWaterFrame = true;
		m_waterFrameTimer = 0;
	}

	void ResetPacketLinking()
	{
		for(u32 i=0; i<NUM_LINKED_PACKET_TYPES; i++)
		{
			m_PacketLinkingInfo[i].m_PreviousCubicPatchWaterFrame = NULL;
			m_PacketLinkingInfo[i].m_PreviousCubicPatchSessionBlock = 0;
			m_PacketLinkingInfo[i].m_PreviousCubicPatchPosition = 0;
		}
	}

	bool			MatchesType(const atHashWithStringNotFinal& type) const	{	return type == atHashWithStringNotFinal("Game",0XB427DD3B);}
	void			LinkCreatePacket(const CPacketBase*)				{}
	bool			IsRelevantUpdatePacket(eReplayPacketId) const		{return false;}
	void			DeleteEntitiesCreatedThisFrame(const FrameRef&, const u16, const u16)	{}
	void			DeleteEntitiesCreatedAfterFrame(const FrameRef&, const u16, const u16)	{}
	void			DeleteEntitiesCreatedBeforeFrame(const FrameRef&, const u16, const u16)	{}
	bool			IsCurrentlyBeingRecorded(const CReplayID&) const	{	return false;	}
	void			OffsetCreatePackets(s64)							{};
	void			OffsetUpdatePackets(s64 offset, u8* pStart, u32 blockSize);
	void			PlaybackSetup(ReplayController& controller);
	void			ClearPacketInformation();
	void			ResetCreatePacketsForCurrentBlock();	

	bool			FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& requestCount, const u32 systemTime, bool isSingleFrame);
	bool			TryPreloadPacket(const CPacketBase *pPacket, bool &preloadSuccess, u32 currTime PRELOADDEBUG_ONLY(, atString& failureReason));
	void			UpdatePreloadRequests(u32 time);

	bool			GetPacketName(eReplayPacketId packetID, const char*& packetName) const;
	bool			IsRelevantPacket(eReplayPacketId) const;

	void			RecordFrame(u16 sessionBlockIndex);
	void			PostRecordFrame(ReplayController& controller, u16 previousBlockFrameCount, bool blockChange);
	void			LinkUpdatePackets(ReplayController& controller, u16 previousBlockFrameCount, u32 endPosition);
	template < class PACKETTYPE >
	void			UpdatePacketLinkingMaintainedInfo(ReplayController& controller, CPacketBase *pBasePacket, PacketLinkingMaintainedInfo &linkingInfo);

	bkGroup* 		AddDebugWidgets();

	bool			MatchesType(const CEntity*) const						{	return false;	}

	void			RegisterForRecording(CPhysical*, FrameRef)				{}
	void			RegisterAllCurrentEntities()							{}
	bool			EnsureIsBeingRecorded(const CEntity*)					{	replayFatalAssertf(false, "");	return false; }
	bool			EnsureWillBeRecorded(const CEntity*)					{	replayFatalAssertf(false, "");	return false; }
	bool			SetCreationUrgent(const CReplayID&)						{	return false;	}

	void			ClearElementsFromWorld()	{}

	void			PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);
	void			FlushPreloadRequests();				

	ReplayInteriorManager *GetInteriorManager()								{ return &m_InteriorManager; }
	void ExtractWaterFrame(float *pDest, float *pSecondaryDest);

	ReplayClothPacketManager &GetClothPacketManager() { return m_ClothPacketManager; }

	void			SetReplayTimeInMillisecondsForNextFrameToRecord(u32 replayTime) { m_GameTime = replayTime; }

private:
	template<typename PACKETTYPE>
	void			AddPacketDescriptor(eReplayPacketId packetID, const char* packetName);

	bool			FindPacketDescriptor(eReplayPacketId packetID, const CPacketDescriptorBase*& pDescriptor) const;

	ePlayPktResult	PreprocessPackets(ReplayController& controller, bool entityMayNotExist);
	static CPacketCubicPatchWaterFrame const *SeekWaterFrameForwards(CPacketCubicPatchWaterFrame const *pStart, ReplayController& controller, u32 frameMod);
	static CPacketCubicPatchWaterFrame const *SeekWaterFrameBackwards(CPacketCubicPatchWaterFrame const *pStart, ReplayController& controller, u32 frameMod);

	ePlayPktResult	PlayPackets(ReplayController&, bool);

	void			JumpPackets(ReplayController& controller);

	static s32		GetLinkedPacketIndex(eReplayPacketId packetId);

	atRangeArray<CPacketDescriptorBase, maxPacketDescriptorsPerInterface>	m_packetDescriptors;
	u32								m_numPacketDescriptors;

	float&							m_timeScale;
	bool&							m_overrideTime;
	bool&							m_overrideWeather;

#if REPLAY_ENABLE_WATER
	u32								m_waterFrameTimer;
#endif
	u32								m_PrevGameTime;
	u32								m_GameTime;
	u32								m_FrameStep;

#if REPLAY_ENABLE_WATER
	bool							m_recordWaterFrame;
#endif


	PacketLinkingMaintainedInfo m_PacketLinkingInfo[NUM_LINKED_PACKET_TYPES];

	CPacketCubicPatchWaterFrame const*		m_CubicPatchWaterFrameT0;
	CPacketCubicPatchWaterFrame const*		m_CubicPatchWaterFrameTPlus1;
	float									m_interpWaterFrame;
	u16										m_waterGridPos[2];

	WaterSimInterp					m_interperWaterSim;
	CPacketWaterSimulate*			m_PreviousSimPacket;

	ReplayInteriorManager			m_InteriorManager;
	ReplayClothPacketManager		m_ClothPacketManager;
};


template<typename PACKETTYPE>
void CReplayInterfaceGame::AddPacketDescriptor(eReplayPacketId packetID, const char* packetName)
{
	replayFatalAssertf(m_numPacketDescriptors < maxPacketDescriptorsPerInterface, "Max number of packet descriptors breached");
	m_packetDescriptors[m_numPacketDescriptors++] = CPacketDescriptorBase(	packetID, 
											sizeof(PACKETTYPE), 
											packetName,
											&iReplayInterface::template PrintOutPacketDetails<PACKETTYPE>);
}


#endif // GTA_REPLAY

#endif // REPLAYINTERFACEGAME_H
