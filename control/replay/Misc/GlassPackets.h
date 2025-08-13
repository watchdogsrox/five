#ifndef GLASS_PACKET_H
#define GLASS_PACKET_H

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/PacketBasics.h"
#include "control/replay/Effects/ParticlePacket.h"
#include "Objects/object.h"

#include "breakableglass/glassmanager.h"

class CPacketCreateBreakableGlass : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketCreateBreakableGlass(int groupIndex, int boneIndex, Vec3V_In position, Vec3V_In impact, int glassCrackType);

	void				Extract(const CEventInfo<CObject>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CObject>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfo<CObject>& eventInfo) const;
	bool				NeedsEntitiesForExpiryCheck() const  { return true; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_GLASS_CREATEBREAKABLEGLASS, "Validation of CPacketCreateBreakableGlass Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_GLASS_CREATEBREAKABLEGLASS;
	}

private:
	CPacketVector<3> m_position;
	CPacketVector<3> m_impact;
	u8 m_boneIndex;
	u8 m_groupIndex;
	u8 m_glassCrackType;
};

class CPacketHitGlass : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketHitGlass(int groupIndex, int crackType, Vec3V_In position, Vec3V_In impulse);

	void				Extract(const CEventInfo<CObject>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CObject>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfo<CObject>& eventInfo) const;
	bool				NeedsEntitiesForExpiryCheck() const  { return true; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_GLASS_HIT, "Validation of CPacketHitGlass Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_GLASS_HIT;
	}

private:
	CPacketVector<3> m_position;
	CPacketVector<3> m_impulse;
	u8 m_groupIndex;
	u8 m_crackType;
	
};

class CPacketTransferGlass : public CPacketEvent, public CPacketEntityInfo<2>
{
public:
	CPacketTransferGlass(int groupIndex);

	void				Extract(const CEventInfo<CObject>& eventInfo) const;
	ePreloadResult		Preload(const CEventInfo<CObject>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult		Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool				HasExpired(const CEventInfo<CObject>& eventInfo) const;
	bool				NeedsEntitiesForExpiryCheck() const  { return true; }

	void				PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool				ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_GLASS_TRANSFER, "Validation of CPacketTransferGlass Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_GLASS_TRANSFER;
	}

private:
	u8 m_groupIndex;
};

class replayGlassManager : public glassRecorderInterface
{
public:
	void OnCreateBreakableGlass(const phInst* pInst, int groupIndex, int boneIndex, Vec3V_In position, Vec3V_In impact, int glassCrackType);
	void OnHitGlass(bgGlassHandle handle, int crackType, Vec3V_In position, Vec3V_In impulse);
	void OnTransferGlass(const phInst* pSourceInst, const phInst* pDestInst, u16 groupIndex);

	//helper functions for replay
	static int GetGroupFromGlassHandle(const fragHierarchyInst* pGlassObjectHierInst, bgGlassHandle glassHandle);
	static bgGlassHandle GetGlassHandleFromGroup(const fragHierarchyInst* pGlassObjectHierInst, int groupIndex);
	static fragHierarchyInst::GlassInfo* GetGlassInfoFromGroup(const fragHierarchyInst* pGlassObjectHierInst, int groupIndex);
	const static CObject* GetObjectFromInst(const phInst* pInst);
	static void ResetGlassOnBone(const CObject* pGlassObject, int groupIndex);
	static bool HasValidGlass(const CObject* pGlassObject, int groupIndex);

};

#endif // GTA_REPLAY

#endif // GLASS_PACKET_H
