#ifndef INC_AGITATED_ACTIONS_H
#define INC_AGITATED_ACTIONS_H

// Rage headers
#include "atl/array.h"
#include "data/base.h"
#include "fwutil/Flags.h"
#include "fwutil/rtti.h"
#include "parser/macros.h"

// Game headers
#include "task/Default/AmbientAudio.h"
#include "task/Response/Info/AgitatedEnums.h"
#include "task/Response/Info/AgitatedSay.h"

// Forward declarations
class CAgitatedCondition;
class CPed;
class CTask;
class CTaskAgitated;

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionContext
////////////////////////////////////////////////////////////////////////////////

struct CAgitatedActionContext
{
	CTaskAgitated* m_Task;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedAction
////////////////////////////////////////////////////////////////////////////////

class CAgitatedAction : public datBase
{

	DECLARE_RTTI_BASE_CLASS(CAgitatedAction);

public:

	virtual ~CAgitatedAction() {}

public:

	virtual bool IsTask() const
	{
		return false;
	}
	
public:
	
	virtual void Execute(const CAgitatedActionContext& rContext) const = 0;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionConditional
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionConditional : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionConditional, CAgitatedAction);

public:

	CAgitatedActionConditional();
	virtual ~CAgitatedActionConditional();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	CAgitatedCondition*	m_Condition;
	CAgitatedAction*	m_Action;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionMulti
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionMulti : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionMulti, CAgitatedAction);

public:

	CAgitatedActionMulti();
	virtual ~CAgitatedActionMulti();
	
private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	atArray<CAgitatedAction *> m_Actions;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionChangeResponse
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionChangeResponse : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionChangeResponse, CAgitatedAction);

public:

	CAgitatedActionChangeResponse();
	virtual ~CAgitatedActionChangeResponse();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	atHashWithStringNotFinal m_Response;
	atHashWithStringNotFinal m_State;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionTask
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionTask : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionTask, CAgitatedAction);

public:

	CAgitatedActionTask();
	virtual ~CAgitatedActionTask();

public:

	virtual bool IsTask() const
	{
		return true;
	}

public:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const = 0;
	
private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionAnger
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionAnger : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionAnger, CAgitatedAction);

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	float	m_Set;
	float	m_Add;
	float	m_Min;
	float	m_Max;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionApplyAgitation
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionApplyAgitation : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionApplyAgitation, CAgitatedAction);

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	AgitatedType m_Type;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionCallPolice
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionCallPolice : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionCallPolice, CAgitatedActionTask);

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionClearHash
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionClearHash : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionClearHash, CAgitatedAction);

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	atHashWithStringNotFinal m_Value;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionConfront
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionConfront : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionConfront, CAgitatedActionTask);

public:

	CAgitatedActionConfront();
	virtual ~CAgitatedActionConfront();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	atHashWithStringNotFinal	m_AmbientClips;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionEnterVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionEnterVehicle : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionEnterVehicle, CAgitatedActionTask);

public:

	CAgitatedActionEnterVehicle();
	virtual ~CAgitatedActionEnterVehicle();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionExitVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionExitVehicle : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionExitVehicle, CAgitatedActionTask);

public:

	CAgitatedActionExitVehicle();
	virtual ~CAgitatedActionExitVehicle();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFace
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionFace : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionFace, CAgitatedActionTask);

public:

	CAgitatedActionFace();
	virtual ~CAgitatedActionFace();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	atHashWithStringNotFinal m_AmbientClips;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFear
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionFear : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionFear, CAgitatedAction);

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	float	m_Set;
	float	m_Add;
	float	m_Min;
	float	m_Max;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFight
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionFight : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionFight, CAgitatedActionTask);

public:

	CAgitatedActionFight();
	virtual ~CAgitatedActionFight();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFlee
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionFlee : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionFlee, CAgitatedActionTask);
	
public:

	enum Flags
	{
		ClaimPropInsteadOfDestroy	= BIT0,
		ClaimPropInsteadOfDrop		= BIT1,
	};
	
private:

	struct AmbientClipsInfo
	{
		AmbientClipsInfo()
		{}
		
		bool						m_bDestroyPropInsteadOfDrop;
		atHashWithStringNotFinal	m_hAmbientClips;
	};

public:

	CAgitatedActionFlee();
	virtual ~CAgitatedActionFlee();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;
	
private:

	bool LoadAmbientClipsInfo(const CPed& rPed, AmbientClipsInfo& rInfo) const;

private:

	float						m_FleeStopDistance;
	float						m_MoveBlendRatio;
	bool						m_ForcePreferPavements;
	bool						m_NeverLeavePavements;
	bool						m_KeepMovingWhilstWaitingForFleePath;
	atHashWithStringNotFinal	m_AmbientClips;
	fwFlags8					m_Flags;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFlipOff
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionFlipOff : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionFlipOff, CAgitatedAction);

public:

	CAgitatedActionFlipOff();
	virtual ~CAgitatedActionFlipOff();

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	float m_Time;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionFollow
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionFollow : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionFollow, CAgitatedActionTask);

public:

	CAgitatedActionFollow();
	virtual ~CAgitatedActionFollow();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionHurryAway
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionHurryAway : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionHurryAway, CAgitatedActionTask);

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSetFleeAmbientClips
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionSetFleeAmbientClips : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionSetFleeAmbientClips, CAgitatedAction);

public:

	CAgitatedActionSetFleeAmbientClips();
	virtual ~CAgitatedActionSetFleeAmbientClips();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	atHashWithStringNotFinal	m_AmbientClips;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionIgnoreForcedAudioFailures
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionIgnoreForcedAudioFailures : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionIgnoreForcedAudioFailures, CAgitatedAction);

public:

	CAgitatedActionIgnoreForcedAudioFailures();
	virtual ~CAgitatedActionIgnoreForcedAudioFailures();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionMakeAggressiveDriver
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionMakeAggressiveDriver : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionMakeAggressiveDriver, CAgitatedAction);

public:

	CAgitatedActionMakeAggressiveDriver();
	virtual ~CAgitatedActionMakeAggressiveDriver();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionReportCrime
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionReportCrime : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionReportCrime, CAgitatedAction);

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionReset
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionReset : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionReset, CAgitatedAction);

public:

	CAgitatedActionReset();
	virtual ~CAgitatedActionReset();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSay
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionSay : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionSay, CAgitatedAction);

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	CAgitatedSay m_Say;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSayAgitator
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionSayAgitator : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionSayAgitator, CAgitatedAction);

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	CAmbientAudio m_Audio;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSetFleeMoveBlendRatio
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionSetFleeMoveBlendRatio : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionSetFleeMoveBlendRatio, CAgitatedAction);

public:

	CAgitatedActionSetFleeMoveBlendRatio();
	virtual ~CAgitatedActionSetFleeMoveBlendRatio();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	float	m_MoveBlendRatio;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionSetHash
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionSetHash : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionSetHash, CAgitatedAction);

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	atHashWithStringNotFinal m_Value;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionStopVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionStopVehicle : public CAgitatedActionTask
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionStopVehicle, CAgitatedActionTask);

public:

	CAgitatedActionStopVehicle();
	virtual ~CAgitatedActionStopVehicle();

protected:

	virtual CTask* CreateTask(const CAgitatedActionContext& rContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedActionTurnOnSiren
////////////////////////////////////////////////////////////////////////////////

class CAgitatedActionTurnOnSiren : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedActionTurnOnSiren, CAgitatedAction);

protected:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

private:

	float m_Time;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionalAction
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionalAction
{
public:

	CAgitatedConditionalAction();
	~CAgitatedConditionalAction();
	
public:

	const CAgitatedCondition*	GetCondition()	const { return m_Condition; }
	const CAgitatedAction*		GetAction()		const { return m_Action; }

public:

	bool HasAction() const
	{
		return (m_Action != NULL);
	}
	
private:

	CAgitatedCondition*	m_Condition;
	CAgitatedAction*	m_Action;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioExit
////////////////////////////////////////////////////////////////////////////////

class CAgitatedScenarioExit : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedScenarioExit, CAgitatedAction);

public:

	CAgitatedScenarioExit();
	virtual ~CAgitatedScenarioExit();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioNormalExit
////////////////////////////////////////////////////////////////////////////////

class CAgitatedScenarioNormalCowardExit : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedScenarioNormalCowardExit, CAgitatedAction);

public:

	CAgitatedScenarioNormalCowardExit();
	virtual ~CAgitatedScenarioNormalCowardExit();

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioFastCowardExit
////////////////////////////////////////////////////////////////////////////////

class CAgitatedScenarioFastCowardExit : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedScenarioFastCowardExit, CAgitatedAction);

public:

	CAgitatedScenarioFastCowardExit() {}
	virtual ~CAgitatedScenarioFastCowardExit() {}

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedScenarioFleeExit
////////////////////////////////////////////////////////////////////////////////

class CAgitatedScenarioFleeExit : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedScenarioFleeExit, CAgitatedAction);

public:

	CAgitatedScenarioFleeExit() {}
	virtual ~CAgitatedScenarioFleeExit() {}

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedSetWaterSurvivalTime
////////////////////////////////////////////////////////////////////////////////

class CAgitatedSetWaterSurvivalTime : public CAgitatedAction
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedSetWaterSurvivalTime, CAgitatedAction);

public:

	CAgitatedSetWaterSurvivalTime() {}
	virtual ~CAgitatedSetWaterSurvivalTime() {}

private:

	virtual void Execute(const CAgitatedActionContext& rContext) const;

	float m_Time;

	PAR_PARSABLE;
};



#endif // INC_AGITATED_ACTIONS_H
