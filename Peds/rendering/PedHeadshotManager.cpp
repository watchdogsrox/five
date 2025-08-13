//
// renderer/PedHeadshotManager.h
//
// Copyright (C) 2012-2014 Rockstar Games.  All Rights Reserved.
//
// Handles headshot requests of player peds
//

#include "Peds/rendering/PedHeadshotManager.h"

// Rage headers:
#include "file/device.h"
#include "file/stream.h"

#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "grcore/stateblock.h"
#include "grcore/quads.h"
#include "grcore/image.h"
#include "grcore/dds.h"
#include "grcore/texturegcm.h"
#include "grcore/wrapper_d3d.h"
#include "grmodel/shader.h"
#include "grmodel/shaderfx.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "crSkeleton/Skeleton.h"
#include "crSkeleton/SkeletonData.h"
#include "crSkeleton/BoneData.h"
#include "streaming/streamingengine.h"
#include "system/memory.h"

#include "fwdebug/picker.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponent.h"
#include "fwanimation/directorcomponentfacialrig.h"
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwanimation/facialclipsetgroups.h"
#include "fwdrawlist/drawlist.h"

// Game headers:
#include "animation/animBones.h"
#include "debug/MarketingTools.h"
#include "camera/CamInterface.h"
#include "Network/NetworkInterface.h"
#include "Peds/ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PlayerPed.h"
#include "Peds/rendering/pedVariationPack.h"
#include "peds/rendering/PedVariationDebug.h"
#include "renderer/DrawLists/drawlistMgr.h"
#include "scene/EntityTypes.h"
#include "scene/world/gameWorld.h"
#include "script/thread.h"
#include "script/script.h"
#include "Shaders/CustomShaderEffectPed.h"
#include "Stats/StatsInterface.h"
#include "streaming/streaming.h"
#include "system/interlocked.h"
#include "system/system.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/rendertargets.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/Util/Util.h"	 
#include "renderer/Lights/lights.h"
#include "renderer/Sprite2d.h"
#include "renderer/PostProcessFX.h"

#if __XENON
#include "grcore/texturexenon.h"
#include "system/XMemWrapper.h"
#include "system/xgraphics.h"
#endif

#if __D3D11
#include "grcore/texturefactory_d3d11.h"
#endif

#include "system/param.h" 
#if __BANK
PARAM(pedheadshotdebug,"Output debug info");
#endif

#define PEDHEADSHOT_USE_RENDERTARGET_POOLS  ( !RSG_PC && !RSG_DURANGO && !RSG_ORBIS )
using namespace rage;

#define PEDMUGSHOT_FILE_NAME_PATTERN "pedmugshot_%02d"

//OPTIMISATIONS_OFF()
static grcDepthStencilStateHandle ms_headshotDefDS = grcStateBlock::DSS_Invalid;
static grcDepthStencilStateHandle ms_headshotTestDS = grcStateBlock::DSS_Invalid;

//////////////////////////////////////////////////////////////////////////

const fwMvClipSetId BIND_POSE_CLIPSET_ID("mp_sleep",0xECD664BF);	// Animated bind pose contained in here
const fwMvClipId BIND_POSE_CLIP_ID("BIND_POSE_180",0x8EF0B2E4);		// Bind pose clip itself

//////////////////////////////////////////////////////////////////////////
//
#define USE_STB_COMPRESSION_ON_RENDERTHREAD		(__D3D11)

#define PEDHEADSHOT_USE_STB		(USE_STB_COMPRESSION_ON_RENDERTHREAD)

#if PEDHEADSHOT_USE_STB

#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"
#include "grcore/image_dxt.h"


#include "grcore/texturedefault.h"

#if RSG_PC && __D3D11
#include "grcore/texture_d3d11.h"
#include "grcore/rendertarget_d3d11.h"
#endif // RSG_PC && __D3D11

#if RSG_DURANGO
#include "grcore/texture_durango.h"
#include "grcore/rendertarget_durango.h"
#endif // RSG_DURANGO


bool PedHeadshot_Compress_STB(grcTexture* dst, const grcRenderTarget* src)
{
	Assert(src->GetWidth() == dst->GetWidth());
	Assert(src->GetHeight() == dst->GetHeight());

	const grcImage::Format dstFormat      = (grcImage::Format)dst->GetImageFormat();
	const bool bIsDXT5 = (dstFormat == grcImage::DXT5); // stb only supports DXT1 and DXT5
	const int  dstStep = (16*(bIsDXT5 ? 8 : 4))/(8*4); // block size divided by 4

	int width = src->GetWidth();
	int height = src->GetHeight();

#if (RSG_PC && __D3D11)
	grcTextureDX11* ptex = (grcTextureDX11*)dst;
	ptex->BeginTempCPUResource();

	grcRenderTargetD3D11* pRT = (grcRenderTargetD3D11*)src;
	pRT->GenerateMipmaps();
	pRT->UpdateLockableTexture(false);
#endif

	int mipMapCount = rage::Min(src->GetMipMapCount(),dst->GetMipMapCount());
	for (int mipIdx = 0; mipIdx < mipMapCount; ++mipIdx) 
	{
		grcTextureLock srcLock;
		if (!src->LockRect(0, mipIdx, srcLock, grcsRead)) 
			return false;

		grcTextureLock dstLock;
		if (!dst->LockRect(0, mipIdx, dstLock, grcsWrite | grcsAllowVRAMLock))
		{
			src->UnlockRect(srcLock);
			return false;
		}

		u8* dest = (u8*)dstLock.Base;
		int dstPitch = dstLock.Pitch;

		const int w = width >> mipIdx;
		const int h = height >> mipIdx;

		u8* srcTex = (u8*)srcLock.Base;
		for (int y = 0; y < h; y += 4)
		{
			const int by = y / 4;
			u8* dstRow = dest + (by * dstPitch);

			const u8* srcRow0 = (const u8*)srcTex + (y + 0) * srcLock.Pitch;
			const u8* srcRow1 = (const u8*)srcTex + (y + 1) * srcLock.Pitch;
			const u8* srcRow2 = (const u8*)srcTex + (y + 2) * srcLock.Pitch;
			const u8* srcRow3 = (const u8*)srcTex + (y + 3) * srcLock.Pitch;

			for (int x = 0; x < w; x += 4)
			{
				DXT::ARGB8888 temp[16];

				temp[0x00] = ((const DXT::ARGB8888*)srcRow0)[x + 0];
				temp[0x01] = ((const DXT::ARGB8888*)srcRow0)[x + 1];
				temp[0x02] = ((const DXT::ARGB8888*)srcRow0)[x + 2];
				temp[0x03] = ((const DXT::ARGB8888*)srcRow0)[x + 3];
				temp[0x04] = ((const DXT::ARGB8888*)srcRow1)[x + 0];
				temp[0x05] = ((const DXT::ARGB8888*)srcRow1)[x + 1];
				temp[0x06] = ((const DXT::ARGB8888*)srcRow1)[x + 2];
				temp[0x07] = ((const DXT::ARGB8888*)srcRow1)[x + 3];
				temp[0x08] = ((const DXT::ARGB8888*)srcRow2)[x + 0];
				temp[0x09] = ((const DXT::ARGB8888*)srcRow2)[x + 1];
				temp[0x0a] = ((const DXT::ARGB8888*)srcRow2)[x + 2];
				temp[0x0b] = ((const DXT::ARGB8888*)srcRow2)[x + 3];
				temp[0x0c] = ((const DXT::ARGB8888*)srcRow3)[x + 0];
				temp[0x0d] = ((const DXT::ARGB8888*)srcRow3)[x + 1];
				temp[0x0e] = ((const DXT::ARGB8888*)srcRow3)[x + 2];
				temp[0x0f] = ((const DXT::ARGB8888*)srcRow3)[x + 3];

#if !RSG_PC && !RSG_DURANGO
#if __XENON
				// argb -> rgba
				for (int i = 0; i < 16; i++)
				{
					const u8 tmp = temp[i].a;
					temp[i].a = temp[i].r;
					temp[i].r = temp[i].g;
					temp[i].g = temp[i].b;
					temp[i].b = tmp;
				}
#else
				// argb -> bgra
				for (int i = 0; i < 16; i++)
				{
					const u8 tmp1 = temp[i].a;
					temp[i].a = temp[i].b;
					temp[i].b = tmp1;
					const u8 tmp2 = temp[i].r;
					temp[i].r = temp[i].g;
					temp[i].g = tmp2;
				}
#endif
#endif
				stb_compress_dxt_block(dstRow + x * dstStep, (const u8*)temp, bIsDXT5 ? 1 : 0, STB_DXT_HIGHQUAL);
			}
		}

#if !RSG_PC && ! RSG_DURANGO
		// no need to unlock for pc since pc doesn't need gpu copy
		dst->UnlockRect(dstLock);
#endif
		// tile the destination texture after compression
		XENON_ONLY(dst->Tile(mipIdx);)

			src->UnlockRect(srcLock);
	}
#if (RSG_PC && __D3D11)
	ptex->EndTempCPUResource();
	pRT->UpdateLockableTexture(true);
#endif
	return true;
}
#endif	// PEDHEADSHOT_USE_STB

//////////////////////////////////////////////////////////////////////////
#if RSG_PC
static bool UseCpuRoundtrip()
{
	return GRCDEVICE.GetDxFeatureLevel() <= 1000 || GRCDEVICE.GetGPUCount() > 1;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotInfo::Reset()
{
	pPed = NULL;
	pSourcePed = NULL;

	texture = NULL;
	refAdded = false;
	txdId = -1;
	hash = 0;

	state = PHS_INVALID;
	refCount = 0;

	isActive = false;
	isInProcess = false;
	isRegistered = false;
	isWaitingOnRelease = false;
	hasTransparentBackground = false;
	forceHiRes = false;
	isPosed = false;
	isCloned = false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureBuilder::Init()
{
	m_pRTCol0 = NULL; 
	m_pRTCol0Small = NULL;
	m_pRTCol1 = NULL; 
	m_pRTColCopy = NULL;
	m_RTPoolIdCol = 0; 
	m_RTPoolIdDepth = 0; 
	m_state = kNotReady; 
	m_dstTextureRefAdded = false;
	m_bRenderTargetAcquired = false; 
	m_alphaThreshold = 0; 
	m_bUseGpuCompression = false;
	m_bUseSTBCompressionRenderThread = false;

	AllocateRenderTargetPool();

	GpuDXTCompressor::Init();

#if PHM_RT_RESIDENT
	if (PreAllocateRenderTarget() == false)
	{
#if __BANK
		if (PARAM_pedheadshotdebug.Get())
		{
			Displayf("[PHM] PedHeadshotManager::ProcessHeadshot: no memory to allocate render target");
		}
#endif
		// not enough memory to allocate render target
		return false;
	}
#endif	// PHM_RT_RESIDENT
	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::Shutdown()
{
	ReleaseRenderTarget();
#if PHM_RT_RESIDENT
	PostReleaseRenderTarget();
#endif
	ReleaseRenderTargetPool();

	GpuDXTCompressor::Shutdown();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::AllocateRenderTargetPool()
{
	//////////////////////////////////////////////////////////////////////////
	// Colour target

	// compute total size including mip levels
	u32 memSize = 0;
	u32 mipHeight = kRtHeight;
	for (u32 i = 0; i < kRtMips; i++)
	{
		memSize += mipHeight * kRtPitch;
		mipHeight = Max(mipHeight >> 1, 1u);
	}


	m_RTPoolSizeCol = pgRscBuilder::ComputeLeafSize(memSize, true);

	// set up pool settings
	grcRTPoolCreateParams params;
	params.m_Size = (u32)m_RTPoolSizeCol;
	params.m_HeapCount = 2;
	params.m_AllocOnCreate = false;
	params.m_InitOnCreate = false;
#if __GCM
	params.m_Alignment = kRtAligment;
	params.m_PhysicalMem = 0;
	params.m_Tiled = 0;
	params.m_Pitch = kRtPitch;	
#endif

#if PEDHEADSHOT_USE_RENDERTARGET_POOLS
	// allocate the pool for colour target	
	char name[128];
	sprintf(&name[0],"PedHeadshotTexBuilderCol");
	m_RTPoolIdCol = grcRenderTargetPoolMgr::CreatePool(name, params);
	Assert(m_RTPoolIdCol!=kRTPoolIDInvalid);
#endif

	//////////////////////////////////////////////////////////////////////////
	// Depth target
	memSize = kRtHeight * kRtPitch;
	m_RTPoolSizeDepth = pgRscBuilder::ComputeLeafSize(memSize, true);

	params.m_Size = (u32)m_RTPoolSizeDepth;
	params.m_HeapCount = 1;
	params.m_AllocOnCreate = false;
	params.m_InitOnCreate = false;
#if __GCM
	params.m_Alignment = kRtAligment;
	params.m_PhysicalMem = 0;
	params.m_Tiled = 0;
	params.m_Pitch = kRtPitch;	
#endif

#if PEDHEADSHOT_USE_RENDERTARGET_POOLS
	// allocate the pool for colour target	
	sprintf(&name[0],"PedHeadshotTexBuilderDepth");
	m_RTPoolIdDepth = grcRenderTargetPoolMgr::CreatePool(name, params);
	Assert(m_RTPoolIdDepth!=kRTPoolIDInvalid);
#endif

	// allocate a temporary colour target for ps3 and xenon.
	// we render the headshot to this target, and then use the 
	// dynamically allocated one to output the tonemapped version.
#if __XENON || __PS3
	grcTextureFactory::CreateParams paramsAlt;
	paramsAlt.Multisample = 0;
	paramsAlt.HasParent = true;
	paramsAlt.Parent = XENON_SWITCH(PEDHEADSHOTMANAGER.GetParentTarget(), NULL); 

	paramsAlt.UseFloat = false;
	paramsAlt.IsResolvable = true;
	paramsAlt.ColorExpBias = 0;
	paramsAlt.IsSRGB = false; 
	paramsAlt.IsRenderable = true;
	paramsAlt.AllocateFromPoolOnCreate = true;
	paramsAlt.MipLevels = 1;
	paramsAlt.Format = grctfA8R8G8B8;

	#if __XENON
		m_pRTCol0 = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER,"PedHeadshot Col 0", grcrtPermanent, kRtWidth, kRtHeight, 32, &paramsAlt, kUIPedHeadshotHeap);
	#else
		m_pRTCol0 = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "PedHeadshot Col 0", grcrtPermanent, kRtWidth, kRtHeight, 32, &paramsAlt, 5);
	#endif

	m_pRTCol0->AllocateMemoryFromPool();

#else
	grcTextureFactory::CreateParams paramsAlt;
	paramsAlt.Multisample = 0;
	paramsAlt.HasParent = true;
	paramsAlt.Parent = NULL; 

	paramsAlt.UseFloat = false;
	paramsAlt.IsResolvable = true;
	paramsAlt.ColorExpBias = 0;
	paramsAlt.IsSRGB = false; 
	paramsAlt.IsRenderable = true;
	paramsAlt.AllocateFromPoolOnCreate = true;
	paramsAlt.MipLevels = 1;
	paramsAlt.Format = grctfA8R8G8B8;
#if RSG_ORBIS
	paramsAlt.ForceNoTiling = true;
#endif // RSG_ORBIS
	m_pRTCol0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "PedHeadshot Col 0", grcrtPermanent, kRtWidth, kRtHeight, 32, &paramsAlt);
#if RSG_PC
	m_pRTColCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "PedHeadshot Col Copy", grcrtPermanent, kRtWidth, kRtHeight, 32, &paramsAlt);
#endif // RSG_PC
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::ReleaseRenderTargetPool()
{
#if PEDHEADSHOT_USE_RENDERTARGET_POOLS
	//////////////////////////////////////////////////////////////////////////
	// Colour
	grcRenderTargetPoolEntry* poolEntry = grcRenderTargetPoolMgr::GetPoolEntry(m_RTPoolIdCol);

	if (poolEntry)
	{
		// memory should have been previously released
		Assert(poolEntry->m_PoolMemory == NULL);
		grcRenderTargetPoolMgr::DestroyPool(m_RTPoolIdCol);
		m_RTPoolIdCol = kRTPoolIDInvalid;
	}

	//////////////////////////////////////////////////////////////////////////
	// Depth
	poolEntry = grcRenderTargetPoolMgr::GetPoolEntry(m_RTPoolIdDepth);

	if (poolEntry)
	{
		// memory should have been previously released
		Assert(poolEntry->m_PoolMemory == NULL);
		grcRenderTargetPoolMgr::DestroyPool(m_RTPoolIdDepth);
		m_RTPoolIdDepth = kRTPoolIDInvalid;
	}
#endif // PEDHEADSHOT_USE_RENDERTARGET_POOLS
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureBuilder::AllocateRenderTarget(bool bHasAlpha, bool bForceHiRes)
{
	// allocate memory for the pool
#if PEDHEADSHOT_USE_RENDERTARGET_POOLS
	if (m_RTPoolIdCol == kRTPoolIDInvalid)
	{
		Displayf("PedHeadshotTextureBuilder::AllocateRenderTarget: Render target pool has not been initialised");
		return false;
	}
#endif // PEDHEADSHOT_USE_RENDERTARGET_POOLS

	m_bUseSmallRT = ((bHasAlpha == false) && (bForceHiRes == false));


	//////////////////////////////////////////////////////////////////////////
	// Colour target
	void* memCol = 0;
#if PEDHEADSHOT_USE_RENDERTARGET_POOLS
	grcRenderTargetPoolEntry* poolEntry = grcRenderTargetPoolMgr::GetPoolEntry(m_RTPoolIdCol);
	if (poolEntry == NULL)
	{
		return false;
	}

	{
		MEM_USE_USERDATA(MEMUSERDATA_PEDHEADSHOT);
		memCol = strStreamingEngine::GetAllocator().Allocate(m_RTPoolSizeCol, kRtAligment, MEMTYPE_RESOURCE_VIRTUAL);
	}

	if (memCol == NULL) 
	{
		return false;
	}

	// allocate the render target
	poolEntry->m_PoolMemory = memCol;
#endif // PEDHEADSHOT_USE_RENDERTARGET_POOLS

#if !PHM_RT_RESIDENT
	grcTextureFactory::CreateParams params;
	params.PoolID = m_RTPoolIdCol;  
	params.PoolHeap = 0;
	params.Multisample = 0;
	params.HasParent = true;
#if __XENON
	params.Parent = m_pRTCol0; 
#else
	params.Parent = NULL; 
#endif
	params.UseFloat = false;
	params.IsResolvable = true;
	params.ColorExpBias = 0;
	params.IsSRGB = false; 
	params.IsRenderable = true;
	params.AllocateFromPoolOnCreate = true;
	params.MipLevels = kRtMips;
#if __PS3
	params.InTiledMemory = false;
	params.InLocalMemory = false;
#endif 
#if RSG_ORBIS
	params.ForceNoTiling = true;
	params.Format = grctfA8B8G8R8;
#else
	params.Format = grctfA8R8G8B8;
#endif // RSG_ORBIS

	int bpp = kRtColourBpp;
	int hSize = m_bUseSmallRT ? kRtWidth/2	: kRtWidth;
	int vSize = m_bUseSmallRT ? kRtHeight/2 : kRtHeight;

#if __D3D11
	params.Lockable = true;
#endif
	m_pRTCol1 = grcTextureFactory::GetInstance().CreateRenderTarget("Headshot Color 1", grcrtPermanent, hSize, vSize, bpp, &params);
#else	// !PHM_RT_RESIDENT
	m_pRTCol1 = m_bUseSmallRT ? m_pRTCol1S : m_pRTCol1L;
#endif	// !PHM_RT_RESIDENT

	if (m_pRTCol1 == NULL)
	{
		MEM_USE_USERDATA(MEMUSERDATA_PEDHEADSHOT);
		strStreamingEngine::GetAllocator().Free(memCol);
		m_pRTDepth->Release();
		return false;
	}

#if __XENON

	if (m_bUseSmallRT == false)
	{
		// Rage seems to always create the backend textures for render targets assuming they live in write combine memory.
		// However, the texture storage for this one (which we allocate manually just a few lines above) is in cacheable memory.
		// Cannot find a way to specify this via Rage, so let's do it manually (otherwise D3D asserts whenever we lock the texture
		// and I'm not sure whether this could actually be a problem):
		D3DTexture* pD3DTex = reinterpret_cast<D3DTexture*>(m_pRTCol1->GetTexturePtr());

		u32 baseAddr = pD3DTex->Format.BaseAddress;
		u32 mipAddr = pD3DTex->Format.MipAddress;

		XGTEXTURE_DESC texDesc;
		XGGetTextureDesc(pD3DTex,0,&texDesc);
		u32 physSize = XGSetTextureHeaderEx(texDesc.Width, texDesc.Height, 1, D3DUSAGE_CPU_CACHED_MEMORY, texDesc.Format, 0, XGHEADEREX_NONPACKED,0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, NULL, NULL, NULL);

		XGSetTextureHeader(
			texDesc.Width,
			texDesc.Height,
			1,
			D3DUSAGE_CPU_CACHED_MEMORY,
			texDesc.Format,
			0,
			0,
			XGHEADER_CONTIGUOUS_MIP_OFFSET,
			texDesc.RowPitch,
			pD3DTex,
			&physSize,
			NULL);

			pD3DTex->Format.BaseAddress = baseAddr;
			pD3DTex->Format.MipAddress = mipAddr;
	}

#endif

	sysInterlockedExchange((unsigned*)&m_state, kAcquired);

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
#if PHM_RT_RESIDENT
bool PedHeadshotTextureBuilder::PreAllocateRenderTarget()
{
	grcTextureFactory::CreateParams params;
	params.PoolID = m_RTPoolIdCol;  
	params.PoolHeap = 0;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = false;
	params.IsResolvable = true;
	params.ColorExpBias = 0;
	params.IsSRGB = false; 
	params.IsRenderable = true;
	params.AllocateFromPoolOnCreate = true;
	params.MipLevels = kRtMips;
#if RSG_ORBIS
	params.ForceNoTiling = true;
	params.Format = grctfA8B8G8R8;
#else
	params.Format = grctfA8R8G8B8;
#endif // RSG_ORBIS

	int bpp = kRtColourBpp;
	int hSize;
	int vSize;

#if __D3D11
	params.Lockable = true;
#endif
	hSize = kRtWidth;
	vSize = kRtHeight;
	m_pRTCol1L = grcTextureFactory::GetInstance().CreateRenderTarget("Headshot Color 1L", grcrtPermanent, hSize, vSize, bpp, &params);

	hSize = kRtWidth/2;
	vSize = kRtHeight/2;
	m_pRTCol1S = grcTextureFactory::GetInstance().CreateRenderTarget("Headshot Color 1S", grcrtPermanent, hSize, vSize, bpp, &params);
	if (!m_pRTCol1L || !m_pRTCol1S)
		return false;

	return true;
}
#endif	// PHM_RT_RESIDENT

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::Abort()
{
	sysInterlockedExchange((unsigned*)&m_state, kFailed);
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureBuilder::ProcessHeadshot(atHashString dstTXD, atHashString dstTEX, bool bHasAlpha, bool bForceHiRes)
{
	if (sysInterlockedRead((unsigned*)&m_state) != kNotReady)
	{
		// already processing a headshot
		return false;
	}

	if (!Verifyf(m_bRenderTargetAcquired == false, "PedHeadshotTextureBuilder::ProcessHeadshot: render target is still acquired"))
	{
		// this path shouldn't be hit
		return false;
	}

	if (AllocateRenderTarget(bHasAlpha, bForceHiRes) == false)
	{
	#if __BANK
		if (PARAM_pedheadshotdebug.Get())
		{
			Displayf("[PHM] PedHeadshotManager::ProcessHeadshot: no memory to allocate render target");
		}
	#endif
		// not enough memory to allocate render target
		return false;
	}

	// cache texture dictionary and texture names
	m_txdName = dstTXD;
	m_texName = dstTEX;
	strStreamingObjectName txdHash = strStreamingObjectName(m_txdName);

	strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlotFromHashKey(txdHash.GetHash()));

	// add slot to txd store
	if (txdSlot == -1)
	{
		txdSlot = g_TxdStore.AddSlot(txdHash.GetHash());
	}

	// check the specified texture dictionary actually exists
	if (Verifyf(g_TxdStore.IsValidSlot(txdSlot), "PedHeadshotTextureBuilder::ProcessHeadshot() - failed to get valid txd with name %s", txdHash.TryGetCStr()))
	{
		// mark as dirty so it doesn't get moved (otherwise we might end up writing to some random memory)
		CStreaming::SetDoNotDefrag(txdSlot, g_TxdStore.GetStreamingModuleId());

		// all good to go
		m_bRenderTargetAcquired = true;
		sysInterlockedExchange((unsigned*)&m_state, kWaitingOnTexture);


#if __BANK
		PEDHEADSHOTMANAGER.sm_numFramesToDelayTextureLoadingPrev = PEDHEADSHOTMANAGER.sm_numFramesToDelayTextureLoading;
#endif
		return true;
	}

	// render target allocation succeeded but texture dictionary doesn't exist
	ReleaseRenderTarget();
	return false;

}

//////////////////////////////////////////////////////////////////////////
// 
void PedHeadshotTextureBuilder::Update()
{
	if (sysInterlockedRead((unsigned*)&m_state) == kNotReady)
	{
		return;
	}

	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingOnTexture)
	{
		// check whether texture is loaded, if so, change state to allow
		// PedHeadshotManager to render ped
		UpdateTextureStreamingStatus();
	}
	else if (sysInterlockedRead((unsigned*)&m_state) ==  kWaitingForCompression)
	{				

	#if __BANK
		if (PEDHEADSHOTMANAGER.sm_maxNumFramesToWaitForRender == 0)
		{
			Abort();
			return;
		}
	#endif
		// check whether fence is still pending
		if (PEDHEADSHOTMANAGER.IsRenderingFinished() BANK_ONLY(|| PEDHEADSHOTMANAGER.sm_numFramesToSkipSafeWaitForCompression > 0)) // don't wait for fence!
		{
			// clean up any ped data we might no longer need
			PEDHEADSHOTMANAGER.CleanUpPedData();

			// kick off compression
			KickOffCompression();
		}
		else
		{
			// TBR: fix to avoid indefinitely stalling the rest of the queued requests when a headshot takes too long to generate
			// hacky temporary fix until a) i get a consistent repro b) i have more time to figure out when it goes wrong c) gpu dxt5 compression
			PEDHEADSHOTMANAGER.IncNumFramesWaitingForRender();
			if (PEDHEADSHOTMANAGER.m_numFramesWaitingForRender >= PEDHEADSHOTMANAGER.sm_maxNumFramesToWaitForRender)
			{
				Abort();
			}
		}
	}
	else if (sysInterlockedRead((unsigned*)&m_state) == kCompressing)
	{
		// allow output compressed texture to be released to PedHeadshotManager
		// if compression is finished
		UpdateCompressionStatus();
	}
	else if (sysInterlockedRead((unsigned*)&m_state) == kOutputTextureReleased)
	{
		// release resources and allow texture builder to handle pending requests
		// as compressed texture has already been claimed by PedHeadshotManager
		ReleaseRenderTarget();
		ReleaseRefs();
	}
	else if (sysInterlockedRead((unsigned*)&m_state) == kFailed)
	{
		// stay on kFailed state until PedHeadshotManager releases render target
		// a headshot might have been canceled while already in progress
		if (m_bRenderTargetAcquired == false)
		{
			ReleaseRenderTarget();
			ReleaseRefs();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::KickOffGpuCompression()
{
	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingForGpuCompression)
	{
		// kick off compression and bail if something went wrong
		if (GpuDXTCompressor::CompressDXT1(m_pRTCol1, m_dstTexture) == false)
		{
			sysInterlockedExchange((unsigned*)&m_state, kFailed);
			return;
		}

		sysInterlockedExchange((unsigned*)&m_state, kCompressionSubmitted);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::KickOffSTBCompressionRenderThread()
{
	int mipMapCount = rage::Min(m_dstTexture->GetMipMapCount(),m_pRTCol1->GetMipMapCount());
	bool bOk = MESHBLENDMANAGER.CompressDxtSTB(m_dstTexture, m_pRTCol1, mipMapCount);
	if (bOk == false)
	{
		sysInterlockedExchange((unsigned*)&m_state, kFailed);
		return;
	}

	sysInterlockedExchange((unsigned*)&m_state, kCompressionSubmitted);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::UpdateRender()
{
	if (sysInterlockedRead((unsigned*)&m_state) == kCompressionSubmitted)
	{
		sysInterlockedExchange((unsigned*)&m_state, kCompressionWait);
	}
	else if (sysInterlockedRead((unsigned*)&m_state) == kCompressionWait)
	{
#if RSG_PC
		if (GpuDXTCompressor::RequiresFinishing())
		{
			sysInterlockedExchange((unsigned*)&m_state, kCompressionWaitForFinish);
		}
		else
#endif
		sysInterlockedExchange((unsigned*)&m_state, kCompressionFinished);
	}
#if RSG_PC
	else if (sysInterlockedRead((unsigned*)&m_state) == kCompressionWaitForFinish)
	{
		if (!GpuDXTCompressor::FinishCompressDxt())
			return;
		sysInterlockedExchange((unsigned*)&m_state, kCompressionFinished);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::ReleaseRefs()
{
	// remove refs from txd and output texture
	if (m_dstTextureRefAdded)
	{
		strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_txdName.GetHash()));
		if (txdId != -1)
		{
		#if __BANK
			if (PARAM_pedheadshotdebug.Get())
			{
				Displayf("[PHM] PedHeadshotTextureBuilder::ReleaseRefs: texture num refs: %d", g_TxdStore.GetNumRefs(txdId));
			}
		#endif
			g_TxdStore.RemoveRef(txdId, REF_OTHER);
		
			m_dstTexture = NULL;

			m_txdName.Clear();
			m_texName.Clear();
		}
	#if __BANK
		else if (PARAM_pedheadshotdebug.Get())
		{
			Displayf("[PHM] PedHeadshotTextureBuilder::ReleaseRefs: invalid txd");
		}
	#endif
		m_dstTextureRefAdded = false;
	}
}

//////////////////////////////////////////////////////////////////////////
//
#if __XENON

void PedHeadshotTextureBuilder::UntileTarget()
{
	bool bOk = true;

	Assertf(XGraphicsExtMemBufferHelper::ms_bufferSize[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] == 0, "Multi-threaded concurrent access to ms_XGraphicsMemSize - if you want to support that, simply mark ms_XGraphicsMemSize as __THREAD.");
	XGraphicsExtMemBufferHelper::ms_bufferSize[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] = m_pRTCol1->GetWidth() * m_pRTCol1->GetHeight() * (m_pRTCol1->GetBitsPerPixel() >> 3);

	MEM_USE_USERDATA(MEMUSERDATA_PEDHEADSHOT);
	XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] = strStreamingEngine::GetAllocator().Allocate(XGraphicsExtMemBufferHelper::ms_bufferSize[XGRAPHICS_MB_USER_PEDHEADSHOTMGR], 16); // 256k for untiling
	bOk = XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] != NULL;

	if (bOk)
	{
		int mipMapCount = rage::Min((s8)m_pRTCol1->GetMipMapCount(), (s8)kRtMips);
		for (s32 mip = 0; mip < mipMapCount; ++mip)
		{
			m_pRTCol1->Untile(mip);
		}

		MEM_USE_USERDATA(MEMUSERDATA_PEDHEADSHOT);
		strStreamingEngine::GetAllocator().Free(XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR]);
		XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] = NULL;
	}
	else
	{
		Warningf("[PHM] PedHeadshotTextureBuilder::UntileTarget: out of memory for tiling buffer, aborting headshot request");
		Abort();
	}

	XGraphicsExtMemBufferHelper::ms_bufferSize[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] = 0;

}
#endif

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::UpdateCompressionStatus()
{
#if PEDHEADSHOT_USE_STB
	sysInterlockedExchange((unsigned*)&m_state, kCompressionFinished);
#else

	if (m_bUseGpuCompression)
	{
		return;
	}

	#if !__WIN32PC && !RSG_DURANGO 			// TODO: Need to copy the headshot render target to the compressed texture
	if (m_textureCompressionState.Poll())
	#endif
	{
		sysInterlockedExchange((unsigned*)&m_state, kCompressionFinished);
	}
#endif 
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::KickOffCompression()
{
#if !__WIN32PC && !RSG_DURANGO				// TODO: Need to copy the headshot render target to the compressed texture

	// gpu path!!
	if (m_bUseGpuCompression)
	{
		return;
	}

#if __XENON
	#if !SQUISH_USE_INLINE_UNTILE
		UntileTarget();
	#endif
	D3DTexture *destTex = static_cast<D3DTexture *>(m_pRTCol1->GetTexturePtr());
	destTex->Format.Tiled = 0; // on 360 turn of tiling, since the tiler stomps on memory past the resource in some cases
#endif

	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingForCompression)
	{

#if __BANK
		if (PEDHEADSHOTMANAGER.m_pCurrentEntry != NULL && PEDHEADSHOTMANAGER.m_pCurrentEntry->txdId != -1 && PARAM_pedheadshotdebug.Get())
		{
			if (Verifyf(g_TxdStore.HasObjectLoaded(PEDHEADSHOTMANAGER.m_pCurrentEntry->txdId), "[PHM] PedHeadshotTextureBuilder::KickOffCompression: txd %d is not loaded", PEDHEADSHOTMANAGER.m_pCurrentEntry->txdId.Get()))
			{
				Displayf("[PHM] PedHeadshotTextureBuilder::KickOffCompression: texture has %d refs", g_TxdStore.GetNumRefs(PEDHEADSHOTMANAGER.m_pCurrentEntry->txdId));
			}
		}
#endif
		

#if PEDHEADSHOT_USE_STB
		sysInterlockedExchange((unsigned*)&m_state, kCompressing);

		int mipMapCount = rage::Min(m_dstTexture->GetMipMapCount(),m_pRTCol1->GetMipMapCount());
		bool bOk = MESHBLENDMANAGER.CompressDxtSTB(m_dstTexture, m_pRTCol1, mipMapCount);
		if (bOk == false)
		{
			Abort();
		}
#else

		sysInterlockedExchange((unsigned*)&m_state, kCompressing);

		// default is kColourClusterFit which takes about 100ms for a 128x128.
		int flags = squish::kColourIterativeClusterFit;
		m_textureCompressionState.Compress(	m_pRTCol1, 
											m_dstTexture,
											flags,
											m_alphaThreshold,
											0);		// may want to set up a new scheduler slot for this.
#endif //PEDHEADSHOT_USE_STB
	}
	else
	{
		Warningf("[PHM] PedHeadshotTextureBuilder::KickOffCompression: aborting headshot request (state is %d)", (int)m_state);
		Abort();
	}

#else
	m_state = kCompressing;
	Assertf(0, "PedHeadshotTextureBuilder::KickOffCompression() - Not Implemented");
#endif
}

//////////////////////////////////////////////////////////////////////////
// 
void PedHeadshotTextureBuilder::UpdateTextureStreamingStatus()
{

#if __BANK
	if (PEDHEADSHOTMANAGER.sm_numFramesToDelayTextureLoading == -1)
	{
		return;
	}
	else if (PEDHEADSHOTMANAGER.sm_numFramesToDelayTextureLoading > 0)
	{
		PEDHEADSHOTMANAGER.sm_numFramesToDelayTextureLoading--;
		return;
	}
#endif

	// check if our TXD is loaded yet, if not bale
	strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_txdName.GetHash()));
	if(!g_TxdStore.HasObjectLoaded(txdId))
	{
		g_TxdStore.StreamingRequest(txdId, STRFLAG_FORCE_LOAD);
		return;  // let the higher level code know we did not get the texture yet.
	}
	// make sure out texture exists in the TXD
	fwTxd* pTxd = g_TxdStore.Get(txdId);

	grcTexture* pTex = pTxd->Lookup(m_texName.GetHash());
	if (Verifyf(pTex, "PedHeadshotTextureBuilder::UpdateTextureStreamingStatus() - Texture '%s' does not exist in txd '%s'",m_texName.TryGetCStr(),m_txdName.TryGetCStr()))
	{
		// okay save texture pointer and add refs
		m_dstTexture = pTex;

		// add ref for us
		g_TxdStore.AddRef(txdId, REF_OTHER);

		// add ref for manager
		PEDHEADSHOTMANAGER.AddRefToCurrentEntry();

		m_bUseGpuCompression = false;
	#if !RSG_DURANGO && !RSG_ORBIS
		#if !RSG_PC
		m_bUseGpuCompression = (m_dstTexture->GetImageFormat() == grcImage::DXT1);

		if (m_bUseGpuCompression && GpuDXTCompressor::AllocateRenderTarget(m_dstTexture) == false)
		{
			m_txdName.Clear();
			m_texName.Clear();
			Abort();
			return;
		}
		#else	// !RSG_PC
		m_bUseGpuCompression = (m_dstTexture->GetImageFormat() == grcImage::DXT1
			&& GpuDXTCompressor::FindTempDxtRenderTarget(m_dstTexture->GetWidth()>>2, m_dstTexture->GetHeight()>>2) != NULL);
		#endif	// !RSG_PC
	#endif
	#if USE_STB_COMPRESSION_ON_RENDERTHREAD
		m_bUseSTBCompressionRenderThread = (m_dstTexture->GetImageFormat() == grcImage::DXT1 || m_dstTexture->GetImageFormat() == grcImage::DXT5);
	#endif
	
		m_dstTextureRefAdded = true;
		sysInterlockedExchange((unsigned*)&m_state, kWaitingOnRender);

	#if __BANK
		PEDHEADSHOTMANAGER.sm_numFramesToDelayTextureLoading = PEDHEADSHOTMANAGER.sm_numFramesToDelayTextureLoadingPrev;
	#endif
	}
	else
	{
		// well darn, it's not in the txd, let's clear the txd hash so we don't keep looking for non existent textures.
		m_txdName.Clear();
		m_texName.Clear();
		Abort();
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureBuilder::IsReadyForRendering() const
{
	return 	(sysInterlockedRead((unsigned*)&m_state) == kWaitingOnRender);
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureBuilder::IsCompressionFinished() const
{
	return (sysInterlockedRead((unsigned*)&m_state) == kCompressionFinished);
}

//////////////////////////////////////////////////////////////////////////
//
grcTexture* PedHeadshotTextureBuilder::GetTexture(bool& bUsedGpuCompression)
{
	// if compression is finished release render target and refs
	if (sysInterlockedRead((unsigned*)&m_state) == kCompressionFinished)
	{
		grcTexture* pTexture = m_dstTexture;
		bUsedGpuCompression = m_bUseGpuCompression;
		m_bRenderTargetAcquired = false;
		m_bUseGpuCompression = false;
		ReleaseRefs();
		sysInterlockedExchange((unsigned*)&m_state, kOutputTextureReleased);

#if __XENON
		// Force a cache flush if we compressed on the cpu. Compression code locks and unlocks just once to cache the
		// pointers to the texture data and *then* runs the compression.
		// Make sure the changes are visible to the GPU before the texture gets used.
		if (pTexture && bUsedGpuCompression == false)
		{
			grcTextureLock texLock;
			if (pTexture->LockRect(0,0,texLock,grcsRead))
			{
				pTexture->UnlockRect(texLock);
			}
		}
#endif

		return pTexture;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
grcRenderTarget* PedHeadshotTextureBuilder::GetColourRenderTarget0()
{
	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingOnRender)
	{
		return m_pRTCol0;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
grcRenderTarget* PedHeadshotTextureBuilder::GetColourRenderTargetCopy()
{
	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingOnRender)
	{
		return m_pRTColCopy;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
grcRenderTarget* PedHeadshotTextureBuilder::GetColourRenderTarget1()
{
	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingOnRender)
	{
		return m_pRTCol1;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
grcRenderTarget* PedHeadshotTextureBuilder::GetDepthRenderTarget()
{
	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingOnRender)
	{
		return m_pRTDepth;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::SetReadyForCompression()
{
	Assertf((m_state == kWaitingOnRender), "PedHeadshotTextureBuilder::SetReadyForCompression: state is %d", m_state);
	if (sysInterlockedRead((unsigned*)&m_state) == kWaitingOnRender)
	{
		PEDHEADSHOTMANAGER.ResetNumFramesWaitingForRender();
		sysInterlockedExchange((unsigned*)&m_state, kWaitingForCompression);
	}

}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::SetReadyForGpuCompression()
{
	sysInterlockedExchange((unsigned*)&m_state, kWaitingForGpuCompression);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::RecoverFromFailure()
{
	// PedHeadshotManager has acknowledged texture builder failure,
	// rely on this to allow the texture builder to reset its state
	m_bRenderTargetAcquired = false;
	m_bUseGpuCompression = false;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureBuilder::ReleaseRenderTarget()
{
	Assertf(m_bRenderTargetAcquired == false, "PedHeadshotTextureBuilder::ReleaseRenderTarget: releasing render target while acquired");

	GpuDXTCompressor::ReleaseRenderTarget();

	m_bUseSmallRT = false;

	if (m_pRTCol1)
	{
#if !PHM_RT_RESIDENT
		m_pRTCol1->Release();
		m_pRTCol1 = NULL;
#endif

#if PEDHEADSHOT_USE_RENDERTARGET_POOLS
		grcRenderTargetPoolEntry* poolEntry = grcRenderTargetPoolMgr::GetPoolEntry(m_RTPoolIdCol);
		if (poolEntry)
		{
			// should only have one heap
			Assert(poolEntry && poolEntry->m_PoolHeaps.GetCount() == 2);
			Assert(poolEntry->m_PoolMemory);
			MEM_USE_USERDATA(MEMUSERDATA_PEDHEADSHOT);
			strStreamingEngine::GetAllocator().Free(poolEntry->m_PoolMemory);
			poolEntry->m_PoolMemory = NULL;
			sysInterlockedExchange((unsigned*)&m_state, kNotReady);
		}
#else
		sysInterlockedExchange((unsigned*)&m_state, kNotReady);
#endif // PEDHEADSHOT_USE_RENDERTARGET_POOLS
	}

	if (m_pRTDepth)
	{
		m_pRTDepth->Release();
		m_pRTDepth = NULL;

#if PEDHEADSHOT_USE_RENDERTARGET_POOLS
		grcRenderTargetPoolEntry* poolEntry = grcRenderTargetPoolMgr::GetPoolEntry(m_RTPoolIdDepth);
		if (poolEntry)
		{
			// should only have one heap
			Assert(poolEntry && poolEntry->m_PoolHeaps.GetCount() == 1);
			Assert(poolEntry->m_PoolMemory);
			MEM_USE_USERDATA(MEMUSERDATA_PEDHEADSHOT);
			strStreamingEngine::GetAllocator().Free(poolEntry->m_PoolMemory);
			poolEntry->m_PoolMemory = NULL;
			sysInterlockedExchange((unsigned*)&m_state, kNotReady);
		}
#else
		sysInterlockedExchange((unsigned*)&m_state, kNotReady);
#endif // PEDHEADSHOT_USE_RENDERTARGET_POOLS
	}
}

//////////////////////////////////////////////////////////////////////////
//
#if PHM_RT_RESIDENT
void PedHeadshotTextureBuilder::PostReleaseRenderTarget()
{
	if (m_pRTCol1S)
	{
		m_pRTCol1S->Release();
		m_pRTCol1S = NULL;
	}
	if (m_pRTCol1L)
	{
		m_pRTCol1L->Release();
		m_pRTCol1L = NULL;
	}
}
#endif


//////////////////////////////////////////////////////////////////////////
//
PedHeadshotManager* PedHeadshotManager::smp_Instance = 0;
grmMatrixSet* PedHeadshotManager::sm_pDummyMtxSet = NULL;
CLightSource PedHeadshotManager::sm_customLightSource[4];
CLightSource PedHeadshotManager::sm_defLightSource[4];
Vector4 PedHeadshotManager::sm_ambientLight[2];
char PedHeadshotManager::sm_textureNameCache[16];
const int PedHeadshotManager::INVALID_HANDLE = 0;
bool PedHeadshotManager::sm_bUseCustomLighting = false;
float PedHeadshotManager::sm_defaultVFOVDeg = 26.718f;
float PedHeadshotManager::sm_defaultAspectRatio = 1.0f;
float PedHeadshotManager::sm_defaultCamHeightOffset = 0.7f;
float PedHeadshotManager::sm_defaultCamDepthOffset = 0.7f;
float PedHeadshotManager::sm_defaultExposure = 0.0f;
Vec3V PedHeadshotManager::sm_posedPedCamPos;
Vec3V PedHeadshotManager::sm_posedPedCamAngles;
bool PedHeadshotManager::sm_bPosedPedRequestPending = false;
int PedHeadshotManager::sm_maxNumFramesToWaitForRender = 15;
s32 PedHeadshotManager::sm_prevForcedTechniqueGroup = -1;

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::Init() 
{

	PedHeadshotManager::sm_textureNameCache[0] = '\0';

	smp_Instance = rage_new PedHeadshotManager;

	smp_Instance->m_pViewport = rage_new grcViewport;
	smp_Instance->m_pViewport->Perspective(sm_defaultVFOVDeg, sm_defaultAspectRatio, 0.01f, 5000.0f);

	smp_Instance->m_entriesPool.Reserve(kMaxPedHeashots);

	for (int i = 0; i < kMaxPedHeashots; i++) 
	{
		PedHeadshotInfo info;
		smp_Instance->m_entriesPool.Push(info);
	}

	smp_Instance->m_delayedDeletionPed.Reset();

	smp_Instance->ResetCurrentEntry();

	smp_Instance->m_animDataState = kClipHelperReady;
	smp_Instance->m_clipDictionaryName = atHashString("taxi_hail",0x776190C3);
	smp_Instance->m_clipName = atHashString("fuck_u",0x2C91F157);

	smp_Instance->m_renderCompletionFence = GRCDEVICE.AllocFence();
	smp_Instance->m_renderState = kRenderIdle;
	smp_Instance->m_numFramesWaitingForRender = 0;
	smp_Instance->sm_maxNumFramesToWaitForRender = 15;

	BuildDummyMatrixSet();

	InitLightSources();

	smp_Instance->SetupViewport();
	DeferredRendererWrapper::InitialisePedHeadshotRenderer(&(smp_Instance->m_renderer));

	smp_Instance->m_textureBuilder.Init();
	smp_Instance->m_textureExporter.Init();

	sm_posedPedCamPos = Vec3V(0.0f, -2.0f, 0.4f);
	sm_posedPedCamAngles = Vec3V(V_ZERO);

	grcDepthStencilStateDesc ds;
	ds.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
	ds.StencilEnable           		= true;
	ds.StencilReadMask         		= 0xff;
	ds.StencilWriteMask        		= 0xff;
	ds.FrontFace.StencilFailOp      = grcRSV::STENCILOP_KEEP;
	ds.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	ds.FrontFace.StencilPassOp      = grcRSV::STENCILOP_REPLACE;
	ds.FrontFace.StencilFunc        = grcRSV::CMP_ALWAYS;
	ds.BackFace                     = ds.FrontFace;
	ms_headshotDefDS = grcStateBlock::CreateDepthStencilState(ds);

	ds.DepthEnable					= false;
	ds.DepthWriteMask				= 0;
	ds.StencilReadMask              = 0xff;
	ds.StencilWriteMask             = 0;
	ds.FrontFace.StencilFailOp      = grcRSV::STENCILOP_KEEP;
	ds.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	ds.FrontFace.StencilPassOp      = grcRSV::STENCILOP_KEEP;
	ds.FrontFace.StencilFunc        = grcRSV::CMP_EQUAL;
	ds.BackFace                     = ds.FrontFace;
	ms_headshotTestDS = grcStateBlock::CreateDepthStencilState(ds);

#if __BANK
	PEDHEADSHOTMANAGER.UpdateEntryNamesList(PEDHEADSHOTMANAGER.m_entriesPool);
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::InitLightSources() 
{

	sm_defLightSource[0] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(4.175f, 0.700f, 1.350f),	Vector3(1.000f, 1.000f, 1.000f),	4.000000f, LIGHT_ALWAYS_ON);
	sm_defLightSource[0].SetRadius(10.000000f);
	sm_defLightSource[1] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(-1.100f, 0.750f, -0.050f),	Vector3(1.000f, 0.866f, 0.500f),	1.600000f, LIGHT_ALWAYS_ON);
	sm_defLightSource[1].SetRadius(5.000000f);
	sm_defLightSource[2] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(-1.050f, -1.225f, 1.50f),	Vector3(0.709f, 0.847f, 1.000f),	4.000000f, LIGHT_ALWAYS_ON);
	sm_defLightSource[2].SetRadius(20.000000f);
	sm_defLightSource[3] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(3.350f, 3.600f, 8.975f),	Vector3(0.741f, 0.964f, 1.000f),	2.340000f, LIGHT_ALWAYS_ON);
	sm_defLightSource[3].SetRadius(20.000000f);

	sm_ambientLight[0] = Vector4(0.439f, 0.596f, 0.709f, 1.0f);
	sm_ambientLight[1] = Vector4(0.078f, 0.262f, 0.360f, 1.0f);

	sm_customLightSource[0] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(0.000000f, -2.000000f, 0.750000f), Vector3(1.000000f, 1.000000f, 1.000000f), 4.000000f, LIGHT_ALWAYS_ON);
	sm_customLightSource[0].SetRadius(30.000000f);
	sm_customLightSource[1] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(-1.000000f, 0.000000f, 1.250000f), Vector3(0.662745f, 0.901961f, 0.921569f), 2.500000f, LIGHT_ALWAYS_ON);
	sm_customLightSource[1].SetRadius(20.000000f);
	sm_customLightSource[2] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(1.000000f, 0.000000f, 1.250000f), Vector3(0.662745f, 0.901961f, 0.921569f), 2.500000f, LIGHT_ALWAYS_ON);
	sm_customLightSource[2].SetRadius(10.000000f);
	sm_customLightSource[3] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(0.000000f, -2.000000f, 0.750000f), Vector3(0.980392f, 0.956863f, 0.635294f), 3.000000f, LIGHT_ALWAYS_ON);
	sm_customLightSource[3].SetRadius(10.000000f);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::Shutdown() 
{
	GRCDEVICE.CpuFreeFence(smp_Instance->m_renderCompletionFence);
	smp_Instance->m_headShotAnimRequestHelper.Release();
	smp_Instance->m_textureBuilder.Shutdown();
	smp_Instance->m_textureExporter.Shutdown();
	delete smp_Instance->m_pViewport;
	delete smp_Instance;
	smp_Instance = NULL; 
	grmMatrixSet::Free(sm_pDummyMtxSet);

}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::BuildDummyMatrixSet()
{
#if RSG_SM_50
	const u32 boneCount = 255;
#else
	const u32 boneCount = 200;
#endif
	sm_pDummyMtxSet = grmMatrixSet::Create(boneCount);
	Assertf(sm_pDummyMtxSet, "PedHeadshotManager::BuildDummyMatrixSet: dummy matrix set failed to allocate");

	if (sm_pDummyMtxSet)
	{

		const Matrix34 mat(Matrix34::IdentityType);
		const u32 numMatrices = (u32)sm_pDummyMtxSet->GetMatrixCount();


		grmMatrixSet::MatrixType* pMatrices = sm_pDummyMtxSet->GetMatrices();
		for (u32 i = 0; i < numMatrices; i++) 
		{
			pMatrices[i].Identity();
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::ResetCurrentEntry()
{
	m_pCurrentEntry = NULL;
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotHandle PedHeadshotManager::RegisterPedWithAnimPose(CPed* pPed, atHashString animClipDictionaryName, atHashString animClipName, Vector3& camPos, Vector3& camAngles, bool bNeedTransparentBackground) 
{
	// continue only if there's no pending request for another posed ped
	if (CanRequestPoseForPed() == false || animClipDictionaryName.IsNull() || animClipName.IsNull())
	{
		return INVALID_HANDLE;
	}

	// check if dictionary exists
	strLocalIndex animDictIndex = strLocalIndex(fwAnimManager::FindSlot(animClipDictionaryName));
	if (fwAnimManager::IsValidSlot(animDictIndex) == false)
	{
		Assertf(0,"PedHeadshotManager::RegisterPedWithAnimPose: Invalid clip dictionary");
		return INVALID_HANDLE;
	}

	PedHeadshotHandle handle = RegisterPed(pPed, bNeedTransparentBackground);

	// regular registration failed
	if (handle == INVALID_HANDLE)
	{
		return handle;
	}

	// Mark it as a posed ped
	PedHeadshotInfo& info = m_entriesPool[HandleToSlotIndex(handle)];
	info.isPosed = true;

	// All good to go - it might still fail loading the animation data
	sm_bPosedPedRequestPending = true;
	sm_posedPedCamPos = VECTOR3_TO_VEC3V(camPos);
	sm_posedPedCamAngles = VECTOR3_TO_VEC3V(camAngles);
	m_clipDictionaryName = animClipDictionaryName;
	m_clipName = animClipName;

	return handle;
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotHandle PedHeadshotManager::RegisterPedWithAnimPose(CPed* pPed, bool bNeedTransparentBackground)
{
	Vector3 camPos =  VEC3V_TO_VECTOR3(sm_posedPedCamPos);
	Vector3 camAngles =  VEC3V_TO_VECTOR3(sm_posedPedCamAngles);
	return RegisterPedWithAnimPose(pPed, m_clipDictionaryName, m_clipName, camPos, camAngles, bNeedTransparentBackground);
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::InitPosedPed(PedHeadshotInfo& entry) 
{
	if (Verifyf(entry.isPosed, "PedHeadshotManager::InitPosedPed: ped has not been flagged as a posed ped"))
	{
		return InitClonedPed(entry);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::InitClonedPed(PedHeadshotInfo& entry) 
{

	CPed* pPed = entry.pPed;

	if (pPed == NULL)
	{
		return false;
	}

	// Ped initialisation
	pPed->SetBaseFlag(fwEntity::IS_FIXED);
	pPed->DisableCollision();
	pPed->DisablePhysicsIfFixed();
	pPed->GetMotionData()->Reset();
	pPed->SetHairScaleLerp(false);
	pPed->SetClothForcePin(1);

	CGameWorld::Remove(pPed);

	Matrix34 mat;
	mat.Identity();
	mat.FromEulersYXZ(Vector3(0.0f, 0.0f, DEG2RAD(180.0f)));
	mat.d.Set(Vector3(0.0f,0.0f,-0.11f));	// Sufficiently far away from the origin to avoid asserts about peds at the origin during replay playback

	pPed->SetMatrix(mat, false, false, false);
	pPed->SetLodMultiplier(1000.0f);		// multiplier used in AddHeadshotToDrawList

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotHandle PedHeadshotManager::RegisterPed(CPed* pPed, bool bNeedTransparentBackground, bool bForceHiRes) 
{
	if (pPed == NULL)
	{	
		Warningf("PedHeadshotManager::RegisterPed: pPed is NULL");
		return INVALID_HANDLE;
	}

	// try to find the ped in an already registered entry
	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		if (	m_entriesPool[i].pSourcePed == pPed &&
				m_entriesPool[i].isRegistered &&
				m_entriesPool[i].isWaitingOnRelease == false &&
				m_entriesPool[i].hasTransparentBackground == bNeedTransparentBackground &&
				m_entriesPool[i].forceHiRes == bForceHiRes
			)
		{
			m_entriesPool[i].refCount++;
		#if __BANK
			if (PARAM_pedheadshotdebug.Get())
			{
				Displayf("[PHM] PedHeadshotManager::RegisterPed (%d): \"%s\" (ref count: %d)", i+1, pPed->GetDebugName(), m_entriesPool[i].refCount);
			#if !__FINAL
				if (CTheScripts::GetCurrentGtaScriptThread() != NULL)
				{
					scrThread::PrePrintStackTrace();
				}
			#endif
			}
		#endif
			return SlotIndexToHandle(i);
		}
	}

	// ped is not registered, try finding an available entry
	int entryFound = -1;

	// only the first entry (i.e. the one that will use pedmugshot_01) can use
	// a transparent background
	// 0    - DXT5 128x128
	// 1- 7 - DXT1 128x128
	// 7-34 - DXT1 64x64
	if (bNeedTransparentBackground)
	{
		if (m_entriesPool.GetCount() > 0 && m_entriesPool[0].isRegistered == false && IsTextureAvailableForSlot(0))
		{
			m_entriesPool[0].isActive = false;
			m_entriesPool[0].isRegistered = true;
			m_entriesPool[0].hasTransparentBackground = true;
			m_entriesPool[0].forceHiRes = false;
			m_entriesPool[0].refCount = 1;
			m_entriesPool[0].pPed = pPed;
			m_entriesPool[0].pSourcePed = pPed;
			m_entriesPool[0].state = PHS_QUEUED;
			entryFound = 0;

		#if __BANK
			if (PARAM_pedheadshotdebug.Get())
			{
				Displayf("[PHM] PedHeadshotManager::RegisterPed (%d): \"%s\" (ref count: %d)", 1, pPed->GetDebugName(), m_entriesPool[0].refCount);
			#if !__FINAL
				if (CTheScripts::GetCurrentGtaScriptThread() != NULL)
				{
					scrThread::PrePrintStackTrace();
				}
			#endif
			}
		#endif
		}
	}
	else if (bForceHiRes)
	{
		if(m_entriesPool.GetCount() >= (kMaxHiResPedHeashots+1))
		for (int i=1; i<(kMaxHiResPedHeashots+1); i++)				// 1..7
		{
			if (m_entriesPool[i].isRegistered == false && IsTextureAvailableForSlot(i))
			{
				m_entriesPool[i].isActive = false;
				m_entriesPool[i].isRegistered = true;
				m_entriesPool[i].hasTransparentBackground = false;
				m_entriesPool[i].forceHiRes = true;
				m_entriesPool[i].refCount = 1;
				m_entriesPool[i].pPed = pPed;
				m_entriesPool[i].pSourcePed = pPed;
				m_entriesPool[i].state = PHS_QUEUED;

			#if __BANK
				if (PARAM_pedheadshotdebug.Get())
				{
					Displayf("[PHM] PedHeadshotManager::RegisterPed (%d): \"%s\" (ref count: %d)", i+1, pPed->GetDebugName(), m_entriesPool[i].refCount);
				#if !__FINAL
					if (CTheScripts::GetCurrentGtaScriptThread() != NULL)
					{
						scrThread::PrePrintStackTrace();
					}
				#endif
				}
			#endif

				entryFound = i;
				break;
			}
		}
	}
	else
	{
		// start at the end so we don't use the transparent slot
		// unless it's the only one available
		if(m_entriesPool.GetCount() >= (kMaxHiResPedHeashots+2))
		for (int i=m_entriesPool.GetCount()-1; i>kMaxHiResPedHeashots; i--)		// 33..8
		{
			if (m_entriesPool[i].isRegistered == false && IsTextureAvailableForSlot(i))
			{
				m_entriesPool[i].isActive = false;
				m_entriesPool[i].isRegistered = true;
				m_entriesPool[i].hasTransparentBackground = false;
				m_entriesPool[i].forceHiRes = false;
				m_entriesPool[i].refCount = 1;
				m_entriesPool[i].pPed = pPed;
				m_entriesPool[i].pSourcePed = pPed;
				m_entriesPool[i].state = PHS_QUEUED;
			#if __BANK
				if (PARAM_pedheadshotdebug.Get())
				{
					Displayf("[PHM] PedHeadshotManager::RegisterPed (%d): \"%s\" (ref count: %d)", i+1, pPed->GetDebugName(), m_entriesPool[i].refCount);
				#if !__FINAL
					if (CTheScripts::GetCurrentGtaScriptThread() != NULL)
					{
						scrThread::PrePrintStackTrace();
					}
				#endif
				}
			#endif
				entryFound = i;
				break;
			}
		}
	}


	return SlotIndexToHandle(entryFound);
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotHandle PedHeadshotManager::GetRegisteredWithTransparentBackground() const
{
	// try to find the ped in an already registered entry
	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		if (m_entriesPool[i].isRegistered && m_entriesPool[i].hasTransparentBackground)
		{
			return SlotIndexToHandle(i);
		}
	}

	return SlotIndexToHandle(-1);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::UnregisterPed(PedHeadshotHandle handle)
{	
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::UnregisterPed: index out of range (%d)", slot) &&
		m_entriesPool[slot].isRegistered == true)
	{
		m_entriesPool[slot].refCount--;
		Assertf(m_entriesPool[slot].refCount >= 0, "PedHeadshotManager::UnregisterPed (%d): headshot has been unregistered too many times (%d)", slot+1, m_entriesPool[slot].refCount);

#if __BANK
		if (PARAM_pedheadshotdebug.Get())
		{
			if (m_entriesPool[slot].pSourcePed != NULL)
			{
				Displayf("[PHM] PedHeadshotManager::UnregisterPed (%d): \"%s\" (ref count: %d)", slot+1, m_entriesPool[slot].pSourcePed->GetDebugName(), m_entriesPool[slot].refCount);
			}
			else
			{
				Displayf("[PHM] PedHeadshotManager::UnregisterPed (%d): \"NULL PED\" (ref count: %d)", slot+1, m_entriesPool[slot].refCount);
			}
		#if !__FINAL
			if (CTheScripts::GetCurrentGtaScriptThread() != NULL)
			{
				scrThread::PrePrintStackTrace();
			}
		#endif

		}
#endif
		if (m_entriesPool[slot].refCount == 0)
		{
		#if __BANK
			if (PARAM_pedheadshotdebug.Get() && m_entriesPool[slot].refAdded)
			{
				if (Verifyf(m_entriesPool[slot].txdId.Get() != -1 && g_TxdStore.HasObjectLoaded(strLocalIndex(m_entriesPool[slot].txdId.Get())), "[PHM] PedHeadshotManager::UnregisterPed (%d): texture had a ref added but now is not loaded (txd slot: %d)", slot+1, m_entriesPool[slot].txdId.Get()))
				{				
					Displayf("[PHM] PedHeadshotManager::UnregisterPed (%d): (texture has %d refs)", slot+1, g_TxdStore.GetNumRefs(m_entriesPool[slot].txdId));
				}
			}
		#endif
			m_entriesPool[slot].isWaitingOnRelease = true;
			m_entriesPool[slot].isActive = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
u32 PedHeadshotManager::GetRegisteredCount() const
{
	u32 count = 0;
	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		if (m_entriesPool[i].isRegistered)
		{
			count++;
		}
	}

	return count;
}

//////////////////////////////////////////////////////////////////////////
//
int PedHeadshotManager::GetNumRefsforEntry(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::GetNumRefsForEntry: index out of range (%d)", slot))
	{
		return m_entriesPool[slot].refCount;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
int PedHeadshotManager::GetTextureNumRefsforEntry(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::GetTextureNumRefsForEntry: index out of range (%d)", slot))
	{
		if (m_entriesPool[slot].txdId != -1 && g_TxdStore.HasObjectLoaded(strLocalIndex(m_entriesPool[slot].txdId.Get())))
		{
			return g_TxdStore.GetNumRefs(m_entriesPool[slot].txdId);
		}

	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
//
u32 PedHeadshotManager::GetTextureHash(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::GetTextureHash: index out of range (%d)", slot)
		&& m_entriesPool[slot].isActive && m_entriesPool[slot].isWaitingOnRelease == false)
	{
		// check we've got a valid txd slot
		if (m_entriesPool[slot].txdId != -1)
		{
			// check it's got at least one ref
			int numRefs = g_TxdStore.GetNumRefs(m_entriesPool[slot].txdId);
			if (numRefs > 0)
			{
				return m_entriesPool[slot].hash;
			}

			Assertf(numRefs > 0, "PedHeadshotManager::GetTextureHash: active slot %d using a txd (%d) with no refs", slot, m_entriesPool[slot].txdId.Get());
		}

	}

#if __ASSERT
	if (bInRange && m_entriesPool[slot].isActive && m_entriesPool[slot].isWaitingOnRelease == false)
	{
		Assertf(m_entriesPool[slot].txdId != -1, "PedHeadshotManager::GetTextureName: active slot %d referencing an invalid txd (-1)", slot);
	}
#endif

	return 0;
}


//////////////////////////////////////////////////////////////////////////
//
const char* PedHeadshotManager::GetTextureName(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::GetTextureName: index out of range (%d)", slot)
		&& m_entriesPool[slot].isActive && m_entriesPool[slot].isWaitingOnRelease == false)
	{

		// check we've got a valid txd slot
		if (m_entriesPool[slot].txdId != -1)
		{
			// check it's got at least one ref
			int numRefs = g_TxdStore.GetNumRefs(m_entriesPool[slot].txdId);
			if (numRefs > 0)
			{
				sprintf(&sm_textureNameCache[0], PEDMUGSHOT_FILE_NAME_PATTERN, slot+1);
				return &sm_textureNameCache[0];
			}

			Assertf(numRefs > 0, "PedHeadshotManager::GetTextureName: active slot %d using a txd (%d) with no refs", slot, m_entriesPool[slot].txdId.Get());
		}
	}

#if __ASSERT
	if (bInRange && m_entriesPool[slot].isActive && m_entriesPool[slot].isWaitingOnRelease == false)
	{
		Assertf(m_entriesPool[slot].txdId != -1, "PedHeadshotManager::GetTextureName: active slot %d referencing an invalid txd (-1)", slot);
	}
#endif

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotState PedHeadshotManager::GetPedHeashotState(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::GetPedHeashotState: index out of range (%d)", slot))
	{
		return m_entriesPool[slot].state;
	}

	return PHS_INVALID;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::IsAnyRegistered() const
{
	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		if (m_entriesPool[i].isRegistered)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::IsRegistered(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::IsRegistered: index out of range (%d)", slot))
	{
		return m_entriesPool[slot].isRegistered && m_entriesPool[slot].isWaitingOnRelease == false;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::IsActive(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::IsActive: index out of range (%d)", slot))
	{
		return m_entriesPool[slot].isActive &&  (m_entriesPool[slot].isWaitingOnRelease == false);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::HasTransparentBackground(PedHeadshotHandle handle) const
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::HasTransparentBackground: index out of range (%d)", slot))
	{
		return m_entriesPool[slot].hasTransparentBackground;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//
grcTexture* PedHeadshotManager::GetTexture(PedHeadshotHandle handle)
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::GetTexture: index out of range (%d)", slot)
		&& m_entriesPool[slot].isActive && m_entriesPool[slot].isWaitingOnRelease == false)
	{
		// check we've got a valid txd slot
		if (m_entriesPool[slot].txdId != -1)
		{
			// check it's got at least one ref
			int numRefs = g_TxdStore.GetNumRefs(m_entriesPool[slot].txdId);
			if (numRefs > 0)
			{
				return m_entriesPool[slot].texture;
			}

			Assertf(numRefs > 0, "PedHeadshotManager::GetTexture: active slot %d using a txd (%d) with no refs", slot, m_entriesPool[slot].txdId.Get());
		}
	}

#if __ASSERT
	if (bInRange && m_entriesPool[slot].isActive && m_entriesPool[slot].isWaitingOnRelease == false)
	{
		Assertf(m_entriesPool[slot].txdId != -1, "PedHeadshotManager::GetTexture: active slot %d referencing an invalid txd (-1)", slot);
	}
#endif

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::SetUseCustomLighting(bool bEnable) 
{
	sm_bUseCustomLighting = bEnable;
}

//////////////////////////////////////////////////////////////////////////
// 
void PedHeadshotManager::SetCustomLightParams(int index, const Vector3& pos, const Vector3& col, float intensity, float radius) 
{
	if (Verifyf(index >= 0 && index < 4, "PedHeadshotManager::SetCustomLightParams: light index out of range (%d)", index))
	{
		sm_customLightSource[index].SetPosition(pos);
		sm_customLightSource[index].SetColor(col);
		sm_customLightSource[index].SetIntensity(intensity);
		sm_customLightSource[index].SetRadius(radius);
	}
}

//////////////////////////////////////////////////////////////////////////
// 
void PedHeadshotManager::Update()
{
	// update texture builder first
	m_textureBuilder.Update();

	// update texture exporter
	m_textureExporter.Update();

	// return if there are no active requests
	if (IsAnyRegistered() == false)
	{
		return;
	}

	// process released entries
	ProcessReleasedEntries();

	// deal with any entry that's already being processed by the texture builder
	bool bContinueUpdate = UpdateCurrentEntry();
	
	// check if the texture builder has failed
	bContinueUpdate = bContinueUpdate && UpdateTextureBuilderStatus();

	// add next registered entry to texture builder
	bContinueUpdate = bContinueUpdate && ProcessPendingEntries();

#if __BANK
	// simulates the case where the entry currently getting processed gets released;
	// this shouldn't be a problem: the entry will be eventually released, but only
	// once it gets processed (i.e.: rendered and compressed).
	if (m_pCurrentEntry && sm_bReleaseNextCurrentEntry)
	{
		m_pCurrentEntry->isWaitingOnRelease = true;
		m_pCurrentEntry->isActive = false;
		sm_bReleaseNextCurrentEntry = false;
	}
#endif

}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::CleanUpPedData()
{
	if (m_pCurrentEntry == NULL)
	{
		return false;
	}

	if (m_pCurrentEntry->isPosed)
	{
		// no other way we should have ended up here
		Assert(m_animDataState == kClipHelperDataWaitingForRendering);

		// release clips
		m_clipRequestHelper.ReleaseClips();
	
		// delete the cloned ped in UpdateSafe
		sm_bPosedPedRequestPending = false;
	}

	MarkClonePedForDeletion(*m_pCurrentEntry);

	// make the clip helper available again
	m_animDataState = kClipHelperReady;


	return true;
}

//////////////////////////////////////////////////////////////////////////
// 
bool PedHeadshotManager::ProcessReleasedEntries()
{
	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		PedHeadshotInfo& entry = m_entriesPool[i];

		if (entry.isWaitingOnRelease	== true && 
			entry.isInProcess			== false)
		{
			ReleaseEntry(entry);
		}

	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::UpdateSafe()
{
	int pedCount = PEDHEADSHOTMANAGER.m_delayedDeletionPed.GetCount();
	if (pedCount != 0)
	{
		CPedFactory* pFactory = CPedFactory::GetFactory();
		if (pFactory == NULL)
		{ 
			return;
		}

		for (int i = 0; i < pedCount; i++)
		{
			CPed* pPed = PEDHEADSHOTMANAGER.m_delayedDeletionPed[i];
			pFactory->DestroyPed(pPed);
		}

		PEDHEADSHOTMANAGER.m_delayedDeletionPed.Reset();
	}

#if __BANK
	PEDHEADSHOTMANAGER.UpdateDebug();
#endif

	PEDHEADSHOTMANAGER.Update();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::ReleaseEntry(PedHeadshotInfo& entry)
{
	MarkClonePedForDeletion(entry);
	RemoveRefFromEntry(entry);
	entry.Reset();
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::IsCurrentEntryReadyForRendering()
{
	if (m_pCurrentEntry == NULL)
	{
		return false;
	}
	
	Assert(m_pCurrentEntry->isInProcess);

	// if it's a streamed ped, make sure its PedRenderGfx instance is ready, if it isn't keep checking every update
	if (m_pCurrentEntry->pPed != NULL)
	{
		const CPedModelInfo* pModelInfo = m_pCurrentEntry->pPed->GetPedModelInfo();
		if (pModelInfo && pModelInfo->GetIsStreamedGfx() && m_pCurrentEntry->pPed->GetPedDrawHandler().GetConstPedRenderGfx() == NULL)
		{
			return false;
		}
	}

	// no dependencies on animation data, check our clone has been removed from the world first
	if (m_pCurrentEntry->isPosed == false)
	{
		return true;
	}

	// request the animation data
	if (m_animDataState == kClipHelperReady)
	{
		strLocalIndex animDictIndex = strLocalIndex(fwAnimManager::FindSlot(m_clipDictionaryName));
		if (fwAnimManager::IsValidSlot(animDictIndex) == false)
		{
			Assertf(0,"PedHeadshotManager::IsCurrentEntryReadyForRendering: Invalid clip dictionary");

			OnFailure(*m_pCurrentEntry);
			ResetCurrentEntry();
			m_textureBuilder.Abort();

			return false;
		}
		m_clipRequestHelper.RequestClipsByIndex(animDictIndex.Get());
		m_animDataState = kClipHelperWaitingOnData;

		// not ready yet
		return false;
	}
	// check if the animation data is ready
	else if (m_animDataState == kClipHelperWaitingOnData) 
	{

		if (CGameWorld::HasEntityBeenAdded(m_pCurrentEntry->pPed))
		{
			return false;
		}

		if (m_clipRequestHelper.HaveClipsBeenLoaded())
		{
			CPed* pPed = m_pCurrentEntry->pPed;

			strLocalIndex animDictIndex = strLocalIndex(fwAnimManager::FindSlot(m_clipDictionaryName));
			if (fwAnimManager::IsValidSlot(animDictIndex) == false)
			{
				Assertf(0,"PedHeadshotManager::IsCurrentEntryReadyForRendering: Invalid clip dictionary");
			
				OnFailure(*m_pCurrentEntry);
				ResetCurrentEntry();
				m_textureBuilder.Abort();
				
				return false;
			}

			const crClip* pBodyClip = fwAnimManager::GetClipIfExistsByDictIndex(animDictIndex.Get(), m_clipName);
		
			if (pBodyClip)
			{
				pPed->GetSkeleton()->Reset();

				// Make a frame for the body animation 
				crFrameDataInitializerBoneAndMover initFrame(pPed->GetSkeletonData(), true);
				crFrame frameBody;	
				crFrameData pedFrameData;
				initFrame.InitializeFrameData(pedFrameData);
				frameBody.Init(pedFrameData);

				// Compose the animation into a frame
				pBodyClip->Composite(frameBody, 0.5f);

				//Pose the peds skeleton using the body frame
				frameBody.Pose(*pPed->GetSkeleton(),false, NULL);						
				pPed->GetSkeleton()->Update();

				// Do the animation pre-render update (to update the expressions)
				fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePreRender);
				pPed->GetAnimDirector()->StartUpdatePreRender(fwTimer::GetTimeStep(), NULL);
				pPed->GetAnimDirector()->WaitForPreRenderUpdateToComplete();
				fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);

				pPed->GetSkeleton()->Update();

				m_animDataState = kClipHelperDataLoaded;
			}
			else
			{
				OnFailure(*m_pCurrentEntry);
				ResetCurrentEntry();
				m_textureBuilder.Abort();
			}
				
		}

		// not ready yet
		return false;
	}
	// animation data is ready (new posed peds cannot be registered yet)
	else if (m_animDataState == kClipHelperDataLoaded)
	{
		m_animDataState = kClipHelperDataWaitingForRendering;
		return true;
	}
#if __BANK
	// if we're locked in headshot rendering and data is ready, force it to render
	else if (m_animDataState == kClipHelperDataWaitingForRendering && (sm_bLockInHeadshotRendering == true))
	{
		return true;
	}
#endif

	// not ready yet
	return false;

}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::UpdateCurrentEntry()
{
	bool bContinueUpdate = true;

	if (m_pCurrentEntry)
	{
		Assert(m_pCurrentEntry->isInProcess);

		if (m_textureBuilder.IsCompressionFinished())
		{
			// compression's finished, claim compressed output texture
			// and mark entry as active
			bool bUsedGpuCompression = false;
			m_pCurrentEntry->texture = m_textureBuilder.GetTexture(bUsedGpuCompression);
			m_pCurrentEntry->isInProcess = false;

			// clean up ped data here now everything's over
			CleanUpPedData();

			// only mark it as active if it hasn't been marked for release; otherwise, before we actually release the entry
			// somebody could have already grabbed a hold of the texture; the texture would have still be processed correctly
			// because of the entry isInProcess state being true, which keeps the rendering and compression stages going
			// and stops the entry from being released.
			// However, the manager would clear the entry and remove one ref from the texture upon release next frame.
			m_pCurrentEntry->isActive = (m_pCurrentEntry->isWaitingOnRelease == false);
			m_pCurrentEntry->state = (m_pCurrentEntry->isWaitingOnRelease == false) ? PHS_READY : PHS_FAILED;

			ResetCurrentEntry();
		}
		else if (m_textureBuilder.HasFailed())
		{
			// texture builder has failed, let's forget 
			// about our current entry and mark it for deletion
			OnFailure(*m_pCurrentEntry);
			ResetCurrentEntry();

			// make the clip helper available again
			m_animDataState = kClipHelperReady;
		}

		bContinueUpdate = false;
	}

	return bContinueUpdate;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::MarkClonePedForDeletion(PedHeadshotInfo& entry)
{
	if (entry.isCloned == false)
	{
		entry.pPed = NULL;
		entry.pSourcePed = NULL;
		return;
	}

	if (entry.pPed != NULL)
	{
		// avoid pushing the same ped twice
		for (int i = 0; i <  m_delayedDeletionPed.GetCount(); i++)
		{
			if (m_delayedDeletionPed[i] == entry.pPed)
			{
				entry.pPed = NULL;
				return;
			}
		}

		// delete the cloned ped in UpdateSafe
		m_delayedDeletionPed.Push(entry.pPed);
		entry.pPed = NULL;
		entry.pSourcePed = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::OnFailure(PedHeadshotInfo& entry) 
{
	entry.isWaitingOnRelease = true;
	entry.isInProcess = false;
	entry.state = PHS_FAILED;
	MarkClonePedForDeletion(entry);
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::UpdateTextureBuilderStatus()
{
	bool bContinueUpdate = true;

	// if texture builder has failed, allow it doing any
	// pending housekeeping before pushing any pending request
	if (m_textureBuilder.HasFailed())
	{
		m_textureBuilder.RecoverFromFailure();
		bContinueUpdate = false;
	}

	return bContinueUpdate;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::IsTextureAvailableForSlot(int idx) const 
{
	char textureName[32];
	sprintf(&textureName[0], PEDMUGSHOT_FILE_NAME_PATTERN, idx+1);
	atHashString txdName(textureName);

	strStreamingObjectName txdHash = strStreamingObjectName(txdName);
	strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlotFromHashKey(txdHash.GetHash()));

	// slot doesn't even exist
	if (txdSlot == -1)
	{
		return true;
	}

	// slot exists, but make sure no loading requests are pending
	if (!g_TxdStore.HasObjectLoaded(txdSlot) && !g_TxdStore.IsObjectRequested(txdSlot) && !g_TxdStore.IsObjectLoading(txdSlot))
	{
		return true;
	}

	// if it's loaded and it's got no references to it, we're good to use it too;
	// it doesn't matter if the slot gets removed afterwards or the object gets released, 
	// since whenever this headshot gets processed the code will take care of it
	if (g_TxdStore.HasObjectLoaded(txdSlot) && g_TxdStore.GetNumRefs(txdSlot) == 0)
	{
		return true;
	}
	
	// the texture is being used somewhere, don't allow this slot to be reused.
	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::ProcessPendingEntries()
{
	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		// pick first entry still not processed
		bool bCanProcess = CanProcessEntry(m_entriesPool[i]);
		if (bCanProcess)
		{
			// Make sure the animation is loaded in
			m_headShotAnimRequestHelper.Request(BIND_POSE_CLIPSET_ID);
			if (!m_headShotAnimRequestHelper.IsLoaded())
			{
				return false;
			}

			char textureName[32];
			sprintf(&textureName[0], PEDMUGSHOT_FILE_NAME_PATTERN, i+1);
			atHashString txdName(textureName);

			// if the texture builder returns false, either it's already processing another headshot
			// or it's in a failed state (should recover itself).
			bool bOk = m_textureBuilder.ProcessHeadshot(txdName, txdName, m_entriesPool[i].hasTransparentBackground, m_entriesPool[i].forceHiRes);
			if (bOk)
			{
				// set active entry
				m_pCurrentEntry = &m_entriesPool[i];

				m_pCurrentEntry->isInProcess = true;
				m_pCurrentEntry->state = PHS_WAITING_ON_TEXTURE;

				// try cloning the ped
				CPed* pClonedPed = CPedFactory::GetFactory()->ClonePed(m_pCurrentEntry->pPed, false, true, true);
				bOk = (pClonedPed != NULL);

				// setup our cloned ped
				if(bOk)
				{
					m_pCurrentEntry->pPed = NULL;
					m_pCurrentEntry->pPed = pClonedPed;
					m_pCurrentEntry->isCloned = true;
					bOk = InitClonedPed(*m_pCurrentEntry);
                    pClonedPed->FinalizeHeadBlend();
				}

				if (bOk)
				{
					m_pCurrentEntry->txdId = g_TxdStore.FindSlotFromHashKey(txdName.GetHash());
					m_pCurrentEntry->hash = txdName.GetHash();

					// the animation data state machine should be in a ready state
					if (m_pCurrentEntry->isPosed && m_animDataState != kClipHelperReady)
					{
						return false;
					}
				}
				else // if anything went wrong, get rid of the entry
				{
				#if __BANK
					if (PARAM_pedheadshotdebug.Get())
					{
						Displayf("[PHM] PedHeadshotManager::ProcessPendingEntries: failed to clone ped, aborting");
					}
				#endif
					OnFailure(*m_pCurrentEntry);
					ResetCurrentEntry();
					m_textureBuilder.Abort();
				}

			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::AddRefToCurrentEntry() 
{
	if (Verifyf(m_pCurrentEntry != NULL, "PedHeadshotManager::AddRefToCurrentEntry: no current entry") && 
		Verifyf(m_pCurrentEntry->refAdded == false, "PedHeadshotManager::AddRefToCurrentEntry: this entry had already added a reference") &&
		m_pCurrentEntry->txdId != -1 &&
		g_TxdStore.HasObjectLoaded(strLocalIndex(m_pCurrentEntry->txdId.Get())))
	{
		m_pCurrentEntry->refAdded = true;
		g_TxdStore.AddRef(m_pCurrentEntry->txdId, REF_OTHER);

#if __BANK
		if (PARAM_pedheadshotdebug.Get())
		{
			Displayf("[PHM] PedHeadshotManager::AddRefToCurrentEntry: texture has %d refs", g_TxdStore.GetNumRefs(m_pCurrentEntry->txdId));
		}
#endif
	}
}
//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::AddRefToEntry(PedHeadshotInfo& entry)
{
	if (Verifyf(entry.refAdded == false, "PedHeadshotManager::AddRefToEntry: this entry had already added a reference") &&
		entry.txdId != -1 &&
		g_TxdStore.HasObjectLoaded(strLocalIndex(entry.txdId.Get())))
	{
		entry.refAdded = true;
		g_TxdStore.AddRef(entry.txdId, REF_OTHER);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::RemoveRefFromEntry(PedHeadshotInfo& entry)
{
#	if PGHANDLE_REF_COUNT
		entry.texture = NULL;
#	endif
	if (entry.refAdded == true)
	{
		// if we added a ref, the texture must still be loaded since this is the only point where we remove
		// our ref to it; txdId should also be valid
		entry.refAdded = false;
		Assertf(entry.txdId != -1, "PedHeadshotManager::RemoveRefFromEntry: Cannot remove a ref if we never setup a txdId");
		Assertf(g_TxdStore.HasObjectLoaded(strLocalIndex(entry.txdId.Get())), "PedHeadshotManager::RemoveRefFromEntry: Cannot remove ref while entry isnt loaded");
		g_TxdStore.RemoveRef(entry.txdId, REF_OTHER);	
	}
	// the txdId might have not been setup yet - but if it's valid, it means we previously added the slot
	else if ( g_TxdStore.IsValidSlot(entry.txdId))
	{
		Assertf(g_TxdStore.GetNumRefs(entry.txdId) == 0, "PedHeadshotManager::RemoveRefFromEntry: Should never get here if the txd is referenced");
		g_TxdStore.Remove(entry.txdId);
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::CanProcessEntry(const PedHeadshotInfo& entry) const
{
	return	(entry.isRegistered			== true		&& 
			 entry.isInProcess			== false	&& 
			 entry.isActive				== false	&& 
			 entry.isWaitingOnRelease	== false	&&
			 m_delayedDeletionPed.GetCount() == 0);
}

//////////////////////////////////////////////////////////////////////////
// 
#if __PS3
// This is needed to avoid edge pixel culling
// causing visible cracks in the ped geometry
extern bool SetEdgeNoPixelTestEnabled(bool bEnabled);
static bool bWasEdgeNoPixelTestEnabled = false;
#endif

bool PedHeadshotManager::HasPedsToDraw() const
{
	if  (m_pCurrentEntry && m_textureBuilder.IsReadyForRendering() && m_pCurrentEntry->pPed)
	{
		// no dependencies on animation data, check our clone has been removed from the world first
		if (m_pCurrentEntry->isPosed == false)
		{
			return true;
		}

		// animation data is ready (new posed peds cannot be registered yet)
		if (m_animDataState == kClipHelperDataLoaded)
		{
			return true;
		}
#if __BANK
		// if we're locked in headshot rendering and data is ready, force it to render
		else if (m_animDataState == kClipHelperDataWaitingForRendering && (sm_bLockInHeadshotRendering == true))
		{
			return true;
		}
#endif
	}

	return false;
}

void PedHeadshotManager::PreRender() 
{
	if (HasPedsToDraw())
	{
		if (m_pCurrentEntry && m_pCurrentEntry->pPed)
		{
			u8 damageSetId = m_pCurrentEntry->pPed->GetDamageSetID();
			if (damageSetId!=kInvalidPedDamageSet)
				CPedDamageManager::GetInstance().UpdatePriority(damageSetId, 4.0f, true, false);
		}
	}
}

void PedHeadshotManager::AddToDrawList() 
{
	// return if there are no active requests
	if (m_pCurrentEntry && m_textureBuilder.IsReadyForRendering() && IsCurrentEntryReadyForRendering())
	{

		SetupViewport();
#if __PS3
		bWasEdgeNoPixelTestEnabled = SetEdgeNoPixelTestEnabled(false);
#endif

		DLC_Add(PedHeadshotManager::SetupLights);
		//DLC_Add(PedHeadshotManager::DrawListPrologue);

		grcRenderTarget* pRT0 = m_textureBuilder.GetColourRenderTarget0();
		grcRenderTarget* pRT1 = m_textureBuilder.GetColourRenderTarget1();
		grcRenderTarget* pRTCopy = m_textureBuilder.GetColourRenderTargetCopy();

		bool bOk = (pRT0 != NULL && pRT1 != NULL);

#if __BANK
		bOk = bOk && (sm_bSkipRendering == false);
#endif

		bOk = bOk && AddHeadshotToDrawList(pRT0, pRT1, pRTCopy, NULL, *m_pCurrentEntry);			
	
		//DLC_Add(PedHeadshotManager::DrawListEpilogue);

#if __PS3
		if (bWasEdgeNoPixelTestEnabled)
		{
			SetEdgeNoPixelTestEnabled(true);
		}
#endif

		if (bOk == false)
		{
			// texture builder has failed, let's forget 
			// about our current entry and mark it for deletion
			OnFailure(*m_pCurrentEntry);
			ResetCurrentEntry();
			m_textureBuilder.Abort();
			return;
		}

#if __BANK
		if (!sm_bContinousHeadshot)
#endif // __BANK
		{
			if (m_textureBuilder.IsUsingGpuCompression() BANK_ONLY(&& (sm_bLockInHeadshotRendering == false)))
			{
				DLC_Add(PedHeadshotManager::SetReadyForGpuCompression);
				DLC_Add(PedHeadshotManager::RunCompression);
			}
			else if( m_textureBuilder.IsUsingSTBCompressionRenderThread() )
			{
				DLC_Add(PedHeadshotManager::RunSTBCompressionRenderThread);
			}
			else
			{
				DLC_Add(PedHeadshotManager::SetRenderCompletionFence);
			}
#if __BANK
			if (sm_bLockInHeadshotRendering == false)
#endif
			{
				if (m_textureBuilder.IsUsingGpuCompression() == false && m_textureBuilder.IsUsingSTBCompressionRenderThread() == false
					)
				{
					DLC_Add(PedHeadshotManager::SetReadyForCompression);
				}
			}

#if __BANK
			if (sm_bMultipleAddToDrawlist && (m_textureBuilder.IsUsingGpuCompression() == false && m_textureBuilder.IsUsingSTBCompressionRenderThread() == false))
			{	
				AddToDrawList();
				sm_bMultipleAddToDrawlist = false;
			}
#endif
		}
	}

	DLC_Add(PedHeadshotManager::RenderUpdate);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::RunCompression()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	PEDHEADSHOTMANAGER.m_textureBuilder.KickOffGpuCompression();
}

void PedHeadshotManager::RunSTBCompressionRenderThread()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	PEDHEADSHOTMANAGER.m_textureBuilder.KickOffSTBCompressionRenderThread();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::RenderUpdate()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	PEDHEADSHOTMANAGER.UpdateRenderCompletionFenceStatus();
	PEDHEADSHOTMANAGER.m_textureBuilder.UpdateRender();
}

void PedHeadshotManager::AnimationUpdate(CPed* pPed)
{
	if (Verifyf(pPed->GetSkeleton() && pPed->GetPedModelInfo(), "Ped should always have a skeleton and model info"))
	{
		const crClip* pBindPoseClip = fwAnimManager::GetClipIfExistsBySetId(BIND_POSE_CLIPSET_ID, BIND_POSE_CLIP_ID);
		if (Verifyf(pBindPoseClip, "Bind pose anim not found within bind pose clipset"))
		{
			// External DOFs
			pPed->SetupExternallyDrivenDOFs();
			pPed->ProcessExternallyDrivenDOFs();

			// Force the high-heels off
			crFrame* pExternallyDrivenDofFrame = pPed->GetExternallyDrivenDofFrame();
			if (pExternallyDrivenDofFrame)
			{
				pExternallyDrivenDofFrame->SetFloat(kTrackGenericControl, BONETAG_HIGH_HEELS, 0.0f);
			}

			// Get delta
			const float deltaTime = 0.0f;

			//Mat34V entMatrix = pPed->GetMatrix();
			pPed->GetSkeleton()->SetParentMtx(&pPed->GetMatrixRef());

			fwAnimDirector* director = pPed->GetAnimDirector();

			fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePrePhysics);
			director->StartUpdatePrePhysics(deltaTime, true);
			director->WaitForMotionTreeByPhase(fwAnimDirectorComponent::kPhasePrePhysics);
			fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);

			// Make a frame for the body animation 
			const crFrameData& pedFrameData = pPed->GetCreature()->GetFrameData();
			crFrame frameBody;	
			frameBody.Init(pedFrameData);

			// Composite in the bind pose clip
			pBindPoseClip->Composite(frameBody, 0.0f);

			//// Play a facial animation if one is available
			fwFacialClipSetGroup* pFacialClipSetGroup = fwFacialClipSetGroupManager::GetFacialClipSetGroup(pPed->GetPedModelInfo()->GetFacialClipSetGroupId());
			if (pFacialClipSetGroup)
			{
				fwMvClipSetId facialBaseClipSetId = fwClipSetManager::GetClipSetId(pFacialClipSetGroup->GetBaseClipSetName());
				Assertf(facialBaseClipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognised facial base clipset", pFacialClipSetGroup->GetBaseClipSetName().GetCStr());

				const crClip* pFacialClip = fwAnimManager::GetClipIfExistsBySetId(facialBaseClipSetId, CLIP_FACIAL_MOOD_NORMAL);
				if (pFacialClip)
				{
					// Make a frame for the facial animation
					crFrame frameFacial;
					frameFacial.Init(pedFrameData);

					const float fFacialSampleTime = 0.0f;//fwRandom::GetRandomNumberInRange(0.0f, pFacialClip->GetDuration());

					// Compose the animation into a frame
					pFacialClip->Composite(frameFacial, fFacialSampleTime);

					// Merge the facial frame into the body frame
					frameBody.Merge(frameFacial, NULL);
				}
			}

			pPed->GetSkeleton()->Reset();

			//Pose the peds skeleton using the combined body and facial frame
			frameBody.Pose(*pPed->GetSkeleton(),false, NULL);
			pPed->GetSkeleton()->Update();

			fwAnimDirectorComponentFacialRig* pFacialRigComp = director->GetComponentByPhase<fwAnimDirectorComponentFacialRig>(fwAnimDirectorComponent::kPhaseAll);
			if(pFacialRigComp)
			{
				pFacialRigComp->GetFrame().Set(frameBody);
			}

			fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhaseMidPhysics);
			director->StartUpdateMidPhysics(deltaTime);
			director->WaitForMotionTreeByPhase(fwAnimDirectorComponent::kPhaseMidPhysics);
			fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);

			// Do the animation pre-render update (to update the expressions)
			fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePreRender);
			director->StartUpdatePreRender(deltaTime);
			director->WaitForMotionTreeByPhase(fwAnimDirectorComponent::kPhasePreRender);
			fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::AddHeadshotToDrawList(grcRenderTarget* pRtCol0, grcRenderTarget* pRtCol1, grcRenderTarget* WIN32PC_ONLY(pRtColCopy), grcRenderTarget* UNUSED_PARAM(pDepth), const PedHeadshotInfo& headshotInfo)
{
	// render ped
	CPed* pPed = headshotInfo.pPed;
	const bool bIsPosed = headshotInfo.isPosed;

	if (pPed == NULL BANK_ONLY(|| sm_bForceNullPedPointerDuringRender)) 
	{
		Warningf("PedHeadshotManager::AddHeadshotToDrawList: ped is NULL");
		return false;
	}

	if (pPed->GetDrawable() == NULL)
	{
		Warningf("PedHeadshotManager::AddHeadshotToDrawList: ped doesn't have a drawable");
		return false;
	}

	DLCPushTimebar("Headshot");

	
	Mat34V matIdentity(V_IDENTITY);
	if (bIsPosed == false)
	{
		AnimationUpdate(pPed);

		matIdentity.SetCol0(-matIdentity.GetCol0());
		matIdentity.SetCol1(-matIdentity.GetCol1());

		// only override root matrix (we want to keep the pose)
		DLC (dlCmdOverrideSkeleton, (matIdentity));
	}
	else
	{
		// rotate identity matrix by 180 degrees around the z axis
		matIdentity.SetCol0(-matIdentity.GetCol0());
		matIdentity.SetCol1(-matIdentity.GetCol1());

		// only override root matrix (we want to keep the pose)
		DLC (dlCmdOverrideSkeleton, (matIdentity));
	}

	float curLodMultiplier = pPed->GetLodMultiplier();
	pPed->SetLodMultiplier(1000.0f);

	// update the ped damage variables (since this ped is not in the normal list of peds)
	pPed->UpdateDamageCustomShaderVars();


	DLC_Add(DeferredRendererWrapper::Initialize, &m_renderer, m_pViewport, &sm_defLightSource[0], 4);

#if __PS3
	bWasEdgeNoPixelTestEnabled = SetEdgeNoPixelTestEnabled(false);
#endif

	DLC_Add(DeferredRendererWrapper::SetAmbients, &m_renderer);

	bool bOk = true;

	DLC_Add(DeferredRendererWrapper::DeferredRenderStart, &m_renderer);
	{
		// Opaque
		DLC_Add(DeferredLighting::PrepareDefaultPass, (RPASS_VISIBLE));
		{
			DLC (dlCmdSetBucketsAndRenderMode, (CRenderer::RB_OPAQUE, rmStandard));
			pPed->GetDrawHandler().AddToDrawList(pPed, NULL);
		}
		DLC_Add(DeferredLighting::FinishDefaultPass);

		// Cutout
		#if __PS3
		if (PostFX::UseSubSampledAlpha())
		{
			DLC_Add(DeferredRendererWrapper::UnlockRenderTargets, &m_renderer, false, false);
			DLC_Add(DeferredRendererWrapper::LockSingleTarget, &m_renderer, 0, true);
		}
		#endif // __PS3

		DLC_Add(DeferredLighting::PrepareCutoutPass, false, false, false);
		{
			DLC (dlCmdSetBucketsAndRenderMode, (CRenderer::RB_CUTOUT, rmStandard));
			pPed->GetDrawHandler().AddToDrawList(pPed, NULL);
		}
		DLC_Add(DeferredLighting::FinishCutoutPass);


		#if __PS3
		if (PostFX::UseSubSampledAlpha())
		{
			DLC_Add(DeferredRendererWrapper::UnlockSingleTarget, &m_renderer, false, false, false);
			DLC_Add(DeferredRendererWrapper::LockRenderTargets, &m_renderer, true);
		}
		#endif // __PS3

		DLC_Add(DeferredLighting::PrepareCutoutPass, true, false, false);
		{
			DLC (dlCmdSetBucketsAndRenderMode, (CRenderer::RB_CUTOUT, rmStandard));
			pPed->GetDrawHandler().AddToDrawList(pPed, NULL);
		}
		DLC_Add(DeferredLighting::FinishCutoutPass);


		// Decal
		DLC_Add(DeferredLighting::PrepareDecalPass, false);
		{
			DLC (dlCmdSetBucketsAndRenderMode, (CRenderer::RB_DECAL, rmStandard));
			pPed->GetDrawHandler().AddToDrawList(pPed, NULL);
		}
		DLC_Add(DeferredLighting::FinishDecalPass);
	}
	DLC_Add(DeferredRendererWrapper::DeferredRenderEnd, &m_renderer);

	// Lighting
	DLC_Add(DeferredRendererWrapper::DeferredLight, &m_renderer);


	// Skin pass
#if RSG_PC
	if (DeferredRendererWrapper::GetLightingTargetCopy(&m_renderer) !=  DeferredRendererWrapper::GetLightingTarget(&m_renderer))
	{
		DLC_Add(DeferredRendererWrapper::CopyTarget, &m_renderer, DeferredRendererWrapper::GetLightingTargetCopy(&m_renderer), DeferredRendererWrapper::GetLightingTarget(&m_renderer));
	}
#endif // RSG_PC

	DLC_Add(DeferredRendererWrapper::LockLightingTarget, &m_renderer, true);
	DLC_Add(DeferredRendererWrapper::SetUIProjectionParams, &m_renderer, DEFERRED_SHADER_LIGHTING);
	DLC_Add(DeferredRendererWrapper::SetGbufferTargets, &m_renderer);
	DLC_Add(DeferredLighting::ExecuteUISkinLightPass, DeferredRendererWrapper::GetLightingTarget(&m_renderer), DeferredRendererWrapper::GetLightingTargetCopy(&m_renderer));
	DLC_Add(DeferredRendererWrapper::UnlockSingleTarget, &m_renderer, true, false, false);

	// Forward 
	DLC_Add(DeferredRendererWrapper::LockLightingTarget, &m_renderer, true);
	DLC_Add(DeferredRendererWrapper::ForwardRenderStart, &m_renderer);
	{
		DLC (dlCmdSetBucketsAndRenderMode, (CRenderer::RB_ALPHA, rmStandard));
		pPed->GetDrawHandler().AddToDrawList(pPed, NULL);
	}
	DLC_Add(DeferredRendererWrapper::ForwardRenderEnd, &m_renderer);
	DLC_Add(DeferredRendererWrapper::UnlockSingleTarget, &m_renderer, true, false, false);

	pPed->SetLodMultiplier(curLodMultiplier);

	DLC (dlCmdOverrideSkeleton, (true));

	// We want to run an AA pass for the high-res headshots
	bool bRunAAPass = (pRtCol1->GetWidth() == 128);
	grcRenderTarget* pTonemappedRTSrc;
	grcRenderTarget* pTonemappedRTDst = pTonemappedRTSrc = bRunAAPass ? DeferredRendererWrapper::GetCompositeTarget(&m_renderer) : pRtCol0;
	grcRenderTarget* pCompositeRT0 = bRunAAPass ? pRtCol0 : pRtCol1;

	Vector4 scissorRect = Vector4(0.0f, 0.0f, (float)pTonemappedRTDst->GetWidth(), (float)pTonemappedRTDst->GetHeight());
	DLC_Add(DeferredRendererWrapper::RenderToneMapPass, &m_renderer, pTonemappedRTDst, DeferredRendererWrapper::GetLightingTarget(&m_renderer), Color32(0,0,0,0), true);

#if RSG_PC
	if (pTonemappedRTDst == pTonemappedRTSrc)
	{
		pTonemappedRTSrc = bRunAAPass ? DeferredRendererWrapper::GetCompositeTargetCopy(&m_renderer) : pRtColCopy;
		DLC_Add(DeferredRendererWrapper::CopyTarget, &m_renderer, pTonemappedRTSrc, pTonemappedRTDst);
	}
#endif // RSG_PC

	DLC_Add(PostFX::ProcessUISubSampleAlpha, pTonemappedRTDst, pTonemappedRTSrc, DeferredRendererWrapper::GetTarget(&m_renderer, 0), DeferredRendererWrapper::GetDepth(&m_renderer), scissorRect);
#if RSG_PC
	Assert(pCompositeRT0 != pTonemappedRTDst);
#endif // RSG_PC
	DLC_Add(PedHeadshotManager::CompositePass, pCompositeRT0, pTonemappedRTDst);

	if (bRunAAPass)
	{
		DLC_Add(PedHeadshotManager::AAPass, pRtCol1, pCompositeRT0);
	}

	DLC_Add(DeferredRendererWrapper::Finalize, &m_renderer);

#if __PS3
	if (bWasEdgeNoPixelTestEnabled)
	{	
		SetEdgeNoPixelTestEnabled(true);
	}
#endif
	// Reset the prototype to make sure it gets flushed. Worst case (like during pause menu), it never gets
	// flushed and leaves a stale prototype around.
	CDrawListPrototypeManager::SetPrototype(NULL);

	DLCPopTimebar();

	return bOk;

}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::CompositePass(grcRenderTarget* pDstRT, grcRenderTarget* pSrcRT)
{
	PF_PUSH_MARKER("Composite");

	// lock composite target
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, NULL);

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(XENON_SWITCH(grcStateBlock::RS_Default_HalfPixelOffset, grcStateBlock::RS_NoBackfaceCull));

	// blit 
	CSprite2d blit;
	const Vector4 col(1.0f, 1.0f, 1.0f, 1.0f);
	blit.SetGeneralParams(col, col);
	blit.BeginCustomList(CSprite2d::CS_BLIT, pSrcRT);
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0, 0, 1, 1, Color32(255,255,255,255));
	blit.EndCustomList();

	// reset viewport
	POP_DEFAULT_SCREEN();

	// unlock composite target
	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = true;
#endif
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::AAPass(grcRenderTarget* pDstRT, grcRenderTarget* pSrcRT)
{
	PF_PUSH_MARKER("AA");

	// lock composite target
	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, NULL);

	GRCDEVICE.Clear(true,	Color32(0,0,0,0), 
					false,	1.0f, 
					false,	0);

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_CompositeAlpha);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	CSprite2d UIAASprite;
	UIAASprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	const Vector2 TexSize(1.0f/float(pSrcRT->GetWidth()), 1.0f/float(pSrcRT->GetHeight()));
	UIAASprite.SetTexelSize(TexSize);
	UIAASprite.BeginCustomList(CSprite2d::CS_BLIT_AAAlpha, pSrcRT);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
	UIAASprite.EndCustomList();

	// reset viewport
	POP_DEFAULT_SCREEN();

	// unlock composite target
	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = true;
#endif
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	PF_POP_MARKER();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::RenderToneMapPass(grcRenderTarget* pRT, grcRenderTarget* pDepthTarget, grcTexture* pSourceTexture)
{
	const float packScalar = 4.0f;
	const float toneMapScaler = 1.0f/packScalar;
	const float unToneMapScaler = packScalar;
	Vector4 scalers(toneMapScaler, unToneMapScaler, toneMapScaler, unToneMapScaler);
	CShaderLib::SetGlobalToneMapScalers(scalers);

	// lock target
	grcTextureFactory::GetInstance().LockRenderTarget(0, pRT, pDepthTarget);


	GRCDEVICE.Clear(true,	Color32(0,0,0,0), 
					false,	1.0f, 
					false,	0);

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(ms_headshotTestDS, 0xff);

	Vector4 filmic0, filmic1, tonemapParams;
	PostFX::GetDefaultFilmicParams(filmic0, filmic1);
	
	// set custom exposure
	tonemapParams.z = pow(2.0f, sm_defaultExposure);
	tonemapParams.w = 1.0f;

	CSprite2d tonemapSprite;
	tonemapSprite.SetGeneralParams(filmic0, filmic1);
	tonemapSprite.SetRefMipBlurParams(Vector4(0.0f, 0.0f, 1.0f, (1.0f / 2.2f)));
	Vector4 colFilter0, colFilter1;
	colFilter0.Zero();
	colFilter1.Zero();
	tonemapSprite.SetColorFilterParams(colFilter0, colFilter1);
	tonemapSprite.BeginCustomList(CSprite2d::CS_TONEMAP, pSourceTexture, 1);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
	tonemapSprite.EndCustomList();

	// unlock target
	grcResolveFlags flags;
	flags.NeedResolve = true;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

	// reset viewport
	POP_DEFAULT_SCREEN();

	// reset state
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::SetupViewport() 
{
	if (PEDHEADSHOTMANAGER.m_pCurrentEntry == NULL)
	{
		return;
	}

	float offsetForTransparentBg = 0.0f;
	if (PEDHEADSHOTMANAGER.m_pCurrentEntry->hasTransparentBackground)
	{
		offsetForTransparentBg = -0.25f;
	}

	Vec3V vCamPos(0.0f, -Abs(sm_defaultCamDepthOffset)+offsetForTransparentBg, sm_defaultCamHeightOffset);

	CPed* pPed = PEDHEADSHOTMANAGER.m_pCurrentEntry->pPed;
	float radius = 1.0f;
	if (pPed)
	{
		#if __BANK
			PEDHEADSHOTMANAGER.m_pViewport->Perspective(sm_defaultVFOVDeg, sm_defaultAspectRatio, 0.01f, 5000.0f);
		#endif

		if (pPed->GetBaseModelInfo())
		{
			radius = pPed->GetBaseModelInfo()->GetBoundingSphereRadius();
		}

		// todo: come up with a more solid way to derive an ideal camera height
		// totally experimental values from going through different peds...
		const float tallHeight = 1.15f;
		const float tallHeightHeadRatio = 0.42f;
		const float smallHeightHeadRatio = 0.46f;

		bool bIsTall = (radius > tallHeight);
		float camHeight = radius-(radius*(bIsTall ? tallHeightHeadRatio : smallHeightHeadRatio));

		vCamPos.SetZf(camHeight);

	}



	Mat34V vCamMtx(Vec3V(V_X_AXIS_WZERO), Vec3V(V_Z_AXIS_WZERO), Vec3V(V_Y_AXIS_WZERO), vCamPos);
	vCamMtx.Setc(Negate(vCamMtx.c()));
	
	if (PEDHEADSHOTMANAGER.m_pCurrentEntry->isPosed == true)
	{

		Mat34V vGameCamMtx;
		Mat34VFromEulersYXZ(vGameCamMtx, Scale(sm_posedPedCamAngles, ScalarV(V_TO_RADIANS)), sm_posedPedCamPos);
		Mat34V vRageCamMtx = GameCameraMtxToRageCameraMtx(vGameCamMtx);
		vCamMtx = vRageCamMtx;

		//vCamPos.SetZ(vCamPos.GetZ()-ScalarV(V_THIRD));
		//vCamPos.SetY(ScalarVFromF32(2.75f)*(vCamPos.GetY()));

		//vCamMtx.Setd(vCamPos);
	}

	PEDHEADSHOTMANAGER.m_pViewport->SetCameraMtx(vCamMtx);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::SetupLights()
{
	CRenderPhaseCascadeShadowsInterface::SetSharedShadowMap(NULL);

	// set local lights
	if (sm_bUseCustomLighting)
	{
		Lights::m_lightingGroup.SetForwardLights(sm_customLightSource, 4);
	}
	else 
	{
		Lights::m_lightingGroup.SetForwardLights(sm_defLightSource, 4);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::DrawListPrologue()	//NOTE: not actually used
{

	sm_prevForcedTechniqueGroup = grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(grmShaderFx::FindTechniqueGroupId("ui"));

	CShaderLib::SetGlobalEmissiveScale(1.0f,true);

	const float packScalar = 4.0f;
	const float toneMapScaler = 1.0f/packScalar;
	const float unToneMapScaler = packScalar;
	Vector4 scalers(toneMapScaler, unToneMapScaler, toneMapScaler, unToneMapScaler);

	CShaderLib::SetGlobalToneMapScalers(scalers);

	//turn off fog and depthfx
	CShaderLib::DisableGlobalFogAndDepthFx();

	//grcLightState::SetEnabled(false);

	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_LessEqual);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

	// force reflection off
	CRenderPhaseReflection::SetReflectionMapExt(grcTexture::NoneBlack);

	// disable directional
	Lights::m_lightingGroup.SetDirectionalLightingGlobals(false);

	Vector4 ambientValues[7];
	Vector4 zero = Vector4(Vector4::ZeroType);

	// set up ambient lighting
	ambientValues[0] = sm_ambientLight[0];
	ambientValues[1] = sm_ambientLight[1];

	// artificial interior ambient
	ambientValues[2] = zero;
	ambientValues[3] = zero;
	
	// artificial exterior ambient
	ambientValues[4] = zero;
	ambientValues[5] = zero;

	// directional ambient
	ambientValues[6] = zero;

	Lights::m_lightingGroup.SetAmbientLightingGlobals(1.0f, 1.0f, 1.0f, ambientValues);

	CShaderLib::SetGlobalNaturalAmbientScale(1.0f);
	CShaderLib::SetGlobalArtificialAmbientScale(0.0f);

	SetupLights();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::DrawListEpilogue()	//NOTE: not actually used
{
	grmShaderFx::SetForcedTechniqueGroupId(sm_prevForcedTechniqueGroup);

	CShaderLib::EnableGlobalFogAndDepthFx();
	Lights::ResetLightingGlobals();

	Lights::m_lightingGroup.SetDirectionalLightingGlobals(true);
	Lights::m_lightingGroup.SetAmbientLightingGlobals();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::SetRenderCompletionFence()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	PedHeadshotManager *const self = &PEDHEADSHOTMANAGER;

	self->m_renderState = kRenderBusy;

	grcFenceHandle fh = self->m_renderCompletionFence;
	GRCDEVICE.CpuMarkFencePending(fh);
	GRCDEVICE.GpuMarkFenceDone(fh);

#if __BANK
	if (PARAM_pedheadshotdebug.Get())
	{
		Displayf("[PHM] PedHeadshotManager::SetRenderCompletionFence: state: %u", self->m_renderState);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::SetReadyForCompression()
{
	PEDHEADSHOTMANAGER.m_textureBuilder.SetReadyForCompression();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::SetReadyForGpuCompression()
{
	PEDHEADSHOTMANAGER.m_textureBuilder.SetReadyForGpuCompression();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::UpdateRenderCompletionFenceStatus()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	// not waiting on anything
	if (m_renderState != kRenderBusy)
	{
		return;
	}

	if(m_textureBuilder.IsUsingGpuCompression() && m_textureBuilder.IsUsingSTBCompressionRenderThread())
	{
		return;
	}

	bool bFencePending = GRCDEVICE.IsFencePending(m_renderCompletionFence);
	if (bFencePending == false)
	{
		// reset fence
		m_renderState = kRenderDone;

#if __BANK
		if (PARAM_pedheadshotdebug.Get())
		{
			Displayf("[PHM] PedHeadshotManager::UpdateRenderCompletionFenceStatus (CROSSED): state: %u", m_renderState);
		}
#endif
	}
#if __BANK
	else if (PARAM_pedheadshotdebug.Get())
	{
		Displayf("[PHM] PedHeadshotManager::UpdateRenderCompletionFenceStatus (WAITING): state: %u", m_renderState);
	}

#endif
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::IsRenderingFinished()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	if (m_renderState == kRenderDone)
	{
		m_renderState = kRenderIdle;
		
#if __BANK
		if (PARAM_pedheadshotdebug.Get())
		{
			Displayf("[PHM] PedHeadshotManager::IsRenderingFinished (OK - RESET): state: %u", m_renderState);
		}
#endif
		return true;
	}
#if __BANK
	else if (PARAM_pedheadshotdebug.Get())
	{
		Displayf("[PHM] PedHeadshotManager::IsRenderingFinished (WAITING): state: %u", m_renderState);
	}

#endif
	return false;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::DebugRender() 
{

#if __BANK
	if (!sm_bDiplayActiveHeadshots)
	{
		return;
	}

	const float pad = 32.0f;
	const float w = (float)kHeadshotWidth;
	const float h = (float)kHeadshotHeight;
	float x = w;
	float y = pad + sm_debugDrawVerticalOffset;

	for (int i = 0; i < kMaxPedHeashots; i++)
	{
		PedHeadshotHandle handle = PEDHEADSHOTMANAGER.SlotIndexToHandle(i);

		bool bIsActive = PEDHEADSHOTMANAGER.IsActive(handle);
		bool bIsRegistered = PEDHEADSHOTMANAGER.IsRegistered(handle);

		if (bIsRegistered)
		{
			const bool bTransparentBg = bIsActive ? PEDHEADSHOTMANAGER.HasTransparentBackground(handle) : false;

			PUSH_DEFAULT_SCREEN();
			grcStateBlock::SetBlendState((bTransparentBg ? grcStateBlock::BS_Normal : grcStateBlock::BS_Default));
			const grcTexture* pTexture = (bIsActive ? PEDHEADSHOTMANAGER.GetTexture(handle) : grcTexture::NoneWhite);
			grcBindTexture(pTexture);
			grcDrawSingleQuadf(x,y,x+w,y+h,0,0,0,1,1,Color32(255,255,255,255));

			const char* pName	= bIsActive ? PEDHEADSHOTMANAGER.GetNameForEntry(handle)			: NULL;
			int numRefs			= bIsActive ? PEDHEADSHOTMANAGER.GetNumRefsforEntry(handle)			: 0;
			int texNumRefs		= bIsActive ? PEDHEADSHOTMANAGER.GetTextureNumRefsforEntry(handle)	: 0;

			const float ooWidth = 1.0f/(float)grcDevice::GetWidth();
			const float ooHeight = 1.0f/(float)grcDevice::GetHeight();
			Vector2 pos;

			if (pName)
			{
				pos.x = x*ooWidth;
				pos.y = (y+h+4.0f)*ooHeight;
				grcDebugDraw::Text(pos, Color32(255,255,255,255), pName, true);
			}

			char cNumRefs[32];
			sprintf(cNumRefs, "shot %2d ref'd: %3d", handle, numRefs);
			
			pos.x = x*ooWidth;
			pos.y = (y+h+4.0f+12.0f)*ooHeight;
			grcDebugDraw::Text(pos, Color32(255,255,255,255), &cNumRefs[0], true);

			pos.x = x*ooWidth;
			pos.y = (y+h+4.0f+12.0f*2.0f)*ooHeight;

			if (bIsActive == false)
			{
				const char* pStr = GetDebugInfoStr(handle);
				grcDebugDraw::Text(pos, Color32(255,255,255,255), pStr, true);
			}
			else
			{
				sprintf(cNumRefs, "texture ref'd: %3d", texNumRefs);
				grcDebugDraw::Text(pos, Color32(255,255,255,255), &cNumRefs[0], true);
			}

			POP_DEFAULT_SCREEN();

			x += w+pad;
			if (x+w > GRCDEVICE.GetWidth()-w)
			{
				x = w;
				y += h+pad*2;
			}
		}
	}

#endif
}

#if __BANK
PedHeadshotManager::PedHeadshotDebugLight			PedHeadshotManager::sm_debugLights[4];

bool 					PedHeadshotManager::sm_bEditLights 								= false;
bool 					PedHeadshotManager::sm_bSyncLights 								= false;
bool 					PedHeadshotManager::sm_dumpLightSettings						= false;
bool 					PedHeadshotManager::sm_bAddHeadshot								= false;
bool 					PedHeadshotManager::sm_bContinousHeadshot						= false;
bool 					PedHeadshotManager::sm_bDiplayActiveHeadshots					= false;
bool 					PedHeadshotManager::sm_bReleaseHeadshot							= false;
bool 					PedHeadshotManager::sm_bLockInHeadshotRendering					= false;
bool 					PedHeadshotManager::sm_bSkipRenderOpaque						= false;
bool 					PedHeadshotManager::sm_bSkipRenderAlpha 						= false;
bool 					PedHeadshotManager::sm_bSkipRenderCutout						= false;
bool 					PedHeadshotManager::sm_bDoNextHeadshotWithTransparentBackground = false;
bkCombo*				PedHeadshotManager::sm_pEntryNamesCombo							= 0;
int						PedHeadshotManager::sm_entryNamesComboIdx						= 0;
int						PedHeadshotManager::sm_selectedEntryIdx							= -1;
atArray<const char*>	PedHeadshotManager::sm_entryNames;
bool					PedHeadshotManager::sm_bNextPedWithPose							= false;
bool					PedHeadshotManager::sm_bForceNullPedPointerDuringRender			= false;
int						PedHeadshotManager::sm_numFramesToDelayTextureLoading			= 0;
int						PedHeadshotManager::sm_numFramesToDelayTextureLoadingPrev		= 0;
int						PedHeadshotManager::sm_numFramesToSkipSafeWaitForCompression	= -1;
char					PedHeadshotManager::sm_debugStr[64]								= "";
bool					PedHeadshotManager::sm_bAutoRegisterPed							= false;
bool					PedHeadshotManager::sm_bSkipRendering							= false;
bool					PedHeadshotManager::sm_bReleaseNextCurrentEntry					= false;
float					PedHeadshotManager::sm_maxSqrdRadiusForAutoRegisterPed			= 5000.0f;
float					PedHeadshotManager::sm_debugDrawVerticalOffset					= 0.0f;
bool					PedHeadshotManager::sm_bMultipleAddToDrawlist					= false;

//////////////////////////////////////////////////////////////////////////
//
const char* PedHeadshotManager::GetNameForEntry(const PedHeadshotInfo& entry) const 
{
	if (entry.isRegistered && entry.isWaitingOnRelease == false && entry.pPed != NULL) 
	{
		return entry.pPed->GetDebugName();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
const char* PedHeadshotManager::GetNameForEntry(PedHeadshotHandle handle) const 
{
	int slot = HandleToSlotIndex(handle);
	bool bInRange = (slot >= 0 && slot < kMaxPedHeashots);

	if (Verifyf(bInRange, "PedHeadshotManager::GetNameForEntry: index out of range (%d)", slot) && 
		m_entriesPool[slot].pSourcePed != NULL)
	{
		return m_entriesPool[slot].pSourcePed->GetDebugName();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
int PedHeadshotManager::GetSlotIndexFromName(atHashString inName) const 
{
		
	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		if (m_entriesPool[i].isRegistered && m_entriesPool[i].isWaitingOnRelease == false  && m_entriesPool[i].pSourcePed != NULL && m_entriesPool[i].pSourcePed->GetDebugName() != NULL)
		{
			const atHashString pedName(m_entriesPool[i].pSourcePed->GetDebugName());
			
			if (inName == pedName) 
			{
				return i;
			}
		}
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::UpdateEntryNamesList(const atVector<PedHeadshotInfo, kMaxPedHeashots>& entries) 
{
	sm_entryNames.Reset();
	sm_entryNames.PushAndGrow("All");

	for (int i = 0; i < entries.GetCount(); i++)
	{
		if (entries[i].isRegistered && entries[i].isWaitingOnRelease == false)
		{
			const char* pName = GetNameForEntry(entries[i]);
			sm_entryNames.PushAndGrow(pName);
		}
	}

	sm_entryNamesComboIdx = (sm_entryNames.GetCount()==2)?1:0;
	sm_selectedEntryIdx = -1;

	if (sm_entryNamesComboIdx == 1)
	{
		OnEntryNameSelected();
	}

	if (sm_pEntryNamesCombo != NULL)
	{
		sm_pEntryNamesCombo->UpdateCombo("Registered Headshots", &sm_entryNamesComboIdx, sm_entryNames.GetCount(), &sm_entryNames[0], datCallback(MFA(PedHeadshotManager::OnEntryNameSelected), (datBase*)this));
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::OnEntryNameSelected() 
{
	int idx = sm_entryNamesComboIdx;
	if (idx < 0 || idx > sm_entryNames.GetCount())
	{
		return;
	}

	static const atHashString allEntries("All",0x45835226);

	// if the new entry option is selected,  reset the editable preset instance
	atHashString selectionName = atHashString(sm_entryNames[idx]);
	if (selectionName == allEntries)
	{
		sm_selectedEntryIdx = -1;
	}
	else
	{
		sm_selectedEntryIdx = GetSlotIndexFromName(selectionName);
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::AddWidgets(rage::bkBank& bank) 
{
	bank.PushGroup("Ped Headshots");
	bank.AddToggle("Add Headshot From Picker Selection", &sm_bAddHeadshot);
	bank.AddToggle("Continous Headshot From Picker Selection", &sm_bContinousHeadshot);
	bank.AddToggle("Display Reg. Headshot Info", &sm_bDiplayActiveHeadshots);
	bank.AddSlider("Scroll Reg. Headshot Info", &sm_debugDrawVerticalOffset, -720.0f, 720.0f, 0.05f);
	sm_pEntryNamesCombo = bank.AddCombo("Registered Headshots", &sm_entryNamesComboIdx, sm_entryNames.GetCount(), &sm_entryNames[0], datCallback(MFA(PedHeadshotManager::OnEntryNameSelected), (datBase*)&PEDHEADSHOTMANAGER));
	bank.AddToggle("Release Selected Headshot", &sm_bReleaseHeadshot);

	bank.AddSeparator();
	PEDHEADSHOTMANAGER.m_textureExporter.AddWidgets(bank);

	bank.AddSeparator();
	bank.PushGroup("Stress Testing");
		bank.AddToggle("Automatically Register Headshots Via Ped Population", &sm_bAutoRegisterPed);
		bank.AddToggle("Call AddToDrawlist Twice", &sm_bMultipleAddToDrawlist);
		bank.AddSlider("Max Num Frames To Wait For Render", &sm_maxNumFramesToWaitForRender, 0, 5000, 1);
		bank.AddSlider("Max Squared Radius To Accept Ped Population Requests", &sm_maxSqrdRadiusForAutoRegisterPed, 0.0f, 500000.0f, 0.25f);
		bank.AddSeparator();
		bank.AddToggle("Release Next Entry In Process", &sm_bReleaseNextCurrentEntry);
		bank.AddToggle("Skip Rendering", &sm_bSkipRendering);
		bank.AddToggle("Force Null Ped Pointer During Rendering", &sm_bForceNullPedPointerDuringRender);
		bank.AddSlider("Num Frames to Delay Texture Loading", &sm_numFramesToDelayTextureLoading, -1, 200, 1);
		bank.AddSlider("Num Frames to Skip Compression Safe Wait", &sm_numFramesToSkipSafeWaitForCompression, -1, 3, 1);
	bank.PopGroup();

	bank.AddSeparator();
	bank.AddToggle("Request Transparent Background For Next Shot", &sm_bDoNextHeadshotWithTransparentBackground);

	bank.AddSeparator();
	bank.AddToggle("Do Next Headshot With Pose", &sm_bNextPedWithPose);
	bank.AddVector("Posed Ped Camera Position", &sm_posedPedCamPos, -100.0f, 100.0f, 0.001f);
	bank.AddVector("Posed Ped Camera Angles", &sm_posedPedCamAngles, -180.0f, 180.0f, 0.001f);

	bank.AddSeparator();
	bank.AddToggle("Lock Headshot in rendering stage", &sm_bLockInHeadshotRendering);
	bank.AddToggle("Skip Opaque Bucket", &sm_bSkipRenderOpaque);
	bank.AddToggle("Skip Alpha Bucket", &sm_bSkipRenderAlpha);
	bank.AddToggle("Skip Cutout Bucket", &sm_bSkipRenderCutout);

	bank.AddSeparator();
	bank.AddSlider("Camera Vertical FOV" ,&sm_defaultVFOVDeg,  10.0f, 90.0f, 0.01f);
	bank.AddSlider("Camera Depth Offset", &sm_defaultCamDepthOffset, -2.0f, 2.0f, 0.01f);
	bank.AddSlider("Camera Height Offset", &sm_defaultCamHeightOffset, 0.1f, 2.0f, 0.01f);
	bank.AddSlider("Tonemap Exposure", &sm_defaultExposure, 0.0f, 16.0f, 0.01f);

	bank.AddSeparator();
	bank.PushGroup("Lighting", false);
		bank.AddToggle("Dump Light Settings", &sm_dumpLightSettings);
		bank.AddToggle("Edit Lights", &sm_bEditLights);
		for (u32 i = 0; i < 4; i++)
		{
			bank.AddToggle("Disable", &sm_debugLights[i].isDisabled);
			bank.AddVector("Light Position:", &sm_debugLights[i].pos, -100.0f, 100.0f, 0.01f, datCallback(CFA(PedHeadshotManager::DebugLightsCallback)),NULL);
			bank.AddColor("Light Colour:",  &sm_debugLights[i].color, datCallback(CFA(PedHeadshotManager::DebugLightsCallback)));
			bank.AddSlider("Light Intensity:",  &sm_debugLights[i].intensity, 0.0f,  20.0f, 0.01f, datCallback(CFA(PedHeadshotManager::DebugLightsCallback)));
			bank.AddSlider("Light Radius:",  &sm_debugLights[i].radius, 0.0f,  200.0f, 0.01f, datCallback(CFA(PedHeadshotManager::DebugLightsCallback)));
			bank.AddSeparator();
		}
		bank.AddVector("Ambient Up:", &sm_ambientLight[0], -100.0f, 100.0f, 0.01f,NullCB,NULL);
		bank.AddVector("Ambient Down:", &sm_ambientLight[1], -100.0f, 100.0f, 0.01f,NullCB,NULL);	
	bank.PopGroup();

	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::SetSourcePed(PedHeadshotHandle handle, CPed* pPed)
{
	m_entriesPool[HandleToSlotIndex(handle)].pSourcePed = pPed;
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotHandle PedHeadshotManager::RegisterPedAuto(CPed* pPed)
{
	PedHeadshotHandle handle = 0;
	if (sm_bAutoRegisterPed && pPed != NULL)
	{
		Vector3 delta = (VEC3V_TO_VECTOR3(pPed->GetMatrix().d()) - camInterface::GetPos());
		float magSqrd = delta.Mag2();

		if (magSqrd < sm_maxSqrdRadiusForAutoRegisterPed)
		{
			handle = RegisterPed(pPed);

			if (handle != INVALID_HANDLE)
			{
				// Keep track of the original ped so it can be unregistered
				SetSourcePed(handle, pPed);
			}
		}
	}

	return handle;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::UnregisterPedAuto(CPed* pPed)
{
	if (sm_bAutoRegisterPed == false)
	{
		return;
	}

	for (int i = 0; i < m_entriesPool.GetCount(); i++)
	{
		if (m_entriesPool[i].pSourcePed == pPed && m_entriesPool[i].isRegistered && m_entriesPool[i].isWaitingOnRelease == false)
		{
			PedHeadshotHandle handle = SlotIndexToHandle(i);
			UnregisterPed(handle);
			return;
		}
	}	
}

//////////////////////////////////////////////////////////////////////////
//
const char*	PedHeadshotManager::GetDebugInfoStr(PedHeadshotHandle handle)
{
	PedHeadshotState state = PEDHEADSHOTMANAGER.GetPedHeashotState(handle);

	switch (state)
	{
		case (PHS_QUEUED):
		{
			sprintf(&sm_debugStr[0], "%s (%d)", "QUEUED", (int)(handle));
			break;
		}

		case (PHS_WAITING_ON_TEXTURE):
		{
			sprintf(&sm_debugStr[0], "%s (%d)", "WAIT_ON_TEXTURE", (int)(handle));
			break;
		}

		case (PHS_READY):
		{
			sprintf(&sm_debugStr[0], "%s (%d)", "READY", (int)(handle));
			break;
		}

		case (PHS_FAILED):
		{
			sprintf(&sm_debugStr[0], "%s (%d)", "FAILED", (int)(handle));
			break;
		}

		default:
		{
			sprintf(&sm_debugStr[0], "%s (%d)", "INVALID", (int)(handle));
		}

	}

	return &sm_debugStr[0];
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::UpdateDebug()
{

	UpdateDebugLights();

	if (sm_bReleaseHeadshot)
	{
		if (sm_selectedEntryIdx != -1)
		{
			PEDHEADSHOTMANAGER.UnregisterPed(PedHeadshotHandle(sm_selectedEntryIdx + 1));
			sm_bReleaseHeadshot = false;
		}
		else
		{
			for (int i = 0; i < kMaxPedHeashots; i++) 
			{
				PedHeadshotHandle handle(i + 1);
				if (PEDHEADSHOTMANAGER.IsRegistered(handle))
				{
					PEDHEADSHOTMANAGER.UnregisterPed(handle);
				}
			}
		}

		PEDHEADSHOTMANAGER.UpdateEntryNamesList(PEDHEADSHOTMANAGER.m_entriesPool);
		sm_bReleaseHeadshot = false;
	}
	else if (sm_bAddHeadshot)
	{
		// Peds from picker
		CPed* pPeds[4] = { NULL, NULL, NULL, NULL };

		s32 numEntities = rage::Min<s32>(g_PickerManager.GetNumberOfEntities(), 4);
		s32 numPeds = 0;

		for (s32 i = 0; i < numEntities; i++)
		{
			fwEntity* pEntity = g_PickerManager.GetEntity(i);

			if (pEntity != NULL && pEntity->GetType() == ENTITY_TYPE_PED)
			{
				pPeds[i] = static_cast<CPed*>(pEntity);
				numPeds++;
			}
		}

		if (numPeds > 0)
		{
			for (int i = 0; i < numPeds; i++)
			{
				if (pPeds[i] != NULL)
				{
					if (sm_bNextPedWithPose)
					{
						PEDHEADSHOTMANAGER.RegisterPedWithAnimPose(pPeds[i], sm_bDoNextHeadshotWithTransparentBackground);
					}
					else
					{
						PEDHEADSHOTMANAGER.RegisterPed(pPeds[i], sm_bDoNextHeadshotWithTransparentBackground);
					}
					sm_bDoNextHeadshotWithTransparentBackground = false;
				}
			}
		}
		sm_bAddHeadshot = false;
		PEDHEADSHOTMANAGER.UpdateEntryNamesList(PEDHEADSHOTMANAGER.m_entriesPool);
	}

}

//////////////////////////////////////////////////////////////////////////
//
#define PH_ENABLE_DUMP_LIGHT_SETTINGS (0)

void PedHeadshotManager::UpdateDebugLights() 
{
	if (sm_dumpLightSettings)
	{
		sm_dumpLightSettings = false;

#if PH_ENABLE_DUMP_LIGHT_SETTINGS
		for (u32 i = 0; i < 4; i++) 
		{
			Displayf("	sm_defLightSource[%u] = CLightSource(LIGHT_TYPE_POINT, LIGHTFLAG_FX, Vector3(%ff, %ff, %ff), Vector3(%ff, %ff, %ff), %ff, LIGHT_ALWAYS_ON);\n \
						sm_defLightSource[%u].SetRadius(%ff);", 
						i, 
						sm_debugLights[i].pos.x, sm_debugLights[i].pos.y, sm_debugLights[i].pos.z,
						sm_debugLights[i].color.GetRedf(), sm_debugLights[i].color.GetGreenf(), sm_debugLights[i].color.GetBluef(),
						sm_debugLights[i].intensity, i, sm_debugLights[i].radius);
		}
#endif
	}

	if (sm_bEditLights == false) 
	{
		sm_bSyncLights = true;
		return;
	}

	if (sm_bSyncLights)
	{
		for (u32 i = 0; i < 4; i++) 
		{
			sm_debugLights[i].pos		= sm_defLightSource[i].GetPosition();
			sm_debugLights[i].intensity = sm_defLightSource[i].GetIntensity();
			sm_debugLights[i].radius	= sm_defLightSource[i].GetRadius();
			sm_debugLights[i].color		= Color32(sm_defLightSource[i].GetColor());			
		}
		sm_bSyncLights = false;
		return;
	}

	for (u32 i = 0; i < 4; i++) 
	{
		if (sm_debugLights[i].isDisabled == false)
		{
			sm_defLightSource[i].SetPosition(sm_debugLights[i].pos);
			sm_defLightSource[i].SetIntensity(sm_debugLights[i].intensity);
			sm_defLightSource[i].SetRadius(sm_debugLights[i].radius);
			Vec3V vCol = sm_debugLights[i].color.GetRGB();
			sm_defLightSource[i].SetColor(RCC_VECTOR3(vCol));
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::DebugLightsCallback()
{
	sm_bReleaseHeadshot = true;
	sm_selectedEntryIdx = -1;
}

#endif 

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotManager::RequestTextureUpload(PedHeadshotHandle handle, int forcedCharacterId)
{
	// Fail if there's already an upload in progress
	if (IsTextureUploadAvailable() == false)
	{
		return false;
	}

	// Only continue if the current headshot is active
	if (PEDHEADSHOTMANAGER.IsActive(handle) == false)
	{
		return false;
	}

	int idx = HandleToSlotIndex(handle);
	s32 txdSlot = m_entriesPool[idx].txdId.Get();
	int characterId = forcedCharacterId;

	if (forcedCharacterId == -1)
	{
		characterId = StatsInterface::GetCurrentMultiplayerCharaterSlot(); 
	}


	if (characterId < 0 || characterId > 4)
	{
		return false;
	}

#if RSG_ORBIS
	const char* platformExt = "_ps4";
#elif RSG_DURANGO
	const char* platformExt = "_xboxone";
#else
	const char* platformExt = "";
#endif

	char filename[64];
	sprintf(&filename[0], "share/gta5/mpchars/%d%s.dds", characterId, platformExt);

	bool bOk = m_textureExporter.RequestUpload(txdSlot, &filename[0]);
	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotManager::ReleaseTextureUploadRequest(PedHeadshotHandle handle)
{
	// Only continue if the current headshot is active
	if (PEDHEADSHOTMANAGER.IsActive(handle) == false)
	{
		return;
	}

	int idx = HandleToSlotIndex(handle);
	s32 txdSlot = m_entriesPool[idx].txdId.Get();

	m_textureExporter.ReleaseRequest(txdSlot);
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::Init()
{

}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::Shutdown()
{
	ReleaseRequestData();
	Reset();
}


//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::Reset()
{
	m_state = AVAILABLE;
	m_txdSlot = -1;
	m_txdSlotReleaseCheck = -1;
	m_texture = NULL;
	m_bReleaseRequested = false;
	m_pWorkBuffer0 = NULL;
#if __XENON
	m_pWorkBuffer1 = NULL;
#endif
	m_requestId = -1;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureExporter::RequestUpload(s32 txdSlot, const char* pCloudPath)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// We might be busy with another request already
	if (CanProcessRequest() == false)
	{
		return false;
	}

	// Check path is valid
	if (pCloudPath == NULL)
	{
		return false;
	}

	// Check the txd slot is valid and has at least one reference
	if (g_TxdStore.IsValidSlot(strLocalIndex(txdSlot)) == false || g_TxdStore.GetNumRefs(strLocalIndex(txdSlot)) < 1)
	{
		return false;
	}

	// Check the txd has a valid entry
	fwTxd* pTxd = g_TxdStore.Get(strLocalIndex(txdSlot));
	if (pTxd == NULL || pTxd->GetCount() == 0)
	{
		return false;
	}

	// Check the texture is valid
	grcTexture* pTexture = pTxd->GetEntry(0);
	if (pTexture == NULL)
	{
		return false;
	}

	// All good, add a ref to the txd and cache the texture 
	g_TxdStore.AddRef(strLocalIndex(txdSlot), REF_OTHER);
	m_texture = pTexture;
	
	// Bail if we can't allocate the temporary buffers
	if (PrepareRequestData() == false)
	{		
		// Remove ref
		g_TxdStore.RemoveRef(strLocalIndex(txdSlot), REF_OTHER);
		m_texture = NULL;
		return false;
	}

	// Cache the txdSlot for sanity checks
	m_txdSlot = txdSlot;
	m_txdSlotReleaseCheck = txdSlot;

	// Cache file path
	strcpy(&m_fileName[0], pCloudPath);

	// Lock further requests 
	m_state = REQUEST_IN_PROGRESS;

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::ReleaseRequest(s32 txdSlot)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	if (Verifyf(m_txdSlotReleaseCheck == txdSlot, "PedHeadshotTextureExporter::ReleaseRequest: request for txd slot %d doesn't match current request (%d)", txdSlot, m_txdSlotReleaseCheck))
	{
		m_txdSlotReleaseCheck = -1;
		m_bReleaseRequested = true;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::Update()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

#if __BANK
	UpdateDebug();
#endif

	// Nothing to do
	if (m_state == AVAILABLE || m_state == REQUEST_WAITING_ON_UPLOAD)
	{
		return;
	}

	// Prepare the DDS buffer
	if (m_state == REQUEST_IN_PROGRESS)
	{
		m_state = ProcessDDSBuffer();
	}

	// Process upload request
	if (m_state == REQUEST_UPLOADING)
	{
	#if __BANK
		// Save to disk
		if (ms_bSaveToDisk)
		{
			SaveDDSToDisk();
		}

		// Skip uploading
		if (ms_bDontUpload)
		{
			m_state = REQUEST_SUCCEEDED;
			return;
		}
	#endif
		m_state = ProcessUploadRequest();
	}

	// Check for failures
	if (m_state == REQUEST_FAILED)
	{
		m_state = ProcessFailedRequest();
	}

	// If the request is on a failed or succedded state and the user has requested for it to be released,
	// this is where we can safely do it.
	if ((m_state == REQUEST_FAILED || m_state == REQUEST_SUCCEEDED) && m_bReleaseRequested)
	{
		ReleaseRequestData();
		m_state = AVAILABLE;
		m_bReleaseRequested = false;
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::OnCloudEvent(const CloudEvent* pEvent)
{
	// We only care about requests
	if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
	{
		return;
	}

	// Grab event data
	const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

	// Check if we're interested
	if(pEventData->nRequestID != m_requestId)
	{
		return;
	}

	// Check if it succeeded
	if (pEventData->bDidSucceed == false)
	{
		m_state = REQUEST_FAILED;
		return;
	}

	// Succeeded!
	m_state = REQUEST_SUCCEEDED;
	return;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureExporter::CanProcessRequest() const
{
	return (m_state == AVAILABLE);
}

//////////////////////////////////////////////////////////////////////////
//
bool PedHeadshotTextureExporter::PrepareRequestData()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	if (m_texture == NULL)
	{
		return false;
	}

	int width = m_texture->GetWidth();
	int height = m_texture->GetHeight();
	grcImage::Format format = (grcImage::Format)m_texture->GetImageFormat();

	// Check the format is correct
	if (format != grcImage::DXT1 && format != grcImage::DXT5)
	{
		return false;
	}

	// Compute required size
#if __D3D11 || RSG_ORBIS
	int pitch = (format == grcImage::DXT1) ? (((width+3)/4) * 2) : (((width+3)/4) * 4);
#else
	int pitch = (format == grcImage::DXT1) ? (((width+3)/4) * 8) : (((width+3)/4) * 16); 
#endif
	u32 headerSize = GetDDSHeaderSize();
	u32 textureDataSize = (u32)(pitch*height);

	// Try allocating the buffers
	Assertf(m_pWorkBuffer0 == NULL, "PedHeadshotTextureExporter::PrepareRequestData: work buffers have not been released");
	m_pWorkBuffer0 = rage_new char[headerSize+textureDataSize];
	if (m_pWorkBuffer0 == NULL)
	{
		Displayf("PedHeadshotTextureExporter::PrepareRequestData: failed to allocate buffer");
		return false;
	}
#if __XENON
	Assertf(m_pWorkBuffer1 == NULL, "PedHeadshotTextureExporter::PrepareRequestData: work buffers have not been released");
	m_pWorkBuffer1 = rage_new char[textureDataSize];
	if (m_pWorkBuffer1 == NULL)
	{
		delete m_pWorkBuffer0;
		m_pWorkBuffer0 = NULL;
		Displayf("PedHeadshotTextureExporter::PrepareRequestData: failed to allocate buffer");
		return false;
	}
#endif

	// Cache data neeeded when packing the dds buffer
	m_textureData.width = (u32)width;
	m_textureData.height = (u32)height;
	m_textureData.pitch = (u32)pitch;
	m_textureData.imgFormat = (u32)format;
	m_textureData.isValid = true;

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::ReleaseRequestData()
{
	// Clean up if we haven't done so already
	if (m_pWorkBuffer0 != NULL)
	{
		delete (char*) m_pWorkBuffer0;
		m_pWorkBuffer0 = NULL;
#if __XENON
		delete (char*) m_pWorkBuffer1;
		m_pWorkBuffer1 = NULL;
#endif

		// Check the txd slot is valid and has at least one reference
		if (g_TxdStore.IsValidSlot(m_txdSlot) != false && g_TxdStore.GetNumRefs(m_txdSlot) > 0)
		{
			// Remove our ref and forget about the texture
			g_TxdStore.RemoveRef(m_txdSlot, REF_OTHER);
			m_txdSlotReleaseCheck = m_txdSlot.Get();
			m_txdSlot = -1;
			m_texture = NULL;
		}

		// Reset cached texture data
		m_textureData.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
//
u32 PedHeadshotTextureExporter::GetDDSHeaderSize() const
{
	// header + magic number
	return (u32)(sizeof(DDSURFACEDESC2)+sizeof(s32));
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotTextureExporter::State PedHeadshotTextureExporter::ProcessDDSBuffer()
{
	// Is the texture still with us?
	if (m_texture == NULL || m_pWorkBuffer0 == NULL XENON_ONLY(|| m_pWorkBuffer1 == NULL))
	{
		return REQUEST_FAILED;
	}

	// Check texture hasn't changed without our knowledge!
	if (m_texture->GetWidth() != (int)m_textureData.width	||
		m_texture->GetHeight() != (int)m_textureData.height ||
		m_texture->GetImageFormat() != m_textureData.imgFormat)
	{
		return REQUEST_FAILED;
	}


	// Create a stream from our memory buffer to simplify writing to it...
	char memFileName[RAGE_MAX_PATH];
#if __D3D11 || RSG_ORBIS
	u32 bufferSize = GetDDSHeaderSize()+(m_textureData.pitch*m_textureData.height);
#else
	u32 bufferSize = GetDDSHeaderSize()+(m_textureData.height*m_textureData.height);
#endif
	fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, m_pWorkBuffer0, bufferSize, false, "PedHeadshotManager");
	fiStream* S = ASSET.Create(memFileName, "");

	if (S == NULL)
	{
		return REQUEST_FAILED;
	}

	// Now we can assemble the dds file...

	// Write magic number
	s32 magic = MAKE_MAGIC_NUMBER('D','D','S',' ');
	S->WriteInt(&magic, 1);

	// Write header
	DDSURFACEDESC2 header;
	memset(&header,0,sizeof(header));
	header.dwSize = sizeof(header);
	header.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
	header.dwHeight = m_textureData.height;
	header.dwWidth =  m_textureData.width;
	header.dwPitchOrLinearSize =  m_textureData.pitch* m_textureData.height;
	header.dwFlags |= DDSD_LINEARSIZE;

	DDPIXELFORMAT9 &ddpf = header.ddpfPixelFormat;
	ddpf.dwSize = sizeof(ddpf);
	ddpf.dwFlags = DDPF_FOURCC;
	ddpf.dwFourCC = ((grcImage::Format)m_textureData.imgFormat == grcImage::DXT1) ? MAKE_MAGIC_NUMBER('D','X','T','1') : MAKE_MAGIC_NUMBER('D','X','T','5');

	S->WriteInt(&header.dwSize,sizeof(header)/4);

	// Write data
	void* pBits = NULL;
#if __PS3
	grcTexture* pTexture = m_texture;
	grcTextureGCM* pGcmTex = (grcTextureGCM*)pTexture;
	pBits = pGcmTex->GetBits();
#elif __XENON
	grcTexture* pTexture = m_texture;
	grcTextureXenon* pD3DTex = (grcTextureXenon*)pTexture;
	IDirect3DTexture9* texObj = static_cast<IDirect3DTexture9*>(pD3DTex->GetTexturePtr());

	XGTEXTURE_DESC desc;
	XGGetTextureDesc(texObj, 0, &desc);
	u32 texAddr = ((u32)texObj->Format.BaseAddress) << 12;
	pBits = (void*)texAddr;
#elif RSG_PC || RSG_ORBIS
	grcTexture* pTexture = m_texture;

#if RSG_PC
	((grcTextureD3D11*)pTexture)->InitializeTempStagingTexture();
	((grcTextureD3D11*)pTexture)->UpdateCPUCopy();
#endif
	grcTextureLock srcLock;
	if (pTexture->LockRect(0, 0, srcLock, grcsRead)) 
	{
		pBits = srcLock.Base;
	}
#elif RSG_DURANGO
	grcTexture* pTex = m_texture;
	grcTextureDurango* pTexture = (grcTextureD3D11*)pTex;
	u32 pitch = pTexture->GetWidth()*pTexture->GetBitsPerPixel()/8;
	u32 buffSize = pitch*pTexture->GetHeight();
	void* pBuff = alloca(buffSize);
	pTexture->GetLinearTexels(pBuff,buffSize,0);
	pBits = pBuff;
#else	// other platform
	AssertMsg(0, "PedHeadshotTextureExporter::ProcessDDSBuffer() - Not Implemented");
#endif

	if (pBits == NULL)
	{
		S->Close();
		return REQUEST_FAILED;
	}

#if __XENON
	S->Seek(bufferSize);
	void* pDst = (void*)(((char*)m_pWorkBuffer0)+GetDDSHeaderSize());
	void* pSrc = (void*)m_pWorkBuffer1;

	// Copy pixel data to our work buffer
	sysMemCpy(pSrc, pBits, header.dwPitchOrLinearSize);

	// Byte swap copied pixel data
	int formatByteSwap = grcImage::GetFormatByteSwapSize((grcImage::Format)m_textureData.imgFormat);
	if (formatByteSwap == 2)
	{
		s16* pSrcDataAsS16 = (s16*)pSrc;
		for (int i = 0; i < header.dwPitchOrLinearSize/sizeof(s16); i++)
		{
			pSrcDataAsS16[i] = sysEndian::LtoB(pSrcDataAsS16[i]);
		}
	}

	// Untile the byte-swapped copy into our work buffer - XGraphics won't allocate anything internally since
	// source and destination are different.
	DWORD flags = 0;
	XGUntileTextureLevel(m_textureData.width, m_textureData.height, 0, XGGetGpuFormat(desc.Format), flags, pDst, m_textureData.pitch, NULL, pSrc, NULL);
#elif RSG_ORBIS
	u32 pitch = srcLock.Width*srcLock.BitsPerPixel/8;
	u32 buffSize = pitch*srcLock.Height;
	void* pBuff = alloca(buffSize);

	const sce::Gnm::Texture *srcGnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(pTexture->GetTexturePtr());
	uint64_t offset;
	uint64_t size;
	sce::GpuAddress::computeTextureSurfaceOffsetAndSize(&offset,&size,srcGnmTexture,0,0);
	sce::GpuAddress::TilingParameters tp;
	tp.initFromTexture(srcGnmTexture, 0, 0);

	u8* pSrc = (u8*)alloca(size*4);
	sce::GpuAddress::detileSurface(pSrc, ((char*)srcGnmTexture->getBaseAddress() + offset),&tp);



	u8* pDst = (u8*)pBuff;
	for (u32 i=0;i<srcLock.Height/4;++i)
	{
		sysMemCpy(pDst,pSrc,pitch*4);
		pDst += pitch*4;
		pSrc += srcLock.Pitch;
	}
	pTexture->UnlockRect(srcLock);
#elif RSG_PC
	pTexture->UnlockRect(srcLock);
	((grcTextureD3D11*)pTexture)->ReleaseTempStagingTexture();
#endif
#if RSG_ORBIS || RSG_DURANGO
	S->WriteByte((char*)pBuff, (size_t)buffSize);
#else
	S->WriteByte((char*)pBits, (size_t)header.dwPitchOrLinearSize);
#endif

	// Close stream
	S->Close();

	return REQUEST_UPLOADING;
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotTextureExporter::State PedHeadshotTextureExporter::ProcessUploadRequest() 
{
	// Check the data is still there
	if (m_pWorkBuffer0 == NULL || m_textureData.isValid == false)
	{
		return REQUEST_FAILED;
	}

	// Check the cloud is available
	if (NetworkInterface::IsCloudAvailable() == false)
	{
		return REQUEST_FAILED;
	}

	// Submit the post request
	u32 bufferSize = GetDDSHeaderSize()+(m_textureData.pitch*m_textureData.height);
	const char* pFilePath = &m_fileName[0];
	m_requestId = CloudManager::GetInstance().RequestPostMemberFile(NetworkInterface::GetLocalGamerIndex(), pFilePath, m_pWorkBuffer0, bufferSize, true);	//	last param is bCache

	// Bail if we got an invalid cloud request id
	if (m_requestId == -1)
	{
		return REQUEST_FAILED;
	}


	return REQUEST_WAITING_ON_UPLOAD;
}

//////////////////////////////////////////////////////////////////////////
//
PedHeadshotTextureExporter::State PedHeadshotTextureExporter::ProcessFailedRequest() 
{
	ReleaseRequestData();

	return REQUEST_FAILED;

}

#if __BANK
bool 	PedHeadshotTextureExporter::ms_bSaveToDisk = false;
bool 	PedHeadshotTextureExporter::ms_bDontUpload = false;
bool 	PedHeadshotTextureExporter::ms_bReleaseRequest = false;
bool 	PedHeadshotTextureExporter::ms_bTriggerRequest = false;
bool	PedHeadshotTextureExporter::ms_bUseGameCharacterSlot = false;
s32		PedHeadshotTextureExporter::ms_txdSlot = -1;

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::AddWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Texture Export");
		bank.AddToggle("Use Character Slot (Only Works in MP)", &ms_bUseGameCharacterSlot);
		bank.AddToggle("Save Texture To Disk", &ms_bSaveToDisk);
		bank.AddToggle("Don't Upload Texture", &ms_bDontUpload);
		bank.AddToggle("Trigger Request", &ms_bTriggerRequest);
		bank.AddToggle("Release Request", &ms_bReleaseRequest);
	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::UpdateDebug()
{

	if (ms_bReleaseRequest)
	{
		ms_bReleaseRequest = false;
		ReleaseRequest(ms_txdSlot);
		ms_txdSlot = -1;
		return;
	}

	if (ms_bTriggerRequest)
	{
		ms_bTriggerRequest = false;

		int i = 0;
		for (i = 0; i < PEDHEADSHOTMANAGER.m_entriesPool.GetCount(); i++)
		{
			if (PEDHEADSHOTMANAGER.IsActive(PEDHEADSHOTMANAGER.SlotIndexToHandle(i)))
			{
				ms_txdSlot = PEDHEADSHOTMANAGER.m_entriesPool[i].txdId.Get();
				break;
			}
		}

		if (ms_txdSlot != -1)
		{
			if (PEDHEADSHOTMANAGER.RequestTextureUpload(PEDHEADSHOTMANAGER.SlotIndexToHandle(i), ms_bUseGameCharacterSlot ? -1 : 0) == false)
			{
				ms_txdSlot = -1;
			}
		}
	
		return;
	}

}

//////////////////////////////////////////////////////////////////////////
//
void PedHeadshotTextureExporter::SaveDDSToDisk()
{
	if (m_txdSlot == -1 || m_textureData.isValid == false || m_pWorkBuffer0 == NULL)
	{
		return;
	}

	char fileName[RAGE_MAX_PATH];
	sprintf(&fileName[0], "pedheadshot_%d_%s", m_txdSlot.Get(), PS3_SWITCH("ps3", "360"));

	fiStream *S = ASSET.Create(&fileName[0],"dds");
	if (!S)
	{
		return;
	}

	S->WriteByte((char*)m_pWorkBuffer0, (size_t)(GetDDSHeaderSize()+(m_textureData.pitch*m_textureData.height)));
	S->Close();

}

#endif

//////////////////////////////////////////////////////////////////////////
//
grcRenderTarget*	GpuDXTCompressor::ms_pRenderTargetAlias = NULL;
grmShader*			GpuDXTCompressor::ms_pShader = NULL;
grcEffectTechnique	GpuDXTCompressor::ms_compressTechniqueId;
grcEffectVar 		GpuDXTCompressor::ms_texture3VarId;
grcEffectVar 		GpuDXTCompressor::ms_texelSizeVarId;
grcEffectVar 		GpuDXTCompressor::ms_compressLodVarId;
#if RSG_PC
grcRenderTarget*	GpuDXTCompressor::ms_pRTDxtCompS = NULL;
grcRenderTarget*	GpuDXTCompressor::ms_pRTDxtCompL = NULL;
grcTexture*			GpuDXTCompressor::ms_pDxtCompS = NULL;
grcTexture*			GpuDXTCompressor::ms_pDxtCompL = NULL;

grcTexture*			GpuDXTCompressor::m_pCompressionDest = NULL;
u32					GpuDXTCompressor::m_nCompressionMipStatusBits = 0;
#endif

//////////////////////////////////////////////////////////////////////////
//
void GpuDXTCompressor::Init()
{
	ASSET.PushFolder("common:/shaders");
		ms_pShader = grmShaderFactory::GetInstance().Create();
		ms_pShader->Load("skinblend");
	ASSET.PopFolder();

	ms_compressTechniqueId	= ms_pShader->LookupTechnique("psCompressDxt1");
	ms_texture3VarId		= ms_pShader->LookupVar("PointTexture");
	ms_texelSizeVarId		= ms_pShader->LookupVar("TexelSize");
	ms_compressLodVarId		= ms_pShader->LookupVar("CompressLod");

#if RSG_PC
	int nWidth;
	int nHeight;

	if ( !UseCpuRoundtrip() )
	{
		grcTextureFactory::CreateParams pcParams;
		pcParams.Format = grctfA16B16G16R16;
		pcParams.MipLevels = 1;

		nWidth = (PedHeadshotTextureBuilder::kRtWidth)>>2;
		nHeight = (PedHeadshotTextureBuilder::kRtHeight)>>2;
		ms_pRTDxtCompL = grcTextureFactory::GetInstance().CreateRenderTarget("Dxt Comp L", grcrtPermanent, nWidth, nHeight, 64, &pcParams);

		nWidth = (PedHeadshotTextureBuilder::kRtWidth/2)>>2;
		nHeight = (PedHeadshotTextureBuilder::kRtHeight/2)>>2;
		ms_pRTDxtCompS = grcTextureFactory::GetInstance().CreateRenderTarget("Dxt Comp S", grcrtPermanent, nWidth, nHeight, 64, &pcParams);
	}
	else
	{
		grcTextureFactory::TextureCreateParams TextureParams(
			grcTextureFactory::TextureCreateParams::SYSTEM, 
			grcTextureFactory::TextureCreateParams::LINEAR,	
			grcsRead|grcsWrite, 
			NULL, 
			grcTextureFactory::TextureCreateParams::RENDERTARGET, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);

		nWidth = (PedHeadshotTextureBuilder::kRtWidth)>>2;
		nHeight = (PedHeadshotTextureBuilder::kRtHeight)>>2;
		ms_pDxtCompL = grcTextureFactory::GetInstance().Create( nWidth, nHeight, grctfA16B16G16R16, NULL, 1, &TextureParams );
		ms_pRTDxtCompL = grcTextureFactory::GetInstance().CreateRenderTarget("Dxt Comp L", ms_pDxtCompL->GetTexturePtr(), NULL);			

		nWidth = (PedHeadshotTextureBuilder::kRtWidth/2)>>2;
		nHeight = (PedHeadshotTextureBuilder::kRtHeight/2)>>2;
		ms_pDxtCompS = grcTextureFactory::GetInstance().Create( nWidth, nHeight, grctfA16B16G16R16, NULL, 1, &TextureParams );
		ms_pRTDxtCompS = grcTextureFactory::GetInstance().CreateRenderTarget("Dxt Comp S", ms_pDxtCompS->GetTexturePtr(), NULL);			
	}

#endif
}

//////////////////////////////////////////////////////////////////////////
//
void GpuDXTCompressor::Shutdown()
{
	ms_pShader = NULL;
	ReleaseRenderTarget();
#if RSG_PC
	if (ms_pRTDxtCompS)
	{
		ms_pRTDxtCompS->Release();
		ms_pRTDxtCompS = NULL;
	}
	if (ms_pRTDxtCompL)
	{
		ms_pRTDxtCompL->Release();
		ms_pRTDxtCompL = NULL;
	}
	if (ms_pDxtCompS)
	{
		ms_pDxtCompS->Release();
		ms_pDxtCompS = NULL;
	}
	if (ms_pDxtCompL)
	{
		ms_pDxtCompL->Release();
		ms_pDxtCompL = NULL;
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//
bool GpuDXTCompressor::AllocateRenderTarget(grcTexture* pDestTexture)
{
	if (ms_pRenderTargetAlias != NULL || pDestTexture == NULL)
	{
		Warningf("GpuDXTCompressor::AllocateRenderTarget: alias rt already allocated");
		return false;
	}

	char name[32];
	sprintf(&name[0], "GpuDXTCompressor");
	ms_pRenderTargetAlias = grcTextureFactory::GetInstance().CreateRenderTarget(name, pDestTexture->GetTexturePtr());

	return (ms_pRenderTargetAlias != NULL);
}

//////////////////////////////////////////////////////////////////////////
//
void GpuDXTCompressor::ReleaseRenderTarget()
{
	if (ms_pRenderTargetAlias != NULL)
	{
		ms_pRenderTargetAlias->Release();
		ms_pRenderTargetAlias = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
#if !RSG_PC
//
// Code borrowed from MeshBlendManager until we support DXT5 and can create a common interface, etc.
// NOTE: Don't pass in non-square textures into this function unless you know what you're doing.
// The resolve on 360 will auto tile the texture. If it's square it'll happen in place, if it isn't,
// the tiling will use more memory than consumed by the texture. If you resolve into a resource texture
// that's not square you're guaranteed to overwrite whatever memory comes after your last mip
bool GpuDXTCompressor::CompressDXT1(grcRenderTarget* pSrcRt, grcTexture* pDstTex)
{
	if (ms_pRenderTargetAlias == NULL || pSrcRt == NULL || pDstTex == NULL)
	{
		Warningf("GpuDXTCompressor::CompressDXT1: alias rt, src or dst texture is null");
		return false;
	}

#if __PS3
	grcTextureGCM * gcmTexture = (grcTextureGCM*)pDstTex;
	gcmTexture->OverrideMipMapCount(Min(gcmTexture->GetMipMapCount(), 4));
	gcmTexture->UpdatePackedTexture();
#endif

	// TEMP: need to fix dxt5
	if (pDstTex->GetBitsPerPixel() != 4)
		return true;

#if __PS3
	s32 renderWidth = (s32)pDstTex->GetWidth() >> 1;
	s32 renderHeight = (s32)pDstTex->GetHeight() >> 2;
#elif __XENON
	s32 renderWidth = (s32)pDstTex->GetWidth() >> 2;
	s32 renderHeight = (s32)pDstTex->GetHeight() >> 2;
#else
	s32 renderWidth = (s32)pDstTex->GetWidth();
	s32 renderHeight = (s32)pDstTex->GetHeight();
#endif

	grcRenderTarget* rt = NULL;
	if (renderWidth == ms_pRenderTargetAlias->GetWidth() && renderHeight && ms_pRenderTargetAlias->GetHeight())
	{
		rt = ms_pRenderTargetAlias;
	}

	if (!rt)
	{
		Assertf(false, "No appropriated aliased render target for texture with dimensions %d / %d!", pDstTex->GetWidth(), pDstTex->GetHeight());
		return false;
	}

	{
#if __PS3
		grcRenderTargetGCM* gcmTarget = (grcRenderTargetGCM*)rt;
		gcmTarget->OverrideTextureOffsets(pDstTex->GetTexturePtr()->offset, pDstTex->GetWidth(), pDstTex->GetHeight());
#elif __XENON
		grcDeviceSurface* prevRt = NULL;
		s32 prevWidth = GRCDEVICE.GetWidth();
		s32 prevHeight = GRCDEVICE.GetHeight();
		GRCDEVICE.GetRenderTarget(0, &prevRt);

		grcRenderTargetXenon* xenonTarget = (grcRenderTargetXenon*)rt;
		rt->GetTexturePtr()->Format.BaseAddress = pDstTex->GetTexturePtr()->Format.BaseAddress;
		rt->GetTexturePtr()->Format.MipAddress = pDstTex->GetTexturePtr()->Format.MipAddress;
#else
		Assertf(0, "GpuDXTCompressor::CompressDXT1 - Not Implemented");
#endif
		s32 numMips = rage::Min(rt->GetMipMapCount(), pDstTex->GetMipMapCount());

		//for (u32 mipIdx = 0; mipIdx < numMips; ++mipIdx)
		for (s32 mipIdx = numMips - 1; mipIdx >= 0; --mipIdx)
		{
			u32 mipWidth = pDstTex->GetWidth() >> mipIdx;
			if (mipWidth <= 16)
				break; // don't deal with lower mips than this, can't set the required pitch on the RT surface

			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
#if __PS3
			gcmTarget->LockSurface(0, mipIdx);
			u32 newPitch = Max(64, (((pDstTex->GetWidth()>>mipIdx)+3)/4) * 8);
			gcmTarget->OverridePitch(newPitch);

			GRCDEVICE.SetRenderTarget(gcmTarget, NULL);
#elif __XENON
			xenonTarget->SetColorExpBias(-10); // render -10, resolve +10
			xenonTarget->LockSurface(0, mipIdx);

			GRCDEVICE.SetRenderTarget(0, xenonTarget->GetSurface());
			GRCDEVICE.SetSize(xenonTarget->GetWidth()>>mipIdx, xenonTarget->GetHeight()>>mipIdx);
#else
			Assertf(0, "GpuDXTCompressor::CompressDXT1 - Not Implemented");
#endif
			grcViewport vp;
			vp.Screen();
			grcViewport *old = grcViewport::SetCurrent(&vp);

			float floatWidth = static_cast<float>(pSrcRt->GetWidth() >> mipIdx);
			float floatHeight = static_cast<float>(pSrcRt->GetHeight() >> mipIdx);
			Vector4 texelSize(1.f / floatWidth, 1.f / floatHeight, floatWidth, floatHeight);
			ms_pShader->SetVar(ms_texelSizeVarId, texelSize);
			ms_pShader->SetVar(ms_texture3VarId, pSrcRt);
			ms_pShader->SetVar(ms_compressLodVarId, (float)mipIdx);
			if (ms_pShader->BeginDraw(grmShader::RMC_DRAW, true, ms_compressTechniqueId))
			{
				ms_pShader->Bind(0);

				grcDrawSingleQuadf(0, 0, (float)(renderWidth >> mipIdx), (float)(renderHeight >> mipIdx), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

				ms_pShader->UnBind();
				ms_pShader->EndDraw();
			}
			ms_pShader->SetVar(ms_compressLodVarId, 0.f);
			ms_pShader->SetVar(ms_texture3VarId, grcTexture::None);

#if __XENON
			D3DTexture* rtTex = reinterpret_cast<D3DTexture*>(xenonTarget->GetTexturePtr());

			D3DRECT rect;
			rect.x1 = 0; 
			rect.y1 = 0;
			rect.x2 = (u32)(xenonTarget->GetWidth() >> mipIdx);
			rect.y2 = (u32)(xenonTarget->GetHeight() >> mipIdx);

			D3DPOINT* pOffset = NULL;

			u32 flags = D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS;
			flags |= D3DRESOLVE_EXPONENTBIAS(10); // render -10, resolve +10

			GRCDEVICE.GetCurrent()->Resolve(flags,  &rect, rtTex, pOffset, mipIdx, 0, NULL, 0, 0, NULL);
#else
		Assertf(0, "GpuDXTCompressor::CompressDXT1 - Not Implemented");
#endif // __XENON
			grcViewport::SetCurrent(old);
		}

#if __PS3
		grcTextureFactory::GetInstance().BindDefaultRenderTargets();
#elif __XENON
		GRCDEVICE.SetRenderTarget(0, prevRt);
		GRCDEVICE.SetSize(prevWidth, prevHeight);
#else
		Assertf(0, "GpuDXTCompressor::CompressDXT1 - Not Implemented");
#endif
	}

	return true;

}
#else // !RSG_PC
// Code borrowed from MeshBlendManager until we support DXT5 and can create a common interface, etc.
// NOTE: Don't pass in non-square textures into this function unless you know what you're doing.
//
// D3D 10.0 will require compression to span multiple frames since it requires a round-trip from GPU->CPU->GPU. GPU Compression on 10.1 can
// stay entirely on the GPU and will never require waiting for results. If you are on D3D 10.0 you will need to query FinishCompressDxt()
// each frame until it returns TRUE. While a compress job is in flight, no other compression work can be submitted.
//
#define MIP_COMPRESS_CUTOFF_SIZE (16)  // Don't deal with lower mips than this, can't set the required pitch on the RT surface

//////////////////////////////////////////////////////////////////////////
//
bool GpuDXTCompressor::CompressDXT1(grcRenderTarget* srcRt, grcTexture* dstTex)
{
	PF_AUTO_PUSH_TIMEBAR("CompressDXT1");

//	Assert( sysInterlockedRead((unsigned*)&m_compressionStatus) == DXT_COMPRESSOR_AVAILABLE );
	u32 nMipBitMask = 0;

	// TEMP: need to fix dxt5
	if (dstTex->GetBitsPerPixel() != 4)
		return false;

	// In order to copy into the destination texture, we need it to not be immutable
	ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
		static_cast<grcTextureDX11*>(dstTex)->StripImmutability(true);
	ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))

	s32 renderWidth = (s32)dstTex->GetWidth() >> 2;
	s32 renderHeight = (s32)dstTex->GetHeight() >> 2;

	PIXBegin(0,"Compress DXT1");
	{
		grcRenderTargetD3D11* ngTarget = NULL;

//		s32 numMips = dstTex->GetMipMapCount();
		s32 numMips = 1;

		s32 numMipsDone = 0;

		for (s32 mipIdx = numMips - 1; mipIdx >= 0; --mipIdx)
		{
			u32 mipWidth = dstTex->GetWidth() >> mipIdx;
			if (mipWidth <= MIP_COMPRESS_CUTOFF_SIZE)
				continue; 

			numMipsDone++;

			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			u32 mipHeight = dstTex->GetHeight() >> mipIdx;
			ngTarget = (grcRenderTargetD3D11*) FindTempDxtRenderTarget(mipWidth>>2,mipHeight>>2);
			if ( !Verifyf(ngTarget != nullptr, "No appropriated temp DXT render target with dimensions %d x %d!", mipWidth>>2, mipHeight>>2) )
			{
				return false;
			}
			grcTextureFactory::GetInstance().LockRenderTarget(0, ngTarget, NULL, 0, false, 0);
			grcViewport vp;
			vp.Screen();
			grcViewport *old = grcViewport::SetCurrent(&vp);

			float floatWidth = static_cast<float>(srcRt->GetWidth() >> mipIdx);
			float floatHeight = static_cast<float>(srcRt->GetHeight() >> mipIdx);
			Vector4 texelSize(1.f / floatWidth, 1.f / floatHeight, floatWidth, floatHeight);
			ms_pShader->SetVar(ms_texelSizeVarId, texelSize);
			ms_pShader->SetVar(ms_texture3VarId, srcRt);
			ms_pShader->SetVar(ms_compressLodVarId, (float)mipIdx);
			if (ms_pShader->BeginDraw(grmShader::RMC_DRAW, true, ms_compressTechniqueId))
			{
				ms_pShader->Bind(0);

				grcDrawSingleQuadf(0, 0, (float)(renderWidth >> mipIdx), (float)(renderHeight >> mipIdx), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

				ms_pShader->UnBind();
				ms_pShader->EndDraw();
			}
			ms_pShader->SetVar(ms_compressLodVarId, 0.f);
			ms_pShader->SetVar(ms_texture3VarId, grcTexture::None);

			grcViewport::SetCurrent(old);

			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			nMipBitMask |= 0x1<<mipIdx;
		}

		if ( !UseCpuRoundtrip() )
		{
			// D3D 10.1 and higher can use CopyResource to convert RGBA_16_UNORM render target to DXT1 texture
			for (s32 mipIdx = numMips - 1; mipIdx >= 0; --mipIdx)
			{
				u32 mipWidth = dstTex->GetWidth() >> mipIdx;
				u32 mipHeight = dstTex->GetWidth() >> mipIdx;
				if (mipWidth <= MIP_COMPRESS_CUTOFF_SIZE)
					continue; 

				dstTex->Copy(FindTempDxtRenderTarget(mipWidth>>2,mipHeight>>2),0,mipIdx,0,0); 
			}
		}
		else
		{
			// For D3D 10.0 we need to read back the render target and then copy on the CPU. We don't want to stall waiting for the
			// read back, so this will have to occur across several frames
			for (s32 mipIdx = numMips - 1; mipIdx >= 0; --mipIdx)
			{
				u32 mipWidth = dstTex->GetWidth() >> mipIdx;
				u32 mipHeight = dstTex->GetWidth() >> mipIdx;
				if (mipWidth <= MIP_COMPRESS_CUTOFF_SIZE)
					continue; 

				static_cast<grcTextureD3D11*>(FindTempDxtTexture(mipWidth>>2,mipHeight>>2))->CopyFromGPUToStagingBuffer();
			}
			m_pCompressionDest = dstTex;
			m_nCompressionMipStatusBits = nMipBitMask;
		}
	}
	PIXEnd();

	return true;
}

bool GpuDXTCompressor::FinishCompressDxt()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (m_pCompressionDest == nullptr)
		return true;

	PF_AUTO_PUSH_TIMEBAR("PHM FinishCompressDxt");

	const s32 numMips = m_pCompressionDest->GetMipMapCount();
	for (s32 mipIdx = numMips - 1; mipIdx >= 0; --mipIdx)
	{
		// Skip any mips that either weren't compressed or have already finished
		if ( (m_nCompressionMipStatusBits & (0x1<<mipIdx)) == 0x0 )
			continue; 

		u32 mipWidth = m_pCompressionDest->GetWidth() >> mipIdx;
		u32 mipHeight = m_pCompressionDest->GetHeight() >> mipIdx;
		grcTexture* pSrcTex = FindTempDxtTexture(mipWidth>>2,mipHeight>>2);
		if ( !AssertVerify( nullptr != pSrcTex ) )
		{
			// That didn't work... we're going to bail out, this texture will never get compressed
			m_pCompressionDest = NULL;
			return true;
		}

		if ( !static_cast<grcTextureD3D11*>(pSrcTex)->MapStagingBufferToBackingStore(true) )
		{
			// Keep trying other mips
			continue;
		}

		grcTextureLock srcLock;
		if ( !AssertVerify( pSrcTex->LockRect(0, 0, srcLock, grcsRead) )) 
		{
			// That didn't work... we're going to bail out, this texture will never get compressed
			m_pCompressionDest = NULL;
			return true;
		}

		static_cast<grcTextureD3D11*>(m_pCompressionDest)->UpdateGPUCopy(0,mipIdx,srcLock.Base);
		pSrcTex->UnlockRect(srcLock);

		// Mark this mip as done
		m_nCompressionMipStatusBits = m_nCompressionMipStatusBits & ~(0x1<<mipIdx);
	}

	// If all the mips finished, then we're done
	if ( 0x0 == m_nCompressionMipStatusBits )
	{
		m_pCompressionDest = NULL;
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
//
grcRenderTarget* GpuDXTCompressor::FindTempDxtRenderTarget( const int nWidth, const int nHeight )
{
	if (ms_pRTDxtCompS && nWidth == ms_pRTDxtCompS->GetWidth() && nHeight == ms_pRTDxtCompS->GetHeight())
	{
		return ms_pRTDxtCompS;
	}
	else if (ms_pRTDxtCompL && nWidth == ms_pRTDxtCompL->GetWidth() && nHeight == ms_pRTDxtCompL->GetHeight())
	{
		return ms_pRTDxtCompL;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
//
grcTexture* GpuDXTCompressor::FindTempDxtTexture( const int nWidth, const int nHeight )
{
	if (ms_pDxtCompS && nWidth == ms_pDxtCompS->GetWidth() && nHeight == ms_pDxtCompS->GetHeight())
	{
		return ms_pDxtCompS;
	}
	else if (ms_pDxtCompL && nWidth == ms_pDxtCompL->GetWidth() && nHeight == ms_pDxtCompL->GetHeight())
	{
		return ms_pDxtCompL;
	}
	return NULL;
}

#endif // !RSG_PC

//////////////////////////////////////////////////////////////////////////
//
grcRenderTarget* DeferredRendererWrapper::_GetDepthCopy()
{
#if RSG_PC
	return m_pDepthCopy;
#else
	return _GetDepth();
#endif // RSG_PC
}

#if RSG_PC
void DeferredRendererWrapper::CopyTarget(DeferredRendererWrapper* /*pRenderer*/, grcRenderTarget* pDst, grcRenderTarget* pSrc)
{
	if (pSrc == NULL || pDst == NULL)
		return;

	CRenderTargets::CopyTarget(pSrc, pDst);
}
#endif // RSG_PC

void DeferredRendererWrapper::InitialisePedHeadshotRenderer(DeferredRendererWrapper* pRenderer)
{
	grcRenderTarget* pGBufferRTs[4] = {NULL, NULL, NULL, NULL};
	grcRenderTarget* pDepthRT = NULL;
#if __D3D11
	grcRenderTarget* pStencilRT = NULL;
#endif
	grcRenderTarget* pDepthAliasRT = NULL; 
	grcRenderTarget* pLightRT = NULL;
	grcRenderTarget* pCompositeRT = NULL;
	grcRenderTarget* pDepthCopyRT = NULL;
	grcRenderTarget* pLightRTCopy = NULL;
	grcRenderTarget* pCompositeRTCopy = NULL;

#if __PS3
	const int hiResSize = 128;
	const int hiResHeap = 4;
	const int hiResLightHeap = 9;

	// Allocate hi-res targets
	grcTextureFactory::CreateParams paramsHiRes;
	paramsHiRes.Format		= grctfA16B16G16R16F;
	paramsHiRes.UseFloat	= true;
	pLightRT = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2, "Headshot Deferred Light Buffer  HR",	grcrtPermanent, hiResSize, hiResSize, 64, &paramsHiRes, hiResLightHeap); 
	pLightRT->AllocateMemoryFromPool();

	paramsHiRes.Format		= grctfA8R8G8B8;
	paramsHiRes.UseFloat	= false;
	paramsHiRes.IsSRGB		= false;
	pGBufferRTs[0] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "Headshot Deferred Buffer 0 HR",	grcrtPermanent, hiResSize, hiResSize, 32, &paramsHiRes, hiResHeap); 
	pGBufferRTs[0]->AllocateMemoryFromPool();
	pGBufferRTs[1] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "Headshot Deferred Buffer 1 HR",	grcrtPermanent, hiResSize, hiResSize, 32, &paramsHiRes, hiResHeap); 
	pGBufferRTs[1]->AllocateMemoryFromPool();
	pGBufferRTs[2] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "Headshot Deferred Buffer 2 HR",	grcrtPermanent, hiResSize, hiResSize, 32, &paramsHiRes, hiResHeap); 
	pGBufferRTs[2]->AllocateMemoryFromPool();
	pGBufferRTs[3] = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "Headshot Deferred Buffer 3 HR",	grcrtPermanent, hiResSize, hiResSize, 32, &paramsHiRes, hiResHeap); 
	pGBufferRTs[3]->AllocateMemoryFromPool();

	// skip over GBuffer0
	grcRenderTarget* pDummyTarget = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "Headshot Composite Buffer  HR",	grcrtPermanent, hiResSize, hiResSize, 32, &paramsHiRes, hiResHeap+1); 
	pDummyTarget->AllocateMemoryFromPool();

	pCompositeRT = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "Headshot Composite Buffer  HR",	grcrtPermanent, hiResSize, hiResSize, 32, &paramsHiRes, hiResHeap+1); 
	pCompositeRT->AllocateMemoryFromPool();

	paramsHiRes.Format				= grctfA8R8G8B8;
	paramsHiRes.UseHierZ			= true;
	paramsHiRes.EnableCompression	= true;
	paramsHiRes.InTiledMemory		= true;
	paramsHiRes.Multisample			= 0;
	paramsHiRes.CreateAABuffer		= false;

	pDepthRT = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "Headshot Depth Buffer HR",grcrtDepthBuffer, hiResSize, hiResSize, 32, NULL, hiResHeap);
	pDepthRT->AllocateMemoryFromPool();

	paramsHiRes.InTiledMemory		= true;
	pDepthAliasRT = ((grcRenderTargetGCM*)pDepthRT)->CreatePrePatchedTarget(false);


#elif __XENON
	const int hiResSize = 128;
	grcTextureFactory::CreateParams params;

	// Depth buffer
	params.Format		= grctfA8R8G8B8;
	params.IsResolvable = true;
	params.Multisample	= 0;
	params.HasParent	= true;
	params.Parent		= NULL;
	pDepthRT = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Depth Buffer",	grcrtDepthBuffer, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap);
	pDepthRT->AllocateMemoryFromPool();

	params.Format		= grctfA8R8G8B8;
	params.HasParent	= true;
	params.Parent		= NULL;
	pDepthAliasRT = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Depth Buffer Alias", grcrtPermanent, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap2);
	pDepthAliasRT->AllocateMemoryFromPool();

	// Light buffer
	params.Format		= grctfA2B10G10R10;
	params.UseFloat		= true;
	params.IsResolvable = true;
	params.HasParent	= true;
	params.Parent		= pDepthRT;

	pLightRT = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Deferred Light Buffer",	grcrtPermanent, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap); 
	pLightRT->AllocateMemoryFromPool();

	// Composite Buffer 0
	params.Format		= grctfA8R8G8B8;
	params.UseFloat		= false;
	params.IsSRGB		= false;
	params.Parent		= pLightRT;

	pCompositeRT = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Deferred Composite Buffer",	grcrtPermanent, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap);
	pCompositeRT->AllocateMemoryFromPool();

	// GBuffer 0
	params.Parent		= pDepthRT;	
	pGBufferRTs[0] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Deferred Buffer 0",	grcrtPermanent, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap); 
	pGBufferRTs[0]->AllocateMemoryFromPool();
	// GBuffer 1
	params.Parent		= pGBufferRTs[0];
	pGBufferRTs[1] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Deferred Buffer 1",	grcrtPermanent, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap); 
	pGBufferRTs[1]->AllocateMemoryFromPool();
	// GBuffer 2
	params.Parent		= pGBufferRTs[1];
	pGBufferRTs[2] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Deferred Buffer 2",	grcrtPermanent, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap); 
	pGBufferRTs[2]->AllocateMemoryFromPool();
	// GBuffer 3
	params.Parent		= pGBufferRTs[2];
	pGBufferRTs[3] = CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_TEMP_BUFFER, "Headshot Deferred Buffer 3",	grcrtPermanent, hiResSize, hiResSize, 32, &params, kUIPedHeadshotHeap); 
	pGBufferRTs[3]->AllocateMemoryFromPool();

#else

	// NEXTGEN TODO: make sure this is correct for nextgen platforms
	const int hiResSize = 128;
	grcTextureFactory::CreateParams params;

	// Depth buffer
	params.Format		= grctfD32FS8;
	params.IsResolvable = true;
	params.Multisample	= 0;
	params.HasParent	= true;
	params.Parent		= NULL;
	pDepthRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Depth Buffer",	grcrtDepthBuffer, hiResSize, hiResSize, 48, &params);

#if __D3D11
	grcTextureFormat eStencilFormat = grctfX32S8;
	grcTextureFactoryDX11 &textureFactory11 = static_cast<grcTextureFactoryDX11&>( grcTextureFactory::GetInstance() );
	pStencilRT = textureFactory11.CreateRenderTarget("Headshot Stencil Buffer", pDepthRT->GetTexturePtr(), eStencilFormat, DepthStencilRW);
#endif

#if RSG_PC
	// GBuffer 0
	params.Parent		= pDepthRT;	
	params.IsResolvable = false;
	params.HasParent	= true;

	if(GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 
		pDepthCopyRT = textureFactory11.CreateRenderTarget("Headshot Depth Buffer Read Only", pDepthRT->GetTexturePtr(), params.Format, DepthStencilReadOnly);
	} else
		pDepthCopyRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Depth Buffer Copy",	grcrtDepthBuffer, hiResSize, hiResSize, 32, &params);

	params.IsResolvable = true;
#endif // RSG_PC

	params.Format		= grctfA8R8G8B8;
	params.HasParent	= true;
	params.Parent		= NULL;
	pDepthAliasRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Depth Buffer Alias", grcrtPermanent, hiResSize, hiResSize, 32, &params);

	// Light buffer
	params.Format		= grctfA16B16G16R16F;
	params.UseFloat		= true;
	params.IsResolvable = true;
	params.HasParent	= true;
	params.Parent		= pDepthRT;

	pLightRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Deferred Light Buffer",	grcrtPermanent, hiResSize, hiResSize, 64, &params); 

#if RSG_PC
	pLightRTCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Copy of Headshot DefLight Buf",	grcrtPermanent, hiResSize, hiResSize, 32, &params); 
#endif // RSG_PC

	// Composite Buffer 0
	params.Format		= grctfA8R8G8B8;
	params.UseFloat		= false;
	params.IsSRGB		= false;
	params.Parent		= pLightRT;

	pCompositeRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Deferred Composite Buffer",	grcrtPermanent, hiResSize, hiResSize, 32, &params);

#if RSG_PC
	pCompositeRTCopy = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Copy Headshot DefComp Buf",	grcrtPermanent, hiResSize, hiResSize, 32, &params);
#endif // RSG_PC

	// GBuffer 0
	params.Parent		= pDepthRT;	
	pGBufferRTs[0] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Deferred Buffer 0",	grcrtPermanent, hiResSize, hiResSize, 32, &params); 

	// GBuffer 1
	params.Parent		= pGBufferRTs[0];
	pGBufferRTs[1] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Deferred Buffer 1",	grcrtPermanent, hiResSize, hiResSize, 32, &params); 

	// GBuffer 2
	params.Parent		= pGBufferRTs[1];
	pGBufferRTs[2] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Deferred Buffer 2",	grcrtPermanent, hiResSize, hiResSize, 32, &params); 

	// GBuffer 3
	params.Parent		= pGBufferRTs[2];
	pGBufferRTs[3] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Headshot Deferred Buffer 3",	grcrtPermanent, hiResSize, hiResSize, 32, &params); 
#endif

	pRenderer->Init(pGBufferRTs, pDepthRT, pDepthAliasRT, pLightRT, pLightRTCopy, pCompositeRT, pCompositeRTCopy, pDepthCopyRT
#if __D3D11
		,pStencilRT
#endif
		);
}

void DeferredRendererWrapper::Init(grcRenderTarget* pGBufferRTs[4], grcRenderTarget* pDepthRT, grcRenderTarget* pDepthAliasRT, grcRenderTarget* pLightRT, grcRenderTarget* WIN32PC_ONLY(pLightRTCopy), grcRenderTarget* pCompositeRT, grcRenderTarget* WIN32PC_ONLY(pCompositeRTCopy), grcRenderTarget* WIN32PC_ONLY(pDepthCopyRT)
#if __D3D11
								   ,grcRenderTarget* pStencilRT
#endif
								   )
{
	m_pGBufferRTs[0] = pGBufferRTs[0];
	m_pGBufferRTs[1] = pGBufferRTs[1];
	m_pGBufferRTs[2] = pGBufferRTs[2];
	m_pGBufferRTs[3] = pGBufferRTs[3];
	m_pCompositeRT	 = pCompositeRT;

	m_pDepthRT		= pDepthRT;
#if __D3D11
	m_pStencilRT	= pStencilRT;
#endif
	m_pDepthAliasRT = pDepthAliasRT;
	m_pLightRT		= pLightRT;
	m_pLightRTCopy  = NULL;
	m_pCompositeRTCopy = NULL;

#if RSG_PC
	m_pDepthCopy	= pDepthCopyRT;
	m_pLightRTCopy	= pLightRTCopy;
	m_pCompositeRTCopy = pCompositeRTCopy;
#endif // RSG_PC

	grcDepthStencilStateDesc dsd;
	dsd.StencilEnable = TRUE;
	dsd.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	dsd.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	dsd.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	m_DSSWriteStencilHandle = grcStateBlock::CreateDepthStencilState(dsd);

	grcDepthStencilStateDesc dsdTest;
	dsdTest.DepthEnable = TRUE;
	dsdTest.DepthWriteMask = FALSE;
	dsdTest.DepthFunc = grcRSV::CMP_ALWAYS;
	dsdTest.StencilEnable = TRUE;
	dsdTest.StencilReadMask = 0xFF;
	dsdTest.StencilWriteMask = 0x00;
	dsdTest.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	dsdTest.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	m_DSSTestStencilHandle = grcStateBlock::CreateDepthStencilState(dsdTest);


	grcDepthStencilStateDesc insideVolumeDepthStencilState;
	insideVolumeDepthStencilState.DepthWriteMask = FALSE;
	insideVolumeDepthStencilState.DepthEnable = TRUE;
	insideVolumeDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATER);
	insideVolumeDepthStencilState.StencilEnable = TRUE;	
	insideVolumeDepthStencilState.StencilReadMask = 0xFF;
	insideVolumeDepthStencilState.StencilWriteMask = 0x00;
	insideVolumeDepthStencilState.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
	insideVolumeDepthStencilState.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
	insideVolumeDepthStencilState.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	insideVolumeDepthStencilState.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	insideVolumeDepthStencilState.BackFace = insideVolumeDepthStencilState.FrontFace; 
	m_insideLightVolumeDSS = grcStateBlock::CreateDepthStencilState(insideVolumeDepthStencilState);

	grcDepthStencilStateDesc outsideVolumeDepthStencilState;
	outsideVolumeDepthStencilState.DepthWriteMask = FALSE;
	outsideVolumeDepthStencilState.DepthEnable = TRUE;
	outsideVolumeDepthStencilState.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	outsideVolumeDepthStencilState.StencilEnable = insideVolumeDepthStencilState.StencilEnable;
	outsideVolumeDepthStencilState.StencilReadMask = insideVolumeDepthStencilState.StencilReadMask;
	outsideVolumeDepthStencilState.StencilWriteMask = insideVolumeDepthStencilState.StencilWriteMask;
	outsideVolumeDepthStencilState.FrontFace = insideVolumeDepthStencilState.FrontFace;
	outsideVolumeDepthStencilState.BackFace = insideVolumeDepthStencilState.BackFace;
	m_outsideLightVolumeDSS = grcStateBlock::CreateDepthStencilState(outsideVolumeDepthStencilState);

}

void DeferredRendererWrapper::Shutdown()
{

}


void DeferredRendererWrapper::_SetUIProjectionParams(eDeferredShaders currentShader)
{
	const float w = (float)m_pLightRT->GetWidth();
	const float h = (float)m_pLightRT->GetHeight();

	const Vector4 screenSize(w, h, 1.0f/w, 1.0f/h);

	Vec4V projParams;
	Vec3V shearProj0, shearProj1, shearProj2;
	DeferredLighting::GetProjectionShaderParams(projParams, shearProj0, shearProj1, shearProj2);

	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_SCREENSIZE, screenSize, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PROJECTION_PARAMS, projParams, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS0, shearProj0, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS1, shearProj1, MM_SINGLE);
	DeferredLighting::SetDeferredVar(currentShader, DEFERRED_SHADER_PERSPECTIVESHEAR_PARAMS2, shearProj2, MM_SINGLE);
}

void DeferredRendererWrapper::_SetUILightingGlobals()
{
	const float w = (float)m_pLightRT->GetWidth();
	const float h = (float)m_pLightRT->GetHeight();

	Vec4V bakeParams(0.5f, 1.5 - 0.5f, 0.0f, 0.0f );
	m_prevInInterior = CShaderLib::GetGlobalInInterior();

	// Set all lighting values to default
	CShaderLib::SetGlobalDynamicBakeBoostAndWetness(bakeParams);
	CShaderLib::SetGlobalEmissiveScale(1.0f, true);
	CShaderLib::DisableGlobalFogAndDepthFx();
	CShaderLib::SetGlobalNaturalAmbientScale(1.0f);
	CShaderLib::SetGlobalArtificialAmbientScale(0.0f);
	CShaderLib::SetGlobalInInterior(false);
	CShaderLib::SetGlobalScreenSize(Vector2(w, h));

	Lights::m_lightingGroup.SetDirectionalLightingGlobals(1.0f, false);
}

void DeferredRendererWrapper::_SetAmbients()
{
	Vector4 ambientDown = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	Vector4 ambientBase = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	Vector4 ambientValues[7];
	Vector4 zero = Vector4(Vector4::ZeroType);

	// set up ambient lighting
	ambientValues[0] = ambientDown;
	ambientValues[1] = ambientBase;

	// artificial interior ambient
	ambientValues[2] = zero;
	ambientValues[3] = zero;

	// artificial exterior ambient
	ambientValues[4] = zero;
	ambientValues[5] = zero;

	// directional ambient
	ambientValues[6] = zero;

	Lights::m_lightingGroup.SetAmbientLightingGlobals(1.0f, 1.0f, 1.0f, ambientValues);
}

void DeferredRendererWrapper::_RestoreLightingGlobals()
{
	CShaderLib::EnableGlobalFogAndDepthFx();
	CShaderLib::SetGlobalInInterior(m_prevInInterior);
}

void DeferredRendererWrapper::_SetToneMapScalars()
{
	const float packScalar = 4.0f;
	const float toneMapScaler = 1.0f/packScalar;
	const float unToneMapScaler = packScalar;
	Vector4 scalers(toneMapScaler, unToneMapScaler, toneMapScaler, unToneMapScaler);
	CShaderLib::SetGlobalToneMapScalers(scalers);
}


void DeferredRendererWrapper::_Initialize(const grcViewport* pVp, CLightSource* pLightSources, u32 numLights)
{
	PF_PUSH_MARKER("Initalize");

	GRC_ALLOC_SCOPE_SINGLE_THREADED_PUSH(m_AllocScope)

	_LockRenderTargets(true);

	m_pLastViewport = grcViewport::GetCurrent();
	m_viewport = *pVp;
	m_pLightSource = pLightSources;
	m_numLights = numLights;

	grcViewport::SetCurrent(&m_viewport);
#if SUPPORT_INVERTED_PROJECTION
	m_viewport.SetInvertZInProjectionMatrix(true);
#endif

	_UnlockRenderTargets(false, false);

	_SetUILightingGlobals();
	_SetToneMapScalars();

	PF_POP_MARKER();
}

void DeferredRendererWrapper::_Finalize()
{
	PF_PUSH_MARKER("Finalize");

	_RestoreLightingGlobals();

	grcViewport::SetCurrent(m_pLastViewport);
	Lights::ResetLightingGlobals();
	Lights::m_lightingGroup.SetDirectionalLightingGlobals(true);

	GRCDEVICE.SetScissor(0, 0, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight());

	GRC_ALLOC_SCOPE_SINGLE_THREADED_POP(m_AllocScope)

	PF_POP_MARKER();
}


void DeferredRendererWrapper::_DeferredRenderStart()
{
	PF_PUSH_TIMEBAR_DETAIL("Deferred G-Buffer");

	_LockRenderTargets(true);

	grcViewport::GetCurrent()->SetWorldIdentity();
	
	GBuffer::SetRenderingTo(true);
	CRenderPhaseReflection::SetReflectionMapExt(grcTexture::NoneBlack);

	_SetToneMapScalars();

#if __PS3
	CRenderTargets::ResetStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0xff, false, false);
#endif

#if __XENON
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILENABLE, FALSE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, FALSE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILREF, 0 ));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL ));

	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, D3DHIZ_AUTOMATIC));

	CHECK_HRESULT(GRCDEVICE.GetCurrent()->FlushHiZStencil (D3DFHZS_ASYNCHRONOUS));
#endif //__XENON

	float depth = USE_INVERTED_PROJECTION_ONLY ? grcDepthClosest : grcDepthFarthest;
	GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), true, depth, true, DEFERRED_MATERIAL_CLEAR);

	grcStateBlock::SetDepthStencilState(m_DSSWriteStencilHandle, DEFERRED_MATERIAL_PED);

	m_prevForcedTechniqueGroup = grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(grmShaderFx::FindTechniqueGroupId("deferred"));
}

void DeferredRendererWrapper::_DeferredRenderEnd()
{
	grmShaderFx::SetForcedTechniqueGroupId(m_prevForcedTechniqueGroup);

#if __XENON
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HIZWRITEENABLE, FALSE));
#endif
	_UnlockRenderTargets(true, false);
	GBuffer::SetRenderingTo(false);

	PF_POP_TIMEBAR_DETAIL();
}


void DeferredRendererWrapper::_ForwardRenderStart()
{
	PF_PUSH_TIMEBAR_DETAIL("Forward Render");

	grcLightState::SetEnabled(true);
	CRenderPhaseReflection::SetReflectionMapExt(grcTexture::NoneBlack);

	_SetToneMapScalars();

	grcStateBlock::SetDepthStencilState(m_DSSWriteStencilHandle, DEFERRED_MATERIAL_PED);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

	Lights::BeginLightweightTechniques();
}

void DeferredRendererWrapper::_ForwardRenderEnd()
{
	CShaderLib::ResetStippleAlpha();
	CShaderLib::SetGlobalAlpha(1.0f);

	Lights::EndLightweightTechniques();
	PF_POP_TIMEBAR_DETAIL();
}


void DeferredRendererWrapper::_DeferredLight()
{
	PF_PUSH_TIMEBAR_DETAIL("Deferred Lighting");

	const grcViewport* vp = &m_viewport;

	const float nearPlaneBoundIncrease = 
		vp->GetNearClip() * 
		sqrtf(1.0f + vp->GetTanHFOV() * vp->GetTanHFOV() + vp->GetTanVFOV() * vp->GetTanVFOV());
	Vector3 camPos = VEC3V_TO_VECTOR3(vp->GetCameraPosition());

#if __XENON
	GBuffer::RestoreDepthOptimized(_GetDepthAlias(), false);
#endif
#if RSG_PC
	if (!GRCDEVICE.IsReadOnlyDepthAllowed())
	{
		CRenderTargets::CopyDepthBuffer(_GetDepth(), _GetDepthCopy());
	}
#endif // RSG_PC

	grcTextureFactory::GetInstance().LockRenderTarget(0, _GetLightingTarget(), _GetDepthCopy());

	grcViewport::GetCurrent()->SetWorldIdentity();

#if __PS3	// we want to set the scull function and restore the depth compare direction a t the same time to avoid invalidation
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default, DEFERRED_MATERIAL_ALL);
	CRenderTargets::RefreshStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0xFF, false);
#endif

#if __XENON
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, TRUE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILWRITEENABLE, FALSE));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILFUNC, D3DHSCMP_NOTEQUAL));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILREF, 0xFF));
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->FlushHiZStencil(D3DFHZS_ASYNCHRONOUS));
#endif

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, grcDepthFarthest, false, DEFERRED_MATERIAL_CLEAR);

	_SetGbufferTargets();

	_SetUIProjectionParams(DEFERRED_SHADER_DIRECTIONAL);
	_SetToneMapScalars();

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_DSSTestStencilHandle, DEFERRED_MATERIAL_CLEAR);

	// Draw light
	DeferredLighting::RenderDirectionalLight(1, false, MM_SINGLE);

	Lights::BeginLightRender();
	for (u32 i = 0; i < m_numLights; i++)
	{
		CLightSource &lightSource = m_pLightSource[i];

		eDeferredShaders currentShader = DeferredLighting::GetShaderType(lightSource.GetType());
		DeferredLighting::SetLightShaderParams(currentShader, &lightSource, NULL, 1.0f, false, false, MM_SINGLE);
		_SetUIProjectionParams(currentShader);

		grcEffectTechnique technique = DeferredLighting::GetTechnique(lightSource.GetType(), MM_SINGLE);
		const s32 pass = DeferredLighting::CalculatePass(&lightSource, false, false, false);

		bool cameraInside = lightSource.IsPointInLight(camPos, nearPlaneBoundIncrease);	

		grcStateBlock::SetDepthStencilState(cameraInside ? m_insideLightVolumeDSS : m_outsideLightVolumeDSS, DEFERRED_MATERIAL_CLEAR);
		grcStateBlock::SetRasterizerState(Lights::m_Volume_R[Lights::kLightTwoPassStencil][cameraInside ? Lights::kLightCameraInside : Lights::kLightCameraOutside][Lights::kLightNoDepthBias]);
		grcStateBlock::SetBlendState(Lights::m_SceneLights_B);

		const grmShader* shader = DeferredLighting::GetShader(currentShader, MM_SINGLE);
		if (ShaderUtil::StartShader("light", shader, technique, pass, false))
		{
			Lights::RenderVolumeLight(lightSource.GetType(), false, false, false);
			ShaderUtil::EndShader(shader, false);
		}
	}
	Lights::EndLightRender();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if __XENON
	CHECK_HRESULT(GRCDEVICE.GetCurrent()->SetRenderState(D3DRS_HISTENCILENABLE, FALSE));
#endif

	PF_POP_TIMEBAR_DETAIL();
}


void DeferredRendererWrapper::_LockRenderTargets(bool bLockDepth)
{
	grcRenderTarget* depthTarget = (bLockDepth) ? _GetDepth() : NULL;

#if __XENON 
	grcTextureFactory::GetInstance().LockRenderTarget(0, _GetTarget(0), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(1, _GetTarget(1), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(2, _GetTarget(2), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(3, _GetTarget(3), depthTarget);
#endif //__XENON

#if __PPU || __D3D11 || RSG_ORBIS
	const grcRenderTarget* rendertargets[grcmrtColorCount] = {
		_GetTarget(0),
		_GetTarget(1),
		_GetTarget(2),
		_GetTarget(3)
	};

	grcTextureFactory::GetInstance().LockMRT(rendertargets, depthTarget);

#elif __WIN32PC
	grcTextureFactory::GetInstance().LockRenderTarget(0, _GetTarget(0), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(1, _GetTarget(1), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(2, _GetTarget(2), depthTarget);
	grcTextureFactory::GetInstance().LockRenderTarget(3, _GetTarget(3), depthTarget);
#endif //__PPU || __WIN32
}

void DeferredRendererWrapper::_UnlockRenderTargets(bool XENON_ONLY(bResolveCol), bool XENON_ONLY(bResolveDepth))
{
#if __XENON 
	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = bResolveCol;
	grcTextureFactory::GetInstance().UnlockRenderTarget(3, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(2, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(1, &resolveFlags);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
	if (bResolveDepth)
	{
		_GetDepth()->Realize();
	}
#elif __PPU || __D3D11 || RSG_ORBIS
	grcTextureFactory::GetInstance().UnlockMRT();
#else
	grcTextureFactory::GetInstance().UnlockRenderTarget(3);
	grcTextureFactory::GetInstance().UnlockRenderTarget(2);
	grcTextureFactory::GetInstance().UnlockRenderTarget(1);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif
}

void DeferredRendererWrapper::_LockSingleTarget(u32 index, bool bLockDepth)
{
	grcTextureFactory::GetInstance().LockRenderTarget(
		0, 
		_GetTarget(index), 
		bLockDepth ? _GetDepth() : NULL);
}

void DeferredRendererWrapper::_LockLightingTarget(bool bLockDepth)
{
	grcTextureFactory::GetInstance().LockRenderTarget(
		0, 
		_GetLightingTarget(), 
		bLockDepth ? _GetDepthCopy() : NULL);
}

void DeferredRendererWrapper::_UnlockSingleTarget(bool bResolveColor, bool bClearColor, bool bClearDepth)
{
	grcResolveFlags resolveFlags;

#if __XENON || (RSG_PC || RSG_DURANGO || RSG_ORBIS)
	resolveFlags.NeedResolve=bResolveColor;
	resolveFlags.ClearDepthStencil=bClearDepth;
	resolveFlags.ClearColor=bClearColor;
	resolveFlags.Color=Color32(0,0,0,0);
#else
	(void*)bResolveColor;
	(void*)bClearColor;
	(void*)bClearDepth;
#endif // __XENON

	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
}

void DeferredRendererWrapper::_SetGbufferTargets()
{
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_0, _GetTarget(0));
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_1, _GetTarget(1));
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_2, _GetTarget(2));
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_3, _GetTarget(3));

#if __PS3
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, _GetDepthAlias());
#else
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, _GetDepth());
#endif

#if __XENON
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_STENCIL, _GetDepthAlias());
#elif RSG_PC || RSG_DURANGO || RSG_ORBIS
#if __D3D11
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_STENCIL, _GetStencil());
#else
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_STENCIL, _GetDepth());
#endif
#endif
}

void DeferredRendererWrapper::_RenderToneMapPass(grcRenderTarget* pDstRT, grcTexture* pSrcRT, Color32 clearCol, bool bClearDst)
{
	PF_PUSH_MARKER("ToneMap");

	PostFX::SetLDR8bitHDR16bit();

	grcTextureFactory::GetInstance().LockRenderTarget(0, pDstRT, _GetDepth());

	if (bClearDst)
	{
		GRCDEVICE.Clear(true, clearCol, false, grcDepthFarthest, false, DEFERRED_MATERIAL_CLEAR);
	}

	// setup viewport
	PUSH_DEFAULT_SCREEN();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(m_DSSTestStencilHandle, DEFERRED_MATERIAL_CLEAR);

	Vector4 filmic0, filmic1, tonemapParams;
	PostFX::GetDefaultFilmicParams(filmic0, filmic1);

	// set custom exposure
	tonemapParams.z = pow(2.0f, 0.0f);

	// set custom gamma
	tonemapParams.w = 1.0f;

	CSprite2d tonemapSprite;
	tonemapSprite.SetGeneralParams(filmic0, filmic1);
	Vector4 colFilter0, colFilter1;
	colFilter0.Zero();
	colFilter1.Zero();
	tonemapSprite.SetColorFilterParams(colFilter0, colFilter1);
	tonemapSprite.SetRefMipBlurParams(Vector4(0.0f, 0.0f, 1.0f, (1.0f / 2.2f)));
	tonemapSprite.BeginCustomList(CSprite2d::CS_TONEMAP, pSrcRT);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
	tonemapSprite.EndCustomList();

	// reset viewport
	POP_DEFAULT_SCREEN();

	// reset state
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	PF_POP_MARKER();
}
