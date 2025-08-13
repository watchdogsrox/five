#ifndef REPLAYOVERLAY_H
#define REPLAYOVERLAY_H

#include "../ReplaySettings.h"

#if GTA_REPLAY_OVERLAY

#include "control/replay/ReplayBufferMarker.h"
#include "control/replay/Preload/ReplayPreloader.h"

class CBufferInfo;
class CAddressInReplayBuffer;
class FrameRef;
class CReplayInterfaceManager;
class CReplayEventManager;

#define NUM_BLOCK_STATS	10

class CReplayOverlay
{
public:

	//If add to this need to add to overlayModeStrings in cpp
	enum OverlayMode
	{
		StatusOnly = 0,
		BlockStatus,
		BlockDetails,
		BlockUsageStats,
		Extensions,
		MemoryDetails,
		MarkerRender,
		BlockGraph,
		CutsceneDetails,
		NoOverlay,
	};

	static void	UpdateRecord(const FrameRef& streamingFrame, const CAddressInReplayBuffer& address, const CBufferInfo& bufferInfo, const CReplayInterfaceManager& interfaceManager, const CReplayEventManager& eventManager, const CReplayAdvanceReader& advanceReader, u64 availableMemory, bool isRecording, bool overwatchEnabled);
	static void	UpdatePlayback(const FrameRef& streamingFrame, const CReplayFrameData& frameData, u32 frameFlags, const CAddressInReplayBuffer& address, const CBufferInfo& bufferInfo, const CReplayInterfaceManager& interfaceManager, const CReplayEventManager& eventManager, const CReplayAdvanceReader& advanceReader, u64 availableMemory);

	static void DisplayBlockStatus(const CBufferInfo& bufferInfo);
	static void DisplayBlockDetails(const CReplayInterfaceManager& interfaceManager, const CReplayEventManager& eventManager, const CReplayAdvanceReader& advanceReader, bool recording);
	static void DisplayExtensionDetails();
	static void DisplayMemoryDetails(u64 availableMemory);
	static void DisplayMarkerDebugDetails();
	static void DisplayCutsceneStatus();
	static void DisplayAllBlockUsageStats(const CBufferInfo& bufferInfo);

	static void IncrementOverlayMode()
	{
		overlayMode++;
		if(overlayMode > NoOverlay)
		{
			overlayMode = StatusOnly;
		}
	}
	static int overlayMode;

	static void AddWidgets(bkBank& bank);
	static void UpdateComboWidgets();
	static bool	ShouldCalculateBlockStats() { return overlayMode == BlockUsageStats; }

	static void GeneratePacketNameList(CReplayInterfaceManager& interfaceManager, CReplayEventManager& eventManager);
	static const char*	sm_PacketNames[PACKETID_MAX];
	static bkCombo* sm_ComboWidgets[NUM_BLOCK_STATS];

	static void SetPrecacheString(const char* str, const atString& param)
	{	
		sm_precachWaitingOnString = str;
		sm_precachWaitingOnString += param;
		sm_precachString = sm_precachWaitingOnString;
	}
	static void SetPrecacheString(const char* str, const s32 param = 0)
	{	
		if(param != 0)
		{
			char temp[128];
			sprintf(temp, "%s %u", str, param);
			sm_precachWaitingOnString = temp;
		}
		else
		{
			sm_precachWaitingOnString = str;
		}
		sm_precachString = sm_precachWaitingOnString;
	}
	static void PrependPrecacheString(const char* str)
	{
		sm_precachString = str;
		sm_precachString += sm_precachWaitingOnString;
	}

	static atString		sm_precachWaitingOnString;
	static atString		sm_precachString;

	struct debugMessage
	{
		debugMessage()
		: ID(-1)
		{}
		debugMessage(int i)
		: ID(i)
		{}

		bool operator==(const debugMessage& other) const
		{
			return ID == other.ID;
		}

		int ID;
		atString str;
	};
	static atArray<debugMessage>	sm_debugMessages;

	static void AddDebugMessage(int id, const char* pMessage)
	{
#if __BANK
		if(sm_debugMessages.GetCount() == sm_debugMessages.GetCapacity())
			return;

		if(sm_debugMessages.Find(debugMessage(id)) == -1)
		{
			debugMessage& msg = sm_debugMessages.Append();
			msg.ID = id;
			msg.str = pMessage;
		}
#endif // __BANK
	}

	static void AddDebugMessage(const char* pMessage)
	{
#if __BANK
		if(sm_debugMessages.GetCount() == sm_debugMessages.GetCapacity())
			return;

		int id = atStringHash(pMessage);
		if(sm_debugMessages.Find(debugMessage(id)) == -1)
		{
			debugMessage& msg = sm_debugMessages.Append();
			msg.ID = id;
			msg.str = pMessage;
		}
#endif // __BANK
	}
};

#endif // GTA_REPLAY_OVERLAY

#endif // REPLAYOVERLAY_H
