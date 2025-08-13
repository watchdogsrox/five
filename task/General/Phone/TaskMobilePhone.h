#ifndef INC_TASK_MOBILEPHONE_H
#define INC_TASK_MOBILEPHONE_H

// Game headers
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"


class CTaskMobilePhone : public CTask
{
public:

	enum PhoneMode
	{
		Mode_None,
		Mode_ToCall,
		Mode_ToText,
		Mode_ToCamera,
		Mode_Max,
	};

	enum PhoneRequest
	{
		Request_PutUpToEar,
		Request_PutUpToCamera,
		Request_PutDownToPrevState,
		Request_PutHorizontalIntro,
		Request_PutHorizontalOutro,
	};

	// helper function
	static bool CanUseMobilePhoneTask( const CPed& in_Ped, PhoneMode in_Mode );
	static void CreateOrResumeMobilePhoneTask(CPed& in_Ped );
	static bool IsRunningMobilePhoneTask(const CPed& in_Ped);

	CTaskMobilePhone(s32 _PhoneMode, s32 _Duration = -1, bool bIsUsingSecondarySlot = true, bool bAllowCloneToEquipPhone = false);
	~CTaskMobilePhone();
	virtual aiTask* Copy() const { return rage_new CTaskMobilePhone(m_PhoneMode, m_Timer.GetDuration(), m_bIsUsingSecondarySlot, m_bAllowCloneToEquipPhone ); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOBILE_PHONE; }
	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32 );
#endif // !__FINAL
		
	void PutUpToEar();
	void PutUpToCamera();
	void PutDownToPrevState();
	void PutHorizontalIntro();
	void PutHorizontalOutro();
	PhoneMode GetPhoneMode() const;

	void GetPlayerSecondaryPartialAnimData(CSyncedPlayerSecondaryPartialAnim& data);

	void CleanUp();
	bool GivePedPhone();
	void RemovePedPhone();
	bool RequestPhoneModel();
	void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;
	bool ProcessPostCamera();
	void ProcessPreRender2();

	FSM_Return ProcessPreFSM();
	FSM_Return ProcessPostFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual bool ProcessMoveSignals();

	void ResetTaskMobilePhone(s32 NewMode, s32 NewDuration = -1, bool RunInSecondarySlot = true);

	void SetPreviousSelectedWeapon(u32 weaponHash) {m_uPreviousSelectedWeapon = weaponHash;}
	s32 GetPreviousVehicleWeaponIndex() { return m_sVehicleWeaponIndex; }

	// PURPOSE: Requests/Clears additional secondary animation (ie mobile phone gestures)
	void RequestAdditionalSecondaryAnims(const char* pAnimDictHash, const char* pAnimName, const char *pFilterName, float fBlendInDuration, float fBlendOutDuration, bool bIsLooping, bool bHoldLastFrame);
	void RemoteRequestAdditionalSecondaryAnims(const CSyncedPlayerSecondaryPartialAnim& data);
	void ClearAdditionalSecondaryAnimation(float fBlendOutOverride = -1.0f);
	bool GetIsPlayingAdditionalSecondaryAnim() { return m_bIsPlayingAdditionalSecondaryAnim; }
	float GetAdditionalSecondaryAnimPhase();
	float GetAdditionalSecondaryAnimDuration() { return m_fClipDuration; }

	u32  GetAdditionalSecondaryAnimHashKey() { return m_uAnimHashKey; }

#if __BANK
	static void InitWidgets();
	static void TestSetAdditionalSecondaryAnimCommand();
	static void TestClearAdditionalSecondaryAnimCommand();
#endif


	// states match with move network
	enum
	{
		State_Init,
		State_HolsterWeapon,
		State_Start,
		State_PutUpText,
		State_TextLoop,
		State_PutDownFromText,
		State_PutToEar,
		State_EarLoop,
		State_PutDownFromEar,
		State_PutToEarFromText,
		State_PutDownToText,
		State_PutUpCamera,
		State_CameraLoop,
		State_PutDownFromCamera,
		State_PutToCameraFromText,
		State_PutDownToTextFromCamera,
		State_HorizontalIntro,
		State_HorizontalLoop,
		State_HorizontalOutro,
		State_Paused,
		State_Finish,
	};

	// Used to track latest player input for thumb anims
	// ENSURE IN SYNC WITH ENUM CELL_INPUT IN COMMANDS_MISC.SCH
	enum ePhoneInput
	{
		CELL_INPUT_NONE,
		CELL_INPUT_UP,
		CELL_INPUT_DOWN,
		CELL_INPUT_LEFT,
		CELL_INPUT_RIGHT,
		CELL_INPUT_SELECT,
	};

protected:

	void Init_OnEnter();
	FSM_Return Init_OnUpdate();

	void HolsterWeapon_OnEnter();
	FSM_Return HolsterWeapon_OnUpdate();

	FSM_Return Start_OnUpdate();

	void PutUpText_OnEnter();
	FSM_Return PutUpText_OnUpdate();
	void PutUpText_OnProcessMoveSignals();

	void TextLoop_OnEnter();
	FSM_Return TextLoop_OnUpdate();

	void PutDownFromText_OnEnter();
	FSM_Return PutDownFromText_OnUpdate();
	void PutDownFromText_OnProcessMoveSignals();

	void PutToEar_OnEnter();
	FSM_Return PutToEar_OnUpdate();
	void PutToEar_OnProcessMoveSignals();

	void EarLoop_OnEnter();
	FSM_Return EarLoop_OnUpdate();

	void PutDownFromEar_OnEnter();
	FSM_Return PutDownFromEar_OnUpdate();
	void PutDownFromEar_OnProcessMoveSignals();

	void PutToEarFromText_OnEnter();
	FSM_Return PutToEarFromText_OnUpdate();
	void PutToEarFromText_OnProcessMoveSignals();

	void PutDownToText_OnEnter();
	FSM_Return PutDownToText_OnUpdate();
	void PutDownToText_OnProcessMoveSignals();

	void PutUpCamera_OnEnter();
	FSM_Return PutUpCamera_OnUpdate();
	void PutUpCamera_OnProcessMoveSignals();

	void CameraLoop_OnEnter();
	FSM_Return CameraLoop_OnUpdate();
	FSM_Return CameraLoop_OnExit();

	void PutDownFromCamera_OnEnter();
	FSM_Return PutDownFromCamera_OnUpdate();
	void PutDownFromCamera_OnProcessMoveSignals();
	
	void PutToCameraFromText_OnEnter();
	FSM_Return PutToCameraFromText_OnUpdate();

	void PutDownToTextFromCamera_OnEnter();
	FSM_Return PutDownToTextFromCamera_OnUpdate();

	void Paused_OnEnter();
	FSM_Return Paused_OnUpdate();
		
	FSM_Return Finish_OnUpdate();

	void HorizontalIntro_OnEnter();
	FSM_Return HorizontalIntro_OnUpdate();
	void HorizontalIntro_OnProcessMoveSignals();

	void HorizontalLoop_OnEnter();
	FSM_Return HorizontalLoop_OnUpdate();
	void HorizontalLoop_OnProcessMoveSignals();

	void HorizontalOutro_OnEnter();
	FSM_Return HorizontalOutro_OnUpdate();
	void HorizontalOutro_OnProcessMoveSignals();


	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	bool ShouldCreatePhoneProp() const;

private:

	void ProcessClipMoveEventTags();
	void ProcessIK();
	void ProcessLeftArmIK(Vec3V_In vCameraPosition, const Mat34V& mtxEntity, const bool bInstant);
#if FPS_MODE_SUPPORTED
	bool IsStateValidForFPSIK() const;
	void ProcessFPSArmIK();
#endif	//#if FPS_MODE_SUPPORTED
	bool ShouldPause() const;
	bool ShouldUnPause() const;

	void BlendInNetwork();
	void BlendOutNetwork();

	CMoveNetworkHelper m_NetworkHelper;
	fwClipSetRequestHelper m_ClipSetRequestHelper;
	fwClipSetRequestHelper m_StealthClipSetRequestHelper;
	fwClipSetRequestHelper m_ParachuteClipSetRequestHelper;
	fwClipSetRequestHelper m_InCarDriverClipSetRequestHelper;
	fwClipSetRequestHelper m_InCarPassengerClipSetRequestHelper;
#if FPS_MODE_SUPPORTED
	fwClipSetRequestHelper m_OnFootFPSClipSetRequestHelper;
#endif

	atQueue<PhoneRequest, 5> m_PhoneRequest;

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper*	m_pFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

	PhoneMode m_PhoneMode;
	PhoneMode m_StartupMode;
	CTaskGameTimer m_Timer;
	CTaskGameTimer m_TransitionAbortTimer;

	u32 m_uPreviousSelectedWeapon;
	s32 m_sVehicleWeaponIndex;
	float m_HeadingBeforeUsingCamera;

	bool m_bHasPhoneEquipped;
	bool m_bRequestGivePedPhone;
	bool m_bIsPhoneModelLoaded;
	bool m_bIsUsingSecondarySlot;

	bool m_bMoveAnimFinished : 1;
	bool m_bMoveInterruptible : 1;
	bool m_bAllowCloneToEquipPhone : 1;

	bool m_bWasInStealthMode : 1;

#if FPS_MODE_SUPPORTED
	Vector3 m_vPrevPhoneIKOffset;
#endif

	static const fwMvFlagId ms_IsTexting;
	static const fwMvFlagId ms_SelfPortrait;
	static const fwMvFlagId ms_UseCameraIntro;

	static const fwMvFlagId ms_UseAlternateFilter;
	static const fwMvFilterId ms_BonemaskFilter;

	static const fwMvBooleanId ms_AnimFinished;
	static const fwMvBooleanId ms_CreatePhone;
	static const fwMvBooleanId ms_DestroyPhone;
	static const fwMvBooleanId ms_Interruptible;

	static const fwMvRequestId ms_PutToEar;
	static const fwMvRequestId ms_PutToText;
	static const fwMvRequestId ms_PutToCamera;
	static const fwMvRequestId ms_SwitchPhoneMode;
	static const fwMvRequestId ms_PutToEarTransit;
	static const fwMvRequestId ms_PutToTextTransit;
	static const fwMvRequestId ms_AbortTransit;
	static const fwMvRequestId ms_PutDownPhone;
	static const fwMvRequestId ms_TextLoop;
	static const fwMvRequestId ms_RestartLoop;
	static const fwMvRequestId ms_HorizontalMode;
	static const fwMvRequestId ms_QuitHorizontalMode;
	static const fwMvRequestId ms_RequestSelectAnim;
	static const fwMvRequestId ms_RequestSwipeUpAnim;
	static const fwMvRequestId ms_RequestSwipeDownAnim;
	static const fwMvRequestId ms_RequestSwipeLeftAnim;
	static const fwMvRequestId ms_RequestSwipeRightAnim;

	static const fwMvFloatId ms_FPSRestartBlend;
	static const fwMvFloatId ms_PutDownFromTextPhase;
	static const fwMvFlagId ms_InFirstPersonMode;

	static const fwMvBooleanId ms_OnEnterPutUp;
	static const fwMvBooleanId ms_OnEnterLoop;
	static const fwMvBooleanId ms_OnEnterPutDown;
	static const fwMvBooleanId ms_OnEnterPutToCamera;
	static const fwMvBooleanId ms_OnEnterPutDownToTextFromCamera;
	static const fwMvBooleanId ms_OnEnterTransit;
	static const fwMvBooleanId ms_OnEnterHorizontalIntro;
	static const fwMvBooleanId ms_OnEnterHorizontalLoop;
	static const fwMvBooleanId ms_OnEnterHorizontalOutro;

	static const fwMvStateId ms_State_Start;
	static const fwMvStateId ms_State_PutUpText;
	static const fwMvStateId ms_State_PutToEarFromText;
	static const fwMvStateId ms_State_PutToEar;
	static const fwMvStateId ms_State_PutToCamera;
	static const fwMvStateId ms_State_TextLoop;
	static const fwMvStateId ms_State_PutDownToText;
	static const fwMvStateId ms_State_EarLoop;
	static const fwMvStateId ms_State_CameraLoop;
	static const fwMvStateId ms_State_PutDownFromText;
	static const fwMvStateId ms_State_PutDownFromEar;
	static const fwMvStateId ms_State_PutDownFromCamera;
	static const fwMvStateId ms_State_PutToCameraFromText;
	static const fwMvStateId ms_State_PutDownToTextFromCamera;

	// Additional secondary anims:
	void SetAdditionalSecondaryAnimation(const crClip* pClip, crFrameFilter* pFilter);
	void GetAdditionalSecondaryAnimClipAndFilter();
	static const fwMvRequestId ms_AdditionalSecondaryAnimOn;
	static const fwMvRequestId ms_AdditionalSecondaryAnimOff;
	static const fwMvClipId ms_AdditionalSecondaryAnimClip;
	static const fwMvFlagId ms_AdditionalSecondaryAnimHoldLastFrame;
	static const fwMvBooleanId ms_AdditionalSecondaryAnimIsLooped;
	static const fwMvBooleanId ms_AdditionalSecondaryAnimFinished;
	static const fwMvFilterId ms_AdditionalSecondaryAnimFilter;
	static const fwMvFloatId ms_AdditionalSecondaryAnimBlendInDuration;
	static const fwMvFloatId ms_AdditionalSecondaryAnimBlendOutDuration;
	static const fwMvFloatId ms_AdditionalSecondaryAnimPhase;

	float m_fBlendInDuration;
	float m_fBlendOutDuration;
	s32 m_sAnimDictIndex;
	u32 m_uAnimHashKey;
	u32 m_filterId;
	const char *m_pFilterName;
	bool m_bWaitingOnClips;
	bool m_bIsPlayingAdditionalSecondaryAnim;
	float m_fClipDuration;
	CClipRequestHelper m_SecondaryAnimRequestHelper;

	bool m_bAdditionalSecondaryAnim_HoldLastFrame : 1;
	bool m_bAdditionalSecondaryAnim_Looped : 1;
	bool m_bForceInstantIkSetup : 1;
	bool m_bPrevTickInFirstPersonCamera : 1;

#if __BANK
	void TestAdditionalSecondaryAnimCommand();
	void TestSetAdditionalSecondaryAnims();
	static void ToggleLooping();
	static void ToggleHoldLastFrame();
	static bool ms_bAdditionalSecondaryAnim_IsLooping;
	static bool ms_bAdditionalSecondaryAnim_HoldLastFrame ;
	static bool sm_bAdditionalSecondaryAnim_Start;
	static bool ms_bAdditionalSecondaryAnim_Stop;
	static char m_AdditionalSecondaryAnim_DictName[64];
	static char m_AdditionalSecondaryAnim_ClipName[64];
	static float ms_fDebugBlendInDuration;
	static float ms_fDebugBlendOutDuration;
	static CClipRequestHelper m_DebugSecondaryAnimRequestHelper;
	static bool ms_bDebugWaitingOnClips;

	static bool ms_bEnterHorizontalMode;
	static bool ms_bExitHorizontalMode;
	static bool ms_bPlaySelectAnim;
	static bool ms_bPlayUpSwipeAnim;
	static bool ms_bPlayDownSwipeAnim;
	static bool ms_bPlayLeftSwipeAnim;
	static bool ms_bPlayRightSwipeAnim;

#endif

	static const s32 ms_AbortDuration;

};

#endif // INC_TASK_MOBILEPHONE_H

