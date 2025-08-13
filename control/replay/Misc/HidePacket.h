#ifndef __HIDE_PACKET_H__
#define __HIDE_PACKET_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"

//======================================================================

struct  ReplayClosestObjectToHideSearchStruct
{
	CEntity *pEntity;
	u32		modelNameHashToCheck;
	float	closestDistanceSquared;
	Vector3 coordToCalculateDistanceFrom;	
};


class CutsceneNonRPObjectHide
{
public:
	static	u32	GetModelAndPositionHash(CEntity *pEntity);

	u8		shouldHide:1;					// If false, needs making visible again

	u32		modelNameHash;
	float	searchRadius;
	CPacketVector3 searchPos;
};

class CPacketCutsceneNonRPObjectHide : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketCutsceneNonRPObjectHide(CutsceneNonRPObjectHide &hide)	: CPacketEvent(PACKETID_CUTSCENE_NONRP_OBJECT_HIDE, sizeof(CPacketCutsceneNonRPObjectHide))
	{
		m_CutsceneHide = hide;
	}

	void Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_CUTSCENE_NONRP_OBJECT_HIDE, "Validation of CPacketCutsceneNonRPObjectHide Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_CUTSCENE_NONRP_OBJECT_HIDE;
	}

private:

	static CEntity	*FindEntity(const u32 modelNameHash, float searchRadius, Vector3 &searchPos);
	static bool		GetClosestObjectCB(CEntity* pEntity, void* data);

	CutsceneNonRPObjectHide	m_CutsceneHide;
};

//======================================================================

#endif	//GTA_RPELAY
#endif //__HIDE_PACKET_H__