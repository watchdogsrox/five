//=====================================================================================================
// name:		PlayerPacket.h
// description:	Player replay packet.
//=====================================================================================================
#if 0
#ifndef INC_PLAYERPACKET_H_
#define INC_PLAYERPACKET_H_

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Ped\PedPacket.h"
#include "Peds\Ped.h"

//=====================================================================================================
class CPacketPlayerUpdate : public CPacketPedUpdate
{
public:
	//-------------------------------------------------------------------------------------------------
	CPacketPlayerUpdate();
	~CPacketPlayerUpdate();

	void Store(CPed* pPed);
	void StoreExtensions(CPed* pPed) { PACKET_EXTENSION_RESET(CPacketPlayerUpdate); (void)pPed; }
	void Load(CPed* pPed, CPacketPlayerUpdate* pNextPacket, float fInterp);

protected:
		//-------------------------------------------------------------------------------------------------
	void LoadXtraBoneRotation(Matrix34& rMatrix, u8 sBoneID);
	void LoadXtraBoneRotation(Matrix34& rMatrix, Quaternion& rQuatNew, float fInterp, u8 sBoneID);

	void StoreXtraBoneRotation(const Matrix34& rMatrix, u8 uBoneID);

	void FetchXtraBoneQuaternion(Quaternion& rQuat, u8 sBoneID);
	//-------------------------------------------------------------------------------------------------
	u32		m_XtraBoneQuaternion[PED_XTRA_BONE_COUNT];
	DECLARE_PACKET_EXTENSION(CPacketPlayerUpdate);
};

#endif // GTA_REPLAY

#endif // INC_PLAYERPACKET_H_
#endif