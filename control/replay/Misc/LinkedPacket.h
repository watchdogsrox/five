#ifndef LINKED_PACKET_H
#define LINKED_PACKET_H

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/misc/ReplayPacket.h"
#include "control/replay/ReplaySupportClasses.h"

////////////////////////////////////////////////////////////////////////
static const u32 LINKED_PACKET_INVALID_BLOCK_POS = 0xFFFFFFFF;

class CPacketLinkedBase : public CPacketBase
{
public:
	CPacketLinkedBase(const eReplayPacketId packetID, tPacketSize size)
	: CPacketBase(packetID, size)
	{
		Reset();
	}

	void Reset()
	{
		m_PreviousBlockIndex = 0;
		m_PreviousPosition = LINKED_PACKET_INVALID_BLOCK_POS;
		m_NextBlockIndex = 0;
		m_NextPosition = LINKED_PACKET_INVALID_BLOCK_POS;
	}

	void SetFrameRecorded(const FrameRef& currentFrameRef)
	{
		replayAssertf(currentFrameRef != FrameRef::INVALID_FRAME_REF, "Error, setting frame in CPacketLinked::SetFrameRecorded() this frame isn't valid!");
		m_FrameRecorded = currentFrameRef;
	}

	const FrameRef&	GetFrameRecorded() const { return m_FrameRecorded; }

	void SetPreviousPacket(u16 block, u32 position)
	{
		m_PreviousBlockIndex = block;
		m_PreviousPosition = position;
	}

	void SetNextPacket(u16 block, u32 position)
	{
		m_NextBlockIndex = block;
		m_NextPosition = position;
	}

	bool HasPreviousPacket() const  { return m_PreviousPosition != LINKED_PACKET_INVALID_BLOCK_POS; }
	bool HasNextPacket() const { return m_NextPosition != LINKED_PACKET_INVALID_BLOCK_POS; }

	float GetInterpValue(FrameRef currentFrameRef, const CBufferInfo& bufferInfo) const
	{
		Assertf(HasNextPacket(), "Error, this packet doesn't have a next one, can't interp!");

		FrameRef startWaterFrameRef = GetFrameRecorded();
		FrameRef endWaterFrameRef = GetFromBuffer(bufferInfo, m_NextBlockIndex, m_NextPosition)->GetFrameRecorded();

		s32 totalFramesInCurrentBlock = bufferInfo.FindBlockFromSessionIndex(startWaterFrameRef.GetBlockIndex())->GetFrameCount();

		s32 totalNumberOfFrames = -1;

		//if the start and end point are in the same block, we can simply compute the difference
		if(startWaterFrameRef.GetBlockIndex() == endWaterFrameRef.GetBlockIndex())
		{
			totalNumberOfFrames = endWaterFrameRef.GetFrame() - startWaterFrameRef.GetFrame();	
		}
		else
		{
			//work out the total based on how many frames we span in the current block, plus the next block frames	
			totalNumberOfFrames = (totalFramesInCurrentBlock - startWaterFrameRef.GetFrame()) + endWaterFrameRef.GetFrame();
		}

		s32 currentInterpFrame = -1;

		if(currentFrameRef.GetBlockIndex() == startWaterFrameRef.GetBlockIndex())
		{
			//if the start and end point are in the same block, we can simply compute the difference
			currentInterpFrame = currentFrameRef.GetFrame() - startWaterFrameRef.GetFrame();
		}
		else
		{
			//otherwise we're in the end block, so we need to add the amount of frames we gone through in the first block
			currentInterpFrame = (totalFramesInCurrentBlock - startWaterFrameRef.GetFrame()) + currentFrameRef.GetFrame();
		}

		replayAssertf(currentInterpFrame >= 0 && currentInterpFrame <= totalNumberOfFrames && totalNumberOfFrames >= 0, 
			"Error computing interp, current frame %u total frames %u, start block %u, end block %u, current %u", 
			currentInterpFrame, totalNumberOfFrames, startWaterFrameRef.GetBlockIndex(), endWaterFrameRef.GetBlockIndex(), currentFrameRef.GetBlockIndex());

		float interpValue = float(currentInterpFrame) / float(totalNumberOfFrames);

		replayDebugf2("interp info: current frame %i total frames %i", currentInterpFrame, totalNumberOfFrames);

		return Clamp(interpValue, 0.0f, 1.0f);
	}

protected:
	const CPacketLinkedBase* GetFromBuffer(const CBufferInfo& bufferInfo, u16 block, u32 position) const
	{
		CBufferInfo* info = const_cast<CBufferInfo*>(&bufferInfo);

		CAddressInReplayBuffer rBufferAddress;
		rBufferAddress.SetPosition(position);
		rBufferAddress.SetBlock(info->FindBlockFromSessionIndex(block));

		CPacketLinkedBase* packet = rBufferAddress.GetBufferPosition<CPacketLinkedBase>();
		return packet;
	}


	u16	m_PreviousBlockIndex;
	u32 m_PreviousPosition;

	u16	m_NextBlockIndex;
	u32 m_NextPosition;

	FrameRef m_FrameRecorded;
};


template<typename PACKETTYPE>
class CPacketLinked : public CPacketLinkedBase
{
public:
	CPacketLinked(const eReplayPacketId packetID, tPacketSize size)
	: CPacketLinkedBase(packetID, size)
	{

	}

	const PACKETTYPE* GetPreviousPacket(const CBufferInfo& bufferInfo) const
	{
		replayAssertf(m_PreviousPosition != LINKED_PACKET_INVALID_BLOCK_POS, "Error, this doesn't have a valid previous packet, use HasPreviousPacket to check before calling!");
		return reinterpret_cast < PACKETTYPE const * >(GetFromBuffer(bufferInfo, m_PreviousBlockIndex, m_PreviousPosition));
	}

	const PACKETTYPE* GetNextPacket(const CBufferInfo& bufferInfo) const
	{
		replayAssertf(m_NextPosition != LINKED_PACKET_INVALID_BLOCK_POS, "Error, this doesn't have a valid next packet, use HasNextPacket to check before calling!");
		return reinterpret_cast < PACKETTYPE const * >(GetFromBuffer(bufferInfo, m_NextBlockIndex, m_NextPosition));
	}
};

#endif // GTA_REPLAY

#endif // LINKED_PACKET_H