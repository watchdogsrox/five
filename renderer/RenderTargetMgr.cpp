//
// filename:	RenderTargetMgr.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "renderer/rendertargetmgr.h"
// C headers
// Rage headers
#include "fragment/drawable.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "system/param.h" 

#include "entity/archetype.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/gameskeleton.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#if __D3D11
#include "grcore/texturefactory_d3d11.h"
#endif //__D3D11

#if __BANK
PARAM(namedrtdebug,"Output debug info");
#endif

CRenderTargetMgr gRenderTargetMgr;

void CRenderTargetMgr::InitClass(unsigned initMode)
{
	if(initMode == INIT_SESSION)
	{
	
	}
}

void CRenderTargetMgr::ShutdownClass(unsigned shutdownMode)
{
	if( shutdownMode == SHUTDOWN_SESSION )
	{
		gRenderTargetMgr.ReleaseAllTargets();
	}
}

//
// name:		CRenderTargetMgr::CreateRenderTarget
// description:	Create a new render target. Return a unique render context id
grcRenderTarget* CRenderTargetMgr::CreateRenderTarget(u16 poolID, const char* pName, s32 width, s32 height, s32 poolHeap WIN32PC_ONLY(, grcRenderTarget* originalTarget))
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	// set render target descriptions
	grcTextureFactory::CreateParams params;
	params.Multisample	= 0;
	params.HasParent	= true;
#if __XENON //requires parent to prevent this from spamming backbuffer in EDRAM
	params.Parent		= grcTextureFactory::GetInstance().GetBackBuffer(false); 
#else
	params.Parent		= NULL; 
#endif
	params.UseFloat		= true;
	params.Format		= grctfA8R8G8B8;
	params.MipLevels	= 1;
#if __PPU
	Assert(params.Format == grctfA8R8G8B8);	// this is format of tiled memorypool used
	params.InTiledMemory= true;
#endif
	
	// don't want unpooled render targets, force kRTPoolIDAuto if needed
	params.PoolID = (poolID==kRTPoolIDInvalid ? kRTPoolIDAuto : poolID);
	params.PoolHeap = (u8)poolHeap;

	// do not allocate from pool straight away unless it's an auto pool
	params.AllocateFromPoolOnCreate = (params.PoolID == kRTPoolIDAuto ? true : false); 

	grcRenderTarget *rt = grcTextureFactory::GetInstance().CreateRenderTarget( pName, grcrtPermanent, width, height, 32, &params WIN32PC_ONLY(, originalTarget));
	Assert(rt);
	return(rt);
}

bool CRenderTargetMgr::namedRendertarget::CreateRenderTarget(grcTexture* texture)
{
	bool result = true;
#if !__FINAL
	u32 imgFormat = texture->GetImageFormat();
	bool isSupported =	imgFormat == grcImage::A8R8G8B8 ||
						imgFormat == grcImage::A8B8G8R8 ||
						imgFormat == grcImage::L8 ||
						imgFormat == grcImage::A1R5G5B5 ||
						imgFormat == grcImage::R5G6B5;

	Assertf( isSupported == true, "Warning: RT %s is in an unsupported format (%s)",name.GetCStr(), grcImage::GetFormatString((grcImage::Format)imgFormat));
	if( isSupported )
	{
		rendertarget = grcTextureFactory::GetInstance().CreateRenderTarget(name.GetCStr(),texture->GetTexturePtr());
	}
	else
	{
		rendertarget = NULL;
		modelidx = fwModelId::MI_INVALID;
		diffuseTexVar = grcevNONE;
		isLinked = false;
		shaderIdx = -1;
		result = false;
	}
#else								
	rendertarget = grcTextureFactory::GetInstance().CreateRenderTarget(name.GetCStr(), texture->GetTexturePtr());
#endif

	if (!rendertarget)
	{
		result = false;
	}

	return result;						
}

#if __SCRIPT_MEM_CALC
u32 CRenderTargetMgr::namedRendertarget::GetMemoryUsage() const
{
	u32 TotalSize = 0;
	
	if (rendertarget)
	{
		TotalSize += rendertarget->GetPhysicalSize();
	}

	return TotalSize;
}
#endif	//	__SCRIPT_MEM_CALC

bool CRenderTargetMgr::SetNamedRendertargetDelayLoad(const atFinalHashString &name, unsigned int ownerTag, bool loadType)
{
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		namedRendertarget &rt = namedRendertargetList[i];
		if( rt.name == name && rt.owner == ownerTag )
		{
			rt.delayedLoad = loadType;
			return true;
		}
	}
	
	return Verifyf( false, "Named Rendertarget %s not found",name.GetCStr());
}

static inline void EnsureTextureGpuWritable(grcTexture *texture)
{
#if RSG_DURANGO || RSG_ORBIS
	static_cast<grcOrbisDurangoTextureBase*>(texture)->EnsureGpuWritable();
#else
	(void)texture;
#endif
}

bool CRenderTargetMgr::SetupNamedRendertargetDelayLoad(const atFinalHashString &name, unsigned int ownerTag, grcTexture *texture)
{
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		namedRendertarget &rt = namedRendertargetList[i];
		if( rt.name == name && rt.owner == ownerTag )
		{
			EnsureTextureGpuWritable(texture);
			rt.texture = texture;
			rt.delayedLoadReady = true;
			rt.CreateRenderTarget(texture);
			return true;
		}
	}
	
	return Verifyf( false, "Named Rendertarget %s not found",name.GetCStr());
}


CRenderTargetMgr::namedRendertarget *CRenderTargetMgr::GetNamedRendertarget(const atFinalHashString &name, unsigned int ownerTag)
{
	return GetNamedRendertarget(name.GetHash(), ownerTag);
}

CRenderTargetMgr::namedRendertarget *CRenderTargetMgr::GetNamedRendertarget(u32 nameHashValue, unsigned int ownerTag)
{
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		const namedRendertarget &rt = namedRendertargetList[i];
		if( rt.name.GetHash() == nameHashValue && rt.owner == ownerTag )
		{
			return &namedRendertargetList[i];
		}
	}
	
	return NULL;
}

const CRenderTargetMgr::namedRendertarget *CRenderTargetMgr::GetNamedRendertarget(const u32	nameHashValue)
{
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		const namedRendertarget &rt = namedRendertargetList[i];
		if( rt.name.GetHash() == nameHashValue )
		{
			return &rt;
		}
	}
	return NULL;
}

const CRenderTargetMgr::namedRendertarget *CRenderTargetMgr::GetNamedRenderTargetFromUniqueID(u32 uniqueID)
{
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		const namedRendertarget &rt = namedRendertargetList[i];
		if( rt.uniqueId == uniqueID )
		{
			return &rt;
		}
	}
	return NULL;
}

bool CRenderTargetMgr::RegisterNamedRendertarget(const char *name, unsigned int ownerTag)
{
	atFinalHashString RtName(name);
	if (Verifyf(!GetNamedRendertarget(RtName,ownerTag), "Named RenderTarget %s was already registered",name) && 
		Verifyf(namedRendertargetList.IsFull() == false, "Too many Named Rendertargets registered, maximum is %d",namedRendertargetList.GetMaxCount()))
	{
		namedRendertarget &rt = namedRendertargetList.Append();
		rt.name = RtName;
		rt.uniqueId = GetRenderTargetUniqueId(name, ownerTag);
		rt.texture = NULL;
		rt.rendertarget = NULL;
		rt.owner = ownerTag;
		rt.release = false;
#if RSG_PC
		rt.release_count = 0;
#endif
		rt.delayedLoad = false;
		rt.delayedLoadReady = false;
		rt.delayedLoadSetup = false;
		rt.isLinked = false;
		rt.drawableRefAdded = false;
		rt.drawIdx = strLocalIndex::INVALID_INDEX;
		rt.drawDictDrawableIdx = 0xff;
		rt.drawableType = fwArchetype::DT_UNINITIALIZED;
		rt.modelidx = fwModelId::MI_INVALID;

#if __BANK
		if (PARAM_namedrtdebug.Get())
		{

			Displayf("[RTM] Registered (%s)\t ownerTag: %u", rt.name.GetCStr(), ownerTag);
		}
#endif
		return true;
	}

	return false;
}

bool CRenderTargetMgr::RegisterNamedRendertarget(const atFinalHashString& RtName, u32 ownerTag, u32 uniqueId) 
{
	if (Verifyf(!GetNamedRendertarget(RtName,ownerTag), "Named RenderTarget %s was already registered",RtName.GetCStr()) && 
		Verifyf(namedRendertargetList.IsFull() == false, "Too many Named Rendertargets registered, maximum is %d",namedRendertargetList.GetMaxCount()))
	{
		namedRendertarget &rt = namedRendertargetList.Append();
		rt.name = RtName;
		rt.uniqueId = uniqueId;
		rt.texture = NULL;
		rt.rendertarget = NULL;
		rt.owner = ownerTag;
		rt.release = false;
#if RSG_PC
		rt.release_count = 0;
#endif
		rt.delayedLoad = false;
		rt.delayedLoadReady = false;
		rt.delayedLoadSetup = false;
		rt.isLinked = false;
		rt.drawableRefAdded = false;
		rt.drawIdx = strLocalIndex::INVALID_INDEX;
		rt.drawDictDrawableIdx = 0xFF;			// this is a bit of a 'magic' invalid value. It's the index in the dict.
		rt.drawableType = fwArchetype::DT_UNINITIALIZED;
		rt.modelidx = fwModelId::MI_INVALID;

#if __BANK
		if (PARAM_namedrtdebug.Get())
		{

			Displayf("[RTM] Registered (%s)\t ownerTag: %u", rt.name.GetCStr(), ownerTag);
		}
#endif
		return true;
	}

	return false;
}


bool CRenderTargetMgr::ReleaseNamedRendertarget(const char *name, unsigned int ownerTag)
{
	atFinalHashString RtName(name);
	Assertf( GetNamedRendertarget(RtName,ownerTag) != NULL, "Cannot release Script RenderTarget %s as it doesn't exist",name);
	return ReleaseNamedRendertarget(RtName, ownerTag);
}

bool CRenderTargetMgr::ReleaseNamedRendertarget(atFinalHashString RtName, unsigned int ownerTag) 
{
	return ReleaseNamedRendertarget(RtName.GetHash(), ownerTag);
}

bool CRenderTargetMgr::ReleaseNamedRendertarget(u32 nameHashValue, unsigned int ownerTag)
{
	int idx = -1;
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		const namedRendertarget &rt = namedRendertargetList[i];
		if( rt.name.GetHash() == nameHashValue && rt.owner == ownerTag )
		{
			idx = i;
			break;
		}
	}
	
	if( idx > -1 )
	{
		namedRendertargetList[idx].release = true;
		
#if RSG_PC
		namedRendertargetList[idx].release_count = GRCDEVICE.GetGPUCount()-1;
#endif
	#if __BANK
		if (PARAM_namedrtdebug.Get() && namedRendertargetList[idx].modelidx != fwModelId::MI_INVALID)
		{
			CBaseModelInfo *modelinfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(namedRendertargetList[idx].modelidx)));
			if (modelinfo)
			{
				Displayf("[RTM] Marking for release (%s): (%u)\t ownerTag: %u\t", namedRendertargetList[idx].name.GetCStr(), namedRendertargetList[idx].modelidx, ownerTag);
			}
		}
	#endif
		return true;
	}

	return false;
}


unsigned int CRenderTargetMgr::GetNamedRendertargetId(const char *name, unsigned int ownerTag) const
{
	atFinalHashString RtName(name);

	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		const namedRendertarget &rt = namedRendertargetList[i];
		if( rt.name == RtName && rt.owner == ownerTag && rt.release == false )
		{
			return rt.uniqueId;
		}
	}

	return 0;
}


unsigned int CRenderTargetMgr::GetNamedRendertargetId(const char *name, unsigned int ownerTag, u32 modelIdx) const
{
	atFinalHashString RtName(name);

	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		const namedRendertarget &rt = namedRendertargetList[i];
		if( rt.name == RtName && rt.owner == ownerTag && rt.modelidx == modelIdx && rt.release == false )
		{
			return rt.uniqueId;
		}
	}

	return 0;
}

#if !__FINAL
bool CRenderTargetMgr::IsNamedRenderTargetValid(const char *name)
{
	char *scriptRT = strstr((char *)name,"script_rt_");
	
	return scriptRT == NULL;
}
#endif

void CRenderTargetMgr::StoreNamedRendertargets(NamedRenderTargetBufferedData *out)
{
	const unsigned count = namedRendertargetList.GetCount();
	for (unsigned i=0; i<count; ++i)
	{
		namedRendertarget &nrt = namedRendertargetList[i];
		grcRenderTarget *rt = nrt.rendertarget;
		grcTexture *texture = NULL;
		if( rt )
		{
			texture = nrt.texture;

			if( nrt.delayedLoad && nrt.delayedLoadSetup == false && nrt.delayedLoadReady == true && nrt.modelidx != fwModelId::MI_INVALID)
			{
				// Patch up the drawable.				
				rmcDrawable *drawable = GetDrawable(&nrt);
			    if( drawable )
			    {
				    grmShaderGroup *shaderGroup = &drawable->GetShaderGroup();
				    grmShader *shader = shaderGroup->GetShaderPtr(nrt.shaderIdx);
				    shader->SetVar(nrt.diffuseTexVar,nrt.texture);
				    nrt.delayedLoadSetup = true;
			    }
			}

			// Add a temporary ref to the resource the render target belongs to.
			// Should be inside of a txd, a frag, etc.
			if( texture )
			{
				const strIndex strIdx = strStreamingEngine::GetStrIndex(&*texture);
				Assert(strIdx.IsValid());
				strStreamingModule *const module = strStreamingEngine::GetInfo().GetModule(strIdx);
				Assert(module);
				const strLocalIndex objIdx = module->GetObjectIndex(strIdx);
				module->AddRef(objIdx, REF_RENDER);
				gDrawListMgr->AddRefCountedModuleIndex(objIdx.Get(), module);
			}
			else
			{
				// Clear out the render target pointer that is going to be
				// stored.
				rt = NULL;
			}
		}

		out->rt  = rt;
		out->tex = texture;
		out->id  = nrt.uniqueId;
		++out;
	}
}

// This gets called every frame from CGtaRenderThreadGameInterface::PerformSafeModeOperations
void CRenderTargetMgr::CleanupNamedRenderTargets()
{
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		namedRendertarget &rt = namedRendertargetList[i];
		
		if( rt.release )
		{	
#if RSG_PC
			if (rt.release_count == 0)
#endif
			{
			if( rt.rendertarget )
				rt.rendertarget->Release();
			
			// we need to release our ref here as we're about to clear the entry in namedRendertargetList
			// and it won't be picked up for release in CRenderTargetMgr::Update
			if(rt.rendertarget && rt.modelidx != fwModelId::MI_INVALID && rt.drawableRefAdded)
			{
				RemoveRefFromDrawable(&rt);
				rt.isLinked = false;
				rt.drawableRefAdded = false;
			#if __BANK
				if (PARAM_namedrtdebug.Get())
				{
					Displayf("[RTM] Releasing (%s): (%u)\t ownerTag: %u\t", rt.name.GetCStr(), rt.modelidx, rt.owner);
				}
			#endif
			}
			
			rt.texture = NULL;			
			
		#if __BANK
			if (PARAM_namedrtdebug.Get())
			{
				Displayf("[RTM] Released (%s): (%u)\t ownerTag: %u\t", rt.name.GetCStr(), rt.modelidx, rt.owner);
			}
		#endif
			namedRendertargetList.DeleteFast(i);
			i--; // Due to the way the array delete stuff, we need to ensure we stay on the same line...
			}
#if RSG_PC
			else
				rt.release_count--;
#endif
		}
	}
}


void CRenderTargetMgr::Update()
{

}


void CRenderTargetMgr::ReleaseAllTargets()
{
	unsigned int modelidxList[NUM_NAMEDRENDERTARGETS];
	int namedRenderTargetEntry[NUM_NAMEDRENDERTARGETS];
	int modelidxCount = 0;

	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		namedRendertarget &rt = namedRendertargetList[i];
		if( rt.modelidx != fwModelId::MI_INVALID)
		{
			int j;
			for(j=0;j<modelidxCount;j++)
			{
				if( modelidxList[j] == rt.modelidx )
					break;
			}
			if( j == modelidxCount )
			{
				modelidxList[modelidxCount] = rt.modelidx;
				namedRenderTargetEntry[modelidxCount] = i;
				modelidxCount++;
			}
		}
		
		// Primed for cleanup
		rt.release = true;
#if RSG_PC
		rt.release_count = 0;
#endif
	}
	
	for(int i=0;i<modelidxCount;i++)
	{
		int curEntry = namedRenderTargetEntry[i];
		namedRendertarget &rt =  namedRendertargetList[curEntry];

		if (rt.drawableRefAdded)
		{
			Displayf("CRenderTargetMgr::ReleaseAllTargets: releasing %u", rt.modelidx);		
			RemoveRefFromDrawable(&rt);	
			rt.drawableRefAdded = false;
		}
	}
	
	CleanupNamedRenderTargets();
}

#if __SCRIPT_MEM_CALC
u32 CRenderTargetMgr::GetMemoryUsageForNamedRenderTargets(unsigned int ownerTag) const
{
	u32 TotalMemoryUsage = 0;
	for(int i=0;i<namedRendertargetList.GetCount();i++)
	{
		const namedRendertarget &rt = namedRendertargetList[i];
		if( rt.owner == ownerTag )
		{
			TotalMemoryUsage += rt.GetMemoryUsage();
		}
	}

	return TotalMemoryUsage;
}
#endif	//	__SCRIPT_MEM_CALC


const CRenderTargetMgr::namedRendertarget* CRenderTargetMgr::LinkNamedRendertargets(u32 modelidx, unsigned int ownerTag)
{
	if( namedRendertargetList.GetCount() == 0 )
		return NULL;

	CBaseModelInfo *modelinfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelidx)));
	namedRendertarget *rt = NULL;

	rmcDrawable *drawable = modelinfo->GetDrawable();
	if( drawable )
	{
		Assert(drawable);		

		const grmShaderGroup *shaderGroup = &drawable->GetShaderGroup();
		bool foundNewRT = false;
		
		for (u32 i = 0; i < shaderGroup->GetCount(); i++)
		{
			grmShader *shader = shaderGroup->GetShaderPtr(i);
			// If we got a diffuse texture, use it in the name
			grcEffectVar varId = shader->LookupVar("diffuseTex", FALSE);
			if(varId)
			{
				char texName[256];
				grcTexture *diffuseTex = NULL;
				shader->GetVar(varId, diffuseTex);

				if (diffuseTex != NULL)
				{
					StringNormalize(texName,diffuseTex->GetName(),sizeof(texName));
					fiAssetManager::RemoveExtensionFromPath(texName, sizeof(texName), texName);

					char *scriptRT = strstr(texName,"script_rt_");

					if( scriptRT )
					{
						char *rtName;
						rtName = texName + strlen("script_rt_");
						atFinalHashString RtName(rtName);
						
						rt = GetNamedRendertarget(RtName,ownerTag);
						if( rt )
						{
							Assertf(!rt->isLinked, "[RTM] render target '%s' has already been linked!", rtName);
							rt->release = false;
#if RSG_PC
							rt->release_count = 0;
#endif

							rt->modelidx = modelidx;
							rt->diffuseTexVar = varId;
							rt->shaderIdx = i;
							foundNewRT = true;

							if( !rt->delayedLoad )
							{
								foundNewRT = rt->CreateRenderTarget(diffuseTex);

								if (foundNewRT)
								{
									EnsureTextureGpuWritable(diffuseTex);
									rt->texture = diffuseTex;
									rt->isLinked = true;
								}
							}

							if (foundNewRT)
							{
								break;
							}
						}
					}
				}
			}
		}


		if( foundNewRT )
		{
			// cache drawable type and store index
			rt->drawableType = (u8)modelinfo->GetDrawableType();
			switch ((fwArchetype::DrawableType)rt->drawableType) 
			{
				case fwArchetype::DT_FRAGMENT:
					rt->fragIdx = static_cast<u16>(modelinfo->GetFragmentIndex());
					break;
				case fwArchetype::DT_DRAWABLE:
					rt->drawIdx = modelinfo->GetDrawableIndex();
					break;
				case fwArchetype::DT_DRAWABLEDICTIONARY:
					rt->drawDictIdx = static_cast<u16>(modelinfo->GetDrawDictIndex());
					rt->drawDictDrawableIdx = (u8)modelinfo->GetDrawDictDrawableIndex();
					break;
				default:
					break;
			}

			// add our permanent ref (kept for the lifetime of this render target)
			AddRefToDrawable(rt);
			rt->drawableRefAdded = true;
			Displayf("[RTM] Linked (%s): (%u)\t ownerTag: %u\t",rt->name.TryGetCStr(), modelidx, ownerTag);
		}
	}

	return rt;
}

void CRenderTargetMgr::LinkNamedRendertargets(const atFinalHashString& name, unsigned int ownerTag, grcTextureHandle texture, u32 modelIdx, fwArchetype::DrawableType assetType, s32 assetIdx)
{
	if (namedRendertargetList.GetCount() == 0)
		return;

	bool foundNewRT = false;

	namedRendertarget *rt = GetNamedRendertarget(name, ownerTag);
	if (rt)
	{
		Assertf(!rt->isLinked, "[RTM] render target '%s' has already been linked!", name.GetCStr()? name.GetCStr():"null");
		rt->release = false;
#if RSG_PC
		rt->release_count = 0;
#endif

		rt->modelidx = modelIdx;
		foundNewRT = true;

		if (!rt->delayedLoad)
		{
			foundNewRT = rt->CreateRenderTarget(texture);

			if (foundNewRT)
			{
				EnsureTextureGpuWritable(texture);
				rt->texture = texture;
				rt->isLinked = true;
			}
		}
	}

	if (foundNewRT)
	{
		// cache drawable type and store index
		rt->drawableType = (u8)assetType;
        rt->fragIdx = (u16)assetIdx;

		// add our permanent ref (kept for the lifetime of this render target)
		AddRefToDrawable(rt);
		rt->drawableRefAdded = true;
		Displayf("[RTM] Linked (%s): (%u)\t ownerTag: %u\t",rt->name.TryGetCStr(), modelIdx, ownerTag);
	}
}

bool CRenderTargetMgr::IsNamedRenderTargetLinked(u32 modelidx, unsigned int ownerTag) 
{
	if (modelidx != fwModelId::MI_INVALID)
	{
		for(int i=0;i<namedRendertargetList.GetCount();i++)
		{
			const namedRendertarget &rt = namedRendertargetList[i];
			if(rt.owner == ownerTag && rt.modelidx == modelidx && rt.isLinked)
			{
				namedRendertargetList[i].release = false;	//we need this RT, make sure delayed release isn't about to delete it.
			#if RSG_PC
				namedRendertargetList[i].release_count = 0;
			#endif
			#if __BANK
				if (PARAM_namedrtdebug.Get())
				{
					Displayf("[RTM] IsNamedRenderTargetLinked: RT cleared from release (%s): (%u)\t ownerTag: %u\t", rt.name.GetCStr(), rt.modelidx, rt.owner);
				}
			#endif
				return true;
			}
		}
	}

	return false;
}

//
//
// BS#5828732: works like IsNamedRenderTargetLinked(), but not changing release/release_count fields
//
bool CRenderTargetMgr::IsNamedRenderTargetLinkedConst(u32 modelidx, unsigned int ownerTag) const
{
	if(modelidx != fwModelId::MI_INVALID)
	{
		for(int i=0; i<namedRendertargetList.GetCount(); i++)
		{
			const namedRendertarget &rt = namedRendertargetList[i];
			if(rt.owner == ownerTag && rt.modelidx == modelidx && rt.isLinked)
			{
				return true;
			}
		}
	}

	return false;
}


void CRenderTargetMgr::ClearRenderPhasePointersNew()
{
	for (int x=0; x<CRenderTargetMgr::RTI_Count; x++)
	{
		SetRenderPhase((CRenderTargetMgr::RenderTargetId) x, (CRenderPhase *) NULL);
	}
}


rmcDrawable* CRenderTargetMgr::GetDrawable(const namedRendertarget* pRt)
{
	rmcDrawable* ret = NULL;

	switch (pRt->drawableType) 
	{
	case fwArchetype::DT_FRAGMENT:
		{
			Assert(pRt->fragIdx != strLocalIndex::INVALID_INDEX);
			fragType* pFragment = g_FragmentStore.Get(strLocalIndex(pRt->fragIdx));
			ret = (pFragment == NULL) ? NULL : pFragment->GetCommonDrawable();
		}
		break;
	case fwArchetype::DT_DRAWABLE:
		{
			Assert(pRt->drawIdx != strLocalIndex::INVALID_INDEX);
			ret = g_DrawableStore.Get(strLocalIndex(pRt->drawIdx));
		}
		break;
	case fwArchetype::DT_DRAWABLEDICTIONARY:
		{
			Assert(pRt->drawDictIdx != strLocalIndex::INVALID_INDEX);
			pgDictionary<rmcDrawable> * pDwd = g_DwdStore.Get(strLocalIndex(pRt->drawDictIdx));
			ret = (pDwd == NULL) ? NULL : pDwd->GetEntry(pRt->drawDictDrawableIdx);
		}
		break;
	default:
		break;
	} 

	return(ret);
}

void CRenderTargetMgr::ModifyDrawableRefCount(const namedRendertarget* pRt, bool bAddRef)
{
	strLocalIndex index;

	strStreamingModule* pModule = NULL;

	switch (pRt->drawableType) 
	{
	case (fwArchetype::DT_FRAGMENT):
		pModule = &g_FragmentStore;
		index = pRt->fragIdx;
		break;
	case (fwArchetype::DT_DRAWABLE):
		pModule = &g_DrawableStore;
		index = pRt->drawIdx;
		break;
	case (fwArchetype::DT_DRAWABLEDICTIONARY):
		pModule = &g_DwdStore;
		index = pRt->drawDictIdx;
		break;
	default:
		break;
	}

	if (!pModule)
		return;

	if (bAddRef)
		pModule->AddRef(index, REF_RENDER);
	else
		pModule->RemoveRef(index, REF_RENDER);

}

u32 CRenderTargetMgr::GetRenderTargetUniqueId(const char *name, unsigned int ownerTag)
{
	u32 uniqueIdHash = 0;
	
	char ownerTagNum[16];
	sprintf(ownerTagNum, "%u", ownerTag);

	uniqueIdHash = atPartialStringHash(name, uniqueIdHash);
	uniqueIdHash = atPartialStringHash("_", uniqueIdHash);
	uniqueIdHash = atPartialStringHash(ownerTagNum, uniqueIdHash);

	return atFinalizeHash(uniqueIdHash);
}
