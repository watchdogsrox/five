// FILE :    TaskConfront.h
// CREATED : 4/16/2012

#ifndef TASK_CONFRONT_H
#define TASK_CONFRONT_H

// Game headers
#include "physics/WorldProbe/shapetestresults.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskConfront
////////////////////////////////////////////////////////////////////////////////

class CTaskConfront : public CTask
{

private:

	enum RunningFlags
	{
		RF_IsIntimidating	= BIT0,
		RF_HasShoved		= BIT1,
		RF_DisableMoving	= BIT2,
		RF_IsNotObstructed	= BIT3,
	};

public:

	struct Tunables : CTuning
	{
		Tunables();
		
		float	m_IdealDistanceIfUnarmed;
		float	m_IdealDistanceIfArmed;
		float	m_MinDistanceToMove;
		float	m_MaxRadius;
		float	m_ChancesToIntimidateArmedTarget;
		float	m_ChancesToIntimidateUnarmedTarget;

		PAR_PARSABLE;
	};

	enum ConfrontState
	{
		State_Start = 0,
		State_Confront,
		State_Finish
	};

	explicit CTaskConfront(CPed* pTarget);
	virtual ~CTaskConfront();
	
public:

	void SetAmbientClips(atHashWithStringNotFinal hAmbientClips) { m_hAmbientClips = hAmbientClips; }
	
public:
	
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_CONFRONT; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Start; }

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	void		Start_OnEnter();
	FSM_Return	Start_OnUpdate();
	void		Confront_OnEnter();
	FSM_Return	Confront_OnUpdate();
	
private:

	Vec3V_Out	CalculateCenter() const;
	float		CalculateIdealDistance() const;
	Vec3V_Out	CalculateTargetPosition() const;
	bool		CanIntimidate();
	bool		CanShove() const;
	bool		CanStartShove() const;
	CTask*		CreateMoveTask() const;
	CTask*		CreateSubTaskForAmbientClips() const;
	CTask*		CreateSubTaskForIntimidate() const;
	CTask*		CreateSubTaskForShove() const;
	bool		HasWeaponInHand(const CPed& rPed) const;
	bool		IsShoving() const;
	bool		IsTargetInOurTerritory() const;
	bool		IsTargetMovingAway() const;
	void		ProcessProbes();
	void		ProcessStreaming();
	bool		ShouldProcessProbes() const;
	bool		ShouldStopMoving() const;
	void		UpdateHeading();
	bool		WantsToIntimidate() const;

private:

	Vec3V								m_vCenter;
	WorldProbe::CShapeTestSingleResult	m_ProbeResults;
	CPrioritizedClipSetRequestHelper	m_ClipSetRequestHelper;
	RegdPed								m_pTarget;
	atHashWithStringNotFinal			m_hAmbientClips;
	u32									m_uTimeTargetWasLastMovingAway;
	u32									m_uLastTimeProcessedProbes;
	fwFlags8							m_uRunningFlags;
	
private:

	static Tunables	sm_Tunables;

};

#endif //TASK_CONFRONT_H