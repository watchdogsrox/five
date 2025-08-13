/********************************************************************
filename:	VehicleGadgets.h
created:	2008/10/29
author:		jack.potter

purpose:	Code for handling special vehicle extras which might want to adjust
			a vehicle's skeleton, contorls or physics.

*********************************************************************/
#ifndef __VEHICLE_GADGETS_H__
#define __VEHICLE_GADGETS_H__

// rage headers 
#include "vector/vector3.h"
#include "atl/array.h"
#include "atl/string.h"

#include "camera/helpers/HandShaker.h"
#include "network/General/NetworkUtil.h"
#include "Streaming/streamingrequest.h"
#include "Scene/RegdRefTypes.h"
#include "Scene/Physical.h"
#include "Vehicles/VehicleDefines.h"
#include "Vehicles/handlingMgr.h"
#include "renderer/HierarchyIds.h"
#include "physics/rope.h"
#include "physics/constrainthandle.h"
#include "physics/constraintRotation.h"
#include "physics/WorldProbe/worldprobe.h"
#include "timecycle/TimeCycleConfig.h"
#include "network/General/NetworkUtil.h"
#include "control/replay/replay.h"
#include "game/ModelIndices.h"
#include "weapons/WeaponEnums.h"

//Forward declarations
class CVehicle;
class CWeapon;
class CVehicleModelInfo;
class CWeaponInfo;
class CTrailer;
class audVehicleTurret;
class audVehicleForks;
class audVehicleDigger;
class audVehicleHandlerFrame;
class audVehicleTowTruckArm;
class audVehicleGrapplingHook;
class audVehicleGadgetMagnet;
class audVehicleGadgetDynamicSpoiler;

#define SEARCHLIGHTSAMPLES (6)	// Number of predictions stored for the //searchlight
namespace rage
{
	struct fragHierarchyInst;
	class ropeInstance;
};


enum eVehicleGadgetType
{
	// networked types
	VGT_FORKS,
	VGT_SEARCHLIGHT,
	VGT_PICK_UP_ROPE,
	VGT_DIGGER_ARM,
	VGT_HANDLER_FRAME,
	VGT_PICK_UP_ROPE_MAGNET,
	VGT_BOMBBAY,
	VGT_NUM_NETWORKED_TYPES,

	// non-networked types
	VGT_FIXED_VEHICLE_WEAPON,
	VGT_VEHICLE_WEAPON_BATTERY,
	VGT_TURRET,
	VGT_TURRET_PHYSICAL,
	VGT_TRACKS,
	VGT_LEAN_HELPER,
	VGT_TRAILER_ATTACH_POINT,
	VGT_TRAILER_LEGS,
	VGT_ARTICULATED_DIGGER_ARM,
	VGT_PARKING_SENSOR,
	VGT_TOW_TRUCK_ARM,
    VGT_THRESHER,
    VGT_BOATBOOM,
	VGT_RADAR,
	VGT_DYNAMIC_SPOILER,
};

struct JointInfo
{
	s32		m_iFragChild;
	s32		m_iBone;

	float	m_fDesiredAcceleration;
	float	m_fSpeed;
	float	m_fPrevPosition;

	bool m_bPrismatic;
	bool m_bContinuousInput; // False for joint that get targets set once(fire and forget) - True for periodic updates of target (typically user input)
	Vector3 m_vecJointChildPosition;//Only used for prismatic joints, can't add vector3 to union

	bool    m_bSwingFreely;
	union{
		struct{
				float   m_fPosition;
				float   m_fDesiredPosition;
				float   m_fOpenPosition;
				float   m_fClosedPosition;
				bool	m_bOnlyControlSpeed;

		}m_PrismaticJointInfo;

		struct{
				eRotationAxis m_eRotationAxis;
				float	m_fAngle;
				float	m_fDesiredAngle;
				float	m_fOpenAngle;
				float	m_fClosedAngle;
				bool    m_bOnlyControlSpeed;
				bool	m_bSnapToAngle;
		}m_1dofJointInfo;
	};
};


struct SearchLightLightInfo {
	float falloffScale;
	float falloffExp;
	float innerAngle;
	float outerAngle;

	float globalIntensity;
	float lightIntensityScale;
	float volumeIntensityScale;
	float volumeSizeScale;

	Color32 outerVolumeColor;
	float	outerVolumeIntensity;
	float	outerVolumeFallOffExponent;

	bool enable;
	bool specular;
	bool shadow;
	bool volume;

	float lightFadeDist;

	float coronaIntensity;
	float coronaIntensityFar;
	float coronaOffset;
	float coronaSize;
	float coronaSizeFar;
	float coronaZBias;
	
#if __BANK
	void InitWidgets(bkBank &bank);
#endif
};


struct SearchLightInfo {

	float length;
	float offset;
	Color32 color;

	SearchLightLightInfo mainLightInfo;
	SearchLightInfo();
	void Set(const CVisualSettings& visualSettings, const char* type);

#if __BANK
	void InitWidgets(bkBank &bank);
#endif
};

// defines interface used to control custom parts of vehicles
// e.g. tank turrets, watercannons, guns, etc.
class CVehicleGadget
{
public:
	
	CVehicleGadget() :m_bEnabled(true) {};
	virtual ~CVehicleGadget() {}

#if __BANK
	static void AddWidgets(bkBank& bank);
#endif //__BANK

    virtual void ProcessPrePhysics(CVehicle* UNUSED_PARAM(pParent)) {}
	virtual void ProcessPhysics(CVehicle* UNUSED_PARAM(pParent), const float UNUSED_PARAM(fTimestep), const int UNUSED_PARAM(nTimeSlice)) {}
    virtual void ProcessPhysics2(CVehicle* UNUSED_PARAM(pParent), const float UNUSED_PARAM(fTimestep)) {}
	virtual void ProcessPhysics3(CVehicle* UNUSED_PARAM(pParent), const float UNUSED_PARAM(fTimestep)) {}
	virtual void ProcessPreRender(CVehicle* UNUSED_PARAM(pParent)) {}
	virtual void ProcessPostPreRender(CVehicle* UNUSED_PARAM(pParent)) {}
	virtual void ProcessControl(CVehicle* UNUSED_PARAM(pParent)) {}
	virtual void Fix() {}
    // ProcessPreComputeImpacts should reset the impact iterator after use.
    virtual void ProcessPreComputeImpacts(CVehicle *UNUSED_PARAM(pVehicle), phCachedContactIterator &UNUSED_PARAM(impacts)){}
	virtual s32 GetType() const = 0;

	virtual bool IsVehicleWeapon() const { return false;} 

	// Check whether the physics on this vehicle should be activated this frame
	virtual bool WantsToBeAwake(CVehicle* UNUSED_PARAM(pParent)) { return false; }

	// Transition this gadget's LOD
	virtual void ChangeLod(CVehicle & UNUSED_PARAM(parentVehicle), int UNUSED_PARAM(lod)) { };

    virtual void BlowUpCarParts(CVehicle* UNUSED_PARAM(pParent)){}

	virtual int GetGadgetEntities(int UNUSED_PARAM(entityListSize), const CEntity ** UNUSED_PARAM(entityListOut)) const { return 0; };

	virtual void SetIsVisible(CVehicle* UNUSED_PARAM(pParent), bool UNUSED_PARAM(bIsVisible)) {}

	//helper functions for 1dof joints 
	static void DriveComponentToAngle(fragHierarchyInst* pHierInst,
		s32 iFragChild,
		float fDesiredAngle,
		float fDesiredSpeed,
		float& fCurrentAngleOut,
        float fMuscleStiffness = 0.05f,
        float fMuscleAngleStrength = 10.0f,
        float fMuscleSpeedStrength = 20.0f,
        float fMinAndMaxMuscleTorque = 100.0f,
        bool bOnlyControlSpeed = false,
        float fSpeedStrengthFadeOutAngle = (PI / 45.0f),
		bool bOnlyUpdateCurrentAngle = false);

	static void SnapComponentToAngle(fragInstGta* pFragInst,
		s32 iFragChild, 
		float fDesiredAngle,
		float& fCurrentAngleOut);

	static void DriveHiearchyIdToAngle(eHierarchyId nId,
		CVehicle* pParent,
		float fDesiredAngle,
		float fDesiredSpeed,
		float& fCurrentAngleOut);

	static float GetJointAngle(fragHierarchyInst* pHierInst,
		s32 iFragChild);

    //helper functions for prismatic joints
    static void DriveComponentToPosition(fragHierarchyInst* pHierInst, 
        s32 iFragChild, 
        float fDesiredPosition, 
        float fDesiredSpeed, 
        float &fCurrentPositionOut,
        Vector3 &vecJointChildPosition,
        float fMuscleStiffness = 0.05f,
        float fMusclePositionStrength = 10.0f,
        float fMuscleSpeedStrength = 20.0f,
        float fMinAndMaxMuscleForce = 100.0f,
		bool bOnlyControlSpeed = true);

    static void SwingJointFreely(fragHierarchyInst* pHierInst, 
        s32 iFragChild);

	// This allows us to disable weapons individually
	virtual void SetEnabled( CVehicle* , bool bEnabled ) { m_bEnabled = bEnabled; }
	bool GetIsEnabled() const { return m_bEnabled; }

	// network methods:
	virtual bool IsNetworked() const { return false; }
	virtual void Serialise(CSyncDataBase&) { Assertf(0, "Serialiser not defined for networked vehicle gadget"); }

	// collision flag reset
	virtual void ResetIncludeFlags(CVehicle* UNUSED_PARAM(pParent)) {}

private:
	bool	m_bEnabled;
};

// For updating vehicle tank tracks
class CVehicleTracks : public CVehicleGadget
{
public:
	CVehicleTracks(u32 iWheelMask = 0xffffffff);
	virtual void ProcessPhysics(CVehicle* pParent, const float fTimeStep, const int nTimeSlice);
	virtual void ProcessPreRender(CVehicle* pParent);

	virtual s32 GetType() const { return VGT_TRACKS; }

protected:
	float m_fWheelAngleLeft;
	float m_fWheelAngleRight;

	// Defines which wheels are driving the track
	u32 m_iWheelMask;
};

class CVehicleLeanHelper : public CVehicleGadget
{
public:
	CVehicleLeanHelper();

	virtual void ProcessPhysics2(CVehicle* pParent, const float fTimeStep);
	virtual s32 GetType() const { return VGT_LEAN_HELPER; }

protected:

	void ProcessLeanSideways(CVehicle* pParent);
	
	// fLeanInput: 1.0f lean fully back, -1.0f lean fully forward
	void ProcessLeanFwd(CVehicle* pParent, float fLeanInputY);

	static dev_float ms_fAngInertiaRaiseFactor;

	// Static members
	static dev_float ms_fLeanMinSpeed;
	static dev_float ms_fLeanBlendSpeed;
	static dev_float ms_fLeanForceSpeedMult;
	static dev_float ms_fWheelieForceCgMult;

	// Triggers for stabilising wheelies
	static dev_float ms_fWheelieOvershootAngle;	// Start helping after we overshoot by this far
	static dev_float ms_fWheelieOvershootCap;		// Scale forces up to this angle

	static dev_float ms_fStoppieStabiliserMult;
};

class CVehicleWeapon : public CVehicleGadget
{
public:
	CVehicleWeapon() { m_weaponIndex = 0; m_handlingIndex = -1;}
	virtual void Fire(const CPed* pFiringPed, CVehicle* pParent, const Vector3& vTargetPosition, const CEntity* pTarget = NULL, bool bForceShotInWeaponDirection = false) = 0;

	virtual const CWeaponInfo* GetWeaponInfo() const = 0;
	virtual u32 GetHash() const = 0;

	virtual bool IsVehicleWeapon() const { return true;} 
	virtual bool IsWaterCannon() const {return false;}
	virtual void TriggerEmptyShot(CVehicle* UNUSED_PARAM(pParent)) const {};

	virtual bool GetReticlePosition(Vector3&) const { return false; }

	virtual void GetForceShotInWeaponDirectionEnd(CVehicle* UNUSED_PARAM(pParent), Vector3& UNUSED_PARAM(vForceShotInWeaponDirectionEnd)) {};

	virtual void SetOverrideWeaponDirection(const Vector3 &UNUSED_PARAM(vOverrideDirection)) {}

	virtual void SetOverrideLaunchSpeed(float UNUSED_PARAM(fOverrideLaunchSpeed)) {}

	virtual bool IsUnderwater(const CVehicle* UNUSED_PARAM(pParent)) const { return false; }

#if !__NO_OUTPUT
	virtual const char* GetName() const = 0;
#endif

#if !__FINAL
	virtual bool HasWeapon(const CWeapon* pWeapon) const = 0;
#endif

	s32 m_weaponIndex;
	s32 m_handlingIndex;
};


// Class CTurret
//
// Rotates a turret to face desired direction
// Note it does not handle any weapons, these are handled by seperate 'gadgets'
class CTurret : public CVehicleGadget
{
public:

	enum eFlags
	{
		TF_RenderDesiredAngleForPhysicalTurret = BIT0,
		TF_HaveUsedDesiredAngleForRendering = BIT1,
		TF_UseDesiredAngleForPitch = BIT2,
		TF_DisableMovement = BIT3,
		TF_InitialAdjust = BIT4,
        TF_Hide = BIT5,
	};

	CTurret(eHierarchyId iBaseBone, eHierarchyId iPitchBone, float fTurretSpeed, float fTurretPitchMin, float fTurretPitchMax, float fTurretCamPitchMin, 
			float fTurretCamPitchMax, float fBulletVelocityForGravity, float fTurretPitchForwardMin, CVehicleWeaponHandlingData::sTurretPitchLimits sPitchLimits);

	virtual void InitJointLimits(const CVehicleModelInfo* pModelInfo);

	virtual void ProcessPreRender(CVehicle* pParent);
	virtual void ProcessControl(CVehicle* pParent);

	void InitAudio(CVehicle* pParent);

	virtual s32 GetType() const { return VGT_TURRET; }
	
	// returns the matrix of the turret's barrel
	// This will be correct whether the parent vehicle's world matrices are up to date or not
	void GetTurretMatrixWorld(Matrix34& matOut, const CVehicle* pParent) const;

	void AimAtWorldPos(const Vector3& vDesiredTarget, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim);
	void AimAtLocalDir(const Vector3& vLocalTargetDir, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim);		// Local means in _turret's_ space

	Vector3 ComputeLocalDirFromWorldPos(const Vector3& vDesiredTarget, const CVehicle* pParent) const;
	Vector3 ComputeWorldPosFromLocalDir(const Vector3& vDesiredTarget, const CVehicle* pParent) const;
	float ComputeRelativeHeadingDelta(const CVehicle* pParent) const;

	void SetAssociatedSeatIndex(s32 iSeatIndex) { m_AssociatedSeatIndex = iSeatIndex; }
	void SetMinPitch(float fMinPitch) {m_fMinPitch = fMinPitch;}
	float GetMinPitch() const {return m_fMinPitch;}

	void SetMaxPitch(float fMaxPitch) {m_fMaxPitch = fMaxPitch;}
	float GetMaxPitch() const {return m_fMaxPitch;}

	float GetCurrentPitch() const {	return m_AimAngle.TwistAngle(XAXIS); }
	float GetDesiredPitch() const {	return m_DesiredAngle.TwistAngle(XAXIS); }
	float GetLerpValueForRearMountedTurret(float fdesiredAngMag, float& fForcedPitchAng);
	float GetCurrentHeading(const CVehicle* pParent) const;
	float GetDesiredHeading() const { return m_DesiredAngle.TwistAngle(ZAXIS); }
	float GetAimHeading() const { return m_AimAngle.TwistAngle(ZAXIS); }
	float GetLocalHeading(const CVehicle& rParent) const;

	void SetMinHeading(float fMinHeading) {m_fMinHeading = fMinHeading;}
	float GetMinHeading() const {return m_fMinHeading;}

	void SetMaxHeading(float fMaxHeading) {m_fMaxHeading = fMaxHeading;}
	float GetMaxHeading() const {return m_fMaxHeading;}

	void SetHeadingSpeedModifier(float fPerFrameHeadingModifier) { m_fPerFrameHeadingModifier = fPerFrameHeadingModifier; }
	void SetPitchSpeedModifier(float fPitchSpeedModifier) { m_fPerFramePitchModifier = fPitchSpeedModifier; }
	void SetTurretSpeedModifier(float fTurretSpeedModifier) { m_fSpeedModifier = fTurretSpeedModifier; }
	void SetTurretSpeedOverride(float fSpeed) { m_fTurretSpeedOverride = fSpeed; }
	void SetTurretSpeed(float fSpeed) { m_fTurretSpeed = fSpeed; }
	float GetTurretSpeed() const;

	float GetBulletVelocityForGravity() const { return m_fBulletVelocityForGravity; }

	inline virtual bool IsPhysicalTurret() const {return false;}
	bool IsWithinFiringAngularThreshold( float fAngularThresholdInRads ) const;

	void SetDesiredAngle(const Quaternion& desiredAngle, CVehicle* pParent);

	eHierarchyId GetBaseBoneId() const { return m_iBaseBoneId; }
	eHierarchyId GetBarrelBoneId() const { return m_iPitchBoneId; }

	fwFlags32& GetFlags() { return m_Flags; }
	const fwFlags32& GetFlags() const { return m_Flags; }

protected:

	float CalculateTrajectoryForGravity(const Vector3& vLocalDir, float projVelocity);
	float GetTurretSpeedModifier() const { return m_fSpeedModifier; }
	void ComputeTurretSpeedModifier(const CVehicle *pParent);

	eHierarchyId m_iBaseBoneId;
	eHierarchyId m_iPitchBoneId;

	Quaternion m_DesiredAngleUnclamped;
	Quaternion m_DesiredAngle;
	Quaternion m_AimAngle;

	s32 m_AssociatedSeatIndex;
	audVehicleTurret* m_VehicleTurretAudio;

	float m_fSpeedModifier;
	float m_fPerFrameHeadingModifier;
	float m_fPerFramePitchModifier;
	float m_fTurretSpeedOverride;			//Used to override turret speed from script per-frame

	// Configuration vars
	// From bone dofs
	float m_fMinPitch;
	float m_fMaxPitch;
	float m_fMinHeading;
	float m_fMaxHeading;

	// From handling.dat
	float m_fTurretSpeed;
	float m_fTurretPitchMin;
	float m_fTurretPitchMax;
	float m_fTurretCamPitchMin;
	float m_fTurretCamPitchMax;
	float m_fBulletVelocityForGravity;
	float m_fTurretPitchForwardMin;			//This is used for the firetruck to limit it's pitch aiming forwards and back as the water would hit the truck. It lerps between this and m_fTurretPitchMin at the sides.

	CVehicleWeaponHandlingData::sTurretPitchLimits m_PitchLimits;

	fwFlags32 m_Flags;
};

class CTurretPhysical : public CTurret
{
public:
	CTurretPhysical(eHierarchyId iBaseBone, eHierarchyId iPitchBone, float fTurretSpeed, float fTurretPitchMin, float fTurretPitchMax, 
					float fTurretCamPitchMin, float fTurretCamPitchMax, float fBulletVelocityForGravity, float fTurretPitchForwardMin, 
					CVehicleWeaponHandlingData::sTurretPitchLimits sPitchLimits);
	virtual ~CTurretPhysical() {};

	virtual void InitJointLimits(const CVehicleModelInfo* pModelInfo);

	virtual void ProcessPreRender(CVehicle* pParent);
	virtual void ProcessControl(CVehicle* ){};
	virtual void ProcessPhysics(CVehicle* pParent, const float fTimeStep, const int nTimeSlice);
	void ProcessPreComputeImpacts(CVehicle *pVehicle, phCachedContactIterator &impacts);

	virtual s32 GetType() const { return VGT_TURRET_PHYSICAL; }

    void DriveTurret(CVehicle* pParent, const int nTimeSlice = 0);

    void SetEnabled( CVehicle* pParent, bool bEnabled );

	inline virtual bool IsPhysicalTurret() const {return true;}
	s32 GetBaseFragChild() const {return m_iBaseFragChild;}
	s32 GetBarrelFragChild() const {return m_iBarrelFragChild;}

	virtual void BlowUpCarParts(CVehicle* pParent);
	void ResetTurretsMuscles(CVehicle* pParent);

	void SetPedHasControl( bool bPedHasControl ) { m_bPedHasControlOfTurret = bPedHasControl; }

	void ResetIncludeFlags(CVehicle* pParent);

	float ClampAngleToJointLimits(float fAngle, const CVehicle& rParent, s32 iFragChild);

	bool WantsToBeAwake(CVehicle *pVehicle);
	bool GetIsBarrelInCollision() {	return m_bBarrelInCollision; }

protected:
	s32 m_iBaseFragChild;
	s32 m_iBarrelFragChild;
	float m_fTimeSinceTriggerDesiredAngleBlendInOut;
	float m_RenderAngleX;
	float m_RenderAngleZ;
	bool m_bBarrelInCollision;
	bool m_bHasReachedDesiredAngle;
	bool m_bPedHasControlOfTurret;
	bool m_bHeldTurret;
	bool m_bResetTurretHeading;
    bool m_bHidden;
};

// Class to represent a single weapon mounted to a vehicle
// The weapon is fixed (always points along bone's forward vector)
class CFixedVehicleWeapon : public CVehicleWeapon
{
public:
	CFixedVehicleWeapon(u32 nWeaponHash, eHierarchyId iWeaponBone);
	virtual ~CFixedVehicleWeapon();

	virtual void ProcessPreRender(CVehicle* pParent);
	virtual void ProcessPostPreRender(CVehicle* pParent);
	virtual void ProcessControl(CVehicle* pParent);
	virtual s32 GetType() const { return VGT_FIXED_VEHICLE_WEAPON; }
	virtual f32 GetProbeDist() const { return m_fProbeDist; }
	eHierarchyId GetWeaponBone() const { return m_iWeaponBone; }
	CWeapon* GetWeapon() const { return m_pVehicleWeapon; }

	virtual void TriggerEmptyShot(CVehicle* pParent) const;
	virtual void Fire(const CPed* pFiringPed, CVehicle* pParent, const Vector3& vTargetPosition, const CEntity* pTarget = NULL, bool bForceShotInWeaponDirection = false);
	void SpinUp(CPed* pFiringPed);
	virtual const CWeaponInfo* GetWeaponInfo() const;
	virtual u32 GetHash() const;
	virtual bool GetReticlePosition(Vector3&) const;

	virtual void GetForceShotInWeaponDirectionEnd(CVehicle* pParent, Vector3& vForceShotInWeaponDirectionEnd);

	virtual void SetOverrideWeaponDirection(const Vector3 &vOverrideDirection);

	virtual void SetOverrideLaunchSpeed(float fOverrideLaunchSpeed);

	void ClearOverrideWeaponDirection()
	{
		m_bUseOverrideWeaponDirection = false;
		m_vOverrideWeaponDirection.Zero();
	}
	
	void ResetIncludeFlags(CVehicle* pParent);

	virtual bool IsUnderwater(const CVehicle* pParent) const;

#if !__NO_OUTPUT
	virtual const char* GetName() const;
#endif

#if !__FINAL
	virtual bool HasWeapon(const CWeapon* pWeapon) const { return m_pVehicleWeapon == pWeapon; }
#endif

protected:

	Vector3 GetForwardNormalBasedOnGround(const CVehicle* pVehicle);
	bool IsWeaponBoneValidForRotaryCannon();
	s32 GetRotationBoneOffset();

	CFixedVehicleWeapon();

	CWeapon* m_pVehicleWeapon;
	eHierarchyId m_iWeaponBone;

	// Kickback vars
	u32 m_iTimeLastFired;

	// Fire vars need queuing so that weapon is only fired at the correct point in the frame	
	RegdConstPed m_pFiringPed;
	RegdConstEnt m_pQueuedTargetEnt;
	Vector3		 m_vQueuedTargetPos;
	Vector3		 m_vQueuedFireDirection;
	Vector3		 m_vOverrideWeaponDirection;
	Vector3		 m_vTurretForwardLastUpdate;
	bool m_bFireIsQueued;
	bool m_bForceShotInWeaponDirection;
	bool m_bUseOverrideWeaponDirection;
	float m_fOverrideLaunchSpeed;

	// Required to cache off in post pre render
	Vector3 m_v3ReticlePosition;

	// Total dist used for frame by frame line probe
	f32 m_fProbeDist;

	// Number of radians still to rotate "ammo belt" through after firing a bullet.
	float m_fAmmoBeltAngToTurn;

	// Called from pre render so that all matrices have been updated
	virtual bool FireInternal(CVehicle* pParent);

	void ApplyBulletDirectionOffset(Matrix34 &weaponMat, CVehicle* pParent);

private:
	strLocalIndex GetModelLocalIndexForStreaming() const;

	strRequest m_projectileRequester;
};

class CVehicleWaterCannon : public CFixedVehicleWeapon
{
public:
	CVehicleWaterCannon(u32 nWeaponHash, eHierarchyId iWeaponBone);
	virtual ~CVehicleWaterCannon();

	virtual const CWeaponInfo* GetWeaponInfo() const;

	virtual bool IsWaterCannon() const {return true;}

	virtual void ProcessControl(CVehicle* pParent);

	//This function allows cloned vehicles to update collision of the water stream to reflect remote machine
	bool FireWaterCannonFromMatrixPositionAndAlignment(CVehicle* pParent, const Matrix34 &weaponMat);

	virtual u32 GetHash() const;

#if !__NO_OUTPUT
	virtual const char* GetName() const;
#endif

	static const float FIRETRUCK_CANNON_RANGE;
private:
	// Called from pre render so that all matrices have been updated
	// We're gonna overload this to use the water cannon manager instead of
	// some CWeapon, since the water cannon doesn't really jive with a lot 
	// of the assumptions of the weapon code
	virtual bool FireInternal(CVehicle* pParent);

	static const float FIRETRUCK_CANNON_JET_SPEED;

	Vec3V m_CannonDirLastFrame;
};

class CSearchLight : public CFixedVehicleWeapon
{
public:
	CSearchLight(u32 nWeaponHash, eHierarchyId iWeaponBone);
	virtual ~CSearchLight();

	static void UpdateVisualDataSettings();

#if __BANK
	static void InitWidgets(bkBank& pBank);
#endif // __BANK

	virtual void ProcessPostPreRender(CVehicle* parent);

	virtual s32 GetType() const { return VGT_SEARCHLIGHT; }

	void SetLightOn(bool b);
	bool GetLightOn() const 							{ return m_lightOn; }
	void SetForceLightOn(bool b)						{ m_bForceOn = b; }
	bool GetForceLightOn() const						{ return m_bForceOn; }
	float GetSweepTheta() const { return m_SweepTheta; }
	void SetSweepTheta(float in_Value) { m_SweepTheta = in_Value; }
	Vector3 GetSearchLightExtendTimer() const { return m_fSearchLightExtendTimer; }
	void SetSearchLightExtendTimer(Vector3& in_Value) { m_fSearchLightExtendTimer = in_Value; }

	void SetTargetPosition(const Vector3& targetPos)	{m_targetPos = targetPos;}
	const Vector3 &GetTargetPos()						{return m_targetPos;}
	void ProcessSearchLight(CVehicle* parent);
	CPhysical *GetSearchLightTarget()					{return SafeCast(CPhysical, m_SearchLightTarget.GetEntity());}
#if GTA_REPLAY
	CReplayID GetSearchLightTargetReplayId()			{return m_SearchLightTargetReplayId;}
#endif // GTA_REPLAY
	void SetSearchLightTarget(CPhysical* pTarget);
	float GetLightBrightness()							{return m_LightBrightness;}
	bool GetSearchLightBlocked()						{return m_searchLightBlocked;}

	void SetHasPriority(bool value)						{m_bHasPriority = value;}
	bool GetHasPriority()								{return m_bHasPriority;}

	bool GetIsDamaged() {return m_isDamaged;}
	void SetIsDamaged() {m_isDamaged = true;}

	// network methods:
	virtual bool IsNetworked() const					{ return true; }
	virtual void Serialise(CSyncDataBase& serialiser);

	static const SearchLightInfo &GetSearchLightInfo(CVehicle *parent);


	void CopyTargetInfo( CSearchLight& searchLight);
	Vector3 GetCloneTargetPos() {return m_CloneTargetPos; }

protected:

	// Called from pre render so that all matrices have been updated
	virtual bool FireInternal(CVehicle* pParent);

	float m_SweepTheta;
	bool m_searchLightBlocked;
	bool m_lightOn;
	bool m_bHasPriority;
	bool m_isDamaged;
	bool m_bForceOn;

	//new heli search lights stuff
	CSyncedEntity m_SearchLightTarget;	
#if GTA_REPLAY
	CReplayID m_SearchLightTargetReplayId;
#endif // GTA_REPLAY
	Vector3 m_OldSearchLightTargetPos[SEARCHLIGHTSAMPLES];
	Vector3 m_targetPos;
	Vector3 m_CloneTargetPos;
	Vector3 m_fSearchLightExtendTimer;
	camHandShaker m_searchLightTilt;
	WorldProbe::CShapeTestSingleResult m_targetProbeHandle;
	u32		m_LastSearchLightSample;
	float 	m_LightBrightness;
};

class CVehicleRadar : public CFixedVehicleWeapon
{
public:
	CVehicleRadar(u32 nWeaponHash, eHierarchyId iWeaponBone) : CFixedVehicleWeapon(nWeaponHash, iWeaponBone) 
	{ 
		m_fAngle = 0.0f;
	}
	virtual ~CVehicleRadar() {};

#if __BANK
	static void InitWidgets(bkBank& pBank);
#endif // __BANK

	virtual void ProcessPostPreRender(CVehicle* parent);

	virtual s32 GetType() const { return VGT_RADAR; }

protected:

	float m_fAngle;
};

// Gadget to handle multiple vehicle weapons fired sequentially
// e.g. a load of weapons on the front of a helicopter
class CVehicleWeaponBattery : public CVehicleWeapon
{
public:

	enum eMissileFlapState
	{
		MFS_CLOSED,
		MFS_OPEN,
		MFS_CLOSING,
		MFS_OPENING
	};

	enum eMissileBatteryState
	{
		MBS_IDLE,
		MBS_RELOADING
	};

	CVehicleWeaponBattery();
	virtual ~CVehicleWeaponBattery();

	static void InitTunables();

	virtual void ProcessControl(CVehicle* pParent);
	virtual void ProcessPostPreRender(CVehicle* pParent);
	virtual void ProcessPreRender(CVehicle* pParent);

	virtual s32 GetType() const { return VGT_VEHICLE_WEAPON_BATTERY; }

	virtual void Fire(const CPed* pFiringPed, CVehicle* pParent, const Vector3& vTargetPosition, const CEntity* pTarget = NULL, bool bForceShotInWeaponDirection = false);

	// Returns the first weapon's weapon info 
	// (although it is possible to have different weapon types in a single battery)
	virtual const CWeaponInfo* GetWeaponInfo() const;
	virtual u32 GetHash() const;

	void AddVehicleWeapon(CVehicleWeapon* pNewWeapon);
	const CVehicleWeapon* GetVehicleWeapon(s32 iIndex) const {return (Verifyf(iIndex < m_iNumVehicleWeapons && iIndex >= 0,"Invalid index")) ? m_VehicleWeapons[iIndex] : NULL; }
	CVehicleWeapon* GetVehicleWeapon(s32 iIndex) {return (Verifyf(iIndex < m_iNumVehicleWeapons && iIndex >= 0,"Invalid index")) ? m_VehicleWeapons[iIndex] : NULL; }
	
	s32 GetNumWeaponsInBattery() const {return m_iNumVehicleWeapons; }
	
	eMissileBatteryState GetMissileBatteryState() const { return m_MissileBatteryState; }
	void SetMissileBatteryState(eMissileBatteryState state) { m_MissileBatteryState = state; }
	eMissileFlapState GetMissileFlapState(s32 iIndex) const { return m_MissileFlapState[iIndex]; }
	void SetMissileFlapState(s32 iIndex, eMissileFlapState state) { m_MissileFlapState[iIndex] = state; }

	void DeleteAllVehicleWeapons();

	virtual void SetOverrideWeaponDirection(const Vector3 &vOverrideDirection);

	virtual void SetOverrideLaunchSpeed(float fOverrideLaunchSpeed);

	s32 GetFiredWeaponCounter() const { return m_iCurrentWeaponCounter; }
	void IncrementCurrentWeaponCounter(CVehicle& rVeh);

	bool CanFire() { return fwTimer::GetTimeInMilliseconds() > m_uNextLaunchTime; }

	s32 GetLastFiredWeapon() const { return m_iLastFiredWeapon; }
	s32 GetTimeToNextLaunch() const { return rage::Max((s32)m_uNextLaunchTime - (s32)fwTimer::GetTimeInMilliseconds(), 0); }

	virtual bool IsUnderwater(const CVehicle* pParent) const;

#if !__NO_OUTPUT
	virtual const char* GetName() const;
#endif

#if !__FINAL
	virtual bool HasWeapon(const CWeapon* pWeapon) const;
#endif


private:

	static float sm_fOppressor2MissileAlternateWaitTimeOverride;

	CVehicleWeapon* m_VehicleWeapons[MAX_NUM_BATTERY_WEAPONS];
	eMissileFlapState m_MissileFlapState[MAX_NUM_BATTERY_WEAPONS];	// Probably should live on vehicle weapon?
	eMissileBatteryState m_MissileBatteryState;
	s32 m_iLastFiredWeapon;

	s32 m_iNumVehicleWeapons;

	// Weapons are not fired in order
	s32 m_iCurrentWeaponCounter;

	u32 m_uNextLaunchTime;

#if !__NO_OUTPUT
	atString m_debugName;
#endif
};

// Num CVehicleWeapons per vehicle
// e.g. a primary and secondary
// note: 1 weapon could be a battery of rocket launchers
class CVehicleWeaponMgr
{
public:

	CVehicleWeaponMgr();
	virtual ~CVehicleWeaponMgr();

	void ProcessControl(CVehicle* pParent);
	void ProcessPreRender(CVehicle* pParent);
	void ProcessPostPreRender(CVehicle* pParent);
	void ProcessPhysics(CVehicle* pParent, float fTimeStep, int nTimeSlice);
	void ProcessPreComputeImpacts(CVehicle* pVehicle, phCachedContactIterator &impacts);

	void AimTurretsAtWorldPos(const Vector3 &vWorldPos, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim);
	void AimTurretsAtLocalDir(const Vector3 &vLocalDir, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim);

	bool AddVehicleWeapon(CVehicleWeapon* pVehicleWeapon, s32 seatIndex);
	CVehicleWeapon* GetVehicleWeapon(s32 iIndex) const { Assert(iIndex < m_iNumVehicleWeapons && iIndex > -1); return m_pVehicleWeapons[iIndex]; }
	s32 GetVehicleWeaponIndexByHash(u32 iHash) const;
	void DisableAllWeapons(CVehicle* pParent);
	void DisableWeapon( bool bDisable, CVehicle* pParent, int index );

	bool AddTurret(CTurret* pTurret, s32 seatIndex, s32 weaponIndex);
	CTurret* GetTurret(s32 iIndex) const { Assert(iIndex < m_iNumTurrets && iIndex > -1); return m_pTurrets[iIndex]; }

	void GetWeaponsForSeat(s32 seatIndex, atArray<CVehicleWeapon*>& weaponOut);
	void GetWeaponsForSeat(s32 seatIndex, atArray<const CVehicleWeapon*>& weaponOut) const;
	void GetWeaponsAndTurretsForSeat(s32 seatIndex, atArray<CVehicleWeapon*>& weaponOut, atArray<CTurret*>& turretOut);
	void GetWeaponsAndTurretsForSeat(s32 seatIndex, atArray<const CVehicleWeapon*>& weaponOut, atArray<const CTurret*>& turretOut) const;
	CTurret* GetFirstTurretForSeat(s32 seatIndex) const;
	CFixedVehicleWeapon* GetFirstFixedWeaponForSeat(s32 seatIndex) const;

	s32 GetNumWeaponsForSeat(s32 seatIndex) const;
	s32 GetNumTurretsForSeat(s32 seatIndex) const;
	s32 GetSeatIndexForTurret(const CTurret& rTurret) const;
	s32 GetWeaponIndexForTurret(const CTurret& rTurret) const;
#if GTA_REPLAY
	s32 GetSeatIndexFromWeaponIdx(s32 weaponIndex) const;
#endif 

	void Fire(const CPed* pFiringPed, CVehicle* pParent, s32 iWeaponIndex);

	bool WantsToBeAwake(CVehicle* pParent);

	s32 GetNumVehicleWeapons() const {return m_iNumVehicleWeapons;}
	s32 GetNumTurrets() const {return m_iNumTurrets;}

	void ResetIncludeFlags(CVehicle* pParent);

	// Returns the weapon battery with weapons of the fire type specified (projectile, instant hit, etc.)
	CVehicleWeaponBattery* GetWeaponBatteryOfType(eFireType fireType);

	s32 GetIndexOfVehicleWeapon(const CVehicleWeapon* pVehicleWeapon) const;

protected:
	CTurret* m_pTurrets[MAX_NUM_VEHICLE_TURRETS];
	CVehicleWeapon* m_pVehicleWeapons[MAX_NUM_VEHICLE_WEAPONS];

	s32 m_WeaponSeatIndices[MAX_NUM_VEHICLE_WEAPONS];			// Vehicle seat index that an entry in m_pVehicleWeapons is associated with
	s32 m_TurretSeatIndices[MAX_NUM_VEHICLE_TURRETS];			// Vehicle seat index that an entry in m_pTurrets is associated with
	s32 m_TurretWeaponIndices[MAX_NUM_VEHICLE_TURRETS];			// Vehicle weapon index (to be used as m_pVehicleWeapons lookup) that an entry in m_pTurrets is associated with

	s32 m_iNumTurrets;
	s32 m_iNumVehicleWeapons;

	// Static functions
public:
	
	// Creates a weapon manager if one is needed for this vehicle
	// otherwise returns null
	static CVehicleWeaponMgr* CreateVehicleWeaponMgr(CVehicle* pVehicle); 

	static bool IsValidModTypeForVehicleWeapon(eVehicleModType modtype);
	
};


//////////////////////////////////////////////////////////////////////////

// Lives on any CVehicle, and will attach its specified attach bone to a CTrailer when in range
class CVehicleTrailerAttachPoint : public CVehicleGadget
{
public:
	// iAttachBoneIndex - The attach bone whose position we use to search for trailers near by. E.g. a truck's rear hook
	CVehicleTrailerAttachPoint(s32 iAttachBoneIndex);

	s32 GetType() const { return VGT_TRAILER_ATTACH_POINT; }

	virtual void ProcessControl(CVehicle* pParent);

	void DetachTrailer(CVehicle* pParent);

	CTrailer* GetAttachedTrailer(const CVehicle* const pParent) const;

    void SetAttachedTrailer( CVehicle *pTrailer );

	static bank_float ms_fAttachRadius;				// Tolerance distance on attach bones before attach can be made

protected:
	s32 m_iAttachBone;

	RegdPhy m_pTrailer;

	// Dont try to re attach immediately after detaching
	u32 m_uIgnoreAttachTimer;

	// Static config variables
	static bank_float ms_fAttachSearchRadius;		// Distance of initial shapetest to look for trailers
	static bank_u32 ms_uAttachResetTime;

};


//////////////////////////////////////////////////////////////////////////

//Base class for gadgets with articulated joints.
class CVehicleGadgetWithJointsBase : public CVehicleGadget
{
public:
	CVehicleGadgetWithJointsBase( s16 iNumberOfJoints, 
                                  float fMoveSpeed,
                                  float fMuscleStiffness,
                                  float fMuscleAngleOrPositionStrength,
                                  float fMuscleSpeedStrength,
                                  float fMinAndMaxMuscleTorqueOrForce );

	~CVehicleGadgetWithJointsBase();

    virtual void    ProcessPrePhysics(CVehicle* pParent);
	virtual void	ProcessPhysics(CVehicle* pParent, const float fTimeStep, const int nTimeSlice);

	void			InitJointLimits(const CVehicleModelInfo* pVehicleModelInfo);
	void			SetupTransmissionScale(const CVehicleModelInfo* pVehicleModelInfo, s32 boneIndex);
    bool            SetJointLimits(CVehicle* pParent, s16 iJointIndex, const float fOpenAngle, const float fClosedAngle);//modify joint limits, returns true if they have been modified successfully

    void            DriveJointToRatio(CVehicle* pParent, s16 iJointIndex, float fTargetRatio);

	virtual bool	WantsToBeAwake(CVehicle*);

	JointInfo		*GetJoint(s16 iJointIndex);


protected:
	s16			m_iNumberOfJoints;
	JointInfo	*m_JointInfo;
	float		m_fMoveSpeed;

    float       m_fMuscleStiffness;
    float       m_fMuscleAngleOrPositionStrength;
    float       m_fMuscleSpeedStrength;
    float       m_fMinAndMaxMuscleTorqueOrForce;
};



//////////////////////////////////////////////////////////////////////////

// A digger arm is rotated to pick things up
class CVehicleGadgetDiggerArm : public CVehicleGadgetWithJointsBase
{
public:
	CVehicleGadgetDiggerArm(s32 iDiggerArmBoneIndex, float fMoveSpeed);

	s32 GetType() const { return VGT_DIGGER_ARM; }
	void ProcessControl(CVehicle* pParent);
	void InitAudio(CVehicle* pParent);

	void SetDesiredPitch( float desiredPitch){GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = desiredPitch;}
	void SetSnapToPosition( bool bSnapToPosition){GetJoint(0)->m_1dofJointInfo.m_bSnapToAngle = bSnapToPosition;}
	void SetDesiredAcceleration( float acceleration){GetJoint(0)->m_fDesiredAcceleration = acceleration;}

	float GetJointPositionRatio();
	void SetDefaultRatioOverride(bool bRatioOverride) { m_bDefaultRatioOverride = bRatioOverride; }

	// network methods:
	virtual bool IsNetworked() const { return true; }
	virtual void Serialise(CSyncDataBase& serialiser);

protected:
	bool m_bDefaultRatioOverride;
	audVehicleDigger*	m_VehicleDiggerAudio;
	float m_fJointPositionRatio;
};

/*
//Naming of the different parts of a Backhoe/JCB
//					____
//				   /	|
//				  /  /| |
//			Boom-/  / | |
//		_______	/  /  | |-Stick
//		|	  |/  /	  |	|
//	 ___|	  |__/   /__/-Bucket
//	(			 )
//	(____________)-Base
//
*/

enum eDiggerJoints
{
	DIGGER_JOINT_BASE,
	DIGGER_JOINT_BOOM,
	DIGGER_JOINT_STICK,
	DIGGER_JOINT_BUCKET,
	DIGGER_JOINT_MAX,
};

// An articulated digger arm is moved to dig up dirt
class CVehicleGadgetArticulatedDiggerArm : public CVehicleGadgetWithJointsBase
{
public:
	CVehicleGadgetArticulatedDiggerArm( float fMoveSpeed );

	void	SetDiggerJointBone(eDiggerJoints diggerJointType, s32 iBoneIndex);

	s32 	GetType() const { return VGT_ARTICULATED_DIGGER_ARM; }
	void	InitAudio(CVehicle* pParent);
	void	ProcessControl(CVehicle* pParent);

	s32		GetNumberOfSetupDiggerJoint();//returns the number of joints that have valid bone indices
    bool    GetJointSetup(eDiggerJoints diggerJoint);
	float	GetJointAngle(eDiggerJoints diggerJointType) { return GetJoint((s16)diggerJointType)->m_1dofJointInfo.m_fAngle; }
	void	SetDesiredAcceleration(eDiggerJoints diggerJointType, float acceleration){GetJoint((s16)diggerJointType)->m_fDesiredAcceleration = acceleration;}

protected:
	audVehicleDigger*	m_VehicleDiggerAudio;
	float				m_fMoveSpeed;
};

//////////////////////////////////////////////////////////////////////////

struct ParkingSensorInfo
{
	Vector3 startPosition;
	Vector3 endPosition;
};

#define PARKING_SENSOR_CAPSULE_MAX 3
// Beeps when something is close to the back of a car
class CVehicleGadgetParkingSensor : public CVehicleGadget
{
public:
	CVehicleGadgetParkingSensor(CVehicle* pParent, float parkingSensorRadius = 0.4f, float parkingSensorLength = 2.5f);

	s32				GetType() const { return VGT_PARKING_SENSOR; }
	virtual void	ProcessControl(CVehicle* pParent);

protected:

	void			Init(CVehicle* pParent);

	bool				m_BeepPlaying;
	float				m_TimeToStopParkingBeep;
	float				m_ParkingSensorRadius;
	float				m_ParkingSensorLength;
	float				m_VehicleWidth;
	ParkingSensorInfo	m_ParkingSensorCapsules[PARKING_SENSOR_CAPSULE_MAX];
};

//////////////////////////////////////////////////////////////////////////

// Tow truck arm and hook
class CVehicleGadgetTowArm : public CVehicleGadgetWithJointsBase
{
public:
	CVehicleGadgetTowArm(s32 iTowArmBoneIndex, s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex, float fMoveSpeed);
	~CVehicleGadgetTowArm();

    void DeleteRopesHookAndConstraints(CVehicle* pParent = NULL);

	s32 GetType() const { return VGT_TOW_TRUCK_ARM; }

	void SetPropObject(CObject* pObject) { m_PropObject = pObject; }
	CObject* GetPropObject() { return m_PropObject; }

	void Init(CVehicle* pParent, const CVehicleModelInfo* pVehicleModelInfo);

    virtual void  BlowUpCarParts(CVehicle* pParent);

	void SetDesiredPitch( CVehicle *pVehicle,  float desiredPitch);
    void SetDesiredPitchRatio( CVehicle *pVehicle, float desiredPitchRatio);
	void SetDesiredAcceleration( CVehicle *pVehicle,  float acceleration);
	void SetIsTeleporting(bool bIsTeleporting) { m_bIsTeleporting = bIsTeleporting; }

    void ProcessPreComputeImpacts(CVehicle *pVehicle, phCachedContactIterator &impacts);
	void ProcessPhysics(CVehicle* pParent, const float fTimestep, const int nTimeSlice);
	void ProcessControl(CVehicle* pParent);
	bool WantsToBeAwake(CVehicle *pVehicle);

	void ProcessTowArmAndHook(CVehicle *pVehicle);
    
	void ChangeLod(CVehicle & parentVehicle, int lod);

	virtual void SetIsVisible(CVehicle* pParent, bool bIsVisible);
    
    void DetachVehicle(CVehicle *pTowTruck = NULL, bool bReAttachHook = true, bool bImmediateDetach = false);  // Sets a flag to detach the vehicle
    const CVehicle* GetAttachedVehicle() const;

    void AttachRopesToObject( CVehicle* pParent, CPhysical *pObject, s32 iComponentIndex = 0, bool REPLAY_ONLY(bReattachHook) = false );

    void AttachVehicleToTowArm( CVehicle *pTowTruck, CVehicle *pVehicleToAttach, const Vector3 &vPositionToAttach, s32 iComponentIndex = 0 );
    void AttachVehicleToTowArm( CVehicle *pTowTruck, CVehicle *pVehicleToAttach, s16 vehicleBoneIndex, const Vector3 &vVehicleOffsetPosition, s32 iComponentIndex = 0 );

	int GetGadgetEntities(int entityListSize, const CEntity ** entityListOut) const;

	void WarpToPosition(CVehicle* pTowTruck, bool bUpdateConstraints = true);
	void Teleport(CVehicle *pTwoTruck, const Matrix34 &matOld);

	ropeInstance* GetRopeA() const { return  m_RopeInstanceA; }
	ropeInstance* GetRopeB() const { return  m_RopeInstanceB; }
	void SetRopeA(ropeInstance* rope) { m_RopeInstanceA = rope; }
	void SetRopeB(ropeInstance* rope) { m_RopeInstanceB = rope; }

	bool IsRopeAndHookInitialised() const { return m_bRopesAndHookInitialized; }

#if GTA_REPLAY
	void SnapToAngleForReplay(float snapToAngle);
#endif //GTA_REPLAY

protected:

    void ActuallyDetachVehicle( CVehicle *pTowTruck, bool bReattachHook = true );  // This function actually detached the vehicle inside process physics
    void UpdateConstraintDistance( CVehicle *pTowTruck );
	void UpdateHookPosition(CVehicle *pTowTruck, bool bUpdateConstraints = false);
	void UpdateTowedVehiclePosition(CVehicle *pTowTruck, CVehicle *pVehicleToAttach);

    float GetTowArmPositionRatio();
    void GetTowHookIdealPosition(CVehicle *pParent, Vector3 &vecTowHookIdealPosition_Out, float fTowArmPositionRatio);

	void SetDesiredHookPositionWithDamage(CVehicle *pVehicleToAttach, float hookVehicleOffsetY, float hookPointPositionZ);

    void EnforceConstraintLimits(phConstraintRotation* pConstraint, float fMin, float fMax, float fCurrent, bool bIsPitch);

	void AddRopesAndHook(CVehicle & parentVehicle);
	void RemoveRopesAndHook();
	inline bool HasTowArm() {return m_iNumberOfJoints > 0;}
	bool IsPickUpFromTopVehicle(const CVehicle *pParent) const;

	bool    m_Setup;
	bool	m_bHoldsDrawableRef;
	bool	m_bRopesAndHookInitialized;
	bool	m_bJointLimitsSet;
	bool	m_bIsTeleporting;
	s32     m_iTowMountAChild;
	s32     m_iTowMountA;

	s32     m_iTowMountBChild;
	s32     m_iTowMountB;
	f32		m_fPrevJointPosition;

    float   m_LastTowArmPositionRatio;//used to work out whether we should be winding the rope

    bool    m_HookRestingOnTruck;
    Vector3 m_vLastFramesDistance;

	RegdObj m_PropObject;

    float m_fForceMultToApplyInConstraint;

	audVehicleTowTruckArm*	m_VehicleTowArmAudio;
	ropeInstance*       m_RopeInstanceA;
	ropeInstance*       m_RopeInstanceB;
    phInst*             m_RopesAttachedVehiclePhysicsInst;  // The physics inst the rope is attached to

    phConstraintHandle  m_posConstraintHandle;
    phConstraintHandle  m_sphericalConstraintHandle;
    phConstraintHandle  m_rotYConstraintHandle;

	RegdPhy m_pTowedVehicle;
    bool    m_bDetachVehicle;
	u32     m_uIgnoreAttachTimer;   // Don't try to re attach immediately after detaching
	Vector3 m_vTowedVehicleOriginalAngDamp;

    Vector3 m_vStartTowHookPosition;// varaible to deal with lerping the initial hoom position to the middle of the car
    Vector3 m_vCurrentTowHookPosition;
    Vector3 m_vDesiredTowHookPosition;

    float m_fTowTruckPositionTransitionTime; //1.0f if the tow hook has moved to the center of the car

	Vec3V m_vLocalPosConstraintStart;

    bool m_bUsingSphericalConstraint;
    bool m_bTowTruckTouchingTowVehicle;
	bool m_bIsVisible;

	// Static config variables
	static bank_float   ms_fAttachSearchRadius;         // Distance of initial shape test to look for trailers
	static bank_float   ms_fAttachRadius;               // Tolerance distance on attach bones before attach can be made
	static bank_u32     ms_uAttachResetTime;
};


//////////////////////////////////////////////////////////////////////////

// A forklift's forks
class CVehicleGadgetForks : public CVehicleGadgetWithJointsBase
{
public:
	enum PalletBounds
	{
		// The "extra" bounds added at runtime for better stability.
		PALLET_EXTRA_BOX_BOUND_MIDDLE,
		PALLET_EXTRA_BOX_BOUND_TOP,
		PALLET_EXTRA_BOX_BOUND_BOTTOM,

		NUM_EXTRA_PALLET_BOUNDS
	};

	CVehicleGadgetForks(s32 iForksBoneIndex, s32 iCarriageBoneIndex, float fMoveSpeed);

	s32 GetType() const { return VGT_FORKS; }

	void Init(CVehicle* pVehicle);
	virtual void InitJointLimits(const CVehicleModelInfo* pModelInfo);

	static void CreateNewBoundsForPallet(phArchetypeDamp* pPhysics, float vBoundingBoxMinZ, float vBoundingBoxMaxZ, bool bAddTopAndBottomBoxBounds);
	static void SetTypeAndIncludeFlagsForPallet(CObject* pObject, u32 nIndexOfFirstExtraBoxBound, const bool bAddTopAndBottomBoxBounds);

	void SetDesiredHeight( float desiredPosition){GetJoint(0)->m_PrismaticJointInfo.m_fDesiredPosition = desiredPosition;}
	void SetDesiredAcceleration( float acceleration);

    void ProcessControl(CVehicle* pParent);
    void ProcessPreComputeImpacts(CVehicle *pVehicle, phCachedContactIterator &impacts);

	void AttachPalletInstanceToForks(CEntity* pPallet);
	void DetachPalletFromForks();
	void DetachPalletImmediatelyAndCleanUp(CVehicle* pParent);
    bool IsAtLowestPosition();
	bool CanMoveForksUp() { return m_bCanMoveForksUp; }
	bool CanMoveForksDown() { return m_bCanMoveForksDown; }

	void LockForks() { m_bForksLocked = true; }
	void UnlockForks() { m_bForksLocked = false; }
	bool AreForksLocked() const { return m_bForksLocked; }

	virtual bool WantsToBeAwake(CVehicle*);

	// Does a breadth first walk over the tree of attachments to the pallet and stores a list
	// of all nodes in the atArray passed in.
	void PopulateListOfAttachedObjects();

	// Get a reference to the list objects attached to the forks. The camera code (for example) can query this
	// to know which objects it should ignore occlusion clipping on.
	// NB - This is a const-reference since the forklift attach/detach code should be the only thing altering
	// this list!
	atArray<CPhysical*> const& GetListOfAttachedObjects() const { return m_AttachedObjects; }

#if __BANK
	static void RenderDebug();
#endif // __BANK

#if __BANK
	static bool ms_bShowAttachFSM;
	static bool ms_bDrawForkAttachPoints;
	static bool ms_bHighlightExtraPalletBoundsInUse;
	static bool ms_bDrawContactsInExtraPalletBounds;
	static bool ms_bRenderDebugInfoForSelectedPallet;
	static bool ms_bRenderTuningValuesForSelectedPallet;
	static float ms_fPalletAttachBoxWidth;
	static float ms_fPalletAttachBoxLength;
	static float ms_fPalletAttachBoxHeight;
	static float ms_vPalletAttachBoxOriginX;
	static float ms_vPalletAttachBoxOriginY;
	static float ms_vPalletAttachBoxOriginZ;
#endif // __BANK

	// network methods:
	virtual bool IsNetworked() const { return true; }
	virtual void Serialise(CSyncDataBase& serialiser);

protected:

	enum eForkLiftAttachState
	{
		UNATTACHED = 0,
		READY_TO_ATTACH,
		ATTACHING,
		ATTACHED,
		DETACHING
	};

	audVehicleForks* m_VehicleForksAudio;
	eForkLiftAttachState m_attachState;

	f32 m_PrevPrismaticJointDistance;
    s32 m_iForksChild;
	s32 m_iForks;

    s32 m_iCarriageChild;
    s32 m_iCarriage;

    // Constraint for attaching pallet to forklift.
    phConstraintHandle m_constraintHandle;

	// This is a reference to the pallet for attaching / detaching.
	RegdEnt m_pAttachEntity;

	atArray<CPhysical*> m_AttachedObjects;

	// This variable is used to force a pallet to be attached, usually after warping it through script.
	bool m_bForceAttach;

	bool m_bCanMoveForksUp;
	bool m_bCanMoveForksDown;

	// Set by script to stop the forks moving under player control.
	bool m_bForksLocked;

	// Used for deciding on the context of whether the forks should collide against the middle box bound
	// or the two outer box bounds.
	bool m_bForksWithinPalletMiddleBox;
	bool m_bForksInRegionAboveOrBelowPallet;
	bool m_bForksWereWithinPalletMiddleBox;
	bool m_bForksWereInRegionAboveOrBelowPallet;
};

//////////////////////////////////////////////////////////////////////////

// Class to support the "Handler" vehicle.
class CVehicleGadgetHandlerFrame : public CVehicleGadgetWithJointsBase
{
public:
	CVehicleGadgetHandlerFrame(s32 iFrame1BoneIndex, s32 iFrame2BoneIndex, float fMoveSpeed);

	s32 GetType() const { return VGT_HANDLER_FRAME; }

	void Init(CVehicle* pVehicle);
	virtual void InitJointLimits(const CVehicleModelInfo* pModelInfo);

	void SetDesiredHeight( float desiredPosition) { GetJoint(1)->m_PrismaticJointInfo.m_fDesiredPosition = desiredPosition; }
	void SetDesiredAcceleration( float acceleration);
	float GetCurrentHeight() { return GetJoint(1)->m_PrismaticJointInfo.m_fPosition; }

	void ProcessControl(CVehicle* pParent);
	void ProcessPreComputeImpacts(CVehicle* pVehicle, phCachedContactIterator& impacts);

	bool TestForGoodAttachmentOrientation(CVehicle* pHandler, CEntity* pContainer);
	void AttachContainerInstanceToFrame(CEntity* pContainer);
	void AttachContainerInstanceToFrameWhenLinedUp(CEntity* pContainer);
	void DetachContainerFromFrame();
	bool IsAtLowestPosition();
	bool IsReadyToAttach() const { return m_bReadyToAttach; }
	bool CanMoveFrameUp() { return m_bCanMoveFrameUp; }
	bool CanMoveFrameDown() { return m_bCanMoveFrameDown; }
	void SetTryingToMoveFrameDown(bool bValue) { m_bTryingToMoveFrameDown = bValue; }

	void LockFrame() { m_bHandlerFrameLocked = true; }
	void UnlockFrame() { m_bHandlerFrameLocked = false; }
	bool IsFrameLocked() const { return m_bHandlerFrameLocked; }

	virtual bool WantsToBeAwake(CVehicle* pParent);

	bool IsContainerAttached() { return m_attachEntity.GetEntity()!=NULL; }
	const CEntity* GetAttachedObject() const { return m_attachEntity.GetEntity(); }

#if __BANK
	void DebugFrameMotion(CVehicle* pParent);
	static bool ms_bShowAttachFSM;
	static bool ms_bDebugFrameMotion;
#endif // __BANK

	// network methods:
	virtual bool IsNetworked() const { return true; }
	virtual void Serialise(CSyncDataBase& serialiser);

protected:

	enum eHandlerAttachState
	{
		UNATTACHED = 0,
		READY_TO_ATTACH,
		ATTACHING,
		ATTACHED,
		DETACHING
	};

	eHandlerAttachState m_attachState;

	s32 m_iFrame1;
	s32 m_iFrame2;

	// Constraint for attaching container to Handler.
	phConstraintHandle m_constraintHandle;

	// This is a reference to the container for attaching / detaching.
	CSyncedEntity m_attachEntity;

	// The matrix of the container at the initial point of attaching.
	Matrix34 m_matContainerInitial;

	// Scalar value to track how far the interpolation from initial to ideal attach matrix we are.
	float m_fInterpolationValue;

	// The position we are driving the frame to while attaching.
	float m_fFrameDesiredAttachHeight;

	//atArray<CPhysical*> m_AttachedObjects;

	// This variable is used to force a container to be attached, usually after warping it through script.
	bool m_bForceAttach;

	// Is the frame in a good position to attach to the container?
	bool m_bReadyToAttach;

	bool m_bCanMoveFrameUp;
	bool m_bCanMoveFrameDown;

	// Is the player pushing down on the left stick?
	bool m_bTryingToMoveFrameDown;

	// The position at which we disabled moving the frame down.
	float m_fLockedDownwardMotionPos;

	// How many frames have we gone without any contact on the frame.
	u32 m_nFramesWithoutCollisionHistory;

	// Set by script to stop the frame moving under player control.
	bool m_bHandlerFrameLocked;

	audVehicleHandlerFrame* m_VehicleHandlerAudio;
	f32 m_PrevPrismaticJointDistance;

	// Some tunable parameters for the attachment tests.
#if __BANK
public:
#endif // __BANK
	static float ms_fTestSphereRadius;
	static float ms_fTransverseOffset;
	static float ms_fLongitudinalOffset;
	static float ms_fFrameUpwardOffsetAfterAttachment;
};

//////////////////////////////////////////////////////////////////////////

//This gadget rotates the thresher parts of the combine harvester
class CVehicleGadgetThresher : public CVehicleGadgetWithJointsBase
{
public:
    CVehicleGadgetThresher(CVehicle* pParent, s32 iReelBoneId, s32 iAugerBoneId, float fRotationSpeed);

    s32				GetType() const { return VGT_THRESHER; }
    virtual void	ProcessPhysics(CVehicle* pParent, const float fTimeStep, const int nTimeSlice);

protected:

    s32		m_iReelBoneId;
    s32		m_iAugerBoneId;

    float   m_ThresherAngle;
    float	m_fRotationSpeed;
};

//This gadget opens and closes bomb bay doors
class CVehicleGadgetBombBay : public CVehicleGadgetWithJointsBase
{
public:
    CVehicleGadgetBombBay( s32 iDoorOneBoneId, s32 iDoorTwoBoneId, float fSpeed);

    s32				GetType() const { return VGT_BOMBBAY; }
    virtual void	ProcessPhysics(CVehicle* pParent, const float fTimeStep, const int nTimeSlice);

    void            OpenDoors(CVehicle* pVehicle);
    void            CloseDoors(CVehicle* pVehicle);
	bool			GetOpen() { return m_bOpen; }

	// network methods:
	virtual bool IsNetworked() const					{ return true; }
	virtual void Serialise(CSyncDataBase& serialiser);

protected:

    s32		m_iDoorOneBoneId;
    s32		m_iDoorTwoBoneId;

    float	m_fSpeed;
	bool	m_bOpen;
	bool	m_bShouldBeOpen;
};


//This gadget rotates the boat boom
class CVehicleGadgetBoatBoom : public CVehicleGadgetWithJointsBase
{
public:
    CVehicleGadgetBoatBoom( s32 iBoomBoneId, float fRotationSpeed);

    s32				GetType() const { return VGT_BOATBOOM; }

    virtual void	ProcessPhysics(CVehicle* pParent, const float fTimeStep, const int nTimeSlice);

    void            SwingInBoatBoom(CVehicle* pVehicle);
    void            SwingOutBoatBoom(CVehicle* pVehicle);
    void            SwingToRatio(CVehicle* pVehicle, float fTargetRatio);
    void            SetSwingFreely(CVehicle* pVehicle, bool bSwingFreely);
    float           GetPositionRatio();
    void            SetAllowBoomToAnimate(CVehicle* pVehicle, bool bAnimateBoom);

protected:

    s32		m_iBoomBoneId;
    float	m_fRotationSpeed;
};

//////////////////////////////////////////////////////////////////////////

enum ePickupRopeGadgetType
{	
	PICKUP_HOOK,
	PICKUP_MAGNET,	
};

// Base for cargobob pick-up rope gadgets.
class CVehicleGadgetPickUpRope : public CVehicleGadget
{
public:
	CVehicleGadgetPickUpRope(s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex);
	~CVehicleGadgetPickUpRope();

	void DeleteRopesPropAndConstraints(CVehicle* pParent = NULL);

	s32 GetType() const { return VGT_PICK_UP_ROPE; }
	virtual ePickupRopeGadgetType GetPickupType() = 0;

	CObject* GetPropObject() { return m_PropObject; }

	void Init(CVehicle* pParent);

	void BlowUpCarParts(CVehicle* pParent);

	virtual void ProcessPreComputeImpacts(CVehicle *pVehicle, phCachedContactIterator &impacts);
	virtual void ProcessPhysics3(CVehicle* pParent, const float fTimestep);
	virtual void ProcessControl(CVehicle* pParent);
	bool WantsToBeAwake(CVehicle *pVehicle);

	void ChangeLod(CVehicle & parentVehicle, int lod);

	void DetachEntity(CVehicle *pParentVehicle = NULL, bool bReAttachHook = true, bool bImmediateDetach = false);  // Sets a flag to detach the vehicle
	const CVehicle* GetAttachedVehicle() const;
	const CPhysical* GetAttachedEntity() const;

	void AttachRopesToObject( CVehicle* pParent, CPhysical *pObject, s32 iComponentIndex = 0 );

	void AttachEntityToPickUpProp( CVehicle *pPickUpVehicle, CPhysical *pEntityToAttach, const Vector3 &vPositionToAttach, s32 iComponentIndex = 0, bool bNetAssert = true );
	virtual void AttachEntityToPickUpProp( CVehicle *pPickUpVehicle, CPhysical *pEntityToAttach, s16 iBoneIndex, const Vector3 &vVehicleOffsetPosition, s32 iComponentIndex = 0, bool bNetAssert = true );
	static bool ComputeIdealAttachmentOffset(const CEntity *pEntity, Vector3 &vIdealPosition);

	int GetGadgetEntities(int entityListSize, const CEntity **entityListOut) const;

	void WarpToPosition(CVehicle* pParentVehicle, bool bUpdateConstraints = true);
	void Teleport(CVehicle *pParentVehicle, const Matrix34 &matOld);

	void SetRopeLength(float fDetachedRopeLength, float fAttachedRopeLength, bool bSetRopeLengthInstantly);
	bool IsDetaching() const {return m_bDetachVehicle;}

	void SetForceNoVehicleDetach( bool bNoDetach ) { m_bForceDontDetachVehicle = bNoDetach; }
	bool GetForceNoVehicleDetach() { return m_bForceDontDetachVehicle; }
	void SetExcludeEntity( CPhysical* excludeEntity ) { m_excludeEntity = excludeEntity; }
	CEntity* GetExcludeEntity() { return m_excludeEntity.GetEntity(); }

	virtual void Trigger();
	
    float GetExtraPickupBoundAllowance() const { return m_fExtraBoundAttachAllowance; }
    void SetExtraPickupBoundAllowance( float extraAllowance ) { m_fExtraBoundAttachAllowance = extraAllowance; }

	// network methods:
	virtual bool IsNetworked() const { return true; }
	virtual void Serialise(CSyncDataBase&);

	float	m_fDampingMultiplier;

	bool	m_bEnsurePickupEntityUpright;	// If true, the attachment transition will always blend the picked-up entity such that it is the right way up relative to the pick-up prop.
											// Otherwise, the entity will blend to whichever OOB orientation is closest.

	// RAG overrides for tuning.
	static bool		ms_bShowDebug;
	static bool		ms_logCallStack;
	static bool		ms_bEnableDamping;
	static float	ms_fSpringConstant;
	static float	ms_fDampingConstant;	
	static float	ms_fDampingFactor;
	static float	ms_fMaxDampAccel;
		
protected:

	virtual void ProcessInRangeVehicle(CVehicle* pParent, CVehicle *pVehicle, Vector3 vPosition, Vector3 vNormal, int nComponent, phMaterialMgr::Id nMaterial, bool bIsClosest) = 0;
	virtual void ProcessInRangeObject(CVehicle* pParent, CObject *pObject, Vector3 vPosition, int nComponent, phMaterialMgr::Id nMaterial, bool bIsClosest) = 0;

	virtual void ProcessNothingInRange(CVehicle* pParent) = 0;

	virtual Matrix34 GetPickUpPropMatrix();

	virtual void ActuallyDetachVehicle( CVehicle *pParentVehicle, bool bReattachHook = true );  // This function actually detached the vehicle inside process physics
	void UpdateAttachments( CVehicle *pParentVehicle );
	void UpdateHookPosition(CVehicle *pParentVehicle, bool bUpdateConstraints = false);
	void UpdateTowedVehiclePosition(CVehicle *pParentVehicle, CPhysical *pEntityToAttach);

	void GetPickupPropIdealPosition(CVehicle *pParent, Vector3 &vecTowHookIdealPosition_Out);

	void EnforceConstraintLimits(phConstraintRotation* pConstraint, float fMin, float fMax, float fCurrent, bool bIsPitch);

	virtual void AddRopesAndProp(CVehicle & parentVehicle);
	void RemoveRopesAndProp();
	void ComputeAndSetInverseMassScale(CVehicle *pParentVehicle);

	inline bool CheckObjectDimensionsForPickup(CObject *pObject);
		
	virtual float GetPropObjectMass() = 0;
	virtual s32 GetPropRopeAttachBoneA() = 0;
	virtual s32 GetPropRopeAttachBoneB() = 0;
	virtual s32 GetPickupAttachBone() = 0;
	virtual Vector3 GetPickupAttachOffset() = 0;
	virtual u32 GetAttachResetTime() = 0; 
	virtual float GetEntitySearchRadius() = 0;
	virtual Vector3 GetEntitySearchCentreOffset() = 0;
	virtual bool GetAffectsVehicles() = 0;
	virtual bool GetAffectsObjects() = 0;
	virtual bool ShouldBeScanningForPickup() = 0;
	virtual bool UseIdealAttachPosition() = 0;
		
	bool    m_Setup;
	bool	m_bHoldsDrawableRef;
	bool	m_bRopesAndHookInitialized;
	s32     m_iTowMountAChild;
	s32     m_iTowMountA;

	s32     m_iTowMountBChild;
	s32     m_iTowMountB;
	f32		m_fPrevJointPosition;

	bool	m_bAdjustingRopeLength;
	float	m_fRopeLength;
	float	m_fPreviousDesiredRopeLength;
	float	m_fDetachedRopeLength;
	float	m_fAttachedRopeLength;
	float	m_fPickUpRopeLengthChangeRate;
	bool	m_bUnwindRope;

	//Vector3 m_vPickupPropPointDirection;

	bool m_bEnableDamping;
	Vector3 m_vLastFramesDistance;

	RegdObj m_PropObject;

	float m_fForceMultToApplyInConstraint;

	audVehicleGrapplingHook*	m_VehicleTowArmAudio;
	ropeInstance*       m_RopeInstanceA;
	ropeInstance*       m_RopeInstanceB;
	phInst*             m_RopesAttachedVehiclePhysicsInst;  // The physics inst the rope is attached to

	RegdPhy			m_pTowedEntity;
	CSyncedEntity	m_excludeEntity;
	bool    m_bDetachVehicle;
	bool	m_bForceDontDetachVehicle;
	bool	m_bEnableFreezeWaitingOnCollision;
	u32     m_uIgnoreAttachTimer;   // Don't try to re attach immediately after detaching
	Vector3 m_vTowedVehicleOriginalAngDamp;
	Vector3 m_vTowedVehicleOriginalAngDampC;

	// Information to allow for the pickup prop to lerp from the initial attach position to the "ideal" attach position (Usually so the object is centred on the pickup prop).
	Vector3 m_vStartPickupPropPosition;
	Vector3 m_vCurrentPickupPropPosition;
	Vector3 m_vDesiredPickupPropPosition;

	Quaternion m_qStartPickupPropRotation;
	Quaternion m_qCurrentPickupPropRotation;
	Quaternion m_qDesiredPickupPropRotation;

	float m_fPickupPositionTransitionTime; //1.0f if the tow hook has moved to the center of the car
	float m_fExtraBoundAttachAllowance;

private:

	inline void SetupRopeWithPickupProp(CVehicle *pParent);
	inline void HandleRopeVisibilityAndLength(CVehicle *pParent);
	inline void DoSmoothPickupTransition(CVehicle *pParent);
	inline void DampAttachedEntity(CVehicle *pParent);
	inline void ScanForPickup(CVehicle *pParent);

};

//////////////////////////////////////////////////////////////////////////

// Pick up rope with a hook.
class CVehicleGadgetPickUpRopeWithHook: public CVehicleGadgetPickUpRope
{
public:
	CVehicleGadgetPickUpRopeWithHook(s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex);

	virtual ePickupRopeGadgetType GetPickupType() {return PICKUP_HOOK; }

protected:
	virtual void ProcessInRangeVehicle(CVehicle* pParent, CVehicle *pVehicle, Vector3 vPosition, Vector3 vNormal, int nComponent, phMaterialMgr::Id nMaterial, bool bIsClosest); 
	virtual void ProcessInRangeObject(CVehicle* pParent, CObject *pObject, Vector3 vPosition, int nComponent, phMaterialMgr::Id nMaterial, bool bIsClosest); 
	virtual void ProcessNothingInRange(CVehicle* UNUSED_PARAM(pParent)) {}
		
	virtual float GetPropObjectMass() { return 50.0f; }
	virtual s32 GetPropRopeAttachBoneA() { return 2; }
	virtual s32 GetPropRopeAttachBoneB() { return 3; }
	virtual s32 GetPickupAttachBone() { return 0; }
	virtual Vector3 GetPickupAttachOffset() { return Vector3(0, 0, -0.25f); }
	virtual u32 GetAttachResetTime() { return 6000; }
	virtual float GetEntitySearchRadius() { return 0.4f; }
	virtual Vector3 GetEntitySearchCentreOffset() { return Vector3(0, 0, 0); }
	virtual bool GetAffectsVehicles() { return true; }
	virtual bool GetAffectsObjects() { return true; }
	virtual bool ShouldBeScanningForPickup() { return m_pTowedEntity ? false : true; }
	virtual bool UseIdealAttachPosition() { return true; }
};

//////////////////////////////////////////////////////////////////////////

// Pick up rope with a magnet.
class CVehicleGadgetPickUpRopeWithMagnet : public CVehicleGadgetPickUpRope
{
public:
	CVehicleGadgetPickUpRopeWithMagnet(s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex);
	~CVehicleGadgetPickUpRopeWithMagnet();

	s32 GetType() const { return VGT_PICK_UP_ROPE_MAGNET; }

	virtual ePickupRopeGadgetType GetPickupType() { return PICKUP_MAGNET; }

	virtual void ProcessPhysics3(CVehicle* pParent, const float fTimestep);
	virtual void ProcessPreComputeImpacts(CVehicle *pVehicle, phCachedContactIterator &impacts);
	virtual void ProcessControl(CVehicle* pParent);

	virtual void Trigger();
	virtual void ActuallyDetachVehicle( CVehicle *pParentVehicle, bool bReattachHook = true );  // This function actually detached the vehicle inside process physics

	// We do some extra stuff when something is attached to the magnet.
	virtual void AttachEntityToPickUpProp( CVehicle *pPickUpVehicle, CPhysical *pEntityToAttach, s16 iBoneIndex, const Vector3 &vVehicleOffsetPosition, s32 iComponentIndex = 0, bool bNetAssert = true );

	void InitAudio(CVehicle* pParent);	
	void SetTargetedMode(CPhysical *pTargetEntity);
	void SetAmbientMode(bool bAffectVehicles, bool bAffectObjects);

	CPhysical *GetTargetEntity() { return m_pAttractEntity; }

	bool GetIsActive() { return m_bActive; }
	
	// Network methods.
	virtual void Serialise(CSyncDataBase&);

	bool	m_bOwnerSetup;				// If we are running a clone version of the heli/magnet, we need to know if the owner has fully setup their ropes.
										// If they haven't, we override the network blender for the magnet to prevent non-physical behaviour being forced on our physical magnet/rope system.

	float	m_fMagnetStrength;			// Overall multiplier for the magnet force.
	float	m_fMagnetFalloff;			// Power that defines how the magnet force magnitude drops off the further from the magnet an entity is.
	
	float	m_fReducedMagnetStrength;	// Overall multiplier for the reduced magnet force applied while the magnet is not yet "active".
	float	m_fReducedMagnetFalloff;	// Falloff for the reduced magnet effect.
	
	float	m_fMagnetPullStrength;		// How strongly the magnet itself will pull towards the nearest entity.
	float	m_fMagnetPullRopeLength;	// How much extra rope can be provided to allow the magnet to pull itself to the entity.

	float	m_fMagnetEffectRadius;		// Size of the area of the magnet effect.

	// RAG overrides for tuning.
	static bool		ms_bOverrideParameters;

	static float	ms_fMagnetStrength;			
	static float	ms_fMagnetFalloff;			

	static float	ms_fReducedMagnetStrength;	
	static float	ms_fReducedMagnetFalloff;	

	static float	ms_fMagnetPullStrength;		
	static float	ms_fMagnetPullRopeLength;

	static float	ms_fMagnetEffectRadius;
	
private:
	void DoReducedMagnet(CVehicle* pParent, CPhysical *pPhysical, Vector3 vPosition, bool bIsClosest); //, Vector3 vNormal, int nComponent, bool bIsClosest);
	void DoFullMagnet(CVehicle* pParent);

	inline void PointMagnetAtTarget(CVehicle* pParent, Vector3 vTargetPos);
	
	bool	m_bFullyDisabled;			// No aspect of the magnet is active. For when there's no driver.
	bool	m_bActive;	

protected:

	virtual void ProcessInRangeVehicle(CVehicle* pParent, CVehicle *pVehicle, Vector3 vPosition, Vector3 vNormal, int nComponent, phMaterialMgr::Id nMaterial, bool bIsClosest); 
	virtual void ProcessInRangeObject(CVehicle* pParent, CObject *pObject, Vector3 vPosition, int nComponent, phMaterialMgr::Id nMaterial, bool bIsClosest);
	virtual void ProcessNothingInRange(CVehicle* pParent);

	virtual void AddRopesAndProp(CVehicle &parentVehicle);

	virtual Matrix34 GetPickUpPropMatrix();

	virtual float GetPropObjectMass() { return 50.0f; }
	virtual s32 GetPropRopeAttachBoneA() { return 1; }
	virtual s32 GetPropRopeAttachBoneB() { return 1; }
	virtual s32 GetPickupAttachBone() { return 2; }
	virtual Vector3 GetPickupAttachOffset();
	virtual u32 GetAttachResetTime() { return 0; }
	virtual float GetEntitySearchRadius() { return m_fMagnetEffectRadius; }
	virtual Vector3 GetEntitySearchCentreOffset() { return Vector3(0, 0, -(m_fMagnetEffectRadius * 0.4f)); }
	virtual bool GetAffectsVehicles() { return m_bAffectsVehicles; }
	virtual bool GetAffectsObjects() { return m_bAffectsObjects; }
	virtual bool ShouldBeScanningForPickup() { return true; }
	virtual bool UseIdealAttachPosition() { return false; }

	enum MagnetOperatingMode
	{
		VEH_PICK_UP_MAGNET_AMBIENT_MODE,				// The magnet will affect any entity of a type specified in the type include flags below. Only one entity can be properly attached.
		VEH_PICK_UP_MAGNET_TARGETED_MODE,				// The magnet will affect only the entity specified as the attract entity.
	};
	
	MagnetOperatingMode		m_nMode;
	
	bool					m_bAffectsVehicles;
	bool					m_bAffectsObjects;
		
	RegdPhy					m_pAttractEntity;			// The entity that is the target of the magnet in targeted mode.
		
	CSyncedEntity			m_pNetMagnetProp;
	CSyncedEntity			m_pNetAttractEntity;

	audVehicleGadgetMagnet*	m_pAudioMagnetGadget;
};

// For updating dynamic spoilers
class CVehicleGadgetDynamicSpoiler : public CVehicleGadget
{
public:
	CVehicleGadgetDynamicSpoiler();
	virtual void ProcessPrePhysics(CVehicle* pParent);
	virtual void Fix();
	virtual s32 GetType() const { return VGT_DYNAMIC_SPOILER; }

	void SetDynamicSpoilerEnabled(bool enabled) { m_bEnableDynamicSpoilers = enabled; }
	void InitAudio(CVehicle* pParent);
	void ResetSpoilerBones() {m_bResetSpoilerBones = true;}

protected:
	audVehicleGadgetDynamicSpoiler* m_pDynamicSpoilerAudio;
	float m_fCurrentSpoilerRise;
	float m_fCurrentStrutRise;
	float m_fCurrentSpoilerAngle;
	bool m_bEnableDynamicSpoilers;
	bool m_bResetSpoilerBones;
};

// For updating dynamic flaps
class CVehicleGadgetDynamicFlaps : public CVehicleGadget
{
public:
	CVehicleGadgetDynamicFlaps();
	virtual void ProcessPrePhysics(CVehicle* pParent);
	virtual void Fix();
	virtual s32 GetType() const { return VGT_DYNAMIC_SPOILER; }

	void SetDynamicFlapsEnabled(bool enabled) { m_bEnableDynamicFlaps = enabled; }
	void InitAudio(CVehicle* pParent);

protected:
	Vec3V m_vPrevVelocity;
	float m_fDesiredFlapAmount[ MAX_NUM_SPOILERS ];
	bool m_bEnableDynamicFlaps;
	bool m_bResetFlapBones;
};

#endif	// #ifdef __VEHICLE_GADGETS_H__

