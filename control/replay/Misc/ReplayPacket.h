#ifndef REPLAY_PACKET_H
#define REPLAY_PACKET_H

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "Control/Replay/ReplayEnums.h"
#include "control/replay/replay_channel.h"
#include "control/replay/PacketBasics.h"
#include "control/replay/ReplaySupportClasses.h"
#include "timecycle/TimeCycle.h"
#include "vfx/decal/decalmanager.h"
#include "Vfx/Decals/DecalManager.h"

#define FLOAT_1_0_TO_U8	(255.0f)
#define U8_TO_FLOAT_0_1	(1.0f / 255.0f)
#define SPEEDMULT (32767.0f / 120.0f)	// Max speed = 120 m/s = 432km/h

// Packet Group Flags
#define	REPLAY_PACKET_GROUP_NONE			( 0 )
#define	REPLAY_PACKET_GROUP_GAME			( 1 << 0 )
#define	REPLAY_PACKET_GROUP_VEHICLE			( 1 << 1 )
#define	REPLAY_PACKET_GROUP_BUILDING		( 1 << 2 )
#define	REPLAY_PACKET_GROUP_PED				( 1 << 3 )
#define	REPLAY_PACKET_GROUP_OBJECT			( 1 << 4 )
#define REPLAY_PACKET_GROUP_PICKUP			( 1 << 5 )
#define	REPLAY_PACKET_GROUP_FRAG			( 1 << 6 )
#define	REPLAY_PACKET_GROUP_PARTICLE		( 1 << 7 )
#define	REPLAY_PACKET_GROUP_PROJ_TEX		( 1 << 8 )
#define	REPLAY_PACKET_GROUP_SOUND			( 1 << 9 )
#define	REPLAY_PACKET_GROUP_SMASH			( 1 << 10 )
#define	REPLAY_PACKET_GROUP_SCRIPT			( 1 << 11 )

#define REPLAY_PACKET_GROUP_ALL				( 0xFFFFFFFF )


#define REPLAY_GUARD_BASE					0xAAAAAAAA
#define REPLAY_GUARD_INTERP					0xABABABAB
#define REPLAY_GUARD_HISTORY				0xACACACAC
#define REPLAY_GUARD_END_DATA				0xADADADAD
#define REPLAY_GUARD_EVENT					0xAEAEAEAE
#define REPLAY_GUARD_PARTICLEINTERP			0xAFAFAFAF
#define REPLAY_GUARD_SOUND					0xBBBBBBBB

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

typedef void* FileHandle;
class CEntity;


const u8 ShouldPreloadBit = 7;
const u8 PadU8AsBits = 1;

// See m_packetSize below.
#define REPLAY_PACKET_SIZE_MAX	((0x1 << 24) - 1)

//////////////////////////////////////////////////////////////////////////
class CPacketBase
{
public:
	eReplayPacketId	GetPacketID() const					{	return (eReplayPacketId)m_PacketID;		}
	tPacketSize		GetPacketSize() const				{	return m_packetSize;					}
	void			AddToPacketSize(tPacketSize s)		{	tPacketSize newSize = m_packetSize + s;
															replayFatalAssertf(newSize <= REPLAY_PACKET_SIZE_MAX, "CPacketBase()...Packet size too large.");
															m_packetSize = newSize;						
														}
	tPacketVersion	GetPacketVersion() const			{	return m_packetVersion;					}

	void			Print() const;
	void			PrintXML(FileHandle handle) const;
	CPacketBase*	TraversePacketChain(u32 count, bool validateLastPacket = true) const;
	void			SetShouldPreload(bool b)			{	SetPadBoolInternal(ShouldPreloadBit, b);		}
	bool			ShouldPreload() const				{	return GetPadBoolInternal(ShouldPreloadBit);	}

	u32				GetExpiryTrackerID() const			{ return 0; }

	bool			ValidatePacket() const				
	{	
#if REPLAY_GUARD_ENABLE
		/*replayFatalDumpAssertf(m_BaseGuard == REPLAY_GUARD_BASE, this, 5*1024, "Validation of CPacketBase Failed!");*/
		replayAssertf(m_BaseGuard == REPLAY_GUARD_BASE, "Validation of CPacketBase Failed! 0x%X, %p", m_BaseGuard, this);
		return m_BaseGuard == REPLAY_GUARD_BASE;
#else
		return true;
#endif
	}

	REPLAY_GUARD_ONLY(size_t	GetGuardOffset() const	{	return (u8*)&m_BaseGuard - (u8*)this;	}	)

protected:
	CPacketBase(const eReplayPacketId packetID, tPacketSize size, tPacketVersion version = InitialVersion)
		: m_PacketID((tPacketID)packetID)
		, m_packetSize(size)
		, m_packetVersion(version)
		REPLAY_GUARD_ONLY(, m_BaseGuard(REPLAY_GUARD_BASE))
	{
		replayFatalAssertf(size <= REPLAY_PACKET_SIZE_MAX, "CPacketBase()...Packet size too large.");
		m_padU8[0] = m_padU8[1] = 0;
	}

	void			Store(eReplayPacketId id, tPacketSize size, tPacketVersion version = InitialVersion)
	{
		m_padU8[0] = m_padU8[1] = 0;	
		m_PacketID = (tPacketID)id;
		m_packetSize = size;
		m_packetVersion = version;
		REPLAY_GUARD_ONLY(m_BaseGuard = REPLAY_GUARD_BASE;) 
	}

	void			SetPadU8(u8 index, u8 val) const		{	SetPadU8Internal(index, val);	}
	u8				GetPadU8(u8 index) const				{	return GetPadU8Internal(index);	}

	void			SetPadBool(u8 index, bool val) const	{	replayAssert(index != ShouldPreloadBit);	SetPadBoolInternal(index, val);		}
	bool			GetPadBool(u8 index) const				{	replayAssert(index != ShouldPreloadBit);	return GetPadBoolInternal(index);	}

	template <typename packettype>
	void CloneBase(const CPacketBase* pSource)
	{
		if(pSource != NULL)
		{
			sysMemCpy(this, pSource, sizeof(packettype));
		}
	}

private:
	CPacketBase()
		: m_PacketID((tPacketID)PACKETID_INVALID)
		REPLAY_GUARD_ONLY(, m_BaseGuard(REPLAY_GUARD_BASE))
	{
		m_padU8[0] = m_padU8[1] = 0;
	}

	void			SetPadU8Internal(u8 index, u8 val) const
	{	
		replayFatalAssertf(index <= PadU8AsBits, "");
		m_padU8[index] = val;
	}
	u8				GetPadU8Internal(u8 index) const
	{	
		replayFatalAssertf(index <= PadU8AsBits, "");
		return m_padU8[index];
	}

	void			SetPadBoolInternal(u8 index, bool val) const
	{
		replayFatalAssertf(index < 8, "");
		if(val)	m_padU8[PadU8AsBits] |= (u8)1 << index;	else m_padU8[PadU8AsBits] &= (~((u8)1 << index));
	}

	bool			GetPadBoolInternal(u8 index) const
	{
		replayFatalAssertf(index < 8, "");
		return (m_padU8[PadU8AsBits] & ((u8)1 << index)) != 0;
	}

	tPacketID			m_PacketID;
	mutable u8	m_padU8[2];

	u32			m_packetSize : 24;
	u32			m_packetVersion : 8;

	REPLAY_GUARD_ONLY(u32 m_BaseGuard;)
};

//////////////////////////////////////////////////////////////////////////
class CPacketEnd : public CPacketBase
{
public:
	CPacketEnd()
		: CPacketBase(PACKETID_END, sizeof(CPacketEnd))
	{}

	void	Store()								{ CPacketBase::Store(PACKETID_END, sizeof(CPacketEnd)); }

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_END, "Validation of CPacketEnd Failed!, %d", GetPacketID());

		return	CPacketBase::ValidatePacket() &&
			GetPacketID() == PACKETID_END;
	}
};


//////////////////////////////////////////////////////////////////////////
class CPacketAlign : public CPacketBase
{
public:
	CPacketAlign(u32 align)
		: CPacketBase(PACKETID_ALIGN, sizeof(CPacketAlign))
	{
		Store(align);
	}

	void	Store(u32 align)
	{
		CPacketBase::Store(PACKETID_ALIGN, sizeof(CPacketAlign));

		m_alignment = align;
		u8* endPtr = (u8*)REPLAY_ALIGN(((size_t)this + sizeof(CPacketAlign)), m_alignment);
		AddToPacketSize((tPacketSize)((size_t)endPtr - (size_t)this) - sizeof(CPacketAlign));
	}

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ALIGN, "Validation of CPacketAlign Failed!, %d", GetPacketID());
		replayAssertf((((size_t)this + (size_t)GetPacketSize()) % (size_t)m_alignment == 0), "Validation of CPacketAlign Failed!");

		return	CPacketBase::ValidatePacket() &&
				GetPacketID() == PACKETID_ALIGN &&
				(((size_t)this + (size_t)GetPacketSize()) % (size_t)m_alignment == 0);
	}

private:
	u32		m_alignment;
};


//////////////////////////////////////////////////////////////////////////
const u8 IsInitialStateBit	= 4;
const u8 IsPlayedBit		= 5;
const u8 CancelledBit		= 6;
const u8 TypeU8				= 0;
class CPacketEvent : public CPacketBase
{
public:
	void			SetGameTime(u32 time)						{	m_GameTime = time;								}
	u32				GetGameTime() const							{	return m_GameTime;								}

	bool			ValidatePacket() const
	{	
#if REPLAY_GUARD_ENABLE
		replayAssertf(m_HistoryGuard == REPLAY_GUARD_HISTORY, "Validation of CPacketHistory Failed! 0x%X", m_HistoryGuard);
		return CPacketBase::ValidatePacket() && m_HistoryGuard == REPLAY_GUARD_HISTORY;	
#else
		return CPacketBase::ValidatePacket();
#endif // REPLAY_GUARD_ENABLE
	}

	void			Print() const	{}
	void			PrintXML(FileHandle handle) const;

	bool			ShouldInvalidate() const					{	return IsPlayed() == false;						}
	void			Invalidate()								{	SetPlayed(true);								}

	bool			IsCancelled() const							{	return CPacketBase::GetPadBool(CancelledBit);	}
	void			Cancel()									{	CPacketBase::SetPadBool(CancelledBit, true);	}

	bool			IsPlayed()	const							{	return CPacketBase::GetPadBool(IsPlayedBit);	}
	void			SetPlayed(bool b) const						{	CPacketBase::SetPadBool(IsPlayedBit, b);		}

	bool			IsInitialStatePacket() const				{	return CPacketBase::GetPadBool(IsInitialStateBit);	}
	void			SetIsInitialStatePacket(bool b)				{	CPacketBase::SetPadBool(IsInitialStateBit, b);	}

	s16				GetTrackedID() const						{	return -1;	}

	eShouldExtract			ShouldExtract(u32 playbackFlags, const u32 packetFlags, bool isFirstFrame) const
	{
		// If it's an initial state packet then only play it if we're on the first frame
		// and we're playing forwards....unless the REPLAY_MONITORED_PLAY_BACKWARDS is set...
		bool playInitialStatePacket = isFirstFrame && (playbackFlags & REPLAY_DIRECTION_FWD || packetFlags & REPLAY_MONITORED_PLAY_BACKWARDS);
		if(IsInitialStatePacket())
		{
			if(!playInitialStatePacket)
			{
				return REPLAY_EXTRACT_FAIL;
			}
		}

		if( ((IsPlayed() == false || packetFlags & REPLAY_EVERYFRAME_PACKET) && (playbackFlags & REPLAY_DIRECTION_FWD)) ||
			((IsPlayed() == true || packetFlags & REPLAY_EVERYFRAME_PACKET) && (playbackFlags & REPLAY_DIRECTION_BACK)) )
			return REPLAY_EXTRACT_SUCCESS;
		else
			return REPLAY_EXTRACT_FAIL;
	}

	bool			ShouldAbort() const							{	return false;									}
	bool			ShouldInterp() const						{	return false;									}
	bool			ShouldTrack() const							{	return false;									}
	bool			HasExpired(const CEventInfoBase&) const		{	replayAssertf(false, "Packet does not have a valid 'HasExpired' function");	return true;	}
	bool			NeedsEntitiesForExpiryCheck() const			{	return false;									}
	void			SetNextOffset(u16, u32)						{	replayFatalAssertf(false, "Should never be called"); }
	u16				GetNextBlockIndex() const					{	replayFatalAssertf(false, "Should never be called"); return 0; }
	u32				GetNextPosition() const						{	replayFatalAssertf(false, "Should never be called"); return 0; }

	u32				GetPreplayTime(const CEventInfoBase&) const						{	return 0;	}

	void			UpdateMonitorPacket() {}

protected:
	CPacketEvent(const eReplayPacketId packetID, tPacketSize size, tPacketVersion version = InitialVersion);

	void			Store(const eReplayPacketId packetID, tPacketSize size, tPacketVersion version = InitialVersion);

	void			SetPadU8(u8 index, u8 val) const			{	replayAssert(index != TypeU8);	CPacketBase::SetPadU8(index, val);	}
	void			SetPadBool(u8 index, bool val) const		{	replayAssert(index != IsPlayedBit && index != CancelledBit && index != IsInitialStateBit);	CPacketBase::SetPadBool(index, val);	}

private:
	REPLAY_GUARD_ONLY(u32		m_HistoryGuard;)
	u32				m_GameTime;

	u32				m_unused;	// Unused extra space for flexibility.
};

class CPacketDecalEvent : public CPacketEvent
{
public:

#define NUM_REPLAY_DECALS_PER_FRAME		16	//DECAL_MAX_ADD_INFOS	// This seems to result in missing decals...

	eShouldExtract ShouldExtract(u32 playbackFlags, const u32 packetFlags, bool isFirstFrame) const
	{
		eShouldExtract shouldExtract = CPacketEvent::ShouldExtract(playbackFlags, packetFlags, isFirstFrame);
		bool canPlay = DECALMGR.GetNumAddInfos() < NUM_REPLAY_DECALS_PER_FRAME;
		bool shouldCleanUp = (m_decalId > 0 && playbackFlags & REPLAY_DIRECTION_BACK);

		if( shouldExtract == REPLAY_EXTRACT_FAIL )
		{
			return REPLAY_EXTRACT_FAIL;
		}
		else if(shouldExtract == REPLAY_EXTRACT_SUCCESS && (canPlay || shouldCleanUp))
		{
			return REPLAY_EXTRACT_SUCCESS;
		}
		else
		{
			return REPLAY_EXTRACT_FAIL_RETRY;
		}
	}

	bool HasExpired(const CEventInfoBase&) const
	{
		return !g_decalMan.IsAlive(m_decalId);
	}

	void PrintXML(FileHandle handle) const
	{
		CPacketEvent::PrintXML(handle);

		char str[1024];
		snprintf(str, 1024, "\t\t<m_decalId>%u</m_decalId>\n", m_decalId);
		CFileMgr::Write(handle, str, istrlen(str));
	}

protected:

	CPacketDecalEvent(u32 decalId, const eReplayPacketId packetID, tPacketSize size, tPacketVersion version = InitialVersion)
		: CPacketEvent(packetID, size, version)

	{
		m_decalId = decalId;
	}

	//cleans up the decals during an extract
	bool CanPlayBack(const CReplayState& playbackFlags) const
	{
		//if we're playing back we want to stop extracting so we don't add any decals
		if(playbackFlags.GetState() & REPLAY_DIRECTION_BACK)
		{
			//if we've definitely added a decal it needs to be removed
			if( m_decalId > 0 )
			{
				if(g_decalMan.IsAlive(m_decalId))
					g_decalMan.Remove(m_decalId);
			}

			m_decalId = 0;
			return false;
		}

		return true;
	}

	mutable u32 m_decalId;
};

//////////////////////////////////////////////////////////////////////////
class CPacketHistoryEnd : public CPacketEvent
{
public:
	CPacketHistoryEnd()
		: CPacketEvent(PACKETID_END_HISTORY, sizeof(CPacketHistoryEnd))
	{}

	void	Store()								{ CPacketEvent::Store(PACKETID_END_HISTORY, sizeof(CPacketHistoryEnd)); }

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_END_HISTORY, "Validation of CPacketHistoryEnd Failed!, %d", GetPacketID());

		return	CPacketEvent::ValidatePacket() &&
			GetPacketID() == PACKETID_END_HISTORY;
	}
};


//////////////////////////////////////////////////////////////////////////
class CPacketNextFrame : public CPacketBase
{
public:
	CPacketNextFrame()
		: CPacketBase(PACKETID_NEXTFRAME, sizeof(CPacketNextFrame))
	{}

	void	Store()								{ CPacketBase::Store(PACKETID_NEXTFRAME, sizeof(CPacketNextFrame)); }

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_NEXTFRAME, "Validation of CPacketNextFrame Failed!, %d", GetPacketID());

		return	CPacketBase::ValidatePacket() &&
			GetPacketID() == PACKETID_NEXTFRAME;
	}
};


//////////////////////////////////////////////////////////////////////////
extern tPacketVersion CPacketFrame_V1;
extern tPacketVersion CPacketFrame_V2;
extern tPacketVersion CPacketFrame_V3;
class CPacketFrame : public CPacketBase
{
public:
	CPacketFrame()
		: CPacketBase(PACKETID_FRAME, sizeof(CPacketFrame), CPacketFrame_V3)
		, m_nextFrameDiffBlock(0)
		, m_nextFrameOffset(0)
		, m_prevFrameDiffBlock(0)
		, m_previousFrameOffset(0)
		, m_offsetToEvents(0)
		, m_frameFlags(0)
		, m_disableArtificialLights(0)
		, m_disableArtificialVehLights(0)
		, m_useNightVision(0)
		, m_useThermalVision(0)
		, m_unusedBits(0)
	{
		m_unused[0] = 0;
		m_unused[1] = 0;
		m_unused[2] = 0;
	}

	void		Store(FrameRef frame);
	void		StoreExtensions(FrameRef frame) { PACKET_EXTENSION_RESET(CPacketFrame); (void)frame; }


	void		SetNextFrameOffset(u32 offset, bool diffBlock)	{	m_nextFrameOffset = offset;	m_nextFrameDiffBlock = (diffBlock ? 1: 0); }
	u32			GetNextFrameOffset() const						{	return m_nextFrameOffset;		}
	bool		GetNextFrameIsDiffBlock() const					{	return m_nextFrameDiffBlock == 1; }

	void		SetPrevFrameOffset(u32 offset, bool diffBlock)	{	m_previousFrameOffset = offset;	m_prevFrameDiffBlock = (diffBlock ? 1: 0);	}
	u32			GetPrevFrameOffset() const						{	return m_previousFrameOffset;	}
	bool		GetPrevFrameIsDiffBlock() const					{	return m_prevFrameDiffBlock == 1;	}

	void		SetOffsetToEvents(u32 offset)					{	m_offsetToEvents = offset;		}
	u32			GetOffsetToEvents() const						{	return m_offsetToEvents;		}

	FrameRef	GetFrameRef() const								{	return m_frameRef;				}

	void		PrintXML(FileHandle handle) const;

	bool		ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_FRAME, "Validation of CPacketFrame Failed!, %d", GetPacketID());
		
		return CPacketBase::ValidatePacket() && GetPacketID() == PACKETID_FRAME;
	}

	void		SetFrameFlags(u32 flag)							{	m_frameFlags |= flag;				}
	bool		IsFrameFlagSet(u32 flag) const					{	return (m_frameFlags & flag) !=0;	}
	void		ClearFrameFlags()								{	m_frameFlags = 0;					}
	u32			GetFrameFlags() const							{   return m_frameFlags;				}

	const replayPopulationValues& GetPopulationValues() const	{	return m_populationValues;			}
	const replayTimeWrapValues& GetTimeWarpValues() const		{	return m_timeWrapValues;			}

	u32			GetReplayInstancePriority()	const				{   return m_entityInstancePriority;	}

	bool		GetShouldDisableArtificialLights() const		{	return m_disableArtificialLights;	}
	bool		GetShouldDisableArtificialVehLights() const		{	return m_disableArtificialVehLights;}
	bool		GetShouldUseNightVision() const					{   return m_useNightVision;			}
	bool		GetShouldUseThermalVision() const				{   return m_useThermalVision;			}
	
private:
	u32			m_nextFrameOffset		: 31;
	u32			m_nextFrameDiffBlock	: 1;
	
	u32			m_previousFrameOffset	: 31;
	u32			m_prevFrameDiffBlock	: 1;
	
	u32			m_offsetToEvents;

	u32			m_frameFlags;

	replayPopulationValues	m_populationValues;
	replayTimeWrapValues	m_timeWrapValues;

	FrameRef	m_frameRef;

	u32			m_entityInstancePriority;

	u8			m_disableArtificialLights	: 1;
	u8			m_useNightVision			: 1;
	u8			m_useThermalVision			: 1;
	u8			m_disableArtificialVehLights: 1;
	u8			m_unusedBits				: 4;
	u8			m_unused[3];

	DECLARE_PACKET_EXTENSION(CPacketFrame);
};


class CPacketThermalVisionValues : public CPacketBase
{
public:
	CPacketThermalVisionValues()
		: CPacketBase(PACKETID_THERMALVISIONVALUES, sizeof(CPacketThermalVisionValues))
	{}

	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketThermalVisionValues); };

	void	Extract() const;

private:
	float m_fadeStartDistance;
	float m_fadeEndDistance;
	float m_maxThickness;

	float m_minNoiseAmount;
	float m_maxNoiseAmount;
	float m_hiLightIntensity;
	float m_hiLightNoise;

	u32 m_colorNear;
	u32 m_colorFar;
	u32 m_colorVisibleBase;
	u32 m_colorVisibleWarm;
	u32 m_colorVisibleHot; 

	float m_heatScale[/*TB_COUNT*/4];

	DECLARE_PACKET_EXTENSION(CPacketThermalVisionValues);
};


//////////////////////////////////////////////////////////////////////////
// Timescale value is stored in one of CPacketBase padding members
const u8 TimeScaleU8		= 0;
class CPacketGameTime : public CPacketBase
{
public:
	CPacketGameTime()
		: CPacketBase(PACKETID_GAMETIME, sizeof(CPacketGameTime))
	{}

	void	Store(u32 gameTime);
	void	StoreExtensions(u32) { PACKET_EXTENSION_RESET(CPacketGameTime); };

	u32		GetGameTime() const					{ return m_GameTimeInMs;	}
	u32		GetSystemTime() const				{ return m_SystemTime;		}
	float	GetTimeScale() const				{ return (GetPadU8(TimeScaleU8) * U8_TO_FLOAT_0_1); }

	void	PrintXML(FileHandle handle) const;

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_GAMETIME, "Validation of CPacketGameTime Failed!, %d", GetPacketID());

		return GetPacketID() == PACKETID_GAMETIME && CPacketBase::ValidatePacket();
	}
private:
	u32		m_GameTimeInMs;
	u32		m_SystemTime;
	DECLARE_PACKET_EXTENSION(CPacketGameTime);
};


//////////////////////////////////////////////////////////////////////////
class CPacketClock : public CPacketBase
{
public:
	CPacketClock()
		: CPacketBase(PACKETID_CLOCK, sizeof(CPacketClock))
	{}

	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketClock); }

	u8		GetClockHours() const				{ return m_hours;	}
	u8		GetClockMinutes() const				{ return m_mins;	}
	u8		GetClockSeconds() const				{ return m_seconds; }

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_CLOCK, "Validation of CPacketClock Failed!, %d", GetPacketID());

		return GetPacketID() == PACKETID_CLOCK && CPacketBase::ValidatePacket();
	}

	u8		m_hours;
	u8		m_mins;
	u8		m_seconds;
	u8		m_unused;
	DECLARE_PACKET_EXTENSION(CPacketClock);
};


//////////////////////////////////////////////////////////////////////////
// Wind value is stored in one of CPacketBase padding members
const unsigned int numWindType = 2;
class CPacketWeather : public CPacketBase
{
public:
	
	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketWeather); };
	void	Extract() const;

	u32		GetOldWeatherType() const				{ return m_OldWeatherType;		}
	u32		GetNewWeatherType() const				{ return m_NewWeatherType;		}
	f32		GetWeatherInterp() const				{ return m_WeatherInterp;		}
	f32		GetWeatherRandom() const				{ return m_random;		}
	f32		GetWeatherRandomCloud() const			{ return m_randomCloud;	}
	f32		GetWeatherWetness() const				{ return m_wetness;		}
	float	GetWeatherWind() const					{ return m_wind;	}

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_WEATHER, "Validation of CPacketWeather Failed!, %d", GetPacketID());

		return GetPacketID() == PACKETID_WEATHER && CPacketBase::ValidatePacket();
	}

private:
	CPacketWeather()
		: CPacketBase(PACKETID_WEATHER, sizeof(CPacketWeather))
	{}

	u32		m_OldWeatherType;
	u32		m_NewWeatherType;
	f32		m_WeatherInterp;
	f32		m_random;
	f32		m_randomCloud;
	f32		m_wetness;
	f32		m_wind;
	u32		m_forceOverTimeTypeIndex;
	float	m_overTimeInterp;

	struct WindSettings
	{
		float			angle;
		float			speed;
		float			maxSpeed;
		float			globalVariationMult;
		float			globalVariationSinPosMult;
		float			globalVariationSinTimeMult;
		float			globalVariationCosPosMult;
		float			globalVariationCosTimeMult;
		float			positionalVariationMult;
		bool			positionalVariationIsFluid;
		float			gustChance;
		float			gustSpeedMultMin;
		float			gustSpeedMultMax;
		float			gustTimeMin;
		float			gustTimeMax;
	}		m_windSettings[numWindType];

	struct WindState
	{
		CPacketVector<3>	vBaseVelocity;
		CPacketVector<3>	vGustVelocity;
		CPacketVector<3>	vGlobalVariationVelocity;
	}		m_windState[numWindType];

	struct GustState
	{
		float			gustDir;
		float			gustSpeed;
		float			gustTotalTime;
		float			gustCurrTime;
	}		m_gustState[numWindType];

	DECLARE_PACKET_EXTENSION(CPacketWeather);

	float m_overTimeTransistionLeft;
	float m_overTimeTransistionTotal;

	u8		m_useSnowFootVfx	: 1;
	u8		m_useSnowWheelVfx	: 1;
	u8		m_unusedBits		: 6;
	u8		m_unused[3];
};

//////////////////////////////////////////////////////////////////////////
// Script Time Cycle Modifiers (it's not Script anymore, all modifiers)
// NOTE: To Be Removed at the next packet flattening, now handled by CPacketTimeCycleModifier
class CPacketScriptTCModifier : public CPacketBase
{
public:
	CPacketScriptTCModifier()
		: CPacketBase(PACKETID_TC_SCRIPT_MODIFIER, sizeof(CPacketScriptTCModifier))
	{}

	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketScriptTCModifier); }
	void	Extract() const;

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_TC_SCRIPT_MODIFIER, "Validation of CPacketScriptTCModifier Failed!, %d", GetPacketID());
		return GetPacketID() == PACKETID_TC_SCRIPT_MODIFIER && CPacketBase::ValidatePacket();
	}

	bool					m_bSniperScope	:1;
	tcKeyframeUncompressed	m_keyData;
	DECLARE_PACKET_EXTENSION(CPacketScriptTCModifier);
};

//////////////////////////////////////////////////////////////////////////
// Time Cycle Modifiers
class CPacketTimeCycleModifier : public CPacketBase
{
public:
	CPacketTimeCycleModifier()
		: CPacketBase(PACKETID_TIMECYCLE_MODIFIER, sizeof(CPacketTimeCycleModifier))
	{}

	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketTimeCycleModifier); }
	void	Extract() const;
	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_TIMECYCLE_MODIFIER, "Validation of CPacketTimeCycleModifier Failed!, %d", GetPacketID());
		return GetPacketID() == PACKETID_TIMECYCLE_MODIFIER && CPacketBase::ValidatePacket();
	}

private:
	CTimeCycle::TCNameAndValue *GetTCValues() const
	{
		return (CTimeCycle::TCNameAndValue *)((u8 *)GetTCDefaultValues() - m_NoOfTCKeys*sizeof(CTimeCycle::TCNameAndValue));
	}

	CTimeCycle::TCNameAndValue *GetTCDefaultValues() const
	{
		return (CTimeCycle::TCNameAndValue *)((u8 *)this + GetPacketSize() - m_NoOfDefaultTCKeys*sizeof(CTimeCycle::TCNameAndValue));
	}

	bool ShouldUseDefaultData() const;

	bool IsSniperScopeFxActive() const { return (bool)m_bSniperScope; }
	bool IsAnimPostFxActive() const { return (bool)m_bAnimPostFxActive; }
	bool IsPausePostFxActive() const { return (bool)m_bPausePostFxActive; }

	u8								m_bSniperScope			:1;
	u8								m_bAnimPostFxActive		:1;
	u8								m_bPausePostFxActive	:1;
	u8								m_unUsedBits			:5;
	u8								m_unUsedBytes[3];

	u32								m_NoOfTCKeys;
	u32								m_NoOfDefaultTCKeys;

	//needed to determine if we're currently running script fx
	u32								m_ScriptModIndex;
	u32								m_TransistionModIndex;
	
	DECLARE_PACKET_EXTENSION(CPacketTimeCycleModifier);
};


//////////////////////////////////////////////////////////////////////////
// Map change packets.
#define REPLAY_MAP_CHANGE_INITIAL_STATE_SIZE	20

class CPacketMapChange : public CPacketBase
{
private:
	class SearchVol
	{
	public:
		SearchVol()
		{
			searchVol[0] = searchVol[1] = searchVol[2] = searchVol[3] = 0.0f;
		}
		bool operator==(const SearchVol &other) const
		{
			for(u32 i=0; i<4; i++)
				if(searchVol[i] != other.searchVol[i])
					return false;
			return true;
		}
		void Set(spdSphere &in)
		{
			searchVol[0] = in.GetV4Ref().GetXf();
			searchVol[1] = in.GetV4Ref().GetYf();
			searchVol[2] = in.GetV4Ref().GetZf();
			searchVol[3] = in.GetV4Ref().GetWf();
		}
		void Get(spdSphere &out)
		{
			Vec3V c = Vec3V(searchVol[0], searchVol[1], searchVol[2]);
			ScalarV r = ScalarV(searchVol[3]);
			out.Set(c, r);
		}
	public:
		float searchVol[4];
	};
	class MapChange
	{
	public:
		MapChange()
		{
			oldHash = 0;
			newHash = 0;
			changeType = 0;
			bAffectsScriptObjects = false;
			bSurviveMapReload = false;
			bExecuted = false;
		}
		
		bool operator==(const MapChange &other) const
		{
			return (oldHash==other.oldHash && newHash==other.newHash && searchVol==other.searchVol && changeType==other.changeType && bSurviveMapReload == other.bSurviveMapReload && bAffectsScriptObjects==other.bAffectsScriptObjects && bExecuted==other.bExecuted);
		}
	public:
		u32 oldHash;
		u32 newHash;
		u32 changeType;
		bool bAffectsScriptObjects	: 1;
		bool bSurviveMapReload		: 1;
		bool bExecuted				: 1;
		SearchVol searchVol;
	};

public:
	CPacketMapChange();
	~CPacketMapChange();

	void Store();
	void StoreExtensions() { PACKET_EXTENSION_RESET(CPacketMapChange); }
	void Extract() const;

	static void OnBeginPlaybackSession();
	static void OnCleanupPlaybackSession();

	void	PrintXML(FileHandle handle) const;

private:
	MapChange *GetMapChanges() const
	{
		return (MapChange *)((u8 *)this + GetPacketSize() - m_NoOfMapChanges*sizeof(MapChange));
	}

	static void AttainState(MapChange *pToAttain, u32 toAttainSize);
	static void CollectActiveMapChanges(MapChange *pDest);
	static void StuffInListANotInListB(MapChange *pA, u32 sizeA, MapChange *pB, u32 sizeB, MapChange *pOut, u32 &outSize);
	static void ExecuteIfRequired(MapChange *pToAttain, u32 toAttainSize);

private:
	u32 m_NoOfMapChanges;
	DECLARE_PACKET_EXTENSION(CPacketMapChange);

	static atFixedArray<MapChange, REPLAY_MAP_CHANGE_INITIAL_STATE_SIZE> ms_ActiveMapChangesUponPlayStart;
};

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Distanst car flags
class CPacketDistantCarState : public CPacketBase
{
public:
	CPacketDistantCarState()
		: CPacketBase(PACKETID_DISTANTCARSTATE, sizeof(CPacketDistantCarState))
	{}

	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketDistantCarState); }
	void	Extract() const;

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_DISTANTCARSTATE, "Validation of CPacketDistantCarState Failed!, %d", GetPacketID());
		return GetPacketID() == PACKETID_DISTANTCARSTATE && CPacketBase::ValidatePacket();
	}

private:

	float m_RandomVehicleDensityMult;
	bool m_enableDistantCars : 1;

	DECLARE_PACKET_EXTENSION(CPacketDistantCarState);
};


// General purpose class for "Stuff that should be disabled this frame"
// Currently includes:-
//		model culls this frame			(to replace CPacketModelCull)
//		disabled occlusion this frame	(to replace CPacketDisableOcclusion)
const tPacketVersion CPacketDisabledThisFrame_V1 = 1;
class CPacketDisabledThisFrame : public CPacketBase
{
public:
	CPacketDisabledThisFrame()
		: CPacketBase(PACKETID_DISABLED_THIS_FRAME, sizeof(CPacketDisabledThisFrame))
	{}

	void	Store();
	void	StoreExtensions() { PACKET_EXTENSION_RESET(CPacketDisabledThisFrame); }
	void	Extract() const;

	bool	ValidatePacket() const	
	{	
		replayAssertf(GetPacketID() == PACKETID_DISABLED_THIS_FRAME, "Validation of CPacketDisabledThisFrame Failed!, %d", GetPacketID());
		return GetPacketID() == PACKETID_DISABLED_THIS_FRAME && CPacketBase::ValidatePacket();
	}

	static void Reset()
	{
		ms_ModelCullsToStore.Reset();
		ms_ModelShadowCullsToStore.Reset();
		ms_DisableOcclusionThisFrame = false;
	}

	static void RegisterModelCullThisFrame(u32 modelHash)
	{
		ms_ModelCullsToStore.PushAndGrow(modelHash);
	}

	static void RegisterModelShadowCullThisFrame(u32 modelHash)
	{
		ms_ModelShadowCullsToStore.PushAndGrow(modelHash);
	}
		
	static void RegisterOcclusionThisFrame() 
	{ 
		ms_DisableOcclusionThisFrame = true; 
	}

private:

	u32 *GetModelCulls() const
	{
		if(GetPacketVersion() >= CPacketDisabledThisFrame_V1)
			return (u32*)((u8 *)this + GetPacketSize() - (m_NoOfModelCulls*sizeof(u32) + m_NoOfModelShadowCulls*sizeof(u32)));
		else
			return (u32*)((u8 *)this + GetPacketSize() - m_NoOfModelCulls*sizeof(u32));
	}

	u32 *GetModelShadowCulls() const
	{
		replayAssert(GetPacketVersion() >= CPacketDisabledThisFrame_V1);
		return (u32*)((u8*)GetModelCulls() + m_NoOfModelCulls*sizeof(u32));
	}

	u32				m_NoOfModelCulls;
	u32				m_NoOfModelShadowCulls;
	bool			m_DisableOcclusionThisFrame;

	static atArray<u32> ms_ModelCullsToStore;
	static atArray<u32> ms_ModelShadowCullsToStore;
	static bool			ms_DisableOcclusionThisFrame;

	DECLARE_PACKET_EXTENSION(CPacketDisabledThisFrame);
};


#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#	endif // GTA_REPLAY

#endif  // REPLAY_PACKET_H
