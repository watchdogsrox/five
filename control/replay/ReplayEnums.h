#ifndef REPLAYENUMS_H
#define REPLAYENUMS_H

#if GTA_REPLAY

#include "ReplayPacketIDs.h"

//todo4five move to an nicer location
typedef u16 tPacketID;

//todo4five move to a nicer location 2
class CRecordType
{
public:
	CRecordType() : m_pPtr(NULL), m_iInstID(-1) {}

	void Reset()
	{
		m_pPtr = NULL;
		m_iInstID = -1;
	}

	void  SetInstID(s16 iInstID)	{ m_iInstID = iInstID; }
	s16	  GetInstID() const			{ return m_iInstID; }

	bool  IsEmpty() const			{ return (m_pPtr == NULL); }

protected:
	void*	m_pPtr;
	s16		m_iInstID;
};



//--------------------------------------------------------------------------------------------------------------------
class CPfxInstTracker : public CRecordType
{
public:
	CPfxInstTracker() : CRecordType() {}

	void  Reset() { CRecordType::Reset(); m_uHash = 0; m_sReplayID = 0; m_bIsUpdated = false; m_bIsSystem = 0;}

	void  SetPfxInst(void* pInst)	{ m_pPtr = pInst; }
	void* GetPfxInst() const		{ return m_pPtr; }

	u32   GetPfxHash() const		{ return m_uHash; }
	void  SetPfxHash(u32 uHash)		{ m_uHash = uHash; }

	s32	  GetReplayID() const		{ return m_sReplayID; }
	void  SetReplayID(s32 sReplayID){ m_sReplayID = sReplayID; }

	bool  IsSystem() const			{ return m_bIsSystem; }
	void  SetIsSystem(bool bIsSystem) { m_bIsSystem = bIsSystem; }

	bool  IsUpdated() const			{ return m_bIsUpdated; }
	void  SetIsUpdated(bool bUpdated) { m_bIsUpdated = bUpdated; }

protected:
	u32		m_uHash;
	s32		m_sReplayID;
	bool	m_bIsUpdated;
	bool	m_bIsSystem;
};

enum eReplayLoadingState
{
	REPLAYLOADING_NONE,
	REPLAYLOADING_CLIP,
	REPLAYLOADING_PRECACHING,
};

enum eReplayClipTraversalStatus
{
	UNHANDLED_PACKET,
	END_CLIP,
};

enum eReplayBlockStatus
{
	REPLAYBLOCKSTATUS_EMPTY,
	REPLAYBLOCKSTATUS_FULL,
	REPLAYBLOCKSTATUS_BEINGFILLED,
	REPLAYBLOCKSTATUS_SAVING,
};


enum eReplayBlockLock
{
	REPLAYBLOCKLOCKED_NONE,
	REPLAYBLOCKLOCKED_READ,
	REPLAYBLOCKLOCKED_WRITE
};


enum ePlayPktResult
{
	PACKET_UNHANDLED,
	PACKET_PLAYED,
	PACKET_ENDBLOCK
};



enum eReplayMode
{
	REPLAYMODE_DISABLED,
	REPLAYMODE_RECORD,
	REPLAYMODE_EDIT,
	REPLAYMODE_LOADCLIP,
	REPLAYMODE_WAITINGFORSAVE,
};

enum eReplayIDType
{
	REPLAYIDTYPE_RECORD,
	REPLAYIDTYPE_PLAYBACK
};

// Replay state. Note, order is important.
enum eReplayState
{
	REPLAY_STATE_PLAY						= 1 << 0,
	REPLAY_STATE_FAST						= 1 << 1,
	REPLAY_STATE_SLOW						= 1 << 2,
	REPLAY_STATE_PAUSE						= 1 << 3,
	REPLAY_STATE_CLIP_TRANSITION_REQUEST	= 1 << 5,
	REPLAY_STATE_CLIP_TRANSITION_LOAD		= 1 << 6,
	REPLAY_STATE_MASK				= REPLAY_STATE_PLAY | REPLAY_STATE_FAST | REPLAY_STATE_SLOW | REPLAY_STATE_PAUSE | REPLAY_STATE_CLIP_TRANSITION_REQUEST | REPLAY_STATE_CLIP_TRANSITION_LOAD
};

// These should carry on from the previous enum
enum eReplayCursorState
{
	REPLAY_CURSOR_NORMAL			= 1 << 8,
	REPLAY_CURSOR_SPEED				= 1 << 9,		// Equates to m_cursorSpeed (ffwd, rwd)
	REPLAY_CURSOR_JUMP				= 1 << 10,		// Equates to m_TimeToJumpToMS which will go to m_jumpDelta
	REPLAY_CURSOR_JUMP_LINEAR		= 1 << 11,		// Equates to small incremental jumps in one direction
	REPLAY_CURSOR_MASK				= REPLAY_CURSOR_NORMAL | REPLAY_CURSOR_SPEED | REPLAY_CURSOR_JUMP | REPLAY_CURSOR_JUMP_LINEAR
};

// These should carry on from the previous enum
enum eReplayDirection
{
	REPLAY_DIRECTION_FWD			= 1 << 16,
	REPLAY_DIRECTION_BACK			= 1 << 17,
	REPLAY_DIRECTION_MASK			= REPLAY_DIRECTION_FWD | REPLAY_DIRECTION_BACK
};

// These should carry on from the previous enum
enum eReplayPacketFlags
{
	REPLAY_PLAYBACK_FLAGS			= REPLAY_STATE_MASK | REPLAY_CURSOR_MASK | REPLAY_DIRECTION_MASK,
	REPLAY_PRELOAD_PACKET_FWD		= 1 << 20,
	REPLAY_PRELOAD_PACKET_BACK		= 1 << 21,
	REPLAY_PRELOAD_PACKET			= REPLAY_PRELOAD_PACKET_FWD | REPLAY_PRELOAD_PACKET_BACK,	// Scan ahead during playback to load assets
	REPLAY_PREPLAY_PACKET			= 1 << 22,
	REPLAY_MONITOR_PACKET_LOW_PRIO	= 1 << 23,		// Stores event at the beginning of every block
	REPLAY_MONITOR_PACKET_HIGH_PRIO = 1 << 24,		// Low priority packets will get turfed out of the buffer if it gets near full
	REPLAY_MONITOR_PACKET_FRAME		= 1 << 25,		// Stores event at the beginning of every frame instead of block
	REPLAY_MONITOR_PACKET_JUST_ONE	= 1 << 26,		// Allows only one of a particular type of packet in the monitor buffer.
	REPLAY_MONITOR_PACKET_ONE_PER_EFFECT = 1 << 27,
	// This is an amalgamation of thee monitor flags and should not be used on it's own to register a packet....(cant be both high and low priority)
	REPLAY_MONITOR_PACKET_FLAGS		= REPLAY_MONITOR_PACKET_LOW_PRIO | REPLAY_MONITOR_PACKET_HIGH_PRIO | REPLAY_MONITOR_PACKET_FRAME | REPLAY_MONITOR_PACKET_JUST_ONE,
	REPLAY_EVERYFRAME_PACKET		= 1 << 28,		// Ignore setting the packet to 'played' so it will play every frame
	REPLAY_MONITORED_PLAY_BACKWARDS	= 1 << 29,
	REPLAY_MONITORED_UPDATABLE		= 1 << 30,		// Allow monitored packets to call UpdateMonitorPacket function to allow it to be modified.
};

// These should carry on from the previous enum (Ignore, these aren't actually used)
enum eReplaySRLState
{
	REPLAY_RECORD_SRL				= 1 << 29,
	REPLAY_PLAYBACK_SRL				= 1 << 30,	
	REPLAY_SRL_MASK					= REPLAY_RECORD_SRL | REPLAY_PLAYBACK_SRL
};


enum eReplayUIFlags
{
	REPLAY_UI_PLAY_1_FRAME					= (1<<0),
	REPLAY_UI_PLAY_1_FRAME_PRECACHE			= (1<<1),
	REPLAY_UI_MUTE							= (1<<2)
};

enum eDelaySaveState
{
	REPLAY_DELAYEDSAVE_DISABLED,
	REPLAY_DELAYEDSAVE_ENABLED,
	REPLAY_DELAYEDSAVE_REQUESTED,	
	REPLAY_DELAYEDSAVE_PROCESS,
	REPLAY_DELAYEDSAVE_PROCESS_AFTER_MSG				// not used if USE_THREAD_SAVE is enabled
};

enum eEventStatus
{
	PFX_NEW = 0,
	PFX_PLAYED
};

enum eReplayParticleType
{
	PFX_PEDPROCESSFX = 0,

	PFX_VEHPROCESSFX,
	PFX_VEHPRERENDERFX,
	PFX_VEHPRERENDER2FX,

	PFX_FIREUPDATEFX,	// + water cannon (extinguish), cfireman::update
	PFX_FIREUPDATEAFTERPRERENDERFX, // pfx::update(extinguish)
	PFX_FIRECUSTOMSHADERFX,			// CCustomShaderEffectPedBoneDamageFX - not impl. for PC, PSN only?

	PFX_WEAPONPROCESSFX,
	PFX_WEAPONPRERENDERFX,
	PFX_WEAPONUPDATEFX,

	PFX_ENTITYPROCESSFX,
	PFX_ENTITYPRERENDERFX,
	PFX_ENTITYUPDATEFX,

	PFX_BLOODPROCESSFX,
	PFX_BLOODPRERENDERFX,
	PFX_BLOODUPDATEFX,

	PFX_MARKPRERENDERFX,

	PFX_MATERIALBANGPROCESSFX,

	PFX_MISCGLASSPROCESSFX,

	PFX_WATERPROCESSFX,

	PFX_RIPPLEPROCESSFX,

	PFX_SCRIPTEDFX,

	PFX_MAX_NUM
};

enum eReplayPacketType
{
	HISTORY_DAMAGE = PFX_MAX_NUM+1,

	HISTORY_MAX_NUM
};

#if __BANK
enum eReplayBankFlags
{
	REPLAY_BANK_FRAMES_ELAPSED				= (1<<0),
	REPLAY_BANK_SECONDS_ELAPSED				= (1<<1),

	REPLAY_BANK_PEDS_BEFORE_REPLAY			= (1<<2),
	REPLAY_BANK_VEHS_BEFORE_REPLAY			= (1<<3),
	REPLAY_BANK_OBJS_BEFORE_REPLAY			= (1<<4),
	REPLAY_BANK_NODES_BEFORE_REPLAY			= (1<<5),

	REPLAY_BANK_PEDS_TO_DEL					= (1<<6),
	REPLAY_BANK_VEHS_TO_DEL					= (1<<7),
	REPLAY_BANK_OBJS_TO_DEL					= (1<<8),

	REPLAY_BANK_PEDS_DURING_REPLAY			= (1<<9),
	REPLAY_BANK_VEHS_DURING_REPLAY			= (1<<10),
	REPLAY_BANK_OBJS_DURING_REPLAY			= (1<<11),
	REPLAY_BANK_NODES_DURING_REPLAY			= (1<<12),

	REPLAY_BANK_DISABLE_PEDS				= (1<<13),
	REPLAY_BANK_DISABLE_VEHS				= (1<<14),
	REPLAY_BANK_DISABLE_OBJS				= (1<<15),

	REPLAY_BANK_PFX_EFFECT_LIST				= (1<<16)
};
#endif

enum eReplayPacketOutputMode
{
	REPLAY_OUTPUT_DISPLAY,
	REPLAY_OUTPUT_XML,
};

enum ePreloadResult
{
	PRELOAD_SUCCESSFUL = 0,
	PRELOAD_FAIL = 1 << 0,
	PRELOAD_FAIL_MISSING_ENTITY = 1 << 1, 
};

enum ePreplayResult
{
	PREPLAY_SUCCESSFUL = 0,
	PREPLAY_WAIT_FOR_TIME,
	PREPLAY_TOO_LATE,
	PREPLAY_FAIL,
	PREPLAY_MISSING_ENTITY,
};

enum eShouldExtract
{
	REPLAY_EXTRACT_SUCCESS		= 0,
	REPLAY_EXTRACT_FAIL			= 1 << 0,
	REPLAY_EXTRACT_FAIL_RETRY	= 1 << 1,	// Failed because we're waiting on something...retry next frame
	REPLAY_EXTRACT_FAIL_PAUSED	= 1 << 2,	// Failed because the timer is paused...will retry when unpaused
};


#endif // GTA_REPLAY

#endif // REPLAYENUMS_H
