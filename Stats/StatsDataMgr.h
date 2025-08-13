//
// StatsDataMgr.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STATSDATAMGR_H
#define STATSDATAMGR_H


// --- Include Files ------------------------------------------------------------

// Rage headers
#include "atl/array.h"
#include "atl/map.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "atl/inmap.h"
#include "string/string.h"
#include "rline/rlgamerinfo.h"
#include "security/obfuscatedtypes.h"

#if __BANK
#include "vector/vector3.h"
#endif // __BANK


//Framework Headers
#include "fwnet/nettypes.h"

// Game headers
#include "Network/Live/NetworkProfileStats.h"
#include "Network/NetworkTypes.h"
#include "network/stats/NetworkStockMarketStats.h"
#include "Stats/StatsTypes.h"
#include "Stats/StatsSavesMgr.h"

// --- Forward Definitions ------------------------------------------------------
namespace rage
{
	class  parTreeNode;
	class  rlProfileStatsValue;
}

#if __BANK
namespace rage
{
	class bkBank;
	class bkGroup;
}
#endif


// --- Structure/Class Definitions ----------------------------------------------

//#pragma pack(push)  /* push current alignment to stack */
//#pragma pack(1)     /* set alignment to 1 byte boundary */

//PURPOSE
//  Description of a stat that belongs to the Single Player Game
typedef struct
{
	u32       m_Dirty                        : 1;  // True when the value has changed.
	u32       m_DirtyProfile                 : 1;  // True when the value has changed and should dirty profile stats (see STATUPDATEFLAG_DIRTY_PROFILE).
	u32       m_DirtySavegame                : 1;  // True when the value has changed and will only be cleared after being saved to the savegame.
	u32       m_OnlineStat                   : 1;  // True for Online Stats.
	u32       m_ProfileStat                  : 1;  // True for Profile Stats.
	u32       m_ProfileSetting               : 1;  // True for Profile Settings.
	u32       m_CommunityStat                : 1;  // True for Community Stats.
	u32       m_ServerAuthoritative          : 1;  // Profile Stat that is Server Authoritative Stat.
	u32       m_ReadPriority                 : 2;  // Profile Stat with priority set to PS_HIGH_PRIORITY or PS_LOW_PRIORITY will be read during multiplayer game for the remote players.
	u32       m_FlushPriority                : 4;  // Profile Stat Flush priority.
	u32       m_Category                     : 4;  // Stat Category - We can have up to 15 categories to split savegames
	u32		  m_CharacterStat				 : 1;  // True if stat is being tracked for each individual character
	u32		  m_TriggerEventValueChanged     : 1;  // True if an event is triggered when the value changes
	u32       m_Group                        : 3;  // The group that this stat resides in (for profile stats). This is relative to PS_GRP_MP_START/PS_GRP_SP_START
	u32       m_BlockReset                   : 1;  // True if the value is never reset.
	u32       m_BlockProfileStatFlush        : 1;  // True if the value is never flushed.
	u32       m_BlockSynchEventFlush         : 1;  // True if we are blocking stat flush on syncing.
	u32       m_DirtyInThisSession           : 1;  // True if this value has changed this session.
	u32       m_PlayerInventory              : 1;  // True if this stat is part of the player inventory.
	u32       m_Synched                      : 1;  // True when Online can be used after game load.
	u32       m_ForcedSynched                : 1;  // True if the stat has been synched with the online value.
	u32       m_NoSaveStat					 : 1;  // True if the stat is only used for during the session and doesnt need to be saved
	u32       m_AlwaysFlush					 : 1;  // True if the stat should always be flushed, even when we're in MP.
} DescForSinglePlayer;
CompileTimeAssert(sizeof(DescForSinglePlayer) == 4);

//Verify relative SP/MP groups fit in 3 bits for m_Group
CompileTimeAssert(PS_GRP_MP_END - PS_GRP_MP_START < (1 << 3));
CompileTimeAssert(PS_GRP_SP_END - PS_GRP_SP_START < (1 << 3));

//PURPOSE
//  Description of a stat that belongs to the Multi Player Game
typedef struct
{
	u32       m_Dirty                        : 1;  // True when the value has changed.
    u32       m_DirtyProfile                 : 1;  // True when the value has changed and should dirty profile stats (see STATUPDATEFLAG_DIRTY_PROFILE).
	u32       m_DirtySavegame                : 1;  // True when the value has changed and will only be cleared after being saved to the savegame.
	u32       m_OnlineStat                   : 1;  // True for Online Stats.
	u32       m_ProfileStat                  : 1;  // True for Profile Stats.
	u32       m_ProfileSetting               : 1;  // True for Profile Settings.
	u32       m_CommunityStat                : 1;  // True for Community Stats.
	u32       m_ServerAuthoritative          : 1;  // Profile Stat that is Server Authoritative Stat.
	u32       m_ReadPriority                 : 2;  // Profile Stat with priority set to PS_HIGH_PRIORITY or PS_LOW_PRIORITY will be read during multiplayer game for the remote players.
	u32       m_FlushPriority                : 4;  // Profile Stat Flush priority.
	u32       m_Category                     : 4;  // We can have up to 15 categories to split stats in more than one savegame file.
	u32		  m_CharacterStat				 : 1;  // True if stat is being tracked for each individual character
	u32		  m_TriggerEventValueChanged     : 1;  // True if an event is triggered when the value changes
    u32       m_Group                        : 3;  // The group that this stat resides in (for profile stats). This is relative to PS_GRP_MP_START/PS_GRP_SP_START
	u32       m_BlockReset                   : 1;  // True if the value is never reset.
	u32       m_BlockProfileStatFlush        : 1;  // True if the value is never flushed.
	u32       m_BlockSynchEventFlush         : 1;  // True if we are blocking stat flush on syncing.
	u32       m_DirtyInThisSession           : 1;  // True if this value has changed this session.
	u32       m_PlayerInventory              : 1;  // True if this stat is part of the player inventory.
	u32       m_Synched                      : 1;  // True when Online can be used after game load.
	u32       m_ForcedSynched                : 1;  // True if the stat has been synched with the online value.
	u32       m_NoSaveStat					 : 1;  // True if the stat is only used for during the session and doesnt need to be saved
	u32       m_AlwaysFlush					 : 1;  // True if the stat should always be flushed, even when we're in MP.
} DescForMultiPlayer;
CompileTimeAssert(sizeof(DescForMultiPlayer) == 4);

//#pragma pack(pop) /* restore original alignment from stack */

//PURPOSE
//   This struct represents a stat description.
struct sStatDescription
{
private:
	// Data description
	union {
		DescForSinglePlayer as_SinglePlayer;
		DescForMultiPlayer  as_MultiPlayer;
	} m_Desc;

public:
	sStatDescription()
	{
		m_Desc.as_MultiPlayer.m_Dirty               = false;
		m_Desc.as_MultiPlayer.m_DirtyProfile        = false;
		m_Desc.as_MultiPlayer.m_DirtySavegame       = false;
		m_Desc.as_MultiPlayer.m_OnlineStat          = false;
		m_Desc.as_MultiPlayer.m_ProfileStat         = false;
		m_Desc.as_MultiPlayer.m_CommunityStat       = false;
		m_Desc.as_MultiPlayer.m_ServerAuthoritative = false;
		m_Desc.as_MultiPlayer.m_ReadPriority        = 0;
		m_Desc.as_MultiPlayer.m_FlushPriority       = 0;
		m_Desc.as_MultiPlayer.m_Category            = 0;
		m_Desc.as_MultiPlayer.m_Synched             = false;
		m_Desc.as_MultiPlayer.m_ForcedSynched       = false;
        m_Desc.as_MultiPlayer.m_Group               = 7;
		m_Desc.as_MultiPlayer.m_CharacterStat       = false;
		m_Desc.as_MultiPlayer.m_TriggerEventValueChanged = false;
		m_Desc.as_MultiPlayer.m_ProfileSetting      = false;
		m_Desc.as_MultiPlayer.m_BlockReset          = false;
		m_Desc.as_MultiPlayer.m_BlockProfileStatFlush = false;
		m_Desc.as_MultiPlayer.m_BlockSynchEventFlush = false;
		m_Desc.as_MultiPlayer.m_DirtyInThisSession = false;
		m_Desc.as_MultiPlayer.m_PlayerInventory = false;
		m_Desc.as_MultiPlayer.m_NoSaveStat = false;
		m_Desc.as_MultiPlayer.m_AlwaysFlush			= false;
	}

	sStatDescription(const bool online
					,const bool communityStat
					,const bool profileStat
					,const bool serverAuthoritative
					,const u32 readPriority
					,const u32 flushPriority
					,const u32 category
					,const bool characterStat
					,const bool triggerEventValueChanged
					,const bool profileSetting
					,const bool blockReset
					,const bool blockProfileStatFlush
					,const bool blockSynchEventFlush
					,const bool bNoSaveStat
					,const bool bAlwaysFlush)
	{
		Assert(readPriority <= PS_LOW_PRIORITY);
		Assert(flushPriority <= RL_PROFILESTATS_MAX_PRIORITY);
		Assert(category <= 15);

		m_Desc.as_MultiPlayer.m_Dirty                 = false;
        m_Desc.as_MultiPlayer.m_DirtyProfile          = false;
		m_Desc.as_MultiPlayer.m_DirtySavegame         = false;
		m_Desc.as_MultiPlayer.m_OnlineStat            = online;
		m_Desc.as_MultiPlayer.m_ProfileStat           = profileStat;
		m_Desc.as_MultiPlayer.m_CommunityStat         = communityStat;
		m_Desc.as_MultiPlayer.m_ServerAuthoritative   = serverAuthoritative;
		m_Desc.as_MultiPlayer.m_ReadPriority          = readPriority <= PS_LOW_PRIORITY ? readPriority : 0;
		m_Desc.as_MultiPlayer.m_FlushPriority         = flushPriority <= RL_PROFILESTATS_MAX_PRIORITY ? flushPriority : RL_PROFILESTATS_MIN_PRIORITY;
		m_Desc.as_MultiPlayer.m_Category              = category <= 15 ? category : 0;
		m_Desc.as_MultiPlayer.m_CharacterStat         = characterStat;
		m_Desc.as_MultiPlayer.m_TriggerEventValueChanged = triggerEventValueChanged;
		m_Desc.as_MultiPlayer.m_ProfileSetting        = profileSetting;
        m_Desc.as_MultiPlayer.m_Group                 = 7;
		m_Desc.as_MultiPlayer.m_BlockReset            = blockReset;
		m_Desc.as_MultiPlayer.m_BlockProfileStatFlush = blockProfileStatFlush;
		m_Desc.as_MultiPlayer.m_BlockSynchEventFlush  = blockSynchEventFlush;
		m_Desc.as_MultiPlayer.m_DirtyInThisSession    = false;
		m_Desc.as_MultiPlayer.m_PlayerInventory       = false;
		m_Desc.as_MultiPlayer.m_NoSaveStat			  = bNoSaveStat;
		m_Desc.as_MultiPlayer.m_AlwaysFlush			  = bAlwaysFlush;

		if (online)
		{
			m_Desc.as_MultiPlayer.m_Synched       = false;
			m_Desc.as_MultiPlayer.m_ForcedSynched = false;
		}
	}

	~sStatDescription() {;}

	inline bool                                       GetDirty() const  {return m_Desc.as_SinglePlayer.m_Dirty;}
	inline bool                                GetIsOnlineData() const  {return m_Desc.as_SinglePlayer.m_OnlineStat;}
	inline bool                               GetIsProfileStat() const  {return m_Desc.as_SinglePlayer.m_ProfileStat;}
	inline bool                             GetIsCommunityStat() const  {return m_Desc.as_SinglePlayer.m_CommunityStat;}
	inline bool                         GetServerAuthoritative() const  {return m_Desc.as_SinglePlayer.m_ServerAuthoritative;}
	inline bool                        GetBlockSynchEventFlush() const  {return m_Desc.as_SinglePlayer.m_BlockSynchEventFlush;}
	inline bool                          GetDirtyInThisSession() const  {return m_Desc.as_SinglePlayer.m_DirtyInThisSession;}
	inline unsigned                        GetDownloadPriority() const  {return m_Desc.as_SinglePlayer.m_ReadPriority;}
	inline rlProfileStatsPriority             GetFlushPriority() const  {return m_Desc.as_SinglePlayer.m_FlushPriority;}
	inline unsigned                                GetCategory() const  {return m_Desc.as_SinglePlayer.m_Category;}
	inline bool                                     GetSynched() const  {return m_Desc.as_MultiPlayer.m_Synched;}
	inline bool                               GetForcedSynched() const  {return m_Desc.as_MultiPlayer.m_ForcedSynched;}
	inline bool                        GetReadForRemotePlayers() const  {return (PS_HIGH_PRIORITY == GetDownloadPriority() || PS_LOW_PRIORITY == GetDownloadPriority());}
	inline bool					            GetIsCharacterStat() const  {return m_Desc.as_SinglePlayer.m_CharacterStat;}
	inline bool					               GetIsPlayerStat() const  {return !GetIsCharacterStat();}
	inline bool					   GetTriggerEventValueChanged() const  {return m_Desc.as_SinglePlayer.m_TriggerEventValueChanged;}
	inline bool					           GetIsProfileSetting() const  {return m_Desc.as_SinglePlayer.m_ProfileSetting;}
	inline bool                               GetSavegameDirty() const  {return m_Desc.as_SinglePlayer.m_DirtySavegame;}
	inline bool                                  GetBlockReset() const  {return (m_Desc.as_SinglePlayer.m_BlockReset || GetIsCommunityStat());}
	inline bool                       GetBlockProfileStatFlush() const  {return m_Desc.as_SinglePlayer.m_BlockProfileStatFlush;}
	inline bool                             GetPlayerInventory() const  {return m_Desc.as_MultiPlayer.m_PlayerInventory;}
	inline bool                             GetIsNoSaveStat() const		{return m_Desc.as_MultiPlayer.m_NoSaveStat;}
	inline bool                             GetAlwaysFlush() const      {return m_Desc.as_MultiPlayer.m_AlwaysFlush;}

	inline void                    SetForcedSynched(const bool value)   { m_Desc.as_MultiPlayer.m_ForcedSynched = value; }
	inline void                    SetSynched(const bool value)         { m_Desc.as_MultiPlayer.m_Synched       = value; }
	inline void                    SetCategory(const u32 value)         { m_Desc.as_SinglePlayer.m_Category     = value; }

	inline void					            SetBlockedReset(const bool value)   { m_Desc.as_MultiPlayer.m_BlockReset = value; }
	inline void					   SetBlockProfileStatFlush(const bool value)	{ m_Desc.as_MultiPlayer.m_BlockProfileStatFlush = value; }
	inline void					     SetServerAuthoritative(const bool value)   { m_Desc.as_MultiPlayer.m_ServerAuthoritative = value; }
	inline void                       SetDirtyInThisSession(const bool value)   {m_Desc.as_SinglePlayer.m_DirtyInThisSession = value;}

    //PURPOSE:
    //  Flags the stat description that the stat is dirty
    //  You can also optionally specify whether or not the profile should be dirtied as a result
	void                                   SetDirty(const bool value, const bool bDirtyProfile = true);
	void                            SetDirtyProfile(const bool value);
	void                           SetDirtySavegame(const bool value);
	bool                       SetIsPlayerInventory( );

    inline eProfileStatsGroups GetGroup() const
    {
        return static_cast<eProfileStatsGroups>(GetIsOnlineData() ? PS_GRP_MP_START + m_Desc.as_SinglePlayer.m_Group : PS_GRP_SP_START + m_Desc.as_SinglePlayer.m_Group);
    }

	//This matches eCharacterIndex for mp characters.
	inline u32 GetMultiplayerCharacterSlot() const { Assert(GetIsOnlineData()); Assert(GetIsCharacterStat()); return (m_Desc.as_MultiPlayer.m_Group - PS_GRP_MP_1); }

    inline void SetGroup(const eProfileStatsGroups group)
    {
        Assert(GetIsOnlineData() && group >= PS_GRP_MP_START && group <= PS_GRP_MP_END
               || !GetIsOnlineData() && group >= PS_GRP_SP_START && group <= PS_GRP_SP_END);

        const eProfileStatsGroups base = GetIsOnlineData() ? PS_GRP_MP_START : PS_GRP_SP_START;
        m_Desc.as_SinglePlayer.m_Group = static_cast<eProfileStatsGroups>(group - base);
    }


	inline bool GetIsSavedInSaveGame(const bool bMultiplayerSave, const unsigned saveCategory) const  
	{
		bool result = false;

		if (GetServerAuthoritative())
			return result;

		if (GetIsCommunityStat())
			return result;

		if (GetIsProfileSetting())
			return result;

		if (GetIsOnlineData() != bMultiplayerSave)
			return result;

		if (GetIsNoSaveStat())
			return result;

		if (GetCategory() == MAX_STAT_SAVE_CATEGORY)
			result = true;
		else if (saveCategory == MAX_STAT_SAVE_CATEGORY)
			result = true;
		else if (saveCategory == GetCategory())
			result = true;

		return result;
	}

	sStatDescription &operator=(const sStatDescription &rhs)
	{
		m_Desc.as_MultiPlayer.m_Dirty                    = rhs.m_Desc.as_MultiPlayer.m_Dirty;
        m_Desc.as_MultiPlayer.m_DirtyProfile             = rhs.m_Desc.as_MultiPlayer.m_DirtyProfile;
		m_Desc.as_MultiPlayer.m_DirtySavegame            = rhs.m_Desc.as_MultiPlayer.m_DirtySavegame;
		m_Desc.as_MultiPlayer.m_OnlineStat               = rhs.m_Desc.as_MultiPlayer.m_OnlineStat;
		m_Desc.as_MultiPlayer.m_ProfileStat              = rhs.m_Desc.as_MultiPlayer.m_ProfileStat;
		m_Desc.as_MultiPlayer.m_CommunityStat            = rhs.m_Desc.as_MultiPlayer.m_CommunityStat;
		m_Desc.as_MultiPlayer.m_ServerAuthoritative      = rhs.m_Desc.as_MultiPlayer.m_ServerAuthoritative;
		m_Desc.as_MultiPlayer.m_ReadPriority             = rhs.m_Desc.as_MultiPlayer.m_ReadPriority;
		m_Desc.as_MultiPlayer.m_FlushPriority            = rhs.m_Desc.as_MultiPlayer.m_FlushPriority;
		m_Desc.as_MultiPlayer.m_Category                 = rhs.m_Desc.as_MultiPlayer.m_Category;
		m_Desc.as_MultiPlayer.m_Synched                  = rhs.m_Desc.as_MultiPlayer.m_Synched;
		m_Desc.as_MultiPlayer.m_ForcedSynched            = rhs.m_Desc.as_MultiPlayer.m_ForcedSynched;
        m_Desc.as_MultiPlayer.m_Group                    = rhs.m_Desc.as_MultiPlayer.m_Group;
		m_Desc.as_MultiPlayer.m_CharacterStat            = rhs.m_Desc.as_MultiPlayer.m_CharacterStat;
		m_Desc.as_MultiPlayer.m_TriggerEventValueChanged = rhs.m_Desc.as_MultiPlayer.m_TriggerEventValueChanged;
		m_Desc.as_MultiPlayer.m_ProfileSetting           = rhs.m_Desc.as_MultiPlayer.m_ProfileSetting;
		m_Desc.as_MultiPlayer.m_BlockReset               = rhs.m_Desc.as_MultiPlayer.m_BlockReset;
		m_Desc.as_MultiPlayer.m_BlockProfileStatFlush    = rhs.m_Desc.as_MultiPlayer.m_BlockProfileStatFlush;
		m_Desc.as_MultiPlayer.m_BlockSynchEventFlush     = rhs.m_Desc.as_MultiPlayer.m_BlockSynchEventFlush;
		m_Desc.as_MultiPlayer.m_DirtyInThisSession       = rhs.m_Desc.as_MultiPlayer.m_DirtyInThisSession;
		m_Desc.as_MultiPlayer.m_NoSaveStat				 = rhs.m_Desc.as_MultiPlayer.m_NoSaveStat;
		m_Desc.as_MultiPlayer.m_AlwaysFlush				 = rhs.m_Desc.as_MultiPlayer.m_AlwaysFlush;

		return *this;
	}
};
CompileTimeAssert(sizeof(sStatDescription) == 4);

//PURPOSE
//   This class represents a base class for a stat. Data is stored in a map.
class sStatData
{
	friend class  CStatsDataMgr;
	friend struct sStatDataPtr;

private:
#if __BANK
	static bool ms_obfuscationDisabled;
#endif
	// Data description
	sStatDescription  m_Desc;

	// Spew all changes to this stat.
	NOTFINAL_ONLY(bool m_ShowChanges;)

	// Display this stat on screen.
	NOTFINAL_ONLY(bool m_ShowOnScreen;)

	// True if the stat is updated in code False if it is updated in script.
	ASSERT_ONLY(bool m_CoderStat;)

protected:
	// PURPOSE: Accessor for the stat description.
	sStatDescription& GetDesc() { return m_Desc; }

public:
	// Tag type of data is set in constructor... ( Only set once at the start, no more changes to data type from here !!!)
	sStatData(const char* pName, sStatDescription& desc);

	virtual ~sStatData() { }

	// PURPOSE: Resets the value of the stat.
	virtual void Reset();

	// PURPOSE: Set/Get data values.
	bool  SetData(void* pData, const unsigned sizeofData, bool& retHaschangedValue);
	bool  GetData(void* pData, const unsigned sizeofData) const;

	template<typename T>
	u8 GetByte(const T Source, const u8 iByte) const
	{
		if(iByte >= sizeof(Source)){return 0;}
		u8 * C_Ptr = (u8 *)&Source;
		return *(C_Ptr+iByte);
	}
	template<typename T>
	void Encode(T& target, const T& value) const
	{
		u8 buf[sizeof(T)];
		u8 ptrBuf[sizeof(T*)];
		T* targetPtr = &target;

		memcpy(buf,&value,sizeof(T));
		memcpy(ptrBuf,&targetPtr,sizeof(T*));
		BANK_ONLY(if(!ms_obfuscationDisabled))
		for(u8 i=0;i<sizeof(T);i++)
		{
			buf[i]=buf[i]^ptrBuf[i%sizeof(T*)];
		}
		memcpy(&target,buf,sizeof(T));
#if __BANK
		if(!Verifyf(value==Decode(target),"Stat encoding issue!"))
		{
			Errorf("Stat encoding issue!");
		}
#endif
	}

	template<typename T>
	T Decode(const T& source) const 
	{
		u8 buf[sizeof(T)];
		u8 ptrBuf[sizeof(T*)];
		const u8* targetPtr = (const u8*)&source;

		memcpy(buf,&source,sizeof(T));
		memcpy(ptrBuf,&targetPtr,sizeof(T*));
		BANK_ONLY(if(!ms_obfuscationDisabled))
		for(u8 i=0;i<sizeof(T);i++)
		{
			buf[i]=buf[i]^ptrBuf[i%sizeof(T*)];			
		}
		
		T retval = static_cast<T>(0);
		memcpy(&retval,buf,sizeof(T));
		return retval;
	}
protected:

	virtual void      SetInt(const   int /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void    SetInt64(const   s64 /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void    SetFloat(const float /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void  SetBoolean(const  bool /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void    SetUInt8(const    u8 /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void   SetUInt16(const   u16 /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void   SetUInt32(const   u32 /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void   SetUInt64(const   u64 /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual void   SetString(const char* /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) }
	virtual bool   SetUserId(const char* /*value*/) { BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); ) return false; }

public:
	virtual int             GetInt()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return 0;}
	virtual s64           GetInt64()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return 0;}
	virtual float         GetFloat()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return 0.0f;}
	virtual bool        GetBoolean()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return false;}
	virtual u8            GetUInt8()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return 0;}
	virtual u16          GetUInt16()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return 0;}
	virtual u32          GetUInt32()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return 0;}
	virtual u64          GetUInt64()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return 0;}
	virtual const char*  GetString()                 const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return NULL;}
	virtual bool         GetUserId(char*, const int) const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return false;}
	virtual bool         GetProfileSetting(void* )   const {BANK_ONLY( Assertf(0, "Invalid call for Stat type=%s", GetTypeLabel()); )return false;}

public:
	// PURPOSE: Return TRUE if the values match.
	virtual bool ValueIsEqual(void* pData) const = 0;

	// PURPOSE: Returns the stat type.
	virtual StatType     GetType()      const { return STAT_TYPE_NONE; }
	virtual const char*  GetTypeLabel() const { return "NONE"; };

	// PURPOSE: Returns the size of the data.
	virtual unsigned  GetSizeOfData() const = 0;

	// PURPOSE: Returns true if this stat is actually an obfuscated typr
	virtual bool IsObfuscated() const { return false; }

	// PURPOSE: Returns TRUE if the base type of the stat matches "type".
	bool  GetIsBaseType(const StatType type) const { return (GetType() == type); }

	// PURPOSE: Accessor for the stat description.
	const sStatDescription& GetDesc() const { return static_cast<const sStatDescription&>(m_Desc); }

	// PURPOSE: Read in value of the profile stat.
	bool  ReadInProfileStat(const rlProfileStatsValue* val, bool& flushStat);
	bool  ProfileStatMatches(const rlProfileStatsValue* val);

	// PURPOSE: Read/Sets the playstat message and clears the playstat flag.
	bool       WriteOutProfileStat(rlProfileStatsValue* val);
	bool  ConstWriteOutProfileStat(rlProfileStatsValue* val) const;

	// PURPOSE: Return the expected profile stat type depending on our type.
	rlProfileStatsType  GetExpectedProfileStatType() const;

	// PURPOSE: Return true if the profile stat type matches our type.
	bool  ExpectedProfileStatType(const rlProfileStatsValue* val) const;

	//PURPOSE: Return TRUE if the stat is updated in code.
	ASSERT_ONLY( inline bool GetIsCodeStat() const { return m_CoderStat; } )

	// PURPOSE: Clear Dirty flag of stat.
	virtual void SetDirty(const bool bDirtyProfile);
	BANK_ONLY( void SetDirtyBank(); );

	// PURPOSE: Setup flags to re-synch server authoritative stats.
	virtual void ResynchServerAuthoritative();

protected:
	// PURPOSE: Return True if the current value is the default.
	virtual bool IsDefaultValue() const { return false; }

#if !__FINAL

public:
	char* ValueToString(char* buf, const unsigned bufSize) const;
	void  ToString() const;

	bool  DebugStatChanges() const { return m_ShowChanges; }
	void  SetOwner(const bool ASSERT_ONLY(coder)) { ASSERT_ONLY( m_CoderStat = coder; ) }

private:
	void  AssertsAtCompileTime();

#endif // !__FINAL
};

//Integer
class sIntStatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	int m_Value; //stat value

public:
	sIntStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value()
	{
		s32 init = 0;
		Encode(m_Value,init);
	}

	virtual bool   IsDefaultValue()          const {return Decode(m_Value) == 0;}
	virtual void   Reset()                         {sStatData::Reset(); Encode(m_Value,0);}
	virtual bool   ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((int*)pData) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_INT;}
	virtual const char*   GetTypeLabel() const {return "INT";}
	virtual unsigned     GetSizeOfData() const {return sizeof(int);}

	virtual float             GetFloat() const {return static_cast<float>(Decode(m_Value));}
	virtual int                 GetInt() const {return Decode(m_Value);}
	virtual u8                GetUInt8() const {return static_cast<u8>(Decode(m_Value));}
	virtual u16              GetUInt16() const {return static_cast<u16>(Decode(m_Value));}
	virtual u32              GetUInt32() const {return static_cast<u32>(Decode(m_Value));}
	virtual u64              GetUInt64() const {return static_cast<u64>(Decode(m_Value));}
	virtual s64               GetInt64() const {return static_cast<s64>(Decode(m_Value));}

	virtual void SetInt(const int value) { Encode(m_Value,value);}
};

class sInt64StatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	s64 m_Value; //stat value

public:
	sInt64StatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0)
	{
		s64 init = 0;
		Encode(m_Value,init);
	}

	virtual bool   IsDefaultValue()          const {return Decode(m_Value) == 0;}
	virtual void   Reset()                         {sStatData::Reset(); Encode(m_Value, (s64)0);}
	virtual bool   ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((s64*)pData) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_INT64;}
	virtual const char*   GetTypeLabel() const {return "INT64";}
	virtual unsigned     GetSizeOfData() const {return sizeof(s64);}

	virtual float             GetFloat() const {return static_cast<float>(Decode(m_Value));}
	virtual int                 GetInt() const {return static_cast<int>(Decode(m_Value));}
	virtual s64               GetInt64() const {return Decode(m_Value);}

	virtual void SetInt64(const s64 value) {Encode(m_Value,value);}
};


//Obfuscated integer
class sObfIntStatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	sysObfuscated<int> m_Value; //stat value

public:
	sObfIntStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0)
	{
	}

	virtual bool   IsDefaultValue()          const {return m_Value == 0;}
	virtual void   Reset()                         {sStatData::Reset(); m_Value = 0;}
	virtual bool   ValueIsEqual(void* pData) const {return pData ? m_Value == *((int*)pData) : false;}

	// NB: Obfuscated types report back as their unobfuscated values
	virtual StatType           GetType() const {return STAT_TYPE_INT;}
	virtual const char*   GetTypeLabel() const {return "INT";}
	virtual unsigned     GetSizeOfData() const {return sizeof(int);}
	virtual bool          IsObfuscated() const { return true; }

	virtual float             GetFloat() const {return static_cast<float>(m_Value);}
	virtual int                 GetInt() const {return m_Value;}
	virtual u8                GetUInt8() const {return static_cast<u8>(m_Value);}
	virtual u16              GetUInt16() const {return static_cast<u16>(m_Value);}
	virtual u32              GetUInt32() const {return static_cast<u32>(m_Value);}
	virtual u64              GetUInt64() const {return static_cast<u64>(m_Value);}
	virtual s64               GetInt64() const {return static_cast<s64>(m_Value);}

	virtual void   SetInt(const int value) { m_Value = value;}
};

class sObfInt64StatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	sysObfuscated<s64> m_Value; //stat value

public:
	sObfInt64StatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0)
	{
	}

	virtual bool   IsDefaultValue()          const {return m_Value == 0;}
	virtual void   Reset()                         {sStatData::Reset(); m_Value = 0;}
	virtual bool   ValueIsEqual(void* pData) const {return pData ? m_Value == *((s64*)pData) : false;}

	// NB: Obfuscated types report back as their unobfuscated values
	virtual StatType           GetType() const {return STAT_TYPE_INT64;}
	virtual const char*   GetTypeLabel() const {return "INT64";}
	virtual unsigned     GetSizeOfData() const {return sizeof(s64);}
	virtual bool          IsObfuscated() const { return true; }

	virtual float             GetFloat() const {return static_cast<float>(m_Value);}
	virtual int                 GetInt() const {return static_cast<int>(m_Value);}
	virtual s64               GetInt64() const {return m_Value;}

	virtual void SetInt64(const s64 value) {m_Value = value;}
};

//Label
class sLabelStatData : public sIntStatData
{
	friend class CStatsDataMgr;

private:
	BANK_ONLY(char m_Label[MAX_STAT_LABEL_SIZE];)

public:
	sLabelStatData(const char* pName, sStatDescription& desc) : sIntStatData(pName, desc)
	{
		BANK_ONLY(m_Label[0] = '\0';)
	}

	virtual StatType          GetType() const { return STAT_TYPE_TEXTLABEL; }
	virtual const char*  GetTypeLabel() const { return "TEXTLABEL"; }

	virtual void SetInt(const int value);
};

//Float
class sFloatStatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	float m_Value; //stat value

public:
	sFloatStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0.0f)
	{
		float init = 0.0f;
		Encode(m_Value,init);
	}

	virtual bool   IsDefaultValue()          const {return (Decode(m_Value) == 0.0f);}
	virtual void   Reset()                         {sStatData::Reset(); float init= 0.0f; Encode(m_Value,init);}
	virtual bool   ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((float*)pData) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_FLOAT;}
	virtual const char*   GetTypeLabel() const {return "FLOAT";}
	virtual unsigned     GetSizeOfData() const {return sizeof(float);}

	virtual float             GetFloat() const {return Decode(m_Value);}
	virtual int                 GetInt() const {return static_cast<int>(Decode(m_Value));}
	virtual u8                GetUInt8() const {return static_cast<u8>(Decode(m_Value));}
	virtual u16              GetUInt16() const {return static_cast<u16>(Decode(m_Value));}
	virtual u32              GetUInt32() const {return static_cast<u32>(Decode(m_Value));}
	virtual u64              GetUInt64() const {return static_cast<u64>(Decode(m_Value));}

	virtual void   SetFloat(const float value) { Encode(m_Value,value); }
};

class sObfFloatStatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	sysObfuscated<float> m_Value; //stat value

public:
	sObfFloatStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0.0f)
	{
	}

	virtual bool   IsDefaultValue()          const {return (m_Value == 0.0f);}
	virtual void   Reset()                         {sStatData::Reset(); m_Value = 0.0f;}
	virtual bool   ValueIsEqual(void* pData) const {return pData ? m_Value == *((float*)pData) : false;}

	// NB: Obfuscated types report back as their unobfuscated values
	virtual StatType           GetType() const {return STAT_TYPE_FLOAT;}
	virtual const char*   GetTypeLabel() const {return "FLOAT";}
	virtual unsigned     GetSizeOfData() const {return sizeof(float);}
	virtual bool          IsObfuscated() const { return true; }

	virtual float             GetFloat() const {return m_Value;}
	virtual int                 GetInt() const {return static_cast<int>(m_Value);}
	virtual u8                GetUInt8() const {return static_cast<u8>(m_Value);}
	virtual u16              GetUInt16() const {return static_cast<u16>(m_Value);}
	virtual u32              GetUInt32() const {return static_cast<u32>(m_Value);}
	virtual u64              GetUInt64() const {return static_cast<u64>(m_Value);}

	virtual void   SetFloat(const float value) { m_Value = value; }
};

//Boolean
class sBoolStatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	bool m_Value; //stat value

public:
	sBoolStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(false)
	{
		bool init = false;
		Encode(m_Value,init);
	}


	virtual bool  IsDefaultValue()          const {return (Decode(m_Value) == false);}
	virtual void  Reset()                         {sStatData::Reset(); bool init = false; Encode(m_Value,init);}
	virtual bool  ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((bool*)pData) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_BOOLEAN;}
	virtual const char*   GetTypeLabel() const {return "BOOL";}
	virtual unsigned     GetSizeOfData() const {return sizeof(bool);}
	virtual bool            GetBoolean() const {return Decode(m_Value);}

	virtual void SetBoolean(const bool value) {Encode(m_Value, value);}
};

//String
class sStringStatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	char m_Value[MAX_STAT_STRING_SIZE]; //stat value

public:
	sStringStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
	{
		sysMemSet(m_Value, 0, GetSizeOfData());
	}

	virtual bool  IsDefaultValue()          const {return (strlen(m_Value) == 0);}
	virtual void  Reset()                         {sStatData::Reset(); sysMemSet(m_Value, 0, GetSizeOfData());}
	virtual bool  ValueIsEqual(void* pData) const {return pData ? ( (strlen(m_Value) == strlen((char*)pData)) && (memcmp(m_Value, pData, strlen((char*)pData)) == 0)) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_STRING;}
	virtual const char*   GetTypeLabel() const {return "STRING";}
	virtual unsigned     GetSizeOfData() const {return sizeof(m_Value);}
	virtual const char*      GetString() const {return m_Value;}

	virtual void SetString(const char* value) {if(value) safecpy(m_Value, value, GetSizeOfData()); }
};

//Unsigned 8
class sUns8StatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	u8 m_Value; //stat value

public:
	sUns8StatData(const char* pName, sStatDescription& desc) : sStatData(pName, desc), m_Value(0)
	{
		u8 init = 0;
		Encode(m_Value,init);
	}

	virtual bool IsDefaultValue()          const {return (Decode(m_Value) == 0);}
	virtual void Reset()                         {sStatData::Reset(); u8 init = 0; Encode(m_Value,init);}
	virtual bool ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((u8*)pData) : false;}

	virtual StatType           GetType()   const {return STAT_TYPE_UINT8;}
	virtual const char*   GetTypeLabel()   const {return "UINT8";}
	virtual unsigned     GetSizeOfData()   const {return sizeof(u8);}

	virtual float               GetFloat() const {return static_cast<float>(Decode(m_Value));}
	virtual int                   GetInt() const {return static_cast<int>(Decode(m_Value));}
	virtual u8                  GetUInt8() const {return Decode(m_Value);}
	virtual u16                GetUInt16() const {return static_cast<u16>(Decode(m_Value));}
	virtual u32                GetUInt32() const {return static_cast<u32>(Decode(m_Value));}
	virtual u64                GetUInt64() const {return static_cast<u64>(Decode(m_Value));}

	virtual void SetUInt8(const u8 value)        {Encode(m_Value,value);}
};

//Unsigned 16
class sUns16StatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	u16 m_Value; //stat value

public:
	sUns16StatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0)
	{
		u16 init = 0;
		Encode(m_Value,init);
	}

	virtual bool IsDefaultValue()          const {return (Decode(m_Value) == 0);}
	virtual void Reset()                         {sStatData::Reset(); u16 init = 0; Encode(m_Value,init);}
	virtual bool ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((u16*)pData) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_UINT16;}
	virtual const char*   GetTypeLabel() const {return "UINT16";}
	virtual unsigned     GetSizeOfData() const {return sizeof(u16);}

	virtual float             GetFloat() const {return static_cast<float>(Decode(m_Value));}
	virtual int                 GetInt() const {return static_cast<int>(Decode(m_Value));}
	virtual u8                GetUInt8() const {return static_cast<u8>(Decode(m_Value));}
	virtual u16              GetUInt16() const {return Decode(m_Value);}
	virtual u32              GetUInt32() const {return static_cast<u32>(Decode(m_Value));}
	virtual u64              GetUInt64() const {return static_cast<u64>(Decode(m_Value));}

	virtual void  SetUInt16(const u16 value)       {Encode(m_Value,value);}
};

//Unsigned 32
class sUns32StatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	u32 m_Value; //stat value

public:
	sUns32StatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0)
	{
		u32 init = 0;
		Encode(m_Value,init);
	}

	virtual bool IsDefaultValue()          const {return (Decode(m_Value) == 0);}
	virtual void Reset()                         {sStatData::Reset(); u32 init = 0; Encode(m_Value,init);}
	virtual bool ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((u32*)pData) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_UINT32;}
	virtual const char*   GetTypeLabel() const {return "UINT32";}
	virtual unsigned     GetSizeOfData() const {return sizeof(u32);}

	virtual float             GetFloat() const {return static_cast<float>(Decode(m_Value));}
	virtual int                 GetInt() const {return static_cast<int>(Decode(m_Value));}
	virtual u8                GetUInt8() const {return static_cast<u8>(Decode(m_Value));}
	virtual u16              GetUInt16() const {return static_cast<u16>(Decode(m_Value));}
	virtual u32              GetUInt32() const {return Decode(m_Value);}
	virtual u64              GetUInt64() const {return static_cast<u64>(Decode(m_Value));}

	virtual void  SetUInt32(const u32 value) {Encode(m_Value,value);}
};

//Unsigned 64
class sUns64BaseStatData : public sStatData
{
	friend class CStatsDataMgr;

protected:
	u64 m_Value; //stat value

public:
	sUns64BaseStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0)
	{
		u64 init = 0;
		Encode(m_Value,init);
	}

	virtual bool IsDefaultValue()          const {return (Decode(m_Value) == 0);}
	virtual void Reset()                         {sStatData::Reset(); u64 init = 0;Encode(m_Value,init);}
	virtual bool ValueIsEqual(void* pData) const {return pData ? Decode(m_Value) == *((u64*)pData) : false;}

	virtual StatType           GetType() const {return STAT_TYPE_UINT64;}
	virtual const char*   GetTypeLabel() const {return "UINT64";}
	virtual unsigned     GetSizeOfData() const {return sizeof(u64);}

	virtual float             GetFloat() const {return static_cast<float>(Decode(m_Value));}
	virtual int                 GetInt() const {return static_cast<int>(Decode(m_Value));}
	virtual u8                GetUInt8() const {return static_cast<u8>(Decode(m_Value));}
	virtual u16              GetUInt16() const {return static_cast<u16>(Decode(m_Value));}
	virtual u32              GetUInt32() const {return static_cast<u32>(Decode(m_Value));}
	virtual u64              GetUInt64() const {return Decode(m_Value);}

	virtual void SetUInt64(const u64 value) {Encode(m_Value,value);}
};

//Position
class sUns64StatData : public sUns64BaseStatData
{
	friend class CStatsDataMgr;

private:
#if __BANK
	u32 m_ValueLo;
	u32 m_ValueHi;
#endif // __BANK

public:
	sUns64StatData(const char* pName, sStatDescription& desc) 
		: sUns64BaseStatData(pName, desc)
#if __BANK
		,m_ValueLo(0)
		,m_ValueHi(0)
#endif // __BANK
	{
	}

	virtual void  Reset() 
	{
		sUns64BaseStatData::Reset(); 
		BANK_ONLY( m_ValueLo = 0; )
		BANK_ONLY( m_ValueHi = 0; ) 
	}

	virtual void  SetUInt64(const u64 value);
};

//Unsigned 64
class sObfUns64StatData : public sStatData
{
	friend class CStatsDataMgr;

protected:
	sysObfuscated<u64> m_Value; //stat value

public:
	sObfUns64StatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
		, m_Value(0)
	{
	}

	virtual bool IsDefaultValue()          const {return (m_Value == 0);}
	virtual void Reset()                         {sStatData::Reset(); m_Value = 0;}
	virtual bool ValueIsEqual(void* pData) const {return pData ? m_Value == *((u64*)pData) : false;}

	// NB: Obfuscated types report back as their unobfuscated values
	virtual StatType           GetType() const {return STAT_TYPE_UINT64;}
	virtual const char*   GetTypeLabel() const {return "UINT64";}
	virtual unsigned     GetSizeOfData() const {return sizeof(u64);}
	virtual bool          IsObfuscated() const { return true; }

	virtual float             GetFloat() const {return static_cast<float>(m_Value);}
	virtual int                 GetInt() const {return static_cast<int>(m_Value);}
	virtual u8                GetUInt8() const {return static_cast<u8>(m_Value);}
	virtual u16              GetUInt16() const {return static_cast<u16>(m_Value);}
	virtual u32              GetUInt32() const {return static_cast<u32>(m_Value);}
	virtual u64              GetUInt64() const {return m_Value;}

	virtual void SetUInt64(const u64 value) {m_Value = value;}
};

//Position
class sPosStatData : public sUns64BaseStatData
{
	friend class CStatsDataMgr;

private:
	BANK_ONLY( Vector3 m_Position; ) //Human readable position

public:
	sPosStatData(const char* pName, sStatDescription& desc) : sUns64BaseStatData(pName, desc)
	{
		BANK_ONLY( m_Position = Vector3(0.0f, 0.0f, 0.0f); )
	}

	virtual void  Reset() { sUns64BaseStatData::Reset(); BANK_ONLY( m_Position = Vector3(0.0f, 0.0f, 0.0f); ) }

	virtual StatType     GetType()       const {return STAT_TYPE_POS;}
	virtual const char*  GetTypeLabel()  const {return "POS";}

	virtual void  SetUInt64(const u64 value);
};

//Date
class sDateStatData : public sUns64BaseStatData
{
	friend class CStatsDataMgr;

public:
#if __BANK
	struct sDate
	{
		sDate() : m_year(0), m_month(0), m_day(0), m_hour(0), m_minutes(0), m_seconds(0), m_milliseconds(0)
		{
		}

		void Reset()
		{
			m_year         = 0;
			m_month        = 0;
			m_day          = 0;
			m_hour         = 0;
			m_minutes      = 0;
			m_seconds      = 0;
			m_milliseconds = 0;
		}

		int m_year;
		int m_month;
		int m_day;
		int m_hour;
		int m_minutes;
		int m_seconds;
		int m_milliseconds;
	};
#endif // __BANK

private:
	BANK_ONLY( sDate m_Date; ) //Human readable date

public:
	sDateStatData(const char* pName, sStatDescription& desc)
		: sUns64BaseStatData(pName, desc)
	{
	}

	virtual void  Reset() { sUns64BaseStatData::Reset(); BANK_ONLY( m_Date.Reset(); ) }

	virtual StatType     GetType()       const { return STAT_TYPE_DATE; }
	virtual const char*  GetTypeLabel()  const { return "DATE"; }

	virtual void  SetUInt64(const u64 value);
};


//Position
class sPackedStatData : public sUns64BaseStatData
{
	friend class CStatsDataMgr;

#if __BANK
public:
	static const unsigned WIDGET_STRING_SIZE = 128;

private:
	char m_WidgetValue[WIDGET_STRING_SIZE];
	int  m_NumberOfBits;
#endif // __BANK

public:
	sPackedStatData(const char* pName, sStatDescription& desc, const int BANK_ONLY( numberOfBits)) : sUns64BaseStatData(pName, desc)
	{
#if __BANK
		m_WidgetValue[0] = '\0';
		m_NumberOfBits = numberOfBits;
#endif // __BANK
	}

	virtual void  Reset() 
	{
		sUns64BaseStatData::Reset();
		BANK_ONLY( m_WidgetValue[0] = '\0'; )
	}

	virtual StatType     GetType()       const { return STAT_TYPE_PACKED; }
	virtual const char*  GetTypeLabel()  const { return "PACKED"; }

	virtual void  SetUInt64(const u64 value);

#if __BANK
	void SetupWidget();
	bool NumberOfBitsIsValid() const { return (m_NumberOfBits == 1 || m_NumberOfBits == 8); }
#endif // __BANK
};

//Unsigned 64
class sUserIdStatData : public sStatData
{
	friend class CStatsDataMgr;

protected:
	u64  m_UserId;

#if RLGAMERHANDLE_NP
	//The Online ID is used to identify accounts, and is chosen by the user upon signup. 
	//  It is a 3- to 16-character (A–Z, a-z, 0-9) string composed of alphanumeric characters, 
	//  hyphens, and/or underscores. The Online ID is guaranteed to be unique.
	sUserIdStatData*  m_NextUserId;
#endif

#if __BANK
public:
	static const unsigned WIDGET_STRING_SIZE = 128;
private:
	char m_WidgetValue[WIDGET_STRING_SIZE];
#endif

public:
	sUserIdStatData(const char* pName, sStatDescription& desc)
		: sStatData(pName, desc)
	{
		m_UserId = 0;
#if RLGAMERHANDLE_NP
		m_NextUserId = 0;
#endif

		BANK_ONLY( m_WidgetValue[0] = '\0'; )
	}

	virtual bool IsDefaultValue() const;
	virtual void Reset();
	virtual bool ValueIsEqual(void* pData) const;

	virtual StatType     GetType()       const { return STAT_TYPE_USERID; }
	virtual const char*  GetTypeLabel()  const { return "USERID"; }
	virtual unsigned     GetSizeOfData() const;

	virtual bool  GetUserId(char* userId, const int bufSize) const;
	virtual bool  SetUserId(const char* userId);

	virtual u64   GetUInt64() const {return m_UserId;}
	virtual void  SetUInt64(const u64 value) {m_UserId = value;}

	virtual void SetDirty(const bool bDirtyProfile);
	virtual void ResynchServerAuthoritative();
};

//Profile Setting
class sProfileSettingStatData : public sStatData
{
	friend class CStatsDataMgr;

private:
	u16 m_profileId;

public:
	sProfileSettingStatData(const char* pName, sStatDescription& desc, const u16 profileId) : sStatData(pName, desc), m_profileId(profileId) 
	{
		Reset();
	}

	virtual StatType           GetType() const {return STAT_TYPE_PROFILE_SETTING;}
	virtual const char*   GetTypeLabel() const {return "PROFILE_SETTING";}
	virtual unsigned     GetSizeOfData() const {return sizeof(int);}

	virtual bool          ValueIsEqual(void* pData) const;

	virtual bool     GetProfileSetting(void* pData) const;
	virtual int                 GetInt() const;
};


// Class to encapsulate the stat data pointer
struct sStatDataPtr
{
	friend class CStatsDataMgr;

private:
	StatId      m_StatKey;
	sStatData*  m_pStat;

public:
	sStatDataPtr() : m_pStat(0) {}
	sStatDataPtr(sStatData* pStat) : m_pStat(pStat) {}

	StatId&               GetKey()       {return m_StatKey;}
	const StatId&         GetKey() const {return m_StatKey;}
	bool              KeyIsvalid() const {return m_StatKey.IsValid();}

#if !__NO_OUTPUT
	const char*       GetKeyName() const {return m_StatKey.GetName();}
#else
	const char*       GetKeyName() const {FastAssert(0); return 0;}
#endif

	operator const    sStatData*() const {FastAssert(KeyIsvalid());return m_pStat;}
	operator const    sStatData*()       {FastAssert(KeyIsvalid());return m_pStat;}
	operator const sStatDataPtr*()       {return this;}

	sStatDataPtr& operator=(const sStatDataPtr& rhs)
	{
		m_StatKey = rhs.m_StatKey;
		m_pStat   = rhs.m_pStat;
		return *this;
	}

	bool operator<(const sStatDataPtr& rhs) {return m_StatKey.GetHash() < rhs.m_StatKey.GetHash();}
	bool operator>(const sStatDataPtr& rhs) {return m_StatKey.GetHash() > rhs.m_StatKey.GetHash();}


private:
	void            SetKey(const char* key)                  {m_StatKey = key;}
	void        SetKeyHash(const u32 hash, const char* name) {m_StatKey.SetHash(hash, name);}

	operator    sStatData*() {return m_pStat;}
	operator sStatDataPtr*() {return this;}
};

//Forward declaration
//Convenient struct used for parsing the stats from the xml files.
struct sStatParser
{
public:
	ConstString       m_Name;
	ConstString       m_Multihash;
	ConstString       m_Owner;
	sStatDescription  m_Desc;
	StatType          m_Type;
	const char*       m_RawType;
	parTreeNode*      m_Node;
	int               m_NumBitsPerValue;
	u16               m_ProfileSettingId;
	u32               m_MetricSettingId;
	bool              m_GroupIsSet;
};

//PURPOSE
//  Manage all stats data - stats storage, display lists, etc.
class CStatsDataMetricMgr
{
	typedef atMap < u32, u32 > StatsMetricsMap;

public:
	struct TrackedData 
	{
		TrackedData()
			: m_Count(0)
			, m_NumStats(0)
			, m_ElapsedTime(0)
			, m_LevelUpElapsedTime(0)
			, m_MissionType(0)
			, m_Rank(-1)
			, m_SessionType(-1)
			, m_RootContentId(0)
		{
		}
		TrackedData(const TrackedData& data)
			: m_Count(data.m_Count)
			, m_NumStats(data.m_NumStats)
			, m_ElapsedTime(data.m_ElapsedTime)
			, m_LevelUpElapsedTime(data.m_LevelUpElapsedTime)
			, m_MissionType(data.m_MissionType)
			, m_Rank(data.m_Rank)
			, m_SessionType(data.m_SessionType)
			, m_RootContentId(data.m_RootContentId)
		{
		}

		//itemCount	u32	number of stats changed
		u32 m_Count;
		u32 m_NumStats;

		//elapsedTime	u32	time passed since we started counting in seconds
		u32 m_ElapsedTime;

		//levelUp	bool	whether the player has just leveled up
		u32 m_LevelUpElapsedTime;

		//missionType	int	what type of mission the player is currently in; null if not applicable
		u32 m_MissionType;

		//rootContentId	string	root content ID; null if not applicable
		u32 m_RootContentId;

		//pRank	int	the player's current rank
		int m_Rank;

		//sessionType	int	type of session : freemode public, instanced, private etc.
		int m_SessionType;
	};

public:
	CStatsDataMetricMgr() 
		: m_LastFlushTime(0)
		, m_StatsUpdateCount(0)
		, m_FlushCount(0)
		, m_LevelUpElapsedTime(0)
	{ 
		; 
	}
	void Update();
	void RegisterNewStatMetric(sStatParser& statparser, const StatId& key);
	void RegisterStatChanged(const StatId& statKey);

private:
	StatsMetricsMap m_Stats;
	u32 m_LastFlushTime;
	u32 m_StatsUpdateCount;
	u32 m_FlushCount;
	u32 m_LevelUpElapsedTime;
};

//PURPOSE
//  Manage all stats data - stats storage, display lists, etc.
class CStatsDataMgr
{
public:
	typedef atInBinaryMap< sStatDataPtr, StatId > StatsMap;

private:

	//Store data for all stats
	StatsMap m_aStatsData;

	//Multiplayer savegame manager
	CStatsSavesMgr  m_SaveMgr;

	//True if online data has been synched.
	bool m_OnlineDataIsSynched;

	//Hold Profile stats for remote players
	unsigned               m_NumHighPriorityStats;
	int                   *m_HighPriorityStatIds;
	CProfileStatsRecords   m_HighPriorityRecords[MAX_NUM_REMOTE_GAMERS];
	unsigned               m_NumLowPriorityStats;
	int                   *m_LowPriorityStatIds;
	CProfileStatsRecords   m_LowPriorityRecords[MAX_NUM_REMOTE_GAMERS];

	CProfileStatsReadSessionStats::EndReadCallback  m_CallbackEndReadRemoteProfileStats;
	CProfileStatsReadSessionStats  m_ReadSessionProfileStats;

	//Track last time we checked for dirty stats and give it some time 
	//    before we can do such a operation.
	u32  m_CheckForDirtyProfileStats;

    //Stores the profile stats timestamp that we last loaded from our savegame
    u64  m_ProfileStatsTimestampLoadedFromSavegame;
    //Stores the savegame timestamp that we last loaded from our savegame
    u64  m_SavegameTimestampLoadedFromSavegame;

	//Local Community stats manager
	CNetworkCommunityStats m_CommunityStats;

	//Stats we track metrics
	CStatsDataMetricMgr m_StatsMetricsMgr;

public:
	CStatsDataMgr() 
		: m_OnlineDataIsSynched(true)
		  ,m_NumHighPriorityStats(0)
		  ,m_HighPriorityStatIds(0)
		  ,m_NumLowPriorityStats(0)
		  ,m_LowPriorityStatIds(0)
		  ,m_CheckForDirtyProfileStats(0)
          ,m_ProfileStatsTimestampLoadedFromSavegame(0)
          ,m_SavegameTimestampLoadedFromSavegame(0)
	{
		m_aStatsData.Reset();

#if __ASSERT
		m_numObfuscatedStatTypes = 0;
#endif // __ASSERT

#if  __BANK
		m_BkGroup    = NULL;
		m_ScriptSpew = false;
		m_TestFail   = false;

		m_bSpBankCreated = false;
		m_bMpBankCreated = false;
#endif // __BANK
	}

	~CStatsDataMgr();

	void  Init(unsigned initMode);
	void  Shutdown(unsigned shutdownMode);
	void  Update();
	void  Clear();
	void  ClearProcessFile(const char* file);

	//Reset all stats values.
	//Specify dirtyProfile=true if you aren't also doing a server reset and need these stats
    //to be flushed to the backend
	void  ResetAllStats(const bool offlineDataOnly = false, const bool dirtyProfile = false);
	void  ResetAllOnlineCharacterStats(const int prefix, const bool saveGame, const bool localOnly);

	//Called when Player Sign in/out
	void  SignedOffline();
	void  SignedOut();
	void  SignedIn();

	//Called on a PreSaveBaseStats
	void  PreSaveBaseStats(const bool multiplayer);
    //Called on a PostLoadBaseStats
    void  PostLoadBaseStats(const bool multiplayer);

	OUTPUT_ONLY( void  SpewDirtyCloudOnlySats(const bool multiplayer); )

	// --- Accessors --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
public:
	CStatsSavesMgr&             GetSavesMgr() {return m_SaveMgr;}

	StatsMap::Iterator        StatIterBegin();
	StatsMap::Iterator          StatIterEnd();
	bool                       StatIterFind(const StatId& keyHash, StatsMap::Iterator& iter);
	u32                            GetCount() const {return m_aStatsData.GetCount();}
	bool                         AddNewStat(const char* name, sStatDescription& desc, const StatType& type, const u16 profileSettingId);

	void         ResynchServerAuthoritative(const bool onlinestat);
	void               SetAllStatsToSynched(const int savegameSlot, const bool onlinestat, const bool includeServerAuthoritative = false);
	void        SetAllStatsToSynchedByGroup(const eProfileStatsGroups group, const bool onlinestat, const bool includeServerAuthoritative = false);
	void                     SetStatSynched(const StatId& keyHash);
	void                     SetStatSynched(StatsMap::Iterator& statIter);
	bool                        SetStatData(const StatId& keyHash, void* pData, const unsigned sizeofData, const u32 flags = STATUPDATEFLAG_DEFAULT);
	bool                    SetStatIterData(StatsMap::Iterator& statIter, void* pData, const unsigned sizeofData, const u32 flags = STATUPDATEFLAG_DEFAULT);
	bool        GetAllOnlineStatsAreSynched( );

#if __NET_SHOP_ACTIVE
	bool               SetIsPlayerInventory(StatsMap::Iterator& statIter);
	void               ClearPlayerInventory( );
#endif //__NET_SHOP_ACTIVE

#if __BANK
	void                       SetStatDirty(const StatId& keyHash);
	static bool                GetStatObfuscationDisabled();
	//PURPOSE:  
	//	Override default BlockProfileStatFlush flag.
	void SetBlockProfileStatFlush(const StatId& keyHash, const bool value);
#endif

	bool                         IsKeyValid(const StatId& keyHash) const;
	const sStatData*                GetStat(const StatId& keyHash) const;
	const sStatDataPtr*          GetStatPtr(const StatId& keyHash) const;
	bool                   GetStatIsNotNull(const StatId& keyHash) const;
	const char*            GetStatTypeLabel(const StatId& keyHash) const;
	bool                      GetIsBaseType(const StatId& keyHash, const StatType type) const;
	bool                        GetStatData(const StatId& keyHash, void* pData, const unsigned sizeofData) const;

	CNetworkCommunityStats& GetCommunityStatsMgr() { return m_CommunityStats; }

private:
	sStatData*                GetStat(const StatId& keyHash);
	sStatDataPtr*          GetStatPtr(const StatId& keyHash);

	// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

public:
	//PURPOSE:  
	//	Traverse all stats and set to dirty profile stats that need it.
	//	This uses our internal dirty state to update the provided dirty set.
	//RETURN:
	//  Returns the number of important stats that are dirty (Important stats are stats with flush priority > RL_PROFILESTATS_MIN_PRIORITY).
	u32  CheckForDirtyProfileStats(CProfileStats::DirtyCache& dirtyCache, const bool force, const bool online);


	//PURPOSE:
	//  Returns the number of dirty stats
	u32 GetDirtyStatCount();

    //PURPOSE:
    //  Returns the number of dirty profile stats
    u32 GetDirtyProfileStatCount();

	//PURPOSE:  Callback Called when profile stats are written.
	void HandleEventProfileStats(const rlProfileStatsEvent* evt);

	// --- Network Methods ---------------------------------

	//Called when a network match starts.
	void  StartNetworkMatch();
	//Called when a network match ends.
	void  EndNetworkMatch();
	//When a player joins the game this is called.
	void  PlayerHasJoinedSession(const ActivePlayerIndex playerIndex);
	//When a player leaves the game this is called.
	void  PlayerHasLeftSession(const ActivePlayerIndex playerIndex);
	// The EndReadCallback is a callback that gets called any time when an operation has finished.
	void  EndReadRemoteProfileStatsCallback(const bool succeeded);
	// Called when we should synch the profile stats for remote players.
	void  SyncRemoteGamerProfileStats(const unsigned priority);
	//Return the value of a profile stat for a remote gamer.
	const rlProfileStatsValue*  GetRemoteProfileStat(const rlGamerHandle& handle, const int statId) const;

	void  LoadDataXMLFile(const char* cFileName, bool bSinglePlayer = true );
	void  PostLoad();

    //Return the profile stats timestamp loaded from our SP savegame
    u64 GetProfileStatsTimestampLoadedFromSavegame() {return m_ProfileStatsTimestampLoadedFromSavegame;}
    //Return the savegame timestamp loaded from our SP savegame
    u64 GetSavegameTimestampLoadedFromSavegame() {return m_SavegameTimestampLoadedFromSavegame;}

	//Reset a stat from the stats map.
	void ResetStat(CStatsDataMgr::StatsMap::Iterator& statIter, const bool dirtyProfile) { ResetStat(*statIter, dirtyProfile); }

	//PURPOSE: called to synchs Community stats.
	void UpdateCommunityStatValues();

private:
	//Reset a stat from the stats map.
	void ResetStat(sStatDataPtr* ppStat, const bool dirtyProfile);

	//Load stats metadata
	void  LoadDataFiles();
	void  LoadStatsData(parTreeNode* pNode, bool bSingleplayer);
	void  RegisterNewStat(sStatParser& statparser);
	void  RegisterNewStatForEachCharacter(sStatParser& statparser, const bool singlePlayerFile);

	//PURPOSE: called to synchs Community stats.
	void OnSynchCommunityStats(const int statId, const float result);

#if __DEV
	void  TestPosPack(float x, float y, float z, const char* statName);
	void  TestDatePack(int year , int month , int day , int hour , int min , int sec , int mill, const char* statName);
	void  TestMaskedStats();
	void  ValidateStaticStatsIds() const;
#endif

	//Spew stats data.
	void  SpewStats();


public:
	//Check for our binary data being valid to be used
	bool  IsStatsDataValid() const { return (m_aStatsData.GetCount()>0 && m_aStatsData.IsSorted()); }

private:
	// Keep track of which files we already processed
	atMap<atHashString, bool> m_ProcessedFiles;

#if __ASSERT
	u32 m_numObfuscatedStatTypes;
#endif // __ASSERT

#if __BANK
	bkGroup*  m_BkGroup;
	bool      m_ScriptSpew;
	bool      m_TestFail;
	bool	  m_bSpBankCreated;
	bool	  m_bMpBankCreated;
	netStatus m_SyncConsistencyStatus;
	bool	  m_enableDebugDraw;
	struct sStatDisplayData
	{
		sStatDisplayData():Stat(NULL),Name(NULL)
		{
		}
		sStatData* Stat;
		const char* Name;
		bool operator==(const sStatDisplayData& other)
		{
			return Stat==other.Stat;
		}
	};
	atArray<sStatDisplayData> m_debugDrawStatsData;

public:
	void GrcDebugDraw();
	void AddToDebugDraw(sStatData* data);
	void UpdateDebugDraw();
	void  Bank_InitWidgets();
	void  Bank_CreateWidgets();
	void  Bank_WidgetShutdown();
	void  Bank_CreateSpWidgets();
	void  Bank_CreateMpWidgets();
	void  Bank_CreateMpWidgetsCurrentSlot();
	void  Bank_AddStatsToGroup(bkBank* bank, const char* name);
	void  Bank_AddStatsToGroupMP(bkBank* bank, const char* name);
	void  Bank_AddStatToHisGroup(bkBank* bank, StatsMap::Iterator& iter, sStatData* pStat);
	void  Bank_AddStatWidgetValue(bkBank* bank, sStatDataPtr* ppStat);
	void  Bank_SetStatDirty(sStatDataPtr* ppStat);
	void  Bank_FlushProfileStats();
	void  Bank_ForceFlushProfileStats();
	void  Bank_ForceSynchProfileStats();
	void  Bank_DirtySomeProfileStats();
	void  Bank_ResetAllOnlineCharacterStats();
	void  Bank_ResetAllProfileStatsByGroup();
	void  Bank_ToggleUpdateProfileStats();
	bool  Bank_ScriptSpew() const { return m_ScriptSpew; }
	void  Bank_ToggleTestStatGetFail() { m_TestFail = !m_TestFail; }
	bool  Bank_ScriptTestStatGetFail() const { return m_TestFail; }
	void  Bank_ClearAllTerritoryData();
	void  Bank_SetFailSlot();
	void  Bank_ClearFailSlot();
	void  Bank_ToggleScriptChangesToTerritoryData();
	unsigned  Bank_CalculateAllDataSizes(const bool returnProfileStats = false);
	void Bank_TestCommandLeaderboards2ReadRankPrediction();
	void  Bank_CheckProfileSyncConsistency();
	void  Bank_Enable_Disable_Stats_Tracking();
#endif // __BANK
};

#endif // STATSDATAMGR_H

// EOF
