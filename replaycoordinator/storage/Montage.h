//=====================================================================================================
// name:		Montage.h
// description:	Montage classes.
//=====================================================================================================
#ifndef INC_MONTAGE_H_
#define INC_MONTAGE_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

// rage
#include "atl/array.h"

// framework
#include "fwlocalisation/templateString.h"

// game
#include "Clip.h"
#include "control/replay/File/FileData.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "MontageMusicClip.h"
#include "MontageText.h"

//=====================================================================================================
class CMontage
{
public: // declarations and variables

	//-------------------------------------------------------------------------------------------------
	// format: 1 based incrementing
	static const u32 MONTAGE_VERSION = 5;

    static const u32 MAX_NAME_SIZE_BYTES = MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE;

	typedef rage::fwTemplateString<32>                      MontageName_V1;
    typedef rage::fwTemplateString<32 * 3>                  MontageName_V4;
    typedef rage::fwTemplateString<MAX_NAME_SIZE_BYTES>     MontageName;

	struct MontageHeaderVersionBlob
	{
		u32		m_montageVersion;
		u32		m_clipVersion;
		u32		m_replayVersion;
		u32		m_gameVersion;
		u32		m_textVersion;
		u32		m_musicVersion;
		u32		m_markerVersion;

		MontageHeaderVersionBlob()
			: m_montageVersion( 0 )
			, m_clipVersion( 0 )
			, m_replayVersion( 0 )
			, m_textVersion( 0 )
			, m_musicVersion( 0 )
			, m_markerVersion( 0 )
		{

		}

        void MakeCurrent();
	};

	struct MontageHeader_V1
	{
		u32		m_montageVersion;
		u32		m_clipVersion;
		u32		m_replayVersion;
		u32		m_gameVersion;
		u32		m_textVersion;
		u32		m_musicVersion;
		u32		m_markerVersion;

		u32		m_cachedDurationMs;
		u32		m_timeSpentEditingMs;
		u32		m_numberOfClipObjects;
		u32		m_numberOfTextObjects;
		u32		m_numberOfMusicClips;

		size_t	m_montageObjectSize;
		size_t	m_clipObjectSize;
		size_t  m_textObjectSize;
		size_t	m_musicObjectSize;
		size_t	m_markerObjectSize;

		MontageName_V1 m_name;

		MontageHeader_V1()
			: m_montageVersion( 0 )
			, m_clipVersion( 0 )
			, m_replayVersion( 0 )
			, m_textVersion( 0 )
			, m_musicVersion( 0 )
			, m_markerVersion( 0 )
			, m_cachedDurationMs( 0 )
			, m_timeSpentEditingMs( 0 )
			, m_numberOfClipObjects( 0 )
			, m_numberOfTextObjects( 0 )
			, m_numberOfMusicClips( 0 )
			, m_montageObjectSize( 0 )
			, m_clipObjectSize( 0 )
			, m_textObjectSize( 0 )
			, m_musicObjectSize( 0 )
			, m_markerObjectSize( 0 )
			, m_name()
		{

		}
	};

    struct MontageHeader_V2
    {
        MontageHeaderVersionBlob m_versionBlob;

        u32		m_cachedDurationMs;
        u32		m_timeSpentEditingMs;
        u32		m_numberOfClipObjects;
        u32		m_numberOfTextObjects;
        u32		m_numberOfMusicClips;

        size_t	m_montageObjectSize;
        size_t	m_clipObjectSize;
        size_t  m_textObjectSize;
        size_t	m_musicObjectSize;
        size_t	m_markerObjectSize;

        MontageName_V1 m_name;

        MontageHeader_V2()
            : m_versionBlob()
            , m_cachedDurationMs( 0 )
            , m_timeSpentEditingMs( 0 )
            , m_numberOfClipObjects( 0 )
            , m_numberOfTextObjects( 0 )
            , m_numberOfMusicClips( 0 )
            , m_montageObjectSize( 0 )
            , m_clipObjectSize( 0 )
            , m_textObjectSize( 0 )
            , m_musicObjectSize( 0 )
            , m_markerObjectSize( 0 )
            , m_name()
        {

        }
    };

    // No V3 - V3 was purely a version change without data change. Shared the V2 header.

	struct MontageHeader_V4
	{
		MontageHeaderVersionBlob m_versionBlob;

		u32		m_cachedDurationMs;
		u32		m_timeSpentEditingMs;
		u32		m_numberOfClipObjects;
		u32		m_numberOfTextObjects;
        u32		m_numberOfMusicClips;
        u32		m_numberOfAmbientClips;

		size_t	m_montageObjectSize;
		size_t	m_clipObjectSize;
		size_t  m_textObjectSize;
        size_t	m_musicObjectSize;
		size_t	m_markerObjectSize;

		MontageName_V4 m_name;

		MontageHeader_V4()
			: m_versionBlob()
			, m_cachedDurationMs( 0 )
			, m_timeSpentEditingMs( 0 )
			, m_numberOfClipObjects( 0 )
			, m_numberOfTextObjects( 0 )
			, m_numberOfMusicClips( 0 )
            , m_numberOfAmbientClips( 0 )
			, m_montageObjectSize( 0 )
			, m_clipObjectSize( 0 )
			, m_textObjectSize( 0 )
			, m_musicObjectSize( 0 )
			, m_markerObjectSize( 0 )
			, m_name()
		{

		}
	};

    struct MontageHeader_Current
    {
        MontageHeaderVersionBlob m_versionBlob;

        u32		m_cachedDurationMs;
        u32		m_timeSpentEditingMs;
        u32		m_numberOfClipObjects;
        u32		m_numberOfTextObjects;
        u32		m_numberOfMusicClips;
        u32		m_numberOfAmbientClips;

        size_t	m_montageObjectSize;
        size_t	m_clipObjectSize;
        size_t  m_textObjectSize;
        size_t	m_musicObjectSize;
        size_t	m_markerObjectSize;

        MontageName m_name;

        MontageHeader_Current()
            : m_versionBlob()
            , m_cachedDurationMs( 0 )
            , m_timeSpentEditingMs( 0 )
            , m_numberOfClipObjects( 0 )
            , m_numberOfTextObjects( 0 )
            , m_numberOfMusicClips( 0 )
            , m_numberOfAmbientClips( 0 )
            , m_montageObjectSize( 0 )
            , m_clipObjectSize( 0 )
            , m_textObjectSize( 0 )
            , m_musicObjectSize( 0 )
            , m_markerObjectSize( 0 )
            , m_name()
        {

        }

        MontageHeader_Current( MontageHeader_V1 const& other )
        {
            CopyData( other );
        }

        MontageHeader_Current& operator=( MontageHeader_V1 const& rhs )
        {			
            CopyData( rhs );
            return *this;
        }

        void CopyData( MontageHeader_V1 const& other );

        MontageHeader_Current( MontageHeader_V2 const& other )
        {
            CopyData( other );
        }

        MontageHeader_Current& operator=( MontageHeader_V2 const& rhs )
        {			
            CopyData( rhs );
            return *this;
        }

        void CopyData( MontageHeader_V2 const& other );

        MontageHeader_Current( MontageHeader_V4 const& other )
        {
            CopyData( other );
        }

        MontageHeader_Current& operator=( MontageHeader_V4 const& rhs )
        {			
            CopyData( rhs );
            return *this;
        }

        void CopyData( MontageHeader_V4 const& other );
    };

#define STR_MONTAGE_FILE_EXT ".vid"

	enum eEDIT_LIMITS
	{
		MAX_MARKERS_PER_CLIP = CClip::MAX_MARKERS,

#if RSG_PC
		MAX_CLIP_OBJECTS	= 512,
		MAX_TEXT_OBJECTS	= 256,
        MAX_MUSIC_OBJECTS	= 256,
        MAX_AMBIENT_OBJECTS	= 256,
#else
		MAX_CLIP_OBJECTS	= 128,
		MAX_TEXT_OBJECTS	= 128,
        MAX_MUSIC_OBJECTS	= 64,
        MAX_AMBIENT_OBJECTS	= 64,
#endif
		MAX_DURATION_MS		= 1800000, //( 30 * 60 * 1000 ) - 30 minutes
	};

public: // methods

	~CMontage();

	//! Clip Access
	CClip*			AddClip( const char* szFileName, u64 const ownerID, ReplayMarkerTransitionType eInType, ReplayMarkerTransitionType eOutType );
	CClip*			AddClip( const char* szFileName, u64 const ownerID, s32 sIndex, ReplayMarkerTransitionType eInType, ReplayMarkerTransitionType eOutType );
	CClip*			AddClipCopy( s32 sIndexToCopyFrom, s32 sIndex );
	CClip*			AddClipCopy( CClip const& clip, s32 sIndex );
	CClip*			SetClipCopy( CClip const& clip, s32 sIndex );

	s32				GetClipCount() const;
	s32				GetMaxClipCount() const;
	CClip*			GetClip(s32 index);
	CClip const *	GetClip(s32 index) const;
	CClip*			GetClipByName(const char* szName);
	CClip const *	GetClipByName(const char* szName) const;

	float			GetTrimmedNonDilatedTimeToClipMs(s32 const index) const;
	float			GetTrimmedTimeToClipMs(s32 const index) const;
	u64				GetTrimmedTimeToClipNs(s32 const index) const;
	float			GetTotalClipLengthTimeMs() const;

	float			GetMaxDurationMs() const { return (float)MAX_DURATION_MS; }

	float			ConvertClipNonDilatedTimeMsToClipTimeMs( s32 const index, float const clipNonDilatedTimeMs ) const;
	float			ConvertTimeMsToClipNonDilatedTimeMs( s32 const index, float const timeMs ) const;
	float			ConvertPlaybackPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const playbackPercent ) const;
	float			ConvertClipPercentageToClipNonDilatedTimeMs( u32 const clipIndex, float const clipPercent ) const;
	float			ConvertClipNonDilatedTimeMsToClipPercentage( u32 const clipIndex, float const nonDilatedTimeMs ) const;
	float			GetRawEndTimeAsTimeMs( s32 const index ) const;
	float			GetRawEndTimeMs( s32 const index ) const;

	inline bool		IsValidClipIndex( s32 const index ) const { return index >= 0 && index < m_aClips.GetCount(); }
	bool			IsValidClipReference( s32 const index ) const;
	static bool		IsValidClipFile( u64 const ownerId, char const * const filename );

	bool			DoesClipHaveModdedContent( s32 const index ) const;

	bool			AreAllClipFilesValid();
	bool			AreAnyClipsOfModdedContent();

	void			DeleteClip(const char* szName);
	void			DeleteClip(s32 index);

	//! Montage Name
	void			SetName( const char* szName );
	const char*		GetName() const	{ return m_szName; }

	//! Transitions
	void ValidateTransitionDuration( u32 const uIndex );

	ReplayMarkerTransitionType GetStartTransition( u32 const uIndex ) const;
	void			SetStartTransition( u32 const uIndex, ReplayMarkerTransitionType const eInType);

	float			GetStartTransitionDurationMs( u32 const uIndex ) const;
	void			SetStartTransitionDurationMs( u32 const uIndex, float const durationMs );

	ReplayMarkerTransitionType GetEndTransition( u32 const uIndex ) const;
	void			SetEndTransition( u32 const uIndex, ReplayMarkerTransitionType const eOutType);

	float			GetEndTransitionDurationMs( u32 const uIndex ) const;
	void			SetEndTransitionDurationMs( u32 const uIndex, float const durationMs );

	void			GetActiveTransition( s32 const activeClipIndex, float const clipTime,
						ReplayMarkerTransitionType& out_transition, float& out_fraction ) const;

	//! Music Clip functionality
	s32							AddMusicClip( u32 const soundHash, u32 const uTime = 0 );
	CMontageMusicClip*			GetMusicClip( u32 const uIndex );
	CMontageMusicClip const*	GetMusicClip( u32 const uIndex ) const;

	u32				GetMusicClipSoundHash( u32 const uIndex = 0 ) const;
	u32				GetMusicClipCount() const { return (u32)m_musicClips.size(); }
	u32				GetMusicClipMusicOnlyCount() const;
	u32				GetMusicClipCommercialsOnlyCount() const;
	u32				GetMusicClipScoreOnlyCount() const;
	u32				GetMaxMusicClipCount() const { return (u32)MAX_MUSIC_OBJECTS; }
	inline bool		IsValidMusicClipIndex( s32 const index ) const { return index >= 0 && index < m_musicClips.GetCount(); }
	s32				GetMusicStartTimeMs( u32 const uIndex) const;
	void			SetMusicStartTimeMs( u32 const uIndex, u32 const uTimeMs );
	u32				GetMusicDurationMs( u32 const uIndex) const;
	u32				GetMusicStartOffsetMs( u32 const uIndex) const;
	bool			DoesClipContainMusicClipWithBeatMarkers( u32 const clipIndex ) const;
	u32				GetHashOfFirstInvalidMusicTrack() const;

	void			DeleteMusicClip(  u32 const uIndex );

	//! Ambient Clip functionality
	s32							AddAmbientClip( u32 const soundHash, u32 const uTime = 0 );
	CMontageMusicClip*			GetAmbientClip( u32 const uIndex );
	CMontageMusicClip const*	GetAmbientClip( u32 const uIndex ) const;

	u32				GetAmbientClipSoundHash( u32 const uIndex = 0 ) const;
	u32				GetAmbientClipCount() const { return (u32)m_ambientClips.size(); }
	u32				GetMaxAmbientClipCount() const { return (u32)MAX_AMBIENT_OBJECTS; }
	inline bool		IsValidAmbientClipIndex( s32 const index ) const { return index >= 0 && index < m_ambientClips.GetCount(); }
	s32				GetAmbientStartTimeMs( u32 const uIndex) const;
	void			SetAmbientStartTimeMs( u32 const uIndex, u32 const uTimeMs );
	u32				GetAmbientDurationMs( u32 const uIndex) const;
	u32				GetAmbientStartOffsetMs( u32 const uIndex) const;

	void			DeleteAmbientClip(  u32 const uIndex );


	//! Text Functionality
	CMontageText*		AddText( u32 const startTimeMs, u32 const durationMs );
	u32					GetTextCount() const { return (u32)m_aText.size(); }
	u32					GetMaxTextCount() const { return MAX_TEXT_OBJECTS; }
	inline bool			IsValidTextIndex( s32 const index ) const { return index >= 0 && index < m_aText.GetCount(); }

	CMontageText*		GetText( s32 const index );
	CMontageText const*	GetText( s32 const index ) const;
	s32					GetTextIndex( CMontageText const* target ) const;

	void			DeleteText( CMontageText*& item );
	void			DeleteText( s32 const index );

	// Rendering/Playback Support
	s32				GetActiveTextIndex( s32 const activeClipIndex, float const clipTime ) const;
	bool			GetActiveEffectParameters( s32 const activeClipIndex, float const clipTime, s32& out_effectIndex, 
								float& out_intensity, float& out_saturation, float& out_contrast, float& out_brightness, float& out_vignette ) const;

	bool			GetActiveRenderIntensities(s32 const activeClipIndex, float const clipTimeMs, 
										float& out_saturation, float& out_contrast, float& out_vignette ) const;
	s32				GetActiveMusicIndex( s32 const activeClipIndex, float const clipTimeMs ) const;
	s32				GetNextActiveMusicIndex( s32 const activeClipIndex, float const clipTimeMs ) const;
	s32				GetPreviousActiveMusicIndex( s32 const activeClipIndex, float const clipTimeMs ) const;
	void			ConvertMusicBeatTimeToNonDilatedTimeMs( float const beatTimeMs, s32& clipIndex, float& clipTimeMs ) const;

	s32				GetActiveAmbientTrackIndex( s32 const activeClipIndex, float const clipTimeMs ) const;

	//! Montage Construction and I/O
	static size_t	ApproximateMinimumSizeForSaving();
	static size_t	ApproximateMaximumSizeForSaving();
	size_t			ApproximateSizeForSaving() const;

	static bool		IsSpaceAvailableForSaving( char const * const szFilename, size_t const expectedSize );
	bool			IsSpaceAvailableForSaving( char const * const szFilename );
	bool			IsSpaceAvailableForSaving();

	bool			IsSpaceToAddClip();
	bool			IsSpaceToAddMarker();
    bool			IsSpaceToAddText();
    bool			IsSpaceToAddMusic();
    bool			IsSpaceToAddAmbientTrack();

	u64				Save( const char* szFilename = NULL, bool const showSpinnerWithText = true );
	u64				Load( const char* szFullPath, u64 const userId, bool const previewOnly = false, const char * const fileNameForCheck = NULL );
	bool			Validate();
	
	static CMontage*	Create();
	static void			Destroy(CMontage*& pMontage);
	static bool			IsValidMontageFile( u64 const userId, char const * const filename );

	u32				UpdateTimeSpentEditingMs();

	void			UpdateCachedDurationMs() { m_cachedDurationMs = GetTotalClipLengthTimeMs(); }
	float			GetCachedDurationMs() const { return m_cachedDurationMs; }

	u32				GetRating();

	void UpdateBespokeThumbnailCaptures();

    //! These version data are for B/C behavioural support
    //! When a project is loaded, we cache the version on _load_
    //! This version changes when we save out again to be the current versions.
    u32 GetCachedMontageVersion() const { return m_cachedVersionBlob.m_montageVersion; }
    u32 GetCachedClipVersion() const { return m_cachedVersionBlob.m_clipVersion; }
    u32 GetCachedReplayVersion() const { return m_cachedVersionBlob.m_replayVersion; }
    u32 GetCachedGameVersion() const { return m_cachedVersionBlob.m_gameVersion; }
    u32 GetCachedTextVersion() const { return m_cachedVersionBlob.m_textVersion; }
    u32 GetCachedMusicVersion() const { return m_cachedVersionBlob.m_musicVersion; }
    u32 GetCachedMarkerVersion() const { return m_cachedVersionBlob.m_markerVersion; }

private: // declarations and variables
	atArray<CClip*>				m_aClips;
	atArray<CMontageText*>		m_aText;
	atArray<CMontageMusicClip>	m_musicClips;
	atArray<CMontageMusicClip>	m_ambientClips;

    MontageHeaderVersionBlob    m_cachedVersionBlob;
    float			            m_cachedDurationMs;
    u32				            m_editTimeStampMs;
    u32				            m_timeSpentEditingMs;
    u64				            m_userId;

    MontageName		            m_szName;

private: // methods
	CMontage();

	//! Copy construction
	CClip* AddClipCopy( CClip const& clip );
	CMontageText* AddTextCopy( CMontageText const * const pText );

	size_t	ApproximateSizeForSavingInternal( MontageHeader_Current const& header ) const;

	void MarkClipAsUsedInProject( CClip const& clip );
	void MarkClipAsNotUsedInProject( CClip const& clip );
};

#endif // GTA_REPLAY

#endif // INC_MONTAGE_H_
