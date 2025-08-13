#ifndef _ANIM_SCENE_PLAY_ANIM_EVENT_H_
#define _ANIM_SCENE_PLAY_ANIM_EVENT_H_

// game includes
#include "animation/AnimScene/AnimSceneEvent.h"
#include "animation/AnimScene/AnimSceneHelpers.h"
#include "task/Animation/AnimTaskFlags.h"
#include "task/System/TaskHelpers.h"


class CAnimScenePlayAnimEvent : public CAnimSceneEvent
{
public:

	CAnimScenePlayAnimEvent()
		: m_blendInDuration(0.0f)
		, m_blendOutDuration(0.0f)
		, m_moverBlendInDuration(-1.0f)
		, m_startPhase(0.0f)
		, m_useSceneOrigin(true)
		, m_pSceneOffset(NULL)
		, m_startFullyIn(0.0f)
	{

	}

	CAnimScenePlayAnimEvent(const CAnimScenePlayAnimEvent& other)
		: CAnimSceneEvent(other)
		, m_animFlags(other.m_animFlags)
		, m_blendInDuration(other.m_blendInDuration)
		, m_blendOutDuration(other.m_blendOutDuration)
		, m_moverBlendInDuration(other.m_moverBlendInDuration)
		, m_filter(other.m_filter)
		, m_ikFlags(other.m_ikFlags)
		, m_ragdollBlockingFlags(other.m_ragdollBlockingFlags)
		, m_startPhase(other.m_startPhase)
		, m_useSceneOrigin(other.m_useSceneOrigin)
		, m_pSceneOffset(NULL)
		, m_startFullyIn(other.m_startFullyIn)
	{
		m_entity = other.m_entity;
		m_clip = other.m_clip;
		if (other.m_pSceneOffset)
		{
			m_pSceneOffset = rage_new CAnimSceneMatrix();
			m_pSceneOffset->SetMatrix(other.m_pSceneOffset->GetMatrix());
		}
	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimScenePlayAnimEvent(*this); }

	virtual ~CAnimScenePlayAnimEvent()
	{
		if(m_pSceneOffset)
		{
			delete m_pSceneOffset;
			m_pSceneOffset = NULL; 
		}
	}
	
	virtual bool OnUpdatePreload(CAnimScene* pScene);

	virtual void OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);
	virtual void OnShutdown(CAnimScene* pScene);

	virtual u32 GetType() { return PlayAnimEvent; }

	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_entity.GetId(); }

	virtual bool Validate();

#if __BANK
	// PURPOSE: Set and get the blend in duration (for script editing tools)
	void SetPoseBlendInDuration(float duration) { m_blendInDuration = duration; }
	float GetPoseBlendInDuration() const { return m_blendInDuration; }

	void SetMoverBlendInDuration(float duration) { m_moverBlendInDuration = duration; }
	float GetMoverBlendInDuration() const { return m_moverBlendInDuration; }

	// PURPOSE: Set and get the blend out duration (for script editing tools)
	void SetPoseBlendOutDuration(float duration) { m_blendOutDuration = duration; }
	float GetPoseBlendOutDuration() const { return m_blendOutDuration; }

	void SetMoverBlendOutDuration(float duration) { m_blendOutDuration = duration; }
	float GetMoverBlendOutDuration() const { return m_blendOutDuration; }

	float GetStartFullBlendInTime();

	// PURPOSE: Called automatically after the pargen widgets for the scene are added - used to add editing functionality 
	void OnPostAddWidgets(bkBank& pBank);
	// PURPOSE: Callback for the clip selector
	void OnClipChanged(atHashString clipDict, atHashString clip);
	// PURPOSE: Handle changes to the event length
	virtual void OnRangeUpdated();
	// PURPOSE: Add the structure for adding and removing child events
	void AddSceneOffset(bkBank* pBank);
	// PURPOSE: Remove the structure for adding and removing child events
	void RemoveSceneOffset(bkBank* pBank);
#if __DEV
	// PURPOSE: Scene debug rendering
	virtual void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif // __DEV
	// PURPOSE: Init the event for use in the editor
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
	// PURPOSE: Display the clip name on the event in the editor
	virtual bool GetEditorEventText(atString& textToDisplay, CAnimSceneEditor& editor);
	// PURPOSE: Display the clip name and frame range when hovering
	virtual bool GetEditorHoverText(atString& textToDisplay, CAnimSceneEditor& editor);
	// PURPOSE: Override the color of the event in the editor
	virtual void GetEditorColor(Color32& color, CAnimSceneEditor& editor, int colourId);
	// PURPOSE: Get the clip description
	CAnimSceneClip& GetClip() { return m_clip; } 
	// PURPOSE: Calculate the mover offset of the anim at the given event relative time (in the space of the anim scene)
	void CalcMoverOffset(CAnimScene* pScene, Mat34V_InOut mat, float eventTime);
#endif //__BANK

	CAnimSceneEntity::Id GetEntityId() const { return m_entity.GetId(); }
	const CAnimSceneClip& GetClip() const { return m_clip; }
	bool UseSceneOrigin() const { return m_useSceneOrigin; }

	bool IsEntityOptional() const;

private:

	void CalcAnimOrigin(CAnimScene* pScene, Mat34V_InOut mat);

	CAnimSceneEntityHandle m_entity;
	CAnimSceneClip m_clip;
	eScriptedAnimFlagsBitSet m_animFlags;
	eRagdollBlockingFlagsBitSet m_ragdollBlockingFlags;
	eIkControlFlagsBitSet m_ikFlags;
	atHashString m_filter; //TODO - add a helper for filters
	float m_blendInDuration;
	float m_blendOutDuration;
	float m_moverBlendInDuration;
	float m_startPhase;
	float m_startFullyIn;
	bool m_useSceneOrigin;

	CAnimSceneMatrix* m_pSceneOffset; // an optional offset from the origin of the scene

	struct RuntimeData{
		RuntimeData()
			: m_pSceneParentMat(NULL)
		{

		}
		CAnimSceneMatrix* m_pSceneParentMat;
		CClipRequestHelper m_requestHelper;
		RegdTask m_pTask;
	};

	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);

#if __BANK
	struct Widgets
	{
		Widgets()
			:m_pSceneOffsetButton(NULL)
		{
		}

		bkButton* m_pSceneOffsetButton;
	};
#endif // __BANK
	DECLARE_ANIM_SCENE_BANK_DATA(Widgets, m_widgets);

	public:

#if __BANK
		// Editor options and preferences for play anim events
		struct EditorSettings
		{
			EditorSettings()
				: m_clampEventLengthToAnim(true)
				, m_displayDictionaryNames(false)
			{

			}

			bool m_clampEventLengthToAnim;
			bool m_displayDictionaryNames;

		};
		static EditorSettings ms_editorSettings;
#endif // __BANK

	PAR_PARSABLE;
};

#endif //_ANIM_SCENE_PLAY_ANIM_EVENT_H_
