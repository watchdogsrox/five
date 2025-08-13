#ifndef _ANIM_SCENE_PLAY_CAMERA_ANIM_EVENT_H_
#define _ANIM_SCENE_PLAY_CAMERA_ANIM_EVENT_H_

// game includes
#include "animation/AnimScene/AnimSceneEvent.h"
#include "animation/AnimScene/AnimSceneHelpers.h"
#include "animation/AnimScene/Events/AnimScenePlayCameraAnimFlags.h"
#include "task/System/TaskHelpers.h"


class CAnimScenePlayCameraAnimEvent : public CAnimSceneEvent
{
public:

	CAnimScenePlayCameraAnimEvent()
		: m_blendInDuration(0.0f)
		, m_blendOutDuration(0.0f)
		, m_startPhase(0.0f)
		//, m_useSceneOrigin(true)
		, m_pSceneOffset(NULL)
	{
		m_data->m_blendOutStart = 0.0f;
	}

	CAnimScenePlayCameraAnimEvent(const CAnimScenePlayCameraAnimEvent& other)
		: CAnimSceneEvent(other)
		, m_blendInDuration(other.m_blendInDuration)
		, m_blendOutDuration(other.m_blendOutDuration)
		, m_startPhase(other.m_startPhase)
		//, m_useSceneOrigin(other.m_useSceneOrigin)
		, m_cameraSettings(other.m_cameraSettings)
		, m_pSceneOffset(NULL)
	{
		m_entity = other.m_entity;
		m_clip = other.m_clip;
		if (other.m_pSceneOffset)
		{
			m_pSceneOffset = rage_new CAnimSceneMatrix();
			m_pSceneOffset->SetMatrix(other.m_pSceneOffset->GetMatrix());
		}

		m_data->m_blendOutStart = other.m_data->m_blendOutStart;
	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimScenePlayCameraAnimEvent(*this); }

	virtual ~CAnimScenePlayCameraAnimEvent()
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

	virtual u32 GetType() { return PlayCameraAnimEvent; }

	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_entity.GetId(); }
	virtual bool Validate();

#if __BANK

	// PURPOSE: Set and get the blend in and out duration (for script editing tools)
	void SetBlendInDuration(float duration) { m_blendInDuration = duration; }
	float GetBlendInDuration() const { return m_blendInDuration; }

	void SetBlendOutDuration(float duration) { m_blendOutDuration = duration; }
	float GetBlendOutDuration() const { return m_blendOutDuration; }

	//PURPOSE: Get the time at which the blend out begins (for display purposes)
	float GetBlendOutStart();

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

	// PURPOSE: display the dictionary / clip name after the frame range.
	virtual bool GetEditorHoverText(atString& textToDisplay, CAnimSceneEditor& editor);

	CAnimSceneClip& GetClip() { return m_clip; } 
#endif //__BANK

	CAnimSceneEntity::Id GetEntityId() const { return m_entity.GetId(); }
	const CAnimSceneClip& GetClip() const { return m_clip; }
	bool UseSceneOrigin() const { return m_cameraSettings.BitSet().IsSet(ASCF_USE_SCENE_ORIGIN); }// return m_useSceneOrigin; }
	bool IsOldUseSceneOrigin() const { return m_useSceneOrigin; }

	void SetUseSceneOrigin(bool val) {m_cameraSettings.BitSet().Set(ASCF_USE_SCENE_ORIGIN, val); }

private:
	//PURPOSE: Set the time (offset from the initial clip) at which the blend out begins
	void SetBlendOutStart(float time);

	CAnimSceneEntityHandle m_entity;
	CAnimSceneClip m_clip;
	float m_blendInDuration;
	float m_blendOutDuration;
	float m_blendInStart;
	float m_blendInEnd;
	float m_startPhase;
	bool m_useSceneOrigin;
	eCameraSettingsBitSet m_cameraSettings;
	CAnimSceneMatrix* m_pSceneOffset; // an optional offset from the origin of the scene


	struct RuntimeData{
		RuntimeData()
			: m_pSceneParentMat(NULL)
			, m_blendOutStart(0.f)
		{
			
		}
		CAnimSceneMatrix* m_pSceneParentMat;
		CClipRequestHelper m_requestHelper;
		float m_blendOutStart;
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

	PAR_PARSABLE;
};

#endif //_ANIM_SCENE_PLAY_CAMERA_ANIM_EVENT_H_
