// 
// animation/debug/GestureEditor.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

// TODO - re-enable this when gestures are restored
#if __BANK && 1

#include "GestureEditor.h"

// system includes
#include <vector>

// rage includes
#include "fwdebug/picker.h"
#include "fwmaths/random.h"
#include "system/namedpipe.h"

// game includes
#include "animation/debug/AnimDebug.h"
#include "animation/debug/AnimViewer.h"
#include "animation/FacialData.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportManager.h"
#include "peds/ped.h"
#include "peds/PedFactory.h"
#include "peds/PedIntelligence.h"
#include "peds/Ped.h"
#include "modelinfo/PedModelInfo.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "Task/general/TaskBasic.h"
#include "Task/General/Phone/TaskMobilePhone.h"


ANIM_OPTIMISATIONS()

// Keep in sync with eGestureContext
const char* CGestureEditor::m_gestureContextToString[CONTEXT_MAX] =
{
	"CONTEXT_STANDING",											// 0
	"CONTEXT_WALKING",											// 1
	"CONTEXT_SITTING",											// 2
	"CONTEXT_CAR_DRIVER",										//
	"CONTEXT_CAR_PASSENGER",									//
	//"CONTEXT_MAX",
};

// Keep in sync with eGestureCamera
const char* CGestureEditor::m_gestureCameraToString[CAMERA_MAX] =
{
	"CAMERA_SPEAKER_AND_LISTENER",								// 0
	"CAMERA_SPEAKER_FULL",										// 1
	"CAMERA_SPEAKER_THREE_QUARTER",								// 2
	"CAMERA_SPEAKER_HEAD",										// 3
	"CAMERA_LISTENER_FULL",										// 4
	"CAMERA_LISTENER_THREE_QUARTER",							// 5
	"CAMERA_LISTENER_HEAD",										// 6
	//"CAMERA_MAX",
};


// Gesture tool
PARAM(gesturetool, "Enable gesture editor connection");

static const fwMvClipId CLIP_ADDITIVE_FAT("ADDITIVE_FAT", 0xe7877924);

bool CGestureEditor::m_bEnabled = false;

fwDebugBank* CGestureEditor::m_pGestureBank = NULL;

CPed *CGestureEditor::m_pSpeaker = NULL;
CClipHelper CGestureEditor::m_speakerClipHelper;
CGenericClipHelper CGestureEditor:: m_speakerGenericClipHelper;

int	CGestureEditor::m_speakerModel = 0;
CDebugPedModelSelector CGestureEditor::m_speakerSelector;
bool CGestureEditor::m_bSpeakerModelChanged = false;
bkCombo *CGestureEditor::m_pSpeakerModelCombo = NULL;
int	CGestureEditor::m_speakerContext = 0;
bkCombo *CGestureEditor::m_pSpeakerContextCombo = NULL;
bool CGestureEditor::m_speakerUsePhone = false;
Vector3 CGestureEditor::m_speakerPosition = Vector3(-0.5f, 0.0f, 1.0f);
float CGestureEditor::m_speakerHeading = 180.0f;
bool CGestureEditor::m_speakerAutoRotate = false;
float CGestureEditor::m_speakerAutoRotateRate = 30.0f;
bool CGestureEditor::m_speakerUseCustomBaseAnim = false;
CDebugClipSelector CGestureEditor::m_speakerCustomBaseAnim(true, true, true);

CPed *CGestureEditor::m_pListener = NULL;
CClipHelper CGestureEditor::m_listenerClipHelper;
CGenericClipHelper CGestureEditor:: m_listenerGenericClipHelper;

int	CGestureEditor::m_listenerModel = 0;
CDebugPedModelSelector CGestureEditor::m_listenerSelector;
bool CGestureEditor::m_bListenerModelChanged = false;
bkCombo *CGestureEditor::m_pListenerModelCombo = NULL;
int	CGestureEditor::m_listenerContext = 0;
bkCombo *CGestureEditor::m_pListenerContextCombo = NULL;
bool CGestureEditor::m_listenerUsePhone = false;
Vector3 CGestureEditor::m_listenerPosition = Vector3(0.5f, 0.0f, 1.0f);
float CGestureEditor::m_listenerHeading = 180.0f;
bool CGestureEditor::m_listenerAutoRotate = false;
float CGestureEditor::m_listenerAutoRotateRate = 30.0f;
bool CGestureEditor::m_listenerUseCustomBaseAnim = false;
CDebugClipSelector CGestureEditor::m_listenerCustomBaseAnim(true, true, true);

float CGestureEditor::m_deltaForAutoRotate = 0.0f;

int	CGestureEditor::m_cameraIndex = 0;
bkCombo *CGestureEditor::m_pCameraCombo = NULL;

Vector3 CGestureEditor::m_playerPosition = Vector3(0.0f, 0.0f, -3.0f);
float CGestureEditor::m_playerHeading = 0.0f;


///////////////////////////////////////////////////////////////////////////////////////////////////
// Gesture tool stuff
///////////////////////////////////////////////////////////////////////////////////////////////////

sysNamedPipe m_GestureToolPipe;

void CGestureEditor::Initialise()
{
	if(animVerifyf(!m_pGestureBank, "Calling CGestureEditor::Initialise when gesture editor has already been initialized!"))
	{
		m_pGestureBank = fwDebugBank::CreateBank("Anim Gesture Editor", MakeFunctor(CGestureEditor::ActivateBank), MakeFunctor(CGestureEditor::DeactivateBank));
	}
}

void CGestureEditor::Update()
{
	//early out if the tool is disabled
	if (!m_bEnabled)
		return;

	if (m_bSpeakerModelChanged)
	{
		SelectFromSpeakerModelCombo();
		m_bSpeakerModelChanged = false;
	}

	if (m_bListenerModelChanged)
	{
		SelectFromListenerModelCombo();
		m_bListenerModelChanged = false;
	}

	// Only use a delta time during a genuine Update() as slider changes will also call UpdateGestureTool causing two updates to the rotations.
	m_deltaForAutoRotate = fwTimer::GetGameTimer().GetTimeStep();

	UpdateGestureTool();

	m_deltaForAutoRotate = 0.0f;
}

void CGestureEditor::Shutdown()
{
	if(animVerifyf(m_pGestureBank, "Calling CGestureEditor::Shutdown when the gesture editor has not been initialized!"))
	{
		if(m_bEnabled)
		{
			m_bEnabled = false;
			OnEnabledChanged();
		}

		if (m_pGestureBank)
		{
			m_pGestureBank->Shutdown();
			m_pGestureBank = NULL;
		}
	}
}

void CGestureEditor::OnEnabledChanged()
{
	if (m_bEnabled)
	{
		InitGestureTool();
	}
	else
	{
		ShutdownGestureTool();
	}
}

void CGestureEditor::InitGestureTool()
{
	fwClipSetManager::Request_DEPRECATED(CLIP_SET_SIT_CHAIR_M);
	fwClipSetManager::Request_DEPRECATED(CLIP_SET_SIT_CHAIR_F);
	fwClipSetManager::Request_DEPRECATED(CLIP_SET_VEH_STD_DS_BASE);
	fwClipSetManager::Request_DEPRECATED(CLIP_SET_VEH_STD_PS_BASE);

	CStreaming::LoadAllRequestedObjects();

	if (animVerifyf(fwClipSetManager::IsStreamedIn_DEPRECATED(CLIP_SET_SIT_CHAIR_M), "Could not load CLIP_SET_SIT_CHAIR_M!") &&
		animVerifyf(fwClipSetManager::IsStreamedIn_DEPRECATED(CLIP_SET_SIT_CHAIR_F), "Could not load CLIP_SET_SIT_CHAIR_F!") &&
		animVerifyf(fwClipSetManager::IsStreamedIn_DEPRECATED(CLIP_SET_VEH_STD_DS_BASE), "Could not load CLIP_SET_VEH_STD_DS_BASE!") &&
		animVerifyf(fwClipSetManager::IsStreamedIn_DEPRECATED(CLIP_SET_VEH_STD_PS_BASE), "Could not load CLIP_SET_VEH_STD_PS_BASE!"))
	{
		fwClipSetManager::AddRef_DEPRECATED(CLIP_SET_SIT_CHAIR_M);
		fwClipSetManager::AddRef_DEPRECATED(CLIP_SET_SIT_CHAIR_F);
		fwClipSetManager::AddRef_DEPRECATED(CLIP_SET_VEH_STD_DS_BASE);
		fwClipSetManager::AddRef_DEPRECATED(CLIP_SET_VEH_STD_PS_BASE);
	}

	if(!m_GestureToolPipe.Open(7564))
	{
		animAssertf(false, "Failed to connect to gesture tool proxy - gesture tool preview won't be available");
	}

	SelectFromCameraCombo();

	// Pick a speaker at random
	m_speakerSelector.Select(fwRandom::GetRandomNumberInRange(1, m_speakerSelector.GetModelCount()-5));
	SelectFromSpeakerModelCombo();

	// Pick a listener at random
	m_listenerSelector.Select(fwRandom::GetRandomNumberInRange(1, m_listenerSelector.GetModelCount()-5));
	SelectFromListenerModelCombo();

	// Move the player to behind the camera
	CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if(pLocalPlayer)
	{
		Vector3 playerPosition = pLocalPlayer->GetPreviousPosition();
		pLocalPlayer->SetPosition(m_playerPosition, true, true, true); m_playerPosition = playerPosition;
		float playerHeading = pLocalPlayer->GetCurrentHeading();
		pLocalPlayer->SetHeading(DtoR * m_playerHeading); m_playerHeading = playerHeading;
		pLocalPlayer->SetDesiredHeading(DtoR * m_playerHeading); 
	}

	// Stop the player from responding to input
	CAnimViewer::m_unInterruptableTask = true;
	CAnimViewer::ToggleUninterruptableTask();
}

void CGestureEditor::ShutdownGestureTool()
{
	m_GestureToolPipe.Close();

	CAnimViewer::m_unInterruptableTask = false;
	CAnimViewer::ToggleUninterruptableTask();

	//get rid of the test peds

	if (m_pSpeaker)
	{
		if (!m_pSpeaker->IsLocalPlayer())	
		{
			m_pSpeaker->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
			CGameWorld::Remove(m_pSpeaker);
			CPedFactory::GetFactory()->DestroyPed(m_pSpeaker);

			//get rid of the clip player pointer since it won't exist any more
			m_pSpeaker = NULL;
		}
	}
	if (m_pListener)
	{
		if (!m_pListener->IsLocalPlayer())	
		{
			m_pListener->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
			CGameWorld::Remove(m_pListener);
			CPedFactory::GetFactory()->DestroyPed(m_pListener);

			//get rid of the clip player pointer since it won't exist any more
			m_pListener = NULL;
		}
	}

	// Move the player back
	CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if(pLocalPlayer)
	{
		Vector3 playerPosition = pLocalPlayer->GetPreviousPosition();
		pLocalPlayer->SetPosition(m_playerPosition, true, true, true); m_playerPosition = playerPosition;
		float playerHeading = pLocalPlayer->GetCurrentHeading();
		pLocalPlayer->SetHeading(DtoR * m_playerHeading); m_playerHeading = playerHeading;
		pLocalPlayer->SetDesiredHeading(DtoR * m_playerHeading); 
	}

	//go back to the player cam
	camInterface::GetDebugDirector().DeactivateFreeCam();
	CControlMgr::SetDebugPadOn(false);

	fwClipSetManager::Release_DEPRECATED(CLIP_SET_SIT_CHAIR_M);
	fwClipSetManager::Release_DEPRECATED(CLIP_SET_SIT_CHAIR_F);
}

void CGestureEditor::SendGestureClipInfo()
{
	if(!CDebugClipDictionary::ClipDictionariesLoaded())
	{
		CDebugClipDictionary::LoadClipDictionaries();
	}

	char buf[128];
	for (atVector<CDebugClipDictionary *>::iterator group = CDebugClipDictionary::GetFirstDictionary();
		group != CDebugClipDictionary::GetLastDictionary(); ++group)
	{
		atString dictionaryName((*group)->GetName().GetCStr());
		dictionaryName.Lowercase();
		if(dictionaryName.StartsWith("gestures@") || dictionaryName.StartsWith("facials@"))
		{
			for(int i = 0; i < (*group)->GetClipCount(); i++)
			{
				const char *clipName = (*group)->GetClipName(i);

				const crClip* clip = fwAnimManager::GetClipIfExistsByName((*group)->GetName().GetCStr(), clipName);
				if(clip)
				{
					u32 durationMs = static_cast<u32>(clip->GetDuration()*1000.f);
					formatf(buf, 128, "%s/%s:%u\n", (*group)->GetName().GetCStr(), clipName, durationMs);
					m_GestureToolPipe.SafeWrite(buf, strlen(buf));
				}
				else
				{
					animAssertf(false, "Couldn't instantiate clip to compute duration: %s %s", (*group)->GetName().GetCStr(), clipName);
				}
			}
		}
	}

	m_GestureToolPipe.SafeWrite("END\n", 3);
}

void CGestureEditor::RecieveGestureClipInfo()
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	if(m_GestureToolPipe.IsValid())
	{
		int bytes = m_GestureToolPipe.GetReadCount();
		if(bytes)
		{
			char *buf = rage_new char[bytes+1];

			int read = 0;
			bool readEOL = false;
			while(read < bytes && !readEOL)
			{
				char b;
				m_GestureToolPipe.SafeRead(&b, 1);

				if(b == '\n')
				{
					readEOL = true;
				}
				else
				{
					buf[read++] = b;
				}
			}

			buf[read] = 0;

			if(!strncmp(buf, "!INIT!", 6))
			{
				SendGestureClipInfo();
			}
			else
			{
				char *dictionary = buf;
				char *ptr = buf;
				while(*ptr!=':')
				{
					ptr++;
				}
				*ptr = 0;

				buf = ptr+1;

				while(*ptr != ':')
				{
					ptr++;
				}
				ptr++;
				while(*ptr != ':')
				{
					ptr++;
				}
				*ptr = 0;

				s32 duration = 0;
				u32 markerTime = 0;
				f32 startPhase = 0.0f;
				f32 endPhase = 1.0f;
				f32 rate = 0.0f;
				f32 maxWeight = 0.0f;
				f32 blendInTime = 0.0f;
				f32 blendOutTime = 0.0f;
				sscanf(ptr+1, "%i:%u:%f:%f:%f:%f:%f:%f",
					&duration,
					&markerTime,
					&startPhase,
					&endPhase,
					&rate,
					&maxWeight,
					&blendInTime,
					&blendOutTime);

				const char *category = "G_S";//g_SpeechMetadataCategoryNames[AUD_SPEECH_METADATA_CATEGORY_GESTURE_SPEAKER];
				int len = istrlen(category);
				if(!strncmp(buf, category, len))
				{
					TriggerSpeakerBodyGesture(dictionary, &buf[len+1], duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime);
				}
				else
				{
					category = "G_L";//g_SpeechMetadataCategoryNames[AUD_SPEECH_METADATA_CATEGORY_GESTURE_LISTENER];
					len = istrlen(category);
					if(!strncmp(buf, category, len))
					{
						TriggerListenerBodyGesture(dictionary, &buf[len+1], duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime);
					}
					else
					{
						category = "F_L";//g_SpeechMetadataCategoryNames[AUD_SPEECH_METADATA_CATEGORY_FACIAL_LISTENER];
						len = istrlen(category);
						if(!strncmp(buf, category, len))
						{
							TriggerListenerFacialGesture(dictionary, &buf[len+1], duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime);
						}
						else
						{
							category = "F_S";//g_SpeechMetadataCategoryNames[AUD_SPEECH_METADATA_CATEGORY_FACIAL_SPEAKER];
							len = istrlen(category);
							if(!strncmp(buf, category, len))
							{
								TriggerSpeakerFacialGesture(dictionary, &buf[len+1], duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime);
							}
						}
					}
				}
			}
		}
	}
}

// TODO: substitute m_pPed for the appropriate speaker/listener ped
void CGestureEditor::TriggerListenerBodyGesture(const char *dictionary, const char *clip, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime)
{
	if (m_pListener)
	{
		TriggerBodyGesture(m_pListener, dictionary, clip, duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime, m_listenerContext, m_listenerUsePhone);
	}
}

void CGestureEditor::TriggerListenerFacialGesture(const char *dictionary, const char *clip, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime)
{

	if (m_pListener)
	{
		TriggerFacialGesture(m_pListener, dictionary, clip, duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime);
	}
}

void CGestureEditor::TriggerSpeakerBodyGesture(const char *dictionary, const char *clip, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime)
{
	if(m_pSpeaker)
	{
		TriggerBodyGesture(m_pSpeaker, dictionary, clip, duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime, m_speakerContext, m_speakerUsePhone);
	}
}

void CGestureEditor::TriggerSpeakerFacialGesture(const char *dictionary, const char *clip, const s32 duration, const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime)
{
	if(m_pSpeaker)
	{
		TriggerFacialGesture(m_pSpeaker, dictionary, clip, duration, markerTime, startPhase, endPhase, rate, maxWeight, blendInTime, blendOutTime);
	}
}

void CGestureEditor::TriggerBodyGesture(CPed *pPed, const char *dictionary, const char *clip, const s32 UNUSED_PARAM(duration), const u32 markerTime, const f32 startPhase, const f32 endPhase, const f32 rate, const f32 maxWeight, const f32 blendInTime, const f32 blendOutTime, int context, bool usePhone)
{
	if(animVerifyf(pPed == m_pSpeaker || pPed == m_pListener, "Ped is not the speaker or the listener!"))
	{
		strLocalIndex iSlot = strLocalIndex(g_ClipDictionaryStore.FindSlot(dictionary));
		if(animVerifyf(g_ClipDictionaryStore.IsValidSlot(iSlot), "Could not find clip dictionary in clip dictionary store!"))
		{
			fwClipDictionaryLoader clipDictionaryLoader(iSlot.Get());
			if(animVerifyf(clipDictionaryLoader.IsLoaded(), "Clip dictionary is not loaded!"))
			{
				// Does a gesture clip exist in the given dictionary?
				const crClip *pGestureClip = fwAnimManager::GetClipIfExistsByName(dictionary, clip);
				if(animVerifyf(pGestureClip, "Could not find clip '%s' from dictionary '%s'!", clip, dictionary))
				{
					// Figure out which bones to mask ask based on the context
					fwMvFilterId gestureFilterId(FILTER_UPPERBODYANDIK);
					float gestureExpressionWeight = 1.0f;
					CPed::GestureContext gestureContext;

					switch (context)
					{
					case CONTEXT_STANDING:
						{
							// Can use entire upper body
							gestureFilterId = FILTER_UPPERBODYANDIK;
							gestureExpressionWeight = 1.0f;
							gestureContext = CPed::GC_DEFAULT;
							CAnimViewer::m_componentRoot = Vector3(0.8f, 1.0f, 0.0f);
						}
						break;
					case CONTEXT_WALKING:
						{
							// Can't move the spine
							gestureFilterId = BONEMASK_HEAD_NECK_AND_ARMS;
							gestureExpressionWeight = 0.0f;
							gestureContext = CPed::GC_WALKING;
							CAnimViewer::m_componentRoot = Vector3(0.8f, 1.0f, 0.0f);
						}
						break;
					case CONTEXT_SITTING:
						{
							// Can't move the spine
							gestureFilterId = BONEMASK_HEAD_NECK_AND_ARMS;
							gestureExpressionWeight = 0.0f;
							gestureContext = CPed::GC_SITTING;
							CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
						}
						break;
					case CONTEXT_CAR_DRIVER:
						{
							// Can't move the spine or left arm
							gestureFilterId = BONEMASK_HEAD_NECK_AND_R_ARM;
							gestureExpressionWeight = 0.0f;
							gestureContext = CPed::GC_VEHICLE_DRIVER;
							CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
						}
						break;
					case CONTEXT_CAR_PASSENGER:
						{
							// Can't move the spine
							gestureFilterId = BONEMASK_HEAD_NECK_AND_ARMS;
							gestureExpressionWeight = 0.0f;
							gestureContext = CPed::GC_VEHICLE_PASSENGER;
							CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
						}
						break;
					default:
						{
							// Can use entire upper body
							gestureFilterId = FILTER_UPPERBODYANDIK;
							gestureExpressionWeight = 1.0f;
							gestureContext = CPed::GC_DEFAULT;
						}
						break;
					}

					if (usePhone)
					{
						// Can't move the spine, neck or head or right arm
						gestureFilterId = BONEMASK_ARMONLY_L;
						gestureExpressionWeight = 0.0f;
						gestureContext = CPed::GC_CONVERSATION_PHONE;
					}

					static audGestureData gestureData;
					gestureData.MarkerTime = markerTime;
					gestureData.StartPhase = startPhase;
					gestureData.EndPhase = endPhase;
					gestureData.Rate = rate;
					gestureData.MaxWeight = maxWeight;
					gestureData.BlendInTime = blendInTime;
					gestureData.BlendOutTime = blendOutTime;
					pPed->BlendInGesture(pGestureClip, gestureFilterId, gestureExpressionWeight, &gestureData, gestureContext);
				}
			}
		}
	}
}

void CGestureEditor::TriggerFacialGesture(CPed *pPed, const char *dictionary, const char *clip, const s32 UNUSED_PARAM(duration), const u32 UNUSED_PARAM(markerTime), const f32 UNUSED_PARAM(startPhase), const f32 UNUSED_PARAM(endPhase), const f32 UNUSED_PARAM(rate), const f32 UNUSED_PARAM(maxWeight), const f32 blendInTime, const f32 UNUSED_PARAM(blendOutTime))
{
	if (pPed && pPed->GetAnimDirector())
	{
		if(animVerifyf(pPed == m_pSpeaker || pPed == m_pListener, "Ped is not the speaker or the listener!"))
		{
			strLocalIndex iSlot = strLocalIndex(g_ClipDictionaryStore.FindSlot(dictionary));
			
			// Uncomment this verify once B* 1005872 is done.
			//  if(g_ClipDictionaryStore.IsValidSlot(iSlot), "Could not find the clip dictionary in the clip dictionary store!"))
			if (g_ClipDictionaryStore.IsValidSlot(iSlot))
			{
				fwClipDictionaryLoader clipDictionaryLoader(iSlot.Get());
				if(animVerifyf(clipDictionaryLoader.IsLoaded(), "Could not load the clip dictionary!"))
				{
					// Does a gesture clip exist in the given dictionary?
					const crClip *pGestureFacialClip = fwAnimManager::GetClipIfExistsByName(dictionary, clip);
					if(animVerifyf(pGestureFacialClip, "Could not find clip '%s' from dictionary '%s'!", clip, dictionary))
					{
						CMovePed& move = pPed->GetMovePed();

						move.PlayOneShotFacialClip(pGestureFacialClip, blendInTime);
					}
				}
			}
		}
	}
}

void CGestureEditor::ActivateBank()
{
	if(animVerifyf(m_pGestureBank, "Gesture editor has not been initialized!"))
	{
		if(!CAnimViewer::m_pDynamicEntity)
		{
			CAnimViewer::m_pDynamicEntity = FindPlayerPed();
		}

		m_pGestureBank->AddToggle("Enable", &m_bEnabled, OnEnabledChanged, "Tick this box to activate the gesture tool");

		m_pGestureBank->PushGroup("Speaker settings", true, NULL);
		{
			m_speakerSelector.AddWidgets(m_pGestureBank, "Select model", SpeakerModelChangedCB);
			m_pSpeakerContextCombo = m_pGestureBank->AddCombo("Select context", &m_speakerContext, CONTEXT_MAX, (const char **)m_gestureContextToString, SelectFromSpeakerContextCombo, "Choose a context for the speaker");
			m_pGestureBank->AddToggle("Use phone", &m_speakerUsePhone, ToggleSpeakerUsePhone, "");
			m_pGestureBank->AddSlider("Heading", &m_speakerHeading, -180.0f, 180.0f, 1.0f, UpdateGestureTool, "");
			m_pGestureBank->AddToggle("Auto rotate", &m_speakerAutoRotate );
			m_pGestureBank->AddSlider("Auto rotate rate", &m_speakerAutoRotateRate, -180.0f, 180.0f, 1.0f, UpdateGestureTool, "");
			m_pGestureBank->AddToggle("Use custom base anim", &m_speakerUseCustomBaseAnim, SpeakerCustomBaseAnimChanged);
			m_speakerCustomBaseAnim.AddWidgets(m_pGestureBank, SpeakerCustomBaseAnimChanged, SpeakerCustomBaseAnimChanged);
		}
		m_pGestureBank->PopGroup();

		m_pGestureBank->PushGroup("Listener settings", true, NULL);
		{
			m_listenerSelector.AddWidgets(m_pGestureBank, "Select model", ListenerModelChangedCB);
			m_pListenerContextCombo = m_pGestureBank->AddCombo("Select context", &m_listenerContext, CONTEXT_MAX, (const char **)m_gestureContextToString, SelectFromListenerContextCombo, "Choose a context for the listener");
			m_pGestureBank->AddToggle("Use phone", &m_listenerUsePhone, ToggleListenerUsePhone, "");
			m_pGestureBank->AddSlider("Heading", &m_listenerHeading, -180.0f, 180.0f, 1.0f, UpdateGestureTool, "");
			m_pGestureBank->AddToggle("Auto rotate", &m_listenerAutoRotate );
			m_pGestureBank->AddSlider("Auto rotate rate", &m_listenerAutoRotateRate, -180.0f, 180.0f, 1.0f, UpdateGestureTool, "");
			m_pGestureBank->AddToggle("Use custom base anim", &m_listenerUseCustomBaseAnim, ListenerCustomBaseAnimChanged);
			m_listenerCustomBaseAnim.AddWidgets(m_pGestureBank, ListenerCustomBaseAnimChanged, ListenerCustomBaseAnimChanged);
		}
		m_pGestureBank->PopGroup();

		m_pGestureBank->PushGroup("Camera settings", true, NULL);
		{
			m_pCameraCombo = m_pGestureBank->AddCombo("Select ", &m_cameraIndex, CAMERA_MAX, (const char **)m_gestureCameraToString, SelectFromCameraCombo, "Choose a camera");
		}
		m_pGestureBank->PopGroup(); // "camera settings"
	}
}

void CGestureEditor::DeactivateBank()
{
	if(animVerifyf(m_pGestureBank, "Gesture editor has not been initialized!"))
	{
		if(m_bEnabled)
		{
			m_bEnabled = false;
			OnEnabledChanged();
		}

		m_speakerCustomBaseAnim.RemoveWidgets(m_pGestureBank);

		m_listenerCustomBaseAnim.RemoveWidgets(m_pGestureBank);

		m_pGestureBank->RemoveAllMainWidgets();
	}
}

void CGestureEditor::SelectFromSpeakerModelCombo()
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	// Push viewport
	gVpMan.PushViewport(gVpMan.GetGameViewport());

	// Do we want the new speaker be the player?
	if (!strcmp(m_speakerSelector.GetSelectedModelName(), "player"))
	{
		// Is the current listener the player?
		if (m_pListener && !strcmp(CModelInfo::GetBaseModelInfoName(m_pListener->GetModelId()), "player"))
		{
			return;
		}
	}

	if (m_pSpeaker)
	{
		// Is the current speaker the player
		if (!strcmp(CModelInfo::GetBaseModelInfoName(m_pSpeaker->GetModelId()), "player"))
		{
			// UpdateGestureTool will move the player offscreen
			m_pSpeaker = NULL;
		}
		else
		{
			CGameWorld::Remove(m_pSpeaker);
			CPedFactory::GetFactory()->DestroyPed(m_pSpeaker);
			m_pSpeaker = NULL;
		}
	}

	// Is the new speaker the player?
	if (!strcmp(m_speakerSelector.GetSelectedModelName(), "player"))
	{
		// UpdateGestureTool will set the players position to
		CPed * pPlayerPed  = FindPlayerPed();
		animAssert(pPlayerPed);
		m_pSpeaker = pPlayerPed;
	}
	else
	{
		// Load the selected model
		fwModelId modelId = m_speakerSelector.GetSelectedModelId();
		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		// Create a new speaker using the selected model
		const CControlledByInfo localAiControl(false, false);
		m_pSpeaker = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, NULL, true, true, false);

		// Speaker should be a tool char
		m_pSpeaker->PopTypeSet(POPTYPE_TOOL);
		m_pSpeaker->SetDefaultDecisionMaker();
		m_pSpeaker->SetCharParamsBasedOnManagerType();
		m_pSpeaker->GetPedIntelligence()->SetDefaultRelationshipGroup();

		// Add the new speaker to the world
		CGameWorld::Add(m_pSpeaker, CGameWorld::OUTSIDE );
	}


	// Set the position of the new speaker
	m_pSpeaker->SetPosition(m_speakerPosition, true, true, true);

	// Set the heading of the new speaker
	m_pSpeaker->SetHeading(( DtoR * m_speakerHeading));
	m_pSpeaker->SetDesiredHeading(( DtoR * m_speakerHeading));

	// Set the health of the new speaker
	m_pSpeaker->SetHealth(100.0f);

	// This overrides the default task CTaskPlayerOnFoot which keep starting idle clips
	// It also stops other task from starting
	//m_pSpeaker->GetPedIntelligence()->AddTaskDefault(new CTaskSimpleUninterruptable());
	m_pSpeaker->GetPedIntelligence()->AddTaskDefault(rage_new CTaskDoNothing(-1));

	//stop any collision, ragdolling etc from interfering with the ped
	m_pSpeaker->SetUsingLowLodPhysics(true);

	SelectFromSpeakerContextCombo();
	ToggleSpeakerUsePhone();

	// Set the listener to listen to the speaker
	if (m_pListener)
	{
		m_pListener->SetSpeakerListenedTo(m_pSpeaker);
	}

	// Select the speaker (necessary for "Make Ped Speak" to work)
	g_PickerManager.SetEnabled(true);
	g_PickerManager.SetUiEnabled(false);
	g_PickerManager.AddEntityToPickerResults(m_pSpeaker, true, true);
	CAnimViewer::SetSelectedEntity(m_pSpeaker);

	// Pop viewport
	gVpMan.PopViewport();
}

void CGestureEditor::SelectFromSpeakerContextCombo()
{
	if (!m_pSpeaker)
		return;

	m_pSpeaker->GetPedIntelligence()->ClearTasks(true, true);

	m_speakerClipHelper.SetAssociatedEntity(m_pSpeaker);

	CPedModelInfo* pModelInfo = m_pSpeaker->GetPedModelInfo();

	int flags = APF_ISLOOPED;
	switch (m_speakerContext)
	{
	case CONTEXT_STANDING:
		{
			if(!m_speakerUseCustomBaseAnim || !m_speakerCustomBaseAnim.GetSelectedClip())
			{
				m_speakerClipHelper.StartClipBySetAndClip(m_pSpeaker, pModelInfo->GetMovementClipSet(), CLIP_IDLE, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 1.0f, 0.0f);
		}
		break;
	case CONTEXT_WALKING:
		{
			if(!m_speakerUseCustomBaseAnim || !m_speakerCustomBaseAnim.GetSelectedClip())
			{
				m_speakerClipHelper.StartClipBySetAndClip(m_pSpeaker, pModelInfo->GetMovementClipSet(), CLIP_WALK, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 1.0f, 0.0f);
		}
		break;
	case CONTEXT_SITTING:
		{
			if (pModelInfo->IsMale())
			{
				if (CDebugClipDictionary::StreamDictionary(fwAnimManager::GetClipDictIndex(CLIP_SET_SIT_CHAIR_M)))
				{
					if(!m_speakerUseCustomBaseAnim || !m_speakerCustomBaseAnim.GetSelectedClip())
					{
						m_speakerClipHelper.StartClipBySetAndClip(m_pSpeaker, CLIP_SET_SIT_CHAIR_M, CLIP_SIT_IDLE, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
					}
				}
			}
			else
			{
				if (CDebugClipDictionary::StreamDictionary(fwAnimManager::GetClipDictIndex(CLIP_SET_SIT_CHAIR_F)))
				{
					if(!m_speakerUseCustomBaseAnim || !m_speakerCustomBaseAnim.GetSelectedClip())
					{
						m_speakerClipHelper.StartClipBySetAndClip(m_pSpeaker, CLIP_SET_SIT_CHAIR_F, CLIP_SIT_IDLE, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
					}
				}
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
		}
		break;
	case CONTEXT_CAR_DRIVER:
		{
			if (CDebugClipDictionary::StreamDictionary(fwAnimManager::GetClipDictIndex(CLIP_SET_SIT_CHAIR_M)))
			{
				if(!m_speakerUseCustomBaseAnim || !m_speakerCustomBaseAnim.GetSelectedClip())
				{
					m_speakerClipHelper.StartClipBySetAndClip(m_pSpeaker, CLIP_SET_VEH_STD_DS_BASE, CLIP_VEH_STD_SIT, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
				}
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
		}
		break;
	case CONTEXT_CAR_PASSENGER:
		{
			if (CDebugClipDictionary::StreamDictionary(fwAnimManager::GetClipDictIndex(CLIP_SET_SIT_CHAIR_M)))
			{
				if(!m_speakerUseCustomBaseAnim || !m_speakerCustomBaseAnim.GetSelectedClip())
				{
					m_speakerClipHelper.StartClipBySetAndClip(m_pSpeaker, CLIP_SET_VEH_STD_PS_BASE, CLIP_VEH_STD_SIT, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
				}
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
		}
		break;
	default:
		{
		}
		break;
	}

	if(m_speakerUseCustomBaseAnim && m_speakerCustomBaseAnim.GetSelectedClip())
	{
		m_speakerClipHelper.StartClipByClip(m_pSpeaker, m_speakerCustomBaseAnim.GetSelectedClip(), flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	}

	CTask *pTask = m_pSpeaker->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
	if(m_speakerUsePhone)
	{
		if(!pTask)
		{
			pTask = rage_new CTaskMobilePhone(CTaskMobilePhone::Mode_ToCall);
			m_pSpeaker->GetPedIntelligence()->AddTaskSecondary(pTask, PED_TASK_SECONDARY_PARTIAL_ANIM);
		}
	}
	else
	{
		if(pTask)
		{
			m_pSpeaker->GetPedIntelligence()->ClearSecondaryTask(PED_TASK_SECONDARY_PARTIAL_ANIM);
		}
	}

	if(fwAnimManager::GetClipIfExistsBySetId(pModelInfo->GetMovementClipSet(), CLIP_ADDITIVE_FAT))
	{
		m_speakerGenericClipHelper.BlendInClipBySetAndClip(m_pSpeaker, pModelInfo->GetMovementClipSet(), CLIP_ADDITIVE_FAT, INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, false, true);
		m_pSpeaker->GetMovePed().SetAdditiveNetwork(m_speakerGenericClipHelper);
	}
}

void CGestureEditor::SelectFromListenerModelCombo()
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	// Push viewport
	gVpMan.PushViewport(gVpMan.GetGameViewport());

	// Do we want the new listener be the player?
	if (!strcmp(m_listenerSelector.GetSelectedModelName(), "player"))
	{
		// Is the current speaker the player?
		if (m_pListener && !strcmp(CModelInfo::GetBaseModelInfoName(m_pSpeaker->GetModelId()), "player"))
		{
			return;
		}
	}

	if (m_pListener)
	{
		// Is the current listener the player
		if (!strcmp(CModelInfo::GetBaseModelInfoName(m_pListener->GetModelId()), "player"))
		{
			// UpdateGestureTool will move the player offscreen
			m_pListener = NULL;
		}
		else
		{
			CGameWorld::Remove(m_pListener);
			CPedFactory::GetFactory()->DestroyPed(m_pListener);
			m_pListener = NULL;
		}
	}

	// Is the new listener the player?
	if (!strcmp(m_listenerSelector.GetSelectedModelName(), "player"))
	{
		// UpdateGestureTool will set the players position to
		CPed * pPlayerPed  = FindPlayerPed();
		animAssert(pPlayerPed);
		m_pListener = pPlayerPed;
	}
	else
	{
		// Load the selected model
		fwModelId modelId = m_listenerSelector.GetSelectedModelId();
		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		// Create a new listener using the selected model
		const CControlledByInfo localAiControl(false, false);
		m_pListener = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, NULL, true, true, false);

		// Listener should be a tool char
		m_pListener->PopTypeSet(POPTYPE_TOOL);
		m_pListener->SetDefaultDecisionMaker();
		m_pListener->SetCharParamsBasedOnManagerType();
		m_pListener->GetPedIntelligence()->SetDefaultRelationshipGroup();

		// Add the new listener to the world
		CGameWorld::Add(m_pListener, CGameWorld::OUTSIDE );
	}

	// Set the position of the new listener
	m_pListener->SetPosition(m_listenerPosition, true, true, true);

	// Set the heading of the new listener
	m_pListener->SetHeading((DtoR * m_listenerHeading));
	m_pListener->SetDesiredHeading((DtoR * m_listenerHeading));

	// Set the health of the new listener
	m_pListener->SetHealth(100.0f);

	// Stop the listener from falling
	//m_pListener->SetFixedPhysics(true);
	m_pListener->SetUsingLowLodPhysics(true);

	// This overrides the default task CTaskPlayerOnFoot which keep starting idle clips
	// It also stops other task from starting
	//m_pListener->GetPedIntelligence()->AddTaskDefault(new CTaskSimpleUninterruptable());
	m_pListener->GetPedIntelligence()->AddTaskDefault(rage_new CTaskDoNothing(-1));

	m_pListener->SetSpeakerListenedTo(m_pSpeaker);

	SelectFromListenerContextCombo();
	ToggleListenerUsePhone();

	// Pop viewport
	gVpMan.PopViewport();
}

void CGestureEditor::SelectFromListenerContextCombo()
{
	if (!m_pListener)
		return;

	m_pListener->GetPedIntelligence()->ClearTasks(true, true);

	m_listenerClipHelper.SetAssociatedEntity(m_pListener);

	CPedModelInfo* pModelInfo = m_pListener->GetPedModelInfo();

	int flags = APF_ISLOOPED;
	switch (m_listenerContext)
	{
	case CONTEXT_STANDING:
		{
			if(!m_listenerUseCustomBaseAnim || !m_listenerCustomBaseAnim.GetSelectedClip())
			{
				m_listenerClipHelper.StartClipBySetAndClip(m_pListener, pModelInfo->GetMovementClipSet(), CLIP_IDLE, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 1.0f, 0.0f);
		}
		break;
	case CONTEXT_WALKING:
		{
			if(!m_listenerUseCustomBaseAnim || !m_listenerCustomBaseAnim.GetSelectedClip())
			{
				m_listenerClipHelper.StartClipBySetAndClip(m_pListener, pModelInfo->GetMovementClipSet(), CLIP_WALK, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 1.0f, 0.0f);
		}
		break;
	case CONTEXT_SITTING:
		{
			if (pModelInfo->IsMale())
			{
				if(!m_listenerUseCustomBaseAnim || !m_listenerCustomBaseAnim.GetSelectedClip())
				{
					m_listenerClipHelper.StartClipBySetAndClip(m_pListener, CLIP_SET_SIT_CHAIR_M, CLIP_SIT_IDLE, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
				}
			}
			else
			{
				if(!m_listenerUseCustomBaseAnim || !m_listenerCustomBaseAnim.GetSelectedClip())
				{
					m_listenerClipHelper.StartClipBySetAndClip(m_pListener, CLIP_SET_SIT_CHAIR_F, CLIP_SIT_IDLE, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
				}
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 1.0f, 0.0f);
		}
		break;
	case CONTEXT_CAR_DRIVER:
		{
			if(!m_listenerUseCustomBaseAnim || !m_listenerCustomBaseAnim.GetSelectedClip())
			{
				m_listenerClipHelper.StartClipBySetAndClip(m_pListener, CLIP_SET_VEH_STD_DS_BASE, CLIP_VEH_STD_SIT, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
		}
		break;
	case CONTEXT_CAR_PASSENGER:
		{
			if(!m_listenerUseCustomBaseAnim || !m_listenerCustomBaseAnim.GetSelectedClip())
			{
				m_listenerClipHelper.StartClipBySetAndClip(m_pListener, CLIP_SET_VEH_STD_PS_BASE, CLIP_VEH_STD_SIT, flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
			}
			CAnimViewer::m_componentRoot = Vector3(0.8f, 0.0f, 0.0f);
		}
		break;
	default:
		{
		}
		break;
	}

	if(m_listenerUseCustomBaseAnim && m_listenerCustomBaseAnim.GetSelectedClip())
	{
		m_listenerClipHelper.StartClipByClip(m_pListener, m_listenerCustomBaseAnim.GetSelectedClip(), flags, BONEMASK_ALL, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	}

	CTask *pTask = m_pListener->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
	if(m_listenerUsePhone)
	{
		if(!pTask)
		{
			pTask = rage_new CTaskMobilePhone(CTaskMobilePhone::Mode_ToCall);
			m_pListener->GetPedIntelligence()->AddTaskSecondary(pTask, PED_TASK_SECONDARY_PARTIAL_ANIM);
		}
	}
	else
	{
		if(pTask)
		{
			m_pListener->GetPedIntelligence()->ClearSecondaryTask(PED_TASK_SECONDARY_PARTIAL_ANIM);
		}
	}

	if(fwAnimManager::GetClipIfExistsBySetId(pModelInfo->GetMovementClipSet(), CLIP_ADDITIVE_FAT))
	{
		m_listenerGenericClipHelper.BlendInClipBySetAndClip(m_pListener, pModelInfo->GetMovementClipSet(), CLIP_ADDITIVE_FAT, INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, false, true);
		m_pListener->GetMovePed().SetAdditiveNetwork(m_listenerGenericClipHelper);
	}
}


void CGestureEditor::SelectFromCameraCombo()
{
	//Turn on debug (free) cam.
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	debugDirector.ActivateFreeCam();
	camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();

	Vector3 front = freeCamFrame.GetFront(); //(0.0f, 1.0f, 0.0f);
	Vector3 up = freeCamFrame.GetUp(); //(0.0f, 1.0f, 0.0f);
	Vector3 position(0.0f, 0.0f, 0.0f);
	switch (m_cameraIndex)
	{
	case CAMERA_SPEAKER_AND_LISTENER:
		{
			m_speakerPosition = Vector3(-0.5f, 0.0f, 1.0f);
			m_listenerPosition = Vector3(0.5f, 0.0f, 1.0f);

			position.x = 0.0f;
			position.y = -2.5f;
			position.z = 0.85f;

			// move debug camera to desired place
			freeCamFrame.SetWorldMatrixFromFrontAndUp(front, up);
			freeCamFrame.SetPosition(position);

			CControlMgr::SetDebugPadOn(true);
		}
		break;

	case CAMERA_SPEAKER_FULL:
		{
			m_speakerPosition = Vector3(0.0f, 0.0f, 1.0f);
			m_listenerPosition = Vector3(0.0f, 0.0f, 100.0f);

			position.x = m_speakerPosition.x;
			position.y = m_speakerPosition.y - 2.5f;
			position.z = m_speakerPosition.z - 0.15f;

			// move debug camera to desired place
			freeCamFrame.SetWorldMatrixFromFrontAndUp(front, up);
			freeCamFrame.SetPosition(position);

			CControlMgr::SetDebugPadOn(true);
		}
		break;

	case CAMERA_SPEAKER_THREE_QUARTER:
		{
			m_speakerPosition = Vector3(0.0f, 0.0f, 1.0f);
			m_listenerPosition = Vector3(0.0f, 0.0f, 100.0f);

			position.x = m_speakerPosition.x;
			position.y = m_speakerPosition.y - 1.75f;
			position.z = m_speakerPosition.z + 0.2f;

			// move debug camera to desired place
			freeCamFrame.SetWorldMatrixFromFrontAndUp(front, up);
			freeCamFrame.SetPosition(position);

			CControlMgr::SetDebugPadOn(true);
		}
		break;

	case CAMERA_SPEAKER_HEAD:
		{
			m_speakerPosition = Vector3(0.0f, 0.0f, 1.0f);
			m_listenerPosition = Vector3(0.0f, 0.0f, 100.0f);

			position.x = m_speakerPosition.x;
			position.y = m_speakerPosition.y - 0.5f;
			position.z = m_speakerPosition.z + 0.65f;

			// move debug camera to desired place
			freeCamFrame.SetWorldMatrixFromFrontAndUp(front, up);
			freeCamFrame.SetPosition(position);

			CControlMgr::SetDebugPadOn(true);
		}
		break;

	case CAMERA_LISTENER_FULL:
		{
			m_speakerPosition = Vector3(0.0f, 0.0f, 100.0f);
			m_listenerPosition = Vector3(0.0f, 0.0f, 1.0f);

			position.x = m_listenerPosition.x;
			position.y = m_listenerPosition.y - 2.5f;
			position.z = m_listenerPosition.z - 0.15f;

			// move debug camera to desired place
			freeCamFrame.SetWorldMatrixFromFrontAndUp(front, up);
			freeCamFrame.SetPosition(position);

			CControlMgr::SetDebugPadOn(true);
		}
		break;

	case CAMERA_LISTENER_THREE_QUARTER:
		{
			m_speakerPosition = Vector3(0.0f, 0.0f, 100.0f);
			m_listenerPosition = Vector3(0.0f, 0.0f, 1.0f);

			position.x = m_listenerPosition.x;
			position.y = m_listenerPosition.y - 1.75f;
			position.z = m_listenerPosition.z + 0.2f;

			// move debug camera to desired place
			freeCamFrame.SetWorldMatrixFromFrontAndUp(front, up);
			freeCamFrame.SetPosition(position);

			CControlMgr::SetDebugPadOn(true);
		}
		break;

	case CAMERA_LISTENER_HEAD:
		{
			m_speakerPosition = Vector3(0.0f, 0.0f, 100.0f);
			m_listenerPosition = Vector3(0.0f, 0.0f, 1.0f);

			position.x = m_listenerPosition.x;
			position.y = m_listenerPosition.y - 0.5f;
			position.z = m_listenerPosition.z + 0.65f;

			// move debug camera to desired place
			freeCamFrame.SetWorldMatrixFromFrontAndUp(front, up);
			freeCamFrame.SetPosition(position);

			CControlMgr::SetDebugPadOn(true);
		}
		break;

	default:
		{
		}
		break;
	}
}

void CGestureEditor::ToggleSpeakerUsePhone()
{
	SelectFromSpeakerContextCombo();
}

void CGestureEditor::ToggleListenerUsePhone()
{
	SelectFromListenerContextCombo();
}

void CGestureEditor::SpeakerCustomBaseAnimChanged()
{
	SelectFromSpeakerContextCombo();
}

void CGestureEditor::ListenerCustomBaseAnimChanged()
{
	SelectFromListenerContextCombo();
}

void CGestureEditor::UpdateGestureTool()
{
	//early out if the tool is disabled
	if (!m_bEnabled)
		return;


	if (m_pSpeaker)
	{
		m_pSpeaker->SetIsStanding(true);
		m_pSpeaker->SetPosition(m_speakerPosition, true, true, true);

		// Auto-rotate
		if ( m_speakerAutoRotate )
		{
			m_speakerHeading += m_speakerAutoRotateRate * m_deltaForAutoRotate;

			// Clamp heading
			if ( m_speakerHeading < -180.0f )
			{
				m_speakerHeading += 360.0f;
			}
			else if ( m_speakerHeading > 180.0f )
			{
				m_speakerHeading -= 360.0f;
			}
		}

		m_pSpeaker->SetHeading(( DtoR * m_speakerHeading));
		m_pSpeaker->SetDesiredHeading(( DtoR * m_speakerHeading));
		m_pSpeaker->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodEventScanning);
	}

	if (m_pListener)
	{
		m_pListener->SetIsStanding(true);
		m_pListener->SetPosition(m_listenerPosition, true, true, true);

		// Auto-rotate
		if ( m_listenerAutoRotate )
		{
			m_listenerHeading += m_listenerAutoRotateRate * m_deltaForAutoRotate;

			// Clamp heading
			if ( m_listenerHeading < -180.0f )
			{
				m_listenerHeading += 360.0f;
			}
			else if ( m_listenerHeading > 180.0f )
			{
				m_listenerHeading -= 360.0f;
			}
		}

		m_pListener->SetHeading(( DtoR * m_listenerHeading));
		m_pListener->SetDesiredHeading(( DtoR * m_listenerHeading));
		m_pListener->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodEventScanning);
	}

	RecieveGestureClipInfo();
}


#endif //__BANK
