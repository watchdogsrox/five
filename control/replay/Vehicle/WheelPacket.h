#ifndef __WHEEL_PACKET_H
#define __WHEEL_PACKET_H

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "Control/replay/PacketBasics.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/ReplayController.h"

class CVehicle;
class WheelValueExtensionData;

//========================================================================================================================
// CPacketWheel.
//========================================================================================================================

class CPacketWheel : public CPacketBase
{
public:
	void	Store(const CVehicle *pVehicle, u16 sessionBlockIndex);
	void	StoreExtensions(const CVehicle *pVehicle, u16 sessionBlockIndex) { PACKET_EXTENSION_RESET(CPacketWheel); (void)pVehicle; (void)sessionBlockIndex; };
	void	Extract(ReplayController& controller, CVehicle *pVehicle, CPacketWheel *pNext, float interp);

	u32 *GetWheelData()
	{
		return (u32 *)((u8 *)this + GetPacketSize() - m_NoOf4ByteElements*sizeof(int));
	}
public:
	static void OnRecordStart();

public:
	static WheelValueExtensionData *GetExtensionDataForRecording(CVehicle *pVehicle, bool isForDeletePacket, u16 sessionBlockIndex);
	static WheelValueExtensionData *GetExtensionDataForPlayback(CVehicle *pVehicle);
private:
	static WheelValueExtensionData *SetUpExtensions(CVehicle *pVehicle, bool isForDeletePacket, bool &wasAdded);

private:
	u16	m_NoOfWheels		: 4;
	u16	m_NoOf4ByteElements : 12;
	u16 m_sessionBlockIndex;
	DECLARE_PACKET_EXTENSION(CPacketWheel);

	u16 m_deadTyres;

	static u32 ms_RecordSession;

};

#endif // GTA_REPLAY

#endif //__WHEEL_PACKET_H
