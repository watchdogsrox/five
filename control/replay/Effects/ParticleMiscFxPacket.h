//=====================================================================================================
// name:		ParticleMiscFxPacket.h
// description:	Misc Fx particle replay packet.
//=====================================================================================================

#ifndef INC_MISCFXPACKET_H_
#define INC_MISCFXPACKET_H_

#include "control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Entity/ObjectPacket.h"
#include "control/replay/Effects/ParticlePacket.h"
#include "vector/vector3.h"
#include "control/replay/ReplayEventManager.h"

class CEntity;

//=====================================================================================================
class CPacketMiscGlassGroundFx : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketMiscGlassGroundFx(u32 uPfxHash, const Vector3& rPos);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_GLASSGROUNDFX, "Validation of CPacketMiscGlassGroundFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_GLASSGROUNDFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StorePos(const Vector3& rVecPos);
	void LoadPos(Vector3& oVecPos) const;

	//-------------------------------------------------------------------------------------------------
	float	m_Position[3];
	u32		m_pfxHash;
};

//=====================================================================================================
class CPacketMiscGlassSmashFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketMiscGlassSmashFx(u32 uPfxHash, const Vector3& rPos, const Vector3& rNormal, float fForce);

	void Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_GLASSSMASHFX, "Validation of CPacketMiscGlassSmashFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_GLASSSMASHFX;
	}

protected:

	void StorePos(const Vector3& rVecPos);
	void LoadPos(Vector3& oVecPos) const;
	//-------------------------------------------------------------------------------------------------
	void StoreNormal(const Vector3& rVecNormal);
	void LoadNormal(Vector3& rVecNormal) const;

	//-------------------------------------------------------------------------------------------------
	float	m_Position[3];
	s8		m_Normal[3];
	u8		m_Pad;
	u32		m_pfxHash;
	float	m_fForce;
};


//////////////////////////////////////////////////////////////////////////
class CPacketTrafficLight : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketTrafficLight(const char* trafficLightPrev, const char* trafficLightNext);

	void	Extract(const CEventInfo<CObject>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CObject>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CObject>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const;
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRAFFICLIGHTOVERRIDE, "Validation of CPacketTrafficLight Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_TRAFFICLIGHTOVERRIDE;
	}

	const char*	GetPrevTrafficLightCommands() const		{	return m_trafficLightPrev;	}

private:
	char	m_trafficLightPrev[REPLAY_NUM_TL_CMDS];
	char	m_trafficLightNext[REPLAY_NUM_TL_CMDS];
};

//////////////////////////////////////////////////////////////////////////
class CPacketStartScriptPtFx : public CPacketEventTracked, public CPacketEntityInfoStaticAware<1,CEntityCheckerAllowNoEntity>
{
public:
	CPacketStartScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes, bool hasColour = false, u8 r = 0, u8 g = 0, u8 b = 0);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_STARTSCRIPTFX, "Validation of CPacketStartScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_STARTSCRIPTFX;
	}

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const;

	//Can be removed if we flatten the clips again.
	const CReplayID&	GetReplayID(u8 = 0) const;

	bool	ShouldStartTracking() const						{	return true;	}
	bool	Setup(bool)
	{
		return true;
	}

	s32 GetSlotId() const { return m_Slot; }

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

protected:
	u32					m_PFxNameHash;
	u32					m_PFxAssetHash;
	int					m_BoneTag;
	CPacketVector<3>	m_Position;
	CPacketVector<3>	m_Rotation;
	float				m_Scale;
	u8					m_InvertAxes;
	s32					m_Slot;	

	bool				m_hasColour;
	u8					m_r;
	u8					m_g;
	u8					m_b;
};


//////////////////////////////////////////////////////////////////////////
class CPacketStartNetworkScriptPtFx : public CPacketEventTracked, public CPacketEntityInfoStaticAware<1,CEntityCheckerAllowNoEntity>
{
public:
	CPacketStartNetworkScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes,
		bool hasColour,	u8 r, u8 g, u8 b,
		u32 evoIDA, float evoValA, u32 evoIDB, float evoValB);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const;
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_STARTNETWORKSCRIPTFX, "Validation of CPacketStartNetworkScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_STARTNETWORKSCRIPTFX;
	}

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const;

	bool	ShouldStartTracking() const						{	return true;	}
	bool	Setup(bool)
	{
		return true;
	}

	s32 GetSlotId() const { return m_Slot; }

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

protected:
	u32					m_PFxNameHash;
	u32					m_PFxAssetHash;
	int					m_BoneTag;
	CPacketVector<3>	m_Position;
	CPacketVector<3>	m_Rotation;
	float				m_Scale;
	u8					m_InvertAxes;
	s32					m_Slot;	

	bool     m_hasColour;
	u8		 m_r;
	u8		 m_g;
	u8		 m_b;

	float    m_evoValA;
	float	 m_evoValB;
	u32		 m_evoIDA;
	u32		 m_evoIDB;
};


//////////////////////////////////////////////////////////////////////////
class CPacketForceVehInteriorScriptPtFx : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketForceVehInteriorScriptPtFx(bool force);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>& ) const {  return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FORCEVEHINTERIORSCRIPTFX, "Validation of CPacketForceVehInteriorScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FORCEVEHINTERIORSCRIPTFX;
	}

private:
	bool		m_forceVehicleInterior;
};


//////////////////////////////////////////////////////////////////////////
class CPacketStopScriptPtFx : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketStopScriptPtFx();

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>& ) const {  return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_STOPSCRIPTFX, "Validation of CPacketStartScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_STOPSCRIPTFX;
	}

	bool	ShouldEndTracking() const						{	return true;	}
};


//////////////////////////////////////////////////////////////////////////
class CPacketDestroyScriptPtFx : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketDestroyScriptPtFx();

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>& ) const {  return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DESTROYSCRIPTFX, "Validation of CPacketDestroyScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DESTROYSCRIPTFX;
	}

	bool	ShouldEndTracking() const						{	return true;	}
};

//////////////////////////////////////////////////////////////////////////
class CPacketUpdateOffsetScriptPtFx : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketUpdateOffsetScriptPtFx(const Matrix34& mat);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>& ) const {  return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_UPDATEOFFSETSCRIPTFX, "Validation of CPacketUpdateOffsetScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_UPDATEOFFSETSCRIPTFX;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const;

protected:

	CPacketPositionAndQuaternion m_Matrix;

};

//////////////////////////////////////////////////////////////////////////
class CPacketEvolvePtFx : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketEvolvePtFx(u32 pEvoHash, float evoVal);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>& ) const {  return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_EVOLVEPTFX, "Validation of CPacketUpdateOffsetScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_EVOLVEPTFX;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const;

protected:

	u32		m_EvoHash;
	float	m_Value;
};

//////////////////////////////////////////////////////////////////////////
class CPacketUpdatePtFxFarClipDist : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketUpdatePtFxFarClipDist(float dist);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>& ) const {  return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_UPDATESCRIPTFXFARCLIPDIST, "Validation of CPacketStartScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_UPDATESCRIPTFXFARCLIPDIST;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const;

private:

	float m_dist;
};


//////////////////////////////////////////////////////////////////////////
class CPacketTriggeredScriptPtFx : public CPacketEvent, public CPacketEntityInfoStaticAware<1, CEntityCheckerAllowNoEntity>
{
public:
	CPacketTriggeredScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes);
	CPacketTriggeredScriptPtFx(u32 pFxNameHash, u32 pFxAssetHash, int boneTag, const Matrix34& mat);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRIGGEREDSCRIPTFX, "Validation of CPacketTriggeredScriptPtFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_TRIGGEREDSCRIPTFX;
	}
	
	s32 GetSlotId() const { return m_Slot; }

protected:
	u32					m_PFxNameHash;
	u32					m_PFxAssetHash;
	int					m_BoneTag;
	CPacketVector<3>	m_Position;
	CPacketVector<3>	m_Rotation;
	float				m_Scale;
	u8					m_InvertAxes;
	s32					m_Slot;

	bool m_HasMatrix;
	CPacketPositionAndQuaternion m_Matrix;
};

//////////////////////////////////////////////////////////////////////////
class CPacketTriggeredScriptPtFxColour : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketTriggeredScriptPtFxColour(float r, float g, float b);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_TRIGGEREDSCRIPTFXCOLOUR, "Validation of CPacketTriggeredScriptPtFxColour Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_TRIGGEREDSCRIPTFXCOLOUR;
	}
	
protected:
	float		m_Red;
	float		m_Green;
	float		m_Blue;
};

//////////////////////////////////////////////////////////////////////////
class CPacketUpdateScriptPtFxColour : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketUpdateScriptPtFxColour(float r, float g, float b);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& eventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_UPDATESCRIPTFXCOLOUR, "Validation of CPacketUpdateScriptPtFxColour Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_UPDATESCRIPTFXCOLOUR;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;
	bool	ShouldEndTracking() const						{	return false;	}

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const;

protected:
	float		m_Red;
	float		m_Green;
	float		m_Blue;
};

//////////////////////////////////////////////////////////////////////////
class CPacketParacuteSmokeFx : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketParacuteSmokeFx(u32 hashName, s32 bneIndex, const Vector3& color);

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PARACUTESMOKE, "Validation of CPacketParacuteSmokeFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PARACUTESMOKE;
	}

private:
	u32					m_HashName;
	s32					m_BoneIndex;
	CPacketVector<3>	m_Color;
};

//////////////////////////////////////////////////////////////////////////
class CPacketRequestPtfxAsset : public CPacketEvent, public CPacketEntityInfo<0>
{
public:

	CPacketRequestPtfxAsset(s32 slot);

	void	Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const	{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_REQUESTPTFXASSET, "Validation of CPacketRequestPtfxAsset Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_REQUESTPTFXASSET;
	}

	s32 GetSlotId() const { return m_Slot; }

protected:	
	s32					m_Slot;
};


//////////////////////////////////////////////////////////////////////////
class CPacketRequestNamedPtfxAsset : public CPacketEvent, public CPacketEntityInfo<0>
{
public:

	CPacketRequestNamedPtfxAsset(u32 hash);

	void	Extract(const CEventInfo<void>&) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const;/*	{ return PRELOAD_SUCCESSFUL; }*/
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool			HasExpired(const CEventInfoBase&) const
	{
		return m_requestedNamedHashes.Find(m_hash) == -1;
	}

	void	Print() const						{}
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{	
		replayAssertf(GetPacketID() == PACKETID_REQUESTNAMEDPTFXASSET, "Validation of CPacketRequestNamedPtfxAsset Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_REQUESTNAMEDPTFXASSET;
	}

	u32 GetHash() const { return m_hash; }

	static bool CheckRequestedHash(u32 hash)
	{
		if(m_requestedNamedHashes.Find(hash) == -1)
		{
			m_requestedNamedHashes.PushAndGrow(hash);
			return true;
		}

		return false;
	}

	static void RemoveRequestedHash(u32 hash)
	{
		int i = m_requestedNamedHashes.Find(hash);
		if(i != -1)
			m_requestedNamedHashes.DeleteFast(i);
	}

	static void ClearAll()
	{
		m_requestedNamedHashes.clear();
	}

protected:	
	u32					m_hash;

	static atArray<u32>	m_requestedNamedHashes;
};


//////////////////////////////////////////////////////////////////////////
class CPacketFocusEntity : public CPacketEvent, public CPacketEntityInfo<1>
{
public:
	CPacketFocusEntity();

	void	Extract(const CEventInfo<CEntity>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<CEntity>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<CEntity>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	NeedsEntitiesForExpiryCheck() const			{ return true; }
	bool	HasExpired(const CEventInfo<CEntity>& eventInfo) const;

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FOCUSENTITY, "Validation of CPacketFocusEntity Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FOCUSENTITY;
	}

private:
};


//////////////////////////////////////////////////////////////////////////
class CPacketFocusPosAndVel : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketFocusPosAndVel(const Vector3& pos, const Vector3& vel);

	void	Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult	Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	NeedsEntitiesForExpiryCheck() const			{ return false; }
	bool	HasExpired(const CEventInfo<void>& eventInfo) const;

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_FOCUSPOSANDVEL, "Validation of CPacketFocusPosAndVel Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_FOCUSPOSANDVEL;
	}

private:

	CPacketVector<3>	m_focusPos;
	CPacketVector<3>	m_focusVel;
};


#endif // GTA_REPLAY

#endif // INC_MISCFXPACKET_H_
