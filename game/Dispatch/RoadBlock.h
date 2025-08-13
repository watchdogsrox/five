// Title	:	RoadBlock.h
// Purpose	:	Keeps track of road blocks as discrete objects
// Started	:	1/20/2012

#ifndef INC_ROADBLOCK_H_
#define INC_ROADBLOCK_H_

// Rage headers
#include "atl/array.h"
#include "entity/archetypemanager.h"
#include "fwtl/pool.h"
#include "fwutil/Flags.h"
#include "fwvehicleai/pathfindtypes.h"

// Game headers
#include "scene/RegdRefTypes.h"

class CRoadBlock
{

public:

	FW_REGISTER_CLASS_POOL(CRoadBlock);

public:

	enum RoadBlockType
	{
		RBT_None,
		RBT_Vehicles,
		RBT_SpikeStrip
	};

	enum ConfigFlags
	{
		CF_DisperseIfTargetIsNotWanted	= BIT0,
	};

private:

	enum RunningFlags
	{
		RF_HasDispersed	= BIT0,
	};

protected:

	CRoadBlock(const CPed* pTarget, float fMinTimeToLive);
	virtual ~CRoadBlock();
	
public:

	struct CreateInput
	{
		CreateInput(CNodeAddress atNode, CNodeAddress towardsNode, u32 uMaxVehs)
		: m_AtNode(atNode)
		, m_TowardsNode(towardsNode)
		, m_vPosition(VEC3_ZERO)
		, m_vDirection(0.0f, 0.0f)
		, m_aVehModelIds()
		, m_aPedModelIds()
		, m_aObjModelIds()
		, m_uMaxVehs(uMaxVehs)
		, m_fVehSpacing(0.5f)
		, m_fPedSpacing(0.5f)
		, m_fObjSpacing(0.0f)
		{}
		
		CreateInput(const Vector3& vPosition, const Vector2& vDirection, u32 uMaxVehs)
		: m_AtNode()
		, m_TowardsNode()
		, m_vPosition(vPosition)
		, m_vDirection(vDirection)
		, m_aVehModelIds()
		, m_aPedModelIds()
		, m_aObjModelIds()
		, m_uMaxVehs(uMaxVehs)
		, m_fVehSpacing(0.5f)
		, m_fPedSpacing(0.5f)
		, m_fObjSpacing(0.0f)
		{}

		CNodeAddress		m_AtNode;
		CNodeAddress		m_TowardsNode;
		Vector3				m_vPosition;
		Vector2				m_vDirection;
		atArray<fwModelId>	m_aVehModelIds;
		atArray<fwModelId>	m_aPedModelIds;
		atArray<fwModelId>	m_aObjModelIds;
		u32					m_uMaxVehs;
		float				m_fVehSpacing;
		float				m_fPedSpacing;
		float				m_fObjSpacing;
	};
	
	struct CreateFromTypeInput
	{
		CreateFromTypeInput(RoadBlockType nType, const CPed* pTarget, float fMinTimeToLive)
			: m_nType(nType)
			, m_pTarget(pTarget)
			, m_fMinTimeToLive(fMinTimeToLive)
		{

		}

		RoadBlockType	m_nType;
		const CPed*		m_pTarget;
		float			m_fMinTimeToLive;
	};
	
	struct PreCreateOutput
	{
		PreCreateOutput()
		{}
		
		Vector3	m_vLeftCurb;
		Vector3	m_vRightCurb;
		Vector3	m_vLeftCurbToRightCurb;
		Vector3	m_vDirectionLeftCurbToRightCurb;
		Vector3	m_vDirectionFront;
		Vector3	m_vDirectionBack;
	};

public:

			fwFlags8&	GetConfigFlags()			{ return m_uConfigFlags; }
	const	fwFlags8	GetConfigFlags()	const	{ return m_uConfigFlags; }
			Vec3V_Out	GetPosition()		const	{ return m_vPosition; }

public:

	void SetPosition(Vec3V_In vPosition) { m_vPosition = vPosition; }
	
public:
	
	u32 GetEntitiesCount() const
	{
		return (GetVehCount() + GetPedCount() + GetObjCount());
	}
	
	CObject* GetObj(const u32 uIndex)
	{
		return m_aObjs[uIndex];
	}

	u32 GetObjCount() const
	{
		return m_aObjs.GetCount();
	}
	
	CPed* GetPed(const u32 uIndex)
	{
		return m_aPeds[uIndex];
	}
	
	u32 GetPedCount() const
	{
		return m_aPeds.GetCount();
	}
	
	CVehicle* GetVeh(const u32 uIndex)
	{
		return m_aVehs[uIndex];
	}
	
	u32 GetVehCount() const
	{
		return m_aVehs.GetCount();
	}

	bool HasDispersed() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_HasDispersed));
	}

public:

	bool Despawn(bool bForce = false);
	bool HasEntity(const CEntity& rEntity) const;
	bool IsEmpty() const;
	bool RemoveVeh(CVehicle& rVehicle);
	void SetPersistentOwner(bool bPersistentOwner);

public:
	
	static CRoadBlock*	CreateFromType(const CreateFromTypeInput& rTypeInput, const CreateInput& rInput);
	static CRoadBlock*	Find(const CEntity& rEntity);
	static void			OnRespawn(const CPed& rTarget);
	static bool			PreCreate(const CreateInput& rInput, PreCreateOutput& rOutput);
	
public:
	
	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned initMode);
	static void Update();
	
protected:

	bool		CalculateDimensionsForModel(fwModelId iModelId, float& fWidth, float& fLength, float& fHeight) const;
	fwModelId	ChooseRandomModel(const atArray<fwModelId>& aModelIds) const;
	fwModelId	ChooseRandomModelForSpace(const atArray<fwModelId>& aModelIds, float fSpace, float& fWidth, float& fLength, float& fHeight) const;
	
	struct CreateTransformInput
	{
		CreateTransformInput(const Vector3& vPosition, const Vector3& vForward, const fwModelId iModelId)
		: m_vPosition(vPosition)
		, m_vForward(vForward)
		, m_iModelId(iModelId)
		{}
		
		Vector3		m_vPosition;
		Vector3		m_vForward;
		fwModelId	m_iModelId;
	};
	bool CreateTransform(const CreateTransformInput& rInput, Matrix34& mTransform) const;
	
	struct CreateObjInput : CreateTransformInput
	{
		CreateObjInput(const Vector3& vPosition, const Vector3& vForward, const fwModelId iModelId)
		: CreateTransformInput(vPosition, vForward, iModelId)
		, m_bScanForInterior(false)
		{}

		bool m_bScanForInterior;
	};
	CObject* CreateObj(const CreateObjInput& rInput);
	
	struct CreatePedInput : CreateTransformInput
	{
		CreatePedInput(const Vector3& vPosition, const Vector3& vForward, const fwModelId iModelId)
		: CreateTransformInput(vPosition, vForward, iModelId)
		, m_bScanForInterior(false)
		{}

		bool m_bScanForInterior;
	};
	CPed* CreatePed(const CreatePedInput& rInput);
	
	struct CreateVehInput : CreateTransformInput
	{
		CreateVehInput(const Vector3& vPosition, const Vector3& vForward, const fwModelId iModelId)
		: CreateTransformInput(vPosition, vForward, iModelId)
		, m_pExceptionForCollision(NULL)
		, m_bScanForInterior(false)
		{}
		
		const CEntity*	m_pExceptionForCollision;
		bool			m_bScanForInterior;
	};
	CVehicle* CreateVeh(const CreateVehInput& rInput);
	
protected:

	virtual void Update(float fTimeStep);
	
private:

	bool	CanDespawn() const;
	bool	CanDespawnObjs() const;
	bool	CanDespawnPeds() const;
	bool	CanDespawnVehs() const;
	void	Disperse();
	void	DisperseObjs();
	void	DispersePeds();
	void	DisperseVehs();
	bool	HasEntityStrayed(const CEntity& rEntity) const;
	void	MakeVehsDriveAway();
	void	ReleaseObj(CObject& rObj) const;
	void	ReleaseObjs();
	void	ReleasePed(CPed& rPed) const;
	void	ReleasePeds();
	void	ReleaseVeh(CVehicle& rVehicle) const;
	void	ReleaseVehs();
	bool	ShouldDisperse() const;
	bool	ShouldUpdateStrayEntities() const;
	bool	TryToDespawn();
	bool	TryToDespawnObjs();
	bool	TryToDespawnPeds();
	bool	TryToDespawnVehs();
	void	UpdateForceOtherVehiclesToStop();
	void	UpdateStrayEntities();
	void	UpdateStrayObjs();
	void	UpdateStrayPeds();
	void	UpdateStrayVehs();
	void	UpdateTimers(float fTimeStep);
	bool	WillVehBeDrivenAway(const CVehicle& rVehicle) const;
	
private:

	virtual bool Create(const CreateInput& rInput, const PreCreateOutput& rPreOutput) = 0;
	
protected:

	static const int sMaxVehs = 4;
	static const int sMaxPeds = 4;
	static const int sMaxObjs = 5;
	
protected:

	atFixedArray<RegdVeh, sMaxVehs>	m_aVehs;
	atFixedArray<RegdPed, sMaxPeds>	m_aPeds;
	atFixedArray<RegdObj, sMaxObjs>	m_aObjs;
	Vec3V							m_vPosition;
	RegdConstPed					m_pTarget;
	float							m_fMinTimeToLive;
	float							m_fTotalTime;
	float							m_fTimeSinceLastStrayEntitiesUpdate;
	float							m_fInitialDistToTarget;
	float							m_fTimeOutOfRange;
	fwFlags8						m_uConfigFlags;
	fwFlags8						m_uRunningFlags;
	
private:

	static float	ms_fTimeSinceLastDespawnCheck;
	
};

class CRoadBlockVehicles : public CRoadBlock
{

	friend class CRoadBlock;

private:

	CRoadBlockVehicles(const CPed* pTarget, float fMinTimeToLive);
	virtual ~CRoadBlockVehicles();
	
private:

	virtual bool Create(const CreateInput& rInput, const PreCreateOutput& rPreOutput);

};

class CRoadBlockSpikeStrip : public CRoadBlock
{

	friend class CRoadBlock;

private:

	CRoadBlockSpikeStrip(const CPed* pTarget, float fMinTimeToLive);
	virtual ~CRoadBlockSpikeStrip();
	
private:

	bool ShouldPopTires() const;

private:

	virtual bool Create(const CreateInput& rInput, const PreCreateOutput& rPreOutput);
	virtual void Update(float fTimeStep);
	
private:

	bool	m_bPopTires;

};

#endif // INC_ROADBLOCK_H_
