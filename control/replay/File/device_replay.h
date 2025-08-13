#ifndef FILE_DEVICE_REPLAY_H
#define FILE_DEVICE_REPLAY_H

#include "../ReplaySettings.h"

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "FileData.h"
#include "file/device_relative.h"
#include "control/replay/replay_channel.h"
#include "string/string.h"

enum eReplayFileErrorCode
{
	REPLAY_ERROR_SUCCESS = 0,
	REPLAY_ERROR_FAILURE,
	REPLAY_ERROR_FILE_CORRUPT,
	REPLAY_ERROR_LOW_DISKSPACE,
	REPLAY_ERROR_FILE_IO_ERROR,
};

//! These aren't replay specific so should be moved to a framework level "in-game file management" project
typedef atArray<CFileData> FileDataStorage;

#if GTA_REPLAY

#include "system/criticalsection.h"

enum eReplayMemoryLimit
{
	REPLAY_MEM_250MB = 0, 
	REPLAY_MEM_500MB, 
	REPLAY_MEM_750MB, 
	REPLAY_MEM_1GB,
	REPLAY_MEM_1_5GB,
	REPLAY_MEM_2GB,
	REPLAY_MEM_5GB,
	REPLAY_MEM_10GB,
	REPLAY_MEM_15GB,
	REPLAY_MEM_25GB,
	REPLAY_MEM_50GB,
	REPLAY_MEM_LIMIT_MAX,
};

struct MemLimitInfo
{
	u64					AsBytes()	const	{ return m_Size << 10; }
	u64					AsKB()		const	{ return m_Size; }
	float				AsMB()		const	{ return (AsBytes() >> 10) / 1024.0f; }
	float				AsGB()		const	{ return (AsBytes() >> 20) / 1024.0f; }
	const char*			GetName()	const	{ return m_Name; }

	eReplayMemoryLimit	m_Limit;
	u64					m_Size;
	const char*			m_Name;
};

enum FileStoreOperation
{
	CREATE_OP,
	READ_ONLY_OP,
	EDIT_OP,
};

enum eFileStoreVersionOperation
{
	VERSION_OPERATION_INVALID = 0,			// Invalid, holding place.
	VERSION_OPERATION_DELETE,
	VERSION_OPERATION_LEAVE_OUT_OF_CACHE,
	VERSION_OPERATION_OK,
};

enum eMarkedUpType
{
	MARK_AS_FAVOURITE = 0,
	UNMARK_AS_FAVOURITE,
	MARK_AS_DELETE,
	UNMARK_AS_DELETE
};

//////////////////////////////////////////////////////////////////////////
class ClipUID
{
public:
	static const ClipUID INVALID_UID;

public:
	ClipUID() : m_UID(0)
	{
		m_Unused[0] = 0;
	}

	explicit ClipUID(u32 uid) : m_UID(uid)
	{
		m_Unused[0] = 0;
	}

	virtual ~ClipUID(){}

	inline ClipUID &operator =( const u32& id )
	{
		m_UID = id;
		return *this;
	}

	inline bool operator ==( const ClipUID& uid ) const
	{
		return IsEqual( *this, uid);
	}

	inline bool operator !=( const ClipUID& uid ) const
	{
		return !IsEqual( *this, uid );
	}

	static bool IsEqual( const ClipUID& uidA,const ClipUID& uidB )
	{
		return uidA.m_UID == uidB.m_UID;
	}

	bool IsValid() const 
	{ 
		return *this != ClipUID::INVALID_UID;
	}

	// Only for debug purposes, use the comparison operator for forward compatibility.
	u32 get() const { return m_UID; }

private:
	u32 m_UID;
	//We might want to swap the UID out for a GUID so i've added some
	//extra padding to make it scalable.
	char m_Unused[12];
};



//////////////////////////////////////////////////////////////////////////

//These values NEED to be in sync with eReplayMemoryLimit.
static MemLimitInfo MemLimits[] = 
{ 
	{REPLAY_MEM_250MB,	256000,		"250"},
	{REPLAY_MEM_500MB,	512000,		"500"},
	{REPLAY_MEM_750MB,	768000,		"750"},
	{REPLAY_MEM_1GB,	1048576,	"1"},
	{REPLAY_MEM_1_5GB,	1572864,	"1.5"},
	{REPLAY_MEM_2GB,	2097152,	"2"},
	{REPLAY_MEM_5GB,	5242880,	"5"},
	{REPLAY_MEM_10GB,	10485760,	"10"},
	{REPLAY_MEM_15GB,	15728640,	"15"},
	{REPLAY_MEM_25GB,	26214400,	"25"},
	{REPLAY_MEM_50GB,	52428800,	"50"}
};

static const u64 DATA_FILE_LIMIT = 1 * 1024 * 1024;

struct ReplayFileInfo;
class CMontage;

//Not really sure what values we should have here.
#define MAX_CLIPS		800
#define MAX_MONTAGES	100

class CacheFileData
{
public:
	CacheFileData()
	{
		Init("");
	}

	explicit CacheFileData(const char* filePath)
	{
		Init(filePath);
	}

	virtual ~CacheFileData(){}

	void Init(const char* filePath)
	{
		m_Filehash = atStringHash(filePath);
		safecpy( m_FilePath, filePath );
		m_Duration = 0;
		m_Size = 0;
		m_LastWriteTime = 0;
		m_userId = 0;
		m_markForDelete = false;
		m_Corrupt = false;
	}

	bool IsEqual( const char* const filePath ) const
	{
		return m_Filehash == atStringHash(filePath);
	}

	bool operator==(const char* const filePath) const
	{
		return IsEqual( filePath );
	}

	bool IsEqual( const CacheFileData& data ) const
	{
		return data.m_Filehash == m_Filehash;
	}

	bool operator==(const CacheFileData& data) const
	{
		return IsEqual( data );
	}

	bool IsEqual( u32 const filehash ) const
	{
		return filehash == m_Filehash;
	}

	bool operator==(u32 & filehash) const
	{
		return IsEqual( filehash );
	}

	const char* GetFilePath() const { return m_FilePath; }
	u32 GetFileHash() const			{ return m_Filehash; }

	void SetDuration(u32 duration){ m_Duration = duration; }
	void SetSize(u64 size){ m_Size = size; }
	void SetLastWriteTime(u64 lastWriteTime){ m_LastWriteTime = lastWriteTime; }
	void SetUserId( u64 userId ) { m_userId = userId; }
	void SetMarkForDelete( bool markForDelete ) { m_markForDelete = markForDelete; }

	u32 GetDuration() const { return m_Duration; }
	u64 GetSize() const { return m_Size; }
	u64 GetLastWriteTime() const { return m_LastWriteTime; }
	u64 GetUserId() const { return m_userId; }
	bool GetMarkForDelete() const { return m_markForDelete; }
	bool GetIsCorrupt() const { return m_Corrupt; }

protected:
	u32			m_Duration;
	bool		m_Corrupt;

private:
	u32			m_Filehash;
	u64			m_Size;
	u64			m_LastWriteTime;
	u64			m_userId;
	bool		m_markForDelete;
	char		m_FilePath[REPLAYPATHLENGTH];
};

struct ReplayHeader;
class ClipData : public CacheFileData
{
public:
	ClipData() : CacheFileData(),
		m_VersionOp(VERSION_OPERATION_INVALID),
		m_HeaderVersion(0),
		m_Favourite(false),
		m_FavouriteModified(false),
		m_Uid(0),
		m_prevSequenceHash(0),
		m_nextSequenceHash(0),
		m_FrameFlags(0),
		m_ModdedContent(false)
	{}

	void Init(const char* filename, const ReplayHeader& header, eFileStoreVersionOperation versionOp, bool favourite = false, bool moddedContent = false, bool bCorrupt = false);

	void SetFavourite(bool value)		
	{ 
		m_Favourite = value;

		if( m_FavouriteModified )
			m_FavouriteModified = false;
		else
			m_FavouriteModified = true;
	}

	bool GetFavourite() const			{ return m_Favourite; }
	bool IsFavouriteModified() const	{ return m_FavouriteModified; }

	bool GetHasModdedContent() const	{ return m_ModdedContent; }

	u32 GetFrameFlags() const			{ return m_FrameFlags; }

	float GetLengthAsFloat() const		{ return float( GetDuration() ); }
	const ClipUID& GetUID() const		{ return m_Uid; }

	u32	GetPreviousSequenceHash() const	{	return m_prevSequenceHash;	}
	u32	GetNextSequenceHash() const		{	return m_nextSequenceHash;	}

	bool IsEqual( const ClipUID& uid ) const
	{
		return m_Uid == uid;
	}

	bool operator == (const ClipUID& uid) const
	{
		return IsEqual( uid );
	}

	using CacheFileData::IsEqual;

private:
	eFileStoreVersionOperation	m_VersionOp;
	u32							m_HeaderVersion;
	bool						m_Favourite;
	bool						m_FavouriteModified;
	ClipUID						m_Uid;

	u32							m_prevSequenceHash;
	u32							m_nextSequenceHash;

	u32							m_FrameFlags;
	bool						m_ModdedContent;
};

class ProjectData : public CacheFileData
{	
public:
	ProjectData() : CacheFileData() { m_Clips.clear(); }
	~ProjectData() { m_Clips.clear(); }

	void Init(const char* filePath)
	{
		CacheFileData::Init(filePath);
		m_Clips.clear();
	}

	s32 FindFirstIndexOfClip( ClipUID const& uid ) const
	{
		s32 index = -1;
		for( u32 i = 0; i < m_Clips.GetCount(); i++)
		{
			const ClipUID& c_ClipUID = m_Clips[i];
			if( uid == c_ClipUID )
			{
				index = i;
				break;
			}
		}

		return index;
	}

	void RemoveClip(const ClipData& clip, bool force = false)
	{
		ClipUID const c_uid = clip.GetUID();
		s32 index = FindFirstIndexOfClip( c_uid );
		
		//if force is set we want to remove all occurrences of the clip
		if( force )
		{
			while(index != -1)
			{
				m_Clips.DeleteFast(index);
				index = FindFirstIndexOfClip(c_uid);
			}
		} 
		else if( index != -1 )
		{
			m_Clips.DeleteFast(index);
		}
	}

	void AddClip(const ClipData* clip)
	{
		if( clip )
		{
			m_Clips.PushAndGrow(clip->GetUID());
		}
	}

private:
	atArray<ClipUID> m_Clips;
};

class fiDeviceReplay : public fiDeviceRelative
{
	friend class ReplayFileManager;

public:
	fiDeviceReplay();

	void Init();
	void Process();

#if __BANK
	void AddDebugWidgets(bkBank& bank);
	void UpdateDebugSpaceWidgets();
	const char* GetDebugCurrentClip() const;
	float BytesToMB(const u64 bytes) { return (bytes >> 10) / 1024.0f; }
#endif // __BANK
	
	virtual u64 GetMaxMemoryAvailable() const { return 0; }
	
	virtual u64	GetMaxMemoryAvailableForClips() const;
	virtual u64	GetMaxMemoryAvailableForClipsAndMontages() const;
	virtual u64 GetMaxMemoryAvailableForVideo() const { return 0; }

	virtual u64 GetFreeSpaceAvailable(bool force = false);

	virtual u64 GetUsedSpaceForClips();
	virtual u64 GetFreeSpaceAvailableForClips();
	u64 GetSizeOfProject(const char* path) const;

	virtual u64 GetFreeSpaceAvailableForClipsAndMontages();
	virtual u64 GetFreeSpaceAvailableForMontages();
	virtual u64 GetFreeSpaceAvailableForVideos();
	virtual u64 GetFreeSpaceAvailableForData();

	eReplayMemoryLimit GetMaxAvailableMemoryLimit();
	eReplayMemoryLimit GetMinAvailableMemoryLimit();
	u64 GetAvailableAndUsedClipSpace();
	float GetAvailableAndUsedClipSpaceInGB();

	virtual bool CanFullfillUserSpaceRequirement() { return true; }
	virtual bool CanFullfillSpaceRequirement( eReplayMemoryLimit size );

	u64 GetTotalSpaceAvaliable() const { return 0; }

	void SetShouldUpdateDirectorySize();
	void SetShouldUpdateClipsSize();
	void SetShouldUpdateMontagesSize();
	
	eReplayFileErrorCode Enumerate(const ReplayFileInfo &info, const char* filePathParam = NULL);

	eReplayFileErrorCode EnumerateMontages(ReplayFileInfo &info);

	eReplayFileErrorCode EnumerateClips(ReplayFileInfo &info);

	eReplayFileErrorCode LoadClip(ReplayFileInfo &info);

	eReplayFileErrorCode LoadMontage( ReplayFileInfo &info, u64& inout_extendedResultData );

	eReplayFileErrorCode SaveMontage( ReplayFileInfo &info, u64& inout_extendedResultData );

	eReplayFileErrorCode LoadHeader(ReplayFileInfo& info);

	eReplayFileErrorCode DeleteFile(ReplayFileInfo& info);

	eReplayFileErrorCode UpdateFavourites(ReplayFileInfo& info);

	eReplayFileErrorCode ProcessMultiDelete(ReplayFileInfo& info);

	bool IsClipFileValid(u64 const userId,  const char* path, bool checkDisk = false);

	bool IsProjectFileValid( u64 const userId, const char* path, bool checkDisk = false);

	const ClipUID* GetClipUID(u64 const userId, const char* path) const;
	bool DoesClipHaveModdedContent(const char* path);

	float GetClipLength(u64 const userId, const char* path);
	u32 GetClipSequenceHash(u64 const userId, const char* path, bool prev);

	// Cache of clips hashes in each project
	void AddClipToProjectCache(const char* path, u64 const userId, ClipUID const& uid );
	void DeleteClipFromProjectCache(const char* path, u64 const userId, ClipUID const& uid);
	void DeleteClipFromAllProjectCaches(const ClipData& clip);
	void AddProjectToCache(const char* path, u64 const userId, u64 const size, u32 const duration );
	bool IsProjectInProjectCache( const char* path, u64 const userId ) const;
	void RemoveCachedProject( const char* path, u64 const userId );
	bool IsClipInAProject(const ClipUID& clipUID) const;

	ClipData* AppendClipToCache(const ClipUID& clipUID);

	bool IsMarkedAsFavourite(const char* path);
	bool IsMarkedAsDelete(const char* path);
	void MarkFile(const char* path, eMarkedUpType type);

	u32 GetCountOfFilesToDelete(const char* filter) const;

	u32 GetClipRestrictions(const ClipUID& clipUID);

	u32 GetClipCount() const { return m_CachedClips.GetCount(); }
	u32 GetProjectCount() const { return m_CachedMontages.GetCount(); }

	bool IsClipCacheFull() const { return m_CachedClips.IsFull(); }

	u32 GetMaxClips() const { return MAX_CLIPS; }
	u32 GetMaxProjects() const { return MAX_MONTAGES; }

	const MemLimitInfo& GetMemLimitInfo(eReplayMemoryLimit limit) const { return MemLimits[(int)limit]; }
	
	void RefreshCaches() 
	{
		m_ResetEnumerationState = true;
	}

	static u64 GetUserIdFromFileInfo( ReplayFileInfo const& fileInfo );
	static u64 GetUserIdFromPath( char const * const filePath );

protected:
	virtual const char* GetDirectory() const { return ""; }
	virtual u64 GetDirectorySize(const char* pathName, bool recurse = false );
	u64 RefreshSizesAndGetTotalSize();

	atFixedArray<ClipData, MAX_CLIPS> m_CachedClips;
	atFixedArray<ProjectData, MAX_MONTAGES> m_CachedMontages;
	atArray<CacheFileData> m_FilesToDelete;
	
	bool m_ShouldUpdateDirectorySize;
	bool m_ShouldUpdateClipsSize;
	bool m_ShouldUpdateMontageSize;
	u64 m_clipSize;
	u64 m_montageSize;
	u64 m_videoSize;
	u64 m_dataSize;

private:
	eFileStoreVersionOperation	GetVersionOperation(u32 version);

	bool ValidateMontage(const char* path);

	void RemoveCachedClip(const char* path);

	bool IsFileValid(const char* path);

	void DeleteOldClips(ReplayFileInfo& fileOpInfo);
	void UpdateClipHeaderVersion(ReplayFileInfo& fileOpInfo);

	bool CheckMontageCaches(FileDataStorage* fileList);
	bool CheckClipCaches(FileDataStorage* fileList);

	template<class _Type, int Count>
	void PopulateFileListWithCache(FileDataStorage* fileList, const char* path, const atFixedArray<_Type, Count>& cache);

	sysCriticalSectionToken ms_CritSec;

	bool m_HasEnumeratedClips;
	bool m_HasEnumeratedMontages;

	bool m_IsEnumerating;
	bool m_ResetEnumerationState;

#if DO_CUSTOM_WORKING_DIR
	bool GetDebugPath(const char* &path);
#endif // DO_CUSTOM_WORKING_DIR

#if __BANK
	void UpdateClipCacheWidget();
	void UpdateFavouritesWidget();

	static bool		sm_UpdateClipWidget;
	static bool		sm_UpdateFavouritesWidget;
	static bool		sm_UpdateSpaceWidgets;

	static int		sm_ClipIndex;
	static bkCombo*	sm_ClipCacheCombo;
	static int		sm_FavouritesIndex;
	static bkCombo*	sm_FavouritesCombo;

	static char		sm_montMemBuffer[64];
	static bkText*	sm_MontageMemory;
	
	static char		sm_clipMemBuffer[64];
	static bkText*	sm_ClipMemory;

	static char		sm_montUsgBuffer[64];
	static bkText*	sm_MontageUsage;

	static char		sm_clipUsgBuffer[64];
	static bkText*	sm_ClipUsage;

	static char		sm_vidUsgBuffer[64];
	static bkText*	sm_VideoUsage;

	static char		sm_dataUsgBuffer[64];
	static bkText*	sm_DataUsage;
#endif // __BANK
};

#endif // GTA_REPLAY

#endif // FILE_DEVICE_REPLAY_H
