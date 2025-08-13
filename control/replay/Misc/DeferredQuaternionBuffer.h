#ifndef INC_DEFERRED_QUATERNION_BUFFER_H
#define INC_DEFERRED_QUATERNION_BUFFER_H

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "vector/quaternion.h"
#include "control/replay/PacketBasics.h"

#define REPLAY_DEFERRED_QUATERION_BUFFER_SIZE_FRAGMENT	1024 * 16
#define REPLAY_DEFERRED_QUATERION_BUFFER_SIZE_PED		1024 * 18

template<int QUATSIZE>
class DeferredQuaternionBuffer
{
public:
	class TempBucket
	{
	public:
		TempBucket(u32 capacity, Quaternion *pMem)
		{
			m_Capacity = capacity;
			m_NoOfQuaternions = 0;
			m_pMem = pMem;
		}
		~TempBucket() 
		{
		}

		void AddQuaternion(Quaternion &q)
		{
			replayAssertf(m_NoOfQuaternions <= m_Capacity, "DeferredQuaternionBuffer()....Out of space!");
			m_pMem[m_NoOfQuaternions++] = q;
		}

	public:
		u32 m_Capacity;
		u32 m_NoOfQuaternions;
		Quaternion *m_pMem;
	};

public:
	DeferredQuaternionBuffer(){}
	~DeferredQuaternionBuffer(){}

public:
	void Reset()
	{
		m_NoOfQuaternions = 0;
	}
	u32 AddQuaternions(TempBucket &tempBucket)
	{
		u32 destIndex = sysInterlockedAdd(&m_NoOfQuaternions, tempBucket.m_NoOfQuaternions);
		replayAssertf(destIndex + tempBucket.m_NoOfQuaternions <=QUATSIZE, "DeferredQuaternionBuffer::AddQuaternions()...Out of deferred quaternion buffer space.");
		sysMemCpy(&m_Quaternions[destIndex], tempBucket.m_pMem, sizeof(Quaternion)*tempBucket.m_NoOfQuaternions);
		return destIndex;
	}
	void PackQuaternions(u32 index, u32 noOfQuaternions, CPacketQuaternionL *pDest)
	{
		if(replayVerifyf(index < QUATSIZE, "Index should be within 0 and %u but is %u (L)", QUATSIZE, index))
		{
			Quaternion *pSrc = &m_Quaternions[index];

			for(u32 i = 0; i < noOfQuaternions; i++)
				pDest[i].StoreQuaternion(pSrc[i]);
		}
	}
	void PackQuaternions(u32 index, u32 noOfQuaternions, CPacketQuaternionH *pDest)
	{
		if(replayVerifyf(index < QUATSIZE, "Index should be within 0 and %u but is %u (H)", QUATSIZE, index))
		{
			Quaternion *pSrc = &m_Quaternions[index];

			for(u32 i = 0; i < noOfQuaternions; i++)
				pDest[i].StoreQuaternion(pSrc[i]);
		}
	}


private:
	u32 m_NoOfQuaternions;
	Quaternion m_Quaternions[QUATSIZE];
};



#endif // GTA_REPLAY

#endif // INC_DEFERRED_QUATERNION_BUFFER_H
