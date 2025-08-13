//
// name:		InterpPacket.h
// description:	Interpolation packet needed for all the packets requiring interpolation during playback in the replay system.
// written by:	Al-Karim Hemraj (R* Toronto)
//

#ifndef INTERP_PACKET_H
#define INTERP_PACKET_H

#include "Control/replay/ReplaySettings.h"
#include "Control/replay/ReplayController.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/misc/ReplayPacket.h"

#define DEBUG_INTERP	(0)




class CInterpFramePosition;
class ReplayController;

//////////////////////////////////////////////////////////////////////////
class CPacketInterp
{
public:
	static const u32 INVALID_INTERP_PACKET = 0xFFFFFFFF;
	static const u32 HAS_BEEN_DELETED = 0xFFFFFFFE;

	void Store();

	bool ValidatePacket() const		
	{	
#if REPLAY_GUARD_ENABLE
		replayFatalAssertf(m_InterpGuard == REPLAY_GUARD_INTERP, "Validation of CPacketInterp Failed!");
		return m_InterpGuard == REPLAY_GUARD_INTERP;
#else
		return true;
#endif // REPLAY_GUARD_ENABLE
	}

	u16	GetNextBlockIndex() const { return m_NextBlockIndex; }
	u32 GetNextPosition() const { return m_NextPosition; }

	bool HasNextOffset() const { return IsNextDeleted() == false && m_NextPosition != INVALID_INTERP_PACKET; }
	bool IsNextDeleted() const { return m_NextPosition == HAS_BEEN_DELETED; }

	void SetNextOffset(u16 blockIndex, u32 position);
	void SetNextDeleted() { m_NextPosition = HAS_BEEN_DELETED; }

private:
	REPLAY_GUARD_ONLY(u32 m_InterpGuard;)

	u16	m_NextBlockIndex;
	u32 m_NextPosition;
};


//////////////////////////////////////////////////////////////////////////
template<typename PACKETTYPE>
class CInterpolator
{
public:
	CInterpolator()
		: m_pPrevPacket(NULL)
		, m_pNextPacket(NULL)
	{}
	virtual ~CInterpolator() {}

	virtual void Init(const ReplayController& controller, PACKETTYPE const* pPrevPacket) = 0;

	void	Reset()					{	m_pPrevPacket = m_pNextPacket = NULL;			}
	bool	IsEmpty() const			{	return (m_pPrevPacket == m_pNextPacket == NULL); }

	void	SetNextPacket(PACKETTYPE const* pNextPacket)
	{
		m_pNextPacket = pNextPacket;
		if(m_pNextPacket)
			m_pNextPacket->ValidatePacket();
	}

	void	SetPrevPacket(PACKETTYPE const* pPrevPacket)
	{
		m_pPrevPacket = pPrevPacket;
		if(m_pPrevPacket)
			m_pPrevPacket->ValidatePacket();
	}

	PACKETTYPE const*	GetNextPacket() const
	{
		if(m_pNextPacket)
			m_pNextPacket->ValidatePacket();
		return m_pNextPacket;
	}

	PACKETTYPE const*	GetPrevPacket() const
	{
		if(m_pPrevPacket)
			m_pPrevPacket->ValidatePacket();
		return m_pPrevPacket;
	}

private:
	PACKETTYPE const*	m_pPrevPacket;
	PACKETTYPE const*	m_pNextPacket;
};


//////////////////////////////////////////////////////////////////////////
template<typename INTERP>
class InterpWrapper
{
public:
	InterpWrapper(INTERP& interper, ReplayController& replayController)
		: m_interper(interper)
		, m_controller(replayController)
	{}

	~InterpWrapper()
	{
		m_controller.AdvanceUnknownPacket(m_interper.GetPrevFullPacketSize());
		m_interper.Reset();
	}

private:
	INTERP&				m_interper;
	ReplayController&	m_controller;
};


#	endif // GTA_REPLAY

#endif  // INTERP_PACKET_H
