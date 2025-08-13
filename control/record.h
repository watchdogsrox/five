

/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    record.h
// PURPOSE : Record the input and elapse of time so that we can accurately
//			 play back the game.
//			 This stuff was originally used for the start of GTAIII (the jail break). It was changed
//			 for SA to allow for single cars to be recorded and played back easily by the level designers.
// AUTHOR :  Obbe.
// CREATED : 28/2/00
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _RECORD_H_
	#define _RECORD_H_

// rage includes
#include "atl/hashstring.h"
#include "bank/bank.h"
#include "file/device.h"
#include "paging/base.h"
#include "atl\binmap.h"
#include "atl\string.h"
#include "vector/quaternion.h"
#include "streaming/streamingdefs.h"

// game includes
#include "scene/RegdRefTypes.h"
#include "system/fileMgr.h"
#include "vector/matrix34.h"
#include "vector/vector3.h"
#include "VehicleAI/vehmission.h"

#include "fwcontrol/record.h"

#define CAR_RECORDING_NAME_LENGTH	24
#define CAR_RECORDING_BUFFER_SIZE	(480000)	// Should be enough for about 40 minutes of recording.
#define CAR_RECORDING_FLUSH_BUFFER_SIZE		(3072)	// Enough for more than 10 seconds of recording - to be used for recordings that are
													// continously flushed to disk
	// Indices for the recordings that are used by the code to control
	// buses and stuff.

enum {
	FOR_CODE_INDEX_LAST = 0
//	FOR_CODE_INDEX_BUS_0,
//	FOR_CODE_INDEX_BUS_1,
//	FOR_CODE_INDEX_BUS_2,
//	FOR_CODE_INDEX_LAST			// Should always be at the end.
};

#define FRAMERECORDPERIOD (125)		// Once every this time do we record a frame for a car.
#define RECORDFLUSHPERIOD (10000)	// Once every this time we flush the buffer to disk

		// The different display modes for a recording.
enum { RDM_NONE = 0, RDM_WHOLELINE, RDM_JUSTINFRONT, RDM_JUSTBEHIND, RDM_AROUNDVEHICLE };

////////////////////////////////////////////////////////////
// Stores the orientation, coors, speed etc for one car. Used to
// save the states of the cars for the big chase scene at the start.
////////////////////////////////////////////////////////////

class CVehicleRecordingStreaming
{
public:
	void InitCore();

	void RegisterRecordingFile(const char *pFilename);

	void Place(CVehicleRecording *pR);

	void StartPlayback(u8 **ppPlaybackBuffer, s32 &PlaybackBufferSize);
	
	void Remove();

	void AddRef();
	void RemoveRef();
	int GetNumRefs();

	void GetPositionOfCarRecordingAtTime(float time, Vector3 &RetVal);
	void GetTransformOfCarRecordingAtTime(float time, Matrix34 &RetVal);

	float GetTotalDurationOfCarRecording();

	u32	GetHashKey() {return HashKey;}
	u32	GetAudioHashKey() {return AudioHashKey;}

	CVehicleRecording *GetRecordingRsc() { return pRecordingRsc; }

	bool HasRecordingData() { return (pRecordingData != NULL); }

	void Invalidate();

#if !__FINAL
	const char* GetRecordingName() {return HashKey.GetCStr();}
#endif

private:
	atHashWithStringNotFinal HashKey;					// hash of the recordingname
	atHashWithStringNotFinal AudioHashKey;					//audio hash of the recordingname
	u8	*pRecordingData;								// The actual data for this recording. (NULL if not streamed in)
	s32	RecordingDataSize;
	s32	m_NumTimesUsed;
	CVehicleRecording * pRecordingRsc;
};

////////////////////////////////////////////////////////////
// Everything having to do with recording cars and playing them back by the level
// designers.
////////////////////////////////////////////////////////////

namespace rage { class strStreamingModule; }

class CVehicleRecordingMgr
{
public:
	class CRecording
	{
	public:
		void Init();

		bool IsRecording() const {return m_isRecording;}
		const CVehicle* GetVehicle() const {return m_pVehicle;}
		bool StartRecording(CVehicle* pVehicle, int fileNumber, const char* pRecordingName);
		void StartRecordingTransitionFromPlayback(CVehicle* pVehicle, int fileNumber, const char* pRecordingName, int playbackSlot);
		void StopRecording(u8 **ppBuffer, int *pSize);
		void SaveFrame();
		float FindTimePositionInRecording();
		
		static void WriteBufferToFile(FileHandle fid, u8* buffer, s32 length);
		static void SaveToFile(u8 *pBuffer, int length, int fileNumber, const char* pRecordingName);
		static FileHandle OpenRecordingFile(const s32 fileNumber, const char* pRecordingName);
		static void	LoadFromFile(u8 **ppBuffer, int *pLength, int fileNumber, const char* pRecordingName);
		static void EndianSwapRecording(u8 *pData, s32 dataSize);
		static void ShiftRecording(s32 RecordingNumber, const char* pRecordingName, float ShiftByX, float ShiftByY, float ShiftByZ);
		static void FixRecordingTimeStamps(s32 RecordingNumber, const char* pRecordingName);

		static void	StoreInfoForMatrix(const Matrix34 *pMatrix, CVehicleStateEachFrame *pCarState);
		static void	StoreInfoForCar(class CVehicle *pCar, CVehicleStateEachFrame *pCarState);

	private:
		RegdVeh		m_pVehicle;
		s32			m_fileNumber;
		atString	m_fileName;
		FileHandle	m_fileHandle;
		u8*			m_pRecordingBuffer;
		s32			m_recordingIndex;
		u32			m_startTime;
		u32			m_lastFrameRecordedTime;
		u32			m_lastFrameFlushedTime;
		bool		m_flushToDisk;
		bool		m_isFirstFrame;
		bool		m_isRecording;
	};

	struct CPlayback
	{
		Quaternion	PlaybackAdditionalRotation;
		RegdVeh		pVehicleForPlayback;
		u8			*pPlaybackBuffer;
		s32			PlaybackIndex;
		s32			PlaybackBufferSize;
		float		PlaybackRunningTime;
		float		PlaybackSpeed;
		bool		bPlaybackGoingOn;
		bool		bPlaybackLooped;
		bool		bPlaybackPaused;
		bool		bUseCarAI;
		s32			aiVechicleRecordingFlags;
		u32			aiDelayUntilRevertingBack;
		u32			TurnBackIntoFullPlayBackTime;
		u32			DisplayMode;
		s32			PlayBackStreamingIndex;
	};
#if __BANK
#if !__FINAL
	static int ms_debug_RecordingNumToPlayWith;
	static char	ms_debug_RecordingNameToPlayWith[CAR_RECORDING_NAME_LENGTH];
#endif
#endif
public:
	enum eVehicleRecordingFlags
	{
		VRF_ConvertToAIOnImpact_PlayerVehcile	= BIT(0),
		VRF_ConvertToAIOnImpact_AnyVehicle		= BIT(1),
		VRF_StartEngineInstantly				= BIT(2),
		VRF_StartEngineWithStartup				= BIT(3),
		VRF_ContinueRecordingIfCarDestroyed		= BIT(4),
		VRF_FirstUpdate							= BIT(5)
	};
	static void SetVehicleForPlayback(int SlotNumber, CVehicle *pNewVehicle);

	static void Init(unsigned initMode);

	static void RegisterStreamingModule();

	static void	Shutdown(unsigned shutdownMode);

	static strStreamingModule* GetStreamingModule();

	//access functions
	static bool GetUseCarAI(int index) {return bUseCarAI[index];}
	static u32 GetDelayUntilRevertingBack(int index) {return aiDelayUntilRevertingBack[index];}
	static s32 GetVehicleRecordingFlags(int index) {return aiVechicleRecordingFlags[index];}
	static u8* GetPlaybackBuffer(int index) {return pPlaybackBuffer[index];}
	static s32 GetPlaybackBufferSize(int index) {return PlaybackBufferSize[index];}
	static bool IsPlaybackGoingOn(int index) {return bPlaybackGoingOn[index];}
	static bool IsPlaybackPaused(int index) {return bPlaybackPaused[index];}
	static s32 GetPlaybackIndex(int index) {return PlaybackIndex[index];}
	static void SetPlaybackIndex(int index, s32 value) { PlaybackIndex[index] = value;}
	static float GetPlaybackSpeed(int index) {return PlaybackSpeed[index];}

#if !__FINAL
	static void	SaveDataForThisFrame();
#endif // !__FINAL
	static void RetrieveDataForThisFrame();

	static void	UpdatePlaybackForVehicleRecording(s32 RecordingIndex, float TimeStep);
	static void RestoreInfoForMatrix(Matrix34& mat, class CVehicleStateEachFrame *pCarState);
	static void RestoreInfoForPosition(Vector3& pos, class CVehicleStateEachFrame *pCarState);
	static void	RestoreInfoForCar(class CVehicle *pCar, CVehicleStateEachFrame *pCarState, bool bAnimEnd, float playbackSpeed, bool bUpdateTrailer);
	static void HandleRecordingTeleport(CVehicle *pCar, const Matrix34 &matOld, bool bUpdateTrailer);

	static void	InterpolateInfoForCar(CVehicle *pCar, CVehicleStateEachFrame *pCarState, float Interp, float playbackSpeed, const Quaternion& additionalRotation, Vec3V_In localPositionOffset, Vec3V_In globalPositionOffset, const Matrix34 &PreviousMatrix, const float fTimeStep);

	// The following functions are to be called from the script.
#if !__FINAL

	//static FileHandle OpenRecordingFile(const s32 fileNumber, const char* pRecordingName);
	//static void WriteBufferToFile(FileHandle fid, u8* buffer, s32 length);
	//static void	SaveToFile(u8 *pBuffer, int length, int fileNumber, const char* pRecordingName);
	//static void	LoadFromFile(u8 **ppBuffer, int *pLength, int fileNumber, const char* pRecordingName);
	static bool DoesRecordingFileExist(const s32 fileNumber, const char* pRecordingName);
	
	static bool	StartRecordingCar(class CVehicle *pCar, u32 FileNum, const char* pRecordingName, bool allowOverwrite = false);
	static bool	StartRecordingCarTransitionFromPlayback(class CVehicle *pCar, u32 FileNum, const char* pRecordingName, bool allowOverwrite = false);
	static void	StopRecordingCar(CVehicle *pVehToStopRecordingFor = NULL, u8 **ppBuffer = NULL, int *pSize = NULL);
	static bool	IsCarBeingRecorded(const class CVehicle *pCar);
	//static bool IsAnyCarBeingRecorded();
#endif
	static void	StartPlaybackRecordedCar(class CVehicle *pCar, int index, bool bUseCarAI = false, float fSpeed = 10, s32 iDrivingFlags = DMode_StopForCars, bool bLooped = false, s32 ForCodeIndex = -1, u8 *pData = NULL, int dataSize = 0, s32 iVehicleRecordingFlags = VRF_StartEngineInstantly, u32 delayUntilTurningBackOnAIActivation = 0);

	static void ForcePlaybackRecordedCarUpdate(class CVehicle *pCar);

#if !__FINAL
	static void DisplayPlaybackRecordedCar(class CVehicle *pCar, s32 NewDisplayMode);
#endif

	static void* GetPtr(s32 index);
	static bool Place(s32 index, datResourceMap& map, datResourceInfo& header);

	static void AddRef(strLocalIndex index);
	static void RemoveRef(strLocalIndex index);
	static int GetNumRefs(strLocalIndex index);
	
	static bool HasRecordingFileBeenLoaded(int index);

	static void SetRecordingToPointClosestToCoors(s32 C, const Vector3& Coors, Vector3 *CoorsClosest = NULL);

	static void	StopPlaybackWithIndex(s32 C);
	static void StopPlaybackRecordedCar(class CVehicle *pCar);
	static int GetPlaybackSlot(const CVehicle * pCar);
	static int GetPlaybackSlotFast(const CVehicle* pCar);
	static bool IsPlaybackGoingOnForCar(const CVehicle *pCar);

	static int GetPlaybackIdForCar(const CVehicle *pCar);

	static bool	IsPlaybackPausedForCar(class CVehicle *pCar);
	static bool IsPlaybackSwitchedToAiForCar(const CVehicle *pCar);
	static void SetPlaybackSpeed(class CVehicle *pCar, float Speed);

	static void	ChangeCarPlaybackToUseAI(class CVehicle *pVeh, u32 timeAfterWhichToTryAndReturnToFullRecording, s32 iDrivingFlags = DMode_StopForCars, bool bSnapToPositionIfNotVisible = false);

	static void PausePlaybackRecordedCar(class CVehicle *pCar);
	static void UnpausePlaybackRecordedCar(class CVehicle *pCar);
	static void SkipToEndAndStopPlaybackRecordedCar(class CVehicle *pCar);
	static void	SkipForwardInRecording(class CVehicle *pCar, float Dist);
	static void	SkipTimeForwardInRecording(class CVehicle *pCar, float Time);

	static void SetAdditionalRotationForRecordedVehicle(class CVehicle *pVehicle, Vec3V_In axisAngleRotation);
	static void SetLocalPositionOffsetForRecordedVehicle(class CVehicle *pVehicle, Vec3V_In localOffset);
	static void SetGlobalPositionOffsetForRecordedVehicle(class CVehicle *pVehicle, Vec3V_In globalOffset);

	static float FindPositionInRecording(const class CVehicle *pCar);
	static float FindTimePositionInRecording(const class CVehicle *pCar);

	static void	SetRecordingToPointNearestToCoors(class CVehicle *pCar, Vector3 *pPos);
	static void GetTransformOfCarRecordingAtTime(int index, float time, Matrix34 &RetVal );
	static void GetPositionOfCarRecordingAtTime(int index, float time, Vector3 &RetVal);

	// Note: This is in the script domain (as most programmers would use GetTransform...)
	// EULER_YXZ * 180/PI to match GET_ENTITY_ROT
	static void GetRotationOfCarRecordingAtTime(int index, float time, Vector3 &RetVal);


	static float GetTotalDurationOfCarRecording(int index);

#if !__FINAL
	static float FindTimePositionInRecordedRecording(const class CVehicle *pCar);
	static const char *GetRecordingName(s32 index) {return sm_StreamingArray[index].GetRecordingName();}
#endif
	static u32 GetEndTimeOfRecording(int index);
	static float GetPlaybackRunningTime(int index); 
	static const atHashWithStringNotFinal GetRecordingHash(s32 index) {return sm_StreamingArray[index].GetHashKey();}
	static const atHashWithStringNotFinal GetRecordingAudioHash(s32 index) {return sm_StreamingArray[index].GetAudioHashKey();}
	static void TryToTurnAIRecordingBackIntoFull(s32 C);

	static void Render();

	static s32 RegisterRecordingFile(const char *pFilename);
	static s32 FindRecordingFile(const char *pFilename);
	static s32 FindIndexWithFileNameNumber(s32 FileNameNumber, const char* pRecordingName);
	
	static void Remove(s32 index);
	static void RemoveSlot(int index);


	//static void SmoothRecording(s32 Index);

	static u32 FindPlaybackRunningTimeAccordingToSchedule(s32 Recording, u32 MillisecondsOffset);

	static Matrix34	*GetMatrixForCodeRecording(s32 r);
	static Matrix34 *GetMatrixForCodeRecordingWithTimeOffset(s32 C, u32 TimeOffsetInMilliseconds);

	static s32 FindTimeJumpRequiredToGetToNextStaticPoint(s32 C);

#if !__FINAL
/*	static void ConvertFilesToAscii();
	static void ConvertFilesFromAscii();
	static void ConvertFileFromRRRToAAA(FileHandle fileRRR, FileHandle fileAAA, bool bAlsoHeading = false);
	static void ConvertFileFromAAAToRRR(FileHandle fileAAA, FileHandle fileRRR);
	static void	FindFileCallback(const fiFindData &data, void *);*/
#endif

#if __BANK
#if !__FINAL
	static void InitWidgets();
	static void CreateAudioWidgets();
	static void SetCurrentRecording(const char* name,u32 num);
	static bkButton*	ms_pCreateAudioWidgetsButton;
	static rage::bkGroup*			sm_WidgetGroup; 
#endif
#endif

	static s32						GetRecordingIndexFromHashKey(u32 key);
	static s32						GetRecordingIndex(const char* pName);
	static CVehicleRecordingStreaming*	GetRecordingInfoFromHashKey(u32 key, s32* pReturnIndex);
	static CVehicleRecordingStreaming*	GetRecordingInfo(const char *pName, s32* pReturnIndex);

    static bool     sm_bUpdateWithPhysics; //should the recording be updated on every physics update?
	static bool		sm_bUpdateBeforePreSimUpdate; // IF 'sm_bUpdateWithPhysics' is true, should we update before objects get PreSimUpdate called?

private:
	static void			InitialiseData();

	// All the stuff required for recording.
#if !__FINAL
	static CRecording ms_vehiclesRecordings[MAX_CARS_RECORDED_NUMBER];
#endif

	// All the stuff required for playback
	static	Vec3V		PlaybackAdditionalRotation[MAX_CARS_PLAYBACK_NUMBER];
	static	Vec3V		PlaybackPreviousAdditionalRotation[MAX_CARS_PLAYBACK_NUMBER];
	static	Vec3V			PlaybackLocalPositionOffset[MAX_CARS_PLAYBACK_NUMBER];
	static	Vec3V			PlaybackGlobalPositionOffset[MAX_CARS_PLAYBACK_NUMBER];
	static	RegdVeh			pVehicleForPlayback[MAX_CARS_PLAYBACK_NUMBER];
	static	u8			*pPlaybackBuffer[MAX_CARS_PLAYBACK_NUMBER];
	static	s32			PlaybackIndex[MAX_CARS_PLAYBACK_NUMBER];
	static	s32			PlaybackBufferSize[MAX_CARS_PLAYBACK_NUMBER];
	static	float			PlaybackRunningTime[MAX_CARS_PLAYBACK_NUMBER];
	static	float			PlaybackSpeed[MAX_CARS_PLAYBACK_NUMBER];
	static	bool			bPlaybackGoingOn[MAX_CARS_PLAYBACK_NUMBER];
	static	bool			bPlaybackLooped[MAX_CARS_PLAYBACK_NUMBER];
	static	bool			bPlaybackPaused[MAX_CARS_PLAYBACK_NUMBER];
	static	bool			bUseCarAI[MAX_CARS_PLAYBACK_NUMBER];
	static  s32				aiVechicleRecordingFlags[MAX_CARS_PLAYBACK_NUMBER];
	static	u32				aiDelayUntilRevertingBack[MAX_CARS_PLAYBACK_NUMBER];
	static	u32			TurnBackIntoFullPlayBackTime[MAX_CARS_PLAYBACK_NUMBER];	// Do we want to turn this back into a full playback and if so; when?
	//	static	u32			ScheduleOffsetTime[MAX_CARS_PLAYBACK_NUMBER];
	static	u32			DisplayMode[MAX_CARS_PLAYBACK_NUMBER];
	static	s32			PlayBackStreamingIndex[MAX_CARS_PLAYBACK_NUMBER];

	// The stuff needed to stream the recordings in.
	static	CVehicleRecordingStreaming*	sm_StreamingArray;	// Pointer to array with size matching CVehicleRecordingStreamingModule.
	static	s32						sm_NumPlayBackFiles;
};



#endif
