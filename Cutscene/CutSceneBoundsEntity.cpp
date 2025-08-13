/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneBoundsEntity.cpp
// PURPOSE : 
// AUTHOR  : Thomas French
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers


//Game Headers
#include "ai/BlockingBounds.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayHideManager.h"
#include "cutscene/CutSceneBoundsEntity.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Cutscene/cutscene_channel.h"
#include "debug/DebugScene.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/world/worldmgr.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Objects/object.h"
#include "Objects/Door.h"
#include "Objects/DummyObject.h"
#include "pathserver/PathServer.h"
#include "peds/pedpopulation.h"
#include "scene/world/GameWorld.h"

ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////

///////////////////////////////
// Fixup bounds
///////////////////////////////

CCutSceneFixupBoundsEntity::CCutSceneFixupBoundsEntity(const cutfObject* pObject)
: cutsUniqueEntity(pObject)
{
	m_iModelIndex = fwModelId::MI_INVALID;
	m_pFixupObject = static_cast<const cutfFixupModelObject*>(pObject);
}

//////////////////////////////////////////////////////////////////////

CCutSceneFixupBoundsEntity::~CCutSceneFixupBoundsEntity()
{
	m_pFixupObject = NULL;
}

//////////////////////////////////////////////////////////////////////

void CCutSceneFixupBoundsEntity::DispatchEvent( cutsManager* pManager, const cutfObject* UNUSED_PARAM(pObject), s32 iEventId, const cutfEventArgs* UNUSED_PARAM(pEventArgs), const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId) )
{
	switch ( iEventId )
	{
		//case CUTSCENE_START_OF_SCENE_EVENT:
		//{
		//	cutsceneBoundsEntityDebugf("CUTSCENE_START_OF_SCENE_EVENT"); 
		//	if (m_pFixupObject)
		//	{
		//		CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager); 
		//		pCutsManager->FixupRequestedObjects(m_pFixupObject->GetPosition(), m_pFixupObject->GetRadius(), m_iModelIndex);
		//	}
		//}
		//break;
		
		case CUTSCENE_FIXUP_OBJECTS_EVENT:
		{
			cutsceneBoundsEntityDebugf("CUTSCENE_FIXUP_OBJECTS_EVENT"); 
			if (m_pFixupObject)
			{
				CutSceneManager* pCutsManager = static_cast<CutSceneManager*>(pManager); 
				
				if(m_iModelIndex == fwModelId::MI_INVALID)
				{
					m_iModelIndex = CModelInfo::GetModelIdFromName( m_pFixupObject->GetName()).GetModelIndex();
				}
				
				if(m_iModelIndex != fwModelId::MI_INVALID)
				{
					pCutsManager->FixupRequestedObjects(m_pFixupObject->GetPosition(), m_pFixupObject->GetRadius(), m_iModelIndex);
				}
			}
		}
		break; 
	}
}

//////////////////////////////////////////////////////////////////////


#if __BANK		
void CCutSceneFixupBoundsEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Fixup Bound Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}
#endif

///////////////////////////////
// Hidden bounds
///////////////////////////////

//////////////////////////////////////////////////////////////////////

void CCutSceneHiddenBoundsEntity::DispatchEvent( cutsManager* UNUSED_PARAM(pManager), const cutfObject* UNUSED_PARAM(pObject), s32 iEventId, const cutfEventArgs* UNUSED_PARAM(pEventArgs),const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId) )
{
	switch ( iEventId )
	{
		case CUTSCENE_HIDE_OBJECTS_EVENT:	
			{
				cutsceneBoundsEntityDebugf("CUTSCENE_HIDE_OBJECTS_EVENT"); 
				if (m_pHiddenObject)
				{
					Vector3 pos =  m_pHiddenObject->GetPosition(); 
					
					ComputeModelIndex(); 

					if(m_iModelIndex != fwModelId::MI_INVALID)
					{
						m_bHaveHidden = SetObjectInAreaVisibility(pos, m_pHiddenObject->GetRadius(), m_iModelIndex, false);
					}

					if(m_bHaveHidden)
					{
						cutsceneBoundsEntityDebugf("CUTSCENE_HIDE_OBJECTS_EVENT - Succeeded to hide object"); 
					}
					else
					{
						cutsceneBoundsEntityDebugf("CUTSCENE_HIDE_OBJECTS_EVENT - Failed to hide object");
					}


					m_bShouldHide = true; 
					m_bShouldShow = false; 
				}
				
			}
		break;

		case CUTSCENE_UPDATE_EVENT:
			{
				if(!m_bShouldShow && m_bShouldHide && !m_bHaveHidden)
				{
					if(m_iModelIndex == fwModelId::MI_INVALID)
					{
						ComputeModelIndex(); 
					}

					if(m_iModelIndex != fwModelId::MI_INVALID && SetObjectInAreaVisibility(m_pHiddenObject->GetPosition(), m_pHiddenObject->GetRadius(), m_iModelIndex, false))
					{
						if (SetObjectInAreaVisibility(m_pHiddenObject->GetPosition(), m_pHiddenObject->GetRadius(), m_iModelIndex, false))
						{
							cutsceneBoundsEntityDebugf("CUTSCENE_UPDATE_EVENT - Succeeded to hide object"); 
							m_bHaveHidden = true; 
						}
						else
						{
							cutsceneBoundsEntityDebugf("CUTSCENE_UPDATE_EVENT - Failed to hide object"); 
						}
					}		
				}
				
				if(m_bShouldShow && !m_bShouldHide && !m_bHaveShown)
				{
					if(m_iModelIndex == fwModelId::MI_INVALID)
					{
						ComputeModelIndex(); 
					}
					
					if(m_iModelIndex != fwModelId::MI_INVALID)
					{
						if (SetObjectInAreaVisibility(m_pHiddenObject->GetPosition(), m_pHiddenObject->GetRadius(), m_iModelIndex, true))
						{
							cutsceneBoundsEntityDebugf("CUTSCENE_UPDATE_EVENT - Succeeded to show object");
							m_bHaveShown = true; 
						}
						else
						{
							cutsceneBoundsEntityDebugf("CUTSCENE_UPDATE_EVENT - Failed to show object"); 
						}
					}
				}

			}
		break;

		case CUTSCENE_SHOW_OBJECTS_EVENT:
			{
				cutsceneBoundsEntityDebugf("CUTSCENE_SHOW_OBJECTS_EVENT"); 
				if (m_pHiddenObject)
				{
					Vector3 pos =  m_pHiddenObject->GetPosition();
					
					if(m_iModelIndex == fwModelId::MI_INVALID)
					{
						ComputeModelIndex(); 
					}
					
					if(m_iModelIndex != fwModelId::MI_INVALID)
					{	
						m_bHaveShown = SetObjectInAreaVisibility(pos, m_pHiddenObject->GetRadius(), m_iModelIndex, true);
					}

					if(m_bHaveShown)
					{
						cutsceneBoundsEntityDebugf("CUTSCENE_SHOW_OBJECTS_EVENT - Succeeded to show object"); 
					}
					else
					{
						cutsceneBoundsEntityDebugf("CUTSCENE_SHOW_OBJECTS_EVENT - Failed to show object");
					}

					m_bShouldShow = true; 
					m_bShouldHide = false; 
				}
			}
		break;

		case CUTSCENE_STOP_EVENT:
			{
				cutsceneBoundsEntityDebugf("CUTSCENE_STOP_EVENT"); 
				if (m_pHiddenObject)
				{
					Vector3 pos =  m_pHiddenObject->GetPosition();

					if(m_iModelIndex == fwModelId::MI_INVALID)
					{
						ComputeModelIndex(); 
					}

					bool bSuccess = false;

					if(m_iModelIndex != fwModelId::MI_INVALID)
					{
						bSuccess = SetObjectInAreaVisibility(pos, m_pHiddenObject->GetRadius(), m_iModelIndex, true);
					}

					if(bSuccess)
					{
						cutsceneBoundsEntityDebugf("CUTSCENE_STOP_EVENT - Succeeded to show object"); 
					}
					else
					{
						cutsceneBoundsEntityDebugf("CUTSCENE_STOP_EVENT - Failed to show object");
					}

				}
			}
		break;
	}
}	

void CCutSceneHiddenBoundsEntity::ComputeModelIndex()
{
	if(m_pHiddenObject)
	{
		if(m_iModelIndex == fwModelId::MI_INVALID)
		{
			atHashString modelHash = m_pHiddenObject->GetName(); 
			
			cutsceneAssertf(modelHash,"bounds object has no name chekc the cut file");

			m_iModelIndex = CModelInfo::GetModelIdFromName(modelHash).GetModelIndex();
		}
	}
}

//////////////////////////////////////////////////////////////////////
//Store our model index so  

CCutSceneHiddenBoundsEntity::CCutSceneHiddenBoundsEntity(const cutfObject* pObject)
: cutsUniqueEntity(pObject)
{ 
	m_iModelIndex = fwModelId::MI_INVALID;
	m_pHiddenObject = static_cast<const cutfHiddenModelObject*>(pObject);
	m_pEntity = NULL; 
	m_bSkippedCalled = false; 
	m_bHaveHidden = false; 
	m_bShouldHide = false; 
	m_bShouldShow = false; 
	m_bHaveShown = false; 
}

//////////////////////////////////////////////////////////////////////

CCutSceneHiddenBoundsEntity::~CCutSceneHiddenBoundsEntity()
{
	m_pHiddenObject = NULL;
}

//////////////////////////////////////////////////////////////////////
//Sets the objects visibility

bool CCutSceneHiddenBoundsEntity::SetObjectInAreaVisibility(const Vector3 &vPos, float fRadius, s32 iModelIndex, bool bVisble)
{
	if(m_pEntity == NULL)
	{
		m_pEntity = CutSceneManager::GetEntityToAtPosition(vPos, fRadius,iModelIndex); 
	}
		
#if __ASSERT
	
	//Firing asserts during skips is invalid because the objects may not be loaded in by this point.  
	if(!CutSceneManager::GetInstance()->IsSkippingPlayback() && !m_bSkippedCalled)
	{
		const char* ObjectName = ""; 
		
		if(m_pHiddenObject)
		{
			ObjectName = m_pHiddenObject->GetDisplayName().c_str(); 
		}
	
		if(!m_pEntity)
		{
			if(!bVisble)
			{
				cutsceneBoundsEntityDebugf("Cant find object %s at (%f, %f, %f)(%d) to HIDE", ObjectName, vPos.x, vPos.y, vPos.z, fwTimer::GetFrameCount()); 
			}
			else
			{
				cutsceneBoundsEntityDebugf("Cant find object %s at (%f, %f, %f)(%d) to SHOW", ObjectName, vPos.x, vPos.y, vPos.z, fwTimer::GetFrameCount());
			}
		}
	}
	else
	{
		m_bSkippedCalled = true; 
	}

#endif

	if(m_pEntity)
	{
		if ( m_pEntity->GetIsTypeObject() && static_cast<CObject*>(m_pEntity.Get())->IsADoor() )
		{
			CDoor*	door = static_cast<CDoor*>(m_pEntity.Get());
			door->SetDoorControlFlag( CDoor::DOOR_CONTROL_FLAGS_CUTSCENE_DONT_SHUT, !bVisble );
			door->UpdatePortalState( true );
		}
		else if ( m_pEntity->GetIsTypeDummyObject() )
		{
			CDummyObject*	dummy = static_cast<CDummyObject*>(m_pEntity.Get());
			dummy->ForceIsVisibleExtension();
		}

#if GTA_REPLAY
		// Make sure the replay records this object so we can play back the visibility information...
		// Without this the map objects don't get hidden resulting in two objects during recorded cutscenes.
		if(CReplayMgr::IsEditModeActive() == false && m_pEntity->GetIsTypeObject())
		{
			CReplayMgr::RecordMapObject(static_cast<CObject*>(m_pEntity.Get()));
		}

		if( CReplayMgr::IsRecording() && ReplayHideManager::IsNonReplayTrackedObjectType(m_pEntity) )
		{
			ReplayHideManager::AddNewHide(m_pEntity, bVisble);
		}

#endif // GTA_REPLAY

		m_pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, bVisble); 
		/*if(bVisble)
		{
			cutsceneBoundsEntityDebugf("CCutSceneHiddenBoundsEntity::SetObjectInAreaVisibility: Showing entity (%s)", m_pEntity->GetModelName());
		}
		else
		{	
			cutsceneBoundsEntityDebugf("CCutSceneHiddenBoundsEntity::SetObjectInAreaVisibility: Hiding entity (%s)", m_pEntity->GetModelName());
		}*/
		
		return true; 
	}
	return false; 
}

//////////////////////////////////////////////////////////////////////
//Debug draw for hidden bounds entities
#if __BANK		
void CCutSceneHiddenBoundsEntity::DebugDraw() const
{
	if(m_pEntity)
	{
		char Message[50]; 
		sprintf(Message, "Model %s set to be hidden",m_pHiddenObject->GetDisplayName().c_str());
		CDebugScene::DrawEntityBoundingBox(m_pEntity, Color32(0, 0, 255, 255));
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_pEntity->GetTransform().GetPosition()), CRGBA(0,0,255,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), Message );
	}
	else
	{
		Vector3 vSearchPos = m_pHiddenObject->GetPosition() + CutSceneManager::GetInstance()->GetSceneOffset(); 
		char Message[50]; 
		sprintf(Message, "Object with model: %s was not found at %f,%f,%f with radius",m_pHiddenObject->GetDisplayName().c_str(), vSearchPos.x, vSearchPos.y, vSearchPos.z );
	}
}
void CCutSceneHiddenBoundsEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Hidden Bound Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}
#endif
//////////////////////////////////////////////////////////////////////

///////////////////////////////
//Blocking Bounds
///////////////////////////////

//////////////////////////////////////////////////////////////////////

CCutSceneBlockingBoundsEntity::CCutSceneBlockingBoundsEntity(const cutfObject *pObject)
: cutsUniqueEntity(pObject)
{
	m_pBlockingBounds = static_cast<const cutfBlockingBoundsObject*>(pObject); 
	
	Vector3 vPointsInSceneSpace[MAX_NUM_CORNERS]; 
	
	//sort corners into order
	for (int i =0; i < MAX_NUM_CORNERS; i++)
	{
		vPointsInSceneSpace[i] = m_pBlockingBounds->GetCorner(i); 
	}

	float creationTime = 0.0f;

	atArray<cutfEvent*>ObjectEvents;
	const cutfCutsceneFile2* pCutFile = static_cast<const CutSceneManager*>(CutSceneManager::GetInstance())->GetCutfFile();
	pCutFile->FindEventsForObjectIdOnly( pObject->GetObjectId(), pCutFile->GetEventList(), ObjectEvents );

	for (s32 i=0; i<ObjectEvents.GetCount(); i++)
	{
		if (ObjectEvents[i]->GetEventId()==CUTSCENE_ADD_BLOCKING_BOUNDS_EVENT)
		{
			creationTime = ObjectEvents[i]->GetTime();
		}
	}
	
	//create a version of an angled area that describes it as a matrix and 2 vectors
	CutSceneManager::GetInstance()->ConvertAngledAreaToMatrixAndVectors(vPointsInSceneSpace, m_pBlockingBounds->GetHeight(), m_vAreaMin, m_vAreaMax, m_mArea, creationTime );
	
	m_ScenarioBlockingBoundId = CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid; 
}

//////////////////////////////////////////////////////////////////////

CCutSceneBlockingBoundsEntity::~CCutSceneBlockingBoundsEntity()
{
	m_pBlockingBounds = NULL;

}

//////////////////////////////////////////////////////////////////////

void CCutSceneBlockingBoundsEntity::DispatchEvent( cutsManager* pManager, const cutfObject* BANK_ONLY(pObject), s32 iEventId, const cutfEventArgs* BANK_ONLY(pEventArgs), const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId) )
{
	switch ( iEventId )
	{
	case CUTSCENE_START_OF_SCENE_EVENT:
		{
			cutsceneBoundsEntityDebugf("CUTSCENE_START_OF_SCENE_EVENT"); 
		}
		break; 

	case CUTSCENE_ADD_BLOCKING_BOUNDS_EVENT:	
		{
			cutsceneBoundsEntityDebugf("CUTSCENE_ADD_BLOCKING_BOUNDS_EVENT"); 
			AddPedGenBlockingArea(pManager); 
			AddPedWanderBlockingArea(); 
			AddPedScenarioBlockingArea();
			RemovePedsAndCars();
	
		}
		break;

	case CUTSCENE_REMOVE_BLOCKING_BOUNDS_EVENT:	
		{
			cutsceneBoundsEntityDebugf("CUTSCENE_REMOVE_BLOCKING_BOUNDS_EVENT"); 
			RemovePedWanderBlockingArea(); 
			CPathServer::GetAmbientPedGen().ClearAllPedGenBlockingAreas(true, false); 
			if(m_ScenarioBlockingBoundId != CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid)
			{
				CScenarioBlockingAreas::RemoveScenarioBlockingArea(m_ScenarioBlockingBoundId); 
				m_ScenarioBlockingBoundId = CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid; 
			}
		}
		break;

	case CUTSCENE_END_OF_SCENE_EVENT:
		{
			cutsceneBoundsEntityDebugf("CUTSCENE_END_OF_SCENE_EVENT");
			RemovePedWanderBlockingArea(); 
			CPathServer::GetAmbientPedGen().ClearAllPedGenBlockingAreas(true, false); 
			CPathServer::GetAmbientPedGen().ClearAllPedGenBlockingAreas(true, false); 
			if(m_ScenarioBlockingBoundId != CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid)
			{
				CScenarioBlockingAreas::RemoveScenarioBlockingArea(m_ScenarioBlockingBoundId); 
				m_ScenarioBlockingBoundId = CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid; 
			}
		}
		break; 
	}

#if __BANK
	cutsEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
#endif 
}

//////////////////////////////////////////////////////////////////////
//Checks if a point is inside a

bool CCutSceneBlockingBoundsEntity::IsInside(const Vector3 &vVec)
{
	return (geomBoxes::TestSphereToBox(vVec, 0.01f, m_vAreaMin, m_vAreaMax, m_mArea));
}

//////////////////////////////////////////////////////////////////////
//Runs all the processes for a blocking bound.

void CCutSceneBlockingBoundsEntity::AddPedGenBlockingArea(cutsManager* pManager)
{
	Matrix34 SceneMat(M34_IDENTITY);

	pManager->GetSceneOrientationMatrix(SceneMat); 
	
	for (int i=0; i < MAX_NUM_CORNERS; i++)
	{
		SceneMat.Transform(m_pBlockingBounds->GetCorner(i), m_vWorldCornerPoints[i]); //store the points in world position. 
	}

	//The points have been wound in a clockwise order, can pass the points in directly
	CPathServer::GetAmbientPedGen().AddPedGenBlockedArea(&m_vWorldCornerPoints[0], m_vWorldCornerPoints[0].z + m_pBlockingBounds->GetHeight(),  m_vWorldCornerPoints[0].z, 0, CPedGenBlockedArea::ECutscene);
}

void CCutSceneBlockingBoundsEntity::AddPedScenarioBlockingArea()
{
	if(m_ScenarioBlockingBoundId == CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid)
	{
		Vector3 vMin, vMax; 
		vMin = m_vWorldCornerPoints[0]; 
		vMax = m_vWorldCornerPoints[0]; 

		//approximate the angled box into an axis aligned box
		for(int i =1; i< MAX_NUM_CORNERS; i++)
		{
			vMin.x  = Min(m_vWorldCornerPoints[i].x, vMin.x); 
			vMin.y  = Min(m_vWorldCornerPoints[i].y, vMin.y);

			vMax.x  = Max(m_vWorldCornerPoints[i].x, vMax.x); 
			vMax.y  = Max(m_vWorldCornerPoints[i].y, vMax.y);

		}
			vMax.z = vMax.z + m_pBlockingBounds->GetHeight(); 
		

		const char* debugUserName = NULL;
#if __BANK
		if(m_pBlockingBounds)
		{
			debugUserName = m_pBlockingBounds->GetName().GetCStr();
		}
#endif	// __BANK

		//The points have been wound in a clockwise order, can pass the points in directly
		m_ScenarioBlockingBoundId = CScenarioBlockingAreas::AddScenarioBlockingArea(vMin, vMax, true, true, true, CScenarioBlockingAreas::kUserTypeCutscene, debugUserName);
	}
}

//////////////////////////////////////////////////////////////////////
//approximate the angled box into an axis aligned box
void CCutSceneBlockingBoundsEntity::AddPedWanderBlockingArea()
{
	// Prevent peds from wandering into this area (just takes a min/max)
	// We're cheating a bit by using a dummy script id.
	
	Vector3 vMin, vMax; 
	vMin = m_vWorldCornerPoints[0]; 
	vMax = m_vWorldCornerPoints[0]; 

	//approximate the angled box into an axis aligned box
	for(int i =1; i< MAX_NUM_CORNERS; i++)
	{
		vMin.x  = Min(m_vWorldCornerPoints[i].x, vMin.x); 
		vMin.y  = Min(m_vWorldCornerPoints[i].y, vMin.y);

		vMax.x  = Max(m_vWorldCornerPoints[i].x, vMax.x); 
		vMax.y  = Max(m_vWorldCornerPoints[i].y, vMax.y);

	}
	vMax.z = vMax.z + m_pBlockingBounds->GetHeight(); 

	scrThreadId iDummyScriptId = static_cast<scrThreadId>(0xFFFFFFFF);

	CPathServer::SwitchPedsOffInArea(vMin, vMax, iDummyScriptId);
	CPedPopulation::PedNonCreationAreaSet(vMin, vMax);
}

//////////////////////////////////////////////////////////////////////
//Remove all the processes for a blocking bound.

void CCutSceneBlockingBoundsEntity::RemovePedWanderBlockingArea()
{
	Vector3 vMin, vMax; 
	vMin = m_vWorldCornerPoints[0]; 
	vMax = m_vWorldCornerPoints[0]; 

	//approximate the angled box into an axis aligned box
	for(int i =1; i< MAX_NUM_CORNERS; i++)
	{
		vMin.x  = Min(m_vWorldCornerPoints[i].x, vMin.x); 
		vMin.y  = Min(m_vWorldCornerPoints[i].y, vMin.y);

		vMax.x  = Max(m_vWorldCornerPoints[i].x, vMax.x); 
		vMax.y  = Max(m_vWorldCornerPoints[i].y, vMax.y);

	}
	vMax.z = vMax.z + m_pBlockingBounds->GetHeight(); 
	//scrThreadId iDummyScriptId = static_cast<scrThreadId>(0xFFFFFFFF);
	//CPathServer::SwitchPedsOnInArea(vMin, vMax, iDummyScriptId);

	CPathServer::ClearPedSwitchAreas(vMin, vMax);
	CPedPopulation::PedNonCreationAreaClear();
}

//////////////////////////////////////////////////////////////////////

void CCutSceneBlockingBoundsEntity::RemovePedsAndCars()
{
	// have to delete from the axis aligned area used by the scenario and 
	// population blocking systems, otherwise peds could be left within the blocked area.
	Vector3 vMin, vMax; 
	vMin = m_vWorldCornerPoints[0]; 
	vMax = m_vWorldCornerPoints[0]; 

	//approximate the angled box into an axis aligned box
	for(int i =1; i< MAX_NUM_CORNERS; i++)
	{
		vMin.x  = Min(m_vWorldCornerPoints[i].x, vMin.x); 
		vMin.y  = Min(m_vWorldCornerPoints[i].y, vMin.y);

		vMax.x  = Max(m_vWorldCornerPoints[i].x, vMax.x); 
		vMax.y  = Max(m_vWorldCornerPoints[i].y, vMax.y);

	}
	vMax.z = vMax.z + m_pBlockingBounds->GetHeight();

    float DistanceToFace = vMax.y - vMin.y; //distance to opposite face

	vMax.y = vMin.y;

	CGameWorld::ClearCarsFromAngledArea(vMin, vMax, DistanceToFace, true, false, false, false, false);
	CGameWorld::ClearPedsFromAngledArea(vMin, vMax, DistanceToFace); 
}

//////////////////////////////////////////////////////////////////////

#if __BANK
void CCutSceneBlockingBoundsEntity::DebugDraw() const
{
	grcDebugDraw::Axis(m_mArea, 0.5, true); 
	grcDebugDraw::BoxOriented(RCC_VEC3V(m_vAreaMin), RCC_VEC3V(m_vAreaMax), RCC_MAT34V(m_mArea), Color32(255,0,0,255), false); 
}
void CCutSceneBlockingBoundsEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Blocking Bound Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}
#endif

//////////////////////////////////////////////////////////////////////
