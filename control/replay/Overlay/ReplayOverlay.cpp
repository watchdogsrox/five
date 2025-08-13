 
#include "ReplayOverlay.h"

#if GTA_REPLAY_OVERLAY
#include "../Replay.h"
#include "../ReplaySupportClasses.h"
#include "../Streaming/ReplayStreaming.h"
#include "../ReplayInterfaceManager.h"
#include "../ReplayEventManager.h"
#include "../ReplayExtensions.h"
#include "control/Replay/ReplayBufferMarker.h"
#include "control/replay/ReplayLightingManager.h"

#include "camera/CamInterface.h"
#include "renderer/PostProcessFXHelper.h"
#include "SaveLoad/GenericGameStorage.h"

#include "debug/bar.h"

int CReplayOverlay::overlayMode = CReplayOverlay::StatusOnly;
const char* CReplayOverlay::sm_PacketNames[PACKETID_MAX];
atString CReplayOverlay::sm_precachWaitingOnString;
atString CReplayOverlay::sm_precachString;
bkCombo* CReplayOverlay::sm_ComboWidgets[NUM_BLOCK_STATS];
atArray<CReplayOverlay::debugMessage>	CReplayOverlay::sm_debugMessages;

const float initialLinePos = 0.008f;
const float lineHeight = 0.02f;
float currentLine = initialLinePos;

u32 currentBlockID = 0;
u32 blockStatsToTrack[NUM_BLOCK_STATS];
Color32 blockStatColours[NUM_BLOCK_STATS] = 
{
	Color32(255, 0, 0, 255), Color32(0, 255, 0, 255), Color32(255, 255, 0, 255), Color32(255, 0, 255, 255), Color32(0, 255, 255, 255),
	Color32(255, 155, 0, 255), Color32(185, 66, 255, 255), Color32(255, 255, 255, 255), Color32(160, 160, 160, 255), Color32(15, 255, 187, 255),
};
bool BlockStatsUseCustomSettings = false;
bool BlockStatsMonitorBufferTopTen = false;
bool BlockStatsSeperateMonitorCategory = false;

static const char * overlayModeStrings[] =
{
	"StatusOnly",
	"BlockStatus",
	"BlockDetails",
	"BlockUsageStats",
	"Extensions",
	"MemoryDetails",
	"BlockGraph",
	"MarkerRender",
	"CutsceneDetails",
	"NoOverlay"
};

CompileTimeAssert(CReplayOverlay::NoOverlay == COUNTOF(overlayModeStrings) - 1);

//////////////////////////////////////////////////////////////////////////
void CReplayOverlay::UpdateRecord(const FrameRef& streamingFrame, const CAddressInReplayBuffer& address, const CBufferInfo& bufferInfo, const CReplayInterfaceManager& interfaceManager, const CReplayEventManager& eventManager, const CReplayAdvanceReader& advanceReader, u64 availableMemory, bool isRecording, bool overwatchEnabled)
{
	(void)streamingFrame;
	(void)address;
	(void)bufferInfo;
	(void)interfaceManager;
	(void)eventManager;
	(void)advanceReader;
	(void)availableMemory;
	(void)isRecording;
	(void)overwatchEnabled;

	if(overlayMode == NoOverlay FINAL_DISPLAY_BAR_ONLY(|| CDebugBar::GetDisplayReleaseDebugText() != DEBUG_DISPLAY_STATE_STANDARD))
		return;

#if DEBUG_DRAW
	static char debugString[256];
	currentLine = initialLinePos;

	if(isRecording)
	{
		sprintf(debugString, "REC MemBlock: %u Pos:%010i FrameRef %u:%u", address.GetMemoryBlockIndex(), address.GetPosition(), streamingFrame.GetBlockIndex(), streamingFrame.GetFrame());

		Color32 color = Color32(0.8f, 0.0f, 0.0f);
		if(CReplayMgr::IsSaving())
		{
			color = Color32(0.0f, 0.0f, 1.0f);
#if RSG_ORBIS
			if(!CGenericGameStorage::IsSafeToSaveReplay())
			{
				color = Color32(0.0f, 1.0, 1.0f);
			}
#endif // RSG_ORBIS
			if(ReplayFileManager::IsForceSave())
			{
				color = Color32(1.0f, 0.0, 1.0f);
			}
		}

		grcDebugDraw::Text(Vec2V(0.002f, currentLine), color, debugString, false, 1.2f);
		currentLine += lineHeight;

		if(CReplayMgr::IsSaving() && ReplayFileManager::IsForceSave())
		{
			sprintf(debugString, "Forcing save... Reason: %s", CReplayControl::GetLastStateChangeReason().c_str());
			grcDebugDraw::Text(Vec2V(0.002f, currentLine), color, debugString, false, 1.2f);
			currentLine += lineHeight;
		}
	}
	
	if((isRecording || overlayMode > StatusOnly) && overwatchEnabled)
	{
		u32 count, bytes, count1, bytes1;
		eventManager.GetMonitorStats(count, bytes, count1, bytes1);
		sprintf(debugString, "Replay Overwatch: b- %u, %u f- %u, %u", count, bytes, count1, bytes1);
		grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(0.8f, 0.0f, 0.0f), debugString, false, 1.2f);
		currentLine += lineHeight;
	}

	if(overlayMode == BlockStatus && isRecording)
		DisplayBlockStatus(bufferInfo);

	if(overlayMode == BlockDetails)
		DisplayBlockDetails(interfaceManager, eventManager, advanceReader, true);

	if(overlayMode == Extensions)
		DisplayExtensionDetails();

	if(overlayMode == MemoryDetails)
		DisplayMemoryDetails(availableMemory);

	if(overlayMode == MarkerRender && isRecording)
	{
		DisplayMarkerDebugDetails();
		ReplayBufferMarkerMgr::DrawDebug(false);
	}

	if(overlayMode == BlockGraph && isRecording)
	{
		DisplayMarkerDebugDetails();
		ReplayBufferMarkerMgr::DrawDebug(true);
	}

	currentBlockID = 0;
	if(overlayMode == BlockUsageStats && isRecording)		
		DisplayAllBlockUsageStats(bufferInfo);
#endif // DEBUG_DRAW
}


//////////////////////////////////////////////////////////////////////////
void CReplayOverlay::UpdatePlayback(const FrameRef& streamingFrame, const CReplayFrameData& frameData, u32 frameFlags, const CAddressInReplayBuffer& address, const CBufferInfo& bufferInfo, const CReplayInterfaceManager& interfaceManager, const CReplayEventManager& eventManager, const CReplayAdvanceReader& advanceReader, u64 availableMemory)
{
	(void)streamingFrame;
	(void)frameData;
	(void)frameFlags;
	(void)address;
	(void)bufferInfo;
	(void)interfaceManager;
	(void)eventManager;
	(void)advanceReader;
	(void)availableMemory;

	if(overlayMode == NoOverlay FINAL_DISPLAY_BAR_ONLY(|| CDebugBar::GetDisplayReleaseDebugText() != DEBUG_DISPLAY_STATE_STANDARD))
		return;
	
#if DEBUG_DRAW
	static char debugString[256];
	currentLine = initialLinePos;

	sprintf(debugString, "PLAY MemBlock: %u Pos:%010i FrameRef %u:%u Time %f", address.GetMemoryBlockIndex(), address.GetPosition(), 
																									streamingFrame.GetBlockIndex(), streamingFrame.GetFrame(), CReplayMgr::GetCurrentTimeRelativeMs()/1000.0f);
	grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(0.0f, 0.8f, 0.0f), debugString, false, 1.2f);
	currentLine += lineHeight;

	sprintf(debugString, "PopValues:%f, %f, %f, %f",
		frameData.m_populationData.m_innerBandRadiusMin, 
		frameData.m_populationData.m_innerBandRadiusMax, 
		frameData.m_populationData.m_outerBandRadiusMin, 
		frameData.m_populationData.m_outerBandRadiusMax);

	grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(0.0f, 0.8f, 0.0f), debugString, false, 1.2f);
	currentLine += lineHeight;

	sprintf(debugString, "IsFadedOut:%s", camInterface::IsFadedOut() ? "Yes" : "No");

	grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(0.0f, 0.8f, 0.0f), debugString, false, 1.2f);
	currentLine += lineHeight;
	
	for(int i = 0; i < sm_debugMessages.size(); ++i)
	{
		const debugMessage& msg = sm_debugMessages[i];

		grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(0.8f, 0.8f, 0.0f), msg.str.c_str(), true, 1.2f);
		currentLine += lineHeight;
	}

	u32 flags = frameFlags;
	int index = 0;
	while(flags != 0)
	{
		if(flags & 1)
		{
			sprintf(debugString, "Flag enabled: %s", framePacketFlagStrings[index]);

			grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(0.0f, 0.8f, 0.0f), debugString, false, 1.2f);
			currentLine += lineHeight;
		}
		flags = flags >> 1;
		++index;
	}

	if(sm_precachString.GetLength() > 0)
	{
		sprintf(debugString, "Precaching String:%s", sm_precachString.c_str());
		grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(0.0f, 0.8f, 0.0f), debugString, false, 1.2f);
		currentLine += lineHeight;
	}

	if(overlayMode == StatusOnly)
	{
		for(int i = 0; i < interfaceManager.GetInterfaceCount(); ++i)
		{
			int j = 0;
			while(interfaceManager.ScanForAnomalies(&debugString[0], i, j))
			{
				grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(1.0f, 0.0f, 0.0f), debugString, true, 1.2f);
				currentLine += lineHeight;
			}
		}
	}

	if(overlayMode == BlockStatus)
		DisplayBlockStatus(bufferInfo);

	if(overlayMode == BlockDetails)
		DisplayBlockDetails(interfaceManager, eventManager, advanceReader, false);

	if(overlayMode == Extensions)
		DisplayExtensionDetails();

	if(overlayMode == MemoryDetails)
		DisplayMemoryDetails(availableMemory);
		
	if(overlayMode == CutsceneDetails)
		DisplayCutsceneStatus();
#endif // DEBUG_DRAW
}


//////////////////////////////////////////////////////////////////////////
int DisplayBlock(const CBlockInfo& block, void*)
{
	(void)block;

#if DEBUG_DRAW
	static char displayString[512];
	Color32 colour(0.0f, 0.8f, 0.0f);

	sprintf(displayString, "PB:%i SB:%u Sta:%s MrkCnt:%i Temp:%s Lock:%s SzUsd:%u (L:%u) FrCnt:%u, AvFSz:%u Dur:%03f s Thumb:%s %s", 
		block.GetMemoryBlockIndex(), 
		block.GetSessionBlockIndex(), 
		block.GetStatusAsString(block.GetStatus()), 
		ReplayBufferMarkerMgr::GetMarkerCount(block.GetMemoryBlockIndex()),
		block.IsTempBlock() ? "Y" : "N",
		block.GetLockAsString(), 
		block.GetSizeUsed(), 
		block.GetSizeLost(),
		block.GetFrameCount(),
		block.GetFrameCount() > 0 ? block.GetSizeUsed() / block.GetFrameCount() : 0,
		(float)block.GetTimeSpan() / 1000.0f
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
		,(block.IsThumbnailPopulated() ? "Got" : (block.HasRequestedThumbNail() ? "Req" : "No"))
		,((!block.IsThumbnailPopulated() && block.HasRequestedThumbNail()) ? AnimPostFXManagerReplayEffectLookup::GetFilterName(block.GetThumbnailRef().GetThumbnail().GetFailureReason()) : "")
#else
		,"-", "-", ""
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
		);

	if(block.IsBlockPlaying())
		colour = Color32(1.0f, 1.0f, 0.0f);
	else if(block.GetStatus() == REPLAYBLOCKSTATUS_EMPTY)
		colour = Color32(1.0f, 1.0f, 1.0f);
	else if(block.GetStatus() == REPLAYBLOCKSTATUS_FULL)
		colour = Color32(0.0f, 1.0f, 0.0f);
	else if(block.GetStatus() == REPLAYBLOCKSTATUS_SAVING)
		colour = Color32(0.0f, 0.0f, 1.0f);
	else
		colour = Color32(1.0f, 0.0f, 0.0f);

	grcDebugDraw::Text(Vec2V(0.002f, currentLine), colour, displayString, false, 1.2f);
	currentLine += lineHeight;
#endif // DEBUG_DRAW

	return 0;
}

//Sort the normal packet memory stats
static bool SortBlockUsageStats(const CReplayBlockStats& a, const CReplayBlockStats& b)
{
	return a.GetMemoryUse() > b.GetMemoryUse();
}

//Sort the monitored usage memory stats
static bool SortBlockUsageStatsMonitored(const CReplayBlockStats& a, const CReplayBlockStats& b)
{
	return a.GetMonitoredMemoryUse() > b.GetMonitoredMemoryUse();
}

//Sort the normal packet memory stats but include the monitored packets as a seperate stat
static bool SortBlockUsageStatsWithMonitored(const CReplayBlockStats& a, const CReplayBlockStats& b)
{
	return (a.GetMemoryUse() - a.GetMonitoredMemoryUse()) > (b.GetMemoryUse() - b.GetMonitoredMemoryUse());
}

void DisplayBlockUsageStatKey(Vector2 pos, Color32 barColor, Vector2 oneOverScreenSize, CReplayBlockStats& stat, const char* packetName, bool subtractMonitoredCount)
{
	float boxSize = 10.0f;
	grcDebugDraw::RectAxisAligned(pos, pos + (Vector2(boxSize, boxSize) * oneOverScreenSize), barColor);

	Vector2 textPos = pos + (Vector2(boxSize + 5.0f, 0.0f) * oneOverScreenSize);

	u32 numPackets = stat.GetNumPackets();
	if( subtractMonitoredCount )
		numPackets -= stat.GetNumMonitoredPackets();

	u32 packetSize = stat.GetPacketSize();
	u32 totalSize = stat.GetMemoryUse();

	char keyString[256];
	sprintf(keyString, "%s - numPackets(%d), packetSize(%d), total(%d)", packetName, numPackets, packetSize, totalSize);
	grcDebugDraw::Text(Vec2V(textPos.x, textPos.y), Color32(255, 255, 255, 255), keyString, true, 1.2f);
}

//////////////////////////////////////////////////////////////////////////
int DisplayBlockUsageStats(const CBlockInfo& block, void* pParam)
{
	float screenWidth = (float)GRCDEVICE.GetWidth();
	float screenHeight = (float)GRCDEVICE.GetHeight();
	bool lastFilledBlock = block.GetNextBlock()->GetStatus() == REPLAYBLOCKSTATUS_BEINGFILLED;

	int barAlpha = 90;
	if( lastFilledBlock )
		barAlpha = 255;

	float xStart = 20.0f;
	float yStart = 50.0f;
	float boxHeight = 5.0f;
	float yOffset = boxHeight + 5.0f;	
	float keyLabelYOffset = 15.0f;

	float boxMaxWidth = screenWidth - (xStart*2);
	Vector2 startBoxMin(xStart, yStart + (currentBlockID * yOffset));
	Vector2 startBoxMax(screenWidth - xStart, startBoxMin.y + boxHeight);
	Vector2 boxMin = startBoxMin;
	Vector2 boxMax = startBoxMax;
	Vector2 oneOverScreenSize(1.0f / screenWidth, 1.0f / screenHeight);

	CBufferInfo* bufferInfo = (CBufferInfo*)pParam;
	float KeyXOffsetStart = 300.0f;
	float KeyYOffsetStart = yStart + (bufferInfo->GetNumNormalBlocks() * yOffset);

	if( block.GetStatus() == REPLAYBLOCKSTATUS_FULL )
	{
		float totalAdded = 0.0f;
		u32 statsAdded = 0;
		u32 otherPackets = 0;

		if( BlockStatsUseCustomSettings )
		{
			//Go through the custom stats and see if any are set and display the percentage bar for them
			for( u32 i = 0; i < NUM_BLOCK_STATS; i++ )
			{
				if( blockStatsToTrack[i] != PACKETID_INVALID )
				{
					CReplayBlockStats stat = block.GetStat((eReplayPacketId)blockStatsToTrack[i]);
					float percentageUse = ((float) stat.GetMemoryUse() / (float) block.GetSizeUsed());
					boxMax.x = boxMin.x + (boxMaxWidth * percentageUse);
					Color32 barColor = blockStatColours[i];
					barColor.SetAlpha(barAlpha);
					grcDebugDraw::RectAxisAligned(boxMin * oneOverScreenSize, boxMax * oneOverScreenSize, barColor);
					boxMin.x = boxMax.x;
					totalAdded += percentageUse;

					//Add the key for the last block to be filled
					if( lastFilledBlock )
						DisplayBlockUsageStatKey(Vector2(KeyXOffsetStart, (KeyYOffsetStart + (statsAdded*keyLabelYOffset))) * oneOverScreenSize, barColor, oneOverScreenSize, stat, CReplayOverlay::sm_PacketNames[stat.GetPacketId()], false);

					statsAdded++;
				}
			}
		}
		//Show top 10
		else
		{
			//Copy all the stats locally so we can sort them, and work out the total monitored amount.
			u32 totalMonitoredMem = 0;
			u32 totalMonitoredCount = 0;
			CReplayBlockStats blockStats[PACKETID_MAX];
			for( u32 i = 0; i < PACKETID_MAX; i++ )
			{
				CReplayBlockStats stat = block.GetStat((eReplayPacketId)i);
				blockStats[i] = stat;
				totalMonitoredMem += stat.GetMonitoredMemoryUse();
				totalMonitoredCount += stat.GetNumMonitoredPackets();
			}
			//Add in the total monitored into the Invalid slot
			#define PACKETID_MONITORED	PACKETID_INVALID
			blockStats[PACKETID_MONITORED].Reset(PACKETID_MONITORED);
			if( BlockStatsSeperateMonitorCategory )
			{
				blockStats[PACKETID_MONITORED].IncrementMemoryUse(totalMonitoredMem, false);
				blockStats[PACKETID_MONITORED].SetNumPackets(totalMonitoredCount);
			}
			
			//Sort the stats so we can get the top10
			std::sort(&blockStats[0], &blockStats[PACKETID_MAX], BlockStatsMonitorBufferTopTen ? SortBlockUsageStatsMonitored : BlockStatsSeperateMonitorCategory ? SortBlockUsageStatsWithMonitored : SortBlockUsageStats);
			
			//Go through the first 10 and display the correct percentage bar
			for( u32 i = 0; i < 10; i++ )
			{
				CReplayBlockStats stat = blockStats[i];
				float percentageUse = 0.0f;
				if( BlockStatsMonitorBufferTopTen )
					percentageUse = ((float) stat.GetMonitoredMemoryUse() / (float) (EVENT_BLOCK_MONITOR_SIZE));
				else
					percentageUse = ((float) stat.GetMemoryUse() / (float) block.GetSizeUsed());

				if( percentageUse > 0.0f )
				{
					boxMax.x = boxMin.x + (boxMaxWidth * percentageUse);
					Color32 barColor = blockStatColours[i];
					barColor.SetAlpha(barAlpha);
					grcDebugDraw::RectAxisAligned(boxMin * oneOverScreenSize, boxMax * oneOverScreenSize, barColor);
					boxMin.x = boxMax.x;
					totalAdded += percentageUse;

					//Add the key for the last block to be filled
					if( lastFilledBlock )
					{
						DisplayBlockUsageStatKey(Vector2(KeyXOffsetStart, (KeyYOffsetStart + (statsAdded*keyLabelYOffset))) * oneOverScreenSize, barColor, 
						oneOverScreenSize, stat, stat.GetPacketId() == PACKETID_INVALID ? "MONITORED PACKETS" : CReplayOverlay::sm_PacketNames[stat.GetPacketId()], BlockStatsSeperateMonitorCategory && !BlockStatsMonitorBufferTopTen);
					}

					statsAdded++;
				}
			}
			//Add up the remaining packets so we can see how many "other" packets we have.
			for( u32 i = 10; i < PACKETID_MAX; i++ )
			{
				otherPackets += blockStats[i].GetNumPackets();
			}
		}


		//Add any remaining as "other" or "free" for monitored
		if( totalAdded < 1.0f )
		{
			Color32 OtherColor = Color32(0, 0, 255, barAlpha);
			boxMax.x = boxMin.x + (boxMaxWidth * (1.0f - totalAdded));
			grcDebugDraw::RectAxisAligned(boxMin * oneOverScreenSize, boxMax * oneOverScreenSize, OtherColor);

			//Add the key for the last block to be filled
			if( lastFilledBlock )
			{
				//Setup a fake stat for the remaining to add on the key
				char labelString[32];
				float memUsed = (1.0f - totalAdded);
				u32 numPackets = 1;
				if( BlockStatsMonitorBufferTopTen && !BlockStatsUseCustomSettings )
				{
					memUsed *= (EVENT_BLOCK_MONITOR_SIZE);
					sprintf(labelString, "FREE");
				}
				else
				{
					memUsed *= block.GetSizeUsed();
					sprintf(labelString, "OTHER");
					numPackets = otherPackets;
				}
				CReplayBlockStats stat;
				stat.Reset(PACKETID_INVALID);
				stat.IncrementMemoryUse((u32)memUsed, false);
				stat.SetNumPackets(numPackets);
				DisplayBlockUsageStatKey(Vector2(KeyXOffsetStart, (KeyYOffsetStart + (statsAdded*keyLabelYOffset))) * oneOverScreenSize, OtherColor, oneOverScreenSize, stat, labelString, false);
			}

		}

		currentBlockID++;	
	}				

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CReplayOverlay::DisplayBlockStatus(const CBufferInfo& bufferInfo)
{
	(void)bufferInfo;

#if DEBUG_DRAW
	static char displayString[512];

	Color32 colour(0.0f, 0.8f, 0.0f);
	bool alert = false;
	if(bufferInfo.IsAnyTempBlockOn() && bufferInfo.GetTempBlocksHaveLooped())
	{
		alert = true;
		colour = Color32(1.0f, 0.0f, 0.0f);
	}

	sprintf(displayString, "Recording into %s blocks %s", bufferInfo.IsAnyTempBlockOn() ? "Temp" : "Normal", alert == true ? "...TEMP BLOCKS HAVE LOOPED" : "");
	grcDebugDraw::Text(Vec2V(0.002f, currentLine), colour, displayString, false, 1.2f);
	currentLine += lineHeight;

	bufferInfo.ForeachBlock(DisplayBlock);
#endif // DEBUG_DRAW
}

//////////////////////////////////////////////////////////////////////////
void CReplayOverlay::DisplayAllBlockUsageStats(const CBufferInfo& bufferInfo)
{
#if DEBUG_DRAW
	bufferInfo.ForeachBlock(DisplayBlockUsageStats, (void*)&bufferInfo);
#endif // DEBUG_DRAW
}

//////////////////////////////////////////////////////////////////////////
void CReplayOverlay::DisplayBlockDetails(const CReplayInterfaceManager& interfaceManager, const CReplayEventManager& eventManager, const CReplayAdvanceReader& advanceReader, bool recording)
{
	(void)interfaceManager;
	(void)eventManager;
	(void)recording;
	(void)advanceReader;

#if DEBUG_DRAW
	static char displayString[512];
	int i = 0;
	bool err = false;
	for(; i < interfaceManager.GetInterfaceCount(); ++i)
	{
		if(interfaceManager.GetBlockDetails(&displayString[0], i, err, recording))
		{
			if(err)
				grcDebugDraw::Text(Vec2V(0.002f, 0.02f + i * 0.080f), Color32(1.0f, 0.0f, 0.0f), displayString, true, 1.2f);
			else
				grcDebugDraw::Text(Vec2V(0.002f, 0.02f + i * 0.080f), Color32(0.0f, 1.0f, 0.0f), displayString, true, 1.2f);
		}
	}

 	eventManager.GetBlockDetails(displayString, err);
	if(err)
 		grcDebugDraw::Text(Vec2V(0.002f, 0.02f + i * 0.080f), Color32(1.0f, 0.0f, 0.0f), displayString, true, 1.2f);
	else
		grcDebugDraw::Text(Vec2V(0.002f, 0.02f + i * 0.080f), Color32(0.0f, 1.0f, 0.0f), displayString, true, 1.2f);
/*
	++i;

	advanceReader.GetBlockDetails(displayString, err);
	if(err)
		grcDebugDraw::Text(Vec2V(0.002f, 0.02f + i * 0.080f), Color32(1.0f, 0.0f, 0.0f), displayString, true, 1.2f);
	else
		grcDebugDraw::Text(Vec2V(0.002f, 0.02f + i * 0.080f), Color32(0.0f, 1.0f, 0.0f), displayString, true, 1.2f);*/

#endif // DEBUG_DRAW
}


void CReplayOverlay::DisplayExtensionDetails()
{
#if DEBUG_DRAW
	static char displayString[512];
	int i = 0;

	while(CReplayExtensionManager::GetExtensionStatusString(displayString, i++))
	{
		grcDebugDraw::Text(Vec2V(0.002f, currentLine), Color32(1.0f, 0.0f, 0.0f), displayString, true, 1.2f);
		currentLine += lineHeight;
	}

#endif // DEBUG_DRAW
}

//////////////////////////////////////////////////////////////////////////
void CReplayOverlay::DisplayMemoryDetails(u64 availableMemory)
{
	(void)availableMemory;

#if DEBUG_DRAW
	float space = float(availableMemory) / 1024 / 1024;
	static char debugString[256];
	sprintf(debugString, "Current Memory Available: %fmb", space);
	grcDebugDraw::Text(Vec2V(0.002f, 0.05f),  Color32(1.0f, 1.0f, 1.0f), debugString, false, 1.2f);
#endif // DEBUG_DRAW
}

//////////////////////////////////////////////////////////////////////////
void CReplayOverlay::DisplayMarkerDebugDetails()
{
#if DEBUG_DRAW
	const MarkerDebugInfo markerDebugInfo = ReplayBufferMarkerMgr::GetDebugInfo();
	static char debugString[256];
	sprintf(debugString, "MaxBlockSize: %u AveBlockSize: %u, BlockSampleSize: %u, Variance: %u, Std Deviation: %u",  markerDebugInfo.m_MaxBlockSize, markerDebugInfo.m_AveBlockSize, markerDebugInfo.GetCount(), markerDebugInfo.m_Variance, markerDebugInfo.m_StdDeviation);
	grcDebugDraw::Text(Vec2V(0.002f, 0.025f),  Color32(0.0f, 0.8f, 0.0f), debugString, false, 1.2f);
#endif // DEBUG_DRAW
}

void CReplayOverlay::DisplayCutsceneStatus()
{
#if DEBUG_DRAW
	static char debugString[256];
	sprintf(debugString, "Num. Cutscene Lights %u Num. Ped Lights %u", (u32)ReplayLightingManager::GetNumberCutsceneLights(), (u32)ReplayLightingManager::GetNumberPedLights());
	grcDebugDraw::Text(Vec2V(0.002f, 0.05f),  Color32(0.0f, 0.8f, 0.0f), debugString, false, 1.2f);
#endif // DEBUG_DRAW
}

void CReplayOverlay::GeneratePacketNameList(CReplayInterfaceManager& interfaceManager, CReplayEventManager& eventManager)
{
	for(u32 i = 0; i < PACKETID_INVALID; i++)
	{
		eReplayPacketId packetID = (eReplayPacketId) i;
		interfaceManager.GeneratePacketNameList(packetID, sm_PacketNames[packetID]);
		eventManager.GeneratePacketNameList(packetID, sm_PacketNames[packetID]);
	}
}

void CReplayOverlay::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Overlay", false);

	bank.AddCombo("Overlay Mode", (s32*)&overlayMode, NoOverlay, overlayModeStrings );

	bank.PushGroup("Block Stats", false);
	bank.AddToggle("Separate monitor category",	&BlockStatsSeperateMonitorCategory);
	bank.AddToggle("Show monitored buffer top10",	&BlockStatsMonitorBufferTopTen);

	bank.AddSeparator();
	bank.AddToggle("Use custom settings",	&BlockStatsUseCustomSettings);

	for(u32 i = 0; i < NUM_BLOCK_STATS; i++)
	{
		char comboName[32];
		sprintf(comboName, "PacketID %d", i);
		blockStatsToTrack[i] = PACKETID_INVALID;
		sm_ComboWidgets[i] = bank.AddCombo(comboName, (s32*)&blockStatsToTrack[i], PACKETID_INVALID, sm_PacketNames );
	}	

	bank.PopGroup();

	bank.PopGroup();
}

void CReplayOverlay::UpdateComboWidgets()
{
	for(u32 i = 0; i < NUM_BLOCK_STATS; i++)
	{
		if(sm_ComboWidgets[i])
		{
			for(u32 packetId = 0; packetId < PACKETID_INVALID; packetId++)
			{
				sm_ComboWidgets[i]->SetString(packetId, sm_PacketNames[packetId]);
			}
		}
	}
}

#endif // GTA_REPLAY_OVERLAY
