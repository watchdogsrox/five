//
// StatsDataMgr.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef _PROFILESTATSREADER_H
#define _PROFILESTATSREADER_H

//Rage Includes
#include "rline/profilestats/rlprofilestats.h"
#include "rline/profilestats/rlprofilestatscommon.h"
#include "rline/rlgamerinfo.h"
#include "atl/bitset.h"
#include "atl/delegate.h"
#include "atl/array.h"
#include "atl/bitset.h"
#include "net/status.h"

//Framework Headers
#include "fwnet/netTypes.h"

//Game Headers
#include "stats/StatsTypes.h"

#if __BANK
namespace rage 
{
	class bkBank;
};
#endif

//PURPOSE
//  A concrete class used to define a stats result record within a 
//  rlProfileStatsReadResultsBase. It is also used to hold all records for a certain player.
class CProfileStatsRecords : public rlProfileStatsRecordBase
{
	friend class CStatsDataMgr;
	friend class CProfileStatsReadScriptRequest;

private:
	unsigned  m_NumRecords;
	rlProfileStatsValue*  m_Records;

public:
	CProfileStatsRecords() : m_NumRecords(0), m_Records(0) {;}

	virtual ~CProfileStatsRecords()
	{
		m_NumRecords = 0;
		m_Records    = 0;
	}

	const rlProfileStatsValue&  GetValue(const unsigned column) const;
	virtual unsigned GetNumValues() const override { return m_NumRecords; }

protected:
	virtual const rlProfileStatsValue* GetValues() const { return m_Records; }

private:
	void  ResetRecords(const unsigned numValues, rlProfileStatsValue* records);
};


//PURPOSE
//  Read profile stats for any number of stats and gamers.
class CProfileStatsRead
{
private:
	//Read operation status.
	netStatus  m_Status;

	//Read stats values and hold all records
	rlProfileStatsReadResults m_Results;

public:
	CProfileStatsRead(){ }
	~CProfileStatsRead()
	{
		Shutdown();
	}

	void  Shutdown();

	void  Cancel();
	void  Clear();
	void  Start(const int localGamerIndex, rlGamerHandle* gamers, const unsigned numGamers);
	void  Reset(const unsigned numStats, int* statIds, const unsigned numGamers,  CProfileStatsRecords* gamerRecords);

	const rlProfileStatsValue*  GetStatValue(const int statId, const rlGamerHandle& gamer) const;
	unsigned                    GetNumberStats(const rlGamerHandle& gamer) const;
	int                         GetStatId(const int column) const;
	unsigned                    GetNumberStatIds() const;

	bool Idle()      const { return (netStatus::NET_STATUS_NONE == m_Status.GetStatus()); }
	bool Pending()   const { return m_Status.Pending();   }
	bool Failed()    const { return m_Status.Failed();    }
	bool Succeeded() const { return m_Status.Succeeded(); }
};


//PURPOSE
//  Read profile stats for script requests.
class CProfileStatsReadScriptRequest
{
	struct sStatsRecordsValues
	{
	public:
		atArray< rlProfileStatsValue >  m_Values;

	public:
		sStatsRecordsValues() {}
		~sStatsRecordsValues() {m_Values.clear();}
	};

private:
	atArray< rlGamerHandle >        m_GamerHandlers;
	atArray< int >                  m_StatIds;
	atArray< CProfileStatsRecords > m_Records;
	atArray< sStatsRecordsValues >  m_RecordValues;
	CProfileStatsRead               m_Reader;

public:
	CProfileStatsReadScriptRequest() {}
	~CProfileStatsReadScriptRequest() { Clear(); }

	void      Clear();
	bool  StartRead();

	bool   GamerExists(const rlGamerHandle& gamer) const;
	bool  StatIdExists(const int id)               const;

	bool   AddGamer(const rlGamerHandle& gamer);
	bool  AddStatId(const int id);

	const CProfileStatsRead* GetReader( ) const {return &m_Reader;}
};


//Class responsible for reading all online player profile stats. Manages all 
//stats froma ctive players in the session.
class CProfileStatsReadSessionStats
{
public:

	//PURPOSE
	//  The EndReadCallback is a callback that gets called any time a read operation has finished.
	typedef atDelegate<void ( const bool succeeded )> EndReadCallback;

private:

	//Timer before starting a High priority synch operation.
	static const unsigned TIME_FOR_HIGH_PRIORITY_SYNCH = 20*60*1000;

	//Timer before starting a Low priority synch operation.
	static const unsigned TIME_FOR_LOW_PRIORITY_SYNCH = 30*60*1000;

	//Delay before starting any synch operation.
	static const unsigned SYNCH_DELAY_TIMER = 15*1000;

	//TRUE when is initialized.
	bool  m_Initialized;

	//Total number of gamers that should be read.
	unsigned  m_TotalNumGamers;

	//Current update priority
	unsigned m_Priority;

	//Callback that gets called any time a read operation has finished.
	EndReadCallback  m_EndReadCB;

	//Profile stats reader
	CProfileStatsRead  m_Reader;

	//Hold records for high/low priority updates
	unsigned               m_NumHighPriorityStatIds;
	int                   *m_HighPriorityStatIds;
	unsigned               m_NumLowPriorityStatIds;
	int                   *m_LowPriorityStatIds;
	unsigned               m_MaxNumGamerRecords;
	CProfileStatsRecords  *m_HighPriorityGamerRecords;
	CProfileStatsRecords  *m_LowPriorityGamerRecords;

	//Maintain the gamers that we have read stats for.
	ActivePlayerIndex  m_Gamers[MAX_NUM_REMOTE_GAMERS];
	rlGamerHandle      m_GamerHandlers[MAX_NUM_REMOTE_GAMERS];

	//True when we need to synch stats.
	bool  m_StartStatsRead;

	//True when we need to synch stats for all download priorities.
	bool  m_ReadForAllPriorities;

	//Time delayed before synch start
	unsigned  m_ReadDelayTimer;

	//Last time it synched the high priority stats
	unsigned  m_LastHighPrioritySynch;

	//Last time it synched the low priority stats
	unsigned  m_LastLowPrioritySynch;


public:

	CProfileStatsReadSessionStats()
		: m_Initialized(false)
		, m_TotalNumGamers(0)
		, m_Priority(PS_HIGH_PRIORITY)
		, m_EndReadCB(NULL)
		, m_StartStatsRead(false)
		, m_ReadForAllPriorities(false)
		, m_ReadDelayTimer(0)
		, m_LastHighPrioritySynch(0)
		, m_LastLowPrioritySynch(0)
		, m_NumHighPriorityStatIds(0)
		, m_MaxNumGamerRecords(0)
		, m_HighPriorityStatIds(0)
		, m_HighPriorityGamerRecords(0)
		, m_NumLowPriorityStatIds(0)
		, m_LowPriorityStatIds(0)
		, m_LowPriorityGamerRecords(0)
	{
	}
	~CProfileStatsReadSessionStats()
	{
		Shutdown();
	}

	void  Init(const unsigned numHighPriorityStatIds
				,int* highPriorityStatIds
				,CProfileStatsRecords* highPriorityRecords
				,const unsigned numLowPriorityStatIds
				,int* lowPriorityStatIds
				,CProfileStatsRecords* lowPriorityRecords
				,const unsigned maxNumGamers
				,EndReadCallback& cb);
	void  Update();
	void  Shutdown();

	//Call this to read all profile stats for all gamer in a session.
	bool  ReadStats(const unsigned priority);

	//Should be called to remove a player from the players stats downloads.
	void  PlayerHasLeft(const ActivePlayerIndex playerIndex);

	//Should be called to download the stats for a certain player.
	void  PlayerHasJoined(const ActivePlayerIndex playerIndex);

	//Returns True if there are pending operations.
	bool  Pending() const { return (m_Reader.Pending() || m_StartStatsRead); }


private:
	void  StartStatsRead();
	void  EndStatsRead();

	bool  GamerExists(const ActivePlayerIndex playerIndex) const;
};

//Class responsible for managing the local player profile stats
class CProfileStats
{
public:
	enum eSyncType {PS_SYNCH_IDLE, PS_SYNC_SP, PS_SYNC_MP};

	#if GEN9_STANDALONE_ENABLED
	// enum that can be used to identify which game generation the profile was created on.
	enum ePofileGeneration
	{
		PS_GENERATION_NOTSET = 0,
		PS_GENERATION_GEN8   = 8,
		PS_GENERATION_GEN9   = 9,

		PS_GENERATION_MAX,
	};
	#endif

private:
	struct sSynchOperation
	{
	public:
		u32  m_OperationId;
		u32  m_ForceTimer;
		u32  m_ForceInterval;
		u32  m_ForceOnNewGameTimer;
		u32  m_SuccessCounterForSP;
		u32  m_SuccessCounterForMP;
		bool m_Force;
		bool m_FailedMP;
		int  m_httpErrorCode;
		bool m_FailedSP;
		netStatus m_Status;
		OUTPUT_ONLY( u32 m_Timer; )
#if !__FINAL
		//Used when -noynchronizesometimes is present.
		//Represents the percentage of time we should simulate failures.
		int m_SimulateFailPct;
#endif // !__FINAL

	public:
		sSynchOperation()
			: m_OperationId(PS_SYNCH_IDLE)
			, m_ForceTimer(0)
			, m_ForceInterval(0)
			, m_ForceOnNewGameTimer(0xFFFFFFFF)
			, m_SuccessCounterForSP(0)
			, m_SuccessCounterForMP(0)
			, m_Force(false)
			, m_FailedMP(false)
			, m_FailedSP(false)
			OUTPUT_ONLY( , m_Timer(0) )
			NOTFINAL_ONLY( , m_SimulateFailPct(0) )
			, m_httpErrorCode(0)
		{}

		void Shutdown();
		void OperationDone(const bool success);
		void Cancel();
	};

	struct sResetGroup
	{
	public:
        CompileTimeAssert(PS_GRP_SP_END > PS_GRP_MP_END);
        atFixedBitSet<PS_GRP_SP_END + 1> m_ResetGroup;
		bool m_Pending;
	public:
		sResetGroup() : m_ResetGroup(false), m_Pending(false) {;}
		void UnSet(const u32 grp) {m_ResetGroup.Clear(grp);}
		void   Set(const u32 grp) {m_ResetGroup.Set(grp);}
		bool IsSet(const u32 grp) const {return m_ResetGroup.IsSet(grp);}
        bool IsAnySet() const {return m_ResetGroup.AreAnySet();}
	};

private:
	static const u32 MAX_DIRTY            = 1000;
	static const u32 MAX_SUBMISSION_SIZE  = 8000;
	static const u32 FORCE_SYNCH_INTERVAL = 1*60*1000;
	static const u32 FORCE_SYNCH_NEWGAME_INTERVAL = 1*60*1000;

private:
	rlProfileStats::EventDelegate  m_EventDlgt;
	netStatus m_FlushStatus;
	bool m_FlushInProgress;
	bool m_FlushOnlineStats;
	bool m_FlushFailedMP;
	sResetGroup m_ResetGroups;
	sSynchOperation m_SynchOp;
	u32  m_minForceFlushTime;
	u32  m_lastForceFlushTime;
    // Indicates that we had too many stats to flush last time
    // and need to flush again to finish everything
    bool m_CouldntFlushAllStats;
	//Flag multiplayer Server Authoritative stats are Synched. This is independent of Flushes that FAILED.
	bool m_MPSynchIsDone;

	//Flag a MP migration satus check has been checked
	GEN9_STANDALONE_ONLY(bool m_MPMigrationStatusChecked);

#if !__FINAL
	int m_SimulateCredentialsFailPct;
	int m_SimulateFlushFailPct;
	BANK_ONLY( bool m_ClearSaveTransferStats; )
#endif

	//Value set when we start a Multiplayer Flush Operation so we can save it 
	//   locally if the flush succeeds and use it to find out about "dirty reads".
	s64 m_FlushPosixtimeMP;
	s64 m_FlushPreviousPosixTime;

public:
	bool  m_ForceFlush : 1;
	bool  m_ForceCloudOverwriteOnMPLocalStats : 1;

public:

	CProfileStats();
	~CProfileStats();

	void  Init();
	void  Shutdown(const u32 shutdownMode);
	void  CancelPendingFlush();
	void  Update();
	void  OpenNetwork();
	void  CloseNetwork();

	BANK_ONLY( void Bank_InitWidgets( bkBank& bank ); )
	BANK_ONLY( void Bank_ClearAllSaveTransferData( ); )

	//PURPOSE
	//  Reads all the stats from the backend for specified local gamer,
	//  compares them with current local values, and calls back to title code
	//  when they differ.  The title should then either call SetDirty() to
	//  note that the local value needs to be written to the backend, or
	//  simply store the value if it is server-authoritative.
	bool  Synchronize(const bool force = false, const bool multiplayer = false);
	bool  CanSynchronize(const bool force = false);

	//PURPOSE
	//  Return TRUE when profile stats have synched.
	bool        Synchronized( const eSyncType type) const;
	void     SetSynchronized(const eSyncType type NOTFINAL_ONLY(, const bool force = false));
	void   ClearSynchronized(const eSyncType type);

	bool       GetMPSynchIsDone() const {return m_MPSynchIsDone;}
	void     ClearMPSynchIsDone()       {m_MPSynchIsDone = false;}

	bool  SynchronizedFailed(const eSyncType type, int& errorCode) const;
	bool  SynchronizePending(const eSyncType type) const;

	//PURPOSE
	//  Submits the current value of dirty stats to the backend.  Because this is
	//  an expensive operation, submissions are throttled at a rate set by the 
	//  backend at runtime.  This means that if Flush is called too soon after the 
	//  previous call, nothing will be sent.  Also, only so many stats may be 
	//  submitted per flush, so it is not guaranteed that every currently dirty stat 
	//  will have been flushed.
	void  Flush(const bool forceFlush, const bool flushOnlineStats);
	bool  FlushFailed() const {return m_FlushFailedMP;}

	//PURPOSE
	//  Reset a group of stats.
	bool ResetGroup(const u32 group);
	bool IsResetGroupSet(const u32 group) const {return m_ResetGroups.IsSet(group);}

	void  HandleEvent(const class rlProfileStatsEvent*);

	bool                    PendingFlush() const {return m_FlushInProgress;}
	bool              PendingFlushStatus() const {return m_FlushStatus.Pending();}
	bool  PendingMultiplayerFlushRequest() const {return (m_FlushOnlineStats && m_ForceFlush);}

	void ForceMPProfileStatsGetFromCloud();
	inline bool ForcedToOverwriteAllLocalMPStats() const { return m_ForceCloudOverwriteOnMPLocalStats; }

private:
	
	// If no groups left to reset, consider flushing
	void  CheckAndTryFlushing();

	//Status processing functions
	void  ProcessSynchOpStatusNone();
	void  ProcessSynchOpStatusSucceeded();

	void  ProcessStatusSucceeded();
	void  ProcessStatusFailed();

	GEN9_STANDALONE_ONLY(void  ProcessSaveMigrationStatus();)

public:

	//PURPOSE
    //  An implementation of a cache for tracking a limited number
    //  of dirty stats, with an eviction policy based on priority.
    //  and prioritize them by their flush priority. Works by
    //  associating a bin with each flush priority, and building
    //  a linked list of stats within each bin. When a stat
    //  needs to be evicted, we simply search all bins with a
    //  lower priority for a stat to evict.
    class DirtyCache
    {
    private:
        //PURPOSE
        //  Structure that represents a DirtyStat managed
        //  by our DirtyCache. Simply stores the
        //  rlProfileDirtyStat, along with a pointer to the
        //  next stat in our bin in order to
        //  construct a linked list of stats in the same bin
        //  (same priority)
        struct DirtyStat
        {
            rlProfileDirtyStat m_Stat;
            DirtyStat *m_NextInBin;

            DirtyStat()
            : m_Stat()
            , m_NextInBin(NULL)
            {}

            DirtyStat(const rlProfileDirtyStat &stat)
            : m_Stat(stat)
            , m_NextInBin(NULL)
            {}
        };

        //PURPOSE
        //  Structure that represents a bin of DirtyStats
        //  Basically just points to the first stat in this
        //  bin's linked list of stats
        struct DirtyStatBin
        {
            DirtyStat *m_FirstInBin;

            DirtyStatBin()
            : m_FirstInBin(NULL)
            {}

            // Adds the specified stat to this bin
            void Link(DirtyStat *stat);

            // Removes the specified stat from this bin
            // Note: Because this is a singly linked list, currently
            // we only support removing the first item in the bin
            void Unlink(DirtyStat *stat);
        };

    public:
        //PURPOSE
        //  An implementation of the rlProfileStatsDirtyIterator interface
        //  Iterates over stats by priority using our bins
        class FlushIterator : public rlProfileStatsDirtyIterator
        {
        public:
            FlushIterator(const DirtyCache &dirtyCache);

            virtual bool GetCurrentStat(rlProfileDirtyStat &stat) const;
            virtual void Next();
            virtual bool AtEnd() const;

        private:
            const DirtyCache& m_DirtyCache;
            int m_CurBinIndex;
            // Pointer to our current stat, or NULL if we're at the end
            DirtyStat* m_CurStat;
        };

    public:
        DirtyCache()
        : m_Count(0)
        {}

        //PURPOSE
        //  Adds a dirty stat, using the stat's priority directly
        bool AddDirtyStat(const rlProfileDirtyStat& stat);

        //PURPOSE
        //  Adds a dirty stat, using the specified bin as it's priority
        bool AddDirtyStat(const rlProfileDirtyStat& stat, DirtyStatBin *bin);

        u32 GetCount() const { return m_Count; }

        //PURPOSE
        //  Returns the min priority bin
        DirtyStatBin* GetMinPriorityBin();

        //PURPOSE
        //  Returns the max priority bin
        DirtyStatBin* GetMaxPriorityBin();

    private:
        // Returns the bin for the specified stat
        DirtyStatBin *GetBin(const rlProfileDirtyStat &stat);

    private:
        // The number of stats allocated from our m_DirtyStats array currently
        u32 m_Count;
        DirtyStat m_DirtyStats[MAX_DIRTY];
        // Right now we just have a 1-1 mapping between bins/priorities because there aren't
        // very many, plus a min/max priority bin, mainly used to ensure that version stats
        // are always written last/first as needed
        // Bins are ordered from lowest to highest priority
        static const u32 BIN_COUNT = (RL_PROFILESTATS_MAX_PRIORITY - RL_PROFILESTATS_MIN_PRIORITY + 1) + 2;
        DirtyStatBin m_DirtyStatBins[BIN_COUNT];
    };
};

#endif // _PROFILESTATSREADER_H

// EOF

