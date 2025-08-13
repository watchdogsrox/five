// Filename   :	ThreatenedHelper.h
// Description:	Helper class for natural motion tasks which need threatened behaviour.

#ifndef INC_THREATENEDHELPER_H_
#define INC_THREATENEDHELPER_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// This class is NOT a task, it is NOT derived from CTaskNMBehaviour, it's just a helper
// that can manage the threatened behavior. It does have methods for handling messages,
// controlling and monitoring, and aborting that should be passed down / called from
// the owner task.
class CThreatenedHelper
{
public:

	typedef enum eUseOfGun
	{
		THREATENED_GUN_NO_USE,
		THREATENED_GUN_THREATEN,
		THREATENED_GUN_THREATEN_AND_FIRE
	} eUseOfGun;

	struct Parameters {
		u16 m_nMinTimeBeforeGunThreaten;
		u16 m_nMaxTimeBeforeGunThreaten;
		u16 m_nMinTimeBeforeFireGun;
		u16 m_nMaxTimeBeforeFireGun;
		u16 m_nMaxAdditionalTimeBetweenFireGun;

		Parameters();
	};

	CThreatenedHelper();

	void BehaviourFailure(ARTFeedbackInterfaceGta* pFeedbackInterface);
	void ControlBehaviour(CPed* pPed);

	// returns true if the given ped is someone who can threaten others 
	// eg. they are holding a gun! 
	bool PedCanThreaten(CPed* pPed);

	void Start(eUseOfGun eUseOfGun, const Parameters& timingParams);
	void Update(CPed* pPed, const Vector3& target, const Vector3& worldTarget, int levelIndex);

	bool HasBalanceFailed() const { return m_bBalanceFailed; }
	bool HasStarted() const { return m_eUseOfGun != THREATENED_GUN_NO_USE; }
	bool HasBegunThreatening() const { return m_bStartedGunThreaten; }

	// each time you Start() a threaten, the class chooses a semi-random bone tag to use as a target
	// taken from a small selection of upper-body choices
	eAnimBoneTag getChosenWeaponTargetBoneTag() const { return m_eChosenWeaponTargetBoneTag; }

private:

	Parameters	m_Parameters;

	u32 m_nStartTimeForGunThreaten;
	u32 m_nTimeTillFireGunWhileThreaten;
	u32 m_nTimeTillCancelPointAndShoot;	// used when balance fails, want to have ped keep pointing/firing momentarily

	eAnimBoneTag	m_eChosenWeaponTargetBoneTag;

	eUseOfGun m_eUseOfGun;

	bool m_bBalanceFailed;	
	bool m_bStartedGunThreaten;
};

#endif // !INC_THREATENEDHELPER_H_
