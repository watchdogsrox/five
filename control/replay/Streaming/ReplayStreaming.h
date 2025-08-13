// #ifndef REPLAY_STREAMING_H
// #define REPLAY_STREAMING_H
// 
// #include "control/replay/replaysettings.h"
// 
// #if GTA_REPLAY
// 
// //rage
// #include "atl/queue.h"
// #include "atl/string.h"
// #include "system/ipc.h"
// 
// //game
// #include "control/replay/replay_channel.h"
// #include "control/replay/ReplaySupportClasses.h"
// 
// typedef void* FileHandle;
// 
// enum BlockStreamingState
// {
// 	BLOCK_UNMODIFIED = 0,
// 
// 	BLOCK_SAVING,
// 	BLOCK_SAVING_FINISHED,
// 
// 	BLOCK_LOADING,
// 	BLOCK_LOADING_FINISHED,
// };
// 
// struct ReplayBlockTOCEntry
// {
// 	ReplayBlockTOCEntry()
// 	{
// 		m_MasterIndex = 0;
// 		m_DataIndex = 0;
// 		m_StartTimeMS = 0;
// 		m_EndTimeMS = 0;
// 	}
// 
// 	s32	m_MasterIndex;
// 	u32	m_DataIndex;
// 	u32 m_StartTimeMS;
// 	u32 m_EndTimeMS;
// };
// 
// struct StreamRequest
// {
// 	StreamRequest()
// 	: m_MasterBlockIndex(0)
// 	, m_PhysicalBlockIndex(0)
// 	, mp_Data(0)
// 	, m_InsertAtStart(false)
// 	{
// 	}
// 
// 	StreamRequest(u16 masterBlockIndex, u16 physicalBlockIndex, const u8* pData, bool insertAtStart = false)
// 	: m_MasterBlockIndex(masterBlockIndex)
// 	, m_PhysicalBlockIndex(physicalBlockIndex)
// 	, mp_Data(pData)
// 	, m_InsertAtStart(insertAtStart)
// 	{
// 	}
// 
// 	u16 m_MasterBlockIndex;
// 	u16 m_PhysicalBlockIndex;
// 	const u8* mp_Data;
// 	bool m_InsertAtStart;
// };
// 
// //////////////////////////////////////////////////////////////////////////
// class OnBlockLoadFunctor
// {
// public:
// 	typedef void(*tFunc)(const u16 masterBlockIndex, const u16 physicalBlockIndex,  bool insertAtStart);
// 
// 	OnBlockLoadFunctor(tFunc func)
// 	: m_Function(func)
// 	{}
// 
// 	__declspec(noinline) void Execute(const u16 masterBlockIndex, const u16 physicalBlockIndex, bool insertAtStart)
// 	{
// 		(*(m_Function))(masterBlockIndex, physicalBlockIndex, insertAtStart);
// 	}
// 
// private:
// 	tFunc m_Function;
// };
// 
// //////////////////////////////////////////////////////////////////////////
// 
// class ReplayStreaming
// {
// public:
// 	enum eStreamingState
// 	{
// 		STREAMING_DISABLED = 0,
// 		STREAMING_RECORD,
// 		STREAMING_PLAYBACK,
// 	};
// 
// 	enum eStreamingErrors
// 	{
// 		STREAMING_NO_ERROR = 0,
// 		STREAMING_SAVE_ERROR,
// 		STREAMING_LOAD_ERROR,
// 	};
// 
// 	static const  u8 Q_SIZE = 4;
// 	static const u16 UNUSED_STREAM_SLOT = 0xFFFF;
// 
// 	ReplayStreaming();
// 	~ReplayStreaming();
// 
// 	static void						SetBufferSize(const u16 numberOfBlocks, const u32 sizeOfBlock);
// 
// 	//streaming out
// 	static bool						StartStreamingOut(const atString& filename, void* fileStore);
// 
// 	static void						RequestBlockSave(const u16 masterBlockIndex, const u16 physicalBlockIndex, const u8* dataSource);
// 	static bool						IsBlockBeingSaved(const u16 physicalBlockIndex);
// 	static void						WaitUntilBlockSaveComplete(const u16 physicalBlockIndex);
// 
// 	static bool						HasReachedFileSizeLimit(u32 limit);
// 
// 	//streaming in
// 	static bool						StartStreamingIn(const atString& filename, void* fileStore);
// 
// 	static void						RequestBlockLoad(const u16 masterBlockIndex, const u8 physicalBlockIndex, const u8* pData, bool insertAtStart, bool blockingLoad);
// 	static bool						IsBlockBeingLoaded(const u8 physicalBlockIndex);
// 	static void						WaitUntilBlockLoadComplete(const u8 physicalBlockIndex);
// 
// 	static bool						IsBlockCurrentlyLoaded(const u16 masterBlockIndex);
// 
// 	static u16						GetTotalNumberOfBlocks() { return sm_TotalNumberOfBlocks; }
// 
// 	static void						SetOnBlockLoadFunction(OnBlockLoadFunctor* callback) { ClearOnBlockLoadFunction(); sm_OnBlockLoadCallback = callback; }
// 	static void						ClearOnBlockLoadFunction() { if(sm_OnBlockLoadCallback) delete sm_OnBlockLoadCallback; sm_OnBlockLoadCallback = NULL; }
// 
// 	//general
// 	static void						ProcessStreaming();
// 	static eStreamingState			GetCurrentState() { return sm_CurrentState; }
// 	static eStreamingErrors			GetError() { return sm_LastError; }
// 	static void						ResetStreaming();
// 
// private:
// 	static bool						OpenToc(const atString& filename, bool readOnly);
// 	static bool						OpenData(const atString& filename,  bool readOnly);
// 	static bool						WriteHeader(u16 numberOfBlocks);
// 	static void						CloseFiles();
// 
// 	static void						StreamBlockOut(const StreamRequest& request);
// 	static void						StreamBlockIn(const StreamRequest& request);
// 	
// 	static eStreamingState			sm_CurrentState;
// 	static eStreamingErrors			sm_LastError;
// 
// 	static BlockStreamingState		sm_BlockStreamingStates[MAX_NUMBER_OF_ALLOCATED_BLOCKS];
// 	static u16						sm_NumberOfBlocks;
// 	static u32						sm_SizeOfBlocks;
// 
// 	static atQueue<StreamRequest, Q_SIZE> sm_StreamRequests;
// 
// 	static OnBlockLoadFunctor*		sm_OnBlockLoadCallback;
// 
// 	static ReplayBlockTOCEntry*		sm_TOCEntries;
// 
// 	static u16						sm_CurrentBlockStreamedIn[MAX_NUMBER_OF_ALLOCATED_BLOCKS];
// 	static u16						sm_TotalNumberOfBlocks;
// 
// 	static void*				sm_FileStore;
// 	static void*			sm_TOCFileHandle;
// 	static void*			sm_DataFileHandle;
// };
// 
// #endif // GTA_REPLAY
// 
// #endif // REPLAY_STREAMING_H
