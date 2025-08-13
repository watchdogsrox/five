/********************************************************************
filename:	Propeller.h
created:	01/10/2009
author:		jack.potter

purpose:	Code for handling rendering and collision processing
of vehicle propellers

*********************************************************************/

#ifndef __PROPELLER_H__
#define __PROPELLER_H__

// Rage headers
#include "phcore/materialmgr.h"
#include "physics/contactiterator.h"
#include "vector/vector3.h"
#include "data/base.h"
#if __BANK
#include "bank/bank.h"
#endif


// Game headers
#include "renderer/HierarchyIds.h"
#include "scene/RegdRefTypes.h"
#include "vehicles/VehicleDefines.h"


class CVehicle;

#define PROPELLER_INVALID_BONE_INDEX (0xffff)

// TODO: A lot of the members of CPropeller and CPropellerBlurred are type data, not instance
// and should be split into the model info

//////////////////////////////////////////////////////////////////////////
// Class: CPropeller
// Purpose:
//  Little class to deal with rotating propellers
//  Could be a vehicle gadget I guess
class CPropeller: public datBase		// Declaring data and virtuals in a base class causes bizarre code generation on x64?
{
public:
	CPropeller();
	virtual ~CPropeller() {}

	virtual void Init(eHierarchyId nComponent, eRotationAxis nRotateAxis, CVehicle* pParentVehicle);

	virtual void UpdatePropeller(float fPropSpeed, float fTimestep);
	virtual void SetPropellerSpeed(float fPropSpeed);
	virtual void PreRender(CVehicle* pParentVehicle);

	s32 GetBoneIndex() const { return m_uMainPropellerBone; }

	float GetSubmergeFraction(CVehicle* pParent);
	float GetSpeed() const { return m_fPropellerSpeed; }
	void SetSpeed(float fPropSpeed){ m_fPropellerSpeed = fPropSpeed;}
	float GetAngle() const { return m_fPropellerAngle; }
	u32 GetAxis() const { return m_nRotationAxis; }
	virtual void SetPropellerVisible( bool visible ) { m_nVisible = visible; }

#if __BANK
	static void InitWidgets(bkBank& pBank);
#endif

	void ApplyDeformation(CVehicle* pParentVehicle, const void* basePtr);
	Vector3 GetDeformation() const { return m_vDeformation; }
	void Fix() { m_vDeformation.Zero(); }

protected:
	float m_fPropellerSpeed;
	float m_fPropellerAngle;
	Vector3 m_vDeformation;

	u16 m_uMainPropellerBone;
	u32 m_nRotationAxis : 3;
	u32 m_nReverse : 1;
	u32 m_nVisible : 1;
};

//////////////////////////////////////////////////////////////////////////
// Class: CPropellerBlurred
// Purpose:
//	Extension of CPropeller to support rotor blades which are to blur at high speed.
//	Propellers need to be set up with child bones of the same name as their parent
//	appended with _fast and _slow.
//	The propeller will switch to these at a certain speed threshold.
class CPropellerBlurred : public CPropeller
{
public:
	CPropellerBlurred();
	virtual ~CPropellerBlurred() {}

	virtual void Init(eHierarchyId nComponent, eRotationAxis nRotateAxis, CVehicle* pParentVehicle);

	virtual void PreRender(CVehicle* pParentVehicle);
	virtual void UpdatePropeller(float fPropSpeed, float fTimestep);
	virtual void SetPropellerSpeed(float fPropSpeed);

#if __BANK
	static void AddWidgetsToBank(bkBank& bank);
#endif

protected:

	// These are children of the main properller bone
	// They containt render geometry to swap to when the blades spin fast
	u16 m_uFastBoneIndex;
	u16 m_uSlowBoneIndex;
	float m_fTimeInBlurredState;

	// At what propeller speed do we switch to fast propellers ?
	static bank_float ms_fTransitionAngleChangeThreshold;
	static bank_float ms_fBlurredSpeedReduceFactor;	// Should be in range 0 to 1
};

//////////////////////////////////////////////////////////////////////////
// Class: CPropellerCollision
// Purpose:
//  One of these is stored for every bit of propeller collision on a vehicle
//  Handles impact processing with for a single vehicle component.
//  It is assumed that the CPropellerCollision is owned by the CVehicle!
class CPropellerCollision
{
public:
	CPropellerCollision();

	void Init(int iFragChild) { Assert(iFragChild < 128); m_iFragChild = (s8)iFragChild; m_iFragGroup = -1; m_iFragDisc = m_iFragChild;}
	void Init(eHierarchyId nId, CVehicle* pOwner);

	// Processes a SINGLE impact
	// This does NOT iterate over the entire iterator
	// This is called within the sim update, causing impact data to be stored
	// and the impact disabled
	void ProcessPreComputeImpacts(CVehicle* pPropellerOwner, phContactIterator impact, float fPropSpeedMult = 1.0f);
	bool ShouldDisableImpact(CVehicle *pOwnerVehicle, CEntity *pOtherEntity, int nOwnerComponent, float fPropSpeedMult);
	bool ShouldDisableAndIgnoreImpact(CVehicle *pOwnerVehicle, CEntity *pOtherEntity, float fCollisionDepth) const;

	// Apply propeller collision to pOtherEntity
	// Note the vehicle itself processes this collision separately
	void ApplyImpact(CVehicle* pOwnerVehicle, CEntity* pOtherEntity, const Vector3& vecNormal, const Vector3& vecPos, const Vector3& vecOtherPos,
		int nOtherComp, int nPartIndex, phMaterialMgr::Id nMaterialId, float fPropSpeedMult, float fTimeStep, bool bIsPositiveDepth, bool bIsNewContact);

	s8 GetFragChild() const { return m_iFragChild; }
	s8 GetFragGroup() const { return m_iFragGroup; }
	s8 GetFragDisc() const {return m_iFragDisc;}

	void UpdateBound(CVehicle *pOwnerVehicle, int iBoneIndex, float fAngle, int iAxis, bool fullUpdate);
	bool IsPropellerComponent(const CVehicle *pOwnerVehicle, int nComponent) const;

protected:

	s8 m_iFragChild;
	s8 m_iFragGroup;
	s8 m_iFragDisc;
};

#define NUM_STORED_PROPELLER_IMPACTS (20)

//////////////////////////////////////////////////////////////////////////
// Class: CPropellerCollisionProcessor
// Purpose:
//  This class stores impacts with rotors over a frame and 
//  processes them at an appropriate time
//  This is necessary because we can't touch the physics level
//  during collision detection
class CPropellerCollisionProcessor
{
protected:
	class CPropellerImpactData
	{
	public:
		CPropellerImpactData();

		void Reset();

		RegdVeh m_pPropOwner;
		RegdEnt m_pOtherEntity;
		Vector3 m_vecHitNorm;
		Vector3 m_vecHitPos;
		Vector3 m_vecOtherPosOffset;
		CPropellerCollision* m_pPropellerCollision;
		int m_nOtherComponent;
		int m_nPartId;
		phMaterialMgr::Id m_nMaterialId;
		float m_fPropSpeedMult;		// range 0 - 1
		bool m_bIsPositiveDepth;
		bool m_bIsNewContact;
	};

public:
	CPropellerCollisionProcessor();

	void Reset();

	void ProcessImpacts(float fTimeStep);
	void AddPropellerImpact(CVehicle* pPropOwner, CEntity* pHitEntity, const Vector3& vecNorm, const Vector3& vecPos,
		CPropellerCollision* pPropellerCollision, int nOtherComponent, int nPartId, phMaterialMgr::Id nMaterialId, float fPropSpeedMult, bool bIsPositiveDepth, bool bIsNewContact);

	static CPropellerCollisionProcessor& GetInstance() { return sm_Instance; }

protected:
	int m_nNumRotorImpacts;
	CPropellerImpactData m_aStoredImpacts[NUM_STORED_PROPELLER_IMPACTS];

	static CPropellerCollisionProcessor sm_Instance;
};

#endif // __PROPELLER_H__
