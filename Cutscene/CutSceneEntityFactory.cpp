// File header
#include "Cutscene\CutSceneEntityFactory.h"

//Rage header

//game header 
#include "Objects/object.h"
#include "CutsceneObject.h"
#include "Modelinfo\BaseModelInfo.h "
#include "Modelinfo\ModelInfo.h"
#include "Cutscene/cutscene_channel.h"

namespace CutsceneCreatureFactory
{
	CCutSceneObject* Create(u32 ModelIndex, const Vector3 &vPos)
	{
		CCutSceneObject* pCutsceneCreature = NULL;
		
		CBaseModelInfo *pModelInfo = (CBaseModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(ModelIndex));
		
		if (pModelInfo)
		{
			cutsceneDebugf1("Creating cutscene object: %s", pModelInfo->GetModelName());
#if __DEV
			CObject::bInObjectCreate = true;		//used in dev to make sure that the objects get created through the correct channels.
#endif
			if (pModelInfo->GetModelType() == MI_TYPE_PED)
			{
				Assertf(0,"cannot create ped type object"); 
			}
			else if (pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
			{
				pCutsceneCreature = rage_new CCutSceneVehicle( ENTITY_OWNEDBY_CUTSCENE, ModelIndex,vPos);
			}
			else //(pModelInfo->GetIsTypeObject())
			{
				pCutsceneCreature = rage_new CCutSceneProp( ENTITY_OWNEDBY_CUTSCENE, ModelIndex, vPos);
			}
			
#if __DEV
			CObject::bInObjectCreate = false;
#endif
			return pCutsceneCreature;
		}
		return pCutsceneCreature;

	};
}
