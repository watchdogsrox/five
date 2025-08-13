#include "ReplayBufferMarker.h"

#if GTA_REPLAY

#include "control/replay/replay.h"
#include "control/replay/ReplayRecording.h"
#include "scene/world/GameWorld.h"
#include "Peds/Ped.h"

#if __DEV
#include "grcore/debugdraw.h"
#endif

atFixedArrayReplayMarkers		ReplayBufferMarkerMgr::sm_ProcessedMarkers;
atFixedArrayReplayMarkers		ReplayBufferMarkerMgr::sm_Markers;

atFixedArray<ReplayBufferMarker, REPLAY_HEADER_EVENT_MARKER_MAX> ReplayBufferMarkerMgr::sm_SavedMarkers;

s32						ReplayBufferMarkerMgr::sm_BlockMarkerCounts[MAX_NUM_REPLAY_BLOCKS];

bool					ReplayBufferMarkerMgr::sm_Saving = false;
bool					ReplayBufferMarkerMgr::sm_Flush = false;
bool					ReplayBufferMarkerMgr::sm_StartFlush = false;

eMissionMarkerState		ReplayBufferMarkerMgr::sm_MissionMarkerState = MISSION_MARKER_STATE_IDLE;
FileDataStorage			ReplayBufferMarkerMgr::sm_DataStorage;

#if __BANK
s32						ReplayBufferMarkerMgr::sm_TimeInFuture = 0;
s32						ReplayBufferMarkerMgr::sm_TimeInPast = 0;
MarkerDebugInfo			ReplayBufferMarkerMgr::sm_DebugInfo;
#endif //__BANK

PARAM(autoreplaymarkup, "Enables Automatic Replay Marker Up");

void ReplayBufferMarkerMgr::InitSession(unsigned)
{
	Reset();
}

void ReplayBufferMarkerMgr::ShutdownSession(unsigned)
{
	Reset();
}

void ReplayBufferMarkerMgr::Reset()
{
	for(int i = 0; i < MAX_NUM_REPLAY_BLOCKS; ++i)
	{
		sm_BlockMarkerCounts[i] = 0;
	}
	
	sm_SavedMarkers.clear();
	
	sm_Markers.clear();
	sm_ProcessedMarkers.clear();
}

void ReplayBufferMarkerMgr::InitWidgets()
{
#if __BANK
	bkBank* bank = BANKMGR.FindBank("Replay");

	if( bank != NULL )
	{
		bank->PushGroup("Marker Manager", true);

		bank->AddButton("Add Marker", &AddMarkerCallback);

		sm_TimeInFuture = 5000;
		bank->AddSlider("Time In Future", &sm_TimeInFuture, 0, 100000, 1);

		sm_TimeInPast = 10000;
		bank->AddSlider("Time In Past", &sm_TimeInPast, 0, 100000, 1);

		bank->PopGroup();
	}
#endif //__BANK
}

#if __BANK
void ReplayBufferMarkerMgr::AddMarkerCallback()
{
	AddMarkerInternal(sm_TimeInPast, sm_TimeInFuture, IMPORTANCE_NORMAL, CODE_RAG_TEST);
}
#endif //__BANK

void ReplayBufferMarkerMgr::AddMarker(u32 msInThePast, u32 msInTheFuture, MarkerImportance importance, eMarkerSource source, MarkerOverlap markerOverlap)
{
	bool missionModeEnabled = CReplayControl::IsMissionModeEnabled();

	if(!missionModeEnabled)
	{
		return;
	}

	if(markerOverlap != OVERLAP_IGNORE && CheckForMarkerOverlap(msInThePast, msInTheFuture, markerOverlap))
	{
		return;
	}

	AddMarkerInternal(msInThePast, msInTheFuture, importance, source, markerOverlap);
}

bool ReplayBufferMarkerMgr::AddMarkerInternal(u32 msInThePast, u32 msInTheFuture, MarkerImportance importance, eMarkerSource source, MarkerOverlap markerOverlap)
{
	if(CReplayMgr::IsRecordingEnabled())
	{
		if( sm_Markers.GetCount() >= REPLAY_HEADER_EVENT_MARKER_MAX )
		{
			replayAssertf(false, "Too many markers have been added in quick succession, use the marker timeline (F3 on replay keyboard) on to debug issue.  Number Markers: %u Marker Limit: %u", sm_Markers.GetCount(), REPLAY_HEADER_EVENT_MARKER_MAX);
			return false;
		}

		// B*2028310 - Don't allow any markers from script or code... manual only.
		if( source == SCRIPT || source == CODE )
		{
			return false;
		}

		CBlockProxy firstBlock = CReplayMgr::FindFirstBlock();
		u32 blockStartTime = firstBlock.GetStartTime();
		u32 current_replay_time = CReplayMgr::GetCurrentBlock().GetEndTime();

		// Only go back as far as buffer allows -
		u32 recordingLength = current_replay_time - blockStartTime;
		msInThePast = Min(msInThePast, recordingLength);

		u32 time_since_all_suppression = current_replay_time - CReplayMgr::GetFrameTimeAllEventsWereSuppressed();
		msInThePast = Min(msInThePast, time_since_all_suppression);

		// If the source is script, we must clip the start to the EventsEnabledTime...
		if ( source == SCRIPT || source == CODE )
		{
			// last disable length - needs to be just the disabled time
			u32 time_since_last_disable = current_replay_time - CReplayMgr::GetFrameTimeScriptEventsWereDisabled();
			msInThePast = Min(msInThePast, time_since_last_disable);
		}

		//B*1790485 - Clips should be at least 3 seconds long.
		if( (msInThePast + msInTheFuture) < MINIMUM_CLIP_LENGTH )
		{
			msInTheFuture = MINIMUM_CLIP_LENGTH - msInThePast;
		}

		u32 startTime = current_replay_time - msInThePast;
		u32 endTime = current_replay_time + msInTheFuture;

		Assert( startTime >= blockStartTime);

		time_t unixTime;
		time(&unixTime);

		ReplayBufferMarker marker;
		marker.SetUnixTime(u64(unixTime));
		marker.SetStartTime(startTime);
		marker.SetEndTime(endTime);
		marker.SetTimeInPast(msInThePast);
		marker.SetTimeInFuture(msInTheFuture);
		marker.SetImportance(importance);
		marker.SetSource((u8)source);
		marker.SetMarkerOverlap(markerOverlap);
		if(CReplayMgr::IsEnableEventsThisFrame())
		{
			marker.SetMissionName(CReplayMgr::GetMissionName().c_str());
			marker.SetMontageFilter(CReplayMgr::GetMontageFilter().c_str());
			marker.SetMissionIndex((u32)CReplayMgr::GetMissionIndex());
		}

		const CPed* pPlayer = CGameWorld::FindLocalPlayer();
		marker.SetPlayerType(pPlayer ? (s8)pPlayer->GetPedType() : (s8)PEDTYPE_INVALID);

		replayAssertf((marker.GetRelativeEndTime() - marker.GetRelativeStartTime()) >= MINIMUM_CLIP_LENGTH, "Clip Length is too short.");
		replayAssertf(marker.GetLength() >= MINIMUM_CLIP_LENGTH, "Clip Length is too short.");
		replayAssertf(endTime > startTime, "End time is before start time.");
		replayAssertf(marker.GetStartTime() < marker.GetEndTime(), "End time is before start time.");

#if __BANK
		const char *pSources[3] = 
		{
			"Code",
			"Script",
			"Manual"
		};

		replayDisplayf("REPLAY MARKER: Adding Marker - Start Time: %u End Time: %u Importance: %u source: %s", marker.GetStartTime(), marker.GetEndTime(), marker.GetImportance(), pSources[source]);
#endif
		sm_Markers.Push(marker);
	}
	else
	{
		replayDisplayf("REPLAY MARKER: Not adding a marker as replay isn't enabled!");
	}

	return true;
}

void ReplayBufferMarkerMgr::Process()
{
	if(!CReplayMgr::IsRecordingEnabled())
	{
		return;
	}

	// DEBUG
	/*
	u16 startBlockIndex = CReplayMgr::FindFirstBlockIndex();
	u16 endBlockIndex = CReplayMgr::FindLastBlockIndex();
	u32 blockStartTime =  CReplayMgr::GetBlockStartTime(CReplayMgr::FindFirstBlockIndex());
	u32 currentTime = CReplayMgr::GetBlockEndTime(CReplayMgr::FindLastBlockIndex());
	Displayf("ReplayBufferMarkerMgr::Process() - TIME DATA - Start Block Index = %d, End Block Index = %d, Start Block Time = %d, End Block/Current Time = %d", startBlockIndex, endBlockIndex, blockStartTime, currentTime );
	*/
	// DEBUG

	if( CReplayControl::GetStartParam() == REPLAY_START_PARAM_DIRECTOR )
	{
		// Auto add a marker for the entire buffer, so save out this buffer
		CheckForSplittingManualMarker();
	}

	// Check any pending markers have crossed the end time, and add to processed list
	ProcessMarkers();				

	UpdateMissionMarkerClips();

	ProcessSaving();
	
	FlushInternal();
}

void ReplayBufferMarkerMgr::ProcessMarkers()
{
	if( !CReplayMgr::IsSaving() && !CReplayMgr::IsAnyTempBlockOn() )
	{
		CBlockProxy firstBlock = CReplayMgr::FindFirstBlock();
		CBlockProxy lastBlock = CReplayMgr::FindLastBlock();

		u32 currentTime = lastBlock.GetEndTime();
		u32 blockStartTime = firstBlock.GetStartTime();

		for(int i=0; i < sm_Markers.GetCount(); i++)
		{
			ReplayBufferMarker &marker = sm_Markers[i];
			u32 markerEndTime = marker.GetEndTime();

			// Reject any that end have times in the future
			if( markerEndTime > currentTime )
			{
				replayDebugf1("[REPLAYBUFFERMARKER] - Marker end time hasn't been reached yet, will get processed again next frame - EndTime: %d, CurrentTime: %d", markerEndTime, currentTime);

				continue;
			}

			// Check if we want to make this a little longer (awaiting for the end of speech etc..)
			// We can't do this for manual recorded clips.
			if( CReplayMgr::ShouldSustainRecording() && marker.GetSource() != CODE_MANUAL )
			{
				u32 diffTime = currentTime - marker.GetEndTime();
				marker.SetEndTime(marker.GetEndTime() + diffTime );

				replayDebugf1("[REPLAYBUFFERMARKER] - Extending the life of the marker, will get processed again the next frame!");
				continue;
			}

			// Every marker after this point will either be deleted due to length, our inability to process it, or passed to the processed list
			if( Verifyf(sm_ProcessedMarkers.GetCount() < sm_ProcessedMarkers.GetMaxCount(),"[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::ProcessMarkers() - ERROR - sm_ProcessedMarkers list is full!" ) )
			{
				// Get a clamped start time for this marker
				u32 markerStartTime = Max( marker.GetStartTime(), blockStartTime);
				marker.SetStartTime(markerStartTime);

				replayDebugf1("First Block Start Time: %u", blockStartTime);
				replayDebugf1("Last Block End Time: %u", currentTime);
				replayDebugf1("Clip Length: %u", currentTime - blockStartTime);

				// Only pass onto the processed list markers that pass the minimum length requirements.
				if( marker.GetLength() >= MINIMUM_CLIP_LENGTH )
				{
					replayAssertf(currentTime - blockStartTime >= MINIMUM_CLIP_LENGTH, "Clip length in blocks is < minimum clip length.  Clip Length:%u - Min Clip Length: %u", (currentTime - blockStartTime), MINIMUM_CLIP_LENGTH);

					sm_ProcessedMarkers.Push(marker);
				}
				else
				{
					replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::ProcessMarkers() - Clip is too short so disregarding");
				}
			}
			else
			{
				replayDebugf1("[REPLAYBUFFERMARKER] - Too many markers have been added, ignoring this marker!!!");
			}

			// Now delete the unprocessed marker
			sm_Markers.Delete(i);

			replayDebugf1("[REPLAYBUFFERMARKER] - Removing marker as it has been processed");

			i--;	// Make sure we don't skip the entry that will fill the one we just deleted.
		}

		// Now do any processing on the processed markers, essentially, clipping to available data
		for(int i=sm_ProcessedMarkers.GetCount()-1; i >=0 ; i--)
		{
			ReplayBufferMarker &marker = sm_ProcessedMarkers[i];

			if( blockStartTime > marker.GetStartTime() )
			{
				marker.SetStartTime(blockStartTime);

				// If the start time has reduced the length (or made it negative), remove this marker.
				if( marker.GetLength() < MINIMUM_CLIP_LENGTH)
				{
					replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::ProcessMarkers() - Clip is too short, deleting!");
					sm_ProcessedMarkers.Delete(i);
				}
			}
		}

	}
}


// Must be done before process markers
void ReplayBufferMarkerMgr::CheckForSplittingManualMarker()
{
	if(CReplayMgr::IsRecordingEnabled() && CReplayMgr::CanSaveClip() && !sm_StartFlush)		// If we're already flushing, there's no reason to bother with this check here
	{
		u32 startTime;
		if( CReplayControl::GetCachedUserStartTime( startTime ) )
		{
			CBlockProxy firstBlock = CReplayMgr::FindFirstBlock();
			CBlockProxy lastBlock = CReplayMgr::FindLastBlock();

			// Is the end about to overwrite the start
			if( lastBlock.GetNextBlock() == firstBlock)
			{
				u32 endTime = firstBlock.GetEndTime();
				// If the start time is less than the end of our start block and we're about to wrap, then we must add a new marker
				if( startTime < endTime )
				{
					u32 current_replay_time = CReplayMgr::GetCurrentBlock().GetEndTime();
					u32 timeInPast = current_replay_time - startTime;
					AddMarkerInternal(timeInPast, 0, IMPORTANCE_NORMAL, CODE_MANUAL);
					CReplayControl::UpdateCachedUserStartTime( CReplayMgr::GetCurrentBlock().GetEndTime() );
				}
			}
		}
	}
}

void ReplayBufferMarkerMgr::ProcessSaving()
{
	if(CReplayMgr::IsRecordingEnabled() && CReplayMgr::CanSaveClip() && !sm_StartFlush)		// If we're already flushing, there's no reason to bother with this check here
	{
		CBlockProxy firstBlock = CReplayMgr::FindFirstBlock();
		CBlockProxy lastBlock = CReplayMgr::FindLastBlock();

		// Is the end about to overwrite the start
		if( lastBlock.GetNextBlock() == firstBlock)
		{
			// Check if we need to write anything out
			u32 endTime = firstBlock.GetEndTime();

			// Check for any processed markers that have a start time lower than our next block end time
			for(int i=0; i<sm_ProcessedMarkers.GetCount(); i++)
			{
				ReplayBufferMarker &marker = sm_ProcessedMarkers[i];
				if( marker.GetStartTime() <= endTime )
				{
					// At least one marker has a start time that is lower than the next block to be overwritten's end time.
					// So attempt to write out the blocks.
					Flush();
					break;
				}
			}
		}
	}
}

// Go through every processed marker and find the one with the lowest start time
bool ReplayBufferMarkerMgr::GetProcessedMarkerWithLowestStartTime(u32 &index)
{
	bool found = false;
	u32 time = MAX_UINT32;
	index = 0xffffffff;
	for(int i = 0; i < sm_ProcessedMarkers.GetCount(); i++)
	{
		ReplayBufferMarker &marker = sm_ProcessedMarkers[i];
		if( marker.GetStartTime() < time )
		{
			time = marker.GetStartTime();
			index = i;
			found = true;
		}
	}
	return found;
}


int printBlock(CBlockProxy OUTPUT_ONLY(block), void*)
{
	replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Block %d: SessionBlockIndex = %d - Status = %s, Start Time = %d, End Time = %d", block.GetMemoryBlockIndex(), block.GetSessionBlockIndex(), CBlockInfo::GetStatusAsString(block.GetStatus()), block.GetStartTime(), block.GetEndTime() );
	return 0;
}

bool ReplayBufferMarkerMgr::MarkBlocksForSave(u8& importance)
{
	Assertf(!CReplayMgr::IsSaving(), "ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Marking blocks for save when saving!");

	if( sm_ProcessedMarkers.GetCount() == 0 )
	{
		return false;
	}

	// Clear the marked for save block pointer array
	CReplayMgr::ClearSaveBlocks();

	importance = 0;

	BANK_ONLY(int totalMarkerBlockCount = 0;)

	// Find all the markers that we can write out in the single write of consecutive blocks
	bool firstMarker = true;
	bool allConsecutiveFound = false;

	do
	{
		u32 markerIndex;
		bool validMarker = GetProcessedMarkerWithLowestStartTime(markerIndex);
		if( validMarker )
		{
			ReplayBufferMarker &marker = sm_ProcessedMarkers[markerIndex];

			if( marker.GetLength() < MINIMUM_CLIP_LENGTH )
			{
				replayAssertf(false, "[REPLAYBUFFERMARKER] - Clip was too short - Length:%f", (double)marker.GetLength());
				sm_ProcessedMarkers.Delete(markerIndex);
				continue;
			}

			// DEBUG
			/*
			{
				u16 startBlockIndex = CReplayMgr::FindFirstBlockIndex();
				u16 endBlockIndex = CReplayMgr::FindLastBlockIndex();
				u32 blockStartTime =  CReplayMgr::GetBlockStartTime(CReplayMgr::FindFirstBlockIndex());
				u32 currentTime = CReplayMgr::GetBlockEndTime(CReplayMgr::FindLastBlockIndex());
				Displayf("ReplayBufferMarkerMgr::MarkBlocksForSave() - BLOCK TIME DATA - Start Block Index = %d, End Block Index = %d, Start Block Time = %d, End Block/Current Time = %d", startBlockIndex, endBlockIndex, blockStartTime, currentTime );
				Displayf("ReplayBufferMarkerMgr::MarkBlocksForSave() - MARKER TIME DATA - Start Time = %d, End Time = %d", marker.GetStartTime(), marker.GetEndTime());
			}
			*/
			// DEBUG

			// Get the start and end index's for the blocks at this time
			CBlockProxy firstBlock = CReplayMgr::FindFirstBlock();
			CBlockProxy lastBlock = CReplayMgr::FindLastBlock();

			// Find the block where the start of the marker resides
			CBlockProxy startBlock = firstBlock;
			do 
			{
				u32 startTime = startBlock.GetStartTime();
				u32 endTime = startBlock.GetEndTime();
				if( (marker.GetStartTime() >= startTime && marker.GetStartTime() <= endTime) || startBlock == lastBlock )
				{
					break;
				}
				startBlock = startBlock.GetNextBlock();
			} while (startBlock.IsValid());

#if __ASSERT
			if( startBlock.IsValid() == false)
			{
				replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Marker Start Time = %d, End Time = %d", marker.GetStartTime(), marker.GetEndTime() );
				CReplayMgr::ForeachBlock(printBlock);
			}
#endif
			replayFatalAssertf( startBlock.IsValid() , "ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Unable to find block Index for startTime" );

			// Find the block where the end of the marker resides
			CBlockProxy endBlock = startBlock;
			do 
			{
				u32 startTime = endBlock.GetStartTime();
				u32 endTime = endBlock.GetEndTime();
				if( (marker.GetEndTime() >= startTime && marker.GetEndTime() <= endTime) || endBlock == lastBlock)
				{
					break;
				}
				endBlock = endBlock.GetNextBlock();
			} while (endBlock.IsValid());

#if __ASSERT
			if( endBlock.IsValid() == false )
			{
				replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Marker Start Time = %d, End Time = %d", marker.GetStartTime(), marker.GetEndTime() );
				CReplayMgr::ForeachBlock(printBlock);
			}
#endif
			replayFatalAssertf( endBlock.IsValid() , "ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Unable to find block Index for endTime" );

			replayDebugf1("[REPLAYBUFFERMARKER] - First Block Start Time: %u", startBlock.GetStartTime());
			replayDebugf1("[REPLAYBUFFERMARKER] - Last Block End Time: %u", endBlock.GetEndTime());
			replayDebugf1("[REPLAYBUFFERMARKER] - Clip Length: %u", endBlock.GetEndTime() - startBlock.GetStartTime());
			replayAssertf(endBlock.GetEndTime() - startBlock.GetStartTime() >= MINIMUM_CLIP_LENGTH, "Clip length in blocks is < minimum clip length.  Clip Length:%u - Min Clip Length: %u", (endBlock.GetEndTime() - startBlock.GetStartTime()), MINIMUM_CLIP_LENGTH);

#if __ASSERT
			if( startBlock != endBlock )
			{
				// Check contiguity of session block index's for this marker
				CBlockProxy block = startBlock/*.GetNextBlock()*/;
				u32 sessionBlockIndex = block.GetSessionBlockIndex();
				if( block.IsValid())
				{
					do
					{
						block = block.GetNextBlock();
						u32 nextSessionBlockIndex = block.GetSessionBlockIndex();
						if( (sessionBlockIndex+1) != nextSessionBlockIndex )
						{
							//////////////////////////////////////////////////////////////////
							// Dump the entire list of block index's and sessionblock index's for this range
							//////////////////////////////////////////////////////////////////
							replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Marker Start Time = %d, End Time = %d", marker.GetStartTime(), marker.GetEndTime() );
							replayDebugf1("[REPLAYBUFFERMARKER] - Startblock is %u, EndBlock is %u...", startBlock.GetMemoryBlockIndex(), endBlock.GetMemoryBlockIndex());
							CReplayMgr::ForeachBlock(printBlock);
							Assertf(false, "ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - Session Block Index's for marker aren't contiguous! Block Data Above");
							//////////////////////////////////////////////////////////////////
						}

 						sessionBlockIndex = nextSessionBlockIndex;
					} while( block.IsValid() && block != endBlock);
				}
			}
#endif	//__ASSERT

			ASSERT_ONLY(u16 currentBlockIndex = startBlock.GetMemoryBlockIndex();)

			// Is this the first marker to be saved, if so, don't check we're starting in a block already marked for saving
			bool shouldWriteMarker = false;
			if( firstMarker )
			{
				// Always write the first marker
				shouldWriteMarker = true;
				firstMarker = false;
			}
			else
			{
				// Check the start block index is already marked for saving.
				shouldWriteMarker = CReplayMgr::IsBlockMarkedForSave(startBlock);
			}

			if( shouldWriteMarker )
			{
				CBlockProxy block = startBlock;
				if(!CReplayMgr::IsBlockMarkedForSave(block))
				{
					BANK_ONLY(totalMarkerBlockCount++;)
					Verifyf( CReplayMgr::SaveBlock(block), "ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - CReplayMgr::SaveBlock(%d) just failed! 1", currentBlockIndex);	// assert that this is successful or something has gone wrong

					replayAssertf(block.GetStatus() == REPLAYBLOCKSTATUS_FULL || block == endBlock, "Incorrectly saving a block that isn't full and isn't the end of the clip");
				}

				while(block != endBlock)
				{
					block = block.GetNextBlock();
					if(!CReplayMgr::IsBlockMarkedForSave(block))
					{
						BANK_ONLY(totalMarkerBlockCount++;)
						Verifyf( CReplayMgr::SaveBlock(block), "ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - CReplayMgr::SaveBlock(%d) just failed! 2", currentBlockIndex);	// assert that this is successful or something has gone wrong

						replayAssertf(block.GetStatus() == REPLAYBLOCKSTATUS_FULL || block == endBlock, "Incorrectly saving a block that isn't full and isn't the end of the clip");
					}
				}

				// Update importance
				if(marker.GetImportance() > importance)
				{
					importance = marker.GetImportance();
				}

				// Set these in the marker now, since they're required by the montage markup (I think).
				marker.SetStartBlockIndex(startBlock.GetMemoryBlockIndex());
				marker.SetEndBlockIndex(endBlock.GetMemoryBlockIndex());

				// Push into the savelist, assert if that's full, 'cos it should never be!
				if( Verifyf( sm_SavedMarkers.GetCount() < REPLAY_HEADER_EVENT_MARKER_MAX, "ReplayBufferMarkerMgr::MarkBlocksForSave() - ERROR - sm_SavedMarkers - Array is full!" ) )
				{
					sm_SavedMarkers.Push(marker);
				}
				sm_ProcessedMarkers.Delete(markerIndex);

				replayDebugf1("[REPLAYBUFFERMARKER] - Marker has been flagged to save so remove the processed marker");
			}
			else
			{
				// Since we process these in time order, we're not going to find any more contiguous ones after this
				allConsecutiveFound = true;
			}
		}
		else
		{
			// No more markers, all must be consecutive
			allConsecutiveFound = true;
		}
	} while( !allConsecutiveFound );

	BANK_ONLY(sm_DebugInfo.SetMaxBlockSize(totalMarkerBlockCount);)

	return CReplayMgr::GetBlocksToSave().GetCount() > 0;
}


bool ReplayBufferMarkerMgr::SaveMarkers()
{
	u8 importance = 0;
	if( MarkBlocksForSave(importance) )
	{
		MarkerBufferInfo info;
		info.callback = OnSaveCallback;
		info.importance = importance;
		info.mission = 0; // TODO: handle mission

		u32 clipStartTime = 0, clipEndTime = 0;

		const atArray<CBlockProxy>& savedBlocks = CReplayMgrInternal::GetBlocksToSave();

		if (savedBlocks.GetCount() > 0)
		{
			CBlockProxy lastBlock = savedBlocks[savedBlocks.GetCount()-1];

			clipStartTime = savedBlocks[0].GetStartTime();
			clipEndTime = lastBlock.GetStartTime() + lastBlock.GetTimeSpan();

			replayDebugf1("[REPLAYBUFFERMARKER] - First block: %u, last block: %u", savedBlocks[0].GetSessionBlockIndex(), lastBlock.GetSessionBlockIndex());

/*
#if RSG_ASSERT
			bool notInOrder = false;
			int prevSessionIndex = -1;

			for (int i = 0; i < (int)savedBlocks.GetCount(); i++)
			{
				const CBlockInfo* block = savedBlocks[i];
				if (!replayVerifyf(block->GetStartTime() >= clipStartTime, "First block was not the earliest."))
				{
					clipStartTime = block->GetStartTime();

					notInOrder = true;
				}
				if (!replayVerifyf(block->GetStartTime() + block->GetTimeSpan() <= clipEndTime, "Last block was not the latest."))
				{
					clipEndTime = block->GetStartTime() + block->GetTimeSpan();

					notInOrder = true;
				}

				if( prevSessionIndex != -1 && prevSessionIndex+1 != block->GetSessionBlockIndex() )
				{
					replayAssertf(false, "Session block index was not in sequence. Please contact Jay Walton");
				}

				prevSessionIndex = block->GetSessionBlockIndex();
			}

			replayDebugf2("REPLAY BLOCK SAVE INFO - START");

			for (int i = 0; i < (int)savedBlocks.GetCount(); i++)
			{
				const CBlockInfo* block = savedBlocks[i];

				replayDebugf2("Block: %i Session Index: %u Start Time: %u End Time: %u", block->GetMemoryBlockIndex(), block->GetSessionBlockIndex(), block->GetStartTime(), block->GetStartTime() + block->GetTimeSpan());
			}

			replayDebugf2("REPLAY BLOCK SAVE INFO - END");
#endif
*/
		}

		replayDebugf1("[REPLAYBUFFERMARKER] - Clip start time: %7u", clipStartTime);
		replayDebugf1("[REPLAYBUFFERMARKER] - Clip end time: %7u", clipEndTime);

		for(int i = 0; i < sm_SavedMarkers.GetCount(); ++i)
		{
			u32 markerStart = sm_SavedMarkers[i].GetStartTime();
			u32 markerEnd = sm_SavedMarkers[i].GetEndTime();

			if (!(replayVerifyf(markerStart >= clipStartTime && markerStart < clipEndTime, "[REPLAYBUFFERMARKER] - Marker start time (%u) must be within clip start and end (%7u -> %7u)!", markerStart, clipStartTime, clipEndTime)))
			{
				markerStart = Clamp(markerStart, clipStartTime, clipEndTime);
				sm_SavedMarkers[i].SetStartTime(markerStart);
			}
			if (!(replayVerifyf(markerEnd > clipStartTime && markerEnd <= clipEndTime, "[REPLAYBUFFERMARKER] - Marker end time (%u) must be within clip start and end (%7u -> %7u)!", markerEnd, clipStartTime, clipEndTime)))
			{
				markerEnd = Clamp(markerEnd, clipStartTime, clipEndTime);
				sm_SavedMarkers[i].SetEndTime(markerEnd);
			}
			
			sm_SavedMarkers[i].SetClipStartTime(clipStartTime);
		}


		info.eventMarkers = sm_SavedMarkers;

		CReplayMgr::SaveMarkedUpClip(info);

		sm_Saving = true;


		return true;
	}
	else
	{
		replayDebugf1("[REPLAYBUFFERMARKER] - Marker failed to find any blocks to save out so no clip will be saved!!!!");
	}

	return false;
}

void ReplayBufferMarkerMgr::Flush()
{
	replayDebugf1("[REPLAYBUFFERMARKER] - Trigger flush of markers");
	sm_StartFlush = true;
}

void ReplayBufferMarkerMgr::FlushInternal()
{
	if( sm_StartFlush )
	{
		//if we have no more markers that could possibly be saved just break out.
		if( sm_ProcessedMarkers.GetCount() == 0 && sm_Markers.GetCount() == 0 )
		{
			sm_StartFlush = false;

			replayDebugf1("[REPLAYBUFFERMARKER] - Flush has finished, No markers to process");
			return;
		}

		// If we can't save ATM, exit, we'll check again next frame
		if( !CReplayMgr::CanSaveClip() )
		{
			replayDebugf1("[REPLAYBUFFERMARKER] - CReplayMgr::CanSaveClip() is false, we can't currently save so we'll just wait.");
			return;
		}

		// See if we have any markers to save
		if( SaveMarkers() )
		{
			replayDebugf1("[REPLAYBUFFERMARKER] - Found some markers, start the save");

			// Yes, signal to start the save.
			sm_Flush = true;
		}
		else
		{
			replayDebugf1("[REPLAYBUFFERMARKER] - Not found any markers to save this frame!");
		}
	}
}

bool ReplayBufferMarkerMgr::OnSaveCallback(int)
{
	//clear out previously saved markers
	sm_SavedMarkers.clear();
	sm_Saving = false;
	sm_Flush = false;

	replayDebugf1("[REPLAYBUFFERMARKER] - Save has finished, reset save flags");
	return true;
}

bool ReplayBufferMarkerMgr::CheckForMarkerOverlap(u32 msInThePast, u32 msInTheFuture, MarkerOverlap markerOverlap)
{
	u32 currentTime = 0;
	if(CReplayMgr::GetCurrentBlock().IsValid())
		currentTime = CReplayMgr::GetCurrentBlock().GetEndTime();

	u32 markerStart = currentTime - msInThePast;
	u32 markerEnd = currentTime + msInTheFuture;

	for(int i = sm_Markers.GetCount()-1; i >= 0; --i)
	{
		ReplayBufferMarker &marker = sm_Markers[i];

		if(marker.GetMarkerOverlap() == markerOverlap)
		{
			if(markerStart <= marker.GetEndTime() && markerEnd >= marker.GetStartTime())
			{
				return true;
			}
		}
	}

	return false;
}

// Called to advance the start time for markers added when events aren't enabled, or suppression is on.
void ReplayBufferMarkerMgr::ClipUnprocessedMarkerStartTimesToCurrentEndTime(bool includeManualRecordings)
{
	u32 currentTime = 0;
	if(CReplayMgr::GetCurrentBlock().IsValid())
		currentTime = CReplayMgr::GetCurrentBlock().GetEndTime();

	for(int i = sm_Markers.GetCount()-1; i >= 0; --i)
	{
		ReplayBufferMarker &marker = sm_Markers[i];

		bool bSkip = false;
		if( marker.GetSource() == CODE_MANUAL && !includeManualRecordings )
		{
			bSkip = true;
		}

		if(!bSkip)
		{
			//check for any end time that overlaps the cut off point
			if( currentTime > marker.GetStartTime())
			{
				marker.SetStartTime(currentTime);

				//if the length is < 3s after clamping then remove it.
				if( marker.GetLength() < MINIMUM_CLIP_LENGTH )
				{
					replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::ClipUnprocessedMarkerStartTimesToCurrentEndTime() - Clip is too short, deleting!");
					sm_Markers.Delete(i);
				}
			}

		}
	}
}

// Call to clip any end times to the current time.
bool ReplayBufferMarkerMgr::ClipUnprocessedMarkerEndTimesToCurrentEndTime(bool includeManualRecordings)
{
	bool clipped = false;

	u32 currentTime = 0;
	if(CReplayMgr::GetCurrentBlock().IsValid())
		currentTime = CReplayMgr::GetCurrentBlock().GetEndTime();

	for(int i = sm_Markers.GetCount()-1; i >= 0; --i)
	{
		ReplayBufferMarker &marker = sm_Markers[i];

		bool bSkip = false;
		if( marker.GetSource() == CODE_MANUAL && !includeManualRecordings )
		{
			bSkip = true;
		}

		if(!bSkip)
		{
			//check for any end time that overlaps the cut off point
			if( marker.GetEndTime() > currentTime )
			{
				marker.SetEndTime(currentTime);

				//if the length is < 3s after clamping then remove it.
				if( marker.GetLength() < MINIMUM_CLIP_LENGTH )
				{
					replayDebugf1("[REPLAYBUFFERMARKER] - ReplayBufferMarkerMgr::ClipUnprocessedMarkerEndTimesToCurrentEndTime() - Clip is too short, deleting!");
					sm_Markers.Delete(i);
					clipped = true;
				}
			}

		}
	}

	return clipped;
}

void ReplayBufferMarkerMgr::DeleteMissionMarkerClips()
{
	replayAssertf(sm_MissionMarkerState == MISSION_MARKER_STATE_IDLE, "Marker is not idle when attempt to delete old clips.");

	sm_MissionMarkerState = MISSION_MARKER_STATE_ENUMERATE;
}

void ReplayBufferMarkerMgr::UpdateMissionMarkerClips()
{
	switch(sm_MissionMarkerState)
	{
	case MISSION_MARKER_STATE_IDLE:
		{
			break;
		}
	case MISSION_MARKER_STATE_ENUMERATE:
		{
			atString name = atString(CReplayMgr::GetMissionName());
			name.Lowercase();

			if( !ReplayFileManager::StartEnumerateClipFiles(sm_DataStorage, name) )
			{
				sm_MissionMarkerState = MISSION_MARKER_STATE_IDLE;
				break;
			}

			sm_MissionMarkerState = MISSION_MARKER_STATE_CHECK_ENUMERATE;
			break;
		}
	case MISSION_MARKER_STATE_CHECK_ENUMERATE:
		{		
			bool result = false;
			if( ReplayFileManager::CheckEnumerateClipFiles(result) )
			{
				if( sm_DataStorage.GetCount() > 0 )
				{
					sm_MissionMarkerState = MISSION_MARKER_STATE_GET_MISSION_INDEX;
				}
				else
				{
					sm_MissionMarkerState = MISSION_MARKER_STATE_IDLE;
				}
			}

			break;
		}
	case MISSION_MARKER_STATE_GET_MISSION_INDEX:
		{
			int missionOverride = -1;
			for( int i = 0; i < sm_DataStorage.GetCount(); ++i )
			{
				atString filename = atString(sm_DataStorage[i].getFilename());
				int startindex = filename.LastIndexOf("_");
				int endindex = filename.LastIndexOf(".");
				if(startindex != -1 && endindex != -1)
				{
					atString missionIndex;
					missionIndex.Set(filename.c_str(), filename.length(), startindex + 1, endindex - startindex - 1);
					if(missionIndex.length() > 0)
					{
						missionOverride = atoi(missionIndex);
						break;
					}
				}
			}

			ReplayRecordingScriptInterface::SetCurrentMissionOverride(missionOverride);

			if(missionOverride != -1)
			{
				ReplayRecordingScriptInterface::SetMissionIndex(missionOverride);
			}

			sm_MissionMarkerState = MISSION_MARKER_STATE_DELETE;

			break;
		}
	case MISSION_MARKER_STATE_DELETE:
		{
			if( sm_DataStorage.GetCount() == 0 )
			{
				sm_MissionMarkerState = MISSION_MARKER_STATE_IDLE;
				break;
			}

			CFileData& fileData = sm_DataStorage[0];
	
			// REPLAY_CLIPS_PATH change
			char directory[REPLAYPATHLENGTH];
			ReplayFileManager::getClipDirectory(directory);
			char path[REPLAYPATHLENGTH];
			sprintf_s(path, REPLAYPATHLENGTH, "%s%s", directory, fileData.getFilename());

			sm_DataStorage.Delete(0);

			if( !ReplayFileManager::StartDeleteClip(path) )
			{
				sm_MissionMarkerState = MISSION_MARKER_STATE_IDLE;
			}

			sm_MissionMarkerState = MISSION_MARKER_STATE_CHECK_DELETE;
			
			break;
		}
	case MISSION_MARKER_STATE_CHECK_DELETE:
		{
			bool result = false;
			if( ReplayFileManager::CheckDeleteClip(result) )
			{
				//deleted all the clips, change to idle state.
				if( sm_DataStorage.GetCount() == 0 )
				{
					sm_MissionMarkerState = MISSION_MARKER_STATE_IDLE;
				}
				else
				{
					sm_MissionMarkerState = MISSION_MARKER_STATE_DELETE;
				}
			}
			break;
		}
	}
}

#if __BANK

#define REPLAY_MARKER_DEBUG_TIME_IN_ADVANCE 30000

void AddMarkerToLine(u32 startTime, u32 endTime, float fGraphCentreY, float rangeStart, float rangeEnd, Color32 colour)
{
	u32 currentTime = fwTimer::GetReplayTimeInMilliseconds();
	u32 blockStartTime = CReplayMgr::FindFirstBlock().GetStartTime();
	s32 diff = currentTime - blockStartTime + REPLAY_MARKER_DEBUG_TIME_IN_ADVANCE;
	float startPoint = (((float)startTime - (float)blockStartTime) / (float)diff);
	float length = rangeEnd - rangeStart;

	grcDebugDraw::Line(Vector2(rangeStart + (length * startPoint), fGraphCentreY), Vector2(rangeStart + (length * startPoint), fGraphCentreY - 0.01f), colour);
	
	if(endTime != 0 )
	{
		float endPoint = (((float)endTime - (float)blockStartTime) / (float)diff);
	
		grcDebugDraw::Line(Vector2(rangeStart + (length * endPoint), fGraphCentreY), Vector2(rangeStart + (length * endPoint), fGraphCentreY - 0.01f), colour);

		grcDebugDraw::Line(Vector2(rangeStart + (length * startPoint), fGraphCentreY- 0.005f), Vector2(rangeStart + (length * endPoint), fGraphCentreY- 0.005f), colour);
	}
}

struct drawBlockInfo
{
	float graphCentreY;
	Vector2 rangeStart;
	Vector2 rangeEnd;
	float blockWidth;
	float height;
	Color32 colour;
};

int DrawBlockBoundry(CBlockProxy block, void* pParam)
{
	drawBlockInfo* info = (drawBlockInfo*)pParam;
	AddMarkerToLine(block.GetStartTime(), 0, info->graphCentreY, info->rangeStart.x, info->rangeEnd.x, info->colour);
	return 0;
}


int DrawBlock(CBlockProxy block, void* pParam)
{
	drawBlockInfo* info = (drawBlockInfo*)pParam;

	if( block.IsValid() )
	{
		Vector2 start,end;
		start.x = info->rangeStart.x+(block.GetMemoryBlockIndex()*info->blockWidth);
		start.y = info->rangeStart.y - info->height;
		end.x = start.x + info->blockWidth;
		end.y = info->rangeEnd.y;

		Color32	theColour;
		switch(block.GetStatus())
		{
		case REPLAYBLOCKSTATUS_BEINGFILLED:
			theColour = block.IsTempBlock() ? Color_DarkGreen : Color_green;
			break;
		case REPLAYBLOCKSTATUS_FULL:
			theColour = block.IsTempBlock() ? Color_brown : Color_red;
			break;
		case REPLAYBLOCKSTATUS_SAVING:
			theColour = Color_blue;
			break;
		case	REPLAYBLOCKSTATUS_EMPTY:
		default:
			if( block.IsTempBlock() )
				theColour = Color_grey;
			else
				theColour = Color_black;
		}
		grcDebugDraw::RectAxisAligned(start, end, theColour, true);

		if(block.IsHeadBlock())
		{
			grcDebugDraw::Text(Vec2V(start.x, end.y), Color32(0.0f, 0.8f, 0.0f), "Head", false, 1.2f);
		}
	}

	return 0;
}

void ReplayBufferMarkerMgr::DrawDebug(bool drawBlocks)
{
#if DEBUG_DRAW
	if(drawBlocks)
	{

		//draw the tag graph for this clipName:
		static float fMarkerGraphXOffset = 0.0f;
		static float fMarkerGraphWidth = 0.3f;
		static float fMarkerGraphHeight = 0.007f;
		static float fMarkerGraphIncrement = -0.01f;

		static float fMarkerGraphXpos = 0.53f;
		static float fMarkerGraphYpos = 0.9f;

		float fGraphCentreX = fMarkerGraphXpos + fMarkerGraphXOffset;
		float fGraphCentreY = fMarkerGraphYpos + fMarkerGraphHeight / 2.0f;

		Vector2 vRangeStart(fGraphCentreX - fMarkerGraphWidth, fGraphCentreY);
		Vector2 vRangeEnd(fGraphCentreX + fMarkerGraphWidth, fGraphCentreY);
		float height = (sm_ProcessedMarkers.GetCount() + MAX(sm_Markers.GetCount(), 1 )) * fabs(fMarkerGraphIncrement);

		int	blockCount = CReplayMgr::GetNumberOfReplayBlocks() + NUMBER_OF_TEMP_REPLAY_BLOCKS;
		float blockWidth = (vRangeEnd.x - vRangeStart.x) / blockCount;

		drawBlockInfo drawInf;
		drawInf.rangeStart = vRangeStart;
		drawInf.rangeEnd = vRangeEnd;
		drawInf.height = height;
		drawInf.blockWidth = blockWidth;
		CReplayMgr::ForeachBlock(&DrawBlock, &drawInf);
	}
	else
	{
		//draw the tag graph for this clipName:
		static float fMarkerGraphXOffset = 0.0f;
		static float fMarkerGraphWidth = 0.3f;
		static float fMarkerGraphHeight = 0.007f;
		static float fMarkerGraphIncrement = -0.01f;

		static float fMarkerGraphXpos = 0.53f;
		static float fMarkerGraphYpos = 0.9f;

		float fGraphCentreX = fMarkerGraphXpos + fMarkerGraphXOffset;
		float fGraphCentreY = fMarkerGraphYpos + fMarkerGraphHeight / 2.0f;

		Vector2 vRangeStart(fGraphCentreX - fMarkerGraphWidth, fGraphCentreY);
		Vector2 vRangeEnd(fGraphCentreX + fMarkerGraphWidth, fGraphCentreY);

		float height = (sm_ProcessedMarkers.GetCount() + MAX(sm_Markers.GetCount(), 1 )) * fabs(fMarkerGraphIncrement);

		grcDebugDraw::RectAxisAligned(Vector2(vRangeStart.x, vRangeStart.y - height), vRangeEnd, Color32(Vector4(0.1f, 0.1f, 0.1f, 0.75f)), true);

		// draw the range
		grcDebugDraw::Line(vRangeStart, vRangeEnd, Color_white);

		// draw the current phase marker
		AddMarkerToLine(fwTimer::GetReplayTimeInMilliseconds(), 0, fGraphCentreY, vRangeStart.x, vRangeEnd.x, Color_red);

		// draw the block boundaries
		drawBlockInfo drawInf;
		drawInf.graphCentreY = fGraphCentreY;
		drawInf.rangeStart = vRangeStart;
		drawInf.rangeEnd = vRangeEnd;
		drawInf.colour = Color_white;
		CReplayMgr::ForeachBlock(DrawBlockBoundry, &drawInf);

		// draw the start marker
		AddMarkerToLine(CReplayMgr::FindFirstBlock().GetStartTime(), 0, fGraphCentreY, vRangeStart.x, vRangeEnd.x, Color_white);

		// draw the end marker
		AddMarkerToLine(CReplayMgr::GetCurrentBlock().GetEndTime() + REPLAY_MARKER_DEBUG_TIME_IN_ADVANCE, 0, fGraphCentreY, vRangeStart.x, vRangeEnd.x, Color_white);

		// draw the start time of a current script recording
		if( ReplayRecordingScriptInterface::HasStartedRecording() )
		{
			AddMarkerToLine(ReplayRecordingScriptInterface::GetStartTime(), CReplayMgr::GetCurrentBlock().GetEndTime(), fGraphCentreY, vRangeStart.x, vRangeEnd.x, Color_VioletRed);
		}

		if(sm_ProcessedMarkers.GetCount() > 0)
		{
			// draw the tag positions
			for(int i = 0; i < sm_ProcessedMarkers.GetCount(); ++i)
			{
				ReplayBufferMarker &marker = sm_ProcessedMarkers[i];

				Color32 colour = Color_DarkOrange;
				if(marker.GetSource() == CODE)
				{
					colour = Color_DarkGreen;
				}

				AddMarkerToLine(marker.GetStartTime(), marker.GetEndTime(), fGraphCentreY, vRangeStart.x, vRangeEnd.x, colour);
				fGraphCentreY +=  fMarkerGraphIncrement;
			}
		}

		if(sm_Markers.GetCount() > 0)
		{
			// draw the tag positions
			for(int i = 0; i < sm_Markers.GetCount(); ++i)
			{
				ReplayBufferMarker &marker = sm_Markers[i];

				Color32 colour = Color_OrangeRed;
				if(marker.GetSource() == CODE)
				{
					colour = Color_green;
				}

				AddMarkerToLine(marker.GetStartTime(), marker.GetEndTime(), fGraphCentreY, vRangeStart.x, vRangeEnd.x, colour);
				fGraphCentreY +=  fMarkerGraphIncrement;
			}
		}
	}
#endif // DEBUG_DRAW
}

#endif // __BANK

#endif //GTA_REPLAY
