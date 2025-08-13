/////////////////////////////////////////////////////////////////////////////////
// FILE :		CScenarioAnimDebugHelper.h
// PURPOSE :	Helper used for debugging scenario animations
// AUTHOR :		Jason Jurecka.
// CREATED :	5/1/2012
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_SCENARIO_ANIM_DEBUG_HELPER_H
#define INC_SCENARIO_ANIM_DEBUG_HELPER_H

// rage headers
#include "atl/array.h"
#include "data/base.h"

// framework headers
#include "fwanimation/animdefines.h"

// game includes
#include "ai/ambient/ConditionalAnims.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/scdebug.h" // for SCENARIO_DEBUG

#if SCENARIO_DEBUG
// Forward declarations

struct ScenarioAnimDebugInfo
{
	ScenarioAnimDebugInfo() 
		: m_BasePlayTime(0.0f)
		, m_AnimType(CConditionalAnims::AT_COUNT) 
	{}

	atHashString m_ConditionName;
	atHashString m_PropName;
	fwMvClipSetId m_ClipSetId;
	fwMvClipId m_ClipId;
	CConditionalAnims::eAnimType m_AnimType;
	float m_BasePlayTime;//used when using base anims as queue items.

	PAR_SIMPLE_PARSABLE
};

//Used only for save/load of scenario queue files
struct ScenarioQueueLoadSaveObject
{
	ScenarioQueueLoadSaveObject(){}

	atArray<ScenarioAnimDebugInfo> m_QueueItems;

	PAR_SIMPLE_PARSABLE
};

class CScenarioAnimDebugHelper : public rage::datBase
{
public:
	CScenarioAnimDebugHelper();

	void Init(bkGroup* parent);
	void Update(CScenarioPoint* selectedPoint, CEntity* selectedEntity);
	void Render();
	void RebuildUI();

	CScenarioPoint* GetScenarioPoint();
	bool			UsesEnterExitOff() const { return m_UseEnterExitOff; }
	Vec3V_Out		GetEnterExitOff() const { Assert(m_UseEnterExitOff); return m_EnterExitOff; }

	//Queue interface
	bool WantsToPlay();
	void GoToQueueStart();
	bool GoToQueueNext(bool ignoreLoop = false);
	void GoToQueuePrev(bool ignoreLoop = false);
	ScenarioAnimDebugInfo GetCurrentInfo() const;

	// Widget callbacks
	void OnStopQueuePressed();
private:

	struct ScenarioAnimQueueItem : public rage::datBase
	{
		ScenarioAnimQueueItem()
			: m_Invalid(true)
			, m_WidgetMinIdleTime(0.0f)
			, m_WidgetMaxIdleTime(0.0f)
		{}

		void AddWidgets(bkGroup* parent);
		void OnRemovePressed();

		ScenarioAnimDebugInfo m_Info;

		float m_WidgetMinIdleTime;
		float m_WidgetMaxIdleTime;
		bool m_Invalid; //invalid items are automatically removed from queue
	};

	// Widget callbacks
	void OnSelScenarioTypeChange();
	void OnSelFTypeChange();
	void OnSelCTypeChange();
	void OnSelFConGroupChange();
	void OnSelCConGroupChange();
	void OnSelCConClipsChange();
	void OnSelFClipSetChange();
	void OnSelUFClipSetChange();
	void OnSelCClipSetChange();
	void OnQueueFSelPressed();
	void OnQueueUFSelPressed();
	void OnQueueCSelPressed();
	void OnClearQueuePressed();
	void OnPlayQueuePressed();
	void OnLoadQueuePressed();
	void OnAppendQueuePressed();
	void OnSaveQueuePressed();

	//Help functions
	void AddWidgets();
	void ResetSelected();
	void UpdateScenarioTypeNames();
	void UpdateFConGroupNames();
	void UpdateCConGroupNames();
	void UpdateCConClipsNames();
	void UpdateTypeNames();
	void UpdateFClipSetNames();
	void UpdateUFClipSetNames();
	void UpdateCClipSetNames();
	void UpdateFClipNames();
	void UpdateUFClipNames();
	void UpdateCClipNames();
	void UpdateFPropNames();
	void UpdateCPropNames();
	void UpdateQueueUI();
	void ValidateNameFill(atArray<const char *>& nameArray, bool sort = true);
	void QueueClip(fwMvClipSetId clipSetId, fwMvClipId clipId, atHashString conGroupName, int animType, atHashString propName);
	void SwitchPoint(CScenarioPoint& point);
	void SwitchEntity(CEntity& entity);
	void ClearEntity();
	void EnqueueFromFile(const char* filename);

	char    m_SelectedEntityName[64];
	char	m_FPropSetName[64];
	char	m_CPropSetName[64];
	
	bkGroup*	m_Parent;
	bkCombo*    m_FConGroupCombo;
	bkCombo*	m_FClipSetCombo;
	bkCombo*	m_FClipCombo;
	bkCombo*	m_UFClipSetCombo;
	bkCombo*	m_UFClipCombo;
	bkCombo*    m_CConClipsCombo;
	bkCombo*	m_CClipSetCombo;
	bkCombo*	m_CClipCombo;
	bkCombo*	m_FPropCombo;
	bkCombo*	m_CPropCombo;
	bkGroup*	m_QueueGroup;

	int m_SelScenarioType;
	int m_SelFType;		
	int m_SelFConGroup;
	int m_SelFClipSet;
	int m_SelFClip;
	int m_SelFProp;
	int m_SelUFClipSet;
	int m_SelUFClip;
	int m_SelUFType;
	int m_SelCType;
	int m_SelCConGroup;
	int m_SelCConClips;
	int m_SelCClipSet;
	int m_SelCClip;
	int m_SelCProp;
	int m_CurrentQueueItem;
	bool m_PlayQueue;
	bool m_LoopQueue;
	bool m_ToolEnabled;
	bool m_HideUI;
	float m_BasePlayTime;
	bool m_UseEnterExitOff;
	Vec3V m_EnterExitOff; //used for enter and exit clips as a point to where the ped should start from or go to.

	RegdScenarioPnt m_ScenarioPoint;
	RegdEnt m_SelectedEntity;

	atArray<ScenarioAnimQueueItem> m_AnimQueue;
	atArray<const char *> m_ScenatioTypeNames;	// for combo box
	atArray<const char *> m_AnimTypeNames;		// for combo box
	atArray<const char *> m_FConGroupNames;		// for combo box
	atArray<const char *> m_FClipsetNames;		// for combo box
	atArray<const char *> m_FClipNames;			// for combo box
	atArray<const char *> m_FPropNames;			// for combo box
	atArray<const char *> m_UFClipsetNames;		// for combo box
	atArray<const char *> m_UFClipNames;		// for combo box
	atArray<const char *> m_CConGroupNames;		// for combo box
	atArray<const char *> m_CConClipsNames;		// for combo box
	atArray<const char *> m_CClipsetNames;		// for combo box
	atArray<const char *> m_CClipNames;			// for combo box
	atArray<const char *> m_CPropNames;			// for combo box
};

#endif // SCENARIO_DEBUG

#endif // INC_SCENARIO_ANIM_DEBUG_HELPER_H
