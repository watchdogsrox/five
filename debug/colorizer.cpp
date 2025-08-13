/////////////////////////////////////////////////////////////////////////////////
// FILE		: colorizer.cpp
// PURPOSE	: To provide in game visual helpers for debugging.
/////////////////////////////////////////////////////////////////////////////////

#if __BANK

#include "colorizer.h"

#include "grcore/vertexbuffer.h"
#include "grmodel/geometry.h"
#include "rmcore/drawable.h"

#include "modelInfo/PedModelInfo.h"
#include "peds/ped.h"
#include "peds/rendering/PedVariationDebug.h"
#include "scene/entity.h"

// ================================================================================================

void ColorizerUtils::GetDrawablePolyCount(const rmcDrawable* pDrawable, u32* pResult, u32 lodLevel)
{
	if (!pDrawable) { return; }

	const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

	if(lodGroup.ContainsLod(lodLevel))
	{
		const rmcLod& lod = lodGroup.GetLod(lodLevel);

		for(s32 i=0;i<lod.GetCount();i++)
		{
			const grmModel* model = lod.GetModel(i);
			if (model)
			{
				for(s32 j=0;j<model->GetGeometryCount();j++)
				{
					const grmGeometry& geo = model->GetGeometry(j);
					*pResult += geo.GetPrimitiveCount();
				}
			}
		}
	}
}

void ColorizerUtils::GetDrawableInfo(const rmcDrawable* pDrawable, u32* pPolyCount, u32* pVertexCount, u32* pVertexSize, u32* pIndexCount, u32* pIndexSize, u32 lodLevel)
{
	if (!pDrawable) { return; }

	const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

	if(lodGroup.ContainsLod(lodLevel))
	{
		const rmcLod& lod = lodGroup.GetLod(lodLevel);

		for(s32 i=0;i<lod.GetCount();i++)
		{
			const grmModel* model = lod.GetModel(i);
			if (model)
			{
				for(s32 j=0;j<model->GetGeometryCount();j++)
				{
					grmGeometry& geo = model->GetGeometry(j);
					*pPolyCount += geo.GetPrimitiveCount();

					if (geo.GetType() == grmGeometry::GEOMETRYEDGE)
					{
#if USE_EDGE
						grmGeometryEdge& edgeGeometry = static_cast<grmGeometryEdge&>(geo);
						for (s32 g = 0; g < edgeGeometry.GetBlendHeaderCount(); ++g)
						{
							*pVertexCount += edgeGeometry.GetEdgeGeomPpuConfigInfos()[g].spuConfigInfo.numVertexes;
							*pVertexSize += edgeGeometry.GetEdgeGeomPpuConfigInfos()[g].spuConfigInfo.numVertexes * sizeof(Vector4);
							*pIndexCount += edgeGeometry.GetEdgeGeomPpuConfigInfos()[g].spuConfigInfo.numIndexes;
							*pIndexSize += edgeGeometry.GetEdgeGeomPpuConfigInfos()[g].spuConfigInfo.numIndexes * sizeof(u16);
						}	
#endif // USE_EDGE...
					}
					else
					{
						*pVertexCount += geo.GetVertexCount();
						grcVertexBuffer* vb = geo.GetVertexBuffer(true);
						const grcFvf* fvf = vb->GetFvf();
						Assert(fvf);

						if (fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsHalf4 || fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsHalf3)
						{
							*pVertexSize += geo.GetVertexCount() * sizeof(u16);
						}
						else if (fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsFloat4 || fvf->GetDataSizeType(grcFvf::grcfcPosition) == grcFvf::grcdsFloat3)
						{
							*pVertexSize += geo.GetVertexCount() * sizeof(float);
						}
						else
						{
							Assert(false);
						}

						grcIndexBuffer* ib = geo.GetIndexBuffer(true);
						*pIndexCount += ib->GetIndexCount();
						*pIndexSize += ib->GetIndexCount() * sizeof(u16);
					}
				}
			}
		}
	}
}

void ColorizerUtils::GetPolyCountFrag(CEntity *pEntity, CBaseModelInfo *pModelInfo, u32* pResult, u32 lodLevel)
{
	const fragType* pFragType = pModelInfo->GetFragType();
	if (pFragType)
	{
		fragDrawable* pDrawable = pFragType->GetCommonDrawable();
		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);
		fragInst* pFragInst = pDynamicEntity->GetFragInst();
		Assert(pDrawable);
		if (pFragType->GetSkinned() || !pFragInst)
		{
			GetDrawablePolyCount(pDrawable, pResult, lodLevel);
		} 
		else 
		{
			u64 damagedBits;
			u64 destroyedBits;

			PopulateBitFields(pFragInst, damagedBits, destroyedBits);

			//in this case, we have a list of entities to draw...
			for (u64 child=0; child<pFragInst->GetTypePhysics()->GetNumChildren(); ++child)
			{
				fragTypeChild* pChildType = pFragInst->GetTypePhysics()->GetAllChildren()[child];
				pDrawable = NULL;
				if ((destroyedBits & ((u64)1<<child)) == 0) {
					if ((damagedBits & ((u64)1<<child)) != 0)
					{
						//broken...
						pDrawable = pChildType->GetDamagedEntity() ? pChildType->GetDamagedEntity() : pChildType->GetUndamagedEntity();
					}
					else
					{
						//not broken...
						pDrawable = pChildType->GetUndamagedEntity();
					}
				}
				GetDrawablePolyCount(pDrawable, pResult, lodLevel);
			}
		}
	}
	else
	{
		GetDrawablePolyCount(pEntity->GetDrawable(), pResult, lodLevel);
	}
}

void ColorizerUtils::GetFragInfo(CEntity *pEntity, CBaseModelInfo *pModelInfo, u32* pPolyCount, u32* pVertexCount, u32* pVertexSize, u32* pIndexCount, u32* pIndexSize, u32 lodLevel)
{
	const fragType* pFragType = NULL;
	if (pEntity->GetIsTypeVehicle())
	{
		CVehicleModelInfo* pVMI = static_cast<CVehicleModelInfo*>(pModelInfo);
		if (pVMI->GetAreHDFilesLoaded())
		{
			pFragType = pVMI->GetHDFragType();
		}
	}

	if (!pFragType)
		pFragType = pModelInfo->GetFragType();

	if (pFragType)
	{
		fragDrawable* pDrawable = pFragType->GetCommonDrawable();
		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);
		fragInst* pFragInst = pDynamicEntity->GetFragInst();
		Assert(pDrawable);
		if (pFragType->GetSkinned() || !pFragInst)
		{
			GetDrawableInfo(pDrawable, pPolyCount, pVertexCount, pVertexSize, pIndexCount, pIndexSize, lodLevel);
		} 
		else 
		{
			u64 damagedBits;
			u64 destroyedBits;

			PopulateBitFields(pFragInst, damagedBits, destroyedBits);

			//in this case, we have a list of entities to draw...
			for (u64 child=0; child<pFragInst->GetTypePhysics()->GetNumChildren(); ++child)
			{
				fragTypeChild* pChildType = pFragInst->GetTypePhysics()->GetAllChildren()[child];
				pDrawable = NULL;
				if ((destroyedBits & ((u64)1<<child)) == 0) {
					if ((damagedBits & ((u64)1<<child)) != 0)
					{
						//broken...
						pDrawable = pChildType->GetDamagedEntity() ? pChildType->GetDamagedEntity() : pChildType->GetUndamagedEntity();
					}
					else
					{
						//not broken...
						pDrawable = pChildType->GetUndamagedEntity();
					}
				}
				GetDrawableInfo(pDrawable, pPolyCount, pVertexCount, pVertexSize, pIndexCount, pIndexSize, lodLevel);
			}
		}
	}
	else
	{
		GetDrawableInfo(pEntity->GetDrawable(), pPolyCount, pVertexCount, pVertexSize, pIndexCount, pIndexSize, lodLevel);
	}
}

u32 ColorizerUtils::GetPolyCount(CEntity* pEntity)
{
	u32 polyCount = 0;
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

		if(pEntity->GetIsTypePed())
		{
			const CPedModelInfo* pPedModelInfo = static_cast<const CPedModelInfo*>(pModelInfo);
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if (pPedModelInfo->GetIsStreamedGfx())
			{
				const CPedStreamRenderGfx* pGfxData = pPed->GetPedDrawHandler().GetConstPedRenderGfx();
				if (pGfxData)
				{
					CPedVariationDebug::GetPolyCountPlayerInternal(pGfxData, &polyCount);
				}
			} 
			else 
			{
				CPedVariationDebug::GetPolyCountPedInternal(pPedModelInfo, pPed->GetPedDrawHandler().GetVarData(), &polyCount);
			}	
		}
		else
		{
			if(pModelInfo->GetFragType())
			{
				GetPolyCountFrag(pEntity, pModelInfo, &polyCount);
			}
			else
			{
				rmcDrawable* pDrawable = pModelInfo->GetDrawable();
				if(pDrawable)
				{
					GetDrawablePolyCount(pDrawable, &polyCount);
				}
			}
		}
	}
	return polyCount;
}

u32 ColorizerUtils::GetBoneCount(CEntity* pEntity)
{
	u32 polyCount = 0;
	if (pEntity)
	{
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

		rmcDrawable* pDrawable = pModelInfo->GetDrawable();
		if(pDrawable)
		{
			if(pDrawable->GetSkeletonData())
			{
				return pDrawable->GetSkeletonData()->GetNumBones();
			}
		}		
	}
	return polyCount;
}

u32 ColorizerUtils::GetShaderCount(CEntity* pEntity)
{
	u32 shaderCount = 0;
	if (pEntity)
	{
		rmcDrawable* pDrawable = pEntity->GetDrawable();
		if (pDrawable)
		{
			shaderCount = (u32) pDrawable->GetShaderGroup().GetCount();
		}
	}
	return shaderCount;
}

void ColorizerUtils::GetVVRData(const rmcDrawable *drawable, float &vtxCount, float &volume)
{
	const rmcLodGroup &lodGroup= drawable->GetLodGroup();

	if( lodGroup.ContainsLod(LOD_HIGH) )
	{
		const rmcLod& lod = lodGroup.GetLod(LOD_HIGH);

		for(int i=0;i<lod.GetCount();i++)
		{
			const grmModel* model = lod.GetModel(i);
			if (model)
			{
				for(int j=0;j<model->GetGeometryCount();j++)
				{
					const grmGeometry& geo = model->GetGeometry(j);
					vtxCount += (float)geo.GetVertexCount();
				}
			}
		}

		Vector3 boxMin,boxMax;
		lodGroup.GetBoundingBox(boxMin,boxMax);
		Vector3 lengths = boxMax - boxMin;
		if( lengths.x == 0.0f || lengths.y == 0.0f || lengths.z == 0.0f )
		{ // Flat volume ? let's take an area, then...
			lengths.x = lengths.x == 0.0f ? 1.0f : lengths.x;
			lengths.y = lengths.y == 0.0f ? 1.0f : lengths.y;
			lengths.z = lengths.z == 0.0f ? 1.0f : lengths.z;
		}

		volume += lengths.x * lengths.y * lengths.z;
	}
}

void ColorizerUtils::FragGetVVRData(const CEntity *entity, const CBaseModelInfo *modelinfo, float &vtxCount, float &volume)
{
	const fragType* type = modelinfo->GetFragType();
	if (type)
	{
		fragDrawable* toDraw = type->GetCommonDrawable();

		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(entity);
		fragInst* pFragInst = pDynamicEntity->GetFragInst();


		Assert(toDraw != NULL);
		if (type->GetSkinned() || pFragInst == NULL )
		{
			GetVVRData(toDraw, vtxCount, volume);
		} 
		else 
		{
			u64 damagedBits;
			u64 destroyedBits;

			PopulateBitFields(pFragInst, damagedBits, destroyedBits);

			//in this case, we have a list of entities to draw...
			for (u64 child = 0; child < pFragInst->GetTypePhysics()->GetNumChildren(); ++child)
			{
				fragTypeChild* childType = pFragInst->GetTypePhysics()->GetAllChildren()[child];

				toDraw = NULL;
				if ((destroyedBits & ((u64)1<<child)) == 0) {
					if ((damagedBits & ((u64)1<<child)) != 0)
					{
						//broken...
						toDraw = childType->GetDamagedEntity() ? 
							childType->GetDamagedEntity() : 
						childType->GetUndamagedEntity();
					}
					else
					{
						//not broken...
						toDraw = childType->GetUndamagedEntity();
					}
				}

				GetVVRData(toDraw, vtxCount, volume);
			}
		}
	}
	else
	{
		GetVVRData(entity->GetDrawable(), vtxCount, volume);
	}
}

#if __PPU
bool ColorizerUtils::GetEdgeUseage(const rmcDrawable *drawable)
{
	bool result = false;

	const rmcLodGroup &lodGroup= drawable->GetLodGroup();

	if( lodGroup.ContainsLod(LOD_HIGH) )
	{
		const rmcLod& lod = lodGroup.GetLod(LOD_HIGH);

		for(int i=0;i<lod.GetCount();i++)
		{
			const grmModel* model = lod.GetModel(i);
			if (model)
			{
				for(int j=0;j<model->GetGeometryCount();j++)
				{
					grmGeometry& geo = model->GetGeometry(j);
					result |= geo.GetType() == grmGeometry::GEOMETRYEDGE;
				}
			}
		}
	}

	return result;
}

bool ColorizerUtils::FragGetEdgeUseage(const CEntity *entity, const CBaseModelInfo *modelinfo)
{
	const fragType* type = modelinfo->GetFragType();
	if (type)
	{
		fragDrawable* toDraw = type->GetCommonDrawable();

		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(entity);
		fragInst* pFragInst = pDynamicEntity->GetFragInst();


		Assert(toDraw != NULL);
		if (type->GetSkinned() || pFragInst == NULL )
		{
			return GetEdgeUseage(toDraw);
		} 
		else 
		{
			// If its a non-skinned frag use first child to decide whether to use Edge
			u64 damagedBits;
			u64 destroyedBits;

			PopulateBitFields(pFragInst, damagedBits, destroyedBits);

			//in this case, we have a list of entities to draw...
			Assertf(pFragInst->GetTypePhysics()->GetNumChildren() > 0,"This frag has no children");	// Whats the point of this entity?
			fragTypeChild* childType = pFragInst->GetTypePhysics()->GetAllChildren()[0];

			toDraw = NULL;
			if ((destroyedBits & ((u64)BIT(0))) == 0) {
				if ((damagedBits & ((u64)BIT(0))) != 0)
				{
					//broken...
					toDraw = childType->GetDamagedEntity() ? 
						childType->GetDamagedEntity() : 
					childType->GetUndamagedEntity();
				}
				else
				{
					//not broken...
					toDraw = childType->GetUndamagedEntity();
				}
			}

			return GetEdgeUseage(toDraw);
		}
	}
	else
	{
		return GetEdgeUseage(entity->GetDrawable());
	}
}
#endif // __PPU

#endif // __BANK
