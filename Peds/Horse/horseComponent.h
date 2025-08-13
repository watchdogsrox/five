
#ifndef HRS_COMPONENT_H
#define HRS_COMPONENT_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "parser/manager.h"

#include "Peds\Horse\horseAgitation.h"
#include "Peds\Horse\horseBrake.h"
#include "Peds\Horse\horseFreshness.h"
#include "Peds\Horse\horseObstacleAvoidance.h"
#include "Peds\Horse\horseSpeed.h"
#include "scene/RegdRefTypes.h"

class hrsSimTune;
class CDraftVeh;
class CPed;

namespace rage
{
	class phAsyncShapeTestMgr;
	class ropeInstance;


struct CReinsAttachData
{
	Vec3V			m_Offset;	
	int				m_BoneIndex;
	int				m_VertexIndex;
	RegdConstEnt	m_Entity;	
	atString		m_BoneName;

	PAR_SIMPLE_PARSABLE;
};

struct CReinsCollisionData
{	
	ScalarV			m_Rotation;
	float			m_CapsuleRadius;
	float			m_CapsuleLen;
	int				m_BoneIndex;
	int				m_BoundIndex;
	atString		m_BoneName;
	RegdConstEnt	m_Entity;	
	phBound*		m_BoundPtr;
	bool			m_Enabled;

	PAR_SIMPLE_PARSABLE;
};


class CReins : datBase
{	
public:
	
	static CReins* GetDefaultReins();

	CReins( const CReins& copyme )
		: m_CompositeBoundPtr(NULL)
		, m_NumBindFrames(0)
		, m_HasRider(false)
		, m_RopeInstance(NULL)
		, m_IsRiderAiming(false)
#if __BANK
		, m_RAGGroup(NULL) 
#endif
	{
		m_Name				= copyme.m_Name;		
		m_Length			= copyme.m_Length;
		m_NumSections		= copyme.m_NumSections;
		m_UseSoftPinning	= copyme.m_UseSoftPinning;

		m_AttachData.Resize( copyme.m_AttachData.GetCount() );
		m_CollisionData.Resize( copyme.m_CollisionData.GetCount() );
		m_BindPos.Resize( copyme.m_BindPos.GetCount() );
		m_BindPos2.Resize( copyme.m_BindPos2.GetCount() );
		m_SoftPinRadiuses.Resize( copyme.m_SoftPinRadiuses.GetCount() );

		m_AttachData		= copyme.m_AttachData;
		m_CollisionData		= copyme.m_CollisionData;
		m_BindPos			= copyme.m_BindPos;
		m_BindPos2			= copyme.m_BindPos2;
		m_SoftPinRadiuses	= copyme.m_SoftPinRadiuses;
	}

	CReins()
		: m_CompositeBoundPtr(NULL)
		, m_NumSections(32)
		, m_NumBindFrames(0)
		, m_Length(2.7f)
		, m_Name("horseReins")
		, m_UseSoftPinning(false)
		, m_HasRider(false)
		, m_IsRiderAiming(false)
		, m_RopeInstance(NULL)
		, m_ExpectedAttachState(0)
#if __BANK
		, m_RAGGroup(NULL) 
#endif
	{}
	~CReins();

	bool Init();

	static void InitClass();
	
	void AttachToBone(const CEntity* entity, int index);
	void AttachToBones(const CEntity* entity);	
	void AttachBound(const CEntity* entity, int index);
	void AttachBounds(const CEntity* entity);

	void Update();
	void UpdateToBind(atArray<Vec3V>& pose);
	void UpdateSoftPinning(bool useSoftPinning);
	void Hide();
	void AttachTwoHands(const CEntity* entity);
	void AttachOneHand(const CEntity* entity);
	void Detach(const CEntity* entity);

	void ReserveAttachData(const int arrayCapacity);
	void ReserveCollisionData(const int arrayCapacity);
	void SetHasRider(bool hasRider) { m_HasRider = hasRider; }
	void SetIsRiderAiming(bool isRiderAiming) { m_IsRiderAiming = isRiderAiming; }
	bool GetIsRiderAiming() const { return m_IsRiderAiming; }
	int  GetExpectedAttachState() const { return m_ExpectedAttachState; }
	void SetExpectedAttachState(int expectedAttachState) { m_ExpectedAttachState = expectedAttachState; }
	const ropeInstance* GetRopeInstance() const { return m_RopeInstance; }

	static const char* sm_tuneFilePath;
	void Load(const char* pFileName);
	void Reload(const char* pFileName);
	void Save();

	CReinsAttachData& GetAttachData(int idx) { return m_AttachData[idx]; }
	void UnpinVertex(int idx);
	void SetNumBindFrames(int numBindFrames) { Assert( numBindFrames > -1 && numBindFrames < 29 ); m_NumBindFrames = numBindFrames; }
	atArray<Vec3V>& GetDefaultBindPose() { return m_BindPos; }

	static Mat34V_Out GetObjectMtx(const CEntity *entity, int boneIndex);

#if __BANK
	void AddWidgets();
	void ToggleBound(void*);
	void UpdateBound(void*);
#endif

private:
#if __BANK
	bkGroup*	m_RAGGroup;
#endif
	
	const char* m_Name;
	ropeInstance* m_RopeInstance;
	phBoundComposite* m_CompositeBoundPtr;
	
	float m_Length;
	int m_NumSections;
	int m_NumBindFrames;
	atArray<CReinsAttachData>	 m_AttachData;	
	atArray<CReinsCollisionData> m_CollisionData;
	atArray<Vec3V>	m_BindPos;
	atArray<Vec3V>	m_BindPos2;
	atArray<float>	m_SoftPinRadiuses;

	int	 m_ExpectedAttachState;	// 0: no hands, 1: one hand, 2: two hands
	bool m_UseSoftPinning;
	bool m_HasRider;
	bool m_IsRiderAiming;


	PAR_SIMPLE_PARSABLE;
};


}

////////////////////////////////////////////////////////////////////////////////

class CHorseComponent : datBase
{
public:	

	CHorseComponent();
	~CHorseComponent();

	void Init(CPed* owner);
	void Shutdown();

	static phAsyncShapeTestMgr * GetAsyncShapeTestMgr();
	static void SetAsyncShapeTestMgr(phAsyncShapeTestMgr * mgr);

	void Reset();
	void Update();
	
	void ResetGaits(); 
	void UpdateInput(bool bSpur, float fTimeSinceSpur, bool bHardBrake, float fBrake, bool bMaintainSpeed, bool bAiming, Vector2& vStick, float fMatchSpeed);
	bool AdjustHeading(float & fInOutGoalHeading);

	// PURPOSE: Inform the individual control subsystems of our mount state
	// (mounted or not, and whether the rider is the main player).
	void UpdateMountState();

	void HandleOnMount(float fRiderSpeedNorm);
	void HandleCollision();	
	void InitReins();
	void HideReins();

	bool GetSpurredThisFrame() const
	{	return m_SpurredThisFrame;	}

	// Spur but don't animate it, lasts until next update without a spur  
	void SetSuppressSpurAnim() {m_bSuppressSpurAnim = true;}

	bool GetSuppressSpurAnim() const
	{	return m_bSuppressSpurAnim;	}

	//resets each frame
	void SetForceHardBraking() {
		m_ForceHardBraking = true;
	}

	void ApplyAgitationScale(float f) {
		m_AgitationCtrl.ApplyAgitationFactor(f);
	}

	void SetCurrentSpeed(float s) {
		m_SpeedCtrl.SetCurrSpeed(s);
	}

	void SetMaxSpeedWhileAiming(float s) {
		m_SpeedCtrl.SetMaxSpeedWhileAiming(s);
	}	

	CPed* GetRider();
	const CPed* GetRider() const;

	CPed* GetLastRider();
	const CPed* GetLastRider() const;

	void SetLastRider(CPed* pPed);

	// PURPOSE:
	//	Find the correct hrsSimTune for this horse, taking into account	
	void ResetSimTune();

	const hrsSimTune *GetSimTune() const
	{	return m_pHrsSimTune;	}

	hrsSpeedControl & GetSpeedCtrl() { return m_SpeedCtrl; }
	const hrsSpeedControl & GetSpeedCtrl() const { return m_SpeedCtrl; }
	const hrsFreshnessControl & GetFreshnessCtrl() const { return m_FreshnessCtrl; }	
	const hrsAgitationControl & GetAgitationCtrl() const { return m_AgitationCtrl; }
	const hrsBrakeControl & GetBrakeCtrl() const { return m_BrakeCtrl; }	
	const hrsObstacleAvoidanceControl & GetAvoidanceCtrl() const { return m_ObstacleAvoidanceCtrl; }	
	
	CReins* GetReins() const { return m_Reins; }
#if __BANK
	static void AddStaticWidgets(bkBank &bank);
	// PMD - Not sure why but this seems to be dead code
#if 0
	virtual void AddWidgets(bkBank &bank);
#endif
#endif

	void SetReplicatedSpeedFactor(float factor) { m_ReplicatedSpeedFactor = factor; }
	float GetReplicatedSpeedFactor() { return m_ReplicatedSpeedFactor; }

	//HORIBBLE HACK FOR PLAYER CONTORL DURING FOLLOW
	void ClearHeadingOverride() { m_fHeadingOverride = LARGE_FLOAT; }
	void SetHeadingOverride(float fValue, float fStrength) { m_fHeadingOverride = fValue; m_fHeadingOverrideStrength=fStrength;}
	float GetHeadingOverride() const { return m_fHeadingOverride; }
	float GetHeadingOverrideStrength() const { return m_fHeadingOverrideStrength; }
	bool HasHeadingOverride() const { return m_fHeadingOverride < LARGE_FLOAT; }

	bool GetHasBeenOwnedByPlayer() const { return m_bHasBeenOwnedByPlayer; }
	void SetHasBeenOwnedByPlayer(bool b) { m_bHasBeenOwnedByPlayer = b; }

	void SetupMissionState();

	void SetStirrupEnable(bool bRight, bool bEnable) {if (bRight) m_bRightStirrupEnable=bEnable; else m_bLeftStirrupEnable=bEnable;} 
	float& GetLeftStirrupBlendPhase() {return m_fLeftStirrupBlendPhase;}
	bool GetLeftStirrupEnable() {return m_bLeftStirrupEnable;}
	float& GetRightStirrupBlendPhase() {return m_fRightStirrupBlendPhase;}
	bool GetRightStirrupEnable() {return m_bRightStirrupEnable;}

protected:

	void OnUpdateMountState();
	void AutoFlee();

	void AttachTuning(const hrsSimTune & rTune);
	void DetachTuning();	

	void UpdateFreshness(const float fCurrSpdMS, const bool bSpurredThisFrame);
	void UpdateAgitation(const bool bSpurredThisFrame, const float fFreshness);
	
	void UpdateObstacleAvoidance(const float fCurrSpeedMS, const float fStickTurn, const bool bOnMountCheck = false);
	void UpdateBrake(const float fSoftBrake, const bool bHardBrake, const float fMoveBlendRatio);	
	void UpdateSpeed(float fCurrMvrSpdMS, const bool bSpurredThisFrame, const bool bMaintainSpeed, const float fTimeSinceSpur, const Vector2 & vStick, const bool bIsAiming, float speedMatchSpeed);
	float ComputeMaxSpeedNorm() const;
	
	const hrsSimTune * m_pHrsSimTune;
	hrsFreshnessControl m_FreshnessCtrl;
	hrsSpeedControl m_SpeedCtrl;	
	hrsAgitationControl m_AgitationCtrl;
	hrsObstacleAvoidanceControl m_ObstacleAvoidanceCtrl;
	
	hrsBrakeControl m_BrakeCtrl;	

	CReins* m_Reins;

	// PURPOSE:	I added this, so that the action tree signals can be a function
	//			of the state of CHorseComponent. Other signals, like the
	//			SoftBrake, are already stored by the sub-objects.
	// NOTES:	Feel free to move this into hrsFreshnessControl, hrsAgitationControl,
	//			or hrsSpeedControl if you think it better belongs there. /FF
	bool				m_SpurredThisFrame;
	bool				m_bSuppressSpurAnim;

	bool				m_ForceHardBraking;
	
	// Owner ped - not a RegdPed as it is owned by CPed
	CPed* m_pPed;

	// It is more efficient to just replicate this factor than to replicate all the data required to compute it
	float m_ReplicatedSpeedFactor;

	// TMS: Hack after fighting the task system
	float m_fHeadingOverride;
	float m_fHeadingOverrideStrength;

	u32 m_LastTimeCouldNotFleeInMillis;
	bool m_WasEverMounted;
	bool m_bHasBeenOwnedByPlayer;

	//Stirrup data
	float		m_fRightStirrupBlendPhase;
	float		m_fLeftStirrupBlendPhase;	
	bool		m_bRightStirrupEnable;
	bool		m_bLeftStirrupEnable;

#if __BANK
	// The name of the attached tuning:
	enum { kTuneNameMaxLen = 64 };
	char m_AttachedTuneName[kTuneNameMaxLen];
#endif

	static u32 ms_MPAutoFleeBeforeMountedWaitTime;
	static u32 ms_MPAutoFleeAfterMountedWaitTime;
	static bool ms_MPAutoFleeBeforeMountedEnabled;
	static bool ms_MPAutoFleeAfterMountedEnabled;
	static bool ms_MPAutoFleeAfterMountedMaintainMBR;
};

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE

#endif
