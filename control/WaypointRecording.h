#ifndef WAYPOINT_RECORDING_H
#define WAYPOINT_RECORDING_H

#include "control\WaypointRecordingRoute.h"

//*****************************************************************************

#include "fwmaths\Vector.h"
#include "script/thread.h"
#include "streaming\streamingmodule.h"

#include "paging/base.h"

class CPed;


class CLoadedWaypointRecording
{
	friend class CWaypointRecording;
	friend class CWaypointRecordingStreamingInterface;

public:
	CLoadedWaypointRecording();
	~CLoadedWaypointRecording() { }

	void Reset();

	inline u32 GetHashKey() const { return m_iFileNameHashKey; }
	inline float GetAssistedMovementPathWidth() const { return m_fAssistedMovementPathWidth; }
	inline float GetAssistedMovementPathTension() const { return m_fAssistedMovementPathTension; }
	inline void SetAssistedMovementPathWidth(const float f) { m_fAssistedMovementPathWidth = f; }
	inline void SetAssistedMovementPathTension(const float f) { m_fAssistedMovementPathTension = f; }

protected:

	// Hash of the filename (excluding path)
	u32 m_iFileNameHashKey;
	// The recording - will be NULL until streamed in
	::CWaypointRecordingRoute * m_pRecording;
	// How many peds are playing this recording (**NOT** how many different scripts have requested this route!!)
	s32 m_iNumReferences;
	// The script which requested this.  For now lets impose the restriction that only one
	// script may use a particular waypoint recording at a time.
	scrThreadId m_iScriptID;

	// IFF this recording is also being used as an assisted-movement route, the params are stored here
	float m_fAssistedMovementPathWidth;
	float m_fAssistedMovementPathTension;

#if __BANK
	// Purely for debug
	char m_Filename[64];
#endif
};

class CWaypointRecordingStreamingInterface : public strStreamingModule
{
public:

	CWaypointRecordingStreamingInterface();
	~CWaypointRecordingStreamingInterface() { }

	void SetCount(int sz)
	{	m_size = sz;	}

	//***********************************
	// Implement the streaming interface:

#if !__FINAL
	virtual const char* GetName(strLocalIndex index) const;
#endif
	virtual strLocalIndex Register(const char* name);
	virtual strLocalIndex FindSlot(const char* name) const;
	virtual void Remove(strLocalIndex index);
	virtual void RemoveSlot(strLocalIndex index);
	virtual bool Load(strLocalIndex index, void * object, int size);
	virtual void PlaceResource(strLocalIndex index, datResourceMap& map, datResourceInfo& header);
	virtual void* GetPtr(strLocalIndex index);

};

//***********************************************************************
// CWaypointRecording
// The recording part of this will probably be usable as-is in the __DEV
// build of the game.  The playback part won't as this will require some
// extra work to allow multiple simultaneous playback routes, as well as
// the controlling tasks.

class CWaypointRecording
{
	friend class CWaypointRecordingStreamingInterface;
public:

	static bank_float ms_fMBRDeltaForNewWaypointOnSkis;
	static bank_float ms_fMBRDeltaForNewWaypointOnFoot;
	static bank_float ms_fMBRDeltaForNewWaypointInVehicle;
	static bank_float ms_fMinWaypointSpacing;
	static bank_float ms_fMaxWaypointSpacingOnSkis;
	static bank_float ms_fMaxWaypointSpacingInVehicle;
	static bank_float ms_fMaxWaypointSpacingOnFoot;
	static bank_float ms_fMaxPlaneDistOnSkis;
	static bank_float ms_fMaxPlaneDistOnFoot;
	static bank_float ms_fMaxPlaneDistInVehicle;

#if __BANK
	static bool ms_bDebugDraw;
	static bool ms_bDisplayStore;
	static int ms_iDebugStoreIndex;
	static char ms_TestRouteName[128];
	static bool ms_bLoopRoutePlayback;
	static bool ms_bUseAISlowdown;
	static bool ms_bWaypointEditing;

	static char m_LoadedRouteName[256];
	static int m_iLoadedRouteNumWaypoints;

	static float ms_fAssistedMovementPathWidth;
	static float ms_fAssistedMovementPathTension;

	struct TEditingControls
	{
		struct TFlags
		{
			bool m_bJump;
			bool m_bApplyJumpForce;
			bool m_bLand;
			bool m_bFlipBackwards;
			bool m_bFlipForwards;
			bool m_bDropDown;
			bool m_bSurfacingFromUnderwater;
			bool m_bNonInterrupt;
			bool m_bVehicle;
		};
		TEditingControls()
		{
			m_iSelectedWaypoint = 0;
			m_fWaypointHeading = 0.0f;
			m_fWaypointMBR = 0.0f;
			ms_bRepositioningWaypoint = false;
			m_vTranslateRoute = Vector3(0.0f, 0.0f, 0.0f);
			m_fFreeSpaceToLeft = 0.0f;
			m_fFreeSpaceToRight = 0.0f;
		}
		int m_iSelectedWaypoint;
		float m_fWaypointHeading;
		float m_fWaypointZ;
		float m_fWaypointMBR;
		float m_fFreeSpaceToLeft;
		float m_fFreeSpaceToRight;
		bool ms_bRepositioningWaypoint;		
		TFlags m_Flags;
		Vector3 m_vTranslateRoute;
	};
	static TEditingControls ms_EditingControls;
#endif // __BANK

	CWaypointRecording();
	virtual ~CWaypointRecording();

	static void RegisterStreamingModule();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

#if __BANK
	static void InitWidgets();
#endif //__BANK

	// API for script
	static void RequestRecording(const char * pRecordingName, const scrThreadId iScriptID);
	static void RemoveRecording(const char * pRecordingName, const scrThreadId iScriptID);
	static bool IsRecordingLoaded(const char * pRecordingName);
	static bool IsRecordingRequested(const char * pRecordingName);
	static bool IncReferencesToRecording(const char * pRecordingName);
	static bool DecReferencesToRecording(const char * pRecordingName);
	static bool IncReferencesToRecording(const ::CWaypointRecordingRoute * pRecording);
	static bool DecReferencesToRecording(const ::CWaypointRecordingRoute * pRecording);
	static ::CWaypointRecordingRoute * GetRecording(const char * pRecordingName);
	static ::CWaypointRecordingRoute * GetRecordingBySlotIndex(const int iSlotIndex);
	static CLoadedWaypointRecording * GetLoadedWaypointRecordingInfo(const ::CWaypointRecordingRoute * pRecording);
	static CLoadedWaypointRecording * GetLoadedWaypointRecordingInfo(const int iSlot);

	static void RemoveAllRecordingsRequestedByScript(const scrThreadId iScriptID=THREAD_INVALID);

#if __BANK
	static void StartPlayback(CPed * pPed, bool bPlaybackVehicle=false);
	static void StopPlayback();
	static void Debug();
	static void StartRecording(CPed * pPed, bool bRecordVehicle=false);
	static void StopRecording();
	static void UpdateRecording();
	static bool GetIsRecording() { return ms_bRecording; }
	static void ClearRoute();
	static ::CWaypointRecordingRoute * GetRecordingRoute() { return &ms_RecordingRoute; }

	// Editing controls
	static void Edit_InitWaypointWidgets();
	static void Edit_OnChangeWaypoint();
	static void Edit_CopyFlagsToWaypoint();
	static void Edit_OnWarpToWaypoint();
	static void Edit_RepositionWaypoint();
	static void Edit_InsertWaypoint();
	static void Edit_RemoveWaypoint();
	static void Edit_OnModifyHeading();
	static void Edit_OnModifyMBR();
	static void Edit_OnIncreaseX();
	static void Edit_OnDecreaseX();
	static void Edit_OnIncreaseY();
	static void Edit_OnDecreaseY();
	static void Edit_OnIncreaseZ();
	static void Edit_OnDecreaseZ();
	static void Edit_OnTranslateNow();
	static void Edit_OnModifyDistLeft();
	static void Edit_OnModifyDistRight();
	static void Edit_OnApplyAllDistances();
	static void Edit_OnSnapToGround();
#endif

	static bool CheckWaypointRecordingName(const char * pRecordingName);

	struct TStreamingInfo
	{
		u32 m_iFilenameHashKey;
#if !__FINAL
		char m_Filename[64];
#endif
	};

	static CWaypointRecordingStreamingInterface m_StreamingInterface;

	static int ms_iMaxNumRecordingsPerLevel;
	static TStreamingInfo*	m_StreamingInfo;

	static const int ms_iMaxNumLoaded = 32;

protected:

#if __BANK
	static CPed * ms_pPed;
	static ::CWaypointRecordingRoute ms_RecordingRoute;
	//static char ms_RecordingRouteName[256];
	static bool ms_bRecording;
	static bool ms_bRecordingVehicle;
	static bool ms_bWasFlipped;
	static bool ms_bUnderwaterLastFrame;
#endif

	static CLoadedWaypointRecording ms_LoadedRecordings[ms_iMaxNumLoaded];

	static int GetFreeRecordingSlot();
	static int FindExistingRecordingSlot(const u32 iHash);
	static int GetStreamingIndexForRecording(const u32 iHash);
	static void SetRecordingFromStreamingIndex(s32 iStreamingIndex, ::CWaypointRecordingRoute * pRec);
	static int FindSlotForPointer(const ::CWaypointRecordingRoute * pRec);
};





#endif	// WAYPOINT_RECORDING_H
