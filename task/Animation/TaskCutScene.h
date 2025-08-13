//
// Task/Animation/TaskCutScene.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_CUTSCENE_H
#define TASK_CUTSCENE_H

//Framework

//Rage

//Game
#include "Task/Animation/TaskCutScene.h"
#include "Task/System/TaskFSMClone.h"

class CTaskCutScene : public CTaskFSMClone
{
public:
	enum
	{
		State_Start,
		State_RunningScene,
		State_Exit
	};

	CTaskCutScene(const crClip* pClip, const Matrix34& SceneOrigin, float phase, float cutsceneTimeAtStart); 
	~CTaskCutScene();

	void SetPhase(float Phase);
	float GetPhase() const { return m_fPhase; }  
	atHashString GetFilter() { return m_FilterHash; }

	const crClip* GetClip() { return m_pClip; }
	void SetClip(const crClip* pClip, const Matrix34& SceneOrigin, float cutsceneStartTime);

	virtual aiTask* Copy() const { return rage_new CTaskCutScene(m_pClip, m_AnimOrigin, m_fPhase, m_fCutsceneTimeAtStart); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_CUTSCENE; }
	virtual s32	GetDefaultStateAfterAbort()	const {return State_Exit;}
	virtual bool ProcessPostPreRender();

	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	void CleanUp();

    // Clone task implementation
    virtual CTaskInfo* CreateQueriableState() const { return CTask::CreateQueriableState(); }
    virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent) { return UpdateFSM(iState, iEvent); }
    virtual bool       OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return true; }

	virtual float GetScopeDistanceMultiplierOverride() { return ms_CutsceneScopeDistMultiplierOverride; }

	static void SetScopeDistanceMultiplierOverride(float multi) { ms_CutsceneScopeDistMultiplierOverride = multi; }

	void SetAnimOrigin(const Matrix34& Mat) { m_AnimOrigin = Mat; }
	const Matrix34& GetAnimOrigin(void) const { return m_AnimOrigin; }

	void SetFilter(const atHashString& filter); 

	bool WillFinishThisPhase(float fPhase); 
	
	void SetExitNextUpdate() {m_bExitNextUpdate = true; }
	bool GetExitNextUpdate() {return m_bExitNextUpdate; }

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent); 

	float GetVehicleLightingScalar() const { return m_fVehicleLightingScalar; }
	void SetVehicleLightingScalar(float fValue) { m_fVehicleLightingScalar = fValue; }

	void SetPreserveHairScale(bool bPreserveHairScale) { m_bPreserveHairScale = bPreserveHairScale; }
	bool GetPreserveHairScale() const { return m_bPreserveHairScale; }
	void SetInstantHairScaleUpdate(bool bInstantHairScaleUpdate) { m_bInstantHairScaleUpdate = bInstantHairScaleUpdate; }
	bool GetInstantHairScaleUpdate() const { return m_bInstantHairScaleUpdate; }
	void SetOriginalHairScale(float fOriginalHairScale) { m_fOriginalHairScale = fOriginalHairScale; }
	float GetOriginalHairScale() const { return m_fOriginalHairScale; }

#if __DEV
	void SetCanApplyMoverTrackUpdate(bool bCanApply) { m_bApplyMoverTrackFixup = bCanApply; }
#endif


#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif

private:
	// State Functions
	// Start
	FSM_Return Start_OnUpdate();
	
	// running the scene
	FSM_Return RunningScene_OnUpdate();

	bool CreateMoveNetworkPlayerForEntityType(const CPhysical* pPhysical); 
	void RemoveMoveNetworkPlayerForEntityType(const CPhysical* pEnt); 	
	
	void UpdatePhysics(CPhysical* pPhysical); 

	void ApplyMoverTrackFixup(CPhysical * pPhysical); 

	void SetBlendOutTag(const crClip* pClip);

	CMoveNetworkHelper m_networkHelper;	// Loads the parent move network 
	
	pgRef<const crClip>	m_pClip;	
	strStreamingObjectName m_dictionary;
	Matrix34 m_AnimOrigin; 
	float m_fPhase;
	float m_fLastPhase;
	float m_BlendOutPhase; 
	float m_BlendOutDuration; 
	float m_fCutsceneTimeAtStart;  // the start time of this clip relative to the cutscene start
	float m_fOriginalHairScale;
	bool m_HaveEventTag; 
	atHashString m_FilterHash; 

	bool m_bExitNextUpdate;
	bool m_bIsWarpFrame;	// set to true if the peds position is being warped this frame (either by a jump cut or an anim transition)
	bool m_bPreRender: 1;
	bool m_bPreserveHairScale : 1;
	bool m_bInstantHairScaleUpdate : 1;

	// 0.0 = outside vehicle, 1.0 = in vehicle.  Set by the AnimatedActorEntity
	float m_fVehicleLightingScalar;

#if __DEV
	bool m_bApplyMoverTrackFixup;
#endif

	static float ms_CutsceneScopeDistMultiplierOverride;
	//move
	static const fwMvFloatId ms_Phase;
	static const fwMvFloatId ms_Rate;
	static const fwMvClipId ms_Clip; 
	static const fwMvFilterId ms_Filter; 
};

#endif
