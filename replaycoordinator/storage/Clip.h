//=====================================================================================================
// name:		Clip.h
// description:	Clip class.
//=====================================================================================================
#ifndef INC_CLIP_H_
#define INC_CLIP_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "math/vecmath.h"

// framework
#include "fwlocalisation/templateString.h"

// game
#include "control/replay/File/device_replay.h"
#include "control/replay/File/ReplayFileManager.h"
#include "control/replay/ReplayMarkerInfo.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "control/replay/IReplayMarkerStorage.h"
#include "SaveLoad/savegame_photo_async_buffer.h"
#include "system/FileMgr.h"

class CClip : public IReplayMarkerStorage 
{
public: // declarations and variables

	//-------------------------------------------------------------------------------------------------
	// format: 1 based incrementing
	static const u32 CLIP_VERSION = 4;

	typedef fwTemplateString< CReplayEditorParameters::MAX_CLIP_NAME_LENGTH > ClipNameBuffer;

	struct ClipHeader_V1
	{
		u32				m_numberOfMarkers;
		u32				m_clipTimeMs;
		s32				m_thumbnailBufferSize;
		ClipNameBuffer	m_name;

		ClipHeader_V1()
			: m_numberOfMarkers( 0 )
			, m_clipTimeMs( 0 )
			, m_thumbnailBufferSize( 0 )
			, m_name()
		{

		}
	};

	struct ClipHeader_V2
	{
		ClipUID			m_clipUID;
		u32				m_numberOfMarkers;
		u32				m_rawDurationMs;
		s32				m_thumbnailBufferSize;
		ClipNameBuffer	m_name;

		ClipHeader_V2()
			: m_clipUID()
			, m_numberOfMarkers( 0 )
			, m_rawDurationMs( 0 )
			, m_thumbnailBufferSize( 0 )
			, m_name()
		{

		}
	};

	struct ClipHeader_Current
	{
		ClipUID			m_clipUID;
		u64				m_ownerId;
		u32				m_numberOfMarkers;
		u32				m_rawDurationMs;
		s32				m_thumbnailBufferSize;
		ClipNameBuffer	m_name;

		ClipHeader_Current()
			: m_clipUID()
			, m_ownerId(0)
			, m_numberOfMarkers( 0 )
			, m_rawDurationMs( 0 )
			, m_thumbnailBufferSize( 0 )
			, m_name()
		{

		}

		ClipHeader_Current( ClipHeader_V2 const& other )
		{
			CopyData( other );
		}

		ClipHeader_Current& operator=( ClipHeader_V2 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( ClipHeader_V2 const& rhs );

		ClipHeader_Current( ClipHeader_V1 const& other )
		{
			CopyData( other );
		}

		ClipHeader_Current& operator=( ClipHeader_V1 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( ClipHeader_V1 const& rhs );

		bool IsValid() const;
	};

	enum eEDIT_LIMITS
	{
		MAX_MARKERS = 512,
	};

public: // methods

	CClip();
	CClip( CClip const& other );

	virtual ~CClip();

	CClip& operator=(const CClip& rhs);

	bool Populate( char const * const szFileName, u64 const ownerId,
		ReplayMarkerTransitionType const eInType, ReplayMarkerTransitionType const eOutType );

	bool IsValid() const;
	bool HasValidFileReference() const;

	bool HasModdedContent() const;

	// General Accessors
	inline void			SetName( char const * const szName) { m_szName.Set( szName ); }
	inline bool			IsValidName() const { return !m_szName.IsNullOrEmpty(); }
	inline const char*	GetName() const	{ return m_szName.getBuffer(); }

	inline void			SetOwnerId( u64 const ownerId ) { m_ownerId = ownerId; }
	inline u64			GetOwnerId() const { return m_ownerId; }

	inline ClipUID const& GetUID() const { return m_clipUID; }

	//! TODO - Try and refactor so we handle markers internally, rather than exposing the collection.
	ReplayMarkerInfoCollectionPointer GetMarkerData()						{ return &m_aoMarkers; }

	//////////////////////////////////////////////////////////////////////////
	// IReplayMarkerStorage interface
    virtual void CopyMarkerData( u32 const index, sReplayMarkerInfo& out_destMarker );
    virtual void PasteMarkerData( u32 const indexToPasteTo, sReplayMarkerInfo const&  sourceMarker, eMarkerCopyContext const context = COPY_CONTEXT_ALL ) ;

	virtual bool AddMarker( bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, float const nonDilatedTimeMs, s32& out_Index );
	virtual bool AddMarker( bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, s32& out_Index );
	virtual bool AddMarker( sReplayMarkerInfo* marker, bool const ignoreBoundary, bool const sortMarkersAfterAdd, bool const inheritMarkerProperties, s32& out_Index );
	virtual void RemoveMarker( u32 const index );

	virtual void ResetMarkerToDefaultState( u32 const markerIndex );

	virtual bool CanPlaceMarkerAtNonDilatedTimeMs( float const nonDilatedTimeMs, bool const ignoreBoundary = false ) const;
	virtual bool CanIncrementMarkerNonDilatedTimeMs( u32 const markerIndex, float const nonDilatedTimeDelta, float& inout_markerNonDilatedTime );
	virtual bool GetMarkerNonDilatedTimeConstraintsMs( u32 const markerIndex, float& out_minNonDilatedTimeMs, float& out_maxNonDilatedTimeMs ) const;
	virtual bool GetPotentialMarkerNonDilatedTimeConstraintsMs( u32 const markerIndex, float const nonDilatedTimeDelta, 
		float& out_minNonDilatedTimeMs, float& out_maxNonDilatedTimeMs );

	virtual float UpdateMarkerNonDilatedTimeForMarkerDragMs( u32 const markerIndex, float const clipPlaybackPercentage );

	virtual void SetMarkerNonDilatedTimeMs( u32 const markerIndex, float const newNonDilatedTimeMs );
	virtual float GetMarkerDisplayTimeMs( u32 const markerIndex ) const;
	virtual void IncrememtMarkerSpeed( u32 const markerIndex, s32 const delta );
	virtual float GetMarkerFloatSpeedAtCurrentNonDilatedTimeMs( float const nonDilatedTimeMs ) const;
	virtual u32 GetMarkerSpeedAtCurrentNonDilatedTimeMs( float const nonDilatedTimeMs ) const;

	virtual void RecalculateMarkerPositionsOnSpeedChange( u32 const startingMarkerIndex = 0 );
	virtual void RemoveMarkersOutsideOfRangeNonDilatedTimeMS( float const startNonDilatedTimeMs, float const endNonDilatedTimeMs );

	virtual sReplayMarkerInfo* GetMarker( u32 const index );
	virtual sReplayMarkerInfo const* GetMarker( u32 const index ) const;
	virtual sReplayMarkerInfo* TryGetMarker( u32 const index );
	virtual sReplayMarkerInfo const* TryGetMarker( u32 const index ) const;

	virtual sReplayMarkerInfo* GetFirstMarker();
	virtual sReplayMarkerInfo const* GetFirstMarker() const;
	virtual sReplayMarkerInfo* GetLastMarker();
	virtual sReplayMarkerInfo const* GetLastMarker() const;

	virtual s32 GetPrevMarkerIndexWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 ) const;
	virtual s32 GetNextMarkerIndexWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 ) const;

	virtual sReplayMarkerInfo* GetPrevMarkerWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 );
	virtual sReplayMarkerInfo* GetNextMarkerWrapAround( u32 const startingIndex, float const startNonDilatedTimeMs = 0, float const endNonDilatedTimeMs = 0 );

	virtual s32 GetNextMarkerIndex( u32 const startingIndex, bool const ignoreAnchors = false ) const ;
	virtual sReplayMarkerInfo* GetNextMarker( u32 const startingIndex, bool const ignoreAnchors = false );

	virtual s32 GetMarkerCount() const;
	virtual s32 GetMaxMarkerCount() const;

	virtual s32 GetFirstValidMarkerIndex( float const nonDilatedTimeMs ) const;
	virtual s32 GetLastValidMarkerIndex( float const nonDilatedTimeMs ) const;
	virtual sReplayMarkerInfo* GetFirstValidMarker( float const nonDilatedTimeMs );
	virtual sReplayMarkerInfo* GetLastValidMarker( float const nonDilatedTimeMs );

	virtual s32 GetClosestMarkerIndex( float const nonDilatedTimeMs, eClosestMarkerPreference const preference = CLOSEST_MARKER_ANY, float* out_nonDilatedTimeDiff = NULL, sReplayMarkerInfo const* markerToIgnore = NULL, bool const ignoreAnchors = false ) const;
	virtual sReplayMarkerInfo* GetClosestMarker( float const nonDilatedTimeMs, eClosestMarkerPreference const preference = CLOSEST_MARKER_ANY, float* out_nonDilatedTimeDiff = NULL, sReplayMarkerInfo const* markerToIgnore = NULL, bool const ignoreAnchors = false );
	
	virtual float GetAnchorNonDilatedTimeMs( u32 const index ) const;
	virtual float GetAnchorTimeMs( u32 const index ) const;
	virtual s32 GetAnchorCount() const;

	virtual IReplayMarkerStorage::eEditRestrictionReason IsMarkerControlEditable( eMarkerControls const controlType ) const;

	virtual void BlendTypeUpdatedOnMarker( u32 const index );

	// End IReplayMarkerStorage interface
	//////////////////////////////////////////////////////////////////////////

	// Transitions
	inline float GetMinTransitionDurationMs() const { return MARKER_FRAME_BOUNDARY_MS / 2.f; }
	inline float GetMaxTransitionDurationMs() const { return MIN( GetTrimmedTimeMs() / 2.f, 1000.f ); }

	ReplayMarkerTransitionType GetStartTransition() const;
	void SetStartTransition( ReplayMarkerTransitionType const eType);

	float GetStartTransitionDurationMs() const;
	void SetStartTransitionDurationMs( float const durationMs );

	ReplayMarkerTransitionType	GetEndTransition() const;
	void SetEndTransition(ReplayMarkerTransitionType const eType);

	float GetEndTransitionDurationMs() const;
	void SetEndTransitionDurationMs( float const durationMs );

	// Time - Time as affected by time-dilation (if used)
	inline float GetRawStartTimeMs() const						{ return 0; }
	inline float GetRawDurationMs() const						{ return (float)m_rawDurationMs; }
	inline void	SetRawDurationMs( float const rawDurationMs )	{ m_rawDurationMs = rawDurationMs; }

	float GetStartNonDilatedTimeMs() const;
	float GetTrimmedNonDilatedTimeMs() const { return GetEndNonDilatedTimeMs() - GetStartNonDilatedTimeMs(); }
	float GetEndNonDilatedTimeMs() const;

	float ConvertNonDilatedTimeMsToTimeMs( float const nonDilatedTimeMs ) const;
	float ConvertTimeMsToNonDilatedTimeMs( float const timeMs ) const;
	float ConvertPlaybackPercentageToNonDilatedTimeMs( float const playbackPercent ) const;
	float ConvertClipPercentageToNonDilatedTimeMs( float const clipPercent ) const;
	float ConvertClipNonDilatedTimeMsToClipPercentage( float const nonDilatedTimeMs ) const;
	float GetRawEndTimeAsTimeMs() const;		

	inline void SetAllTimesMs( float const nonDilatedStartTimeMs, float const nonDilatedEndTimeMs, float const rawDurationMs, 
										float const startRealTimeMs, float const endRealTimeMs )
	{
		SetStartNonDilatedTimeMs( nonDilatedStartTimeMs );
		SetEndNonDilatedTimeMs( nonDilatedEndTimeMs );
		m_rawDurationMs		= rawDurationMs;
		m_startTimeMs	= startRealTimeMs;
		m_endTimeMs		= endRealTimeMs;
		ValidateTimeData();
	}

	inline float GetStartTimeMs() const		{ return m_startTimeMs; }
	inline float GetTrimmedTimeMs() const	{ return m_endTimeMs-m_startTimeMs; }
	inline float GetEndTimeMs() const		{ return m_endTimeMs; }

	inline u32 GetNumberOfMarkers() const		{ return m_aoMarkers.size(); }

	// Management and I/O
	// ==================
	// IMPORTANT!
	// If you make any significant changes to this class that affect saving and loading,
	// be sure to update the handling to support different versions
	static size_t ApproximateMinimumSizeForSaving();
	static size_t ApproximateMaximumSizeForSaving();
	size_t ApproximateSizeForSaving() const;
	u64 Save(FileHandle hFile);
	u64 Load(FileHandle hFile, u32 const serializedVersion, size_t const serializedSize, 
		u32 const markerSerializedVersion, size_t const markerSerializedSize, bool const ignoreValidity = false );
	void Clear();

	// Bespoke Thumbnails
	// ==================
	//
	inline bool HasBespokeThumbnail() const { return m_thumbnailBuffer != NULL && m_thumbnailBufferSize > 0; }
	inline char const * GetBespokeThumbnailFileName() const { return m_thumbnailBufferMemFileName.getBuffer(); }
	inline bool HasBespokeCapturePending() const { return m_asyncThumbnailBuffer.IsInitialized(); }

	bool CaptureBespokeThumbnail();
	void UpdateBespokeCapture();
	inline static u32 GetThumbnailWidth() { return SCREENSHOT_WIDTH; }
	inline static u32 GetThumbnailHeight() { return SCREENSHOT_HEIGHT; }
	
	void ValidateTransitionTimes();

private: // declarations and variables
	ClipNameBuffer						m_szName;
	ClipUID								m_clipUID;

	ReplayMarkerInfoCollection			m_aoMarkers;

	// Time
	float								m_rawDurationMs;

	// Real Time
	float								m_startTimeMs;
	float								m_endTimeMs;

	SimpleString_128					m_thumbnailBufferMemFileName;
	u8*									m_thumbnailBuffer;
	s32									m_thumbnailBufferSize;
	u64									m_ownerId;

	CSavegamePhotoAsyncBuffer			m_asyncThumbnailBuffer;

private: // methods

	void SetStartNonDilatedTimeMs( float const nonDilatedTimeMs );
	void SetEndNonDilatedTimeMs( float const nonDilatedTimeMs );

	void AddMarkersForStartAndEndPoints();

	// NOTE - Returns true if action had to be taken to bring time data back to a valid state
	bool ValidateTimeData();

	void ValidateThumbnailBufferSize();
	static bool IsValidThumbnailBufferSize( size_t const bufferSize );

	void CleanupMarkers();
	void CopyMarkerData( CClip const& other );

	void AllocateBespokeThumbnail();
	static u32 GetExpectedThumbnailSize();

	void CleanupBespokeThumbnail();
	void CleanupBespokeThumbnailCapture();

	void CopyBespokeThumbnail( CClip const& other );
	void MakeBespokeThumbnailBufferFileName();

	inline u8* GetBespokeThumbnailBuffer() const { return m_thumbnailBuffer;}
	inline s32 GetBespokeThumbnailBufferSize() const { return m_thumbnailBufferSize; }

	float CalculateTimeForNonDilatedTimeMs( float const nonDilatedTimeMs ) const;
	float CalculateTimeForMarkerMs( u32 const markerIndex ) const;
	float CalculateNonDilatedTimeForTimeMs( float const timeMs ) const;
	float CalculateNonDilatedTimeForTrimmedPercentMs( float const playbackPercent ) const;
	float CalculateNonDilatedTimeForTotalPercentMs( float const clipPercent ) const;
	float CalculateTotalPercentForNonDilatedTimeForMs( float const nonDilatedTimeMs ) const;
	float CalculatePlaybackPercentageForMarker( u32 const markerIndex ) const;

	void RecalculateDilatedTimeStartEndPoints();
	void PostLoadMarkerValidation();

	sReplayMarkerInfo* PopMarker( u32 const index );
};

#endif // GTA_REPLAY

#endif // INC_CLIP_H_
