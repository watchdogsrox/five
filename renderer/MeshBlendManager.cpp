//
// renderer/MeshBlendManager.cpp
//
// Copyright (C) 2011-2014 Rockstar Games.  All Rights Reserved.
//

#include "MeshBlendManager.h"

#if __BANK
#include "bank/slider.h"
#include "grcore/debugdraw.h"
#include "peds/rendering/PedVariationDebug.h"
#endif
#include "grcore/gfxcontext.h"
#include "grcore/quads.h"
#include "grcore/image.h"
#include "grcore/texturegcm.h"
#include "grcore/texturedefault.h"
#include "system/interlocked.h"
#include "system/memory.h"
#include "system/param.h"
#include "system/stack.h"

#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/gameskeleton.h"

#include "peds/ped_channel.h"

#include "shaders/ShaderLib.h"
#include "shaders/CustomShaderEffectPed.h"
#include "streaming/streaming.h"
#include "streaming/streaming_channel.h"
#include "system/FileMgr.h"
#include "system/system.h"

#include "renderer/sprite2d.h"
#include "vector/colors.h"

#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"
#include "grcore/image_dxt.h"

#if __D3D11
#include "grcore/texture_d3d11.h"
#endif

#if __XENON
#include "grcore/texturexenon.h"
#include "system/xtl.h"
#include "system/xgraphics.h"
#endif

RENDER_OPTIMISATIONS()

PARAM(headblenddebug, "Debug for head blend memory overwrites");

#if !RSG_FINAL
PARAM(disableHeadBlendCompress, "Disable head blend manager compression (ignoring the blend results)");
#endif // !RSG_FINAL

namespace rage
{
#if __ASSERT
	extern DECLARE_MTR_THREAD bool g_bIgnoreNullTextureOnBind;
#endif // __ASSERT

	extern __THREAD grcContextHandle *g_grcCurrentContext;
}

#if RSG_PC
static bool UseCpuRoundtrip()
{
	return GRCDEVICE.GetDxFeatureLevel() <= 1000 || GRCDEVICE.GetGPUCount() > 1;
}
#endif //RSG_PC

MeshBlendManager* MeshBlendManager::ms_instance = NULL;
s32 MeshBlendManager::ms_intermediateSlotRefCount = 0;
u32 MeshBlendManager::ms_rampCount[RT_MAX];
Color32 MeshBlendManager::ms_rampColors[RT_MAX][MAX_RAMP_COLORS];

#if !__FINAL
u16 MeshBlendManager::ms_blendSlotBacktraces[MBT_MAX][MAX_NUM_BLENDS];
#endif //!__FINAL

#if __BANK
bkSlider* MeshBlendManager::ms_activeBlendEntrySlider = NULL;
s32 MeshBlendManager::ms_activeBlendEntry = 0;
s32 MeshBlendManager::ms_activeBlendType = 0;
bool MeshBlendManager::ms_showBlendTextures = false;
bool MeshBlendManager::ms_showPalette = false;
bool MeshBlendManager::ms_showNonFinalizedPeds = false;
bool MeshBlendManager::ms_bypassMicroMorphs = false;
bool MeshBlendManager::ms_showDebugText = false;
float MeshBlendManager::ms_unsharpPass = 0.f;
float MeshBlendManager::ms_paletteScale = 1.f;

const char* s_blendTypeNames[MBT_MAX] = { "Ped heads" };
#endif

const char* s_miscTxdName = "strm_peds_mpshare";
strLocalIndex s_miscTxdIndex = strLocalIndex(-1);

const char* s_paletteTexName = "mp_crewpalette";
u32 s_paletteMiscIdx = ~0U;

const char* s_rampTexName[RT_MAX] = {"RT_NONE", "mp_hair_tint", "mp_makeup_tint"};
u32 s_rampMiscIdx[RT_MAX] = {~0U, ~0U, ~0U};

const char* s_eyeColorTxdName = "mp_eye_colour";
strLocalIndex s_eyeColorTxdIndex = strLocalIndex(-1);

float s_targetEyeU0 = 0.78125f;
float s_targetEyeV0 = 0.78711f;
float s_targetEyeU1 = 0.97852f;
float s_targetEyeV1 = 0.98437f;

static sysCriticalSectionToken s_assetToken;

static grmShader* s_blendShader = NULL;
static grcEffectTechnique s_skinToneBlendTechnique;
static grcEffectTechnique s_skinToneBlendRgbTechnique;
static grcEffectTechnique s_skinOverlayBlendTechnique;
static grcEffectTechnique s_skinSpecBlendTechnique;
static grcEffectTechnique s_skinPaletteBlendTechnique;
static grcEffectTechnique s_unsharpTechnique;
static grcEffectTechnique s_normalBlendTechnique;
static grcEffectTechnique s_compressTechnique;
static grcEffectTechnique s_createPalette;
static grcEffectVar s_texture0;
static grcEffectVar s_texture1;
static grcEffectVar s_texture2;
static grcEffectVar s_texture3;
static grcEffectVar s_paletteId;
static grcEffectVar s_paletteId2;
static grcEffectVar s_paletteColor0;
static grcEffectVar s_paletteColor1;
static grcEffectVar s_paletteColor2;
static grcEffectVar s_paletteColor3;
static grcEffectVar s_texelSize;
static grcEffectVar s_compressLod;
static grcEffectVar s_paletteIndex;
static grcEffectVar s_unsharpAmount;

#define MIP_COMPRESS_CUTOFF_SIZE (16)  // Don't deal with lower mips than this, can't set the required pitch on the RT surface

const char* s_microMorphAssets[NUM_MICRO_MORPH_ASSETS] =
{
	// micro morph pairs
	"micro_nose_thin", "micro_nose_wide",
	"micro_nose_up", "micro_nose_down",
	"micro_nose_long", "micro_nose_short",
	"micro_nose_profile_crooked", "micro_nose_profile_curved",
	"micro_nose_tip_up", "micro_nose_tip_down",
	"micro_nose_broke_left", "micro_nose_broke_right",

	"micro_brow_up", "micro_brow_down",
	"micro_brow_in", "micro_brow_out",

	"micro_cheek_up", "micro_cheek_down",
	"micro_cheek_in", "micro_cheek_out",
	"micro_cheek_puffed", "micro_cheek_gaunt",

	"micro_eyes_wide", "micro_eyes_squinted",

	"micro_lips_fat", "micro_lips_thin",

	"micro_jaw_narrow", "micro_jaw_wide",
	"micro_jaw_round", "micro_jaw_square",

	"micro_chin_up", "micro_chin_down",
	"micro_chin_in", "micro_chin_out",
	"micro_chin_pointed", "micro_chin_square",


	// single micro morphs
	"micro_chin_bum",
	"micro_neck_male"
};

MeshBlendManager::MeshBlendManager() : 
m_bCanRegisterSlots(true),
m_renderTarget(NULL),
m_renderTargetSmall(NULL),
#if RSG_ORBIS || RSG_DURANGO
m_renderTargetAliased256(NULL),
m_renderTargetAliased128(NULL),
m_renderTargetAliased64(NULL),
#elif RSG_PC
m_compressionStatus(MeshBlendManager::DXT_COMPRESSOR_AVAILABLE),
m_pCompressionDest(NULL),
m_nCompressionMipStatusBits(0x0),
#endif
m_renderTargetPalette(NULL)
{
	m_blends.Resize(MBT_MAX);
	m_slots.Resize(MBT_MAX);
	m_reqs.Resize(MAX_ACTIVE_REQUESTS);
} 

MeshBlendManager::sBlendEntry::sBlendEntry() :
state(STATE_NOT_USED),
skinCompState(SBC_FEET),
geomBlend(0.f),
texBlend(0.f),
varBlend(0.f),
parentBlend1(0.f),
parentBlend2(0.f),
slotIndex(-1),
deleteFrame(-1),
reblend(false),
overlayCompSpec(false),
srcTexIdx1(-1),
srcTexIdx2(-1),
srcTexIdx3(-1),
srcGeomIdx1(-1),
srcGeomIdx2(-1),
srcGeomIdx3(-1),
srcParentGeomIdx1(-1),
srcParentGeomIdx2(-1),
addedRefs(false),
reblendMesh(false),
srcDrawable1(-1),
srcDrawable2(-1),
rt(NULL),
mipCount(0),
refCount(0),
async(false),
male(false),
finalizeFrame(-1),
eyeColorIndex(-1),
hasReqs(false),
paletteIndex(-1),
usePaletteRgb(false),
hasParents(false),
parentBlendsFrozen(false),
isParent(false),
copyNormAndSpecTex(true),
hairRampIndex(0),
hairRampIndex2(0),
lastRenderFrame(0)
{
	for (s32 i = 0; i < MAX_OVERLAYS; ++i)
	{
		overlayAlpha[i] = 1.f;
		overlayNormAlpha[i] = 1.f;
		overlayTintIndex[i] = 0;
		overlayTintIndex2[i] = 0;
		overlayRampType[i] = RT_NONE;
	}

    for (s32 i = 0; i < MAX_MICRO_MORPHS; ++i)
	{
        microMorphGeom[i] = strLocalIndex(-1);
        microMorphAlpha[i] = 0.f;
	}

	BANK_ONLY(outOfMem = false;)
}

void MeshBlendManager::Init(unsigned initMode) 
{
	if (initMode == INIT_CORE)
	{
		ms_instance = rage_new MeshBlendManager();

		// Temporary hack for url:bugstar:2194886, etc - PC-only blending issue
		#if RSG_PC && !__FINAL
			PARAM_headblenddebug.Set("");
		#endif

		ASSET.PushFolder("common:/shaders");
		s_blendShader = grmShaderFactory::GetInstance().Create();
		s_blendShader->Load("skinblend");
		ASSET.PopFolder();

		s_skinToneBlendTechnique = s_blendShader->LookupTechnique("skinToneBlendPaletteTex");
		s_skinToneBlendRgbTechnique = s_blendShader->LookupTechnique("skinToneBlendPaletteRgb");
		s_skinOverlayBlendTechnique = s_blendShader->LookupTechnique("psOverlay");
		s_skinSpecBlendTechnique = s_blendShader->LookupTechnique("psSpecBlend");
		s_skinPaletteBlendTechnique = s_blendShader->LookupTechnique("psPaletteBlend");
		s_unsharpTechnique = s_blendShader->LookupTechnique("psUnsharp");
		s_normalBlendTechnique = s_blendShader->LookupTechnique("psReorientedNormalBlend");
		s_compressTechnique = s_blendShader->LookupTechnique("psCompressDxt1");
		s_createPalette = s_blendShader->LookupTechnique("psCreatePalette");
		s_texture0 = s_blendShader->LookupVar("DiffuseTex");
		s_texture1 = s_blendShader->LookupVar("LinearTexture");
		s_texture2 = s_blendShader->LookupVar("PaletteTexture");
		s_texture3 = s_blendShader->LookupVar("PointTexture");
		s_paletteId = s_blendShader->LookupVar("PaletteSelector");
		s_paletteId2 = s_blendShader->LookupVar("PaletteSelector2");
		s_paletteColor0 = s_blendShader->LookupVar("PaletteColor0");
		s_paletteColor1 = s_blendShader->LookupVar("PaletteColor1");
		s_paletteColor2 = s_blendShader->LookupVar("PaletteColor2");
		s_paletteColor3 = s_blendShader->LookupVar("PaletteColor3");
		s_texelSize = s_blendShader->LookupVar("TexelSize");
		s_compressLod = s_blendShader->LookupVar("CompressLod");
		s_paletteIndex = s_blendShader->LookupVar("PaletteIndex");
		s_unsharpAmount = s_blendShader->LookupVar("UnsharpAmount");

		GetInstance().UpdateEyeColorData();
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
        if (CFileMgr::IsGameInstalled())
		{
#if !__FINAL
			for(u32 j = 0;j<MBT_MAX;j++)
				for(u32 i = 0;i<MAX_NUM_BLENDS;i++)
				{
					ms_blendSlotBacktraces[j][i] = 0;
				}
#endif //!__FINAL

			s_miscTxdIndex = g_TxdStore.FindSlot(s_miscTxdName);
			if (Verifyf(s_miscTxdIndex != -1, "Mesh blend palette texture '%s' wasn't found!", s_miscTxdName))
			{
				CStreaming::SetDoNotDefrag(s_miscTxdIndex, g_TxdStore.GetStreamingModuleId());

				strIndex streamingIndex = g_TxdStore.GetStreamingIndex(s_miscTxdIndex);
				strStreamingEngine::GetInfo().RequestObject(streamingIndex, STRFLAG_DONTDELETE);
				CStreaming::LoadAllRequestedObjects();

				fwTxd* txd = g_TxdStore.Get(s_miscTxdIndex);
				if (txd)
				{
					for (int i = 0; i < txd->GetCount(); ++i)
					{
						grcTexture* texture = txd->GetEntry(i);
						if (!texture)
							continue;

						u32 rampType = RT_MAX;
						if (!stricmp(s_paletteTexName, texture->GetName()))
						{
							s_paletteMiscIdx = (u32)i;
						}
						else
						{
							for (u32 f = 0; f < RT_MAX; ++f)
							{
								if (!stricmp(s_rampTexName[f], texture->GetName()))
								{
									rampType = f;
									break;
								}
							}
						}

						if (rampType < RT_MAX)
						{
							s_rampMiscIdx[rampType] = (u32)i;
							ms_rampCount[rampType] = texture->GetHeight(); // uncompressed texture, one entry per pixel row;

							Assertf(ms_rampCount[rampType] <= MAX_RAMP_COLORS, "MP hair ramp texture %s is too large. Has %d row, only %d supported", s_rampTexName[rampType], texture->GetHeight(), MAX_RAMP_COLORS);
							s32 height = rage::Min((s32)ms_rampCount[rampType], MAX_RAMP_COLORS);
							ms_rampCount[rampType] = (u32)height;

							grcTextureLock lock;
							if (AssertVerify(texture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock)))
							{
								u8* texData = NULL;
#if RSG_ORBIS
								const sce::Gnm::Texture* srcGnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(texture->GetTexturePtr());
								uint64_t offset;
								uint64_t size;
								sce::GpuAddress::computeTextureSurfaceOffsetAndSize(&offset, &size, srcGnmTexture, 0, 0);
								sce::GpuAddress::TilingParameters tp;
								tp.initFromTexture(srcGnmTexture, 0, 0);

								texData = (u8*)alloca(size*4);
								sce::GpuAddress::detileSurface(texData, ((char*)srcGnmTexture->getBaseAddress() + offset), &tp);
#elif RSG_DURANGO
								grcTextureDurango* pTexture = (grcTextureD3D11*)texture;
								u32 pitch = pTexture->GetWidth() * pTexture->GetBitsPerPixel() / 8;
								u32 buffSize = pitch * pTexture->GetHeight();
								texData = (u8*)alloca(buffSize);
								pTexture->GetLinearTexels(texData, buffSize, 0);
#else
								texData = (u8*)lock.Base;
#endif
								s32 x = (s32)((texture->GetWidth()>>1) + (texture->GetWidth()>>2));
								for (s32 i = 0; i < height; ++i)
								{
									u32 col = *(((u32*)(((u8*)texData) + i * lock.Pitch)) + x);
									ms_rampColors[rampType][i].Set(col);
								}
								texture->UnlockRect(lock);
							}
						}
					}
				}
			}
		}
	}
}

void MeshBlendManager::Shutdown(unsigned shutdownMode) 
{
	// shutdown core is not really necessary on consoles and it's just additional code
	// we might need this for PC though

	if (shutdownMode != SHUTDOWN_SESSION)
	{
		GetInstance().ReleaseRenderTarget();
	}

	if (shutdownMode == SHUTDOWN_CORE)
	{
		delete ms_instance;
		ms_instance = NULL;

		s_blendShader = NULL;
	}
	else if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		if (s_miscTxdIndex != -1)
		{
			strIndex streamingIndex = g_TxdStore.GetStreamingIndex(s_miscTxdIndex);
			strStreamingEngine::GetInfo().ClearRequiredFlag(streamingIndex, STRFLAG_DONTDELETE);
			strStreamingEngine::GetInfo().RemoveObject(streamingIndex);

			s_paletteMiscIdx = ~0U;
			s_rampMiscIdx[RT_HAIR] = ~0U;
			s_rampMiscIdx[RT_MAKEUP] = ~0U;
		}
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		GetInstance().FlushDelete();
	}
}

bool MeshBlendManager::IsLastRenderDone(const sBlendEntry& blend)
{
	u32 curFrame = grcDevice::GetFrameCounter();
	bool result = (curFrame - blend.lastRenderFrame) >= (MAX_FRAMES_RENDER_THREAD_AHEAD_OF_GPU + 1);
	return result;
}

void MeshBlendManager::FlushDelete() 
{
	gRenderThreadInterface.Flush();
	
	// + 1 here, because Update() checks the value before decrementing so ReleaseBlend is called on the DELAYED_DELETE_FRAMES + 1 frame.
	for(int i = 0; i < DELAYED_DELETE_FRAMES + 1; i++)
	{
		Update();
	}

#if __ASSERT
	for (s32 i = 0; i < MBT_MAX; ++i)
	{
		for (s32 f = 0; f < m_blends[i].GetCount(); ++f)
		{
			sBlendEntry& next = m_blends[i][f];
			Assert(sysInterlockedRead((unsigned*)&next.state) == STATE_NOT_USED);
		}
	}
#endif
}

void MeshBlendManager::Update()
{
#if __BANK
	if (ms_showDebugText)
		grcDebugDraw::AddDebugOutputEx(false, Color_white, "Slot   Source 1            Source 2            Geom   Tex    Var");

	u32 totalRam = 0;
	u32 totalVram = 0;
	atArray<s32> dwdAssets;
	atArray<s32> txdAssets;
#endif

#if RSG_PC
	GRCDEVICE.SetHeadBlendingOverride(IsAnyBlendActive());
#endif

	for (s32 i = 0; i < MBT_MAX; ++i)
	{
#if __BANK
		if (ms_showDebugText)
			grcDebugDraw::AddDebugOutputEx(false, Color_white, "%s - %d slots registered", s_blendTypeNames[i], GetInstance().m_slots[i].GetCount());
#endif

		for (s32 f = 0; f < GetInstance().m_blends[i].GetCount(); ++f)
		{
			sBlendEntry& next = GetInstance().m_blends[i][f];
			MeshBlendHandle hnd = i << 16 | f;
			// if this entry is flagged for delayed deletion handle it now
			if (next.deleteFrame > 0)
			{
				next.deleteFrame--;
				continue;
			}
			else if (next.deleteFrame == 0)
			{
				if (PARAM_headblenddebug.Get())
					pedWarningf("[HEAD BLEND] releasing blend %d\n", f);

				if(IsLastRenderDone(next))
					GetInstance().ReleaseBlend(hnd, false);
				continue;
			}

			if (sysInterlockedRead((unsigned*)&next.state) == STATE_NOT_USED)
			{
				continue;
			}

#if __BANK
			DebugText(i, f, totalRam, totalVram, dwdAssets, txdAssets);
#endif

			Assertf(next.slotIndex != -1, "Slot index for current blend is -1!");
			Assertf(GetInstance().m_slots[next.type][next.slotIndex].inUse, "Slot for current blend is flagged as not used. This is NOT good!");
			Assertf(next.slotIndex < GetInstance().m_slots[next.type].GetCount() - 1, "Bad slot index found");

			sBlendSlot& slot = GetInstance().m_slots[next.type][next.slotIndex];

			sysCriticalSection cs(s_assetToken);

			if (next.finalizeFrame > 0)
			{
				// we wait with finalizing until all current processing is done for this blend
				if (GetInstance().HasBlendFinished(hnd))
					next.finalizeFrame--;
				else
					next.finalizeFrame = DELAYED_FINALIZE_FRAMES;
			}
			else if (next.finalizeFrame == 0)
			{
				if (PARAM_headblenddebug.Get())
					pedWarningf("[HEAD BLEND]: finalizing blend %d", f);

				if(IsLastRenderDone(next))
					GetInstance().FinalizeHeadBlend(hnd, false);
			}

			// overlay requests are handled outside of any state on the update thread since they can come in at any time
			for (s32 i = 0; i < MAX_OVERLAYS; ++i)
			{
				s32 overlayReq = slot.overlayReq[i];
				if (overlayReq != -1/* && GetInstance().m_reqs[overlayReq].IsValid()*/)
				{
					if (GetInstance().m_reqs[overlayReq].HasLoaded())
					{
						Assertf((slot.addedOverlayRefs & (1 << i)) == 0, "Leaking overlay texture refs, most likely");
						g_TxdStore.AddRef(strLocalIndex(slot.overlayIndex[i]), REF_OTHER);
						g_TxdStore.ClearRequiredFlag(slot.overlayIndex[i], STRFLAG_DONTDELETE);
						slot.addedOverlayRefs |= (1 << i);
						if (Verifyf(g_TxdStore.Get(strLocalIndex(slot.overlayIndex[i]))->GetCount() > 0, "Meshblendmanager: Selected overlay txd has no textures! (%s)", g_TxdStore.GetName(strLocalIndex(GetInstance().m_slots[next.type][next.slotIndex].overlayIndex[i]))))
						{
							if (i < MAX_OVERLAYS - NUM_BODY_OVERLAYS)
							{
								if (PARAM_headblenddebug.Get())
									pedWarningf("[HEAD BLEND] Overlays in for blend %d, reblend=true", f);
								next.reblendOverlayMaps = true;
								next.reblend = true;
							}
							else
							{
								next.reblendSkin[SBC_UPPR] = true;
							}
						}

						GetInstance().m_reqs[overlayReq].Release();
						slot.overlayReq[i] = -1;
					}
				}
			}

			if (sysInterlockedRead((unsigned*)&next.state) == STATE_REQUEST_SLOT)
			{
				if (PARAM_headblenddebug.Get())
					pedWarningf("[HEAD BLEND] %d in state request_slot (%d, %d)", f, slot.dwdReq, slot.txdReq);

				// wait for our slot assets to stream in, this state is used for the main texture and geometry used for this slot
				// which is only requested when a new slot is allocated.
				s32 dwdRequest = slot.dwdReq;
				s32 txdRequest = slot.txdReq;

				if (dwdRequest != -1 && txdRequest != -1)
				{
					if (!GetInstance().m_reqs[dwdRequest].HasLoaded() || !GetInstance().m_reqs[txdRequest].HasLoaded() || !GetInstance().m_reqs[MAX_ACTIVE_REQUESTS - 1].HasLoaded())
						continue;

					g_DwdStore.AddRef(slot.dwdIndex, REF_OTHER);
					g_DwdStore.ClearRequiredFlag(slot.dwdIndex.Get(), STRFLAG_DONTDELETE);
					g_TxdStore.AddRef(slot.txdIndex, REF_OTHER);
					g_TxdStore.ClearRequiredFlag(slot.txdIndex.Get(), STRFLAG_DONTDELETE);

					// add ref to last slot used for intermediate storage
					g_DwdStore.AddRef(GetInstance().m_slots[next.type][GetInstance().m_slots[next.type].GetCount() - 1].dwdIndex, REF_OTHER);

					next.addedRefs = true;
					next.reblendMesh = true;
					next.reblendSkin[SBC_FEET] = true;
					next.reblendSkin[SBC_UPPR] = true;
					next.reblendSkin[SBC_LOWR] = true;

					// do geometry blending since we now have the destination assets loaded
					GetInstance().SetNewBlendValues(hnd, next.geomBlend, next.texBlend, next.varBlend);

					slot.headNorm = NULL;
					slot.headSpec = NULL;

					GetInstance().m_reqs[dwdRequest].Release();
					GetInstance().m_reqs[txdRequest].Release();
					slot.dwdReq = -1;
					slot.txdReq = -1;

					if (--ms_intermediateSlotRefCount == 0)
					{
						GetInstance().m_reqs[MAX_ACTIVE_REQUESTS - 1].Release();
						s32 lastSlotIdx = GetInstance().m_slots[next.type][GetInstance().m_slots[next.type].GetCount() - 1].dwdIndex.Get();
						if (lastSlotIdx != -1)
							g_DwdStore.ClearRequiredFlag(lastSlotIdx, STRFLAG_DONTDELETE);
					}

					Assert(ms_intermediateSlotRefCount > -1);
				}

				bool cont = false;
				for (s32 i = 0; i < SBC_MAX; ++i)
				{
					s32 skinCompReq = slot.skinCompReq[i];
					if (skinCompReq != -1)
					{
						if (!GetInstance().m_reqs[skinCompReq].HasLoaded())
						{
							cont = true;
							break;
						}

						Assertf((slot.addedSkinCompRefs & (1 << i)) == 0, "Leaking skin component texture refs, most likely");
						strLocalIndex skinCompIndex = slot.skinCompIndex[i];
						g_TxdStore.AddRef(skinCompIndex, REF_OTHER);
						g_TxdStore.ClearRequiredFlag(skinCompIndex.Get(), STRFLAG_DONTDELETE);
						slot.addedSkinCompRefs |= (1 << i);

						GetInstance().m_reqs[skinCompReq].Release();
						slot.skinCompReq[i] = -1;
					}
				}

				if (cont)
					continue;

				sysInterlockedExchange((unsigned*)&next.state, STATE_INIT_RT);
			}
		}
	}

#if __BANK
	if (ms_showDebugText)
		grcDebugDraw::AddDebugOutputEx(false, Color_white, "Total asset memory used: (m%dk v%dk)", totalRam>>10, totalVram>>10);
#endif
}

void MeshBlendManager::UpdateAtEndOfFrame()
{
	if (!&GetInstance())
		return;

	for (s32 i = 0; i < MBT_MAX; ++i)
	{
		for (s32 f = 0; f < GetInstance().m_blends[i].GetCount(); ++f)
		{
			sBlendEntry& next = GetInstance().m_blends[i][f];
			MeshBlendHandle hnd = i << 16 | f;

			// don't process entry if it's flagged for deletion
			if (next.deleteFrame > -1)
				continue;

			if (sysInterlockedRead((unsigned*)&next.state) == STATE_NOT_USED)
			{
				continue;
			}

			// don't process if it's finalized (source assets are gone)
			if (next.finalizeFrame == IS_FINALIZED)
				continue;

			if (sysInterlockedRead((unsigned*)&next.state) == STATE_REQUEST_SLOT)
				continue;

			sysCriticalSection cs(s_assetToken);

			// do geometry blend if needed
			if (next.reblendMesh && next.addedRefs)
			{
				ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
				GetInstance().DoGeometryBlend(hnd);
				ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
			}
		}
	}
}

#if RSG_PC
bool MeshBlendManager::IsAnyBlendActive()
{
	for (s32 i = 0; i < MBT_MAX; ++i)
	{
		for (s32 f = 0; f < GetInstance().m_blends[i].GetCount(); ++f)
		{
			MeshBlendHandle hnd = i << 16 | f;
			if (!GetInstance().HasBlendFinished(hnd))
			{
				return true;
			}
		}
	}
	return false;
}
#endif

// spread the processing of blends across several frames by doing at most one state per frame
#define FRAME_SPREAD 0
#if FRAME_SPREAD
#define FRAME_SPREAD_ONLY(x) x
#else
#define FRAME_SPREAD_ONLY(x)
#endif

#define ENABLE_SIMULATE_SLOW_BLEND 0
#if ENABLE_SIMULATE_SLOW_BLEND
static u32 s_numFrameDelay = 20; // simulate slow blend time by spending this amount of frames in each state
static u32 s_curFrameDelay = s_numFrameDelay;

#define SIMULATE_SLOW_BLEND()               \
    if (--s_curFrameDelay == 0)             \
    {                                       \
        s_curFrameDelay = s_numFrameDelay;  \
    }                                       \
    else                                    \
        continue;
#else
#define SIMULATE_SLOW_BLEND()
#endif

void MeshBlendManager::RefreshPalette()
{
#if RSG_PC
	// If any compression work was kicked off in a previous frame then we can't proceed until the compression is done
	if ( !GetInstance().FinishCompressDxt() )
	{
		return; // Not done yet, try again next frame
	}
#endif

	if (GetInstance().m_palette.NeedsRefresh() && GetInstance().m_renderTargetPalette)
	{
		if (Verifyf(s_miscTxdIndex != -1, "No mesh blend palette txd was found! Is '%s' missing?", s_miscTxdName))
		{
			grcTexture* paletteTex = GetMiscTxdTexture(s_paletteMiscIdx);
			if (Verifyf(paletteTex, "No palette texture in mesh blend misc txd '%s'!", s_miscTxdName))
			{
				grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
				grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

				grcTextureFactory::GetInstance().LockRenderTarget(0, GetInstance().m_renderTargetPalette, NULL);
				grcViewport vp;
				vp.Screen();
				grcViewport *old = grcViewport::SetCurrent(&vp);
				GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, 0, 0);

				for (s32 i = 0; i < MAX_PALETTE_ENTRIES; ++i)
				{
					//if (!GetInstance().m_palette.IsUsed(i))
					//    continue;

					s_blendShader->SetVar(s_paletteIndex, (float)i);

					Color32 col0 = GetInstance().m_palette.GetColor((s8)i, 0);
					Color32 col1 = GetInstance().m_palette.GetColor((s8)i, 1);
					Color32 col2 = GetInstance().m_palette.GetColor((s8)i, 2);
					Color32 col3 = GetInstance().m_palette.GetColor((s8)i, 3);

					s_blendShader->SetVar(s_paletteColor0, col0);
					s_blendShader->SetVar(s_paletteColor1, col1);
					s_blendShader->SetVar(s_paletteColor2, col2);
					s_blendShader->SetVar(s_paletteColor3, col3);

					if (s_blendShader->BeginDraw(grmShader::RMC_DRAW, true, s_createPalette))
					{
						s_blendShader->Bind(0);

						grcDrawSingleQuadf(0, 0, (float)GetInstance().m_renderTargetPalette->GetWidth(), (float)GetInstance().m_renderTargetPalette->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

						s_blendShader->UnBind();
						s_blendShader->EndDraw();
					}
				}

				s_blendShader->SetVar(s_paletteIndex, 0.f);

				Vector4 palColReset(0.f, 0.f, 0.f, 0.f);
				s_blendShader->SetVar(s_paletteColor0, palColReset);
				s_blendShader->SetVar(s_paletteColor1, palColReset);
				s_blendShader->SetVar(s_paletteColor2, palColReset);
				s_blendShader->SetVar(s_paletteColor3, palColReset);

				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				grcViewport::SetCurrent(old);

#if RSG_DURANGO
				g_grcCurrentContext->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_PS_PARTIAL_FLUSH);
#endif

				GetInstance().CompressDxt(GetInstance().m_renderTargetPalette, paletteTex, false);
				GetInstance().m_palette.Refreshed();
			}
		}
	}
}

grcTexture* MeshBlendManager::GetMiscTxdTexture(u32 idx)
{
	if (s_miscTxdIndex == -1 || idx == ~0U)
		return NULL;

	fwTxd* txd = g_TxdStore.Get(s_miscTxdIndex);
	if (!txd || txd->GetCount() <= idx)
		return NULL;

	return txd->GetEntry(idx);
}

void MeshBlendManager::RenderThreadUpdate()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
    if (!&GetInstance())
        return;

	PIXBegin(0,"Head diffuse blending");
	CShaderLib::SetGlobalEmissiveScale(1.0f,true);
	RefreshPalette();

#if RSG_PC
	// If any compression work was kicked off in a previous frame then we can't proceed until the compression is done
	if ( !GetInstance().FinishCompressDxt() )
	{
		return; // Not done yet, try again next frame
	}
#endif

	for (s32 i = 0; i < MBT_MAX; ++i)
	{
		for (s32 f = 0; f < GetInstance().m_blends[i].GetCount(); ++f)
		{
			sBlendEntry& next = GetInstance().m_blends[i][f];
			MeshBlendHandle hnd = i << 16 | f;

			Assertf(next.state == STATE_NOT_USED || next.slotIndex != -1, "Slot index for current blend is -1!");
			Assertf(next.state == STATE_NOT_USED || GetInstance().m_slots[next.type][next.slotIndex].inUse, "Slot for current blend is flagged as not used. This is NOT good!");
			Assertf(next.state == STATE_NOT_USED || next.slotIndex < GetInstance().m_slots[next.type].GetCount() - 1, "Bad slot index found");

			// don't process entry if it's flagged for deletion
			if (next.deleteFrame > -1)
				continue;

			if (sysInterlockedRead((unsigned*)&next.state) == STATE_NOT_USED)
			{
				continue;
			}

			PF_PUSH_TIMEBAR("s_assetToken - Critical Section");
			sysCriticalSection cs(s_assetToken);
			PF_POP_TIMEBAR();

			sBlendSlot& slot = GetInstance().m_slots[next.type][next.slotIndex];

#if __BANK
			DebugRender(i, f);
#endif

			// don't process if it's finalized (source assets are gone)
			if (next.finalizeFrame == IS_FINALIZED)
				continue;

			if (sysInterlockedRead((unsigned*)&next.state) == STATE_REQUEST_SLOT)
				continue;

			if (next.hasReqs)
				continue;

            // extract specular and normal textures from the target geometry if we don't already have them
            if (next.addedRefs && (!slot.headSpec || !slot.headNorm || next.copyNormAndSpecTex))
			{
				PF_AUTO_PUSH_TIMEBAR("ExtractGeometryTextures");
				ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
                GetInstance().ExtractGeometryTextures(hnd);
				ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
			}

			if (sysInterlockedRead((unsigned*)&next.state) == STATE_INIT_RT)
			{
				if (PARAM_headblenddebug.Get())
					pedWarningf("[HEAD BLEND] %d in state init_rt", f);

				// set up blend target
				grcTexture* dstTex = g_TxdStore.Get(slot.txdIndex)->GetEntry(0);
				if (Verifyf(dstTex, "Target texture for slot %d is not in memory!", next.slotIndex))
				{
					if (Verifyf(g_TxdStore.Get(slot.skinCompIndex[SBC_FEET]), "No slot(%d) texture for feet found!", next.slotIndex))
					{
						grcTexture* dstFeetTex = g_TxdStore.Get(slot.skinCompIndex[SBC_FEET])->GetEntry(0);
						if (Verifyf(dstFeetTex, "Target texture for feet slot %d is not in memory!", next.slotIndex))
						{
							PF_AUTO_PUSH_TIMEBAR("AllocateRenderTarget");
							ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
							if (!GetInstance().AllocateRenderTarget(dstTex, dstFeetTex OUTPUT_ONLY(, next.slotIndex)))
							{
								Warningf("MeshBlendManager::RenderThreadUpdate - Failed to create render target!");
								ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
								continue;
							}
							ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
						}
					}

					next.reblend = true;
					next.lastRenderFrame = grcDevice::GetFrameCounter();
					sysInterlockedExchange((unsigned*)&next.state, STATE_RENDERING);
				}
			}

			////////////////////////////////////////////////////////////////////////////
			// Head diffuse and overlay rendering
			////////////////////////////////////////////////////////////////////////////
			FRAME_SPREAD_ONLY(else) if (sysInterlockedRead((unsigned*)&next.state) == STATE_RENDERING)
			{
				if (PARAM_headblenddebug.Get())
					pedWarningf("[HEAD BLEND] %d in state rendering (reblend=%s)", f, next.reblend ? "true" : "false");

                SIMULATE_SLOW_BLEND();

				bool failed = true;
				next.skinCompState = SBC_FEET;
				if (!next.reblend)
				{
					PF_AUTO_PUSH_TIMEBAR("sysInterlockedExchange");
					// jump to either skin blend or overlay blend if those flags are set
					if (next.reblendSkin[SBC_FEET] || next.reblendSkin[SBC_UPPR] || next.reblendSkin[SBC_LOWR])
					{
						if (PARAM_headblenddebug.Get())
							pedWarningf("[HEAD BLEND]: %d in state rendering, reblend exit to render_body_skin", f);
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_BODY_SKIN);
                        f--; // redo this blend this frame
					}
					else if (next.reblendOverlayMaps)
					{
						if (PARAM_headblenddebug.Get())
							pedWarningf("[HEAD BLEND] %d in state rendering, reblend exit to render_overlay", f);
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_OVERLAY);
                        f--; // redo this blend this frame
					}
					continue;
				}

				next.reblend = false;

				next.rt = NULL;
				next.mipCount = 0;

				if (Verifyf(g_TxdStore.Get(slot.txdIndex), "Target texture for slot %d is not in memory!", next.slotIndex))
				{
					grcTexture* dstTex = g_TxdStore.Get(slot.txdIndex)->GetEntry(0);
					if (Verifyf(dstTex, "No destination memory available for head texture. This is not good!"))
					{
						s32 width = dstTex->GetWidth();
						s32 height = dstTex->GetHeight();

						if (width == GetInstance().m_renderTarget->GetWidth() && height == GetInstance().m_renderTarget->GetHeight())
						{
							next.rt = GetInstance().m_renderTarget;
#if __D3D11 || RSG_ORBIS
							next.rtTemp = GetInstance().m_renderTargetTemp;
#endif 
							next.mipCount = (s8)dstTex->GetMipMapCount();
							failed = false;
						}
					}
				}

				if (failed)
				{
					Assertf(false, "Ped head blend destination texture has changed, size doesn't match");
					continue;
				}
				Assert(next.rt);

				// do texture blend
				// FA: if this triggers it might be that script has finalized a blend and then changed overlay or blend data.
				// it won't work because we'll need the assets in memory, so they'll need to re-set the blend data to trigger the streaming again
				if (!Verifyf(next.srcTexIdx1 != -1 && g_TxdStore.Get(next.srcTexIdx1), "Source texture 1 for head blend handle 0x%08x is not loaded!", (i << 16) | f) ||
					!Verifyf(next.srcTexIdx2 != -1 && g_TxdStore.Get(next.srcTexIdx2), "Source texture 2 for head blend handle 0x%08x is not loaded!", (i << 16) | f) || 
					!Verifyf(next.srcTexIdx3 != -1 && g_TxdStore.Get(next.srcTexIdx3), "Source texture 3 for head blend handle 0x%08x is not loaded!", (i << 16) | f))
				{
					if (PARAM_headblenddebug.Get())
						pedWarningf("[HEAD BLEND] %d in state rendering, bailing due to missing texture!", f);
					continue;
				}

				const grcTexture* srcTex1 = g_TxdStore.Get(next.srcTexIdx1)->GetEntry(0);
				const grcTexture* srcTex2 = g_TxdStore.Get(next.srcTexIdx2)->GetEntry(0);
				const grcTexture* srcTex3 = g_TxdStore.Get(next.srcTexIdx3)->GetEntry(0);

				if (srcTex1 && srcTex2)
				{
					grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
					grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

					// Composite the render target based on master blend
					next.lastRenderFrame = grcDevice::GetFrameCounter();
					grcTextureFactory::GetInstance().LockRenderTarget(0, next.rt, NULL);
					grcViewport vp;
					vp.Screen();
					grcViewport *old = grcViewport::SetCurrent(&vp);
					GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, 0, 0);

					if (next.texBlend != 1.f)
					{
						grcBindTexture(srcTex1);
						grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));
					}
					if (next.texBlend)
					{
						grcBindTexture(srcTex2);
						grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(next.texBlend * 255)));
					}

					// blend third texture if it exists
					if (srcTex3 && next.varBlend)
					{
						grcBindTexture(srcTex3);
						grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(next.varBlend * 255)));
					}

					// blend eye color, if we want a custom one
                    if (s_eyeColorTxdIndex != -1 && next.eyeColorIndex != -1)
					{
                        if (next.eyeColorIndex < (GetInstance().m_eyeColorRows * GetInstance().m_eyeColorCols))
						{
							const u32 x = next.eyeColorIndex % GetInstance().m_eyeColorCols;
							const u32 y = next.eyeColorIndex / GetInstance().m_eyeColorCols;

							const float u = x * GetInstance().m_uStepEyeTex;
							const float v = y * GetInstance().m_vStepEyeTex;

							const grcTexture* eyeTex = g_TxdStore.Get(s_eyeColorTxdIndex)->GetEntry(0);
							if (eyeTex)
							{
								float rtx = next.rt->GetWidth() * s_targetEyeU0;
								float rty = next.rt->GetHeight() * s_targetEyeV0;
								float rtw = next.rt->GetWidth() * s_targetEyeU1;
								float rth = next.rt->GetHeight() * s_targetEyeV1;
								grcBindTexture(eyeTex);
								grcDrawSingleQuadf(rtx, rty, rtw, rth, 0, u, v, u + GetInstance().m_uStepEyeTex, v + GetInstance().m_vStepEyeTex, Color32(255, 255, 255, 255));
							}
						}
					}

					// blend overlays if we have any
					for (s32 g = 0; g < MAX_OVERLAYS - NUM_BODY_OVERLAYS; ++g)
					{
						RenderOverlay(hnd, next, slot, g);
					}

#if __BANK // just a test
                    if (ms_unsharpPass > 0.01f)
					{
						next.rtTemp->Copy(next.rt);
						s_blendShader->SetVar(s_texture0, next.rtTemp);

						Vector4 texelSize(1.f / next.rt->GetWidth(), 1.f / next.rt->GetHeight(), (float)next.rt->GetWidth(), (float)next.rt->GetHeight());
						s_blendShader->SetVar(s_texelSize, texelSize);
                        s_blendShader->SetVar(s_unsharpAmount, ms_unsharpPass);

						if (s_blendShader->BeginDraw(grmShader::RMC_DRAW, true, s_unsharpTechnique))
						{
							s_blendShader->Bind(0);

							grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

							s_blendShader->UnBind();
							s_blendShader->EndDraw();
						}

						s_blendShader->SetVar(s_texture0, grcTexture::None);
					}
#endif

					grcTextureFactory::GetInstance().UnlockRenderTarget(0);
					grcViewport::SetCurrent(old);

#if RSG_DURANGO
					g_grcCurrentContext->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_PS_PARTIAL_FLUSH);
#endif

					BANK_ONLY(next.startTime = sysTimer::GetSystemMsTime();)
					Assertf(g_TxdStore.Get(slot.txdIndex), "Target texture for slot %d is not in memory!", next.slotIndex);
					grcTexture* dstTex = g_TxdStore.Get(slot.txdIndex)->GetEntry(0);
					if (Verifyf(dstTex, "No destination memory available for head texture. This is not good!"))
					{
						failed = !GetInstance().CompressDxt(next.rt, dstTex, true);
					}
					BANK_ONLY(next.totalTime = sysTimer::GetSystemMsTime() - next.startTime;)

					if (failed)
					{
						// if we failed to compress we stay in the same state to try again
						if (PARAM_headblenddebug.Get())
							pedWarningf("[HEAD BLEND] %d in state rendering, failed to compress to Dxt!", f);
						continue;
					}
					else
					{
						if (PARAM_headblenddebug.Get())
							pedWarningf("[HEAD BLEND] %d in state rendering, exit to render_body_skin", f);
						PF_AUTO_PUSH_TIMEBAR("sysInterlockedExchange");
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_BODY_SKIN);

						// Exit if we're waiting on compression, we can't proceed until it's done
						WIN32PC_ONLY( if ( !GetInstance().FinishCompressDxt() ) break );
					}
				}
				else
				{
					if (PARAM_headblenddebug.Get())
					{
						pedWarningf("[HEAD BLEND] %d in state rendering, but missing textures! %p (%d, %p), %p (%d, %p)",
							f, srcTex1, next.srcTexIdx1.Get(), g_TxdStore.Get(next.srcTexIdx1), srcTex2, next.srcTexIdx2.Get(), g_TxdStore.Get(next.srcTexIdx2));
					}
				}
			}

			////////////////////////////////////////////////////////////////////////////
			// Body skin blending
			////////////////////////////////////////////////////////////////////////////
			FRAME_SPREAD_ONLY(else) if (sysInterlockedRead((unsigned*)&next.state) == STATE_RENDER_BODY_SKIN)
			{
				grcTexture* dstTex = NULL;
				strLocalIndex skinCompIndex = strLocalIndex(-1);
				bool failed = false;

				if (PARAM_headblenddebug.Get())
					pedWarningf("[HEAD BLEND] %d in state render_body_skin (%d)", f, next.skinCompState);

                SIMULATE_SLOW_BLEND();

				if (!next.reblendSkin[SBC_FEET] && !next.reblendSkin[SBC_UPPR] && !next.reblendSkin[SBC_LOWR])
				{
					sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_OVERLAY);
                    f--; // loop again this frame, it's fast enough
					continue;
				}

				if(!Verifyf(next.skinCompState < SBC_MAX, "Stuck in RENDER_BODY_SKIN, state is SBC_MAX so something went wrong"))
					continue;


				if (next.skinBlend[next.skinCompState].srcSkin0 == -1 ||
					next.skinBlend[next.skinCompState].srcSkin1 == -1 ||
					next.skinBlend[next.skinCompState].srcSkin2 == -1 ||
					next.skinBlend[next.skinCompState].clothTex == -1)
				{
					failed = true;
				}

				if (!next.reblendSkin[next.skinCompState])
				{
					failed = true;
				}

				skinCompIndex = slot.skinCompIndex[next.skinCompState];

				// consider this done, even if it fails after this point
				next.reblendSkin[next.skinCompState] = false;

				// choose correct render target based on the size of the destination texture
				next.rt = NULL;
				if (!failed)
				{
					if (Verifyf(g_TxdStore.Get(skinCompIndex), "Target texture for skin component on slot %d (state %d) is not in memory!", next.slotIndex, next.skinCompState))
					{
						dstTex = g_TxdStore.Get(skinCompIndex)->GetEntry(0);
						if (Verifyf(dstTex, "No destination memory available for skin texture. This is not good!"))
						{
							s32 width = dstTex->GetWidth();
							s32 height = dstTex->GetHeight();

							if (width == GetInstance().m_renderTarget->GetWidth() && height == GetInstance().m_renderTarget->GetHeight())
							{
								next.rt = GetInstance().m_renderTarget;
								next.mipCount = (s8)dstTex->GetMipMapCount();
#if __D3D11 || RSG_ORBIS
								next.rtTemp = GetInstance().m_renderTargetTemp;
#endif
							}
							else if (width == GetInstance().m_renderTargetSmall->GetWidth() && height == GetInstance().m_renderTargetSmall->GetHeight())
							{
								next.rt = GetInstance().m_renderTargetSmall;
								next.mipCount = (s8)dstTex->GetMipMapCount();
#if __D3D11 || RSG_ORBIS
								next.rtTemp = GetInstance().m_renderTargetSmallTemp;
#endif
							}
							else
							{
								Assertf(false, "Skin texture size mismatch for '%s' %dx%d vs RT %dx%d", g_TxdStore.GetName(skinCompIndex),
									width, height, GetInstance().m_renderTarget->GetWidth(), GetInstance().m_renderTarget->GetHeight());
								failed = true;
							}
						}
						else
						{
							failed = true;
						}
					}
					else
					{
						failed = true;
					}
				}

				if (failed)
				{
					if (next.skinCompState < (SBC_MAX - 1))
					{
						// not quite done yet, continue with next component that might have skin visible
						next.skinCompState++;
						next.rt = NULL;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_BODY_SKIN);
					}
					else
					{
						// we're done with skin blending, jump to overlays
						next.skinCompState = SBC_FEET; // first component
						next.rt = NULL;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_OVERLAY);
					}
                    f--; // redo this blend this frame
					continue;
				}

				// skip if we don't have the required assets
				const fwTxd* skin0 = g_TxdStore.Get(strLocalIndex(next.skinBlend[next.skinCompState].srcSkin0));
				const fwTxd* skin1 = g_TxdStore.Get(strLocalIndex(next.skinBlend[next.skinCompState].srcSkin1));
				const fwTxd* skin2 = g_TxdStore.Get(strLocalIndex(next.skinBlend[next.skinCompState].srcSkin2));
				const fwTxd* cloth = g_TxdStore.Get(strLocalIndex(next.skinBlend[next.skinCompState].clothTex));

				if (!skin0 || !skin0->GetEntry(0) || !skin1 || !skin1->GetEntry(0) || !cloth || !cloth->GetEntry(0))
					continue;

				strLocalIndex skinMaskParent = strLocalIndex(next.skinBlend[next.skinCompState].skinMaskParent);
				if (!g_DwdStore.Get(skinMaskParent))
					continue;

				rmcDrawable* skinDrawableParent = g_DwdStore.Get(skinMaskParent)->GetEntry(0);
				if (!skinDrawableParent)
					continue;

				const char* dbgName = "n/a";
				NOTFINAL_ONLY(dbgName = g_DwdStore.GetName(skinMaskParent));
				grcTexture* mask = GetInstance().GetSpecTextureFromDrawable(skinDrawableParent, dbgName);
				if (!mask)
					continue;

				u8 palId = 0;
				grcTexture* pal = NULL;
                
                if (!next.usePaletteRgb)
                {
                    palId = next.skinBlend[next.skinCompState].palId;
                    pal = GetInstance().GetPalTextureFromDrawable(skinDrawableParent);
                }

				grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
				grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

				next.lastRenderFrame = grcDevice::GetFrameCounter();
				grcTextureFactory::GetInstance().LockRenderTarget(0, next.rt, NULL);
				grcViewport vp;
				vp.Screen();
				grcViewport *old = grcViewport::SetCurrent(&vp);
				GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, 0, 0);

				if (next.texBlend != 1.f)
				{
					grcBindTexture(skin0->GetEntry(0));
					grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));
				}
				if (next.texBlend)
				{
					grcBindTexture(skin1->GetEntry(0));
					grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(next.texBlend * 255)));
				}

				// blend third texture if it exists
				if (skin2 && skin2->GetEntry(0) && next.varBlend)
				{
					grcBindTexture(skin2->GetEntry(0));
					grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(next.varBlend * 255)));
				}

				// blend cloth on top
				s_blendShader->SetVar(s_texture0, cloth->GetEntry(0));
				s_blendShader->SetVar(s_texture1, mask);

				grcEffectTechnique technique = s_skinToneBlendTechnique;
                if (!next.usePaletteRgb)
                {
                    const float divider = 1.f / PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM;
                    float palIdVar = (palId + 0.5f) * divider;
                    s_blendShader->SetVar(s_texture2, pal);
                    s_blendShader->SetVar(s_paletteId, palIdVar);
                }
                else
                {
                    Assertf(next.paletteIndex > -1, "No palette index assigned to blend '%d', something went wrong!", f);
                    Color32 col0 = GetInstance().m_palette.GetColor(next.paletteIndex, 0);
                    Color32 col1 = GetInstance().m_palette.GetColor(next.paletteIndex, 1);
                    Color32 col2 = GetInstance().m_palette.GetColor(next.paletteIndex, 2);
                    Color32 col3 = GetInstance().m_palette.GetColor(next.paletteIndex, 3);

                    s_blendShader->SetVar(s_paletteColor0, col0);
                    s_blendShader->SetVar(s_paletteColor1, col1);
                    s_blendShader->SetVar(s_paletteColor2, col2);
                    s_blendShader->SetVar(s_paletteColor3, col3);

					technique = s_skinToneBlendRgbTechnique;
                }

				ASSERT_ONLY(g_bIgnoreNullTextureOnBind = true);

				if (s_blendShader->BeginDraw(grmShader::RMC_DRAW, true, technique))
				{
					s_blendShader->Bind(0);

					grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

					s_blendShader->UnBind();
					s_blendShader->EndDraw();
				}
				s_blendShader->SetVar(s_texture0, grcTexture::None);
				s_blendShader->SetVar(s_texture1, grcTexture::None);
                s_blendShader->SetVar(s_paletteId, 0.f);

				ASSERT_ONLY(g_bIgnoreNullTextureOnBind = false);

                if (!next.usePaletteRgb)
                {
                    s_blendShader->SetVar(s_texture2, grcTexture::None);
                }
                else
                {
                    Vector4 palColReset(0.f, 0.f, 0.f, 0.f);
                    s_blendShader->SetVar(s_paletteColor0, palColReset);
                    s_blendShader->SetVar(s_paletteColor1, palColReset);
                    s_blendShader->SetVar(s_paletteColor2, palColReset);
                    s_blendShader->SetVar(s_paletteColor3, palColReset);
                }

				// blend body overlays if we have any
				if (next.skinCompState == SBC_UPPR)
				{
					for (s32 g = MAX_OVERLAYS - NUM_BODY_OVERLAYS; g < MAX_OVERLAYS; ++g)
					{
						RenderOverlay(hnd, next, slot, g);
					}
				}

				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				grcViewport::SetCurrent(old);

#if RSG_DURANGO
				g_grcCurrentContext->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_PS_PARTIAL_FLUSH);
#endif

				BANK_ONLY(next.startTime = sysTimer::GetSystemMsTime();)
				if (Verifyf(dstTex, "No destination memory available for body skin texture. This is not good!"))
				{
					failed = !GetInstance().CompressDxt(next.rt, dstTex, true);
				}
				BANK_ONLY(next.totalTime = sysTimer::GetSystemMsTime() - next.startTime;)

                if (failed)
                {
					if (next.skinCompState < (SBC_MAX - 1))
					{
						// not quite done yet, continue with next component that might have skin visible
						next.skinCompState++;
						next.rt = NULL;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_BODY_SKIN);
					}
					else
					{
						// we're done with skin blending, jump to overlays
						next.skinCompState = SBC_FEET; // first component
						next.rt = NULL;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_OVERLAY);
					}
                    f--; // loop again this frame, it's fast enough
					continue;
                }
                else
                {
					next.skinBlend[next.skinCompState].used = true;
					if (next.skinCompState < (SBC_MAX - 1))
					{
						// not quite done yet, continue with next component that might have skin visible
						next.skinCompState++;
						f--; // loop again this frame, it's fast enough
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_BODY_SKIN);
					}
					else
					{
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_OVERLAY);
					}
					
					// Exit if we're waiting on compression, we can't proceed until it's done
					WIN32PC_ONLY( if ( !GetInstance().FinishCompressDxt() ) break );
                }
			}

			////////////////////////////////////////////////////////////////////////////
			// Overlay normal and spec map blend
			////////////////////////////////////////////////////////////////////////////
			FRAME_SPREAD_ONLY(else) if (sysInterlockedRead((unsigned*)&next.state) == STATE_RENDER_OVERLAY)
			{
				grcTexture* dstTex = NULL;
				bool failed = false;

				if (PARAM_headblenddebug.Get())
					pedWarningf("[HEAD BLEND] %d in state render_overlay (%d)", f, next.overlayCompSpec);

                SIMULATE_SLOW_BLEND();

				// early out and jump to render state if no overlay texture are found, overlay spec/normal maps are optional
				const grcTexture* srcTex = next.overlayCompSpec ? slot.headSpecSrc : slot.headNormSrc;
				if (!srcTex || !next.reblendOverlayMaps)
				{
					if (next.overlayCompSpec || !slot.addedOverlayRefs || !next.reblendOverlayMaps)
					{
                        next.reblendOverlayMaps = false;
                        next.overlayCompSpec = false;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDERING);
					}
					else
						next.overlayCompSpec = true;

					continue;
				}

				// choose correct render target for this overlay based on the size of the destination texture
				next.rt = GetInstance().m_renderTarget;
#if __D3D11 || RSG_ORBIS
				next.rtTemp = GetInstance().m_renderTargetTemp;
#endif
				dstTex = next.overlayCompSpec ? slot.headSpec : slot.headNorm;

				if (Verifyf(dstTex, "No destination memory available for head normal/specular texture. This is not good!"))
				{
					if (dstTex->GetWidth() != next.rt->GetWidth() || dstTex->GetHeight() != next.rt->GetHeight())
					{
						next.rt = GetInstance().m_renderTargetSmall;
#if __D3D11 || RSG_ORBIS
						next.rtTemp = GetInstance().m_renderTargetSmallTemp;
#endif
						if (dstTex->GetWidth() != next.rt->GetWidth() || dstTex->GetHeight() != next.rt->GetHeight())
						{
							Assertf(false, "Head blend normal/specular map size mismatch for '%s'", g_DwdStore.GetName(slot.dwdIndex));
							failed = true;
						}
					}
				}
				else
					failed = true;

				if (failed)
				{
					if (next.overlayCompSpec)
					{
						next.reblendOverlayMaps = false;
						next.overlayCompSpec = false;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDERING);
					}
					else
					{
						next.overlayCompSpec = true;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_OVERLAY);
                        f--; // loop again this frame, it's fast enough
					}
					continue;
				}

				Assert(dstTex);
				next.mipCount = (s8)dstTex->GetMipMapCount();

				// grab all textures we need for rendering, if any
				grcTexture* overlayTextures[MAX_OVERLAYS - NUM_BODY_OVERLAYS] = {0};
				for (s32 g = 0; g < MAX_OVERLAYS - NUM_BODY_OVERLAYS; ++g)
				{
					strLocalIndex overlayIndex = strLocalIndex(slot.overlayIndex[g]);

					if ((slot.addedOverlayRefs & (1 << g)) == 0)
						continue;

					if (overlayIndex == -1 || !g_TxdStore.Get(overlayIndex) || !Verifyf(g_TxdStore.Get(overlayIndex)->GetCount() > 0, "Meshblendmanager: Selected overlay txd has no textures!"))
						continue;

					overlayTextures[g] = GetInstance().GetOverlayTexture(overlayIndex, next.overlayCompSpec ? OTT_SPECULAR : OTT_NORMAL);
				}

                grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
                grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

				next.lastRenderFrame = grcDevice::GetFrameCounter();
                grcTextureFactory::GetInstance().LockRenderTarget(0, next.rt, NULL);
                grcViewport vp;
                vp.Screen();
                grcViewport *old = grcViewport::SetCurrent(&vp);
                GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, 0, 0);

                grcBindTexture(srcTex);
                grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

                // blend additional normal and spec maps
                for (s32 g = 0; g < MAX_OVERLAYS - NUM_BODY_OVERLAYS; ++g)
                {
                    if (!overlayTextures[g])
                        continue;

					next.rt->Realize();
#if __D3D11 || RSG_ORBIS
					next.rtTemp->Copy(next.rt);
					s_blendShader->SetVar(s_texture0, next.rtTemp);
#else
					s_blendShader->SetVar(s_texture0, next.rt);
#endif                      
					s_blendShader->SetVar(s_texture1, overlayTextures[g]);
					if (s_blendShader->BeginDraw(grmShader::RMC_DRAW, true, next.overlayCompSpec ? s_skinSpecBlendTechnique : s_normalBlendTechnique))
					{
						s_blendShader->Bind(0);

						grcDrawSingleQuadf(0, 0, (float)next.rt->GetWidth(), (float)next.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(next.overlayNormAlpha[g] * 255)));

						s_blendShader->UnBind();
						s_blendShader->EndDraw();
					}
					s_blendShader->SetVar(s_texture0, grcTexture::None);
					s_blendShader->SetVar(s_texture1, grcTexture::None);
                }

                grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				grcViewport::SetCurrent(old);

#if RSG_DURANGO
				g_grcCurrentContext->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_PS_PARTIAL_FLUSH);
#endif

				BANK_ONLY(next.startTime = sysTimer::GetSystemMsTime();)
                if (Verifyf(dstTex, "No destination memory available for overlay texture. This is not good!"))
                {
                    failed = !GetInstance().CompressDxt(next.rt, dstTex, true);
                }
                BANK_ONLY(next.totalTime = sysTimer::GetSystemMsTime() - next.startTime;)

                // we don't care if we fail or not, we either try next overlay (specular) or we fall back to rendering state were we idle
                {
					// spec map is the last one
					if (next.overlayCompSpec)
					{
						next.overlayCompSpec = false;
						next.reblendOverlayMaps = false;
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDERING);
					}
					else
					{
						// do the spec map now
						next.overlayCompSpec = true;
						f--; // loop again this frame, it's fast enough
						sysInterlockedExchange((unsigned*)&next.state, STATE_RENDER_OVERLAY);
					}

					// Exit if we're waiting on compression, we can't proceed until it's done
					WIN32PC_ONLY( if ( !GetInstance().FinishCompressDxt() ) break );
                }
			}
		}
	}
	
	PIXEnd();
}

void MeshBlendManager::RenderOverlay(MeshBlendHandle hnd, const sBlendEntry& blend, const sBlendSlot& slot, u32 idx)
{
	strLocalIndex overlayIndex = strLocalIndex(slot.overlayIndex[idx]);

	if ((slot.addedOverlayRefs & (1 << idx)) == 0)
		return;

	if (overlayIndex == -1 || !g_TxdStore.Get(overlayIndex) || !Verifyf(g_TxdStore.Get(overlayIndex)->GetCount() > 0, "Meshblendmanager: Selected overlay txd has no textures!"))
		return;

	grcTexture* overlay = GetInstance().GetOverlayTexture(overlayIndex, OTT_DIFFUSE);
	Assertf(overlay, "No diffuse texture found for overlay %d (%s)", idx, g_TxdStore.GetName(overlayIndex));
	if (overlay)
	{
		if (blend.blendType[idx] == OBT_OVERLAY)
		{
			blend.rt->Realize();

			s_blendShader->SetVar(s_texture0, overlay);
#if __D3D11 || RSG_ORBIS
			blend.rtTemp->Copy(blend.rt);
			s_blendShader->SetVar(s_texture1, blend.rtTemp);
#else
			s_blendShader->SetVar(s_texture1, blend.rt);
#endif  
			if (s_blendShader->BeginDraw(grmShader::RMC_DRAW, true, s_skinOverlayBlendTechnique))
			{
				s_blendShader->Bind(0);

				grcDrawSingleQuadf(0, 0, (float)blend.rt->GetWidth(), (float)blend.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(blend.overlayAlpha[idx] * 255)));

				s_blendShader->UnBind();
				s_blendShader->EndDraw();
			}
			s_blendShader->SetVar(s_texture0, grcTexture::None);
			s_blendShader->SetVar(s_texture1, grcTexture::None);
		}
		else
		{
			if (blend.overlayRampType[idx] == RT_NONE)
			{
				grcBindTexture(overlay);
				grcDrawSingleQuadf(0, 0, (float)blend.rt->GetWidth(), (float)blend.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(blend.overlayAlpha[idx] * 255)));
			}
			else
			{
				s32 rampSel = 0;
				s32 rampSel2 = 0;
				grcTexture* rampTex = GetInstance().GetTintTexture(hnd, (eRampType)blend.overlayRampType[idx], rampSel, rampSel2);
				if (rampTex)
				{
					s_blendShader->SetVar(s_texture0, overlay);
					s_blendShader->SetVar(s_texture3, rampTex);

					const float divider = 1.f / PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM_LARGE;
					float palIdVar = (blend.overlayTintIndex[idx] + 0.5f) * divider;
					s_blendShader->SetVar(s_paletteId, palIdVar);
					float palIdVar2 = (blend.overlayTintIndex2[idx] + 0.5f) * divider;
					s_blendShader->SetVar(s_paletteId2, palIdVar2);

					if (s_blendShader->BeginDraw(grmShader::RMC_DRAW, true, s_skinPaletteBlendTechnique))
					{
						s_blendShader->Bind(0);

						grcDrawSingleQuadf(0, 0, (float)blend.rt->GetWidth(), (float)blend.rt->GetHeight(), 0, 0, 0, 1, 1, Color32(255, 255, 255, int(blend.overlayAlpha[idx] * 255)));

						s_blendShader->UnBind();
						s_blendShader->EndDraw();
					}

					s_blendShader->SetVar(s_paletteId2, 0.f);
					s_blendShader->SetVar(s_paletteId, 0.f);
					s_blendShader->SetVar(s_texture0, grcTexture::None);
					s_blendShader->SetVar(s_texture3, grcTexture::None);
				}

				Vector4 palColReset(0.f, 0.f, 0.f, 0.f);
				s_blendShader->SetVar(s_paletteColor0, palColReset);
			}
		}
	}
}

void MeshBlendManager::ExtraContentUpdated(bool enable)
{
	if (enable)
	{
		if (s_eyeColorTxdIndex == -1)
		{
			s_eyeColorTxdIndex = g_TxdStore.FindSlot(s_eyeColorTxdName);
			if (s_eyeColorTxdIndex != -1)
			{
#if RSG_PC
				// NB: Need to get / set / restore texture quality here rather than pushing / popping, because
				// the actual load is performed on the resource placement thread, and we can't push / pop
				// texture quality for another thread.
				// Make sure we flush all pending loads before we change the texture quality though!
				CStreaming::LoadAllRequestedObjects();
				u32 oldQuality = grcTexturePC::GetTextureQuality();
				grcTexturePC::SetTextureQuality(grcTexturePC::HIGH);
#endif

				CStreaming::SetDoNotDefrag(s_eyeColorTxdIndex, g_TxdStore.GetStreamingModuleId());

				strIndex streamingIndex = g_TxdStore.GetStreamingIndex(s_eyeColorTxdIndex);
				strStreamingEngine::GetInfo().RequestObject(streamingIndex, STRFLAG_DONTDELETE);
				CStreaming::LoadAllRequestedObjects(); // :(

#if RSG_PC
				grcTexturePC::SetTextureQuality(oldQuality);
#endif
			}
		}
	}
	else
	{
		if (s_eyeColorTxdIndex != -1)
		{
			CStreaming::SetObjectIsDeletable(s_eyeColorTxdIndex, g_TxdStore.GetStreamingModuleId());
			s_eyeColorTxdIndex = -1;
			gRenderThreadInterface.Flush(); // We should be on a loading screen when removing eye colours
		}
	}

	GetInstance().UpdateEyeColorData();

#if __BANK
	CPedVariationDebug::UpdateHeadBlendEyeColorSlider();
#endif
}

Color32 MeshBlendManager::GetRampColor(eRampType type, u32 index)
{
	Color32 col;
	if (index >= ms_rampCount[type])
		return col;

	return ms_rampColors[type][index];
}

bool MeshBlendManager::IsHandleValid(MeshBlendHandle handle) const
{
	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	if (type >= MBT_MAX || index >= m_blends[type].GetCount())
		return false;

	return true;
}

MeshBlendHandle MeshBlendManager::GetHandleFromIndex(eMeshBlendTypes type, u16 index) const
{
	return type << 16 | index;
}

grcTexture* MeshBlendManager::GetCrewPaletteTexture(MeshBlendHandle handle, s32& palSelector)
{
	if (!IsHandleValid(handle))
		return NULL;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	palSelector = m_blends[type][index].paletteIndex;
	return GetMiscTxdTexture(s_paletteMiscIdx);
}

grcTexture* MeshBlendManager::GetTintTexture(MeshBlendHandle handle, eRampType rampType, s32& palSelector, s32& palSelector2)
{
	if (!IsHandleValid(handle))
		return NULL;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	palSelector = m_blends[type][index].hairRampIndex;
	palSelector2 = m_blends[type][index].hairRampIndex2;
	return GetMiscTxdTexture(s_rampMiscIdx[rampType]);
}

void MeshBlendManager::ReleaseBlend(MeshBlendHandle handle, bool delayed)
{
	pedDebugf1("MeshBlendManager::ReleaseBlend: Releasing handle 0x%08x", handle);
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	sBlendEntry& blend = m_blends[type][index];

	if (delayed)
	{
		Assertf(blend.refCount >= 1, "Refcount not expected to be this low on mesh blend! (%d)", blend.refCount);
		if (blend.refCount <= 1)
		{
			blend.deleteFrame = DELAYED_DELETE_FRAMES;
			blend.reblendMesh = false;
		}

		blend.refCount--;
		return;
	}
	Assertf(blend.refCount == 0, "Refcount expected to be 0! (%d)", blend.refCount);

	blend.compressState.Abort();

	sysInterlockedExchange((unsigned*)&blend.state, STATE_NOT_USED);

	blend.deleteFrame = -1;
	blend.copyNormAndSpecTex = true;

	s32 slotIndex = blend.slotIndex;
	Assertf(slotIndex != -1, "Bad slot index found on blend!");

	sBlendSlot& slot = m_slots[type][slotIndex];

	// if we early out for some reason make sure we don't remove refs we haven't added
	if (m_blends[type][index].addedRefs)
	{
		g_DwdStore.RemoveRef(slot.dwdIndex, REF_OTHER);
		g_TxdStore.RemoveRef(slot.txdIndex, REF_OTHER);

		g_DwdStore.RemoveRef(m_slots[type][m_slots[type].GetCount() - 1].dwdIndex, REF_OTHER);

		m_blends[type][index].addedRefs = false;
	}

	if (slot.dwdReq != -1)
	{
		m_reqs[slot.dwdReq].Release();
		if (slot.dwdIndex != -1)
			g_DwdStore.ClearRequiredFlag(slot.dwdIndex.Get(), STRFLAG_DONTDELETE);
	}
	slot.dwdReq = -1;

	if (slot.txdReq != -1)
	{
		m_reqs[slot.txdReq].Release();
		if (slot.txdIndex != -1)
			g_TxdStore.ClearRequiredFlag(slot.txdIndex.Get(), STRFLAG_DONTDELETE);
	}
	slot.txdReq = -1;

	slot.headNorm = NULL;
	slot.headSpec = NULL;
	slot.headNormSrc = NULL;
	slot.headSpecSrc = NULL;
	slot.inUse = false;

#if !__FINAL
	Displayf("[MESHBLEND] free at [%d][%d]", type, index);
	ms_blendSlotBacktraces[type][index] = 0;
#endif //!__FINAL

	for (s32 i = 0; i < MAX_OVERLAYS; ++i)
	{
		s32 req = slot.overlayReq[i];
		strLocalIndex txdIndex = strLocalIndex(slot.overlayIndex[i]);

		if (req != -1)
			m_reqs[req].Release();

		if (txdIndex != -1 && ((slot.addedOverlayRefs & (1 << i)) != 0))
			g_TxdStore.RemoveRef(txdIndex, REF_OTHER);

		slot.overlayReq[i] = -1;
		slot.overlayIndex[i] = -1;
	}

	// clear mask that stores which overlays have incremented refcounts
	slot.addedOverlayRefs = 0;

	if (m_blends[type][index].srcDrawable1 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcDrawable1, REF_OTHER);
	if (m_blends[type][index].srcDrawable2 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcDrawable2, REF_OTHER);

	m_blends[type][index].srcDrawable1 = -1;
	m_blends[type][index].srcDrawable2 = -1;

	for (s32 i = 0; i < SBC_MAX; ++i)
	{
		m_blends[type][index].skinBlend[i].used = false;

		if (m_blends[type][index].skinBlend[i].clothTex != -1)
			g_TxdStore.RemoveRef(strLocalIndex(m_blends[type][index].skinBlend[i].clothTex), REF_OTHER);
		if (m_blends[type][index].skinBlend[i].srcSkin0 != -1)
			g_TxdStore.RemoveRef(strLocalIndex(m_blends[type][index].skinBlend[i].srcSkin0), REF_OTHER);
		if (m_blends[type][index].skinBlend[i].srcSkin1 != -1)
			g_TxdStore.RemoveRef(strLocalIndex(m_blends[type][index].skinBlend[i].srcSkin1), REF_OTHER);
		if (m_blends[type][index].skinBlend[i].srcSkin2 != -1)
			g_TxdStore.RemoveRef(strLocalIndex(m_blends[type][index].skinBlend[i].srcSkin2), REF_OTHER);
		if (m_blends[type][index].skinBlend[i].skinMaskParent != -1)
			g_DwdStore.RemoveRef(strLocalIndex(m_blends[type][index].skinBlend[i].skinMaskParent), REF_OTHER);

		m_blends[type][index].skinBlend[i].clothTex = -1;
		m_blends[type][index].skinBlend[i].srcSkin0 = -1;
		m_blends[type][index].skinBlend[i].srcSkin1 = -1;
		m_blends[type][index].skinBlend[i].srcSkin2 = -1;
		m_blends[type][index].skinBlend[i].skinMaskParent = -1;

		if (slot.skinCompReq[i] != -1)
		{
			m_reqs[slot.skinCompReq[i]].Release();
			if (slot.skinCompIndex[i] != -1)
				g_TxdStore.ClearRequiredFlag(slot.skinCompIndex[i].Get(), STRFLAG_DONTDELETE);
		}
		slot.skinCompReq[i] = -1;

		Assertf(slot.skinCompIndex[i] != -1, "Missed skin component index %d for slot %d", i, slotIndex);
		if ((slot.addedSkinCompRefs & (1 << i)) != 0)
			g_TxdStore.RemoveRef(slot.skinCompIndex[i], REF_OTHER);
	}

	// clear mask that stores which skin component texture have incremented refcounts
	slot.addedSkinCompRefs = 0;

	if (m_blends[type][index].srcTexIdx1 != -1)
		g_TxdStore.RemoveRef(m_blends[type][index].srcTexIdx1, REF_OTHER);
	if (m_blends[type][index].srcTexIdx2 != -1)
		g_TxdStore.RemoveRef(m_blends[type][index].srcTexIdx2, REF_OTHER);
	if (m_blends[type][index].srcTexIdx3 != -1)
		g_TxdStore.RemoveRef(m_blends[type][index].srcTexIdx3, REF_OTHER);

	m_blends[type][index].srcTexIdx1 = -1;
	m_blends[type][index].srcTexIdx2 = -1;
	m_blends[type][index].srcTexIdx3 = -1;

	if (m_blends[type][index].srcGeomIdx1 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcGeomIdx1, REF_OTHER);
	if (m_blends[type][index].srcGeomIdx2 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcGeomIdx2, REF_OTHER);
	if (m_blends[type][index].srcGeomIdx3 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcGeomIdx3, REF_OTHER);

	m_blends[type][index].srcGeomIdx1 = -1;
	m_blends[type][index].srcGeomIdx2 = -1;
	m_blends[type][index].srcGeomIdx3 = -1;

	if (m_blends[type][index].srcParentGeomIdx1 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcParentGeomIdx1, REF_OTHER);
	if (m_blends[type][index].srcParentGeomIdx2 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcParentGeomIdx2, REF_OTHER);

	m_blends[type][index].srcParentGeomIdx1 = -1;
	m_blends[type][index].srcParentGeomIdx2 = -1;

	Color32 col(255, 255, 255, 255);
	m_palette.SetColor(blend.paletteIndex, 0, col);
	m_palette.SetColor(blend.paletteIndex, 1, col);
	m_palette.SetColor(blend.paletteIndex, 2, col);
	m_palette.SetColor(blend.paletteIndex, 3, col);
    m_palette.ReleaseSlot(m_blends[type][index].paletteIndex);

	for (s32 i = 0; i < m_blends[type][index].clones.GetCount(); ++i)
		m_blends[type][index].clones[i] = NULL;
    m_blends[type][index].clones.Reset();
	pedDebugf1("[HEAD BLEND] ReleaseBlend: Released blend handle 0x%08x", handle);
}

MeshBlendHandle MeshBlendManager::CloneBlend(MeshBlendHandle handle, CPed* targetPed)
{
	if (!IsHandleValid(handle))
		return MESHBLEND_HANDLE_INVALID;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];
	blend.refCount++;

    s32 cloneIndex = -1;
    for (s32 i = 0; i < blend.clones.GetCount(); ++i)
	{
        if (blend.clones[i] == NULL)
		{
            cloneIndex = i;
            break;
		}
	}

    if (cloneIndex != -1)
        blend.clones[cloneIndex] = targetPed;
	else
	{
        blend.clones.PushAndGrow(RegdPed(targetPed));
	}

	pedDebugf1("[HEAD BLEND] CloneBlend: Cloned blend handle 0x%08x for new ped %p. Geo(%d, %d, %d, %f), Tex(%d, %d, %d, %f)",
		handle, targetPed, blend.srcGeomIdx1.Get(), blend.srcGeomIdx2.Get(), blend.srcGeomIdx3.Get(), blend.geomBlend,
		blend.srcTexIdx1.Get(), blend.srcTexIdx2.Get(), blend.srcTexIdx3.Get(), blend.texBlend);
	return handle;
}

u32 MeshBlendManager::GetPedCloneCount(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return 0;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	return m_blends[type][index].clones.GetCount();
}

CPed* MeshBlendManager::GetPedClone(MeshBlendHandle handle, u32 cloneIndex)
{
	if (!IsHandleValid(handle))
		return NULL;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

    if (cloneIndex >= m_blends[type][index].clones.GetCount())
        return NULL;

    return m_blends[type][index].clones[cloneIndex];
}

void MeshBlendManager::MicroMorph(MeshBlendHandle handle, eMicroMorphType morphType, s32 storeIndex, float blendVal)
{
	if (!IsHandleValid(handle))
		return;

	if (morphType >= MMT_MAX)
		return;

	u32 type = GetTypeFromHandle(handle);
	u32 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

	strLocalIndex localIndex = strLocalIndex(storeIndex);
    if (localIndex == blend.microMorphGeom[morphType] && fabs(blend.microMorphAlpha[morphType] - blendVal) < 0.0001f)
        return;

	sysCriticalSection cs(s_assetToken);

	bool stillRequesting = sysInterlockedRead((unsigned*)&blend.state) == STATE_REQUEST_SLOT;
	bool stillWaitingForMem = sysInterlockedRead((unsigned*)&blend.state) == STATE_INIT_RT;

	if (localIndex.IsValid())
		g_DrawableStore.AddRef(localIndex, REF_OTHER);

	if (blend.microMorphGeom[morphType].IsValid())
		g_DrawableStore.RemoveRef(blend.microMorphGeom[morphType], REF_OTHER);

    blend.microMorphGeom[morphType] = localIndex;
    blend.microMorphAlpha[morphType] = blendVal;

	blend.reblendMesh = true;
	m_blends[type][index].finalizeFrame = -1;

	if (stillWaitingForMem)
		sysInterlockedExchange((unsigned*)&blend.state, STATE_INIT_RT);
	else
		sysInterlockedExchange((unsigned*)&blend.state, stillRequesting ? STATE_REQUEST_SLOT : STATE_RENDERING);
}

void MeshBlendManager::SetNewBlendValues(MeshBlendHandle handle, float geomBlend, float texBlend, float varBlend)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

    geomBlend = rage::Max(rage::Min(geomBlend, 1.f), 0.f);
    texBlend = rage::Max(rage::Min(texBlend, 1.f), 0.f);
    varBlend = rage::Max(rage::Min(varBlend, 1.f), 0.f);

	sysCriticalSection cs(s_assetToken);

	bool changeGeomBlend = (fabs(blend.geomBlend - geomBlend) > 0.0001f);
    bool changeTexVarBlend = (fabs(blend.varBlend - varBlend) > 0.0001f || fabs(blend.texBlend - texBlend) > 0.0001f);

	// Debug print
	if (changeGeomBlend || changeTexVarBlend)
	{
		if (PARAM_headblenddebug.Get())
		{
			pedWarningf("[HEAD BLEND] MeshBlendManager::SetNewBlendValues(0x%08x, %f, %f, %f): Old values: %f, %f, %f",
				handle, geomBlend, texBlend, varBlend, blend.geomBlend, blend.texBlend, blend.varBlend);
		}

		#if !__NO_OUTPUT
			if(blend.finalizeFrame == IS_FINALIZED)
			{
				Displayf("[HEAD BLEND] Trying to modify finalized head blend 0x%08x!", handle);
				#if __BANK
					sysStack::PrintStackTrace();
					scrThread::PrePrintStackTrace();
				#endif
			}
		#endif
	}
	else if (PARAM_headblenddebug.Get())
	{
		pedWarningf("[HEAD BLEND] MeshBlendManager::SetNewBlendValues(0x%08x, %f, %f, %f): No change, early out",
			handle, geomBlend, texBlend, varBlend);
	}

	if (changeGeomBlend)
	{
		if (blend.isParent)
		{
			// if this is a parent blend and we crossed the 0.5f threshold make sure we copy the spec/norm maps again
			float oldBlend = blend.geomBlend;
			if ((oldBlend <= 0.5f && geomBlend > 0.5f) || (oldBlend > 0.5f && geomBlend <= 0.5f))
				blend.copyNormAndSpecTex = true;
		}

		blend.reblendMesh = true;
		blend.geomBlend = geomBlend;
	}

    if (changeTexVarBlend)
    {
		blend.reblend = true;
		blend.reblendSkin[SBC_FEET] = true;
		blend.reblendSkin[SBC_UPPR] = true;
		blend.reblendSkin[SBC_LOWR] = true;

		blend.varBlend = varBlend;
		blend.texBlend = texBlend;
	}
}

void MeshBlendManager::SetEyeColor(MeshBlendHandle handle, s32 colorIndex)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

    if (colorIndex < 0 || colorIndex >= (m_eyeColorCols * m_eyeColorRows))
        colorIndex = -1;

	if (blend.eyeColorIndex == (s8)colorIndex)
	{
		if (PARAM_headblenddebug.Get())
			pedWarningf("[HEAD BLEND] MeshBlendManager::SetEyeColor(0x%08x, %d): No change, early out", handle, colorIndex);
		return;
	}

#if !__NO_OUTPUT
	if(blend.finalizeFrame == IS_FINALIZED)
	{
        Displayf("[HEAD BLEND] Trying to modify finalized head blend 0x%08x!", handle);
		#if __BANK
			sysStack::PrintStackTrace();
			scrThread::PrePrintStackTrace();
		#endif
	}
#endif

	if (PARAM_headblenddebug.Get())
		pedWarningf("[HEAD BLEND] MeshBlendManager::SetEyeColor(0x%08x, %d): Old colour: %d", handle, colorIndex, blend.eyeColorIndex);
	blend.eyeColorIndex = (s8)colorIndex;
	blend.reblend = true;
}

void MeshBlendManager::DoGeometryBlend(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

	if (blend.deleteFrame > -1)
		return;

	float varBlend = blend.varBlend;

	s32 slotIndex = blend.slotIndex;
	if (slotIndex < 0)
		return;

	if (blend.srcGeomIdx1 == -1 || blend.srcGeomIdx2 == -1 ||
		blend.srcGeomIdx3 == -1 || m_slots[type][slotIndex].dwdIndex == -1 ||
		m_slots[type][m_slots[type].GetCount() - 1].dwdIndex == -1)
		return;

	Dwd* srcDwd1 = g_DwdStore.Get(blend.srcGeomIdx1);
	Dwd* srcDwd2 = g_DwdStore.Get(blend.srcGeomIdx2);
	Dwd* srcDwd3 = g_DwdStore.Get(blend.srcGeomIdx3);
	Dwd* targetDwd = g_DwdStore.Get(m_slots[type][slotIndex].dwdIndex);
	Dwd* tempDwd = g_DwdStore.Get(m_slots[type][m_slots[type].GetCount() - 1].dwdIndex);
	if (!srcDwd1 || !srcDwd2 || !srcDwd3 || !targetDwd || !tempDwd)
		return;

	// handle parents first, if we need to
	rmcDrawable* parent1Geom1 = NULL;
	rmcDrawable* parent2Geom1 = NULL;
	if (blend.hasParents && !blend.parentBlendsFrozen && blend.srcParentGeomIdx1 != -1 && blend.srcParentGeomIdx2 != -1)
	{
		blend.reblendMesh = false;

		Dwd* parentDwd1 = g_DwdStore.Get(blend.srcParentGeomIdx1);
		Dwd* parentDwd2 = g_DwdStore.Get(blend.srcParentGeomIdx2);
		if (!parentDwd1 || !parentDwd2)
			return;

		parent1Geom1 = parentDwd1->GetEntry(0);
		parent2Geom1 = parentDwd2->GetEntry(0);
		Assert(parent1Geom1);
		Assert(parent2Geom1);
	}

	rmcDrawable* srcGeom1 = srcDwd1->GetEntry(0);
	rmcDrawable* srcGeom2 = srcDwd2->GetEntry(0);
	rmcDrawable* srcGeom3 = srcDwd3->GetEntry(0);
	rmcDrawable* dstGeom = targetDwd->GetEntry(0);
	rmcDrawable* slotGeom = tempDwd->GetEntry(0);

	Assertf(dstGeom, "Target head geometry not in memory for handle %d", (u32)handle);
	Assertf(slotGeom, "Intermediate target head geometry not in memory for handle %d", (u32)handle);

	if (srcGeom1 && srcGeom2 && dstGeom && slotGeom)
	{
        grmGeometry* destBlend = NULL;

		// if we have parents we do the two generation blends, otherwise fall back to regular
		if (parent1Geom1 && parent2Geom1)
		{
			// first parent
			grmGeometry* parent1Src1 = &parent1Geom1->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			grmGeometry* parent1Src2 = &srcGeom1->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			grmGeometry* parentBlend1 = &slotGeom->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			parentBlend1->BlendPositionsFromOtherGeometries(parent1Src1, parent1Src2, 1.f - blend.parentBlend1, blend.parentBlend1);

			// second parent
			grmGeometry* parent2Src1 = &parent2Geom1->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			grmGeometry* parent2Src2 = &srcGeom2->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			destBlend = &dstGeom->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			destBlend->BlendPositionsFromOtherGeometries(parent2Src1, parent2Src2, 1.f - blend.parentBlend2, blend.parentBlend2);

			// final blend
			destBlend->BlendPositionsFromOtherGeometries(parentBlend1, destBlend, 1.f - blend.geomBlend, blend.geomBlend);
		}
		else
		{
			Assertf(g_DwdStore.GetNumRefs(blend.srcGeomIdx1) > 0, "start - head blend geom 1 refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			Assertf(g_DwdStore.GetNumRefs(blend.srcGeomIdx2) > 0, "start - head blend geom 2 refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			Assertf(g_DwdStore.GetNumRefs(m_slots[type][slotIndex].dwdIndex) > 0, "start - head blend geom dest refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			Assertf(g_DwdStore.GetNumRefs(m_slots[type][m_slots[type].GetCount() - 1].dwdIndex) > 0, "start - head blend geom intermediate refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			grmGeometry* geom1 = &srcGeom1->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			grmGeometry* geom2 = &srcGeom2->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			grmGeometry* blendedGeom = &slotGeom->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			blendedGeom->BlendPositionsFromOtherGeometries(geom1, geom2, 1.f - blend.geomBlend, blend.geomBlend);

			if (srcGeom3)
			{
				Assertf(g_DwdStore.GetNumRefs(blend.srcGeomIdx3) > 0, "start - head blend geom 3 refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
				grmGeometry* geom3 = &srcGeom3->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
				blendedGeom->BlendPositionsFromOtherGeometries(blendedGeom, geom3, 1.f - varBlend, varBlend);
				Assertf(g_DwdStore.GetNumRefs(blend.srcGeomIdx3) > 0, "start - head blend geom 3 refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			}

			destBlend = &dstGeom->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
			destBlend->BlendPositionsFromOtherGeometries(blendedGeom, destBlend, 1.f, 0.f);

			Assertf(g_DwdStore.GetNumRefs(blend.srcGeomIdx1) > 0, "start - head blend geom 1 refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			Assertf(g_DwdStore.GetNumRefs(blend.srcGeomIdx2) > 0, "start - head blend geom 2 refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			Assertf(g_DwdStore.GetNumRefs(m_slots[type][slotIndex].dwdIndex) > 0, "start - head blend geom dest refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
			Assertf(g_DwdStore.GetNumRefs(m_slots[type][m_slots[type].GetCount() - 1].dwdIndex) > 0, "start - head blend geom intermediate refcount <= 0. DON'T CONTINUE, TELL FLAVIUS!");
		}

        // we have are newly blended final head, apply micromorph now
        if (destBlend)
		{
#if __BANK
			if (!ms_bypassMicroMorphs)
#endif // __BANK
			{
				for (s32 i = 0; i < MAX_MICRO_MORPHS; ++i)
				{
					if (blend.microMorphGeom[i].IsInvalid())
						continue;

					if (blend.microMorphAlpha[i] < 0.01f)
						continue;

					rmcDrawable* srcDraw = g_DrawableStore.Get(blend.microMorphGeom[i]);
					Assertf(srcDraw, "Source micro morph geometry not in memory for handle %d and storeIndex %d", (u32)handle, blend.microMorphGeom[i].Get());

					if (srcDraw)
					{
						grmGeometry* srcGeom = &srcDraw->GetLodGroup().GetLod(LOD_HIGH).GetModel(0)->GetGeometry(0);
						destBlend->AddPositionsFromOtherGeometries(srcGeom, blend.microMorphAlpha[i]);
					}
				}
			}
		}
	}

	// make sure we have pointers to the specular and normal maps for the slot drawable we use for rendering
	if (dstGeom && (!m_slots[type][slotIndex].headSpec || !m_slots[type][slotIndex].headNorm))
	{
		m_slots[type][slotIndex].headSpec = GetDrawableTexture(dstGeom, true);
		m_slots[type][slotIndex].headNorm = GetDrawableTexture(dstGeom, false);
	}

	// use gender (set when blend was requested) to decide if we should use dad's or mom's normals, so only do it once
	// dad is always the second head
	bool useDad = blend.male;

	// NOTE: if this is a parent blend it will blend between the beauty and ugly heads so we need to swap spec/norm maps
	// here when we cross the 0.5f threshold
	if (blend.isParent)
	{
		useDad = blend.geomBlend > 0.5f;
	}

	if (blend.copyNormAndSpecTex)
	{
		// find the spec and normal maps for the head
		strLocalIndex drawableIdx = useDad ? blend.srcDrawable2 : blend.srcDrawable1;
		if (drawableIdx.IsValid() && g_DwdStore.Get(drawableIdx))
		{
			rmcDrawable* drawable = g_DwdStore.Get(drawableIdx)->GetEntry(0);
			m_slots[type][slotIndex].headSpecSrc = GetDrawableTexture(drawable, true);
			m_slots[type][slotIndex].headNormSrc = GetDrawableTexture(drawable, false);

			Assertf(m_slots[type][slotIndex].headSpecSrc, "Couldn't find spec tex in head drawable %s!", g_DwdStore.GetName(drawableIdx));
			Assertf(m_slots[type][slotIndex].headNormSrc, "Couldn't find norm tex in head drawable %s!", g_DwdStore.GetName(drawableIdx));

			// trigger an overlay spec/norm reblend since we've switched the base textures
			blend.reblendOverlayMaps = true;
		}

		// copy the specular and normal map from the dominant parent to the slot we use for rendering
		if (m_slots[type][slotIndex].headSpecSrc && m_slots[type][slotIndex].headNormSrc)
		{
			if (Verifyf(m_slots[type][slotIndex].headSpec, "Target slot head specular is not in memory!"))
				CloneTexture(m_slots[type][slotIndex].headSpecSrc, m_slots[type][slotIndex].headSpec);

			if (Verifyf(m_slots[type][slotIndex].headNorm, "Target slot head normal is not in memory!"))
				CloneTexture(m_slots[type][slotIndex].headNormSrc, m_slots[type][slotIndex].headNorm);
		}

		blend.copyNormAndSpecTex = false;
	}

	Assert(m_slots[type][slotIndex].headSpec);
	Assert(m_slots[type][slotIndex].headNorm);
	Assertf(m_slots[type][slotIndex].headSpecSrc, "No headSpecSrc tex found, please break and tell Flavius!");
	Assertf(m_slots[type][slotIndex].headNormSrc, "No headNormSrc tex found, please break and tell Flavius!");

	blend.overlayCompSpec = false;
	blend.reblendMesh = false;
}

void MeshBlendManager::ExtractGeometryTextures(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

	if (blend.deleteFrame > -1)
		return;

	s32 slotIndex = blend.slotIndex;
	if (slotIndex < 0)
		return;

	if (m_slots[type][slotIndex].dwdIndex == -1)
		return;

	Dwd* targetDwd = g_DwdStore.Get(m_slots[type][slotIndex].dwdIndex);
	if (!targetDwd)
		return;

	rmcDrawable* dstGeom = targetDwd->GetEntry(0);

	Assertf(dstGeom, "Target head geometry not in memory for handle %d", (u32)handle);

	// make sure we have pointers to the specular and normal maps for the slot drawable we use for rendering
	if (dstGeom && (!m_slots[type][slotIndex].headSpec || !m_slots[type][slotIndex].headNorm))
	{
		m_slots[type][slotIndex].headSpec = GetDrawableTexture(dstGeom, true);
		m_slots[type][slotIndex].headNorm = GetDrawableTexture(dstGeom, false);
	}

	// use gender (set when blend was requested) to decide if we should use dad's or mom's normals, so only do it once
	// dad is always the second head
	bool useDad = blend.male;

	// NOTE: if this is a parent blend it will blend between the beauty and ugly heads so we need to swap spec/norm maps
	// here when we cross the 0.5f threshold
	if (blend.isParent)
	{
		useDad = blend.geomBlend > 0.5f;
	}

	if (blend.copyNormAndSpecTex)
	{
		// find the spec and normal maps for the head
		strLocalIndex drawableIdx = useDad ? blend.srcDrawable2 : blend.srcDrawable1;
		if (drawableIdx.IsValid() && g_DwdStore.Get(drawableIdx))
		{
			rmcDrawable* drawable = g_DwdStore.Get(drawableIdx)->GetEntry(0);
			m_slots[type][slotIndex].headSpecSrc = GetDrawableTexture(drawable, true);
			m_slots[type][slotIndex].headNormSrc = GetDrawableTexture(drawable, false);

			Assertf(m_slots[type][slotIndex].headSpecSrc, "Couldn't find spec tex in head drawable %s!", g_DwdStore.GetName(drawableIdx));
			Assertf(m_slots[type][slotIndex].headNormSrc, "Couldn't find norm tex in head drawable %s!", g_DwdStore.GetName(drawableIdx));

			// trigger an overlay spec/norm reblend since we've switched the base textures
			blend.reblendOverlayMaps = true;
		}

		// copy the specular and normal map from the dominant parent to the slot we use for rendering
		if (m_slots[type][slotIndex].headSpecSrc && m_slots[type][slotIndex].headNormSrc)
		{
			if (Verifyf(m_slots[type][slotIndex].headSpec, "Target slot head specular is not in memory!"))
				CloneTexture(m_slots[type][slotIndex].headSpecSrc, m_slots[type][slotIndex].headSpec);

			if (Verifyf(m_slots[type][slotIndex].headNorm, "Target slot head normal is not in memory!"))
				CloneTexture(m_slots[type][slotIndex].headNormSrc, m_slots[type][slotIndex].headNorm);
		}

		blend.copyNormAndSpecTex = false;
	}

	Assert(m_slots[type][slotIndex].headSpec);
	Assert(m_slots[type][slotIndex].headNorm);
	Assertf(m_slots[type][slotIndex].headSpecSrc, "No headSpecSrc tex found, please break and tell Flavius!");
	Assertf(m_slots[type][slotIndex].headNormSrc, "No headNormSrc tex found, please break and tell Flavius!");

	blend.overlayCompSpec = false;
}

s32 MeshBlendManager::GetBlendTexture(MeshBlendHandle handle, bool mustBeLoaded, bool mustBeBlended)
{
	if (!IsHandleValid(handle))
		return -1;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	if (mustBeBlended && (sysInterlockedRead((unsigned*)&m_blends[type][index].state) != STATE_RENDERING || m_blends[type][index].reblend))
	{
		return -1;
	}

	if (mustBeLoaded && sysInterlockedRead((unsigned*)&m_blends[type][index].state) <= STATE_REQUEST_SLOT)
	{
		return -1;
	}

	u32 slotIndex = m_blends[type][index].slotIndex;
	return m_slots[type][slotIndex].txdIndex.Get();
}

s32 MeshBlendManager::GetBlendDrawable(MeshBlendHandle handle, bool mustBeLoaded, bool mustBeBlended)
{
	if (!IsHandleValid(handle))
		return -1;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	if (mustBeBlended && (sysInterlockedRead((unsigned*)&m_blends[type][index].state) != STATE_RENDERING || m_blends[type][index].reblend))
	{
		return -1;
	}

	if (mustBeLoaded && sysInterlockedRead((unsigned*)&m_blends[type][index].state) <= STATE_REQUEST_SLOT)
	{
		return -1;
	}

	u32 slotIndex = m_blends[type][index].slotIndex;
	return m_slots[type][slotIndex].dwdIndex.Get();
}

s32 MeshBlendManager::GetBlendSkinTexture(MeshBlendHandle handle, eSkinBlendComp slot, bool mustBeLoaded, bool mustBeBlended, s32& dwdIndex)
{
	dwdIndex = -1;

	if (!IsHandleValid(handle))
		return -1;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	if (!m_blends[type][index].skinBlend[slot].used)
		return -1;

	if (mustBeBlended && (sysInterlockedRead((unsigned*)&m_blends[type][index].state) != STATE_RENDERING || m_blends[type][index].reblend))
	{
		return -1;
	}

	if (mustBeLoaded && sysInterlockedRead((unsigned*)&m_blends[type][index].state) <= STATE_REQUEST_SLOT)
	{
		return -1;
	}

	u32 slotIndex = m_blends[type][index].slotIndex;
	if (!Verifyf((m_slots[type][slotIndex].addedSkinCompRefs & (1 << slot)) != 0, "No ref added to skin component which we regard as loaded now!"))
		return -1;

	dwdIndex = m_blends[type][index].skinBlend[slot].skinMaskParent;
	return m_slots[type][slotIndex].skinCompIndex[slot].Get();
}

bool MeshBlendManager::HasBlendFinished(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return true;

#if RSG_PC
	// Wait for any compression work that's still in flight. Really we only need to wait if this particular blend is in flight, so
	// we're being overly cautious here but I think that should still be OK. 
	if ( IsDxtCompressorReserved() )
	{
		return false;
	}
#endif

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

    if (m_blends[type][index].state == STATE_NOT_USED)
        return true;

    if (m_blends[type][index].hasReqs)
        return false;

	sysCriticalSection cs(s_assetToken);

	bool notFinished = m_blends[type][index].state != STATE_RENDERING;
	notFinished |= m_blends[type][index].reblend;
	notFinished |= m_blends[type][index].reblendMesh;
	notFinished |= m_blends[type][index].reblendOverlayMaps;
	notFinished |= m_blends[type][index].reblendSkin[SBC_FEET];
	notFinished |= m_blends[type][index].reblendSkin[SBC_UPPR];
	notFinished |= m_blends[type][index].reblendSkin[SBC_LOWR];

	return !notFinished;
}

bool MeshBlendManager::IsBlendFinalized(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return true;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

    return blend.finalizeFrame == IS_FINALIZED;
}

void MeshBlendManager::SetRequestsInProgress(MeshBlendHandle handle, bool val)
{
    if (!IsHandleValid(handle))
        return;

    u16 type = GetTypeFromHandle(handle);
    u16 index = GetIndexFromHandle(handle);

    m_blends[type][index].hasReqs = val;
}

void MeshBlendManager::FinalizeHeadBlend(MeshBlendHandle handle, bool delayed)
{
	if (!IsHandleValid(handle))
	{
		pedDebugf1("[HEAD BLEND] MeshBlendManager::FinalizeHeadBlend(%d): Invalid handle", handle);
		return;
	}

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	sBlendEntry& blend = m_blends[type][index];

	if (blend.state == STATE_NOT_USED)
	{
		pedDebugf1("[HEAD BLEND] MeshBlendManager::FinalizeHeadBlend(%d): Blend not used", handle);
		return;
	}

	if (delayed)
	{
		pedDebugf1("[HEAD BLEND] MeshBlendManager::FinalizeHeadBlend(%d): Delaying finalize while in state %d (finalizeFrame=%d)",
			handle, blend.state, blend.finalizeFrame);
		if (PARAM_headblenddebug.Get())
		{
			sysStack::PrintStackTrace();
		}
		blend.finalizeFrame = DELAYED_FINALIZE_FRAMES;
		return;
	}

	pedDebugf1("[HEAD BLEND] FinalizeHeadBlend: Finalized blend handle 0x%08x - parents set to -1 while in state %d", handle, blend.state);
	ReplaceAssets(blend, true);

	// release micro morph assets
	for (s32 i = 0; i < MAX_MICRO_MORPHS; ++i)
	{
		if (blend.microMorphGeom[i].IsInvalid())
			continue;

		g_DrawableStore.RemoveRef(blend.microMorphGeom[i], REF_OTHER);
		blend.microMorphGeom[i].Invalidate();
	}

    blend.finalizeFrame = IS_FINALIZED;
}

void MeshBlendManager::RegisterSlot(eMeshBlendTypes type, s32 dwdIndex, s32 txdIndex, s32 feetTxdIndex, s32 upprTxdIndex, s32 lowrTxdIndex)
{
    if (!m_bCanRegisterSlots)
        return;

	if (m_slots[type].GetCount() >= m_slots[type].GetMaxCount())
	{
		return;
	}

	sBlendSlot newSlot;
	newSlot.dwdIndex = dwdIndex;
	newSlot.txdIndex = txdIndex;
	newSlot.skinCompIndex[SBC_FEET] = feetTxdIndex;
	newSlot.skinCompIndex[SBC_UPPR] = upprTxdIndex;
	newSlot.skinCompIndex[SBC_LOWR] = lowrTxdIndex;

	Assertf(dwdIndex != -1, "Bad mesh blend target geometry! Type: %d", type);
	Assertf(txdIndex != -1, "Bad mesh blend target texture! Type: %d", type);
	Assertf(feetTxdIndex != -1, "Bad mesh blend target feet texture! Type: %d", type);
	Assertf(lowrTxdIndex != -1, "Bad mesh blend target lowr texture! Type: %d", type);
	Assertf(upprTxdIndex != -1, "Bad mesh blend target uppr texture! Type: %d", type);

	m_slots[type].Push(newSlot);
}

MeshBlendHandle MeshBlendManager::RequestNewBlend(eMeshBlendTypes type, CPed* targetPed, s32 srcGeom1, s32 srcTex1, s32 srcGeom2, s32 srcTex2, s32 srcGeom3, s32 srcTex3, float geomBlend, float texBlend, float varBlend, bool async, bool male, bool parent)
{
	if (srcGeom1 == -1 || srcTex1 == -1 || srcGeom2 == -1 || srcTex2 == -1 || srcGeom3 == -1 || srcTex3 == -1)
	{
		Assertf(false, "MeshBlendManager: Invalid data passed to RequestNewBlend! geom1 %d, tex1 %d, geom2 %d, tex2 %d, geom3 %d, tex3 %d",
			srcGeom1, srcTex1, srcGeom2, srcTex2, srcGeom3, srcTex3);
		return MESHBLEND_HANDLE_INVALID;
	}

	const grcTexture* tex1 = g_TxdStore.Get(strLocalIndex(srcTex1))->GetEntry(0);
	const grcTexture* tex2 = g_TxdStore.Get(strLocalIndex(srcTex2))->GetEntry(0);
	const grcTexture* tex3 = g_TxdStore.Get(strLocalIndex(srcTex3))->GetEntry(0);

	const rmcDrawable* geom1 = g_DwdStore.Get(strLocalIndex(srcGeom1))->GetEntry(0);
	const rmcDrawable* geom2 = g_DwdStore.Get(strLocalIndex(srcGeom2))->GetEntry(0);
	const rmcDrawable* geom3 = g_DwdStore.Get(strLocalIndex(srcGeom3))->GetEntry(0);

	if (!geom1 || !tex1 || !geom2 || !tex2 || !geom3 || !tex3)
	{
		Assertf(false, "MeshBlendManager: Missing assets in RequestNewBlend! geom1 %p, tex1 %p, geom2 %p, tex2 %p, geom3 %p, tex3 %p",
			geom1, tex1, geom2, tex2, geom3, tex3);
		return MESHBLEND_HANDLE_INVALID;
	}

	// find first empty slot, ignore last slot which is used for intermediate storage of mesh blending
	s32 slotIndex = -1;
	for (s32 i = 0; i < m_slots[type].GetCount() - 1; ++i)
	{
		if (!m_slots[type][i].inUse)
		{
			slotIndex = i;
			break;
		}
	}

	if (!Verifyf(slotIndex != -1, "MeshBlendManager: Failed to find available blend slot!"))
	{
		DumpSlotDebug();
		return MESHBLEND_HANDLE_INVALID;
	}

	for (s32 i = 0; i < SBC_MAX; ++i)
		m_slots[type][slotIndex].skinCompReq[i] = -1;

	// find two available streaming slots for head data and one for each skin component (hands, uppr, lowr...),
	// use last request index for the temp slot
	s32 dwdReqIndex = -1;
	s32 txdReqIndex = -1;
	for (s32 i = 0; i < MAX_ACTIVE_REQUESTS - 1; ++i)
	{
		if (!m_reqs[i].IsValid())
		{
			if (dwdReqIndex == -1)
				dwdReqIndex = i;
			else if (txdReqIndex == -1)
				txdReqIndex = i;
			else
			{
				for (s32 f = 0; f < SBC_MAX; ++f)
				{
					if (m_slots[type][slotIndex].skinCompReq[f] == -1)
					{
						m_slots[type][slotIndex].skinCompReq[f] = i;
						break;
					}
				}
			}

			// break when our last item has had a request index assigned
			if (m_slots[type][slotIndex].skinCompReq[SBC_MAX - 1] != -1)
				break;
		}
	}

	// just check last item for success
	if (!Verifyf(m_slots[type][slotIndex].skinCompReq[SBC_MAX- 1] != -1, "MeshBlendManager: Failed to find available slots for blend assets!"))
		return MESHBLEND_HANDLE_INVALID;

	m_slots[type][slotIndex].inUse = true;

#if !__FINAL
	Displayf("[MESHBLEND] allocate at [%d][%d]", type, slotIndex);
	ms_blendSlotBacktraces[type][slotIndex] =  sysStack::RegisterBacktrace();
#endif //!__FINAL

	// mark the assets as dirty, don't want them to suddenly switch place in memory
	CStreaming::SetDoNotDefrag(GetInstance().m_slots[type][slotIndex].txdIndex, g_TxdStore.GetStreamingModuleId());

	m_reqs[dwdReqIndex].Request(m_slots[type][slotIndex].dwdIndex, g_DwdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);
	m_reqs[txdReqIndex].Request(m_slots[type][slotIndex].txdIndex, g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);
	m_slots[type][slotIndex].dwdReq = dwdReqIndex;
	m_slots[type][slotIndex].txdReq = txdReqIndex;
	
	for (s32 i = 0; i < SBC_MAX; ++i)
	{
		m_reqs[m_slots[type][slotIndex].skinCompReq[i]].Request(m_slots[type][slotIndex].skinCompIndex[i], g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);
		CStreaming::SetDoNotDefrag(m_slots[type][slotIndex].skinCompIndex[i], g_TxdStore.GetStreamingModuleId());
	}

	// request last slot to store temp geometry in
	if (ms_intermediateSlotRefCount == 0)
	{
		Assertf(!m_reqs[MAX_ACTIVE_REQUESTS - 1].IsValid(), "Intermediate slot request is valid but has no ref counts!");
		m_reqs[MAX_ACTIVE_REQUESTS - 1].Request(m_slots[type][m_slots[type].GetCount() - 1].dwdIndex, g_DwdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);
	}
	ms_intermediateSlotRefCount++;

	// grab lock so we the render thread doesn't start using this blend during this function
	sysCriticalSection cs(s_assetToken); 

	// try to find an empty blend entry
	sBlendEntry* newEntry = NULL;
	s32 newIndex = -1;
	for (s32 i = 0; i < m_blends[type].GetCount(); ++i)
	{
		if (sysInterlockedRead((unsigned*)&m_blends[type][i].state) == STATE_NOT_USED)
		{
			newEntry = &m_blends[type][i];
			newIndex = i;
			break;
		}
	}

	if (!newEntry)
	{
		if (m_blends[type].GetCount() >= m_blends[type].GetMaxCount())
		{
			Assertf(false, "No blend slots left. Why do we need more than %d?", MAX_NUM_BLENDS);
			return MESHBLEND_HANDLE_INVALID;
		}

		newEntry = &m_blends[type].Append();
		newIndex = m_blends[type].GetCount() - 1;
	}

	for (s32 i = 0; i < SBC_MAX; ++i)
	{
		newEntry->skinBlend[i].clothTex = -1;
		newEntry->skinBlend[i].srcSkin0 = -1;
		newEntry->skinBlend[i].srcSkin1 = -1;
		newEntry->skinBlend[i].srcSkin2 = -1;
		newEntry->skinBlend[i].skinMaskParent = -1;
		newEntry->skinBlend[i].used = false;
	}

	g_TxdStore.AddRef(strLocalIndex(srcTex1), REF_OTHER);
	g_TxdStore.AddRef(strLocalIndex(srcTex2), REF_OTHER);
	g_TxdStore.AddRef(strLocalIndex(srcTex3), REF_OTHER);

	g_DwdStore.AddRef(strLocalIndex(srcGeom1), REF_OTHER);
	g_DwdStore.AddRef(strLocalIndex(srcGeom2), REF_OTHER);
	g_DwdStore.AddRef(strLocalIndex(srcGeom3), REF_OTHER);

	newEntry->srcTexIdx1 = srcTex1;
	newEntry->srcTexIdx2 = srcTex2;
	newEntry->srcTexIdx3 = srcTex3;
	newEntry->srcGeomIdx1 = srcGeom1;
	newEntry->srcGeomIdx2 = srcGeom2;
	newEntry->srcGeomIdx3 = srcGeom3;
	
	newEntry->type = type;
	newEntry->slotIndex = slotIndex;
	newEntry->skinCompState = SBC_FEET;
	newEntry->overlayCompSpec = false;
	Assert(!newEntry->addedRefs);
	newEntry->addedRefs = false;
	newEntry->refCount = 1;
	newEntry->async = async;
	newEntry->male = male;
	newEntry->finalizeFrame = -1;
	newEntry->copyNormAndSpecTex = true;
	newEntry->isParent = parent;
	newEntry->hasParents = false;
	newEntry->parentBlendsFrozen = false;

    newEntry->paletteIndex = m_palette.GetFreeSlot();

	newEntry->clones.PushAndGrow(RegdPed(targetPed), 5);

	Assert(newIndex > -1);
	MeshBlendHandle handle = type << 16 | newIndex;
	SetNewBlendValues(handle, geomBlend, texBlend, varBlend);
	pedDebugf1("[HEAD BLEND] RequestNewBlend: Allocated blend handle 0x%08x for ped %p: Geo(%d, %d, %d, %f), Tex(%d (%s), %d (%s), %d (%s), %f), varBlend %f, %s, %s, %s",
		handle, targetPed, srcGeom1, srcGeom2, srcGeom3, geomBlend, srcTex1, tex1->GetName(), srcTex2, tex2->GetName(), srcTex3, tex3->GetName(),
		texBlend, varBlend, async?"async":"notAsync", male?"male":"notMale", parent?"parent":"notParent");
#if __BANK
	ActiveBlendTypeChangedCB();
#endif

	sysInterlockedExchange((unsigned*)&newEntry->state, STATE_REQUEST_SLOT);
	return handle;
}

// There are two modes to the blends:
// 1. normal mode where a blend is requested with data for two parents. Here we use this data to blend two sets
// of assets into a final set which is used on the player ped.
// 2. parent mode where the data from above and the resulting ped is used as a parent. In this function data for a second parent
// is provided. This way we end up with two blended peds which we use to blend a third ped, the final player ped.
// In essence this gives us "grandparents" and 2 generations of blends.
bool MeshBlendManager::SetParentBlendData(MeshBlendHandle handle, s32 srcParentGeom1, s32 srcParentGeom2, float parentBlend1, float parentBlend2)
{
	if (!IsHandleValid(handle))
		return false;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blendEntry = m_blends[type][index];

    parentBlend1 = rage::Max(rage::Min(parentBlend1, 1.f), 0.f);
    parentBlend2 = rage::Max(rage::Min(parentBlend2, 1.f), 0.f);

	// early out if we're setting same parent settings
	if (blendEntry.srcParentGeomIdx1 == srcParentGeom1 && blendEntry.srcParentGeomIdx2 == srcParentGeom2 &&
		fabs(blendEntry.parentBlend1 - parentBlend1) < 0.0001f && fabs(blendEntry.parentBlend2 - parentBlend2) < 0.0001f)
		return true;

	const rmcDrawable* geom1 = g_DwdStore.Get(strLocalIndex(srcParentGeom1))->GetEntry(0);
	const rmcDrawable* geom2 = g_DwdStore.Get(strLocalIndex(srcParentGeom2))->GetEntry(0);

	if (!geom1 || !geom2)
		return false;

	sysCriticalSection cs(s_assetToken);

	bool stillRequesting = sysInterlockedRead((unsigned*)&blendEntry.state) == STATE_REQUEST_SLOT;
	bool stillWaitingForMem = sysInterlockedRead((unsigned*)&blendEntry.state) == STATE_INIT_RT;

	g_DwdStore.AddRef(strLocalIndex(srcParentGeom1), REF_OTHER);
	g_DwdStore.AddRef(strLocalIndex(srcParentGeom2), REF_OTHER);

    if (blendEntry.srcParentGeomIdx1 != -1)
        g_DwdStore.RemoveRef(blendEntry.srcParentGeomIdx1, REF_OTHER);
    if (blendEntry.srcParentGeomIdx2 != -1)
        g_DwdStore.RemoveRef(blendEntry.srcParentGeomIdx2, REF_OTHER);

	blendEntry.srcParentGeomIdx1 = srcParentGeom1;
	blendEntry.srcParentGeomIdx2 = srcParentGeom2;

	blendEntry.parentBlend1 = parentBlend1;
	blendEntry.parentBlend2 = parentBlend2;

	// flag for reblend
	blendEntry.reblendMesh = true;
	//blendEntry.reblend = true;
	//blendEntry.reblendSkin[SBC_FEET] = true;
	//blendEntry.reblendSkin[SBC_UPPR] = true;
	//blendEntry.reblendSkin[SBC_LOWR] = true;

	//// mark skin textures as inappropriate until they're reblended
	//blendEntry.skinBlend[SBC_FEET].used = false;
	//blendEntry.skinBlend[SBC_UPPR].used = false;
	//blendEntry.skinBlend[SBC_LOWR].used = false;

	// unfinalize this blend
	blendEntry.finalizeFrame = -1;

	blendEntry.hasParents = true;
	blendEntry.parentBlendsFrozen = false;

	if (!Verifyf(!blendEntry.isParent, "Parent blend is assigned parents, we don't support this!"))
		blendEntry.isParent = false;

#if __BANK
	ActiveBlendTypeChangedCB();
#endif

	if (stillWaitingForMem)
		sysInterlockedExchange((unsigned*)&blendEntry.state, STATE_INIT_RT);
	else
		sysInterlockedExchange((unsigned*)&blendEntry.state, stillRequesting ? STATE_REQUEST_SLOT : STATE_RENDERING);
	pedDebugf1("[HEAD BLEND] SetParentBlendData: Blend handle 0x%08x parents = %d, %d", handle,
		srcParentGeom1, srcParentGeom2);
	return true;
}

bool MeshBlendManager::UpdateBlend(MeshBlendHandle handle, s32 srcGeom1, s32 srcTex1, s32 srcGeom2, s32 srcTex2, s32 srcGeom3, s32 srcTex3, float geomBlend, float texBlend, float varBlend)
{
	if (!IsHandleValid(handle))
		return false;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

	if (!Verifyf(blend.slotIndex != -1, "Trying to update blend with bad slot index!") ||
		!Verifyf(m_slots[type][blend.slotIndex].inUse, "Trying to update blend with slot flagged as unused!"))
	{
		return false;
	}

	if (srcGeom1 == -1 || srcTex1 == -1 || srcGeom2 == -1 || srcTex2 == -1 || srcGeom3 == -1 || srcTex3 == -1)
	{
		pedWarningf("[HEAD BLEND] MeshBlendManager::UpdateBlend(0x%08x, %d, %d, %d, %d, %d, %d, %f, %f, %f): Invalid data passed",
			handle, srcGeom1, srcTex1, srcGeom2, srcTex2, srcGeom3, srcTex3, geomBlend, texBlend, varBlend);
		return false;
	}

	// early out of we're setting the same settings
	sBlendEntry& blendEntry = blend;
	if (blendEntry.srcGeomIdx1 == srcGeom1 && blendEntry.srcGeomIdx2 == srcGeom2 && blendEntry.srcGeomIdx3 == srcGeom3 &&
		blendEntry.srcTexIdx1 == srcTex1 && blendEntry.srcTexIdx2 == srcTex2 && blendEntry.srcTexIdx3 == srcTex3 &&
		fabs(blendEntry.geomBlend - geomBlend) < 0.0001f && fabs(blendEntry.texBlend - texBlend) < 0.0001f && fabs(blendEntry.varBlend - varBlend) < 0.0001f)
	{
		pedDebugf2("[HEAD BLEND] MeshBlendManager::UpdateBlend(0x%08x): No-op, bailing", handle);
		return true;
	}

	if (PARAM_headblenddebug.Get())
	{
		pedWarningf("[HEAD BLEND] MeshBlendManager::UpdateBlend(0x%08x, %d, %d, %d, %d, %d, %d, %f, %f, %f): Old data: %d, %d, %d, %d, %d, %d, %f, %f, %f",
			handle, srcGeom1, srcTex1, srcGeom2, srcTex2, srcGeom3, srcTex3, geomBlend, texBlend, varBlend, blendEntry.srcGeomIdx1.Get(),
			blendEntry.srcGeomIdx2.Get(), blendEntry.srcGeomIdx3.Get(), blendEntry.srcTexIdx1.Get(), blendEntry.srcTexIdx2.Get(), blendEntry.srcTexIdx3.Get(),
			blendEntry.geomBlend, blendEntry.texBlend, blendEntry.varBlend);
	}

#if !__NO_OUTPUT
	if(blend.finalizeFrame == IS_FINALIZED)
	{
        Displayf("[HEAD BLEND] Trying to modify finalized head blend 0x%08x!", handle);
		#if __BANK
			sysStack::PrintStackTrace();
			scrThread::PrePrintStackTrace();
		#endif
	}
#endif

	const grcTexture* tex1 = g_TxdStore.Get(strLocalIndex(srcTex1))->GetEntry(0);
	const grcTexture* tex2 = g_TxdStore.Get(strLocalIndex(srcTex2))->GetEntry(0);
	const grcTexture* tex3 = g_TxdStore.Get(strLocalIndex(srcTex3))->GetEntry(0);

	const rmcDrawable* geom1 = g_DwdStore.Get(strLocalIndex(srcGeom1))->GetEntry(0);
	const rmcDrawable* geom2 = g_DwdStore.Get(strLocalIndex(srcGeom2))->GetEntry(0);
	const rmcDrawable* geom3 = g_DwdStore.Get(strLocalIndex(srcGeom3))->GetEntry(0);

	if (!geom1 || !tex1 || !geom2 || !tex2 || !geom3 || !tex3)
	{
		pedDebugf2("[HEAD BLEND] MeshBlendManager::UpdateBlend(0x%08x): Failed to look up one or more assets! %d=%p, %d=%p, %d=%p, %d=%p, %d=%p, %d=%p",
			handle, srcTex1, tex1, srcTex2, tex2, srcTex3, tex3, srcGeom1, geom1, srcGeom2, geom2, srcGeom3, geom3);
		return true;
	}

	sysCriticalSection cs(s_assetToken);

	bool stillRequesting = sysInterlockedRead((unsigned*)&blend.state) == STATE_REQUEST_SLOT;
	bool stillWaitingForMem = sysInterlockedRead((unsigned*)&blend.state) == STATE_INIT_RT;

	g_TxdStore.AddRef(strLocalIndex(srcTex1), REF_OTHER);
	g_TxdStore.AddRef(strLocalIndex(srcTex2), REF_OTHER);
	g_TxdStore.AddRef(strLocalIndex(srcTex3), REF_OTHER);

	g_DwdStore.AddRef(strLocalIndex(srcGeom1), REF_OTHER);
	g_DwdStore.AddRef(strLocalIndex(srcGeom2), REF_OTHER);
	g_DwdStore.AddRef(strLocalIndex(srcGeom3), REF_OTHER);

	// release refs on old assets and swap
	ReplaceAssets(blend, false, srcTex1, srcTex2, srcTex3, srcGeom1, srcGeom2, srcGeom3, false);

	blend.skinCompState = SBC_FEET;
	blend.overlayCompSpec = false;

	SetNewBlendValues(handle, geomBlend, texBlend, varBlend);

	// force blends since values might match but the assets are different
	if (PARAM_headblenddebug.Get())
		pedWarningf("[HEAD BLEND] MeshBlendManager::UpdateBlend(0x%08x): reblend=true", handle);
	blend.reblendMesh = true;
	blend.reblend = true;
	blend.reblendSkin[SBC_FEET] = true;
	blend.reblendSkin[SBC_UPPR] = true;
	blend.reblendSkin[SBC_LOWR] = true;

	blend.finalizeFrame = -1;

#if __BANK
	ActiveBlendTypeChangedCB();
#endif

	if (stillWaitingForMem)
		sysInterlockedExchange((unsigned*)&blend.state, STATE_INIT_RT);
	else
		sysInterlockedExchange((unsigned*)&blend.state, stillRequesting ? STATE_REQUEST_SLOT : STATE_RENDERING);
	return true;
}

bool MeshBlendManager::RequestNewOverlay(MeshBlendHandle handle, u32 slot, strLocalIndex txdIndex, eOverlayBlendType blendType, float blendAlpha, float normBlend)
{
	if (!IsHandleValid(handle))
		return false;

	if (slot >= MAX_OVERLAYS)
		return false;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];
	sBlendSlot& blendSlot = m_slots[type][blend.slotIndex];

	sysCriticalSection cs(s_assetToken);

	if (PARAM_headblenddebug.Get())
	{
		pedWarningf("[HEAD BLEND] MeshBlendManager::RequestNewOverlay(0x%08x, %d, %d, %d, %f, %f)",
			handle, slot, txdIndex.Get(), blendType, blendAlpha, normBlend);
	}

#if !__NO_OUTPUT
	if(blend.finalizeFrame == IS_FINALIZED)
	{
        Displayf("[HEAD BLEND] Trying to modify finalized head blend 0x%08x!", handle);
		#if __BANK
			sysStack::PrintStackTrace();
			scrThread::PrePrintStackTrace();
		#endif
	}
#endif

	if (blendSlot.overlayIndex[slot] == txdIndex.Get())
    {
		bool needAssets = false;
        if (fabs(blend.overlayAlpha[slot] - blendAlpha) > 0.0001f ||
            fabs(blend.overlayNormAlpha[slot] - normBlend) > 0.0001f)
        {
			blend.overlayAlpha[slot] = blendAlpha;
			blend.overlayNormAlpha[slot] = normBlend;

			if (slot < MAX_OVERLAYS - NUM_BODY_OVERLAYS)
			{
				blend.reblendOverlayMaps = true;
				blend.reblend = true;
			}
			else
			{
				blend.reblendSkin[SBC_UPPR] = true;
			}

			needAssets = true;
        }

		// don't return in case we need assets and we have no refs (assets have been ejected from memory)
		if (!needAssets || (blendSlot.addedOverlayRefs & (1 << slot)) != 0)
			return true;
    }

	strLocalIndex txdReqIndex = strLocalIndex(-1);
	for (s32 i = 0; i < MAX_ACTIVE_REQUESTS - 1; ++i)
	{
		if (!m_reqs[i].IsValid())
		{
			txdReqIndex = i;
			break;
		}
	}

	if (txdReqIndex == -1)
		return false;

	m_reqs[txdReqIndex.Get()].Request(txdIndex, g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);

	// release any request we happen to have for this overlay slot and the txd reference
	s32 curReq = blendSlot.overlayReq[slot];
	strLocalIndex curIndex = strLocalIndex(blendSlot.overlayIndex[slot]);
	if (curReq != -1)
	{
		m_reqs[curReq].Release();
		if (curIndex != -1)
			g_TxdStore.ClearRequiredFlag(curIndex.Get(), STRFLAG_DONTDELETE);
	}
	if (curIndex != -1 && ((blendSlot.addedOverlayRefs & (1 << slot)) != 0))
		g_TxdStore.RemoveRef(curIndex, REF_OTHER);

	blendSlot.addedOverlayRefs &= ~(1 << slot);

	blendSlot.overlayReq[slot] = txdReqIndex.Get();
	blendSlot.overlayIndex[slot] = txdIndex.Get();

	blend.blendType[slot] = blendType;
	blend.overlayAlpha[slot] = blendAlpha;
	blend.overlayNormAlpha[slot] = normBlend;

	// it's too early to trigger the blends here, do it once the overlay asset has streamed in instead
	//m_blends[type][index].reblendOverlayMaps = true;
	//m_blends[type][index].reblend = true;

	blend.finalizeFrame = -1;

	return true;
}

bool MeshBlendManager::UpdateOverlayBlend(MeshBlendHandle handle, u32 slot, float blendAlpha, float normBlend)
{
	if (!IsHandleValid(handle))
		return false;

	if (slot >= MAX_OVERLAYS)
		return false;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];
	sBlendSlot& blendSlot = m_slots[type][blend.slotIndex];

	sysCriticalSection cs(s_assetToken);

	if (fabs(blend.overlayAlpha[slot] - blendAlpha) < 0.0001f &&
		fabs(blend.overlayNormAlpha[slot] - normBlend) < 0.0001f)
	{
		return true;
	}

	blend.overlayAlpha[slot] = blendAlpha;
	blend.overlayNormAlpha[slot] = normBlend;

	if (!Verifyf((blendSlot.addedOverlayRefs & (1 << slot)) != 0, "UpdateOverlayBlend: Updating an overlay with no refs!"))
		return false;

	if (slot < MAX_OVERLAYS - NUM_BODY_OVERLAYS)
	{
		blend.reblendOverlayMaps = true;
		blend.reblend = true;
	}
	else
	{
		blend.reblendSkin[SBC_UPPR] = true;
	}

	blend.finalizeFrame = -1;

	return true;
}

void MeshBlendManager::ResetOverlay(MeshBlendHandle handle, u32 slot)
{
	if (!IsHandleValid(handle))
		return;

	if (slot >= MAX_OVERLAYS)
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];
	sBlendSlot& blendSlot = m_slots[type][blend.slotIndex];;

	s32 curReq = blendSlot.overlayReq[slot];
	strLocalIndex curIndex = strLocalIndex(blendSlot.overlayIndex[slot]);

	if (PARAM_headblenddebug.Get())
		pedWarningf("[HEAD BLEND] MeshBlendManager::ResetOverlay(0x%08x, %d)", handle, slot);

    // this overlay is already reset, bail, we save an unnecessary blend this way
    if (curReq == -1 && curIndex == -1 && (blendSlot.addedOverlayRefs & (1 << slot)) == 0)
	{
		if (PARAM_headblenddebug.Get())
			pedWarningf("[HEAD BLEND] MeshBlendManager::ResetOverlay(0x%08x): No change, early out", handle);
        return;
	}

#if !__NO_OUTPUT
	if(blend.finalizeFrame == IS_FINALIZED)
	{
        Displayf("[HEAD BLEND] Trying to modify finalized head blend 0x%08x!", handle);
		#if __BANK
			sysStack::PrintStackTrace();
			scrThread::PrePrintStackTrace();
		#endif
	}
#endif

	sysCriticalSection cs(s_assetToken);

	blend.overlayAlpha[slot] = 1.f;
	blend.overlayNormAlpha[slot] = 1.f;
	blend.overlayTintIndex[slot] = 0;
	blend.overlayTintIndex2[slot] = 0;
	blend.overlayRampType[slot] = RT_NONE;

	// release any request we happen to have for this overlay slot and the txd reference
	if (curReq != -1)
	{
		m_reqs[curReq].Release();
		if (curIndex != -1)
			g_TxdStore.ClearRequiredFlag(curIndex.Get(), STRFLAG_DONTDELETE);
	}
	if (curIndex != -1 && ((blendSlot.addedOverlayRefs & (1 << slot)) != 0))
		g_TxdStore.RemoveRef(curIndex, REF_OTHER);

	blendSlot.addedOverlayRefs &= ~(1 << slot);

	blendSlot.overlayReq[slot] = -1;
	blendSlot.overlayIndex[slot] = -1;

    // trigger reblend to remove any overlays we might have had
	if (slot < MAX_OVERLAYS - NUM_BODY_OVERLAYS)
	{
		blend.reblendOverlayMaps = true;
		blend.reblend = true;
	}
	else
	{
		blend.reblendSkin[SBC_UPPR] = true;
	}
}

void MeshBlendManager::SetOverlayTintIndex(MeshBlendHandle handle, u32 slot, eRampType rampType, u8 tint, u8 tint2)
{
	if (!IsHandleValid(handle))
		return;

	if (slot >= MAX_OVERLAYS)
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

	if (PARAM_headblenddebug.Get())
	{
		pedWarningf("[HEAD BLEND] MeshBlendManager::SetOverlayTintIndex(0x%08x, %d, %d, %d, %d): Old values: %d, %d, %d",
			handle, slot, rampType, tint, tint2, blend.overlayTintIndex[slot], blend.overlayTintIndex2[slot], blend.overlayRampType[slot]);
	}

	if (blend.overlayTintIndex[slot] == tint && blend.overlayTintIndex2[slot] == tint2 && blend.overlayRampType[slot] == rampType)
	{
		if (PARAM_headblenddebug.Get())
			pedWarningf("[HEAD BLEND] MeshBlendManager::SetOverlayTintIndex(): No change, early out");
		return;
	}

#if !__NO_OUTPUT
	if(blend.finalizeFrame == IS_FINALIZED)
	{
        Displayf("[HEAD BLEND] Trying to modify finalized head blend 0x%08x!", handle);
		#if __BANK
			sysStack::PrintStackTrace();
			scrThread::PrePrintStackTrace();
		#endif
	}
#endif

	sysCriticalSection cs(s_assetToken);
	
	blend.overlayTintIndex[slot] = tint;
	blend.overlayTintIndex2[slot] = tint2;
	blend.overlayRampType[slot] = (u8)rampType;

	if (slot < MAX_OVERLAYS - NUM_BODY_OVERLAYS)
		blend.reblend = true;
	else
		blend.reblendSkin[SBC_UPPR] = true;
}

void MeshBlendManager::SetSkinBlendComponentData(MeshBlendHandle handle, eSkinBlendComp slot, s32 cloth, s32 skin0, s32 skin1, s32 skin2, s32 maskParentDwd, u8 palId)
{
	if (!IsHandleValid(handle))
		return;

	if (slot >= SBC_MAX)
		return;

	if (cloth == -1 || skin0 == -1 || skin1 == -1 || skin2 == -1 || maskParentDwd == -1)
		return;

	u32 type = GetTypeFromHandle(handle);
	u32 index = GetIndexFromHandle(handle);

	sSkinBlendInfo& blendInfo = m_blends[type][index].skinBlend[slot];

	// early out if we're setting same values
	if (cloth == blendInfo.clothTex && skin0 == blendInfo.srcSkin0 && skin1 == blendInfo.srcSkin1 && skin2 == blendInfo.srcSkin2 && maskParentDwd == blendInfo.skinMaskParent && palId == blendInfo.palId)
		return;

	sysCriticalSection cs(s_assetToken);

	bool stillRequesting = sysInterlockedRead((unsigned*)&m_blends[type][index].state) == STATE_REQUEST_SLOT;
	bool stillWaitingForMem = sysInterlockedRead((unsigned*)&m_blends[type][index].state) == STATE_INIT_RT;

	g_TxdStore.AddRef(strLocalIndex(cloth), REF_OTHER);
	g_TxdStore.AddRef(strLocalIndex(skin0), REF_OTHER);
	g_TxdStore.AddRef(strLocalIndex(skin1), REF_OTHER);
	g_TxdStore.AddRef(strLocalIndex(skin2), REF_OTHER);
	g_DwdStore.AddRef(strLocalIndex(maskParentDwd), REF_OTHER);

	// clear old refs if any were set
	if (blendInfo.clothTex != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.clothTex), REF_OTHER);
	if (blendInfo.srcSkin0 != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.srcSkin0), REF_OTHER);
	if (blendInfo.srcSkin1 != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.srcSkin1), REF_OTHER);
	if (blendInfo.srcSkin2 != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.srcSkin2), REF_OTHER);
	if (blendInfo.skinMaskParent != -1)
		g_DwdStore.RemoveRef(strLocalIndex(blendInfo.skinMaskParent), REF_OTHER);

	blendInfo.clothTex = cloth;
	blendInfo.srcSkin0 = skin0;
	blendInfo.srcSkin1 = skin1;
	blendInfo.srcSkin2 = skin2;
	blendInfo.palId = palId;
    blendInfo.used = true;

	// skin mask comes from a spec texture that usually resides in a dwd, so we store the dwd index to be able to add/remove refs as needed
	blendInfo.skinMaskParent = maskParentDwd;

	m_blends[type][index].reblendSkin[slot] = true;
	m_blends[type][index].finalizeFrame = -1;

	if (stillWaitingForMem)
		sysInterlockedExchange((unsigned*)&m_blends[type][index].state, STATE_INIT_RT);
	else
		sysInterlockedExchange((unsigned*)&m_blends[type][index].state, stillRequesting ? STATE_REQUEST_SLOT : STATE_RENDERING);
}

void MeshBlendManager::RemoveSkinBlendComponentData(MeshBlendHandle handle, eSkinBlendComp slot)
{
	if (!IsHandleValid(handle))
		return;

	if (slot >= SBC_MAX)
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	sSkinBlendInfo& blendInfo = m_blends[type][index].skinBlend[slot];

    // early out if this operation won't change anything
    if (blendInfo.clothTex == -1 && blendInfo.srcSkin0 == -1 && blendInfo.srcSkin1 == -1 && blendInfo.srcSkin2 == -1 && blendInfo.skinMaskParent == -1 && blendInfo.used == false)
        return;

	sysCriticalSection cs(s_assetToken);

	bool stillRequesting = sysInterlockedRead((unsigned*)&m_blends[type][index].state) == STATE_REQUEST_SLOT;
	bool stillWaitingForMem = sysInterlockedRead((unsigned*)&m_blends[type][index].state) == STATE_INIT_RT;

	// clear old refs if any were set
	if (blendInfo.clothTex != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.clothTex), REF_OTHER);
	if (blendInfo.srcSkin0 != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.srcSkin0), REF_OTHER);
	if (blendInfo.srcSkin1 != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.srcSkin1), REF_OTHER);
	if (blendInfo.srcSkin2 != -1)
		g_TxdStore.RemoveRef(strLocalIndex(blendInfo.srcSkin2), REF_OTHER);
	if (blendInfo.skinMaskParent != -1)
		g_DwdStore.RemoveRef(strLocalIndex(blendInfo.skinMaskParent), REF_OTHER);

	blendInfo.clothTex = -1;
	blendInfo.srcSkin0 = -1;
	blendInfo.srcSkin1 = -1;
	blendInfo.srcSkin2 = -1;
	blendInfo.skinMaskParent = -1;
	blendInfo.used = false;
	blendInfo.palId = 0;

	m_blends[type][index].reblendSkin[slot] = false;
	m_blends[type][index].finalizeFrame = -1;

	if (stillWaitingForMem)
		sysInterlockedExchange((unsigned*)&m_blends[type][index].state, STATE_INIT_RT);
	else
		sysInterlockedExchange((unsigned*)&m_blends[type][index].state, stillRequesting ? STATE_REQUEST_SLOT : STATE_RENDERING);

}

void MeshBlendManager::SetSpecAndNormalTextures(MeshBlendHandle handle, strLocalIndex drawable1, strLocalIndex drawable2)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	if (m_blends[type][index].srcDrawable1 == drawable1 && m_blends[type][index].srcDrawable2 == drawable2)
		return;

	sysCriticalSection cs(s_assetToken);

	if (drawable1 != -1)
		g_DwdStore.AddRef(drawable1, REF_OTHER);
	if (drawable2 != -1)
		g_DwdStore.AddRef(drawable2, REF_OTHER);

	m_slots[type][m_blends[type][index].slotIndex].headSpecSrc = NULL;
	m_slots[type][m_blends[type][index].slotIndex].headNormSrc = NULL;

	// clear old skin component refs
	if (m_blends[type][index].srcDrawable1 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcDrawable1, REF_OTHER);
	if (m_blends[type][index].srcDrawable2 != -1)
		g_DwdStore.RemoveRef(m_blends[type][index].srcDrawable2, REF_OTHER);

	m_blends[type][index].srcDrawable1 = drawable1;
	m_blends[type][index].srcDrawable2 = drawable2;
	CStreaming::SetDoNotDefrag(drawable1, g_DwdStore.GetStreamingModuleId());
	CStreaming::SetDoNotDefrag(drawable2, g_DwdStore.GetStreamingModuleId());

	bool useDad = m_blends[type][index].male;
	if (m_blends[type][index].isParent)
	{
		useDad = m_blends[type][index].geomBlend > 0.5f;
	}

	// update head spec and normal maps for new heads
	strLocalIndex drawableIdx =  useDad ? m_blends[type][index].srcDrawable2 : m_blends[type][index].srcDrawable1;
	if (drawableIdx.IsValid() && g_DwdStore.Get(drawableIdx))
	{
		rmcDrawable* drawable = g_DwdStore.Get(drawableIdx)->GetEntry(0);
		fwTxd* txd = drawable->GetShaderGroup().GetTexDict();
		if (txd)
		{
			for (int i = 0; i < txd->GetCount(); ++i)
			{
				grcTexture* texture = txd->GetEntry(i);
				if (!strncmp(texture->GetName(), "head_spec", 9))
				{
					m_slots[type][m_blends[type][index].slotIndex].headSpecSrc = texture;
				}
				else
				{
					m_slots[type][m_blends[type][index].slotIndex].headNormSrc = texture;
				}
			}

			// trigger an overlay spec/norm reblend since we've switched the base textures
			m_blends[type][index].reblendOverlayMaps = true;
		}
	}

	m_blends[type][index].finalizeFrame = -1;
}

void MeshBlendManager::SetHairBlendRampIndex(MeshBlendHandle handle, u8 rampIndex, u8 rampIndex2)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	sBlendEntry& blend = m_blends[type][index];

	blend.hairRampIndex = rampIndex;
	blend.hairRampIndex2 = rampIndex2;
}

bool MeshBlendManager::SetRefreshPaletteColor(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return false;

	u32 type = GetTypeFromHandle(handle);
	u32 index = GetIndexFromHandle(handle);

	sBlendEntry& blend = m_blends[type][index];

	if (blend.usePaletteRgb)
	{
		m_palette.SetRefresh();
	}

	return true;
}

bool MeshBlendManager::SetPaletteColor(MeshBlendHandle handle, bool enable, u8 r, u8 g, u8 b, u8 colorIndex, bool bforce)
{
	pedDebugf3("MeshBlendManager::SetPaletteColor i[%d] rgb[%d %d %d] bforce[%d] enable[%d]",colorIndex,r,g,b,bforce,enable);

	if (!IsHandleValid(handle))
	{
		pedDebugf3("!IsHandleValid(handle)-->return false");
		return false;
	}

	u32 type = GetTypeFromHandle(handle);
	u32 index = GetIndexFromHandle(handle);

    sBlendEntry& blend = m_blends[type][index];

	// early out if we're setting same values
    if (blend.usePaletteRgb == enable && !enable)
	{
		pedDebugf3("(blend.usePaletteRgb == enable && !enable)-->return true");
        return true;
	}

    if (!Verifyf(blend.paletteIndex > - 1, "No palette index assigned to blend %d, something went wrong!", index))
	{
		pedDebugf3("!(blend.paletteIndex > -1)-->return false");
        return false;
	}

	Color32 oldColor = m_palette.GetColor(blend.paletteIndex, colorIndex);
	if (!bforce && oldColor.GetRed() == r && oldColor.GetGreen() == g && oldColor.GetBlue() == b && blend.usePaletteRgb == enable)
	{
		pedDebugf3("!bforce && colors same-->return true");
		return true;
	}

	sysCriticalSection cs(s_assetToken);

	bool stillRequesting = sysInterlockedRead((unsigned*)&m_blends[type][index].state) == STATE_REQUEST_SLOT;
	bool stillWaitingForMem = sysInterlockedRead((unsigned*)&m_blends[type][index].state) == STATE_INIT_RT;

    // don't overwrite the previous color if we just disable the custom palette color
    if (enable)
    {
        Color32 col(r, g, b, 255);
        m_palette.SetColor(blend.paletteIndex, colorIndex, col);
    }

    blend.usePaletteRgb = enable;


	if (stillWaitingForMem)
		sysInterlockedExchange((unsigned*)&m_blends[type][index].state, STATE_INIT_RT);
	else
		sysInterlockedExchange((unsigned*)&m_blends[type][index].state, stillRequesting ? STATE_REQUEST_SLOT : STATE_RENDERING);

	pedDebugf3("complete-->return true");
	return true;
}

void MeshBlendManager::GetPaletteColor(MeshBlendHandle handle, u8& r, u8& g, u8& b, u8 colorIndex)
{
	if (!IsHandleValid(handle))
	{
		pedDebugf3("GetPaletteColor--!IsHandleValid(handle)-->return false");
		return;
	}

	u32 type = GetTypeFromHandle(handle);
	u32 index = GetIndexFromHandle(handle);

	sBlendEntry& blend = m_blends[type][index];

	Color32 col = m_palette.GetColor(blend.paletteIndex, colorIndex);

	r = col.GetRed();
	g = col.GetGreen();
	b = col.GetBlue();
}

bool MeshBlendManager::AllocateRenderTarget(grcTexture* normalDxt1, grcTexture* smallDxt1 OUTPUT_ONLY(, int slotId))
{
	if (!Verifyf(normalDxt1 && smallDxt1, "MeshBlendManager::AllocateRenderTarget: No destination texture given for allocating render targets!"))
		return false;

	// early out if all RTs are allocated, might need to remove this if the dimensions will vary in the future
	if ( m_renderTarget && m_renderTargetSmall && m_renderTargetPalette 
#if RSG_ORBIS || RSG_DURANGO
		&& m_renderTargetAliased256 && m_renderTargetAliased128 && m_renderTargetAliased64 
#elif RSG_PC
		&& m_tempDxtTargets.GetCount() > 0
#endif 
		)
	{
		return true;
	}
	
	if ( m_renderTarget || m_renderTargetSmall || m_renderTargetPalette
#if RSG_ORBIS || RSG_DURANGO
		|| m_renderTargetAliased256 || m_renderTargetAliased128 || m_renderTargetAliased64
#elif RSG_PC
		|| m_tempDxtTargets.GetCount() > 0
#endif
		)
	{
		ReleaseRenderTarget();
	}

	const u32 bpp = 32;
	const u32 maxDim = (__XENON || __PS3) ? 256 : 512;
	u32 width = normalDxt1->GetWidth();
	u32 height = normalDxt1->GetHeight();
#if !RSG_PC
	Assertf(width == maxDim && height == maxDim, "Head blend render targets are not allocated for 256 textures as expected! (%d - %d)", width, height);
#else  //Since the texture dimension can change based on settings, just make sure the dimensions don't exceed the maximum.
	Assertf(width <= maxDim && height <= maxDim, "Head blend render targets are not allocated for larger than 512 textures as expected! (%d - %d)", width, height);
#endif

	// calculate memory required for top mip + rest of mip chain
	u32 mipLevels = normalDxt1->GetMipMapCount();
	const u32 pitch = maxDim * bpp / 8;

	u32 mipHeight = maxDim;

	u32 mipSize = 0;
	mipHeight = Max(mipHeight >> 1, 1u);
	while (mipHeight >= 4)
	{
		mipSize += mipHeight * pitch;
		mipHeight = Max(mipHeight >> 1, 1u);
	}

	grcTextureFactory::CreateParams params;
	params.MipLevels = mipLevels;
	params.HasParent = true;
	params.Parent = NULL;
	params.Format = grctfA8R8G8B8;

#if RSG_PC || RSG_ORBIS
	params.Lockable = true;
#endif

	char name[128];
	sprintf(name, "MeshBlendManager");
	pedDebugf1("[MESHBLEND]: Main render target for slot %d dimensions are %dx%d", slotId, width, height);
	m_renderTarget = grcTextureFactory::GetInstance().CreateRenderTarget(name, grcrtPermanent, width, height, bpp, &params);
#if __D3D11 || RSG_ORBIS
	sprintf(name, "MeshBlendManagerTemp");
	m_renderTargetTemp = grcTextureFactory::GetInstance().CreateRenderTarget(name, grcrtPermanent, width, height, bpp, &params);
#endif

	bool failed = !m_renderTarget;
	if (!failed)
	{
		Assert(mipLevels >= 1);
		sprintf(name, "MeshBlendManagerSmall");

		params.MipLevels = mipLevels - 1;
		m_renderTargetSmall = grcTextureFactory::GetInstance().CreateRenderTarget(name, grcrtPermanent, width >> 1, height >> 1, bpp, &params);

#if __D3D11 || RSG_ORBIS
		sprintf(name, "MeshBlendManagerSmallTemp");
		m_renderTargetSmallTemp = grcTextureFactory::GetInstance().CreateRenderTarget(name, grcrtPermanent, width >> 1, height >> 1, bpp, &params);
#endif
		failed |= !m_renderTargetSmall;
	}

	params.MipLevels = mipLevels;

#if RSG_DURANGO || RSG_ORBIS
	sprintf(name, "MeshBlendDxt1Target256");
	m_renderTargetAliased256 = grcTextureFactory::GetInstance().CreateRenderTarget(name, normalDxt1->GetTexturePtr());
	failed |= m_renderTargetAliased256 == NULL;

	params.MipLevels = mipLevels - 1;
	sprintf(name, "MeshBlendDxt1Target128");
	m_renderTargetAliased128 = grcTextureFactory::GetInstance().CreateRenderTarget(name, smallDxt1->GetTexturePtr());
	failed |= m_renderTargetAliased128 == NULL;
#else
	{
		u32 nWidth = normalDxt1->GetWidth()>>2;
		u32 nHeight = normalDxt1->GetHeight()>>2;

		grcTextureFactory::TextureCreateParams TextureParams(
			grcTextureFactory::TextureCreateParams::SYSTEM, 
			grcTextureFactory::TextureCreateParams::LINEAR,	
			grcsRead|grcsWrite, 
			NULL, 
			grcTextureFactory::TextureCreateParams::RENDERTARGET, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);

		grcTextureFactory::CreateParams pcParams;
		pcParams.Format = grctfA16B16G16R16;
		pcParams.MipLevels = 1;

		while ( nWidth>1 && nHeight>1 && !failed )
		{
			sprintf(name, "MeshBlendDxt1Target%dx%d",nWidth,nHeight);

			if ( !UseCpuRoundtrip() )
			{
				grcRenderTarget* pRenderTarget = grcTextureFactory::GetInstance().CreateRenderTarget(name, grcrtPermanent, nWidth, nHeight, 64, &pcParams);
				failed |= pRenderTarget == NULL;

				if ( !failed )
				{
					m_tempDxtTargets.PushAndGrow(pRenderTarget);
				}
			}
			else
			{
				grcTexture* pTexture = grcTextureFactory::GetInstance().Create( nWidth, nHeight, grctfA16B16G16R16, NULL, 1, &TextureParams );
				Assert(pTexture != nullptr);
				failed |= pTexture == NULL;

				grcRenderTarget* pRenderTarget = grcTextureFactory::GetInstance().CreateRenderTarget(name, pTexture->GetTexturePtr(), NULL);			
				Assert(pRenderTarget != nullptr);

				failed |= pTexture == NULL;
				failed |= pRenderTarget == NULL;
			
				if ( !failed )
				{
					m_tempDxtTargets.PushAndGrow(pRenderTarget);
					m_tempDxtTextures.PushAndGrow(pTexture);
				}
			}

			nWidth = nWidth >> 1;
			nHeight = nHeight >> 1;
		}
	}
#endif

	if (!failed && s_miscTxdIndex != -1)
	{
		const grcTexture* palTex = GetMiscTxdTexture(s_paletteMiscIdx);
		if (palTex)
		{
			#if RSG_DURANGO || RSG_ORBIS
				sprintf(name, "MeshBlendDxt1Target64");
				m_renderTargetAliased64 = grcTextureFactory::GetInstance().CreateRenderTarget(name, palTex->GetTexturePtr());
				Assertf(m_renderTargetAliased64, "Failed to create mesh blend palette alias render target!");
				failed |= !m_renderTargetAliased64;
			#endif	

			sprintf(name, "MeshBlendPaletteTarget");
			params.MipLevels = 1;

			m_renderTargetPalette = grcTextureFactory::GetInstance().CreateRenderTarget(name, grcrtPermanent, palTex->GetWidth(), palTex->GetHeight(), bpp, &params);
			Assertf(m_renderTargetPalette, "Failed to create mesh blend palette render target!");
			failed |= !m_renderTargetPalette;
		}
	}

	if (failed)
	{
		MEM_USE_USERDATA(MEMUSERDATA_MESHBLEND);
		Assertf(false, "MeshBlendManager::AllocateRenderTarget: Failed to allocate render targets needed!");
		return false;
	}

	return true;
}

#if RSG_PC
grcRenderTarget* MeshBlendManager::FindTempDxtRenderTarget( const int nWidth, const int nHeight )
{
	const int nNumTargets = m_tempDxtTargets.GetCount();
	for ( int n=0; n<nNumTargets; n++ )
	{
		if ( m_tempDxtTargets[n] != nullptr && m_tempDxtTargets[n]->GetWidth() == nWidth && m_tempDxtTargets[n]->GetHeight() == nHeight )
		{
			return m_tempDxtTargets[n];
		}
	}

	// We'll assert here for now since I don't expect this to ever return NULL unless we've done something wrong
	Assertf(false, "Unable to find temp render target for DXT compression of mesh blend data!" );
	return nullptr;
}

grcTexture* MeshBlendManager::FindTempDxtTexture( const int nWidth, const int nHeight )
{
	const int nNum = m_tempDxtTextures.GetCount();
	for ( int n=0; n<nNum; n++ )
	{
		if ( m_tempDxtTextures[n] != nullptr && m_tempDxtTextures[n]->GetWidth() == nWidth && m_tempDxtTextures[n]->GetHeight() == nHeight )
		{
			return m_tempDxtTextures[n];
		}
	}

	// We'll assert here for now since I don't expect this to ever return NULL unless we've done something wrong
	Assertf(false, "Unable to find temp texture for DXT compression of mesh blend data!" );
	return nullptr;
}
#endif

void MeshBlendManager::ReleaseRenderTarget()
{
	if (m_renderTarget)
	{
		m_renderTarget->Release();
		m_renderTarget = NULL;
	}

	if (m_renderTargetSmall)
	{
		m_renderTargetSmall->Release();
		m_renderTargetSmall = NULL;
	}

#if RSG_ORBIS || RSG_DURANGO
	if (m_renderTargetAliased256)
	{
		m_renderTargetAliased256->Release();
		m_renderTargetAliased256 = NULL;
	}

	if (m_renderTargetAliased128)
	{
		m_renderTargetAliased128->Release();
		m_renderTargetAliased128 = NULL;
	}

	if (m_renderTargetAliased64)
	{
		m_renderTargetAliased64->Release();
		m_renderTargetAliased64 = NULL;
	}
#elif RSG_PC
	for ( int n=0; n<m_tempDxtTargets.GetCount(); n++ )
	{
		SAFE_RELEASE( m_tempDxtTargets[n] );
	}
	m_tempDxtTargets.Reset();

	for ( int n=0; n<m_tempDxtTextures.GetCount(); n++ )
	{
		SAFE_RELEASE( m_tempDxtTextures[n] );
	}
	m_tempDxtTextures.Reset();
#endif 

	if (m_renderTargetPalette)
	{
		m_renderTargetPalette->Release();
		m_renderTargetPalette = NULL;
	}

#if __D3D11 || RSG_ORBIS
	if (m_renderTargetTemp)
	{
		m_renderTargetTemp->Release();
		m_renderTargetTemp = NULL;
	}
	if (m_renderTargetSmallTemp)
	{
		m_renderTargetSmallTemp->Release();
		m_renderTargetSmallTemp = NULL;
	}
#endif

	{
		MEM_USE_USERDATA(MEMUSERDATA_MESHBLEND);
	}
}

#if __BANK
void MeshBlendManager::AddWidgets(bkBank& bank)
{
	int sliderMax = 0;
	if (ms_activeBlendType > -1 && ms_activeBlendType < MBT_MAX)
		sliderMax = Max<int>(0, GetInstance().m_blends[ms_activeBlendType].GetCount() - 1);

	bank.PushGroup("Mesh blending");
	bank.AddCombo("Blend types", &ms_activeBlendType, MBT_MAX, s_blendTypeNames, ActiveBlendTypeChangedCB);
	ms_activeBlendEntrySlider = bank.AddSlider("Debug blend entry", &ms_activeBlendEntry, 0, sliderMax, 1);
	bank.AddToggle("Show textures", &ms_showBlendTextures);
	bank.AddToggle("Show debug text", &ms_showDebugText);
	bank.AddToggle("Show palette", &ms_showPalette);
	bank.AddSlider("Palette scale", &ms_paletteScale, 0.f, 10.f, 0.1f);
	bank.PopGroup();
}

void MeshBlendManager::SetActiveDebugEntry(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return;

	ms_activeBlendType = (u32)GetTypeFromHandle(handle);
	ms_activeBlendEntry = (u32)GetIndexFromHandle(handle);
}

#define GET_ASSET_SIZES(ram, vram, index, module)							\
	if (index != -1)														\
	{																		\
		strIndex idx = module.GetStreamingIndex(index);						\
		ram += strStreamingEngine::GetInfo().GetObjectVirtualSize(idx);		\
		vram += strStreamingEngine::GetInfo().GetObjectPhysicalSize(idx);	\
	}

void MeshBlendManager::CalculateMemoryCost(MeshBlendHandle hnd, u32& ram, u32& vram, atArray<s32>& dwdAssets, atArray<s32>& txdAssets, u32& totalRam, u32& totalVram)
{
	if (!IsHandleValid(hnd))
		return;

	atArray<s32> txds;
	txds.Reserve(4 + SBC_MAX + MAX_OVERLAYS);
	atArray<s32> dwds;
	dwds.Reserve(6);

	u16 type = GetTypeFromHandle(hnd);
	u16 index = GetIndexFromHandle(hnd);

	const sBlendSlot& slot = m_slots[type][m_blends[type][index].slotIndex];
	GET_ASSET_SIZES(ram, vram, slot.dwdIndex, g_DwdStore);		dwds.Push(slot.dwdIndex.Get());
	GET_ASSET_SIZES(ram, vram, slot.txdIndex, g_TxdStore);		txds.Push(slot.txdIndex.Get());
	for (s32 i = 0; i < MAX_OVERLAYS; ++i)
	{
		GET_ASSET_SIZES(ram, vram, strLocalIndex(slot.overlayIndex[i]), g_TxdStore);
		txds.Push(slot.overlayIndex[i]);
	}
	for (s32 i = 0; i < SBC_MAX; ++i)
	{
		GET_ASSET_SIZES(ram, vram, strLocalIndex(slot.skinCompIndex[i]), g_TxdStore);
		txds.Push(slot.skinCompIndex[i].Get());
	}

	const sBlendEntry& blend = m_blends[type][index];
	GET_ASSET_SIZES(ram, vram, blend.srcTexIdx1, g_TxdStore);	txds.Push(blend.srcTexIdx1.Get());
	GET_ASSET_SIZES(ram, vram, blend.srcTexIdx2, g_TxdStore);	txds.Push(blend.srcTexIdx2.Get());
	GET_ASSET_SIZES(ram, vram, blend.srcTexIdx3, g_TxdStore);	txds.Push(blend.srcTexIdx3.Get());
	GET_ASSET_SIZES(ram, vram, blend.srcGeomIdx1, g_DwdStore);	dwds.Push(blend.srcGeomIdx1.Get());
	GET_ASSET_SIZES(ram, vram, blend.srcGeomIdx2, g_DwdStore);	dwds.Push(blend.srcGeomIdx2.Get());
	GET_ASSET_SIZES(ram, vram, blend.srcGeomIdx3, g_DwdStore);	dwds.Push(blend.srcGeomIdx3.Get());
	GET_ASSET_SIZES(ram, vram, blend.srcDrawable1, g_DwdStore);	dwds.Push(blend.srcDrawable1.Get());
	GET_ASSET_SIZES(ram, vram, blend.srcDrawable2, g_DwdStore);	dwds.Push(blend.srcDrawable2.Get());

	// get totals, don't count same assets twice
	for (s32 i = 0; i < dwdAssets.GetCount(); ++i)
	{
		for (s32 f = 0; f < dwds.GetCount(); ++f)
		{
			if (dwdAssets[i] == dwds[f])
				dwds[f] = -1;
		}
	}
	for (s32 i = 0; i < txdAssets.GetCount(); ++i)
	{
		for (s32 f = 0; f < txds.GetCount(); ++f)
		{
			if (txdAssets[i] == txds[f])
				txds[f] = -1;
		}
	}

	for (s32 i = 0; i < dwds.GetCount(); ++i)
	{
		if (dwds[i] == -1)
			continue;

		dwdAssets.PushAndGrow(dwds[i]);
		GET_ASSET_SIZES(totalRam, totalVram, strLocalIndex(dwds[i]), g_DwdStore);
	}
	for (s32 i = 0; i < txds.GetCount(); ++i)
	{
		if (txds[i] == -1)
			continue;

		txdAssets.PushAndGrow(txds[i]);
		GET_ASSET_SIZES(totalRam, totalVram, strLocalIndex(txds[i]), g_TxdStore);
	}
}

void MeshBlendManager::ActiveBlendTypeChangedCB()
{
	if (ms_activeBlendEntrySlider && ms_activeBlendType > -1 && ms_activeBlendType < MBT_MAX && GetInstance().m_blends[ms_activeBlendType].GetCount() > 0)
		ms_activeBlendEntrySlider->SetRange(0.f, (float)GetInstance().m_blends[ms_activeBlendType].GetCount() - 1.f);
}

void MeshBlendManager::DebugRender(s32 type, s32 blend)
{
	sBlendEntry& next = GetInstance().m_blends[type][blend];
	sBlendSlot& slot = GetInstance().m_slots[next.type][next.slotIndex];

	const grcTexture* srcTex1 = NULL;
	if (next.srcTexIdx1.IsValid() && g_TxdStore.Get(next.srcTexIdx1))
		srcTex1 = g_TxdStore.Get(next.srcTexIdx1)->GetEntry(0);

	const grcTexture* srcTex2 = NULL;
	if (next.srcTexIdx2.IsValid() && g_TxdStore.Get(next.srcTexIdx2))
		srcTex2 = g_TxdStore.Get(next.srcTexIdx2)->GetEntry(0);

	const grcTexture* srcTex3 = NULL;
	if (next.srcTexIdx3.IsValid() && g_TxdStore.Get(next.srcTexIdx3))
		srcTex3 = g_TxdStore.Get(next.srcTexIdx3)->GetEntry(0);

	const grcTexture* dstTex = NULL;
	if (next.addedRefs && slot.txdIndex.IsValid() && g_TxdStore.Get(slot.txdIndex))
		dstTex = g_TxdStore.Get(slot.txdIndex)->GetEntry(0);

	if (ms_showBlendTextures && ms_activeBlendType == type && ms_activeBlendEntry == blend)
	{
		char buf[64];

		// Debug visualization
		PUSH_DEFAULT_SCREEN();
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		if (srcTex1)
		{
			grcBindTexture(srcTex1);
			grcDrawSingleQuadf(630,50,830,250,0,0,0,1,1,Color32(255,255,255,255));
		}
		if (srcTex2)
		{
			grcBindTexture(srcTex2);
			grcDrawSingleQuadf(830,50,1030,250,0,0,0,1,1,Color32(255,255,255,255));
		}
		if (srcTex3)
		{
			grcBindTexture(srcTex3);
			grcDrawSingleQuadf(1030,270,1230,470,0,0,0,1,1,Color32(255,255,255,255));
		}
		grcBindTexture(dstTex);
		grcDrawSingleQuadf(1030,50,1230,250,0,0,0,1,1,Color32(255,255,255,255));
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
		grcColor(Color_black);
		grcDraw2dText(630,260,formatf(buf,"Last compression completed in %u ms",next.totalTime));
		if (srcTex1)
			grcDraw2dText(630,35,formatf(buf,"S1 %s", srcTex1->GetName()));
		if (srcTex2)
			grcDraw2dText(830,35,formatf(buf,"S2 %s", srcTex2->GetName()));
		if (srcTex3)
			grcDraw2dText(1030,260,formatf(buf,"Gen var %s", srcTex3->GetName()));
		grcDraw2dText(1030,35,formatf(buf,"Blend target"));
		POP_DEFAULT_SCREEN();

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
	}

	if (ms_showPalette)
	{
		grcTexture* palTex = GetMiscTxdTexture(s_paletteMiscIdx);
		if (palTex)
		{
			PUSH_DEFAULT_SCREEN();
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

			float x = 50.f;
			float y = 50.f;
			float x2 = x + palTex->GetWidth() * ms_paletteScale;
			float y2 = y + palTex->GetHeight() * ms_paletteScale;
			grcBindTexture(palTex);
			grcDrawSingleQuadf(x, y, x2, y2, 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

			if (GetInstance().m_renderTargetPalette)
			{
				x = x2 + 1.f;
				x2 = x + palTex->GetWidth() * ms_paletteScale;
				grcBindTexture(GetInstance().m_renderTargetPalette);
				grcDrawSingleQuadf(x, y, x2, y2, 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));
			}
			POP_DEFAULT_SCREEN();

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_Default);
		}
	}
}

void MeshBlendManager::DebugText(s32 type, s32 blend, u32& totalRam, u32& totalVram, atArray<s32>& dwdAssets, atArray<s32>& txdAssets)
{
	if (ms_showDebugText)
	{
		MeshBlendHandle hnd = type << 16 | blend;
		sBlendEntry& next = GetInstance().m_blends[type][blend];

		u32 ram = 0;
		u32 vram = 0;
		GetInstance().CalculateMemoryCost(hnd, ram, vram, dwdAssets, txdAssets, totalRam, totalVram);

		bool finalized = next.finalizeFrame == IS_FINALIZED;
		char strState[32] = {0};
		switch (next.state)
		{
		case STATE_IDLE:
			formatf(strState, "idle %s %s", next.outOfMem ? "- OOM!" : "", finalized ? "fin" : "");
			break;
		case STATE_REQUEST_SLOT:
			formatf(strState, "request slot %s %s", next.outOfMem ? "- OOM!" : "", finalized ? "fin" : "");
			break;
		case STATE_INIT_RT:
			formatf(strState, "init rt %s %s", next.outOfMem ? "- OOM!" : "", finalized ? "fin" : "");
			break;
		case STATE_RENDERING:
			formatf(strState, "rendering %s %s", next.outOfMem ? "- OOM!" : "", finalized ? "fin" : "");
			break;
		case STATE_RENDER_BODY_SKIN:
			formatf(strState, "render skin %s %s", next.outOfMem ? "- OOM!" : "", finalized ? "fin" : "");
			break;
		case STATE_RENDER_OVERLAY:
			formatf(strState, "render overlay %s %s %s", next.overlayCompSpec ? "spec" : "norm", next.outOfMem ? "- OOM!" : "", finalized ? "fin" : "");
			break;
		case STATE_NOT_USED:
			formatf(strState, "not used %s %s", next.outOfMem ? "- OOM!" : "", finalized ? "fin" : "");
			break;
		}

		Color32 col = Color_white;
		if (GetInstance().HasBlendFinished(hnd))
			col = (ms_activeBlendType == type && ms_activeBlendEntry == blend) ? Color_yellow3 : Color_green;
		else
			col = (ms_activeBlendType == type && ms_activeBlendEntry == blend) ? Color_copper : col;

		grcDebugDraw::AddDebugOutputEx(false, col, " %d    %20s%20s %.2f   %.2f   %.2f %s (m%dk v%dk) (%s%s%s%s%s%s%s)", blend, "no texture", "no texture", next.geomBlend, next.texBlend, next.varBlend, strState, ram>>10, vram>>10,
			next.hasReqs ? "1" : "0",
			next.reblend ? "1" : "0",
			next.reblendMesh ? "1" : "0",
			next.reblendOverlayMaps ? "1" : "0",
			next.reblendSkin[SBC_FEET] ? "1" : "0",
			next.reblendSkin[SBC_UPPR] ? "1" : "0",
			next.reblendSkin[SBC_LOWR] ? "1" : "0");
	}
}

void MeshBlendManager::ForceTextureBlend(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	if (PARAM_headblenddebug.Get())
		pedWarningf("[HEAD BLEND] MeshBlendManager::ForceTextureBlend(0x%08x): reblend=true", handle);
	m_blends[type][index].reblend = true;
	m_blends[type][index].reblendSkin[SBC_FEET] = true;
	m_blends[type][index].reblendSkin[SBC_UPPR] = true;
	m_blends[type][index].reblendSkin[SBC_LOWR] = true;
}

void MeshBlendManager::ForceMeshBlend(MeshBlendHandle handle)
{
	if (!IsHandleValid(handle))
		return;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);

	m_blends[type][index].reblendMesh = true;
}

#endif

void MeshBlendManager::CloneTexture(const grcTexture* src, grcTexture* dst) const
{
	if (src->GetDepth() == 1 && dst->GetLayerCount() == 1) // only works for 2D textures
	{
		const u8 c = src->GetConversionFlags();
		//const u16 t = src->GetTemplateType();

		if (!Verifyf(src->GetWidth() == dst->GetWidth() && src->GetHeight() == dst->GetHeight(), "MeshBlendManager::CloneTexture - Texture dimension mismatch! %d == %d && %d == %d (%s) (%s)", src->GetWidth(), dst->GetWidth(), src->GetHeight(), dst->GetHeight(), src->GetName(), dst->GetName()))
			return;
		if (!Verifyf(src->GetMipMapCount() == dst->GetMipMapCount() || src->GetMipMapCount() >= 4, "MeshBlendManager::CloneTexture - Texture mip map count mismatch!"))
			return;

		grcTexture::eTextureSwizzle swizzle[4];
		src->GetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

#if RSG_PC && __D3D11
		// In order to copy into the destination texture, we need it to not be immutable
		ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
		static_cast<grcTextureDX11*>(dst)->StripImmutability(false);
		ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
		dst->Copy(src);
#elif RSG_DURANGO
		dst->Copy(src);
#else
		const int              m =                   dst->GetMipMapCount ();
		const grcImage::Format f = (grcImage::Format)src->GetImageFormat ();

		for (int i = 0; i < m; i++) // copy each mipmap
		{
			grcTextureLock srcLock;
			grcTextureLock newLock;

			if (src->LockRect(0, i, srcLock, grcsRead  | grcsAllowVRAMLock) )
			{
				if ( dst->LockRect(0, i, newLock, grcsWrite | grcsAllowVRAMLock) )
				{
					if (newLock.Base && srcLock.Base)
					{
						const int blockSize = grcImage::IsFormatDXTBlockCompressed( f ) ? 4 : 1;
						memcpy(newLock.Base, srcLock.Base, ( newLock.Pitch / blockSize ) * newLock.Height);
					}
					dst->UnlockRect(newLock);
				}
				src->UnlockRect(srcLock);
			}
		}
#endif
		dst->SetTextureSwizzle(swizzle[0], swizzle[1], swizzle[2], swizzle[3], false);
		dst->SetConversionFlags(c);
		//dst->SetTemplateType(t);
	}
}

grcTexture* MeshBlendManager::GetOverlayTexture(strLocalIndex txdIndex, eOverlayTextureType type) const
{
	fwTxd* txd = g_TxdStore.Get(txdIndex);
	if (txd)
	{
		for (int k = 0; k < txd->GetCount(); k++)
		{
			grcTexture* texture = txd->GetEntry(k);
            const char* texName = texture->GetName();
            s32 len = istrlen(texName);
            const char* lastTwo = &texName[len - 2];
            
			if (!strcmp(lastTwo, "_n"))
			{
				if (type == OTT_NORMAL)
					return texture;
			}
			else if (!strcmp(lastTwo, "_s"))
			{
				if (type == OTT_SPECULAR)
					return texture;
			}
			else if (type == OTT_DIFFUSE)
				return texture;
		}
	}

	return NULL;
}

grcTexture* MeshBlendManager::GetDrawableTexture(rmcDrawable* drawable, bool specular) const
{
	if (!drawable)
		return NULL;

	fwTxd* txd = drawable->GetShaderGroup().GetTexDict();
	if (txd)
	{
		for (int i = 0; i < txd->GetCount(); ++i)
		{
			grcTexture* texture = txd->GetEntry(i);
			if (!texture)
				continue;

			if (specular && !strncmp(texture->GetName(), "head_spec", 9))
				return texture;
			else if (!specular && !strncmp(texture->GetName(), "head_normal", 11))
				return texture;
		}
	}

	return NULL;
}

grcTexture* MeshBlendManager::GetSpecTextureFromDrawable(rmcDrawable* drawable, const char* ASSERT_ONLY(dbgName))
{
	if (!drawable)
		return NULL;

	// find the specular in the drawable to use as the skin mask
	fwTxd* txd = drawable->GetShaderGroup().GetTexDict();
	if (txd)
	{
		for (int k = 0; k < txd->GetCount(); k++)
		{
			grcTexture* texture = txd->GetEntry(k);
			if (!Verifyf(texture, "Missing spec texture in drawable %s", dbgName))
				continue;

			if (strstr(texture->GetName(), "_spec_"))
			{
				return texture;
			}
		}
	}

	return NULL;
}

grcTexture* MeshBlendManager::GetPalTextureFromDrawable(rmcDrawable* drawable)
{
	if (!drawable)
		return NULL;

	fwTxd* txd = drawable->GetShaderGroup().GetTexDict();
	if (txd)
	{
		for (int k = 0; k < txd->GetCount(); k++)
		{
			grcTexture* texture = txd->GetEntry(k);
			if (strstr(texture->GetName(), "_pall_"))
			{
				return texture;
			}
		}
	}

	return NULL;
}

void MeshBlendManager::ReplaceAssets(sBlendEntry& blend, bool releaseOverlays, s32 newSrcTex1, s32 newSrcTex2, s32 newSrcTex3, s32 newSrcGeom1, s32 newSrcGeom2, s32 newSrcGeom3, bool finalizing)
{
	pedDebugf1("[HEAD BLEND] MeshBlendManager::ReplaceAssets(slot %d, %s, %d, %d, %d, %d, %d, %d, %s)", blend.slotIndex, releaseOverlays ? "true" : "false",
		newSrcTex1, newSrcTex2, newSrcTex3, newSrcGeom1, newSrcGeom2, newSrcGeom3, finalizing ? "true" : "false");

	// we do it the long way to avoid having assets being temporarily invalid on the render thread
	strLocalIndex oldSrcTex1 = blend.srcTexIdx1;
	strLocalIndex oldSrcTex2 = blend.srcTexIdx2;
	strLocalIndex oldSrcTex3 = blend.srcTexIdx3;
	strLocalIndex oldSrcGeom1 = blend.srcGeomIdx1;
	strLocalIndex oldSrcGeom2 = blend.srcGeomIdx2;
	strLocalIndex oldSrcGeom3 = blend.srcGeomIdx3;

	blend.srcTexIdx1 = newSrcTex1;
	blend.srcTexIdx2 = newSrcTex2;
	blend.srcTexIdx3 = newSrcTex3;
	blend.srcGeomIdx1 = newSrcGeom1;
	blend.srcGeomIdx2 = newSrcGeom2;
	blend.srcGeomIdx3 = newSrcGeom3;

	if (oldSrcTex1 != -1)
		g_TxdStore.RemoveRef(oldSrcTex1, REF_OTHER);
	if (oldSrcTex2 != -1)
		g_TxdStore.RemoveRef(oldSrcTex2, REF_OTHER);
	if (oldSrcTex3 != -1)
		g_TxdStore.RemoveRef(oldSrcTex3, REF_OTHER);
	if (oldSrcGeom1 != -1)
		g_DwdStore.RemoveRef(oldSrcGeom1, REF_OTHER);
	if (oldSrcGeom2 != -1)
		g_DwdStore.RemoveRef(oldSrcGeom2, REF_OTHER);
	if (oldSrcGeom3 != -1)
		g_DwdStore.RemoveRef(oldSrcGeom3, REF_OTHER);

	sBlendSlot& slot = m_slots[blend.type][blend.slotIndex];

    // drawables for spec and normal maps aren't cleared here unless finalize the blend
    if (finalizing)
    {
        if (blend.srcDrawable1 != -1)
            g_DwdStore.RemoveRef(blend.srcDrawable1, REF_OTHER);
        if (blend.srcDrawable2 != -1)
            g_DwdStore.RemoveRef(blend.srcDrawable2, REF_OTHER);
        blend.srcDrawable1 = -1;
        blend.srcDrawable2 = -1;

        slot.headSpecSrc = NULL;
        slot.headNormSrc = NULL;

		if (blend.srcParentGeomIdx1 != -1)
			g_DwdStore.RemoveRef(blend.srcParentGeomIdx1, REF_OTHER);
		if (blend.srcParentGeomIdx2 != -1)
			g_DwdStore.RemoveRef(blend.srcParentGeomIdx2, REF_OTHER);
		blend.srcParentGeomIdx1 = -1;
		blend.srcParentGeomIdx2 = -1;
    }

	for (s32 i = 0; i < SBC_MAX; ++i)
	{
		if (blend.skinBlend[i].clothTex != -1)
			g_TxdStore.RemoveRef(strLocalIndex(blend.skinBlend[i].clothTex), REF_OTHER);
		if (blend.skinBlend[i].srcSkin0 != -1)
			g_TxdStore.RemoveRef(strLocalIndex(blend.skinBlend[i].srcSkin0), REF_OTHER);
		if (blend.skinBlend[i].srcSkin1 != -1)
			g_TxdStore.RemoveRef(strLocalIndex(blend.skinBlend[i].srcSkin1), REF_OTHER);
		if (blend.skinBlend[i].srcSkin2 != -1)
			g_TxdStore.RemoveRef(strLocalIndex(blend.skinBlend[i].srcSkin2), REF_OTHER);
		if (blend.skinBlend[i].skinMaskParent != -1)
			g_DwdStore.RemoveRef(strLocalIndex(blend.skinBlend[i].skinMaskParent), REF_OTHER);

		blend.skinBlend[i].clothTex = -1;
		blend.skinBlend[i].srcSkin0 = -1;
		blend.skinBlend[i].srcSkin1 = -1;
		blend.skinBlend[i].srcSkin2 = -1;
		blend.skinBlend[i].skinMaskParent = -1;
	}

	if (releaseOverlays)
	{
		for (s32 i = 0; i < MAX_OVERLAYS; ++i)
		{
			s32 req = slot.overlayReq[i];
			strLocalIndex txdIndex = strLocalIndex(slot.overlayIndex[i]);

			if (req != -1)
			{
				m_reqs[req].Release();
				if (txdIndex != -1)
					g_TxdStore.ClearRequiredFlag(txdIndex.Get(), STRFLAG_DONTDELETE);
			}

			if (txdIndex != -1 && ((slot.addedOverlayRefs & (1 << i)) != 0))
				g_TxdStore.RemoveRef(txdIndex, REF_OTHER);

			slot.overlayReq[i] = -1;
			slot.overlayIndex[i] = -1;
		}
	}
}

// NOTE: Don't pass in non-square textures into this function unless you know what you're doing.
// The resolve on 360 will auto tile the texture. If it's square it'll happen in place, if it isn't,
// the tiling will use more memory than consumed by the texture. If you resolve into a resource texture
// that's not square you're guaranteed to overwrite whatever memory comes after your last mip
//
// D3D 10.0 will require compression to span multiple frames since it requires a round-trip from GPU->CPU->GPU. GPU Compression on 10.1 can
// stay entirely on the GPU and will never require waiting for results. If you are on D3D 10.0 you will need to query FinishCompressDxt()
// each frame until it returns TRUE. While a compress job is in flight, no other compression work can be submitted.
//
bool MeshBlendManager::CompressDxt(grcRenderTarget* srcRt, grcTexture* dstTex, bool doMips)
{
	PF_AUTO_PUSH_TIMEBAR("CompressDxt");

#if RSG_PC
	Assert( sysInterlockedRead((unsigned*)&m_compressionStatus) == DXT_COMPRESSOR_AVAILABLE );
	u32 nMipBitMask = 0;
#endif

	// TEMP: need to fix dxt5
    if (dstTex->GetBitsPerPixel() != 4)
		return true;

#if !RSG_FINAL
	if ( PARAM_disableHeadBlendCompress.Get() )
		return true;
#endif

#if RSG_ORBIS || RSG_DURANGO
	if (!Verifyf(m_renderTargetAliased256 && m_renderTargetAliased128 && m_renderTargetAliased64, "No aliased dxt1 target exists! Check log for errors"))
		return false;
#elif RSG_PC
	if (!Verifyf(m_tempDxtTargets.GetCount()>0, "No dxt1 temp target exists! Check log for errors"))
		return false;
#endif


#if __D3D11 && !RSG_DURANGO
	// In order to copy into the destination texture, we need it to not be immutable
	ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
	static_cast<grcTextureDX11*>(dstTex)->StripImmutability(true);
	ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
#endif

	s32 renderWidth = (s32)dstTex->GetWidth() >> 2;
	s32 renderHeight = (s32)dstTex->GetHeight() >> 2;

#if RSG_ORBIS || RSG_DURANGO
	grcRenderTarget* rt = NULL;
	if (renderWidth == m_renderTargetAliased256->GetWidth() && renderHeight == m_renderTargetAliased256->GetHeight())
	{
		rt = m_renderTargetAliased256;
	}
	else if (renderWidth == m_renderTargetAliased128->GetWidth() && renderHeight == m_renderTargetAliased128->GetHeight())
	{
		rt = m_renderTargetAliased128;
	}
	else if (renderWidth == m_renderTargetAliased64->GetWidth() && renderHeight == m_renderTargetAliased64->GetHeight())
	{
		rt = m_renderTargetAliased64;
	}

	if (!rt)
	{
		Assertf(false, "No appropriated aliased render target for texture with dimensions %d / %d!", dstTex->GetWidth(), dstTex->GetHeight());
		return false;
	}
#endif

	PIXBegin(0,"Compress DXT1");
	{
		#if RSG_ORBIS
			grcRenderTargetGNM* ngTarget = (grcRenderTargetGNM*)rt;
			ngTarget->UpdateMemoryLocation(dstTex->GetTexturePtr());
		#elif RSG_DURANGO
			grcRenderTargetDurango* ngTarget = (grcRenderTargetDurango*)rt;
			ngTarget->UpdateMemoryLocation(dstTex->GetTexturePtr());
		#elif RSG_PC
			grcRenderTargetD3D11* ngTarget = NULL;
		#endif

#if RSG_ORBIS || RSG_DURANGO
		s32 numMips = doMips ? rage::Min(rt->GetMipMapCount(), dstTex->GetMipMapCount()) : 1;
#elif RSG_PC
		s32 numMips = doMips ? dstTex->GetMipMapCount() : 1;
#endif

        s32 numMipsDone = 0;

		//for (u32 mipIdx = 0; mipIdx < numMips; ++mipIdx)
		for (s32 mipIdx = numMips - 1; mipIdx >= 0; --mipIdx)
		{
			u32 mipWidth = dstTex->GetWidth() >> mipIdx;
			if (mipWidth <= MIP_COMPRESS_CUTOFF_SIZE)
				continue; 

            numMipsDone++;

			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
#if RSG_ORBIS || RSG_DURANGO 
            grcTextureFactory::GetInstance().LockRenderTarget(0, ngTarget, NULL, 0, false, mipIdx);
#elif RSG_PC
			u32 mipHeight = dstTex->GetHeight() >> mipIdx;
			ngTarget = (grcRenderTargetD3D11*) FindTempDxtRenderTarget(mipWidth>>2,mipHeight>>2);
			if ( !Verifyf(ngTarget != nullptr, "No appropriated temp DXT render target with dimensions %d x %d!", mipWidth>>2, mipHeight>>2) )
			{
				return false;
			}
			grcTextureFactory::GetInstance().LockRenderTarget(0, ngTarget, NULL, 0, false, 0);
#endif
			grcViewport vp;
			vp.Screen();
			grcViewport *old = grcViewport::SetCurrent(&vp);

			float floatWidth = static_cast<float>(srcRt->GetWidth() >> mipIdx);
			float floatHeight = static_cast<float>(srcRt->GetHeight() >> mipIdx);
			Vector4 texelSize(1.f / floatWidth, 1.f / floatHeight, floatWidth, floatHeight);
			s_blendShader->SetVar(s_texelSize, texelSize);
			s_blendShader->SetVar(s_texture3, srcRt);
			s_blendShader->SetVar(s_compressLod, (float)mipIdx);
			if (s_blendShader->BeginDraw(grmShader::RMC_DRAW, true, s_compressTechnique))
			{
				s_blendShader->Bind(0);

				grcDrawSingleQuadf(0, 0, (float)(renderWidth >> mipIdx), (float)(renderHeight >> mipIdx), 0, 0, 0, 1, 1, Color32(255, 255, 255, 255));

				s_blendShader->UnBind();
				s_blendShader->EndDraw();
			}
			s_blendShader->SetVar(s_compressLod, 0.f);
			s_blendShader->SetVar(s_texture3, grcTexture::None);

			grcViewport::SetCurrent(old);

			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			WIN32PC_ONLY( nMipBitMask |= 0x1<<mipIdx );
		}

		#if RSG_PC
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

				// We can't let other compression work proceed until we're done waiting for this data, otherwise we risk stomping on the data that we're waiting for
				ReserveDxtCompressor( dstTex, nMipBitMask );
			}
		#endif
	}
	PIXEnd();

	return true;
}

#if RSG_PC
bool MeshBlendManager::FinishCompressDxt( bool bStallUntilFinished )
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	PF_AUTO_PUSH_TIMEBAR("FinishCompressDxt");

	if (sysInterlockedRead((unsigned*)&m_compressionStatus) == DXT_COMPRESSOR_AVAILABLE)
		return true;

	if ( !AssertVerify( (m_pCompressionDest != nullptr) && (m_nCompressionMipStatusBits != 0x0) ) )
	{
		// Return true so that we don't cause infinite loops perpetually waiting for compression to finish. This is probably a bad sign!
		ReleaseDxtCompressor();
		return true; 
	}

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
			ReleaseDxtCompressor();
			return true;
		}

		if ( !static_cast<grcTextureD3D11*>(pSrcTex)->MapStagingBufferToBackingStore(!bStallUntilFinished) )
		{
			// Keep trying other mips
			continue;
		}
	
		grcTextureLock srcLock;
		if ( !AssertVerify( pSrcTex->LockRect(0, 0, srcLock, grcsRead) )) 
		{
			// That didn't work... we're going to bail out, this texture will never get compressed
			ReleaseDxtCompressor();
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
		ReleaseDxtCompressor();
		return true;
	}
	else
	{
		return false;
	}
}

void MeshBlendManager::ReserveDxtCompressor(grcTexture* pDest, u32 nMipStatusBits)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	Assert( pDest != nullptr );
	Assert( nMipStatusBits != 0x0 );

	if ( !AssertVerify( DXT_COMPRESSOR_AVAILABLE == sysInterlockedCompareExchange((unsigned*)&m_compressionStatus, DXT_COMPRESSOR_RESERVED, DXT_COMPRESSOR_AVAILABLE) ) )
	{
		return;
	}

	m_pCompressionDest = pDest;
	m_pCompressionDest->AddRef();
	m_nCompressionMipStatusBits = nMipStatusBits;
}

void MeshBlendManager::ReleaseDxtCompressor()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	AssertVerify( DXT_COMPRESSOR_RESERVED == sysInterlockedCompareExchange((unsigned*)&m_compressionStatus, DXT_COMPRESSOR_AVAILABLE, DXT_COMPRESSOR_RESERVED) );
	SAFE_RELEASE(m_pCompressionDest);
	m_nCompressionMipStatusBits = 0x0;
}

bool MeshBlendManager::IsDxtCompressorReserved() const 
{
	return sysInterlockedRead((unsigned*)&m_compressionStatus) == DXT_COMPRESSOR_RESERVED;
}

#endif // RSG_PC

bool MeshBlendManager::CompressDxtSTB(grcTexture* dst, grcRenderTarget* src, s32 numMips)
{
	PF_AUTO_PUSH_TIMEBAR("CompressDxtSTB");

	Assert(src->GetWidth() == dst->GetWidth());
	Assert(src->GetHeight() == dst->GetHeight());

	const grcImage::Format dstFormat      = (grcImage::Format)dst->GetImageFormat();
	const bool bIsDXT5 = (dstFormat == grcImage::DXT5); // stb only supports DXT1 and DXT5
	const int  dstStep = (16*(bIsDXT5 ? 8 : 4))/(8*4); // block size divided by 4

	int width = src->GetWidth();
	int height = src->GetHeight();

#if (RSG_PC && __D3D11)
	grcTextureD3D11* ptex = (grcTextureD3D11*)dst;
	{
		PF_AUTO_PUSH_TIMEBAR("BeginTempCPUResource");
		ptex->BeginTempCPUResource();
	}

	grcRenderTargetD3D11* pRT = (grcRenderTargetD3D11*)src;
	{
		PF_AUTO_PUSH_TIMEBAR("GenerateMipmaps");
		pRT->GenerateMipmaps();
	}
	{
		PF_AUTO_PUSH_TIMEBAR("UpdateLockableTexture");
		pRT->UpdateLockableTexture(false);
	}
#elif RSG_PC
	#error 1 && "Unknown Platform"
#endif

	int mipMapCount = numMips;
	for (int mipIdx = 0; mipIdx < mipMapCount; ++mipIdx) 
	{
		grcTextureLock srcLock;
		{
			PF_AUTO_PUSH_TIMEBAR("src->LockRect");
			if (!src->LockRect(0, mipIdx, srcLock, grcsRead)) 
				return false;
		}

		grcTextureLock dstLock;
		{
			PF_AUTO_PUSH_TIMEBAR("dst->LockRect");
			if (!dst->LockRect(0, mipIdx, dstLock, grcsWrite | grcsAllowVRAMLock))
			{
				src->UnlockRect(srcLock);
				return false;
			}
		}

		u8* dest = (u8*)dstLock.Base;
		int dstPitch = dstLock.Pitch;

		const int w = width >> mipIdx;
		const int h = height >> mipIdx;

		u8* srcTex = (u8*)srcLock.Base;

		{
			PF_AUTO_PUSH_TIMEBAR("stb_compress_dxt_block");

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
	#elif RSG_DURANGO || RSG_PC
					for (int i = 0; i < 16; i++)
					{
						const u8 tmp = temp[i].b;
						temp[i].b = temp[i].r;
						temp[i].r = tmp;
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
					stb_compress_dxt_block(dstRow + x * dstStep, (const u8*)temp, bIsDXT5 ? 1 : 0, STB_DXT_HIGHQUAL);
				}
			}
		}
#if !RSG_PC
		// no need to unlock for pc since pc doesn't need gpu copy
		dst->UnlockRect(dstLock);
#endif
		// tile the destination texture after compression
		XENON_ONLY(dst->Tile(mipIdx);)
		{
			PF_AUTO_PUSH_TIMEBAR("src->UnlockRect");
			src->UnlockRect(srcLock);
		}
	}
#if RSG_DURANGO
	static_cast<grcTextureDurango*>(dst)->TileInPlace(mipMapCount);
#endif
#if (RSG_PC && __D3D11)
	ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
	{
		PF_AUTO_PUSH_TIMEBAR("EndTempCPUResource");
		ptex->EndTempCPUResource();
	}
	{
		PF_AUTO_PUSH_TIMEBAR("UpdateLockableTexture");
		pRT->UpdateLockableTexture(true);
	}
	ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
#elif RSG_PC
	#error 1 && "Unknown Platform"
#endif
	return true;
}

bool MeshBlendManager::IsWaitingForAssets(MeshBlendHandle handle) const
{
	if (!IsHandleValid(handle))
		return false;

	u16 type = GetTypeFromHandle(handle);
	u16 index = GetIndexFromHandle(handle);
	if (m_blends[type][index].state == STATE_REQUEST_SLOT)
		return true;

    return false;
}

void MeshBlendManager::UpdateEyeColorData()
{
    const u32 eyeDim = 64;
    m_eyeColorCols = m_eyeColorRows = 0;

    if (s_eyeColorTxdIndex != -1)
	{
		fwTxd* eyeTxd = g_TxdStore.Get(s_eyeColorTxdIndex);
		if(Verifyf(eyeTxd, "Eye color texture dictionary is not loaded"))
		{
			const grcTexture* eyeTex = eyeTxd->GetEntry(0);
			if (Verifyf(eyeTex, "Missing eye color texture!"))
			{
				m_eyeColorCols = eyeTex->GetWidth() / eyeDim;
				m_eyeColorRows = eyeTex->GetHeight() / eyeDim;
				m_uStepEyeTex = 1.f / m_eyeColorCols;
				m_vStepEyeTex = 1.f / m_eyeColorRows;
			}
		}
	}
}

void MeshBlendManager::DumpSlotDebug(void)
{
#if !__FINAL
	Displayf("[MESHBLEND]");
	for(u32 j = 0;j<MBT_MAX;j++)
	{
		Displayf("OBT : %d",j);
		for(u32 i = 0;i<MAX_NUM_BLENDS;i++)
		{
			Displayf("	Slot %d",i);
			if (ms_blendSlotBacktraces[j][i] != 0)
			{
				sysStack::PrintRegisteredBacktrace(ms_blendSlotBacktraces[j][i]);
			}
			else
			{
				Displayf("	no trace");
			}
			Displayf("	---");
		}
		Displayf("	------");
	}
#endif //!__FINAL
}

BlendPalette::BlendPalette()
{
    for (s32 i = 0; i < MAX_PALETTE_ENTRIES; ++i)
        used[i] = false;

	refresh = false;
}

Color32 BlendPalette::GetColor(s8 slot, u32 colIndex)
{
    Assert(slot >= 0 && slot < MAX_PALETTE_ENTRIES);
    Assert(colIndex < MAX_PALETTE_COLORS);

    return colors[slot][colIndex];
}

void BlendPalette::SetColor(s8 slot, u32 colIndex, Color32 color)
{
    Assert(slot >= 0 && slot < MAX_PALETTE_ENTRIES);
    Assert(colIndex < MAX_PALETTE_COLORS);

    colors[slot][colIndex] = color;
	refresh = true;
}

s8 BlendPalette::GetFreeSlot()
{
    for (s8 i = 0; i < MAX_PALETTE_ENTRIES; ++i)
    {
        if (!used[i])
        {
            used[i] = true;
            return i;
        }
    }

    return -1;
}

void BlendPalette::ReleaseSlot(s8 slot)
{
    if (slot < 0 || slot >= MAX_PALETTE_ENTRIES)
        return;

    Assertf(used[slot], "Releasing unused palette slot '%d'", slot);
    used[slot] = false;
}

