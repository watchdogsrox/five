#ifndef ROPE_PACKET_H
#define ROPE_PACKET_H

#include "Control/replay/ReplayEventManager.h"
#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY


#define MAX_ROPES_TO_TRACK	256

class CReplayRopeManager 
{
public:

	struct sRopePinData
	{
		int ropeId;
		int idx;
		CPacketVector<3> pos;
		bool pin;
		bool rapelling;
	};

	static void Reset();

	static void DeleteRope(int ropeId);
	static void AttachEntites(int ropeId, CEntity* pEntityA, CEntity* pEntityB);
	static CEntity* GetAttachedEntityA(int ropeId);
	static CEntity* GetAttachedEntityB(int ropeId);
	static void DetachEntity(int ropeId, CEntity* pEntity);
	static void CachePinRope(int idx, const Vector3& pos, bool pin, int ropeID, bool rapelling);
	static sRopePinData* GetLastPinRopeData(int ropeID, int idx);
	static void ClearCachedPinRope(int ropeID, int idx);

private:

	struct sRopeData
	{
		int ropeId;
		CEntity* pAttachA;
		CEntity* pAttachB;
		bool attachedA;
		bool attachedB;
	};

	static atFixedArray<sRopeData, MAX_ROPES_TO_TRACK> m_ropeData;
	static atFixedArray<sRopePinData, MAX_ROPES_TO_TRACK> m_ropePinData;

};


struct cachedAttachedData
{
	CPacketVector<3> m_ObjectSpacePosA;
	CPacketVector<3> m_ObjectSpacePosB;
	u16 m_ComponentA;
	u16 m_ComponentB;
	float m_MaxDistance;
	float m_AllowPenetration;
	bool m_UsePushes;
	bool m_HadContraint;
};

struct cachedAddData
{
	CPacketVector<3> pos;
	float length;
	float minLength;
	float maxLength;
	float lengthChangeRate;
	int type;
	bool isBreakable;

	cachedAttachedData m_AttachData;
};

//////////////////////////////////////////////////////////////////////////
class CPacketAddRope : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketAddRope(int ropeID, Vec3V_In pos, Vec3V_In rot, float initLength, float minLength, float maxLength, float lengthChangeRate, int ropeType, int numSections, bool ppuOnly, bool lockFromFront, float timeMultiplier, bool breakable, bool pinned );

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ADDROPE, "Validation of CPacketAddRope Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ADDROPE;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

	bool	ShouldStartTracking() const						{	return true;	}
	bool	Setup(bool)
	{
		m_Reference = 0;
		return true;
	}

	void UpdateMonitorPacket();

private:
	CPacketVector<3>	m_Position;
	CPacketVector<3>	m_Rotation;
	float				m_InitLength;
	float				m_MinLength;
	float				m_MaxLength;
	float				m_LengthChangeRate;
	float				m_TimeMultiplier;
	int					m_RopeType;
	int					m_NumSections;
	u8					m_PpuOnly		:1;
	u8					m_LockFromFront	:1;
	u8					m_Breakable		:1;
	u8					m_Pinned		:1;

	mutable	int			m_Reference;
};


//////////////////////////////////////////////////////////////////////////
class CPacketDeleteRope : public CPacketEventTracked, public CPacketEntityInfo<2, CEntityCheckerAllowNoEntity, CEntityCheckerAllowNoEntity>
{
public:
	CPacketDeleteRope(int ropeID);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DELETEROPE, "Validation of CPacketDeleteRope Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DELETEROPE;
	}

	bool	Setup(bool)
	{
		return true;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}

	bool	ShouldEndTracking() const						{	return true;	}

	//Not saved, used on extract only
	mutable cachedAddData m_AddData;
};



//////////////////////////////////////////////////////////////////////////
class CPacketAttachRopeToEntity : public CPacketEventTracked, public CPacketEntityInfoStaticAware<1,CEntityCheckerAllowNoEntity>
{
public:
	CPacketAttachRopeToEntity(int ropeID, Vec3V_In worldPosition, int componentPart, int heliAttachment, const Vector3 localOffset);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ATTACHROPETOENTITY, "Validation of CPacketAttachRopeToEntity Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_ATTACHROPETOENTITY;
	}

	bool		Setup(bool)
	{
		m_Reference = 0;
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

private:
	CPacketVector<3>	m_WorldPosition;
	int					m_ComponentPart;
	mutable	int			m_Reference;
	
	int					m_HeliAttachment;
	CPacketVector<3>	m_LocalOffset;
};

//////////////////////////////////////////////////////////////////////////
class CPacketAttachEntitiesToRope : public CPacketEventTracked, public CPacketEntityInfoStaticAware<2>
{
public:
	CPacketAttachEntitiesToRope( int ropeID, Vec3V_In worldPositionA, Vec3V_In worldPositionB, int componentPartA, int componentPartB, float constraintLength, float allowedPenetration, 
								 float massInvScaleA, float massInvScaleB, bool usePushes, const char* boneNamePartA, const char* boneNamePartB, int towTruckAttachment, bool bReattachHook);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ATTACHENTITIESTOROPE, "Validation of CPacketAttachRopeToEntity Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_ATTACHENTITIESTOROPE;
	}

	bool	Setup(bool)
	{
		m_Reference = 0;
		return true;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

private:
	CPacketVector<3>	m_WorldPositionA;
	CPacketVector<3>	m_WorldPositionB;
	int					m_ComponentPartA;
	int					m_ComponentPartB;
	float				m_ConstraintLength;
	float				m_AllowedPenetration;
	float				m_MassInvScaleA;
	float				m_MassInvScaleB;
	int					m_TowTruckRopeComponent;
	bool				m_UsePushes;
	char				m_BoneNamePartA[64];
	char				m_BoneNamePartB[64];
	mutable	int			m_Reference;

	//Version 1
	bool				m_bReattachHook;
};

//////////////////////////////////////////////////////////////////////////
class CPacketAttachObjectsToRopeArray : public CPacketEventTracked, public CPacketEntityInfoStaticAware<2>
{
public:
	CPacketAttachObjectsToRopeArray(int ropeID, Vec3V_In worldPositionA, Vec3V_In worldPositionB, float constraintLength, float constraintChangeRate);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ATTACHOBJECTSTOROPEARRAY, "Validation of CPacketAttachObjectsToRopeArray Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_ATTACHOBJECTSTOROPEARRAY;
	}

	bool	Setup(bool)
	{
		m_Reference = 0;
		return true;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}

	CPacketVector<3>	m_WorldPositionA;
	CPacketVector<3>	m_WorldPositionB;
	float				m_ConstraintLength;
	float				m_ConstraintChangeRate;
	mutable	int			m_Reference;
};


//////////////////////////////////////////////////////////////////////////
class CPacketDetachRopeFromEntity : public CPacketEventTracked, public CPacketEntityInfoStaticAware<2, CEntityCheckerAllowNoEntity, CEntityCheckerAllowNoEntity>
{
public:

	CPacketDetachRopeFromEntity(bool detachUsingAttachments = false);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DETACHROPEFROMENTITY, "Validation of CPacketDetachRopeFromEntity Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_DETACHROPEFROMENTITY;
	}

	bool	Setup(bool)
	{
		return true;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}

	bool	ShouldStartTracking() const						{	return true;	}

private:
	bool m_bDetachUsingAttachments;

	//Not saved, used on extract only
	mutable cachedAttachedData m_AttachData;
};

//////////////////////////////////////////////////////////////////////////
class CPacketPinRope : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketPinRope(int idx, Vec3V_In pos, int ropeID);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	static void Pin(int idx, Vec3V pos, int ropeId);

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_PINROPE, "Validation of CPacketPinRope Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_PINROPE;
	}

	bool		Setup(bool)
	{
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}

private:
	CPacketVector<3>	m_Pos;
	int					m_Idx;

	//Version 1
	u8 					m_hasPinData : 1;
	CReplayRopeManager::sRopePinData m_PinData;
};

//////////////////////////////////////////////////////////////////////////
class CPacketRappelPinRope : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketRappelPinRope(int idx, const Vector3& pos, bool pin, int ropeID);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	static void Pin(int idx, bool bPin, Vec3V pos, int ropeId);

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_RAPPELPINROPE, "Validation of CPacketRappelPinRope Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_RAPPELPINROPE;
	}

	bool		Setup(bool)
	{
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}

private:
	CPacketVector<3>	m_Pos;
	int					m_Idx;
	u8					m_bPin : 1;

	//Version 1
	u8 					m_hasPinData : 1;
	CReplayRopeManager::sRopePinData m_PinData;
	
};

//////////////////////////////////////////////////////////////////////////
class CPacketUnPinRope : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketUnPinRope(int idx, int ropeID);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	static void UnPin(int idx, int ropeID);

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_UNPINROPE, "Validation of CPacketUnPinRope Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_UNPINROPE;
	}

	bool		Setup(bool)
	{
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}

private:
	int					m_Idx;

	//Version 1
	u8 					m_hasPinData : 1;
	CReplayRopeManager::sRopePinData m_PinData;
};

//////////////////////////////////////////////////////////////////////////
class CPacketLoadRopeData : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketLoadRopeData(int ropeID, const char * filename);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_LOADROPEDATA, "Validation of CPacketUnPinRope Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_LOADROPEDATA;
	}

	bool		Setup(bool)
	{
		m_Reference = 0;
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

private:
	char m_Filename[64];

	mutable	int	m_Reference;
};

//////////////////////////////////////////////////////////////////////////
class CPacketRopeWinding : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketRopeWinding(float lengthRate, bool windFront, bool unwindFront, bool unwindback);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ROPEWINDING, "Validation of CPacketRopeWinding Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ROPEWINDING;
	}

	bool		Setup(bool)
	{
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}

private:
	float m_LengthRate;
	u8 m_RopeUnwindFront : 1;
	u8 m_RopeUnwindBack : 1;
	u8 m_RopeWindFront : 1;	
};

//////////////////////////////////////////////////////////////////////////
class CPacketRopeUpdateOrder : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketRopeUpdateOrder(int ropeID, int updateOrder);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ROPEUPDATEORDER, "Validation of CPacketRopeUpdateOrder Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ROPEUPDATEORDER;
	}
	
	bool		Setup(bool)
	{
		m_Reference = 0;
		return true;
	}

	bool HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

private:
	int m_UpdateOrder;
	mutable	int	m_Reference;
	
};

//////////////////////////////////////////////////////////////////////////
/// OLD VERSION TO REMOVE !!
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CPacketAddRope_OLD : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketAddRope_OLD(int ropeID, Vec3V_In pos, Vec3V_In rot, float initLength, float minLength, float maxLength, float lengthChangeRate, int ropeType, int numSections, bool ppuOnly, bool lockFromFront, float timeMultiplier, bool breakable, bool pinned );

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	eShouldExtract	ShouldExtract(u32 flags, const u32 packetFlags, bool isFirstFrame) const;

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ADDROPE_OLD, "Validation of CPacketAddRope_OLD Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ADDROPE_OLD;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const;

	bool	ShouldStartTracking() const						{	return true;	}
	bool	Setup(bool)
	{
		m_Reference = 0;
		return true;
	}

private:
	CPacketVector<3>	m_Position;
	CPacketVector<3>	m_Rotation;
	float				m_InitLength;
	float				m_MinLength;
	float				m_MaxLength;
	float				m_LengthChangeRate;
	float				m_TimeMultiplier;
	int					m_RopeType;
	int					m_NumSections;
	u8					m_PpuOnly		:1;
	u8					m_LockFromFront	:1;
	u8					m_Breakable		:1;
	u8					m_Pinned		:1;

	mutable	int			m_Reference;
};

//////////////////////////////////////////////////////////////////////////
class CPacketDeleteRope_OLD : public CPacketEventTracked, public CPacketEntityInfo<0>
{
public:
	CPacketDeleteRope_OLD();

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DELETEROPE_OLD, "Validation of CPacketDeleteRope_OLD Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DELETEROPE_OLD;
	}

	bool	Setup(bool)
	{
		return true;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}
};

//////////////////////////////////////////////////////////////////////////
class CPacketDetachRopeFromEntity_OLD : public CPacketEventTracked, public CPacketEntityInfoStaticAware<1>
{
public:
	struct cachedAttachedData_OLD
	{
		CPacketVector<3> m_ObjectSpacePosA;
		CPacketVector<3> m_ObjectSpacePosB;
		phInst* m_InstanceA;
		phInst* m_InstanceB;
		u16 m_ComponentA;
		u16 m_ComponentB;
		float m_MaxDistance;
		float m_AllowPenetration;
		bool m_UsePushes;
	};
	CPacketDetachRopeFromEntity_OLD(bool detachUsingAttachments = false);

	void	Extract(CTrackedEventInfo<ptxEffectRef>& pEventInfo) const;
	ePreloadResult	Preload(CTrackedEventInfo<ptxEffectRef>&) const { return PRELOAD_SUCCESSFUL; };
	ePreplayResult		Preplay(const CTrackedEventInfo<ptxEffectRef>&) const			{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEventTracked::PrintXML(handle);	CPacketEntityInfoStaticAware::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DETACHROPEFROMENTITY_OLD, "Validation of CPacketDetachRopeFromEntity_OLD Failed!, 0x%08X", GetPacketID());
		return CPacketEventTracked::ValidatePacket()  && CPacketEntityInfoStaticAware::ValidatePacket() && GetPacketID() == PACKETID_DETACHROPEFROMENTITY_OLD;
	}

	bool	Setup(bool)
	{
		return true;
	}

	bool	HasExpired(const CTrackedEventInfo<ptxEffectRef>&) const
	{
		return true;
	}
private:
	bool m_bDetachUsingAttachments;

	//Not saved, used on extract only
	mutable cachedAttachedData_OLD m_AttachData;
};

#endif // GTA_REPLAY

#endif // GLASS_PACKET_H
