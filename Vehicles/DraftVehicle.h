#ifndef __INC_DRAFTVEH_H__
#define __INC_DRAFTVEH_H__

//#include "physics\constrainthandle.h"
#include "vehicles/automobile.h"
#include "streaming/streamingrequest.h"

//////////////////////////////////////////////////////////////////////////
// Should be off to allow this to all be dead stripped
#define ENABLE_DRAFT_VEHICLE 0

//////////////////////////////////////////////////////////////////////////
typedef enum {
	DRAFT_HARNESS_LR = 0,
	DRAFT_HARNESS_RR,
	DRAFT_HARNESS_LM,
	DRAFT_HARNESS_RM,
	DRAFT_HARNESS_LF,
	DRAFT_HARNESS_RF,
	DRAFT_HARNESS_COUNT
} eDraftHarnessIndex;

#define MAX_DRAFT_ANIMALS DRAFT_HARNESS_COUNT

class CDraftVeh : public CAutomobile
{
public:

protected:
		// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
		// DONT make this public!
		CDraftVeh(const eEntityOwnedBy ownedBy, u32 popType);
		friend class CVehicleFactory;

public:
		~CDraftVeh();

		virtual int  InitPhys(); // During vehicle/frag initialization
		virtual void InitWheels();
		virtual void AddPhysics(); // On add to level and post-init stuff
		virtual void SetMatrix(const Matrix34& mat, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp);

		virtual bool ProcessShouldFindImpactsWithPhysical(const phInst* pOtherInst) const;

		virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);

		void SpawnDraftAnimals(const fwModelId& modelId, bool firstAttempt = false);
		void DeleteDraftAnimals();
		void DriveDraftAnimals();
		void UpdateDraftAnimalMatrix();

		CPed* GetDraftAnimalAtHarnessIndex(eDraftHarnessIndex harnessIndex) const;
		bool IsPedDrafted(CPed* ped) const;

		// Override these to provide more correct information about the direction the draft vehicle will be moving in
		virtual Vec3V_Out GetVehiclePositionForDriving() const;
		virtual Vec3V_Out GetVehicleForwardDirectionForDriving() const;
		virtual Vec3V_Out GetVehicleRightDirectionForDriving() const;
		virtual Vec3V_ConstRef GetVehicleForwardDirectionForDrivingRef() const;

private:
		virtual void SetPosition(const Vector3& vec, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp);
		virtual void SetHeading(float fHeading);
		void SnapDraftsToPosition();

		strRequest m_DraftTypeRequest;

		RegdPed m_pDraftAnimals[MAX_DRAFT_ANIMALS];
		Mat34V m_AveragedDraftAnimalMatrix;
		bool m_bDraftAnimalMatrixValid;

		float m_fThrottleRunningAverage;

		int		m_iDraftAnimalBoneIndices[MAX_DRAFT_ANIMALS];
#if ENABLE_HORSE
		float	m_fDraftAnimalCurSteer[MAX_DRAFT_ANIMALS];
#endif
};

#endif
