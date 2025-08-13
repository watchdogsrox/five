/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneAssetManager.cpp
// PURPOSE : An entity manager class for streaming of cutscene objects
// AUTHOR  : Thomas French
// STARTED : 20/07/2009
//
/////////////////////////////////////////////////////////////////////////////////

//Rage headers
#include "cutscene/cutsevent.h"
#include "cutscene/cutseventargs.h"
#include "vfx/ptfx/ptfxmanager.h "


//Game Headers
#include "script/script.h"
#include "script/commands_interiors.h"
#include "Cutscene/cutscene_channel.h"
#include "cutscene/CutSceneDefine.h"
#include "Cutscene/CutSceneAssetManager.h"
#include "Cutscene/AnimatedModelEntity.h"
#include "Cutscene/CutSceneBoundsEntity.h"
#include "Cutscene/cutscene_channel.h"
#include "Cutscene/CutsceneCustomEvents.h"
#include "entity/archetype.h"
#include "entity/archetypemanager.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "fwscene/stores/maptypesstore.h"
#include "peds/Ped.h"
#include "peds/rendering/PedVariationStream.h"
#include "scene/entities/compEntity.h"
#include "scene/world/GameWorld.h"
#include "scene/texLod.h"
#include "streaming/streaming.h"
#include "modelinfo/PedModelInfo.h"
#include "cutscene/CutSceneManagerNew.h"
#include "script/commands_object.h"
#include "vfx/misc/MovieManager.h"
#include "Vfx/Systems/VfxScript.h"

///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS ()

///////////////////////////////////////////////////////////////////////////////////////////////////

const char *GetStreamingDataTypeString(eStreamingDataType streamingDataType)
{
	switch(streamingDataType)
	{
	case INVALID_TYPE: return "INVALID_TYPE";
	case SCALEFORM_OVERLAY_TYPE: return "SCALEFORM_OVERLAY_TYPE";
	case BINK_OVERLAY_TYPE: return "BINK_OVERLAY_TYPE";
	case RAYFIRE_TYPE: return "RAYFIRE_TYPE";
	case MODEL_TYPE: return "MODEL_TYPE";
	case MODEL_ITYPE_TYPE: return "MODEL_ITYPE_TYPE";
	case SUBTITLES_TYPE: return "SUBTITLES_TYPE";
	case PTFX_TYPE: return "PTFX_TYPE";
	case PED_VARIATION_TYPE: return "PED_VARIATION_TYPE";
	case INTERIOR_TYPE: return "INTERIOR_TYPE";
	default: return "UNKNOWN";
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
CCutSceneStreamingInfo::CCutSceneStreamingInfo()
:m_ObjectId(-1),
m_Index(-1),
m_DataType(INVALID_TYPE),
m_ref(0)
{
}

CCutSceneStreamingInfo::~CCutSceneStreamingInfo()
{

}


CCutSceneStreamingInfo::CCutSceneStreamingInfo(s32 m_ObjectId, s32 m_Index,eStreamingDataType m_DataType, atHashString m_Hash)
:m_ObjectId(m_ObjectId),
m_Index(m_Index),
m_DataType(m_DataType),
m_Hash(m_Hash),
m_ref(0)
{
 AddRef(); 
}

#if !__NO_OUTPUT

atString CCutSceneStreamingInfo::GetDebugString()
{
#if !__NO_OUTPUT
	return atVarString("ObjectId:%i Index:%i DataType:%s Hash:%s", m_ObjectId, m_Index, GetStreamingDataTypeString(m_DataType), m_Hash.GetCStr());
#else
	return atVarString("ObjectId:%i Index:%i DataType:%s Hash:%u", m_ObjectId, m_Index, GetStreamingDataTypeString(m_DataType), m_Hash.GetHash());
#endif
}

#endif // !__NO_OUTPUT

///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneSingleRequestStreamingInfo::~CCutSceneSingleRequestStreamingInfo()
{

}

CCutSceneSingleRequestStreamingInfo::CCutSceneSingleRequestStreamingInfo()
:CCutSceneStreamingInfo()
{

}


CCutSceneSingleRequestStreamingInfo::CCutSceneSingleRequestStreamingInfo(s32 m_ObjectId, s32 m_Index,eStreamingDataType m_DataType, atHashString m_Hash)
:CCutSceneStreamingInfo(m_ObjectId, m_Index, m_DataType, m_Hash)
{

}

#if __BANK

atString CCutSceneSingleRequestStreamingInfo::GetDebugString()
{
	u32 uSize = 0;
	if(m_request.IsValid())
	{
		strStreamingInfo *pStreamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(m_request.GetIndex());
		if(pStreamingInfo)
		{
			uSize += pStreamingInfo->ComputeVirtualSize(m_request.GetIndex(), true);
		}
	}
	return CCutSceneStreamingInfo::GetDebugString() += atVarString(" ModuleId:%u RequestId:%u Size:%u", m_request.GetModuleId(), m_request.GetRequestId().Get(), uSize);
}

#endif // __BANK

CCutsceneModelRequestStreamingInfo::CCutsceneModelRequestStreamingInfo()
{

}

CCutsceneModelRequestStreamingInfo::~CCutsceneModelRequestStreamingInfo()
{

}

CCutsceneModelRequestStreamingInfo::CCutsceneModelRequestStreamingInfo(s32 m_ObjectId, s32 m_Index,eStreamingDataType m_DataType, atHashString m_Hash)
:CCutSceneStreamingInfo(m_ObjectId, m_Index, m_DataType, m_Hash)
{

}

#if __BANK

atString CCutsceneModelRequestStreamingInfo::GetDebugString()
{
#if !__NO_OUTPUT
	return CCutSceneStreamingInfo::GetDebugString() += atVarString(" TypeSlotIndex:%i ModelName:%s Flags:%u", m_request.GetTypSlotIdx(), m_request.GetModelName().GetCStr(), m_request.GetFlags());
#else
	return CCutSceneStreamingInfo::GetDebugString() += atVarString(" TypeSlotIndex:%i ModelName:%u Flags:%u", m_request.GetTypSlotIdx(), m_request.GetModelName().GetHash(), m_request.GetFlags());
#endif
}

#endif // __BANK

///////////////////////////////////////////////////////////////////////////////////////////////////

CCutsceneVariationStreamingInfo::~CCutsceneVariationStreamingInfo()
{
	m_request.ReleaseAll(); 
}

CCutsceneVariationStreamingInfo::CCutsceneVariationStreamingInfo()
:CCutSceneStreamingInfo()
,m_Component(0) 
,m_Texture (0) 
,m_Drawable (0)
{

	m_request.UseAsArray();
}

bool CCutsceneVariationStreamingInfo::IsReleased()
{
	for (int i = 0 ; i < m_request.GetRequestCount(); ++i)
	{
		if(m_request.GetRequest(i).IsValid() )
		{
			return false; 
		}
	}
	return true; 
}

CCutsceneVariationStreamingInfo::CCutsceneVariationStreamingInfo(s32 m_ObjectId, s32 m_Index,eStreamingDataType m_DataType, atHashString m_Hash)
:CCutSceneStreamingInfo(m_ObjectId, m_Index, m_DataType, m_Hash)
,m_Component(0) 
,m_Texture (0) 
,m_Drawable (0)
{
	m_request.UseAsArray(); 
}

#if __BANK

atString CCutsceneVariationStreamingInfo::GetDebugString()
{
	atString debugString = CCutSceneStreamingInfo::GetDebugString();
	
	/*u32 uDrawableSize = 0;
	if(m_request.GetRequest(DRAWABLE_SLOT).IsValid())
	{
		strStreamingInfo *pStreamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(m_request.GetRequest(DRAWABLE_SLOT).GetIndex());
		if(pStreamingInfo)
		{
			uDrawableSize += pStreamingInfo->ComputeVirtualSize(m_request.GetRequest(DRAWABLE_SLOT).GetIndex(), true);
		}
	}

	u32 uTextureSize = 0;
	if(m_request.GetRequest(TEXTURE_SLOT).IsValid())
	{
		strIndex index = m_request.GetRequest(TEXTURE_SLOT).GetIndex();
		strStreamingInfo *pStreamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);
		if(pStreamingInfo)
		{
			uTextureSize += pStreamingInfo->ComputeVirtualSize(index, true);
		}
	}

	u32 uClothSize = 0;
	if(m_request.GetRequest(CLOTH_SLOT).IsValid())
	{
		strStreamingInfo *pStreamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(m_request.GetRequest(CLOTH_SLOT).GetIndex());
		if(pStreamingInfo)
		{
			uClothSize += pStreamingInfo->ComputeVirtualSize(m_request.GetRequest(CLOTH_SLOT).GetIndex(), true);
		}
	}

	debugString += atVarString(" Component:%u (%s), Drawable:%u, Texture:%u - DrawableSize:%u, TextureSize:%u ClothSize:%u", m_Component, CPedVariationData::GetVarOrPropSlotName(m_Component), m_Drawable, m_Texture, uDrawableSize, uTextureSize, uClothSize);*/

	debugString += atVarString(" Component:%u (%s), Drawable:%u, Texture:%u", m_Component, CPedVariationData::GetVarOrPropSlotName(m_Component), m_Drawable, m_Texture);

	return debugString;
}

#endif // __BANK


///////////////////////////////////////////////////////////////////////////////////////////////////
void CCutSceneStreamingInfo::RemoveRef()
{
	if(m_ref > 0)
	{
		m_ref -= 1; 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneAssetMgrEntity::CCutSceneAssetMgrEntity( const cutfObject* pObject )
:cutsUniqueEntity(pObject)
, m_loadState(LOAD_STATE_NONE)
, m_TimeSpentStreaming(0)
{
	sprintf(m_TextBlockName, "");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneAssetMgrEntity::~CCutSceneAssetMgrEntity()
{
	RemoveAllStreamingRequests(); 
	m_TimeSpentStreaming = 0; 
	cutsceneDebugf1("~CCutSceneAssetMgrEntity() ");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Get the Model index of the streaming request

 bool CCutSceneAssetMgrEntity::GetModelStreamingRequestId(s32 iObjectId, const atHashString& Hash,  s32 &iModelRequest)
{
	//Could be optimized with a binary search
	for(int i=0; i < m_StreamingRequests.GetCount(); i++)
	{
		CCutSceneStreamingInfo* pStreamingInfo = m_StreamingRequests[i]; 
		
			if(pStreamingInfo)
			{
				if (iObjectId == pStreamingInfo->GetObjectId() && (pStreamingInfo->GetDataType() == MODEL_TYPE  || pStreamingInfo->GetDataType() == MODEL_ITYPE_TYPE )&& 
					pStreamingInfo->m_Hash.GetHash() == Hash.GetHash())
				{
					if(!pStreamingInfo->IsReleased())
					{
						if(pStreamingInfo->GetDataType() == MODEL_TYPE)
						{
							iModelRequest = static_cast<CCutSceneSingleRequestStreamingInfo*>(m_StreamingRequests[i])->GetRequest()->GetRequestId().Get();
						}
						
						if(pStreamingInfo->GetDataType() == MODEL_ITYPE_TYPE)
						{
							iModelRequest = static_cast<CCutsceneModelRequestStreamingInfo*>(m_StreamingRequests[i])->GetRequest()->GetRequestId().Get();
						}
						
						Assertf(iModelRequest != strLocalIndex::INVALID_INDEX, "Cut scene entity will not load trying to load invalid index");

						return (iModelRequest != strLocalIndex::INVALID_INDEX);  		
					}
					else
					{
						RemoveStreamingRequest(i); 
						return false; 
					}
				}
			}
	}
	
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Check that the model has loaded

 bool CCutSceneAssetMgrEntity::HasRequestedModelLoaded(s32 iObjectId, const atHashString& Hash)
 {
	 for(int i=0; i < m_StreamingRequests.GetCount(); i++)
	 {
		 if( m_StreamingRequests[i])
		 {
			 bool isModleType = m_StreamingRequests[i]->GetDataType() == MODEL_TYPE || m_StreamingRequests[i]->GetDataType() == MODEL_ITYPE_TYPE; 
			 if (iObjectId == m_StreamingRequests[i]->GetObjectId() && isModleType && m_StreamingRequests[i]->m_Hash.GetHash() == Hash.GetHash())
			 {
				 return m_StreamingRequests[i]->HasLoaded();
			 }
		 }
	 }
	 return false;
 }


 ///////////////////////////////////////////////////////////////////////////////////////////////////
 // Check that the model has loaded
 bool CCutSceneAssetMgrEntity::IsModelInRequestList(s32 iObjectId, const atHashString& Hash) const
 {
	 for(int i=0; i < m_StreamingRequests.GetCount(); i++)
	 {
		 bool isModleType = m_StreamingRequests[i]->GetDataType() == MODEL_TYPE || m_StreamingRequests[i]->GetDataType() == MODEL_ITYPE_TYPE; 
		 
		if(m_StreamingRequests[i] && isModleType)		
		{
			 if (iObjectId == m_StreamingRequests[i]->GetObjectId() &&  m_StreamingRequests[i]->m_Hash.GetHash() == Hash.GetHash())
			 {
				 return true;
			 }
		}
	 }
	 return false;
 }

bool CCutSceneAssetMgrEntity::HasPTFXListLoaded(const atHashString& Hash) const
{
	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlot(Hash.GetHash());

	if( cutsceneVerifyf( slot != -1, "PTFX dictionary (Hash %d) %s does not exist", Hash.GetHash(), Hash.GetCStr() ))
	{
		if(ptfxManager::GetAssetStore().HasObjectLoaded(slot))
		{
			return true;
		}
	}
	
	return false; 

}

void CCutSceneAssetMgrEntity::RemoveVariationStreamRequest( u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, s32 iObjectId)
{
	s32 StreamingIndex = GetVariationStreamingIndexFromRequestList(iComp, iDrawable, iTexture, ModelName, iObjectId); 

	if(StreamingIndex != -1 && StreamingIndex < m_StreamingRequests.GetCount())
	{
		if(m_StreamingRequests[StreamingIndex] && m_StreamingRequests[StreamingIndex]->GetType() == VARIATION_REQUEST)
		{
			cutsceneAssetMgrDebugf2("RemoveVariationStreamRequest - Removing streaming request : %s",  m_StreamingRequests[StreamingIndex]->GetDebugString().c_str());

			/*
			CCutsceneVariationStreamingInfo* pInfo = static_cast<CCutsceneVariationStreamingInfo*>(m_StreamingRequests[StreamingIndex]); 			
			if (m_StreamingRequests[StreamingIndex]->HasLoaded())
			{
				cutsceneAssetMgrDebugf("++++++ SUCCESS Loaded Ped variation in time: %s Comp(%s/%d), Drwble(%d) Tex(%d)",  m_StreamingRequests[StreamingIndex]->GetHashString()->GetCStr(), CPedVariationData::GetVarOrPropSlotName(pInfo->m_Component), pInfo->m_Component, pInfo->m_Drawable, pInfo->m_Texture);
			}
			else
			{
				cutsceneAssetMgrDebugf("------ FAILED Loading Ped variation in time: %s Comp(%s/%d), Drwble(%d) Tex(%d)",  m_StreamingRequests[StreamingIndex]->GetHashString()->GetCStr(), CPedVariationData::GetVarOrPropSlotName(pInfo->m_Component), pInfo->m_Component, pInfo->m_Drawable, pInfo->m_Texture);
			}
			*/

			//cutsceneAssertf(m_StreamingRequests[StreamingIndex]->GetRef(), "Variation request does not have a ref");

			m_StreamingRequests[StreamingIndex]->RemoveRef();
			
			if(m_StreamingRequests[StreamingIndex]->m_ref == 0)
			{
				CCutsceneVariationStreamingInfo* pInfo = static_cast<CCutsceneVariationStreamingInfo*>(m_StreamingRequests[StreamingIndex]); 
				pInfo->ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED); 

				delete m_StreamingRequests[StreamingIndex]; 

				m_StreamingRequests[StreamingIndex] = NULL; 

				m_StreamingRequests.Delete(StreamingIndex); 
			}
		}
	}
}

eVariationStreamingStatus CCutSceneAssetMgrEntity::GetVariationLoadingState(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, u32 iObjectId)
{
	s32 StreamingIndex = GetVariationStreamingIndexFromRequestList(iComp, iDrawable, iTexture, ModelName, iObjectId); 

	if(StreamingIndex == -1)
	{
		return NO_REQUEST_FOR_VARIATION;
	}
	else if(StreamingIndex < m_StreamingRequests.GetCount())
	{
		if(m_StreamingRequests[StreamingIndex]->HasLoaded())
		{
			return LOADED_VARIATION; 
		}
		else
		{
			return LOADING_VARIATION; 
		}
	}
	return NO_REQUEST_FOR_VARIATION;
}

s32 CCutSceneAssetMgrEntity::GetVariationStreamingIndexFromRequestList (u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, s32 iObjectId)
{
	for(int i =0; i < m_StreamingRequests.GetCount(); i++ )
	{
		if(m_StreamingRequests[i] && m_StreamingRequests[i]->GetType() == VARIATION_REQUEST)
		{
			CCutsceneVariationStreamingInfo* pInfo = static_cast<CCutsceneVariationStreamingInfo*>(m_StreamingRequests[i]); 
			
			if((iObjectId == pInfo->GetObjectId()) && (pInfo->GetHashString()->GetHash() == ModelName.GetHash()))
			{
				if(iTexture == pInfo->m_Texture && iDrawable == pInfo->m_Drawable 
					&& pInfo->m_Component == iComp) 
				{
					return i; 
				}
			}
		}
	}
	return -1; 
};

#if __ASSERT
bool CCutSceneAssetMgrEntity::IsLoadingAdditionalText(const char * textName)
{ 
	if (textName==NULL || (stricmp(m_TextBlockName,"")==0))
		return false;

	return stricmp(textName, m_TextBlockName)==0; 
}

#endif //__ASSERT

bool CCutSceneAssetMgrEntity::HaveAllVariationsLoaded(eStreamingDataType type)
{
	for(int i =0; i < m_StreamingRequests.GetCount(); i++ )
	{
		if(m_StreamingRequests[i] && m_StreamingRequests[i]->GetDataType() == type)
		{
			if(!m_StreamingRequests[i]->HasLoaded())
			{
				return false;
			}
		}
	}

	return true; 
}

/*void CCutSceneAssetMgrEntity::DumpAllVariationsLoaded(s32 ObjectID)
{
	for(int i =0; i < m_StreamingRequests.GetCount(); i++ )
	{
		if(m_StreamingRequests[i] && m_StreamingRequests[i]->GetDataType() == PED_VARIATION_TYPE)
		{
			// Find the animated actor actor associated with the streaming request
			atArray<cutsEntity*>  ActorMangers; 
			CutSceneManager::GetInstance()->GetEntityByType(CUTSCENE_ACTOR_GAME_ENITITY, ActorMangers); 
			for(int j = 0;  j <ActorMangers.GetCount(); j++ )
			{
				CCutsceneAnimatedActorEntity* pActorManger = static_cast<CCutsceneAnimatedActorEntity*>(ActorMangers[j]);
				if( (pActorManger->GetCutfObject()->GetObjectId() == ObjectID)
				 && (pActorManger->GetCutfObject()->GetObjectId() == m_StreamingRequests[i]->GetObjectId()) )
				{
					UpdateVariationLoading(*m_StreamingRequests[i]);
				}
			}
		}
	}
}*/

 ///////////////////////////////////////////////////////////////////////////////////////////////////
 //Get an index into the compound entity

 s32 CCutSceneAssetMgrEntity::GetGameIndex(s32 iObjectId) const
 {
	 for(int i=0; i < m_StreamingRequests.GetCount(); i++)
	 {
		 if(m_StreamingRequests[i])		
		 {
			 if (iObjectId == m_StreamingRequests[i]->GetObjectId() )
			 {
				return m_StreamingRequests[i]->GetGameIndex();
			 }
		 }
	 }
	 return -1; 
 }


///////////////////////////////////////////////////////////////////////////////////////////////////
//Creates a modified object name if it contains a special character by removing it.

void CCutSceneAssetMgrEntity::GenerateModifiedModelName(const char* pModelName, char* ModelName, int iStringLength)
 {	
	strncpy(ModelName, pModelName, iStringLength);
	//put a terminating character where the 
	char *pChar = strrchr(ModelName, '~') ; 
	char *pSecondChar = strrchr(ModelName, '^') ; 

	if(pChar != NULL)
	{
		*pChar = '\0'; 
	}

	if(pSecondChar != NULL)
	{
		*pSecondChar = '\0';
	}

 }

//add a ref to the streaming we need to match the loads and unloads. 
bool CCutSceneAssetMgrEntity::ShouldAddRequestRefForObject(s32 ObjectId)
{
	for (int i = 0; i < m_StreamingRequests.GetCount(); i++ )		
	{
		if(m_StreamingRequests[i])
		{
			if(ObjectId == m_StreamingRequests[i]->GetObjectId())
			{
				cutsceneAssetMgrDebugf2( "ShouldAddRequestRefForObject: Adding ref! objectId(%d) ", ObjectId);
				m_StreamingRequests[i]->AddRef(); 
				return true; 
			}
		}
	}

	return false; 
}

bool CCutSceneAssetMgrEntity::ShouldAddRequestRefForObject(s32 ObjectId, atHashString mHash , eStreamingDataType dataType)
{
	for (int i = 0; i < m_StreamingRequests.GetCount(); i++ )		
	{
		if(m_StreamingRequests[i])
		{
			if(ObjectId == m_StreamingRequests[i]->GetObjectId() && mHash.GetHash() == m_StreamingRequests[i]->m_Hash.GetHash() && dataType == m_StreamingRequests[i]->GetDataType())
			{
				m_StreamingRequests[i]->AddRef(); 
				//cutsceneAssetMgrDebugf( "SetModelStreamingRequest: AddRef! objectId(%d) modelHash(%s) GetRef(%d) ", ObjectId, mHash.TryGetCStr(), m_StreamingRequests[i]->GetRef() );
				return true; 
			}
		}
	}

	//cutsceneAssetMgrDebugf( "SetModelStreamingRequest: No request yet! objectId(%d) modelHash(%s)", ObjectId, mHash.TryGetCStr() );

	return false; 
}

//For certain load requests they have no object id they should be unique so just check to see if we have one
bool CCutSceneAssetMgrEntity::ShouldAddRequestRefForObjectOfType(eStreamingDataType Type)
{
	for (int i = 0; i < m_StreamingRequests.GetCount(); i++ )		
	{
		if(m_StreamingRequests[i])
		{
			if(m_StreamingRequests[i]->GetDataType() == Type)
			{
				m_StreamingRequests[i]->AddRef(); 
				return true; 
			}
		}
	}

	return false; 
}

void CCutSceneAssetMgrEntity::SetModelStreamingRequest(atHashString ModelName, s32 ObjectId, eStreamingDataType Type)
{ 
	//check the streaming list for an object with the same id, if there is one in the list lets not make another request 
	if(ShouldAddRequestRefForObject(ObjectId, ModelName, Type))
	{
		return; 
	}

	fwModelId iModelId = CModelInfo::GetModelIdFromName(ModelName); 
	
	if (Verifyf( iModelId.IsValid() ,"Cutscene trying to load model: %s, but its not in the image.", ModelName.GetCStr()))
	{	
		CCutSceneSingleRequestStreamingInfo* StreamInfo = rage_new CCutSceneSingleRequestStreamingInfo(ObjectId, iModelId.GetModelIndex(), MODEL_TYPE, ModelName);
		cutsceneAssetMgrDebugf2( "SetModelStreamingRequest: Creating request! objectId(%d) modelHash(%s) ", ObjectId, ModelName.TryGetCStr());
		m_StreamingRequests.PushAndGrow(StreamInfo); 

		strLocalIndex transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId);
		StreamInfo->GetRequest()->Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);	
	}
}

void CCutSceneAssetMgrEntity::SetItypeModelStreamingRequest(atHashString ModelName, atHashString Itype, s32 ObjectId)
{ 
	//check the streaming list for an object with the same id, if there is one in the list lets not make another request 
	atHashString mHash(ModelName.GetHash());  
	if(ShouldAddRequestRefForObject(ObjectId, mHash, MODEL_ITYPE_TYPE))
	{
		return; 
	}
	
	strLocalIndex itypSlotIdx;

	itypSlotIdx = g_MapTypesStore.FindSlotFromHashKey(Itype);

	if (!itypSlotIdx.IsValid())
	{
		cutsceneWarningf("strModelRequest for (%s:%s) contains an unrecognised .ityp file, will treat it as global...", Itype.GetCStr(), ModelName.GetCStr() );
	}
		
	CCutsceneModelRequestStreamingInfo* StreamInfo = rage_new CCutsceneModelRequestStreamingInfo(ObjectId, NULL_STREAMING_INDEX, MODEL_ITYPE_TYPE, mHash);

	m_StreamingRequests.PushAndGrow(StreamInfo); 

	StreamInfo->GetRequest()->Request(itypSlotIdx, ModelName, STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);	
}


//Removing the flag will remove it for the whole slot. So check if there are any others with the same streaming index
bool CCutSceneAssetMgrEntity::CanRemoveCutSceneRequiredFlag(s32 ModuleIndex)
{
	u32 NumOfEntitiesWithTheSameModel = 0; 
	for(int i=0; i < m_StreamingRequests.GetCount(); i++)
	{
		if(	m_StreamingRequests[i] && m_StreamingRequests[i]->GetGameIndex() == ModuleIndex)
		{
			NumOfEntitiesWithTheSameModel++; 
		}
	}

	if(NumOfEntitiesWithTheSameModel > 1)
	{
		return false;
	}

	return true; 
}


void CCutSceneAssetMgrEntity::RemoveStreamingRequest(s32 objectId, bool RespectRefs, eStreamingDataType dataType, atHashString Modelhash)
{
	for(int i = m_StreamingRequests.GetCount()-1; i >= 0 ; i--)
	{
		if(m_StreamingRequests[i] && m_StreamingRequests[i]->GetObjectId() == objectId 
		&& m_StreamingRequests[i]->GetDataType() == dataType 
		&& m_StreamingRequests[i]->m_Hash.GetHash() == Modelhash.GetHash())
		{
			m_StreamingRequests[i]->RemoveRef(); 
			
			cutsceneAssetMgrDebugf2( "RemoveStreamingRequest: RemoveRef! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", objectId, Modelhash.TryGetCStr(), m_StreamingRequests[i]->GetRef(), RespectRefs ? "TRUE" : "FALSE"  );

			if(m_StreamingRequests[i]->GetRef() == 0 || RespectRefs == false)
			{			
				// Replaced this function as it didn't seem to work
				// Now we walk over related streaming requests at the end of this loop and reapply the flag
				//if(CanRemoveCutSceneRequiredFlag(m_StreamingRequests[i]->GetGameIndex()))
				{
					m_StreamingRequests[i]->ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);
				}

				// Keep track of the game index so we can walk over related streaming requests at the end of this loop and reapply the flag
				s32 iGameIndex = m_StreamingRequests[i]->GetGameIndex();

				ReleaseStreamingRequests(m_StreamingRequests[i]);
				cutsceneAssetMgrDebugf2( "RemoveStreamingRequest: Deleting request! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", objectId, Modelhash.TryGetCStr(), m_StreamingRequests[i]->GetRef(), RespectRefs ? "TRUE" : "FALSE" );
				delete m_StreamingRequests[i]; 
				
				m_StreamingRequests[i] = NULL; 

				//delete a array element with the model
				m_StreamingRequests.Delete(i);

				// Add back the cutscene required flags to anything using the same type and slot index
				for(int j = 0; j < m_StreamingRequests.GetCount(); j++)
				{
					if((m_StreamingRequests[j]->GetDataType() == dataType) && (m_StreamingRequests[j]->GetGameIndex() == iGameIndex))
					{
						m_StreamingRequests[j]->SetRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);
					}
				}
			}
		}
	}
}

void CCutSceneAssetMgrEntity::RemoveStreamingRequest(s32 index)
{
	cutsceneAssetMgrDebugf2( "RemoveStreamingRequest (by index): Deleting request! objectId(%d) modelHash(%s) GetRef(%d)", m_StreamingRequests[index]->GetObjectId(), m_StreamingRequests[index]->GetHashString()->TryGetCStr(), m_StreamingRequests[index]->GetRef());
	m_StreamingRequests[index]->ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED); 
	ReleaseStreamingRequests(m_StreamingRequests[index]);

	delete m_StreamingRequests[index]; 

	m_StreamingRequests[index] = NULL; 


	//delete a array element with the model
	m_StreamingRequests.Delete(index);
}


void CCutSceneAssetMgrEntity::RemoveStreamingRequest(s32 objectId, bool RespectRefs, eStreamingDataType dataType)
{
	for(int i = m_StreamingRequests.GetCount()-1; i >= 0 ; i--)
	{
		if(m_StreamingRequests[i] && m_StreamingRequests[i]->GetObjectId() == objectId && 
			m_StreamingRequests[i]->GetDataType() == dataType)
		{
			cutsceneAssetMgrDebugf2( "RemoveStreamingRequest (nmh): RemoveRef! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", objectId, m_StreamingRequests[i]->GetHashString()->TryGetCStr(), m_StreamingRequests[i]->GetRef(), RespectRefs ? "TRUE" : "FALSE"  );
			m_StreamingRequests[i]->RemoveRef(); 

			if(m_StreamingRequests[i]->GetRef() == 0 || RespectRefs == false)
			{
				cutsceneAssetMgrDebugf2( "RemoveStreamingRequest (nmh): Deleting request! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", objectId, m_StreamingRequests[i]->GetHashString()->TryGetCStr(), m_StreamingRequests[i]->GetRef(), RespectRefs ? "TRUE" : "FALSE" );
				// Replaced this function as it didn't seem to work
				// Now we walk over related streaming requests at the end of this loop and reapply the flag
				//if(CanRemoveCutSceneRequiredFlag(m_StreamingRequests[i]->GetGameIndex()))
				{
					m_StreamingRequests[i]->ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);
				}

				// Keep track of the game index so we can walk over related streaming requests at the end of this loop and reapply the flag
				s32 iGameIndex = m_StreamingRequests[i]->GetGameIndex();

				ReleaseStreamingRequests(m_StreamingRequests[i]);

				delete m_StreamingRequests[i]; 

				m_StreamingRequests[i] = NULL; 

				//delete a array element with the model
				m_StreamingRequests.Delete(i);

				// Add back the cutscene required flags to anything using the same type and slot index
				for(int j = 0; j < m_StreamingRequests.GetCount(); j++)
				{
					if((m_StreamingRequests[j]->GetDataType() == dataType) && (m_StreamingRequests[j]->GetGameIndex() == iGameIndex))
					{
						m_StreamingRequests[j]->SetRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);
					}
				}
			}
		}
	}
}

void CCutSceneAssetMgrEntity::RemoveAllStreamingRequestsOfType(eStreamingDataType type, bool RespectRefs)
{
	for(int i = m_StreamingRequests.GetCount()-1; i >= 0 ; i--)
	{
		if(m_StreamingRequests[i] && m_StreamingRequests[i]->GetDataType() == type )
		{
			cutsceneAssetMgrDebugf2( "RemoveAllStreamingRequestsOfType: RemoveRef! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", m_StreamingRequests[i]->GetObjectId(), m_StreamingRequests[i]->GetHashString()->TryGetCStr(), m_StreamingRequests[i]->GetRef(), RespectRefs ? "TRUE" : "FALSE"  );
			m_StreamingRequests[i]->RemoveRef();

			if(m_StreamingRequests[i]->GetRef() == 0 || RespectRefs == false)
			{
				cutsceneAssetMgrDebugf2( "RemoveAllStreamingRequestsOfType: Deleting Request! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", m_StreamingRequests[i]->GetObjectId(), m_StreamingRequests[i]->GetHashString()->TryGetCStr(), m_StreamingRequests[i]->GetRef(), RespectRefs ? "TRUE" : "FALSE"  );
				m_StreamingRequests[i]->ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);

				ReleaseStreamingRequests(m_StreamingRequests[i]);

				delete m_StreamingRequests[i]; 

				m_StreamingRequests[i] = NULL; 

				//delete a array element with the model
				m_StreamingRequests.Delete(i);
			}
		}
	}
}

void CCutSceneAssetMgrEntity::RemoveAllStreamingRequests()
{
	for(int i = m_StreamingRequests.GetCount()-1; i >= 0 ; i--)
	{
		if(m_StreamingRequests[i] && m_StreamingRequests[i]->GetDataType() != MODEL_ITYPE_TYPE)
		{
			cutsceneAssetMgrDebugf2( "RemoveAllStreamingRequests: RemoveRef! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", m_StreamingRequests[i]->GetObjectId(), m_StreamingRequests[i]->GetHashString()->TryGetCStr(), m_StreamingRequests[i]->GetRef(), "FALSE"  );

			m_StreamingRequests[i]->RemoveRef();
			m_StreamingRequests[i]->ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);

			cutsceneAssetMgrDebugf2( "RemoveAllStreamingRequests:  Deleting Request! objectId(%d) modelHash(%s) GetRef(%d) RespectRefs(%s) ", m_StreamingRequests[i]->GetObjectId(), m_StreamingRequests[i]->GetHashString()->TryGetCStr(), m_StreamingRequests[i]->GetRef(), "FALSE"  );

			ReleaseStreamingRequests(m_StreamingRequests[i]);

			delete m_StreamingRequests[i]; 

			m_StreamingRequests[i] = NULL; 

			//delete a array element with the model
			m_StreamingRequests.Delete(i);
		}
	}

	for(int i = m_StreamingRequests.GetCount()-1; i >= 0 ; i--)
	{
		if(m_StreamingRequests[i])	
		{
			m_StreamingRequests[i]->RemoveRef();
			m_StreamingRequests[i]->ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);
			
			ReleaseStreamingRequests(m_StreamingRequests[i]);

			delete m_StreamingRequests[i]; 

			m_StreamingRequests[i] = NULL; 

			//delete a array element with the model
			m_StreamingRequests.Delete(i);
		}
	}
}
	
void CCutSceneAssetMgrEntity::ReleaseStreamingRequests(CCutSceneStreamingInfo* StreaminRequest)
{
	if(StreaminRequest)
	{
		switch (StreaminRequest->GetDataType())
		{
		case MODEL_TYPE:
		case PED_VARIATION_TYPE:
			{
				StreaminRequest->Release();
			}
			break; 

		case SCALEFORM_OVERLAY_TYPE:
			{
				if (CScaleformMgr::IsMovieActive(StreaminRequest->GetGameIndex()))  // only request to remove a movie if its active
				{
					CScaleformMgr::RequestRemoveMovie(StreaminRequest->GetGameIndex());
				}
			}
			break; 

		case BINK_OVERLAY_TYPE:
			{
				g_movieMgr.Release(StreaminRequest->GetGameIndex());
			}
			break; 
		
		case INTERIOR_TYPE:
			{
				CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetAt(StreaminRequest->m_Index);
				Assert(pIntProxy);

				if (cutsceneVerifyf(pIntProxy, "Interior proxy doesn't exist"))
				{
					pIntProxy->SetRequestedState( CInteriorProxy::RM_CUTSCENE, CInteriorProxy::PS_NONE);
				}
			}
			break; 

		case PTFX_TYPE:
			{
				strLocalIndex slot = ptfxManager::GetAssetStore().FindSlot(StreaminRequest->GetHashString()->GetHash());

				if( cutsceneVerifyf( slot.IsValid(), "PTFX dictionary %s does not exist", StreaminRequest->GetHashString()->GetCStr()))
				{
					//ptfxManager::GetAssetStore().StreamingRequest(slot, STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);
					ptfxManager::GetAssetStore().ClearRequiredFlag(slot.Get(), STRFLAG_CUTSCENE_REQUIRED);
					//ptfxManager::GetAssetStore().StreamingRemove(slot); 
				}		
			}
			break; 
		default:
			break;
	
		}
	}
}


s32 CCutSceneAssetMgrEntity::RequestRayFireObject(cutsManager* pManager, u32 ObjectId)
{
	s32 iStreamingId = NULL_RAYFIRE_STREAMING_INDEX; 
	
	const cutfObject* pTempObject = pManager->GetObjectById(ObjectId);		//list of object to load 
	if ( pTempObject && pTempObject->GetType() == CUTSCENE_RAYFIRE_OBJECT_TYPE )
	{   
		const cutfRayfireObject* pRayfireObject = static_cast<const cutfRayfireObject*>(pTempObject);	

		Vector3 vPos = pManager->GetSceneOffset() + pRayfireObject->GetStartPosition(); 
	
		if(pRayfireObject->GetStreamingName().GetHash() != 0)
		{
			iStreamingId = object_commands::CommandGetCompositeEntityAtInternal(vPos, 100.0f, pRayfireObject->GetStreamingName() ); 
		}
		else
		{
			cutsceneAssertf(0,"Rayfire %s has no model streaming name, this will not load", pRayfireObject->GetDisplayName().c_str());
		}
	}
	return iStreamingId; 
}

void CCutSceneAssetMgrEntity::SetScaleformStreamingRequest(const cutfObject* pLoadObject)
{
	if ( pLoadObject )
	{  
		if(ShouldAddRequestRefForObject(pLoadObject->GetObjectId()))
		{
			return; 
		}

		s32 iStreamingId = -1; 
		Vector2 vPos, vSize; 
		vPos = Vector2(0.0f, 0.0f); 
		vSize = Vector2(1.0f, 1.0f); 


		char ModelName[CUTSCENE_OBJNAMELEN]; 

		GenerateModifiedModelName(pLoadObject->GetDisplayName().c_str(), ModelName, CUTSCENE_OBJNAMELEN); 
		iStreamingId = CScaleformMgr::CreateMovie(ModelName,vPos,vSize); 

		if(iStreamingId != -1)
		{
			atHashString mHash(pLoadObject->GetDisplayName().c_str()); 

			CCutSceneSingleRequestStreamingInfo* StreamInfo = rage_new CCutSceneSingleRequestStreamingInfo(pLoadObject->GetObjectId(), iStreamingId, SCALEFORM_OVERLAY_TYPE, mHash);
			
			m_StreamingRequests.PushAndGrow(StreamInfo); 
		}
	}
}

void CCutSceneAssetMgrEntity::SetBinkStreamingRequest(const cutfOverlayObject* pLoadObject)
{
	if ( pLoadObject )
	{  
		if(ShouldAddRequestRefForObject(pLoadObject->GetObjectId()))
		{
			return; 
		}
		
		u32 movieHandle = g_movieMgr.PreloadMovie(pLoadObject->GetDisplayName());

		if(movieHandle != 0)
		{

			atHashString mHash(pLoadObject->GetDisplayName().c_str()); 

			CCutSceneSingleRequestStreamingInfo* StreamInfo = rage_new CCutSceneSingleRequestStreamingInfo(pLoadObject->GetObjectId(), movieHandle, BINK_OVERLAY_TYPE, mHash);
			
			m_StreamingRequests.PushAndGrow(StreamInfo); 

		}
	}
}


void CCutSceneAssetMgrEntity::SetSubtitleStreamingRequest(const char* TextBlockName, eStreamingDataType Type )
{
	if (ShouldAddRequestRefForObjectOfType(Type))
	{
		return; 
	}

	safecpy( m_TextBlockName, TextBlockName, sizeof(m_TextBlockName) );

	//the mission dialogue text is not load so load it  
	if (!TheText.HasThisAdditionalTextLoaded(m_TextBlockName, MISSION_DIALOGUE_TEXT_SLOT)) 
	{
		TheText.RequestAdditionalText(m_TextBlockName, MISSION_DIALOGUE_TEXT_SLOT, CTextFile::TEXT_LOCATION_CHECK_BOTH_GTA5_AND_DLC);

		atHashString mHash(TextBlockName); 

		CCutSceneSingleRequestStreamingInfo* StreamInfo = rage_new CCutSceneSingleRequestStreamingInfo(-1, -1, SUBTITLES_TYPE, mHash);

		m_StreamingRequests.PushAndGrow(StreamInfo); 

		//m_bHasSubtitlesBeenRequested = true; 

	}
}

void CCutSceneAssetMgrEntity::SetParticleFxStreamingRequest(cutsManager* pManager, const cutfObject* pObject, eStreamingDataType Type)
{
	if (ShouldAddRequestRefForObjectOfType(Type))
	{
		return; 
	}
	
	const cutfCutsceneFile2* pCutfile = const_cast<const cutsManager*>(pManager)->GetCutfFile(); 

	// check if there are any particle effects in this cutscene
	atArray<cutfObject *> PtfxList;
	pCutfile->FindObjectsOfType(CUTSCENE_ANIMATED_PARTICLE_EFFECT_OBJECT_TYPE, PtfxList);
	pCutfile->FindObjectsOfType(CUTSCENE_PARTICLE_EFFECT_OBJECT_TYPE, PtfxList);
	if (PtfxList.GetCount()>0)
	{
		// there are - query the name of this cutscene's particle asset 
		const char* pCutsceneName = pManager->GetCutsceneName();
		atHashWithStringNotFinal ptFxAssetHashName = g_vfxScript.GetCutscenePtFxAssetName(pCutsceneName);

		// check that there is a particle asset set up for this cutscene
		if (Verifyf(ptFxAssetHashName.GetHash(), "cutscene %s has particle effects but no particle asset set up - can't request the asset, check with ptfx dept", pCutsceneName))
		{
			// get the asset slot
			strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(ptFxAssetHashName.GetHash());
			if (Verifyf(slot.IsValid(), "%s is trying to load a PTFX list %s which doesnt exist", pCutsceneName, ptFxAssetHashName.GetCStr()))
			{
				// request the asset is streamed in
				CCutSceneSingleRequestStreamingInfo* StreamInfo = rage_new CCutSceneSingleRequestStreamingInfo(pObject->GetObjectId(), slot.Get(), PTFX_TYPE, ptFxAssetHashName);

				m_StreamingRequests.PushAndGrow(StreamInfo); 

				ptfxManager::GetAssetStore().StreamingRequest(slot, STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);
			}
		}
	}
}

eStreamingRequestReturnValue CCutSceneAssetMgrEntity::SetActorPropVariationStreamingRequest(u32 iComp, u32 propID, u32 texID, atHashString& ModelName, u32 ObjectId)
{
	//add a ref to our streaming object if we make another request to it. 
	s32 streamIndex = GetVariationStreamingIndexFromRequestList(iComp, propID, texID, ModelName, ObjectId); 
	if( streamIndex!= -1)
	{
		m_StreamingRequests[streamIndex]->AddRef(); 
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Already %s - %s - (Component: %d (%s), propID: %d, texID: %d)", m_StreamingRequests[streamIndex]->HasLoaded() ? "Loaded" : "Loading", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		return ESRRV_ALREADY_REQUESTED; 
	}

	//convert to anchor id
	s32 anchor = iComp - PV_MAX_COMP; 

	if(anchor <= ANCHOR_NONE || anchor >= NUM_ANCHORS)
	{
		anchor = ANCHOR_NONE; 
	}

	eAnchorPoints anchorID = (eAnchorPoints)anchor;  

	fwModelId iModelId = CModelInfo::GetModelIdFromName(ModelName); 


	CBaseModelInfo *pBaseModelInfo = CModelInfo::GetBaseModelInfo(iModelId);
	if(!pBaseModelInfo)
	{
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Invalid CBaseModelInfo - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		return ESRRV_INVALID_VARIATION;
	}

	if (pBaseModelInfo->GetModelType() != MI_TYPE_PED )
	{
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Not a CPedModelInfo - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		return ESRRV_INVALID_VARIATION;
	}

	CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pBaseModelInfo); 		
	if(!pPedModelInfo)
	{
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Invalid CPedModelInfo - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		//cutsceneAssert(0);
		return ESRRV_INVALID_VARIATION; 
	}

	if(!pPedModelInfo->GetIsStreamedGfx())
	{
		cutsceneAssetMgrDebugf3("SetActorPropVariationStreamingRequest - Not a streamed ped : %s",  ModelName.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	if(!pPedModelInfo->GetVarInfo())
	{
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Invalid CPedVariationInfoCollection - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		//cutsceneAssert(0);
		return ESRRV_INVALID_VARIATION; 
	}

	Assertf(anchorID < NUM_ANCHORS, "Invalid anchor id %d used to set prop on ped '%s'", anchorID,ModelName.GetCStr());
	ASSERT_ONLY(const u32 maxNumTex = pPedModelInfo->GetVarInfo()->GetMaxNumPropsTex(anchorID, propID);)
	Assertf(texID < maxNumTex, "Tried to set texture index %d on %s_%03d. Maximum texture allowed is %d on ped '%s'", texID, propSlotNames[anchorID], propID, maxNumTex, pPedModelInfo->GetModelName());

	if(anchorID != ANCHOR_NONE && (propID >= pPedModelInfo->GetVarInfo()->GetMaxNumProps(anchorID)))
	{
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Invalid propID - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		return ESRRV_INVALID_VARIATION;
	}

	if (anchorID == ANCHOR_NONE)
	{
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Invalid anchorID - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		Assertf(0, "CPedPropData::SetPedProp - shouldn't call this with ANCHOR_NONE. Maybe try CPedPropData::ClearPedProp");
		return ESRRV_INVALID_VARIATION;
	}


	//need to generate the name of the desired prop from slot and prop index
#if __ASSERT
	char sourceName[80];
#endif // __ASSERT

	s32 texIdx = texID;
	if (texIdx == -1)
	{
		texIdx = 0;
	}

	u32 streamingPropHash = 0;
	u32 streamingTxdHash = 0;

	if (pPedModelInfo->GetIsStreamedGfx())
	{
		u32 propFolderHash = 0;
		propFolderHash = atPartialStringHash(pPedModelInfo->GetPropStreamFolder(), propFolderHash);
		propFolderHash = atPartialStringHash("/", propFolderHash);
		propFolderHash = atPartialStringHash(propSlotNames[anchorID], propFolderHash);
		propFolderHash = atPartialStringHash("_", propFolderHash);

		char propIdStr[4] = {0};
		streamingPropHash = atPartialStringHash(CPedVariationStream::CustomIToA3(propID, propIdStr), propFolderHash);
		streamingPropHash = atFinalizeHash(streamingPropHash);

		ASSERT_ONLY(sprintf(sourceName,"%s/%s_%03d",pPedModelInfo->GetPropStreamFolder(), propSlotNames[anchorID], propID));
		Assertf(streamingPropHash == atStringHash(sourceName), "Prop .dwd '%s' hash not what expected: %d == %d", sourceName, streamingPropHash, atStringHash(sourceName));

		char textureIdStr[2] = {0};
		textureIdStr[0] = (char)texIdx+'a';
		textureIdStr[1] = '\0';

		streamingTxdHash = atPartialStringHash("diff_", propFolderHash);
		streamingTxdHash = atPartialStringHash(propIdStr, streamingTxdHash);
		streamingTxdHash = atPartialStringHash("_", streamingTxdHash);
		streamingTxdHash = atPartialStringHash(textureIdStr, streamingTxdHash);
		streamingTxdHash = atFinalizeHash(streamingTxdHash);

		ASSERT_ONLY(sprintf(sourceName, "%s/%s_diff_%03d_%c", pPedModelInfo->GetPropStreamFolder(), propSlotNames[anchorID], propID, (texIdx+'a')));
		Assertf(streamingTxdHash == atStringHash(sourceName), "Prop .txd '%s' hash not what expected: %d == %d", sourceName, streamingTxdHash, atStringHash(sourceName));
	}

	strLocalIndex txdSlotId = g_TxdStore.FindSlot(streamingTxdHash);
	strLocalIndex dwdSlotId = strLocalIndex(g_DwdStore.FindSlot(streamingPropHash));

	if(cutsceneVerifyf(txdSlotId.IsValid(),"Streamed .txd request for ped '%s' is not found : %s", ModelName.GetCStr(), sourceName ))
	{
		if(cutsceneVerifyf(dwdSlotId.IsValid(),"Streamed .dwd request for ped '%s' is not found : %s", ModelName.GetCStr(), sourceName))
		{
			atHashString mModelHash(ModelName); 
			CCutsceneVariationStreamingInfo* pInfo = rage_new CCutsceneVariationStreamingInfo(); 
			g_DwdStore.SetParentTxdForSlot(dwdSlotId, strLocalIndex(CPedModelInfo::GetCommonTxd()));  
			pInfo->m_request.SetRequest(TEXTURE_SLOT, txdSlotId.Get(), g_TxdStore.GetStreamingModuleId(), STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);
			pInfo->m_request.SetRequest(DRAWABLE_SLOT,dwdSlotId.Get(),  g_DwdStore.GetStreamingModuleId(), STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);
			pInfo->m_ObjectId = ObjectId; 
			pInfo->m_Hash = mModelHash; 
			pInfo->m_Index = iComp; 
			pInfo->m_Component = iComp; 
			pInfo->m_Drawable = propID; 
			pInfo->m_Texture = texID; 
			pInfo->m_DataType = PED_VARIATION_TYPE; 

			m_StreamingRequests.PushAndGrow(pInfo); 
		}
		else
		{
			cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Invalid drawable slot - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
			return ESRRV_INVALID_VARIATION;
		}
	}
	else
	{
		cutsceneAssetMgrDebugf3( "SetActorPropVariationStreamingRequest: Invalid texture slot - %s - (Component: %d (%s), propID: %d, texID: %d)", ModelName.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), propID, texID);
		return ESRRV_INVALID_VARIATION;
	}

	return ESRRV_SUCCESSFULL_REQUEST;
};

void CCutSceneAssetMgrEntity::SetInteriorStreamingRequest(s32 ObjectId,  s32 StreamingIndex, atHashString InteriorName, float fTime)
{
	//have one so just add ref
	if(ShouldAddRequestRefForObject(ObjectId, InteriorName, INTERIOR_TYPE))
	{
		//need to update the time of teh event to ensure we dont remove it too early remove 
		for (int i = 0; i < m_StreamingRequests.GetCount(); i++ )		
		{
			if(m_StreamingRequests[i])
			{
				if(m_StreamingRequests[i]->GetDataType() == INTERIOR_TYPE && m_StreamingRequests[i]->GetObjectId() == ObjectId && 
					m_StreamingRequests[i]->GetGameIndex() == StreamingIndex && InteriorName.GetHash() == m_StreamingRequests[i]->GetHashString()->GetHash())
				{
					m_StreamingRequests[i]->m_LoadTime = fTime;
				}
			}
		}
		return; 
	}
	
	
	CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(StreamingIndex);
	
	if(pIntProxy)
	{
		pIntProxy->SetRequestedState(CInteriorProxy::RM_CUTSCENE, CInteriorProxy::PS_FULL_WITH_COLLISIONS);


		CCutSceneSingleRequestStreamingInfo* StreamInfo = rage_new CCutSceneSingleRequestStreamingInfo(ObjectId, StreamingIndex, INTERIOR_TYPE, InteriorName);
		StreamInfo->m_LoadTime = fTime; 

		m_StreamingRequests.PushAndGrow(StreamInfo);
	}
}

eStreamingRequestReturnValue CCutSceneAssetMgrEntity::SetActorVariationStreamingRequest(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, u32 ObjectId)
{
	if(iComp < PV_MAX_COMP)
	{
		return SetActorComponentVariationStreamingRequest(iComp,  iDrawable,  iTexture,  ModelName, ObjectId);
	}
	else
	{
		return SetActorPropVariationStreamingRequest(iComp,  iDrawable, iTexture, ModelName, ObjectId); 
	}
}



eStreamingRequestReturnValue CCutSceneAssetMgrEntity::SetActorComponentVariationStreamingRequest(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelNameHash, u32 ObjectId)
{
	if(!cutsceneVerifyf(iComp < PV_MAX_COMP, "Component value %d on %s is out of valid range 0-PV_MAX_COMP, wont apply component", iComp, ModelNameHash.TryGetCStr()))
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Invalid component : %s",  ModelNameHash.TryGetCStr() );
		return ESRRV_INVALID_COMPONENT; 
	}
	
	//add a ref to our streaming object if we make another request to it. 
	s32 streamIndex = GetVariationStreamingIndexFromRequestList(iComp, iDrawable,  iTexture, ModelNameHash, ObjectId); 
	if( streamIndex!= -1)
	{
		m_StreamingRequests[streamIndex]->AddRef(); 
		cutsceneAssetMgrDebugf3( "SetActorComponentVariationStreamingRequest: Already %s - %s - (Component: %d (%s), Drawable: %d, Texture: %d)", m_StreamingRequests[streamIndex]->HasLoaded() ? "Loaded" : "Loading", ModelNameHash.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), iDrawable, iTexture);
		return ESRRV_ALREADY_REQUESTED; 
	}

	// Make sure this is really a streamed model (with special data) otherwise don't do anything special.
	fwModelId iModelId = CModelInfo::GetModelIdFromName(ModelNameHash); 

	CBaseModelInfo *pBaseModelInfo = CModelInfo::GetBaseModelInfo(iModelId);
	if(!pBaseModelInfo)
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Invalid CBaseModelInfo : %s",  ModelNameHash.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	if (pBaseModelInfo->GetModelType() != MI_TYPE_PED )
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Not a CPedModelInfo : %s",  ModelNameHash.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pBaseModelInfo); 
	if (!pPedModelInfo)
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Invalid CPedModelInfo : %s",  ModelNameHash.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	if (!pPedModelInfo->GetIsStreamedGfx())
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Not a streamed ped : %s",  ModelNameHash.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	CPedVariationInfoCollection* pVarInfo = pPedModelInfo->GetVarInfo();	
	if(!pVarInfo)
	{	
		cutsceneAssetMgrDebugf3( "SetActorComponentVariationStreamingRequest: Invalid CPedVariationInfoCollection - %s - (Component: %d (%s), Drawable: %d, Texture: %d)", ModelNameHash.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), iDrawable, iTexture);
		//cutsceneAssert(0);
		return ESRRV_INVALID_VARIATION; 
	}
	
	CPVDrawblData* pDrawblData = pVarInfo->GetCollectionDrawable(iComp, iDrawable);
	if (!pDrawblData)
	{
		cutsceneAssetMgrDebugf3( "SetActorComponentVariationStreamingRequest: Invalid CPVDrawblData - %s - (Component: %d (%s), Drawable: %d, Texture: %d)", ModelNameHash.TryGetCStr(), iComp, CPedVariationData::GetVarOrPropSlotName(iComp), iDrawable, iTexture);
		//cutsceneAssert(0);
		return ESRRV_INVALID_VARIATION;
	}
	
	//
	// generate the name of the drawable to extract
	//
#if __ASSERT
	char drawablName[STORE_NAME_LENGTH];
	char diffMap[STORE_NAME_LENGTH];
#endif // __ASSERT

	const char* pedFolder = pPedModelInfo->GetStreamFolder();
	const char* dlcName = pVarInfo->GetDlcNameFromDrawableIdx(varSlotEnums[iComp],iDrawable);

	u32 folderHash = 0;
	folderHash = atPartialStringHash(pedFolder, folderHash);
	if(dlcName)
	{
		folderHash = atPartialStringHash("_", folderHash);
		folderHash = atPartialStringHash(dlcName, folderHash);
	}
	folderHash = atPartialStringHash("/", folderHash);
	folderHash = atPartialStringHash(varSlotNames[iComp], folderHash);
	folderHash = atPartialStringHash("_", folderHash);

	char raceModifier[] = "_u";
	if (pDrawblData->IsMatchingPrev() == true)
	{
		raceModifier[1] = 'm';
	}
	else if (pDrawblData->IsUsingRaceTex() == true)
	{
		raceModifier[1] = 'r';
	}

	char drawableIdStr[4] = {0};
	s32 localDrawIdx = pVarInfo->GetDlcDrawableIdx((ePedVarComp)iComp, iDrawable);
	u32 drawableHash = atPartialStringHash(CPedVariationStream::CustomIToA3(localDrawIdx, drawableIdStr), folderHash);

	drawableHash = atPartialStringHash(raceModifier, drawableHash);
	drawableHash = atFinalizeHash(drawableHash);

#if __ASSERT
	if(dlcName)
	{
		sprintf(drawablName, "%s_%s/%s_%03d%s", pedFolder, dlcName, varSlotNames[iComp], localDrawIdx, raceModifier);
	}
	else
	{
		sprintf(drawablName, "%s/%s_%03d%s", pedFolder, varSlotNames[iComp], localDrawIdx, raceModifier);
	}
#endif // __ASSERT

	Assertf(drawableHash == atStringHash(drawablName), "Drawable asset '%s' hash not what expected: %d == %d", drawablName, drawableHash, atStringHash(drawablName));

	if (!Verifyf(iTexture < pDrawblData->GetNumTex(), "Selected texture '%c' doesn't exists for '%s_%03d' component on ped '%s'. Is it present in meta data?", 'a' + iTexture, varSlotNames[iComp], iDrawable, ModelNameHash.TryGetCStr()))
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Invalid texture 6: %s",  ModelNameHash.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	u8 raceIdx = pDrawblData->GetTexData(iTexture);
	if (!cutsceneVerifyf(raceIdx != (u8)PV_RACE_INVALID, "Bad race index for drawable %s with texIdx %d on ped %s", drawablName, iTexture, ModelNameHash.TryGetCStr()))
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Incompatable race : %s",  ModelNameHash.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	// generate the name of the textures required
	char textureIdStr[2] = {0};
	textureIdStr[0] = (char)iTexture+'a';
	textureIdStr[1] = '\0';

	u32 diffMapHash = atPartialStringHash("diff_", folderHash);
	diffMapHash = atPartialStringHash(drawableIdStr, diffMapHash);
	diffMapHash = atPartialStringHash("_", diffMapHash);
	diffMapHash = atPartialStringHash(textureIdStr, diffMapHash);
	diffMapHash = atPartialStringHash("_", diffMapHash);
	diffMapHash = atPartialStringHash(raceTypeNames[raceIdx], diffMapHash);
	diffMapHash = atFinalizeHash(diffMapHash);

#if __ASSERT
	if(dlcName)
	{
		sprintf(diffMap, "%s_%s/%s_diff_%03d_%c_%s", pedFolder, dlcName, varSlotNames[iComp], localDrawIdx, (iTexture+'a'), raceTypeNames[raceIdx]);
	}
	else
	{
		sprintf(diffMap, "%s/%s_diff_%03d_%c_%s", pedFolder, varSlotNames[iComp], localDrawIdx, (iTexture+'a'), raceTypeNames[raceIdx]);
	}
#endif // __ASSERT
	
	Assertf(diffMapHash == atStringHash(diffMap), "Diff Map '%s' hash not what expected: %d == %d", diffMap, diffMapHash, atStringHash(diffMap));
	
	strLocalIndex txdSlotId = g_TxdStore.FindSlotFromHashKey(diffMapHash);
	
	strLocalIndex dwdSlotId = strLocalIndex(g_DwdStore.FindSlotFromHashKey(drawableHash));

	if(cutsceneVerifyf(txdSlotId.IsValid(),"Streamed .txd request for ped '%s' is not found : %s", ModelNameHash.TryGetCStr(), diffMap))
	{
		if(cutsceneVerifyf(dwdSlotId.IsValid(),"Streamed .dwd request for ped '%s' is not found : %s", ModelNameHash.TryGetCStr(), diffMap))
		{
			CCutsceneVariationStreamingInfo* pInfo = rage_new CCutsceneVariationStreamingInfo(); 
			g_DwdStore.SetParentTxdForSlot(dwdSlotId, strLocalIndex(CPedModelInfo::GetCommonTxd()));  
			pInfo->m_request.SetRequest(TEXTURE_SLOT, txdSlotId.Get(), g_TxdStore.GetStreamingModuleId(), STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);
			pInfo->m_request.SetRequest(DRAWABLE_SLOT,dwdSlotId.Get(),  g_DwdStore.GetStreamingModuleId(), STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);
			pInfo->m_ObjectId = ObjectId; 
			pInfo->m_Hash = ModelNameHash; 
			pInfo->m_Index = iComp; 
			pInfo->m_Component = iComp; 
			pInfo->m_Drawable = iDrawable; 
			pInfo->m_Texture = iTexture; 
			pInfo->m_DataType = PED_VARIATION_TYPE; 
			
			if( pDrawblData->m_clothData.HasCloth() )
			{
				strLocalIndex cldSlotId = g_ClothStore.FindSlotFromHashKey(drawableHash);

				if(cldSlotId.IsValid())
				{
					pInfo->m_request.SetRequest(CLOTH_SLOT,cldSlotId.Get(),  g_ClothStore.GetStreamingModuleId(),STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);
				}
				else
				{
					cutsceneAssertf(false,"Streamed .cloth request for ped '%s' is not found : %s", ModelNameHash.TryGetCStr(), drawablName);
				}
			}
			
			m_StreamingRequests.PushAndGrow(pInfo); 
			cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Adding streaming request : %s",  pInfo->GetDebugString().c_str());
		}
		else
		{
			cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Invalid drawable : %s", ModelNameHash.TryGetCStr());
			return ESRRV_INVALID_VARIATION;
		}
	}
	else
	{
		cutsceneAssetMgrDebugf3("SetActorComponentVariationStreamingRequest - Invalid Texture : %s", ModelNameHash.TryGetCStr());
		return ESRRV_INVALID_VARIATION;
	}

	return ESRRV_SUCCESSFULL_REQUEST;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneAssetMgrEntity::DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float fTime, const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
	case CUTSCENE_LOAD_SCENE_EVENT:
		{
			pManager->SetIsLoading( pObject->GetObjectId(), true );
			//possibly warp player 
		}
		break;

	case CutSceneCustomEvents::CUTSCENE_LOAD_INTERIOR_EVENT:
		{
			pManager->SetIsLoading( pObject->GetObjectId(), true );
			
			char interiorName[64]; 

			const char* pInteriorName =	pEventArgs->GetAttributeList().FindAttributeStringValue("InteriorName", "",interiorName, 64); 
			
			Vector3 InteriorPos = VEC3_ZERO; 

			InteriorPos.x =	pEventArgs->GetAttributeList().FindAttributeFloatValue("InteriorPosX", 0.0f, false);
			InteriorPos.y =	pEventArgs->GetAttributeList().FindAttributeFloatValue("InteriorPosY", 0.0f, false);
			InteriorPos.z =	pEventArgs->GetAttributeList().FindAttributeFloatValue("InteriorPosZ", 0.0f, false);

			int doorInteriorProxy = interior_commands::GetProxyAtCoords(InteriorPos, (pInteriorName ? atStringHash(pInteriorName) : 0));
			
			atHashString InteriorHash(pInteriorName); 

			SetInteriorStreamingRequest(pObject->GetObjectId(), doorInteriorProxy, InteriorHash, fTime); 

		}
		break; 

	case CUTSCENE_LOAD_MODELS_EVENT:
		{
			cutsceneAssetMgrDebugf2("CUTSCENE_LOAD_MODELS_EVENT: objectId(%d)", pObject->GetObjectId());

			pManager->SetIsLoading( pObject->GetObjectId(), true );

			//remove this make static and assert on type
			if (pEventArgs->GetType() == CUTSCENE_OBJECT_ID_LIST_EVENT_ARGS_TYPE)
			{
				const cutfObjectIdListEventArgs* pObjectIdListEventArgs = static_cast<const cutfObjectIdListEventArgs*>(pEventArgs);
				
				if ( pObjectIdListEventArgs )
				{
					const atArray<s32>& objectIdList = pObjectIdListEventArgs->GetObjectIdList();
					
					for ( int i = 0; i < objectIdList.GetCount(); ++i )
					{				
						const cutfObject* pLoadObject = pManager->GetObjectById( objectIdList[i] );		//list of object to load 
						if ( pLoadObject )
						{   
							if(pLoadObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE )
							{
								const cutfModelObject* pModelObject = static_cast<const cutfModelObject*>(pLoadObject);				
								
								if(pModelObject->GetModelType() == CUTSCENE_PED_MODEL_TYPE || pModelObject->GetModelType() == CUTSCENE_VEHICLE_MODEL_TYPE
									|| pModelObject->GetModelType() == CUTSCENE_PROP_MODEL_TYPE || pModelObject->GetModelType() == CUTSCENE_WEAPON_MODEL_TYPE)
								{
									atHashString ModelHash; 
									
									ModelHash = pModelObject->GetStreamingName(); 

									cutsceneAssertf(ModelHash, "Streaming name for Model %s does not exist in the cutfile", pModelObject->GetDisplayName().c_str()); 				
									
									//cutsceneAssetMgrDebugf("LOAD_MODELS_EVENT: Add streaming request - %s - eventTime(%f)", ModelHash.TryGetCStr(), fTime);
								
									if (pModelObject->GetModelType() == CUTSCENE_PROP_MODEL_TYPE)
									{
										bool MakeItypeRequest = false; 

										u32 archetypeStreamSlotIdx = fwArchetypeManager::GetArchetypeIndexFromHashKey(ModelHash.GetHash());
										if(archetypeStreamSlotIdx < fwModelId::MI_INVALID)
										{
											fwArchetype* pArcheType = fwArchetypeManager::GetArchetype(archetypeStreamSlotIdx); 

											if(pArcheType->IsStreamedArchetype())
											{
												MakeItypeRequest = true; 
											}
										}
										else
										{
											MakeItypeRequest = true; 
										}
										
										if(MakeItypeRequest && cutsceneVerifyf(pModelObject->GetTypeFile().GetHash() > 0, "Send to Default - Animation Resource: Cutscene prop %s has been exported without an itype value", ModelHash.GetCStr()))
										{
											SetItypeModelStreamingRequest( ModelHash, pModelObject->GetTypeFile(), pModelObject->GetObjectId());
										}
										else
										{
											SetModelStreamingRequest(ModelHash, pModelObject->GetObjectId(), MODEL_TYPE); 
											g_HDAssetMgr.AddCutsceneRequestForHD(ModelHash);
										}
									}
									else
									{
										SetModelStreamingRequest(ModelHash, pModelObject->GetObjectId(), MODEL_TYPE);
										g_HDAssetMgr.AddCutsceneRequestForHD(ModelHash);
									}
								}
							}
						}
					}
				}
			}

			//Tell our streaming we have model to load
			m_loadState = LOAD_STATE_STREAMING;
		
		}
		break;
	
	case CUTSCENE_LOAD_RAYFIRE_EVENT:
		{
			if (pEventArgs->GetType() == CUTSCENE_OBJECT_ID_NAME_EVENT_ARGS_TYPE || pEventArgs->GetType() == CUTSCENE_OBJECT_ID_EVENT_ARGS_TYPE )
			{
				const cutfObjectIdEventArgs* pObjectIdArgs = static_cast<const cutfObjectIdEventArgs*>(pEventArgs);

				if ( pObjectIdArgs )
				{
					s32 streamingId = RequestRayFireObject(pManager, pObjectIdArgs->GetObjectId()); 
					
					if(ShouldAddRequestRefForObject(pObjectIdArgs->GetObjectId()))
					{
						return; 
					}
					else
					{

						atHashString mHash; 

						const cutfObject* pTempObject = pManager->GetObjectById(pObjectIdArgs->GetObjectId());		//list of object to load 
						if ( pTempObject && pTempObject->GetType() == CUTSCENE_RAYFIRE_OBJECT_TYPE )
						{   
							const cutfRayfireObject* pRayfireObject = static_cast<const cutfRayfireObject*>(pTempObject);
							mHash = pRayfireObject->GetStreamingName(); 

							cutsceneAssertf(mHash, "Streaming name for Rayfire  Model %s does not exist in the cutfile", pRayfireObject->GetDisplayName().c_str()); 				

						}

						CCutSceneSingleRequestStreamingInfo* StreamInfo = rage_new CCutSceneSingleRequestStreamingInfo(pObjectIdArgs->GetObjectId(), streamingId, RAYFIRE_TYPE, mHash);

						m_StreamingRequests.PushAndGrow(StreamInfo); 

						if(streamingId != NULL_RAYFIRE_STREAMING_INDEX)
						{	
							object_commands::CommandSetCompositeEntityState(streamingId, CE_STATE_PRIMING); 
						}

					}
				}
			}
			m_loadState = LOAD_STATE_STREAMING;
		}
	break; 

	case CUTSCENE_UNLOAD_RAYFIRE_EVENT:
		{
			if (pEventArgs && (pEventArgs->GetType() == CUTSCENE_OBJECT_ID_NAME_EVENT_ARGS_TYPE || pEventArgs->GetType() == CUTSCENE_OBJECT_ID_EVENT_ARGS_TYPE))
			{
				const cutfObjectIdEventArgs* pObjectIdArgs = static_cast<const cutfObjectIdEventArgs*>(pEventArgs);

				if ( pObjectIdArgs )
				{
					const cutfObject* pTempObject = pManager->GetObjectById( pObjectIdArgs->GetObjectId());		//list of object to load 
					if ( pTempObject && pTempObject->GetType() == CUTSCENE_RAYFIRE_OBJECT_TYPE )
					{  
						const cutfRayfireObject* pRayfireObject = static_cast<const cutfRayfireObject*>(pTempObject);	
						

						RemoveStreamingRequest(pRayfireObject->GetObjectId(), true, RAYFIRE_TYPE); 
					}
				}
			}
			m_loadState = LOAD_STATE_STREAMING;
		}
	break;

	case CUTSCENE_LOAD_SUBTITLES_EVENT:
		{
			if (pEventArgs->GetType() == CUTSCENE_FINAL_NAME_EVENT_ARGS_TYPE)
			{
				const cutfFinalNameEventArgs* pNameEventArgs = static_cast<const cutfFinalNameEventArgs*>(pEventArgs); 
				cutsceneAssetMgrDebugf2("CUTSCENE_LOAD_SUBTITLES_EVENT %s", pNameEventArgs->GetName()); 

				SetSubtitleStreamingRequest(pNameEventArgs->GetName(), SUBTITLES_TYPE); 

				m_loadState = LOAD_STATE_STREAMING;
			}
		}
		break; 

	case CUTSCENE_UNLOAD_SUBTITLES_EVENT:
		{
				
		}
		break; 

	case CUTSCENE_LOAD_OVERLAYS_EVENT:
		{
			if (pEventArgs->GetType() == CUTSCENE_OBJECT_ID_LIST_EVENT_ARGS_TYPE)
			{
				const cutfObjectIdListEventArgs* pOverlayIdListEventArgs = static_cast<const cutfObjectIdListEventArgs*>(pEventArgs);
				
				const atArray<s32>& OverlayList = pOverlayIdListEventArgs->GetObjectIdList();

				for ( int i = 0; i < OverlayList.GetCount(); ++i )
				{		
					const cutfObject* pLoadObject = pManager->GetObjectById( OverlayList[i] );		//list of object to load 
					
					if(pLoadObject->GetType() == CUTSCENE_OVERLAY_OBJECT_TYPE)		
					{
						const cutfOverlayObject* pOverlayObject = static_cast <const cutfOverlayObject*>(pLoadObject);
						
						if(pOverlayObject->GetOverlayType() == CUTSCENE_SCALEFORM_OVERLAY_TYPE)	
						{
							SetScaleformStreamingRequest(pOverlayObject);
						}
						else if(pOverlayObject->GetOverlayType() == CUTSCENE_BINK_OVERLAY_TYPE)
						{
							SetBinkStreamingRequest(pOverlayObject);
						}
					}
				}
			}
		}
		break;
	
	case CUTSCENE_UNLOAD_OVERLAYS_EVENT:
		{
			if (pEventArgs->GetType() == CUTSCENE_OBJECT_ID_LIST_EVENT_ARGS_TYPE)
			{
				const cutfObjectIdListEventArgs* pOverlayIdListEventArgs = static_cast<const cutfObjectIdListEventArgs*>(pEventArgs);

				const atArray<s32>& OverlayList = pOverlayIdListEventArgs->GetObjectIdList();
				
				for ( int i = 0; i < OverlayList.GetCount(); ++i )
				{
					RemoveStreamingRequest(OverlayList[i], true, SCALEFORM_OVERLAY_TYPE); 
				}
			}
		}
		break; 


	case CUTSCENE_LOAD_AUDIO_EVENT:
		{
		/*	if (pEventArgs->GetType() = CUTSCENE_NAME_EVENT_ARGS_TYPE )
			{
				const cutfNameEventArgs* pNamedEventList = static_cast<const cutfNameEventArgs*>(pEventArgs);

				g_CutsceneAudioEntity.
			}*/
		}
		break;
	
	case CUTSCENE_LOAD_PARTICLE_EFFECTS_EVENT:
		{
			SetParticleFxStreamingRequest( pManager, pObject,PTFX_TYPE); 
			
			//Need to check that the particle effects have been loaded
			m_loadState = LOAD_STATE_STREAMING;				 
		}
		break;

	case CUTSCENE_UNLOAD_PARTICLE_EFFECTS_EVENT:
		{
			const char* pCutsceneName = pManager->GetCutsceneName();
			atHashWithStringNotFinal ptFxAssetHashName = g_vfxScript.GetCutscenePtFxAssetName(pCutsceneName);
			RemoveStreamingRequest( pObject->GetObjectId(), true , PTFX_TYPE, ptFxAssetHashName); 
		
		}
		break; 

	case CUTSCENE_UNLOAD_MODELS_EVENT:
		{
			cutsceneAssetMgrDebugf2("CUTSCENE_UNLOAD_MODELS_EVENT: objectId(%d)", pObject ? pObject->GetObjectId(): -1);
			const cutfObjectIdListEventArgs* pObjectIdListEventArgs = dynamic_cast<const cutfObjectIdListEventArgs*>(pEventArgs);
			if ( pObjectIdListEventArgs )
			{

				const atArray<s32>& objectIdList = pObjectIdListEventArgs->GetObjectIdList();
				for ( int i = 0; i < objectIdList.GetCount(); ++i )
				{
					const cutfObject* pUnLoadObject = pManager->GetObjectById( objectIdList[i] );

					const cutfModelObject* pModelObject = static_cast<const cutfModelObject*>(pUnLoadObject);
					if (pModelObject)
					{	
						atHashString modelhash = pModelObject->GetStreamingName(); 
						
						cutsceneAssertf(modelhash, "Streaming name for Model %s does not exist in the cutfile", pModelObject->GetDisplayName().c_str()); 				

						//cutsceneAssetMgrDebugf("UNLOAD_MODELS_EVENT: Remove streaming request - %s - eventTime(%f)", modelhash.TryGetCStr(), fTime);
						
						//The scene is finished force the removal of streaming objects irrespective of the refs
						RemoveStreamingRequest(pModelObject->GetObjectId(), true, MODEL_TYPE, modelhash); 

						atArray<cutfEvent*>ObjectEvents;
						const cutfCutsceneFile2* pCutFile = static_cast<const CutSceneManager*>(CutSceneManager::GetInstance())->GetCutfFile();

						bool bUnloadModelEventRemaining = false;

						const atArray<cutfEvent *> &loadEventList = pCutFile->GetLoadEventList();
						for ( int i = 0; ((i < loadEventList.GetCount()) && !bUnloadModelEventRemaining); ++i )
						{
							if (loadEventList[i]->GetEventId() == CUTSCENE_UNLOAD_MODELS_EVENT)
							{								
								if (loadEventList[i]->GetEventArgs() && (loadEventList[i]->GetEventArgs()->GetType() == CUTSCENE_OBJECT_ID_LIST_EVENT_ARGS_TYPE))
								{
									const cutfObjectIdListEventArgs *pObjectIdListEventArgs = static_cast<const cutfObjectIdListEventArgs *>( loadEventList[i]->GetEventArgs());
									if (pObjectIdListEventArgs != NULL )
									{
										const atArray<s32>& iObjectIdList = pObjectIdListEventArgs->GetObjectIdList();
										for (int j = 0; ((j < iObjectIdList.GetCount()) && !bUnloadModelEventRemaining); ++j)
										{
											if (iObjectIdList[j] == pModelObject->GetObjectId())
											{
												if (loadEventList[i]->GetTime() > CutSceneManager::GetInstance()->GetCutSceneCurrentTime())
												{
													bUnloadModelEventRemaining = true;
												}
											}
										}
									}
								}
							}
						}			

						if (!bUnloadModelEventRemaining)
						{
							RemoveStreamingRequest(pModelObject->GetObjectId(), true, PED_VARIATION_TYPE, modelhash); 
						}

						g_HDAssetMgr.RemoveCutsceneRequestForHD(modelhash);
					}
				}
			}
		}
		break;
	
	//Called only at the prestream of cutscene and prestream of skipping
	case CUTSCENE_UPDATE_LOADING_EVENT:
		{
			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

			pCutSceneManager->SetScriptCanChangeEntitiesPreUpdateLoading(false); 

			//assume that all our models have streamed in (if only), may only have a camera in our scene so no need to stream.
			u32 ObjectWaitingForStreaming = 0;

#if __BANK
			u32 iTextLine = 0; 
#endif

			if ( pManager->IsLoading( pObject->GetObjectId() ) )
			{
				switch(m_loadState)
				{
				case LOAD_STATE_STREAMING:
					{
						m_TimeSpentStreaming +=fwTimer::GetTimeStepInMilliseconds(); 
						
						//Models streaming 
			
						for (int i=0; i < m_StreamingRequests.GetCount(); )
						{
						
							bool PrintDebug = false; 
#if __BANK					
							char DebugOutput[1024];
#endif										
							if(m_StreamingRequests[i])
							{
								// certain assets don't need to be loaded on a skip
								bool bIsSkipping = pCutSceneManager->WasSkipped();
								
								switch(m_StreamingRequests[i]->GetDataType())
								{
								case MODEL_TYPE:
								case MODEL_ITYPE_TYPE:
									{
										if(!UpdateModelLoading(*m_StreamingRequests[i], pCutSceneManager))
										{
											ObjectWaitingForStreaming++;
											PrintDebug = true;
#if __BANK					
											formatf(DebugOutput, sizeof(DebugOutput)-1, "Model: %s", m_StreamingRequests[i]->GetHashString()->GetCStr());
#endif										
										}
									}
									break;
								
								case PTFX_TYPE:
									{
										if(!UpdatePtfxLoading(*m_StreamingRequests[i]))
										{
#if !SETUP_CUTSCENE_STREAMING_FOR_CONTENT_CONTROLLED_BUILD	
											ObjectWaitingForStreaming++;
#endif
											PrintDebug = true; 
#if __BANK					
											formatf(DebugOutput, sizeof(DebugOutput)-1, "PTFX List: %s", m_StreamingRequests[i]->GetHashString()->GetCStr());
#endif										
										}
									}
									break; 
								
								case RAYFIRE_TYPE:
									{
										if(!UpdateRayfireLoading(*m_StreamingRequests[i], pCutSceneManager))
										{
#if !SETUP_CUTSCENE_STREAMING_FOR_CONTENT_CONTROLLED_BUILD	
											ObjectWaitingForStreaming++;
#endif
											PrintDebug = true; 
#if __BANK					
											formatf(DebugOutput, sizeof(DebugOutput)-1, "Rayfire: %s", m_StreamingRequests[i]->GetHashString()->GetCStr());
#endif										
										}
										
									}
									break; 

								case SCALEFORM_OVERLAY_TYPE:
									{
										if(!UpdateScaleformLoading(*m_StreamingRequests[i]))
										{
#if !SETUP_CUTSCENE_STREAMING_FOR_CONTENT_CONTROLLED_BUILD	
											ObjectWaitingForStreaming++;
#endif
											PrintDebug = true; 
#if __BANK					
											formatf(DebugOutput, sizeof(DebugOutput)-1, "Scaleform: %s", m_StreamingRequests[i]->GetHashString()->GetCStr());
#endif										
										}
										
									}
									break;
								case BINK_OVERLAY_TYPE:
									{
										if(!UpdateBinkLoading(*m_StreamingRequests[i]))
										{
#if !SETUP_CUTSCENE_STREAMING_FOR_CONTENT_CONTROLLED_BUILD	
											ObjectWaitingForStreaming++;
#endif
											PrintDebug = true; 
#if __BANK					
											formatf(DebugOutput, sizeof(DebugOutput)-1, "Bink: %s", m_StreamingRequests[i]->GetHashString()->GetCStr());
#endif										
										}
										
									}
									break; 

								case SUBTITLES_TYPE:
									{
										if(!bIsSkipping && !UpdateSubtitleLoading(*m_StreamingRequests[i]))
										{
#if !SETUP_CUTSCENE_STREAMING_FOR_CONTENT_CONTROLLED_BUILD	
											ObjectWaitingForStreaming++;
#endif
											PrintDebug = true; 
#if __BANK					
											formatf(DebugOutput, sizeof(DebugOutput)-1, "Subtitle: %s", m_StreamingRequests[i]->GetHashString()->GetCStr());
#endif										
										}
										
									}
									break; 
								
								case PED_VARIATION_TYPE:
								{
									if(!UpdateVariationLoading(*m_StreamingRequests[i]))
									{
										PrintDebug = true; 
										ObjectWaitingForStreaming++;
#if __BANK					
										CCutsceneVariationStreamingInfo* pInfo = static_cast<CCutsceneVariationStreamingInfo*>(m_StreamingRequests[i]); 
										formatf(DebugOutput, sizeof(DebugOutput)-1, "Ped variation: %s Comp:%d, Drwble:%d Tex:%d",  m_StreamingRequests[i]->GetHashString()->GetCStr(), pInfo->m_Component, pInfo->m_Drawable, pInfo->m_Texture);
#endif										
									}
								}
								break; 
								
								case INTERIOR_TYPE:
									{
										if(!UpdateInteriorLoading(*m_StreamingRequests[i]))
										{
#if !SETUP_CUTSCENE_STREAMING_FOR_CONTENT_CONTROLLED_BUILD											
											ObjectWaitingForStreaming++;
#endif	
											PrintDebug = true; 
#if __BANK					
											formatf(DebugOutput, sizeof(DebugOutput)-1, "Interior: %s", m_StreamingRequests[i]->GetHashString()->GetCStr());
#endif										
										}
									}
									break;

								default:
									break;
								}

								if(ObjectWaitingForStreaming > 0)
								{
#if __BANK					
									if(pCutSceneManager->GetDebugManager().m_bPrintModelLoadingToDebug)
									{
										cutsceneDisplayf("CUTSCENE LOADING (%ums): %s ", m_TimeSpentStreaming, DebugOutput);
									}
									if(PrintDebug && grcDebugDraw::GetDisplayDebugText())
									{
										TUNE_GROUP_FLOAT(CUTSCENE, VeritcalBorder, 0.25f, 0.0f, 1.0f, 0.01f); 
										TUNE_GROUP_FLOAT(CUTSCENE, HorizontalBorder, 0.04f, 0.0f, 1.0f, 0.01f); 
										Vector2 vTextRenderPos(HorizontalBorder, VeritcalBorder + (iTextLine)*0.02f);
										grcDebugDraw::Text(vTextRenderPos, Color_white, DebugOutput);
										iTextLine ++; 
									}
									//grcDebugDraw::AddDebugOutput("Loading: %s", m_DebugOutPut);	
#endif										
									if(pCutSceneManager->DidFailToLoadInTimeBeforePlayWasCalled() && m_TimeSpentStreaming > MAX_STREAMING_TIME_BEFORE_PLAYING)
									{
										cutsceneAssertf(0, "%s : Waited 30 seconds to load all cutscene assets but has failed, starting the cutscene anyway which may cause further streaming asserts.", pCutSceneManager->GetCutsceneName());
										ObjectWaitingForStreaming = 0; 
									}
								}
								
								//only need to apply to these data types as the others dont use the streaming requests, yes the class
								//structure is poor
								if((m_StreamingRequests[i]->GetDataType() == MODEL_TYPE || 
									m_StreamingRequests[i]->GetDataType() == MODEL_ITYPE_TYPE ||
									m_StreamingRequests[i]->GetDataType() == PED_VARIATION_TYPE) && m_StreamingRequests[i]->IsReleased())
								{
									
									RemoveStreamingRequest(i);
								}
								else
								{
									i++; 
								}

							}
						}
				
					}
					break;

				case LOAD_STATE_NONE:
					{
				
					}
					break;
				}
			}
			
			if (ObjectWaitingForStreaming == 0)
			{
				pManager->SetIsLoaded( pObject->GetObjectId(), true );
				m_loadState = LOAD_STATE_NONE;
			}
		}
		break;
	
	case CUTSCENE_UPDATE_EVENT:
		{
			CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

			if(pCutSceneManager->IsCutscenePlayingBack())
			{
				for(int i = 0 ; i <m_StreamingRequests.GetCount(); i++)
				{
					if(m_StreamingRequests[i])
					{
						if(m_StreamingRequests[i]->GetDataType() == INTERIOR_TYPE)
						{
							if((pCutSceneManager->GetCutSceneCurrentTime() - m_StreamingRequests[i]->m_LoadTime) > DEFAULT_STREAMING_OFFSET)
							{
								RemoveStreamingRequest(pObject->GetObjectId(), true, INTERIOR_TYPE); 
							}
						}
					}
				}
			}			
		}
		break; 

	case CUTSCENE_UPDATE_UNLOADING_EVENT:
		{

		}
		break;

	case CUTSCENE_UNLOADED_EVENT:
		{		
			RemoveAllStreamingRequests(); 
			g_HDAssetMgr.FlushCutsceneRequestsHD();

			cutsceneDebugf1("CUTSCENE_UNLOADED_EVENT"); 
		}
		break;

	case CUTSCENE_CANCEL_LOAD_EVENT:
		{
			//remove all our streaming requests the loads have been cancelled. 
			RemoveAllStreamingRequests(); 
			g_HDAssetMgr.FlushCutsceneRequestsHD();

			pManager->SetIsLoaded( pObject->GetObjectId(), true );
			m_loadState = LOAD_STATE_NONE;
		}
		break;

	case CUTSCENE_HIDE_HIDDEN_OBJECT_EVENT:	
		{
			Displayf("CUTSCENE_HIDE_HIDDEN_OBJECT_EVENT to asset manager"); 

		}
		break;

	case CUTSCENE_SHOW_HIDDEN_OBJECT_EVENT:	
		{
			Displayf("CUTSCENE_SHOW_HIDDEN_OBJECT_EVENT to asset manager"); 

		}
		break;


	}
}


bool CCutSceneAssetMgrEntity::UpdateSubtitleLoading(CCutSceneStreamingInfo &streamingRequest)
{
	bool bStreamingComplete = true; 
	if(streamingRequest.GetDataType() == SUBTITLES_TYPE)
	{
		if(!TheText.HasThisAdditionalTextLoaded(m_TextBlockName, MISSION_DIALOGUE_TEXT_SLOT))
		{
			bStreamingComplete = false; 
		}
	}
	return bStreamingComplete; 
}

bool CCutSceneAssetMgrEntity::UpdateBinkLoading(CCutSceneStreamingInfo &streamingRequest)
{
	bool bStreamingComplete = true; 
	if(streamingRequest.GetDataType() == BINK_OVERLAY_TYPE)
	{
		if(!g_movieMgr.IsHandleValid(streamingRequest.GetGameIndex()))
		{
			bStreamingComplete = false; 
		}
	}

	return bStreamingComplete; 
}

bool CCutSceneAssetMgrEntity::UpdateScaleformLoading(CCutSceneStreamingInfo &streamingRequest)
{
	bool bStreamingComplete = true; 
	if(streamingRequest.GetDataType() == SCALEFORM_OVERLAY_TYPE)
	{
		if(!CScaleformMgr::IsMovieActive(streamingRequest.GetGameIndex()))
		{
			bStreamingComplete = false; 
		
		}
	}
	return bStreamingComplete; 
}


bool CCutSceneAssetMgrEntity::UpdateRayfireLoading(CCutSceneStreamingInfo &streamingRequest, CutSceneManager* pCutSceneManager)
{
	bool bStreamingComplete = true; 
	
	if(streamingRequest.GetDataType() == RAYFIRE_TYPE)
	{
		if(streamingRequest.GetGameIndex() > NULL_RAYFIRE_STREAMING_INDEX)
		{
			//only try to run this if we have a valid load index
			if(!pCutSceneManager->WasSkipped())
			{
				if(object_commands::CommandGetCompositeEntityState(streamingRequest.GetGameIndex()) != CE_STATE_PRIMED )
				{
					bStreamingComplete = false; 
				}
			}
		}
		else
		{
			s32 streamingId = RequestRayFireObject(pCutSceneManager, streamingRequest.GetObjectId());

			if(streamingId != NULL_RAYFIRE_STREAMING_INDEX)
			{
				streamingRequest.SetObjectIndex(streamingId); 

				object_commands::CommandSetCompositeEntityState(streamingId, CE_STATE_PRIMING); 
			}

			bStreamingComplete = false; 
		}
	}
	return bStreamingComplete; 
}


bool CCutSceneAssetMgrEntity::UpdatePtfxLoading(CCutSceneStreamingInfo &streamingRequest)
{
	bool bStreamingComplete = true; 
	
	if(streamingRequest.GetDataType() == PTFX_TYPE)
	{
		strLocalIndex slot = ptfxManager::GetAssetStore().FindSlot(streamingRequest.GetHashString()->GetHash());

		if( cutsceneVerifyf( slot != -1, "PTFX dictionary %s does not exist", streamingRequest.GetHashString()->GetCStr() ))
		{
			if(!ptfxManager::GetAssetStore().HasObjectLoaded(slot))
			{
				bStreamingComplete = false; 
			}
		}
	}

	return bStreamingComplete; 
}

bool CCutSceneAssetMgrEntity::UpdateVariationLoading(CCutSceneStreamingInfo &streamingRequest)
{
	bool bStreamingComplete = true; 
	
	if(streamingRequest.GetDataType() == PED_VARIATION_TYPE)
	{

		if(!streamingRequest.HasLoaded())
		{
			bStreamingComplete = false; 
			cutsceneAssetMgrDebugf3("Ped variation Loading : %s Comp:%d/%s, Drwble:%d Tex:%d",  
				streamingRequest.GetHashString()->TryGetCStr(),
				static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Component, 
				CPedVariationData::GetVarOrPropSlotName(static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Component),
				static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Drawable, 
				static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Texture);
		}
		else
		{
			cutsceneAssetMgrDebugf2("Ped variation Loaded : %s Comp:%d/%s, Drwble:%d Tex:%d",  
				streamingRequest.GetHashString()->TryGetCStr(),
				static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Component, 
				CPedVariationData::GetVarOrPropSlotName(static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Component),
				static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Drawable, 
				static_cast<CCutsceneVariationStreamingInfo&>(streamingRequest).m_Texture);
		}
	}

	return bStreamingComplete; 
}

bool CCutSceneAssetMgrEntity::UpdateInteriorLoading(CCutSceneStreamingInfo &streamingRequest)
{
	bool bStreamingComplete = false; 

	if(streamingRequest.GetDataType() == INTERIOR_TYPE)
	{
		if(streamingRequest.m_Index >= 0)
		{
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(streamingRequest.m_Index);
			
			if (cutsceneVerifyf(pIntProxy, "UpdateInteriorLoading - Interior proxy %s doesn't exist ", streamingRequest.GetHashString()->GetCStr()))
			{
				if (pIntProxy->GetCurrentState() >= CInteriorProxy::PS_FULL_WITH_COLLISIONS)
				{
					bStreamingComplete = true;
				}
			}
			else
			{
				bStreamingComplete = true;
			}
		}
		else
		{
			bStreamingComplete = true;
		}
	}
	return bStreamingComplete; 
}

bool CCutSceneAssetMgrEntity::UpdateModelLoading(CCutSceneStreamingInfo &streamingRequest, CutSceneManager* pCutSceneManager)
{
	bool bStreamingComplete = true; 
	atArray<cutsEntity*>  ActorMangers; 
	pCutSceneManager->GetEntityByType(CUTSCENE_ACTOR_GAME_ENITITY, ActorMangers); 

	if(streamingRequest.GetDataType() == MODEL_TYPE || streamingRequest.GetDataType() == MODEL_ITYPE_TYPE )
	{
		if (!streamingRequest.HasLoaded())
		{	
			bStreamingComplete = false;
			cutsceneAssetMgrDebugf3("Model (%s) Loading", streamingRequest.GetHashString()->TryGetCStr());
		}
		else
		{			
			cutsceneAssetMgrDebugf2("Model (%s) Loaded", streamingRequest.GetHashString()->TryGetCStr());

			if(!pCutSceneManager->IsCutscenePlayingBack())
			{
				// Find the animated actor actor associated with the streaming request
				for(int j = 0;  j <ActorMangers.GetCount(); j++ )
				{
					CCutsceneAnimatedActorEntity* pActorManger = static_cast<CCutsceneAnimatedActorEntity*>(ActorMangers[j]);

					if(pActorManger && !pActorManger->IsBlockingVariationStreamingAndApplication() && pActorManger->GetCutfObject()->GetObjectId() == streamingRequest.GetObjectId())
					{
						pActorManger->ComputeInitialVariations(*streamingRequest.GetHashString()); 
						pActorManger->PreStreamVariations(pCutSceneManager, pCutSceneManager->GetStartTime(), PED_VARIATION_STREAMING_OFFSET);						
					}	
				}
			}
			else
			{
				if(pCutSceneManager->WasSkipped())
				{
					// Find the animated actor actor associated with the streaming request
					for(int j = 0;  j <ActorMangers.GetCount(); j++ )
					{
						CCutsceneAnimatedActorEntity* pActorManger = static_cast<CCutsceneAnimatedActorEntity*>(ActorMangers[j]);
						if(pActorManger && !pActorManger->IsBlockingVariationStreamingAndApplication() && pActorManger->GetCutfObject()->GetObjectId() == streamingRequest.GetObjectId())
						{			
							pActorManger->ComputeVariationsForSkip(*streamingRequest.GetHashString()); 
						}	
					}
				}
			}

		}
	}
	return bStreamingComplete; 
}

#if __BANK

void CCutSceneAssetMgrEntity::DebugDraw() const
{
	PrintStreamingRequests();
}


void CCutSceneAssetMgrEntity::PrintStreamingRequests() const
{
	for(int i = 0; i < m_StreamingRequests.GetCount(); i ++)
	{
		CCutSceneStreamingInfo *pCutSceneStreamingInfo = m_StreamingRequests[i];
		if(pCutSceneStreamingInfo)
		{
			if(!pCutSceneStreamingInfo->HasLoaded())
			{
				cutsceneAssetMgrDebugf3("Loading: %s", pCutSceneStreamingInfo->GetDebugString().c_str());
			}
		}
	}
}
#endif //__BANK

#if !__NO_OUTPUT
void CCutSceneAssetMgrEntity::CommonDebugStr( char * debugStr)
{
	CutSceneManager::CommonDebugStr(debugStr);
	sprintf(debugStr, "%sAsset Manager - ", debugStr);
}
#endif // !__NO_OUTPUT

///////////////////////////////////////////////////////////////////////////////////////////////////

//eof
