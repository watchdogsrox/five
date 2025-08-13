
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    record.cpp
// PURPOSE : Record the input and elapse of time so that we can accurately
//			 play back the game.
//			 This stuff was originally used for the start of GTAIII (the jail break). It was changed
//			 for SA to allow for single cars to be recorded and played back easily by the level designers.
// AUTHOR :  Obbe.
// CREATED : 28/2/00
//
/////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "phbullet/SimdTransformUtil.h"
#include "streaming/streamingmodule.h"
#include "system/timer.h"

// Framework Headers
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/vector.h"
#include "fwsys/fileExts.h"

// Game headers
#include "control/gamelogic.h"
#include "control/record.h"
#include "control/replay/replay.h"
#include "core/game.h"
#include "debug/DebugScene.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/Task/TaskVehicleFollowRecording.h"
#include "core/game.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debug.h"
#include "game/modelIndices.h"
#include "game/clock.h"
#include "modelInfo/vehiclemodelinfo.h"
#include "peds/ped.h"
#include "peds/pedpopulation.h"
#include "peds/pedintelligence.h"
#include "physics/physics.h"
#include "phcore/phmath.h"
#include "scene/world/gameWorld.h"
#include "system/fileMgr.h"
#include "system/pad.h"
#include "vehicles/bike.h"
#include "vehicles/train.h"
#include "vehicles/trailer.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

#define DEFAULT_RECORDING_PATH	"assets:/recordings/car/"

#if __BANK
PF_PAGE(VehicleRecordingPage, "Vehicle Recording");

PF_GROUP(VehicleRecordingMetrics);
PF_LINK(VehicleRecordingPage, VehicleRecordingMetrics);

PF_VALUE_FLOAT(vehicleEffectiveSpeed, VehicleRecordingMetrics);
PF_VALUE_FLOAT(vehicleActualSpeed, VehicleRecordingMetrics);

static Vector3 g_VehiclePositionOnPreviousUpdate = VEC3_MAX;
bkButton* CVehicleRecordingMgr::ms_pCreateAudioWidgetsButton = NULL;
#endif // __BANK

#if !__FINAL
static const float	g_MinVehicleSpeedToFixUp					= 1.0f;
static const u32	g_MaxVehicleTimeStampFixUpInMilliseconds	= 33;	//One physics time-slice at 15fps.
static const u32	g_NumStatesToFixUpBeforePinningTimeStamp	= 20;

CVehicleRecordingMgr::CRecording CVehicleRecordingMgr::ms_vehiclesRecordings[MAX_CARS_RECORDED_NUMBER];
#endif

Vec3V	CVehicleRecordingMgr::PlaybackAdditionalRotation[MAX_CARS_PLAYBACK_NUMBER];
Vec3V	CVehicleRecordingMgr::PlaybackPreviousAdditionalRotation[MAX_CARS_RECORDED_NUMBER];
Vec3V		CVehicleRecordingMgr::PlaybackLocalPositionOffset[MAX_CARS_PLAYBACK_NUMBER];
Vec3V		CVehicleRecordingMgr::PlaybackGlobalPositionOffset[MAX_CARS_PLAYBACK_NUMBER];
RegdVeh		CVehicleRecordingMgr::pVehicleForPlayback[MAX_CARS_PLAYBACK_NUMBER];
u8			*CVehicleRecordingMgr::pPlaybackBuffer[MAX_CARS_PLAYBACK_NUMBER];
s32			CVehicleRecordingMgr::PlaybackIndex[MAX_CARS_PLAYBACK_NUMBER];
s32			CVehicleRecordingMgr::PlaybackBufferSize[MAX_CARS_PLAYBACK_NUMBER];
float		CVehicleRecordingMgr::PlaybackRunningTime[MAX_CARS_PLAYBACK_NUMBER];
float		CVehicleRecordingMgr::PlaybackSpeed[MAX_CARS_PLAYBACK_NUMBER];
bool		CVehicleRecordingMgr::bPlaybackGoingOn[MAX_CARS_PLAYBACK_NUMBER];
bool		CVehicleRecordingMgr::bPlaybackLooped[MAX_CARS_PLAYBACK_NUMBER];
bool		CVehicleRecordingMgr::bPlaybackPaused[MAX_CARS_PLAYBACK_NUMBER];
bool		CVehicleRecordingMgr::bUseCarAI[MAX_CARS_PLAYBACK_NUMBER];
s32			CVehicleRecordingMgr::aiVechicleRecordingFlags[MAX_CARS_PLAYBACK_NUMBER];
u32			CVehicleRecordingMgr::aiDelayUntilRevertingBack[MAX_CARS_PLAYBACK_NUMBER];
u32			CVehicleRecordingMgr::TurnBackIntoFullPlayBackTime[MAX_CARS_PLAYBACK_NUMBER];	// Do we want to turn this back into a full playback and if so; when?
u32			CVehicleRecordingMgr::DisplayMode[MAX_CARS_PLAYBACK_NUMBER];
s32			CVehicleRecordingMgr::PlayBackStreamingIndex[MAX_CARS_PLAYBACK_NUMBER];

#define SECONDSBEFOREREMOVAL (20)
#define HIGHESTFILENUMALLOWED (4000)
// File numbers are split up as follows:
// 0 - 1999	   Game (IV)
// 2000 - 2999 mp1 (bikers)
// 3000 - 3999 mp2



#if __BANK
#if !__FINAL
static bool		debug_WidgetRecordingPlaying = false;
static RegdVeh	debug_WidgetRecordingPlaybackVehicle;
static u8 *		debug_pDebugBuffer = NULL;
static int		debug_bufferSize;
static float	debug_timeScale = 1.0f;
static Vector3	debug_AdditionalRotationEulers = VEC3_ZERO;
static Vec3V	debug_LocalPositionOffset = Vec3V(V_ZERO);
static Vec3V	debug_GlobalPositionOffset = Vec3V(V_ZERO);
int		CVehicleRecordingMgr::ms_debug_RecordingNumToPlayWith = 0;
char	CVehicleRecordingMgr::ms_debug_RecordingNameToPlayWith[CAR_RECORDING_NAME_LENGTH] = "";
rage::bkGroup*	CVehicleRecordingMgr::sm_WidgetGroup(NULL);
#endif
#endif


// The stuff needed to stream the recordings in.
CVehicleRecordingStreaming*	CVehicleRecordingMgr::sm_StreamingArray;
s32 CVehicleRecordingMgr::sm_NumPlayBackFiles = 0;


#if __BANK
#if !__FINAL

static	bool	bShiftRecording = false;
static	bool	bFixRecordingTimeStamps = false;
static	float	ShiftX = 0.0f;
static	float	ShiftY = 0.0f;
static	float	ShiftZ = 0.0f;
static	s32	RecordingNumberToBeShifted = -1;
static	char	RecordingNameToBeShifted[CAR_RECORDING_NAME_LENGTH] = "";

static	bool	bRenderAllRecordings = false;

static	float	fShiftPlaybackUpValue = 0.0f;
#endif
#endif

//map to keep track of what filename hashes correspond to what streamingarray indices
static fwNameRegistrar sm_RecordInfoMap;

// PURPOSE:	Replacement for MAX_NUM_OF_PLAYBACK_FILES.
static int s_MaxNumOfPlaybackFiles = 0;

bool CVehicleRecordingMgr::sm_bUpdateWithPhysics = true;
bool CVehicleRecordingMgr::sm_bUpdateBeforePreSimUpdate = false;

static inline int GetMaxNumOfPlaybackFiles()
{
	// Make sure we don't use it until it's been set (by InitMaxNumOfPlaybackFiles()).
	TrapZ(s_MaxNumOfPlaybackFiles);

	return s_MaxNumOfPlaybackFiles;
}

static int InitMaxNumOfPlaybackFiles()
{
	const int numPlaybackFiles = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("carrec", 0x0c3824168), CONFIGURED_FROM_FILE);
	Assert(s_MaxNumOfPlaybackFiles == 0 || s_MaxNumOfPlaybackFiles == numPlaybackFiles);	// Not expected to change during the game, for now at least.
	s_MaxNumOfPlaybackFiles = numPlaybackFiles;

	// The size of sm_RecordInfoMap used to be set by its constructor, but now
	// when we don't know the size at compile time, we do it here.
	if(!sm_RecordInfoMap.IsInitialized())
	{
		sm_RecordInfoMap.Init(numPlaybackFiles);
	}

	return numPlaybackFiles;
}


PARAM(vehrecordpath, "[record] the absolute path to which car recording .rrr files will be saved");

//
// name:		CVehicleRecordingStreamingModule
// description:	Class for registering vehicle recordings with streaming system
class CVehicleRecordingStreamingModule : public strStreamingModule
{
public:
	CVehicleRecordingStreamingModule() : strStreamingModule("carrec",
			VEHICLERECORDING_FILE_ID,
			InitMaxNumOfPlaybackFiles(),	// This sets s_MaxNumOfPlaybackFiles, if needed.
			false, false, CVehicleRecording::RORC_VERSION, 512) {}

#if !__FINAL
	virtual const char* GetName(strLocalIndex index) const;
#endif
	virtual strLocalIndex Register(const char* name);
	virtual strLocalIndex FindSlot(const char* name) const {return strLocalIndex(CVehicleRecordingMgr::FindRecordingFile(name));}

	virtual void Remove(strLocalIndex index) {CVehicleRecordingMgr::Remove(index.Get());}
	virtual void RemoveSlot(strLocalIndex index);
	virtual void PlaceResource(strLocalIndex index, datResourceMap& map, datResourceInfo& header) {CVehicleRecordingMgr::Place(index.Get(), map, header);}
	virtual void* GetPtr(strLocalIndex index) {return CVehicleRecordingMgr::GetPtr(index.Get());}

	virtual void AddRef(strLocalIndex index, strRefKind /*refKind*/) { CVehicleRecordingMgr::AddRef(index); }
	virtual void RemoveRef(strLocalIndex index, strRefKind /*refKind*/) {CVehicleRecordingMgr::RemoveRef(index); }
	virtual int GetNumRefs(strLocalIndex index) const {return CVehicleRecordingMgr::GetNumRefs(index); }
	virtual int GetDependencies(strLocalIndex UNUSED_PARAM(index), strIndex *UNUSED_PARAM(pIndices), int UNUSED_PARAM(indexArraySize)) const {return 0;} 

};

// --- CVehicleRecordingStreamingModule ---------------------------------------------------------------------

strLocalIndex CVehicleRecordingStreamingModule::Register(const char* name)
{
	strLocalIndex result = strLocalIndex(CVehicleRecordingMgr::RegisterRecordingFile(name));

#if USE_PAGED_POOLS_FOR_STREAMING
	AllocateSlot(result);
#endif // USE_PAGED_POOLS_FOR_STREAMING

	return result;
}

void CVehicleRecordingStreamingModule::RemoveSlot(strLocalIndex index)
{
	CVehicleRecordingMgr::RemoveSlot(index.Get());

#if USE_PAGED_POOLS_FOR_STREAMING
	FreeObject(index);
#endif // USE_PAGED_POOLS_FOR_STREAMING
}

#if !__FINAL
const char* CVehicleRecordingStreamingModule::GetName(strLocalIndex index) const
{
	static char name[32];
	formatf(name, "%s", CVehicleRecordingMgr::GetRecordingName(index.Get()));
	return name;
}
#endif


// --- CVehicleRecordingStreaming ---------------------------------------------------------------------

void CVehicleRecordingStreaming::InitCore()
{
	HashKey.Clear();
	AudioHashKey.Clear();

	pRecordingData = NULL;
	RecordingDataSize = 0;
	m_NumTimesUsed = 0;
	pRecordingRsc = NULL;
}

void CVehicleRecordingStreaming::RegisterRecordingFile(const char *pFilename)
{
	HashKey = pFilename;
	pRecordingData = NULL;
	pRecordingRsc = NULL;
}

void CVehicleRecordingStreaming::Place(CVehicleRecording *pR)
{
	pRecordingRsc = pR;
	pRecordingData = (u8 *) pR->GetStates();
	RecordingDataSize = pR->GetNumStates() * sizeof(CVehicleStateEachFrame);
}

void CVehicleRecordingStreaming::StartPlayback(u8 **ppPlaybackBuffer, s32 &PlaybackBufferSize)
{
	*ppPlaybackBuffer = pRecordingData;
	Assertf(pRecordingData, "CVehicleRecordingStreaming::StartPlayback - Recording is played without being streamed in");
	PlaybackBufferSize = RecordingDataSize;
	m_NumTimesUsed++;
}

void CVehicleRecordingStreaming::Remove()
{
	Assert(pRecordingData);

	// delete data
	delete pRecordingRsc;
	pRecordingRsc = NULL;
	pRecordingData = NULL;
	RecordingDataSize = 0;

	Assertf(m_NumTimesUsed == 0, "CVehicleRecordingStreaming::Remove - m_NumTimesUsed = %d for %s so we're just going to set it to 0 now", m_NumTimesUsed, GetRecordingName());
	m_NumTimesUsed = 0;	//	Is this going to work or will I end up deleting a recording from memory while a vehicle is still reading it?
}

void CVehicleRecordingStreaming::AddRef()
{
	m_NumTimesUsed++;
}

void CVehicleRecordingStreaming::RemoveRef()
{
	Assertf(m_NumTimesUsed > 0, "CVehicleRecordingStreaming::RemoveRef - expected m_NumTimesUsed to be greater than 0 but it's %d", m_NumTimesUsed);
	m_NumTimesUsed--;
}

int CVehicleRecordingStreaming::GetNumRefs()
{
	return m_NumTimesUsed;
}

void CVehicleRecordingStreaming::GetPositionOfCarRecordingAtTime(float time, Vector3 &RetVal)
{
	Assertf(pRecordingData, "GET_POSITION_OF_CAR_RECORDING_AT_TIME Recording not streamed in?");

	s32	IndexInRecording = 0;
	s32 IndexToUse = 0;
	while ( (IndexInRecording < RecordingDataSize) && ((CVehicleStateEachFrame *) &(pRecordingData[IndexInRecording]))->TimeInRecording < time)
	{
		IndexToUse = IndexInRecording;
		IndexInRecording += sizeof(CVehicleStateEachFrame);
	}

	CVehicleRecordingMgr::RestoreInfoForPosition( RetVal, reinterpret_cast<CVehicleStateEachFrame *>(&(pRecordingData[IndexToUse])));
}

void CVehicleRecordingStreaming::GetTransformOfCarRecordingAtTime(float time, Matrix34 &RetVal)
{
	Assertf(pRecordingData, "GET_POSITION_OF_CAR_RECORDING_AT_TIME Recording not streamed in?");

	s32	IndexInRecording = 0;
	s32 IndexToUse = 0;
	while ( (IndexInRecording < RecordingDataSize) && ((CVehicleStateEachFrame *) &(pRecordingData[IndexInRecording]))->TimeInRecording < time)
	{
		IndexToUse = IndexInRecording;
		IndexInRecording += sizeof(CVehicleStateEachFrame);
	}

	CVehicleRecordingMgr::RestoreInfoForMatrix( RetVal, reinterpret_cast<CVehicleStateEachFrame *>(&(pRecordingData[IndexToUse])));
}

float CVehicleRecordingStreaming::GetTotalDurationOfCarRecording()
{
	Assertf(pRecordingData, "GET_TOTAL_DURATION_OF_CAR_RECORDING Recording not streamed in?");

	return (float)((CVehicleStateEachFrame *) &(pRecordingData[RecordingDataSize-sizeof(CVehicleStateEachFrame)] ))->TimeInRecording;
}

void CVehicleRecordingStreaming::Invalidate()
{
	Assert(!pRecordingData);
	HashKey.Clear();
	AudioHashKey.Clear();
}

// --- CVehicleRecordingMgr::CRecording ----------------------------------------------------------

#if !__FINAL

void CVehicleRecordingMgr::CRecording::Init()
{
	m_pVehicle = NULL;
	m_pRecordingBuffer = NULL;
	m_isRecording = false;
}

bool CVehicleRecordingMgr::CRecording::StartRecording(CVehicle* pVehicle, int fileNumber, const char* pRecordingName)
{
	Assert(!m_pRecordingBuffer);


	m_pVehicle = pVehicle;
	m_fileNumber = fileNumber;
	m_fileName = pRecordingName;

	m_flushToDisk = true;
	m_fileHandle = OpenRecordingFile(fileNumber, pRecordingName);
	Assertf(CFileMgr::IsValidFileHandle(m_fileHandle), "failed to open or create recording file, is file checked out?");
	if(CFileMgr::IsValidFileHandle(m_fileHandle) == false)
		return false;
	m_pRecordingBuffer = rage_new u8 [CAR_RECORDING_FLUSH_BUFFER_SIZE];
	Assert(m_pRecordingBuffer);

	m_recordingIndex = 0;
	m_startTime = 0;
	m_lastFrameRecordedTime = 0;
	m_lastFrameFlushedTime = 0;

	m_isFirstFrame = true;
	m_isRecording = true;
	//DisplayMode = RDM_NONE;

	return true;
}

void CVehicleRecordingMgr::CRecording::StartRecordingTransitionFromPlayback(CVehicle* pVehicle, int fileNumber, const char* pRecordingName, int playbackSlot)
{
	Assert(!m_pRecordingBuffer);

	m_pVehicle = pVehicle;
	m_fileNumber = fileNumber;
	m_fileName = pRecordingName;
	m_pRecordingBuffer = rage_new u8 [CAR_RECORDING_BUFFER_SIZE];
	Assert(m_pRecordingBuffer);
	m_flushToDisk = false;
	m_isRecording = true;
	//DisplayMode = RDM_NONE;

	// Now copy the relevant part of the playback (data up until now) into the new recording
	m_startTime = fwTimer::GetTimeInMilliseconds() - (u32)PlaybackRunningTime[playbackSlot];

	int	t = 0;
	while (t < PlaybackIndex[playbackSlot])
	{
		m_pRecordingBuffer[t] = pPlaybackBuffer[playbackSlot][t];
		t++;
	}
	m_recordingIndex = PlaybackIndex[playbackSlot];

	m_isFirstFrame = (m_recordingIndex == 0);

	m_lastFrameRecordedTime = 0;  // This will force the next frame to be recorded (which is good)
	m_lastFrameFlushedTime = 0;
}

void CVehicleRecordingMgr::CRecording::StopRecording(u8 **ppBuffer, int *pSize)
{
	Assert(m_pVehicle);
	Assert(m_pRecordingBuffer);

	if ( m_flushToDisk )
	{
		Assertf( ppBuffer == NULL || *ppBuffer == NULL,
			"This recording was flushed to disk and kicked out of memory. Cannot get the data buffer for it." );

		WriteBufferToFile(m_fileHandle, m_pRecordingBuffer, m_recordingIndex);

		CFileMgr::CloseFile( m_fileHandle );
		m_fileHandle = INVALID_FILE_HANDLE;
		delete [] m_pRecordingBuffer;
		m_pRecordingBuffer = NULL;
	}
	else
	{
		if (ppBuffer)		// This is debug. The user may want to rescue the buffer to playback straight away
		{
			*ppBuffer = m_pRecordingBuffer;
			*pSize = m_recordingIndex;
		}
		else
		{
			SaveToFile( m_pRecordingBuffer, m_recordingIndex, m_fileNumber, m_fileName);
			delete [] m_pRecordingBuffer;
		}
	}

	// Restore everything to the original state
	m_isRecording = false;
	m_pVehicle = NULL;
	m_pRecordingBuffer = NULL;
}

void CVehicleRecordingMgr::CRecording::SaveFrame()
{
	Assert(m_pVehicle);

	u32 timeInMilliseconds = fwTimer::GetTimeInMilliseconds();

	const s32 bufferSize = m_flushToDisk ? CAR_RECORDING_FLUSH_BUFFER_SIZE : CAR_RECORDING_BUFFER_SIZE;

	if (timeInMilliseconds - m_lastFrameRecordedTime > FRAMERECORDPERIOD || m_lastFrameRecordedTime == 0) //force a recording if this is the first frame
	{
		m_lastFrameRecordedTime = timeInMilliseconds;

		if (m_recordingIndex < bufferSize - static_cast<s32>(sizeof(CVehicleStateEachFrame)))
		{
			CVehicleStateEachFrame *pVehState = (CVehicleStateEachFrame *) &(m_pRecordingBuffer[m_recordingIndex]);

			// If this is the first frame of the recording we have to set the time correctly.
			if (m_isFirstFrame)
			{
				Assert( m_lastFrameFlushedTime == 0 );

				m_startTime = timeInMilliseconds;
				m_lastFrameFlushedTime = timeInMilliseconds;
				pVehState->TimeInRecording = 0;

				m_isFirstFrame = false;
			}
			else
			{
				pVehState->TimeInRecording = timeInMilliseconds - m_startTime;
			}

			StoreInfoForCar(m_pVehicle, pVehState);
			m_recordingIndex += sizeof(CVehicleStateEachFrame);
		}
		else
		{
			Displayf("Buffer full. No more carpositions are being recorded\n");

			StopRecordingCar();
		}
	}

	if ( m_flushToDisk && timeInMilliseconds - m_lastFrameFlushedTime > RECORDFLUSHPERIOD )
	{
		WriteBufferToFile( m_fileHandle, m_pRecordingBuffer, m_recordingIndex );
		m_recordingIndex = 0;
		m_lastFrameFlushedTime = timeInMilliseconds;
	}

#if __BANK && __STATS
	const phCollider* pCollider		= m_pVehicle->GetCollider();
	const Matrix34 vehicleMatrix	= MAT34V_TO_MATRIX34(pCollider ? pCollider->GetMatrix() : m_pVehicle->GetMatrix());

	if(!g_VehiclePositionOnPreviousUpdate.IsClose(VEC3_MAX, SMALL_FLOAT))
	{
#if __STATS
		const float effectiveSpeed = vehicleMatrix.d.Dist(g_VehiclePositionOnPreviousUpdate) * fwTimer::GetInvTimeStep();

		PF_SET(vehicleEffectiveSpeed, effectiveSpeed);

		const Vector3& velocity = m_pVehicle->GetVelocity();
		const float actualSpeed = velocity.Mag();

		PF_SET(vehicleActualSpeed, actualSpeed);
#endif // __STATS
	}

	g_VehiclePositionOnPreviousUpdate = vehicleMatrix.d;
#endif // __BANK
}

float CVehicleRecordingMgr::CRecording::FindTimePositionInRecording()
{
	return (float)(fwTimer::GetTimeInMilliseconds() - m_startTime);
}

void CVehicleRecordingMgr::CRecording::WriteBufferToFile(FileHandle fid, u8* buffer, s32 length)
{
	if ( Verifyf(CFileMgr::IsValidFileHandle(fid), "Vehicle recording file handle is invalid") )
	{
#if __BE
		EndianSwapRecording( buffer, length );
#endif

		ASSERT_ONLY(s32 numBytesWritten =) CFileMgr::Write( fid, (char*) buffer, length );
		Assert( numBytesWritten == length );

		// Swap endian back. This is only important when using the widgets to record, save and playback a recording. (The data should still be usable after the recording)
#if __BE
		EndianSwapRecording( buffer, length );
#endif
	}

}

void CVehicleRecordingMgr::CRecording::SaveToFile(u8 *pBuffer, int length, int fileNumber, const char* pRecordingName)
{
	FileHandle	fid = OpenRecordingFile( fileNumber, pRecordingName);
	WriteBufferToFile( fid, pBuffer, length );
	CFileMgr::CloseFile( fid );
}

FileHandle CVehicleRecordingMgr::CRecording::OpenRecordingFile(const s32 fileNumber, const char* pRecordingName)
{
	FileHandle	fid;
	char fileName[50];

	// Open a file and save out the data.
	const char *PathForCarRecordings = 0;
	if(PARAM_vehrecordpath.Get(PathForCarRecordings))
	{
		CFileMgr::SetDir(PathForCarRecordings);
	}
	else
	{
		CFileMgr::SetDir(DEFAULT_RECORDING_PATH);
	}

	sprintf(fileName, "%s%03d.ivr", pRecordingName, fileNumber);
	fid = CFileMgr::OpenFileForWriting(fileName);
	CFileMgr::SetDir("");

	return fid;
}

void CVehicleRecordingMgr::CRecording::LoadFromFile(u8 **ppBuffer, int *pLength, int fileNumber, const char* pRecordingName)
{
	FileHandle	fid;
	s32		NumBytes=0;
	char fileName[50];

	// Open a file and load the data.
	const char *PathForCarRecordings = 0;
	if(PARAM_vehrecordpath.Get(PathForCarRecordings))
	{
		CFileMgr::SetDir(PathForCarRecordings);
	}
	else
	{
		CFileMgr::SetDir(DEFAULT_RECORDING_PATH);
	}

	sprintf(fileName, "%s%03d.ivr", pRecordingName, fileNumber);
	fid = CFileMgr::OpenFile(fileName);
	if ( CFileMgr::IsValidFileHandle(fid) )
	{
		*ppBuffer = rage_new u8 [CAR_RECORDING_BUFFER_SIZE];

		if (*ppBuffer)
		{
			NumBytes = CFileMgr::Read(fid, (char *)*ppBuffer, 9999999);
			Assert(NumBytes > 0);

#if __BE
			EndianSwapRecording(*ppBuffer, NumBytes);
#endif
		}
		CFileMgr::CloseFile(fid);
	}
	CFileMgr::SetDir("");
	*pLength = NumBytes;
}

void CVehicleRecordingMgr::CRecording::EndianSwapRecording(u8 *pBuffer, s32 dataSize)
{
	Assert(pBuffer);

	s32	IndexInArray = 0;
	while (IndexInArray < dataSize)
	{
		CVehicleStateEachFrame	*pState = (CVehicleStateEachFrame *) &(pBuffer[IndexInArray]);
		pState->EndianSwap();
		IndexInArray += sizeof(CVehicleStateEachFrame);
	}
}

void CVehicleRecordingMgr::CRecording::ShiftRecording(s32 RecordingNumber, const char* pRecordingName, float ShiftByX, float ShiftByY, float ShiftByZ)
{
	FileHandle				fileIn;
	FileHandle				fileOut;
	char					FileNameIn[32];
	char					FileNameOut[32];
	CVehicleStateEachFrame	VehState;
#if __ASSERT
	s32					BytesLoaded;
#endif

	CFileMgr::SetDir("x:/gta5/assets/recordings/car");
	sprintf(FileNameIn, "%s%03d.ivr", pRecordingName, RecordingNumber);
	sprintf(FileNameOut, "%s%03d.ivrshifter", pRecordingName, RecordingNumber);
	fileIn = CFileMgr::OpenFile(FileNameIn);
	if (CFileMgr::IsValidFileHandle(fileIn))
	{	// File exists.
		fileOut = CFileMgr::OpenFileForWriting(FileNameOut);
		Assertf(CFileMgr::IsValidFileHandle(fileOut), "%s:Could not open file", FileNameOut);

		while( (ASSERT_ONLY(BytesLoaded =) CFileMgr::Read(fileIn, (char *)&VehState, sizeof(CVehicleStateEachFrame))) != 0)
		{
			Assert(BytesLoaded == sizeof(CVehicleStateEachFrame));
#if __BE
			EndianSwapRecording((u8 *)&VehState, sizeof(CVehicleStateEachFrame));
#endif

			VehState.CoorsX += ShiftByX;
			VehState.CoorsY += ShiftByY;
			VehState.CoorsZ += ShiftByZ;
#if __BE
			EndianSwapRecording((u8 *)&VehState, sizeof(CVehicleStateEachFrame));
#endif
			CFileMgr::Write(fileOut, (char *)&VehState, sizeof(CVehicleStateEachFrame));
		}

		CFileMgr::CloseFile(fileOut);
		CFileMgr::CloseFile(fileIn);
	}
	CFileMgr::SetDir("");
}

void CVehicleRecordingMgr::CRecording::FixRecordingTimeStamps(s32 RecordingNumber, const char* pRecordingName)
{
	FileHandle				fileIn;
	FileHandle				fileOut;
	char					FileNameIn[32];
	char					FileNameOut[32];

	CFileMgr::SetDir("x:/gta5/assets/recordings/car");
	sprintf(FileNameIn, "%s%03d.ivr", pRecordingName, RecordingNumber);
	sprintf(FileNameOut, "%s%03d.ivrfixed", pRecordingName, RecordingNumber);
	fileIn = CFileMgr::OpenFile(FileNameIn);
	if (CFileMgr::IsValidFileHandle(fileIn))
	{	// File exists.
		fileOut = CFileMgr::OpenFileForWriting(FileNameOut);
		Assertf(CFileMgr::IsValidFileHandle(fileOut), "%s:Could not open file", FileNameOut);

		CVehicleStateEachFrame vehicleStates[g_NumStatesToFixUpBeforePinningTimeStamp];
		s32 BytesLoaded;

		Vector3 previousPosition(VEC3_MAX);
		Vector3 previousVelocity(VEC3_MAX);
		u32 previousTimeInRecordingBeforeFixUp	= (u32)-1;
		u32 previousTimeInRecordingAfterFixedUp	= (u32)-1;

		const u32 numBytesToLoad = g_NumStatesToFixUpBeforePinningTimeStamp * sizeof(CVehicleStateEachFrame);

		while((BytesLoaded = CFileMgr::Read(fileIn, (char *)vehicleStates, numBytesToLoad)) != 0)
		{
#if __BE
			EndianSwapRecording((u8 *)vehicleStates, BytesLoaded);
#endif

			const u32 numStatesLoaded = Min((u32)BytesLoaded / (u32)sizeof(CVehicleStateEachFrame), g_NumStatesToFixUpBeforePinningTimeStamp);

			for(u32 stateIndex=0; stateIndex<numStatesLoaded; stateIndex++)
			{
				CVehicleStateEachFrame& currentState = vehicleStates[stateIndex];

				const u32 timeInRecording = currentState.TimeInRecording;
				const Vector3 position(currentState.GetCoors());
				const Vector3 velocity(currentState.GetSpeedX(), currentState.GetSpeedY(), currentState.GetSpeedZ());

				if(previousTimeInRecordingBeforeFixUp != (u32)-1)
				{
					Vector3 averageVelocity;
					averageVelocity.Lerp(0.5f, previousVelocity, velocity);

					const float averageSpeed = averageVelocity.Mag();
					if(averageSpeed >= g_MinVehicleSpeedToFixUp)
					{
						const float distanceTraveled	= position.Dist(previousPosition);
						const u32 desiredTimeDelta		= (u32)Floorf(distanceTraveled * 1000.0f / averageSpeed);
						const u32 currentTimeDelta		= timeInRecording - previousTimeInRecordingBeforeFixUp;
						//Ensure that we don't attempt an overzealous fix-up and that the time-stamps cannot reverse direction.
						const u32 minTimeDelta			= (u32)Max((s32)currentTimeDelta - (s32)g_MaxVehicleTimeStampFixUpInMilliseconds, 0);
						const u32 timeDeltaToApply		= Clamp(desiredTimeDelta, minTimeDelta, currentTimeDelta + g_MaxVehicleTimeStampFixUpInMilliseconds);

						const u32 desiredTimeInRecording = (previousTimeInRecordingAfterFixedUp + timeDeltaToApply);

						Displayf("Fix-up: %04d (Before limiting: %04d)", (s32)timeDeltaToApply - (s32)currentTimeDelta, (s32)desiredTimeDelta - (s32)currentTimeDelta);

						if(stateIndex == (g_NumStatesToFixUpBeforePinningTimeStamp - 1))
						{
							//This is the final state in the block, so we must pin the time-stamp and compensate for the error in the preceding states.
							const s32 totalErrorToDistribute = (s32)currentState.TimeInRecording - (s32)desiredTimeInRecording;

							Displayf("Pinning error: %04d", totalErrorToDistribute);

							for(u32 pinStateIndex=0; pinStateIndex<(g_NumStatesToFixUpBeforePinningTimeStamp - 1); pinStateIndex++)
							{
								const float errorRatioToApply = (float)(pinStateIndex + 1) / (float)g_NumStatesToFixUpBeforePinningTimeStamp;

								const s32 errorToApply = (s32)Floorf(errorRatioToApply * (float)totalErrorToDistribute);
								vehicleStates[pinStateIndex].TimeInRecording = (u32)Max((s32)vehicleStates[pinStateIndex].TimeInRecording + errorToApply, 0);
							}
						}
						else
						{
							currentState.TimeInRecording = desiredTimeInRecording;
						}
					}
				}

				previousPosition = position;
				previousVelocity = velocity;
				previousTimeInRecordingBeforeFixUp	= timeInRecording;
				previousTimeInRecordingAfterFixedUp	= currentState.TimeInRecording;
			}

#if __BE
			EndianSwapRecording((u8 *)vehicleStates, BytesLoaded);
#endif

			CFileMgr::Write(fileOut, (char *)vehicleStates, BytesLoaded);
		}

		CFileMgr::CloseFile(fileOut);
		CFileMgr::CloseFile(fileIn);
	}
	CFileMgr::SetDir("");
}

///////////////////////////////////////////////////////////////////////////
// NAME       : StoreInfoForMatrix()
// PURPOSE    : Compresses the matrix for a car
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::CRecording::StoreInfoForMatrix(const Matrix34 *pMatrix, CVehicleStateEachFrame *pCarState)
{
	pCarState->Matrix_a_x = (s8)(pMatrix->a.x * 127);
	pCarState->Matrix_a_y = (s8)(pMatrix->a.y * 127);
	pCarState->Matrix_a_z = (s8)(pMatrix->a.z * 127);
	pCarState->Matrix_b_x = (s8)(pMatrix->b.x * 127);
	pCarState->Matrix_b_y = (s8)(pMatrix->b.y * 127);
	pCarState->Matrix_b_z = (s8)(pMatrix->b.z * 127);
}

///////////////////////////////////////////////////////////////////////////
// NAME       : StoreInfoForCar()
// PURPOSE    : Compresses the matrix and stuff for a car to be played back
//				in an animation.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::CRecording::StoreInfoForCar(CVehicle *pCar, CVehicleStateEachFrame *pCarState)
{
	Matrix34 m = MAT34V_TO_MATRIX34(pCar->GetMatrix());
	StoreInfoForMatrix( &m, pCarState);

	const Vector3 vCarPosition = VEC3V_TO_VECTOR3(pCar->GetVehiclePosition());
	pCarState->CoorsX = vCarPosition.x;
	pCarState->CoorsY = vCarPosition.y;
	pCarState->CoorsZ = vCarPosition.z;

	pCarState->SetSpeedX(pCar->GetVelocity().x);
	pCarState->SetSpeedY(pCar->GetVelocity().y);
	pCarState->SetSpeedZ(pCar->GetVelocity().z);

	pCarState->SteerAngle = (s8)(pCar->m_vehControls.m_steerAngle * 20.0f);
	pCarState->Gas = (s8)(pCar->m_vehControls.m_throttle * 100.0f);
	pCarState->Brake = (s8)(pCar->m_vehControls.m_brake * 100.0f);
	pCarState->HandBrake = pCar->m_vehControls.m_handBrake;
}

#endif


// --- CVehicleRecordingMgr ---------------------------------------------------------------------

//
// name:		CVehicleRecordingMgr::InitialiseData
// description:	
void CVehicleRecordingMgr::InitialiseData()
{
	s32	C;

#if !__FINAL
	for (s32 Recording = 0; Recording < MAX_CARS_RECORDED_NUMBER; Recording++)
	{
		ms_vehiclesRecordings[Recording].Init();
	}
#endif

	for (C = 0; C < MAX_CARS_PLAYBACK_NUMBER; C++)
	{
		SetVehicleForPlayback(C, NULL);
		PlaybackAdditionalRotation[C] = Vec3V(V_ZERO);
		PlaybackPreviousAdditionalRotation[C] = Vec3V(V_ZERO);
		PlaybackLocalPositionOffset[C] = Vec3V(V_ZERO);
		PlaybackGlobalPositionOffset[C] = Vec3V(V_ZERO);
		pPlaybackBuffer[C] = NULL;
		bPlaybackGoingOn[C] = false;
		bPlaybackPaused[C] = false;
//		PlaybackForCodeIndex[C] = -1;
	}

	sm_NumPlayBackFiles = 0;//reset the number of playback files to 0 as they have all been cleared.
}

///////////////////////////////////////////////////////////////////////////
// NAME       : Init()
// PURPOSE    : Initialises this whole recording stuff. If we want to change
//				what happens in the game (record / playback / normal) we can
//				change it here.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_SCRIPT);
    if(initMode == INIT_CORE)
    {
		Assert(!sm_StreamingArray);
		sm_StreamingArray = rage_new CVehicleRecordingStreaming[GetMaxNumOfPlaybackFiles()];

	    for (s32 C = 0; C < MAX_CARS_PLAYBACK_NUMBER; C++)
	    {
		    pVehicleForPlayback[C] = NULL;
	    }
	    InitialiseData();

		const int cnt = GetMaxNumOfPlaybackFiles();
		for (s32 loop = 0; loop < cnt; loop++)
		{
			sm_StreamingArray[loop].InitCore();
		}
    }
	else if (initMode == INIT_SESSION)
	{
		if (!sm_RecordInfoMap.IsInitialized())
		{
			sm_RecordInfoMap.Init(GetMaxNumOfPlaybackFiles());
		}
	}
    else if (initMode == INIT_BEFORE_MAP_LOADED)
    {
        if (!sm_RecordInfoMap.IsInitialized())
        {
            sm_RecordInfoMap.Init(GetMaxNumOfPlaybackFiles());
        }
    }
}


///////////////////////////////////////////////////////////////////////////
// NAME       : GetStreamingModuleId()
// PURPOSE    :
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

strStreamingModule* CVehicleRecordingMgr::GetStreamingModule()
{
	// streaming module 
	static CVehicleRecordingStreamingModule s_VehicleRecordingModule;
	return &s_VehicleRecordingModule;
}


void* CVehicleRecordingMgr::GetPtr(s32 index)
{
	return sm_StreamingArray[index].GetRecordingRsc();
}

///////////////////////////////////////////////////////////////////////////
// NAME       : ShutdownLevel()
// PURPOSE    : Frees the bits of streaming memory we might have.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        sm_RecordInfoMap.Reset();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
		const int maxNumOfPlaybackFiles = GetMaxNumOfPlaybackFiles();
	    for (s32 C = 0; C < maxNumOfPlaybackFiles; C++)
	    {
			if (sm_StreamingArray[C].HasRecordingData())
			{
				CVehicleRecordingMgr::GetStreamingModule()->ClearRequiredFlag(C, STRFLAG_MISSION_REQUIRED);
				CVehicleRecordingMgr::GetStreamingModule()->StreamingRemove(strLocalIndex(C));
			}
			else
			{
				Assertf(GetNumRefs(strLocalIndex(C)) == 0, "CVehicleRecordingMgr::Shutdown - vehicle recording %s has no recording data but still has %d refs", sm_StreamingArray[C].GetRecordingName(), GetNumRefs(strLocalIndex(C)));
			}
	    }

		InitialiseData();
    }
	else if(shutdownMode == SHUTDOWN_CORE)
	{
		Assert(sm_StreamingArray);
		delete []sm_StreamingArray;
		sm_StreamingArray = NULL;
	}
}

//
// name:		CVehicleRecordingMgr::SetVehicleForPlayback
// description:	
void CVehicleRecordingMgr::SetVehicleForPlayback(int SlotNumber, CVehicle *pNewVehicle)
{
	Assertf( (SlotNumber >= 0) && (SlotNumber < MAX_CARS_PLAYBACK_NUMBER), "CVehicleRecordingMgr::SetVehicleForPlayback - SlotNumber is out of range");
	if ((SlotNumber >= 0) && (SlotNumber < MAX_CARS_PLAYBACK_NUMBER))
	{
		pVehicleForPlayback[SlotNumber] = pNewVehicle;
	}
}

#if !__FINAL

bool CVehicleRecordingMgr::DoesRecordingFileExist(const s32 fileNumber, const char* pRecordingName)
{
	char fileName[50];

	// Open a file and save out the data.
	const char *PathForCarRecordings = 0;
	if(PARAM_vehrecordpath.Get(PathForCarRecordings))
	{
		CFileMgr::SetDir(PathForCarRecordings);
	}
	else
	{
		CFileMgr::SetDir(DEFAULT_RECORDING_PATH);
	}

	sprintf(fileName, "%s%03d.ivr", pRecordingName, fileNumber);


	FileHandle	temp_file_handle = CFileMgr::OpenFileForAppending(fileName);
	CFileMgr::SetDir("");

	if(temp_file_handle != INVALID_FILE_HANDLE)
	{	
		Warningf("Recording file already exists %s", fileName);
		CFileMgr::CloseFile(temp_file_handle);
		return true;
	}
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : StartRecordingCar()
// PURPOSE 	  : Once this is called the code will record the position of this vehicle every
//				frame. A file is saved out when StopRecordingCar is called.
// RETURNS    :false if command fails
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

bool CVehicleRecordingMgr::StartRecordingCar(class CVehicle *pCar, u32 FileNum, const char* pRecordingName, bool allowOverwrite /*= false*/)
{
	s32	C, FreeRecording = -1;
	Assert(pCar);
	Assertf(FileNum < HIGHESTFILENUMALLOWED, "Recording number should be < 4000");

	if(allowOverwrite == false)//check whether the file already exits
	{
		if(CVehicleRecordingMgr::DoesRecordingFileExist(FileNum, pRecordingName))
		{
			return false;//file already exits so don't record
		}
	}

	for (C = 0; C < MAX_CARS_RECORDED_NUMBER; C++)
	{
		if (!ms_vehiclesRecordings[C].IsRecording())
		{
			FreeRecording = C; 
		}
	}
	
	Assert(FreeRecording >= 0);

	return ms_vehiclesRecordings[FreeRecording].StartRecording(pCar, FileNum, pRecordingName);
}

// This starts a recording on a vehicle that has a playback running on it.
// The data from the recording being played back is copied up until the moment
// this function is called.
// Can be used to playback a recording and switch to recording when a collision happens
// without loss of quality.

//returns false if command fails
bool CVehicleRecordingMgr::StartRecordingCarTransitionFromPlayback(class CVehicle *pCar, u32 FileNum, const char* pRecordingName, bool allowOverwrite /* = false*/)
{
	s32	C, FreeRecording = -1;
	Assert(pCar);
	Assertf(FileNum < HIGHESTFILENUMALLOWED, "Recording number should be < 4000");

	if(allowOverwrite == false)//check whether the file already exits
	{
		if(CVehicleRecordingMgr::DoesRecordingFileExist(FileNum, pRecordingName))
		{
			return false;//file already exits so dont record
		}
	}


	s32	playbackRecording;
	playbackRecording = 0;
	while (playbackRecording < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[playbackRecording]) || pVehicleForPlayback[playbackRecording] != pCar))
	{
		playbackRecording++;
	}

	Assertf(playbackRecording < MAX_CARS_PLAYBACK_NUMBER, "START_RECORDING_VEHICLE_TRANSITION_FROM_PLAYBACK. Vehicle must have a recording playing on it for this to work");
	if (playbackRecording >= MAX_CARS_PLAYBACK_NUMBER)
	{
		return false;
	}

		// First we need to start a new recording.
	for (C = 0; C < MAX_CARS_RECORDED_NUMBER; C++)
	{
		if (!ms_vehiclesRecordings[C].IsRecording())
		{
			FreeRecording = C;
		}
	}

	Assert(FreeRecording >= 0);

	ms_vehiclesRecordings[FreeRecording].StartRecordingTransitionFromPlayback(pCar, FileNum, pRecordingName, playbackRecording);
	// Now stop the playback of the recording as it's not needed anymore.
	StopPlaybackWithIndex(playbackRecording);

	return true;
}




///////////////////////////////////////////////////////////////////////////
// NAME       : StopRecordingCar()
// PURPOSE 	  : When this function is called we are done recording and save out a file
//				with the playback info for this car.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::StopRecordingCar(CVehicle *pVehToStopRecordingFor, u8 **ppBuffer, int *pSize)
{
	// Stop the recordings one by one
	for (int Recording = 0; Recording < MAX_CARS_RECORDED_NUMBER; Recording++)
	{
		if (ms_vehiclesRecordings[Recording].IsRecording())
		{
			if (pVehToStopRecordingFor == NULL || pVehToStopRecordingFor == ms_vehiclesRecordings[Recording].GetVehicle())
			{
				ms_vehiclesRecordings[Recording].StopRecording(ppBuffer, pSize);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : IsCarBeingRecorded(class CVehicle *pCar)
// PURPOSE 	  : Used by the level designers to find out if a car is being recorded.
// RETURNS    :	true/false
// PARAMETERS :	pointer to the car in question
///////////////////////////////////////////////////////////////////////////

bool CVehicleRecordingMgr::IsCarBeingRecorded(const CVehicle *pCar)
{
	s32	Recording;
	Assert(pCar);
	
	for (Recording = 0; Recording < MAX_CARS_RECORDED_NUMBER; Recording++)
	{
		if((ms_vehiclesRecordings[Recording].GetVehicle() == pCar) && ms_vehiclesRecordings[Recording].IsRecording())
		{
			return true;
		}
	}
	return false;
}

// bool CVehicleRecordingMgr::IsAnyCarBeingRecorded()
// {
// 	s32	Recording;
// 
// 	for (Recording = 0; Recording < MAX_CARS_RECORDED_NUMBER; Recording++)
// 	{
// 		if(ms_vehiclesRecordings[Recording].IsRecording())
// 		{
// 			return true;
// 		}
// 	}
// 	return false;
// }
#endif

///////////////////////////////////////////////////////////////////////////
// NAME       : StartPlaybackRecordedCar()
// PURPOSE 	  : This will start a playback for this car. The file is opened. Data is read in
//				and from this point on the coordinates of this car are overwritten with data
//				from the file.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::StartPlaybackRecordedCar(class CVehicle *pCar, int index, bool bArgUseCarAI, float fSpeed, s32 iDrivingFlags, bool bLooped, s32 ForCodeIndex, u8 *pData, int dataSize, s32 iVehicleRecordingFlags, u32 delayUntilTurningBackOnAIActivation )
{
	s32	C;

	if (pData)
	{
		index = -1;
	}
	else if(index == -1)
	{
		return;
	}

	Assert(ForCodeIndex < FOR_CODE_INDEX_LAST);
	Assert(pCar || ForCodeIndex >= 0);
//	Assert(pCar==NULL || !pCar->IsDummy());		// Make sure the car is not a dummy when we start playing. This could mess things up.

					// Obbe's fix to makes sure car isn't a dummy when we start a recording.
	if (pCar && pCar->IsDummy())
	{
		pCar->TryToMakeFromDummyIncludingParents(true);
		Assert(!pCar->IsDummy());
	}

	if (ForCodeIndex >= 0)
	{
		C = ForCodeIndex;	// First slots are reserved for code controlled vehicles.
		Assertf(!pVehicleForPlayback[C] && !bPlaybackGoingOn[C], "This slot should really be free");
	}
	else
	{
		C = FOR_CODE_INDEX_LAST;
		while (C < MAX_CARS_PLAYBACK_NUMBER && bPlaybackGoingOn[C])
		{
			Assertf( (pVehicleForPlayback[C] != pCar) || (pCar == NULL), "START_PLAYBACK_RECORDED_CAR - Attempting to play two recordings on the same car");
			C++;
		}
	}

	if(!AssertVerify(C < MAX_CARS_PLAYBACK_NUMBER))
	{
		Warningf("StartPlaybackRecordedCar: Too many cars played back simultanuously");
	}

	// Make sure data is streamed in.
	if(!Verifyf(index == -1 || sm_StreamingArray[index].HasRecordingData(), "START_PLAYBACK_RECORDED_CAR - recording %s not loaded", sm_StreamingArray[index].GetRecordingName()))
	{
		return;
	}

	SetVehicleForPlayback(C, pCar);
	PlaybackAdditionalRotation[C] = Vec3V(V_ZERO);
	PlaybackPreviousAdditionalRotation[C] = Vec3V(V_ZERO);
	PlaybackLocalPositionOffset[C] = Vec3V(V_ZERO);
	PlaybackGlobalPositionOffset[C] = Vec3V(V_ZERO);
	PlaybackIndex[C] = 0;
	PlaybackRunningTime[C] = 0.0f;
	PlaybackSpeed[C] = 1.0f;		// Standard speed (same as recording)
	bPlaybackLooped[C] = bLooped;
	Assert(!pPlaybackBuffer[C]);
	PlayBackStreamingIndex[C] = index;
	if (index >= 0)
	{
		sm_StreamingArray[index].StartPlayback(&(pPlaybackBuffer[C]), PlaybackBufferSize[C]);
	}
	else
	{
		pPlaybackBuffer[C] = pData;
		PlaybackBufferSize[C] = dataSize;
	}

	Assert(pPlaybackBuffer[C]);

	pCar->GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR);
	pCar->GetPortalTracker()->RequestRescanNextUpdate();

	bPlaybackGoingOn[C] = true;
	bPlaybackPaused[C] = false;
	bUseCarAI[C] = bArgUseCarAI;
	aiVechicleRecordingFlags[C] = iVehicleRecordingFlags | VRF_FirstUpdate;
	aiDelayUntilRevertingBack[C] = delayUntilTurningBackOnAIActivation;
	TurnBackIntoFullPlayBackTime[C] = 0;		// We're not interested in turning this into a full (non ai) recording.
//	PlaybackForCodeIndex[C] = ForCodeIndex;
	
	// Engine starting
	if( pCar )
	{
		if( iVehicleRecordingFlags & VRF_StartEngineInstantly )
		{
			pCar->SwitchEngineOn(true);
		}
		else if( iVehicleRecordingFlags & VRF_StartEngineWithStartup )
		{
			pCar->SwitchEngineOn(false);
		}
	}

	if (ForCodeIndex < 0)
	{
		if (bUseCarAI[C])
		{		// Tell this car to follow this path. The AI will take over from here,
			Assert(pVehicleForPlayback[C]);
			CVehicle *pVeh = pVehicleForPlayback[C];
			if(!AssertVerify(pVeh->GetDriver()))
			{ 
				Warningf("StartPlaybackRecordedCar: Vehicle doesn't have driver - this will play with an empty vehicle");
			}
			CPed *pDriver = pVeh->GetDriver();

			CTaskControlVehicle* pTask = NULL;

			sVehicleMissionParams params;
			params.m_fCruiseSpeed = fSpeed;
			params.m_iDrivingFlags = iDrivingFlags;
			CTask* pCarTask = rage_new CTaskVehicleFollowRecording(params);

			if (pDriver)
			{
				pTask = rage_new CTaskControlVehicle(pVeh, pCarTask);

				pDriver->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY);
			}
			else
			{
				pVeh->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_PRIMARY);
			}
			
	
			SetRecordingToPointClosestToCoors(C, VEC3V_TO_VECTOR3(pCar->GetVehiclePosition()));
		}
		else
		{
	MUST_FIX_THIS(sandy - need to use movers or something?);
	//		pVehicleForPlayback[C]->m_nPhysicalFlags.bInfiniteMass = true;	// don't let recorded car be affected by collisions
	//		pVehicleForPlayback[C]->m_nPhysicalFlags.bInfiniteMassFixed = false;	// but do affect others with collisions
		}
	}
/* Some scripts are attaching vehicles in a very odd way,so will need to find a different solution
    // If we're not using AI, then disconnect trailers
    if(!bUseCarAI[C])
    {
        if(pVehicleForPlayback[C] && pVehicleForPlayback[C]->GetCurrentPhysicsInst())
        {
            if(pVehicleForPlayback[C]->GetVehicleType() == VEHICLE_TYPE_TRAILER)
            {
                // Don't let people play car recordings on attached trailers
                CTrailer *pTrailer = static_cast<CTrailer*>(pCar);
                pTrailer->DetachFromParent(0);
            }
        }
    }
*/

	if (ForCodeIndex < 0)
	{
		pVehicleForPlayback[C]->GetIntelligence()->SetRecordingNumber((s8)(C));
	}
}


///////////////////////////////////////////////////////////////////////////
// NAME       : ForcePlaybackRecordedCarUpdate()
// PURPOSE 	  : Forces a single update of the playback of a recording on the
//				specified vehicle. This is equivalent to the update that would
//				otherwise occur over the course of a *full* game update,
//				irrespective of whether the recordings are normally updated
//				within the physics timeslices.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::ForcePlaybackRecordedCarUpdate(class CVehicle *pCar)
{
	s32	C;

	C = 0;
	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pCar))
	{
		C++;
	}

	if(Verifyf(C < MAX_CARS_PLAYBACK_NUMBER, "FORCE_PLAYBACK_RECORDED_VEHICLE_UPDATE is refering to a recording that does not exist"))
	{
		//NOTE: We use the current game time step (in milliseconds), as we are forcing a single update of the playback on this game update.
		float TimeStep = (float)(fwTimer::GetTimeInMilliseconds() - fwTimer::GetPrevElapsedTimeInMilliseconds());
		UpdatePlaybackForVehicleRecording(C, TimeStep);
	}
}


///////////////////////////////////////////////////////////////////////////
// NAME       : DisplayPlaybackRecordedCar()
// PURPOSE 	  : 
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CVehicleRecordingMgr::DisplayPlaybackRecordedCar(class CVehicle *pCar, s32 NewDisplayMode)
{
	Assert(pCar);

	for (s32 C = 0; C < MAX_CARS_PLAYBACK_NUMBER; C++)
	{
		if (bPlaybackGoingOn[C] && pCar == pVehicleForPlayback[C])
		{
			DisplayMode[C] = (u8)(NewDisplayMode);
		}
	}
}
#endif

bool CVehicleRecordingMgr::Place(s32 index, datResourceMap& map, datResourceInfo& header)
{
	CVehicleRecording * pR = NULL;
	pgRscBuilder::PlaceStream(pR, header, map, "<unknown>");

	sm_StreamingArray[index].Place(pR);

	return true;
}

void CVehicleRecordingMgr::AddRef(strLocalIndex index)
{
	sm_StreamingArray[index.Get()].AddRef();
}

void CVehicleRecordingMgr::RemoveRef(strLocalIndex index)
{
	sm_StreamingArray[index.Get()].RemoveRef();
}

int CVehicleRecordingMgr::GetNumRefs(strLocalIndex index)
{
	return sm_StreamingArray[index.Get()].GetNumRefs();
}


///////////////////////////////////////////////////////////////////////////
// NAME       : SetRecordingToPointClosestToCoors()
// PURPOSE 	  : Goes through the recording and sets it to the point closest to the coordinates.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SetRecordingToPointClosestToCoors(s32 C, const Vector3& Coors, Vector3 *CoorsClosest)
{
	s32	IndexInRecording = 0;
	
	float	NearestDist = 999999.9f;
	while (IndexInRecording < PlaybackBufferSize[C])
	{
		CVehicleStateEachFrame *pVehState = (CVehicleStateEachFrame *) &((pPlaybackBuffer[C])[IndexInRecording]);

		float Distance = (Coors - Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ) ).Mag();
		if (Distance < NearestDist)
		{
			PlaybackIndex[C] = IndexInRecording;
			NearestDist = Distance;
			if(CoorsClosest)
			{
				CoorsClosest->Set(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ);
			}
		}
		
		IndexInRecording += sizeof(CVehicleStateEachFrame);
	}
}

// returns the slot in the playback arrays
int CVehicleRecordingMgr::GetPlaybackSlot(const CVehicle* pCar)
{
	int	C = 0;

	while (C < MAX_CARS_PLAYBACK_NUMBER)
	{
		if(bPlaybackGoingOn[C] && pVehicleForPlayback[C] == pCar)
			return C;
		C++;
	}
	return -1;
}

int CVehicleRecordingMgr::GetPlaybackSlotFast(const CVehicle* pCar)
{
	if(pCar->GetIntelligence())
	{
		return pCar->GetIntelligence()->GetRecordingNumber();
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : StopPlaybackRecordedCar()
// PURPOSE 	  : Stops playing back the recorded data for this car.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::StopPlaybackRecordedCar(class CVehicle *pCar)
{
	int slot = GetPlaybackSlot(pCar);
	if(slot != -1)
		StopPlaybackWithIndex(slot);
}

///////////////////////////////////////////////////////////////////////////
// NAME       : StopPlaybackRecordedCar()
// PURPOSE 	  : Stops playing back the recorded data for this car.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////
void CVehicleRecordingMgr::StopPlaybackWithIndex(s32 C)
{
	Assert(C<MAX_CARS_PLAYBACK_NUMBER);

//	Assert(pVehicleForPlayback[C]);	This is not always the case since we can call StopPlaybackWithIndex after the vehicle is removed.
	if (pVehicleForPlayback[C])
	{
		pVehicleForPlayback[C]->GetIntelligence()->SetRecordingNumber(-1);
        pVehicleForPlayback[C]->SelectAppropriateGearForSpeed();

        if(pVehicleForPlayback[C] && pVehicleForPlayback[C]->GetCurrentPhysicsInst())
        {
            if(pVehicleForPlayback[C]->GetVehicleType() == VEHICLE_TYPE_TRAILER)
            {
                // Reatttach trailers that have been left attached whilst doing a car recording
				CVehicle *pCar = pVehicleForPlayback[C];
                CTrailer *pTrailer = static_cast<CTrailer*>(pCar);
                CVehicle *pParent = (CVehicle*)pTrailer->GetAttachParent();
                if(pParent)
                {
                    pTrailer->DetachFromParent(0);
                    pTrailer->AttachToParentVehicle(pParent, false);
                }
            }

        }

		pVehicleForPlayback[C]->m_vehControls.Reset();
		pVehicleForPlayback[C]->m_vehControls.SetHandBrake(false);

		// Force the default driving task to be reconstructed
		CPed* pDriver = pVehicleForPlayback[C]->GetDriver();
		if( pDriver )
		{
	        // If the driver is exiting, don't bother recomputing default tasks as they're leaving
	        if (!pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE)
		    && !pDriver->IsLocalPlayer())
		    {
		        pDriver->GetPedIntelligence()->AddTaskDefault(pDriver->ComputeDefaultTask(*pDriver));
	        }
		}

		CTaskVehicleMissionBase *pCarTask = pVehicleForPlayback[C]->GetIntelligence()->GetActiveTask();
		if(pCarTask)
		{
			pCarTask->RequestSoftReset();
		}

		if(pVehicleForPlayback[C]->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsInactive(pVehicleForPlayback[C]->GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			pVehicleForPlayback[C]->ActivatePhysics();
		}

		pVehicleForPlayback[C]->m_nVehicleFlags.bLerpToFullRecording = false;
	}

	SetVehicleForPlayback(C, NULL);
//	delete [] pPlaybackBuffer[C];
	pPlaybackBuffer[C] = NULL;
	PlaybackBufferSize[C] = 0;
	bPlaybackGoingOn[C] = false;
	strLocalIndex StreamingIndex = strLocalIndex(PlayBackStreamingIndex[C]);
	if (StreamingIndex.Get() >= 0)
	{
		CVehicleRecordingMgr::GetStreamingModule()->RemoveRef(StreamingIndex, REF_OTHER);
	}
}


///////////////////////////////////////////////////////////////////////////
// NAME       : IsPlaybackGoingOnForCar()
// PURPOSE 	  : returns true if this car is playing recorded data.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

bool CVehicleRecordingMgr::IsPlaybackGoingOnForCar(const CVehicle *pCar)
{
	return pCar->IsRunningCarRecording();
}

int CVehicleRecordingMgr::GetPlaybackIdForCar(const CVehicle* pCar)
{
	int slot = GetPlaybackSlotFast(pCar);
	if(slot != -1)
		return PlayBackStreamingIndex[slot];
	return -1;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : IsPlaybackPausedForCar()
// PURPOSE 	  : returns true if this car is paused.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

bool CVehicleRecordingMgr::IsPlaybackPausedForCar(class CVehicle *pCar)
{
	int slot = GetPlaybackSlotFast(pCar);
	if (slot != -1)
		return bPlaybackPaused[slot];
	return false;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : IsPlaybackSwitchedToAiForCar()
// PURPOSE 	  : returns true if this car is switched to AI.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

bool CVehicleRecordingMgr::IsPlaybackSwitchedToAiForCar(const CVehicle *pCar)
{
	int slot = GetPlaybackSlotFast(pCar);
	if (slot != -1)
		return bUseCarAI[slot];
	return false;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : SetPlaybackSpeed()
// PURPOSE 	  : Sets the new playback speed (1.0f = standard, 0.8f = 20% slower etc)
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SetPlaybackSpeed(class CVehicle *pCar, float Speed)
{
	int slot = GetPlaybackSlotFast(pCar);
	if (slot != -1)
		PlaybackSpeed[slot] = Speed;
}


///////////////////////////////////////////////////////////////////////////
// NAME       : PausePlaybackRecordedCar()
// PURPOSE 	  : Pauses playing back the recorded data for this car.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::PausePlaybackRecordedCar(class CVehicle *pCar)
{
	int slot = GetPlaybackSlotFast(pCar);
	if (slot != -1)
		bPlaybackPaused[slot] = true;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : UnpausePlaybackRecordedCar()
// PURPOSE 	  : Pauses playing back the recorded data for this car.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::UnpausePlaybackRecordedCar(class CVehicle *pCar)
{
	int slot = GetPlaybackSlotFast(pCar);
	if (slot != -1)
		bPlaybackPaused[slot] = false;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : ChangeCarPlaybackToUseAI()
// PURPOSE 	  : If a direct playback is going on but we'd like to use an AI one instead
//				this function is called.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////


void CVehicleRecordingMgr::ChangeCarPlaybackToUseAI(class CVehicle *pVeh, u32 timeAfterWhichToTryAndReturnToFullRecording, s32 iDrivingFlags, bool bSnapToPositionIfNotVisible)
{
	int slot = GetPlaybackSlot(pVeh);
	if(!Verifyf(slot != -1, "CHANGE_CAR_PLAYBACK_TO_USE_AI looks like this car isn't playing back a recording at the moment"))
		return;

	CTrailer *pTrailerToReattach = NULL;
	if(pVeh->GetAttachedTrailer() && IsPlaybackGoingOnForCar(pVeh->GetAttachedTrailer()) && !IsPlaybackSwitchedToAiForCar(pVeh->GetAttachedTrailer()))
	{
		pTrailerToReattach = pVeh->GetAttachedTrailer();
		CVehicleRecordingMgr::ChangeCarPlaybackToUseAI(pTrailerToReattach, timeAfterWhichToTryAndReturnToFullRecording, iDrivingFlags, bSnapToPositionIfNotVisible);
	}

	bUseCarAI[slot] = true;

	TurnBackIntoFullPlayBackTime[slot] = ~(u32)0;
	if (timeAfterWhichToTryAndReturnToFullRecording)
	{
		TurnBackIntoFullPlayBackTime[slot] = fwTimer::GetTimeInMilliseconds() + timeAfterWhichToTryAndReturnToFullRecording;
	}

	CPed *pDriver = pVeh->GetDriver();

	if (pDriver)
	{
		CTaskControlVehicle* pTask = NULL;

		// Tell this car to follow this path. The AI will take over from here,
		sVehicleMissionParams params;
		params.m_fCruiseSpeed = 20.0f;
		params.m_iDrivingFlags = iDrivingFlags;

		aiTask *pCarTask = rage_new CTaskVehicleFollowRecording(params);
		pTask = rage_new CTaskControlVehicle(pVeh, pCarTask);

		//if we were in a driveby, make sure we restore it
		CTaskVehicleGun* pTaskVehicleGun = static_cast<CTaskVehicleGun*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_GUN));
		CTaskVehicleGun* pNewTaskVehicleGun = NULL;
		if (pTaskVehicleGun) 
		{
			pNewTaskVehicleGun = static_cast<CTaskVehicleGun*>(pTaskVehicleGun->Copy());
		}

		pDriver->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY);
		if (pNewTaskVehicleGun)
			pTask->SetDesiredSubtask(pNewTaskVehicleGun);

		Vector3 vClosestCoords = Vector3(Vector3::ZeroType);
		SetRecordingToPointClosestToCoors(slot, VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition()), &vClosestCoords);

		if(bSnapToPositionIfNotVisible && !pVeh->GetIsVisibleInSomeViewportThisFrame() && vClosestCoords.IsNonZero())
		{
			pVeh->SetPosition(vClosestCoords, true, true, true);
		}
	}
	else
	{
		pVeh->m_vehControls.Reset();
	}

	if(pVeh->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsInactive(pVeh->GetCurrentPhysicsInst()->GetLevelIndex()))
	{
		pVeh->ActivatePhysics();
	}

	if(pTrailerToReattach)
	{
		//Force re-attach to ensure constraints are set
		pTrailerToReattach->AttachToParentVehicle(pVeh, true, 1.0f, true);
	}
}




///////////////////////////////////////////////////////////////////////////
// NAME       : SkipToEndOfFile
// PURPOSE 	  : This will jump to the last recorded state.
//				The specified car will be set to this state.
//				(This should make it easier to record sequences of recodings)
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SkipToEndAndStopPlaybackRecordedCar(class CVehicle *pCar)
{
	int slot = GetPlaybackSlot(pCar);

	if (Verifyf(slot != -1, "SkipToEndAndStopPlaybackRecordedCar: looks like this car isn't playing back a recording at the moment"))
	{
		// Jump to the last stored state in the file.
		//pVehicleForPlayback[C]->m_nPhysicalFlags.bInfiniteMass = false;
		Matrix34 PreviousMatrix = MAT34V_TO_MATRIX34(pVehicleForPlayback[slot]->GetMatrix());
		CVehicleStateEachFrame *pLastVehState = (CVehicleStateEachFrame *) &((pPlaybackBuffer[slot])[PlaybackBufferSize[slot]]);
		pLastVehState--;

		float playbackSpeed;
		if(bPlaybackPaused[slot])
		{
			playbackSpeed = 0.0f;
		}
#if __DEV
		// If this is the debug recording as triggered by the widgets; allow user to change the playback speed with the slider.
		else if(debug_WidgetRecordingPlaybackVehicle == pVehicleForPlayback[slot] && pPlaybackBuffer[slot] == debug_pDebugBuffer)
		{
			playbackSpeed = debug_timeScale;
		}
#endif
		else
		{
			playbackSpeed = PlaybackSpeed[slot];
		}

		RestoreInfoForCar(pVehicleForPlayback[slot], pLastVehState, false, playbackSpeed, true);

		// Code needed to stop the wheels applying silly forces.
		for (s32 i = 0; i < pVehicleForPlayback[slot]->GetNumWheels(); i++)
		{
			pVehicleForPlayback[slot]->GetWheel(i)->UpdateContactsAfterNetworkBlend(PreviousMatrix, MAT34V_TO_MATRIX34(pVehicleForPlayback[slot]->GetMatrix()));
		}

		StopPlaybackWithIndex(slot);

		pCar->GetIntelligence()->SetRecordingNumber(-1);

		pCar->SetAngVelocity(Vector3(0.0f,0.0f,0.0f));
		pCar->SetVelocity(Vector3(0.0f,0.0f,0.0f));
	}
}




///////////////////////////////////////////////////////////////////////////
// NAME       : SaveDataForThisFrame()
// PURPOSE    : All the info that has to be saved for this frame is.
//				This includes all the possible input as well as the
//				time elapsed.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

//u8	BufferDataToBeSaved[128];
// should be in game.cpp right after CClock::Update();

#if !__FINAL
void CVehicleRecordingMgr::SaveDataForThisFrame()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif

	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsPlaying())
		return;	

	// Do what needs to be done if we're recording a cars position.
	for (s32 Recording = 0; Recording < MAX_CARS_RECORDED_NUMBER; Recording++)
	{
		if (ms_vehiclesRecordings[Recording].IsRecording())
		{
			ms_vehiclesRecordings[Recording].SaveFrame();
		}
	}

#if __BANK
	// The user can request for a certain recording to be reduced.
	// This can be used for things like the buses.
//	if (bReduceRecording)
//	{
//		ReduceRecording(RecordingToBeReduced, ReductionSampleStep);
//		bReduceRecording = false;
//	}
	if (bShiftRecording)
	{
		CRecording::ShiftRecording(RecordingNumberToBeShifted, RecordingNameToBeShifted, ShiftX, ShiftY, ShiftZ);
		bShiftRecording = false;
	}
	if (bFixRecordingTimeStamps)
	{
		CRecording::FixRecordingTimeStamps(RecordingNumberToBeShifted, RecordingNameToBeShifted);
		bFixRecordingTimeStamps = false;
	}
#endif // __BANK
}
#endif // !__FINAL

///////////////////////////////////////////////////////////////////////////
// NAME       : RetrieveDataForThisFrame()
// PURPOSE    : All the info that has to be played back for this frame is.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::RetrieveDataForThisFrame()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif
	//Allow car recordings for cut scenes, enabled for mag demo not sure why we dont want it full stop
	/*if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsPlaying()) 
		return;*/

	float TimeStep = (float)(fwTimer::GetTimeInMilliseconds() - fwTimer::GetPrevElapsedTimeInMilliseconds());

	if(CVehicleRecordingMgr::sm_bUpdateWithPhysics)
	{
		// NOTE: We must still respect the game timer when updating within the physics time slices, as the game timer
		// accumulates fixed-point time steps, in milliseconds, and script must be able to control recordings using the
		// game timer. If we accumulated time correctly here, using the floating-point physics time step, we'd accumulate
		// error w.r.t. the game timer.

		int nNumTimeSlices = CPhysics::GetNumTimeSlices();
		if(nNumTimeSlices > 0)
		{
			TimeStep /= (float)nNumTimeSlices;
		}
	}

	// Update the recordings that don't necessarily have vehicles with them (buses etc)

	// Do what needs to be done to handle playback of cars.
	for (s32 C = 0; C < MAX_CARS_PLAYBACK_NUMBER; C++)
	{
		UpdatePlaybackForVehicleRecording(C, TimeStep);
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : UpdatePlaybackForVehicle()
// PURPOSE    : Handles the playback of a specific vehicle recording.
// RETURNS    :
// PARAMETERS :
///////////////////////////////////////////////////////////////////////////
float fMinSpeedForAccelerationInterpolation = 5.0f;
void CVehicleRecordingMgr::UpdatePlaybackForVehicleRecording(s32 RecordingIndex, float TimeStep)
{
	if (bPlaybackGoingOn[RecordingIndex] && !(RecordingIndex < FOR_CODE_INDEX_LAST && pVehicleForPlayback[RecordingIndex]) )	// Don't update code recordings with cars. The ai should update those.
	{
		const bool bContinueRecordingIfCarDestroyed = (GetVehicleRecordingFlags(RecordingIndex) & VRF_ContinueRecordingIfCarDestroyed) != 0;
		if ( (((RecordingIndex >= FOR_CODE_INDEX_LAST) && (!pVehicleForPlayback[RecordingIndex])) || (!bContinueRecordingIfCarDestroyed && pVehicleForPlayback[RecordingIndex]->m_nPhysicalFlags.bRenderScorched)))		// We're done with this playback
		{
			StopPlaybackWithIndex(RecordingIndex);
		}
		else if (bUseCarAI[RecordingIndex])
		{
			if (TurnBackIntoFullPlayBackTime[RecordingIndex])
			{
				TryToTurnAIRecordingBackIntoFull(RecordingIndex);
			}
		}
		else
		{
			float playbackSpeed;
			if(bPlaybackPaused[RecordingIndex])
			{
				playbackSpeed = 0.0f;
			}
#if __DEV
			// If this is the debug recording as triggered by the widgets; allow user to change the playback speed with the slider.
			else if(debug_WidgetRecordingPlaybackVehicle == pVehicleForPlayback[RecordingIndex] && pPlaybackBuffer[RecordingIndex] == debug_pDebugBuffer)
			{
				playbackSpeed = debug_timeScale;
			}
#endif
			else
			{
				playbackSpeed = PlaybackSpeed[RecordingIndex];
			}

			if (!bPlaybackPaused[RecordingIndex])
			{
				if (RecordingIndex < FOR_CODE_INDEX_LAST)	// This is a recording used by code (ie bus)
				{
					PlaybackRunningTime[RecordingIndex] = (float)FindPlaybackRunningTimeAccordingToSchedule(RecordingIndex, 0);
				}
				else
				{
					// The following bit used to smoothe out the playback by averaging the timestep over a few frames
					// We might want to reinstate this at some point. (Better yet; improve the recordings)

					float TimeStepScaled = TimeStep * playbackSpeed;

					const float maxTimeStep = 250.0f;
					if (TimeStepScaled > maxTimeStep)
					{
						Warningf("The vehicle recording time step (%fms) is being limited to %fms", TimeStepScaled, maxTimeStep);

						TimeStepScaled = maxTimeStep;
					}

					PlaybackRunningTime[RecordingIndex] += TimeStepScaled;
				}
			}



			u32 EndTimeInRecording = ((CVehicleStateEachFrame *) &((pPlaybackBuffer[RecordingIndex])[PlaybackBufferSize[RecordingIndex]-sizeof(CVehicleStateEachFrame)]))->TimeInRecording;

			// Deal with time getting below 0
			if (PlaybackRunningTime[RecordingIndex] < 0.0f)
			{
				if (bPlaybackLooped[RecordingIndex])
				{
					PlaybackRunningTime[RecordingIndex] = (float)EndTimeInRecording;
				}
				else
				{
					PlaybackRunningTime[RecordingIndex] = 0.0f;
				}
			}

			// Deal with running off the end of the recording.
			if (PlaybackRunningTime[RecordingIndex] >= EndTimeInRecording)
			{
				if (bPlaybackLooped[RecordingIndex])
				{
					PlaybackRunningTime[RecordingIndex] = 0.0f;
					PlaybackIndex[RecordingIndex] = 0;
				}
				else
				{
					PlaybackRunningTime[RecordingIndex] = (float)EndTimeInRecording;
					StopPlaybackRecordedCar(pVehicleForPlayback[RecordingIndex]);
				}
			}


			if (pVehicleForPlayback[RecordingIndex])		// Make sure recording hasn't been stopped by now.
			{

				CVehicleStateEachFrame *pVehState = (CVehicleStateEachFrame *) &((pPlaybackBuffer[RecordingIndex])[PlaybackIndex[RecordingIndex]]);
				CVehicleStateEachFrame *pVehNextState = pVehState+1;
				CVehicleStateEachFrame *pVehNextOfNextState = pVehState+2;
				CVehicleStateEachFrame *pVehPreviousState = pVehState-1;

				while (PlaybackRunningTime[RecordingIndex] > pVehNextState->TimeInRecording && pVehNextState < (CVehicleStateEachFrame *) &((pPlaybackBuffer[RecordingIndex])[PlaybackBufferSize[RecordingIndex]]))
				{
					PlaybackIndex[RecordingIndex] += sizeof(CVehicleStateEachFrame);
					pVehPreviousState++;
					pVehState++;
					pVehNextState++;
					pVehNextOfNextState++;
				}

				// The PlaybackSpeed can be negative in which case we move backwards through the playback.
				while (PlaybackRunningTime[RecordingIndex] < pVehState->TimeInRecording && pVehState > (CVehicleStateEachFrame *) &((pPlaybackBuffer[RecordingIndex])[0]))
				{
					PlaybackIndex[RecordingIndex] -= sizeof(CVehicleStateEachFrame);
					pVehPreviousState--;
					pVehState--;
					pVehNextState--;
					pVehNextOfNextState--;
				}

				if(pVehNextState == (CVehicleStateEachFrame *) &((pPlaybackBuffer[RecordingIndex])[PlaybackBufferSize[RecordingIndex]]))
				{
					pVehNextOfNextState = NULL;
				}

				if(pVehState == (CVehicleStateEachFrame *) &((pPlaybackBuffer[RecordingIndex])[0]))
				{
					pVehPreviousState = NULL;
				}

				Matrix34 PreviousMatrix = MAT34V_TO_MATRIX34(pVehicleForPlayback[RecordingIndex]->GetMatrix());

				// Store the previous matrix as it is used when updating the possiblyTouchesWaterFlag, etc.
				pVehicleForPlayback[RecordingIndex]->SetPreviousPosition(PreviousMatrix.d);

				RestoreInfoForCar(pVehicleForPlayback[RecordingIndex], pVehState, false, playbackSpeed, (aiVechicleRecordingFlags[RecordingIndex] & VRF_FirstUpdate) > 0);

				// If this is the very first frame of the recording, using the initial playback matrix as the previous matrix
				if(aiVechicleRecordingFlags[RecordingIndex] & VRF_FirstUpdate)
				{
					pVehicleForPlayback[RecordingIndex].Get()->m_Transmission.SelectAppropriateGearForSpeed();
					PreviousMatrix = MAT34V_TO_MATRIX34(pVehicleForPlayback[RecordingIndex]->GetMatrix());
					pVehicleForPlayback[RecordingIndex]->SetPreviousPosition(PreviousMatrix.d);
					aiVechicleRecordingFlags[RecordingIndex] &= ~VRF_FirstUpdate;
				}


				float Interpolation = float(PlaybackRunningTime[RecordingIndex]-pVehState->TimeInRecording) / (pVehNextState->TimeInRecording - pVehState->TimeInRecording);

				// Smooth out the velocity transition from record to record when playing in slow motion
				if(fwTimer::GetTimeWarpActive()<1.f && pVehNextOfNextState && pVehPreviousState)
				{
					Vector3 vPreviousRecordPos = pVehPreviousState->GetCoors();
					Vector3 vCurrentRecordPos = pVehState->GetCoors();
					Vector3 vNextRecordPos = pVehNextState->GetCoors();
					Vector3 vNextOfNextRecordPos = pVehNextOfNextState->GetCoors();

					float fCurrentRecordDistance = (vNextRecordPos - vCurrentRecordPos).Mag();

					float fAveragePreviousRecordSpeed = (vCurrentRecordPos - vPreviousRecordPos).Mag() / (float(pVehState->TimeInRecording - pVehPreviousState->TimeInRecording) * 0.001f);
					float fAverageCurrentRecordSpeed = fCurrentRecordDistance / (float(pVehNextState->TimeInRecording - pVehState->TimeInRecording) * 0.001f);
					float fAverageNextRecordSpeed = (vNextOfNextRecordPos - vNextRecordPos).Mag() / (float(pVehNextOfNextState->TimeInRecording - pVehNextState->TimeInRecording) * 0.001f);

					if(fAveragePreviousRecordSpeed > fMinSpeedForAccelerationInterpolation && fAverageCurrentRecordSpeed > fMinSpeedForAccelerationInterpolation && fAverageNextRecordSpeed > fMinSpeedForAccelerationInterpolation)
					{
						float fAccelerationPreviousToCurrent = (fAverageCurrentRecordSpeed - fAveragePreviousRecordSpeed) / (float(pVehState->TimeInRecording - pVehPreviousState->TimeInRecording) * 0.001f);
						float fAccelerationCurrentToNext = (fAverageNextRecordSpeed - fAverageCurrentRecordSpeed) / (float(pVehNextState->TimeInRecording - pVehState->TimeInRecording) * 0.001f);
						float fAcceleration = (fAccelerationPreviousToCurrent + fAccelerationCurrentToNext) * 0.5f;
						float fTimePassed = float(PlaybackRunningTime[RecordingIndex] - pVehState->TimeInRecording) * 0.001f;

						float fStartSpeed = fAverageCurrentRecordSpeed - fAcceleration * (float(pVehNextState->TimeInRecording - pVehState->TimeInRecording) * 0.001f) * 0.5f;

						// P1 = P0 + V0t + 1/2aT^2
						float fDistanceTraveled = fStartSpeed * fTimePassed + fAcceleration * (fTimePassed * fTimePassed * 0.5f);
						Interpolation = fDistanceTraveled / fCurrentRecordDistance;
						Interpolation = Clamp(Interpolation, 0.0f, 1.0f);
					}
				}

				Assert(Interpolation >= 0.0f && Interpolation <= 1.0f);

				Quaternion additionalRotation;
				int timeSlice = CPhysics::GetCurrentTimeSlice();
#if __BANK
				// If this is the debug recording as triggered by the widgets; allow user to override the additional rotation to be applied.
				if((debug_AdditionalRotationEulers.Mag2() > VERY_SMALL_FLOAT) &&
					(debug_WidgetRecordingPlaybackVehicle == pVehicleForPlayback[RecordingIndex]) && (pPlaybackBuffer[RecordingIndex] == debug_pDebugBuffer))
				{
					additionalRotation.FromEulers(debug_AdditionalRotationEulers * DtoR, eEulerOrderYXZ);
				}
				else
#endif // __BANK
				{
					Vec3V additionalRotationAxisAngle;
					if(timeSlice != -1 && !CPhysics::GetIsLastTimeSlice(timeSlice))
					{
						// If we're the middle of the frame, interpolate the rotation
						ScalarV frameInterpolation = ScalarVFromF32((float)(timeSlice + 1)/CPhysics::GetNumTimeSlices());
						additionalRotationAxisAngle = Lerp(frameInterpolation,PlaybackPreviousAdditionalRotation[RecordingIndex],PlaybackAdditionalRotation[RecordingIndex]);
					}
					else
					{
						// If we reach the end of the frame, store off the current rotation as the previous
						additionalRotationAxisAngle = PlaybackAdditionalRotation[RecordingIndex];
						PlaybackPreviousAdditionalRotation[RecordingIndex] = PlaybackAdditionalRotation[RecordingIndex];
					}
					ScalarV rotationAngle = Mag(additionalRotationAxisAngle);
					Vec3V rotationAxis = InvScaleSafe(additionalRotationAxisAngle,rotationAngle,Vec3V(V_ZERO));
					additionalRotation = QUATV_TO_QUATERNION(QuatVFromAxisAngle(rotationAxis,rotationAngle));
				}

				Vec3V localPositionOffset = PlaybackLocalPositionOffset[RecordingIndex];
				BANK_ONLY(localPositionOffset = Add(localPositionOffset,debug_LocalPositionOffset);)

				Vec3V globalPositionOffset = PlaybackGlobalPositionOffset[RecordingIndex];
				BANK_ONLY(globalPositionOffset = Add(globalPositionOffset,debug_GlobalPositionOffset);)

				InterpolateInfoForCar(pVehicleForPlayback[RecordingIndex], pVehNextState, Interpolation, playbackSpeed, additionalRotation, localPositionOffset, globalPositionOffset, PreviousMatrix, TimeStep);

				Matrix34 m = MAT34V_TO_MATRIX34(pVehicleForPlayback[RecordingIndex]->GetMatrix());
				for (s32 i = 0; i < pVehicleForPlayback[RecordingIndex]->GetNumWheels(); i++)
				{
					pVehicleForPlayback[RecordingIndex]->GetWheel(i)->UpdateContactsAfterNetworkBlend(PreviousMatrix, m);
				}

				// If the vehicle has made a big jump we need to reset suspension or huge forces will be applied.
				if ((VEC3V_TO_VECTOR3(pVehicleForPlayback[RecordingIndex]->GetVehiclePosition()) - PreviousMatrix.d).Mag() > 5.0f)
				{
					pVehicleForPlayback[RecordingIndex]->ResetSuspension();

					CInteriorInst*	pDestInteriorInst = 0;	
					s32			destRoomIdx = -1;
					bool setWaitForAllCollisionsBeforeProbe = false;
					Matrix34 mat = MAT34V_TO_MATRIX34(pVehicleForPlayback[RecordingIndex]->GetMatrix());
					if(pVehicleForPlayback[RecordingIndex]->InheritsFromBike())
					{
						CBike::PlaceOnRoadProperly(static_cast<CBike*>(pVehicleForPlayback[RecordingIndex].Get()),&mat,pDestInteriorInst,destRoomIdx,setWaitForAllCollisionsBeforeProbe,
							pVehicleForPlayback[RecordingIndex]->GetModelIndex(), pVehicleForPlayback[RecordingIndex]->GetCurrentPhysicsInst(), 
							false, NULL, NULL, 0, 1.0f, 1.0f, true); 
					}
					else if(pVehicleForPlayback[RecordingIndex]->InheritsFromAutomobile())
					{
						CAutomobile::PlaceOnRoadProperly(static_cast<CAutomobile*>(pVehicleForPlayback[RecordingIndex].Get()),&mat,pDestInteriorInst,destRoomIdx,setWaitForAllCollisionsBeforeProbe,
							pVehicleForPlayback[RecordingIndex]->GetModelIndex(), pVehicleForPlayback[RecordingIndex]->GetCurrentPhysicsInst(), 
							false, NULL, NULL, 0, PLACEONROAD_DEFAULTHEIGHTUP, PLACEONROAD_DEFAULTHEIGHTDOWN); 
					}
				}


				// If the vehicle in question is a train we need to set the PositionOnTrack
				if (pVehicleForPlayback[RecordingIndex]->GetVehicleType() == VEHICLE_TYPE_TRAIN)
				{
					((CTrain *)pVehicleForPlayback[RecordingIndex].Get())->SetTrackPosFromWorldPos();
				}

#if __BANK && __STATS
				// If this is the debug recording as triggered by the widgets, graph the effective move speed.
				if(debug_WidgetRecordingPlaybackVehicle == pVehicleForPlayback[RecordingIndex] && pPlaybackBuffer[RecordingIndex] == debug_pDebugBuffer)
				{
					const Vector3 vehiclePosition = VEC3V_TO_VECTOR3(pVehicleForPlayback[RecordingIndex]->GetVehiclePosition());

					if(!g_VehiclePositionOnPreviousUpdate.IsClose(VEC3_MAX, SMALL_FLOAT))
					{
#if __STATS
						const float maxTimeStep		= 250.0f;
						const float TimeStepScaled	= Min(TimeStep * playbackSpeed, maxTimeStep);

						//NOTE: TimeStepScaled is in milliseconds...
						const float effectiveSpeed = vehiclePosition.Dist(g_VehiclePositionOnPreviousUpdate) * 1000.0f / TimeStepScaled;

						PF_SET(vehicleEffectiveSpeed, effectiveSpeed);

						const Vector3& velocity = pVehicleForPlayback[RecordingIndex]->GetVelocity();
						const float actualSpeed = velocity.Mag();

						PF_SET(vehicleActualSpeed, actualSpeed);
#endif // __STATS
					}

					g_VehiclePositionOnPreviousUpdate = vehiclePosition;
				}
#endif // __BANK

#if __ASSERT
				Assert(rage::Abs(pVehicleForPlayback[RecordingIndex]->GetVelocity().x) < 200.0f);
				Assert(rage::Abs(pVehicleForPlayback[RecordingIndex]->GetVelocity().y) < 200.0f);
				Assert(rage::Abs(pVehicleForPlayback[RecordingIndex]->GetVelocity().z) < 200.0f);

				const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicleForPlayback[RecordingIndex]->GetVehiclePosition());
				Assert(rage::Abs(rage::Abs(vVehiclePosition.x)) < 10000.0f);
				Assert(rage::Abs(rage::Abs(vVehiclePosition.y)) < 10000.0f);
				Assert(rage::Abs(rage::Abs(vVehiclePosition.z)) < 10000.0f);
#endif
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : RestoreInfoForMatrix()
// PURPOSE    : Uncompresses the matrix to be played back in an animation.
///////////////////////////////////////////////////////////////////////////
void CVehicleRecordingMgr::RestoreInfoForMatrix(Matrix34& mat, class CVehicleStateEachFrame *pCarState)
{
	Vector3	ThirdVec;

	static const float s_fNormalizer = 1.0f / 127.0f;
	mat.a.x = pCarState->Matrix_a_x * s_fNormalizer;
	mat.a.y = pCarState->Matrix_a_y * s_fNormalizer;
	mat.a.z = pCarState->Matrix_a_z * s_fNormalizer;
	mat.b.x = pCarState->Matrix_b_x * s_fNormalizer;
	mat.b.y = pCarState->Matrix_b_y * s_fNormalizer;
	mat.b.z = pCarState->Matrix_b_z * s_fNormalizer;

	// Work out the third vector of the matrix.
	ThirdVec = CrossProduct(mat.a, mat.b);
	mat.c.x = ThirdVec.x;
	mat.c.y = ThirdVec.y;
	mat.c.z = ThirdVec.z;

	mat.Normalize();		// Rage needs the matrix to be orthonormal
	Assert(mat.IsOrthonormal());

	mat.d.x = pCarState->CoorsX;
	mat.d.y = pCarState->CoorsY;
	mat.d.z = pCarState->CoorsZ;

#if __BANK
	mat.d.z += fShiftPlaybackUpValue;
#endif

}

void CVehicleRecordingMgr::RestoreInfoForPosition(Vector3& pos, class CVehicleStateEachFrame *pCarState)
{
	pos.x = pCarState->CoorsX;
	pos.y = pCarState->CoorsY;
	pos.z = pCarState->CoorsZ;

#if __BANK
	pos.z += fShiftPlaybackUpValue;
#endif
}

void CVehicleRecordingMgr::HandleRecordingTeleport(CVehicle *pCar, const Matrix34 &matOld, bool bUpdateTrailer)
{
	pCar->UpdateGadgetsAfterTeleport(matOld, true, bUpdateTrailer);
}

///////////////////////////////////////////////////////////////////////////
// NAME       : RestoreInfoForCar()
// PURPOSE    : Uncompresses the matrix and stuff for a car to be played back
//				in an animation.
///////////////////////////////////////////////////////////////////////////
static dev_float fMinDistSqDeltaToWarpBackToFull = square(0.01f);
static dev_float fMinHeadingDeltaToWarpBackToFull = SMALL_FLOAT;

void CVehicleRecordingMgr::RestoreInfoForCar(CVehicle *pCar, CVehicleStateEachFrame *pCarState, bool bAnimEnd, float playbackSpeed, bool bUpdateTrailer)
{
	Matrix34 Mat;

	RestoreInfoForMatrix(Mat, pCarState);

	Mat34V oldMat = pCar->GetMatrix();
	const Vec3V angVel = rage::SimdTransformUtil::CalculateAngularVelocity(MAT33V_ARG(oldMat.GetMat33()), MAT33V_ARG(MATRIX34_TO_MAT34V(Mat).GetMat33()));
	pCar->SetAngVelocity(VEC3V_TO_VECTOR3(angVel));

	const Matrix34 matOld = MAT34V_TO_MATRIX34(pCar->GetMatrix());
	if(pCar->m_nVehicleFlags.bLerpToFullRecording)
	{
		float fHeadingDelta = camFrame::ComputeHeadingFromMatrix(Mat) - camFrame::ComputeHeadingFromMatrix(matOld);
		pCar->m_nVehicleFlags.bLerpToFullRecording = matOld.d.Dist2(Mat.d) > fMinDistSqDeltaToWarpBackToFull || Abs(fHeadingDelta) > fMinHeadingDeltaToWarpBackToFull;
	}

	if(!pCar->m_nVehicleFlags.bLerpToFullRecording)
	{
		pCar->SetMatrix(Mat, true, true);
		HandleRecordingTeleport(pCar, matOld, bUpdateTrailer);
	}

	pCar->m_vehControls.m_steerAngle = pCarState->SteerAngle / 20.0f;
	pCar->m_vehControls.m_throttle = pCarState->Gas / 100.0f;
	pCar->m_vehControls.m_brake = pCarState->Brake / 100.0f;
	if (pCarState->HandBrake) pCar->m_vehControls.m_handBrake = true; else pCar->m_vehControls.m_handBrake = false;

	if (bAnimEnd)
	{
		pCar->m_vehControls.m_throttle = 0.0f;
		pCar->m_vehControls.m_brake = 0.0f;
		pCar->SetVelocity(ORIGIN);
		pCar->m_vehControls.m_handBrake = false;
	}
	else
	{
        Vector3 vecSpeed(pCarState->GetSpeedX() * playbackSpeed, pCarState->GetSpeedY() * playbackSpeed, pCarState->GetSpeedZ() * playbackSpeed);

        //Ensure the magnitude does not exceed a certain threshold.
        static ScalarV scMaxMagSq = ScalarVFromF32(rage::square(80.0f));
        ScalarV scMagSq = MagSquared(VECTOR3_TO_VEC3V(vecSpeed));
        if(IsLessThanAll(scMagSq, scMaxMagSq))
        {
		    pCar->SetVelocity(vecSpeed);
        }
	}

	// Recorded bikes are never on their side stand really
	if (pCar->InheritsFromBike())
	{
		((CBike *)pCar)->m_nBikeFlags.bOnSideStand = false;
		((CBike *)pCar)->m_nBikeFlags.bGettingPickedUp = false;
	}

	Assert(pCar->GetVehiclePosition().GetZf() > -500.0f);
}

///////////////////////////////////////////////////////////////////////////
// NAME       : InterpolateInfoForCar()
// PURPOSE    : Uncompresses the matrix and stuff for a car to be played back
//				in an animation.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::InterpolateInfoForCar(CVehicle *pCar, CVehicleStateEachFrame *pCarState, float Interp, float playbackSpeed, const Quaternion& additionalRotation, Vec3V_In localPositionOffset, Vec3V_In globalPositionOffset, const Matrix34 &PreviousMatrix, const float fTimeStep)
{
	Matrix34	TempMatr, ResultMatr;
	Matrix34	TempMatr2 = MAT34V_TO_MATRIX34(pCar->GetMatrix());

	RestoreInfoForMatrix(TempMatr, pCarState);
	TempMatr.Scale(Interp);
	TempMatr.d.x *= Interp;
	TempMatr.d.y *= Interp;
	TempMatr.d.z *= Interp;
	
	TempMatr2.Scale(1.0f - Interp);
	TempMatr2.d.x *= (1.0f - Interp);
	TempMatr2.d.y *= (1.0f - Interp);
	TempMatr2.d.z *= (1.0f - Interp);
	
	ResultMatr.a = TempMatr.a+TempMatr2.a;
	ResultMatr.b = TempMatr.b+TempMatr2.b;
	ResultMatr.c = TempMatr.c+TempMatr2.c;
	ResultMatr.d = TempMatr.d+TempMatr2.d;
	ResultMatr.Normalize();
	Assert(ResultMatr.IsOrthonormal());

	if(additionalRotation.GetAngle() > SMALL_FLOAT)
	{
		Quaternion vehicleOrientation;
		ResultMatr.ToQuaternion(vehicleOrientation);

		vehicleOrientation.Multiply(additionalRotation);

		ResultMatr.FromQuaternion(vehicleOrientation);
	}

	// Transform the position offset into world space and add it to the final matrxix
	RC_MAT34V(ResultMatr).SetCol3(Add(RCC_MAT34V(ResultMatr).GetCol3(), Add(Transform3x3(RCC_MAT34V(ResultMatr),localPositionOffset),globalPositionOffset)));

	const Matrix34 matOld = MAT34V_TO_MATRIX34(pCar->GetMatrix());
	pCar->SetMatrix(ResultMatr, true, true);
	HandleRecordingTeleport(pCar, matOld, false);

	Vector3 vecSpeed;
	vecSpeed.x = Interp * (pCarState->GetSpeedX() * playbackSpeed) + (1.0f - Interp) * pCar->GetVelocity().x;
	vecSpeed.y = Interp * (pCarState->GetSpeedY() * playbackSpeed) + (1.0f - Interp) * pCar->GetVelocity().y;
	vecSpeed.z = Interp * (pCarState->GetSpeedZ() * playbackSpeed) + (1.0f - Interp) * pCar->GetVelocity().z;
	
    Assertf(!pCar->IsBaseFlagSet(fwEntity::IS_FIXED), "Can not play a recording on a car (%s) that is fixed", pCar->pHandling->m_handlingName.TryGetCStr());

    //Ensure the magnitude does not exceed a certain threshold.
    static ScalarV scMaxMagSq = ScalarVFromF32(rage::square(DEFAULT_MAX_SPEED));
    ScalarV scMagSq = MagSquared(VECTOR3_TO_VEC3V(vecSpeed));
    if(IsLessThanAll(scMagSq, scMaxMagSq))
    {
	    pCar->SetVelocity(vecSpeed);
    }
	
	//Convert the time step from milliseconds to seconds.
	ScalarV scTimeStep = Scale(ScalarVFromF32(fTimeStep), ScalarV(V_FLT_SMALL_3));

	//Calculate the inverse time step.
	ScalarV scInvTimeStep = InvertSafe(scTimeStep, ScalarV(V_ZERO));
	
	//Calculate the angular velocity.
	Vec3V vAngularVelocity = pCar->CalculateAngVelocityFromMatrices(RCC_MAT34V(PreviousMatrix), pCar->GetMatrix(), scInvTimeStep);

	//Ensure the magnitude does not exceed a certain threshold.
	//This is typically only the case when the vehicle is warping to the initial orientation.
	scMagSq = MagSquared(vAngularVelocity);
	scMaxMagSq = ScalarVFromF32(rage::square(DEFAULT_MAX_ANG_SPEED * 15.0f));
	if(IsLessThanAll(scMagSq, scMaxMagSq))
	{
		//Set the angular velocity.
		pCar->SetAngVelocity(VEC3V_TO_VECTOR3(vAngularVelocity));
	}

	//Interpolate the steer angle
	pCar->m_vehControls.m_steerAngle = Interp * (pCarState->SteerAngle / 20.0f) + (1.0f - Interp) * pCar->m_vehControls.m_steerAngle;
}


///////////////////////////////////////////////////////////////////////////
// NAME       : SkipForwardInRecording()
// PURPOSE    :
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SkipForwardInRecording(class CVehicle *pCar, float Dist)
{
	s32	C;
	CVehicleStateEachFrame	*pOldState, *pNewState;
	bool	bDone = false;
	
	C = 0;
	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pCar))
	{
		C++;
	}
	
	Assert(C < MAX_CARS_PLAYBACK_NUMBER);
	if (C >= MAX_CARS_PLAYBACK_NUMBER)
		return;

	// Set this to the current state in case nothing changes.
	pNewState = (CVehicleStateEachFrame *)&(pPlaybackBuffer[C][PlaybackIndex[C]]);

	while (!bDone)
	{
		if (Dist > 0.0f)
		{
			if (PlaybackIndex[C] >= PlaybackBufferSize[C] - (s32)sizeof(CVehicleStateEachFrame))
			{	// we're at the end of the recording. Call it a day.
				bDone = true;
			}
			else
			{
				pOldState = (CVehicleStateEachFrame *)&(pPlaybackBuffer[C][PlaybackIndex[C]]);
				PlaybackIndex[C] += sizeof(CVehicleStateEachFrame);
				pNewState = (CVehicleStateEachFrame *)&(pPlaybackBuffer[C][PlaybackIndex[C]]);
				Dist -= rage::Sqrtf( (pOldState->CoorsX - pNewState->CoorsX)*(pOldState->CoorsX - pNewState->CoorsX) +
									  (pOldState->CoorsY - pNewState->CoorsY)*(pOldState->CoorsY - pNewState->CoorsY) );
				if (Dist <= 0.0f) bDone = true;
			}
		}
		else
		{
			if (PlaybackIndex[C] <= (s32)(sizeof(CVehicleStateEachFrame)))
			{	// we're at the end of the recording. Call it a day.
				bDone = true;
			}
			else
			{
				pOldState = (CVehicleStateEachFrame *)&(pPlaybackBuffer[C][PlaybackIndex[C]]);
				PlaybackIndex[C] -= sizeof(CVehicleStateEachFrame);
				pNewState = (CVehicleStateEachFrame *)&(pPlaybackBuffer[C][PlaybackIndex[C]]);
				Dist += rage::Sqrtf( (pOldState->CoorsX - pNewState->CoorsX)*(pOldState->CoorsX - pNewState->CoorsX) +
									  (pOldState->CoorsY - pNewState->CoorsY)*(pOldState->CoorsY - pNewState->CoorsY) );
				if (Dist >= 0.0f) bDone = true;
			}
		}
	}

	PlaybackRunningTime[C] = (float)pNewState->TimeInRecording;

	if (bUseCarAI[C])
	{	// Actually place the car exactly on the path
		pNewState = (CVehicleStateEachFrame *) &(pPlaybackBuffer[C][PlaybackIndex[C]]);
		Matrix34 PreviousMatrix = MAT34V_TO_MATRIX34(pVehicleForPlayback[C]->GetMatrix());

		float playbackSpeed;
		if(bPlaybackPaused[C])
		{
			playbackSpeed = 0.0f;
		}
#if __DEV
		// If this is the debug recording as triggered by the widgets; allow user to change the playback speed with the slider.
		else if(debug_WidgetRecordingPlaybackVehicle == pVehicleForPlayback[C] && pPlaybackBuffer[C] == debug_pDebugBuffer)
		{
			playbackSpeed = debug_timeScale;
		}
#endif
		else
		{
			playbackSpeed = PlaybackSpeed[C];
		}

		CVehicleRecordingMgr::RestoreInfoForCar(pVehicleForPlayback[C], pNewState, false, playbackSpeed, true);

		// Code needed to stop the wheels applying silly forces.
		for (s32 i = 0; i < pVehicleForPlayback[C]->GetNumWheels(); i++)
		{
			pVehicleForPlayback[C]->GetWheel(i)->UpdateContactsAfterNetworkBlend(PreviousMatrix, MAT34V_TO_MATRIX34(pVehicleForPlayback[C]->GetMatrix()));
		}
	}

	aiVechicleRecordingFlags[C] |=  VRF_FirstUpdate;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : SkipTimeForwardInRecording()
// PURPOSE    :
///////////////////////////////////////////////////////////////////////////


void CVehicleRecordingMgr::SkipTimeForwardInRecording(class CVehicle *pCar, float Time)
{
	s32	C = 0;

	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pCar))
	{
		C++;
	}

	if (vehicleVerifyf(C < MAX_CARS_PLAYBACK_NUMBER, "Couldn't find recording to skip"))
	{
		PlaybackRunningTime[C] += Time;

		// Clip time to between 0 and the end of the recording.
		PlaybackRunningTime[C] = rage::Max(0.0f, PlaybackRunningTime[C]);

		u32 maxTime = ((CVehicleStateEachFrame*)&pPlaybackBuffer[C][PlaybackBufferSize[C] - sizeof(CVehicleStateEachFrame)])->TimeInRecording;
		PlaybackRunningTime[C] = rage::Min( ((float)maxTime), PlaybackRunningTime[C]);

		aiVechicleRecordingFlags[C] |=  VRF_FirstUpdate;
	}

}
u32 CVehicleRecordingMgr::GetEndTimeOfRecording(int index)
{
	Assertf( index < MAX_CARS_PLAYBACK_NUMBER, "Bad car playback index");
	return ((CVehicleStateEachFrame*)&pPlaybackBuffer[index][PlaybackBufferSize[index] - sizeof(CVehicleStateEachFrame)])->TimeInRecording;
}

float CVehicleRecordingMgr::GetPlaybackRunningTime(int index)
{
	Assertf( index < MAX_CARS_PLAYBACK_NUMBER, "Bad car playback index");
	return PlaybackRunningTime[index];
}

///////////////////////////////////////////////////////////////////////////
// NAME       : SetAdditionalRotationForRecordedVehicle()
// PURPOSE    :	This function allows the script to apply an additional
//				rotation to a vehicle that is playing back a recording.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SetAdditionalRotationForRecordedVehicle(class CVehicle *pVehicle, Vec3V_In axisAngleRotation)
{
	s32	C = 0;

	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pVehicle))
	{
		C++;
	}

	if (vehicleVerifyf(C < MAX_CARS_PLAYBACK_NUMBER, "SetAdditionalRotationForRecordedVehicle: This vehicle doesn't have a playback running on it"))
	{
		PlaybackAdditionalRotation[C] = axisAngleRotation;
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : SetLocalPositionOffsetForRecordedVehicle()
// PURPOSE    :	This function allows the script to apply a local
//				position offset to a vehicle that is playing back a recording.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SetLocalPositionOffsetForRecordedVehicle(class CVehicle *pVehicle, Vec3V_In localOffset)
{
	s32	C = 0;

	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pVehicle))
	{
		C++;
	}

	if (vehicleVerifyf(C < MAX_CARS_PLAYBACK_NUMBER, "SetAdditionalPositionForRecordingVehicle: This vehicle doesn't have a playback running on it"))
	{
		PlaybackLocalPositionOffset[C] = localOffset;
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : SetGlobalPositionOffsetForRecordedVehicle()
// PURPOSE    :	This function allows the script to apply a global
//				position offset to a vehicle that is playing back a recording.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SetGlobalPositionOffsetForRecordedVehicle(class CVehicle *pVehicle, Vec3V_In globalOffset)
{
	s32	C = 0;

	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pVehicle))
	{
		C++;
	}

	if (vehicleVerifyf(C < MAX_CARS_PLAYBACK_NUMBER, "SetAdditionalPositionForRecordingVehicle: This vehicle doesn't have a playback running on it"))
	{
		PlaybackGlobalPositionOffset[C] = globalOffset;
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : FindPositionInRecording()
// PURPOSE    : This function allows the script to find out how far into
//				the recording it is. The distance returned is in meters from
//				the start.
///////////////////////////////////////////////////////////////////////////

float CVehicleRecordingMgr::FindPositionInRecording(const CVehicle *pCar)
{
	s32	C;
	CVehicleStateEachFrame	*pLoopState, *pCurrentState, *pNextState;
	
	C = 0;
	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pCar))
	{
		C++;
	}
	
	Assertf(C < MAX_CARS_PLAYBACK_NUMBER, "FindPositionInRecording: This car doesn't have a playback running on it");
	if (C >= MAX_CARS_PLAYBACK_NUMBER)
		return 0.0f;

	// Set this to the current state in case nothing changes.
	pCurrentState = (CVehicleStateEachFrame *)&(pPlaybackBuffer[C][PlaybackIndex[C]]);
	pLoopState = (CVehicleStateEachFrame *)&(pPlaybackBuffer[C][0]);
	float	TotalDistance = 0;

	// Go through all the states and calculate the total distance.
	while (pCurrentState != pLoopState)
	{
		pNextState = pLoopState + 1;

		TotalDistance += rage::Sqrtf( (pNextState->CoorsX - pLoopState->CoorsX)*(pNextState->CoorsX - pLoopState->CoorsX) +
							 		   (pNextState->CoorsY - pLoopState->CoorsY)*(pNextState->CoorsY - pLoopState->CoorsY) );
		
		pLoopState = pNextState;

		Assert((void*)pNextState <= (void*)&(pPlaybackBuffer[C][PlaybackBufferSize[C]]));
	}

	return TotalDistance;
}


///////////////////////////////////////////////////////////////////////////
// NAME       : FindTimePositionInRecording()
// PURPOSE    : This function allows the script to find out how far into
//				the recording it is. The distance returned is in milliseconds from
//				the start.
///////////////////////////////////////////////////////////////////////////

float CVehicleRecordingMgr::FindTimePositionInRecording(const CVehicle *pCar)
{
	s32	C;
	
	C = 0;
	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pCar))
	{
		C++;
	}
	
	Assertf(C < MAX_CARS_PLAYBACK_NUMBER, "FindTimePositionInRecording: This car doesn't have a playback running on it");
	if (C < MAX_CARS_PLAYBACK_NUMBER)
		return PlaybackRunningTime[C];
	else
		return 0;
}


///////////////////////////////////////////////////////////////////////////
// NAME       : SetRecordingToPointNearestToCoors()
// PURPOSE    : This function allows the script to find out how far into
//				the recording it is. The distance returned is in milliseconds from
//				the start. This command applies to the recording being recorded at the moment.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::SetRecordingToPointNearestToCoors(class CVehicle *pCar, Vector3 *pPos)
{

	s32	C;

	C = 0;
	while (C < MAX_CARS_PLAYBACK_NUMBER && ( (!bPlaybackGoingOn[C]) || pVehicleForPlayback[C] != pCar))
	{
		C++;
	}

	Assertf(C < MAX_CARS_PLAYBACK_NUMBER, "SetRecordingToPointNearestToCoors: This car doesn't have a playback running on it");
	if (C >= MAX_CARS_PLAYBACK_NUMBER)
		return;

	SetRecordingToPointClosestToCoors(C, *pPos);
}

///////////////////////////////////////////////////////////////////////////
// NAME       : GetTransformOfCarRecordingAtTime()
// PURPOSE    : Helper function that gets the full transform at a given time
//				in the recording.
///////////////////////////////////////////////////////////////////////////

void 	CVehicleRecordingMgr::GetTransformOfCarRecordingAtTime(int index, float time, Matrix34 &RetVal )
{
	Assert(index >= 0 && index < GetMaxNumOfPlaybackFiles());
	if (!aiVerifyf(sm_StreamingArray[index].HasRecordingData(), "GET_TRANSFORM_OF_CAR_RECORDING_AT_TIME is looking at a recording that is not streamed in" ))
	{
		RetVal.Identity();
		return;
	}

	sm_StreamingArray[index].GetTransformOfCarRecordingAtTime(time, RetVal);
}


///////////////////////////////////////////////////////////////////////////
// NAME       : GetPositionOfCarRecordingAtTime()
// PURPOSE    : This function will returns the position in a recording so that the
//				level designers can make sure they place cars that are not in view
///////////////////////////////////////////////////////////////////////////

 void 	CVehicleRecordingMgr::GetPositionOfCarRecordingAtTime(int index, float time, Vector3 &RetVal)
{
	Assert(index >= 0 && index < GetMaxNumOfPlaybackFiles());
	if (!aiVerifyf(sm_StreamingArray[index].HasRecordingData(), "GET_POSITION_OF_CAR_RECORDING_AT_TIME is looking at a recording that is not streamed in" ))
	{
		RetVal.Zero();
		return;
	}

	sm_StreamingArray[index].GetPositionOfCarRecordingAtTime(time, RetVal);
}

 ///////////////////////////////////////////////////////////////////////////
 // NAME       : GetRotationOfCarRecordingAtTime()
 // PURPOSE    : This function will returns the rotation in a recording so that the
 //				level designers can get access to YPR info
 ///////////////////////////////////////////////////////////////////////////

 void 	CVehicleRecordingMgr::GetRotationOfCarRecordingAtTime(int index, float time, Vector3 &RetVal)
 {
	 Matrix34 mat;
	 GetTransformOfCarRecordingAtTime( index, time, mat );
	 // We want this to match get_entity_rotation
	 RetVal = CScriptEulers::MatrixToEulers(mat, EULER_YXZ);
	 RetVal *= RtoD;
 }


 ///////////////////////////////////////////////////////////////////////////
 // NAME       : GetTotalDurationOfCarRecording()
 // PURPOSE    : Returns the total duration in milliseconds of a car recording
 ///////////////////////////////////////////////////////////////////////////

float 	CVehicleRecordingMgr::GetTotalDurationOfCarRecording(int index)
{
	Assert(index >= 0 && index < GetMaxNumOfPlaybackFiles());
	Assertf(sm_StreamingArray[index].HasRecordingData(), "GET_TOTAL_DURATION_OF_CAR_RECORDING is looking at a recording that is not streamed in" );

	if( index >= 0 && 
		index < GetMaxNumOfPlaybackFiles() &&
		sm_StreamingArray[ index ].HasRecordingData() )
	{
		return sm_StreamingArray[ index ].GetTotalDurationOfCarRecording();
	}

	return 0.0f;
}


#if !__FINAL
///////////////////////////////////////////////////////////////////////////
// NAME       : FindTimePositionInRecordedRecording()
// PURPOSE    : This function allows the script to find out how far into
//				the recording it is. The distance returned is in milliseconds from
//				the start. This command applies to the recording being recorded at the moment.
///////////////////////////////////////////////////////////////////////////

float CVehicleRecordingMgr::FindTimePositionInRecordedRecording(const class CVehicle *pCar)
{
	s32	C;

	C = 0;
	while(C < MAX_CARS_RECORDED_NUMBER && ( (!ms_vehiclesRecordings[C].IsRecording() || ms_vehiclesRecordings[C].GetVehicle() != pCar)))
	{
		C++;
	}

	Assertf(C < MAX_CARS_RECORDED_NUMBER, "FindPositionInRecordedRecording: This car doesn't have a playback running on it");

	return ms_vehiclesRecordings[C].FindTimePositionInRecording();
}
#endif


///////////////////////////////////////////////////////////////////////////
// NAME       : TryToTurnAIRecordingBackIntoFull()
// PURPOSE    : If so specified by the level designer an car recording that is using ai (not a strict one)
//				will try to revert to being a full recording if the car is close enough to it.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::TryToTurnAIRecordingBackIntoFull(s32 C)
{
	CVehicle *pVeh = pVehicleForPlayback[C];
	Vector3  Coors = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
		// First we find the point nearest the vehicle on the recording that we consider latching on to.
	s32	IndexInRecording = 0;
	s32	NearestIndexInRecording = -1;
	u32	NearestPlaybackRunningTime = 0;
	float fSpeedSqrAtTimeInRecording = -1.0f;

	// Extend the switch back time whilst still in contact with vehicles
	if(pVeh->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_VEHICLE))
	{
		TurnBackIntoFullPlayBackTime[C] = rage::Max(fwTimer::GetTimeInMilliseconds()+500, TurnBackIntoFullPlayBackTime[C]);
	}

	if( fwTimer::GetTimeInMilliseconds() < TurnBackIntoFullPlayBackTime[C] )
	{
		return;
	}

	float	NearestDist = 999999.9f;
	while (IndexInRecording < PlaybackBufferSize[C])
	{
		Vector3 vRot;
		Matrix34 mat;

		CVehicleStateEachFrame *pVehState = (CVehicleStateEachFrame *) &((pPlaybackBuffer[C])[IndexInRecording]);

		RestoreInfoForMatrix( mat, pVehState);
		mat.ToEulersXYZ( vRot );

		const float fTheta = Abs(fwAngle::LimitRadianAngle(vRot.z - pVeh->GetTransform().GetHeading()));
		if( fTheta <= (EIGHTH_PI / 2.0f) )	//one-sixteenth pi
		{
			float Distance = (Coors - Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ) ).Mag();
			if (Distance < NearestDist)
			{
				NearestIndexInRecording = IndexInRecording;
				NearestPlaybackRunningTime = pVehState->TimeInRecording;
				NearestDist = Distance;
				const Vector3 vVelocityAtTimeInRecording(pVehState->GetSpeedX(), pVehState->GetSpeedY(), pVehState->GetSpeedZ());
				float playbackSpeed = GetPlaybackSpeed(C);
				fSpeedSqrAtTimeInRecording = vVelocityAtTimeInRecording.Mag2()*(playbackSpeed*playbackSpeed);
			}
		}

		IndexInRecording += sizeof(CVehicleStateEachFrame);
	}

	//only join back to the recording if our velocity within some acceptable
	//threshold from the target velocity, otherwise we'll notice a visible
	//immediate change in velocity
	const Vector3& velocity = pVeh->GetVelocity();
	const float actualSpeedSqr = velocity.Mag2();
	static dev_float s_fSpeedThresholdSqr = 0.5f * 0.5f;
	const bool bGoingFastEnough = actualSpeedSqr > fSpeedSqrAtTimeInRecording * s_fSpeedThresholdSqr;

	if (NearestDist < 0.5f && bGoingFastEnough)
	{		// Let's go for it. We turn the recording back into a full recording.
		PlaybackIndex[C] = NearestIndexInRecording;
		PlaybackRunningTime[C] = (float)NearestPlaybackRunningTime;
		TurnBackIntoFullPlayBackTime[C] = 0;
		bUseCarAI[C] = false;
		pVehicleForPlayback[C]->m_nVehicleFlags.bLerpToFullRecording = pVehicleForPlayback[C]->m_nVehicleFlags.bShouldLerpFromAiToFullRecording;
	}
}


///////////////////////////////////////////////////////////////////////////
// NAME       : Render()
// PURPOSE    : This function will render the recording as a bunch of lines.
//				Should be handy for debugging and also for certain missions (like
//				showing how the player should fly a stunt)
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::Render()
{

#if DEBUG_DRAW
	u32		PlayBackIndex, NextPlayBackIndex;
	CVehicleStateEachFrame *pVehState;
	s32		OffsetBehind, OffsetInFront;
	bool		bPreviousPointStored = false;
	Vector3		vPreviousPointStored;

	for (s32 C = 0; C < MAX_CARS_PLAYBACK_NUMBER; C++)
	{
		if (bPlaybackGoingOn[C])
		{
#if __BANK	// DW - unfortunate fix for compile error in profile build... could be better if just use __BANK?
#if !__FINAL
			if (DisplayMode[C] != RDM_NONE || bRenderAllRecordings)
#else
			if (DisplayMode[C] != RDM_NONE)
#endif
#else
			if (DisplayMode[C] != RDM_NONE)
#endif

			{
				s32 DisplayModeFinal = DisplayMode[C];

#if __BANK	// DW - unfortunate fix for compile error in profile build... could be better if just use __BANK?
#if !__FINAL
				if (bRenderAllRecordings && audVehicleAudioEntity::ShouldRenderRecording(C))
				{
					DisplayModeFinal = RDM_WHOLELINE;
				}
#endif
#endif

				Color32 LineColour;
				if (C < FOR_CODE_INDEX_LAST)
				{
					LineColour = Color32(255, 255, 0, 255);		// This one is for a code recording (buses)
				}
				else
				{
					LineColour = Color32(255, 0, 0, 255);
				}

				switch(DisplayModeFinal)
				{
					case RDM_WHOLELINE:
							  			  // Set the appropriate rendermode
						PlayBackIndex = 0;
						while (PlayBackIndex < PlaybackBufferSize[C] - sizeof(CVehicleStateEachFrame))
						{
							NextPlayBackIndex = PlayBackIndex + sizeof(CVehicleStateEachFrame);
							pVehState = (CVehicleStateEachFrame *) &((CVehicleRecordingMgr::pPlaybackBuffer[C])[PlayBackIndex]);

								// Draw the line segment between this point and the last.
							if (bPreviousPointStored)
							{
								grcDebugDraw::Line(vPreviousPointStored, 
													Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ),
													LineColour);
							}
							vPreviousPointStored.x = pVehState->CoorsX;
							vPreviousPointStored.y = pVehState->CoorsY;
							vPreviousPointStored.z = pVehState->CoorsZ;
							bPreviousPointStored = true;


	
							PlayBackIndex = NextPlayBackIndex;
						}
						break;
				
					case RDM_JUSTINFRONT:
					case RDM_JUSTBEHIND:
					case RDM_AROUNDVEHICLE:
						#define	SAMPLESAROUND (20)
						OffsetBehind = OffsetInFront = 0;
						
						if (DisplayModeFinal == RDM_JUSTBEHIND || DisplayModeFinal == RDM_AROUNDVEHICLE)
						{
							OffsetBehind = -SAMPLESAROUND;
						}
						if (DisplayModeFinal == RDM_JUSTINFRONT || DisplayModeFinal == RDM_AROUNDVEHICLE)
						{
							OffsetInFront = SAMPLESAROUND;
						}
						
						for (s32 Samples = OffsetBehind; Samples < OffsetInFront; Samples++)
						{
							s32 PointPlayBackIndex = PlaybackIndex[C];
							PointPlayBackIndex += Samples * sizeof(CVehicleStateEachFrame);
							if (PointPlayBackIndex > 0 && PointPlayBackIndex < PlaybackBufferSize[C])
							{
								float	FadeVal = 1.0f - (ABS(Samples) / float(SAMPLESAROUND));
							
								pVehState = (CVehicleStateEachFrame *) &((CVehicleRecordingMgr::pPlaybackBuffer[C])[PointPlayBackIndex]);

								if (bPreviousPointStored)
								{
									grcDebugDraw::Line(vPreviousPointStored, 
														Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ),
														Color32((u8)(100 * FadeVal), (u8)(100 * FadeVal), (u8)(50 * FadeVal), 200));
								}
								vPreviousPointStored.x = pVehState->CoorsX;
								vPreviousPointStored.y = pVehState->CoorsY;
								vPreviousPointStored.z = pVehState->CoorsZ;
								bPreviousPointStored = true;

							}

						
						}
					
						
						break;

					default:
						break;

				}
			}
		}
	}
#endif	// DEBUG_DRAW

}


///////////////////////////////////////////////////////////////////////////
// NAME       : FindRecordingFile()
// PURPOSE    : This function is used by the CVehicleRecordingStreamingModule::FindSlot 
//				it will return -1 if not found
///////////////////////////////////////////////////////////////////////////

s32 CVehicleRecordingMgr::FindRecordingFile(const char *pFilename)
{
	s32 index;

	Assertf(strlen(pFilename) < CAR_RECORDING_NAME_LENGTH, "%s:RecordingFileName too long", pFilename); 

	CVehicleRecordingStreaming* pRecordingInfo = CVehicleRecordingMgr::GetRecordingInfo(pFilename, &index);

	if(pRecordingInfo)//there is already a car recording with this name
		return index;

	return -1;
}



///////////////////////////////////////////////////////////////////////////
// NAME       : RegisterRecordingFile()
// PURPOSE    : This function is called by Adam and will inform the record code that
//				a file has been found in the .img file.
///////////////////////////////////////////////////////////////////////////

s32 CVehicleRecordingMgr::RegisterRecordingFile(const char *pFilename)
{
	s32 index;

	Assertf(strlen(pFilename) < CAR_RECORDING_NAME_LENGTH, "%s:RecordingFileName too long", pFilename); 

	CVehicleRecordingStreaming* pRecordingInfo = CVehicleRecordingMgr::GetRecordingInfo(pFilename, &index);

	if(pRecordingInfo)//there is already a car recording with this name
		return index;

	Assertf(sm_NumPlayBackFiles < GetMaxNumOfPlaybackFiles(), "Ran out of Recordinginfos (%d)", GetMaxNumOfPlaybackFiles());

	if ( !(sm_NumPlayBackFiles < GetMaxNumOfPlaybackFiles()) )
		return -1;

	sm_StreamingArray[sm_NumPlayBackFiles].RegisterRecordingFile(pFilename);

	// Add recordinginfo hash key into map (duplicates are caught above)
	Assertf(sm_RecordInfoMap.Lookup(sm_StreamingArray[sm_NumPlayBackFiles].GetHashKey()) == -1,"Vehicle recording %s is duplicated",pFilename);
	sm_RecordInfoMap.Insert(sm_StreamingArray[sm_NumPlayBackFiles].GetHashKey(),sm_NumPlayBackFiles);
	sm_NumPlayBackFiles++;

	return sm_NumPlayBackFiles-1;
}


///////////////////////////////////////////////////////////////////////////
// NAME       : RemoveSlot()
// PURPOSE    : Eliminate the slot and make it available again.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::RemoveSlot(int index)
{
	sm_RecordInfoMap.Delete(sm_StreamingArray[index].GetHashKey());
	sm_StreamingArray[index].Invalidate();
}


///////////////////////////////////////////////////////////////////////////
// NAME       : FindIndexWithFileNameNumber()
// PURPOSE    : Given the number of the filename carrec012.rrr this will find the
//				index into the streaming array.
///////////////////////////////////////////////////////////////////////////

s32 CVehicleRecordingMgr::FindIndexWithFileNameNumber(s32 ArgFileNameNumber, const char* pRecordingName)
{
	char fileName[64];
	sprintf(fileName, "%s%03d", pRecordingName, ArgFileNameNumber);
	s32 index = GetRecordingIndex(fileName);
	return index;
}


///////////////////////////////////////////////////////////////////////////
// NAME       : HasRecordingFileBeenLoaded()
// PURPOSE    : To be called by the level designers so that they can work out
//				whether a recording file has been streamed in.
///////////////////////////////////////////////////////////////////////////

bool CVehicleRecordingMgr::HasRecordingFileBeenLoaded(int index)
{
	if(index != -1)
		return (sm_StreamingArray[index].HasRecordingData());
	return false;
}


//
// name:		CVehicleRecordingMgr::Remove
// description:	Remove vehicle recording
void CVehicleRecordingMgr::Remove(s32 index)
{
	sm_StreamingArray[index].Remove();
}


///////////////////////////////////////////////////////////////////////////
// NAME       : SmoothRecording()
// PURPOSE    : Recordings can be a bit jittery when recorded. This function smoothes
//				the time steps out a little bit.
///////////////////////////////////////////////////////////////////////////

/*void CVehicleRecordingMgr::SmoothRecording(s32 Index)
{
	u8	*pRecording = sm_StreamingArray[Index].pRecordingData;
	Assert(pRecording);

	s32	IndexInArray = sizeof(CVehicleStateEachFrame);
	while (IndexInArray < sm_StreamingArray[Index].RecordingDataSize - (s32)sizeof(CVehicleStateEachFrame))
	{
		CVehicleStateEachFrame	*pOldState = (CVehicleStateEachFrame *) &(pRecording[IndexInArray-sizeof(CVehicleStateEachFrame)]);
		CVehicleStateEachFrame	*pCurrentState = (CVehicleStateEachFrame *) &(pRecording[IndexInArray]);
		CVehicleStateEachFrame	*pNewState = (CVehicleStateEachFrame *) &(pRecording[IndexInArray+sizeof(CVehicleStateEachFrame)]);

		pCurrentState->TimeInRecording = (u32)((pOldState->TimeInRecording + pNewState->TimeInRecording) * 0.5f);

		IndexInArray += sizeof(CVehicleStateEachFrame);
	}
}*/



///////////////////////////////////////////////////////////////////////////
// NAME       : FindPlaybackRunningTimeAccordingToSchedule()
// PURPOSE    : Depending on the time of day and the schedule of this recording this
//				function calculates the current position within the recording.
///////////////////////////////////////////////////////////////////////////

u32 CVehicleRecordingMgr::FindPlaybackRunningTimeAccordingToSchedule(s32 Recording, u32 MillisecondsOffset)
{
	// Calculate the milliseconds that have passed since midnight.
	int MinutesInRoute = CClock::GetMinutesSinceMidnight() + Recording * 3;
#define LOOP_TIME (1 * 60)	// This route loops every 1 hour
	while (MinutesInRoute > LOOP_TIME)
	{
		MinutesInRoute -= LOOP_TIME;
	}
	
	u32	MilliSecondsInRoute = (u32)(MinutesInRoute * CClock::GetMsPerGameMinute());
	MilliSecondsInRoute += MillisecondsOffset;

	Assert(pPlaybackBuffer[Recording]);	// Make sure recording has been loaded.
	u32	RecordingLength = (((CVehicleStateEachFrame *)&pPlaybackBuffer[Recording][PlaybackBufferSize[Recording]])-1)->TimeInRecording;

	MilliSecondsInRoute = rage::Min(MilliSecondsInRoute, RecordingLength);

	return MilliSecondsInRoute;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : FindTimeJumpRequiredToGetToNextStaticPoint()
// PURPOSE    : Finds out how nuch time needs to expire get to the next point in this
//				recording where the vehicle is static.
//				This could be the next bus stop.
// RETURNS    : Time jump in game minutes
///////////////////////////////////////////////////////////////////////////

s32 CVehicleRecordingMgr::FindTimeJumpRequiredToGetToNextStaticPoint(s32 C)
{
	s32 PlayBackIndex = CVehicleRecordingMgr::PlaybackIndex[C];
	s32 PlayBackIndexAhead;
	CVehicleStateEachFrame *pVehStateAhead = NULL;
	// We consider the vehicle static if it doesn't move more than a meter in 5 seconds.
	s32 Attempts = PlaybackBufferSize[C] / sizeof(CVehicleStateEachFrame);

	while (Attempts > 0)
	{
		PlayBackIndex += sizeof(CVehicleStateEachFrame);
		if (PlayBackIndex >= PlaybackBufferSize[C])
		{
			PlayBackIndex = 0;
		}
		CVehicleStateEachFrame *pVehState = (CVehicleStateEachFrame *) &((pPlaybackBuffer[C])[PlayBackIndex]);

		// Find the point in the recording that is 5 meter ahead.
		PlayBackIndexAhead = PlayBackIndex;

		s32	TimeToBridge = 5000;
		s32	RunningTime = pVehState->TimeInRecording;

		while (TimeToBridge > 0)
		{
			PlayBackIndexAhead += sizeof(CVehicleStateEachFrame);
			if (PlayBackIndexAhead >= PlaybackBufferSize[C])
			{
				PlayBackIndexAhead = 0;
				RunningTime = 0;
			}
	
			pVehStateAhead = (CVehicleStateEachFrame *) &((pPlaybackBuffer[C])[PlayBackIndexAhead]);

			TimeToBridge -= pVehStateAhead->TimeInRecording - RunningTime;
			RunningTime = pVehStateAhead->TimeInRecording;
		}
		Vector3 VecNow = Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ); 
		Assert(pVehStateAhead);
		Vector3 VecAhead = Vector3(pVehStateAhead->CoorsX, pVehStateAhead->CoorsY, pVehStateAhead->CoorsZ);

					// If the coordinates haven't moved a meter or more we have found the next static bit in the recording
		if ( (VecNow-VecAhead).Mag() < 1.0f)
		{
			CVehicleStateEachFrame *pVehStartState = (CVehicleStateEachFrame *) &((pPlaybackBuffer[C])[PlaybackIndex[C]]);

			s32 ExpiredTimeInMilliseconds = pVehState->TimeInRecording - pVehStartState->TimeInRecording;
			if (ExpiredTimeInMilliseconds < 0)
			{
				CVehicleStateEachFrame *pVehLastState = (CVehicleStateEachFrame *) &((pPlaybackBuffer[C])[PlaybackBufferSize[C] - sizeof(CVehicleStateEachFrame)]);
				ExpiredTimeInMilliseconds += pVehLastState->TimeInRecording;
				Assert(ExpiredTimeInMilliseconds >= 0);

					// Find out how many game minutes correspond to the milliseconds found.
				return ExpiredTimeInMilliseconds / CClock::GetMsPerGameMinute();
			}
		}
		Attempts--;
	}
	Assertf(0, "FindTimeJumpRequiredToGetToNextStaticPoint didn't find a static point in the recording");
	return 0;
}



void	Swap4Bytes(void *pPointer)
{
	u8	*pPtr = (u8*)pPointer;
	u8	Temp = pPtr[0];
	pPtr[0] = pPtr[3];
	pPtr[3] = Temp;
	Temp = pPtr[1];
	pPtr[1] = pPtr[2];
	pPtr[2] = Temp;
}

void	Swap2Bytes(void *pPointer)
{
	u8	*pPtr = (u8*)pPointer;
	u8	Temp = pPtr[0];
	pPtr[0] = pPtr[1];
	pPtr[1] = Temp;
}

///////////////////////////////////////////////////////////////////////////
// NAME       : EndianSwap
// PURPOSE    : Fixes the endian order from 360 to Pc and the other way around
///////////////////////////////////////////////////////////////////////////

void	CVehicleStateEachFrame::EndianSwap()
{
	Swap4Bytes(&TimeInRecording);
	Swap4Bytes(&CoorsX);
	Swap4Bytes(&CoorsY);
	Swap4Bytes(&CoorsZ);
	Swap2Bytes(&SpeedX);
	Swap2Bytes(&SpeedY);
	Swap2Bytes(&SpeedZ);
}


/////////////// Stuff from here has to do with converting files to ascii and back.
/////////////// The ascii files can be manipulated in Max.
#if !__FINAL

///////////////////////////////////////////////////////////////////////////
// NAME       : ConvertFilesToAscii()
// PURPOSE    : Goes through all the carrecXXX.rrr files and converts them into
//				carrecXXX.aaa (ascii)
///////////////////////////////////////////////////////////////////////////

/*void CVehicleRecordingMgr::ConvertFilesToAscii()
{
	FileHandle	fileIn;
	FileHandle	fileOut;
	char		FileNameIn[32];
	char		FileNameOut[32];

	Displayf("Converting the carrec files from .rrr to .aaa.\n");
	CFileMgr::SetDir(DEFAULT_RECORDING_PATH);

	atArray<fiFindData*> results;
	ASSET.EnumFiles(DEFAULT_RECORDING_PATH, CVehicleRecordingMgr::FindFileCallback, &results);


	//for (s32 NameNumber = 0; NameNumber < HIGHESTFILENUMALLOWED; NameNumber++)
	for(s32 i = 0 ; i < results.GetCount(); i++)
	{	
		const char* fileNameAndExtension = ASSET.FileName(results[i]->m_Name);
		char* pExtension = const_cast<char*>(ASSET.FindExtensionInPath(fileNameAndExtension));

		if(pExtension != NULL && (stricmp(pExtension, ".rrr")) )
		{
			// Remove the extension leaving just the filename
			char fileName[256];
			ASSET.RemoveExtensionFromPath(fileName, 256, fileNameAndExtension);

			//sprintf(FileNameIn, "carrec%03d.rrr", NameNumber);
			//sprintf(FileNameOut, "carrec%03d.aaa", NameNumber);
			sprintf(FileNameIn, "%s.rrr", fileName);
			sprintf(FileNameOut, "%s.aaa", fileName);

			fileIn = CFileMgr::OpenFile(FileNameIn);
			if (CFileMgr::IsValidFileHandle(fileIn))
			{	// File exists.
				fileOut = CFileMgr::OpenFileForWriting(FileNameOut);
				Assertf(CFileMgr::IsValidFileHandle(fileOut), "%s:Could not open file", FileNameOut);
				ConvertFileFromRRRToAAA(fileIn, fileOut);
				CFileMgr::CloseFile(fileOut);
				CFileMgr::CloseFile(fileIn);
			}
		}
	}
	CFileMgr::SetDir("");

	Displayf("Done.\n");
}

void CVehicleRecordingMgr::FindFileCallback(const fiFindData &data, void *user)
{
	Assert(user);
	atArray<fiFindData*> *results = reinterpret_cast<atArray<fiFindData*>*>(user);
	results->PushAndGrow(rage_new fiFindData(data));
}

///////////////////////////////////////////////////////////////////////////
// NAME       : ConvertFilesFromAscii()
// PURPOSE    : Goes through all the carrecXXX.aaa (ascii) files and converts them into
//				carrecXXX.rrr (binary)
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::ConvertFilesFromAscii()
{
	FileHandle	fileIn; 
	FileHandle	fileOut;
	char		FileNameIn[32];
	char		FileNameOut[32];

	Displayf("Converting the carrec files from .aaa to .rrr.\n");
	CFileMgr::SetDir(DEFAULT_RECORDING_PATH);

	atArray<fiFindData*> results;
	ASSET.EnumFiles(DEFAULT_RECORDING_PATH, CVehicleRecordingMgr::FindFileCallback, &results);

	//for (s32 NameNumber = 0; NameNumber < HIGHESTFILENUMALLOWED; NameNumber++)
	for(s32 i = 0 ; i < results.GetCount(); i++)
	{
		const char* fileNameAndExtension = ASSET.FileName(results[i]->m_Name);
		char* pExtension = const_cast<char*>(ASSET.FindExtensionInPath(fileNameAndExtension));

		if(pExtension != NULL && (stricmp(pExtension, ".aaa")) )
		{
			// Remove the extension leaving just the filename
			char fileName[256];
			ASSET.RemoveExtensionFromPath(fileName, 256, fileNameAndExtension);

			//sprintf(FileNameIn, "carrec%03d.aaa", NameNumber);
			//sprintf(FileNameOut, "carrec%03dnew.rrr", NameNumber);
			sprintf(FileNameIn, "%s.aaa", fileName);
			sprintf(FileNameOut, "%s.rrr", fileName);

			fileIn = CFileMgr::OpenFile(FileNameIn);
			if( CFileMgr::IsValidFileHandle(fileIn) )
			{	// File exists.
				fileOut = CFileMgr::OpenFileForWriting(FileNameOut);
				Assertf(CFileMgr::IsValidFileHandle(fileOut), "%s:Could not open file", FileNameOut);
				ConvertFileFromAAAToRRR(fileIn, fileOut);
				CFileMgr::CloseFile(fileOut);
				CFileMgr::CloseFile(fileIn);
			}
		}
	}
	CFileMgr::SetDir("");

	Displayf("Done.\n");
}



///////////////////////////////////////////////////////////////////////////
// NAME       : ConvertFileFromRRRToAAA()
// PURPOSE    : Converts just this one file.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::ConvertFileFromRRRToAAA(FileHandle fileRRR, FileHandle fileAAA, bool bAlsoHeading)
{
	CVehicleStateEachFrame	VehState;
	Matrix34				VehMatrix;
	char writeString[1024];
	
	while (CFileMgr::Read(fileRRR, (char *)&VehState, sizeof(CVehicleStateEachFrame)))
	{
#if __BE
		EndianSwapRecording((u8*)&VehState, sizeof(CVehicleStateEachFrame));
#endif


		float	TimeVal = VehState.TimeInRecording*0.001f;
		sprintf(writeString, "%.4f\r\n", TimeVal);
		CFileMgr::Write(fileAAA, writeString, strlen(writeString));

		sprintf(writeString, "%.2f %.2f %.2f\r\n", VehState.CoorsX, VehState.CoorsY, VehState.CoorsZ);
		CFileMgr::Write(fileAAA, writeString, strlen(writeString));

		RestoreInfoForMatrix(VehMatrix, &VehState);
		sprintf(writeString, "%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\r\n", VehMatrix.a.x, VehMatrix.b.x, VehMatrix.c.x, VehMatrix.a.y, VehMatrix.b.y, VehMatrix.c.y, VehMatrix.a.z, VehMatrix.b.z, VehMatrix.c.z);
		CFileMgr::Write(fileAAA, writeString, strlen(writeString));

		if (bAlsoHeading)
		{
			float 	fHeading = ( RtoD * rage::Atan2f(-VehMatrix.b.x, VehMatrix.b.y));

			if (fHeading < 0.0f)
			{
				fHeading += 360.0f;
			}		
			if (fHeading > 360.0f)
			{
				fHeading -= 360.0f;
			}
		
			sprintf(writeString, "%.4f\r\n", fHeading);
			CFileMgr::Write(fileAAA, writeString, strlen(writeString));
		}

		sprintf(writeString, "%d %d %d %d %d %d %d\r\n", VehState.SteerAngle, VehState.Gas, VehState.Brake, VehState.HandBrake, VehState.SpeedX, VehState.SpeedY, VehState.SpeedZ);
		CFileMgr::Write(fileAAA, writeString, strlen(writeString));

		sprintf(writeString, "\n");
		CFileMgr::Write(fileAAA, writeString, strlen(writeString));
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : ConvertFileFromAAAToRRR()
// PURPOSE    : Converts just this one file.
///////////////////////////////////////////////////////////////////////////

void CVehicleRecordingMgr::ConvertFileFromAAAToRRR(FileHandle fileAAA, FileHandle fileRRR)
{
	CVehicleStateEachFrame	VehState;
	Matrix34				VehMatrix;
	float					fTemp;
	s32					iTemp1, iTemp2, iTemp3, iTemp4, iTemp5, iTemp6, iTemp7;
	char tempString[1024];

	while (CFileMgr::ReadLine(fileAAA, tempString, 1024))
	{
		// Deal with blank line
		if (strlen(tempString) <= 0)
		{
			if (!CFileMgr::ReadLine(fileAAA, tempString, 1024))
			{
				return;
			}
		}
	
		sscanf(tempString, "%f", &fTemp);
		VehState.TimeInRecording = (s32)(1000.0f * fTemp);

		CFileMgr::ReadLine(fileAAA, tempString, 1024);
		sscanf(tempString, "%f %f %f", &VehState.CoorsX, &VehState.CoorsY, &VehState.CoorsZ);

		CFileMgr::ReadLine(fileAAA, tempString, 1024);
		sscanf(tempString, "%f %f %f %f %f %f %f %f %f", &VehMatrix.a.x, &VehMatrix.b.x, &VehMatrix.c.x, &VehMatrix.a.y, &VehMatrix.b.y, &VehMatrix.c.y, &VehMatrix.a.z, &VehMatrix.b.z, &VehMatrix.c.z);
		StoreInfoForMatrix(&VehMatrix, &VehState);

		CFileMgr::ReadLine(fileAAA, tempString, 1024);
		sscanf(tempString, "%d %d %d %d %d %d %d\n", &iTemp1, &iTemp2, &iTemp3, &iTemp4, &iTemp5, &iTemp6, &iTemp7);
		VehState.SteerAngle = (s8)(iTemp1);
		VehState.Gas = (s8)(iTemp2);
		VehState.Brake = (s8)(iTemp3);
		VehState.HandBrake = (s8)(iTemp4);
		VehState.SpeedX = (s16)(iTemp5);
		VehState.SpeedY = (s16)(iTemp6);
		VehState.SpeedZ = (s16)(iTemp7);
#if __BE
		EndianSwapRecording((u8*)&VehState, sizeof(CVehicleStateEachFrame));
#endif

		CFileMgr::Write(fileRRR, (char *)&VehState, sizeof(CVehicleStateEachFrame));
	}
}
*/

#endif


//
//
//
s32 CVehicleRecordingMgr::GetRecordingIndex(const char* pName)
{
	return GetRecordingIndexFromHashKey(atStringHash(pName));
}

//
//
//
s32 CVehicleRecordingMgr::GetRecordingIndexFromHashKey(u32 key)
{
	return sm_RecordInfoMap.Lookup(key);
}



//
//        name: GetRecordInfo
// description: Get a pointer to a recordingInfo class from a name
//
CVehicleRecordingStreaming* CVehicleRecordingMgr::GetRecordingInfo(const char *pName, s32* pReturnIndex)
{
	s32 idx = sm_RecordInfoMap.Lookup(atStringHash(pName));
	if(idx!=-1)
	{
		if(pReturnIndex)
			*pReturnIndex = idx;
		return &sm_StreamingArray[idx];
	}
	return NULL;
}

//
// name:		GetRecordingInfoFromHashKey
// description:	Get recordinginfo pointer from a hash key identifier
CVehicleRecordingStreaming* CVehicleRecordingMgr::GetRecordingInfoFromHashKey(u32 key, s32* pReturnIndex)
{
	s32 idx = sm_RecordInfoMap.Lookup(key);
	if(idx!=-1)
	{
		if(pReturnIndex)
			*pReturnIndex = idx;
		return &sm_StreamingArray[idx];
	}
	return NULL;
}

#if __BANK
#if !__FINAL

void CVehicleRecordingMgr::SetCurrentRecording(const char* name,u32 num)
{
	formatf(ms_debug_RecordingNameToPlayWith,"%s",name);
	ms_debug_RecordingNumToPlayWith = num;
}

void TidyOldDebugBuffer()
{
	if (debug_pDebugBuffer)		// Tidy up the old buffer
	{
		delete [] debug_pDebugBuffer;
		debug_pDebugBuffer = NULL;
	}
}


void StartRecordingPlayerVehicle()
{
	if (!debug_WidgetRecordingPlaying && FindPlayerVehicle())
	{
		CVehicleRecordingMgr::StartRecordingCar(FindPlayerVehicle(), CVehicleRecordingMgr::ms_debug_RecordingNumToPlayWith, CVehicleRecordingMgr::ms_debug_RecordingNameToPlayWith, true);
		debug_WidgetRecordingPlaying = true;
	}
}

void StartRecordingPlayerVehicleTransitionFromPlayback()
{
	if (!debug_WidgetRecordingPlaying && FindPlayerVehicle())
	{
		CVehicleRecordingMgr::StartRecordingCarTransitionFromPlayback(FindPlayerVehicle(), CVehicleRecordingMgr::ms_debug_RecordingNumToPlayWith, CVehicleRecordingMgr::ms_debug_RecordingNameToPlayWith, true);
		debug_WidgetRecordingPlaying = true;
	}
}

void StopRecordingPlayerVehicle()
{
	if (debug_WidgetRecordingPlaying)
	{
		TidyOldDebugBuffer();

		CVehicleRecordingMgr::StopRecordingCar(FindPlayerVehicle(), &debug_pDebugBuffer, &debug_bufferSize);
		debug_WidgetRecordingPlaying = false;
	}
}

void StopRecordingPlayback()
{
	if(debug_WidgetRecordingPlaybackVehicle)
	{
		CVehicleRecordingMgr::StopPlaybackRecordedCar(debug_WidgetRecordingPlaybackVehicle);
		debug_WidgetRecordingPlaybackVehicle = NULL;
	}
}

void PlaybackRecordingUsingPlayerVehicle()
{
	taskAssertf(debug_pDebugBuffer, "No recording loaded, load recording first");
	if (debug_pDebugBuffer)
	{
		StopRecordingPlayback();
		debug_WidgetRecordingPlaybackVehicle = FindPlayerVehicle();
		if(debug_WidgetRecordingPlaybackVehicle)
		{
			CVehicleRecordingMgr::StartPlaybackRecordedCar(debug_WidgetRecordingPlaybackVehicle, -1, false, 20, DMode_StopForCars, false, -1, debug_pDebugBuffer,
				debug_bufferSize);
			audVehicleAudioEntity::LoadCRAudioSettings();
		}
	}
}

void PlaybackRecordingUsingFocusVehicle()
{
	if (debug_pDebugBuffer)
	{
		StopRecordingPlayback();

		CVehicle* focusVechicle = CVehicleDebug::GetFocusVehicle();
		if(focusVechicle)
		{
			debug_WidgetRecordingPlaybackVehicle = focusVechicle;
			CVehicleRecordingMgr::StartPlaybackRecordedCar(debug_WidgetRecordingPlaybackVehicle, -1, false, 20, DMode_StopForCars, false, -1, debug_pDebugBuffer,
				debug_bufferSize);
		}
	}
}

void PausePlayback()
{
	if(debug_pDebugBuffer && debug_WidgetRecordingPlaybackVehicle)
	{
		CVehicleRecordingMgr::PausePlaybackRecordedCar(debug_WidgetRecordingPlaybackVehicle);
	}
}

void ResumePlayback()
{
	if(debug_pDebugBuffer && debug_WidgetRecordingPlaybackVehicle)
	{
		CVehicleRecordingMgr::UnpausePlaybackRecordedCar(debug_WidgetRecordingPlaybackVehicle);
	}
}

void SaveRecording()
{
	if (debug_pDebugBuffer)
	{
		CVehicleRecordingMgr::CRecording::SaveToFile(debug_pDebugBuffer, debug_bufferSize, CVehicleRecordingMgr::ms_debug_RecordingNumToPlayWith, CVehicleRecordingMgr::ms_debug_RecordingNameToPlayWith);
	}
}

void LoadRecording()
{
	TidyOldDebugBuffer();

	CVehicleRecordingMgr::CRecording::LoadFromFile(&debug_pDebugBuffer, &debug_bufferSize, CVehicleRecordingMgr::ms_debug_RecordingNumToPlayWith, CVehicleRecordingMgr::ms_debug_RecordingNameToPlayWith);
}

void CVehicleRecordingMgr::CreateAudioWidgets()
{
	if(ms_pCreateAudioWidgetsButton)
	{
		bkBank *pBank = BANKMGR.FindBank("Game Logic");
		if(pBank)
		{
			if(audVehicleAudioEntity::AddCarRecordingWidgets(pBank))
			{
				ms_pCreateAudioWidgetsButton->Destroy();
				ms_pCreateAudioWidgetsButton = NULL;
			}
		}
	}
	else 
	{
		return;
	}
}

void CVehicleRecordingMgr::InitWidgets()
{
	bkBank *pBank = CGameLogic::GetGameLogicBank();
	Assert(pBank);

	if (pBank)
	{
		pBank->PushGroup("Car recordings", false);
			// debug widgets:
			pBank->AddToggle("Toggle update with physics", &CVehicleRecordingMgr::sm_bUpdateWithPhysics);
			pBank->AddToggle("Toggle update before PreSimUpdate", &CVehicleRecordingMgr::sm_bUpdateBeforePreSimUpdate);

			// Start test recording on player
			pBank->AddSlider("Recording number used below", &ms_debug_RecordingNumToPlayWith, 0, 10000, 1);
			pBank->AddText("Recording name used below", &ms_debug_RecordingNameToPlayWith[0], sizeof(ms_debug_RecordingNameToPlayWith));

			// Playback test recording on player vehicle
			pBank->AddButton("Start recording player vehicle", datCallback(CFA(StartRecordingPlayerVehicle)));
			pBank->AddButton("Start re-recording player vehicle, transitioning from playback", datCallback(CFA(StartRecordingPlayerVehicleTransitionFromPlayback)));
			pBank->AddButton("Stop recording player vehicle", datCallback(CFA(StopRecordingPlayerVehicle)));
			pBank->AddButton("Playback recording using player vehicle", datCallback(CFA(PlaybackRecordingUsingPlayerVehicle)));
			pBank->AddButton("Playback recording using focus vehicle", datCallback(CFA(PlaybackRecordingUsingFocusVehicle)));
			pBank->AddButton("Pause playback", datCallback(CFA(PausePlayback)));
			pBank->AddButton("Resume playback", datCallback(CFA(ResumePlayback)));
			pBank->AddButton("Stop playback", datCallback(CFA(StopRecordingPlayback)));
			pBank->AddButton("Save recording", datCallback(CFA(SaveRecording)));
			pBank->AddButton("Load recording", datCallback(CFA(LoadRecording)));

			pBank->AddSlider("Playback time scale", &debug_timeScale, 0.01f, 10.0f, 0.1f);

			pBank->AddVector("Playback additional rotation (Eulers in Deg)", &debug_AdditionalRotationEulers, -180.0f, 180.0f, 0.1f);
			pBank->AddVector("Playback local position offset", &debug_LocalPositionOffset, -10000.0f, 10000.0f, 0.1f);
			pBank->AddVector("Playback global position offset", &debug_GlobalPositionOffset, -10000.0f, 10000.0f, 0.1f);

	//		pBank->AddToggle("Reduce car recording", &bReduceRecording);
	//		pBank->AddSlider("Reduction sample step", &ReductionSampleStep, 0, 696969, 1);
	//		pBank->AddSlider("Recording to be reduced", &RecordingToBeReduced, -1, 10000, 1);

			pBank->AddToggle("Fix car recording time-stamps", &bFixRecordingTimeStamps);
			pBank->AddToggle("Shift car recording", &bShiftRecording);
			pBank->AddSlider("Shift X", &ShiftX, -10000.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Shift Y", &ShiftY, -10000.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Shift Z", &ShiftZ, -10000.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Recording number to be shifted", &RecordingNumberToBeShifted, -1, 10000, 1);
			pBank->AddText("Recording name to be shifted", &RecordingNameToBeShifted[0], sizeof(RecordingNameToBeShifted));

			pBank->AddToggle("Render all recordings", &bRenderAllRecordings);

			//pBank->AddButton("Convert all recordings(.rrr) to ascii (.aaa)", datCallback(CFA(ConvertFilesToAscii)));
			//pBank->AddButton("Convert all ascii (.aaa) to recordings(.rrr)", datCallback(CFA(ConvertFilesFromAscii)));

			pBank->AddSlider("Shift recorded cars up during playback", &fShiftPlaybackUpValue, -100.0f, 100.0f, 1.0f);
			pBank->AddToggle("Force recording vehicle to dummy", &CVehicle::sm_bCanForceCarRecordingToDummy);
			pBank->AddToggle("Force recorded vehicle to be inactive", &CVehicle::sm_bForceRecordedVehicleToBeInactive);
			pBank->AddToggle("Force recorded vehicle to be active", &CVehicle::sm_bForceRecordedVehicleToBeActive);

			//Audio tool
			ms_pCreateAudioWidgetsButton = pBank->AddButton("Create audio tool.", datCallback(CFA(CreateAudioWidgets)));
			sm_WidgetGroup = smart_cast<bkGroup*>(pBank->GetCurrentGroup());
			naAssert(sm_WidgetGroup);
		pBank->PopGroup();
	}
}

///////////////////////////////////////////////////////////////////////////
// NAME       : ShiftRecording()
// PURPOSE    : Will load a recording. Shift the recording by a certain amount
//				After that the recording is saved out again (as .rrrshifted)
///////////////////////////////////////////////////////////////////////////

#endif
#endif

void CVehicleRecordingMgr::RegisterStreamingModule()
{
	strStreamingEngine::GetInfo().GetModuleMgr().AddModule(GetStreamingModule());
}

IMPLEMENT_PLACE(CVehicleRecording);

