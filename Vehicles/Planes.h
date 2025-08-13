// Started	:	10/04/2003
//
//
//
#ifndef _PLANE_H_
#define _PLANE_H_

#include "vehicles\automobile.h"
#include "vehicles\landinggear.h"
#include "vehicles\Propeller.h"
#include "vehicles\train.h"

#include "network\objects\entities\netObjPlane.h"
#include "System/PadGestures.h"						// for define

#define PLANE_TURNING_CIRCLE_TAXIIING (5.0f)
#define PLANE_TURNING_CIRCLE_FLYING (200.0f)

class CAircraftDamageBase
{
public:
	CAircraftDamageBase();
	virtual ~CAircraftDamageBase();

	enum {INVALID_SECTION = -1};

	// Main entry point for all plane damage
	void ApplyDamageToPlane(CPlane* pParent, CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPos, const Vector3& vecNorm, const Vector3& vecDirn, int nComponent, phMaterialMgr::Id nMaterialId=0, int nPieceIndex=-1, const bool bFireDriveby=false, const bool bLandingDamage = false, float fDamageRadius = 0.0f);
	void InitCompositeBound(CPlane* pParent);
	static void BreakOffAboveGroupRecursive(int nGroup, fragInst* pFragInst);

	bool HasSectionBrokenOff(const CPlane* pParent, int iSection) const;
	CEntity* GetLastDamageInflictor() const { return m_pLastDamageInflictor; }

	virtual float GetSectionHealth(int iSection) const = 0;
	virtual float GetSectionMaxHealth(int iSection) const = 0;
	virtual void ApplyDamageToSection(CPlane* pParent, int iSection, int iComponent, float fDamage, eDamageType nDamageType, const Vector3 &vHitPos, const Vector3 &vHitNorm) = 0;
	virtual void ApplyDamageToChassis(CPlane* pParent, float fDamage, eDamageType nDamageType, int nComponent, const Vector3 &vHitPos);
	virtual void PostBreakOffSection(CPlane* pParent, int nSection, eDamageType nDamageType);
	virtual float GetInstantBreakOffDamageThreshold(int nSection) const = 0;
	virtual int GetFirstBreakablePart() const = 0;
	virtual int GetNumOfBreakablePart() const = 0;
	virtual void FindAllHitSections(int &nNumHitSections, int *pHitSectionIndices, const CPlane *pParent, eDamageType nDamageType, const Vector3& vecPos, int nComponent, float fDamageRadius) = 0;
	virtual bool BreakOffComponent(CEntity* inflictor, CPlane* pParent, int iComponent, bool bNetworkAllowed = false) = 0;
	void BreakOffSection(CPlane* pParent, int nHitSection, bool bDisappear = false, bool bNetworkAllowed = false);
protected:

	// Child index is 1:1 match to composite bound component
	virtual int GetSectionFromChildIndex(const CPlane* pParent, int iChildIndex) = 0;
	virtual int GetSectionFromHierarchyId(eHierarchyId iComponent) = 0;
	
	// This will give the first hierarchy id associated with a damage section
	// In theory multiple components could be hooked up to one damage section so be warned
	virtual eHierarchyId GetHierarchyIdFromSection(int nAircraftSection) const = 0;

	RegdEnt m_pLastDamageInflictor;
};

class CAircraftDamage : public CAircraftDamageBase
{
public:
	CAircraftDamage();
	~CAircraftDamage();

	enum {NUM_DAMAGE_FLAMES = 20};

	// Use a separate enum list to the hierarchy ids so that we could
	// link multiple hierarchy ids to one damage type
	enum eAircraftSection
	{
		INVALID_SECTION = -1,
		// Main sections
		// These can break off when health runs out
		WING_L,
		WING_R,
		TAIL,

		// Damage to these components affects handling
		ENGINE_L,
		ENGINE_R,
		ELEVATOR_L,
		ELEVATOR_R,
		AILERON_L,
		AILERON_R,
		RUDDER,
		RUDDER_2,
		AIRBRAKE_L,
		AIRBRAKE_R,

		NUM_DAMAGE_SECTIONS
	};

	// Query damage on sections
	virtual float GetSectionHealth(int iSection) const;
	virtual float GetSectionMaxHealth(int iSection) const;
	float GetSectionHealthAsFractionActual(eAircraftSection iSection) const;
	float GetSectionHealthAsFraction(eAircraftSection iSection) const;

	virtual float GetInstantBreakOffDamageThreshold(int nSection) const;
	virtual int GetFirstBreakablePart() const;
	virtual int GetNumOfBreakablePart() const;
	virtual void FindAllHitSections(int &nNumHitSections, int *pHitSectionIndices, const CPlane *pParent, eDamageType nDamageType, const Vector3& vecPos, int nComponent, float fDamageRadius);

	// This will give the first hierarchy id associated with a damage section
	// In theory multiple components could be hooked up to one damage section so be warned
	virtual eHierarchyId GetHierarchyIdFromSection(int nAircraftSection) const;

	// Specifically damage a section
	virtual void SetSectionHealth(int iSection, float fHealth);
	void SetSectionHealthAsFraction(eAircraftSection iSection, float fHealth);
	virtual void ApplyDamageToSection(CPlane* pParent, int iSection, int iComponent, float fDamage, eDamageType nDamageType, const Vector3 &vHitPos, const Vector3 &vHitNorm);
	virtual void ApplyDamageToChassis(CPlane* pParent, float fDamage, eDamageType nDamageType, int nComponent, const Vector3 &vHitPos);
	void ComputeFlameDamageAndCreatNewFire(CPlane* pParent, int iSection, int nComponent, float &fDamage, const Vector3 &vHitPos);
	void UpdateBuoyancyOnPartBreakingOff(CPlane* pParent, int nComponentID);
	virtual void PostBreakOffSection(CPlane* pParent, int nSection, eDamageType nDamageType);

	void ProcessPhysics(CPlane* pParent, float fTimeStep);
	void ProcessVfx(CPlane* pParent);

	// Fly model hooks in to these multipliers for handling
	float GetRollMult(CPlane* pParent) const; 
	float GetPitchMult(CPlane* pParent) const;
	float GetYawMult(CPlane* pParent) const;
	float GetLiftMult(CPlane* pParent) const;
	float GetThrustMult(CPlane* pParent) const;

	void Fix();
	void BlowingUpVehicle(CPlane *pParent);

#if __BANK
	void DisplayDebugOutput(CPlane* pParent) const;
	static bool ms_bDrawFlamePosition;
#endif

	//Damage flame tunings
	static float ms_fMinCausingFlameDamage;
	static float ms_fMaxCausingFlameDamage;
	static float ms_fMinFlameLifeSpan;
	static float ms_fMaxFlameLifeSpan;
	static float ms_fFlameDamageMultiplier;
	static float ms_fFlameMergeableDistance;

	struct DamageFlame
	{
		Vector3 vLocalOffset;
		int iSectionIndex;
		int iEffectIndex;
		float fLifeSpan;
		FW_REGISTER_CLASS_POOL(CAircraftDamage::DamageFlame);
	};

	DamageFlame *NewFlame();
	DamageFlame *FindCloseFlame(int iSection, const Vector3 &vHitPosLocal);

	float ApplyDamageToEngine(CEntity* inflictor, CPlane *pParent, float fDamage, eDamageType iDamageType, const Vector3 &vecPosLocal, const Vector3 &vecNormLocal);
	virtual bool BreakOffComponent(CEntity* inflictor, CPlane* pParent, int iComponent, bool bNetworkAllowed = false);

	bool  GetDestroyedByPed() const {return m_bDestroyedByPed;}
	void  CheckDestroyedByPed(CEntity* inflictor, CPlane* pParent, const int sectionBroken);

	void SetIgnoreBrokenOffPartsForAIHandling( bool bIgnoreBrokenParts ) { m_bIgnoreBrokenOffPartForAIHandling = bIgnoreBrokenParts; }
	bool GetIgnoreBrokenOffPartsForAIHandling() const { return m_bIgnoreBrokenOffPartForAIHandling; }

	void SetSectionDamageScale( int section, float damageScale ) { m_fHealthDamageScale[ section ] = damageScale; if(damageScale != 1.0f) m_bHasCustomSectionDamageScale = true; }
	float GetSectionDamageScale( int section ) { return m_fHealthDamageScale[ section ]; }
	void SetControlSectionsBreakOffFromExplosions( bool instantlyBreakOff ) { m_bControlPartsBreakOffInstantly = instantlyBreakOff; }
	bool GetControlSectionsBreakOffFromExplosions() { return m_bControlPartsBreakOffInstantly; }
	bool GetHasCustomSectionDamageScale() { return m_bHasCustomSectionDamageScale; }
	void SetHasCustomSectionDamageScale(bool val) { m_bHasCustomSectionDamageScale = val; }
#if __BANK && DEBUG_DRAW
	void DebugDraw(CPlane* pParent);
#endif
protected:

	bool GetShouldIgnoreBrokenParts( CPlane* pParent ) const;

	// Child index is 1:1 match to composite bound component
	virtual int GetSectionFromChildIndex(const CPlane* pParent, int iChildIndex);
	virtual int GetSectionFromHierarchyId(eHierarchyId iComponent);

	static const eAircraftSection sm_iHierarchyIdToSection[PLANE_NUM_DAMAGE_PARTS];
	static const float sm_fInitialComponentHealth[NUM_DAMAGE_SECTIONS];
	static const float sm_fInstantBreakOffDamageThreshold[NUM_DAMAGE_SECTIONS];
	float m_fHealth[NUM_DAMAGE_SECTIONS];
	float m_fHealthDamageScale[NUM_DAMAGE_SECTIONS];
	float m_fBrokenOffSmokeLifeTime[NUM_DAMAGE_SECTIONS];
	atFixedArray<DamageFlame *, NUM_DAMAGE_FLAMES> m_pDamageFlames;
	float m_fEngineDegradeTimer;

	//when damage is applied to engine/wings and destroyed this is set to true.
	bool  m_bDestroyedByPed;

	bool m_bIgnoreBrokenOffPartForAIHandling;
	bool m_bControlPartsBreakOffInstantly;
	bool m_bHasCustomSectionDamageScale;
};

inline float CAircraftDamage::GetSectionHealth(int iSection) const { FastAssert(iSection < NUM_DAMAGE_SECTIONS); return m_fHealth[iSection]; }
inline float CAircraftDamage::GetSectionMaxHealth(int iSection) const { FastAssert(iSection < NUM_DAMAGE_SECTIONS); return sm_fInitialComponentHealth[iSection]; }
inline void CAircraftDamage::SetSectionHealth(int iSection, float fHealth) {FastAssert(iSection < NUM_DAMAGE_SECTIONS); m_fHealth[iSection] = fHealth;}
inline float CAircraftDamage::GetInstantBreakOffDamageThreshold(int nSection) const
{
	FastAssert(nSection < NUM_DAMAGE_SECTIONS);
	return sm_fInstantBreakOffDamageThreshold[nSection];
}
inline int CAircraftDamage::GetFirstBreakablePart() const {return PLANE_FIRST_BREAKABLE_PART;}
inline int CAircraftDamage::GetNumOfBreakablePart() const {return PLANE_NUM_BREAKABLES;}


class CLandingGearDamage : public CAircraftDamageBase
{
public:

	CLandingGearDamage();

	// Use a seperate enum list to the hierarchy ids so that we could
	// link multiple hierarchy ids to one damage type
	// e.g. all landing gear can share one health
	// If this turns out to be over complicated should just stick to hierarchy ids
	enum eLandingGearSection
	{
		INVALID_SECTION = -1,

		GEAR_F,
		GEAR_RL,
		GEAR_LM1,
		GEAR_RM,
		GEAR_RM1,
		GEAR_RR,

		NUM_DAMAGE_SECTIONS
	};

	// Query damage on sections
	virtual float GetSectionHealth(int iSection) const;
	virtual float GetSectionMaxHealth(int iSection) const;

	float GetSectionHealthAsFractionActual(eLandingGearSection iSection) const;
	float GetSectionHealthAsFraction(eLandingGearSection iSection) const;
	bool HasSectionBroken(const CPlane* pParent, eLandingGearSection iSection) const;

	// Specifically damage a section
	void SetSectionHealth(int iSection, float fHealth) {FastAssert(iSection < NUM_DAMAGE_SECTIONS); m_fHealth[iSection] = fHealth;}
	void SetSectionHealthAsFraction(eLandingGearSection iSection, float fHealth);
	virtual void ApplyDamageToSection(CPlane* pParent, int iSection, int iComponent, float fDamage, eDamageType nDamageType, const Vector3 &vHitPos, const Vector3 &vHitNorm);
	void BreakSection(CPlane* pParent, eLandingGearSection nHitSection);

	// This will give the first hierarchy id associated with a damage section
	// In theory multiple components could be hooked up to one damage section so be warned
	virtual eHierarchyId GetHierarchyIdFromSection(int nLandingGearSection) const;

	void Fix();

	void BreakAll(CPlane* pParent);

	virtual float GetInstantBreakOffDamageThreshold(int nSection) const;
	virtual int GetFirstBreakablePart() const;
	virtual int GetNumOfBreakablePart() const;
	virtual void FindAllHitSections(int &nNumHitSections, int *pHitSectionIndices, const CPlane *pParent, eDamageType nDamageType, const Vector3& vecPos, int nComponent, float fDamageRadius);
	virtual bool BreakOffComponent(CEntity* inflictor, CPlane* pParent, int iComponent, bool bNetworkAllowed = false);
	virtual void PostBreakOffSection(CPlane* pParent, int nSection, eDamageType nDamageType);
	void ProcessPhysics(CPlane* pParent);
	bool IsComponentBreakable(const CPlane *pParent, int iComponent);

	void SetSectionDamageScale( int section, float damageScale ) { m_fHealthDamageScale[ section ] = damageScale; if(damageScale != 1.0f) m_bHasCustomLandingGearSectionDamageScale = true; }
	float GetSectionDamageScale( int section ) { return m_fHealthDamageScale[ section ]; }
	bool GetHasCustomSectionDamageScale() { return m_bHasCustomLandingGearSectionDamageScale; }
	void SetHasCustomSectionDamageScale(bool val) { m_bHasCustomLandingGearSectionDamageScale = val; }
protected:

	// Child index is 1:1 match to composite bound component
	virtual int GetSectionFromChildIndex(const CPlane* pParent, int iChildIndex);
	virtual int GetSectionFromHierarchyId(eHierarchyId iComponent);

	static const float sm_fInitialComponentHealth[NUM_DAMAGE_SECTIONS];
	static const eLandingGearSection sm_iHierarchyIdToSection[NUM_LANDING_GEARS];
	static const float sm_fInstantBreakOffDamageThreshold[NUM_DAMAGE_SECTIONS];

	float m_fHealth[NUM_DAMAGE_SECTIONS];
	float m_fHealthDamageScale[NUM_DAMAGE_SECTIONS];
	bool m_bHasCustomLandingGearSectionDamageScale;
};

inline float CLandingGearDamage::GetSectionHealth(int iSection) const { FastAssert(iSection < NUM_DAMAGE_SECTIONS); return m_fHealth[iSection]; }
inline float CLandingGearDamage::GetSectionMaxHealth(int iSection) const { FastAssert(iSection < NUM_DAMAGE_SECTIONS); return sm_fInitialComponentHealth[iSection]; }
inline float CLandingGearDamage::GetInstantBreakOffDamageThreshold(int nSection) const {FastAssert(nSection < NUM_DAMAGE_SECTIONS); return sm_fInstantBreakOffDamageThreshold[nSection];}
inline int CLandingGearDamage::GetFirstBreakablePart() const {return FIRST_LANDING_GEAR;}
inline int CLandingGearDamage::GetNumOfBreakablePart() const {return NUM_LANDING_GEARS;}

// forward declare
class CPlaneIntelligence;
struct ConfigPlaneControlPanelSettings;
struct ConfigPlaneEmergencyLightsSettings;
struct ConfigPlaneInsideHullSettings;

class CPlane : public CAutomobile
{
// protected member variables
protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CPlane(const eEntityOwnedBy ownedBy, u32 popType);
	friend class CVehicleFactory;


// public methods
public:
	~CPlane();

	CPlaneIntelligence* GetPlaneIntelligence() const;
	virtual void SetModelId(fwModelId modelId);

	// Control
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	
	// Physics
	virtual void ProcessPrePhysics();
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	virtual void ProcessFlightHandling(float fTimeStep);
	virtual bool WantsToBeAwake();
	virtual void ProcessPreComputeImpacts(phContactIterator impacts);
    virtual void ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const * hitInst, const Vector3& vMyHitPos,
		const Vector3& vOtherHitPos, float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact);

    void BreakOffRotor(int nRotor);
	const bool IsRotorBroken(int nRotor);

    virtual void SwitchEngineOn(bool bNoDelay = false );
	virtual void NotifyWaterImpact(const Vector3& vecForce, const Vector3& vecTorque, int nComponent, float fTimeStep);

	// Rendering
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	void ProcessTurbulence(float fTimeStep);
	bool IsTurbulenceOn();

	virtual bool GetCanMakeIntoDummy(eVehicleDummyMode dummyMode);
	virtual bool TryToMakeIntoDummy(eVehicleDummyMode dummyMode, bool bSkipClearanceTest=false);

	// disable constraints in dummy mode
	virtual bool HasDummyConstraint() const {return false;}
	virtual void ChangeDummyConstraints(const eVehicleDummyMode UNUSED_PARAM(dummyMode), bool UNUSED_PARAM(bIsStatic)) { }
	virtual void UpdateDummyConstraints(const Vec3V * UNUSED_PARAM(pDistanceConstraintPos)=NULL, const float * UNUSED_PARAM(pDistanceConstraintMaxLength)=NULL) { }

	virtual int  InitPhys();
	virtual void InitWheels();
	virtual void InitCompositeBound();

	void UpdateWindowBound(eHierarchyId eWindowId);

	void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false );
	void FinishBlowingUpVehicle( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false );//to be called after blowupcar
	virtual void PostBoundDeformationUpdate();
	void DamageForWorldExtents();
	virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount);
	void BlowUpPlaneParts();

	void SetEngineSpeed( float fEngineSpeed) { m_fEngineSpeed = fEngineSpeed; }
	float GetEngineSpeed() const { return m_fEngineSpeed; }

	int GetNumPropellors() const { return m_iNumPropellers; }
	float GetPropellerHealth(int i) const { FastAssert(i < m_iNumPropellers); return m_fPropellerHealth[i];}
	void SetPropellerHealth( int i, float health ) { FastAssert( i < m_iNumPropellers ); m_fPropellerHealth[i] = health; }
	float GetPropellorSpeedMult(int i) const;
	float GetWindBlowPropellorSpeed();

	const CPropellerBlurred& GetPropellor(int index) const { return m_propellers[index]; }

    void DriveDesiredVerticalFlightModeRatio( float driveToVerticalFlightMode );
    void SetDesiredVerticalFlightModeRatio(float verticalFlightModeRatio); // between 0.0f and 1.0f
	void SetVerticalFlightModeRatio(float verticalFlightModeRatio); // between 0.0f and 1.0f
    float GetDesiredVerticalFlightModeRatio() const {return m_fDesiredVerticalFlightRatio;} 
    float GetVerticalFlightModeRatio() const {return m_fCurrentVerticalFlightRatio;} 
    bool GetVerticalFlightModeAvaliable() const;

	const bool IsInVerticalFlightMode() const;

	// Flight control functions
	// Maybe want a superclass (CAircraft?) so that CPlane uses the same stuff
	float GetYawControl() const { return m_fYawControl; }
	float GetRollControl() const { return m_fRollControl; }
	float GetPitchControl() const { return m_fPitchControl; }
	float GetThrottleControl() const { return m_fThrottleControl; }

	// smoothed
	float GetYaw() const { return m_fYaw; }
	float GetRoll() const { return m_fRoll; }
	float GetPitch() const { return m_fPitch; }
	float GetBrake() const { return m_fBrake; }
	void  SetBrake(float brake) { m_fBrake = brake; }

	void SetYawControl(float fYawControl) { m_fYawControl = fYawControl;}
	void SetRollControl(float fRollControl) { m_fRollControl = fRollControl; }
	void SetPitchControl(float fPitchControl) { m_fPitchControl = fPitchControl; }
	void SetThrottleControl(float fThrottleControl) { m_fThrottleControl = fThrottleControl; } 
	void SetVirtualSpeedControl(float fVirtualSpeedControl) { m_fVirtualSpeedControl = fVirtualSpeedControl; } 
	void SetAirBrakeControl(float fAirBrakeControl) { m_fAirBrakeControl = fAirBrakeControl; }
	void SetEnableThrustFallOffThisFrame( bool in_bEnableThrustFallOffThisFrame ) { m_bEnableThrustFallOffThisFrame = in_bEnableThrustFallOffThisFrame; }

	// Damage
	const CAircraftDamage& GetAircraftDamage() const { return m_aircraftDamage; }
	CAircraftDamage& GetAircraftDamage() { return m_aircraftDamage; }
	bool GetDestroyedByPed();

	const CLandingGearDamage& GetLandingGearDamage() const { return m_landingGearDamage; }
	CLandingGearDamage& GetLandingGearDamage() { return m_landingGearDamage; }

	virtual void ApplyDeformationToBones(const void* basePtr);
	virtual void Fix(bool resetFrag = true, bool allowNetwork = false);

	CLandingGear& GetLandingGear() { return m_landingGear; }
	const CLandingGear& GetLandingGear() const { return m_landingGear; }
	bool GetWheelDeformation(const CWheel *pWheel, Vector3 &vDeformation) const;

	bool IsEntryPointUseable(s32 iEntryPointIndex) const;

	bool IsPlayerDrivingOnGround() const;

	bool GetIsLandingGearintact() const;
	bool AreControlPanelsIntact(bool bCheckForZeroHealth = false) const;
	// Old vehicle pop functions
	// Todo: move out of here
	static void DragPlaneToNewCoordinates(CEntity *pEntity, Vector3 &pos);
	static void FlyPlaneToNewCoordinates(CEntity *pEntity, Vector3 &pos, Vector3 &posAhead, Vector3 &posWayAhead);

	float ComputeWindMult(float fRatioOfVerticalFlightToUse);
	static float WeatherTurbulenceMult();
	static float WeatherTurbulenceMultInv();

	void	EnableIndividualPropeller(int iPropellerIndex, bool bEnable);
	bool	IsIndividualPropellerOn(int iPropellerIndex) const;

	bool DoesBulletHitPropellerBound(int iComponent) const;
	bool DoesBulletHitPropellerBound(int iComponent, bool &bHitDiscBound, int &iPropellerIndex) const;	
	bool DoesBulletHitPropeller(int iEntryComponent, const Vector3 &vEntryPos, int iExitComponent, const Vector3 &vExitPos) const;
	bool DoesProjectileHitPropeller(int iComponent) const;

	void SetDisableExplodeFromBodyDamage( bool disable ) { m_bDisableExplodeFromBodyDamage = disable; }
	bool GetDisableExplodeFromBodyDamage() { return m_bDisableExplodeFromBodyDamage; }

	void SetDisableExplodeFromBodyDamageOnCollision( bool disable ) { m_bDisableExplodeFromBodyDamageOnCollision = disable; }
	bool GetDisableExplodeFromBodyDamageOnCollision() { return m_bDisableExplodeFromBodyDamageOnCollision; }

#if __BANK
	static void InitWidgets(bkBank& bank);
#endif

	bool NeedUpdatePropellerBound(int iPropellerIndex) const;
	void UpdatePropellers();
	void SmoothInputs();

    void DisableAileron(bool bLeftAileron, bool bDisableAileron);
    bool GetRightAileronDisabled() { return m_bDisableRightAileron; }
    bool GetLeftAileronDisabled() { return m_bDisableLeftAileron; }
	bool GetBreakOffPropellerOnContact() {return m_bBreakOffPropellerOnContact;}
	void SetBreakOffPropellerOnContact(bool bBreakOff) {m_bBreakOffPropellerOnContact = bBreakOff;}

	virtual bool IsPropeller (int nComponent) const;
	virtual bool ShouldKeepRotorInCenter(CPropeller *pPropeller) const;

	bool IsEngineTurnedOffByPlayer() const {return m_bIsEngineTurnedOffByPlayer;}
	bool IsEngineTurnedOffByScript() const {return m_bIsEngineTurnedOffByScript;}
	void ResetEngineTurnedOffByPlayer() {m_bIsEngineTurnedOffByPlayer = false;}
	void TurnEngineOffFromScript() {m_bIsEngineTurnedOffByScript = true;}
	void ResetEngineTurnedOffByScript() {m_bIsEngineTurnedOffByScript = false;}

	bool IsStalling() const;

	Vec3V_Out ComputeNosePosition() const;

	bool IsLargePlane() const;
	bool IsFastPlane() const;
	bool AnyPlaneEngineOnFire() const;
	void UpdatePropellerCollision();

protected:
	void ProcessFlightModel(float fTimeStep, float fOverallMultiplier = 1.0f);

	void ModifyControlsBasedOnFlyingStats( CPed* pPilot, CFlyingHandlingData* pFlyingHandling, float &fYawControl, float &fPitchControl, float &fRollControl, float& fThrottleControl, float fTimeStep );
	void ProcessVerticalFlightModel(float fTimeStep, float fOverallMultiplier = 1.0f);

	void DoEmergencyLightEffects(crSkeleton *pSkeleton, s32 boneId, const ConfigPlaneEmergencyLightsSettings &lightParam, float distFade, float rotAmount);
	void DoControlPanelLightEffects(s32 boneId, const ConfigPlaneControlPanelSettings &lightParam, float distFade);
	void DoInsideHullLightEffects(s32 boneId, const ConfigPlaneInsideHullSettings &lightParam, float distFade);

#if __WIN32PC
	virtual void ProcessForceFeedback();
#endif // __WIN32PC

private:
     void SetDampingForFlight(float fRatioOfVerticalFlightToUse);
	 void CalculateYawAndPitchcontrol( CControl* pControl, float& fYawControl, float& fPitchControl );
	 void CalculateDamageEffects( float& fPitchFromDamageEffect, float& fYawFromDamageEffect, float& fRollFromDamageEffect, float fPitchControl, float fYawControl, float fRollControl );
	 void CalculateThrust( float fThrottleControl, float fYawControl, float fForwardSpeed, float fThrustHealthMult, float fOverallMultiplier, bool bPlayerDriver );
	 void CalculateSideSlip( Vector3 vecAirSpeed, float fOverallMultiplier );
	 Vector3 CalculateYaw( Vector3 vecTailOffset, Vector3 vecWindSpeed, float fOverallMultiplier, float fYawControl, float fVirtualFwdSpd, float fYawFromDamageEffect );
	 Vector3 CalculateRoll( float fOverallMultiplier, float fRollControl, float fRollFromDamageEffect, float fVirtualFwdSpd );
	 Vector3 CalculateStuntRoll( float fOverallMultiplier, float fRollControl, float fTimeStep );
	 Vector3 ApplyRollStabilisation( float fOverallMultiplier );
	 Vector3 CalculatePitch( Vector3 vecWindSpeed, Vector3 vecTailOffset, Vector3 vecAirSpeed, float fOverallMultiplier, float fPitchControl, float fPitchFromDamageEffect, float fVirtualFwdSpd, bool bClampAngleOfAttack );
	 void CalculateLandingGearDrag( float fForwardSpd, float fOverallMultiplier );
	 void CalculateLift( Vector3 vecAirSpeed, float fOverallMultiplier, float fPitchControl, float fVirtualFwdSpd, bool bHadCollision );

	 float GetGroundEffectOnDrag();

	 void UpdateMeredithVent();

	// public member variables
	// These should all die
public:

	// Vehicle AI variables
	// TODO: These should get moved into vehicle ai tasks / autopilot stuff
	float	m_fPreviousRoll;
	float	m_TakeOffDirection;
	float	m_LowestFlightHeight;
	float	m_DesiredHeight;
	float	m_MinHeightAboveTerrain;
	float	m_FlightDirection;
	float	m_FlightDirectionAvoidingTerrain;
	float	m_OldTilt;
	float	m_OldOrientation;
	float m_fScriptThrottleControl;
	u32	m_OnGroundTimer;

	bool	m_bVtolRollVariablesInitialised;	//used to work out if these variables have been set to appropriate values before being used.
	float	m_VtolRollIntegral;					//the roll error over time, used by the PID controller
	float	m_VtolRollPreviousError;			//the roll error in the previous frame, used by the PID controller

	float	m_fScriptTurbulenceMult;
	float	m_fScriptPilotSkillNoiseScalar;

	float	m_fRollInputBias;					// the amount by which the input from controller is biased (to be used by script)
	float	m_fPitchInputBias;					// the amount by which the input from controller is biased (to be used by script)

	void	ResetVtolRollVariables();
	void	SetVtolRollVariablesInitialised(bool initialised)	{m_bVtolRollVariablesInitialised = initialised;}
	bool	GetVtolRollVariablesInitialised()					{return m_bVtolRollVariablesInitialised;}
	void	SetVtolRollIntegral(float rollIntegral)				{m_VtolRollIntegral = rollIntegral; }
	float	GetVtolRollIntegral()								{return m_VtolRollIntegral;}
	void	SetVtolRollPreviousError(float rollPreviousError)	{m_VtolRollPreviousError = rollPreviousError; }
	float	GetVtolRollPreviousError()							{return m_VtolRollPreviousError;}
	float	GetWarmUpTime() const								{return m_fEngineWarmUpTimer;}

	void	SetDisableVerticalFlightModeTransition( bool disable ) { m_bDisableVerticalFlightModeTransition = disable; }
	bool	GetDisableVerticalFlightModeTransition()			   { return m_bDisableVerticalFlightModeTransition; }

	void SetDisableAutomaticCrashTask(bool bDisable) { m_bDisableAutomaticCrashTask = bDisable; }
	bool GetDisableAutomaticCrashTask() const { return m_bDisableAutomaticCrashTask; }

    void SetAllowRollAndYawWhenCrashing( bool allowRollAndYaw ) { m_allowRollAndYawWhenCrashing = allowRollAndYaw; }
    bool GetAllowRollAndYawWhenCrashing() { return m_allowRollAndYawWhenCrashing; }

	void SetDipStraightDownWhenCrashing( bool dipDown ) { m_dipStraightDownWhenCrashing = dipDown; }
	bool GetDipStraightDownWhenCrashing() { return m_dipStraightDownWhenCrashing; }

protected:

	// For rendering propellers
	CPropellerBlurred m_propellers[PLANE_NUM_PROPELLERS];	

	// Handle the propeller collision
	CPropellerCollision m_propellerCollision[PLANE_NUM_PROPELLERS];
	int m_iNumPropellers;
    float m_fPropellerHealth[PLANE_NUM_PROPELLERS];

	CLandingGear m_landingGear;

	// Class to handle damage processing
	CAircraftDamage m_aircraftDamage;
	CLandingGearDamage m_landingGearDamage;

	// Instantaneous control inputs
	// Physics model uses these
	float m_fYawControl;
	float m_fPitchControl;
	float m_fRollControl;
	float m_fThrottleControl;
	float m_fAirBrakeControl;
    
	// allows planes to fly slower than min speed
    // for use by the AI
	float m_fVirtualSpeedControl;

	// Smoothed inputs
	// Used for animating bones
	float m_fYaw;
	float m_fPitch;
	float m_fRoll;
	float m_fBrake;

	float m_fVisualRoll;
	float m_fVisualPitch;
	float m_fCurrentFlapAngle;

	float m_fJoystickPitch;
	float m_fJoystickRoll;

	float m_fEngineSpeed;

    float m_fDesiredVerticalFlightRatio;
    float m_fCurrentVerticalFlightRatio;

    bool m_bDisableLeftAileron : 1;
    bool m_bDisableRightAileron : 1;
	bool m_bIsPropellerDamagedThisFrame : 1;
	bool m_bIsEngineTurnedOffByPlayer : 1;
	bool m_bIsEngineTurnedOffByScript : 1;
	bool m_bEnableThrustFallOffThisFrame : 1;
	bool m_bDisableAutomaticCrashTask : 1;
	bool m_bBreakOffPropellerOnContact : 1;
	u8 m_IndividualPropellerFlags : PLANE_NUM_PROPELLERS;
	bool m_bDisableVerticalFlightModeTransition;
	bool m_bDisableExplodeFromBodyDamage;
	bool m_bDisableExplodeFromBodyDamageOnCollision;

	// Turbulence
	float m_fTurbulenceMagnitude;
	float m_fTurbulenceNoise;
	float m_fTurbulenceTimeSinceLastCycle;
	float m_fTurbulenceTimer;
	float m_fTurbulenceRecoverTimer;
	Vector3 m_vTurbulenceAirflow;

	float m_fTurnEngineOffTimer;
	float m_fEngineWarmUpTimer;

	float m_fLeftLightRotation;
	float m_fRightLightRotation;

#if __WIN32PC
	float m_fWingForce;
#endif // __WIN32PC

public:
	
	static bank_float ms_fMaxEngineSpeed;
	static bank_float ms_fMinEngineSpeed;

	static bool ms_ReduceDragWithGroundEffect;

#if  USE_SIXAXIS_GESTURES
	static bank_float MOTION_CONTROL_PITCH_MIN;
	static bank_float MOTION_CONTROL_PITCH_MAX;
	static bank_float MOTION_CONTROL_ROLL_MIN;
	static bank_float MOTION_CONTROL_ROLL_MAX;
	static bank_float MOTION_CONTROL_YAW_MULT;
#endif

protected:

	static bank_float ms_fStdEngineSpeed;	
	static bank_float ms_fEngineSpeedChangeRate;		// rate of change. put in handling?
	static bank_float ms_fEngineSpeedDropRateInWater;
	static bank_float ms_fEngineSpeedDropRateWhenMissFiring;

	static bank_float ms_fPropRenderSpeed;
	static bank_float ms_fEngineSpeedPropMult; // brings the propellors up to full speed faster

	static dev_float ms_fControlSmoothSpeed; // Change rate of rudder, elevator angles 

	static bank_float ms_fCeilingLiftCutoffRange;	// How fast does lift drop off when we go over ceiling

	static bank_float ms_fTopSpeedDampRate;
	static bank_float ms_fTopSpeedDampMaxHeight;
	static bank_float ms_fTopSpeedDampMinHeight;

	static float ms_fPlaneControlLaggingMinBlendingRate;
	static float ms_fPlaneControlLaggingMaxBlendingRate;
	static float ms_fPlaneControlAbilityControlDampMin;
	static float ms_fPlaneControlAbilityControlDampMax;
	static float ms_fPlaneControlAbilityControlRandomMin;
	static float ms_fPlaneControlAbilityControlRandomMax;
	static float ms_fPlaneControlAbilityControlRandomRollMult;
	static float ms_fPlaneControlAbilityControlRandomPitchMult;
	static float ms_fPlaneControlAbilityControlRandomYawMult;
	static float ms_fPlaneControlAbilityControlRandomThrottleMult;

	static float ms_fTimeBetweenRandomControlInputsMin;
	static float ms_fTimeBetweenRandomControlInputsMax;
	static float ms_fRandomControlLerpDown;
	static float ms_fMaxAbilityToAdjustDifficulty;
	static bank_bool ms_bDebugAfterburnerEffect;

	void CacheWindowBones();

	s16	   m_windowBoneIndices[16];
	bool   m_windowBoneCached;
    bool   m_allowRollAndYawWhenCrashing;
	bool   m_dipStraightDownWhenCrashing;

	//Vector3 m_prevTorque;

	NETWORK_OBJECT_TYPE_DECL( CNetObjPlane, NET_OBJ_TYPE_PLANE );
};



#endif//_PLANE_H_

