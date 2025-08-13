//=====================================================================================================
// name:		ParticleMarkFxPacket.h
// description:	Mark Fx particle replay packet.
//=====================================================================================================

#ifndef INC_MARKFXPACKET_H_
#define INC_MARKFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Effects/ParticlePacket.h"
#include "control/replay/packetbasics.h"

namespace rage
{class ptxEffectInst;}

//=====================================================================================================
class CPacketMarkScrapeFx : public CPacketEventInterp, public CPacketEntityInfo<1>
{
public:

	CPacketMarkScrapeFx(u32 uPfxHash, Matrix34& rMatrix, float fTime1, float fTime2, bool bIsDynamic, bool bIsPhysical, u8 uColType, u32 uMagicKey);

	void Extract(CEntity* pEntity, float fInterp = 0.0f);

	float GetEvoTime1() const			{ return m_fEvoTime1; }
	float GetEvoTime2() const			{ return m_fEvoTime2; }

	void StoreMagicKey(u32 uMagicKey);
	u32	 LoadMagicKey();

	bool IsDynamic() const				{ return m_bIsDynamic; }

	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MARKSCRAPEFX, "Validation of CPacketMarkScrapeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventInterp::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_MARKSCRAPEFX;
	}
protected:
	//-------------------------------------------------------------------------------------------------
	void ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketMarkScrapeFx *pNextPacket, float fInterp);
	void FetchQuaternionAndPos(Quaternion& rQuat, Vector3& rPos);

	void StoreMatrix(const Matrix34& rMatrix);
	void LoadMatrix(Matrix34& rMatrix);

	//-------------------------------------------------------------------------------------------------
	u32		m_MarkQuaternion;
	float	m_Position[3];
	u32		m_pfxHash;

	// TODO: Compress this, 0.0f - 1.0f
	float	m_fEvoTime1;
	float	m_fEvoTime2;

	u8		m_bIsDynamic		:1;
	u8		m_bIsPhysical		:1;
	u8		m_uColType			:6;
	u8		m_uMagicKey[3];
};


#endif // GTA_REPLAY

#endif // INC_MARKFXPACKET_H_
