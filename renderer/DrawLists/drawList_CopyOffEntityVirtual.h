#ifndef INC_DRAWLIST_COEV_H_
#define INC_DRAWLIST_COEV_H_

template<class EntityDrawData>
DrawListAddress CopyOffEntityVirtual(CEntity * pEntity)
{
	int sharedMemType = DL_MEMTYPE_ENTITY;
	dlSharedDataInfo& sharedDataInfo = pEntity->GetDrawHandler().GetSharedDataOffset();

	DrawListAddress dataAddress = gDCBuffer->LookupSharedData(sharedMemType, sharedDataInfo);

	if(dataAddress.IsNULL() || !g_cache_entities)
	{
		u32 dataSize = (u32) sizeof(EntityDrawData);
		void * data = gDCBuffer->AddDataBlock(NULL, dataSize, dataAddress);
		if(g_cache_entities)
		{
			gDCBuffer->AllocateSharedData(sharedMemType, sharedDataInfo, dataSize, dataAddress);
		}

		EntityDrawData* pDrawData = rage_placement_new(data) EntityDrawData();
		pDrawData->Init(pEntity);

		gDrawListMgr->AddArchetypeReference(pEntity->GetModelIndex());
	}
#if RAGE_INSTANCED_TECH
	else
	{
		if (pEntity->GetViewportInstancedRenderBit() != 0)
		{
			u32 dataSize = (u32) sizeof(EntityDrawData);
			void *data = gDCBuffer->GetDataBlock(dataSize,dataAddress);
			reinterpret_cast<EntityDrawData*>(data)->SetViewportInstancedRenderBit(pEntity->GetViewportInstancedRenderBit());
		}
	}
#endif

	return dataAddress;
};

#endif