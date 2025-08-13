//=====================================================================================================
// name:		ParticlePacket.h
// description:	Particle replay packet.
//=====================================================================================================

#ifndef INC_PFXPACKET_H_
#define INC_PFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/misc/replaypacket.h"
#include "Control/Replay/Replay_Channel.h"
#include "control/replay/PacketBasics.h"

#define DEBUG_REPLAY_PFX		(0)
#define DEBUG_REPLAY_PTEX		(0)

#if PH_MATERIAL_ID_64BIT
typedef u64 Id;
#else
typedef u32 Id;
#endif

//////////////////////////////////////////////////////////////////////////
const s16 InvalidTrackingID = -1;
class CPacketEventTracked : public CPacketEvent
{
public:
	CPacketEventTracked(const eReplayPacketId packetID, tPacketSize size, tPacketVersion version = InitialVersion)
		: CPacketEvent(packetID, size, version)
		, m_trackedID(InvalidTrackingID)
		, m_endTracking(0)
	{}

	bool		ValidatePacket() const							{	return CPacketEvent::ValidatePacket();	}

	void		SetTrackedID(const s16 id)						{	m_trackedID = id;						}
	s16			GetTrackedID() const							{	return m_trackedID;						}

	void		SetEndTracking()								{	m_endTracking = 1;			}
	bool		ShouldEndTracking() const						{	return m_endTracking == 1;	}
	bool		ShouldStartTracking() const						{	return false;				}
	bool		ShouldTrack() const								{	return true;				}

	bool		HasExpired(const CTrackedEventInfoBase&) const	{	replayAssertf(false, "Packet does not have a valid 'HasExpired' function");	return true;	}

	u32			GetPreplayTime(const CTrackedEventInfoBase&) const { return 0; } 


	bool		Setup(bool tracked)
	{
		replayDebugf2("Continue tracking %d", GetTrackedID());
		return tracked;
	}

	void		PrintXML(FileHandle handle) const
	{
		CPacketEvent::PrintXML(handle);

		char str[1024];
		snprintf(str, 1024, "\t\t<TrackedID>%d</TrackedID>\n", m_trackedID);
		CFileMgr::Write(handle, str, istrlen(str));
	}

protected:
	s16				m_trackedID		: 15;
	s16				m_endTracking	: 1;
	s16				m_pad;
};


//////////////////////////////////////////////////////////////////////////
class CPacketEventInterp : public CPacketEventTracked
{
	static const u32 INVALID_INTERP_PACKET = 0xFFFFFFFF;
	static const u32 HAS_BEEN_DELETED = 0xFFFFFFFE;

public:
	bool				ValidatePacket() const				
	{	
#if REPLAY_GUARD_ENABLE
		replayFatalAssertf(m_ParticleInterpGuard == REPLAY_GUARD_PARTICLEINTERP, "Validation of CPacketEventInterp Failed!");
		return CPacketEventTracked::ValidatePacket() && m_ParticleInterpGuard == REPLAY_GUARD_PARTICLEINTERP;	
#else
		return CPacketEventTracked::ValidatePacket();
#endif
	}

	void	PrintXML(FileHandle handle) const;

	u16		GetNextBlockIndex() const						{ return m_NextBlockIndex; }
	u32		GetNextPosition() const							{ return m_NextPosition; }

	void	Init();

	void	SetPfxKey(u8 key)								{ m_uPfxKey = key;	}
	u8		GetPfxKey() const								{ return m_uPfxKey; }

	bool	ShouldInterp() const							{	return true;	}

	bool	HasNextOffset() const							{ return IsNextDeleted() == false && m_NextPosition != INVALID_INTERP_PACKET; }
	bool	IsNextDeleted() const							{ return m_NextPosition == HAS_BEEN_DELETED; }

	void	SetNextOffset(u16 blockIndex, u32 position);
	void	SetNextDeleted()								{ m_NextPosition = HAS_BEEN_DELETED; }

	bool	ShouldEndTracking()	const						{	return false;	}

	bool	Setup(bool)										{	return true;	}

	u32		GetPreplayTime(const CEventInfo<void>&) const	{ return 0; } 

protected:
	CPacketEventInterp(eReplayPacketId uPacketID, tPacketSize size, tPacketVersion version = InitialVersion)
		: CPacketEventTracked(uPacketID, size, version)
		REPLAY_GUARD_ONLY(, m_ParticleInterpGuard(REPLAY_GUARD_PARTICLEINTERP))
		, m_NextPosition((u32)-1)
		, m_NextBlockIndex(0)
	{}

private:

	REPLAY_GUARD_ONLY(u32	m_ParticleInterpGuard;)

	u32 m_NextPosition;
	u16	m_NextBlockIndex;

	u8	m_uPfxKey;
};

#endif // GTA_REPLAY

#endif // INC_PFXPACKET_H_
