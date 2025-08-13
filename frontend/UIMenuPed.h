#ifndef __UI_MENU_PED_H__
#define __UI_MENU_PED_H__

// rage
#include "fwanimation/clipsets.h"
#include "fwnet/nettypes.h"

// game
#include "Frontend/ScaleformMenuHelper.h"
#include "Scene/RegdRefTypes.h"

namespace rage
{
	class bkBank;
}

class CPed;

class CUIMenuPed
{
public:
	CUIMenuPed();
	~CUIMenuPed();

	void Init();
	void Shutdown();

	void Update(float fAlphaFader);
	void UpdatePedPose();

	void ClearPed(bool clearBG = true);
	void SetCurrColumn(PM_COLUMNS column = PM_COLUMN_MIDDLE) {m_currentDisplayColumn = column;}
	void SetPedModelLocalPlayer(PM_COLUMNS column = PM_COLUMN_MIDDLE) {m_bCloneLocalPlayer = true;  m_nextDisplayColumn = column;}
	void SetPedModel(ActivePlayerIndex playerIndex, PM_COLUMNS column = PM_COLUMN_MIDDLE);
	void SetPedModel(CPed* pPed, PM_COLUMNS column = PM_COLUMN_MIDDLE);// This class takes ownership of pPed and will delete it.

	// Used by the pause menu, so that it can repeatedly call the avatar script until a new ui menu ped has been set.
	void SetWaitingForNewPed() { m_bWaitingForNewPed = true; }
	bool IsWaitingForNewPed() const { return m_bWaitingForNewPed; }

	bool IsVisible() const {return m_isVisible;}
	void SetVisible(bool isVisible);
	void SetFadeIn(u32 fadeDurationMS = 250, u32 fadeStartDelayMS = 0, bool bForceOverrideExistingFade = false);

	void SetSlot(int slot) {m_slot = slot;}
	void SetControlsBG(bool controlsBG) {m_controlsBG = controlsBG;}

	bool HasPed() const;

	void SetIsLit(bool isLit);
	void SetIsAsleep(bool isAsleep) {m_isAsleep = isAsleep;}

#if __BANK
	static void CreateBankWidgets(bkBank *bank);
	static void DebugSetSleepState();
#endif // __BANK

private:
	atHashString GetMenuPedSlot() const;

	void InitPed(CPed* pPed);
	void Clear(CPed* pOldPed);

	void UpdateAnimalPedPose();
	void UpdateAnimState();

	void GoToIdleAnim();
	void GoToSleepIntroAnim();
	void GoToSleepLoopAnim();
	void GoToSleepOutroAnim();

	bool PlayAnim(const fwMvClipId& animClipId, bool bLoop = true, bool bBlend = true, float fInitialPhase = -1.0f);

	static bank_float sm_singleFrontOffset;
	static bank_float sm_singleRightOffset;
	static bank_float sm_singleUpOffset;
	static bank_float sm_doubleLocalFrontOffset;
	static bank_float sm_doubleLocalRightOffset;
	static bank_float sm_doubleLocalUpOffset;
	static bank_float sm_doubleOtherFrontOffset;
	static bank_float sm_doubleOtherRightOffset;
	static bank_float sm_doubleOtherUpOffset;
	static bank_float sm_headingOffset;
	static bank_float sm_pitchOffset;
	static bank_float sm_dimIntensity;
	static bank_float sm_litIntensity;

private:
	enum eANIM_STATES
	{
		ANIM_INVALID,
		ANIM_STATE_IDLE,
		ANIM_STATE_SLEEP_INTRO,
		ANIM_STATE_SLEEP_LOOP,
		ANIM_STATE_SLEEP_OUTRO
	};

	bool m_isVisible;
	bool m_deletePending;
	bool m_isLit;
	bool m_isAsleep;
	bool m_isMale;
	bool m_bCloneLocalPlayer;
	bool m_controlsBG;
	bool m_clearBGonPedDelete;

	ActivePlayerIndex m_iClonePlayerIndex;
	RegdPed m_pScriptPed;
	RegdPed m_pDisplayPed;
	RegdPed m_pOldDisplayPed;
	RegdPed m_pClonePed;
	int m_slot;
	u32 m_uFadeInStartTimeMS;
	u32 m_uFadeDurationMS;
	float m_fFacialTimer;
	float m_fFacialVoiceDrivenMouthMovementTimer;
	PM_COLUMNS m_currentDisplayColumn;
	PM_COLUMNS m_nextDisplayColumn;
	eANIM_STATES m_animState;

	bool m_bWaitingForNewPed;

	float m_fCachedAnimPhaseForNextPed;

	fwClipSetRequestHelper m_sleepAnimRequest;

	// Structure for storing an animation state, so that we can blend between poses.
	struct AnimStateData
	{
		fwMvClipSetId m_ClipSetId;
		fwMvClipId m_ClipId;
		float m_fCurrentPhase;
		float m_fCurrentPhaseDelta;
		float m_fAnimTimer;
		bool m_bLooped;

		void Reset();

		AnimStateData& operator=(const AnimStateData& other);

		void Update(float fDelta);
	};

	AnimStateData	m_animStateDataBuffer[2];
	float			m_fAnimStateBlendWeight; // 0.0f = PREVIOUS, 1.0f = CURRENT
	float			m_fFacialVoiceDrivenMouthMovementBlendWeight; // 0.0f = idle, 1.0f = talking
};

#endif // __UI_MENU_PED_H__
