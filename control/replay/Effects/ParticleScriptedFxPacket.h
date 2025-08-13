//=====================================================================================================
// name:		ParticleScriptedFx.h
// description:	Scripted Fx particle replay packet.
//=====================================================================================================

#ifndef INC_SCRIPTFXPACKET_H_
#define INC_SCRIPTFXPACKET_H_

#include "Control/replay/ReplaySettings.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control\replay\Effects\ParticlePacket.h"
#include "vector\matrix34.h"

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS

//=====================================================================================================
class CPacketScriptedFx : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketScriptedFx(u32 uPfxHash, const Matrix34& rMatrix, float fScale = 1.0f, bool bUseParent = false, u32 uParentID = 0, u32 uBoneTag = 0);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SCRIPTEDFX, "Validation of CPacketScriptedFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SCRIPTEDFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StoreMatrix(const Matrix34& rMatrix);
	void LoadMatrix(Matrix34& rMatrix) const;
	//-------------------------------------------------------------------------------------------------
	u32			m_pfxHash;
	union
	{
		struct 
		{
			u32 m_uParentID;
		};
		float	m_fScale;
	};
	union
	{
		struct 
		{
			u16 m_uBoneTag;
			u16 m_uCheck;
		};
		u32		m_ScriptedQuaternion;
	};

	float	m_Position[3];
};

//=====================================================================================================
class CPacketScriptedUpdateFx : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketScriptedUpdateFx(u32 uPfxHash, const Matrix34& rMatrix);
	CPacketScriptedUpdateFx(u32 uPfxHash, char* szEvolutionName, float fEvolutionValue);
	CPacketScriptedUpdateFx(u32 uPfxHash, u32 uColor);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SCRIPTEDUPDATEFX, "Validation of CPacketScriptedUpdateFx Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SCRIPTEDUPDATEFX;
	}

protected:
	//-------------------------------------------------------------------------------------------------
	void StoreMatrix(const Matrix34& rMatrix);
	void LoadMatrix(Matrix34& rMatrix) const;
	//-------------------------------------------------------------------------------------------------
	u32			m_pfxHash;
	union
	{
		struct  
		{
			// Offset update
			u32		m_ScriptedQuaternion;
			float	m_Position[2];
		};
		char		m_EvolutionName[12];
	};
	union
	{
		struct 
		{
			u32	m_uColor;
		};
		float	m_EvolutionValueOrPositionZ;	
	};
};

//=====================================================================================================
class CPacketBuildingSwap : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketBuildingSwap(float fX, float fY, float fZ, float fRadius, u32 uOldHash, u32 uNewHash);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void Reset() const;

	bool ShouldInvalidate() const			{	return false;	}
	void Invalidate()						{ 	}

	void Print() const						{}
	void PrintXML(FileHandle handle) const	{ (void)handle; }
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SCRIPTEDBUILDINGSWAP, "Validation of CPacketBuildingSwap Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SCRIPTEDBUILDINGSWAP;
	}

protected:
	float	m_Position[3];
	float	m_Radius;
	u32		m_uOldHash;
	u32		m_uNewHash;
};


#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS


#endif // GTA_REPLAY

#endif // INC_SCRIPTFXPACKET_H_
