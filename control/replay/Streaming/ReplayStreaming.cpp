// #include "ReplayStreaming.h"
// 
// #if GTA_REPLAY
// 
// ReplayStreaming::eStreamingState				ReplayStreaming::sm_CurrentState = ReplayStreaming::STREAMING_DISABLED;
// ReplayStreaming::eStreamingErrors				ReplayStreaming::sm_LastError = ReplayStreaming::STREAMING_NO_ERROR;
// 
// BlockStreamingState								ReplayStreaming::sm_BlockStreamingStates[MAX_NUMBER_OF_ALLOCATED_BLOCKS];
// u16												ReplayStreaming::sm_NumberOfBlocks = 0;
// u32												ReplayStreaming::sm_SizeOfBlocks = 0;
// 
// atQueue<StreamRequest, ReplayStreaming::Q_SIZE>	ReplayStreaming::sm_StreamRequests;
// 
// OnBlockLoadFunctor*								ReplayStreaming::sm_OnBlockLoadCallback = NULL;
// 
// ReplayBlockTOCEntry*							ReplayStreaming::sm_TOCEntries = NULL;
// 
// u16												ReplayStreaming::sm_CurrentBlockStreamedIn[MAX_NUMBER_OF_ALLOCATED_BLOCKS];
// u16												ReplayStreaming::sm_TotalNumberOfBlocks = 0;
// 
// void*											ReplayStreaming::sm_FileStore = NULL;
// void*											ReplayStreaming::sm_TOCFileHandle = NULL;
// void*											ReplayStreaming::sm_DataFileHandle = NULL;
// 
// ReplayStreaming::ReplayStreaming()
// {
// }
// 
// ReplayStreaming::~ReplayStreaming()
// {
// }
// 
// void ReplayStreaming::SetBufferSize(const u16 numberOfBlocks, const u32 sizeOfBlocks)
// {
// 	sm_NumberOfBlocks = numberOfBlocks;
// 	sm_SizeOfBlocks = sizeOfBlocks;
// 
// 	ResetStreaming();
// }
// 
// bool ReplayStreaming::StartStreamingOut(const atString& filename, void* fileStore)
// {
// 	ResetStreaming();
// 
// 	//This gets cleared in ResetStreaming(), needs to be called after
// 	sm_FileStore = fileStore;
// 	replayAssertf(sm_FileStore, "sm_FileStore is NULL!\n");
// 
// 	if(OpenToc(filename, false) == false || OpenData(filename, false) == false)
// 	{
// 		replayAssertf(false, "Can't open files for streaming out\n");
// 		CloseFiles();
// 		return false;
// 	}
// 
// 	//write a dummy header
// 	if(WriteHeader(0))
// 	{
// 		sm_CurrentState = STREAMING_RECORD;
// 	}
// 	else
// 	{
// 		ResetStreaming();
// 		return false;
// 	}
// 
// 	return true;
// }
// 
// void ReplayStreaming::RequestBlockSave(const u16 masterBlockIndex, const u16 physicalBlockIndex, const u8* dataSource)
// {
// 	if(sm_CurrentState != STREAMING_RECORD)
// 	{
// 		return;
// 	}
// 
// 	replayAssertf(dataSource, "Data source is NULL");
// 	replayAssertf(ReplayStreaming::sm_DataFileHandle != NULL, "Tried to save a block without any setup!");
// 	replayAssertf(sm_BlockStreamingStates[physicalBlockIndex] != BLOCK_SAVING, "Tried to save a block that is already saving!");
// 
// 	sm_BlockStreamingStates[physicalBlockIndex] = BLOCK_SAVING;
// 
// 	replayDebugf2("RPSRM Requesting stream out of master block %i, physical block %i, data %p", masterBlockIndex, physicalBlockIndex, dataSource);
// 	sm_StreamRequests.Push(StreamRequest(masterBlockIndex, physicalBlockIndex, dataSource)); 
// }
// 
// bool ReplayStreaming::IsBlockBeingSaved(const u16 physicalBlockIndex)
// {
// 	return sm_BlockStreamingStates[physicalBlockIndex] == BLOCK_SAVING;
// }
// 
// void ReplayStreaming::WaitUntilBlockSaveComplete(const u16 physicalBlockIndex)
// {
// 	//only wait for block that have been requested
// 	if(sm_BlockStreamingStates[physicalBlockIndex] == BLOCK_UNMODIFIED)
// 	{
// 		return;
// 	}
// 
// 	while(ReplayStreaming::IsBlockBeingSaved(physicalBlockIndex))
// 	{
// 		replayDebugf2("Waiting for block %i to stream out", physicalBlockIndex);
// 		sysIpcSleep(50);
// 	}
// }
// 
// bool ReplayStreaming::HasReachedFileSizeLimit(u32 /*limit*/)
// {
// 	//u32 fileSize = sm_TOCFileHandle->GetSize() + sm_DataFileHandle->GetSize() + sm_SizeOfBlocks;
// 	return true;//fileSize >= limit;
// }
// 
// bool ReplayStreaming::StartStreamingIn(const atString& filename, void* fileStore)
// {
// 	ResetStreaming();
// 
// 	//This gets cleared in ResetStreaming(), needs to be called after
// 	sm_FileStore = fileStore;
// 	replayAssertf(sm_FileStore, "sm_FileStore is NULL!\n");
// 
// 	//load the TOC file
// 	if(OpenToc(filename, true) == false)
// 	{
// 		CloseFiles();
// 		replayAssertf(false, "Can't open TOC file %s, for streaming in", filename.c_str());
// 		return false;
// 	}
// 
// 	ReplayHeader header;
// 
// 	//read the replay header
// 	//sm_TOCFileHandle->Read((char*)&header, sizeof(ReplayHeader));
// 	replayAssertf(header.PhysicalBlockCount > 0, "Block count not saved correctly!");
// 
// 	sm_TotalNumberOfBlocks = header.TotalNumberOfBlocks;
// 
// 	//read the toc entries
// 	sm_TOCEntries = rage_new ReplayBlockTOCEntry[sm_TotalNumberOfBlocks];
// 	//sm_TOCFileHandle->Read((char*)sm_TOCEntries, sizeof(ReplayBlockTOCEntry) * sm_TotalNumberOfBlocks);
// 
// 	CloseFiles();
// 
// 	//load the data file
// 	if(OpenData(filename, true) == false)
// 	{
// 		CloseFiles();
// 		replayAssertf(false, "Can't open Data file %s, for streaming in", filename.c_str());
// 		return false;
// 	}
// 
// 	sm_CurrentState = STREAMING_PLAYBACK;
// 
// 	return true;
// }
// 
// void ReplayStreaming::RequestBlockLoad(const u16 masterBlockIndex, const u8 physicalBlockIndex, const u8* pData, bool insertAtStart, bool blockingLoad)
// {
// 	if(sm_CurrentState != STREAMING_PLAYBACK)
// 	{
// 		return;
// 	}
// 
// 	replayAssertf(pData, "Data destination is NULL");
// 	replayAssertf(masterBlockIndex < sm_TotalNumberOfBlocks, "Reqested a block to be streamed in which didn't exsist %i, max %i", masterBlockIndex, sm_TotalNumberOfBlocks);
// 	replayAssertf(ReplayStreaming::sm_DataFileHandle != NULL, "Tried to load a block without any setup!");
// 	replayAssertf(sm_BlockStreamingStates[physicalBlockIndex] != BLOCK_LOADING, "Tried to load a block that is already loading!");
// 
// 	//STREAMSEEKFIX will have to have correct timing data
// 
// 	sm_BlockStreamingStates[physicalBlockIndex] = BLOCK_LOADING;
// 
// 	if(blockingLoad)
// 	{
// 		StreamBlockIn(StreamRequest(masterBlockIndex, physicalBlockIndex, pData)); 
// 	}
// 	else
// 	{
// 		replayDebugf2("RPSRM Requesting stream in of master block %i, physical block %i, data %p inserAtStart %i", masterBlockIndex, physicalBlockIndex, pData, insertAtStart);
// 		sm_StreamRequests.Push(StreamRequest(masterBlockIndex, physicalBlockIndex, pData, insertAtStart)); 
// 	}
// }
// 
// bool ReplayStreaming::IsBlockBeingLoaded(const u8 physicalBlockIndex)
// {
// 	return sm_BlockStreamingStates[physicalBlockIndex] == BLOCK_LOADING;
// }
// 
// void ReplayStreaming::WaitUntilBlockLoadComplete(const u8 physicalBlockIndex)
// {
// 	replayAssertf(sm_BlockStreamingStates[physicalBlockIndex] != BLOCK_UNMODIFIED, "Streaming waiting for a block that hasn't been requested!");
// 
// 	while(ReplayStreaming::IsBlockBeingLoaded(physicalBlockIndex))
// 	{
// 		replayDebugf2("Waiting for block %i to stream in", physicalBlockIndex);
// 		sysIpcSleep(250);
// 	}
// }
// 
// bool ReplayStreaming::IsBlockCurrentlyLoaded(const u16 masterBlockIndex)
// {
// 	for(u16 i = 0; i < MAX_NUMBER_OF_ALLOCATED_BLOCKS; ++i)
// 	{
// 		if(sm_CurrentBlockStreamedIn[i] == masterBlockIndex)
// 		{
// 			return true;
// 		}
// 	}
// 
// 	return false;
// }
// 
// void ReplayStreaming::ProcessStreaming()
// {
// 	while(sm_StreamRequests.IsEmpty() == false)
// 	{
// 		StreamRequest request = sm_StreamRequests.Pop();
// 		
// 		if(sm_CurrentState == STREAMING_RECORD)
// 		{
// 			StreamBlockOut(request);
// 		}
// 		else if(sm_CurrentState == STREAMING_PLAYBACK)
// 		{
// 			StreamBlockIn(request);
// 
// 			//inform the main replay system that this block is loaded and can be linked up
// 			replayAssertf(sm_OnBlockLoadCallback, "No OnBlockLoadCallback set!");
// 			sm_OnBlockLoadCallback->Execute(request.m_MasterBlockIndex, request.m_PhysicalBlockIndex, request.m_InsertAtStart);
// 		}
// 	}
// }
// 
// void ReplayStreaming::ResetStreaming()
// {	
// 	if(sm_TOCEntries != NULL)
// 	{
// 		delete [] sm_TOCEntries;
// 		sm_TOCEntries = NULL;
// 	}
// 
// 	CloseFiles();
// 
// 	for (int i = 0; i < MAX_NUMBER_OF_ALLOCATED_BLOCKS; ++i)
// 	{
// 		sm_BlockStreamingStates[i] = BLOCK_UNMODIFIED;
// 		sm_CurrentBlockStreamedIn[i] = UNUSED_STREAM_SLOT;
// 	}
// 
// 	sm_TotalNumberOfBlocks = 0;
// 
// 	sm_StreamRequests.Reset();
// 
// 	sm_CurrentState = STREAMING_DISABLED;
// 	sm_LastError = STREAMING_NO_ERROR;
// }
// 
// //////////////////////////////////////////////////////////////////////////
// 
// bool ReplayStreaming::OpenToc(const atString& /*filename*/, bool /*readOnly*/)
// {
// 	/*atString TOCfilename = filename;
// 	TOCfilename += ".rtoc";
// 
// 	FileStoreOperation op = readOnly ? READ_ONLY_OP : CREATE_OP;
// 	/ *sm_TOCFileHandle = sm_FileStore->Open(TOCfilename, op);* /
// 
// 	if(sm_TOCFileHandle)
// 	{
// 		return true;
// 	}
// 
// 	replayAssertf(false, "Failed to load streaming toc file");
// 	sm_TOCFileHandle = NULL;*/
// 
// 	return false;
// }
// 
// bool ReplayStreaming::OpenData(const atString& /*filename*/, bool /*readOnly*/)
// {
// 	/*atString datafilename = filename;
// 	datafilename += ".rdata";
// 
// 	FileStoreOperation op = readOnly ? READ_ONLY_OP : CREATE_OP;
// 	sm_DataFileHandle = sm_FileStore->Open(datafilename, op);
// 
// 	if(sm_DataFileHandle)
// 	{
// 		return true;
// 	}
// 
// 	replayAssertf(false, "Failed to load streaming data file");
// 	sm_DataFileHandle = NULL;*/
// 
// 	return false;
// }
// 
// bool ReplayStreaming::WriteHeader(u16 numberOfBlocks)
// {
// 	//always place this at the top of the TOC  file
// 	//sm_TOCFileHandle->Seek(0);
// 
// 	//start the toc with the normal header info
// 	ReplayHeader replayHeader;
// 	replayHeader.Reset();
// 
// 	replayHeader.PhysicalBlockSize	= sm_SizeOfBlocks;
// 	replayHeader.PhysicalBlockCount =  sm_NumberOfBlocks;
//  	replayHeader.TotalNumberOfBlocks = numberOfBlocks;
// 	replayHeader.fClipTime	= -1; //STREAMSEEKFIX this will have to be setup correctly
// 
// 	atString dateTimeStr;
// 	ReplayHeader::GenerateDateString(dateTimeStr);
// 	strcpy_s(replayHeader.creationDateTime, REPLAY_DATETIME_STRING_LENGTH, dateTimeStr.c_str());
// 
// 	return false;//sm_TOCFileHandle->Write((char*)&(replayHeader), sizeof(ReplayHeader)) > 0 || sm_TOCFileHandle->Flush() > 0;
// }
// 
// void ReplayStreaming::CloseFiles()
// {
// 	if( sm_TOCFileHandle != NULL )
// 	{
// 		/*sm_TOCFileHandle->Flush();
// 		sm_TOCFileHandle->Close();*/
// 		sm_TOCFileHandle = NULL;
// 	}
// 
// 	if( sm_DataFileHandle != NULL )
// 	{
// 		/*sm_DataFileHandle->Flush();
// 		sm_DataFileHandle->Close();*/
// 		sm_DataFileHandle = NULL;
// 	}
// 
// }
// 
// void ReplayStreaming::StreamBlockOut(const StreamRequest& request)
// {
// 	if(sm_CurrentState != STREAMING_RECORD)
// 	{
// 		return;
// 	}
// 
// 	ReplayBlockTOCEntry TOCEntry;
// 	TOCEntry.m_MasterIndex = request.m_MasterBlockIndex;
// 	TOCEntry.m_DataIndex = 0;//sm_DataFileHandle->Tell();
// 
// 	s32 dataBytesWritten = 0;//sm_DataFileHandle->Write((char*)request.mp_Data, sm_SizeOfBlocks);
// 	s32 dataBytesFlushed = 0;//sm_DataFileHandle->Flush();
// 
// 	if(dataBytesWritten <= 0 && dataBytesFlushed <= 0)
// 	{
// 		ResetStreaming();
// 		sm_LastError = STREAMING_SAVE_ERROR;
// 		return;
// 	}
// 
// 	s32 tocBytesWriten = 0;//sm_TOCFileHandle->Write((char*)&TOCEntry, sizeof(TOCEntry));
// 	s32 tocBytesFlushed = 0;//sm_TOCFileHandle->Flush();
// 
// 	if(tocBytesWriten <= 0 && tocBytesFlushed <= 0)
// 	{
// 		ResetStreaming();
// 		sm_LastError = STREAMING_SAVE_ERROR;
// 		return;
// 	}
// 
// 	//update the header with the latest, as this at the beginning we have to seek back to the current position
// 	//s32 currentFilePos = 0;//sm_TOCFileHandle->Tell();
// 
// 	if(WriteHeader(request.m_MasterBlockIndex + 1) == false)
// 	{
// 		ResetStreaming();
// 		sm_LastError = STREAMING_SAVE_ERROR;
// 		return;
// 	}
// 
// 	//sm_TOCFileHandle->Seek(currentFilePos);
// 
// 	sm_BlockStreamingStates[request.m_PhysicalBlockIndex] = BLOCK_SAVING_FINISHED;
// 	replayDebugf2("RPSRM Finished stream out of master block %i, physical block %i, data %p", request.m_MasterBlockIndex, request.m_PhysicalBlockIndex, request.mp_Data);
// }
// 
// void ReplayStreaming::StreamBlockIn(const StreamRequest& request)
// {
// 	if(sm_CurrentState != STREAMING_PLAYBACK)
// 	{
// 		return;
// 	}
// 
// 	/*ReplayBlockTOCEntry tocEntry = sm_TOCEntries[request.m_MasterBlockIndex];*/
// 	
// 	/*if(sm_DataFileHandle->Seek(tocEntry.m_DataIndex) < 0)
// 	{
// 		ResetStreaming();
// 		replayFatalAssertf(false, "Error seeking to %i, this is past the end of the file at %i, TOC wasn't written correctly", tocEntry.m_DataIndex, sm_DataFileHandle->GetSize());
// 	}*/
// 
// 	/*if(sm_DataFileHandle->Read((char*)request.mp_Data, sm_SizeOfBlocks) <= 0)
// 	{
// 		ResetStreaming();
// 		sm_LastError = STREAMING_LOAD_ERROR;
// 	}*/
// 
// 	sm_CurrentBlockStreamedIn[request.m_PhysicalBlockIndex] = request.m_MasterBlockIndex;
// 
// 	sm_BlockStreamingStates[request.m_PhysicalBlockIndex] = BLOCK_LOADING_FINISHED;
// 	replayDebugf2("RPSRM Finished stream in of master block %i, physical block %i, data %p", request.m_MasterBlockIndex, request.m_PhysicalBlockIndex, request.mp_Data);
// }
// 
// #endif //GTA_REPLAY