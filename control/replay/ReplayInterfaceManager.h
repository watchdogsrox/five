#ifndef REPLAYINTERFACEMANAGER_H
#define REPLAYINTERFACEMANAGER_H

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplaySupportClasses.h"
#include "ReplayPacketIDs.h"
#include "ReplayEnums.h"
#include "replay_channel.h"

#include "scene/EntityTypes.h"

#include "atl/array.h"
#include "atl/map.h"
#include "atl/hashstring.h"
#include "system/filemgr.h"

class iReplayInterface;
template<typename T>
class CReplayInterface;
class CPacketBase;
class CAddressInReplayBuffer;
class ReplayController;
class CEntity;

class CReplayInterfaceManager
{
public:
	CReplayInterfaceManager();
	~CReplayInterfaceManager();

	//////////////////////////////////////////////////////////////////////////
	// Interface Management Functions
	void					AddInterface(iReplayInterface* pInterface);
	iReplayInterface*		FindCorrectInterface(const CEntity* pEntity);

	bool					GetPacketGroup(eReplayPacketId packetID, u32& packetGroup);

	iReplayInterface*		FindCorrectInterface(const atHashWithStringNotFinal& type);
	iReplayInterface*		FindCorrectInterface(const eReplayPacketId packetType);
	iReplayInterface*		FindCorrectInterface(int)	{replayFatalAssertf(false, "Shouldn't call this function");	return NULL;}

	template<typename T>
	CReplayInterface<T>*	FindCorrectInterface();

	template<typename T>
	void					GetEntity(CEntityGet<T>& entityGet);
	CEntity*				FindEntity(CReplayID id);
	CEntity*				GetEntiyAsEntity(s32 id);
	bool					IsRecentlyDeleted(const CReplayID& id);

	void					PreClearWorld();
	void					ClearWorldOfEntities();

	void					RegisterAllCurrentEntities();
	void					DeregisterAllCurrentEntities();
	void					RemoveRecordingInformation();
	bool					EnsureIsBeingRecorded(const CEntity* pElem);
	bool					EnsureWillBeRecorded(const CEntity* pElem);
	bool					IsCurrentlyBeingRecorded(s32 replayID) const;
	void					ResetEntityCreationFrames();
	bool					SetCreationUrgent(s32 replayID);

	bool					ShouldRegisterElement(const CEntity* pEntity);

	void					Reset();				// Call in between looping playback
	void					Clear();				// Call in between recording
	void					ReleaseAndClear();		// Call on shutdown
	void					EnableRecording();

	s32						GetMemoryUsageForFrame(bool blockChange, atArray<u32>& expectedSizes);
	bool					PlaybackSetup(ReplayController& controller);
	bool					ApplyFades(ReplayController& controller, s32 frame);
	void					ApplyFadesSecondPass();
	void					RecordFrame(u32 threadIndex, u16 sessionBlockIndex);
	u32						UpdateFrameHash();
	void					PostRecordFrame(ReplayController& controller, bool blockChange, atArray<u32>& expectedSizes, int& expectedSizeIndex);
	void					PostRecordOptimizations(ReplayController& controller, const CAddressInReplayBuffer& endAddress);
	ePlayPktResult			PlayPackets(ReplayController& controller, bool preprocessOnly, bool entityMayNotExist = false);
	ePlayPktResult			JumpPackets(ReplayController& controller);
	void					Process();
	void					PostProcess();
	void					PostRender();

	void					OnCreateEntity(CPhysical* pEntity);
	void					OnDeleteEntity(CPhysical* pEntity);

	void					ResetAllEntities();
	void					StopAllEntitySounds();

	void					CreateDeferredEntities(ReplayController& controller);
	void					ProcessRewindDeletions(ReplayController& controller);
	bool					CreateBatchedEntities(const CReplayState& state);
	bool					HasBatchedEntitiesToProcess();

	bool					GetPacketName(eReplayPacketId packetID, const char*& packetName) const;

	void					ResetCreatePacketsForCurrentBlock();
	void					LinkCreatePacketsForNewBlock(ReplayController& controller);

	void					OffsetCreatePackets(s64 offset);
	void					OffsetUpdatePackets(s64 offset, u8* pStart, u32 blockSize);

	eReplayClipTraversalStatus	OutputReplayStatistics(ReplayController& controller, atMap<s32, packetInfo>& infos);
	eReplayClipTraversalStatus	PrintOutReplayPackets(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle);

#if GTA_REPLAY_OVERLAY
	eReplayClipTraversalStatus	CalculateBlockStats(ReplayController& controller, CBlockInfo* pBlock);	
	void GeneratePacketNameList(eReplayPacketId packetID, const char*& packetName);
#endif //GTA_REPLAY_OVERLAY

	void					PreloadPackets(tPreloadRequestArray& preloadRequests, tPreloadSuccessArray& successfulPreloads, u32 currGameTime);
	bool					FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& preloadReqCount, const u32 systemTime, bool isSingleFrame);
	void					FlushPreloadRequests();

	void					PrintOutEntities();

	int						GetInterfaceCount() const;
	bool					GetBlockDetails(char* pString, int interfaceIndex, bool& err, bool recording) const;
	bool					ScanForAnomalies(char* pString, int interfaceIndex, int& index) const;

	u32						GetFrameHash() const		{	return m_frameHash;	}
#if __BANK
	void					AddDebugWidgets();
#endif // __BANK

private:

	atArray<iReplayInterface*>	m_interfaces;

	u32							m_frameHash;
};


//////////////////////////////////////////////////////////////////////////
template<typename T>
CReplayInterface<T>* CReplayInterfaceManager::FindCorrectInterface()
{
	return (CReplayInterface<T>*)FindCorrectInterface(T::GetStaticClassId());
}


//////////////////////////////////////////////////////////////////////////
template<>
void CReplayInterfaceManager::GetEntity<CEntity>(CEntityGet<CEntity>& entityGet);

//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterfaceManager::GetEntity(CEntityGet<T>& entityGet)
{
	CReplayInterface<T>* pInterface = FindCorrectInterface<T>();
	replayAssert(pInterface);
	if(pInterface == NULL)
		return;
	pInterface->GetEntity(entityGet);
}

#endif // GTA_REPLAY

#endif // REPLAYINTERFACEMANAGER_H
