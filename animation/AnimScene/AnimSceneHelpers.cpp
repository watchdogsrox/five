#include "AnimSceneHelpers.h"
#include "AnimSceneHelpers_parser.h"

// game includes
#include "animation/AnimScene/AnimScene.h"
#include "animation\AnimScene\AnimSceneManager.h"
#include "animation/AnimScene/entities/AnimSceneEntities.h"
#include "peds/Ped.h"

ANIM_OPTIMISATIONS()

#if __BANK
atArray<CAnimSceneMatrix*> CAnimSceneMatrix::m_AnimSceneMatrix; 
//////////////////////////////////////////////////////////////////////////
// CAnimSceneMatrix - contains a matrix (with better rag widget editing)

void CAnimSceneMatrix::OnPreAddWidgets(bkBank& bank)
{

	m_bankData->m_orientation = Mat34VToEulersXYZ(m_matrix);
	m_bankData->m_orientation = Scale(m_bankData->m_orientation, ScalarV(V_TO_DEGREES));
	m_bankData->m_position = m_matrix.d();
	bank.AddVector("orientation", &m_bankData->m_orientation, -2.0f*PI*RtoD, 2.0f*PI*RtoD, 0.1f); 
	bank.AddVector("position", &m_bankData->m_position, -100000.0f, 100000.0f, 0.01f);
	
	for(int i = 0 ; i <m_AnimSceneMatrix.GetCount(); i++)
	{
		if(m_AnimSceneMatrix[i] == this)
		{
			return; 
		}
	}
	
	m_AnimSceneMatrix.PushAndGrow(this); 
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneMatrix::OnPostAddWidgets(bkBank& UNUSED_PARAM(bank))
{

}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneMatrix::OnWidgetChanged()
{
	Vec3V orientationInRadians(m_bankData->m_orientation);
	orientationInRadians = Scale(orientationInRadians, ScalarV(V_TO_RADIANS));
	Mat34VFromEulersXYZ(m_matrix, orientationInRadians, m_bankData->m_position);
	m_bankData->m_bWidgetChanged = true;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneMatrix::Update()
{
	// Update the transform authoring helper from the rag position and rotation widgets
	Vec3V orientationInRadians(m_bankData->m_orientation);
	orientationInRadians = Scale(orientationInRadians, ScalarV(V_TO_RADIANS));
	Mat34VFromEulersXYZ(m_matrix, orientationInRadians, m_bankData->m_position);
	Matrix34 NewMat; 
	if(m_bankData->m_parentSceneMatrix)
	{
		const Matrix34 parentMat = MAT34V_TO_MATRIX34(m_bankData->m_parentSceneMatrix->GetMatrix()); 
		m_bankData->m_AuthoringHelper.Update(MAT34V_TO_MATRIX34(m_matrix), 1.0f, VEC3_ZERO, NewMat, parentMat); 
	}
	else
	{
		m_bankData->m_AuthoringHelper.Update(MAT34V_TO_MATRIX34(m_matrix), 1.0f, VEC3_ZERO, NewMat); 
	}

	if (m_bankData->m_bWidgetChanged || (m_bankData->m_AuthoringHelper.GetCurrentInput() != CAuthoringHelper::NO_INPUT))
	{
		// Update the position and rotation widgets using the transform authoring helper
		Vector3 orient3; 
		NewMat.ToEulersFastXYZ(orient3);
		m_bankData->m_orientation = VECTOR3_TO_VEC3V(orient3); 
		m_bankData->m_orientation = Scale(m_bankData->m_orientation, ScalarV(V_TO_DEGREES));
		m_bankData->m_position = VECTOR3_TO_VEC3V(NewMat.d); 

		orientationInRadians = m_bankData->m_orientation;
		orientationInRadians = Scale(orientationInRadians, ScalarV(V_TO_RADIANS));
		Mat34VFromEulersXYZ(m_matrix, orientationInRadians, m_bankData->m_position);
	}

	m_bankData->m_bWidgetChanged = false;
}
#endif
////////////////////////////////////////////////////////////////////////////
//// CAnimSceneMatrixPtr - contains a pointer to an optional matrix (with better rag widget editing)
//
//void CAnimSceneMatrixPtr::OnPreAddWidgets(bkBank& bank)
//{
//	if (m_pMatrix)
//	{
//		m_orientation = VEC3V_TO_VECTOR3(Mat34VToEulersXYZ(*m_pMatrix));
//		m_position = m_pMatrix->d();
//	}
//	else
//	{
//		m_orientation.Zero();
//		m_position.ZeroComponents();
//	}
//
//	bank.AddToggle("enable", &m_hasMatrix, datCallback(MFA(CAnimSceneMatrix::OnWidgetChanged),this));
//	bank.AddAngle("heading", &m_orientation.z, bkAngleType::RADIANS,
//		datCallback(MFA(CAnimSceneMatrix::OnWidgetChanged),this) );
//	bank.AddAngle("pitch", &m_orientation.x, bkAngleType::RADIANS,
//		datCallback(MFA(CAnimSceneMatrix::OnWidgetChanged),this) );
//	bank.AddAngle("yaw", &m_orientation.y, bkAngleType::RADIANS,
//		datCallback(MFA(CAnimSceneMatrix::OnWidgetChanged),this) );
//	bank.AddVector("position", &m_position, -100000.0f, 100000.0f, 0.01f, datCallback(MFA(CAnimSceneMatrix::OnWidgetChanged),this));
//}
//
////////////////////////////////////////////////////////////////////////////
//
//void CAnimSceneMatrixPtr::OnPostAddWidgets(bkBank& UNUSED_PARAM(bank))
//{
//
//}
//
////////////////////////////////////////////////////////////////////////////
//
//void CAnimSceneMatrixPtr::OnWidgetChanged()
//{
//	if (m_hasMatrix)
//	{
//		if (!m_pMatrix)
//		{
//			m_pMatrix = rage_new Mat34V();
//		}
//
//		if (m_pMatrix)
//		{
//			Mat34VFromEulersXYZ(*m_pMatrix, RC_VEC3V(m_orientation), m_position);
//		}
//	}
//	else
//	{
//		if (m_pMatrix)
//		{
//			delete m_pMatrix;
//			m_pMatrix = NULL;
//		}
//	}
//}

//////////////////////////////////////////////////////////////////////////
// CAnimSceneClip - identifies a single clip

#if __BANK
void CAnimSceneClip::OnPreAddWidgets(bkBank& bank)
{
	m_widgets->m_pGroup = (bkGroup*)bank.GetCurrentGroup();
	AddClipWidgets();
}

void CAnimSceneClip::AddClipWidgets()
{
	if (m_widgets->m_pGroup)
	{
		bkGroup& group = *m_widgets->m_pGroup;

		if (m_widgets->m_pClipSelector)
		{
			if (m_clipDictionaryName.GetHash()!=0)
			{
				m_widgets->m_pClipSelector->SetCachedClip(m_clipDictionaryName, m_clipName);
			}
			m_widgets->m_pClipSelector->AddWidgets(m_widgets->m_pGroup, datCallback(MFA(CAnimSceneClip::OnDictionaryChanged),this), datCallback(MFA(CAnimSceneClip::OnClipChanged),this));

			// Register for widget shutdown.
			// This isn't normally necessary (widgets get destroyed automatically)
			// but the clip selector requires some internal cleanup before the
			// widgets are deleted.
			CAnimScene* pScene = g_AnimSceneManager->GetSceneEditor().GetScene();
			if (pScene)
			{
				pScene->RegisterWidgetShutdown(MakeFunctor(*this, &CAnimSceneClip::OnShutdownWidgets));
			}

			group.AddButton("Save changes", datCallback(MFA(CAnimSceneClip::SaveChanges),this));
		}
		else
		{
			// Add read only text fields for the dictionary and anim names, and the edit button
			group.AddText("Dictionary", &m_clipDictionaryName, true, NullCB);
			group.AddText("Clip", &m_clipName, true, NullCB);
			group.AddButton("Edit", datCallback(MFA(CAnimSceneClip::Edit),this));
		}
	}	
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneClip::Edit()
{
	if (m_widgets->m_pGroup && m_widgets->m_pClipSelector==NULL)
	{
		m_widgets->m_pClipSelector = rage_new CDebugClipSelector(true, false, false);

		// delete the existing widgets
		while (m_widgets->m_pGroup->GetChild())
		{
			m_widgets->m_pGroup->GetChild()->Destroy();
		}

		AddClipWidgets();
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneClip::SaveChanges()
{
	if (m_widgets->m_pGroup && m_widgets->m_pClipSelector)
	{
		m_widgets->m_pClipSelector->RemoveAllWidgets();
		m_widgets->m_pClipSelector->Shutdown();
		delete m_widgets->m_pClipSelector;
		m_widgets->m_pClipSelector = NULL;
		
		// delete the existing widgets
		while (m_widgets->m_pGroup->GetChild())
		{
			m_widgets->m_pGroup->GetChild()->Destroy();
		}

		AddClipWidgets();
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneClip::OnShutdownWidgets()
{
	if (m_widgets->m_pClipSelector)
		m_widgets->m_pClipSelector->Shutdown();

	delete m_widgets->m_pClipSelector;
	m_widgets->m_pClipSelector = NULL;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneClip::OnDictionaryChanged()
{
	animAssertf(m_widgets->m_pClipSelector,"Clip does not exist (probably invalid selection or search).");
	if(!m_widgets->m_pClipSelector)
	{
		return;
	}	
	if(!m_widgets->m_pClipSelector->HasSelection())
	{
		return;
	}
	m_clipDictionaryName.SetFromString(m_widgets->m_pClipSelector->GetSelectedClipDictionary());
	m_clipName.SetFromString(m_widgets->m_pClipSelector->GetSelectedClipName());
	if (m_widgets->m_clipSelectedCallback)
	{
		m_widgets->m_clipSelectedCallback(m_clipDictionaryName, m_clipName);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneClip::OnClipChanged()
{
	m_clipName.SetFromString(m_widgets->m_pClipSelector->GetSelectedClipName());
	if (m_widgets->m_clipSelectedCallback)
	{
		m_widgets->m_clipSelectedCallback(m_clipDictionaryName, m_clipName);
	}
}

#endif // __BANK

//////////////////////////////////////////////////////////////////////////
// CAnimSceneEntityHandle - Identifies an individual scene entity

void CAnimSceneEntityHandle::OnInit(CAnimScene* pScene)
{
	if (!pScene)
	{
		Assertf(false, "pScene must not be null");
		return;
	}

	if(m_entityId)
	{
		CAnimSceneEntity* pEnt = pScene->GetEntity(m_entityId);
		if (!pEnt)
		{
			Assertf(false, "Could not find entity '%s' in anim scene '%s'", m_entityId.TryGetCStr(), pScene->GetName().TryGetCStr());
			return;
		}
		m_pEntity = pEnt;
	}
	
	m_pOwningScene = pScene;
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEntityHandle::OnPreSave()
{
#if __BANK
	// Update the entity id from the entity
	UpdateEntityId();
#endif // __BANK
}

//////////////////////////////////////////////////////////////////////////
CAnimSceneEntity::Id CAnimSceneEntityHandle::GetId() const
{
#if __BANK
	UpdateEntityId();
#endif

	return m_entityId;
}

//////////////////////////////////////////////////////////////////////////

CAnimSceneEntity* CAnimSceneEntityHandle::GetEntity()
{
#if __BANK
	UpdateEntityId();
	Assertf(m_pOwningScene != NULL, "m_pOwningScene should not be null, was OnInit() called on this handle?");
#endif // __BANK

	return m_pEntity;
}

const CAnimSceneEntity* CAnimSceneEntityHandle::GetEntity() const 
{
#if __BANK
	Assertf(m_pOwningScene != NULL, "m_pOwningScene should not be null, was OnInit() called on this handle?");
#endif // __BANK

	return m_pEntity;
}

CPhysical* CAnimSceneEntityHandle::GetPhysicalEntity()
{
#if __BANK
	UpdateEntityId();
	Assertf(m_pOwningScene != NULL, "m_pOwningScene should not be null, was OnInit() called on this handle?");
#endif // __BANK

	if (m_pEntity)
	{
		switch (m_pEntity->GetType())
		{
		case ANIM_SCENE_ENTITY_PED:
			{
				return static_cast<CAnimScenePed*>(m_pEntity)->GetPed();
			}
			break;
		case ANIM_SCENE_ENTITY_OBJECT:
			{
				return static_cast<CAnimSceneObject*>(m_pEntity)->GetObject();
			}
			break;
		case ANIM_SCENE_ENTITY_VEHICLE:
			{
				return static_cast<CAnimSceneVehicle*>(m_pEntity)->GetVehicle();
			}
			break;
		default:
			{
				return NULL;
			}
			break;
		}		
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

#if __BANK

void CAnimSceneEntityHandle::SetEntity(CAnimSceneEntity* pEnt)
{
	Assert(pEnt);

	if (pEnt)
	{
		m_pEntity = pEnt;
		UpdateEntityId();
	}
	else
	{
		// Only reset the entity id and entity if initialised.
		if (m_pOwningScene)
		{
			m_pEntity = NULL;
			m_entityId.SetHash(0);
		}
		
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEntityHandle::UpdateEntityId() const
{
	if (m_pEntity)
	{
		m_entityId = m_pEntity->GetId();
		m_widgets->m_selector.Select(m_entityId);
	}
	else
	{
		// Only reset the entity id if initialised.
		if (m_pOwningScene)
		{
			m_entityId.SetHash(0);
		}
		m_widgets->m_selector.Select(m_entityId);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEntityHandle::OnPreAddWidgets(bkBank& bank)
{
	m_widgets->m_selector.AddWidgets(bank, CAnimScene::GetCurrentAddWidgetsScene(), MakeFunctor(*this, &CAnimSceneEntityHandle::OnEntityChanged));
	m_widgets->m_selector.Select(m_entityId); // sync the widget selection to our currently selected entity
	m_widgets->m_selector.ForceComboBoxRedraw(); // Ensure the widget is redrawn
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEntityHandle::OnEntityChanged()
{
	if (m_widgets->m_selector.HasSelection())
	{
		m_entityId.SetFromString(m_widgets->m_selector.GetSelectedEntityName().GetCStr());

		Assertf(m_pOwningScene != NULL, "m_pOwningScene should not be null, was OnInit() called on this handle?");
		if (m_pOwningScene)
		{
			m_pEntity = m_pOwningScene->GetEntity(m_entityId);
		}
		else
		{
			// ERROR - We've changed the id, but have no way of getting the entity pointer.
			m_pEntity = NULL;
		}
	}
	else
	{
		m_pEntity = NULL;
		m_entityId.SetHash(0);
	}
}

#endif //__BANK