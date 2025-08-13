// 
// animation/debug/AnimPlacementTool.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#include "AnimPlacementTool.h"

// rage includes
#include "bank/msgbox.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentsyncedscene.h"

// game includes
#include "animation/debug/AnimViewer.h"
#include "animation/MoveObject.h"
#include "camera/CamInterface.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "debug/DebugScene.h"
#include "event/Events.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "parser/manager.h"
#include "peds/PedFactory.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "peds/rendering/PedVariationDebug.h"
#include "peds/rendering/PedVariationStream.h"
#include "physics/physics.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "streaming/streaming.h"
#include "task/Animation/AnimTaskFlags.h"
#include "task/General/TaskBasic.h"
#include "task/System/TaskManager.h"
#include "debug/DebugScene.h"
#include "vehicles/vehiclefactory.h"
#include "vehicleAi/VehicleIntelligence.h"

ANIM_OPTIMISATIONS()

// structure for anim placement tools
atArray<CAnimPlacementTool> CAnimPlacementEditor::m_animPlacementTools;

// If true, the anims will operate in synchronised mode (i.e. a single root matrix will be
// used for all anim tools, and their authored initial offsets will be applied)
bool CAnimPlacementEditor::sm_bSyncMode = true;

char CAnimPlacementEditor::m_newLocationName[CAnimPlacementEditor::locationNameBufferSize];
bkGroup* CAnimPlacementEditor::sm_pLocationsGroup = NULL;
atArray<CLocationInfo*> CAnimPlacementEditor::sm_Locations;
int	CAnimPlacementEditor::sm_SelectedLocationIdx = 0;

float CAnimPlacementEditor::sm_masterPhase = 0.0f;

float CAnimPlacementEditor::sm_masterDuration = 0.0f;

bool CAnimPlacementEditor::sm_masterPause = true;

atHashString CAnimPlacementEditor::sm_AudioWidget;
atHashString CAnimPlacementEditor::sm_Audio;
bool CAnimPlacementEditor::sm_IsAudioSkipped = false;
bool CAnimPlacementEditor::sm_IsAudioPrepared = false;
bool CAnimPlacementEditor::sm_IsAudioPlaying = false;

fwSyncedSceneId CAnimPlacementEditor::ms_sceneId = -1;

atArray<bkGroup*> CAnimPlacementEditor::m_toolGroups;

// Camera editing
bool CAnimPlacementEditor::m_bOverrideCam = false; 
Vector3 CAnimPlacementEditor::m_vCameraPos = VEC3_ZERO; 
Vector3 CAnimPlacementEditor::m_vCameraRot = VEC3_ZERO; 
bool CAnimPlacementEditor::m_bOverrideCamUsingMatrix = false;
Matrix34 CAnimPlacementEditor::m_cameraMtx = M34_IDENTITY;

float CAnimPlacementEditor::m_fFov = 45.0f; 
Vector4 CAnimPlacementEditor::m_vDofPlanes = Vector4(Vector4::ZeroType);  
bool CAnimPlacementEditor::m_bShouldOverrideUseLightDof = false;
float CAnimPlacementEditor::m_fMotionBlurStrength = 0.0f;

//pointer to the rag bank
fwDebugBank* CAnimPlacementEditor::m_pBank = NULL;

bool CAnimPlacementEditor::m_rebuildWidgets = false;	

bool CAnimPlacementTool::sm_bRenderMoverTracks = false;
bool CAnimPlacementTool::sm_bRenderAnimValues = false;
bool CAnimPlacementTool::sm_bRenderDebugDraw = true;

CDebugPedModelSelector CAnimPlacementEditor::sm_modelSelector;
CDebugVehicleSelector CAnimPlacementEditor::sm_vehicleSelector;
CDebugObjectSelector CAnimPlacementEditor::sm_objectSelector;

float CAnimPlacementEditor::sm_BlendInDelta = INSTANT_BLEND_IN_DELTA;
float CAnimPlacementEditor::sm_MoverBlendInDelta = INSTANT_BLEND_IN_DELTA;
float CAnimPlacementEditor::sm_BlendOutDelta = INSTANT_BLEND_OUT_DELTA;

eSyncedSceneFlagsBitSet CAnimPlacementEditor::sm_SyncedSceneFlags;
eRagdollBlockingFlagsBitSet CAnimPlacementEditor::sm_RagdollBlockingFlags;
eIkControlFlagsBitSet CAnimPlacementEditor::sm_IkControlFlags;

EXTERN_PARSER_ENUM(eSyncedSceneFlags);
EXTERN_PARSER_ENUM(eRagdollBlockingFlags);
EXTERN_PARSER_ENUM(eIkControlFlags);

extern const char* parser_eSyncedSceneFlags_Strings[];
extern const char* parser_eRagdollBlockingFlags_Strings[];
extern const char* parser_eIkControlFlags_Strings[];

CAnimPlacementTool* CAnimPlacementEditor::sm_selectedTool = NULL;

CDebugObjectSelector CAnimPlacementEditor::ms_propSelector;

//////////////////////////////////////////////////////////////////////////
// animated camera members
CDebugClipSelector CAnimPlacementEditor::ms_cameraAnimSelector(true, false, false);
bool CAnimPlacementEditor::ms_UseAnimatedCamera = false;

//////////////////////////////////////////////////////////////////////////
// scene attachment
atArray<bkCombo*> CAnimPlacementEditor::ms_attachBoneSelectors;
int CAnimPlacementEditor::ms_selectedAttachBone = 0;

//////////////////////////////////////////////////////////////////////////
// entity management
CSyncedSceneEntityManager CAnimPlacementEditor::m_SyncedSceneEntityManager; 
bkCombo *CAnimPlacementEditor::ms_pSceneNameCombo = nullptr;
char CAnimPlacementEditor::ms_SceneFolder[RAGE_MAX_PATH] = "assets:/anim/synced_scenes";
atArray< atString > CAnimPlacementEditor::ms_SceneNameStrings;
atArray< const char * > CAnimPlacementEditor::ms_SceneNames;
int CAnimPlacementEditor::ms_iSceneName = 0;
char CAnimPlacementEditor::ms_SceneName[RAGE_MAX_PATH] = "";

//////////////////////////////////////////////////////////////////////////
//	Manager for saving and loading synced scenes via the tools 
//////////////////////////////////////////////////////////////////////////

CSyncedSceneEntityManager::~CSyncedSceneEntityManager()
{
}

CSyncedSceneEntityManager::CSyncedSceneEntityManager()
{
}

bool CSyncedSceneEntityManager::Save(const char * name, const char * folder)
{
	StoreSceneInfo();
	PopulateEntityList();

	return fwSyncedSceneEntityManager::Save(name, folder);
}

bool CSyncedSceneEntityManager::Load(const char * name, const char * folder)
{
	bool bfileLoaded = false;

	bfileLoaded = fwSyncedSceneEntityManager::Load(name, folder);

	if(bfileLoaded)
	{
		CAnimPlacementEditor::sm_AudioWidget.Clear();
		CAnimPlacementEditor::AudioWidgetChanged();

		CAnimPlacementEditor::sm_AudioWidget = m_SceneInfo.GetAudio(); // Don't set sm_Audio directly, allow it to load as if the user had entered the text
		CAnimPlacementEditor::AudioWidgetChanged();

		GenerateLocationInfoFromSceneInfo();

		CAnimPlacementEditor:: ToggleEditor(); 
		CAnimPlacementEditor:: ToggleEditor(); 

		CreateSyncSceneWidgets(); 
	}

	return bfileLoaded;
}

void CSyncedSceneEntityManager::CreateAttachedEntites(u32 index)
{
	CDynamicEntity* pChild = CAnimPlacementEditor::m_animPlacementTools[index].GetTestEntity();
	CDynamicEntity* pParent = CAnimPlacementEditor::m_animPlacementTools[index].GetParentEntity();

	atHashString AnimDict = CAnimPlacementEditor::m_animPlacementTools[index].GetAnimSelector().GetSelectedClipDictionary();
	atHashString AnimName = CAnimPlacementEditor::m_animPlacementTools[index].GetAnimSelector().GetSelectedClipName();

	//Create the attachment info
	s32 ParentBone = -1; 
	s32 ChildBone = -1; 
	Vector3 vOffset(VEC3_ZERO); 
	Vector3 vRotation(VEC3_ZERO); 

	CDebugAttachmentTool* pAttachTool = CAnimPlacementEditor::m_animPlacementTools[index].GetAnimDebugAttachTool(); 
	if(pAttachTool)
	{
		ParentBone = pAttachTool->GetParentBone(); 
		ChildBone = pAttachTool->GetChildBone(); 
		vOffset = pAttachTool->GetOffset(); 
		vRotation = pAttachTool->GetRotation(); 
	}

	for(int i =0; i < m_pEntities.GetCount(); i++)
	{
		if(pParent == m_pEntities[i])
		{		
			fwSyncedSceneEntity* NewEntity = NULL; 

			if(pChild->GetIsTypePed())
			{
				NewEntity = rage_new fwSyncedSceneEntity((u32)SST_PED, AnimDict, AnimName, pChild->GetModelName()); 
			}

			if(pChild->GetIsTypeObject())
			{
				NewEntity = rage_new fwSyncedSceneEntity((u32)SST_OBJECT, AnimDict, AnimName, pChild->GetModelName()); 
			}

			if(pChild->GetIsTypeVehicle())
			{
				NewEntity = rage_new fwSyncedSceneEntity((u32)SST_VEHICLE, AnimDict, AnimName, pChild->GetModelName()); 
			}

			if(NewEntity)
			{
				NewEntity->SetAttachmentInfo(vOffset, vRotation, ParentBone, ChildBone); 
				m_SyncedSceneEntites[i].AddAttachedEntity(*NewEntity); 
			}
		}
	}
}

void CSyncedSceneEntityManager::CreateAttachedWidgets(u32 index, fwEntity* pEnt)
{
	for (int i=0; i<m_SyncedSceneEntites[index].GetAttachedEntities().GetCount(); i++)
	{
		fwSyncedSceneEntity* pChild = &m_SyncedSceneEntites[index].GetAttachedEntities()[i]; 

		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromName(pChild->GetModelName(), &modelId);

		CEntity *pEntity = static_cast< CEntity * >(pEnt);
		CDynamicEntity *pDynamicEntity = pEntity->GetIsDynamic() ? static_cast< CDynamicEntity * >(pEntity) : NULL;

		CAnimPlacementEditor::AddAnimPlacementToolAttachedProp(modelId, pDynamicEntity); 

		CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimSelector().SelectClip(pChild->GetAnimDict().GetHash(), pChild->GetAnimName().GetHash() );
		CAnimPlacementEditor::m_animPlacementTools.Top().SetAnim();

		CDebugAttachmentTool* pAttachTool = CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimDebugAttachTool(); 
		if(pAttachTool)
		{
			pAttachTool->SetPosition(pChild->GetAttachementInfo().GetOffset()); 
			pAttachTool->SetRotation(pChild->GetAttachementInfo().GetRotation()); 
			pAttachTool->SetParentBone((s16)pChild->GetAttachementInfo().GetParentBone()); 
			pAttachTool->SetChildBone((s16)pChild->GetAttachementInfo().GetChildBone()); 
			pAttachTool->Attach(); 
		}
	}
}

void CSyncedSceneEntityManager::CreateSyncSceneWidgets()
{
	for (int i = 0; i < m_SyncedSceneEntites.GetCount(); i++)
	{
		if (animVerifyf(m_SyncedSceneEntites[i].GetModelName().IsNotNull(), "Synced Scene Entity %i name is null!", i))
		{
			fwModelId modelId;

			switch (m_SyncedSceneEntites[i].GetType())
			{
			case SST_PED:
				{
					CModelInfo::GetBaseModelInfoFromName(m_SyncedSceneEntites[i].GetModelName(), &modelId);
					CAnimPlacementEditor::AddAnimPlacementTool(modelId, CAnimPlacementEditor::sm_Locations[CAnimPlacementEditor::sm_SelectedLocationIdx]->m_masterRootMatrix);

					CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimSelector().SelectClip(m_SyncedSceneEntites[i].GetAnimDict().GetHash(), m_SyncedSceneEntites[i].GetAnimName().GetHash());
					CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimSelector().SetCachedClip(m_SyncedSceneEntites[i].GetAnimDict().GetHash(), m_SyncedSceneEntites[i].GetAnimName().GetHash());
					CAnimPlacementEditor::m_animPlacementTools.Top().SetAnim();

					CreateAttachedWidgets(i, CAnimPlacementEditor::m_animPlacementTools.Top().GetTestEntity());
				} break;
			case SST_OBJECT:
				{
					CModelInfo::GetBaseModelInfoFromName(m_SyncedSceneEntites[i].GetModelName(), &modelId);
					CAnimPlacementEditor::AddAnimPlacementToolObject(modelId, CAnimPlacementEditor::sm_Locations[CAnimPlacementEditor::sm_SelectedLocationIdx]->m_masterRootMatrix);

					CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimSelector().SelectClip(m_SyncedSceneEntites[i].GetAnimDict().GetHash(), m_SyncedSceneEntites[i].GetAnimName().GetHash());
					CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimSelector().SetCachedClip(m_SyncedSceneEntites[i].GetAnimDict().GetHash(), m_SyncedSceneEntites[i].GetAnimName().GetHash());
					CAnimPlacementEditor::m_animPlacementTools.Top().SetAnim();

					CreateAttachedWidgets(i, CAnimPlacementEditor::m_animPlacementTools.Top().GetTestEntity());
				} break;
			case SST_CAMERA:
				{
					CAnimPlacementEditor::ms_UseAnimatedCamera = true;

					CAnimPlacementEditor::ms_cameraAnimSelector.SetCachedClip(m_SyncedSceneEntites[i].GetAnimDict().GetHash(), m_SyncedSceneEntites[i].GetAnimName().GetHash());

					CAnimPlacementEditor::m_rebuildWidgets = true;
				} break;
			case SST_VEHICLE:
				{
					CModelInfo::GetBaseModelInfoFromName(m_SyncedSceneEntites[i].GetModelName(), &modelId);
					CAnimPlacementEditor::AddAnimPlacementToolVehicle(modelId, CAnimPlacementEditor::sm_Locations[CAnimPlacementEditor::sm_SelectedLocationIdx]->m_masterRootMatrix);

					CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimSelector().SelectClip(m_SyncedSceneEntites[i].GetAnimDict().GetHash(), m_SyncedSceneEntites[i].GetAnimName().GetHash());
					CAnimPlacementEditor::m_animPlacementTools.Top().GetAnimSelector().SetCachedClip(m_SyncedSceneEntites[i].GetAnimDict().GetHash(), m_SyncedSceneEntites[i].GetAnimName().GetHash());
					CAnimPlacementEditor::m_animPlacementTools.Top().SetAnim();

					CreateAttachedWidgets(i, CAnimPlacementEditor::m_animPlacementTools.Top().GetTestEntity());
				} break;
			default:
				{
					animAssertf(false, "SyncedSceneEntity type %u is unknown!", m_SyncedSceneEntites[i].GetType());
				} break;
			}
		}
	}
}

void CSyncedSceneEntityManager::PopulateEntityList()
{
	//reset the list of entities
	m_SyncedSceneEntites.Reset(); 
	m_pEntities.Reset(); 

	//create a list of parent entites
	for(int i=0; i <CAnimPlacementEditor::m_animPlacementTools.GetCount(); i++)
	{
		if(!CAnimPlacementEditor::m_animPlacementTools[i].GetParentEntity())
		{
			atHashString AnimDict = CAnimPlacementEditor::m_animPlacementTools[i].GetAnimSelector().GetSelectedClipDictionary();
			atHashString AnimName = CAnimPlacementEditor::m_animPlacementTools[i].GetAnimSelector().GetSelectedClipName();
			CDynamicEntity* pDynamicEnt = CAnimPlacementEditor::m_animPlacementTools[i].GetTestEntity();  

			if(pDynamicEnt)
			{
				if(pDynamicEnt->GetIsTypePed())
				{
					StoreSyncedSceneEntity(SST_PED, AnimDict, AnimName, pDynamicEnt->GetModelName());
				}

				if(pDynamicEnt->GetIsTypeObject())
				{
					StoreSyncedSceneEntity(SST_OBJECT, AnimDict, AnimName, pDynamicEnt->GetModelName());
				}

				if(pDynamicEnt->GetIsTypeVehicle())
				{
					StoreSyncedSceneEntity(SST_VEHICLE, AnimDict, AnimName, pDynamicEnt->GetModelName());
				}

				m_pEntities.PushAndGrow(pDynamicEnt); // create a list of parent entities
			}
		}
	}

	//add the children entities
	for(int i=0; i < CAnimPlacementEditor::m_animPlacementTools.GetCount(); i++)
	{
		if(CAnimPlacementEditor::m_animPlacementTools[i].GetParentEntity()) // go through any child entities
		{
			CreateAttachedEntites(i); 
		}
	}

	if(CAnimPlacementEditor::ms_UseAnimatedCamera)
	{
		atHashString AnimDict = CAnimPlacementEditor::ms_cameraAnimSelector.GetSelectedClipDictionary();
		atHashString AnimName = CAnimPlacementEditor::ms_cameraAnimSelector.GetSelectedClipName();

		StoreSyncedSceneEntity(SST_CAMERA, AnimDict, AnimName, "Camera");
	}
}

void CSyncedSceneEntityManager::GenerateLocationInfoFromSceneInfo()
{
	USE_DEBUG_MEMORY();

	// Clear existing locations and assume selection of the first.
	for (int j=0; j<CAnimPlacementEditor::sm_Locations.GetCount(); j++)
	{
		delete CAnimPlacementEditor::sm_Locations[j];
	}
	CAnimPlacementEditor::sm_Locations.clear();
	CAnimPlacementEditor::sm_SelectedLocationIdx = 0;

	const int iNumLocations = m_SceneInfo.GetNumLocations();
	if (iNumLocations > 0)
	{
		for (int i=0; i<iNumLocations; i++)
		{
			CLocationInfo* pNewLocation = rage_new CLocationInfo();

			// Copy over the details
			Matrix34 SyncedSceneMat(M34_IDENTITY);
			SyncedSceneMat.FromEulersXYZ(m_SceneInfo.GetLocationRotation(i) * DtoR);
			SyncedSceneMat.d = m_SceneInfo.GetLocationOrigin(i); 

			pNewLocation->m_masterOrientation.Set(m_SceneInfo.GetLocationRotation(i));
			pNewLocation->m_masterRootMatrix.Set(SyncedSceneMat);

			sprintf(pNewLocation->m_name, "%s", m_SceneInfo.GetLocationName(i).c_str());

			CAnimPlacementEditor::sm_Locations.PushAndGrow(pNewLocation);
		}
	}
	else
	{
		// There must always be one at the minimum.
		CLocationInfo* pDefaultLocation = rage_new CLocationInfo();

		CAnimPlacementEditor::sm_Locations.PushAndGrow(pDefaultLocation);
	}
}

void CSyncedSceneEntityManager::StoreSceneInfo()
{
	m_SceneInfo.SetSceneName(CAnimPlacementEditor::ms_SceneName);

	m_SceneInfo.SetAudio(CAnimPlacementEditor::sm_Audio);

	// Clear existing locations
	m_SceneInfo.ClearLocations();

	// Add the current set
	for (int i=0; i<CAnimPlacementEditor::sm_Locations.GetCount(); i++)
	{
		m_SceneInfo.AddLocation(CAnimPlacementEditor::sm_Locations[i]->m_masterOrientation, CAnimPlacementEditor::sm_Locations[i]->m_masterRootMatrix.d, CAnimPlacementEditor::sm_Locations[i]->m_name);
	}
}

fwSyncedSceneEntityManager &GetSyncedSceneEntityManager()
{
	return CAnimPlacementEditor::m_SyncedSceneEntityManager;
}

//////////////////////////////////////////////////////////////////////////
//	Anim placement editor fuctionality
//////////////////////////////////////////////////////////////////////////
void CAnimPlacementEditor::Initialise()
{
	// AnimPlacementTools don't have copy constructors, so they don't survive the array being resized, so just make it a bit bigger here.
	m_animPlacementTools.Reset();
	m_animPlacementTools.Reserve(64);

	m_pBank = fwDebugBank::CreateBank("Anim SyncedScene Editor", MakeFunctor(CAnimPlacementEditor::ActivateBank), MakeFunctor(CAnimPlacementEditor::DeactivateBank));

	sm_modelSelector.Select("P_M_Zero");

	CDebugScene::RegisterEntityChangeCallback(datCallback(CFA1(CAnimPlacementEditor::FocusEntityChanged), 0, true));
}

void CAnimPlacementEditor::FocusEntityChanged(CEntity* pEntity)
{

	UpdateAttachBoneList(pEntity);

	if (pEntity->GetIsDynamic())
	{
		CDynamicEntity* pDyn = static_cast<CDynamicEntity*>(pEntity);

		//check if it's one of our test entities
		for (int i=0;i<m_animPlacementTools.GetCount();i++)
		{
			if (pDyn==m_animPlacementTools[i].GetTestEntity())
			{
				sm_selectedTool = &m_animPlacementTools[i];
				return;
			}
		}
	}
	sm_selectedTool = NULL;
}
 
void CAnimPlacementEditor::Update()
{
	if (m_pBank && m_pBank->IsActive())
	{
		if(ms_sceneId != INVALID_SYNCED_SCENE_ID && sm_masterDuration == 0.0f)
		{
			fwAnimDirectorComponentSyncedScene::UpdateSyncedSceneDuration(ms_sceneId);
			sm_masterDuration = fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(ms_sceneId);
		}

		if(ms_sceneId != INVALID_SYNCED_SCENE_ID && sm_Audio)
		{
			if(!sm_IsAudioPrepared && !sm_IsAudioPlaying)
			{
				u32 uSkipTimeMs = static_cast< u32 >((sm_masterPhase * sm_masterDuration) * 1000.0f);
				if(uSkipTimeMs > 0 && !sm_IsAudioSkipped)
				{
					g_pSyncedSceneAudioInterface->Skip(sm_Audio.GetHash(), uSkipTimeMs);

					sm_IsAudioSkipped = true;
				}

				sm_IsAudioPrepared = fwAnimDirectorComponentSyncedScene::PrepareSynchronizedSceneAudioEvent(ms_sceneId, sm_Audio.GetCStr());
			}

			if(!sm_masterPause)
			{
				if(sm_IsAudioPrepared && !sm_IsAudioPlaying)
				{
					sm_IsAudioPlaying = fwAnimDirectorComponentSyncedScene::PlaySynchronizedSceneAudioEvent(ms_sceneId);
				}
				else if(sm_IsAudioPlaying)
				{
					if(fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(ms_sceneId) >= 1.0f)
					{
						fwAnimDirectorComponentSyncedScene::StopSynchronizedSceneAudioEvent(ms_sceneId);

						sm_IsAudioSkipped = false;
						sm_IsAudioPrepared = false;
						sm_IsAudioPlaying = false;
					}
				}
			}
			else
			{
				if(sm_IsAudioPlaying)
				{
					fwAnimDirectorComponentSyncedScene::StopSynchronizedSceneAudioEvent(ms_sceneId);

					sm_IsAudioSkipped = false;
					sm_IsAudioPrepared = false;
					sm_IsAudioPlaying = false;
				}
			}
		}

		int i=0;
		while ( i<m_animPlacementTools.GetCount() )
		{
			if (m_animPlacementTools[i].Update(true))
			{
				i++;
			}
			else
			{
				//this tool has failed - delete it
				DeleteTool(&m_animPlacementTools[i]);
			}
		}

		if (ms_UseAnimatedCamera)
		{
			
			camInterface::GetSyncedSceneDirector().DebugStartRender(camSyncedSceneDirector::ANIM_PLACEMENT_TOOL_CLIENT); 
		}
		else
		{
				camInterface::GetSyncedSceneDirector().DebugStopRender(camSyncedSceneDirector::ANIM_PLACEMENT_TOOL_CLIENT); 

				//render the camera matrix
				if(CAnimPlacementTool::sm_bRenderDebugDraw)
				{
					const camBaseCamera* cam = camInterface::GetSyncedSceneDirector().GetRenderedCamera();
					if(cam)
					{
						grcDebugDraw::Axis(RCC_MAT34V(cam->GetFrame().GetWorldMatrix()), 0.5f, true);
						char displayText[20];
						sprintf(displayText, "cam: fov=%.3f", cam->GetFrame().GetFov());
						grcDebugDraw::Text(cam->GetFrame().GetWorldMatrix().d, Color_yellow, displayText);
					}
				}
		}

		if (sm_bSyncMode)
		{
			fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(ms_sceneId, sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix);

			//render the master root matrix
			if(CAnimPlacementTool::sm_bRenderDebugDraw)
			{
				Matrix34 root(M34_IDENTITY);
				fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(ms_sceneId, root);

				grcDebugDraw::Axis( root, 1.0f, true);
			}

			fwAnimDirectorComponentSyncedScene::SetSyncedScenePaused(ms_sceneId, sm_masterPause);

			if (sm_masterPause)
			{
				if(sm_Audio.IsNull())
				{
					fwAnimDirectorComponentSyncedScene::SetSyncedScenePhase(ms_sceneId, sm_masterPhase);
				}
			}
			else
			{
				sm_masterPhase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(ms_sceneId);
			}

			// Camera Override
			if (camInterface::GetSyncedSceneDirector().GetRenderedCamera())
			{
				camAnimatedCamera* pCam	= static_cast<camAnimatedCamera*>(camInterface::GetSyncedSceneDirector().GetRenderedCamera());
				if (pCam)
				{
					if (m_bOverrideCam)
					{
						Matrix34 mCameraMat;
						mCameraMat.Identity();
						mCameraMat.FromEulersYXZ(m_vCameraRot * DtoR);
						mCameraMat.d = m_vCameraPos;

						// Make it relative to the selected location
						mCameraMat.Dot(sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix);
						
						pCam->OverrideCameraMatrix(mCameraMat);
						pCam->OverrideCameraFov(m_fFov);
						pCam->OverrideCameraDof(m_vDofPlanes);
						pCam->OverrideCameraCanUseLightDof(m_bShouldOverrideUseLightDof);
						pCam->OverrideCameraMotionBlur(m_fMotionBlurStrength);
					}
					else if (m_bOverrideCamUsingMatrix)
					{
						Matrix34 mCameraMat(m_cameraMtx);

						// Make it relative to the selected location
						mCameraMat.Dot(sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix);

						pCam->OverrideCameraMatrix(mCameraMat);
						pCam->OverrideCameraFov(m_fFov);
						pCam->OverrideCameraDof(m_vDofPlanes);
						pCam->OverrideCameraCanUseLightDof(m_bShouldOverrideUseLightDof);
						pCam->OverrideCameraMotionBlur(m_fMotionBlurStrength);
					}
					else
					{
						Matrix34 mCameraMat(pCam->GetMatrix());
						Matrix34 mRootMat(sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix);
						mRootMat.Inverse();

						// Make it relative to the selected location
						mCameraMat.Dot(mRootMat);

						m_vCameraPos.Set(mCameraMat.d);
						Vector3 vEulers;
						mCameraMat.ToEulersFastYXZ(vEulers);
						m_vCameraRot.SetX(vEulers.x * RtoD);
						m_vCameraRot.SetY(vEulers.y * RtoD);
						m_vCameraRot.SetZ(vEulers.z * RtoD);
					}
				}
			}
		}

		if (m_animPlacementTools.GetCount()>0 || sm_bSyncMode) 
		{
			UpdateMouseControl();
		}

		if (m_rebuildWidgets)
		{
			//should regenerate the widgets for this tool
			RebuildBank();
		}
	}			
}

void CAnimPlacementEditor::UpdateMouseControl()
{
	if (sm_selectedTool)
	{
		if( ioMouse::GetButtons() & ioMouse::MOUSE_LEFT )
		{
			//only drag the matrix if the mouse has moved this frame
			if (CDebugScene::MouseMovedThisFrame())
			{
				//mouse is held down - update selected matrix position

				Vector3 vPos;
				Vector3 vNormal;
				void *entity;

				bool bHasPosition = false;

				bHasPosition = CDebugScene::GetWorldPositionUnderMouse(vPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE , &vNormal, &entity);

				if (bHasPosition)
				{
					//update the necessary matrix position here
					if (sm_bSyncMode)
					{
						sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.d = vPos;
					}
					else if ( sm_selectedTool )
					{	
						//draw a debug cursor to give some perspective on placement height
						if(CAnimPlacementTool::sm_bRenderDebugDraw)
						{
							grcDebugDraw::Sphere(vPos, 0.1f, Color_yellow1);
							grcDebugDraw::Line(vPos, vPos+Vector3(0.0f, 0.0f, PED_HUMAN_GROUNDTOROOTOFFSET),Color_yellow1, Color_yellow1);
						}

						vPos.z+= PED_HUMAN_GROUNDTOROOTOFFSET;

						sm_selectedTool->SetRootMatrixPosition(vPos);
					}
				}
			}			
		}
	}
}

// PURPOSE: Rotates the tool that owns the specified ped by the specified amount around its local z axis
void CAnimPlacementEditor::RotateTool(CPed* pPed, float deltaHeading)
{
	for (int i=0; i<m_animPlacementTools.GetCount(); i++)
	{
		if (m_animPlacementTools[i].GetTestEntity()==pPed)
		{
			if (sm_bSyncMode)
			{
				sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.RotateLocalZ(deltaHeading);
				sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.ToEulersXYZ(sm_Locations[sm_SelectedLocationIdx]->m_masterOrientation);
			}
			else
			{
				m_animPlacementTools[i].GetRootMatrix().RotateLocalZ(deltaHeading);
				m_animPlacementTools[i].RecalcOrientation();
			}
			return;
		}
	}
}



// PURPOSE: Translates the tool that owns the specified ped by the provided position delta
void CAnimPlacementEditor::TranslateTool(CPed* pPed, const Vector3 &deltaPosition)
{
	for (int i=0; i<m_animPlacementTools.GetCount(); i++)
	{
		if (m_animPlacementTools[i].GetTestEntity()==pPed)
		{
			if (sm_bSyncMode)
			{
				sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.Translate(deltaPosition);
			}
			else
			{
				m_animPlacementTools[i].GetRootMatrix().Translate(deltaPosition);
			}
			return;
		}
	}
}

bool CAnimPlacementEditor::IsUsingPed(CPed* pPed)
{
	for (int i=0; i<m_animPlacementTools.GetCount(); i++)
	{
		if (m_animPlacementTools[i].GetTestEntity()==pPed)
			return true;
	}
	return false;
}


void CAnimPlacementEditor::Shutdown()
{
	if(m_pBank)
    {
		m_pBank->Shutdown();
	    m_pBank = NULL;
    }

	{
		USE_DEBUG_MEMORY();
		for (int j=0; j<sm_Locations.GetCount(); j++)
		{
			delete sm_Locations[j];
		}
		sm_Locations.clear();
	}
}

void CAnimPlacementEditor::DeactivateBank()
{
	camInterface::GetSyncedSceneDirector().StopAnimatingCamera(camSyncedSceneDirector::ANIM_PLACEMENT_TOOL_CLIENT); 

	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(ms_sceneId))
	{
		fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(ms_sceneId); //Remove our extra ref on the scene
		ms_sceneId = -1;
	}

	RemoveWidgets();

	ms_cameraAnimSelector.Shutdown();

	for (int i=0; i<m_animPlacementTools.GetCount(); i++)
	{
		m_animPlacementTools[i].Shutdown();
	}

	m_animPlacementTools.clear();

	m_toolGroups.clear();
}

void CAnimPlacementEditor::ActivateBank()
{
	// Create a default location if we need to, we may have already loaded a scene at this point
	bool bCreatedDefaultLocation = false;
	if (sm_Locations.GetCount() == 0)
	{
		USE_DEBUG_MEMORY();
		for (int j=0; j<sm_Locations.GetCount(); j++)
		{
			delete sm_Locations[j];
		}
		sm_Locations.clear();
		CLocationInfo* pDefaultLocation = rage_new CLocationInfo();
		sm_Locations.PushAndGrow(pDefaultLocation);
		sm_SelectedLocationIdx = 0;

		bCreatedDefaultLocation = true;
	}

	// if this is the first time, reposition the tool to somewhere we can see
	if (sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.IsClose(M34_IDENTITY) || sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.IsClose(M34_ZERO))
	{
		if (bCreatedDefaultLocation)
		{
			Matrix34 toolRoot(camInterface::GetMat());
			// remove any pitch or roll on the camera matrix
			// and move the tool forward a little (so the camera can see it)
			toolRoot.MakeUpright();
			toolRoot.d+=(camInterface::GetMat().b * 2.0f);

			sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.Set(toolRoot);
		}

		//make sure all the tools are paused (so they don't get out of sync
		for (int i=0; i<m_animPlacementTools.GetCount(); i++)
		{
			if (!m_animPlacementTools[i].IsPaused())
			{
				m_animPlacementTools[i].TogglePause();
			}
		}
	}

	RebuildBank();

	ms_sceneId = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();
	fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(ms_sceneId); //add an extra ref to the scene here we don't want it getting cleared till the tool is done with it
	fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped(ms_sceneId, true); //set this to loop by default
}

void CAnimPlacementEditor::RebuildBank()
{
	atString sceneName(ms_SceneName);

	// remove any widgets created by the editor
	RemoveWidgets();

	AddWidgets(m_pBank);

	UpdateLocationWidgets();

	SceneFolderChanged();

	for (int i = 0; i < ms_SceneNames.GetCount(); i++)
	{
		if (stricmp(ms_SceneNames[i], sceneName.c_str()) == 0)
		{
			ms_iSceneName = i;

			SceneNameChanged();

			break;
		}
	}

	m_rebuildWidgets = false;
}

void CAnimPlacementEditor::UpdateAttachBoneList(CEntity* pEnt)
{
	atArray<const char*> boneNames;

	boneNames.PushAndGrow("None");
	ms_selectedAttachBone = 0;

	if (pEnt && pEnt->GetIsDynamic() && pEnt->GetSkeleton())
	{
		CDynamicEntity* pDyn = static_cast<CDynamicEntity*>(pEnt);

		for (int i=0; i < pDyn->GetSkeletonData().GetNumBones(); ++i)
		{
			boneNames.PushAndGrow(pDyn->GetSkeletonData().GetBoneData(i)->GetName());
		}
	}
	
	for (int i=0; i<ms_attachBoneSelectors.GetCount(); i++)
	{
		ms_attachBoneSelectors[i]->UpdateCombo("Attach bone", &ms_selectedAttachBone, boneNames.GetCount(), &boneNames[0],NullCB, "The bone on the focus entity the scene will be attached to");
	}

}

void CAnimPlacementEditor::SceneFolderEnumerator(const fiFindData &findData, void *pUserArg)
{
	const char *szFolder = static_cast<const char *>(pUserArg);
	if (szFolder)
	{
		atVarString path("%s/%s", szFolder, findData.m_Name);

		if ((findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			/* It's a directory! */

			ASSET.EnumFiles(path.c_str(), SceneFolderEnumerator, static_cast< void * >(const_cast< char * >(path.c_str())));
		}
		else
		{
			/* It's not a directory, it must be a file! */

			atVarString root("%s/", ms_SceneFolder);

			path.Replace(root.c_str(), "");

			ms_SceneNameStrings.PushAndGrow(atString(path.c_str()));
		}
	}
}

int SceneFolderCompare(const atString *pLeft, const atString *pRight)
{
	if (pLeft && pRight)
	{
		/* Both strings are not null */

		atString left(*pLeft);
		atArray< atString > leftTokens;
		left.Split(leftTokens, "/", true);

		atString right(*pRight);
		atArray< atString > rightTokens;
		right.Split(rightTokens, "/", true);

		if (leftTokens.GetCount() > 0 && rightTokens.GetCount() > 0)
		{
			/* Both strings contain a name */

			atString leftName = leftTokens[leftTokens.GetCount() - 1];
			atString rightName = rightTokens[rightTokens.GetCount() - 1];

			if (leftTokens.GetCount() > 1 && rightTokens.GetCount() > 1)
			{
				/* Both strings contain some folders */

				atArray< atString > leftFolders;
				leftFolders.ResizeGrow(leftTokens.GetCount() - 1);
				for (int i = 0; i < leftFolders.GetCount(); i++)
				{
					leftFolders[i] = leftTokens[i];
				}

				atArray< atString > rightFolders;
				rightFolders.ResizeGrow(rightTokens.GetCount() - 1);
				for (int i = 0; i < rightFolders.GetCount(); i++)
				{
					rightFolders[i] = rightTokens[i];
				}

				for (int i = 0; i < leftFolders.GetCount() && i < rightFolders.GetCount(); i++)
				{
					int result = strcmp(leftFolders[i], rightFolders[i]);

					if (result != 0)
					{
						return result;
					}
				}

				/* Both strings folders match */

				if (leftFolders.GetCount() != rightFolders.GetCount())
				{
					/* One string contains more folders */

					return leftFolders.GetCount() < rightFolders.GetCount() ? 1 : -1;
				}
			}
			else if(leftTokens.GetCount() > 1 || rightTokens.GetCount() > 1)
			{
				/* One string contains some folders */

				return leftTokens.GetCount() < rightTokens.GetCount() ? 1 : -1;
			}

			/* Compare names */

			return strcmp(leftName, rightName);
		}
		else if (leftTokens.GetCount() != rightTokens.GetCount())
		{
			/* One string contains a name */

			return leftTokens.GetCount() < rightTokens.GetCount() ? 1 : -1;
		}

		/* Both strings are empty */

		return 0;
	}
	else if (pLeft || pRight)
	{
		/* One string is not null */

		return pLeft && !pRight ? 1 : -1;
	}

	/* Both strings are null  */

	return 0;
}

void CAnimPlacementEditor::SceneFolderChanged()
{
	ms_SceneNameStrings.Reset();
	ms_SceneNames.Reset();
	ms_iSceneName = 0;

	ASSET.EnumFiles(ms_SceneFolder, SceneFolderEnumerator, ms_SceneFolder);

	if (ms_SceneNameStrings.GetCount() == 0)
	{
		ms_SceneNameStrings.PushAndGrow(atString("Empty"));
	}
	else
	{
		ms_SceneNameStrings.QSort(0, -1, SceneFolderCompare);
	}

	ms_SceneNames.ResizeGrow(ms_SceneNameStrings.GetCount());
	for (int i = 0; i < ms_SceneNameStrings.GetCount(); i++)
	{
		ms_SceneNames[i] = ms_SceneNameStrings[i].c_str();
	}

	if (ms_pSceneNameCombo)
	{
		ms_pSceneNameCombo->UpdateCombo("Scene Name", &ms_iSceneName, ms_SceneNames.GetCount(), &ms_SceneNames[0], SceneNameChanged);
	}

	SceneNameChanged();
}

void CAnimPlacementEditor::SceneNameChanged()
{
	if (ms_iSceneName >= 0 && ms_iSceneName < ms_SceneNames.GetCount())
	{
		if (ms_iSceneName > 1 || strcmp(ms_SceneNames[0], "Empty") != 0)
		{
			formatf(ms_SceneName, "%s", ms_SceneNames[ms_iSceneName]);
		}
	}
}

void CAnimPlacementEditor::SaveSyncedSceneFile()
{
	atString sceneName(ms_SceneName);

	m_SyncedSceneEntityManager.Save(ms_SceneName, ms_SceneFolder);

	SceneFolderChanged();

	for (int i = 0; i < ms_SceneNames.GetCount(); i++)
	{
		if (stricmp(ms_SceneNames[i], sceneName.c_str()) == 0)
		{
			ms_iSceneName = i;

			SceneNameChanged();

			break;
		}
	}
}

void CAnimPlacementEditor::LoadSyncedScene()
{
	atString sceneName(ms_SceneName);

	m_SyncedSceneEntityManager.Load(ms_SceneName, ms_SceneFolder);

	for (int i = 0; i < ms_SceneNames.GetCount(); i++)
	{
		if (stricmp(ms_SceneNames[i], sceneName.c_str()) == 0)
		{
			ms_iSceneName = i;

			SceneNameChanged();

			break;
		}
	}
}

void CAnimPlacementEditor::AddAnimPlacementToolAttachedProp(fwModelId modelId, CDynamicEntity* pEnt)
{
	CPhysical* pChild = (CPhysical*)CScriptDebug::CreateScriptEntity(modelId.GetModelIndex()); 
	CPhysical* pParent = NULL; 

	if(pChild && pEnt)
	{
		if(pEnt->GetIsPhysical() && pChild->GetIsPhysical())
			{
			pParent = static_cast<CPhysical*>(pEnt);
		}
		else
				{
			bkMessageBox("Cant Attach no physical entities", "Couldn't create the selected prop - try another?", bkMsgOk, bkMsgError);
			return; 
				}
	}
	else
	{
		bkMessageBox("Prop creation failed", "Couldn't create the selected prop - try another?", bkMsgOk, bkMsgError);
		return; 
	}

	if(pChild && pParent)
	{
		//Create the new attachment tool
		CDebugAttachmentTool* pAttachment = rage_new CDebugAttachmentTool(); 
		pAttachment->Initialise(pChild, pParent); 

		//Create new anim placement tool
		CAnimPlacementTool* pAnimTool = rage_new CAnimPlacementTool( pAttachment, pParent ); 
		m_animPlacementTools.PushAndGrow(*pAnimTool);

		Matrix34 toolRoot = MAT34V_TO_MATRIX34(pChild->GetMatrix());

		if(pChild->GetIsTypePed())
			{
			CPed* pPed = static_cast<CPed*>(pChild); 			
			m_animPlacementTools.Top().Initialise(pPed, true); 
			m_animPlacementTools.Top().SetRootMatrix(toolRoot);
			}

		if(pChild->GetIsTypeObject())
				{
			CObject* pObject = static_cast<CObject*>(pChild); 
			m_animPlacementTools.Top().Initialise(pObject, true); 
			m_animPlacementTools.Top().SetRootMatrix(toolRoot);
				}

		m_rebuildWidgets = true; 
			}
			else
			{
		bkMessageBox("Prop creation failed", "Couldn't create the selected prop - try another?", bkMsgOk, bkMsgError);
		return; 
			}
		}


void CAnimPlacementEditor::CreateAnimatedProp()
{
	if (sm_selectedTool)
	{
		AddAnimPlacementToolAttachedProp(ms_propSelector.GetSelectedModelId(), sm_selectedTool->GetTestEntity());
	}
	else
	{
		bkMessageBox("No tool selected", "Please select a tool to attach an animated prop to it", bkMsgOk, bkMsgError);
	}

}

void CAnimPlacementEditor::AddWidgets(bkBank* pBank)
{
	m_toolGroups.PushAndGrow(pBank->PushGroup("Anim synced scene editor", true));
	{
		sm_pLocationsGroup = m_pBank->PushGroup("Locations"); // "Locations list"
		{
		}
		m_pBank->PopGroup();

		m_pBank->AddText("New location name:", m_newLocationName, locationNameBufferSize, false );
		m_pBank->AddButton("Add new location", AddLocation);

		pBank->AddSlider("Master phase", &sm_masterPhase, 0.0f,1.0f,0.01f);

		pBank->AddText("Duration", &sm_masterDuration, true);

		pBank->AddButton("Play / pause all", TogglePause);
		SceneFolderChanged();
		pBank->AddText("Scene Folder", ms_SceneFolder, RAGE_MAX_PATH, false, SceneFolderChanged);
		ms_pSceneNameCombo = pBank->AddCombo("Scene Name", &ms_iSceneName, ms_SceneNames.GetCount(), &ms_SceneNames[0], SceneNameChanged);
		pBank->AddText("Scene Name", ms_SceneName, 64, false ); 
		pBank->AddButton("Save Synced Scene", SaveSyncedSceneFile );
		pBank->AddButton("Load Synced Scene", LoadSyncedScene);
		pBank->AddButton("Output script", OutputScriptCalls);

		pBank->PushGroup("Camera Editing", false);
		{
			pBank->PushGroup("Camera Overrides", false);
			{
				pBank->AddToggle("Override Cam (relative to selected location)", &m_bOverrideCam, NullCB, "Override Cam"); 
				pBank->AddVector("Cam Position", &m_vCameraPos, -100000.0f, 100000.0f, 0.25f);
				pBank->AddVector("Cam Rotation", &m_vCameraRot, -360.0f, 360.0f, 1.0f);

				pBank->AddToggle("Override Cam Using Matrix (relative to selected location)", &m_bOverrideCamUsingMatrix, NullCB, "Override Cam Using Matrix"); 
				pBank->AddMatrix("Cam Matrix", &m_cameraMtx, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.01f);

				pBank->AddSlider("Cam FOV", &m_fFov, g_MinFov, g_MaxFov, 0.1f);

				pBank->AddSlider("Near Out Of Focus Dof Plane", &m_vDofPlanes.x, 0.0, 5000.0f, 0.1f);
				pBank->AddSlider("Near In Focus Dof Plane", &m_vDofPlanes.y, 0.0f, 5000.0f, 0.1f);
				pBank->AddSlider("Far In Focus Dof Plane", &m_vDofPlanes.z, 0.0f,  5000.0f, 0.1f);
				pBank->AddSlider("Far Out Of Focus Dof Plane", &m_vDofPlanes.w, 0.0f,  5000.0f, 0.1f);

				pBank->AddToggle("Use Shallow Dof", &m_bShouldOverrideUseLightDof, NullCB, "Use Shallow Dof"); 
				pBank->AddSlider("Cam Motion Blur", &m_fMotionBlurStrength, 0.0f,  1.0f, 0.01f);
			}
			pBank->PopGroup();
		}
		pBank->PopGroup();

		pBank->PushGroup("Animated camera");
		{
			pBank->AddToggle("Use animated camera", &ms_UseAnimatedCamera);
			ms_cameraAnimSelector.AddWidgets(pBank, CAnimPlacementEditor::SelectCameraAnim, CAnimPlacementEditor::SelectCameraAnim );
			SelectCameraAnim();
		}
		pBank->PopGroup(); //Animated camera

		pBank->PushGroup("Attach scene");
		{
			const char * entityString = "Entity";
			ms_attachBoneSelectors.PushAndGrow(pBank->AddCombo("Attach bone:", &ms_selectedAttachBone, 1, &entityString));
			pBank->AddButton("Attach scene to selected entity", CAnimPlacementEditor::AttachScene);
			pBank->AddButton("Detach scene", CAnimPlacementEditor::DetachScene);
		}
		pBank->PopGroup(); //Animated camera

		pBank->PushGroup("Options", false);
		{
			pBank->AddToggle("Enable entity dragging", &CDebugScene::ms_bEnableFocusEntityDragging);
			pBank->AddToggle("Render selected entity skeleton", &CAnimViewer::m_renderSkeleton);
			pBank->AddToggle("Draw dynamic collision", &CPhysics::ms_bDrawDynamicCollision);
			pBank->AddToggle("Render mover tracks", &CAnimPlacementTool::sm_bRenderMoverTracks);
			pBank->AddToggle("Render extracted values", &CAnimPlacementTool::sm_bRenderAnimValues);
			pBank->AddToggle("Render debug draw", &CAnimPlacementTool::sm_bRenderDebugDraw);
		}
		pBank->PopGroup();

		pBank->PushGroup("Add Audio", true);
		{
			pBank->AddText("Audio", &sm_AudioWidget, false, AudioWidgetChanged);
		}
		pBank->PopGroup();

		pBank->PushGroup("Add Ped", true);
		{
			pBank->AddButton("Add with selected ped", AddAnimPlacementToolSelectedPed);
			sm_modelSelector.AddWidgets(pBank, "Ped model");
			AddSyncedSceneFlagWidgets(pBank);
			AddRagdollBlockingFlagWidgets(pBank);
			AddIkControlFlagWidgets(pBank);
			pBank->AddButton("Create and add to placement tool", AddAnimPlacementToolCB);
		}
		pBank->PopGroup();
		pBank->PushGroup("Add Vehicle", true);
		{
			pBank->AddButton("Add with selected vehicle", AddAnimPlacementToolSelectedVehicle);
			sm_vehicleSelector.AddWidgets(pBank, "Vehicle model");
			AddSyncedSceneFlagWidgets(pBank);
			pBank->AddButton("Add placement tool", AddAnimPlacementToolVehicleCB);
		}
		pBank->PopGroup();
		pBank->PushGroup("Add Object", true);
		{
			pBank->AddButton("Add with selected object", AddAnimPlacementToolSelectedObject);
			sm_objectSelector.AddWidgets(pBank, "Object model");
			AddSyncedSceneFlagWidgets(pBank);
			pBank->AddButton("Create and add to placement tool", AddAnimPlacementToolObjectCB);
		}
		pBank->PopGroup();

		pBank->PushGroup("Add animated prop");
		{
			ms_propSelector.AddWidgets(pBank, "Prop model:");
			pBank->AddButton("Create animated prop (selected tool)", CAnimPlacementEditor::CreateAnimatedProp);
		}
		pBank->PopGroup(); //"Add animated prop"

		//recreate the widgets for any existing tools
		for (int i=0;i<m_animPlacementTools.GetCount(); i++)
		{
			const char * groupName = NULL;
			if (m_animPlacementTools[i].GetTestEntity())
			{
				groupName = CModelInfo::GetBaseModelInfoName(m_animPlacementTools[i].GetTestEntity()->GetModelId());
			}

			if (!groupName)
			{
				groupName = "Tool";
			}

			m_animPlacementTools[i].AddWidgets(pBank, groupName, true, NullCB, !sm_bSyncMode);
			m_animPlacementTools[i].SetAnim();
		}
	}
	pBank->PopGroup(); // "Anim placement tool"

	UpdateLocationWidgets();
}

void CAnimPlacementEditor::AddSyncedSceneFlagWidgets(bkBank* pBank)
{
	pBank->PushGroup("Flags");
		// Add a toggle for each bit
		for(int i = 0; i < PARSER_ENUM(eSyncedSceneFlags).m_NumEnums; i++)
		{
			const char* name = parser_eSyncedSceneFlags_Strings[i];
			if (name)
			{
				u32 bitsPerBlock = sizeof(u32) * 8;
				int block = i / bitsPerBlock;
				int bitInBlock = i - (block * bitsPerBlock);
				pBank->AddToggle(name, (reinterpret_cast<u32*>(&sm_SyncedSceneFlags.BitSet()) + block), (u32)(1 << bitInBlock));
			}
		}
	pBank->PopGroup(); 
}

void CAnimPlacementEditor::AddRagdollBlockingFlagWidgets(bkBank* pBank)
{
	pBank->PushGroup("Ragdoll blocking flags"); 
	// Add a toggle for each bit
	for(int i = 0; i < PARSER_ENUM(eRagdollBlockingFlags).m_NumEnums; i++)
	{
		const char* name = parser_eRagdollBlockingFlags_Strings[i];
		if (name)
		{
			u32 bitsPerBlock = sizeof(u32) * 8;
			int block = i / bitsPerBlock;
			int bitInBlock = i - (block * bitsPerBlock);
			pBank->AddToggle(name, (reinterpret_cast<u32*>(&sm_RagdollBlockingFlags.BitSet()) + block), (u32)(1 << bitInBlock));
		}
	}
	pBank->PopGroup(); 
}

void CAnimPlacementEditor::AddIkControlFlagWidgets(bkBank* pBank)
{
	pBank->PushGroup("Ik control flags"); 
	// Add a toggle for each bit
	for(int i = 0; i < PARSER_ENUM(eIkControlFlags).m_NumEnums; i++)
	{
		const char* name = parser_eIkControlFlags_Strings[i];
		if (name)
		{
			u32 bitsPerBlock = sizeof(u32) * 8;
			int block = i / bitsPerBlock;
			int bitInBlock = i - (block * bitsPerBlock);
			pBank->AddToggle(name, (reinterpret_cast<u32*>(&sm_IkControlFlags.BitSet()) + block), (u32)(1 << bitInBlock));
		}
	}
	pBank->PopGroup(); 
}

void CAnimPlacementEditor::AudioWidgetChanged()
{
	if(ms_sceneId != INVALID_SYNCED_SCENE_ID)
	{
		if(sm_AudioWidget != sm_Audio)
		{
			if(sm_Audio.IsNotNull())
			{
				if(sm_IsAudioPrepared || sm_IsAudioPlaying)
				{
					fwAnimDirectorComponentSyncedScene::StopSynchronizedSceneAudioEvent(ms_sceneId);

					sm_IsAudioSkipped = false;
					sm_IsAudioPrepared = false;
					sm_IsAudioPlaying = false;
				}
			}

			sm_Audio = sm_AudioWidget;

			if(sm_Audio)
			{
				sm_IsAudioPrepared = fwAnimDirectorComponentSyncedScene::PrepareSynchronizedSceneAudioEvent(ms_sceneId, sm_Audio.GetCStr());
			}
		}
	}
}

void CAnimPlacementEditor::RemoveWidgets()
{

	sm_objectSelector.RemoveWidgets();
	sm_vehicleSelector.RemoveWidgets();
	ms_propSelector.RemoveWidgets();

	ms_attachBoneSelectors.clear();

	if (m_pBank)
	{
		ms_cameraAnimSelector.RemoveWidgets(m_pBank);
	}

	//delete all of the individual tool widgets first
	for (int i=0;i<m_animPlacementTools.GetCount(); i++)
	{
		m_animPlacementTools[i].RemoveWidgets();
	}

	//destroy all of our widgets
	while (m_toolGroups.GetCount()>0)
	{
		m_toolGroups.Top()->Destroy();
		m_toolGroups.Top() = NULL;
		m_toolGroups.Pop();
	}

	ms_SceneNameStrings.Reset();
	ms_SceneNames.Reset();
	ms_iSceneName = 0;
	ms_pSceneNameCombo = nullptr;
}

void CAnimPlacementEditor::ActivateSyncMode()
{
	//calculate a sensible start position if necessary
	if (sm_bSyncMode)
	{
		if (sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.IsClose(M34_IDENTITY) || sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.IsClose(M34_ZERO))
		{
			Matrix34 toolRoot(camInterface::GetMat());
			// remove any pitch or roll on the camera matrix
			// and move the tool forward a little (so the camera can see it)
			toolRoot.MakeUpright();
			toolRoot.d+=(camInterface::GetMat().b * 2.0f);

			sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.Set(toolRoot);

			//make sure all the tools are paused (so they don't get out of sync
			for (int i=0; i<m_animPlacementTools.GetCount(); i++)
			{
				if (!m_animPlacementTools[i].IsPaused())
				{
					m_animPlacementTools[i].TogglePause();
				}
			}
		}
	}

	m_rebuildWidgets = true;
}

void CAnimPlacementEditor::TogglePause()
{
	sm_masterPause = !sm_masterPause;
}

void CAnimPlacementEditor::AddAnimPlacementToolSelectedPed()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (pEntity)
	{
		if (pEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pEntity);
			if (pPed)
			{
				//check the ped isn't already in one of our existing tools
				for (int i=0; i<m_animPlacementTools.GetCount(); i++)
				{
					if (pPed==m_animPlacementTools[i].GetTestEntity())
					{
						bkMessageBox("Cant add Tool", "The selected ped is already being used by a placement tool. Please pick another", bkMsgOk, bkMsgError);
						return;
					}
				}

				//make sure it's not the player ped (repeated use of the player ped causes a crash - don't know why yet)
				CPed* pPlayerPed = FindPlayerPed();
				if (pPlayerPed)
				{
					if (pPlayerPed==pPed)
					{
						bkMessageBox("Cant add Tool", "The placement tool cannot use the player ped. Please pick another", bkMsgOk, bkMsgError);
						return;
					}
				}

				//should be safe to add the ped
				//add a single anim placement tool (test purposes)
				CAnimPlacementTool& tool = m_animPlacementTools.Grow();

				Matrix34 toolRoot = MAT34V_TO_MATRIX34(pPed->GetMatrix());

				tool.Initialise(pPed);
				//tool.AddWidgets(m_pBank, "Placement tool", true, NullCB);

				tool.SetRootMatrix(toolRoot);

				m_rebuildWidgets = true;
			}
		}
	}


}

void CAnimPlacementEditor::AddAnimPlacementTool(fwModelId ModelId, const Matrix34 &SceneMat )
{
	//add a single anim placement tool (test purposes)
	CAnimPlacementTool& tool = m_animPlacementTools.Grow();

	tool.Initialise(ModelId);

	tool.SetRootMatrix(SceneMat);
	
	m_rebuildWidgets = true;
}

void CAnimPlacementEditor::AddAnimPlacementToolCB()
{
	//make the new tool in front of the current camera position with its 
	Matrix34 toolRoot(camInterface::GetMat());
	// remove any pitch or roll on the camera matrix
	// and move the tool forward a little (so the camera can see it)
	toolRoot.MakeUpright();
	toolRoot.d+=(camInterface::GetMat().b * 2.0f);

	AddAnimPlacementTool(sm_modelSelector.GetSelectedModelId(),toolRoot); 
}

void CAnimPlacementEditor::AddAnimPlacementToolSelectedObject()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (pEntity)
	{
		if (pEntity->GetIsTypeObject())
		{				
			//check the ped isn't already in one of our existing tools
			for (int i=0; i<m_animPlacementTools.GetCount(); i++)
			{
				if (pEntity==m_animPlacementTools[i].GetTestEntity())
				{
					bkMessageBox("Cant add Tool", "The selected entity is already being used by a placement tool. Please pick another", bkMsgOk, bkMsgError);
					return;
				}
			}

			CObject* pObject = static_cast<CObject*>(pEntity);
			if (pObject)
			{

				//should be safe to add the object
				//add a single anim placement tool (test purposes)
				CAnimPlacementTool& tool = m_animPlacementTools.Grow();

				Matrix34 toolRoot = MAT34V_TO_MATRIX34(pObject->GetMatrix());

				tool.Initialise(pObject);
				//tool.AddWidgets(m_pBank, "Placement tool", true, NullCB);

				tool.SetRootMatrix(toolRoot);

				m_rebuildWidgets = true;
			}
		}
	}
}

void CAnimPlacementEditor::AddAnimPlacementToolObject(fwModelId ModelId, const Matrix34 &SceneMat)
{
	//add a single anim placement tool (test purposes)
	CAnimPlacementTool& tool = m_animPlacementTools.Grow();

	tool.InitialiseObject(ModelId);

	tool.SetRootMatrix(SceneMat);

	m_rebuildWidgets = true;
}

void CAnimPlacementEditor::AddAnimPlacementToolObjectCB()
{
	//make the new tool in front of the current camera position with its 
	Matrix34 toolRoot(camInterface::GetMat());
	// remove any pitch or roll on the camera matrix
	// and move the tool forward a little (so the camera can see it)
	toolRoot.MakeUpright();
	toolRoot.d+=(camInterface::GetMat().b * 2.0f);

	AddAnimPlacementToolObject(sm_objectSelector.GetSelectedModelId(), toolRoot);
}



void CAnimPlacementEditor::AddAnimPlacementToolSelectedVehicle()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (pEntity)
	{
		if (pEntity->GetIsTypeVehicle())
		{				
			//check the vehicle isn't already in one of our existing tools
			for (int i=0; i<m_animPlacementTools.GetCount(); i++)
			{
				if (pEntity==m_animPlacementTools[i].GetTestEntity())
				{
					bkMessageBox("Cant add Tool", "The selected entity is already being used by a placement tool. Please pick another", bkMsgOk, bkMsgError);
					return;
				}
			}

			CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
			if (pVeh)
			{

				//should be safe to add the object
				//add a single anim placement tool (test purposes)
				CAnimPlacementTool& tool = m_animPlacementTools.Grow();

				Matrix34 toolRoot = MAT34V_TO_MATRIX34(pVeh->GetMatrix());

				tool.Initialise(pVeh);
				//tool.AddWidgets(m_pBank, "Placement tool", true, NullCB);

				tool.SetRootMatrix(toolRoot);

				m_rebuildWidgets = true;
			}
		}
	}
}

void CAnimPlacementEditor::AddAnimPlacementToolVehicle(fwModelId ModelId, const Matrix34 &SceneMat)
{
	//add a single anim placement tool (test purposes)
	CAnimPlacementTool& tool = m_animPlacementTools.Grow();

	tool.InitialiseVehicle(ModelId);

	tool.SetRootMatrix(SceneMat);

	m_rebuildWidgets = true;
}

void CAnimPlacementEditor::AddAnimPlacementToolVehicleCB()
{
	//make the new tool in front of the current camera position with its 
	Matrix34 toolRoot(camInterface::GetMat());
	// remove any pitch or roll on the camera matrix
	// and move the tool forward a little (so the camera can see it)
	toolRoot.MakeUpright();
	toolRoot.d+=(camInterface::GetMat().b * 2.0f);

	AddAnimPlacementToolVehicle(sm_vehicleSelector.GetSelectedModelId(), toolRoot);
}

void CAnimPlacementEditor::DeleteTool(CAnimPlacementTool* tool)
{

	int toolIndex = m_animPlacementTools.Find(*tool);
	if (toolIndex>-1)
	{
		while(m_animPlacementTools.GetCount()>toolIndex)
		{
			m_animPlacementTools.Top().RemoveWidgets();
			m_animPlacementTools.Top().Shutdown();
			m_animPlacementTools.Pop();
		}

		//recreate the banks to fix th
		m_rebuildWidgets = true;
	}
}

void CAnimPlacementEditor::BuildMasterMatrix()
{
	sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.FromEulersXYZ(sm_Locations[sm_SelectedLocationIdx]->m_masterOrientation*DtoR);
}

void CAnimPlacementEditor::SelectCameraAnim()
{
	if (ms_cameraAnimSelector.HasSelection() && ms_sceneId>-1)
	{
		//create the new one
		camInterface::GetSyncedSceneDirector().AnimateCamera( ms_cameraAnimSelector.GetSelectedClipDictionary(),
			*ms_cameraAnimSelector.GetSelectedClip(),
			sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix, 
			ms_sceneId, 0, camSyncedSceneDirector::ANIM_PLACEMENT_TOOL_CLIENT); 

#if __BANK
		fwAnimDirectorComponentSyncedScene::UpdateSyncedSceneDuration(CAnimPlacementEditor::ms_sceneId);
		CAnimPlacementEditor::sm_masterDuration = fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(CAnimPlacementEditor::ms_sceneId);
#endif // __BANK
	}
}

void CAnimPlacementEditor::AttachScene()
{
	CEntity* pSelected = CDebugScene::FocusEntities_Get(0);

	if (pSelected->GetIsDynamic() && ms_sceneId>-1)
	{
		CDynamicEntity* pDyn = static_cast<CDynamicEntity*>(pSelected);

		//// recalc the master root position based on the new origin
		//Matrix34 globalRoot(M34_IDENTITY);

		//fwAnimDirector::GetSyncedSceneOrigin(ms_sceneId, globalRoot);

		//globalRoot.DotTranspose(pDyn->GetMatrix());

		//sm_masterRootMatrix = globalRoot;

		sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.Identity();

		fwAnimDirectorComponentSyncedScene::AttachSyncedScene(ms_sceneId, pDyn, ms_selectedAttachBone-1);
	}
}

void CAnimPlacementEditor::DetachScene()
{
	// Reset the master root position based on its global position
	fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin( ms_sceneId, sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix );

	fwAnimDirectorComponentSyncedScene::DetachSyncedScene( ms_sceneId );
}

void CAnimPlacementEditor::OutputScriptCalls()
{

	const u32 lineLength = 255;
	char line[lineLength];
	FileHandle file;

	file = CFileMgr::OpenFileForAppending(CScriptDebug::GetNameOfScriptDebugOutputFile());
	animAssert(file);

	sprintf( line, "\r\n/* START SYNCHRONIZED SCENE - %s */", ms_SceneName);
	Displayf( "%s", line );
	if (CFileMgr::IsValidFileHandle(file))
	{
		CFileMgr::Write(file, line, istrlen(line));
	}

	if (sm_bSyncMode)
	{
		//output the controlling variable first
		sprintf( line, "\r\nVECTOR scenePosition = << %.3f, %.3f, %.3f >>", sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.d.x, sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.d.y, sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.d.z);
		Displayf( "%s", line );
		if (CFileMgr::IsValidFileHandle(file))
		{
			CFileMgr::Write(file, line, istrlen(line));
		}

		Vector3 scriptRotation(VEC3_ZERO);

		sm_Locations[sm_SelectedLocationIdx]->m_masterRootMatrix.ToEulersXYZ(scriptRotation);
		scriptRotation *= RtoD;

		sprintf( line, "\r\nVECTOR sceneRotation = << %.3f, %.3f, %.3f >>", scriptRotation.x, scriptRotation.y, scriptRotation.z);
		Displayf( "%s", line );
		if (CFileMgr::IsValidFileHandle(file))
		{
			CFileMgr::Write(file, line, istrlen(line));
		}

		sprintf( line, "\r\nINT sceneId = CREATE_SYNCHRONIZED_SCENE(scenePosition, sceneRotation)");
		Displayf( "%s", line );
		if (CFileMgr::IsValidFileHandle(file))
		{
			CFileMgr::Write(file, line, istrlen(line));
		}

		if (ms_UseAnimatedCamera)
		{
			sprintf( line, "\r\nsceneCamera = CREATE_CAM(\"DEFAULT_ANIMATED_CAMERA\", FALSE)\r\nPLAY_SYNCHRONIZED_CAM_ANIM(sceneCamera, sceneId, \"%s\", \"%s\")\r\nACTIVATE_CAM(sceneCamera)\r\nRENDER_SCRIPT_CAMS(TRUE, FALSE)",
				ms_cameraAnimSelector.GetSelectedClipName(),
				ms_cameraAnimSelector.GetSelectedClipDictionary()
				);
			Displayf( "%s", line );
			if (CFileMgr::IsValidFileHandle(file))
			{
				CFileMgr::Write(file, line, istrlen(line));
			}
		}
	}
	for (int i=0; i<m_animPlacementTools.GetCount(); i++)
	{
		sprintf( line, "%s\r\n", m_animPlacementTools[i].GetScriptCall() );
		Displayf( "%s", line );
		if (CFileMgr::IsValidFileHandle(file))
		{
			CFileMgr::Write(file, line, istrlen(line));
		}
	}
	
	for (int i=0; i<m_animPlacementTools.GetCount(); i++)
	{
		CDebugAttachmentTool* pAttach = m_animPlacementTools[i].GetAnimDebugAttachTool(); 
		if(pAttach)
		{
			sprintf( line, "%s\r\n", pAttach->GetScriptCall());
			Displayf( "%s", line );
			if (CFileMgr::IsValidFileHandle(file))
			{
				CFileMgr::Write(file, line, istrlen(line));
			}
		}
	}

	sprintf( line, "\r\n/* CLEAN UP SYNCHRONIZED SCENE */");
	Displayf( "%s", line );
	if (CFileMgr::IsValidFileHandle(file))
	{
		CFileMgr::Write(file, line, istrlen(line));
	}

	//cleanup
	if (ms_UseAnimatedCamera)
	{
		sprintf( line, "\r\nRENDER_SCRIPT_CAMS(FALSE, FALSE)\r\nDESTROY_CAM(sceneCamera)");
		Displayf( "%s", line );
		if (CFileMgr::IsValidFileHandle(file))
		{
			CFileMgr::Write(file, line, istrlen(line));
		}
	}
	for (int i=0; i<m_animPlacementTools.GetCount(); i++)
	{
		sprintf( line, "%s\r\n", m_animPlacementTools[i].GetCleanupScriptCall() );
		Displayf( "%s", line );
		if (CFileMgr::IsValidFileHandle(file))
		{
			CFileMgr::Write(file, line, istrlen(line));
		}
	}


	if (CFileMgr::IsValidFileHandle(file))
	{
		CFileMgr::CloseFile(file);
	}
}

//////////////////////////////////////////////////////////////////////////
//	Location functionality
//////////////////////////////////////////////////////////////////////////

void CAnimPlacementEditor::UpdateLocationWidgets()
{
	// Delete all the previous widgets
	if (sm_pLocationsGroup)
	{
		while (sm_pLocationsGroup->GetChild())
		{
			sm_pLocationsGroup->GetChild()->Destroy();
		}
	}

	// Recreate the widgets
	animAssert(sm_pLocationsGroup);
	m_pBank->SetCurrentGroup(*sm_pLocationsGroup);
	{
		const int iNumLocations = sm_Locations.GetCount();

		// Clamp the selected index to the num locations just in case
		sm_SelectedLocationIdx = Clamp(sm_SelectedLocationIdx, 0, iNumLocations-1);

		// Go through the locations, generating the widgets for each one
		for (int i=0; i < iNumLocations; i++)
		{
			char GroupName[64];
			sprintf(GroupName, "Location_%d", i);
			m_pBank->PushGroup(GroupName);
			{
				CLocationInfo* pLocInfo = sm_Locations[i];
				animAssert(pLocInfo);

				// Location Name
				m_pBank->AddText("Location Name", sm_Locations[i]->m_name, 64, false);

				// Selected
				pLocInfo->m_bSelected = false; // Enforce false here so that the default value is false (means the selected location will be highlighted).
				if (iNumLocations > 1)
				{
					m_pBank->AddToggle("Selected Location", &pLocInfo->m_bSelected, datCallback(CFA1(SelectLocation), pLocInfo));
				}
				pLocInfo->m_bSelected = (i == sm_SelectedLocationIdx);

				// Add sliders for the three axes rotation values
				m_pBank->AddSlider("Master Heading", &sm_Locations[i]->m_masterOrientation.z, -180.0f, 180.0f, 1.0f,
					BuildMasterMatrix, "The master heading at the start of the anim");
				m_pBank->AddSlider("Master Pitch", &sm_Locations[i]->m_masterOrientation.x, -180.0f, 180.0f, 1.0f,
					BuildMasterMatrix, "The master pitch at the start of the anim");
				m_pBank->AddSlider("Master Roll", &sm_Locations[i]->m_masterOrientation.y, -180.0f, 180.0f, 1.0f,
					BuildMasterMatrix, "The master roll at the start of the anim");

				// Add a vector editor for the position
				m_pBank->AddVector("Master Position", &sm_Locations[i]->m_masterRootMatrix.d, -10000.0f,10000.0f, 0.05f);

				// Select this location button
				if (iNumLocations > 1)
				{
					// Remove this location button
					m_pBank->AddButton("Remove this location", datCallback(CFA1(RemoveLocation), pLocInfo));
				}

			}
			m_pBank->PopGroup();
		}
	}
	m_pBank->UnSetCurrentGroup(*sm_pLocationsGroup);
}

void CAnimPlacementEditor::AddLocation()
{
	USE_DEBUG_MEMORY();

	CLocationInfo* pNewLocation = rage_new CLocationInfo();
	if (strlen(m_newLocationName) > 0)
	{
		sprintf(pNewLocation->m_name, "%s", m_newLocationName);
	}
	sm_Locations.PushAndGrow(pNewLocation);

	UpdateLocationWidgets();
}

void CAnimPlacementEditor::RemoveLocation(CLocationInfo* pLocationInfo)
{
	USE_DEBUG_MEMORY();

	if (sm_Locations.GetCount() <= 1)
	{
		// Can't remove the last location, we need one or more!
		bkMessageBox("Anim Synced Scene Editor", "Can't remove the last location", bkMsgOk, bkMsgInfo);
		return;
	}

	// What index are we removing
	int iRemovalIndex = -1;
	for (int i=0; i<sm_Locations.GetCount(); i++)
	{
		if (sm_Locations[i] == pLocationInfo)
		{
			iRemovalIndex = i;
			break;
		}
	}

	animAssert(iRemovalIndex != -1);

	// Remove it
	CLocationInfo* pLoc = sm_Locations[iRemovalIndex];
	sm_Locations.Delete(iRemovalIndex);
	delete pLoc;

	// Update selected location
	if (iRemovalIndex <= sm_SelectedLocationIdx)
	{
		sm_SelectedLocationIdx--;
	}

	// Clamp the selected index to the num locations just in case
	sm_SelectedLocationIdx = Clamp(sm_SelectedLocationIdx, 0, sm_Locations.GetCount()-1);


	UpdateLocationWidgets();
}

void CAnimPlacementEditor::SelectLocation(CLocationInfo* pLocationInfo)
{
	const int iNumLocations = sm_Locations.GetCount();
	if (iNumLocations == 1)
	{
		pLocationInfo->m_bSelected = true;
		sm_SelectedLocationIdx = 0;
	}
	else
	{
		// Did the user just unselect this location, if so, swap to the previous one if possible
		if (!pLocationInfo->m_bSelected)
		{
			if (sm_SelectedLocationIdx == 0)
			{
				sm_SelectedLocationIdx++;
			}
			else
			{
				sm_SelectedLocationIdx--;
			}

			// Sanity check
			animAssert(sm_SelectedLocationIdx < iNumLocations);

			// Select it
			sm_Locations[sm_SelectedLocationIdx]->m_bSelected = true;
		}
		else
		{
			// Location was ticked, so untick the rest.
			for (int i=0; i<iNumLocations; i++)
			{
				CLocationInfo* pLoc = sm_Locations[i];
				if (pLoc == pLocationInfo)
				{
					pLoc->m_bSelected = true;
					sm_SelectedLocationIdx = i;
				}
				else
				{
					pLoc->m_bSelected = false;
				}
			}
		}
	}
	
	// Sanity check
	animAssert(sm_Locations[sm_SelectedLocationIdx]->m_bSelected == true);
}

//////////////////////////////////////////////////////////////////////////
//	Anim placement tool functionality
//////////////////////////////////////////////////////////////////////////

void CAnimPlacementTool::Shutdown()
{
	//cleanup the test ped
	CleanupEntity();

	m_timeLine.Shutdown();

	m_animSelector.Shutdown();

	Initialise();
}

void CAnimPlacementTool::SetPed(CPed* pPed, bool OwnsEntity)
{
	//Make sure we're not going to orphan an existing ped
	CleanupEntity();

	m_pDynamicEntity = static_cast<CDynamicEntity*>(pPed);
	m_bOwnsEntity = OwnsEntity;
}

void CAnimPlacementTool::SetVehicle(CVehicle* pVehicle, bool OwnsEntity)
{
	//Make sure we're not going to orphan an existing ped
	CleanupEntity();

	if (pVehicle->GetDrawable())
	{
		// lazily create a skeleton if one doesn't exist
		if (!pVehicle->GetSkeleton())
		{
			pVehicle->CreateSkeleton();
		}

		// Lazily create the animBlender if it doesn't exist
		if (!pVehicle->GetAnimDirector() && pVehicle->GetSkeleton())
		{
			pVehicle->CreateAnimDirector(*pVehicle->GetDrawable());
		}

		pVehicle->AddToSceneUpdate();

		// turn off collision so it doesn't mess with the anim
		pVehicle->DisableCollision();
	}		

	m_pDynamicEntity = static_cast<CDynamicEntity*>(pVehicle);
	m_bOwnsEntity = OwnsEntity;
}

void CAnimPlacementTool::SetObject(CObject* pObject, bool OwnsEntity)
{
	//Make sure we're not going to orphan an existing ped
	CleanupEntity();

	if (pObject->GetDrawable())
	{
		// lazily create a skeleton if one doesn't exist
		if (!pObject->GetSkeleton())
		{
			pObject->CreateSkeleton();
		}

		// Lazily create the animBlender if it doesn't exist
		if (!pObject->GetAnimDirector() && pObject->GetSkeleton())
		{
			pObject->CreateAnimDirector(*pObject->GetDrawable());
		}

		pObject->AddToSceneUpdate();

		// turn off collision so it doesn't mess with the anim
		pObject->DisableCollision();
	}		

	m_pDynamicEntity = static_cast<CDynamicEntity*>(pObject);
	m_bOwnsEntity = OwnsEntity;
}

void CAnimPlacementTool::SetPedModel(fwModelId pedModelId)
{
	// create the new ped
	CreatePed(pedModelId);
}

void CAnimPlacementTool::CreatePed(fwModelId pedInfoId)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	//Make sure we're not going to orphan an existing entity
	CleanupEntity();

	// create the test ped
	Matrix34 TempMat;

	//create at the test position
	TempMat.Set(m_rootMatrix);

	const CControlledByInfo localAiControl(false, false);

	// don't allow the creation of player ped type - it doesn't work!
	if (!strcmp(CAnimPlacementEditor::sm_modelSelector.GetSelectedModelName(), "player")) {return;}

	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedInfoId))
	{
		CModelInfo::RequestAssets(pedInfoId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (CModelInfo::HaveAssetsLoaded(pedInfoId))
	{
		CPed* pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedInfoId, &TempMat, true, true, true);

		pPed->PopTypeSet(POPTYPE_TOOL);
		pPed->SetDefaultDecisionMaker();
		pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDoNothing(-1), false);
		pPed->GetPedIntelligence()->AddEvent( event );

		pPed->SetCharParamsBasedOnManagerType();

		CGameWorld::Add(pPed, CGameWorld::OUTSIDE );

		pPed->GetPortalTracker()->RequestRescanNextUpdate();
		pPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));

		m_pDynamicEntity = static_cast<CDynamicEntity*>(pPed);
		m_bOwnsEntity = true;

		m_pDynamicEntity->DisableCollision();
	}
	else
	{
		animAssertf( 0, "Failed to create test ped. The selected ped could not be loaded." );
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimPlacementTool::CreateObject(fwModelId modelId)
{
	CObject * pObject = NULL;

	if(!CModelInfo::HaveAssetsLoaded(modelId))
	{
		CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects();
	}

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	if(pModelInfo)
	{
		pObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_SCRIPT, true, true);

		if (pObject)
		{

			pObject->SetMatrix(m_rootMatrix);

			// Add Object to world after its position has been set
			CGameWorld::Add(pObject, CGameWorld::OUTSIDE );
			pObject->GetPortalTracker()->RequestRescanNextUpdate();
			pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));

			pObject->SetupMissionState();
		}
	}

	if (pObject)
	{	
		if (pObject->GetDrawable())
		{
			// lazily create a skeleton if one doesn't exist
			if (!pObject->GetSkeleton())
			{
				pObject->CreateSkeleton();
			}

			// Lazily create the animBlender if it doesn't exist
			if (!pObject->GetAnimDirector() && pObject->GetSkeleton())
			{
				pObject->CreateAnimDirector(*pObject->GetDrawable());
			}

			// make sure the object is on the process control list
			pObject->AddToSceneUpdate();
		}

		m_pDynamicEntity = static_cast<CDynamicEntity*>(pObject);
		m_bOwnsEntity = true;
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimPlacementTool::CreateVehicle(fwModelId modelId)
{
	bool bForceLoad = false;
	if(!CModelInfo::HaveAssetsLoaded(modelId))
	{
		CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		bForceLoad = true;
	}

	if(bForceLoad)
	{
		CStreaming::LoadAllRequestedObjects(true);
	}

	if(!CModelInfo::HaveAssetsLoaded(modelId))
	{
		return;
	}

	Matrix34 tempMat;
	tempMat.Identity();
	CVehicle *pVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_DEBUG, POPTYPE_MISSION, &tempMat);
	if( pVehicle)
	{		
		m_pDynamicEntity = static_cast<CDynamicEntity*>(pVehicle);
		m_bOwnsEntity = true;

		CGameWorld::Add(pVehicle, CGameWorld::OUTSIDE);
		pVehicle->SetMatrix(m_rootMatrix, true, true, true);
		pVehicle->GetPortalTracker()->RequestRescanNextUpdate();
		pVehicle->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimPlacementTool::CleanupEntity()
{
	if (m_pDynamicEntity)
	{
		//if (m_pDynamicEntity->GetAnimDirector() && m_pDynamicEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>())
		//{
		//	// tell the anim director to end the synced scene
		//	m_pDynamicEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentSyncedScene>()->StopSyncedScenePlayback(INSTANT_BLEND_OUT_DELTA);
		//}

		if (m_pDynamicEntity->GetIsTypePed())
		{	
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());

			pPed->GetPedIntelligence()->ClearTasks(true, false);

			if (m_bOwnsEntity)
			{
				if (!pPed->IsLocalPlayer())	
				{
					//destroy the ped we created earlier
					pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DontStoreAsPersistent, true );
					CPedFactory::GetFactory()->DestroyPed(pPed);
				}
			}			
		}
		else if (m_pDynamicEntity->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(m_pDynamicEntity.Get());

			pVeh->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);

			CVehicleFactory::GetFactory()->Destroy(pVeh);
		}
		else if (m_pDynamicEntity->GetIsTypeObject())
		{
			CObject* pObject = (CObject*)m_pDynamicEntity.Get();

			if (pObject->GetObjectIntelligence())
			{
				pObject->GetObjectIntelligence()->GetTaskManager()->ClearTask(CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
			}

			if (m_bOwnsEntity)
			{
				CObjectPopulation::DestroyObject(pObject);
			}			
		}

		m_pDynamicEntity = NULL;
		m_timeLine.SetClip(NULL);
	}
}

const char * CAnimPlacementTool::GetScriptCall()
{
	m_scriptCall.Clear();

	if (m_pDynamicEntity->GetIsTypePed())
	{
		m_scriptCall += "\r\nTASK_SYNCHRONIZED_SCENE (ENTER_PED_HERE, sceneId, \"";
		m_scriptCall += m_animSelector.GetSelectedClipDictionary(); m_scriptCall+="\", \"";
		m_scriptCall += m_animSelector.GetSelectedClipName(); m_scriptCall+="\", ";

		if (CAnimPlacementEditor::sm_bSyncMode)
		{
			m_scriptCall += "INSTANT_BLEND_IN, INSTANT_BLEND_OUT )";
		}
		else
		{
			char vecText[50];
			sprintf(vecText, "<< %.3f, %.3f, %.3f >>, ", m_rootMatrix.d.x, m_rootMatrix.d.y, m_rootMatrix.d.z);
			m_scriptCall += vecText;

			Vector3 scriptRotation(VEC3_ZERO);

			m_rootMatrix.ToEulersXYZ(scriptRotation);
			scriptRotation*=RtoD;
			sprintf(vecText, "<< %.3f, %.3f, %.3f >>, ", scriptRotation.x, scriptRotation.y, scriptRotation.z);
			m_scriptCall += vecText;

			m_scriptCall += "INSTANT_BLEND_IN, INSTANT_BLEND_OUT, -1, AF_OVERRIDE_PHYSICS)";
		}
	}
	else if ( m_pDynamicEntity->GetIsTypeObject())
	{
		m_scriptCall+= "\r\nPLAY_SYNCHRONIZED_OBJECT_ANIM( ENTER_OBJECT_HERE, sceneId, \"";
		m_scriptCall+= m_animSelector.GetSelectedClipName();
		m_scriptCall+= "\",\"";
		m_scriptCall+= m_animSelector.GetSelectedClipDictionary();
		m_scriptCall+= "\" , INSTANT_BLEND_IN )";
	}
	return m_scriptCall.c_str();
}

const char * CAnimPlacementTool::GetCleanupScriptCall()
{
	m_scriptCall.Clear();

	if (m_pDynamicEntity->GetIsTypePed())
	{
		m_scriptCall += "\r\nCLEAR_PED_TASKS (ENTER_PED_HERE)";
	}
	else if ( m_pDynamicEntity
		->GetIsTypeObject())
	{
		m_scriptCall+= "\r\nSTOP_SYNCHRONIZED_OBJECT_ANIM( ENTER_OBJECT_HERE, INSTANT_BLEND_OUT)";
	}
	return m_scriptCall.c_str();
}


void CAnimPlacementTool::SetAnim(int animDictionaryIndex, int animIndex)
{
	m_animSelector.SelectClip(animDictionaryIndex, animIndex);

	StartAnim();
}

void CAnimPlacementTool::InitialiseObject(fwModelId modelId)
{
	Initialise();
	CreateObject(modelId);
}

void CAnimPlacementTool::InitialiseVehicle(fwModelId modelId)
{
	Initialise();
	CreateVehicle(modelId);
}

void CAnimPlacementTool::Initialise(fwModelId pedModelId)
{
	Initialise();
	//create the ped
	CreatePed(pedModelId);
}

void CAnimPlacementTool::Initialise(CPed* pPed, bool OwnEntity)
{
	Initialise();

	// register an existing ped with the tool
	SetPed(pPed, OwnEntity);
}

void CAnimPlacementTool::Initialise(CVehicle* pVeh, bool OwnEntity)
{
	Initialise();
	SetVehicle(pVeh, OwnEntity);
}


void CAnimPlacementTool::Initialise(CObject* pObject, bool OwnEntity)
{
	Initialise();
	SetObject(pObject, OwnEntity);
}

void CAnimPlacementTool::Initialise()
{
	m_pDynamicEntity = NULL;			// The ped created for testing
	m_rootMatrix.Identity();	// The root matrix for playing the anim
	m_timeLine.SetClip(NULL);	// RAG widget for controlling the phase in the anim
	m_orientation.Zero();		// The heading pitch and roll values displayed in rag - used to build the root matrix
	m_toolGroups.clear();		// A pointer to the tool's RAG widget group kept for cleanup purposes
	m_scriptCall.Clear();			// String storage for the script call that will reproduce this anim
	m_animChosenCallback = NullCB;	// custom callback that can be called when an anim is chosen in the tool
	m_bOwnsEntity = false;
	m_bPedWasInRagdoll = false;
	m_uExitSceneTime = 0;

	m_flags = CAnimPlacementEditor::sm_SyncedSceneFlags;
	m_ragdollFlags = CAnimPlacementEditor::sm_RagdollBlockingFlags;
	m_ikFlags = CAnimPlacementEditor::sm_IkControlFlags;
}

bool CAnimPlacementTool::Update(bool bInEditor)
{
	if (!m_pDynamicEntity )
	{
		return false;
	}
	
	if (!m_pDynamicEntity->GetAnimDirector() && !m_pDynamicEntity->GetDrawable())
	{
		// keep running till the object drawable is loaded in
		return true;
	}
	else if (!m_pDynamicEntity->GetAnimDirector())
	{
		if (m_pDynamicEntity->GetIsTypeObject())
		{
			((CObject*)m_pDynamicEntity.Get())->InitAnimLazy();
		}
		else if (m_pDynamicEntity->GetIsTypeVehicle())
		{
			((CVehicle*)m_pDynamicEntity.Get())->InitAnimLazy();
		}

		if (m_attachmentTool)
		{
			m_attachmentTool->RefreshBoneList();
		}

		CAnimPlacementEditor::m_rebuildWidgets = true;
	}

	if (bInEditor && CAnimPlacementEditor::sm_bSyncMode)
	{
		//override the root matrix for this tool
		m_rootMatrix.Set(CAnimPlacementEditor::sm_Locations[CAnimPlacementEditor::sm_SelectedLocationIdx]->m_masterRootMatrix);
		m_orientation.Set(CAnimPlacementEditor::sm_Locations[CAnimPlacementEditor::sm_SelectedLocationIdx]->m_masterOrientation);
		m_timeLine.SetPhase(CAnimPlacementEditor::sm_masterPhase);
	}
	else
	{
		//render the local root matrix
		if(CAnimPlacementTool::sm_bRenderDebugDraw)
		{
			grcDebugDraw::Axis(RCC_MAT34V(m_rootMatrix), 1.0f, true);
		}
	}

	fwAnimDirector* pDirector = m_pDynamicEntity->GetAnimDirector();

	if (pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>() && pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->IsPlayingSyncedScene())
	{
		
		// update the synced scene root position
		pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->SetSyncedSceneOrigin(CAnimPlacementEditor::ms_sceneId, m_rootMatrix);

		const crClip* pClip = pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->GetSyncedSceneClip();

		// disable this till we make a version that works without anim players
		//if (sm_bRenderMoverTracks)
		//{
		//	//render the mover track
		//	CAnimViewer::RenderMoverTrack(pClip, moverMatrix);
		//}

		if (sm_bRenderAnimValues)
		{
			Vector3 drawPosition = VEC3V_TO_VECTOR3(m_pDynamicEntity->GetTransform().GetPosition());
			int zOffset = 0;
			char debugText[50];
			Matrix34 debugMat;
			Vector3 debugVec;

			sprintf(debugText, "Mover offset:");
			grcDebugDraw::Text(drawPosition, Color_yellow, 0, zOffset, debugText); zOffset+=10;

			if(pClip)
			{
				fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.f, GetPhase(), debugMat);
			}
			sprintf(debugText, "Position - x:%.5f, y:%.5f, z:%.5f", debugMat.d.x, debugMat.d.y, debugMat.d.z);
			grcDebugDraw::Text(drawPosition, Color_yellow, 0, zOffset, debugText); zOffset+=10;
			debugMat.ToEulersXYZ(debugVec);
			debugVec*=RtoD;
			sprintf(debugText, "Rotation - x:%.5f, y:%.5f, z:%.5f", debugVec.x, debugVec.y, debugVec.z);
			grcDebugDraw::Text(drawPosition, Color_yellow, 0, zOffset, debugText); zOffset+=10;

			sprintf(debugText, "Clip offset:");
			grcDebugDraw::Text(drawPosition, Color_red, 0, zOffset, debugText); zOffset+=10;
			debugMat.Identity();
			if(pClip)
			{
				fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, debugMat);
			}
			sprintf(debugText, "Position - x:%.5f, y:%.5f, z:%.5f", debugMat.d.x, debugMat.d.y, debugMat.d.z);
			grcDebugDraw::Text(drawPosition, Color_red, 0, zOffset, debugText); zOffset+=10;
			debugMat.ToEulersXYZ(debugVec);
			debugVec*=RtoD;
			sprintf(debugText, "Rotation - x:%.5f, y:%.5f, z:%.5f", debugVec.x, debugVec.y, debugVec.z);
			grcDebugDraw::Text(drawPosition, Color_red, 0, zOffset, debugText); zOffset+=10;
		}

		//make sure the timeline control is synced to the correct clip
		if (pClip != m_timeLine.GetClip())
		{
			m_timeLine.SetClip(pClip);
		}

		if (pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->IsSyncedScenePaused(CAnimPlacementEditor::ms_sceneId))
		{
			// set the anim player's phase using the slider
			pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->SetSyncedScenePhase(CAnimPlacementEditor::ms_sceneId, m_timeLine.GetPhase());
		}
		else
		{
			// set the ui phase to the playing phase
			m_timeLine.SetPhase(pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->GetSyncedScenePhase(CAnimPlacementEditor::ms_sceneId));
		}
	}
	else
	{
		if (m_pDynamicEntity)
		{
			bool bUpdateMatrix = true;

			if (m_pDynamicEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());

				if (pPed->IsDead())
				{
					CreatePed(pPed->GetModelId());
					StartAnim();
					m_bPedWasInRagdoll = false;
					return true;
				}

				if (pPed->GetUsingRagdoll())
				{
					bUpdateMatrix = false;
					m_bPedWasInRagdoll = true;
				}
				else
				{
					if (m_bPedWasInRagdoll)
					{
						pPed->GetPedIntelligence()->FlushImmediately();
						StartAnim();
						m_bPedWasInRagdoll = false;
						return true;
					}
					pPed->SetDesiredHeading(DtoR*m_orientation.x);
				}

				pPed->SetBlockingOfNonTemporaryEvents(true);
			}
			else if (m_pDynamicEntity->GetIsTypeVehicle())
			{
				CTaskSynchronizedScene* pTask = ((CVehicle*)m_pDynamicEntity.Get())->GetIntelligence() ? static_cast< CTaskSynchronizedScene* >(((CVehicle*)m_pDynamicEntity.Get())->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SYNCHRONIZED_SCENE)) : NULL;
				bUpdateMatrix = (pTask!=NULL);

				if (!pTask)
				{
					if (m_uExitSceneTime==0)
					{
						m_uExitSceneTime = fwTimer::GetTimeInMilliseconds();
					}
					else if (fwTimer::GetTimeInMilliseconds() > (m_uExitSceneTime+5000))
					{
						StartAnim();
						m_uExitSceneTime = 0;
						return true;
					}
				}
				else
				{
					m_uExitSceneTime = 0;
				}

			}

			if (bUpdateMatrix)
			{
				//move the test ped to the root position
				m_pDynamicEntity->DisableCollision();
				m_pDynamicEntity->SetMatrix(m_rootMatrix, true, true, true);
			}
		}
	}

	return true;

}

void CAnimPlacementTool::TogglePause()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetAnimDirector())
	{
		fwAnimDirectorComponentSyncedScene::SetSyncedScenePaused(CAnimPlacementEditor::ms_sceneId, !fwAnimDirectorComponentSyncedScene::IsSyncedScenePaused(CAnimPlacementEditor::ms_sceneId));
	}
}

bool CAnimPlacementTool::IsPaused()
{
	if (m_pDynamicEntity && m_pDynamicEntity->GetAnimDirector())
	{
		return fwAnimDirectorComponentSyncedScene::IsSyncedScenePaused(CAnimPlacementEditor::ms_sceneId);
	}

	return true;
}


void CAnimPlacementTool::AddWidgets(bkBank* pBank, const char * groupName, bool showDeleteButton, datCallback animChangeCallback, bool showStateControls)
{
	m_toolGroups.PushAndGrow(pBank->PushGroup(groupName));
	{
		m_animSelector.AddWidgets(pBank, 
			datCallback(MFA(CAnimPlacementTool::SetDictionary), (datBase*)this),
			datCallback(MFA(CAnimPlacementTool::StartAnim), (datBase*)this)
			);

		if(GetParentEntity())
		{
			GetAnimDebugAttachTool()->AddWidgets(pBank, "Attach Tool");
		}

		if (showStateControls)
		{
			//add sliders for the three axes rotation values
			pBank->AddSlider("Heading", &m_orientation.z, -180.0f, 180.0f,  1.0f,
				datCallback(MFA(CAnimPlacementTool::BuildMatrix), (datBase*)this),
				"The ped's heading at the start of the anim");
			pBank->AddSlider("Pitch", &m_orientation.x, -180.0f, 180.0f,  1.0f,
				datCallback(MFA(CAnimPlacementTool::BuildMatrix), (datBase*)this),
				"The ped's pitch at the start of the anim");
			pBank->AddSlider("Roll", &m_orientation.y, -180.0f, 180.0f,  1.0f,
				datCallback(MFA(CAnimPlacementTool::BuildMatrix), (datBase*)this),
				"The ped's roll at the start of the anim");

			//add a vector editor for the position
			pBank->AddVector("Position", &m_rootMatrix.d, -10000.0f,10000.0f, 0.05f);

			//add the timeline control
			m_timeLine.AddWidgets(pBank);

			pBank->AddButton("Play / Pause", datCallback(MFA(CAnimPlacementTool::TogglePause), (datBase*)this));

			
		}

		pBank->AddToggle("Loop within scene", &m_bLoopWithinScene, datCallback(MFA(CAnimPlacementTool::StartAnim), (datBase*)this));

		if (showDeleteButton)
		{
			pBank->AddButton("Remove this tool", datCallback(CFA1(CAnimPlacementEditor::DeleteTool), (CallbackData)this));
		}
	}
	pBank->PopGroup();

	m_animChosenCallback = animChangeCallback;
}

void CAnimPlacementTool::RemoveWidgets()
{
	//remove any widgets for the sub controls
	m_timeLine.RemoveWidgets();

	//remove the rest of the tool group widgets
	while (m_toolGroups.GetCount()>0)
	{
		m_animSelector.RemoveWidgets(static_cast< bkBank * >(m_toolGroups.Top()->GetParent()->GetParent()));

		m_toolGroups.Top()->Destroy();
		m_toolGroups.Top() = NULL;
		m_toolGroups.Pop();
	}
}

void CAnimPlacementTool::BuildMatrix()
{
	Vector3 eulers;

	eulers.x = DtoR*m_orientation.x;
	eulers.y = DtoR*m_orientation.y;
	eulers.z = DtoR*m_orientation.z;

	m_rootMatrix.FromEulersXYZ(eulers);
}

void CAnimPlacementTool::SetRootMatrix(const Matrix34& mtx)
{
	m_rootMatrix.Set(mtx);
	RecalcOrientation();
}

void CAnimPlacementTool::SetRootMatrixPosition(const Vector3& pos)
{
	m_rootMatrix.d.Set(pos);
}

void CAnimPlacementTool::SetDictionary()
{
	StartAnim();
}

void CAnimPlacementTool::StartAnim()
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	if (m_pDynamicEntity && m_pDynamicEntity->GetAnimDirector() && m_animSelector.HasSelection())
	{
		if (m_pDynamicEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get());
			//clear any running ped tasks
			pPed->GetPedIntelligence()->ClearTasks(true, true);
			pPed->SetBlockingOfNonTemporaryEvents(false);
		}

		// collision is disabled whilst setting up the tool
		m_pDynamicEntity->EnableCollision();

		// Is the animation index valid
		if (m_animSelector.StreamSelectedClipDictionary())
		{
			CTaskSynchronizedScene* pTask = rage_new CTaskSynchronizedScene(CAnimPlacementEditor::ms_sceneId, atPartialStringHash(m_animSelector.GetSelectedClipName()), m_animSelector.GetSelectedClipDictionary(), CAnimPlacementEditor::sm_BlendInDelta, CAnimPlacementEditor::sm_BlendOutDelta, m_flags, m_ragdollFlags, CAnimPlacementEditor::sm_MoverBlendInDelta, -1, m_ikFlags);

			int clipDictionaryIndex; CDebugClipDictionary::FindClipDictionary(atStringHash(m_animSelector.GetSelectedClipDictionary()), clipDictionaryIndex);

			if (m_pDynamicEntity->GetType()==ENTITY_TYPE_PED)
			{
				CPed* pPed = static_cast<CPed*>(m_pDynamicEntity.Get()); 
				if (pPed && pTask)
				{
					pPed->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY , true);	
				}
			}
			else if (m_pDynamicEntity->GetType()==ENTITY_TYPE_OBJECT)
			{
				CObject* pObj = static_cast<CObject*>(m_pDynamicEntity.Get()); 
				if (pObj && pTask)
				{
					pObj->SetTask(pTask, CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY, CObjectIntelligence::OBJECT_TASK_PRIORITY_PRIMARY);
				}
			}
			else if (m_pDynamicEntity->GetType()==ENTITY_TYPE_VEHICLE)
			{
				CVehicle* pVeh = static_cast<CVehicle*>(m_pDynamicEntity.Get()); 
				if (pVeh && pTask)
				{
					pVeh->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(pTask, VEHICLE_TASK_SECONDARY_ANIM); 
				}
			}

#if __BANK
			fwAnimDirectorComponentSyncedScene::UpdateSyncedSceneDuration(CAnimPlacementEditor::ms_sceneId);
			CAnimPlacementEditor::sm_masterDuration = fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(CAnimPlacementEditor::ms_sceneId);
#endif // __BANK
		}
	}

	if (m_animChosenCallback.GetType()!=datCallback::NULL_CALLBACK)
	{
		m_animChosenCallback.Call();
	}

}

void CAnimPlacementTool::RecalcOrientation()
{
	Vector3 eulers;
	m_rootMatrix.ToEulersXYZ(eulers);

	m_orientation.x = RtoD*eulers.x;
	m_orientation.y = RtoD*eulers.y;
	m_orientation.z = RtoD*eulers.z;
}

#endif //__BANK
