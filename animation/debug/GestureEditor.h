
// 
// animation/debug/GestureEditor.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 


//
// GSALES 23/02/2011:	The gesture editor has been disabled as part of the anim blender removal.
//						Replace this when the new gesture system is implemented.
//

#if __BANK && 1

#ifndef GESTURE_EDITOR_H
#define GESTURE_EDITOR_H

// rage includes
#include "vector/vector3.h"

// game includes
#include "animation/debug/AnimDebug.h"
#include "animation/Move.h"

// forward declarations
namespace rage
{
	class bkCombo;
}

class CPed;


// Keep in sync with m_gestureContextToString
enum eGesturesContext
{
	CONTEXT_STANDING = 0,
	CONTEXT_WALKING,
	CONTEXT_SITTING,
	CONTEXT_CAR_DRIVER,
	CONTEXT_CAR_PASSENGER,
	CONTEXT_MAX
};

// Keep in sync with m_gestureCameraToString
enum eGestureCamera
{
	CAMERA_SPEAKER_AND_LISTENER = 0,
	CAMERA_SPEAKER_FULL,
	CAMERA_SPEAKER_THREE_QUARTER,
	CAMERA_SPEAKER_HEAD,
	CAMERA_LISTENER_FULL,
	CAMERA_LISTENER_THREE_QUARTER,
	CAMERA_LISTENER_HEAD,
	CAMERA_MAX
};


//////////////////////////////////////////////////////////////////////////
//	Anim gesture editor
//	Creates a set of RAG widgets that can be used to test gestures
//////////////////////////////////////////////////////////////////////////

class CGestureEditor
{

public:

	static void Initialise();

	static void Update();

	static void Shutdown();

	// Gesture tool
	static void OnEnabledChanged();

	static void InitGestureTool();
	static void ShutdownGestureTool();

	static void UpdateGestureTool();

	static void RecieveGestureClipInfo();
	static void SendGestureClipInfo();

	static void TriggerListenerBodyGesture(const char *dictionary, const char *anim, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime);
	static void TriggerSpeakerBodyGesture(const char *dictionary, const char *anim, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime);
	static void TriggerListenerFacialGesture(const char *dictionary, const char *anim, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime);
	static void TriggerSpeakerFacialGesture(const char *dictionary, const char *anim, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime);
	static void TriggerBodyGesture(CPed *pPed, const char *dictionary, const char *anim, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime, int context, bool usePhone);
	static void TriggerFacialGesture(CPed *pPed, const char *dictionary, const char *anim, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime);

	static void RenderSpeaker();
	static void RenderListener();

	static void ActivateBank();
	static void DeactivateBank();

	static void SelectFromSpeakerModelCombo();
	static void SpeakerModelChangedCB() { m_bSpeakerModelChanged = true; }

	static void SelectFromSpeakerContextCombo();
	static void SelectFromSpeakerCameraCombo();
	static void SelectFromListenerModelCombo();
	static void ListenerModelChangedCB() { m_bListenerModelChanged = true; }

	static void SelectFromListenerContextCombo();
	static void SelectFromCameraCombo();

	static void ToggleSpeakerUsePhone();
	static void ToggleListenerUsePhone();

	static void SpeakerCustomBaseAnimChanged();
	static void ListenerCustomBaseAnimChanged();

	//
	// Gesture tool
	//

	static bool m_bEnabled; // When set to true the gesture tool is active

	static fwDebugBank* m_pGestureBank;

	// Keep in sync with eGesturesContext
	static const char* m_gestureContextToString[CONTEXT_MAX];

	// Keep in sync with eGestureCamera
	static const char* m_gestureCameraToString[CAMERA_MAX];

	static CPed* m_pSpeaker;
	static CClipHelper m_speakerClipHelper;
	static CGenericClipHelper m_speakerGenericClipHelper;

	static int m_speakerModel;
	static CDebugPedModelSelector m_speakerSelector;
	static bool m_bSpeakerModelChanged;
	static bkCombo *m_pSpeakerModelCombo;
	static int m_speakerContext;
	static bkCombo *m_pSpeakerContextCombo;
	static bool	m_speakerUsePhone;
	static Vector3 m_speakerPosition;
	static float m_speakerHeading;
	static bool m_speakerAutoRotate;
	static float m_speakerAutoRotateRate;
	static int m_speakerCamera;
	static bkCombo *m_pSpeakerCameraCombo;
	static bool m_speakerUseCustomBaseAnim;
	static CDebugClipSelector m_speakerCustomBaseAnim;

	static CPed* m_pListener;
	static CClipHelper m_listenerClipHelper;
	static CGenericClipHelper m_listenerGenericClipHelper;

	static int m_listenerModel;
	static CDebugPedModelSelector m_listenerSelector;
	static bool m_bListenerModelChanged;
	static bkCombo *m_pListenerModelCombo;
	static int m_listenerContext;
	static bkCombo *m_pListenerContextCombo;
	static bool	m_listenerUsePhone;
	static Vector3 m_listenerPosition;
	static float m_listenerHeading;
	static bool m_listenerAutoRotate;
	static float m_listenerAutoRotateRate;
	static bool m_listenerUseCustomBaseAnim;
	static CDebugClipSelector m_listenerCustomBaseAnim;

	static float m_deltaForAutoRotate;

	static int m_cameraIndex;
	static bkCombo *m_pCameraCombo;

	static Vector3 m_playerPosition;
	static float m_playerHeading;
};

#endif // GESTUREEDITOR_H

#endif // __BANK