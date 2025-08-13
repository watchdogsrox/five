//=====================================================================================================
// name:		MontageMusicClip.h
// description:	Music display over a montage sequence.
//=====================================================================================================
#ifndef INC_MONTAGE_MUSIC_CLIP_H_
#define INC_MONTAGE_MUSIC_CLIP_H_

#include "control/replay/ReplaySettings.h"
#include "replaycoordinator/ReplayAudioTrackProvider.h"

#if GTA_REPLAY

// game
#include "system/FileMgr.h"

#define MIN_MUSIC_DURATION_MS (30000) // 30 second legal limit (2252816)
#define MIN_SCORE_DURATION_MS (500)

#define MIN_AMBIENT_DURATION_MS     (10000)     // 10 second legal limit (2425752)
#define MAX_AMBIENT_DURATION_MS     (1800000)

class CMontageMusicClip
{
public: // declarations and variables

	//-------------------------------------------------------------------------------------------------
	// format: 1 based incrementing
	static const u32 MONTAGE_MUSIC_CLIP_VERSION = 2;

	struct MusicClipData_V1
	{
		u32		m_soundHash;
		u32		m_startTimeMs;
		u32		m_durationMs;

		MusicClipData_V1()
			: m_soundHash( 0 )
			, m_startTimeMs( 0 )
			, m_durationMs( 0 )
		{

		}

		MusicClipData_V1( u32 const soundHash, u32 const startTimeMs, u32 const durationMs )
			: m_soundHash( soundHash )
			, m_startTimeMs( startTimeMs )
			, m_durationMs( durationMs )
		{

		}

		void Invalidate()
		{
			m_soundHash = 0;
			m_startTimeMs = 0;
			m_durationMs = 0;
		}
	};

	struct MusicClipData_Current
	{
		u32		m_soundHash;
		s32		m_startTimeMs;
		u32		m_durationMs;
		u32		m_startOffsetMs;

		MusicClipData_Current()
			: m_soundHash( 0 )
			, m_startTimeMs( 0 )
			, m_durationMs( 0 )
			, m_startOffsetMs( 0 )
		{

		}

		MusicClipData_Current( u32 const soundHash, s32 const startTimeMs, u32 const durationMs, u32 const startOffsetMs )
			: m_soundHash( soundHash )
			, m_startTimeMs( startTimeMs )
			, m_durationMs( durationMs )
			, m_startOffsetMs( startOffsetMs )
		{

		}

		MusicClipData_Current( MusicClipData_V1 const& other )
		{
			CopyData( other );
		}

		MusicClipData_Current& operator=( MusicClipData_V1 const& rhs )
		{			
			CopyData( rhs );
			return *this;
		}

		void CopyData( MusicClipData_V1 const& other )
		{
			m_soundHash = other.m_soundHash;
			m_startTimeMs = other.m_startTimeMs;
			m_durationMs = CReplayAudioTrackProvider::GetMusicTrackDurationMs( m_soundHash );
		}

		void Invalidate()
		{
			m_soundHash = 0;
			m_startTimeMs = 0;
			m_durationMs = 0;
			m_startOffsetMs = 0;
		}
	};

public:
	CMontageMusicClip();
	CMontageMusicClip( u32 const soundHash, s32 const startTimeMs );
	CMontageMusicClip( CMontageMusicClip const& other );
	~CMontageMusicClip();

	CMontageMusicClip& operator=( CMontageMusicClip const& rhs);

    void Validate();
	void Invalidate();

	static size_t ApproximateMinimumSizeForSaving();
	static size_t ApproximateMaximumSizeForSaving();
	size_t ApproximateSizeForSaving() const;

	u64 Save( FileHandle hFile );
	u64 Load( FileHandle hFile, u32 const serializedVersion, size_t const serializedSize );

	inline u32 getSoundHash() const { return m_data.m_soundHash; }
	inline void setSoundHash( u32 const soundHash ) { m_data.m_soundHash = soundHash; }

	inline s32 getStartTimeMs() const { return m_data.m_startTimeMs; }
	inline void setStartTimeMs( s32 const startTimeMs ) { m_data.m_startTimeMs = startTimeMs; }

	inline u32 getStartOffsetMs() const { return ((s32)m_data.m_startOffsetMs < 0) ? 0 : m_data.m_startOffsetMs;	}
	inline void setStartOffsetMs( u32 const startOffsetMs ) { m_data.m_startOffsetMs = (u32)Max(0 , (s32)startOffsetMs); }

    u32 getMinDurationMs() const;
    u32 getMaxDurationMs() const;
	u32 getTrackTotalDurationMs() const;

	u32 getTrimmedDurationMs() const { return m_data.m_durationMs; }	
	void setTrimmedDurationMs( u32 const durationTimeMs );

	bool IsRadioTrack() const;
	bool IsCommercialTrack() const;
    bool IsScoreTrack() const;
    bool IsAmbientTrack() const;

	bool ContainsBeatMarkers() const;

private:
	MusicClipData_Current m_data;
};

#endif // GTA_REPLAY

#endif // INC_MONTAGE_MUSIC_CLIP_H_
