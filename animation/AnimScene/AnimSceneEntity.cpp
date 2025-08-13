#include "AnimSceneEntity.h"
#include "AnimSceneEntity_parser.h"

// rage includes
#include "bank/bank.h"
#include "bank/msgbox.h"
#include "parser/macros.h"
#include "parser/memberenumdata.h"

//game includes
#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/AnimSceneManager.h"


const CAnimSceneEntity::Id ANIM_SCENE_ENTITY_ID_INVALID((u32)0);

EXTERN_PARSER_ENUM(eAnimSceneEntityType);

#if !__NO_OUTPUT

extern const char* parser_eAnimSceneEntityType_Strings[];

const char * CAnimSceneEntity::GetEntityTypeName(eAnimSceneEntityType type)
{
	return parser_eAnimSceneEntityType_Strings[type];
}

#endif // !__NO_OUTPUT

const atHashString FILE_PLAYBACK_LIST_ID_INVALID("*");

const AnimSceneEntityLocationData* CAnimSceneEntity::GetLocationData(atHashString entityId) const
{
	if (entityId.GetHash()==ANIM_SCENE_ENTITY_ID_INVALID.GetHash())
	{
		entityId = FILE_PLAYBACK_LIST_ID_INVALID;
	}

	 AnimSceneEntityLocationData *const*ppSit = m_locationData.SafeGet(entityId);
	if(ppSit)
	{
		return *ppSit;
	}

	return NULL;
}

void CAnimSceneEntity::SetLocationData(atHashString entityId, const AnimSceneEntityLocationData& mat)
{
	if (entityId.GetHash()==ANIM_SCENE_ENTITY_ID_INVALID.GetHash())
	{
		entityId = FILE_PLAYBACK_LIST_ID_INVALID;
	}

	AnimSceneEntityLocationData* pEnt =  GetLocationData(entityId);

	if (pEnt)
	{
		*pEnt =mat;
	}
	else
	{
		// entity doesn't exist - create it
		pEnt = rage_new AnimSceneEntityLocationData();
		if (pEnt)
		{
			// set the situation values
			*pEnt = mat;
			m_locationData.SafeInsert(entityId, pEnt);
			m_locationData.FinishInsertion();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

AnimSceneEntityLocationData* CAnimSceneEntity::GetLocationData(atHashString entityId)
{
	if (entityId.GetHash()==ANIM_SCENE_ENTITY_ID_INVALID.GetHash())
	{
		entityId = FILE_PLAYBACK_LIST_ID_INVALID;
	}

	AnimSceneEntityLocationData **ppSit = m_locationData.SafeGet(entityId);
	if(ppSit)
	{
		return *ppSit;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneEntity::IsValidId(const CAnimSceneEntity::Id& id)
{
	//Only check for valid ids in bank release
#if __BANK
	const char* idStr = id.TryGetCStr();
	if(!idStr)
	{
		return false;
	}
	std::string idStrS = idStr;
	if(idStrS.length() == 0)
	{
		return false;
	}

	for(int i = 0; i < idStrS.length(); )
	{
		if(idStrS[i] == '\n' || idStrS[i] == '\r')
		{
			return false;
		}
		else
		{
			++i;
		}
	}
#endif
	return id.IsNotNull();
}

#if __BANK


//////////////////////////////////////////////////////////////////////////

char newEntityId[64];

void CAnimSceneEntity::OnPreAddWidgets(bkBank& bank)
{
	// For renaming purposes, we add a read-only entry for it's current name, and a textbox and button to rename it.
	// This is so we have the previous and new name together.

	// Clear rename box each time
	sprintf(newEntityId, "");

	bank.AddText("Id", &m_Id, true);
	bank.AddText("Rename To", &newEntityId[0], sizeof(newEntityId));
	bank.AddButton("Rename Entity", datCallback(MFA(CAnimSceneEntity::Rename),this));
}

//////////////////////////////////////////////////////////////////////////

void CAnimSceneEntity::OnPostAddWidgets(bkBank& bank)
{
	// add the delete button to the widgets
	//bank.AddButton("Delete this entity", datCallback(MFA1(CAnimScene::DeleteEntity), (datBase*)CAnimScene::GetCurrentAddWidgetsScene(), this));
	bank.AddButton("Delete this entity", datCallback(MFA1(CAnimScene::SetShouldDeleteEntityCB), (datBase*)CAnimScene::GetCurrentAddWidgetsScene(), this));
}

//////////////////////////////////////////////////////////////////////////


void CAnimSceneEntity::Rename()
{
	// Check the new name is valid...
	if (strlen(newEntityId) == 0 && IsValidId(newEntityId))
	{
		// No name entered, or has invalid characters
		bkMessageBox("Enter new entity Id", "You must enter a valid entity id (no id entered or invalid characters).", bkMsgOk, bkMsgInfo);
		return;
	}

	// Check that the new name doesn't already exist as an entity...
	CAnimScene::Id newName(&newEntityId[0]);

	CAnimScene* pAnimScene = g_AnimSceneManager->GetSceneEditor().GetScene();
	if (!pAnimScene)
	{
		// No active anim scene
		return;
	}

	// Now rename...
	if (!pAnimScene->RenameEntity(m_Id, newName))
	{
		bkMessageBox("Enter different entity Id", "Could not rename - The entity may not exist or there might already be an entity with the new name", bkMsgOk, bkMsgInfo);
	}
}

//////////////////////////////////////////////////////////////////////////
#if __DEV
void CAnimSceneEntity::DebugRender(CAnimScene* UNUSED_PARAM(pScene), debugPlayback::OnScreenOutput& mainOutput)
{
	if (mainOutput.AddLine(GetDebugString().c_str()) && CAnimSceneManager::IsMouseDown())
	{
		OnDebugTextSelected();
	}
}
#endif //__DEV

//////////////////////////////////////////////////////////////////////////

atString CAnimSceneEntity::GetDebugString()
{
	return atVarString("%s (%s)", GetId().GetCStr(), GetEntityTypeName(GetType()));
}

//////////////////////////////////////////////////////////////////////////

bool CAnimSceneEntity::IsInEditor()
{
	if (!g_AnimSceneManager->GetSceneEditor().IsActive())
	{
		return false;
	}

	if (GetScene()==NULL)
	{
		return false;
	}

	return g_AnimSceneManager->GetSceneEditor().GetScene() == GetScene();
}

#endif //__BANK