//
// StatsSavesMgr.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STATSSAVESMGR_H
#define STATSSAVESMGR_H

//Rage headers
#include "atl/array.h"
#include "net/status.h"

//Stats headers
#include "Stats/StatsTypes.h"

//Game headers
#include "SaveLoad/savegame_defines.h"

#if __BANK
namespace rage { class bkBank; };
#endif

class CStatsSaveHistory
{

public:
	struct SaveRecord
	{
	public:
		u32  m_lastSaveTime;
		bool m_canSaveDuringJob;

	public:
		SaveRecord() : m_lastSaveTime(0), m_canSaveDuringJob(false) {;}
	};
	atFixedArray < SaveRecord, STAT_SAVETYPE_MAX_NUMBER > m_records;

public:
	CStatsSaveHistory() 
	{
		m_records.clear();
		m_records.Reset();
		m_records.Resize( STAT_SAVETYPE_MAX_NUMBER );
		ResetAfterJob();
	}
	~CStatsSaveHistory() {m_records.clear();}

	void ResetAfterJob( )
	{
		for(int save=0; save<STAT_SAVETYPE_MAX_NUMBER; save++)
		{
			if (save != STAT_SAVETYPE_END_MATCH && save != STAT_SAVETYPE_END_SESSION && save != STAT_SAVETYPE_END_MISSION && save != STAT_SAVETYPE_PROLOGUE)
				m_records[save].m_canSaveDuringJob= false;
		}
	}

	void  SetCanSaveDuringJob(const eSaveTypes savetype)
	{
		if (savetype == STAT_SAVETYPE_INVALID || savetype == STAT_SAVETYPE_MAX_NUMBER)
			return;

		m_records[savetype].m_canSaveDuringJob = true;
	}

	bool  GetCanSaveDuringJob(const eSaveTypes savetype) const
	{
		return (u32)savetype < STAT_SAVETYPE_MAX_NUMBER && m_records[savetype].m_canSaveDuringJob;
	}

	void               SaveRequested(const eSaveTypes savetype);
	bool                     CanSave(const int file, const eSaveTypes savetype) const;
	bool   DeferredProfileStatsFlush(const eSaveTypes savetype) const;
	bool           DeferredCloudSave(const eSaveTypes savetype) const;
	bool       ProfileStatsFlushOnly(const eSaveTypes savetype) const;
	bool               CloudSaveOnly(const eSaveTypes savetype) const;
	bool    RequestProfileStatsFlush(const eSaveTypes savetype) const;
	bool            RequestCloudSave(const eSaveTypes savetype) const;
};

struct CSaveTelemetryStats
{
public:
	CSaveTelemetryStats() { Reset(); }

	const CSaveTelemetryStats& operator=(const CSaveTelemetryStats& other) { Reset(); m_Type = other.m_Type; m_Reason = other.m_Reason; return *this; }

	void Reset() { SetFrom(STAT_SAVETYPE_INVALID, 0); }
	void FlushTelemetry(const bool requestFailed, const bool isCloudRequest);
	void SetFrom(const eSaveTypes type, const int reason) { m_Type = type; m_Reason = reason; }

public:

	//Current save type set by script.
	eSaveTypes m_Type;

	//Current save reason set by script.
	int m_Reason;
};

//PURPOSE
//  Manage all stats saves to persistent storage.
class CStatsSavesMgr
{
public:
	static const u32 TOTAL_NUMBER_OF_FILES = MAX_NUM_MP_CLOUD_FILES;

private:

	//Saving....
	struct eSaveOperations
	{
	public:
		static const u32 DELAY_NEXT_SAVE        = 4*1000;  //Delay timer for save operations.
		static const u32 DELAY_FAILED_SAVE      = 30*1000; //Delay timer for save operations.
		static const u32 MAX_NUMBER_RETRY_SAVE  = 3;       //Max Number of saves to retry.
		static const u32 MIN_PROFILESTATS_FLUSH = 30*1000;    //Minimun interval to allow profile stats flushes.
		static const int MIN_CLOUD_SAVE_INTERVAL = 5*60*1000; //Minimun interval to allow cloud savegames.

	public:
		u32   m_delaySaveFailed;
		u32   m_NextSaveTime;                          //When to start next save operation.
		u32   m_CurrentSlot;                           //Always starts at save 0 then makes the save of the current character.
		u32   m_RequestSlot;                           //Pending save requests.
		u32   m_OverrideSlot;                          //When the Override Slot Number is set instead of saving/loading the current character we save/load this value.
		bool  m_InProgress;                            //A save operation is in progress.
		bool  m_Blocked;                               //TRUE is blocked.
		u32   m_RetryFailedSave;                       //When a savegame FAILS we retry saving up to MAX_NUMBER_RETRY_SAVE.
		u32   m_IsCritical;                            //Type Of Save is critical.
		bool  m_CurrentSlotIsCritical;                 //Save the critical state of the current save operation
		u32   m_FailedSlot;                            //Failed Cloud Save files.
		int   m_FailedSlotCode[TOTAL_NUMBER_OF_FILES]; //Saved failure codes.
		u32   m_deferredProfileStatsFlush;             //Trigger a profile stats flush in x milliseconds.
		u32   m_deferredCloudSave;                     //Trigger a profile stats flush in x milliseconds.
		bool  m_flushProfileStat;                      //TRUE if during a save we should flush profile stats
		bool  m_flushProfileStatOnNextCloudSave;       //TRUE if on the next cloud save request we also need to flush the profile stats
		u64   m_CurrentSlotTimestamp;                  //Current save slot timestamp

		CSaveTelemetryStats m_InProgressSaveTelemetry;
		CSaveTelemetryStats m_RequestedSaveTelemetry;

#if __ASSERT
        bool  m_lastFlushProfileStatWasDeferred;       //TRUE if last profile stat flush was a deferred flush
#endif // __ASSERT
		u32   m_lastFlushProfileStatTime;              //Last profile stats flush done.
		u32   m_lastcloudSavegame;                     //Last cloud savegame.
		u32   m_minForceFlushTime;                     //Last profile stats flush done.
#if !__FINAL
		//Used when -nompsavesometimes is present.
		//Represents the percentage of time we should simulate failures.
		int m_SimulateFailPct;
#endif

	public:
		eSaveOperations() 
			: m_delaySaveFailed(DELAY_NEXT_SAVE)
		    , m_NextSaveTime(0)
			, m_CurrentSlot(STAT_MP_CATEGORY_DEFAULT)
			, m_OverrideSlot(STAT_MP_CATEGORY_DEFAULT)
			, m_RequestSlot(0)
			, m_InProgress(false)
			, m_Blocked(false)
			, m_RetryFailedSave(0)
			, m_IsCritical(0)
			, m_CurrentSlotIsCritical(false)
			, m_FailedSlot(0)
			, m_deferredProfileStatsFlush(0)
			, m_deferredCloudSave(0)
			, m_flushProfileStat(false)
			, m_flushProfileStatOnNextCloudSave(false)
            ASSERT_ONLY(, m_lastFlushProfileStatWasDeferred(false))
			, m_lastFlushProfileStatTime(0)
			, m_minForceFlushTime(MIN_PROFILESTATS_FLUSH)
			, m_lastcloudSavegame(0)
			, m_CurrentSlotTimestamp(0)
#if !__FINAL
			, m_SimulateFailPct(0)
#endif
		{
			Reset();
		}

		void Reset()
		{
			m_Blocked      = false;
			m_InProgress   = false;
			m_CurrentSlot  = STAT_MP_CATEGORY_DEFAULT;
			m_OverrideSlot = STAT_MP_CATEGORY_DEFAULT;
			m_RequestSlot  = 0;
			m_NextSaveTime = 0;
			m_RetryFailedSave = 0;
			m_IsCritical   = 0;
			m_CurrentSlotIsCritical = false;
			ClearFailed( -1 );
			m_InProgressSaveTelemetry.Reset();
			m_RequestedSaveTelemetry.Reset();
		}

		bool    IsCritical(const u32 file) const {return ((m_IsCritical & (1<<file)) != 0);}
		bool   HasRequests(const u32 file) const {return ((m_RequestSlot & (1<<file)) != 0);}
		void  ClearRequest(const u32 file) {m_RequestSlot &= (~(1<<file)); m_IsCritical &= (~(1<<file));}

		void  ClearFailed(const int file)
		{
			if (file < 0) //-1 - Default clear all
			{
				m_FailedSlot = 0;
				for(int i=0; i<TOTAL_NUMBER_OF_FILES; i++)
					m_FailedSlotCode[i] = 0;
			}
			else
			{
				Assert(file<TOTAL_NUMBER_OF_FILES);
				if( file<TOTAL_NUMBER_OF_FILES )
				{
					m_FailedSlot &= (~(1<<file));
					m_FailedSlotCode[file] = 0;
				}
			}
		}

		void  SetFailed(const int file, const int failurecode) 
		{
			Assert( 0<=file && file<TOTAL_NUMBER_OF_FILES);
			if (0<=file && file<TOTAL_NUMBER_OF_FILES)
			{
				m_FailedSlot |= (1<<file);
				m_FailedSlotCode[file] = failurecode;
			}
		}

		int  GetFailedCode( const int file ) const 
		{
			Assert( 0<=file && file<TOTAL_NUMBER_OF_FILES);
			if (0<=file && file<TOTAL_NUMBER_OF_FILES)
				return m_FailedSlotCode[file];

			return 0;
		}

		void Shutdown()
		{
			m_delaySaveFailed = DELAY_NEXT_SAVE;
			m_NextSaveTime = 0;
			m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;
			m_OverrideSlot = STAT_MP_CATEGORY_DEFAULT;
			m_RequestSlot = 0;
			m_InProgress = false;
			m_Blocked = false;
			m_RetryFailedSave = 0;
			m_IsCritical = 0;
			m_CurrentSlotIsCritical = false;
			m_FailedSlot = 0;
			m_deferredProfileStatsFlush = 0;
			m_deferredCloudSave = 0;
			m_flushProfileStat = false;
			m_flushProfileStatOnNextCloudSave = false;
			ASSERT_ONLY( m_lastFlushProfileStatWasDeferred = false; )
			m_lastFlushProfileStatTime = 0;
			m_minForceFlushTime = MIN_PROFILESTATS_FLUSH;
			m_lastcloudSavegame = 0;
			m_CurrentSlotTimestamp = 0;
			NOTFINAL_ONLY( m_SimulateFailPct = 0; )

			Reset();
		}

	} m_save;

	//Loading....
	struct eLoadOperations
	{
	public:
		u32   m_PendingSlot;  //Pending Cloud load files.
		u32   m_CurrentSlot;  //Always starts at save 0 then loads the remaining characters.
		u32   m_OverrideSlot; //When the Override Slot Number is set instead of saving/loading the current character we save/load this value.
		u32   m_FailedSlot; //Failed Cloud load files.
		int   m_FailedSlotCode[TOTAL_NUMBER_OF_FILES]; //Saved failure codes.
		bool  m_InProgress;   //True if Loading from cloud storage is in progress.
#if !__FINAL
		//Used when -nomploadsometimes is present.
		//Represents the percentage of time we should simulate failures.
		int m_SimulateFailPct;
#endif

	public:
		eLoadOperations() 
			: m_CurrentSlot(STAT_MP_CATEGORY_DEFAULT)
			, m_OverrideSlot(STAT_MP_CATEGORY_DEFAULT)
			, m_PendingSlot(0)
			, m_FailedSlot(0)
			, m_InProgress(false)
#if !__FINAL
			, m_SimulateFailPct(0)
#endif
		{
			Reset();
		}

		void Shutdown()
		{
			Reset();
		}

		void Reset()
		{
			m_InProgress   = false;
			m_CurrentSlot  = STAT_MP_CATEGORY_DEFAULT;
			m_OverrideSlot = STAT_MP_CATEGORY_DEFAULT;
			m_PendingSlot  = (1<<TOTAL_NUMBER_OF_FILES)-1;
			ClearFailed(-1);
		}

		void  ClearFailed(const int file)
		{
			if (file < 0) //-1 - Default clear all
			{
				m_FailedSlot = 0;
				for(int i=0; i<TOTAL_NUMBER_OF_FILES; i++)
				{
					m_FailedSlotCode[i] = LOAD_FAILED_REASON_NONE;
			}
			}
			else
			{
				Assert(file<TOTAL_NUMBER_OF_FILES);
				if( file<TOTAL_NUMBER_OF_FILES )
				{
					m_FailedSlot &= (~(1<<file));
					m_FailedSlotCode[file] = LOAD_FAILED_REASON_NONE;
				}
			}
		}

		void  SetFailed(const int file, const int failurecode) 
		{
			Assert( 0<=file && file<TOTAL_NUMBER_OF_FILES);
			if (0<=file && file<TOTAL_NUMBER_OF_FILES)
			{
				Assert(failurecode >= LOAD_FAILED_REASON_NONE);
				Assert(failurecode < LOAD_FAILED_REASON_MAX);

				m_FailedSlot |= (1<<file);
				m_FailedSlotCode[file] = failurecode;
			}
		}

		int  GetFailedCode( const int file ) const 
		{
			Assert( 0<=file && file<TOTAL_NUMBER_OF_FILES);

			if (0<=file && file<TOTAL_NUMBER_OF_FILES)
			{
				return m_FailedSlotCode[file];
			}

			return LOAD_FAILED_REASON_NONE;
		}

	} m_load;

	//Keep a history of each savetype requested.
	CStatsSaveHistory  m_saveHistory;

	//True if the manager has been initialized.
	bool  m_Initialized : 1; 

	//True if the synch with the profile stats have not yet been done.
	bool  m_SynchProfileStats : 1;
	u64   m_LoadUid;

	//True if we detect dirty reads.
	bool  m_DirtyProfileStatsReadDetected : 1;
	bool  m_DirtyCloudReadDetected : 1;
	u64   m_DirtyReadServerTime;
	int   m_DirtyReadCloudFile;

	bool m_NoRetryOnFail;

public:
	 CStatsSavesMgr( ) 
		 : m_Initialized(false)
		 , m_SynchProfileStats(false)
		 , m_DirtyProfileStatsReadDetected(false)
		 , m_DirtyCloudReadDetected(false)
		 , m_DirtyReadServerTime(0)
		 , m_DirtyReadCloudFile(-1)
		 , m_LoadUid(0)
		 , m_NoRetryOnFail(false)
	 {;}

	~CStatsSavesMgr( ) {;}

	void            Init(const unsigned initMode);
	void        Shutdown(const unsigned shutdownMode);
	void          Update();
	void   SignedOffline();

	BANK_ONLY( void Bank_InitWidgets( bkBank& bank ); )
	BANK_ONLY( void Bank_SetFailSlot(); )
	BANK_ONLY( void Bank_ClearFailSlot(); )

	//Open save category to be able to be used during a job activity.
	void      SetCanSaveDuringJob(const eSaveTypes savetype){m_saveHistory.SetCanSaveDuringJob(savetype);}
	bool      GetCanSaveDuringJob(const eSaveTypes savetype) const { return m_saveHistory.GetCanSaveDuringJob(savetype); }

	void      FlushProfileStats();

	//Load
	bool                     Load(const eStatsLoadType type, const int file);
	bool         IsLoadInProgress(const int file) const;
	int         GetLoadFailedCode(const int file) const {return m_load.GetFailedCode(file);}
	bool          CloudLoadFailed(const int file) const {return ((m_load.m_FailedSlot & (1<<file)) != 0);}
	bool            IsLoadPending(const int file) const {return ((m_load.m_PendingSlot & (1<<file)) != 0);}
	void         ClearLoadPending(const int file);
	void           SetLoadPending(const u32 file) {m_load.m_PendingSlot |= (1<<file);}
	void          ClearLoadFailed(const int file) {m_load.ClearFailed(file);}
	void            SetLoadFailed(const int file, const int failurecode) {m_load.SetFailed(file, failurecode);}

	//Block saves
	void		SetBlockSaveRequests(const bool blocksave) {m_save.m_Blocked=blocksave;}
	inline bool GetBlockSaveRequests( ) const {return m_save.m_Blocked;}

	//Save
	bool                      Save(const eStatsSaveType type, const u32 file = STAT_MP_CATEGORY_DEFAULT, const eSaveTypes savetype = STAT_SAVETYPE_DEFAULT, const u32 uSaveDelay = eSaveOperations::DELAY_NEXT_SAVE, const int saveReason = 0);
	bool          IsSaveInProgress( ) const;
	bool       PendingSaveRequests( ) const;
	void  ClearPendingSaveRequests(const int file) {m_save.ClearRequest(file);}
	int          GetSaveFailedCode(const int file) const {return m_save.GetFailedCode(file);}
	bool           CloudSaveFailed(const int file) const {return ((m_save.m_FailedSlot & (1<<file)) != 0);}
	void           ClearSaveFailed(const int file) {m_save.ClearFailed(file);}
	void             SetSaveFailed(const int file, const int failurecode) {m_save.SetFailed(file, failurecode);}
	bool      CanFlushProfileStats( ) const;
	void ClearPendingFlushRequests( bool clearOnly = false ) {m_save.m_deferredProfileStatsFlush=0; if(!clearOnly)m_save.m_flushProfileStatOnNextCloudSave=true;}

	//Dirty reads
	void                  ClearDirtyRead( ) {m_DirtyProfileStatsReadDetected=false;m_DirtyReadServerTime=0;m_DirtyCloudReadDetected=false;}
	bool   DirtyProfileStatsReadDetected( ) const {return m_DirtyProfileStatsReadDetected;}
	bool          DirtyCloudReadDetected( ) const {return m_DirtyCloudReadDetected;}
	u64           GetDirtyReadServerTime( ) const {return m_DirtyReadServerTime;}
	int            GetDirtyCloudReadFile( ) const {return m_DirtyReadCloudFile;}

	void				SetNoRetryOnFail(bool value);

	void      CancelAndClearSaveRequests( );

	void   ClearLocalDirtyReadProfileSettings( );

#if __ASSERT
	u32   GetOverrideSaveSlotNumber() const {return m_save.m_OverrideSlot;}
#endif // __ASSERT

private:
	void          Initialize();
	void  UpdateCloudStorage();

	bool           QueueSave(const eStatsSaveType type, const u32 file = STAT_MP_CATEGORY_DEFAULT);
	bool           QueueLoad(const eStatsLoadType type);

	bool  QueueCheckCloudFileLoadPending();
	void       CheckCloudFileLoadPending();

	void   SetupSavegameSlot(const bool save, const int slot = STAT_INVALID_SLOT);
	bool  GetCharacterActive(const u32 character);

	bool      IsPendingSaveRequests(const u32 file) const;
	int   GetNextPendingSaveRequest() const;

	bool   RetrySaveOnHttpErrorCode(const int resultcode) const;

	void    CheckFile(const int ASSERT_ONLY(file)) const {Assert(file>=STAT_MP_CATEGORY_DEFAULT && file<=STAT_MP_CATEGORY_MAX);}

	void  TriggerProfileStatsFlush(CSaveTelemetryStats saveTelemetry);

	bool  CheckForPendingProfileStatsFlush( );

	bool  CheckForProfileStatsDirtyRead( );
	bool  CheckForCloudDirtyRead( const u32 file = STAT_MP_CATEGORY_DEFAULT );
	void  SaveLocalCloudDirtyReadTimestamp();

public:
	u64   GetLocalCloudDirtyReadValue( const u32 file = STAT_MP_CATEGORY_DEFAULT );
};

#endif // STATSSAVESMGR_H

// EOF
