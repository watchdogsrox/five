#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayInterface.h"
#include "Control/Replay/ReplayInternal.h"
#include "control/replay/ReplayController.h"
#include "Control/Replay/Misc/ReplayPacket.h"

int iReplayInterface::sm_totalEntitiesCreatedThisFrame = 0;

//////////////////////////////////////////////////////////////////////////
iReplayInterface::iReplayInterface(u32 recBufferSize, u32 crBufferSize, u32 fullCrBufferSize)
	: m_enableSizing(true)
	, m_enableRecording(true)
	, m_enablePlayback(true)
	, m_enabled(true)
	, m_anomaliesCanExist(true)
	, m_updateOnPreprocess(false)
	, m_recordBuffer(NULL)
	, m_recordBufferFailure(false)
	, m_pCreatePacketsBuffer(NULL)
	, m_createPacketsBufferFailure(false)
	, m_pFullCreatePacketsBuffer(NULL)
	, m_fullCreatePacketsBufferFailure(false)
	, m_frameHash(0)
{
	m_recordBufferSize				= recBufferSize;
	if(m_recordBufferSize != 0)
		m_recordBuffer				= rage_aligned_new(REPLAY_DEFAULT_ALIGNMENT) u8[m_recordBufferSize];
	m_recordBufferUsed				= 0;

	m_createPacketsBufferSize		= crBufferSize;
	if(m_createPacketsBufferSize != 0)
		m_pCreatePacketsBuffer		= rage_aligned_new(REPLAY_DEFAULT_ALIGNMENT) u8[m_createPacketsBufferSize];
	m_createPacketsBufferUsed		= 0;

	m_fullCreatePacketsBufferSize	= fullCrBufferSize;
	if(m_fullCreatePacketsBufferSize != 0)
		m_pFullCreatePacketsBuffer	= rage_aligned_new(REPLAY_DEFAULT_ALIGNMENT) u8[m_fullCreatePacketsBufferSize];
	m_fullCreatePacketsBufferUsed	= 0;
}


//////////////////////////////////////////////////////////////////////////
iReplayInterface::~iReplayInterface()
{
	if(m_recordBuffer)
		delete[] m_recordBuffer;
	if(m_pCreatePacketsBuffer)
		delete[] m_pCreatePacketsBuffer;
	if(m_pFullCreatePacketsBuffer)
		delete[] m_pFullCreatePacketsBuffer;
};


//////////////////////////////////////////////////////////////////////////
u32	 iReplayInterface::GetMemoryUsageForFrame(bool blockChange)
{	
	return m_recordBufferUsed + sizeof(CPacketAlign) + REPLAY_DEFAULT_ALIGNMENT + (blockChange == true ? m_fullCreatePacketsBufferUsed : m_createPacketsBufferUsed) + GetDeletionsSize();
}


//////////////////////////////////////////////////////////////////////////
void iReplayInterface::PostRecordFrame(ReplayController& controller, u16 previousBlockFrameCount, bool blockChange)
{
	u32 indexBeforeRecord = controller.GetCurrentPosition();

	CAddressInReplayBuffer createAddress = controller.GetAddress();
	ReplayController positionBeforeCreationRecord(createAddress, controller.GetBufferInfo(), controller.GetCurrentFrameRef());

	RecordDeletionPackets(controller);

	// Record the creation packets....full or incremental depending on
	// whether a block change has occurred.
	if(blockChange)
		controller.RecordBuffer(m_pFullCreatePacketsBuffer, m_fullCreatePacketsBufferUsed);
	else
		controller.RecordBuffer(m_pCreatePacketsBuffer, m_createPacketsBufferUsed);

	u32 afterCreatePackets = controller.GetCurrentPosition();

	// Align the update packets
	controller.RecordPacketWithParam<CPacketAlign>(REPLAY_DEFAULT_ALIGNMENT);

	// Store the address before recording the update buffer...
	CAddressInReplayBuffer address = controller.GetAddress();
	ReplayController positionBeforeRecord(address, controller.GetBufferInfo(), controller.GetCurrentFrameRef());

	// Copy from update buffer
	controller.RecordBuffer(m_recordBuffer, m_recordBufferUsed);


	controller.DisableRecording();

	// Link all the create packets correctly
	positionBeforeCreationRecord.EnableModify();
	LinkCreatePacketsForPlayback(positionBeforeCreationRecord, afterCreatePackets);
	positionBeforeCreationRecord.DisableModify();

	// Link all the update packets correctly
	positionBeforeRecord.EnableModify();
	LinkUpdatePackets(positionBeforeRecord, previousBlockFrameCount, controller.GetCurrentPosition());
	positionBeforeRecord.DisableModify();

	controller.RenableRecording();

	m_memoryUsageActual = controller.GetCurrentPosition() - indexBeforeRecord;
}


//////////////////////////////////////////////////////////////////////////
bool iReplayInterface::GetPacketGroup(eReplayPacketId packetID, u32& packetGroup) const
{
	if(packetID >= m_relevantPacketMin && packetID <= m_relevantPacketMax)
	{
		packetGroup = m_packetGroup;
		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
void iReplayInterface::ApplyFades(ReplayController& /*controller*/, s32)
{
// 	u32 packetSize = controller.GetCurrentPacket<CPacketBase>()->GetPacketSize();
// 
// 	controller.AdvanceUnknownPacket(packetSize);
}


//////////////////////////////////////////////////////////////////////////
// Find and remove any preload requests that have expired.  That is the time
// they were last requested compared to the current time...  If it's expired
// then either the entity was created, in which case that'll hold a ref, or
// we've moved away and this model is no longer needed.
void iReplayInterface::UpdatePreloadRequests(u32 time)
{
	m_modelManager.UpdatePreloadRequests(time);
}


//////////////////////////////////////////////////////////////////////////
void iReplayInterface::OutputReplayStatistics(ReplayController& controller, atMap<s32, packetInfo>& infos)
{
	u32 packetSize = controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize();

	eReplayPacketId packetID = controller.ReadCurrentPacket<CPacketBase>()->GetPacketID();

	packetInfo* pInfo = infos.Access(packetID);
	if(pInfo == NULL)
	{
		packetInfo info;
		info.count = 1;

		const char* packetName = NULL;
		GetPacketName(packetID, packetName);
		strcpy_s(info.name, sizeof(info.name),  packetName);

		info.size = packetSize;
		infos.Insert(packetID, info);
	}
	else
	{
		++pInfo->count;
	}

	controller.AdvanceUnknownPacket(packetSize);
}


//////////////////////////////////////////////////////////////////////////
void iReplayInterface::PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{	
	(void)mode;
	(void)handle;

	u32 packetSize = controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize();
	replayAssert(packetSize > 0 && "Incorrect interface called!");
	controller.AdvanceUnknownPacket(packetSize);
}


//////////////////////////////////////////////////////////////////////////
void iReplayInterface::PrintOutPacketHeader(CPacketBase const* pPacket, eReplayPacketOutputMode mode, FileHandle handle)
{
	const char* pPacketName = NULL;
	GetPacketName(pPacket->GetPacketID(), pPacketName);
	replayAssert(pPacketName);

	if(mode == REPLAY_OUTPUT_XML)
	{
		char str[1024];
		snprintf(str, 1024, "\t\t<Name>%s</Name>\n", pPacketName);
		CFileMgr::Write(handle, str, istrlen(str));
	}
	else
		replayDebugf1("Packet %s", pPacketName);
}


//////////////////////////////////////////////////////////////////////////
u32	 iReplayInterface::GetHash(u32 id, u32 hash)
{
	return CReplayMgr::hash(&id, 1, hash);
}


//////////////////////////////////////////////////////////////////////////
#if __BANK
bkGroup* iReplayInterface::AddDebugWidgets(const char* groupName)
{
	bkBank* pBank = BANKMGR.FindBank("Replay");
	if(pBank == NULL)
		return NULL;

	bkGroup* pGroup = pBank->AddGroup(groupName, false);

	return pGroup;
}
#endif // __BANK

#endif // GTA_REPLAY
