//
// Filename:	PedDamage.cpp
// Description:	Handles new Ped Damage, including blood, scars,  bruises and Tattoos.
// Written by:	SteveR
//
//	2010/10/25	-	SteveR:	- initial;
//
//
//

#include "Peds/rendering/PedDamage.h"
#include "Peds/rendering/PedDamage_parser.h"

// Rage headers:
#include "fragment/type.h"
#include "fragment/drawable.h"
#include "fragment/instance.h"
#include "fragment/typechild.h"
#include "fragment/cache.h"

#include "physics/intersection.h"
#include "rline/clan/rlclancommon.h"
#include "system/param.h"
#include "grcore/allocscope.h"
#include "grcore/quads.h"
#include "grcore/image.h"
#include "grmodel/shaderfx.h"

#include "vfx/vfxutil.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "crSkeleton/Skeleton.h"
#include "crSkeleton/SkeletonData.h"
#include "crSkeleton/BoneData.h"
#include "streaming/defragmentation.h"
#include "streaming/streamingengine.h"
#include "parser/manager.h"
#include "fwdrawlist/drawlistmgr.h"

// Game headers:
#include "animation/animBones.h"
#include "camera/CamInterface.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/MarketingTools.h"
#include "frontend/WarningScreen.h"
#include "game/localisation.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Live/LiveManager.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/ped.h"
#include "Peds/PlayerInfo.h"
#include "Peds/PlayerPed.h"
#include "Peds/rendering/PedHeadshotManager.h"
#include "Peds/rendering/pedVariationPack.h"
#include "peds/rendering/PedVariationDebug.h"
#include "renderer/DrawLists/drawlistMgr.h"
#include "scene/dlc_channel.h"
#include "scene/world/gameWorld.h"
#include "Shaders/CustomShaderEffectPed.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/rendertargets.h"
#include "renderer/UI3DDrawManager.h"
#include "renderer/Util/Util.h"
#include "stats/StatsInterface.h"
#include "camera/viewports/ViewportManager.h"

#if RSG_PC
#include "grcore/texture_d3d11.h"
#endif // RSG_PC

#if __BANK
#include "system/param.h" 
PARAM(peddamagedebug,"[debug] Output Ped debuging info for compressed decorations");
PARAM(noncompressedpeddamagedebug,"[debug] Output Ped debuging info for non compressed ped damage/decorations");

PARAM(debugpeddamagetargets, "[debug] Allocate a rendertarget from streaming memory for debugging ped damage");
extern const char* parser_RagdollComponent_Strings[];
#endif

PARAM(disableuncompresseddamage, "Disable use of uncompressed textures for MP peds");

using namespace rage;

#if __BANK
bool g_DebugLockCompressionInBlittingStage = false;
#endif

//do some extra stuff that helps debugging
#define COMPRESSED_DECORATION_DEBUG __BANK && (0)
//when clearing the texture, clear with a color pattern to make it more obvious
#define CLEAR_WITH_COLOR_RECTS COMPRESSED_DECORATION_DEBUG && (0)
//show target destination textures on screen
#define DEBUG_COMPRESSED_RENDER_TARGETS COMPRESSED_DECORATION_DEBUG && (0)

#define USE_UNCOMPRESSED_MP_DECORATIONS (PARAM_disableuncompresseddamage.Get() == false WIN32PC_ONLY( && CSettingsManager::GetInstance().GetSettings().m_graphics.m_TextureQuality > 0))

#if __BANK
const char * GetPedName(const CPed * pPed)
{
	if(pPed)
	{
		if (pPed->IsNetworkPlayer() && pPed->GetNetworkObject())
			return pPed->GetNetworkObject()->GetLogName();

		if (CBaseModelInfo* pModelInfo = pPed->GetBaseModelInfo())
			return pModelInfo->GetModelName();
	}
	return "NULL";
}

#define PEDDEBUG_DISPLAYF(fmt,...) do { if (PARAM_noncompressedpeddamagedebug.Get()) Displayf("[PDD] " fmt,##__VA_ARGS__); else pedDebugf1("[PDD] " fmt,##__VA_ARGS__); } while(0)
#define CPEDDEBUG_DISPLAYF(fmt,...) do { if (PARAM_peddamagedebug.Get()) Displayf("[CPDD] " fmt,##__VA_ARGS__); else pedDebugf1("[CPDD] " fmt,##__VA_ARGS__); } while(0)
#else
	#define PEDDEBUG_DISPLAYF(...) 
	#define CPEDDEBUG_DISPLAYF(...)
#endif

namespace
{
	mthRandom	s_Random;
};
// TODO: update this map from new docs...
//
// blood rendertarget layout:
//
//	+-----------+			//		     zone			uv mapping
//	|			|			//	A  kDamageZoneTorso		1,1  to 2,2
//	|			|			//	B  kDamageZoneHead		1,3  to 2,4
//	|	  A		|			//	C  kDamageZoneLeftArm	1,5  to 2,6 
//	|			|			//	D  kDamageZoneRightArm	3,7  to 2,8
//	+--+--+--+--+			//	E  kDamageZoneLeftLeg	5,9  to 2,10
//	|			|			//	F  kDamageZoneRightLeg  7,11 to 2,12
//	|	  B		|			
//	+--+--+--+--+			// NOTE: kDamageZoneMedals maps to the whole target
//	|	  C		|		
//	+--+--+--+--+			
//	|	  D		|		
//	+--+--+--+--+			
//	|	  E		|		
//	+--+--+--+--+	
//	|	  F		|	
//	+--+--+--+--+	

//
//
//  The cylinders around the torso, arms and legs are mapped 0..1 then adjusted to the texture ranges above.
//	they are counter clockwise if looking down on the character. i.e  if 0.0 is down the left side of the torso,
//	.25 is in the middle of the back, .5 is down the right side and .75 down the front
//
//	0.0 is along the left side of the character's torso, and down the "inside seams" of the arms and legs.


static const float kTargetSlice = 1/10.0f; // the target is cut into 10 slices, 4 for torso, 2 for head and on each for limbs
static const Vector4 s_ViewportMappings[kDamageZoneNumZones] =			// {bottom x, bottom y, width, height}
{
	Vector4(0.00f, 0*kTargetSlice,	1.00f, 4*kTargetSlice),	// kDamageZoneTorso,
	Vector4(0.00f, 4*kTargetSlice,	1.00f, 2*kTargetSlice),	// kDamageZoneHead,			
	Vector4(0.00f, 6*kTargetSlice,	1.00f, 1*kTargetSlice),	// kDamageZoneLeftArm,
	Vector4(0.00f, 7*kTargetSlice,	1.00f, 1*kTargetSlice),	// kDamageZoneRightArm,
	Vector4(0.00f, 8*kTargetSlice,	1.00f, 1*kTargetSlice),	// kDamageZoneLeftLeg,
	Vector4(0.00f, 9*kTargetSlice,	1.00f, 1*kTargetSlice), // BloodZoneRightLeg,
	Vector4(0.00f, 0*kTargetSlice,	1.00f, 10*kTargetSlice), // kDamageZoneMedals,
};


atRangeArray <grcRenderTarget *,kMaxHiResBloodRenderTargets>	CPedDamageSetBase::sm_BloodRenderTargetsHiRes;
atRangeArray <grcRenderTarget *,kMaxMedResBloodRenderTargets>	CPedDamageSetBase::sm_BloodRenderTargetsMedRes;
atRangeArray <grcRenderTarget *,kMaxLowResBloodRenderTargets>	CPedDamageSetBase::sm_BloodRenderTargetsLowRes;
atRangeArray <grcRenderTarget *, 4> 							CPedDamageSetBase::sm_DepthTargets;

int					CPedDamageSetBase::sm_TargetSizeW = kTargetSizeW;
int					CPedDamageSetBase::sm_TargetSizeH = kTargetSizeH;

grcRenderTarget *	CPedDamageSetBase::sm_MirrorDecorationRT=NULL;
grcRenderTarget *	CPedDamageSetBase::sm_MirrorDepthRT=NULL;

Mat34V		 		CPedDamageSetBase::sm_ZoneCameras[kDamageZoneNumZones][2];
grmShader *			CPedDamageSetBase::sm_Shader = NULL;

grcEffectTechnique	CPedDamageSetBase::sm_DrawBloodTechniques[3]	= {grcetNONE,grcetNONE,grcetNONE};
grcEffectTechnique	CPedDamageSetBase::sm_DrawSkinBumpTechnique		= grcetNONE;
grcEffectTechnique	CPedDamageSetBase::sm_DrawDecorationTechnique[2]= {grcetNONE,grcetNONE};

grcEffectVar		CPedDamageSetBase::sm_BlitTextureID = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitWoundTextureID = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitSplatterTextureID = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitNoBorderTextureID = grcevNONE;

grcEffectVar		CPedDamageSetBase::sm_BlitSoakFrameInfo = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitWoundFrameInfo = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitSplatterFrameInfo = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitDecorationFrameInfoID = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitDecorationTintPaletteSelID = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitDecorationTintPaletteTexID = grcevNONE;
grcEffectVar		CPedDamageSetBase::sm_BlitDecorationTattooAdjustID = grcevNONE;

grcVertexDeclaration * CPedDamageSetBase::sm_VertDecl;

#if __BANK
u16					CPedDamageSetBase::sm_DebugRTPool = kRTPoolIDInvalid;
grcRenderTarget *	CPedDamageSetBase::sm_DebugTarget = NULL;
#endif
float *				CPedDamageSetBase::sm_ActiveVertexBuffer[NUMBER_OF_RENDER_THREADS] = { NUMBER_OF_RENDER_THREADS_INITIALIZER_LIST(NULL) };

bool				CPedDamageSetBase::sm_Initialized=false;

static bank_float s_StabWidth = 0.025f;
static bank_float s_StabSoakScale = 3.0;

static bank_float s_TorsoSizeBoost = 2.0f;
static bank_float s_LimbSizeBoost = 1.5f;

static bank_float s_CloseThreshold2 = 0.02f*0.02f;
 
static bank_bool  s_AddExtraBlitsAtSeams = false;  // off for now, it makes a lot of extra blits and often looks like double hits.

#if __BANK
static bool		s_DebugBloodHits = false;
static Vector3	s_LastHitPos = VEC3_ZERO;
static int		s_DebugTargetIndex = 0;

static const char * s_ZoneEmumNames[] = {"kDamageZoneTorso", "kDamageZoneHead", "kDamageZoneLeftArm", "kDamageZoneRightArm", "kDamageZoneLeftLeg", "kDamageZoneRightLeg"};
static const char * s_ScriptZoneEmumNames[] = {"PDZ_TORSO", "PDZ_HEAD", "PDZ_LEFT_ARM", "PDZ_RIGHT_ARM", "PDZ_LEFT_LEG",	"PDZ_RIGHT_LEG", "PDZ_MEDALS", "PDZ_INVALID"};
static const char * s_DamageTypeNames[] = {"Blood","Stab","SkinDecoration","SkinBumpDecoration","ClothDecoration","ClothBumpDecoration"};
static const char * s_ZoneCommonNames[] = {"Torso", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg", "Invalid"};
static const char * s_CompressedPedDamageSetState[] = {"kInvalid", "kCompressed", "kWaitingOnTextures", "kTexturesLoaded", "kWaitingOnBlit", "kBlitting",	"kWaitingOnCompression"};

static bool s_DebugDrawPushZones = false;
static bool s_TestFreezeFade = false;
#endif

enum {kClearDamage=0x1, kClearDecorations=0x2, kClearAll = kClearDamage|kClearDecorations};

enum {kPlayerTarget = 0};

//////////////////////////////////////////////////////////////////////////
//
void PedDamageDecorationTintInfo::Copy(const PedDamageDecorationTintInfo & other)
{
	headBlendHandle		= other.headBlendHandle;
	paletteSelector		= other.paletteSelector;
	paletteSelector2	= other.paletteSelector2;
	pPaletteTexture		= other.pPaletteTexture;
	bValid				= other.bValid;
	bReady				= other.bReady;
}

//////////////////////////////////////////////////////////////////////////
//
PedDamageDecorationTintInfo & PedDamageDecorationTintInfo::operator=(const PedDamageDecorationTintInfo & other)
{
	Copy(other);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
//
void PedDamageDecorationTintInfo::Reset()
{
	PEDDEBUG_DISPLAYF("PedDamageDecorationTintInfo::Reset called");
	headBlendHandle = MESHBLEND_HANDLE_INVALID;
	paletteSelector = -1;
	paletteSelector2 = -1;
	pPaletteTexture = NULL;
	bReady = false;
	bValid = false;
}

//
// CPedDamageSetBase
//
u16 CPedDamageSetBase::AllocateStreamedPool(int hSize, int vSize, int texCount, const char *name)
{
	grcRTPoolCreateParams poolParams;
	int bpp = 32;

	poolParams.m_Size =  hSize*vSize*(bpp/8)*texCount; 
	poolParams.m_HeapCount = 3;
	poolParams.m_MaxTargets = 64;

	// strStreamingEngine::GetAllocator() uses buddy heap, which does not respect alignement
	int paddedSize  = (int)poolParams.m_Size + grcRenderTargetPoolEntry::kDefaultAlignment;
	int fakeAlignment = grcRenderTargetPoolEntry::kDefaultAlignment;  // avoid an assert in the allocator

	USE_MEMBUCKET(MEMBUCKET_DEBUG); 
	MEM_USE_USERDATA(MEMUSERDATA_DEBUGSTEAL);
	void * mem = strStreamingEngine::GetAllocator().Allocate(paddedSize, fakeAlignment, MEMTYPE_RESOURCE_PHYSICAL);
	
	mem = (void*)(((size_t)mem+grcRenderTargetPoolEntry::kDefaultAlignment) & ~(grcRenderTargetPoolEntry::kDefaultAlignment-1)); // force alignment

	if (Verifyf(mem,"%s allocation from streaming memory failed",name))	
		return grcRenderTargetPoolMgr::CreatePool(name, poolParams, mem);// create a pool using the resource memory the memory from the resource heap.
	else
		return kRTPoolIDInvalid;
}

void CPedDamageSetBase::AllocateTextures()
{

	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = false;
	params.IsResolvable = true;

	params.ColorExpBias = 0;
	params.IsSRGB = false; // makes fading not work well
	params.IsRenderable = true;
	params.AllocateFromPoolOnCreate = false;


	params.Format = grctfA8R8G8B8;
	int bpp = 32;

#if RSG_PC

	u32 dimension = (u32) rage::Sqrtf((float)(GRCDEVICE.GetWidth() * GRCDEVICE.GetHeight()));

	s32 iMipLevels = 0;
	while (dimension)
	{
		dimension >>= 1;
		iMipLevels++;
	}
	iMipLevels = Max(1, (iMipLevels - 8));
	sm_TargetSizeW = Min((int)(CPedDamageSetBase::kTargetSizeW / 4) * iMipLevels, (int)CPedDamageSetBase::kTargetSizeW);
	sm_TargetSizeH = Min((int)(CPedDamageSetBase::kTargetSizeH / 4) * iMipLevels, (int)CPedDamageSetBase::kTargetSizeH);
#endif
	int vSize = sm_TargetSizeW*2; // swap the width and height to take advantage of the ps3 Stride
	int hSize = sm_TargetSizeH*2;

	char targetName[256];

	// allocate high resolution render targets for the player only damage and decorations
	Assert (kMaxHiResBloodRenderTargets==2);
	for (int i=0;i<kMaxHiResBloodRenderTargets;i++)
	{
		formatf(targetName,sizeof(targetName),"BloodTargetHiRes%d",i);
		sm_BloodRenderTargetsHiRes[i] = grcTextureFactory::GetInstance().CreateRenderTarget(targetName, grcrtPermanent, hSize, vSize, bpp, &params WIN32PC_ONLY(, sm_BloodRenderTargetsHiRes[i]));
		sm_BloodRenderTargetsHiRes[i]->AllocateMemoryFromPool();
	}

	hSize = sm_TargetSizeH; // Side ways makes it more like the ps3 targets (except the last 2)
	vSize = sm_TargetSizeW;

	// allocate medium resolution render targets
	for (int i=0;i<kMaxMedResBloodRenderTargets;i++)
	{
		formatf(targetName,sizeof(targetName),"BloodTargetMedRes%d",i);
		sm_BloodRenderTargetsMedRes[i] = grcTextureFactory::GetInstance().CreateRenderTarget(targetName, grcrtPermanent, hSize, vSize, bpp, &params WIN32PC_ONLY(, sm_BloodRenderTargetsMedRes[i]));
		sm_BloodRenderTargetsMedRes[i]->AllocateMemoryFromPool();
	}

	// allocate low resolution render targets (these overlap with the medium res targets...)
	hSize = sm_TargetSizeH/2;
	vSize = sm_TargetSizeW/2;
	
	for (int i=0;i<kMaxLowResBloodRenderTargets;i++)
	{
		formatf(targetName,sizeof(targetName),"BloodTargetLowRes%d",i);
		sm_BloodRenderTargetsLowRes[i] = grcTextureFactory::GetInstance().CreateRenderTarget(targetName, grcrtPermanent, hSize, vSize, bpp, &params WIN32PC_ONLY(, sm_BloodRenderTargetsLowRes[i]));
		sm_BloodRenderTargetsLowRes[i]->AllocateMemoryFromPool();
	}
	
	// allocate rendertargets for the mirror pass.
	hSize = sm_TargetSizeH/2;	// side ways small, so 2 can fit side by side
	vSize = sm_TargetSizeW/2;

	params.Format		= grctfNone;
	sm_MirrorDepthRT = grcTextureFactory::GetInstance().CreateRenderTarget("PedMirrorDepth", grcrtDepthBuffer, hSize*2, vSize, 32, &params WIN32PC_ONLY(, sm_MirrorDepthRT));

	sm_MirrorDepthRT->AllocateMemoryFromPool(); //360 does not need to allocate mem for non resolvable target

	params.Format = grctfA8R8G8B8;
	sm_MirrorDecorationRT = grcTextureFactory::GetInstance().CreateRenderTarget("Mirror Ped Decoration Target", grcrtPermanent, hSize*2, vSize, bpp, &params WIN32PC_ONLY(, sm_MirrorDecorationRT));
	sm_MirrorDecorationRT->AllocateMemoryFromPool();

	params.Parent = NULL;

	// Allocate depth buffers for the stencil work. these are throw away and can alias to the main depth buffer memory
#if __D3D11 || RSG_ORBIS
	params.Format				= grctfNone;
	params.UseFloat				= true;
#endif

	int rotatedDepthTargetWidth = sm_TargetSizeH;
	int rotatedDepthTargetHeight = sm_TargetSizeW;

	sm_DepthTargets[0] = grcTextureFactory::GetInstance().CreateRenderTarget("PedDamageLowResDepth", grcrtDepthBuffer, rotatedDepthTargetWidth/2, rotatedDepthTargetHeight/2, 32, &params WIN32PC_ONLY(, sm_DepthTargets[0]));
	sm_DepthTargets[0]->AllocateMemoryFromPool();
	sm_DepthTargets[1] = grcTextureFactory::GetInstance().CreateRenderTarget("PedDamageMedResDepth", grcrtDepthBuffer, rotatedDepthTargetWidth, rotatedDepthTargetHeight, 32, &params WIN32PC_ONLY(, sm_DepthTargets[1]));
	sm_DepthTargets[1]->AllocateMemoryFromPool();
	sm_DepthTargets[2] = grcTextureFactory::GetInstance().CreateRenderTarget("PedDamageHiResDepth", grcrtDepthBuffer, sm_TargetSizeH*2, sm_TargetSizeW*2, 32, &params WIN32PC_ONLY(, sm_DepthTargets[2]));
	sm_DepthTargets[2]->AllocateMemoryFromPool();
	
	// the 3rd one needs to be the compressed depth buffer so GetCompressedDepthTarget() works
	sm_DepthTargets[3] = grcTextureFactory::GetInstance().CreateRenderTarget("PedDamagedCompressedDepth", grcrtDepthBuffer, PEDDECORATIONBUILDER.GetTargetWidth(),PEDDECORATIONBUILDER.GetTargetHeight(), 32, &params WIN32PC_ONLY(, sm_DepthTargets[3]));
	sm_DepthTargets[3]->AllocateMemoryFromPool();

#if __BANK
	if (PARAM_debugpeddamagetargets.Get())
	{
		hSize = sm_TargetSizeW;
		vSize = sm_TargetSizeH;

		sm_DebugRTPool = AllocateStreamedPool(hSize*2,vSize,1,"PedDamageDebugPool");
		
		params.IsResolvable		= true;
		params.Format			= grctfA8R8G8B8;
		params.PoolID			= sm_DebugRTPool;
		params.PoolHeap			= 0;
		params.UseFloat			= false;

		// 2x wide to hold damage and decoration target images
		sm_DebugTarget = grcTextureFactory::GetInstance().CreateRenderTarget("PedDamageDebug", grcrtPermanent, hSize*2, vSize, bpp, &params WIN32PC_ONLY(, sm_DebugTarget));
		sm_DebugTarget->AllocateMemoryFromPool();
	}
#endif

	sm_Initialized = true;
}

void CPedDamageSetBase::ReleaseTextures()
{
	for (int i=0;i<kMaxHiResBloodRenderTargets;i++)
	{
		if (sm_BloodRenderTargetsHiRes[i])
		{
			sm_BloodRenderTargetsHiRes[i]->Release();
			sm_BloodRenderTargetsHiRes[i] = NULL;
		}
	}

	for (int i=0;i<kMaxMedResBloodRenderTargets;i++)
	{
		if (sm_BloodRenderTargetsMedRes[i])	
		{
			sm_BloodRenderTargetsMedRes[i]->Release();
			sm_BloodRenderTargetsMedRes[i] = NULL;
		}
	}

	for (int i=0;i<kMaxLowResBloodRenderTargets;i++)
	{
		if (sm_BloodRenderTargetsLowRes[i]) 
		{
			sm_BloodRenderTargetsLowRes[i]->Release();
			sm_BloodRenderTargetsLowRes[i] = NULL;
		}
	}
	
	if (sm_MirrorDecorationRT)
	{
		sm_MirrorDecorationRT->Release();
		sm_MirrorDecorationRT = NULL;
	}

	if (sm_MirrorDepthRT)
	{
		sm_MirrorDepthRT->Release();
		sm_MirrorDepthRT = NULL;
	}

	for (int i=0;i<sm_DepthTargets.GetMaxCount();i++)
	{
		if (sm_DepthTargets[i])
		{
			sm_DepthTargets[i]->Release();
			sm_DepthTargets[i] = NULL;
		}
	}

#if __BANK
	if (sm_DebugTarget)
	{
		sm_DebugTarget->Release();
		sm_DebugTarget = NULL;
	}
	if (sm_DebugRTPool!=kRTPoolIDInvalid)
	{
		grcRenderTargetPoolMgr::DestroyPool(sm_DebugRTPool);
		sm_DebugRTPool = kRTPoolIDInvalid;
	}
#endif

}

void CPedDamageSetBase::DeviceLost()
{
	// Ped decoration builder render target sizes are not dependent on resolution atm (see CompressedDecorationBuilder::GetTargetWidth() etc).
	// This is because we cannot change the targets whilst a decoration build/job is in flight (See CompressedDecorationBuilder::ProcessQueue() & CompressedDecorationBuilder::ProcessCurrentState()).
	//PEDDECORATIONBUILDER.ReleaseRenderTarget();
	//PEDDECORATIONBUILDER.GetTextureManager().ReleaseRenderTargets();
}

void CPedDamageSetBase::DeviceReset()
{
	if (sm_Initialized)
		AllocateTextures();
	// See comments above.
	//PEDDECORATIONBUILDER.AllocateRenderTarget();
	//PEDDECORATIONBUILDER.GetTextureManager().AllocateRenderTargets(PEDDECORATIONBUILDER.GetTargetWidth(), PEDDECORATIONBUILDER.GetTargetHeight());
}

void CPedDamageSetBase::InitShaders()
{
	sm_Shader = grmShaderFactory::GetInstance().Create(); 
	Assert(sm_Shader);

	sm_Shader->Load("damage_blits"); 
	sm_DrawBloodTechniques[0] = sm_Shader->LookupTechnique("DrawBloodSoak");
	sm_DrawBloodTechniques[1] = sm_Shader->LookupTechnique("DrawBloodSplatter");
	sm_DrawBloodTechniques[2] = sm_Shader->LookupTechnique("DrawBloodWound");
	
	sm_DrawSkinBumpTechnique		 = sm_Shader->LookupTechnique("DrawSkinBump");
	sm_DrawDecorationTechnique[0]	 = sm_Shader->LookupTechnique("DrawDecoration");
	sm_DrawDecorationTechnique[1]	 = sm_Shader->LookupTechnique("DrawDecorationNoTex");

	sm_BlitTextureID = sm_Shader->LookupVar("DiffuseTex");
	sm_BlitWoundTextureID = sm_Shader->LookupVar("DiffuseWoundTex");
	sm_BlitNoBorderTextureID = sm_Shader->LookupVar("DiffuseNoBorderTex");
	sm_BlitSplatterTextureID = sm_Shader->LookupVar("DiffuseSplatterTex");

	sm_BlitSoakFrameInfo = sm_Shader->LookupVar("SoakFrameInfo");
	sm_BlitWoundFrameInfo = sm_Shader->LookupVar("WoundFrameInfo");
	sm_BlitSplatterFrameInfo = sm_Shader->LookupVar("SplatterFrameInfo");
	sm_BlitDecorationFrameInfoID = sm_Shader->LookupVar("DecorationFrameInfo");
	sm_BlitDecorationTintPaletteSelID = sm_Shader->LookupVar("DecorationTintPaletteSel");
	sm_BlitDecorationTintPaletteTexID = sm_Shader->LookupVar("DecorationTintPaletteTex");
	sm_BlitDecorationTattooAdjustID = sm_Shader->LookupVar("DecorationTattooAdjust");

	static grcVertexElement bloodElem[] = {
		grcVertexElement(0, grcVertexElement::grcvetPosition,	0, 12, grcFvf::grcdsFloat3),
		grcVertexElement(0, grcVertexElement::grcvetColor,		0, 4,  grcFvf::grcdsColor)
	};

	sm_VertDecl = GRCDEVICE.CreateVertexDeclaration(bloodElem,sizeof(bloodElem)/sizeof(grcVertexElement));
	Assertf(sm_VertDecl->Stream0Size*8*kMaxBloodDamageBlits <= GPU_VERTICES_MAX_SIZE, "largest possible batch of blood polys will not fit in a single BeginVertices() batch" );
}


void CPedDamageSetBase::InitViewports()
{
	for (int i=0;i<kDamageZoneNumZones;i++)
	{
		sm_ZoneCameras[i][0].SetIdentity3x3();
		sm_ZoneCameras[i][1].SetIdentity3x3();
	}

	Matrix34 camMtx; 
	// legs and arms are rotated 90
	camMtx.a.Set(0.0f, 1.0f, 0.0f);
	camMtx.b.Set(-1.0f,0.0f, 0.0f);
	camMtx.c.Set(0.0f, 0.0f, 1.0f);
	camMtx.d.Set(1.0f, 0.0f, 0.0f);
	for (int i=kDamageZoneLeftArm;i<=kDamageZoneRightLeg;i++)
	{
		sm_ZoneCameras[i][0] = RCC_MAT34V(camMtx);
	}

	// some targets are rotated 90 to take advantage for ps3 stride
	// Head and chest are rotated 90
	for (int i=kDamageZoneTorso;i<=kDamageZoneHead;i++)
	{
		sm_ZoneCameras[i][1] = RCC_MAT34V(camMtx);
	}
	sm_ZoneCameras[kDamageZoneMedals][1] = RCC_MAT34V(camMtx);

	// legs and arms are rotated 180
	camMtx.a.Set(-1.0f, 0.0f, 0.0f);
	camMtx.b.Set(0.0f, -1.0f, 0.0f);
	camMtx.c.Set(0.0f,  0.0f, 1.0f);
	camMtx.d.Set(1.0f,  1.0f, 0.0f);
	for (int i=kDamageZoneLeftArm;i<=kDamageZoneRightLeg;i++)
	{
		sm_ZoneCameras[i][1] = RCC_MAT34V(camMtx);
	}
}

void CPedDamageSetBase::ReleaseViewports()
{
}

inline s32 GetBoneIndexFromBoneTag(CPed * pPed, eAnimBoneTag bone)
{
	return pPed->GetBoneIndexFromBoneTag(bone);
}

bool CPedDamageSetBase::SetPed(CPed * pPed)
{
	if( pPed != m_pPed && m_pPed!=NULL )
	{
		ReleasePed(true); // release any previous associated ped
	}
	m_BirthTimeStamp = TIME.GetElapsedTime();

	if (Verifyf(pPed->GetSkeleton(),"CPedDamageSet::SetPed() - Ped does not have a skeleton"))	
	{
		m_pPed = pPed;	   
		const crSkeletonData &pSkelData	= pPed->GetSkeletonData();
		
		m_HasDress = false;
		// need to figure out if it has a dress.
		// well crap, there does not seem to be a way, so for now assume all females have dresses (oh I'm going to get some crap for this...)
		
		fwModelId pedModelId = pPed->GetModelId();
		if (pedModelId.IsValid())
		{
			if (const CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId))
				m_HasDress = !pPedModelInfo->IsMale();
		}

		u8 type = HUMAN;
		if (pPed->HasAnimalAudioParams() && pPed->GetFootstepHelper().GetModelPhysicsParams())
		{
			// quickest way to find out animal type?
			type = pPed->GetFootstepHelper().GetModelPhysicsParams()->PedType;
		}
		CPedDamageCylinderInfoSet *pSet = PEDDAMAGEMANAGER.GetDamageData().GetCylinderInfoSetByIndex(type);
		if( pSet == NULL )
			return false;
		CPedDamageCylinderInfoSet &cSet = *pSet;
		if (!Verifyf(cSet.type == type, "PEDDAMAGE Cylinder parsing error: animal type mismatch"))
			return false;

		bool failed = false;
		for(int i = 0; i < kDamageZoneMedals; ++i)	
		{
#define SET_BONE_INDEX(index,tag) s32 index = GetBoneIndexFromBoneTag(pPed,tag); if (index < 0) failed = true;
			SET_BONE_INDEX(bone1,cSet[i].bone1)
			SET_BONE_INDEX(bone2,cSet[i].bone2)
			SET_BONE_INDEX(bone3,cSet[i].topBone)

			if (failed)  // the "ped" was missing some bones, probably an animal...
				return false;
			
			m_Zones[i].SetFromSkelData( pSkelData, bone1, bone2, bone3, i, cSet[i]);
		}
		m_Zones[kDamageZoneMedals].Set(Vector3(0.0f,0.0f,0.0f), Vector3(0.0f,1.0f,0.0f),(PI*2), 1.0f, 0.0f, kDamageZoneMedals);	

		return true;
	}
	
	return false;
}


bool IsDualTarget(const grcTexture * target)  
{
	if (target)
	{
		int width = target->GetWidth();
		int height = target->GetHeight();
		return (width>height && width>height*3); 
	}
	return false;
}


void CPedDamageSetBase::SetupViewport(grcViewport *vp, int zone, bool sideWays, bool dualTarget, int pass)
{
	float width = float(GRCDEVICE.GetWidth());
	float height = float(GRCDEVICE.GetHeight());

	if (zone==kDamageZoneMedals) // flip medals on there side to use the space better in the longish RT
		sideWays = !sideWays;

	vp->Ortho(0.0,1.0f,1.0f,0.0f, -1.0f,1.0f);
	vp->SetCameraMtx(sm_ZoneCameras[zone][sideWays?1:0]);

	float dualScale = 1.0f;
	float dualOffset = 0.0f;

	if (dualTarget)
	{
		dualScale = .5f;
		if (pass==0)
			dualOffset = 0.5f;
	}

	grcViewport::SetCurrent(vp); // Should we set this after we change the SetWindow?

	// fractional offsets that help keep the damage from shifting due to the Render target resolution changes or rotations
	static const float  sidewaysHeightAdjust = 0.0f;
	static const float  sidewaysWidthAdjust = 0.0f;
	static const float  heightAdjust = 0.0f;
	static const float  widthAdjust = 0.0f;

	if (sideWays) // rotated 
	{
		float halfTexelW = sidewaysWidthAdjust/width;
		float halfTexelH = sidewaysHeightAdjust/height;
		
		float windowWidth = width*(s_ViewportMappings[zone].w*dualScale);

		vp->Ortho(halfTexelW + 0.0f, halfTexelW +1.0f + 1.0f/windowWidth, halfTexelH + 1.0f, halfTexelH + 0.0f, -1.0f, 1.0f);			
		
		vp->SetWindow(	int(width*(s_ViewportMappings[zone].y*dualScale+dualOffset))+1,
						int(height*s_ViewportMappings[zone].x),
						int(windowWidth)-1,
						int(height*s_ViewportMappings[zone].z),
						0.0f,1.0f);
	}
	else
	{
		float halfTexelW = widthAdjust/width;
		float halfTexelH = heightAdjust/height;

		float windowHeight = height*(s_ViewportMappings[zone].w*dualScale);

		vp->Ortho(halfTexelW + 0.0f, halfTexelW + 1.0f, halfTexelH + 1.0f + 1.0f/windowHeight, halfTexelH + 0.0f, -1.0f, 1.0f);			
	
		vp->SetWindow(	int(width*(s_ViewportMappings[zone].x*dualScale+dualOffset)),
						int(height*s_ViewportMappings[zone].y)+1,
						int(width*(s_ViewportMappings[zone].z*dualScale)),
						int(windowHeight)-1,
						0.0f,1.0f);
	}
}


void CPedDamageSetBase::SetBloodBlitShaderVars(const CPedDamageTexture & soakTextureinfo, const CPedDamageTexture & woundTextureinfo, const CPedDamageTexture & splatterTextureinfo)
{
	sm_Shader->SetVar(sm_BlitTextureID, soakTextureinfo.m_pTexture?soakTextureinfo.m_pTexture:grcTexture::NoneBlack);
	sm_Shader->SetVar(sm_BlitWoundTextureID, woundTextureinfo.m_pTexture?woundTextureinfo.m_pTexture:grcTexture::NoneBlack);
	sm_Shader->SetVar(sm_BlitSplatterTextureID, splatterTextureinfo.m_pTexture?splatterTextureinfo.m_pTexture:grcTexture::NoneBlack);

	// should we change these to Vector2's or pack in 2 in a vector4 +Vector2?
	sm_Shader->SetVar(sm_BlitSoakFrameInfo, Vector4((float)soakTextureinfo.m_TexGridRows,(float)soakTextureinfo.m_TexGridCols, 0.0f, 0.0f));
	sm_Shader->SetVar(sm_BlitWoundFrameInfo, Vector4((float)woundTextureinfo.m_TexGridRows,(float)woundTextureinfo.m_TexGridCols, 0.0f, 0.0f));
	sm_Shader->SetVar(sm_BlitSplatterFrameInfo, Vector4((float)splatterTextureinfo.m_TexGridRows,(float)splatterTextureinfo.m_TexGridCols, 0.0f, 0.0f));
}   


void CPedDamageSetBase::ResetBloodBlitShaderVars()
{
	// "unbind" samplers
	sm_Shader->SetVar(sm_BlitTextureID, grcTexture::NoneBlack);
	sm_Shader->SetVar(sm_BlitWoundTextureID, grcTexture::NoneBlack);
	sm_Shader->SetVar(sm_BlitSplatterTextureID, grcTexture::NoneBlack);
}

void CPedDamageSetBase::SetupHiStencil(bool UNUSED_PARAM(forReading))
{
	grcStateBlock::Flush(); // flush, since we're playing with unmanaged state here	
}

void CPedDamageSetBase::DisableHiStencil()
{		
}

grcRenderTarget *CPedDamageSetBase::GetDepthTarget(const grcRenderTarget * colorTarget)
{
	int width = colorTarget->GetWidth();
	int height = colorTarget->GetHeight();

	int index = 2; // default to hires sideways

	if (width>height) // sideways
	{
		if (width==sm_TargetSizeH/2) index = 0;
		else if (width==sm_TargetSizeH) index = 1;
	}
	else
	{
		// up right (only low and medium to worry about)
		if (width==sm_TargetSizeW/2 && height==sm_TargetSizeH/2) index = 0;
		else if (width==sm_TargetSizeW && height==sm_TargetSizeH) index = 1;
		else if (width==PEDDECORATIONBUILDER.GetTargetWidth() && height==PEDDECORATIONBUILDER.GetTargetHeight()) index = 3;
		else {Assertf(0,"No depth buffer for for ped damage color target is %dx%d",width,height);}
	}

	return sm_DepthTargets[index];
}


inline void _SetVert(float * &vbuffer, float x, float y, float z, u32 color)
{
	*vbuffer++ = x;
	*vbuffer++ = y;
	*vbuffer++ = z;
	*(u32*)vbuffer++ = color; // frame info could be stored here saving an extra float write.
}


void CPedDamageSetBase::ProcessSkinAlpha(grcViewport & orthoViewport)
{
	grcViewport::SetCurrent(&orthoViewport);
	
	grcStateBlock::SetBlendState(CPedDamageManager::GetSkinDecorationProcessBlendState());
	grcStateBlock::SetDepthStencilState(CPedDamageManager::GetUseStencilState(), 0x1);
	CPedDamageSetBase::SetupHiStencil(true);			

	// draw a fast fullscreen quad
	if (sm_Shader->BeginDraw(grmShader::RMC_DRAW, FALSE, CPedDamageSetBase::sm_DrawDecorationTechnique[1]))
	{	
		sm_Shader->Bind(0);
		GRCDEVICE.SetVertexDeclaration(CPedDamageSetBase::sm_VertDecl);  

		grcDrawMode drawMode = API_QUADS_SUPPORT ? drawQuads : drawTriStrip;
		float* ptr = (float*)GRCDEVICE.BeginVertices(drawMode, 4 ,CPedDamageSetBase::sm_VertDecl->Stream0Size);

		if (Verifyf(ptr, "GRCDEVICE.BeginVertices failed"))
		{
			_SetVert(ptr, 0.0f, 0.0f, 0.0f, 0xfffffff);
			_SetVert(ptr, 0.0f, 1.0f, 0.0f, 0xfffffff);
#if API_QUADS_SUPPORT
			_SetVert(ptr, 1.0f, 1.0f, 0.0f, 0xfffffff);
			_SetVert(ptr, 1.0f, 0.0f, 0.0f, 0xfffffff);
#else			
			_SetVert(ptr, 1.0f, 0.0f, 0.0f, 0xfffffff);
			_SetVert(ptr, 1.0f, 1.0f, 0.0f, 0xfffffff);
#endif
		}

		GRCDEVICE.EndVertices();
		sm_Shader->UnBind();
		sm_Shader->EndDraw();
	}
}

void CPedDamageSetBase::SetDecorationBlitShaderVars( const grcTexture *tex,  const grcTexture *palTex, const Vector3 & palSel, const CPedDamageTexture * pTextureInfo,  const CPedDamageTexture * pBumpTextureInfo, bool bIsTattoo)
{
	sm_Shader->SetVar(sm_BlitNoBorderTextureID, tex);
	sm_Shader->SetVar(sm_BlitDecorationTattooAdjustID, bIsTattoo ? PEDDAMAGEMANAGER.GetDamageData().m_TattooTintAdjust : Vector4(1.0f,1.0f,1.0f,0.0f));

	Vector4 frameInfo;
	if (pTextureInfo)
		frameInfo.Set((float)pTextureInfo->m_TexGridRows,(float)pTextureInfo->m_TexGridCols, 0.0f, 0.0f);
	else
		frameInfo.Set(1.0f, 1.0f, 0.0f, 0.0f);

	sm_Shader->SetVar(sm_BlitDecorationFrameInfoID, frameInfo);

	sm_Shader->SetVar(sm_BlitDecorationTintPaletteTexID, palTex?palTex:grcTexture::NoneBlack);
	sm_Shader->SetVar(sm_BlitDecorationTintPaletteSelID, palSel);

	if (pBumpTextureInfo && pBumpTextureInfo->m_pTexture) // if we have a bump texture we need to pass that in as well.
	{
		sm_Shader->SetVar(sm_BlitTextureID, pBumpTextureInfo->m_pTexture); 
		
		if (pTextureInfo)
			frameInfo.Set((float)pBumpTextureInfo->m_TexGridRows,(float)pBumpTextureInfo->m_TexGridCols, 0.0f, 0.0f);
		else
			frameInfo.Set(1.0f, 1.0f, 0.0f, 0.0f);

		sm_Shader->SetVar(sm_BlitWoundFrameInfo, frameInfo); // reusing wound info shader var
	}
}


void CPedDamageSetBase::ResetDecorationBlitShaderVars()
{
	// "unbind" samplers
	sm_Shader->SetVar(sm_BlitNoBorderTextureID, grcTexture::NoneBlack);
	sm_Shader->SetVar(sm_BlitDecorationTintPaletteTexID, grcTexture::NoneBlack);
}

void CPedDamageSetBase::AdjustPedDataForMirror(Vec4V_InOut damageData)
{
	// adjust the ped data for the mirror target (dual sideways)
	damageData.SetZ(ScalarV(1.75f));
	damageData.SetW(ScalarV(sm_MirrorDecorationRT->GetHeight() + 1.0f/sm_MirrorDecorationRT->GetHeight()));
}

// draws a blit between 2 endpoints, adding normal and direction vectors.
int CPedDamageSetBase::DrawBlit(const Vector2 & endPoints, const Vector2 & dir, const Vector2 &normal, bool horizontalFlip, bool verticalFlip, u32 color, float frame)
{
	// TODO: should be vectorized (or put into alternate scream and instanced...)
	float x0 = endPoints.x - dir.x+normal.x;
	float x1 = endPoints.x - dir.x-normal.x;  
	float x2 = endPoints.x + dir.x-normal.x;
	float x3 = endPoints.x + dir.x+normal.x; 
	
	float y0 = endPoints.y - dir.y+normal.y;
	float y1 = endPoints.y - dir.y-normal.y;  
	float y2 = endPoints.y + dir.y-normal.y;
	float y3 = endPoints.y + dir.y+normal.y; 

	Color32 temp; temp.SetBytes(0,0,1,0);
	u32 UVOneBlue = temp.GetDeviceColor();

	u32 colorAndUVIndexBase  = color + UVOneBlue*(horizontalFlip?3:0);   // blue channel is used for our uv index (count either 0,1,2,3 or 3,2,1,0)
	s32 UVInc = (horizontalFlip) ? -(s32)UVOneBlue : (s32)UVOneBlue;

	if (verticalFlip)
		colorAndUVIndexBase += UVOneBlue*4;

	// todo: convert this to use index vert buffer, push down the normal, end points, etc, then let the shader do all the work.

	const unsigned rti = g_RenderThreadIndex;
#if !API_QUADS_SUPPORT
	_SetVert(sm_ActiveVertexBuffer[rti], x0, y0, frame, colorAndUVIndexBase + UVInc*0);	
#endif
	_SetVert(sm_ActiveVertexBuffer[rti], x0, y0, frame, colorAndUVIndexBase + UVInc*0);	
	_SetVert(sm_ActiveVertexBuffer[rti], x1, y1, frame, colorAndUVIndexBase + UVInc*1);
#if API_QUADS_SUPPORT
	_SetVert(sm_ActiveVertexBuffer[rti], x2, y2, frame, colorAndUVIndexBase + UVInc*2);
	_SetVert(sm_ActiveVertexBuffer[rti], x3, y3, frame, colorAndUVIndexBase + UVInc*3);
#else
	_SetVert(sm_ActiveVertexBuffer[rti], x3, y3, frame, colorAndUVIndexBase + UVInc*3);
	_SetVert(sm_ActiveVertexBuffer[rti], x2, y2, frame, colorAndUVIndexBase + UVInc*2);
	_SetVert(sm_ActiveVertexBuffer[rti], x2, y2, frame, colorAndUVIndexBase + UVInc*2);
#endif
	

	
	if (x0 >= 1.0f || x1 >= 1.0f || x2 >= 1.0f || x3 >= 1.0f) // part of us crossed the right side
	{
#if !API_QUADS_SUPPORT
		_SetVert(sm_ActiveVertexBuffer[rti], x0-1, y0, frame, colorAndUVIndexBase + UVInc*0); 
#endif
		_SetVert(sm_ActiveVertexBuffer[rti], x0-1, y0, frame, colorAndUVIndexBase + UVInc*0); 
		_SetVert(sm_ActiveVertexBuffer[rti], x1-1, y1, frame, colorAndUVIndexBase + UVInc*1);
#if API_QUADS_SUPPORT
		_SetVert(sm_ActiveVertexBuffer[rti], x2-1, y2, frame, colorAndUVIndexBase + UVInc*2); 
		_SetVert(sm_ActiveVertexBuffer[rti], x3-1, y3, frame, colorAndUVIndexBase + UVInc*3);
#else
		_SetVert(sm_ActiveVertexBuffer[rti], x3-1, y3, frame, colorAndUVIndexBase + UVInc*3);
		_SetVert(sm_ActiveVertexBuffer[rti], x2-1, y2, frame, colorAndUVIndexBase + UVInc*2); 
		_SetVert(sm_ActiveVertexBuffer[rti], x2-1, y2, frame, colorAndUVIndexBase + UVInc*2);
#endif
		return 2;
	}
	else if (x0 <= 0.0f || x1 <= 0.0f || x2 <=0.0f || x3 <= 0.0f) // part of us crossed the left side
	{
#if !API_QUADS_SUPPORT
		_SetVert(sm_ActiveVertexBuffer[rti], x0+1, y0, frame, colorAndUVIndexBase + UVInc*0); 
#endif
		_SetVert(sm_ActiveVertexBuffer[rti], x0+1, y0, frame, colorAndUVIndexBase + UVInc*0); 
		_SetVert(sm_ActiveVertexBuffer[rti], x1+1, y1, frame, colorAndUVIndexBase + UVInc*1); 
#if API_QUADS_SUPPORT
		_SetVert(sm_ActiveVertexBuffer[rti], x2+1, y2, frame, colorAndUVIndexBase + UVInc*2);
		_SetVert(sm_ActiveVertexBuffer[rti], x3+1, y3, frame, colorAndUVIndexBase + UVInc*3);
#else
		_SetVert(sm_ActiveVertexBuffer[rti], x3+1, y3, frame, colorAndUVIndexBase + UVInc*3);
		_SetVert(sm_ActiveVertexBuffer[rti], x2+1, y2, frame, colorAndUVIndexBase + UVInc*2);
		_SetVert(sm_ActiveVertexBuffer[rti], x2+1, y2, frame, colorAndUVIndexBase + UVInc*2);
#endif
		return 2;
	}
	else
	{
		return 1;
	}
}


// remove a ref (delayed so GPU is done with it)
void CPedDamageSetBase::DelayedRemoveRefFromTXD (s32 txdIdx)
{
	if (txdIdx > -1)
	{
		PEDDEBUG_DISPLAYF("CPedDamageSetBase::DelayedRemoveRefFromTXD: tdxId %d (%s), numRefs: %d", txdIdx, g_TxdStore.GetName(strLocalIndex(txdIdx)), g_TxdStore.GetNumRefs(strLocalIndex(txdIdx)));
		gDrawListMgr->AddRefCountedModuleIndex(txdIdx, &g_TxdStore);
	}
}

//
// CPedDamageSet
//
CPedDamageSet::CPedDamageSet()
{	
	m_DecorationBlitList.Reset();
	m_DecorationBlitList.Reserve(kMaxTattooBlits);

	Reset();
}

CPedDamageSet::~CPedDamageSet()
{
	// make sure they release any textures they were holding.
	for (int i=0;i<m_BloodBlitList.GetCount();i++)
		m_BloodBlitList[i].Reset();
	
	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
		m_DecorationBlitList[i].Reset();

	m_DecorationBlitList.Reset();
}

void CPedDamageBlitDecoration::Copy(const CPedDamageBlitDecoration & other)
{
	m_BirthTime					= other.m_BirthTime;
	m_Type						= other.m_Type;
	m_Zone						= other.m_Zone;
	m_Done						= other.m_Done;
	m_RefAdded					= other.m_RefAdded;		
	m_AlphaIntensity			= other.m_AlphaIntensity;		
	m_FlipUVFlags				= other.m_FlipUVFlags;	
	m_UVCoords					= other.m_UVCoords;
	m_Scale						= other.m_Scale;
	m_DecorationTexture			= other.m_DecorationTexture;				
	m_TxdHash					= other.m_TxdHash;
	m_TxtHash					= other.m_TxtHash;
	m_TxdId						= other.m_TxdId;
	m_pSourceInfo				= other.m_pSourceInfo;
	m_CollectionHash			= other.m_CollectionHash;
	m_PresetHash				= other.m_PresetHash;
	m_FixedFrameOrAnimSequence	= other.m_FixedFrameOrAnimSequence;	
	m_FadeInTime				= other.m_FadeInTime;				
	m_FadeOutStartTime			= other.m_FadeOutStartTime;			
	m_FadeOutTime				= other.m_FadeOutTime;	
	m_EmblemDesc				= other.m_EmblemDesc;
	m_TintInfo					= other.m_TintInfo;
}

CPedDamageBlitDecoration & CPedDamageBlitDecoration::operator=(const CPedDamageBlitDecoration & other)
{
	Copy(other);

	m_RefAdded = 0;					
	m_DecorationTexture = 0;		

	if (!m_Done && m_TxdHash.IsNotNull())
	{
		if (m_EmblemDesc.IsValid())
		{
			bool bEmblemRequestOk = NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(m_EmblemDesc  ASSERT_ONLY(, "CPedDamageManager"));

			// if it failed (eg: network down), flag it to have it requested during LoadDecorationTexture
			if (bEmblemRequestOk == false)
				m_DelayedClanTextureLoad = 1;
		}
	
		LoadDecorationTexture(NULL);
	}
	return *this;
}

void CPedDamageSet::Clone(CPed * ped, const CPedDamageSet& src)
{	
	Reset();

	m_pPed = ped;  
	m_BirthTimeStamp = TIME.GetElapsedTime(); // we are newer than the source (otherwise we might get reused to soon)
	m_LastVisibleFrame = CPedDamageManager::GetFrameID(); // just use this frame, it will get reset next frame

	// set the reuse score based on the new ped, not the source					  
	SetReuseScore(camInterface::GetPos().Dist(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition())), m_pPed->GetIsVisibleInSomeViewportThisFrame(), false);

	m_MiscDecorationsCount	  = src.m_MiscDecorationsCount;
	m_TattooDecorationsCount  = src.m_TattooDecorationsCount;
	m_LastDecorationTimeStamp = src.m_LastDecorationTimeStamp;
	m_NoDecorationTarget	  = src.m_NoDecorationTarget;
	m_lastValidTintPaletteSel = src.m_lastValidTintPaletteSel;
	m_lastValidTintPaletteSel2 = src.m_lastValidTintPaletteSel2;

	for (int i=0;i<kDamageZoneNumBloodZones;i++)
		m_BloodZoneLimits[i] = src.m_BloodZoneLimits[i];

	m_HasDress = src.m_HasDress;
	m_HasMedals = src.m_HasMedals;
	m_WaitingForTexture = src.m_WaitingForTexture;
	m_ClearColor = src.m_ClearColor;	

	// empty the damage list(s)
	for (int i=0; i<src.m_BloodBlitList.GetCount(); i++)
	{
		if (!src.m_BloodBlitList[i].IsDone())
			m_BloodBlitList.Append() = src.m_BloodBlitList[i];
	}

	Assertf(src.m_DecorationBlitList.GetCount() < kMaxTattooBlits, "CPedDamageSet::Clone: found more than kMaxTattooBlits (%d)", src.m_DecorationBlitList.GetCount());
	for (int i=0; i<src.m_DecorationBlitList.GetCount(); i++)
	{
		const CPedDamageBlitDecoration& srcDecoration = src.m_DecorationBlitList[i];
		CPEDDEBUG_DISPLAYF("CPedDamageSet::Clone srcDecoration emblem[%s] presethash[%s] IsDone[%d]",srcDecoration.GetEmblemDescriptor().IsValid() ? NETWORK_CLAN_INST.GetCrewEmblemMgr().GetEmblemName(srcDecoration.GetEmblemDescriptor()) : "",srcDecoration.GetPresetNameHash().TryGetCStr(),srcDecoration.IsDone());
		if (!srcDecoration.IsDone())
		{
			CPedDamageBlitDecoration& destDecoration = m_DecorationBlitList.Grow();
			destDecoration = srcDecoration;
		}
	}

	SetRenderTargets(-1);
}

void CPedDamageSet::ReleasePed(bool UNUSED_PARAM(autoRelease))
{
	if (m_pPed)
	{
		PEDDEBUG_DISPLAYF("CPedDamageSet::ReleasePed: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));
		m_pPed->SetDamageSetID(kInvalidPedDamageSet);	// make ped forget about us 
	}

	Reset();											// and we'll forget about the ped and everything else
}

void CPedDamageSet::SetRenderTargets(int index, int lod)
{
	if (index<0)
	{
		m_ActiveDamageTarget.Set(NULL);
		m_ActiveDecorationTarget.Set(NULL);
	}
	else
	{
		m_ActiveDamageTarget.Set((lod==kHiResDamageTarget) ? &sm_BloodRenderTargetsHiRes[index] : ((lod==kMedResDamageTarget) ? &sm_BloodRenderTargetsMedRes[index]: &sm_BloodRenderTargetsLowRes[index]));
		Assert(m_ActiveDamageTarget.Get()!=NULL);

		if (NeedsDecorationTarget()) 
		{
			m_ActiveDecorationTarget.Set((lod==kHiResDamageTarget) ? &sm_BloodRenderTargetsHiRes[index+1] : ((lod==kMedResDamageTarget) ? &sm_BloodRenderTargetsMedRes[index+1]: &sm_BloodRenderTargetsLowRes[index+1]));
			Assert(m_ActiveDecorationTarget.Get()!=NULL);
		}
		else
		{
			m_ActiveDecorationTarget.Set(NULL);
		}
	}
}

void CPedDamageSet::Reset()
{
	PEDDEBUG_DISPLAYF("CPedDamageSet::Reset: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	m_bForcedUncompressedDecorations = false;
	m_pPed = NULL;
	m_BirthTimeStamp = 0.0f;
	m_ReUseScore = 0;
	m_LastVisibleFrame = 0;
	m_MiscDecorationsCount = 0;
	m_TattooDecorationsCount = 0;
	m_LastDecorationTimeStamp = 0.0f;
	m_ClearColor.Set(0,0,255,0);
	m_ClonedFromLocalPlayer = false;
	m_lastValidTintPaletteSel = -1;
	m_lastValidTintPaletteSel2 = -1;

	for (int i=0;i<kDamageZoneNumBloodZones;i++)
		m_BloodZoneLimits[i] = 0xff;

	m_HasDress = false;
	m_HasMedals = false;
	m_WaitingForTexture = false;
	m_NoDecorationTarget = false;
 	m_Locked = false;

	// empty the damage list(s)
	for (int i=0;i<m_BloodBlitList.GetCount();i++)
		m_BloodBlitList[i].Reset();
	
	m_BloodBlitList.SetCount(0);

	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
		m_DecorationBlitList[i].Reset();

	// We only need to free and reallocate the list if it grew beyond the default
	if (m_DecorationBlitList.GetCapacity()>kMaxTattooBlits)
	{
		m_DecorationBlitList.Reset();						 
		m_DecorationBlitList.Reserve(kMaxTattooBlits); 
	}
	else
	{
		m_DecorationBlitList.Resize(0);
	}

	SetRenderTargets(-1);
}


bool CPedDamageSet::CalcUVs(Vector2 &uv, Vector2 &uv2, const Vector3 & localPos, const Vector3 &localDir, ePedDamageZones zone, ePedDamageTypes type, float radius, DamageBlitRotType rotType, bool enableSoakTextureGravity, bool bFromWeapon)
{
	bool isAnimal = m_pPed->HasAnimalAudioParams();
	bool limitScale=isAnimal;
	bool limitScale2=isAnimal;

	// transform into UV space...
	Vector2 uvPush;
	Vector2 mappedUV = m_Zones[zone].MapLocalPosToUV(localPos);
	
	// adjust for push form seems
	uv = m_Zones[zone].AdjustForPushAreas(mappedUV, radius, limitScale, m_HasDress, &uvPush);
	uv2 = uv;

	if ( type==kDamageTypeStab)
	{
		Vector3 dir = localDir;

		uv2 = m_Zones[zone].MapLocalPosToUV(localPos+dir);
		uv2 = m_Zones[zone].AdjustForPushAreas(uv2, radius, limitScale2, m_HasDress, &uvPush, bFromWeapon);
	}
	else if (rotType == kRandomRotation)
	{
		uv2 = uv+Vector2(s_Random.GetRanged(-0.1f, 0.1f),s_Random.GetRanged(-0.1f, 0.1f));		
	}
	else if ((rotType == kGravityRotation) || (rotType == kAutoDetectRotation && enableSoakTextureGravity))
	{
		if (enableSoakTextureGravity)
		{
			Vector3 dir = localDir;
		
			dir.Normalize(); // the local dir is assumed to be facing down toward gravity.
			dir.Scale(0.01f);
	
			uv2 = m_Zones[zone].MapLocalPosToUV(localPos+dir);

			// we have a down uv, but need a uv to the right.
			Vector2 down = uv2-mappedUV;

			uv2 = uv + Vector2(-down.y, down.x);
		}
	}
	
	return limitScale||limitScale2;
}

int CPedDamageSet::AddDamageBlitByUVs(const Vector2 & uv, const Vector2 & uv2, ePedDamageZones zone, ePedDamageTypes type, CPedBloodDamageInfo * pBloodInfo, bool limitScale, bool enableSoakTextureGravity, float scale, float preAge, int forcedFrame, u8 flipUVflags, bool bFromWeapon, bool bReduceBleeding)
{
	// TODO: we need to check to see if we hit the top/bottom and if we need to limit scale.
	// for not just save it directly with no limit checks...
	if(uv.y>=0 && uv.y<=1.0f)
	{	
		PEDDEBUG_DISPLAYF("AddDamageBlitByUVs() - uv = (%f,%f), zone = %s, BloodInfo = %s, enableTextureGravity = %s, scale = %f, preAge = %f, forcedFraem = %d, flipUvFlags = 0x%02x",	uv.x, uv.y, s_ZoneEmumNames[zone], pBloodInfo ? pBloodInfo->m_Name.GetCStr() : "", enableSoakTextureGravity?"True":"False", scale,preAge,forcedFrame,flipUVflags);

		Vector2 uv2Local = uv.IsClose(uv2,0.00001f) ? uv + Vector2(0.01f,0.0f) : uv2;  // prevent normalization error during rendering
		return SaveDamageBlit(uv, uv2Local, zone, type, pBloodInfo, limitScale, enableSoakTextureGravity, scale, preAge, forcedFrame, flipUVflags, bFromWeapon, bReduceBleeding);
	}
	else
	{
		PEDDEBUG_DISPLAYF("CPedDamageSet::AddDamageBlitByUVs(): Ped damage not applied (uv.y = %f)", uv.y);
	}
	return -1;
}

int CPedDamageSet::SaveDamageBlit(const Vector2 & uv, const Vector2 &uv2, ePedDamageZones zone, ePedDamageTypes type, CPedBloodDamageInfo * pBloodInfo, bool limitScale, bool enableSoakTextureGravity, float scale, float preAge, int forcedFrame, u8 flipUVflags, bool bFromWeapon, bool bReduceBleeding)
{
	m_BirthTimeStamp = TIME.GetElapsedTime(); // not really birth, but last time we got something added to us.

	const Vec2V uv_v(uv.x, uv.y); // unfortunately can't cast a Vector2 to a Vec2V .. should maybe pass uv and uv2 as a Vec4V_In?

	// check if we're really close to an existing (and not a stab wound, which is directional)
	if (type == kDamageTypeBlood)
	{
		for (int i=0;i<m_BloodBlitList.GetCount();i++)
		{
			if (!m_BloodBlitList[i].IsDone() && type==m_BloodBlitList[i].GetType() && zone==m_BloodBlitList[i].GetZone() && m_BloodBlitList[i].IsCloseTo(uv_v))
			{
				PEDDEBUG_DISPLAYF("CPedDamageSet::SaveDamageBlit(): Damage not applied (too close to exsisitng damage)");
				m_BloodBlitList[i].RenewHit(preAge); // Todo: this should also check scale, if the new one is bigger, use that scale
				return i;
			}
		}
	}

	// add to the list
	int slot = FindFreeDamageBlit();
	m_BloodBlitList[slot].Set(Vector4(uv.x, uv.y, uv2.x, uv2.y), zone, type, pBloodInfo, scale, limitScale, enableSoakTextureGravity, preAge, forcedFrame, flipUVflags); 
	m_BloodBlitList[slot].SetFromWeapon(bFromWeapon);
	m_BloodBlitList[slot].SetReduceBleeding(bReduceBleeding);
	return slot;
}


void CPedDamageSet::AddDecorationBlit( atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo, const EmblemDescriptor* pEmblemDesc, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 & scale, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, ePedDamageTypes type, float alpha,  CPedDamageDecalInfo * pSourceInfo, float fadeInTime, float fadeOutStart, float fadeOutTime, float preAge, int forcedFrame, u8 flipUVflags)
{
	if (m_NoDecorationTarget && type != kDamageTypeSkinBumpDecoration && type != kDamageTypeClothBumpDecoration) // if they don't have a decoration target, we only care about bump maps (scars,etc)
	{
		PEDDEBUG_DISPLAYF("CPedDamageSet::AddDecorationBlit: failed to add, non-player peds cannot have uncompressed decorations");
		return;
	}
	// NOTE: if posData calculation changes here, make sure to update CPlayerPedSaveStructure::PostLoad(), which extracts the rotation and scale from the UVs
	Vector4 posData(uvPos.x, uvPos.y, uvPos.x+cosf(rotation*DtoR)/2.0f, uvPos.y+sinf(rotation*DtoR)/2.0f); 
	
	if(zone==kDamageZoneMedals)
	{
		if (Verifyf(m_HasMedals || m_DecorationBlitList.GetCount()==0, "trying to add a medal to a ped that already has other decoration types"))
			m_HasMedals = true;
		else
			return; // they cannot be mixed and matched
	}
	else
	{
		if (Verifyf(!m_HasMedals || m_DecorationBlitList.GetCount()==0, "trying to add a non medal decoration to a ped that already has medals"))
			m_HasMedals = false;
		else
			return; // they cannot be mixed and matched
	}

	// add to the list
	// decorations other than tattoos/badges always specify a pSourceInfo; is there a better way to single these out?
	// if a tattoo is being requested we don't want to reuse decoration slots
	bool bIsTattoo = (pSourceInfo == NULL);
	int slot = FindFreeDecorationBlit(bIsTattoo);
	if (slot == -1)
	{
		PEDDEBUG_DISPLAYF("CPedDamageSet::AddDecorationBlit: failed to add, no blits available");
		return;
	}

	m_WaitingForTexture |= m_DecorationBlitList[slot].Set(colName, presetName, pTintInfo, pEmblemDesc, posData, scale, zone, type, txdHash, txtHash, alpha, pSourceInfo, fadeInTime, fadeOutStart, fadeOutTime, preAge, forcedFrame, flipUVflags);

	m_BirthTimeStamp = TIME.GetElapsedTime(); // not really birth time, but last time we got something added to us.
	
	if (colName.IsNotNull())
		m_LastDecorationTimeStamp = TIME.GetElapsedTime();  // need to track the decorations from collections, (they are medals, badges, etc for multiplayer)
}

void CPedDamageSet::DumpDecorations()
{
#if !__NO_OUTPUT
	Displayf("   decorations:");
	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
	{
		CPedDamageBlitDecoration & blit = m_DecorationBlitList[i];
		{
			Vector4 uvCoords = blit.GetUVCoords();

			float rotation = 0.0f;
			Vector2 dir(uvCoords.z,uvCoords.w);
			float mag = dir.Mag();
			if (mag>0.001)
			{
				dir /= mag;
				rotation = atan2(dir.y,dir.x)*RtoD;
				rotation = (rotation >= 0.0f) ? rotation : 360.0f+rotation;
			}

			float age = TIME.GetElapsedTime() - blit.GetBirthTime();
			u8 uvFlags = blit.GetUVFlipFlags();

			Displayf("     i[%d] zone[%d][%s] type[%d][%s] scale[%f %f] uvCoords[%f %f %f %f] uvFlags[0x%x] rotation[%f] age[%f] aintensity[%f] collectionHash[%s] presetHash[%s] emblem[%s][%s] waitingfordecorationtexture[%d]"
				,i
#if __BANK
				,blit.GetZone()
				,blit.GetZone() <= kDamageZoneNumZones ? s_ScriptZoneEmumNames[blit.GetZone()] : ""
				,blit.GetType()
				,blit.GetType() < kNumDamageTypes ? s_DamageTypeNames[blit.GetType()] : ""
#else
				,blit.GetZone()
				,""
				,blit.GetType()
				,""
#endif
				,blit.GetScale().x,blit.GetScale().y
				,uvCoords.x,uvCoords.y,uvCoords.z,uvCoords.w 
				,uvFlags
				,rotation
				,age
				,blit.GetAlphaIntensity()
				,blit.GetCollectionNameHash().TryGetCStr()
				,blit.GetPresetNameHash().TryGetCStr()
				,blit.GetEmblemDescriptor().IsValid() ? blit.GetEmblemDescriptor().GetStr() : ""
				,blit.GetEmblemDescriptor().IsValid() ? NETWORK_CLAN_INST.GetCrewEmblemMgr().GetEmblemName(blit.GetEmblemDescriptor()) : ""
				,blit.WaitingForDecorationTexture());
		}
	}
#endif
}

void CPedDamageSet::CheckForTexture()
{
	PEDDEBUG_DISPLAYF("CCompressedPedDamageSet::CheckForTexture ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	bool stillWaiting = false;
	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
	{
		CPedDamageBlitDecoration & blit = m_DecorationBlitList[i];
		// check to see if it's ready to load/set	
		if (blit.WaitingForDecorationTexture())
			stillWaiting |= blit.LoadDecorationTexture(m_pPed);
	}

	m_WaitingForTexture = stillWaiting;
}

void CPedDamageSet::UpdateScars()
{
	if (m_pPed)
	{
		for (int i=0;i<m_BloodBlitList.GetCount();i++)
		{
			m_BloodBlitList[i].UpdateScar(m_pPed);  // have the blit add a scar if it's the right time.
		}
	}
}

void CPedDamageSet::FreezeBloodFade(float time, float delta)
{
	if (m_pPed)
	{
		for (int i=0;i<m_BloodBlitList.GetCount();i++)
		{
			m_BloodBlitList[i].FreezeFade(time,delta);  // have the blit add a scar if it's the right time.
		}
	}
}

void CPedDamageSet::ClearDecorations(bool keepNonStreamed, bool keepOnlyScarsAndBruises, bool keepOnlyOtherDecorations)
{	
	PEDDEBUG_DISPLAYF("CPedDamageSet::ClearDecorations: ped=0x%p(%s), keepNonStreamed = %s, keepOnlyScarsAndBruises=%s, keepOnlyOtherDecorations=%s",
		m_pPed.Get(), GetPedName(m_pPed.Get()), keepNonStreamed?"True":"False",  keepOnlyScarsAndBruises?"True":"False",  keepOnlyOtherDecorations?"True":"False");

	m_MiscDecorationsCount = 0;

	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
	{
		if (m_DecorationBlitList[i].IsDone())
		{
			m_DecorationBlitList[i].Reset();
		}
		else
		{
			bool keepIt = false;
			
			const CPedDamageDecalInfo * info = m_DecorationBlitList[i].GetSourceInfo();

			bool bScarOrBruise = (info!=NULL) && ((info->GetNameHash()==ATSTRINGHASH("scar",0xda0a88e8)) ||
												  (info->GetNameHash()==ATSTRINGHASH("bruise",0xe838afb1)) || 
												  (info->GetNameHash()==ATSTRINGHASH("bruise_large",0xa1db9d43)));

			if (keepOnlyScarsAndBruises)
			{
				keepIt = bScarOrBruise;
			}
			else if (keepNonStreamed)
			{
				keepIt = (info!=NULL);	// if null, we were streamed
			}

			if (keepOnlyOtherDecorations)
			{
				keepIt |= !bScarOrBruise;
			}

			if (keepIt)
			{
				m_MiscDecorationsCount++;  // we did not delete it so we need to keep Track of it still
			}
			else
			{
				m_DecorationBlitList[i].Reset();
			}
		}
	}

	if (m_MiscDecorationsCount==0) // we cleared them all
	{
		m_DecorationBlitList.Reset();
		m_DecorationBlitList.Reserve(kMaxTattooBlits);
	}

	m_HasMedals = false;
	m_TattooDecorationsCount = 0;  

	m_LastDecorationTimeStamp = TIME.GetElapsedTime();
}

void CPedDamageSet::ClearClanDecorations()
{
	PEDDEBUG_DISPLAYF("CPedDamageSet::ClearClanDecorations: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	atHashString crewHash = PEDDAMAGEMANAGER.GetCrewEmblemDecorationHash();

	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
	{
		if (!m_DecorationBlitList[i].IsDone() && (m_DecorationBlitList[i].GetTxdHash()==crewHash))
		{
			m_DecorationBlitList[i].Reset();
			m_TattooDecorationsCount--;
		}
	}
}

void CPedDamageSet::ClearDecoration(int idx)
{
	PEDDEBUG_DISPLAYF("CPedDamageSet::ClearDecoration: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	if (idx>=0 && idx<m_DecorationBlitList.GetCount())
	{
		if (!(m_DecorationBlitList[idx].GetTxdHash()==0 && m_DecorationBlitList[idx].GetPresetNameHash().IsNull())) // make sure we did not already reset it (otherwise the counts will be off)
		{
			// all non-tattoo decorations have a texture info
			if (m_DecorationBlitList[idx].GetTextureInfo() != NULL && m_MiscDecorationsCount != 0) 
			{
				if (m_DecorationBlitList[idx].GetCollectionNameHash().IsNotNull())
					m_LastDecorationTimeStamp = TIME.GetElapsedTime();  // note the time so to network player will pick up the change

				m_MiscDecorationsCount--;
			}
			else if (m_TattooDecorationsCount != 0)
			{
				m_TattooDecorationsCount--;
			}
		}
		m_DecorationBlitList[idx].Reset();   
	}
}

void CPedDamageSet::ClearDamage()
{	
	PEDDEBUG_DISPLAYF("CPedDamageSet::ClearDamage: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	// TODO: m_BloodBlitList needs to be changed to a linked list, so this will just remove them from the list
	for (int i=0;i<m_BloodBlitList.GetCount();i++)
		m_BloodBlitList[i].Reset();

	m_BloodBlitList.SetCount(0);
}


bool CPedDamageSet::PromoteBloodToScars(bool bScarOverride, float scarU, float scarV, float scarScale)
{	
// loop through and add scars (fully formed, not faded in) to blood damage that needs it, then kill the blood
	if (m_pPed==NULL)
		return false;

	int scarCount = 0;
	int maxScars = (NetworkInterface::IsGameInProgress()) ? PEDDAMAGEMANAGER.GetDamageData().m_NumWoundsToScarsOnDeathMP : PEDDAMAGEMANAGER.GetDamageData().m_NumWoundsToScarsOnDeathSP;

	if (!Verifyf(maxScars<=8,"max wounds to scars per frame is 8, should really be less than that"))
		maxScars = 8;

	int bestCandidates[8];
	for (int i=0;i<maxScars;i++)   // max scars is usually very small 0 or 1, but it could be more, depending on the peddamage.xml tuning value
		bestCandidates[i] = -1;

	// need to pick the "newest" n wounds
	for (int i=0;i<m_BloodBlitList.GetCount();i++)
	{
		if (m_BloodBlitList[i].CanLeaveScar())
		{
			// see if it's better than the ones we already have
			float birthTime = m_BloodBlitList[i].GetReBirthTime();
			for (int j=0;j<maxScars;j++) 
			{
				if (bestCandidates[j]<0) // unused Slot, just take it 
				{
					bestCandidates[j] = i;
					break;
				}
				else if (birthTime>m_BloodBlitList[bestCandidates[j]].GetReBirthTime())
				{
					for (int k = j; k < maxScars-1; k++) // move the existing one down and add this one
						bestCandidates[k+1] = bestCandidates[k];
					bestCandidates[j] = i;
					break;
				}
			}
		}
	}

	for (int i=0;i<maxScars;i++)
	{
		if (bestCandidates[i]>=0)
		{
			m_BloodBlitList[bestCandidates[i]].CreateScarFromWound(m_pPed,false,bScarOverride,scarU,scarV,scarScale);
			scarCount++;
		}
	}

	for (int i=0;i<m_BloodBlitList.GetCount();i++)
			m_BloodBlitList[i].Reset();
	m_BloodBlitList.SetCount(0);
	return scarCount>0;
}

void CPedDamageSet::ClearDamage(ePedDamageZones zone)
{
	PEDDEBUG_DISPLAYF("CPedDamageSet::ClearDamage: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	for (int i=0;i<m_BloodBlitList.GetCount();i++)
	{
		if( zone == m_BloodBlitList[i].GetZone() )
		{
			m_BloodBlitList[i].Reset();
			m_BloodBlitList.DeleteFast(i);
			i--; // Due to the way the array delete stuff, we need to ensure we stay on the same line...
		}
	}
}

#if GTA_REPLAY
//For replay when we rewind we need to remove the blood damage
//as packets are extracted forwards on a frame whether we're rewinding or not
//we need to delete all the blood from bloodBlitID forwards.
void CPedDamageSet::ReplayClearPedBlood(u32 bloodBlitID)
{
	if( bloodBlitID >= m_BloodBlitList.GetCount() )
		return;

	PEDDEBUG_DISPLAYF("CPedDamageSet::ReplayClearPedBlood: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));
	for (int i=bloodBlitID;i<m_BloodBlitList.GetCount();i++)
	{
		m_BloodBlitList[i].Reset();
		m_BloodBlitList.DeleteFast(i);
		i--; // Due to the way the array delete stuff, we need to ensure we stay on the same line...
	}
}

bool CPedDamageSet::ReplayIsBlitIdValid(u32 bloodBlitID)
{
	return (bloodBlitID < m_BloodBlitList.GetCount());
}
#endif //GTA_REPLAY

	
struct limitSortPairs {	
	void Set(int index, float time) {m_Index = index; m_BirthTime = time;}
	int m_Index;
	float m_BirthTime;
};

int LimitBloodSortCompare( limitSortPairs const* A,  limitSortPairs const* B)
{
	float diff =  B->m_BirthTime - A->m_BirthTime;  // higher times means newer
	return (diff < 0.0f) ? -1 : ((diff >0.0f) ? 1: 0);
}

void CPedDamageSet::LimitZoneBlood(ePedDamageZones zone, int limit)
{
	Assert(zone < kDamageZoneNumBloodZones || zone==kDamageZoneInvalid);

	int start = 0;
	int end = kDamageZoneNumBloodZones-1;
	if (zone != kDamageZoneInvalid)
		start = end = zone;

	u8 newLimit = (limit<0) ? 0xff: Min((u8)limit,(u8)kMaxBloodDamageBlits);

	for (int zone=start; zone <=end; zone++ )
		m_BloodZoneLimits[zone] = newLimit;  //	 still track this in case we want to limit in coming blood or something

	if (m_BloodBlitList.GetCount()==0) 
		return;
	
	atUserArray<limitSortPairs> sortedBloodList;
	limitSortPairs *sortedBloodListMem = Alloca(limitSortPairs, CPedDamageSetBase::kMaxBloodDamageBlits);
	if (Verifyf(sortedBloodListMem, "Alloca Failed"))
		sortedBloodList.Assume(sortedBloodListMem,(u16)CPedDamageSetBase::kMaxBloodDamageBlits);

	for (int zone=start; zone <=end; zone++ )
	{
		sortedBloodList.ResetCount();

		// sort the damage by birth time
		for (int index=0;index<m_BloodBlitList.GetCount();index++)
		{
			if( zone == m_BloodBlitList[index].GetZone() && !m_BloodBlitList[index].IsDone() )
				sortedBloodList.Append().Set(index, m_BloodBlitList[index].GetBirthTime());
		}
		sortedBloodList.QSort(0,-1,LimitBloodSortCompare);
		
		// clear out the oldest ones, until we get to the limit count
		for (int reverse = sortedBloodList.GetCount()-1; reverse >= newLimit; reverse--)
		{
			m_BloodBlitList[sortedBloodList[reverse].m_Index].ForceDone();
		}
	}
}

void CPedDamageSet::ClearDamageDecals(ePedDamageZones zone, atHashWithStringBank damageDecalNameHash)
{
	Assert(zone < kDamageZoneNumBloodZones || zone==kDamageZoneInvalid);

	int start = 0;
	int end = kDamageZoneNumBloodZones-1;
	if (zone != kDamageZoneInvalid)
		start = end = zone;

	bool doAll = damageDecalNameHash.IsNull() || damageDecalNameHash.GetHash()==ATSTRINGHASH("ALL",0x45835226);

	for (int zone=start; zone <=end; zone++ )
	{
		for (int index=0;index<m_DecorationBlitList.GetCount();index++)
		{
			if( zone == m_DecorationBlitList[index].GetZone())
			{
				atHashValue	nameHash = m_DecorationBlitList[index].GetSourceNameHash();

				if ( nameHash.IsNotNull() && (doAll || nameHash == damageDecalNameHash.GetHash()))  // only do the ones applied via AddPedDamageDecal()
					m_DecorationBlitList[index].ForceDone();
			}
		}
	}
}

int CPedDamageSet::FindFreeDamageBlit() 
{
	int index;
	if (m_BloodBlitList.IsFull())
	{
		// find oldest or any that are done and and replace them
		// we need a retirement plan for old stains...
		int i;
		float oldest = FLT_MAX;
		int oldestId = 0; // if we don't find anything use first entry (should never happen, kMaxDamageBlits hits in one frame?)
		for (i=0;i<m_BloodBlitList.GetCount();i++)
		{
			if (m_BloodBlitList[i].IsDone())
			{
				oldestId = i;
				break;
			}

			float birthTime = Max(m_BloodBlitList[i].GetBirthTime(),m_BloodBlitList[i].GetReBirthTime());
			if (birthTime<oldest)  
			{
				oldest = birthTime;
				oldestId = i;
			}
		}

		index = oldestId;
	}
	else
	{
		// add one to the end and return it's index
		index = m_BloodBlitList.GetCount();
		m_BloodBlitList.Append();
	}

	return index;
}

int CPedDamageSet::FindFreeDecorationBlit(bool bDontReuse) 
{
	CPedDamageBlitDecoration blit;

	int index = -1;
	if (bDontReuse == false)
	{
		// find oldest or any that are done and and replace them (or add more if we're not at the limit)
		int i;
		float oldest = FLT_MAX;
		int oldestId = -1; // if we don't find anything use first entry (should never happen, kMaxDecorationBlits hits in one frame?)
		for (i=0;i<m_DecorationBlitList.GetCount();i++)
		{
			if (m_MiscDecorationsCount < kMaxDecorationBlits)
			{
				if (m_DecorationBlitList[i].IsDone())		// if we're not at the limit, look for an unused on
				{
					oldestId = i;
					if(m_DecorationBlitList[i].GetTextureInfo() == NULL) // if it was not a scar or reset before we need to account for it   converting to a scar
						m_MiscDecorationsCount++;
					break;
				}
			}
			else if (m_DecorationBlitList[i].GetTextureInfo() != NULL && // since we're at the limit, look for the oldest non reset non tattoo one to replace
					 m_DecorationBlitList[i].GetBirthTime() < oldest) 
			{
				// if kMaxDecorationBlits decorations have already been added just reuse the oldest one,
				// otherwise we'll assign a new slot for it below
				oldest = m_DecorationBlitList[i].GetBirthTime();
				oldestId = i;
			}
		}

		// if not too many scar or dirt decorations had been added, just push a new one
		if (oldestId == -1 && m_MiscDecorationsCount < kMaxDecorationBlits && m_DecorationBlitList.GetCount() < kMaxTattooBlits) 
		{
			// add one to the end and return it's index
			oldestId = m_DecorationBlitList.GetCount();
#if __BANK
			if (oldestId==m_DecorationBlitList.GetCapacity())
			{
				PEDDEBUG_DISPLAYF("CPedDamageSet::FindFreeDecorationBlit() growing m_DecorationBlitList for non tattoo blit");
			}
#endif
			m_DecorationBlitList.PushAndGrow(blit);
			m_MiscDecorationsCount++;
		}

		index = oldestId;
	}
	else
	{
		// look for killed ones first.
		for (int i=0;i<m_DecorationBlitList.GetCount();i++)
		{
			if ( m_DecorationBlitList[i].IsDone()) 
			{
				index = i;
				break;
			}
		}

		if (index == -1 && Verifyf(m_TattooDecorationsCount < kMaxTattooBlits && m_DecorationBlitList.GetCount() < kMaxTattooBlits, "CPedDamageSet::FindFreeDecorationBlit: max. number of tattoos exceeded (%d)", m_TattooDecorationsCount))
		{
			// add one to the end and return it's index
			index = m_DecorationBlitList.GetCount();
#if __BANK
			if (index==m_DecorationBlitList.GetCapacity())
			{
				PEDDEBUG_DISPLAYF("CPedDamageSet::FindFreeDecorationBlit() growing m_DecorationBlitList for tattoo blit");
			}
#endif

			m_DecorationBlitList.PushAndGrow(blit);
			m_TattooDecorationsCount++;
		}
	}
	
	if (index>=0)
		m_DecorationBlitList[index].Reset();   // reset refs, etc. before giving it up for reuse.

	return index;
}


void CPedDamageSet::SetReuseScore(float distanceToCamera, bool isVisible, bool UNUSED_PARAM(isDead))
{
	float age = TIME.GetElapsedTime() - m_BirthTimeStamp;

	if (m_pPed==NULL)	
		age = 1000000.0f;
	else if (m_pPed->IsLocalPlayer()) // don't ever reuse the local player ped.
		age = -1000000.0f; 
	else if (m_pPed->IsPlayer())   // give player peds higher priority than NPCs in multiplayer
		age = -10;

	if (isVisible) 
	{
		if (m_LastVisibleFrame == CPedDamageManager::GetFrameID()) // possible if the headshot manager add a ped already visible, we don't want the head shot manager to lower the res for the in game ped
			distanceToCamera = Min(m_Distance,distanceToCamera);
		
		m_LastVisibleFrame = CPedDamageManager::GetFrameID();
	}

	m_Distance = distanceToCamera;

	if (distanceToCamera<100000.0f)
		age += (distanceToCamera-10.0f)/10.0f;	// add 1 second for each 10m after 10m (less that 10 actually decreases age slightly)

	int invisibleFrameCount = CPedDamageManager::GetFrameID() - m_LastVisibleFrame;

	if (invisibleFrameCount>3)
		age += invisibleFrameCount/10.0f; // invisible frame add to age at 3x rate
	
	// we should lower the priority for dead people as well...

	m_ReUseScore=age;
}

u16 CPedDamageSet::CalcComponentBloodMask() const
{
	u16 bloodMask = 0;

	for (int i = 0; i<m_BloodBlitList.GetCount(); i++)
	{
		if(!m_BloodBlitList[i].IsDone())
			bloodMask |= 1 << static_cast<u16>(m_BloodBlitList[i].GetZone());	
	}

	for (int i = 0; i<m_DecorationBlitList.GetCount(); i++)
	{
		if(!m_DecorationBlitList[i].IsDone()) 
			bloodMask |= 1 << static_cast<u16>(m_DecorationBlitList[i].GetZone());	
	}

	return bloodMask;
}


class SortedDamageListEntry { // NOTE: keep this small since the array goes on the stack
public:
	SortedDamageListEntry() {}
	void Set(int damageIndex, int zone, u16 texId) { m_DamageInfoIndex = (u8)damageIndex; m_Zone=(u8)zone; m_UniqueTexGroupID=texId;}
public:
	u8	m_DamageInfoIndex;
	u8	m_Zone;
	u16	m_UniqueTexGroupID;
};


int DamageSortCompare( SortedDamageListEntry const* A,  SortedDamageListEntry const* B)
{
	if (A->m_Zone == B->m_Zone)
	{
		return (int)(A->m_UniqueTexGroupID) - (int)(B->m_UniqueTexGroupID);
	}
	else
	{
		return (int)(A->m_Zone) - (int)(B->m_Zone);
	}
}

// find a run of similar damage (must have same textures so we can set the shader vars once)
int FindBatch(const atUserArray<SortedDamageListEntry> & list, int first, u8 zone)
{
	Assert (list[first].m_Zone == zone);

	u32 texId=list[first].m_UniqueTexGroupID;
	int last = first+1;

	while (last<list.GetCount() && list[last].m_Zone==zone && list[last].m_UniqueTexGroupID==texId)
		last++;

	return last-1;
}

void CPedDamageSet::Render(const atUserArray<CPedDamageBlitBlood> & bloodArray, const atUserArray<CPedDamageBlitDecoration> &decorationArray, const grcRenderTarget * bloodDamageTarget, const grcRenderTarget *decorationTarget, const atRangeArray<CPedDamageCylinder, kDamageZoneNumZones> & zoneArray, bool bHasMedals, bool BANK_ONLY(bHasDress), const Color32 & clearColor, bool bMirrorMode, float currentTime, s32 cachedPalSel1, s32 cachedPalSel2)
{
	atUserArray<SortedDamageListEntry> sortedBloodList;
	atUserArray<SortedDamageListEntry> sortedDecorationList[2];
	
	grcViewport orthoViewport, zoneVP;
	orthoViewport.Ortho(0.0f,1.0f,1.0f,0.0f,-1.0f,1.0f);

	// sort the blood and damage by zone so we can do it in batches
	int bloodCount = bloodArray.GetCount();
	if (bloodCount)
	{
		SortedDamageListEntry *sortedBloodListMem = (bloodCount) ? Alloca(SortedDamageListEntry, bloodCount) : NULL;
		
		if (Verifyf(sortedBloodListMem, "Alloca Failed"))
		{
			sortedBloodList.Assume(sortedBloodListMem,(u16)bloodCount);

			for (int i=0;i<bloodCount;i++)
				sortedBloodList.Append().Set(i, bloodArray[i].GetZone(), (bloodArray[i].DoesSoakUseGravity())?bloodArray[i].GetBloodInfoPtr()->m_UniqueTexComboIDGravity:bloodArray[i].GetBloodInfoPtr()->m_UniqueTexComboIDNonGravity);

			sortedBloodList.QSort(0,-1,DamageSortCompare);
		}
	}

	int decorationCount = decorationArray.GetCount();
	if (decorationCount)
	{
		SortedDamageListEntry *sortedDecorationListMem[2]; // two lists, one for each pass
		sortedDecorationListMem[0] = Alloca(SortedDamageListEntry, decorationCount);
		sortedDecorationListMem[1] = Alloca(SortedDamageListEntry, decorationCount);

		if (Verifyf(sortedDecorationListMem[0] && sortedDecorationListMem[1], "Alloca Failed"))
		{
			sortedDecorationList[0].Assume(sortedDecorationListMem[0],(u16)decorationCount);
			sortedDecorationList[1].Assume(sortedDecorationListMem[1],(u16)decorationCount);
		
			u16 nextNonXMLUniqueId = 0x4000;  // for the decoration decals that are not preloaded (and don't have a precomputed uniqueID)

			for (int i=0;i<decorationCount;i++)
			{				
				u16 uniqueID = 0;
				if (decorationArray[i].GetTextureInfo())
					uniqueID = decorationArray[i].GetTextureInfo()->m_uniqueId;
				else
					uniqueID = nextNonXMLUniqueId++;

				if (decorationArray[i].HasBumpMap() || bHasMedals)
					sortedDecorationList[0].Append().Set(i, decorationArray[i].GetZone(), uniqueID);
					 
				uniqueID |= (decorationArray[i].IsOnClothOnly())?0x8000:0x0000;  // set a bit to force a break between cloth and skin types (they need to change state) and move them to the end (might be handy for draw order later)
				
				sortedDecorationList[1].Append().Set(i, decorationArray[i].GetZone(), uniqueID);
			}
			sortedDecorationList[0].QSort(0,-1,DamageSortCompare);
			sortedDecorationList[1].QSort(0,-1,DamageSortCompare);
		}
	}

	bool hasPassOneBlits = (bloodCount>0 || sortedDecorationList[0].GetCount()>0) && bloodDamageTarget;
	bool hasPassTwoBlits = (sortedDecorationList[1].GetCount()>0) && decorationTarget;

	bool hasBlits = (hasPassOneBlits || hasPassTwoBlits);

	for (int pass = 0; pass<2; pass++)  // two passes for tattoo targets
	{
		const grcRenderTarget * target = (pass==0) ? bloodDamageTarget : decorationTarget;

		if (target)  // most targets don't have a tattoo target, so this usually fails on the second pass
		{
			bool dualTarget = IsDualTarget(target);
			bool sideWays = target->GetWidth()>target->GetHeight();
			grcRenderTarget * depthTarget = (pass==0) ? NULL : ((bMirrorMode) ?  CPedDamageSetBase::GetMirrorDepthTarget() : CPedDamageSetBase::GetDepthTarget(target)); 

			grcTextureFactory::GetInstance().LockRenderTarget(0, target, depthTarget); // need stencil buffer for the second pass.
			grcViewport::SetCurrent(&orthoViewport);

			if (pass==0)	
			{
				if (dualTarget) // currently ps3 only, since that is the only platform with the weird stride/RT pool issues
					GRCDEVICE.ClearRect(GRCDEVICE.GetWidth()/2, 0, GRCDEVICE.GetWidth()/2, GRCDEVICE.GetHeight(), true, clearColor, false, 1.0, false, 0);
				else
					GRCDEVICE.Clear(true, clearColor, false, 1.0, false, 0);
			}
			else // pass 1
			{
				// enable hi stencil for 2nd pass (to mark out skin only parts)
				CPedDamageSetBase::SetupHiStencil(false);

				if (bMirrorMode) // special case, we have a double wide, but not the special 
				{
					Assert(dualTarget);
					GRCDEVICE.ClearRect(0, 0, GRCDEVICE.GetWidth()/2, GRCDEVICE.GetHeight(), true,  Color32(0,0,0,127), false, 1.0, hasPassTwoBlits, 0);
				}
				else
				{
					GRCDEVICE.Clear(true, Color32(0,0,0,127), false, 1.0, hasPassTwoBlits, 0);
				}
			}
		
			bool wereAnyDecorationsDrawn = false;
		
#if __BANK
			if (s_DebugDrawPushZones && pass==0)
			{
				for (u8 zone=0; zone<kDamageZoneNumBloodZones; zone++)
				{
					if (zoneArray[zone].GetPushArray())
					{
						CPedDamageSet::SetupViewport( &zoneVP, zone, sideWays, dualTarget, pass);
						for (int i=0;i<zoneArray[zone].GetPushArray()->GetCount();i++)
							(*(zoneArray[zone].GetPushArray()))[i].DebugDraw(bHasDress);
					}	
				}
			}
#endif		

			// do we need to render any blits?
			if (hasBlits)
			{
				int firstBlood=0;
				int lastBlood=0;
				int firstDecoration=0;
				int lastDecoration=0;
				u8 zoneBit = 0x1;
				u8 firstZone = 0;
				u8 lastZone = kDamageZoneNumBloodZones-1;

				if (pass==1 && bHasMedals)
					firstZone=lastZone=kDamageZoneMedals;

				for (u8 zone=firstZone; zone<=lastZone; zone++)
				{
					bool vpSet = false;
					float aspectRatio=1.0f;
					float scale=1.0f;
					bool hFlip = zone>=kDamageZoneLeftArm;

					if (pass==0) // blood only ever goes into pass 0
					{
						while (firstBlood < sortedBloodList.GetCount() && sortedBloodList[firstBlood].m_Zone == zone)
						{
							if (!vpSet)	
							{
								aspectRatio = zoneArray[zone].GetHeight()/(PI*2*zoneArray[zone].GetRadius());
								scale = zoneArray[zone].CalcScale();
								vpSet = true;
								CPedDamageSet::SetupViewport( &zoneVP, zone, sideWays, dualTarget, pass);
							}

							lastBlood = FindBatch(sortedBloodList, firstBlood, zone);

							int count = (lastBlood-firstBlood)+1;
							
							if (count)
							{
								for (int bloodPass = 0; bloodPass<3;  bloodPass++) // soak, splat, wound
								{
									int actualCount = 0;
									if (bloodArray[sortedBloodList[firstBlood].m_DamageInfoIndex].RenderBegin(bloodPass, count )) 
									{
										for (int i = firstBlood; i<firstBlood+count; i++)
											actualCount += bloodArray[sortedBloodList[i].m_DamageInfoIndex].Render(bloodPass, scale, aspectRatio, hFlip, currentTime);

										bloodArray[sortedBloodList[firstBlood].m_DamageInfoIndex].RenderEnd(2*count-actualCount); // close the verts and zero out any unused vertslastDecorations
									}
								}
							}
							firstBlood = lastBlood+1;
						}
					}

					// decorations can go in both passes (bump for scars in pass 0, color decoration info(tattoos/logos/bruising/etc) into pass 1)
					while (firstDecoration < sortedDecorationList[pass].GetCount() && sortedDecorationList[pass][firstDecoration].m_Zone == zone)
					{
						if (!vpSet)
						{
							aspectRatio = zoneArray[zone].GetHeight()/(PI*2*zoneArray[zone].GetRadius());
							scale = zoneArray[zone].CalcScale();
							vpSet = true;
							CPedDamageSet::SetupViewport( &zoneVP, zone, sideWays, dualTarget, pass);
						}
							
						wereAnyDecorationsDrawn=true;
					
						lastDecoration = FindBatch(sortedDecorationList[pass], firstDecoration, zone);
						int count = (lastDecoration-firstDecoration)+1;
						
						// we may want to limit decoration here like we do blood

						for (int j=0;j<1;j++)
						{
							int actualCount = 0;

							if (decorationArray[sortedDecorationList[pass][firstDecoration].m_DamageInfoIndex].RenderBegin(pass+j, count, cachedPalSel1, cachedPalSel2))
							{
								for (int i = firstDecoration; i<firstDecoration+count; i++)
									actualCount += decorationArray[sortedDecorationList[pass][i].m_DamageInfoIndex].Render(aspectRatio, hFlip,currentTime);
								
								decorationArray[sortedDecorationList[pass][firstDecoration].m_DamageInfoIndex].RenderEnd(2*count-actualCount); // close the verts and zero out any unused verts
							}
						}
						firstDecoration = lastDecoration+1;
					}
				
					zoneBit=zoneBit<<1;
				}
			}

			if (pass==1)
			{
				if (wereAnyDecorationsDrawn)
					CPedDamageSetBase::ProcessSkinAlpha(orthoViewport);

				CPedDamageSetBase::DisableHiStencil();
			}

			CPedDamageManager::PreDrawStateSetup();
			grcViewport::SetCurrent(NULL);
		
			grcResolveFlags resolveFlags;
			grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);
		}
	}
}


void CPedDamageSet::GetRenderTargets(grcTexture * &bloodTarget, grcTexture * &tattooTarget)
{
	bloodTarget = (m_BloodBlitList.GetCount()>0 || m_DecorationBlitList.GetCount()>0) ? m_ActiveDamageTarget.Get() : NULL; // only really need this if some of the decorations are scars and need bump maps
	tattooTarget = (m_DecorationBlitList.GetCount()>0) ? m_ActiveDecorationTarget.Get() : NULL;
}

bool CPedDamageSet::GetDecorationInfo(int idx, atHashString& colHash, atHashString& presetHash) const
{
	if (idx < 0 || idx > m_DecorationBlitList.GetCount())
	{
		return false;
	}

	presetHash = m_DecorationBlitList[idx].GetPresetNameHash();
	colHash = m_DecorationBlitList[idx].GetCollectionNameHash();

	return true;
}

//
// CCompressedPedDamageSet
//

CCompressedPedDamageSet::CCompressedPedDamageSet()
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::CCompressedPedDamageSet: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	m_bOutputTexRefAdded = false;
	m_pOutTexture = NULL;
	m_DecorationBlitList.Reset();
	m_DecorationBlitList.Reserve(kMaxTattooBlits);
	m_OutTxdHash.Clear();
	m_OutTxtHash.Clear();
	Reset();
}

CCompressedPedDamageSet::~CCompressedPedDamageSet()
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::~CCompressedPedDamageSet: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	// make sure they release any textures they were holding.
	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
		m_DecorationBlitList[i].Reset();

	m_DecorationBlitList.Reset();
}


void CCompressedPedDamageSet::ReleasePed(bool autoRelease)
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::ReleasePed: ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));
#if __BANK
	if (PARAM_peddamagedebug.Get())
	{
		scrThread::PrePrintStackTrace();
		sysStack::PrintStackTrace();
	}
#endif

	if (autoRelease)  // we don't autorelease compressed damage when SetPed() is called, we deal with the at the proxy level
		return;
	
	Assert(m_RefCount>0);

	m_RefCount--;

	if (m_RefCount<=0)
	{
		CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::ReleasePed: m_RefCount<=0, reseting()");
		Reset();
	}
}

void CCompressedPedDamageSet::Reset()
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::Reset ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));

	m_pPed = NULL;
	m_BirthTimeStamp = 0.0f;
	m_State = kInvalid;
	m_pBlitTarget = NULL;
	m_pCompressedTexture = NULL;
	m_HasMedals = false;
	m_ComponentMask = 0;
	m_RefCount = 0;

	if (m_pOutTexture != NULL)
	{
		m_pOutTexture = NULL;
	}

	// No ref counting for uncompressed MP decoration textures
	if (PEDDECORATIONBUILDER.IsUsingUncompressedMPDecorations() == false)
	{
		strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_OutTxdHash.GetHash()));
		if(txdId != -1 && m_bOutputTexRefAdded)
		{
			DelayedRemoveRefFromTXD (txdId.Get());
			CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::Reset: tdxId %d (%s), numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), g_TxdStore.GetNumRefs(txdId));
		}
	}

	m_bOutputTexRefAdded = false;

	PEDDECORATIONBUILDER.ReleaseOutputTexture(m_OutTxdHash.GetHash());

	m_OutTxdHash.Clear();
	m_OutTxtHash.Clear();

	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
		m_DecorationBlitList[i].Reset();

	m_DecorationBlitList.Reset();
	m_DecorationBlitList.Reserve(kMaxTattooBlits);

	PEDDECORATIONBUILDER.Remove(this);
}


void CCompressedPedDamageSetProxy::Clone(CPed * pPed, const CCompressedPedDamageSetProxy* src)
{	
	SetCompressedTextureSet(src->m_pCompressedTextureSet);

	if (src->m_pPed == NULL)
	{
		CPEDDEBUG_DISPLAYF("CCompressedPedDamageSetProxy::Clone: src m_pPed is NULL");
	}
		
	m_pPed = pPed;

	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSetProxy::Clone: ped=0x%p(%s), m_pCompressedTextureSet numRefs: %d", m_pPed.Get(), GetPedName(m_pPed.Get()), m_pCompressedTextureSet ? m_pCompressedTextureSet->GetRefCount() : -1);
}

void CCompressedPedDamageSetProxy::ReleasePed()
{
	if (m_pPed)
	{
		CPEDDEBUG_DISPLAYF("CCompressedPedDamageSetProxy::ReleasePed: ped=0x%p(%s), refCount = %d", m_pPed.Get(), GetPedName(m_pPed.Get()), (m_pCompressedTextureSet)?(m_pCompressedTextureSet)->GetRefCount():-1);

		m_pPed->SetCompressedDamageSetID(kInvalidPedDamageSet);	// make ped forget about us 
		m_pPed = NULL;
		
		if (m_pCompressedTextureSet)
		{
			m_pCompressedTextureSet->ReleasePed(false);
			m_pCompressedTextureSet = NULL;
		} 
	}
}


bool CCompressedPedDamageSetProxy::SetPed(CPed * pPed)
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSetProxy::SetPed: ped=0x%p(%s)", pPed, GetPedName(pPed));

	if (AssertVerify(m_pCompressedTextureSet))
	{
		if (!m_pCompressedTextureSet->SetPed(pPed))
			return false;

		m_pPed=pPed; 
	}
	return true;
}

bool CCompressedPedDamageSet::AddDecorationBlit (atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo,  const EmblemDescriptor* pEmblemDesc, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 & scale, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, ePedDamageTypes type)
{
	if(zone==kDamageZoneMedals)
	{
		if (Verifyf(m_HasMedals || m_DecorationBlitList.GetCount()==0, "trying to add a medal to a ped that already has other decoration types"))
			m_HasMedals = true;
		else
			return false; // they cannot be mixed and matched
	}
	else
	{
		if (Verifyf(!m_HasMedals || m_DecorationBlitList.GetCount()==0, "trying to add a non medal decoration to a ped that already has medals"))
			m_HasMedals = false;
		else
			return false; // they cannot be mixed and matched
	}

	// NOTE: if posData calculation changes here, make sure to update CPlayerPedSaveStructure::PostLoad(), which extracts the rotation and scale from the UVs
	Vector4 posData(uvPos.x, uvPos.y, uvPos.x+cosf(rotation*DtoR)/2.0f,uvPos.y+sinf(rotation*DtoR)/2.0f); 


	// add to the list	
	int slot = FindFreeDecorationBlit();
	if (slot == -1)
	{
		CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::AddDecorationBlit : slot == -1");

		return false;
	}
	m_DecorationBlitList[slot].Set(colName, presetName, pTintInfo, pEmblemDesc, posData, scale, zone, type, txdHash, txtHash);
	
	RestartCompression(); 

	m_ComponentMask |= 1 << static_cast<u16>(zone);

	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::AddDecorationBlit : kWaitingOnTextures = %s",kWaitingOnTextures?"true":"false");
	return true;
}

void CCompressedPedDamageSet::RestartCompression()
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::RestartCompression() : m_State = %s set state kWaitingOnTextures ped=0x%p(%s)",s_CompressedPedDamageSetState[m_State], m_pPed.Get(), GetPedName(m_pPed.Get()));
//	if (m_State!=kInvalid && m_State!=kWaitingOnTextures)
//		{CPEDDEBUG_DISPLAYF("compressus interuptus");}
	// some blits were added or deleted, so we need to trigger a recompression 
	m_State = kWaitingOnTextures;   // force check of textures, they might be out now.
	m_pCompressedTexture = NULL;	// need to reset this one, since we cannot recompress to it while we are render it. NOTE: this is not true anymore for the 360 since we do not tile
}

void CCompressedPedDamageSet::ClearDecorations()
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::ClearDecorations() ped=0x%p(%s)", m_pPed.Get(), GetPedName(m_pPed.Get()));
#if __BANK
	if (PARAM_peddamagedebug.Get())
	{
		scrThread::PrePrintStackTrace();
		sysStack::PrintStackTrace();
	}
#endif

	// free up and ref to the blits textures
	for (int i=0;i<m_DecorationBlitList.GetCount();i++) 
		m_DecorationBlitList[i].Reset();

	m_DecorationBlitList.Reset();
	m_DecorationBlitList.Reserve(kMaxTattooBlits);

	// we used to reset here, but we do not want to now since multiple peds could be referencing to the set
	// we do need to clear a few things though...

	m_BirthTimeStamp = 0.0f;
	m_ComponentMask = 0;
	m_HasMedals = false;
	RestartCompression();
}

void CCompressedPedDamageSet::ClearDecoration(int idx)
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::ClearDecoration idx[%d] ped=0x%p(%s)",idx, m_pPed.Get(), GetPedName(m_pPed.Get()));

	if (idx>=0 && idx<m_DecorationBlitList.GetCount())
	{
		m_DecorationBlitList[idx].Reset();
		RestartCompression();				// if they are removing a clan texture, we need to restart compression
	}
}


void CCompressedPedDamageSet::ReleaseDecorationBlitTextures()
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::ReleaseDecorationBlitTextures ped=0x%p(%s)",m_pPed.Get(), GetPedName(m_pPed.Get()));

	// release the blit textures from memory, we'll need to stream them back in if we need to recompress.
	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
		m_DecorationBlitList[i].ReleaseDecorationTexture();
}

u16 CCompressedPedDamageSet::CalcComponentMask()
{
	return m_ComponentMask;
}

void CCompressedPedDamageSet::Render(const atUserArray<CPedDamageCompressedBlitDecoration> &decorationArray, const grcRenderTarget * blitTarget, const atRangeArray<CPedDamageCylinder, kDamageZoneNumZones> & zoneArray)
{
	bool hasBlits = decorationArray.GetCount()>0;

	grcViewport orthoViewport, zoneVP;
	orthoViewport.Ortho(0.0f,1.0f,1.0f,0.0f,-1.0f,1.0f);

	const grcRenderTarget * target = blitTarget;

	if (target)
	{
		bool sideWays = (target->GetWidth()>target->GetHeight());
		
		grcTextureFactory::GetInstance().LockRenderTarget(0, target, CPedDamageSetBase::GetDepthTarget(target), 0U, true);
		
		grcViewport::SetCurrent(&orthoViewport);

		CPedDamageSetBase::SetupHiStencil(false);

#if RSG_DURANGO
		//clear doesn't appear to work on durango correctly with a linear texture, 
		//seem to only clear alternating lines, like the pitch is incorrect
		if (PEDDECORATIONBUILDER.IsUsingUncompressedMPDecorations()== false)
			GRCDEVICE.ClearRect(0,0, target->GetWidth(), target->GetHeight(), true, Color32(0,0,0,127), true, 1.0, 0);
		else
			GRCDEVICE.Clear(true, Color32(0,0,0,127), false, 1.0, true, 0);
#else
		GRCDEVICE.Clear(true, Color32(0,0,0,127), false, 1.0, true, 0);
#endif

#if CLEAR_WITH_COLOR_RECTS
		int w = target->GetWidth();
		int h = target->GetHeight();

		//debug pattern
		GRCDEVICE.ClearRect(0,0, w/2, h/2, true, Color32(255,0,0,127), false, 1.0, 0);
		GRCDEVICE.ClearRect(w/2,0, w/2, h/2, true, Color32(0,255,0,127), false, 1.0, 0);
		GRCDEVICE.ClearRect(w/2, h/2, w/2, h/2, true, Color32(0,0,255,127), false, 1.0, 0);
		GRCDEVICE.ClearRect(0,h/2, w/2, h/2, true, Color32(255,255,0,127), false, 1.0, 0);
#endif

		bool hasSkinBlits = false;

		if (hasBlits)
		{		
			// need to break this up into 2 passes, skin only first, then cloth only
			for (int pass = 0; pass<2; pass++)
			{
				for (int zone=0;zone<kDamageZoneNumZones;zone++)
				{
					bool vpSet = false;
					float aspectRatio=1.0f;

					for (int i=0;i<decorationArray.GetCount();i++)
					{
						// skip pass if nothing is needed for the blit here...						
						if ((decorationArray[i].GetZone() != zone) ||
							(decorationArray[i].GetDecorationTexture()==NULL))
							continue;

						if ((pass==0 && decorationArray[i].IsOnClothOnly()) || 
							(pass==1 && decorationArray[i].IsOnSkinOnly()))
							continue;

						if (pass==0)
							hasSkinBlits = true;

						if (!vpSet)	
						{
							aspectRatio = zoneArray[zone].GetHeight()/(PI*2*zoneArray[zone].GetRadius());
							vpSet = true;
							CPedDamageSet::SetupViewport( &zoneVP, zone,sideWays,false,1);
						}

						for (int j=0; j<1; j++)
						{
							decorationArray[i].RenderBegin(j, 1);
							int actualCount = decorationArray[i].Render(aspectRatio, zone>=kDamageZoneLeftArm);
							decorationArray[i].RenderEnd(2-actualCount);
						}
					}
				}
			}
		}

		if (hasSkinBlits)
			CPedDamageSetBase::ProcessSkinAlpha(orthoViewport);

		CPedDamageSetBase::DisableHiStencil();

		CPedDamageManager::PreDrawStateSetup();
		grcViewport::SetCurrent(NULL);
	
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}
}

bool CCompressedPedDamageSet::GetDecorationInfo(int idx, atHashString& colHash, atHashString& presetHash) const
{
	if (idx < 0 || idx > m_DecorationBlitList.GetCount())
	{
		return false;
	}

	presetHash = m_DecorationBlitList[idx].GetPresetNameHash();
	colHash = m_DecorationBlitList[idx].GetCollectionNameHash();

	return true;
}


void CCompressedPedDamageSet::CheckForTexture()
{
	CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::CheckForTexture: m_State = %s ped=0x%p(%s)",s_CompressedPedDamageSetState[m_State], m_pPed.Get(), GetPedName(m_pPed.Get()));

	Assert(m_State == kWaitingOnTextures);

	// keep requesting output texture if needed
	bool stillWaiting = LoadOutputTexture();

	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
	{
		CPedDamageCompressedBlitDecoration & blit = m_DecorationBlitList[i];
		// check to see if it's ready to load/set	
		if (blit.WaitingForDecorationTexture())
		{
			stillWaiting = blit.LoadDecorationTexture(m_pPed) || stillWaiting;
		}
	}

	// update state
	if (stillWaiting == false) 
	{
		CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::CheckForTexture: Transitioning to kTextureLoaded State, m_State = %s ped=0x%p(%s)",s_CompressedPedDamageSetState[m_State], m_pPed.Get(), GetPedName(m_pPed.Get()));
		m_State = kTexturesLoaded;
	}
}

void CCompressedPedDamageSet::DumpDecorations()
{
#if !__NO_OUTPUT
	Displayf("   decorations_compressed:");
	for (int i=0;i<m_DecorationBlitList.GetCount();i++)
	{
		CPedDamageCompressedBlitDecoration & blit = m_DecorationBlitList[i];
		{
			Vector4 uvCoords = blit.GetUVCoords(); 

			float rotation = 0.0f;
			Vector2 dir(uvCoords.z,uvCoords.w);
			float mag = dir.Mag();
			if (mag>0.001)
			{
				dir /= mag;
				rotation = atan2(dir.y,dir.x)*RtoD;
				rotation = (rotation >= 0.0f) ? rotation : 360.0f+rotation;
			}

			float age = TIME.GetElapsedTime() - blit.GetBirthTime();
			u8 uvFlags = blit.GetUVFlipFlags();

			Displayf("     i[%d] zone[%d][%s] type[%d][%s] uvCoords[%f %f %f %f] uvFlags[0x%x] rotation[%f] age[%f] aintensity[%f] collectionHash[%s] presetHash[%s] waitingfordecorationtexture[%d]"
				,i
#if __BANK
				,blit.GetZone()
				,blit.GetZone() <= kDamageZoneNumZones ? s_ScriptZoneEmumNames[blit.GetZone()] : ""
				,blit.GetType()
				,blit.GetType() < kNumDamageTypes ? s_DamageTypeNames[blit.GetType()] : ""
#else
				,blit.GetZone()
				,""
				,blit.GetType()
				,""
#endif
				,uvCoords.x,uvCoords.y,uvCoords.z,uvCoords.w 
				,uvFlags
				,rotation
				,age
				,blit.GetAlphaIntensity()
				,blit.GetCollectionNameHash().TryGetCStr()
				,blit.GetPresetNameHash().TryGetCStr(),
				blit.WaitingForDecorationTexture());
		}
	}
#endif
}

bool CCompressedPedDamageSet::SetOutputTexture(const strStreamingObjectName outTxdHash, const strStreamingObjectName outTxtHash)
{
	// already setup
	if (m_pOutTexture != NULL) 
	{
		return true;
	}

	if (outTxdHash.IsNotNull())
	{
		m_OutTxdHash = outTxdHash;
		m_OutTxtHash = outTxtHash;
		LoadOutputTexture();
		return true;
	}
	else
	{
		m_OutTxdHash .Clear();
		m_OutTxtHash .Clear();
	}

	return false;

}



bool CCompressedPedDamageSet::LoadOutputTexture()
{
	// If we're using uncompressed decoration textures all we need to do is hook up the correct render target
	if (PEDDECORATIONBUILDER.IsUsingUncompressedMPDecorations())
	{
		m_pOutTexture = PEDDECORATIONBUILDER.GetTextureManager().GetRenderTarget(m_OutTxdHash);
		if (m_pOutTexture == NULL)
		{
			m_OutTxdHash.Clear();
			m_OutTxtHash.Clear();
		}
		return false;
	}

	// check if our TXD is loaded yet, if not bale
	strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_OutTxdHash.GetHash()));
	if(!g_TxdStore.HasObjectLoaded(txdId))
	{
		CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::LoadOutputTexture: tdxId %d (%s), isRequested = %s, isLoading = %s", txdId.Get(), g_TxdStore.GetName(txdId), g_TxdStore.IsObjectRequested(txdId)?"true":"false", g_TxdStore.IsObjectLoading(txdId)?"true":"false" );
		if (!g_TxdStore.IsObjectRequested(txdId) && !g_TxdStore.IsObjectLoading(txdId))
		{
		
			//If the object isn't loading and isn't requested then re-initiate the streaming request to ensure it continues to try to be loaded -- in some cases it was getting cleared out (not sure why).
			g_TxdStore.StreamingRequest(txdId, STRFLAG_FORCE_LOAD);	
		}

		return true;  // let the higher level code know we did not get the texture yet.
	}

	// make sure out texture exists in the TXD
	fwTxd* pTxd = g_TxdStore.Get(txdId);

	grcTexture * pTex = pTxd->Lookup(m_OutTxtHash.GetHash());
	if (Verifyf(pTex, "LoadOutputTexture() - Texture '%s' does not exist in txd '%s'",m_OutTxtHash.GetCStr(),m_OutTxdHash.GetCStr()))
	{
		if (m_pOutTexture==NULL && m_bOutputTexRefAdded)
		{
			CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::LoadOutputTexture: tdxId %d (%s), (m_pOutTexture==NULL && m_bOutputTexRefAdded) numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), g_TxdStore.GetNumRefs(txdId));
		}

		// okay save texture pointer and add refs
		m_pOutTexture = pTex;
		if (m_bOutputTexRefAdded == false)
		{
			g_TxdStore.AddRef(txdId, REF_RENDER);
			m_bOutputTexRefAdded = true;

			CPEDDEBUG_DISPLAYF("CCompressedPedDamageSet::LoadOutputTexture: tdxId %d (%s), numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), g_TxdStore.GetNumRefs(txdId));	
		}
	}
	else
	{
		// well darn, it's not in the txd, let's clear the txd hash so we don't keep looking for non existent textures.
		m_OutTxdHash.Clear();
		m_OutTxtHash.Clear();
	}
	return false;
}

int CCompressedPedDamageSet::FindFreeDecorationBlit()
{
	CPedDamageCompressedBlitDecoration blit;
	blit.Reset();

	// add one to the end and return it's index
	int index = m_DecorationBlitList.GetCount();

	// don't allow more than kMaxTattooBlits
	if (index >= kMaxTattooBlits)
		return -1;

	m_DecorationBlitList.PushAndGrow(blit);

	return index;
}

//
// CPedDamageCylinder
//

CPedDamageCylinder::CPedDamageCylinder()
{
	m_Base.Zero();
	m_Up.Zero();
	m_Height=0.0f;
	m_Radius=0.0f;
	m_Zone=0;
	m_PushAreaArray = NULL;
}

float CPedDamageCylinder::CalcScale() const 
{
	if (m_Zone==kDamageZoneMedals)
		return 1.0f;
			   
	float scale = 0.5;				// all limbs are 1/2 the texture height wide (they are sideways)

	if (m_Zone<=kDamageZoneHead)	// head and torso are right side up, so we get the scale from the viewport mapping
		scale = s_ViewportMappings[m_Zone].w; 

	scale /= GetHeight();			// tunning assumes 1m height

	return scale;
}

float CPedDamageCylinder::CalcAspectRatio() const
{
	return GetHeight()/(PI*2*GetRadius());
}


// see description of the cylinder mapping requirement at the top of this file.
// the need to match the method used by the art pipeline to generate the UVs.
Vector2 CPedDamageCylinder::MapLocalPosToUV(const Vector3 & localPos)
{
	Vector2 uv;
	float distUpCenter = m_Up.Dot(localPos - m_Base);

	Vector2 cylinderSpace;
	Vector3 r,u;
	float forwardness = m_Up.Dot(YAXIS);
	r.Cross(m_Up, (Abs(forwardness) > 0.99f) ? ZAXIS : YAXIS);   // use YAXIS, unless we're looking down it
	r.Normalize(); 
	u.Cross(r, m_Up);
	u.Normalize();

 	Vector3  closestPointOnBone;
	closestPointOnBone.AddScaled(m_Base,m_Up,distUpCenter);
	Vector3 vectorFromCylinder;
	vectorFromCylinder.Subtract(localPos,closestPointOnBone);
	vectorFromCylinder.Normalize();

	cylinderSpace.x = Dot(u, vectorFromCylinder);
	cylinderSpace.y = Dot(r, vectorFromCylinder);
	
	uv.x = atan2f(cylinderSpace.x, cylinderSpace.y)/(2.0f*PI);
	
	if( m_Zone <= kDamageZoneHead) // the head and tosro are reversed
		uv.x = fmodf((1-uv.x)+m_Rotation,1.0f); 
	else
		uv.x = fmodf((uv.x)+m_Rotation,1.0f); 

	uv.x = Wrap(uv.x, 0.0f, 1.0f); // the m_rotation shift can make them go out of bounds.

	// calculate the 'v' value as the distance above the base, normalized to the length of the spine
	uv.y = distUpCenter / m_Height;

	return uv;
}

Vector2 CPedDamageCylinder::AdjustForPushAreas(const Vector2 &uv, float radius, bool & limitScale, bool hasDress, Vector2 * uvPush, bool bFromWeapon)
{
	Vector2 uvNew = uv;

	// loop though the push areas 
	if (m_Zone<kDamageZoneMedals && m_PushAreaArray)
	{
		radius *= CalcScale() * M_SQRT2/2.0f;

		if (m_Zone<=kDamageZoneHead)	 // the torso get 2x size applied automagically
			radius *= s_TorsoSizeBoost;
		else if (bFromWeapon)
			radius *= s_LimbSizeBoost;

		Vector2 pushAmount(0.0f,0.0f);

		for (int i = 0; i < m_PushAreaArray->GetCount(); i++)
		{
			if ((*m_PushAreaArray)[i].CalcPushAmount(uvNew, radius, pushAmount, hasDress)) 
			{
				uvNew += pushAmount; 
				limitScale = true; // don't want to do this anymore, it makes a row is small bullet wounds along the seam
			}
		}
	}

	if (uvPush)
		*uvPush = uvNew - uv;

	return uvNew;
}

bool CPedDamagePushArea::CalcPushAmount(const Vector2 &inCoord, float radius, Vector2 & outPush, bool hasDress) const
{
	// add push logic here more
	outPush.Set(0.0f,0.0f);
	Vector2 pushDir;
	ePushType type = m_Type;

	if (type == kPushRadialUDress) 
	{ 
		// same as kPushRadalU, but condition on having a dress 
		if (hasDress)
			type = kPushRadialU;
	}

	if (type == kPushRadialU)
	{ 
		// TODO vectorize this to use Vec2V
		Vector2 center = m_Center;
		
		for (int i=0;i<2;i++)
		{
			// this is a u push away from the u center of a ellipse, weird, but it works best around the shoulders, etc.
			
			//float inV = inCoord.y + ((inCoord.y<center.y) ? radius : -radius);
			float v = abs( inCoord.y - center.y);
			if (v < m_RadiusOrAmpDir.y + radius)
			{
				float ev = v/m_RadiusOrAmpDir.y;														// in ellipse space
				float width = (ev<1.0) ? sqrtf(1-ev*ev) : -(v-m_RadiusOrAmpDir.y)/m_RadiusOrAmpDir.y;	// find the "width" of the ellipse with at v (if past the end, just do a little linear tail)
				width *= m_RadiusOrAmpDir.x;															// wdith is "world" space
				
				float u = abs(inCoord.x-center.x);									
				if (u<width+radius)																		// is we're inside we need to push out
				{
					float distToPush = (width-u);	// back to world space.

					outPush.Set ((inCoord.x<center.x) ? -(distToPush+radius) : (distToPush+radius), 0.0f);
					
					if (inCoord.x+outPush.x<0) // some times we PUSH them off the Edge....
						outPush.x = outPush.x+1;
					else if (inCoord.x+outPush.x>1.0 )
						outPush.x = outPush.x-1;
					
					return true;
				}
			}

			// only checking for U wrap for now.
			if (center.x+m_RadiusOrAmpDir.x<=1.0f && center.x-m_RadiusOrAmpDir.x>=0.0f)
				break; // don't need a reflected version

			center.x = (center.x+m_RadiusOrAmpDir.x>1.0f) ? center.x - 1 :  center.x + 1;
		}
	}
	else if (type == kPushSine)
	{
		// evaluate a sinewave using the u value to get a push height
		float s = sinf(inCoord.x + (m_Center.x*2.0f*PI+PI/2));
		float pushLineV = m_Center.y + s*m_RadiusOrAmpDir.x;  // this is the height along the sine wave we will push from

		if (m_RadiusOrAmpDir.y>0)
		{
			float distToPush = pushLineV - (inCoord.y-radius);
			if (distToPush>0.0f)
			{
				outPush.Set(0.0f,distToPush);
				return true;
			}
		}
		else  // negative push
		{
			float distToPush = pushLineV - (inCoord.y+radius);
			if (distToPush<0.0f)
			{
				outPush.Set(0.0f,distToPush);
				return true;
			}
		}
	}

	return false;
}


void CPedDamagePushArea::DebugDraw(bool hasDress) const
{

	ePushType type = m_Type;

	if (type == kPushRadialUDress) 
	{ 
		// same as kPushRadalU, but condition on having a dress 
		if (hasDress)
			type = kPushRadialU;
	}

	if (type == kPushRadialU)
	{ 
		static const int kSegments = 16;
		float stepSize = 2.0f*PI/kSegments;
		Vector2 center = m_Center;
		
		for (int i=0;i<2;i++)
		{
			grcBegin(drawLineStrip, kSegments+1); // +1 for closure
			grcColor3f(1.0f, 0.0f, 0.0f);
			for (int i = 0; i <= kSegments; i++)
			{
				float s = sinf(stepSize*i);
				float c = cosf(stepSize*i);
				grcVertex2f(center.x + c*m_RadiusOrAmpDir.x, center.y + s*m_RadiusOrAmpDir.y);
			}
			grcEnd();
			
			// only checking for U wrap for now.
			if (center.x+m_RadiusOrAmpDir.x<=1.0f && center.x-m_RadiusOrAmpDir.x>=0.0f)
				break; // don't need a reflected version
			
			center.x = (center.x+m_RadiusOrAmpDir.x>1.0f) ? center.x - 1 :  center.x + 1;
		}
	}
	else if (type == kPushSine)
	{
		static const int kSegments = 16;
		float stepSize = 2.0f*PI/kSegments;

		grcBegin(drawLineStrip, kSegments+1);
		grcColor3f(1.0f, 0.0f, 0.0f);
		for (int i = 0; i <= kSegments; i++)
		{
			float s = sinf(stepSize*i + (m_Center.x*2.0f*PI+PI/2));
		
			grcVertex2f( i/float(kSegments),  m_Center.y + s*m_RadiusOrAmpDir.x);
		}
		grcEnd();
	}
	
}


//
// CPedDamageBlitBlood & CPedDw = amageBlitDecoration
//

void  CPedDamageBlitBase::Set(const Vector4 & uvs, ePedDamageZones zone, ePedDamageTypes type, float alpha, float preAge, u8 flipUVflags)
{
	Vector2 dir;   // pre calc the direction and store it in second uv
	dir.Set(uvs.z-uvs.x, uvs.w-uvs.y); 
	dir.Normalize(); 
	Vector4 uvAndDir(uvs.x,uvs.y,dir.x,dir.y);

	Float16Vec4Pack(&m_UVCoords, RCC_VEC4V(uvAndDir)); 
	m_Zone = zone;
	m_Type = type;
	m_BirthTime = TIME.GetElapsedTime() - preAge;
	m_Done = false;
	m_AlphaIntensity = u8(Clamp(alpha,0.0f,1.0f)*63);
	m_FlipUVFlags=flipUVflags;
}


void  CPedDamageBlitBlood::Set(const Vector4 & uvs, ePedDamageZones zone, ePedDamageTypes type, CPedBloodDamageInfo * pBloodInfo, float scale, bool limitScale, bool enableSoakTextureGravity, float preAge, int forcedFrame, u8 flipUVflags)
{
	CPedDamageBlitBase::Set(uvs, zone, type, 1.0f, preAge, flipUVflags);
	
	SetScale(scale);
	m_ReBirthTime = m_BirthTime;
	m_LimitScale = limitScale;
	m_BloodInfoPtr = pBloodInfo;
	m_FixedSoakFrameOrAnimSequence = 0; 
	m_FixedWoundFrameOrAnimSequence = 0; 
	m_FixedSplatterFrameOrAnimSequence = 0; 
	m_NeedsScar = 0;
	m_UseSoakTextureGravity = false;
	m_FromWeapon = false;
	m_ReduceBleeding = false;

	if (pBloodInfo)
	{
		m_UseSoakTextureGravity = (pBloodInfo->m_RotationType == kGravityRotation || (pBloodInfo->m_RotationType == kAutoDetectRotation && enableSoakTextureGravity));

		// check save need a scar flag (we'll clear it once we trigger the scar)
		m_NeedsScar = (pBloodInfo->m_ScarIndex >= 0) ? 1 : 0; // it would be nice if we could check material here, and not put scars under the cloths
		m_FixedWoundFrameOrAnimSequence = pBloodInfo->m_WoundTexture.CalcFixedFrameOrSequence(forcedFrame);
		int requestSoakSyncFrame = -1;
		if(pBloodInfo->m_SyncSoakWithWound)
			requestSoakSyncFrame = int(m_FixedWoundFrameOrAnimSequence - ((pBloodInfo->m_WoundTexture.m_AnimationFPS==0.0f)?pBloodInfo->m_WoundTexture.m_FrameMin:0));  // want the random frame or sequence picked
		m_FixedSoakFrameOrAnimSequence = (m_UseSoakTextureGravity ? pBloodInfo->m_SoakTextureGravity.CalcFixedFrameOrSequence(requestSoakSyncFrame) : pBloodInfo->m_SoakTexture.CalcFixedFrameOrSequence(requestSoakSyncFrame));
		m_FixedSplatterFrameOrAnimSequence = pBloodInfo->m_SplatterTexture.CalcFixedFrameOrSequence(requestSoakSyncFrame);
	}
}

bool  CPedDamageBlitDecoration::Set(atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo, const EmblemDescriptor* pEmblemDesc, const Vector4 & uvs, const Vector2 & scale, ePedDamageZones zone, ePedDamageTypes type, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, float alpha, CPedDamageDecalInfo * pSourceInfo, float fadeInTime, float fadeOutStartTime, float fadeOutTime, float preAge, int forcedFrame, u8 flipUVflags)
{
	Assert(m_RefAdded==0); // they should have called reset at some point before calling set...
	
	CPedDamageBlitBase::Set(uvs, zone, type, alpha, preAge, flipUVflags);
	m_DecorationTexture = NULL;
	m_RefAdded = 0;
	m_FadeInTime = (u16)Min(int(fadeInTime*10), 0x7ff);		// uses 11 bits of a u16
	m_FadeOutTime = (u16)Min(int(fadeOutTime*64.0f),0xffff);

	m_FadeOutStartTime = 0xffff;							// no fading out needed
	m_pSourceInfo = pSourceInfo;							// could be scar or dirt source info, don't care at this point
	m_FixedFrameOrAnimSequence = 0;
	m_CollectionHash = colName;
	m_PresetHash = presetName;
	m_Scale = scale;

	if (pTintInfo)
	{
		m_TintInfo = *pTintInfo;
	}
	else
	{
		m_TintInfo.Reset();
	}

	if (pEmblemDesc)
	{
		m_EmblemDesc = *pEmblemDesc;
	}
	else
	{
		m_EmblemDesc.Invalidate();
	}

	if (fadeOutStartTime>=0)
		m_FadeOutStartTime = (u16)Min(int(fadeOutStartTime*64.0f),0xfffe);
		
	if(m_pSourceInfo)
		m_FixedFrameOrAnimSequence = (u16) m_pSourceInfo->GetTextureInfo().CalcFixedFrameOrSequence(forcedFrame);

	if (txdHash.IsNotNull())
	{
		m_TxdHash = txdHash;
		m_TxtHash = txtHash;
		return LoadDecorationTexture(NULL);
	}
	else
	{
		m_EmblemDesc.Invalidate();
		m_TintInfo.Reset();
		m_TxdHash.Clear();
		m_TxtHash.Clear();
		return false;
	}
}

bool CPedDamageBlitDecoration::WaitingForDecorationTexture() const 
{
	bool bWaitingForTexture = m_TxdHash.IsNotNull() && m_DecorationTexture==NULL;
	bool bWaitingForTintInfo = (m_TintInfo.bValid && m_TintInfo.bReady == false);

#if __BANK
	if (m_TintInfo.bValid)
		PEDDEBUG_DISPLAYF("CPedDamageBlitDecoration::WaitingForDecorationTexture: waitingOnTexture: %d, waitingOnTintInfo: %d", bWaitingForTexture, bWaitingForTintInfo);
#endif

	return bWaitingForTexture || bWaitingForTintInfo;
}

bool CPedDamageBlitDecoration::LoadDecorationTexture(const CPed* pPed)
{	
	if (!m_EmblemDesc.IsValid())
	{
		// check if our TXD is loaded yet, if not bale
		strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_TxdHash.GetHash()));
		if(!g_TxdStore.HasObjectLoaded(txdId))
		{
			//If the object isn't loading and isn't requested then re-initiate the streamingrequest to ensure it continues to try to be loaded -- in some cases it was getting cleared out (not sure why).
			if (!g_TxdStore.IsObjectLoading(txdId) && !g_TxdStore.IsObjectRequested(txdId))
				g_TxdStore.StreamingRequest(txdId, STRFLAG_FORCE_LOAD);

			return true;  // let the higher level code know we did not get the texture yet.
		}

		// make sure out texture exists in the TXD
		fwTxd* pTxd = g_TxdStore.Get(txdId);

		grcTexture * pTex = pTxd->Lookup(m_TxtHash.GetHash());
		if (Verifyf(pTex, "LoadDecorationTexture() - Texture '%s' does not exist in txd '%s'",m_TxtHash.GetCStr(),m_TxdHash.GetCStr()))
		{
			// okay save texture pointer and add refs
			m_DecorationTexture = pTex;
			m_TxdId = txdId.Get();

			if (m_RefAdded == 0)
			{
				g_TxdStore.AddRef(txdId, REF_RENDER);
				m_RefAdded = 1;

				CPEDDEBUG_DISPLAYF("CPedDamageBlitDecoration::LoadDecorationTexture: tdxId %d (%s), m_TxtHash = %s, numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), m_TxtHash.GetCStr(), g_TxdStore.GetNumRefs(txdId));
			}
		}
		else
		{
			// well darn, it's not in the txd, let's clear the txd hash so we don't keep looking for non existent textures.
			m_TxdHash.Clear();
			m_TxtHash.Clear();
			m_Done = true; 
		}

		// Assume we're ready to go
		bool bKeepPolling = false;

		// If we have tint info we might need to wait for the tint data to be ready
		if (m_TintInfo.bValid && m_TintInfo.bReady == false)
		{
			// So try getting it again...
			CPedDamageManager::GetTintInfo(pPed, m_TintInfo);
			bKeepPolling = (m_TintInfo.bReady == false);
		}

		return bKeepPolling;
	}
	// we're waiting on the crew emblem texture
	else
	{
		if (m_DelayedClanTextureLoad)
		{
			// keep requesting it until it's ready 
			m_DelayedClanTextureLoad = NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(m_EmblemDesc  ASSERT_ONLY(, "CPedDamageManager")) == false;
		}

		const char* pClanEmblemTXD = NETWORK_CLAN_INST.GetCrewEmblemMgr().GetEmblemName(m_EmblemDesc);

		if (pClanEmblemTXD)
		{
			fwTxd* pTxd = g_TxdStore.GetSafeFromName(pClanEmblemTXD);
			if (pTxd)
			{
				grcTexture* pTexture = pTxd->Lookup(pClanEmblemTXD);
				if (pTexture)
				{
					m_DecorationTexture = pTexture;
					m_RefAdded = 1;
					return false;
				}
			}
		}

		// not here yet, keep waiting
		return true;
	}
}


bool  CPedDamageCompressedBlitDecoration::Set(atHashString colName, atHashString presetName, const PedDamageDecorationTintInfo* pTintInfo, const EmblemDescriptor* pEmblemDesc, const Vector4 & uvs, const Vector2 & scale, ePedDamageZones zone, ePedDamageTypes type, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash)
{
	CPedDamageBlitBase::Set(uvs, zone, type, 1.0f, 0);
	m_DecorationTexture = NULL;

	m_PresetHash = presetName;
	m_CollectionHash = colName;
	m_Scale = scale;

	if (pTintInfo)
	{
		m_TintInfo = *pTintInfo;
	}
	else
	{
		m_TintInfo.Reset();
	}

	if (pEmblemDesc)
	{
		m_EmblemDesc = *pEmblemDesc;
	}
	else
	{
		m_EmblemDesc.Invalidate();
	}

	if (txdHash.IsNotNull())
	{
		m_TxdHash = txdHash;
		m_TxtHash = txtHash;
		return LoadDecorationTexture(NULL);
	}
	else
	{
		m_TintInfo.Reset();
		m_EmblemDesc.Invalidate();
		m_TxdHash.Clear();
		m_TxtHash.Clear();
		return false;
	}
}

bool CPedDamageCompressedBlitDecoration::WaitingForDecorationTexture() const 
{
	bool bWaitingForTexture = m_TxdHash.IsNotNull() && m_DecorationTexture==NULL;
	bool bWaitingForTintInfo = (m_TintInfo.bValid && m_TintInfo.bReady == false);

#if __BANK
	if (m_TintInfo.bValid)
		PEDDEBUG_DISPLAYF("CPedDamageCompressedBlitDecoration::WaitingForDecorationTexture: waitingOnTexture: %d, waitingOnTintInfo: %d", bWaitingForTexture, bWaitingForTintInfo);
#endif

	return bWaitingForTexture || bWaitingForTintInfo;
}

bool CPedDamageCompressedBlitDecoration::LoadDecorationTexture(const CPed* pPed)
{
	if (!m_EmblemDesc.IsValid())
	{
		// check if our TXD is loaded yet, if not bale
		strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_TxdHash.GetHash()));
		if(!g_TxdStore.HasObjectLoaded(txdId))
		{
			//If the object isn't loading and isn't requested then re-initiate the streaming request to ensure it continues to try to be loaded -- in some cases it was getting cleared out (not sure why).
			if (!g_TxdStore.IsObjectLoading(txdId) && !g_TxdStore.IsObjectRequested(txdId))
				g_TxdStore.StreamingRequest(txdId, STRFLAG_FORCE_LOAD);

			return true;  // let the higher level code know we did not get the texture yet.
		}

		// make sure out texture exists in the TXD
		fwTxd* pTxd = g_TxdStore.Get(txdId);

		grcTexture * pTex = pTxd->Lookup(m_TxtHash.GetHash());
		if (Verifyf(pTex, "LoadDecorationTexture() - Texture '%s' does not exist in txd '%s'",m_TxtHash.GetCStr(),m_TxdHash.GetCStr()))
		{
			// okay save texture pointer and add refs
			m_DecorationTexture = pTex;

			if (m_RefAdded == 0)
			{
				g_TxdStore.AddRef(txdId, REF_RENDER);
				m_RefAdded = 1;

				CPEDDEBUG_DISPLAYF("CPedDamageCompressedBlitDecoration::LoadDecorationTexture: tdxId %d (%s), numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), g_TxdStore.GetNumRefs(txdId));
			}
		}
		else
		{
			CPEDDEBUG_DISPLAYF("CPedDamageCompressedBlitDecoration::LoadDecorationTexture: cannot find txd - clear the txd hash so we don't keep looking for non existent textures");

			// well darn, it's not in the txd, let's clear the txd hash so we don't keep looking for non existent textures.
			m_TxdHash.Clear();
			m_TxtHash.Clear();
			m_Done = true; 
		}

		// Assume we're ready to go
		bool bKeepPolling = false;

		// If we have tint info we might need to wait for the tint data to be ready
		if (m_TintInfo.bValid && m_TintInfo.bReady == false)
		{
			// So try getting it again...
			CPedDamageManager::GetTintInfo(pPed, m_TintInfo);
			bKeepPolling = (m_TintInfo.bReady == false);
		}

		return bKeepPolling;

	}
	// we're waiting on the crew emblem texture
	else
	{
		const char* pClanEmblemTXD = NETWORK_CLAN_INST.GetCrewEmblemMgr().GetEmblemName(m_EmblemDesc);

		if (pClanEmblemTXD)
		{
			fwTxd* pTxd = g_TxdStore.GetSafeFromName(pClanEmblemTXD);
			if (pTxd)
			{
				grcTexture* pTexture = pTxd->Lookup(pClanEmblemTXD);
				if (pTexture)
				{
					m_DecorationTexture = pTexture;
					m_RefAdded = 1;
					return false;
				}
			}
		}

		// not here yet, keep waiting
		return true;
	}
}

// after compression, we release the textures from memory
void CPedDamageCompressedBlitDecoration::ReleaseDecorationTexture()
{
	// only non clan textures
	if (!m_EmblemDesc.IsValid())
	{
		// check if our TXD is loaded yet, if not bale
		if (m_RefAdded)
		{
			s32 txdId = g_TxdStore.FindSlotFromHashKey(m_TxdHash.GetHash()).Get();
			if(txdId != -1)
				CPedDamageSetBase::DelayedRemoveRefFromTXD(txdId);
			m_RefAdded = 0;
		}

		m_DecorationTexture = NULL;
	}
}

void CPedDamageCompressedBlitDecoration::Reset()
{
	m_Done=true;

	if (!m_EmblemDesc.IsValid())
	{
		if (m_DecorationTexture)
		{
			strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_TxdHash.GetHash()));

			if(m_RefAdded && txdId != -1)
			{
				CPedDamageSetBase::DelayedRemoveRefFromTXD(txdId.Get());

				CPEDDEBUG_DISPLAYF("CPedDamageCompressedBlitDecoration::Reset: tdxId %d (%s), numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), g_TxdStore.GetNumRefs(txdId));
			}
		}
	}
	// we're dealing with a crew emblem texture
	else
	{
		if(m_RefAdded)
		{
			NETWORK_CLAN_INST.GetCrewEmblemMgr().ReleaseEmblem(m_EmblemDesc  ASSERT_ONLY(, "CPedDamageManager"));
		}

		m_EmblemDesc.Invalidate();
	}

	m_RefAdded = 0;
	m_DecorationTexture = NULL;
	m_TxdHash.Clear();
	m_TxtHash.Clear();
	m_CollectionHash = 0U;
	m_PresetHash = 0U;
	m_TintInfo.Reset();
}

static void GetTintInfoRenderData(const PedDamageDecorationTintInfo& tintInfo, const grcTexture*& pPalTexture, Vector3& palSelector, s32 cachedPalSelector1, s32 cachedPalSelector2)
{
	// tint info is not valid at all
	if (tintInfo.bValid == false)
	{
		palSelector.x = 0.0f;
		palSelector.y = 0.0f;
		palSelector.z = 0.0f; // do not blend in tint color
		pPalTexture = NULL;
	}
	else 	// it is valid, but might not be ready
	{
		// cache palette texture (might be NULL at this point)
		pPalTexture = tintInfo.pPaletteTexture;

		// assume there's no valid tint info
		bool bValidTintInfo = false;

		// is it ready?
		if (tintInfo.bReady)
		{
			palSelector.x = ((float(tintInfo.paletteSelector)+0.5f) * (1.0f/64.0f));
			palSelector.y = ((float(tintInfo.paletteSelector2)+0.5f) * (1.0f/64.0f));
			bValidTintInfo = (tintInfo.pPaletteTexture != NULL && tintInfo.paletteSelector != -1 && tintInfo.paletteSelector2 != -1);
		}
		// otherwise, is there a valid index we've previously cached?
		else
		{
			palSelector.x = ((float(cachedPalSelector1)+0.5f) * (1.0f/64.0f));
			palSelector.y = ((float(cachedPalSelector2)+0.5f) * (1.0f/64.0f));
			bValidTintInfo = (tintInfo.pPaletteTexture != NULL && cachedPalSelector1 != -1 && cachedPalSelector2 != -1);

		}

		palSelector.z = (bValidTintInfo ? 1.0f : 0.0f); // only blend with tint color if there's valid data
	}
}

bool CPedDamageCompressedBlitDecoration::RenderBegin(int pass, int count) const 
{
	const grcTexture* pPalTexture = NULL;
	Vector3 palSelector;
	GetTintInfoRenderData(m_TintInfo, pPalTexture, palSelector, -1, -1);

	bool bIsTattoo = IsOnSkinOnly();

	CPedDamageSetBase::SetDecorationBlitShaderVars(m_DecorationTexture, pPalTexture, palSelector, (const CPedDamageTexture*)NULL, (const CPedDamageTexture*)NULL, bIsTattoo);
	
	grcStateBlock::SetBlendState(CPedDamageManager::GetDecorationBlendState(pass),~0U,~0ULL);
	grcStateBlock::SetDepthStencilState(CPedDamageManager::GetSetStencilState(), IsOnSkinOnly()? 0x1:0x0);
	grcEffectTechnique technique = CPedDamageSetBase::sm_DrawDecorationTechnique[pass];

	const unsigned rti = g_RenderThreadIndex;

	if (AssertVerify(CPedDamageSetBase::sm_Shader->BeginDraw(grmShader::RMC_DRAW, false, technique) ))
	{
		CPedDamageSetBase::sm_Shader->Bind(0);
		{
			//set up fvf for blood
			GRCDEVICE.SetVertexDeclaration(CPedDamageSetBase::sm_VertDecl);  
			// allocate enough room for 2 quads for each blit (in case it hits an edge)
			grcDrawMode drawMode = API_QUADS_SUPPORT ? drawQuads : drawTriStrip;
			u32 geomCount = (API_QUADS_VERTS_PER_QUAD*2) * count;

			CPedDamageSetBase::sm_ActiveVertexBuffer[rti] = (float*)GRCDEVICE.BeginVertices(drawMode, geomCount,CPedDamageSetBase::sm_VertDecl->Stream0Size);
			if (!Verifyf(CPedDamageSetBase::sm_ActiveVertexBuffer[rti], "GRCDEVICE.BeginVertices failed"))
			{
				CPedDamageSetBase::sm_Shader->UnBind();
				CPedDamageSetBase::sm_Shader->EndDraw();
				return false;
			}
		}
		return true;
	}
	else
	{
		CPedDamageSetBase::sm_ActiveVertexBuffer[rti] = NULL;
		return false;
	}
}

void CPedDamageCompressedBlitDecoration::RenderEnd(int count) const
{
	const unsigned rti = g_RenderThreadIndex;
	Assert(CPedDamageSetBase::sm_ActiveVertexBuffer[rti]);
	// write out dummy verts for the unused polys (TODO: need a better way to do this, we are wasting memory and time)
	if (count>0)
	{
		u32 vertsPerQuad = API_QUADS_VERTS_PER_QUAD;
		memset(CPedDamageSetBase::sm_ActiveVertexBuffer[rti],0, count*vertsPerQuad*CPedDamageSetBase::sm_VertDecl->Stream0Size);
	}
	GRCDEVICE.EndVertices();
	CPedDamageSetBase::sm_Shader->UnBind();
	CPedDamageSetBase::sm_Shader->EndDraw();
}

int CPedDamageCompressedBlitDecoration::Render(float aspectRatio, bool horizontalFlip) const
{
	Vector4 uvCoords2 = GetUVCoords(); // need to be updated to VEC4

	Vector2 dir(uvCoords2.z, uvCoords2.w);
	Vector2 normal(-dir.y, dir.x);
	
	dir *= m_Scale.x;
	normal *= m_Scale.y;

	normal.x *= aspectRatio;
	dir.x *= aspectRatio;

	Vector3 frames(0.0f, 0.0f, 0.0f); 

	Color32 color;
	color.SetBytes(255, 255, 0, 0 );

	Vector2 uvCoords (uvCoords2.x,uvCoords2.y);

	return CPedDamageSetBase::DrawBlit(uvCoords, dir, normal, horizontalFlip, false, color.GetDeviceColor(), 0.0f);
}

bool CPedDamageBlitBlood::IsCloseTo(Vec2V_In uv) const
{
	const Vec2V dxdy = Float16Vec4Unpack(&m_UVCoords).GetXY() - uv;
	return IsLessThanAll(MagSquared(dxdy), ScalarV(s_CloseThreshold2)) != 0;
}

void CPedDamageBlitBlood::RenewHit(float preAge)
{
	m_ReBirthTime = Max(m_BirthTime, (TIME.GetElapsedTime() - preAge));
}

void  CPedDamageBlitBlood::Reset()
{
	m_Done=true;
}

bool CPedDamageBlitBlood::UpdateDoneStatus()
{
	if (!m_Done && m_BloodInfoPtr) // update done status and return if it.
	{
		if ((TIME.GetElapsedTime() - m_ReBirthTime) >= (m_BloodInfoPtr->m_BloodFadeStartTime + m_BloodInfoPtr->m_BloodFadeTime))
			m_Done = true;
	}
	return m_Done;
}

bool CPedDamageBlitBlood::CanLeaveScar()
{
	return (m_BloodInfoPtr && m_NeedsScar && m_BloodInfoPtr->m_ScarIndex>=0);
}

void CPedDamageBlitBlood::CreateScarFromWound(CPed * pPed, bool fadeIn, bool bScarOverride, float scarU, float scarV, float scarScale)
{
	// add a scar at this location
	Vector4 uvCoords = GetUVCoords(); 
	Vector2 center(uvCoords.x,uvCoords.y);
	float scale(GetScale());

	if (bScarOverride)
	{
		center.x = scarU;
		center.y = scarV;
		scale = scarScale;
	}

	if (CPedDamageDecalInfo * scarInfo = PEDDAMAGEMANAGER.GetDamageData().GetDamageDecalInfoByIndex(m_BloodInfoPtr->m_ScarIndex))
	{
		// TODO: need to calc rotation (just use the same as the blood)
		PEDDAMAGEMANAGER.AddPedScar(pPed, (ePedDamageZones)m_Zone, center, 0.0f, scale, scarInfo->m_Name.GetHash(), fadeIn, (int)m_FixedWoundFrameOrAnimSequence);
	}
	m_NeedsScar=false;
}

void CPedDamageBlitBlood::UpdateScar(CPed * pPed)
{
	if (m_NeedsScar && m_BloodInfoPtr)
	{		
		float scarTime = TIME.GetElapsedTime() - m_ReBirthTime;
		if (m_BloodInfoPtr->m_ScarStartTime>=0 && scarTime > m_BloodInfoPtr->m_ScarStartTime)
		{
			CreateScarFromWound(pPed, true);
		}
	}
}


void CPedDamageBlitBlood::FreezeFade(float time, float delta)
{
	if (m_BloodInfoPtr && !m_Done)
	{		
		if ((time - m_ReBirthTime) >= (m_BloodInfoPtr->m_BloodFadeStartTime))
			m_ReBirthTime -= delta; // keep it from fading while we're in  the cutscene
	}
}

void  CPedDamageBlitDecoration::Reset()
{
	m_Done=true;

	if (!m_EmblemDesc.IsValid())
	{
		if (m_DecorationTexture)
		{
			m_DecorationTexture = NULL;

			strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_TxdHash.GetHash()));
			if(txdId != -1 && m_RefAdded > 0)
			{
				CPedDamageSetBase::DelayedRemoveRefFromTXD(txdId.Get());

				CPEDDEBUG_DISPLAYF("CPedDamageBlitDecoration::Reset: tdxId %d (%s), TxtHash = %s, numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), m_TxtHash.TryGetCStr(), g_TxdStore.GetNumRefs(txdId));
			}
			m_RefAdded = 0;
			m_TxdHash.Clear();
			m_TxtHash.Clear();
		}
	}
	// we're dealing with a crew emblem texture
	else
	{
		CPEDDEBUG_DISPLAYF("CPedDamageBlitDecoration::Reset: remove crew emblem[%s][%s]",m_EmblemDesc.GetStr(),NETWORK_CLAN_INST.GetCrewEmblemMgr().GetEmblemName(m_EmblemDesc));

		if(m_RefAdded)
		{
			NETWORK_CLAN_INST.GetCrewEmblemMgr().ReleaseEmblem(m_EmblemDesc  ASSERT_ONLY(, "CPedDamageManager"));
		}

		m_EmblemDesc.Invalidate();

		m_DecorationTexture = NULL;

		m_RefAdded = 0;
		m_TxdHash.Clear();
		m_TxtHash.Clear();
	}

	m_pSourceInfo = NULL;
	m_PresetHash = 0U;
	m_CollectionHash = 0U;
	m_TintInfo.Reset();
	m_DelayedClanTextureLoad = 0;
}

bool CPedDamageBlitDecoration::UpdateDoneStatus()
{
	if (!m_Done && m_FadeOutStartTime!=0xffff )
	{	
		if ((TIME.GetElapsedTime() - m_BirthTime) >= ((u32)m_FadeOutStartTime + (u32)m_FadeOutTime)/64.0f)
			m_Done = true;
	}

	return m_Done;
}

// helper to make the 4 corner uvs for the blits (does simple 90 rotations)
inline Vector4 RotateUVs(float u, float v, float size, u8 rot)
{
	static Vector4 sRotations[4] = {Vector4(1,1,0,0), Vector4(0,1,1,0), Vector4(1,0,0,1), Vector4(0,0,1,1)};
	return Vector4(u, v, u, v) + sRotations[rot]*size;
}
  
// calc the two current two frames and the lerp value.
float CalcFloatFrame(const CPedDamageTexture & textureInfo, float age, float randomAnimIndex)
{
	return textureInfo.m_AnimationFrameCount * randomAnimIndex + Min(age*textureInfo.m_AnimationFPS, textureInfo.m_AnimationFrameCount-1.0f);
}

bool CPedDamageBlitBlood::RenderBegin(int bloodPass, int count) const
{
	if (bloodPass==0) // just set this once on the first pass.
		CPedDamageSet::SetBloodBlitShaderVars(GetSoakTextureInfo(), GetWoundTextureInfo(), GetSplatterTextureInfo());
	
	if (bloodPass == 0)
		grcStateBlock::SetBlendState(CPedDamageManager::GetBloodSoakBlendState());
	else
		grcStateBlock::SetBlendState(CPedDamageManager::GetBloodBlendState());
	
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);

	grcEffectTechnique technique = CPedDamageSetBase::sm_DrawBloodTechniques[bloodPass];

	const unsigned rti = g_RenderThreadIndex;

	if (AssertVerify(CPedDamageSetBase::sm_Shader->BeginDraw(grmShader::RMC_DRAW, false, technique) ))
	{
		CPedDamageSetBase::sm_Shader->Bind(0);
		{
			//set up fvf for blood
			GRCDEVICE.SetVertexDeclaration(CPedDamageSetBase::sm_VertDecl);  // will be soak/would/splatter eventually (i.e always the short version with 6 values)

			grcDrawMode drawMode = API_QUADS_SUPPORT ? drawQuads : drawTriStrip;
			u32 geomCount = (API_QUADS_VERTS_PER_QUAD*2) * count;

			CPedDamageSetBase::sm_ActiveVertexBuffer[rti] = (float*)GRCDEVICE.BeginVertices(drawMode, geomCount,CPedDamageSetBase::sm_VertDecl->Stream0Size);
		
			if (!Verifyf(CPedDamageSetBase::sm_ActiveVertexBuffer[rti], "GRCDEVICE.BeginVertices failed"))
			{
				CPedDamageSetBase::sm_Shader->UnBind();
				CPedDamageSetBase::sm_Shader->EndDraw();
				return false;
			}
		}
		return true;
	}
	else
	{
		CPedDamageSetBase::sm_ActiveVertexBuffer[rti] = NULL;
		return false;
	}
}

void CPedDamageBlitBlood::RenderEnd(int count) const
{
	const unsigned rti = g_RenderThreadIndex;
  	Assert(CPedDamageSetBase::sm_ActiveVertexBuffer[rti]);
 	// write out dummy verts for the unused polys (TODO: need a better way to do this, we are wasting memory and time)
	if (count>0)
	{
		u32 vertsPerQuad = API_QUADS_VERTS_PER_QUAD;
		memset(CPedDamageSetBase::sm_ActiveVertexBuffer[rti],0, count*vertsPerQuad*CPedDamageSetBase::sm_VertDecl->Stream0Size);
	}
	GRCDEVICE.EndVertices();
	CPedDamageSetBase::sm_Shader->UnBind();
	CPedDamageSetBase::sm_Shader->EndDraw();
}

// slow out that assume 0,1 input and approximates the sin with x^2
inline float FastSlowOut(float x ) { float omx = 1.0f-x; return 1.0f-omx*omx;}

int CPedDamageBlitBlood::Render(int bloodPass, float scaleValue, float aspectRatio, bool horizontalFlip, float currentTime ) const 
{
	Vector4 uvCoords2 = GetUVCoords(); 

	// from here to ::END is duplicated work in each pass, can we bring it outside the loop, or simplify it
	float woundAge = (currentTime - m_ReBirthTime);
	float soakAge = (currentTime - m_BirthTime);


#if GTA_REPLAY
	//At the start of a replay clip we add in all blood thats occurred before the clip. This needs doing over multiple frames.
	//When we rewind its possible to go back before this time and so get a negative age value, to prevent this clamp the values above 0.
	if( CReplayMgr::IsEditModeActive() )
	{
		woundAge = Max(0.0f, woundAge);
		soakAge = Max(0.0f, soakAge);
	}
#endif //GTA_REPLAY


	float fadeAge = woundAge - m_BloodInfoPtr->m_BloodFadeStartTime;  // use wound age, since it might have been refreshed.
	float fadeAlpha = FastSlowOut(1.0f-Clamp(fadeAge/m_BloodInfoPtr->m_BloodFadeTime,0.0f,1.0f));

	float alpha, intensity;
	float scaleAlpha;
	float radius;
	const CPedDamageTexture* texture=NULL;
	float frameAge;
	float fixedFrameOrAnimSequence;

	switch(bloodPass)
	{
		case 0:		// Soak
		{
			texture = (m_UseSoakTextureGravity ? &m_BloodInfoPtr->m_SoakTextureGravity : &m_BloodInfoPtr->m_SoakTexture);
			if (texture == NULL || texture->m_pTexture==NULL)  // bail early if they don't want this part.
				return 0;

			alpha = FastSlowOut(Min(1.0f,soakAge/m_BloodInfoPtr->m_SoakDryingTime));
			intensity = Lerp(alpha,m_BloodInfoPtr->m_SoakIntensityWet,m_BloodInfoPtr->m_SoakIntensityDry);
			float woundScale = GetScale();			
			float soakScale = Lerp(woundScale, m_BloodInfoPtr->m_SoakScaleMin, 1.0f); // lerp between min and 1.0 for soak, so it scales with the wound

			float soakScaleStartSize = soakScale * ((m_UseSoakTextureGravity) ? m_BloodInfoPtr->m_SoakStartSizeGravity : m_BloodInfoPtr->m_SoakStartSize);
			float soakScaleEndSize   = soakScale * ((m_UseSoakTextureGravity) ? m_BloodInfoPtr->m_SoakEndSizeGravity : m_BloodInfoPtr->m_SoakEndSize);
			
			if (m_LimitScale)
			{
				// we can go as big as the splatter (since that's what we used as a push away from the seems
				const CPedDamageTexture* splatterTexture = &m_BloodInfoPtr->m_SplatterTexture;
				if (splatterTexture == NULL || splatterTexture->m_pTexture==NULL) // if there is no splatter texture, use the wound size.
					soakScaleEndSize = Lerp(woundScale, m_BloodInfoPtr->m_WoundMinSize, m_BloodInfoPtr->m_WoundMaxSize);
				else
					soakScaleEndSize = Lerp(woundScale, m_BloodInfoPtr->m_SplatterMinSize, m_BloodInfoPtr->m_SplatterMaxSize);
				
				soakScaleEndSize = Max(soakScaleStartSize, soakScaleEndSize);
			}
			
			if (!m_ReduceBleeding)
				scaleAlpha = FastSlowOut(Min(1.0f,soakAge/m_BloodInfoPtr->m_SoakScaleTime));
			else
				scaleAlpha = 0.0f;

			radius = Lerp(scaleAlpha, soakScaleStartSize, soakScaleEndSize)/100.0f;
			frameAge = soakAge;
			fixedFrameOrAnimSequence = (float)m_FixedSoakFrameOrAnimSequence;
			break;
		}
		case 1: // splatter
			texture = &m_BloodInfoPtr->m_SplatterTexture;
			if (texture == NULL || texture->m_pTexture==NULL)
				return 0;

			alpha = FastSlowOut(Min(1.0f,woundAge/m_BloodInfoPtr->m_SplatterDryingTime));
			intensity = Lerp(alpha,m_BloodInfoPtr->m_SplatterIntensityWet,m_BloodInfoPtr->m_SplatterIntensityDry);
			scaleAlpha = GetScale(); // 0 to 1 values for wound/splatter scale
			radius= Lerp(scaleAlpha, m_BloodInfoPtr->m_SplatterMinSize, m_BloodInfoPtr->m_SplatterMaxSize)/100.0f;
			frameAge = soakAge; // so it does not pop
			fixedFrameOrAnimSequence = (float)m_FixedSplatterFrameOrAnimSequence;
			break;

		case 2: // wound
			texture = &m_BloodInfoPtr->m_WoundTexture;
			if (texture == NULL || texture->m_pTexture==NULL)
				return 0;

			alpha = FastSlowOut(Min(1.0f,woundAge/m_BloodInfoPtr->m_WoundDryingTime));
			intensity = Lerp(alpha,m_BloodInfoPtr->m_WoundIntensityWet,m_BloodInfoPtr->m_WoundIntensityDry);
			scaleAlpha = GetScale(); // 0 to 1 values for wound/splatter scale
			radius = Lerp(scaleAlpha, m_BloodInfoPtr->m_WoundMinSize, m_BloodInfoPtr->m_WoundMaxSize)/100.0f;
			frameAge = woundAge;
			fixedFrameOrAnimSequence = (float)m_FixedWoundFrameOrAnimSequence;
			break;

		default:
			return 0;
	}

	float frame = texture->m_FrameMin + ((texture->m_AnimationFPS==0.0f) ? fixedFrameOrAnimSequence : CalcFloatFrame( *texture, frameAge, fixedFrameOrAnimSequence));

	Color32 color;
	color.SetBytes(u8(255*intensity), u8(255*fadeAlpha), 0, 0 );  // Z  will hold UV index,  W unused.

	// if we are torso/head, because torso/head wounds are exaggerated
	if (m_Zone<=kDamageZoneHead )
		scaleValue *= s_TorsoSizeBoost;
	else if (m_FromWeapon)
		scaleValue *= s_LimbSizeBoost;  // only apply if from a weapon so we don't mess with all the script presets.

	Vector2 dir(uvCoords2.z,uvCoords2.w); 
	dir *= (radius*scaleValue/2.0f);

	Vector2 normal(-dir.y*aspectRatio, dir.x);   // TODO aspect ratio should be build into the projection... (then we don't really need the normal, direction is enough
	dir.x *= aspectRatio;

	Vector2 uvCoords(uvCoords2.x,uvCoords2.y);
	
	bool flipU = (m_FlipUVFlags&0x1)!=0 && texture->m_AllowUFlip;  // if this texture can be flipped and the wound is marked to flip, let's flip
	bool flipV = (m_FlipUVFlags&0x2)!=0 && texture->m_AllowVFlip;

	float centerOffSetX = (flipU) ? texture->m_CenterOffset.x : -texture->m_CenterOffset.x;
	float centerOffSetY = (flipV) ? texture->m_CenterOffset.y : -texture->m_CenterOffset.y;

	// TODO: we only need to calc offset for textures with offsets (usually only soak), do we need a flag?
	uvCoords += dir*centerOffSetX + normal*centerOffSetY;
	
	return CPedDamageSet::DrawBlit(uvCoords, dir, normal, horizontalFlip^flipU, flipV, color.GetDeviceColor(), frame);
}

#if __BANK
void CPedDamageBlitBlood::DumpScriptParams(bool dumpTitle) const
{
	if (dumpTitle)
		Displayf("APPLY_PED_BLOOD_SPECIFIC(pedIndex,          Zone,     u,     v, rotation, scale, forceFrame, preAge, bloodName)");

	if (m_BloodInfoPtr)
	{
		Vector4 uvCoords = GetUVCoords();

		Vector2 dir(uvCoords.z-uvCoords.x,uvCoords.w-uvCoords.y);
		float rotation = 0.0f;
		float mag = dir.Mag();
		if (mag>0.001)
		{
			dir /= mag;
			rotation = atan2(dir.y,dir.x)*RtoD;
			if (rotation<0) 
				rotation = 360+rotation;
		}

		float scale = GetScale();
		int frame = (GetSplatterTextureInfo().m_pTexture!=NULL) ? m_FixedSplatterFrameOrAnimSequence : m_FixedWoundFrameOrAnimSequence;

		Printf("APPLY_PED_BLOOD_SPECIFIC(pedIndex, ");
		Printf("%13s, ", s_ScriptZoneEmumNames[GetZone()]);
		Printf("%5.3f, ", uvCoords.x);
		Printf("%5.3f, ", uvCoords.y);
		Printf("%8.3f, ", rotation); 
		Printf("%5.3f, ", scale);
		Printf("%10d, ", frame); 
		Printf("%6.2f, ", 0.0f);
		Printf("\"%s\") ", m_BloodInfoPtr->m_Name.GetCStr());
		Printf("\n");
	}
}

bool CPedDamageBlitBlood::DumpDamagePackEntry(int zone, bool first) const
{
	if (m_BloodInfoPtr && GetZone()==zone)
	{
		Vector4 uvCoords = GetUVCoords();

		Vector2 dir(uvCoords.z-uvCoords.x,uvCoords.w-uvCoords.y);
		float rotation = 0.0f;
		float mag = dir.Mag();
		if (mag>0.001)
		{
			dir /= mag;
			rotation = atan2(dir.y,dir.x)*RtoD;
			if (rotation<0) 
				rotation = 360+rotation;
		}

		float scale = GetScale();
		int frame = (GetSplatterTextureInfo().m_pTexture!=NULL) ? m_FixedSplatterFrameOrAnimSequence : m_FixedWoundFrameOrAnimSequence;

		if (first)
			Displayf("\n        <!-- %s -->\n",s_ZoneCommonNames[zone]);
		Displayf("        <Item>");
		Displayf("          <DamageName>%s</DamageName>", m_BloodInfoPtr->m_Name.GetCStr());
		Displayf("          <Zone>%s</Zone>", s_ZoneEmumNames[zone]);
		Displayf("          <uvPos x=\"%f\" y=\"%f\" />", uvCoords.x, uvCoords.y);
		Displayf("          <Rotation value=\"%f\" />", rotation); 
		Displayf("          <Scale value=\"%f\" />", scale);
		Displayf("          <ForcedFrame value=\"%d\" />", frame); 
		Displayf("        </Item>");
		first = false;
	}
	return first;
}
#endif

void CPedDamageBlitBlood::DumpDecorations() const
{
#if !__NO_OUTPUT
	if (m_BloodInfoPtr)
	{
		int zone = GetZone();

		Vector4 uvCoords = GetUVCoords();

		Vector2 dir(uvCoords.z-uvCoords.x,uvCoords.w-uvCoords.y);
		float rotation = 0.0f;
		float mag = dir.Mag();
		if (mag>0.001)
		{
			dir /= mag;
			rotation = atan2(dir.y,dir.x)*RtoD;
			if (rotation<0) 
				rotation = 360+rotation;
		}

		float scale = GetScale();
		int frame = (GetSplatterTextureInfo().m_pTexture!=NULL) ? m_FixedSplatterFrameOrAnimSequence : m_FixedWoundFrameOrAnimSequence;

		Displayf("     blood[%s] zone[%d][%s] uvCoords[%f %f %f %f] rotation[%f] scale[%f] forcedframe[%d]"
#if __BANK
			,m_BloodInfoPtr->m_Name.GetCStr()
			,zone
			,zone <= kDamageZoneNumZones ? s_ScriptZoneEmumNames[zone] : ""
#else
			,""
			,zone
			,""
#endif
			,uvCoords.x,uvCoords.y,uvCoords.z,uvCoords.w 
			,rotation
			,scale
			,frame);
	}
#endif
}

bool CPedDamageBlitDecoration::RenderBegin(int pass, int count, s32 cachedTintPalSelector1, s32 cachedTintPalSelector2) const
{

	grcEffectTechnique technique;
	bool bIsTattoo = IsOnSkinOnly() && (m_pSourceInfo==NULL); 

	if (pass==0)
	{
		const grcTexture* pPalTexture = NULL;
		Vector3 palSelector;
		GetTintInfoRenderData(m_TintInfo, pPalTexture, palSelector, cachedTintPalSelector1, cachedTintPalSelector2);
		CPedDamageSet::SetDecorationBlitShaderVars (m_DecorationTexture, pPalTexture, palSelector, GetTextureInfo(), GetBumpTextureInfo(), bIsTattoo);
		grcStateBlock::SetBlendState(CPedDamageManager::GetBumpBlendState());
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		technique = CPedDamageSetBase::sm_DrawSkinBumpTechnique; 
	}
	else
	{
		const grcTexture* pPalTexture = NULL;
		Vector3 palSelector;
		GetTintInfoRenderData(m_TintInfo, pPalTexture, palSelector, cachedTintPalSelector1, cachedTintPalSelector2);
		CPedDamageSet::SetDecorationBlitShaderVars (m_DecorationTexture, pPalTexture, palSelector, GetTextureInfo(), NULL, bIsTattoo);
		grcStateBlock::SetBlendState(CPedDamageManager::GetDecorationBlendState(pass-1),~0U,~0ULL);
		grcStateBlock::SetDepthStencilState(CPedDamageManager::GetSetStencilState(), IsOnSkinOnly()? 0x1:0x0);
		technique = CPedDamageSetBase::sm_DrawDecorationTechnique[pass-1];
	}

	const unsigned rti = g_RenderThreadIndex;

	if (AssertVerify(CPedDamageSetBase::sm_Shader->BeginDraw(grmShader::RMC_DRAW, false, technique) ))
	{
		CPedDamageSetBase::sm_Shader->Bind(0);
		{
			// set up fvf for decorations
			GRCDEVICE.SetVertexDeclaration(CPedDamageSetBase::sm_VertDecl);  
			// allocate enough room for 2 quads for each blit (in case it hits an edge)
			grcDrawMode drawMode = API_QUADS_SUPPORT ? drawQuads : drawTriStrip;
			u32 geomCount = (API_QUADS_VERTS_PER_QUAD*2) * count;

			CPedDamageSetBase::sm_ActiveVertexBuffer[rti] = (float*)GRCDEVICE.BeginVertices(drawMode, geomCount,CPedDamageSetBase::sm_VertDecl->Stream0Size);
			if (!Verifyf(CPedDamageSetBase::sm_ActiveVertexBuffer[rti], "GRCDEVICE.BeginVertices failed"))
			{
				CPedDamageSetBase::sm_Shader->UnBind();
				CPedDamageSetBase::sm_Shader->EndDraw();
				return false;
			}
		}
		return true;
	}
	else
	{
		CPedDamageSetBase::sm_ActiveVertexBuffer[rti] = NULL;
		return false;
	}
}

void CPedDamageBlitDecoration::RenderEnd(int count) const
{
	const unsigned rti = g_RenderThreadIndex;
	Assert(CPedDamageSetBase::sm_ActiveVertexBuffer[rti]);
	// write out dummy verts for the unused polys (TODO: need a better way to do this, we are wasting memory and time)
	if (count>0)
	{
		u32 vertsPerQuad = API_QUADS_VERTS_PER_QUAD;
		memset(CPedDamageSetBase::sm_ActiveVertexBuffer[rti],0, count*vertsPerQuad*CPedDamageSetBase::sm_VertDecl->Stream0Size);
	}
	GRCDEVICE.EndVertices();
	CPedDamageSetBase::sm_Shader->UnBind();
	CPedDamageSetBase::sm_Shader->EndDraw();
}



// Draw the decorations into the target
int CPedDamageBlitDecoration::Render(float aspectRatio, bool horizontalFlip, float currentTime) const
{

#if __BANK
	if (m_TintInfo.bValid && m_TintInfo.bReady == false)
	{
		PEDDEBUG_DISPLAYF("CPedDamageBlitDecoration::Render: using cached values for tint indices");
	}
#endif

	Vector4 uvCoords2 = GetUVCoords(); // need to be updated to VEC4
		
	Vector2 uvCoords(uvCoords2.x,uvCoords2.y);
	Vector2 dir(uvCoords2.z, uvCoords2.w);
	Vector2 normal(-dir.y, dir.x);
		
	dir *= m_Scale.x;
	normal *= m_Scale.y;

	float fadeAlpha = GetAlphaIntensity();
	float age = currentTime- m_BirthTime;
		
	if (age > 0.0f)
	{
		if (m_FadeInTime != 0x0 ) // if there is no fade in time, skip this or you get divide by 0 the first frame.
		{
			float fadeTime = m_FadeInTime/10.0f;
			fadeAlpha *= FastSlowOut(Min(1.0f, age/fadeTime)); 
		}

		if (m_FadeOutStartTime!=0xffff )  /// 0xffff mean no fade out
		{
			float fadeAge = age - (m_FadeOutStartTime/64.0f);
			float fadeTime = (m_FadeOutTime==0) ? fadeAge : m_FadeOutTime/64.0f; // avoid divide by 0 if they don't have a fade out length.
			fadeAlpha *= FastSlowOut(1.0f-Clamp(fadeAge/fadeTime,0.0f,1.0f));
		}
	}

	Color32 color;

#if __BANK
	if (CPedDamageManager::sm_EnableBlitAlphaOverride)
	{
		fadeAlpha = CPedDamageManager::sm_BlitAlphaOverride;
	}
#endif
	color.SetBytes(255, u8(255*Min(1.0f,fadeAlpha)), 0, 0 );  // Z  will hold UV index,  W unused.

	normal.x *= aspectRatio;
	dir.x *= aspectRatio;

	const CPedDamageTexture* pTextureInfo = GetTextureInfo();
	float frame = 0.0f;
	bool flipU = false;
	bool flipV = false;
	if (pTextureInfo)
	{
		frame = pTextureInfo->m_FrameMin + ((pTextureInfo->m_AnimationFPS!=0.0f) ? CalcFloatFrame( *pTextureInfo, age, m_FixedFrameOrAnimSequence) : m_FixedFrameOrAnimSequence);
		flipU = (m_FlipUVFlags&0x1)!=0 && pTextureInfo->m_AllowUFlip;  // if this texture can be flipped and the the specific decoration is marked to flip, let's flip
		flipV = (m_FlipUVFlags&0x2)!=0 && pTextureInfo->m_AllowVFlip;
	}

	// should we add texture offset support here?
	return CPedDamageSet::DrawBlit(uvCoords, dir, normal, horizontalFlip^flipU, flipV, color.GetDeviceColor(), frame);
}

#if __BANK
void CPedDamageBlitDecoration::DumpScriptParams(bool dumpTitle) const
{
	if (dumpTitle)
		Displayf("APPLY_PED_DAMAGE_DECAL(pedIndex,          Zone,     u,     v, rotation, scale, alpha, forceFrame, fadeIn, damageDecalName)");

	if (m_pSourceInfo)
	{
		Vector4 uvCoords = GetUVCoords();

		Vector2 dir(uvCoords.z,uvCoords.w);
		float rotation = 0.0f;
		float mag = dir.Mag();
		if (mag>0.001)
		{
			dir /= mag;
			rotation = atan2(dir.y,dir.x)*RtoD;
			if (rotation<0) 
				rotation = 360+rotation;
		}
		
		float scale = GetScale().x*100;
		if ((m_pSourceInfo->m_MaxSize-m_pSourceInfo->m_MinSize)>0.0f)
			scale = (scale - m_pSourceInfo->m_MinSize)/(m_pSourceInfo->m_MaxSize-m_pSourceInfo->m_MinSize); 
		else
			scale = 1.0f;

		int frame = GetFixedFrame();
		if (GetTextureInfo() && GetTextureInfo()->m_AnimationFPS==0.0f)
			frame -= (int)GetTextureInfo()->m_FrameMin;

		Printf("APPLY_PED_DAMAGE_DECAL(pedIndex, ");
		Printf("%13s, ", s_ScriptZoneEmumNames[GetZone()]);
		Printf("%5.3f, ", uvCoords.x);
		Printf("%5.3f, ", uvCoords.y);
		Printf("%8.3f, ", rotation); 
		Printf("%5.3f, ", Clamp(scale,0.0f,1.0f));
		Printf("%5.3f, ", GetAlphaIntensity());
		Printf("%10d, ", frame); 
		Printf("%6s, ", m_FadeInTime==0x7ff?"FALSE":"TRUE"); 
		Printf("\"%s\") ", m_pSourceInfo->m_Name.GetCStr());
		Printf("\n");
	}
}


bool  CPedDamageBlitDecoration::DumpDamagePackEntry(int zone, bool first) const
{
	if (m_pSourceInfo && GetZone()==zone)
	{
		Vector4 uvCoords = GetUVCoords();

		Vector2 dir(uvCoords.z,uvCoords.w);
		float rotation = 0.0f;
		float mag = dir.Mag();
		if (mag>0.001)
		{
			dir /= mag;
			rotation = atan2(dir.y,dir.x)*RtoD;
			if (rotation<0) 
				rotation = 360+rotation;
		}

		float scale = GetScale().x*100;
		if ((m_pSourceInfo->m_MaxSize-m_pSourceInfo->m_MinSize)>0.0f)
			scale = (scale - m_pSourceInfo->m_MinSize)/(m_pSourceInfo->m_MaxSize-m_pSourceInfo->m_MinSize); 
		else
			scale = 1.0f;

		int frame = GetFixedFrame();
		if (GetTextureInfo() && GetTextureInfo()->m_AnimationFPS==0.0f)
			frame -= (int)GetTextureInfo()->m_FrameMin;
		
		if (first)
			Displayf("\n        <!-- %s -->\n",s_ZoneCommonNames[zone]);
		Displayf("        <Item>");
		Displayf("          <DamageName>%s</DamageName>", m_pSourceInfo->m_Name.GetCStr());
		Displayf("          <Zone>%s</Zone>", s_ZoneEmumNames[zone]);
		Displayf("          <uvPos x=\"%f\" y=\"%f\" />", uvCoords.x, uvCoords.y);
		Displayf("          <Rotation value=\"%f\" />", rotation); 
		Displayf("          <Scale value=\"%f\" />", Clamp(scale,0.0f,1.0f));
		Displayf("          <ForcedFrame value=\"%d\" />", frame); 
		Displayf("        </Item>");
	
		first = false;
	}
	return first;
}
#endif

// local helpers to get cumulative transforms of a bone
void GetCumulativeDefaultTransform(Mat34V_InOut transformMtx, const crSkeletonData &skelData, const int startingBoneIndex)
{
	int curBoneIndex = startingBoneIndex;
	transformMtx = skelData.GetDefaultTransform(curBoneIndex);	

	while (curBoneIndex != 0)	
	{
		curBoneIndex = skelData.GetParentIndex(curBoneIndex);
		Transform(transformMtx, skelData.GetDefaultTransform(curBoneIndex), transformMtx );
	}
}


Vec3V_Out GetCumulativeDefaultTranslate(const crSkeletonData &skelData, const int startingBoneIndex)
{
	int curBoneIndex = startingBoneIndex;
	Vec3V outVec(skelData.GetDefaultTransform(curBoneIndex).GetCol3ConstRef());	

	while (curBoneIndex != 0)	
	{
		curBoneIndex = skelData.GetParentIndex(curBoneIndex);
		outVec = Transform(skelData.GetDefaultTransform(curBoneIndex), outVec);
	}
	
	return outVec;
}

void CPedDamageCylinder::SetFromSkelData(const crSkeletonData &sd, s32 baseBone1, s32 baseBone2, s32 topBone, float radius, const Vector3 & baseOffset, const Vector3 & topOffset, float rotation, int zone, float push, float lengthScale)
{

	// TODO: this old code still uses Vector3's...
	Vector3 basePos1 = VEC3V_TO_VECTOR3(GetCumulativeDefaultTranslate(sd, baseBone1));
	Vector3 basePos2 = VEC3V_TO_VECTOR3(GetCumulativeDefaultTranslate(sd, baseBone2));
	Vector3 topPos   = VEC3V_TO_VECTOR3(GetCumulativeDefaultTranslate(sd, topBone));

	Vector3 basePos((basePos1+basePos2)/2.0f);	// want the average of these two joints

	basePos += baseOffset;
	topPos  += topOffset;

	Vector3 up = topPos - basePos;

	float length = up.Mag();
	up.Normalize();
	length *= lengthScale;

	// some scale and offset adjustments along the center axis to make the UV's line up with the damage better
	basePos += push * length *  up;  // move the uv's down a little along the axis.
	length -= push * length;

	Set(basePos, up, length, radius, rotation, zone);	  // still don't have radius....
}

void CPedDamageCylinder::SetFromSkelData(const crSkeletonData &sd, s32 baseBone1, s32 baseBone2, s32 topBone, int zone, const CPedDamageCylinderInfo &cylInfo)
{
	SetFromSkelData(sd, baseBone1, baseBone2, topBone, cylInfo.radius, cylInfo.baseOffset, cylInfo.topOffset, cylInfo.rotation, zone, cylInfo.push, cylInfo.lengthScale);
	m_PushAreaArray = &cylInfo.pushAreas;
}


#if __BANK

void CPedDamageBlitBlood::ReloadTextures()
{
	// TODO: in GTAV need to reset the dictionary?
	Displayf("CPedDamageBlitBlood::ReloadTextures() is not yet implemented");
}

void CPedDamageCylinder::DebugDraw(const Matrix34 &playerMtx)
{
	Matrix34 mtx(M34_IDENTITY);

	// set up matrix so it draw the origin centered cylinder in the correct location
	mtx.MakeRotateTo(YAXIS, m_Up);
	
	// need to offset the base by 1/2 length along the up vector...
	mtx.d = m_Base + mtx.b*.5*m_Height;

	// and adjust it to be local to the ped
	mtx.Dot(playerMtx);

	grcDrawCylinder(m_Height, m_Radius, mtx, 8, true);
}

void CPedDamageSetBase::DebugDraw()
{
	if (m_pPed)
	{
		for (int i=0;i<kDamageZoneNumBloodZones;i++)
		{
			m_Zones[i].DebugDraw(MAT34V_TO_MATRIX34(m_pPed->GetMatrix()));
		}
	}
}

void CPedDamageSetBase::AddWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Blood Tuning", false );
		bank.PushGroup("Stab", false );
			bank.AddSlider("Stab Width",		&s_StabWidth, 0.0f, 1.0f, 0.001f);	
			bank.AddSlider("Stab Soak Scale",	&s_StabSoakScale, 0.0f, 8.0f, 0.1f);
		bank.PopGroup();

		bank.AddSlider("Torso Scale",		&s_TorsoSizeBoost, 0.0f, 4.0f, 0.01f);
		bank.AddSlider("Limb Scale",		&s_LimbSizeBoost, 0.0f, 4.0f, 0.01f);
		
		bank.AddSlider("Combine Threshold (squared)", &s_CloseThreshold2, 0.0f, 1.0f, 0.001f);
		bank.AddToggle("Enable Blood Debug Draw", &s_DebugBloodHits);
		bank.AddButton("Reload Textures",datCallback(CFA(CPedDamageBlitBlood::ReloadTextures)));
	bank.PopGroup();
}

#endif


//
// drawlist functions 
//
class dlCmdRenderPedDamageSet : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_PedDamageSetRender,
	};

	dlCmdRenderPedDamageSet(CPedDamageSet * damageSet, bool BANK_ONLY(debug), bool bMirrorMode );
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdRenderPedDamageSet &) cmd).Execute(); }
	void Execute();

private:			
	atUserArray<CPedDamageBlitBlood> m_BloodArray;
	atUserArray<CPedDamageBlitDecoration> m_DecorationArray;
	grcRenderTarget * m_ActiveBloodDamageTarget;
	grcRenderTarget * m_ActiveDecorationTarget;
	atRangeArray<CPedDamageCylinder, kDamageZoneNumZones> m_Zones;
	bool m_HasMedals;
	bool m_HasDress;
	bool m_MirrorMode;
	Color32 m_ClearColor;
	float m_TimeStamp;
	s32 m_cachedTintPaletteSelector1;
	s32 m_cachedTintPaletteSelector2;
	BANK_ONLY(bool m_DebugFlag;);
};


dlCmdRenderPedDamageSet::dlCmdRenderPedDamageSet( CPedDamageSet * damageSet, bool BANK_ONLY(debug), bool bMirrorMode )
{
	if (bMirrorMode)
	{
		m_ActiveBloodDamageTarget = CPedDamageSetBase::GetMirrorDecorationTarget();    // use the same target (it's a double wide)
		m_ActiveDecorationTarget  = CPedDamageSetBase::GetMirrorDecorationTarget();
	}
	else
	{
		m_ActiveBloodDamageTarget = damageSet->GetActiveBloodDamageTarget();
		m_ActiveDecorationTarget  = damageSet->NeedsDecorationTarget() ? damageSet->GetActiveDecorationTarget() : NULL; // if using compressed damage, we don't need the decoration target (still might need scars though)
	}

	m_cachedTintPaletteSelector1 = damageSet->GetLastValidTintPaletteSelector1();
	m_cachedTintPaletteSelector2 = damageSet->GetLastValidTintPaletteSelector2();
	m_Zones = damageSet->GetZoneArray(); 
	m_HasMedals = damageSet->HasMedals();
	m_HasDress = damageSet->HasDress();
	m_ClearColor = damageSet->GetClearColor();
	m_MirrorMode = bMirrorMode;
	m_TimeStamp = TIME.GetElapsedTime();

	BANK_ONLY(m_DebugFlag = debug;);

	atFixedArray<CPedDamageBlitBlood, CPedDamageSetBase::kMaxBloodDamageBlits> & damageSetBloodList = damageSet->GetPedBloodDamageList();
	u16 bloodArrayCount = (u16)damageSetBloodList.GetCount();
	if (bloodArrayCount>0)
	{
		u16 bloodArraySize =  (u16)(sizeof(CPedDamageBlitBlood)*bloodArrayCount);
		CPedDamageBlitBlood* bloodListMem = gDCBuffer->AllocateObjectFromPagedMemory<CPedDamageBlitBlood>(DPT_LIFETIME_RENDER_THREAD, bloodArraySize);
		if (Verifyf(bloodListMem,"AllocateObjectFromPagedMemory() failed"))
		{
			m_BloodArray.Assume(bloodListMem,bloodArrayCount);

			// copy entries over (skipping done entries)
			for (int i = 0; i<bloodArrayCount; i++)
			{
				if(!damageSetBloodList[i].UpdateDoneStatus())
				{
					m_BloodArray.Append() = damageSetBloodList[i];
				}
			}
			if (m_BloodArray.GetCount()==0) // reset the count of the array, since everything in it is done. This will save generating blank render targets next frame
			{
				PEDDEBUG_DISPLAYF("dlCmdRenderPedDamageSet: ped=0x%p(%s), All Blood is done, reseting the list", damageSet ? damageSet->GetPed() : 0, damageSet ? GetPedName(damageSet->GetPed()) : "");
				damageSetBloodList.Reset();
			}
		}
	}

	atVector<CPedDamageBlitDecoration>& damageSetDecorationList = damageSet->GetDecorationList();
	u16 decorationArrayCount = (u16)damageSetDecorationList.GetCount();
	if (decorationArrayCount>256)  // the render sort can't do more than 256, so might as well limit it here
		decorationArrayCount=256;

	if (decorationArrayCount>0)
	{
		u16 decorationArraySize = (u16)(sizeof(CPedDamageBlitDecoration)*decorationArrayCount);
		CPedDamageBlitDecoration* decorationListMem = gDCBuffer->AllocateObjectFromPagedMemory<CPedDamageBlitDecoration>(DPT_LIFETIME_RENDER_THREAD, decorationArraySize);
		
		if (Verifyf(decorationListMem,"AllocateObjectFromPagedMemory() failed"))
		{
			m_DecorationArray.Assume(decorationListMem,decorationArrayCount);
			sysMemSet(decorationListMem, 0, decorationArrayCount * sizeof(CPedDamageBlitDecoration));

			// copy entries over (skipping done entries)
			int doneCount = 0;		
			for (int i = 0; i<damageSetDecorationList.GetCount(); i++)
			{
				if(!damageSetDecorationList[i].UpdateDoneStatus() && (m_ActiveDecorationTarget || damageSetDecorationList[i].HasBumpMap())) // we can skip decoration with no bump if we don't have a decoration target
				{
					if (damageSetDecorationList[i].GetDecorationTexture()!=NULL && m_DecorationArray.GetCount()<=decorationArrayCount)
					{
						m_DecorationArray.Append().Copy(damageSetDecorationList[i]);
					}
				}
				else
				{
					doneCount++;
				}
			}
		 
			if (doneCount==damageSetDecorationList.GetCount()) // all said they were done, so we can reset the list (some where not added to the list, but they might be waiting on a load)
			{
				PEDDEBUG_DISPLAYF("dlCmdRenderPedDamageSet: ped=0x%p(%s), All decorations are done, reseting the list", damageSet ? damageSet->GetPed() : 0, damageSet ? GetPedName(damageSet->GetPed()) : "");
				// don't just reset the list to 0 count, need to release the textures, and adjust Tattoo and misc counts, so use the ClearDecorations function
				damageSet->ClearDecorations();
			}
		}
	}
}

void dlCmdRenderPedDamageSet::Execute()
{
	PIX_AUTO_TAG(0,"PedDamageSet");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CPedDamageSet::Render( m_BloodArray, m_DecorationArray, m_ActiveBloodDamageTarget, m_ActiveDecorationTarget, m_Zones, m_HasMedals, m_HasDress, m_ClearColor, m_MirrorMode, m_TimeStamp, m_cachedTintPaletteSelector1, m_cachedTintPaletteSelector2);

	for(int index = 0; index<m_DecorationArray.GetCount(); index++)
	{
		m_DecorationArray[index].ResetDecorationTexture();
	}
#if __BANK	
	if (m_DebugFlag)
		CPedDamageManager::SaveDebugTargets(m_ActiveBloodDamageTarget, m_ActiveDecorationTarget);
#endif
}


class dlCmdRenderPedCompressedDamageSet : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_PedCompressedDamageSetRender,
	};

	dlCmdRenderPedCompressedDamageSet(CCompressedPedDamageSet * damageSet);

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdRenderPedCompressedDamageSet &) cmd).Execute(); }
	void Execute();

private:			
	atUserArray<CPedDamageCompressedBlitDecoration> m_DecorationArray;
	grcRenderTarget * m_ActiveDecorationTarget;
	u32 m_RequestedBlitIndex;
	atRangeArray<CPedDamageCylinder, kDamageZoneNumZones> m_Zones;
};


dlCmdRenderPedCompressedDamageSet::dlCmdRenderPedCompressedDamageSet(CCompressedPedDamageSet * damageSet)
{
	m_RequestedBlitIndex = PEDDECORATIONBUILDER.GetRequestedBlitIndex();
	m_ActiveDecorationTarget = damageSet->GetBlitTarget();
	m_Zones = damageSet->GetZoneArray(); 

	atVector<CPedDamageCompressedBlitDecoration>& damageSetDecorationList = damageSet->GetDecorationList();
	u16 decorationArrayCount = (u16)damageSetDecorationList.GetCount();
	
	CPEDDEBUG_DISPLAYF("dlCmdRenderPedCompressedDamageSet::dlCmdRenderPedCompressedDamageSet() decorationArrayCount = %d, m_RequestedBlitIndex = %u", decorationArrayCount, m_RequestedBlitIndex);
	
	if (decorationArrayCount>0)
	{
		u16 decorationArraySize = (u16)(sizeof(CPedDamageCompressedBlitDecoration)*decorationArrayCount);
		CPedDamageCompressedBlitDecoration* decorationListMem = gDCBuffer->AllocateObjectFromPagedMemory<CPedDamageCompressedBlitDecoration>(DPT_LIFETIME_RENDER_THREAD, decorationArraySize);
		Assert(decorationListMem);
		m_DecorationArray.Assume(decorationListMem,decorationArrayCount);

		// copy entries over (skipping done entries)
		for (int i = 0; i<decorationArrayCount; i++)
		{
			if(!damageSetDecorationList[i].IsDone())
			{
				CPEDDEBUG_DISPLAYF("dlCmdRenderPedCompressedDamageSet::dlCmdRenderPedCompressedDamageSet !damageSetDecorationList[%d].IsDone --> Append",i);
				m_DecorationArray.Append() = damageSetDecorationList[i];	
			}	
			else
			{
				CPEDDEBUG_DISPLAYF("dlCmdRenderPedCompressedDamageSet::dlCmdRenderPedCompressedDamageSet damageSetDecorationList[%d].IsDone --> DO NOTHING",i);
			}
		}
		
		BANK_ONLY(if (g_DebugLockCompressionInBlittingStage == false))
		{
 		if (m_DecorationArray.GetCount()==0) // reset the count of the array, since everything in it is done. This will save generating blank render targets next frame
 			damageSetDecorationList.Resize(0);
		}
	}
}


void dlCmdRenderPedCompressedDamageSet::Execute()
{
	PIX_AUTO_TAG(0,"CompressedPedDamageSet");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CPEDDEBUG_DISPLAYF("dlCmdRenderPedCompressedDamageSet::Execute m_RequestedBlitIndex = %u, GetRenderedBlitIndex = %u",m_RequestedBlitIndex, PEDDECORATIONBUILDER.GetRenderedBlitIndex());

	if ((m_RequestedBlitIndex != PEDDECORATIONBUILDER.GetRenderedBlitIndex() // We only do this once, otherwise late render overwrite our untiling
#if PED_DECORATIONS_MULTI_GPU_SUPPORT
		&& !PEDDECORATIONBUILDER.IsGpuDone()
#endif
		)|| BANK_SWITCH(g_DebugLockCompressionInBlittingStage, false))
	{
		CPEDDEBUG_DISPLAYF("dlCmdRenderPedCompressedDamageSet::Execute--(m_RequestedBlitIndex != PEDDECORATIONBUILDER.GetRenderedBlitIndex())");

		BANK_ONLY(if (g_DebugLockCompressionInBlittingStage == false))
		{
#if PED_DECORATIONS_MULTI_GPU_SUPPORT
			if (PEDDECORATIONBUILDER.SetGpuDoneAndCheckAll())
#endif
			{
				// update this here, so we know we really did draw the blits (some times the drawlist is skipped during various types of pause)
				PEDDECORATIONBUILDER.SetRenderedBlitIndex(m_RequestedBlitIndex);
			}
		}
		CPedDamageManager::PreDrawStateSetup();
		CCompressedPedDamageSet::Render( m_DecorationArray, m_ActiveDecorationTarget, m_Zones);
		CPedDamageManager::PostDrawStateSetup();
	}
}


//
// CompressedDecorationBuilder
//
CompressedDecorationBuilder* CompressedDecorationBuilder::smp_Instance = NULL;

grcRenderTarget* CompressedDecorationBuilder::m_pCompressionSrcTarget;
bool CompressedDecorationBuilder::ms_bEnableUncompressedMPDecorations = false;

#if RSG_DURANGO
grcTexture* CompressedDecorationBuilder::m_pCompressionSrcTexture;
#endif
grcTexture*		 CompressedDecorationBuilder::m_pCurOutputTexture;
atHashString	 CompressedDecorationBuilder::m_outputTextureTxdHash;
sqCompressState	 CompressedDecorationBuilder::m_TextureCompressionState;
bool CompressedDecorationBuilder::m_HasFinishedCompressing = false;
CCompressedPedDamageSet* CompressedDecorationBuilder::m_pCurSetToProcess;

void CompressedDecorationBuilder::Init() 
{
	smp_Instance = rage_new CompressedDecorationBuilder();
	GetInstance().AllocateRenderTargetPool();
	GetInstance().InitTextureManager();
}

void CompressedDecorationBuilder::InitTextureManager() 
{
	m_textureManager.Init(IsUsingUncompressedMPDecorations(), GetTargetWidth(), GetTargetHeight());
}

void CompressedDecorationBuilder::ShutdownTextureManager()
{
	m_textureManager.Shutdown();
}

void CompressedDecorationBuilder::Shutdown() 
{
	GetInstance().ReleaseRenderTarget();
	GetInstance().ReleaseRenderTargetPool();
	GetInstance().ShutdownTextureManager();
	delete smp_Instance;
	smp_Instance = NULL;
}

bool CompressedDecorationBuilder::AllocOutputTexture(atHashString& txdName, atHashString& txtName)
{
	return m_textureManager.Alloc(txdName, txtName);
}

void CompressedDecorationBuilder::ReleaseOutputTexture(atHashString outputTexture)
{
	m_textureManager.Release(outputTexture);
}

CompressedDecorationBuilder::CompressedDecorationBuilder() 
{
	m_BlitState = (u32)kWaitFrame0;
	m_pCurSetToProcess = NULL;
	m_pCompressionSrcTarget = NULL;
#if RSG_DURANGO
	m_pCompressionSrcTexture = NULL;
#endif
	m_TargetState = kNotAllocated;
	m_pCurOutputTexture = NULL;
	m_outputTextureTxdHash.Clear();
	m_bDontReleaseRenderTarget = false;
	m_RequestedBlitIndex=0;
	m_RenderedBlitIndex=0;
	m_bEnableCompressedDecorations = true;
	m_bEnableHiresCompressedDecorations = true;

	ms_bEnableUncompressedMPDecorations = USE_UNCOMPRESSED_MP_DECORATIONS;

#if PED_DECORATIONS_MULTI_GPU_SUPPORT
	m_GpuDoneMask = 0;
#endif
}

CompressedDecorationBuilder::~CompressedDecorationBuilder() 
{
	m_pCurOutputTexture = NULL;
	m_outputTextureTxdHash.Clear();
	m_BlitState = (u32)kWaitFrame0;
	m_pCurSetToProcess = NULL;
	m_pCompressionSrcTarget = NULL;
#if RSG_DURANGO
	m_pCompressionSrcTexture = NULL;
#endif
	m_TargetState = kNotAllocated;
	m_bDontReleaseRenderTarget = false;
}

bool CompressedDecorationBuilder::IsRenderTargetAvailable()
{
	bool bOk = true;

	if (m_TargetState == kNotAllocated)
	{
		Assert(m_pCompressionSrcTarget == NULL);

		//render target hasn't been allocated, try allocating some memory for it...
		bOk = AllocateRenderTarget();
	}

	// render target is allocated
	return bOk;
}

int CompressedDecorationBuilder::GetTargetHeight()
{
#if RSG_PC
	u32 dimension = (m_bEnableHiresCompressedDecorations?kDecorationHiResTargetRatio:1)*kDecorationTargetHeight;
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_TextureQuality >0) dimension = dimension << 1;
	return dimension;
#else
	return (m_bEnableHiresCompressedDecorations?kDecorationUltraResTargetRatio:1)*kDecorationTargetHeight;
#endif
}

int CompressedDecorationBuilder::GetTargetWidth()
{
#if RSG_PC
	u32 dimension = (m_bEnableHiresCompressedDecorations?kDecorationHiResTargetRatio:1)*kDecorationTargetWidth;
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_TextureQuality >0) dimension = dimension << 1;
	return dimension;
#else
	return (m_bEnableHiresCompressedDecorations?kDecorationUltraResTargetRatio:1)*kDecorationTargetWidth;
#endif
}

int	CompressedDecorationBuilder::GetTargetPitch() 
{ 
#if RSG_PC
	return (m_bEnableHiresCompressedDecorations?kDecorationHiResTargetRatio:1)*GetTargetWidth()*kDecorationTargetBpp/8;
#else
	return (m_bEnableHiresCompressedDecorations?kDecorationUltraResTargetRatio:1)*GetTargetWidth()*kDecorationTargetBpp/8;
#endif
}

void CompressedDecorationBuilder::AllocateRenderTargetPool() 
{
	// compute total size including mip levels
	u32 memSize = 0;
	u32 mipHeight = GetTargetHeight();
	for (u32 i = 0; i < kDecorationTargetMips; i++)
	{
		memSize += mipHeight * GetTargetPitch();
		mipHeight = Max(mipHeight >> 1, 1u);
	}


	m_RTPoolSize = (u32) pgRscBuilder::ComputeLeafSize(memSize, false);
	m_RTPoolId = kRTPoolIDInvalid;

}

bool CompressedDecorationBuilder::AllocateRenderTarget() 
{
	if(!AssertVerify(m_TargetState==kNotAllocated))
	{
		Errorf("[CPDD] CompressedDecorationBuilder::AllocateRenderTarget() called when m_TargetState!=kNotAllocated");
	}

	// We only allocate the temporary render target when using compressed decorations
	if (ms_bEnableUncompressedMPDecorations == false)
	{
		// allocate memory for the pool
		void* mem = 0;


		grcTextureFactory::CreateParams params;
		params.PoolID = m_RTPoolId;  
		params.PoolHeap = 0;
		params.Multisample = 0;
		params.HasParent = true;
		params.Parent = CPedDamageSetBase::GetCompressedDepthTarget(); 
		params.UseFloat = false;
		params.IsResolvable = true;
		params.ColorExpBias = 0;
		params.IsSRGB = false; // makes fading not work well
		params.IsRenderable = true;
		params.AllocateFromPoolOnCreate = true;
		params.MipLevels = kDecorationTargetMips;
#if RSG_ORBIS
		params.ForceNoTiling = true;
		params.Format = grctfA8B8G8R8;
#else
		params.Format = grctfA8R8G8B8;
#endif
#if RSG_PC
		params.Lockable = true;
#endif

		int hSize = GetTargetWidth();
		int vSize = GetTargetHeight();
		char name[128];
		sprintf(&name[0],"CompressedDecorationBuilder");


#if RSG_DURANGO
		grcTextureFactory::TextureCreateParams TextureParams(
			grcTextureFactory::TextureCreateParams::SYSTEM, 
			grcTextureFactory::TextureCreateParams::LINEAR,
			grcsRead|grcsWrite, 
			NULL, 
			grcTextureFactory::TextureCreateParams::RENDERTARGET,
			grcTextureFactory::TextureCreateParams::MSAA_NONE);


		m_pCompressionSrcTexture = grcTextureFactory::GetInstance().Create(hSize, vSize, params.Format, NULL, 1U, &TextureParams);

		if (m_pCompressionSrcTexture == NULL)
		{
			CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::AllocateRenderTarget() - m_pCompressionSrcTexture == NULL");
			strStreamingEngine::GetAllocator().Free(mem);
			return false;
		}

		m_pCompressionSrcTarget = grcTextureFactory::GetInstance().CreateRenderTarget(&name[0],  m_pCompressionSrcTexture->GetTexturePtr());
#else
		int bpp = kDecorationTargetBpp;
		m_pCompressionSrcTarget = grcTextureFactory::GetInstance().CreateRenderTarget(&name[0], grcrtPermanent, hSize, vSize, bpp, &params);
#endif

		if (m_pCompressionSrcTarget == NULL)
		{
			CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::AllocateRenderTarget() - m_pCompressionSrcTarget == NULL");
			strStreamingEngine::GetAllocator().Free(mem);
			return false;
		}
	}


	m_TargetState = kAvailable;
	CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::AllocateRenderTarget()");
	return true;
}

void CompressedDecorationBuilder::ReleaseRenderTargetPool()
{
	Assert(m_RTPoolId == kRTPoolIDInvalid);
}

void CompressedDecorationBuilder::ReleaseRenderTarget()
{
	// We only allocate the temporary render target when using compressed decorations
	if (ms_bEnableUncompressedMPDecorations == false)
	{
		if (m_pCompressionSrcTarget == NULL)
		{
			Assert(m_TargetState==kNotAllocated);
			return;
		}

		Assert(m_TargetState!=kNotAllocated);

		m_pCompressionSrcTarget->Release();
		m_pCompressionSrcTarget = NULL;

#if RSG_DURANGO
		m_pCompressionSrcTexture->Release();
		m_pCompressionSrcTexture = NULL;
#endif
	}
	else
	{
		m_pCompressionSrcTarget = NULL;
	}

	m_TargetState = kNotAllocated;
}

#if PED_DECORATIONS_MULTI_GPU_SUPPORT
bool CompressedDecorationBuilder::IsGpuDone() const
{
	const u32 gpu = GRCDEVICE.GPUIndex();
	return (m_GpuDoneMask & (1<<gpu)) != 0;
}

bool CompressedDecorationBuilder::SetGpuDoneAndCheckAll()
{
	const u32 gpu = GRCDEVICE.GPUIndex();
	Assertf(!(m_GpuDoneMask & (1<<gpu)), "Compressed ped damage is processing twice on the same GPU");
	m_GpuDoneMask |= (1<<gpu);
	const u8 gpuMask = (1 << GRCDEVICE.GetGPUCount(true)) - 1;
	return (m_GpuDoneMask & gpuMask) == gpuMask;
}

#endif //PED_DECORATIONS_MULTI_GPU_SUPPORT

// add set to builder queue
void CompressedDecorationBuilder::Add(CCompressedPedDamageSet* pDamageSet) 
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	if (pDamageSet->m_State == kTexturesLoaded) 
	{
		pDamageSet->m_State = kWaitingOnBlit;

		if (!m_PendingBlitQueue.Find(pDamageSet))
		{
			m_PendingBlitQueue.Push(pDamageSet);
		}
	}
	else if (pDamageSet->m_State == kWaitingOnTextures)
	{
		m_bDontReleaseRenderTarget = true;
	}
}

void CompressedDecorationBuilder::Remove(CCompressedPedDamageSet* pDamageSet) 
{
	int index;
	if (m_PendingBlitQueue.Find(pDamageSet,&index))
	{
		m_PendingBlitQueue.Delete(index);
		CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::Remove() pDamageSet = 0x08%p, index = %d", pDamageSet, index);
	}
}

// should come from BuildRenderList
// either pops a new request from the queue
// or updates the state machine for the current set
void CompressedDecorationBuilder::UpdateMain()
{	
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	ProcessQueue();
	ProcessCurrentState();
}

#if DEBUG_COMPRESSED_RENDER_TARGETS
static grcTexture* gOutputTexture = NULL;
#endif

// if the render target is available, process the next pending request
void CompressedDecorationBuilder::ProcessQueue()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	if(IsQueueEmpty())
		return;
	
	if (IsRenderTargetAvailable())  // check if the blit target is available, if not allocate it. if not allocatable, don't do the Queue.
	{
		// is there anything queued to process...
		if (m_TargetState == kAvailable) 
		{
			m_pCurSetToProcess = m_PendingBlitQueue.Pop();

			if (m_pCurSetToProcess != NULL) 
			{
				// we can only kick off the blit/compress stages if
				// the current set is actually in a ready state
				if (m_pCurSetToProcess->m_State != kWaitingOnBlit || m_pCurSetToProcess->GetOutputTexture()==NULL)
				{
					m_pCurSetToProcess = NULL;
					return;
				}

				m_pCurOutputTexture = m_pCurSetToProcess->GetOutputTexture();
#if DEBUG_COMPRESSED_RENDER_TARGETS
				gOutputTexture = m_pCurOutputTexture;
#endif
				m_outputTextureTxdHash = m_pCurSetToProcess->GetOutputTxdHash();

				// Use of uncompressed decoration textures is transparent to the rest of the ped damage system:
				// no temporary render target is allocated in this case, so we render directly to m_pCurOutputTexture
				if (ms_bEnableUncompressedMPDecorations)
					m_pCompressionSrcTarget = static_cast<grcRenderTarget*>(m_pCurOutputTexture);
				
				m_pCurSetToProcess->SetBlitTarget(m_pCompressionSrcTarget);

				// Uncompressed render targets are not ref counted
				if (ms_bEnableUncompressedMPDecorations == false)
				{
					// addref here instead of in Add() in case the set is release before it make it to the top of the queue
					strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_outputTextureTxdHash));
					g_TxdStore.AddRef(txdId, REF_RENDER);
					CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::ProcessQueue(): tdxId %d (%s), numRefs: %d", txdId.Get(), g_TxdStore.GetName(txdId), g_TxdStore.GetNumRefs(txdId));
				}

				m_RequestedBlitIndex++;
				m_BlitState = (u32)kWaitFrame0; // make sure it's ready, in case we aborted last time in the middle
				m_TargetState = kBlittingToTarget;
#if PED_DECORATIONS_MULTI_GPU_SUPPORT
				m_GpuDoneMask = 0;
#endif
			}
		}
	}
}

// updates state machine for current set being processed
void CompressedDecorationBuilder::ProcessCurrentState() 
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	switch (m_TargetState)
	{
		case kBlittingToTarget:
		{
			if (m_RequestedBlitIndex==m_RenderedBlitIndex)  // we don't do anything here until the render thread has had a chance to render the blits.
			{
				if ((eBlitState)m_BlitState >= kWaitFrame0 && (eBlitState)m_BlitState < kWaitFrameLast)
				{
					m_BlitState++;
				}
				else if ((eBlitState)m_BlitState == kWaitFrameLast) // finished waiting, so move on to next state
				{
					m_BlitState = kWaitDone;
					m_TargetState = kBlitDone;
				}
			}
			break;
		}
		case kBlitDone:
		{
			// if blit is done kick off compression stage
			if (m_pCurSetToProcess->m_State == kBlitting)
			{
				m_HasFinishedCompressing = false;
				m_TargetState = kCompressingTarget;
			}
			else
			{
				// otherwise the set was probably reset or cleared
				// we need to release the ref we added for blit/compression and clear the ouput has, so just call SetUpCurrentSet()
				SetUpCurrentSet();

				// make the target available
				m_TargetState = kAvailable;
			}
			break;
		}

		case kCompressingTarget:
		{	

#if !COMPRESS_DECORATIONS_ON_GPU_THREAD
			ProcessTextureCompression();
#endif
			
			// if compressed texture is ready, set up
			// current damage set to start using it
			if (IsTextureCompressionDone())
			{
				SetUpCurrentSet();
				m_TargetState = kAvailable;
			}

			break;
		}

		case kAvailable:
		{
			// if there are no pending requests and render target is
			// allocated, release it
			if (PEDDECORATIONBUILDER.IsQueueEmpty() && m_bDontReleaseRenderTarget == false)
			{
				CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::ProcessCurrentState() - Queue is Empty");

				PEDDECORATIONBUILDER.ReleaseRenderTarget();
			}

			// if next update there's still any sets waiting on textures this flag will be set
			// otherwise let the render target to be released
			m_bDontReleaseRenderTarget = false;
			break;
		}

		default: 
		{
		}
	}
}

// blitting stage for the current set being processed
void CompressedDecorationBuilder::AddToDrawList() 
{
	if (PEDDECORATIONBUILDER.BlitPending() && m_pCurSetToProcess)
	{
		if ((m_pCurSetToProcess->m_State == kWaitingOnBlit || m_pCurSetToProcess->m_State == kBlitting)) // we keep sending this until the state changes. (not all a display lists are actually rendered)
		{
			CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::AddToDrawList() blit count = %d", m_pCurSetToProcess ? m_pCurSetToProcess->GetDecorationList().GetCount() : -1);

			DLCPushTimebar("Compressed Damage Target");
			DLC(dlCmdRenderPedCompressedDamageSet, (m_pCurSetToProcess));
			DLCPopTimebar();

		#if __BANK
			if (g_DebugLockCompressionInBlittingStage == false)
		#endif
			{
				m_pCurSetToProcess->m_State = kBlitting; 
			}
		}
		else
		{
			// if we're here the current set must have been reset, in this case, we can get stuck because we'll never render the set so m_RenderedBlitIndex will never equal m_RequestedBlitIndex
			m_RenderedBlitIndex = m_RequestedBlitIndex; // fake it so the state machine can move on.
		}
	}
	
#if COMPRESS_DECORATIONS_ON_GPU_THREAD
	if (m_pCurSetToProcess)
	{
		if(m_TargetState == kCompressingTarget && m_HasFinishedCompressing == false)
		{
			DLCPushTimebar("Compressed Decoration Target");
			DLC_Add(CompressedDecorationBuilder::ProcessTextureCompression);
			DLCPopTimebar();
		}
	}
#endif
}



// called from ProcessCurrentState when the compressed texture
// is ready
void CompressedDecorationBuilder::SetUpCurrentSet()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::SetUpCurrentSet: m_State%s=kWaitingOnCompression",m_pCurSetToProcess && (m_pCurSetToProcess->m_State == kWaitingOnCompression) ? "=" : "!");

	// if we're waiting on compression, everything went well and we did not get reset along the way.
	if (m_pCurSetToProcess->m_State == kWaitingOnCompression)
	{
		m_pCurSetToProcess->SetTexture(m_pCurOutputTexture);
		m_pCurSetToProcess->m_State = kCompressed;
		m_pCurSetToProcess->ReleaseDecorationBlitTextures();
	}

	// release the reference we added for blitting/compression, unless we're running with uncompressed textures
	if (ms_bEnableUncompressedMPDecorations == false)
	{
		s32 txdId = g_TxdStore.FindSlotFromHashKey(m_outputTextureTxdHash).Get();
		if (txdId!=-1)
			CPedDamageSetBase::DelayedRemoveRefFromTXD(txdId);
	}

	// make sure we clear these otherwise we might try to release it again
	m_pCurOutputTexture = NULL;
	m_outputTextureTxdHash.Clear(); 
	m_pCurSetToProcess = NULL;
}

void CompressedDecorationBuilder::KickOffTextureCompression()
{

	Assert(m_pCompressionSrcTarget != NULL);

	CPEDDEBUG_DISPLAYF("CompressedDecorationBuilder::KickOffTextureCompression()");

	if (ms_bEnableUncompressedMPDecorations)
	{
		Assertf(0, "Trying to kick off compression when using uncompressed MP decorations: not good!");
		return;
	}

#if USE_DEFRAGMENTATION
		// stop defrags so it does not move while we're compressing into the target.
		strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_outputTextureTxdHash.GetHash()));
		if (txdId!=-1)
		{
			strIndex index = g_TxdStore.GetStreamingIndex(txdId);
			strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);
			if (info.IsFlagSet(STRFLAG_INTERNAL_DEFRAGGING))
				strStreamingEngine::GetDefragmentation()->Cancel(index, true);

			strStreamingEngine::GetDefragmentation()->Lock(g_TxdStore.Get(txdId), index);
		}
#endif

#if RSG_DURANGO
	grcTexture* textureSource = m_pCompressionSrcTexture;
#else
	grcTexture* textureSource = m_pCompressionSrcTarget;
#endif

	// default is kColourClusterFit which takes about 100ms for a 128x128.
	int flags = squish::kColourIterativeClusterFit;
	m_TextureCompressionState.Compress(	textureSource, 
										m_pCurOutputTexture,
										flags,
										0);		// may want to set up a new scheduler slot for this.
}

void CompressedDecorationBuilder::ProcessTextureCompression()
{
	if(m_HasFinishedCompressing || m_pCurSetToProcess == NULL)
	{
		return;
	} 

	if(m_pCurSetToProcess->m_State == kBlitting)
	{
		// Use of uncompressed decoration textures is transparent to the rest of the ped damage system:
		// keep the state machine the same without actually doing any compression
		if (ms_bEnableUncompressedMPDecorations == false)
		{
		#if RSG_PC
			reinterpret_cast<grcTextureDX11*>(m_pCurOutputTexture)->BeginTempCPUResource();
		#endif
			KickOffTextureCompression();
		}
		m_pCurSetToProcess->m_State = kWaitingOnCompression;
	}
	else
	{
		// Use of uncompressed decoration textures is transparent to the rest of the ped damage system:
		// keep the state machine the same without actually doing any compression
		if (ms_bEnableUncompressedMPDecorations == false)
		{
			m_HasFinishedCompressing = 	m_TextureCompressionState.Poll();

		#if RSG_PC
			if(m_HasFinishedCompressing)
			{
				ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
				reinterpret_cast<grcTextureDX11*>(m_pCurOutputTexture)->EndTempCPUResource();
				ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
			}
		#endif
		}
		else
		{
			m_HasFinishedCompressing = true;
		}
	}
}

bool CompressedDecorationBuilder::IsTextureCompressionDone()
{
	bool done = m_HasFinishedCompressing;
	
#if USE_DEFRAGMENTATION
	// Use of uncompressed decoration textures is transparent to the rest of the ped damage system:
	// keep the state machine the same without actually doing any compression
	if (ms_bEnableUncompressedMPDecorations == false && done)
	{
		// release the lock, so it can be defragged again
		strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlotFromHashKey(m_outputTextureTxdHash.GetHash()));
		if (txdId!=-1)
			strStreamingEngine::GetDefragmentation()->Unlock(g_TxdStore.Get(txdId), g_TxdStore.GetStreamingIndex(txdId));
	}
#endif //USE_DEFRAGMENTATION
	
	return done;
}

#if __BANK

void CompressedDecorationBuilder::AddToggleWidget(bkBank & bank)
{
	bank.AddToggle("Use Compressed Decorations", &m_bEnableCompressedDecorations);
}

void CompressedDecorationBuilder::DebugDraw()
{
#if DEBUG_COMPRESSED_RENDER_TARGETS
	if (gOutputTexture != NULL)
	{
		PUSH_DEFAULT_SCREEN();
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcBindTexture(gOutputTexture);
		grcDrawSingleQuadf(200,50,400,250,0,0,0,1,1,Color32(255,255,255,255));
		
		POP_DEFAULT_SCREEN();
	}
	if (m_pCompressionSrcTarget != NULL)
	{
		PUSH_DEFAULT_SCREEN();
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcBindTexture(m_pCompressionSrcTarget);
		grcDrawSingleQuadf(800,50,1000,250,0,0,0,1,1,Color32(255,255,255,255));
		POP_DEFAULT_SCREEN();
	}
#endif
}
#endif // __BANK


class CPedDamageDataMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		dlcDebugf3("CPedDamageDataMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);

		switch (file.m_fileType)
		{
			case CDataFileMgr::PED_DAMAGE_OVERRIDE_FILE:
				CPedDamageManager::GetInstance().GetDamageData().Load(file.m_filename);
				return true;

			case CDataFileMgr::PED_DAMAGE_APPEND_FILE:
				CPedDamageManager::GetInstance().GetDamageData().AppendUniques(file.m_filename);
				return true;

			default:
				return false;
		}
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file) 
	{
		dlcDebugf3("CPedDamageDataMounter::LoadDataFile: %s type = %d", file.m_filename, file.m_fileType);
		switch (file.m_fileType)
		{
			case CDataFileMgr::PED_DAMAGE_OVERRIDE_FILE:
				CPedDamageManager::GetInstance().GetDamageData().Load("common:/data/effects/peds/PedDamage.xml");
				return;

			case CDataFileMgr::PED_DAMAGE_APPEND_FILE:
				CPedDamageManager::GetInstance().GetDamageData().UnloadAppend(file.m_filename);
				return;

			default:
				return;
		}
	}

} g_PedDamageDataFileMounter;

//
// CPedDamageManager
//
CPedDamageManager* CPedDamageManager::smp_Instance = 0;

grcBlendStateHandle	CPedDamageManager::sm_Decoration_BS[2];
grcBlendStateHandle	CPedDamageManager::sm_Blood_BS;
grcBlendStateHandle	CPedDamageManager::sm_BloodSoak_BS;
grcBlendStateHandle	CPedDamageManager::sm_Bump_BS;
grcBlendStateHandle	CPedDamageManager::sm_SkinDecorationProcess_BS;

grcDepthStencilStateHandle	CPedDamageManager::sm_StencilWrite_RSS;
grcDepthStencilStateHandle	CPedDamageManager::sm_StencilRead_RSS;

#if __BANK
const grcTexture * CPedDamageManager::sm_DebugDisplayDamageTex=NULL;
const grcTexture * CPedDamageManager::sm_DebugDisplayTattooTex=NULL;
int				   CPedDamageManager::sm_DebugRenderMode=0;
bool               CPedDamageManager::sm_ApplyDamage=true;
bkGroup *		   CPedDamageManager::sm_WidgetGroup=NULL;
int				   CPedDamageManager::sm_ReloadCounter=0;

bool				CPedDamageManager::sm_EnableBlitAlphaOverride=false;
float				CPedDamageManager::sm_BlitAlphaOverride=0.0f;
#endif

u32 CPedDamageManager::sm_FrameID = 0;

atArray<CPedScarNetworkData> CPedDamageManager::sm_CachedLocaPedScarData;
bool						CPedDamageManager::sm_IsCachedLocaPedScarDataValid = false;

atArray<CPedBloodNetworkData> CPedDamageManager::sm_CachedLocalPedBloodData;
float						  CPedDamageManager::sm_TimeStamp;

CPedDamageManager:: CPedDamageManager()
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	CPedDamageSet::AllocateTextures();

	for (int i = 0; i<m_DamageSetPool.GetMaxCount();i++)
		m_DamageSetPool[i] = rage_new CPedDamageSet();

	for (int i = 0; i<m_CompressedDamageSetProxyPool.GetMaxCount();i++)
		m_CompressedDamageSetProxyPool[i] = rage_new CCompressedPedDamageSetProxy();
	
	for (int i = 0; i<m_CompressedDamageSetPool.GetMaxCount();i++)
		m_CompressedDamageSetPool[i] = rage_new CCompressedPedDamageSet();

	CPedDamageSet::InitViewports();

	grcImage *image = grcImage::MakeChecker(8, Color32(0,0,255,0), Color32(0,0,255,0));
	BANK_ONLY(grcTexture::SetCustomLoadName("NoBloodTex");)
	m_NoBloodTex = grcTextureFactory::GetInstance().Create(image);
	BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
	image->Release();

	image = grcImage::MakeChecker(8, Color32(0,0,0,127), Color32(0,0,0,127));
	BANK_ONLY(grcTexture::SetCustomLoadName("NoTattooTex");)
	m_NoTattooTex = grcTextureFactory::GetInstance().Create(image);
	BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
	image->Release();

	m_DetailManager.Init();
}

CPedDamageManager::~CPedDamageManager()
{
	for (int i = 0; i<m_DamageSetPool.GetMaxCount();i++)
	{
		delete m_DamageSetPool[i];
		m_DamageSetPool[i]=NULL;
	}

	for (int i = 0; i<m_CompressedDamageSetProxyPool.GetMaxCount();i++)
	{
		delete m_CompressedDamageSetProxyPool[i];
		m_CompressedDamageSetProxyPool[i]=NULL;
	}

	for (int i = 0; i<m_CompressedDamageSetPool.GetMaxCount();i++)
	{
		delete m_CompressedDamageSetPool[i];
		m_CompressedDamageSetPool[i]=NULL;
	}

	CPedDamageSet::ReleaseTextures();
	
	if (m_NoBloodTex)
		m_NoBloodTex->Release();
	if (m_NoTattooTex)
		m_NoTattooTex->Release();

	CPedDamageSet::ReleaseViewports();

	m_DetailManager.Shutdown();

	m_bAllowDecorationsOnNewDamageSet = false;
}


//////////////////////////////////////////////////////////////////////////

void CPedDamageManager::Init(void)
{
	CompressedDecorationBuilder::Init();

	smp_Instance = rage_new CPedDamageManager;

	CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_DAMAGE_OVERRIDE_FILE, &g_PedDamageDataFileMounter, eDFMI_UnloadLast);
	CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_DAMAGE_APPEND_FILE, &g_PedDamageDataFileMounter, eDFMI_UnloadFirst);

	smp_Instance->m_DamageData.Load("common:/data/effects/peds/PedDamage.xml");

	CPedDamageSet::InitShaders();

#define setbs(rt,d,s,o,c1,c2,c3) rt.DestBlend = rt.DestBlendAlpha = grcRSV::BLEND_##d; rt.SrcBlend = rt.SrcBlendAlpha = grcRSV::BLEND_##s; rt.BlendOp = rt.BlendOpAlpha = grcRSV::BLENDOP_##o;  rt.RenderTargetWriteMask = (grcRSV::COLORWRITEENABLE_##c1 | grcRSV::COLORWRITEENABLE_##c2 | grcRSV::COLORWRITEENABLE_##c3); rt.BlendEnable = TRUE;
#define setbsalpha(rt,d,s,o)  rt.DestBlendAlpha = grcRSV::BLEND_##d; rt.SrcBlendAlpha = grcRSV::BLEND_##s; rt.BlendOpAlpha = grcRSV::BLENDOP_##o;

	grcBlendStateDesc stateDescB;

	setbs(stateDescB.BlendRTDesc[GBUFFER_RT_0], INVSRCALPHA, SRCALPHA, ADD, ALL, NONE, NONE);	// blendset = setup for accumulation of alpha blits, colormask = RGBA
	setbsalpha(stateDescB.BlendRTDesc[GBUFFER_RT_0], INVSRCALPHA, ZERO, ADD);					// this RT will be applied with (1,SrcAlpha)
	sm_Decoration_BS[0] = grcStateBlock::CreateBlendState(stateDescB);
	
	setbs(stateDescB.BlendRTDesc[GBUFFER_RT_0], ONE, ONE, ADD, NONE, NONE, NONE);				// no alpha test, want to hi histencil on 360
	sm_Decoration_BS[1] = grcStateBlock::CreateBlendState(stateDescB);

	setbs(stateDescB.BlendRTDesc[GBUFFER_RT_0], ONE, ONE, MAX, GREEN, NONE, NONE);				// blendset = grcbsCompositeMax, colorMask = G
	sm_BloodSoak_BS = grcStateBlock::CreateBlendState(stateDescB);

	setbs(stateDescB.BlendRTDesc[GBUFFER_RT_0],INVSRCALPHA,SRCALPHA,ADD,RED,BLUE,NONE);			// blendset = grcbsCompositeNormal, colormask = RB
	sm_Blood_BS = grcStateBlock::CreateBlendState(stateDescB);

	setbs(stateDescB.BlendRTDesc[GBUFFER_RT_0], ONE, ONE, MAX, ALPHA, NONE, NONE);				// blendset = grcbsCompositeMAX, colorSet = ALPHA
	sm_Bump_BS = grcStateBlock::CreateBlendState(stateDescB);
	
	setbs(stateDescB.BlendRTDesc[GBUFFER_RT_0], ONE, ONE, SUBTRACT, ALPHA, NONE, NONE);
	sm_SkinDecorationProcess_BS = grcStateBlock::CreateBlendState(stateDescB);

	// stencil writing set up (we'll write 0x1 for skin)
	grcDepthStencilStateDesc dss;
	dss.DepthWriteMask = FALSE;
	dss.DepthEnable = TRUE;
	dss.DepthFunc = grcRSV::CMP_ALWAYS;
	
	dss.StencilEnable = TRUE;	
	dss.StencilReadMask = 0xff;
	dss.StencilWriteMask = 0xff;
	dss.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
	dss.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
	dss.FrontFace.StencilFailOp = grcRSV::STENCILOP_REPLACE;
	dss.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_REPLACE;
	dss.BackFace = dss.FrontFace; 

	sm_StencilWrite_RSS = grcStateBlock::CreateDepthStencilState(dss);  
	dss.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
	dss.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	dss.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	dss.BackFace = dss.FrontFace; 
	
	// stencil reading setup 
	dss.StencilEnable = TRUE;	
	dss.StencilReadMask = 0xff;
	dss.StencilWriteMask = 0xff;
	dss.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	dss.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
	dss.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	dss.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	dss.BackFace = dss.FrontFace; 
	
	sm_StencilRead_RSS = grcStateBlock::CreateDepthStencilState(dss); 

	sm_CachedLocalPedBloodData.Reset();
	sm_CachedLocaPedScarData.Reset();
	sm_IsCachedLocaPedScarDataValid = false;

	smp_Instance->m_crewEmblemDecorationHash = atHashString("CREW_EMBLEM_TEXTURE",0xAC25A1C6);
	smp_Instance->m_bAllowDecorationsOnNewDamageSet = false;
}

void CPedDamageManager::Shutdown(void)
{
	CompressedDecorationBuilder::Shutdown();

	delete smp_Instance;
	smp_Instance = NULL;

	sm_CachedLocalPedBloodData.Reset();
	sm_CachedLocaPedScarData.Reset();
	sm_IsCachedLocaPedScarDataValid = false;
}

void CPedDamageManager::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdRenderPedDamageSet);
	DLC_REGISTER_EXTERNAL(dlCmdRenderPedCompressedDamageSet);
}

u8 CPedDamageManager::AllocateDamageSet(CPed * pPed)
{
	if (pPed==NULL)
		return kInvalidPedDamageSet;
	
	u8 bestIndex = kInvalidPedDamageSet;
	float bestScore = -1000.0f;

	// look for free slot or at least best candidate 
	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (m_DamageSetPool[i]->GetPed()==NULL)
		{
			bestIndex = (u8)i;	
			break;
		}
		else if (!m_DamageSetPool[i]->IsLocked())
		{
			float score = m_DamageSetPool[i]->GetReuseScore();
			if (score>bestScore)  // not free, but is it the best candidate so far?
			{
				bestScore = score;
				bestIndex = (u8)i;
			}
		}
	}

	PEDDEBUG_DISPLAYF("CPedDamageManager::AllocateDamageSet: ped=0x%p(%s)", pPed, GetPedName(pPed));

	// let's replace (or use for the first time) the ped using this slot
	// TODO: check to see if we really should reuse the target. we may have a much worse score that the current user...
	if (bestIndex != kInvalidPedDamageSet)
	{
#if __BANK
		if (m_DamageSetPool[bestIndex]->GetPed())
			PEDDEBUG_DISPLAYF("CPedDamageManager::AllocateDamageSet: taking damage set in use by ped=0x%p(%s)", m_DamageSetPool[bestIndex]->GetPed(), GetPedName(m_DamageSetPool[bestIndex]->GetPed()));
#endif
		if (m_DamageSetPool[bestIndex]->SetPed(pPed))
		{
			m_DamageSetPool[bestIndex]->SetReuseScore(camInterface::GetPos().Dist(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())), pPed->GetIsVisibleInSomeViewportThisFrame(), false);
			m_DamageSetPool[bestIndex]->SetSkipDecorationTarget(BANK_ONLY((pPed->GetCompressedDamageSetID() != kInvalidPedDamageSet && !pPed->IsLocalPlayer()) ||) (PEDDECORATIONBUILDER.IsUsingCompressedMPDecorations() && pPed->IsNetworkPlayer()));    // bank only for testing local ped
			m_DamageSetPool[bestIndex]->SetForcedUncompressedDecorations(m_bAllowDecorationsOnNewDamageSet);
			if (m_bAllowDecorationsOnNewDamageSet)
			{
				m_DamageSetPool[bestIndex]->SetSkipDecorationTarget(false);
				m_bAllowDecorationsOnNewDamageSet = false;
			#if __BANK
				PEDDEBUG_DISPLAYF("[PDD] CPedDamageManager::AllocateDamageSet- FORCING decorations on damage set");
			#endif	
			}
		}
	}
	else
	{
#if __BANK
		Warningf("[PDD] CPedDamageManager::AllocateDamageSet- no ped damage set available");
#endif	
	}

	return bestIndex;
}

u8 CPedDamageManager::AllocateCompressedDamageSetProxy(CPed * pPed, bool forClone)
{
	if (pPed==NULL)
	{
		CPEDDEBUG_DISPLAYF("CPedDamageManager::AllocateCompressedDamageSetProxy - pPed is NULL");
		return kInvalidPedDamageSet;
	}

	u8 proxyIndex = kInvalidPedDamageSet;

	// look for free slot
	for (int i=0;i< m_CompressedDamageSetProxyPool.GetMaxCount();i++)
	{
		if (m_CompressedDamageSetProxyPool[i]->IsAvailable())
		{
			proxyIndex = (u8)i;
			break;
		}

	}

	if (proxyIndex == kInvalidPedDamageSet)
	{
		CPEDDEBUG_DISPLAYF("CPedDamageManager::AllocateCompressedDamageSetProxy - out of CompressedDamageSet Proxies. ped=0x%p (%s) will not get a compressed decorations",pPed,GetPedName(pPed));
	}
	else
	{
		CPEDDEBUG_DISPLAYF("CPedDamageManager::AllocateCompressedDamageSetProxy - ped=0x%p(%s)%s will use proxyIndex = %d", pPed, GetPedName(pPed), (forClone)?" for a Clone":"", proxyIndex);
	}

	// associate ped with set
	if (proxyIndex != kInvalidPedDamageSet)
	{
		// we have a proxy, now we need not we need to find a damage set for it.
		// there should be enough assuming there are only 16 MP players and the clones did not steal or leak them
		if (!forClone)
		{
			u8 setIndex = kInvalidPedDamageSet;

			// look for free slot
			for (int i=0;i< m_CompressedDamageSetPool.GetMaxCount();i++)
			{
				if (m_CompressedDamageSetPool[i]->IsAvailable())
				{
					setIndex = (u8)i;
					break;
				}
			}

			if (setIndex != kInvalidPedDamageSet)
			{
				m_CompressedDamageSetProxyPool[proxyIndex]->SetCompressedTextureSet(m_CompressedDamageSetPool[setIndex]);
				CPEDDEBUG_DISPLAYF("CPedDamageManager::AllocateCompressedDamageSetProxy - ped=0x%p (%s) using setIndex %d, refcount=%d",pPed,GetPedName(pPed),setIndex,m_CompressedDamageSetPool[setIndex]->GetRefCount());

				if (m_CompressedDamageSetProxyPool[proxyIndex]->SetPed(pPed) == false)
				{
					m_CompressedDamageSetProxyPool[proxyIndex]->SetCompressedTextureSet(NULL);
					m_CompressedDamageSetPool[setIndex]->DecRef();
					proxyIndex = kInvalidPedDamageSet;
				}
			}
			else
			{
				CPEDDEBUG_DISPLAYF("CPedDamageManager::AllocateCompressedDamageSetProxy - could not find a free CompressedDamageSet. ped=0x%p (%s) will not get a compressed decorations",pPed,GetPedName(pPed));
				proxyIndex = kInvalidPedDamageSet;
			}
		}
	}

	return proxyIndex;
}

void CPedDamageManager::CheckStreaming()
{
	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (m_DamageSetPool[i]->IsWaitingForTexture())
		{
			m_DamageSetPool[i]->CheckForTexture();
		}
	}

	for (int i=0;i< m_CompressedDamageSetProxyPool.GetMaxCount();i++)
	{
		CCompressedPedDamageSet * set = m_CompressedDamageSetProxyPool[i]->GetCompressedTextureSet();
		if (set && set->IsWaitingForTexture())
		{
			set->CheckForTexture();
		}
	}
}

float CPedDamageManager::CalcLodScale()
{
	float lodScale = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) ? m_DamageData.m_CutsceneLODDistanceScale : 1.0f;
	
	if (const grcViewport *vp = gVpMan.GetUpdateGameGrcViewport())
	{
		float tanFov = vp->GetTanHFOV();
		float adjust = 0.80f/tanFov;			// the original tuning was based on the default camera FOV of about .8, so we need to adjust when the camera is zoomed in
		lodScale *= Clamp(adjust,1.0f,3.0f);	// don't let it go too crazy
	}

	return lodScale;
}

void CPedDamageManager::UpdatePriority(u8 damageIndex, float distance, bool isVisible, bool isDead)
{
	if (damageIndex==kInvalidPedDamageSet)
		return;

	if (!Verifyf(m_NeedsSorting,"CPedDamageManager::UpdatePriority() called outside of CPedDamageManager::PreRender()/CPedDamageManager::PostPreRender() block"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::UpdatePriority() called outside of CPedDamageManager::PreRender()/CPedDamageManager::PostPreRender() block");
		return;
	}

	float lodScale = CalcLodScale();

	// adjust the distance for the player ped, so he draws damage even when past the usual cutoff
	// also check if the ped currently has compressed decorations, if so we don't need to allocate a render target for decorations
	CPed * pPed = m_DamageSetPool[damageIndex]->GetPed();
	if(pPed)
	{
		if  (pPed->IsLocalPlayer())
		{
			if (distance>m_DamageData.m_MedResTargetDistanceCutoff*lodScale)
				distance = m_DamageData.m_MedResTargetDistanceCutoff*lodScale+0.1f; // just past the medium cutoff
		}
#if __BANK    // this might change if we're playing with widgets, otherwise it is set when the damage target is created
		if (m_DamageSetPool[damageIndex]->GetForcedUncompressedDecorations() == false)
			m_DamageSetPool[damageIndex]->SetSkipDecorationTarget( pPed->GetCompressedDamageSetID() != kInvalidPedDamageSet && !pPed->IsLocalPlayer());
#endif
	}

	bool bAlreadyAdded = ( m_DamageSetPool[damageIndex]->GetLastVisibleFrame() == CPedDamageManager::GetFrameID() ); // UI sometimes puts the same ped in multiple times...

	m_DamageSetPool[damageIndex]->SetReuseScore(distance, isVisible, isDead);

	// only add it to the active list if it needs a rendertarget generated
	if (!bAlreadyAdded)
	{
		if (isVisible && pPed && distance < m_DamageData.m_LowResTargetDistanceCutoff && (m_DamageSetPool[damageIndex]->HasBlood() || m_DamageSetPool[damageIndex]->HasDecorations()))
			m_ActiveDamageSets.Append() = m_DamageSetPool[damageIndex];
		else
			m_DamageSetPool[damageIndex]->SetRenderTargets(-1); // reset the render target, so we don't get old stuff...
	}
}

ePedDamageZones GetDamageZoneFromBoneTag(eAnimBoneTag boneTag)
{
	ePedDamageZones zone = kDamageZoneInvalid;

	switch( boneTag )
	{
		case BONETAG_ROOT:
		case BONETAG_PELVISROOT:
		case BONETAG_PELVIS:
		case BONETAG_PELVIS1:
		case BONETAG_SPINE_ROOT:
		case BONETAG_SPINE0: 
		case BONETAG_SPINE1: 
		case BONETAG_SPINE2:
		case BONETAG_SPINE3:
		case BONETAG_TAIL0:
		case BONETAG_TAIL1:        
		case BONETAG_TAIL2: 
		case BONETAG_TAIL3:
		case BONETAG_TAIL4:
		case BONETAG_TAIL5:
		case BONETAG_SKEL_TAIL0:
		case BONETAG_SKEL_TAIL1:        
		case BONETAG_SKEL_TAIL2: 
		case BONETAG_SKEL_TAIL3:
		case BONETAG_SKEL_TAIL4:
		case BONETAG_SKEL_TAIL5:
		case BONETAG_TAILM1:       
		case BONETAG_TAILM2:       
		case BONETAG_TAILM3:       
			zone = kDamageZoneTorso;
			break; 
		case BONETAG_NECK: 
		case BONETAG_NECK2: 
		case BONETAG_HEAD:
		case BONETAG_NECKROLL: 
		case BONETAG_L_EYE:        
		case BONETAG_R_EYE:        
		case BONETAG_L_EYE2:       
		case BONETAG_R_EYE2:       
		case BONETAG_JAW:  
		case BONETAG_HAIR_SCALE:   
			zone = kDamageZoneHead;
			break;         
		case BONETAG_R_CLAVICLE:   
		case BONETAG_R_UPPERARM:   
		case BONETAG_R_FOREARM:    
		case BONETAG_R_HAND:       
		case BONETAG_R_FINGER0:    
		case BONETAG_R_FINGER01:   
		case BONETAG_R_FINGER02:   
		case BONETAG_R_FINGER1:    
		case BONETAG_R_FINGER11:   
		case BONETAG_R_FINGER12:   
		case BONETAG_R_FINGER2:    
		case BONETAG_R_FINGER21:   
		case BONETAG_R_FINGER22:   
		case BONETAG_R_FINGER3:   
		case BONETAG_R_FINGER31:   
		case BONETAG_R_FINGER32:   
		case BONETAG_R_FINGER4:    
		case BONETAG_R_FINGER41:  
		case BONETAG_R_FINGER42: 
		case BONETAG_R_ARMROLL:
		case BONETAG_R_FOREARMROLL:
		case BONETAG_R_PH_HAND:    
		case BONETAG_R_IK_HAND:   
		case BONETAG_R_CLAW:       
			zone = kDamageZoneRightArm;
			break; 
		case BONETAG_L_CLAVICLE:   
		case BONETAG_L_UPPERARM:   
		case BONETAG_L_FOREARM:   
		case BONETAG_L_HAND:      
		case BONETAG_L_FINGER0:    
		case BONETAG_L_FINGER01:   
		case BONETAG_L_FINGER02:   
		case BONETAG_L_FINGER1:    
		case BONETAG_L_FINGER11:   
		case BONETAG_L_FINGER12:   
		case BONETAG_L_FINGER2:    
		case BONETAG_L_FINGER21:   
		case BONETAG_L_FINGER22:   
		case BONETAG_L_FINGER3:    
		case BONETAG_L_FINGER31:   
		case BONETAG_L_FINGER32:   
		case BONETAG_L_FINGER4:    
		case BONETAG_L_FINGER41:   
		case BONETAG_L_FINGER42:
		case BONETAG_L_ARMROLL: 
		case BONETAG_L_FOREARMROLL:
		case BONETAG_L_PH_HAND:    
		case BONETAG_L_IK_HAND:    
		case BONETAG_L_CLAW:       
			zone = kDamageZoneLeftArm;
			break;   
		case BONETAG_L_THIGH:      
		case BONETAG_L_CALF:       
		case BONETAG_L_FOOT:      
		case BONETAG_L_TOE:        
		case BONETAG_L_TOE1:
		case BONETAG_L_THIGHROLL:  
		case BONETAG_L_STIRRUP:    
		case BONETAG_L_PH_FOOT:    
		case BONETAG_L_IK_FOOT:    
			zone = kDamageZoneLeftLeg;
			break;       
		case BONETAG_R_THIGH:      
		case BONETAG_R_CALF:       
		case BONETAG_R_FOOT:       
		case BONETAG_R_TOE:        
		case BONETAG_R_TOE1:
		case BONETAG_R_THIGHROLL:  
		case BONETAG_R_STIRRUP:    
		case BONETAG_R_PH_FOOT:    
		case BONETAG_R_IK_FOOT:    
			zone = kDamageZoneRightLeg;
			break;       
		case BONETAG_HIGH_HEELS:   
		case BONETAG_GUN_STOCK:    
		case BONETAG_BAGROOT:      
		case BONETAG_BAGPIVOTROOT: 
		case BONETAG_BAGPIVOT:     
		case BONETAG_BAGBODY:      
		case BONETAG_BAGBONE_R:   
		case BONETAG_BAGBONE_L:
		case BONETAG_WEAPON_GRIP:  
		case BONETAG_WEAPON_GRIP2: 
		case BONETAG_CAMERA:       
		case BONETAG_SKEL_SADDLE:  
		case BONETAG_FACING_DIR:   
		case BONETAG_LOOK_DIR:     
		default:
			break;
	};
	return zone;
}

ePedDamageZones CPedDamageManager::ComputeLocalPositionAndZone( u8 damageIndex, u16 component, Vec3V_InOut pos, Vec3V_InOut dir)
{	
	CPed * pPed = m_DamageSetPool[damageIndex]->GetPed();
	ePedDamageZones zone = kDamageZoneInvalid;

	if (Verifyf(pPed,"The specified Damage Index (%d) is not associated with a ped", damageIndex))
	{
		eAnimBoneTag boneTag = pPed->GetBoneTagFromRagdollComponent(component);
		zone = GetDamageZoneFromBoneTag(boneTag);

		if (zone != kDamageZoneInvalid)
		{
			fragInst*  pFragInst = pPed->GetFragInst();
			if (pFragInst==NULL)
				goto __NoFragFound;

			const fragType* pFragType = pFragInst->GetType();
			if (pFragType==NULL)
				goto __NoFragFound;

			const fragPhysicsLOD * physLOD  = pFragType->GetPhysics(0);
			if (physLOD==NULL || physLOD->GetNumChildren() <= component)
				goto __NoFragFound;

			fragTypeChild* child = physLOD->GetAllChildren()[component];
			crSkeleton * pSkeleton = pPed->GetSkeleton();
			
			if (child==NULL || pSkeleton==NULL) 
				goto __NoFragFound;

			int boneIndex = pFragType->GetBoneIndexFromID(child->GetBoneID());
		
			Mat34V boneMtx;
			pSkeleton->GetGlobalMtx(boneIndex,boneMtx);

			// get back to model space
			
			Vec3V localPos = UnTransformOrtho( boneMtx, pos);	  // assume there  there is no scale in these...
			Vec3V localDir = UnTransform3x3Ortho( boneMtx, dir);

			// then to bind pose default space
			Mat34V localMtx;
			GetCumulativeDefaultTransform(localMtx, pSkeleton->GetSkeletonData(),boneIndex);
			
			pos = Transform( localMtx, localPos);
			dir = Transform3x3( localMtx, localDir);
		}
	}
	return zone;

__NoFragFound:

	PEDDEBUG_DISPLAYF("CPedDamageManager::ComputeLocalPositionAndZone() - couldn't find valid Frag type for ped 0x%p(%s), component %d(%s)",pPed, GetPedName(pPed),component, component<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[component] : "out of range");

	return kDamageZoneInvalid;

}

int CPedDamageManager::AddBloodDamage( u8 damageIndex, u16 component, const Vector3 & pos, const Vector3 & dir, ePedDamageTypes type, CPedBloodDamageInfo * pBloodInfo, bool forceNotStanding, bool bFromWeapon, bool bReduceBleeding)
{	
	
	Vec3V localPos = VECTOR3_TO_VEC3V(pos);
	Vec3V localDir = VECTOR3_TO_VEC3V(dir);

	ePedDamageZones zone  = ComputeLocalPositionAndZone(damageIndex, component, localPos, localDir);

	if (zone!=kDamageZoneInvalid)
	{
		CPed * pPed = m_DamageSetPool[damageIndex]->GetPed();
		bool bIsStanding = IsPedStanding(pPed,forceNotStanding);

		Vector2 uv,uv2;
		float minscale =(zone>kDamageZoneHead) ? 0.5f : 0.0f; // for legs and arms, don't go all the way to the smallest scale, since they are already 1/2 size of the torso, they tend to get really small

		float scale = s_Random.GetRanged(minscale,1.0f); // pick a size, we'll push to keep it in the zone.
	
		if (bReduceBleeding)
			scale = minscale;

		u8 flipUVflags = (u8)s_Random.GetRanged(0.0f, 4.0f); // The textures that are allowed to flip will flip together with this setting
		
		float radius;
		if (pBloodInfo->m_SplatterTexture.m_pTexture == NULL) // if there is no splatter texture, use the wound size.
			radius = Lerp(scale, pBloodInfo->m_WoundMinSize, pBloodInfo->m_WoundMaxSize);
		else
			radius = Lerp(scale, pBloodInfo->m_SplatterMinSize, pBloodInfo->m_SplatterMaxSize);
		
		bool useGravitySoak = (pBloodInfo->m_RotationType == kGravityRotation) || (pBloodInfo->m_RotationType == kAutoDetectRotation && bIsStanding);

		float soakScale = Lerp(scale, pBloodInfo->m_SoakScaleMin, 1.0f); // lerp between min and 1.0 for soak, so it scales with the wound

		radius = Max(soakScale*(useGravitySoak ? pBloodInfo->m_SoakStartSizeGravity: pBloodInfo->m_SoakStartSize), radius)/100.0f;

		bool limitScale = m_DamageSetPool[damageIndex]->CalcUVs(uv, uv2, VEC3V_TO_VECTOR3(localPos), VEC3V_TO_VECTOR3(localDir), zone, type, radius/2.0f, pBloodInfo->m_RotationType, bIsStanding, bFromWeapon);
		
		if (type==kDamageTypeStab)
			scale = uv.Dist(uv2);

		int damageBlitID = -1;
		damageBlitID = m_DamageSetPool[damageIndex]->AddDamageBlitByUVs(uv, uv2, zone, type, pBloodInfo, bReduceBleeding || limitScale, bIsStanding, scale, 0.0f, -1, flipUVflags, bFromWeapon, bReduceBleeding);

#if GTA_REPLAY
		if(damageBlitID >= 0 && CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx(CPacketPedBloodDamage(component, pos, dir, (u8)type, pBloodInfo->m_Name.GetHash(), forceNotStanding, pPed, damageBlitID), pPed);
		}
#endif // GTA_REPLAY

		return damageBlitID;
		
	}
	return -1;
}

u8 CPedDamageManager::GetDamageID( CPed * pPed)
{
	u8 damageId = pPed->GetDamageSetID();
	if (damageId == kInvalidPedDamageSet)
	{
		// need to allocate one
		damageId = CPedDamageManager::GetInstance().AllocateDamageSet(pPed);
		pPed->SetDamageSetID(damageId);
	}
#if __DEV
	if (damageId == kInvalidPedDamageSet)
		Warningf("AllocateDamageSet() faild to AllocateDamageSet of Ped 0x%p",pPed);
#endif
	return damageId;
}

CPedBloodDamageInfo * CPedDamageManager::GetBloodInfoAndDamageId(CPed * pPed, atHashWithStringBank bloodNameHash, u8 & damageId)
{
	if (pPed!=NULL)
	{
		if (CPedBloodDamageInfo * pBloodInfo = GetDamageData().GetBloodDamageInfoByHash(bloodNameHash))
		{
			const grcTexture* pSoakTexture = pBloodInfo->m_SoakTexture.m_pTexture; 
			const grcTexture* pWoundTexture = pBloodInfo->m_WoundTexture.m_pTexture; 
			const grcTexture* pSplatterTexture = pBloodInfo->m_SplatterTexture.m_pTexture; 

			if (pSoakTexture || pWoundTexture || pSplatterTexture || pBloodInfo->m_ScarStartTime==0.0f) // don't add if there is not valid texture for this type (or it an instant scar placeholder)
			{	
				damageId = GetDamageID(pPed);
				if (damageId != kInvalidPedDamageSet)
					return pBloodInfo;
			}
		}
	}

	PEDDEBUG_DISPLAYF("CPedDamageManager::GetBloodInfoAndDamageId: returns NULL for ped 0x%p(%s), BloodName = %s", pPed,GetPedName(pPed), bloodNameHash.GetCStr());

	return NULL;
}


CPedDamageDecalInfo * CPedDamageManager::GetDamageDecalAndDamageId(CPed * pPed, atHashWithStringBank scarNameHash, u8 & damageId)
{
	if (pPed!=NULL)
	{
		if (CPedDamageDecalInfo * pScarInfo = GetDamageData().GetDamageDecalInfoByHash(scarNameHash))
		{
			const grcTexture* pScarTexture = pScarInfo->m_Texture.m_pTexture; 
			if (pScarTexture) // don't add if there is not valid texture for this type.
			{	
				damageId = GetDamageID(pPed);
				if (damageId != kInvalidPedDamageSet)
					return pScarInfo;
			}
		}
	}

	PEDDEBUG_DISPLAYF("CPedDamageManager::GetDamageDecalAndDamageId: returns NULL for ped 0x%p(%s), damageName = %s", pPed,GetPedName(pPed), scarNameHash.GetCStr());

	return NULL;
}




bool CPedDamageManager::IsPedStanding(const CPed * pPed, bool bForceNotStanding)
{
	bool bIsStanding = (pPed->IsDead() == false) && !bForceNotStanding;

	// only check for human peds
	if (pPed->CanCheckIsProne()) 
	{
		bIsStanding = bIsStanding && !pPed->IsProne();
	}
	return bIsStanding;
}


// check add blood to a ped (allocates damage target if needed)
void CPedDamageManager::AddPedBlood( CPed * pPed, u16 component, const Vector3 & pos, atHashWithStringBank bloodNameHash, bool forceNotStanding)
{
	u8 damageId;
	if (CPedBloodDamageInfo * pBloodInfo = GetBloodInfoAndDamageId(pPed, bloodNameHash, damageId))
	{
		AddBloodDamage( damageId, component, pos, Vector3(0.0f,0.0f,-1.0f), kDamageTypeBlood, pBloodInfo, forceNotStanding);
	}
}

// simplified AddPedBlood() for scripts to place damage
void CPedDamageManager::AddPedBlood( CPed * pPed, ePedDamageZones zone, const Vector2 & uv, atHashWithStringBank bloodNameHash, bool forceNotStanding, float preAge, int forcedFrame)
{
	u8 damageId;
	if (CPedBloodDamageInfo * pBloodInfo = GetBloodInfoAndDamageId(pPed, bloodNameHash, damageId))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedBlood: ped=0x%p(%s), damageId=%d, zone=%d(%s), Name=%s, forceNotStanding=%s, preAge=%f, forcedFrame=%d", pPed, GetPedName(pPed), damageId, zone, s_ZoneEmumNames[zone], bloodNameHash.GetCStr(),forceNotStanding?"true":"false", preAge, forcedFrame);

		Vector2 uv2 = uv + ((pBloodInfo->m_RotationType == kRandomRotation) ? Vector2(s_Random.GetRanged(-0.1f, 0.1f),s_Random.GetRanged(-0.1f, 0.1f)) : Vector2(0.01f,0.0f));		
		float scale = s_Random.GetRanged(0.0, 1.0f);
	
		u8 flipUVflags = (u8)s_Random.GetRanged(0.0f, 4.0f); // The textures that are allowed to flip will flip together with this setting

		m_DamageSetPool[damageId]->AddDamageBlitByUVs(uv, uv2, zone, kDamageTypeBlood, pBloodInfo, false, IsPedStanding(pPed,forceNotStanding), scale, preAge, forcedFrame, flipUVflags);
	}
}

// simplified AddPedBlood() for scripts to place damage, this one takes a scale and rotation value
s32 CPedDamageManager::AddPedBlood( CPed * pPed, ePedDamageZones zone, const Vector2 & uv, float rotation, float scale, atHashWithStringBank bloodNameHash, bool forceNotStanding, float preAge, int forcedFrame, bool enableUVPush, u8 uvFlags)
{
	u8 damageId;
	if (CPedBloodDamageInfo * pBloodInfo = GetBloodInfoAndDamageId(pPed, bloodNameHash, damageId))
	{
		bool bIsStanding = IsPedStanding(pPed,forceNotStanding);

		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedBlood (w/scalerot): ped=0x%p(%s), damageId=%02x, zone=%d(%s), Name=%s, rotation=%f, scale=%f, forceNotStanding=%s, preAge=%f, forcedFrame=%d", pPed, GetPedName(pPed), damageId, zone,s_ZoneEmumNames[zone], bloodNameHash.GetCStr(),rotation, scale, forceNotStanding?"true":"false", preAge, forcedFrame);
		Vector2 newUV = uv;
		if (enableUVPush)
		{
			bool limitScale;
			float radius;
			if (pBloodInfo->m_SplatterTexture.m_pTexture == NULL) // if there is no splatter texture, use the wound size.
				radius = Lerp(scale, pBloodInfo->m_WoundMinSize, pBloodInfo->m_WoundMaxSize);
			else
				radius = Lerp(scale, pBloodInfo->m_SplatterMinSize, pBloodInfo->m_SplatterMaxSize);

			bool useGravitySoak = (pBloodInfo->m_RotationType == kGravityRotation) || (pBloodInfo->m_RotationType == kAutoDetectRotation && bIsStanding);
			
			float soakScale = Lerp(scale, pBloodInfo->m_SoakScaleMin, 1.0f); // lerp between min and 1.0 for soak, so it scales with the wound

			radius = Max(soakScale*(useGravitySoak ? pBloodInfo->m_SoakStartSizeGravity: pBloodInfo->m_SoakStartSize), radius)/100.0f;

			newUV = m_DamageSetPool[damageId]->GetZoneArray()[zone].AdjustForPushAreas(uv, radius/2.0f, limitScale, m_DamageSetPool[damageId]->HasDress(), NULL);
		}
	
		u8 flipUVflags = uvFlags != 255 ? uvFlags : (u8)s_Random.GetRanged(0.0f, 4.0f); // The textures that are allowed to flip will flip together with this setting

		Vector2 uv2 = newUV + Vector2(cosf(rotation*DtoR)/2.0f, sinf(rotation*DtoR)/2.0f);

		int damageBlitID = -1;
		if (Verifyf (scale>=0.0 && scale<=1.0f, "scale value %f is not between 0.0 and 1.0 (inclusive)",scale))
			damageBlitID = m_DamageSetPool[damageId]->AddDamageBlitByUVs(newUV, uv2, zone, kDamageTypeBlood, pBloodInfo, false, bIsStanding, scale, preAge, forcedFrame, flipUVflags);

#if GTA_REPLAY
		if(damageBlitID >= 0 && CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx(CPacketPedBloodDamageScript((u8)zone, uv, rotation, scale, bloodNameHash, preAge, forcedFrame, damageBlitID), pPed);
		}
#endif // GTA_REPLAY

		return damageBlitID;

	}

	return -1;
}

// check add stab blood to a ped (allocates damage target if needed)
void CPedDamageManager::AddPedStab( CPed * pPed, u16 component, const Vector3 & pos, const Vector3 & dir, atHashWithStringBank bloodNameHash)
{
	u8 damageId;
	if (CPedBloodDamageInfo * pBloodInfo = GetBloodInfoAndDamageId(pPed, bloodNameHash, damageId))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedStab: ped=0x%p(%s), damageId=%d, component=%d(%s), Name=%s", pPed, GetPedName(pPed), damageId, component, component<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[component] : "out of range", bloodNameHash.GetCStr());
		AddBloodDamage( damageId, component, pos, dir, kDamageTypeStab, pBloodInfo);
	}
}

// simplified AddPedStab() for scripts to place damage
void CPedDamageManager::AddPedStab( CPed * pPed, ePedDamageZones zone, const Vector2 & uv, const Vector2 & uv2, atHashWithStringBank bloodNameHash, float preAge, int forcedFrame)
{
	u8 damageId;
	if (CPedBloodDamageInfo * pBloodInfo = GetBloodInfoAndDamageId(pPed, bloodNameHash, damageId))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedStab: ped=0x%p(%s), damageId=%d, zone=%d(%s), Name=%s", pPed, GetPedName(pPed), damageId, zone, s_ZoneEmumNames[zone], bloodNameHash.GetCStr());
		
		float scale = uv.Dist(uv2);
		m_DamageSetPool[damageId]->AddDamageBlitByUVs(uv, uv2, zone, kDamageTypeBlood, pBloodInfo, false, false, scale, preAge, forcedFrame, 0x0);
	}
}

// check add scar to a ped by hashcode (scars are defined in the peddamge.xml file)
void CPedDamageManager::AddPedScar( CPed * pPed, ePedDamageZones zone, const Vector2 & uvPos, float rotation, float scale, atHashWithStringBank scarNameHash, bool fadeIn, int forceFrame, bool saveScar, float preAge, float alpha, u8 uvFlags)
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedScar: ped=0x%p(%s), scarNameHash=%s",pPed, GetPedName(pPed), scarNameHash.GetCStr());

	AddPedDamageDecal( pPed, zone, uvPos, rotation, scale, scarNameHash, fadeIn, forceFrame, alpha, preAge, uvFlags);

	if (saveScar)
	{
		StatsInterface::SavePedScarData( pPed );
	}
}

// add dirt by hash code...
void CPedDamageManager::AddPedDirt( CPed * pPed, ePedDamageZones zone, const Vector2 & uvPos, float rotation, float scale, atHashWithStringBank dirtNameHash, bool fadeIn, float alpha)
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedDirt: ped=0x%p(%s), dirtNameHash=%s", pPed, GetPedName(pPed), dirtNameHash.GetCStr());
	
	AddPedDamageDecal( pPed, zone, uvPos, rotation, scale, dirtNameHash, fadeIn, -1, alpha);
}

// called from script (could be dirt or scar or tattoo or ?
void CPedDamageManager::AddPedDamageDecal( CPed * pPed, ePedDamageZones zone, const Vector2 & uvPos, float rotation, float scale, atHashWithStringBank damageDecalNameHash, bool fadeIn, int forceFrame, float alpha, float preAge, u8 uvFlags)
{
	u8 damageId;

	if (CPedDamageDecalInfo * pDamageDecalInfo = PEDDAMAGEMANAGER.GetDamageDecalAndDamageId(pPed, damageDecalNameHash, damageId))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedDamageDecal: ped=0x%p(%s), damageId=%02x, zone=%d(%s), damageDecalNameHash=%s, uvPos=(%f,%f), rotation=%f, scale=%f, fadeIn=%s, forceFrame=%d, alpha=%f, preAge=%f, uvFlags=%u", pPed, GetPedName(pPed), damageId, zone, s_ZoneEmumNames[zone], damageDecalNameHash.GetCStr(), uvPos.x, uvPos.y,rotation, scale, fadeIn?"true":"false",forceFrame, alpha, preAge, uvFlags);
		
		if (alpha<0.0f) // -1 mean we get to pick from tuning values
			alpha = s_Random.GetRanged(pDamageDecalInfo->m_MinStartAlpha, pDamageDecalInfo->m_MaxStartAlpha);
		else if (alpha < 0.001f)
			return;		// don't waste time on invisible blits.
			
		ePedDamageTypes type;
		if (pDamageDecalInfo->m_CoverageType==kClothOnly)
			type = (pDamageDecalInfo->m_BumpTexture.m_pTexture!=NULL) ? kDamageTypeClothBumpDecoration:kDamageTypeClothDecoration;
		else
			type = (pDamageDecalInfo->m_BumpTexture.m_pTexture!=NULL) ? kDamageTypeSkinBumpDecoration:kDamageTypeSkinDecoration;  
		
		float size = Lerp(scale, pDamageDecalInfo->m_MinSize, pDamageDecalInfo->m_MaxSize)/100.0f; // divide by 100, so damage decals use the same xml tuning range as blood

		u8 flipUVflags = uvFlags != 255 ? uvFlags : (u8)s_Random.GetRanged(0.0f, 4.0f); // The textures that are allowed to flip will flip together with this setting

		atHashString invalidHash(0U);
		m_DamageSetPool[damageId]->AddDecorationBlit(invalidHash, invalidHash, NULL, NULL, zone, uvPos, rotation, Vector2(size,size), strStreamingObjectName(pDamageDecalInfo->m_Texture.m_TxdName.GetCStr()), pDamageDecalInfo->m_Texture.m_TextureName, type, alpha, pDamageDecalInfo, (fadeIn)?pDamageDecalInfo->GetFadeInTime():0.0f, pDamageDecalInfo->GetFadeOutStartTime(), pDamageDecalInfo->GetFadeOutTime(), preAge, forceFrame, flipUVflags);
	}
}

// this Version is meant to be called from bloodvfx. it also supports bruises as well as armor damage 
void CPedDamageManager::AddPedDamageVfx( CPed * pPed, u16 component, const Vector3 & pos, atHashWithStringBank damageNameHash, bool bReduceBleeding)
{
	u8 damageId;
	if (CPedBloodDamageInfo * pBloodInfo = GetBloodInfoAndDamageId(pPed, damageNameHash, damageId))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedDamageVfx (blood): ped=0x%p(%s), damageId=%d, component=%d(%s), damageDecalNameHash=%s", pPed, GetPedName(pPed), damageId, component, component<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[component] : "out of range", damageNameHash.GetCStr());

		// check to see if we're the player model, we may need to limit blood damage blits (B*1378283)
		if (pPed->IsLocalPlayer())
		{
			int maxBlood = (NetworkInterface::IsGameInProgress()) ? PEDDAMAGEMANAGER.GetDamageData().m_MaxPlayerBloodWoundsMP : PEDDAMAGEMANAGER.GetDamageData().m_MaxPlayerBloodWoundsSP;

			if (maxBlood==0) 
				return;

			if (maxBlood<0)	  // -1 means "no limit"
				maxBlood = CPedDamageSetBase::kMaxBloodDamageBlits;
			else if (damageNameHash==ATSTRINGHASH("ShotgunLarge",0x68f7998b) || damageNameHash==ATSTRINGHASH("ShotgunSmall",0xbdf00aa))
				maxBlood+=8;  // make sure we get separate shotgun hits in

			if (maxBlood>0)
			{
				int bloodAvailableCount = maxBlood;
				for (int i=0;i<m_DamageSetPool[damageId]->GetPedBloodDamageList().GetCount();i++)
				{
					if (!m_DamageSetPool[damageId]->GetPedBloodDamageList()[i].IsDone())
					{
						if (--bloodAvailableCount<=0)
							return; // no more available, just bail.
					}
				}
			}
		}		
		
#if GTA_REPLAY
		//Dont add blood damage here on replay playback as it should have been recorded.
		if( !CReplayMgr::IsEditModeActive() )
#endif //GTA_REPLAY
		{
			AddBloodDamage( damageId, component, pos, Vector3(0.0f,0.0f,-1.0f), kDamageTypeBlood, pBloodInfo, false, true, bReduceBleeding);
		}
	}
	else if (CPedDamageDecalInfo * pDamageDecalInfo = GetDamageDecalAndDamageId(pPed, damageNameHash, damageId))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedDamageVfx (decorations): ped=0x%p(%s), damageId=%d, component=%d(%s), damageDecalNameHash=%s", pPed, GetPedName(pPed), damageId, component, component<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[component] : "out of range", damageNameHash.GetCStr());

		// "scars" can also be bruises also bruises		
		Vec3V localPos = VECTOR3_TO_VEC3V(pos);
		Vec3V localDir = -Vec3V(V_Z_AXIS_WZERO);

		ePedDamageZones zone  = ComputeLocalPositionAndZone(damageId, component, localPos, localDir);

		if (zone!=kDamageZoneInvalid)
		{
			Vector2 uv,uv2;
			float scale;

			ePedDamageTypes type;
			if (pDamageDecalInfo->m_CoverageType==kClothOnly)
				type = (pDamageDecalInfo->m_BumpTexture.m_pTexture!=NULL)? kDamageTypeClothBumpDecoration:kDamageTypeClothDecoration;
			else
				type = (pDamageDecalInfo->m_BumpTexture.m_pTexture!=NULL)? kDamageTypeSkinBumpDecoration:kDamageTypeSkinDecoration;
			
			scale = s_Random.GetRanged(0.0,1.0f); // pick a size, we'll push to keep it in the zone.
			float size = Lerp(scale, pDamageDecalInfo->m_MinSize, pDamageDecalInfo->m_MaxSize) / 100.f; // divide by 100, so damage decals use the same xml tuning range as blood

			m_DamageSetPool[damageId]->CalcUVs(uv, uv2, VEC3V_TO_VECTOR3(localPos), VEC3V_TO_VECTOR3(localDir), zone, type, size/2.0f, kNoRotation, true);	

			float alpha = s_Random.GetRanged(pDamageDecalInfo->m_MinStartAlpha, pDamageDecalInfo->m_MaxStartAlpha);
			
			u8 flipUVflags = (u8)s_Random.GetRanged(0.0f, 4.0f); // The textures that are allowed to flip will flip together with this setting

			atHashString invalidHash(0U);
			m_DamageSetPool[damageId]->AddDecorationBlit(invalidHash, invalidHash, NULL, NULL, zone, uv, s_Random.GetRanged(0.0f, 360.f), Vector2(size,size), strStreamingObjectName(pDamageDecalInfo->m_Texture.m_TxdName.GetCStr()), pDamageDecalInfo->m_Texture.m_TextureName, type, alpha, pDamageDecalInfo, pDamageDecalInfo->GetFadeInTime(), pDamageDecalInfo->GetFadeOutStartTime(), pDamageDecalInfo->GetFadeOutTime(), 0.0f, -1, flipUVflags);		
		}
	}
}


void CPedDamageManager::AddPedDecoration( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const char* txdName, const char * txtName, bool isClothOnly, int crewEmblemVariation)
{
	strStreamingObjectName txdHash = strStreamingObjectName(txdName);
	strStreamingObjectName txtHash = strStreamingObjectName(txtName);

	AddPedDecoration( pPed, collectionHash, presetHash, zone, uvPos, rotation, scale, bRequiresTint, txdHash, txtHash, false, 1.0f, isClothOnly, crewEmblemVariation);
}



bool CPedDamageManager::GetClanIdForPed(const CPed* pPed, rlClanId& clanId)
{
	const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	clanId = RL_INVALID_CLAN_ID;

	// cloned ped with no player info, try figuring out its clan id...
	if (pPlayerInfo == NULL)
	{
		// ...from the currently selected player card if pause menu is open
		if(CPauseMenu::IsActive() && SPlayerCardManager::IsInstantiated())
		{
			clanId = SPlayerCardManager::GetInstance().GetClanId();
			PEDDEBUG_DISPLAYF("CPedDamageManager::GetClanIdForPed: ped=0x%p(%s) got crew emblem (%d) from player card manager", pPed, GetPedName(pPed), (s32)clanId);
		}

		if (clanId == RL_INVALID_CLAN_ID)
		{
			// The saved clan Id i set on player peds when they're created and remain on the ped after it has become a non player Ped.
			// We use it here to display the correct emblem on dead peds that used to be players.
			if(pPed->GetSavedClanId())
			{
				clanId = pPed->GetSavedClanId();
				PEDDEBUG_DISPLAYF("CPedDamageManager::GetClanIdForPed: ped=0x%p(%s) got crew emblem (%d) from previously saved ClanId", pPed, GetPedName(pPed), (s32)clanId);
			}
			// try the ped owner...
			if ((clanId == RL_INVALID_CLAN_ID) && pPed->GetNetworkObject() != NULL)
			{
				CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject());

				if (pNetObjPlayer->GetPlayerOwner() != NULL)
				{
					clanId = pNetObjPlayer->GetPlayerOwner()->GetClanDesc().m_Id;
					PEDDEBUG_DISPLAYF("CPedDamageManager::GetClanIdForPed: ped=0x%p(%s) got crew emblem (%d) from network object", pPed, GetPedName(pPed), (s32)clanId);
				}
			}
			// ...or fall back to local player clan id
			CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
			if ((clanId == RL_INVALID_CLAN_ID) && pLocalPlayerPed)
			{
				pPlayerInfo = pLocalPlayerPed->GetPlayerInfo();
				NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
				rlGamerHandle gamerHandle = pPlayerInfo->m_GamerInfo.GetGamerHandle();
				const rlClanDesc* pMyClan = clanMgr.GetPrimaryClan(gamerHandle);
				if (pMyClan)
					clanId = pMyClan->m_Id;
				PEDDEBUG_DISPLAYF("CPedDamageManager::GetClanIdForPed: ped=0x%p(%s) got crew emblem (%d) from local player", pPed, GetPedName(pPed), (s32)clanId);
			}
		}
	}
	// just grab it from the player info
	else
	{
		NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
		rlGamerHandle gamerHandle = pPlayerInfo->m_GamerInfo.GetGamerHandle();
		const rlClanDesc* pMyClan = clanMgr.GetPrimaryClan(gamerHandle);
		if (pMyClan)
			clanId = pMyClan->m_Id;

		PEDDEBUG_DISPLAYF("CPedDamageManager::GetClanIdForPed: ped=0x%p(%s) got crew emblem (%d) from player info", pPed, GetPedName(pPed), (s32)clanId);
	}

#if GTA_REPLAY
	//When playing back a replay we dont have any playerinfo for remote players
	//so we needed to store it when recording the replay.
	if( CReplayMgr::IsReplayInControlOfWorld() )
	{
		clanId = pPed->GetPedClanIdForReplay();
	}
#endif

	return clanId != RL_INVALID_CLAN_ID;

}

bool CPedDamageManager::GetTintInfo(const CPed* pPed, PedDamageDecorationTintInfo& tintInfo)
{
	// This should mean GetTintInfo has been called from a place where a ped instance cannot be accessed,
	// like a constructor for a damage blit
	if (pPed == NULL)
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::GetTintInfo: NULL ped (tintInfoValid: %d)", tintInfo.bValid);
		tintInfo.bReady = false;
		return tintInfo.bValid;
	}

	// We need script to have passed a ped with valid blend data, otherwise we shouldn't be adding a tinted decoration
	const CPedHeadBlendData* pCurrBlendData = pPed->GetExtensionList().GetExtension<CPedHeadBlendData>();
	tintInfo.bValid = (pCurrBlendData != NULL);
	Assertf(pCurrBlendData, "CPedDamageManager::GetTintInfo: ped=0x%p(%s),%s has NO blend data!", pPed, GetPedName(pPed), pPed && pPed->IsLocalPlayer()?" (localplayer),":"");
	
#if __BANK
	s32 lastCachedTintPalSel1 = -1;
	s32 lastCachedTintPalSel2 = -1;
#endif

	// Blend data is valid...
	if (tintInfo.bValid)
	{
		// Is the tint data ready?
		tintInfo.bReady = MESHBLENDMANAGER.IsHandleValid(pCurrBlendData->m_blendHandle) && MESHBLENDMANAGER.HasBlendFinished(pCurrBlendData->m_blendHandle);
		tintInfo.headBlendHandle = (u32)(pCurrBlendData->m_blendHandle);

		// If it's ready cache it, otherwise we'll keep checking later
		tintInfo.pPaletteTexture = MESHBLENDMANAGER.GetTintTexture(pCurrBlendData->m_blendHandle, RT_HAIR, tintInfo.paletteSelector, tintInfo.paletteSelector2);

		// Once it's ready, we also want to cache the last known palette indices, so when decorations are reapplied by script in specific edge cases
		// (e.g. browsing through items in clothes store) we don't end showing up a green scalp (i.e.: blend still pending) nor stop rendering the scalp underlay altogether,
		// especially since the hair color won't change in these cases.
		if (tintInfo.bReady)
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex<kMaxDamagedPeds)
			{
				PEDDAMAGEMANAGER.m_DamageSetPool[damageIndex]->SetLastValidTintPaletteSelector1(tintInfo.paletteSelector);
				PEDDAMAGEMANAGER.m_DamageSetPool[damageIndex]->SetLastValidTintPaletteSelector2(tintInfo.paletteSelector2);
			}
		}
	#if __BANK
		else
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex<kMaxDamagedPeds)
			{
				lastCachedTintPalSel1 = PEDDAMAGEMANAGER.m_DamageSetPool[damageIndex]->GetLastValidTintPaletteSelector1();
				lastCachedTintPalSel2 = PEDDAMAGEMANAGER.m_DamageSetPool[damageIndex]->GetLastValidTintPaletteSelector2();
			}
		}
	#endif
	}

	PEDDEBUG_DISPLAYF("CPedDamageManager::GetTintInfo: ped=0x%p(%s),%s BlendHandle: %u AddTintInfo: %d, Ready: %d paletteSelector: %d, paletteSelector2: %d, pPaletteTexture is %s (last cached sel1: %d, sel2: %d)", 
						pPed, GetPedName(pPed), pPed && pPed->IsLocalPlayer()?" (localplayer),":"", tintInfo.headBlendHandle,
						tintInfo.bValid, tintInfo.bReady, tintInfo.paletteSelector, tintInfo.paletteSelector2, tintInfo.pPaletteTexture != NULL ? "VALID" : "NULL", lastCachedTintPalSel1, lastCachedTintPalSel2);

	return tintInfo.bValid;
}

// version that just uses hash values for the txd and txt names (good for save game)
void CPedDamageManager::AddPedDecoration( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const strStreamingObjectName txdHash, const strStreamingObjectName txtHash, bool addToBumpMap, float alpha, bool isClothOnly, int crewEmblemVariation, CPedDamageDecalInfo * pSourceInfo, float fadeInTime, float fadeOutStart, float fadeOutTime, float preAge, int forcedFrame, u8 flipUVflags)
{
	if (Verifyf(pPed ,"Null Ped pointer"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedDecoration: ped=0x%p(%s),%s zone=%d(%s), collectionHash=%s, presetHash=%s", pPed, GetPedName(pPed), pPed && pPed->IsLocalPlayer()?" (localplayer),":"", zone, s_ZoneEmumNames[zone], collectionHash.TryGetCStr(), presetHash.TryGetCStr());

		rlClanId clanId = RL_INVALID_CLAN_ID;

		// if we need a crew emblem texture, the loading logic is slightly different
		bool bIsCrewEmblem = IsCrewEmblemTexture(txdHash);
		strLocalIndex txdSlot = strLocalIndex(-1);

		// if it's not a crew emblem texture, interface with the streaming system as usual		
		if (bIsCrewEmblem == false)
		{
			txdSlot = g_TxdStore.FindSlotFromHashKey(txdHash.GetHash());

			if (txdSlot == -1)
				txdSlot = g_TxdStore.AddSlot(txdHash.GetHash());
		}
		// otherwise, request the crew emblem texture
		else
		{
			if (txdHash != txtHash)
			{
				Warningf("CPedDamageManager::AddPedDecoration (failed to process crew emblem preset): txd and texture name mismatch");
				return;
			}

			if (crewEmblemVariation != EmblemDescriptor::INVALID_EMBLEM_ID)
			{
				EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_TOURNAMENT, (EmblemId) crewEmblemVariation);

				if (NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(emblemDesc  ASSERT_ONLY(, "CPedDamageManager")) == false)
				{
					Warningf("CPedDamageManager::AddPedDecoration (failed to process tournament emblem preset): request failed");
					return;
				}
			}
			else
			{
				CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
				bool bPedHadPlayerInfo = pPlayerInfo != NULL;
				GetClanIdForPed(pPed, clanId);

				if (clanId == RL_INVALID_CLAN_ID)
				{
					Warningf("CPedDamageManager::AddPedDecoration (failed to process crew emblem preset): no player info");
					return;
				}

				//cache off the collectionHash and the presetHash for the clan texture information so that if the texture cannot be applied because the clanid hasn't been received yet
				//it will be re-invoked when the clanid is received.  Check IsNetworkOpen rather than IsGameInProgress because this code is hit before the game match is in progress.
				if (bPedHadPlayerInfo && NetworkInterface::IsNetworkOpen() && pPed->IsAPlayerPed() && pPed->GetNetworkObject())
				{
					CNetObjPlayer* pNetObjPlayer = static_cast< CNetObjPlayer* >(pPed->GetNetworkObject());
					if (pNetObjPlayer)
						pNetObjPlayer->SetClanDecorationHashes(collectionHash, presetHash);			
				}

				// request the emblem texture
				EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId);

				if (NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(emblemDesc  ASSERT_ONLY(, "CPedDamageManager")) == false)
				{
					Warningf("CPedDamageManager::AddPedDecoration (failed to process crew emblem preset): request failed");
					return;
				}
			}
		}

		if (Verifyf(bIsCrewEmblem || g_TxdStore.IsValidSlot(txdSlot), "AddPedDecoration() - failed to get valid txd with name %s", txdHash.GetCStr()))
		{
			if (bIsCrewEmblem == false && !g_TxdStore.HasObjectLoaded(txdSlot)) // not streamed in yet, let's request it if we have not done so already
			{
				if (!g_TxdStore.IsObjectRequested(txdSlot) && !g_TxdStore.IsObjectLoading(txdSlot))
				{
					bool bRequestOk = g_TxdStore.StreamingRequest(txdSlot, STRFLAG_FORCE_LOAD);
					if (bRequestOk == false)
					{
						Warningf("CPedDamageManager::AddPedDecoration: streaming request for '%s' failed", txdHash.TryGetCStr());
					}
				}

			}

			// do we already have a damage ID?
			u8 damageId = pPed->GetDamageSetID();
			if (damageId == kInvalidPedDamageSet)
			{
				// nope, need to allocate one
				damageId = CPedDamageManager::GetInstance().AllocateDamageSet(pPed);
				pPed->SetDamageSetID(damageId);
			}

			if (damageId != kInvalidPedDamageSet)
			{ 
				ePedDamageTypes type;
			
				if (isClothOnly)
					type = (addToBumpMap) ? kDamageTypeClothBumpDecoration:kDamageTypeClothDecoration;
				else
					type = (addToBumpMap) ? kDamageTypeSkinBumpDecoration:kDamageTypeSkinDecoration;

				if (bIsCrewEmblem)
				{
					if (crewEmblemVariation != EmblemDescriptor::INVALID_EMBLEM_ID)
					{
						EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_TOURNAMENT, (EmblemId)crewEmblemVariation);
						m_DamageSetPool[damageId]->AddDecorationBlit( collectionHash, presetHash, NULL, &emblemDesc, zone, uvPos, rotation, scale, txdHash, txtHash, type, alpha, pSourceInfo, fadeInTime, fadeOutStart, fadeOutTime, preAge, forcedFrame, flipUVflags);
					}
					else
					{
						EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId);
						m_DamageSetPool[damageId]->AddDecorationBlit( collectionHash, presetHash, NULL, &emblemDesc, zone, uvPos, rotation, scale, txdHash, txtHash, type, alpha, pSourceInfo, fadeInTime, fadeOutStart, fadeOutTime, preAge, forcedFrame, flipUVflags);
					}
				}
				else
				{
					PedDamageDecorationTintInfo tintInfo;
					bool bAddTintInfo = bRequiresTint && GetTintInfo(pPed, tintInfo);
					m_DamageSetPool[damageId]->AddDecorationBlit( collectionHash, presetHash, (bAddTintInfo ? &tintInfo : NULL), NULL, zone, uvPos, rotation, scale, txdHash, txtHash, type, alpha, pSourceInfo, fadeInTime, fadeOutStart, fadeOutTime, preAge, forcedFrame, flipUVflags);
				}
			}
		}
	}
}

void CPedDamageManager::DumpDecorations( CPed* OUTPUT_ONLY(pPed) )
{
#if !__NO_OUTPUT
	if (pPed)
	{
		u8 damageId = pPed->GetDamageSetID();
		if (damageId != kInvalidPedDamageSet)
		{ 
			if (m_DamageSetPool[damageId])
			{
				const atFixedArray<CPedDamageBlitBlood, CPedDamageSetBase::kMaxBloodDamageBlits> & bloodList =  m_DamageSetPool[damageId]->GetPedBloodDamageList();
				if (bloodList.GetCount()>0)
				{
					Displayf("   blood decorations:");

					for (int i = 0;i<bloodList.GetCount();i++)
					{
						bloodList[i].DumpDecorations();
					}
				}
			}

			if (m_DamageSetPool[damageId])
				m_DamageSetPool[damageId]->DumpDecorations();
		}

		damageId = pPed->GetCompressedDamageSetID();
		if (damageId != kInvalidPedDamageSet)
		{
			if (m_CompressedDamageSetProxyPool[damageId] && m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet())
				m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->DumpDecorations();
		}

	}
#endif
}

void CPedDamageManager::AddPedDamagePack( CPed * pPed, atHashWithStringBank damagePackName, float preAge, float alpha)
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::AddPedDamagePack: ped=0x%p(%s), damagePackName=%s, PreAge=%f, alpha=%f", pPed, GetPedName(pPed), damagePackName.GetCStr(), preAge, alpha);
	
	// look for Damage Pack by name (there should only be a couple of these, if there get to be a lot, use an atMap or something.
	for (int i=0; i<m_DamageData.m_DamagePacks.GetCount(); i++)
	{
		if (m_DamageData.m_DamagePacks[i]->m_Name==damagePackName)
		{
			m_DamageData.m_DamagePacks[i]->Apply(pPed, preAge, alpha);
			return;
		}
	}

	Assertf(0,"Undefined ped Damage pack specified (%s)",damagePackName.GetCStr());
}


bool CPedDamageManager::CheckForCompressedPedDecoration( CPed * pPed, atHashString collectionHash, atHashString presetHashTxdName)
{
	if (pPed)
	{
		u8 damageId = pPed->GetCompressedDamageSetID();
		if (damageId != kInvalidPedDamageSet && m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet())	
		{
			const atVector<CPedDamageCompressedBlitDecoration> & list = m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->GetDecorationList();
			for (int i=0;i<list.GetCount();i++)
			{
				if (list[i].GetCollectionNameHash()==collectionHash && list[i].GetPresetNameHash()==presetHashTxdName)
					return true;
			}
		}
	}
	
	return false;
}

// adds a decoration to the ped. this used to be scars and tattoos, but it's really just tattoos now, though it could be bruises and other stuff too
bool CPedDamageManager::AddCompressedPedDecoration( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const char* inTxdName, const char * inTxtName, bool bApplyOnClothes, int crewEmblemVariation)
{
	strStreamingObjectName inTxdHash = strStreamingObjectName(inTxdName);
	strStreamingObjectName inTxtHash = strStreamingObjectName(inTxtName);

	return AddCompressedPedDecoration( pPed, collectionHash, presetHash, zone, uvPos, rotation, scale, bRequiresTint, inTxdHash, inTxtHash, bApplyOnClothes, crewEmblemVariation);
}

// version that just uses hash values for the txd and txt names (good for save game)
bool CPedDamageManager::AddCompressedPedDecoration( CPed * pPed, atHashString collectionHash, atHashString presetHash, ePedDamageZones zone, const Vector2 & uvPos, float rotation, const Vector2 &  scale, bool bRequiresTint, const strStreamingObjectName inTxdHash, const strStreamingObjectName inTxtHash, bool bApplyOnClothes, int crewEmblemVariation)
{
#if COMPRESSED_DECORATION_DEBUG
	//hack always apply over clothes for test purposes
	bApplyOnClothes = true;
#else
	if (!Verifyf(!pPed->IsLocalPlayer(), "local player is not allowed to have compressed Ped Decorations"))
		return false;
#endif

	u8 damageId = pPed->GetCompressedDamageSetID();
	atHashString outputTexHash;
	atHashString outputTxdHash;

	// if ped doesn't have a damage set assigned, allocate a new output texture
	if (damageId == kInvalidPedDamageSet)
	{
		if (PEDDECORATIONBUILDER.AllocOutputTexture(outputTxdHash, outputTexHash) == false)
		{
			CPEDDEBUG_DISPLAYF("CPedDamageManager::AddCompressedPedDecoration: FAILED to allocate damage set for ped=0x%p(%s),%s damageId=%d, zone=%d(%s), collectionHash=%s, presetHash=%s, inTxtHash=%s, inTxtHash=%s, outTxdHash=%s, outTxtHash=%s", pPed, GetPedName(pPed),  pPed && pPed->IsLocalPlayer()?" (localplayer),":"", damageId, zone, s_ZoneEmumNames[zone], collectionHash.TryGetCStr(), presetHash.TryGetCStr(), inTxtHash.TryGetCStr(), inTxtHash.TryGetCStr(), outputTxdHash.TryGetCStr(), outputTexHash.TryGetCStr());
			return false;
		}
	}
	else
	{
		if (AssertVerify(m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()))
		{
			outputTexHash = m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->GetOutputTextureHash();
			outputTxdHash = m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->GetOutputTxdHash();
		}
	}

	if (Verifyf(pPed ,"Null Ped pointer"))
	{
		CPEDDEBUG_DISPLAYF("CPedDamageManager::AddCompressedPedDecoration: ped=0x%p(%s),%s damageId=%d, zone=%d(%s), collectionHash=%s, presetHash=%s, inTxtHash=%s, inTxtHash=%s, outTxdHash=%s, outTxtHash=%s", pPed, GetPedName(pPed),  pPed && pPed->IsLocalPlayer()?" (localplayer),":"", damageId, zone, s_ZoneEmumNames[zone], collectionHash.TryGetCStr(), presetHash.TryGetCStr(), inTxtHash.TryGetCStr(), inTxtHash.TryGetCStr(), outputTxdHash.TryGetCStr(), outputTexHash.TryGetCStr());
		
		rlClanId clanId = RL_INVALID_CLAN_ID;

		strLocalIndex inTxdSlot = strLocalIndex(-1);
		strLocalIndex outTxdSlot = strLocalIndex(-1);
		
		// we only care about ref counting if we're not using uncompressed decoration textures
		if (PEDDECORATIONBUILDER.IsUsingUncompressedMPDecorations() == false)
			outTxdSlot = strLocalIndex(g_TxdStore.FindSlotFromHashKey(outputTxdHash.GetHash()));

		// if we need a crew emblem texture, the loading logic is slightly different
		bool bIsCrewEmblem = IsCrewEmblemTexture(inTxdHash);

		if (bIsCrewEmblem == false)
		{
			inTxdSlot = g_TxdStore.FindSlotFromHashKey(inTxdHash.GetHash());
			if (inTxdSlot == -1)
				inTxdSlot = g_TxdStore.AddSlot(inTxdHash.GetHash());
		}
		// otherwise, request the crew emblem texture
		else
		{
			if (crewEmblemVariation != EmblemDescriptor::INVALID_EMBLEM_ID)
			{
				EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_TOURNAMENT, (EmblemId)crewEmblemVariation);

				if (NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(emblemDesc  ASSERT_ONLY(, "CPedDamageManager")) == false)
				{
					Warningf("CPedDamageManager::AddPedDecoration (failed to process tournament emblem preset): request failed");
					goto failed_exit;
				}
			}
			else
			{
				if (inTxdHash != inTxtHash)
				{
					Warningf("CPedDamageManager::AddCompressedPedDecoration (failed to process crew emblem preset): txd and texture name mismatch");
					goto failed_exit;
				}

				//cache off the collectionHash and the presetHash for the clan texture information so that if the texture cannot be applied because the clanid hasn't been received yet
				//it will be re-invoked when the clanid is received.  Check IsNetworkOpen rather than IsGameInProgress because this code is hit before the game match is in progress.
				if (NetworkInterface::IsNetworkOpen() && pPed->IsAPlayerPed() && pPed->GetNetworkObject())
				{
					CNetObjPlayer* pNetObjPlayer = static_cast< CNetObjPlayer* >(pPed->GetNetworkObject());
					if (pNetObjPlayer)
						pNetObjPlayer->SetClanDecorationHashes(collectionHash, presetHash);			
				}

				GetClanIdForPed(pPed, clanId);
				if (clanId == RL_INVALID_CLAN_ID)
				{
					Warningf("CPedDamageManager::AddPedDecoration (failed to process crew emblem preset): no player info");
					goto failed_exit;
				}

				// request the emblem texture
				EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId);

				if (NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(emblemDesc  ASSERT_ONLY(, "CPedDamageManager")) == false)
				{
					Warningf("CPedDamageManager::AddCompressedPedDecoration (failed to process crew emblem preset): request failed");
					goto failed_exit;

				}
			}
		}

		// we only care about ref counting if we're not using uncompressed decoration textures
		if ((PEDDECORATIONBUILDER.IsUsingUncompressedMPDecorations() == false) && outTxdSlot == -1)
		{
			outTxdSlot = g_TxdStore.AddSlot(outputTxdHash.GetHash());
		}

		bool bIsValidOutputSlot = (PEDDECORATIONBUILDER.IsUsingUncompressedMPDecorations() == false) ? g_TxdStore.IsValidSlot(outTxdSlot) : true;

		if (Verifyf(bIsCrewEmblem || g_TxdStore.IsValidSlot(inTxdSlot), "AddCompressedPedDecoration() - failed to get valid txd with name %s", inTxdHash.GetCStr()) &&
			Verifyf(bIsValidOutputSlot, "AddCompressedPedDecoration() - failed to get valid txd with name %s", outputTexHash.GetCStr()))
		{
			if (bIsCrewEmblem == false && !g_TxdStore.HasObjectLoaded(inTxdSlot)) // not streamed in yet, let's request it if we have not done so already
			{
				if (!g_TxdStore.IsObjectRequested(inTxdSlot) && !g_TxdStore.IsObjectLoading(inTxdSlot))
				{
					bool bRequestOk = g_TxdStore.StreamingRequest(inTxdSlot, STRFLAG_FORCE_LOAD);
					if (bRequestOk == false)
					{
						Warningf("CPedDamageManager::AddCompressedPedDecoration: streaming request for '%s' failed (inTxdHash)", inTxdHash.TryGetCStr());
					}

				}
			}

			// we only care about ref counting and streaming if we're not using uncompressed decoration textures
			if ((PEDDECORATIONBUILDER.IsUsingUncompressedMPDecorations() == false) && !g_TxdStore.HasObjectLoaded(outTxdSlot)) // not streamed in yet, let's request it if we have not done so already
			{
				if (!g_TxdStore.IsObjectRequested(outTxdSlot) && !g_TxdStore.IsObjectLoading(outTxdSlot))
				{
					bool bRequestOk = g_TxdStore.StreamingRequest(outTxdSlot, STRFLAG_FORCE_LOAD);
					if (bRequestOk == false)
					{
						Warningf("CPedDamageManager::AddCompressedPedDecoration: streaming request for '%s' failed (outputTexHash)", outputTexHash.TryGetCStr());
					}
				}
			}

			// do we already have a damage ID?
			if (damageId == kInvalidPedDamageSet)
			{
				// nope, need to allocate one
				damageId = CPedDamageManager::GetInstance().AllocateCompressedDamageSetProxy(pPed, false);
				if (damageId == kInvalidPedDamageSet)
				{
					Displayf("CPedDamageManager::AddCompressedPedDecoration: no sets available");
					goto failed_exit;
				}

				pPed->SetCompressedDamageSetID(damageId);
			}

			ePedDamageTypes type = bApplyOnClothes ? kDamageTypeClothDecoration : kDamageTypeSkinDecoration;

			// if we got one, let's add this damage
			if (damageId != kInvalidPedDamageSet) 
			{
				bool bOk = m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->SetOutputTexture(outputTxdHash, outputTexHash);

				if (bOk)
				{
					if (bIsCrewEmblem)
					{
						if (crewEmblemVariation != EmblemDescriptor::INVALID_EMBLEM_ID)
						{
							EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_TOURNAMENT, (EmblemId) crewEmblemVariation);
							bOk = m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->AddDecorationBlit(collectionHash, presetHash, NULL, &emblemDesc, zone, uvPos, rotation, scale, inTxdHash, inTxtHash, type);
						}
						else
						{
							EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId) clanId);
							bOk = m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->AddDecorationBlit(collectionHash, presetHash, NULL, &emblemDesc, zone, uvPos, rotation, scale, inTxdHash, inTxtHash, type);
						}
					}
					else
					{
						PedDamageDecorationTintInfo tintInfo;
						bool bAddTintInfo = bRequiresTint && GetTintInfo(pPed, tintInfo);
						bOk = m_CompressedDamageSetProxyPool[damageId]->GetCompressedTextureSet()->AddDecorationBlit(collectionHash, presetHash, (bAddTintInfo ? &tintInfo : NULL), NULL, zone, uvPos, rotation, scale, inTxdHash, inTxtHash, type);
					}
				}
				return bOk;
			}
		}
	}

failed_exit:
	if (damageId == kInvalidPedDamageSet)
		PEDDECORATIONBUILDER.ReleaseOutputTexture(outputTexHash);  // release this if we allocated, but did not end up using it.

	return false;
}

void CPedDamageManager::ReleaseDamageSet(u8 damageIndex)
{

	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ReleaseDamageSet: ped=0x%p(%s), damageIndex=%d", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex);
		m_DamageSetPool[damageIndex]->ReleasePed(false);
	}
}

void CPedDamageManager::ReleaseCompressedDamageSet(u8 damageIndex)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!")) 
	{
		CPEDDEBUG_DISPLAYF("CPedDamageManager::ReleaseCompressedDamageSet: ped=0x%p(%s), damageIndex=%d", (damageIndex<kMaxDamagedPeds) && (m_CompressedDamageSetProxyPool[damageIndex]) ? m_CompressedDamageSetProxyPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_CompressedDamageSetProxyPool[damageIndex]) ? GetPedName(m_CompressedDamageSetProxyPool[damageIndex]->GetPed()) : "", damageIndex);
		m_CompressedDamageSetProxyPool[damageIndex]->ReleasePed();
	}
}

bool CPedDamageManager::HasDecorations(const CPed* pPed) const
{

	// try local damage set first...
	const CPedDamageSet* pDmgSet = GetDamageSet(pPed);
	if (pDmgSet)
		return pDmgSet->HasDecorations();

	// ...check for a compressed one
	const CCompressedPedDamageSet* pCmpDmgSet = GetCompressedDamageSet(pPed);
	if (pCmpDmgSet)
		return pCmpDmgSet->HasDecorations();


	return false;
}

void CPedDamageManager::GetRenderData(u8 damageIndex, u8 compressedDamageIndex, grcTexture * &bloodTarget, grcTexture * &tattooTarget, Vector4& data)
{
	Assert(!m_NeedsSorting);

	bloodTarget = NULL;
	tattooTarget = NULL;
	
	bool hasMedals = false;
	bool isCompressedTattooTarget=false;
	bool skipCompressedCheck = false;

	if (damageIndex!=kInvalidPedDamageSet)
	{
		if (Verifyf(damageIndex<kMaxDamagedPeds, "GetRenderData() passed an invalid damageIndex (%d)", damageIndex))
		{
			if (m_DamageSetPool[damageIndex]->GetLastVisibleFrame()==sm_FrameID)	// if they were not visible last frame, they may not be in the active list, so their targets might be old
			{
				grcTexture* pBloodTarget = NULL;
				grcTexture* pTattooTarget = NULL;
				m_DamageSetPool[damageIndex]->GetRenderTargets(pBloodTarget,pTattooTarget);
				bloodTarget = pBloodTarget;
				tattooTarget = pTattooTarget;
				skipCompressedCheck = pTattooTarget != NULL && m_DamageSetPool[damageIndex]->GetForcedUncompressedDecorations();
			}
			hasMedals = m_DamageSetPool[damageIndex]->HasMedals();
		}
	}


	if (skipCompressedCheck == false && compressedDamageIndex != kInvalidPedDamageSet) 
	{
		if (Verifyf(compressedDamageIndex<kMaxDamagedPeds, "GetRenderData() passed an invalid compressedDamageIndex (%d)", compressedDamageIndex))
		{
			if (AssertVerify(m_CompressedDamageSetProxyPool[compressedDamageIndex]->GetCompressedTextureSet()))
			{	
				tattooTarget = m_CompressedDamageSetProxyPool[compressedDamageIndex]->GetCompressedTextureSet()->GetTexture();
				hasMedals = m_CompressedDamageSetProxyPool[compressedDamageIndex]->GetCompressedTextureSet()->HasMedals();
				isCompressedTattooTarget = true;
			}
		}
	}

	if (bloodTarget==NULL && tattooTarget==NULL )  // no blood or tattoos, just disable it
	{
		data.Set(0.0f,0.0f,0.0f,0.0f);
	}
	else
	{
		int width = (bloodTarget) ? bloodTarget->GetWidth() : tattooTarget->GetWidth();
		int height = (bloodTarget) ? bloodTarget->GetHeight() : tattooTarget->GetHeight();
		float bloodSize = (float)Min(width,height);																	// blood "width" (or tattoo if no blood target)
		float tattooSize = (tattooTarget) ? Min(tattooTarget->GetWidth(),tattooTarget->GetHeight()) : bloodSize;	// tattoo "width" (or blood if no tattoo target)
		float bumpScale = bloodSize/128.0f;

		bool bloodNeedsRotate = width>height; // if there is not blood texture, we still might need the dual flag for mirror, so the tattoo rotate flag will do
		bool tattooNeedsRotate = (tattooTarget) ? tattooTarget->GetWidth()>tattooTarget->GetHeight() : false;
		bool dualTarget = IsDualTarget((bloodTarget) ? bloodTarget : tattooTarget);
		
		float rotateFlags = bloodNeedsRotate?(dualTarget?0.75f:0.5f):0.0f; // set rotate flag if sideways.
		if (tattooNeedsRotate) // sometimes the tattoo is vertical (compressed textures) while the blood is horizontal. the shader needs to know that.
			rotateFlags += 1.0f;    

		u16 bloodMask = 0;
		if (damageIndex!=kInvalidPedDamageSet)
			bloodMask |= m_DamageSetPool[damageIndex]->CalcComponentBloodMask();
		if (isCompressedTattooTarget)
			bloodMask |= m_CompressedDamageSetProxyPool[compressedDamageIndex]->GetCompressedTextureSet()->CalcComponentMask();

		// TODO: we need to determine the distance at which we can turn off the normal maps...
		// Flip data.x to -1 if we use medals (i.e. don't use the decoration target for normal decoration
		data.Set(static_cast<float>(bloodMask) * (hasMedals?-1.0f:1.0f), m_DamageData.m_WoundBumpAdjust*bumpScale/2.0f, rotateFlags, tattooSize + 1.0f/bloodSize); // blood on, normals on (and bump scale), debugMode, texture size.
	}

	if (bloodTarget==NULL)
		bloodTarget = m_NoBloodTex;
	
	if (tattooTarget==NULL)
		tattooTarget = m_NoTattooTex;
}


void CPedDamageManager::PreDrawStateSetup()
{
	CPedDamageSetBase::ResetBloodBlitShaderVars();
	CPedDamageSetBase::ResetDecorationBlitShaderVars();
	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull,grcStateBlock::DSS_IgnoreDepth,grcStateBlock::BS_Normal);
}


void CPedDamageManager::PostDrawStateSetup()
{
	grcStateBlock::SetStates(grcStateBlock::RS_Default,CShaderLib::DSS_Default_Invert,grcStateBlock::BS_Default);
	grcBindTexture( NULL );
}


void CPedDamageManager::StartRenderList()
{	
	GetInstance().CheckStreaming();
	GetInstance().CheckPlayerForScars();
	GetInstance().CheckCompressedSets();

#if __BANK
	GetInstance().UpdateDebugMarker();
	GetInstance().CheckDebugPed();
	GetInstance().ReloadUpdate();
#endif
	// update the colors
	// these should be hard coded in the shader at some point
	const CPedDamageData & data = GetInstance().m_DamageData;

	GetInstance().m_PedDamageColors[0].Set(data.m_BloodColor.x,	data.m_BloodColor.y,	data.m_BloodColor.z,data.m_BloodSpecExponent);
	GetInstance().m_PedDamageColors[1].Set(data.m_SoakColor.x,	data.m_SoakColor.y,		data.m_SoakColor.z, data.m_BloodSpecIntensity);
	GetInstance().m_PedDamageColors[2].Set(data.m_BloodFresnel, data.m_BloodReflection, 0.0f,	0.0f );

	sm_FrameID++;

	GetInstance().m_NeedsSorting = true;
	GetInstance().m_ActiveDamageSets.SetCount(0);
}

void CPedDamageManager::PostPreRender()
{
	if (GetInstance().m_NeedsSorting)	
		GetInstance().SortPedDamageSets();

	float lastTimeStamp = sm_TimeStamp;
	sm_TimeStamp = TIME.GetElapsedTime();

	// if in a cutscene, update freeze all blood fading
	if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) BANK_ONLY(|| s_TestFreezeFade) )
	{
		for (int i=0;i< GetInstance().m_DamageSetPool.GetMaxCount();i++)
		{
			GetInstance().m_DamageSetPool[i]->FreezeBloodFade(sm_TimeStamp,lastTimeStamp-sm_TimeStamp);
		}
	}
}

// look through players blood damage to see if we need to add any scaring
void CPedDamageManager::CheckPlayerForScars()
{
	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (m_DamageSetPool[i]->IsForPlayer())
		{
			m_DamageSetPool[i]->UpdateScars();
		}
	}
}

void CPedDamageManager::CheckCompressedSets()
{
	for (int i=0;i< m_CompressedDamageSetProxyPool.GetMaxCount();i++)
	{
		CCompressedPedDamageSet* pDamageSet = m_CompressedDamageSetProxyPool[i]->GetCompressedTextureSet();
		if (pDamageSet)
			PEDDECORATIONBUILDER.Add(pDamageSet);
	}

	PEDDECORATIONBUILDER.UpdateMain();
}


void CPedDamageManager::AddToDrawList(bool bMainDrawList, bool bMirrorMode)
{
	if (!GetInstance().m_NeedsSorting) 
	{
		DLCPushTimebar("Damage Targets");
		DLC_Add(CPedDamageManager::PreDrawStateSetup);
#if __BANK	
		if (bMainDrawList)
			DLC_Add(CPedDamageManager::SaveDebugTargets, (grcTexture*)NULL, (grcTexture*)NULL);
#endif

		for (int i=0; i< GetInstance().m_ActiveDamageSets.GetCount();i++)
		{
#if __BANK
			bool debugFlag = (GetInstance().m_DamageSetPool[s_DebugTargetIndex]==GetInstance().m_ActiveDamageSets[i]);
#endif
			if (GetInstance().m_ActiveDamageSets[i]->GetPedBloodDamageList().GetCount()>0 ||
				GetInstance().m_ActiveDamageSets[i]->GetDecorationList().GetCount()>0)
			{
				if (bMirrorMode) // in mirror mode, we only put the local player in
				{ 
					if (const CPed * pPed = GetInstance().m_ActiveDamageSets[i]->GetPed())
					{
						if (!pPed->IsLocalPlayer() && !GetInstance().m_ActiveDamageSets[i]->WasClonedFromLocalPlayer())
							continue;
					}
				}
				
				DLC(dlCmdRenderPedDamageSet,(GetInstance().m_ActiveDamageSets[i], BANK_SWITCH(debugFlag,false), bMirrorMode));
			}
		}

		DLC_Add(CPedDamageManager::PostDrawStateSetup);
		DLCPopTimebar();
	}

	if (bMainDrawList || !bMirrorMode)  // we can do this in the UI mode too, since some times the buffer is frozen while in the UI
	{
		PEDDECORATIONBUILDER.AddToDrawList();
	}
}


int TargetCompare( CPedDamageSet* const* A,  CPedDamageSet* const* B)
{
	// TODO: make the player characters always closer
	float diff = (*A)->GetDistanceToCamera() - (*B)->GetDistanceToCamera();
	return (diff < 0.0f) ? -1 : ((diff >0.0f) ? 1: 0);
}


void CPedDamageManager::SortPedDamageSets()
{
	m_NeedsSorting = false;
	
	if (m_ActiveDamageSets.GetCount() <= 0)
		return;

	m_ActiveDamageSets.QSort(0, -1, TargetCompare);
	int useableActiveSets = 0;

	int i;		
	bool processed[kMaxDamagedPeds];

	for (i=0; i<m_ActiveDamageSets.GetCount(); i++)
	{
		processed[i] = false;								// use a separate flag to indicate processed, incase we ever get a NULL target
		m_ActiveDamageSets[i]->SetRenderTargets(-1);		// Clear the textures
	}

	int activeCount = Min(m_ActiveDamageSets.GetCount(), m_DamageData.m_MaxTotalTargetsPerFrame);  // limit to  the max number of damage set
 
	float lodScale = CalcLodScale();

	// figure out how many of each type of target we need
	int damageTargetsNeeded[3];
	int decorationTargetsNeeded[3];
	for (i=0;i<3;i++) damageTargetsNeeded[i]=decorationTargetsNeeded[i]=0;
	
	for (i=0; i<activeCount; i++)
	{
		int lod = 2;
		if (m_ActiveDamageSets[i]->GetDistanceToCamera() < m_DamageData.m_HiResTargetDistanceCutoff*lodScale)
			lod = 0;
		else if (m_ActiveDamageSets[i]->GetDistanceToCamera() < m_DamageData.m_MedResTargetDistanceCutoff*lodScale)
			lod = 1;

		damageTargetsNeeded[lod]++;
		if (m_ActiveDamageSets[i]->NeedsDecorationTarget())
			decorationTargetsNeeded[lod]++;
	}

	// assign render targets. 
	int targetIndex = 0;

	//  add the hires targets in two passes, first look for set that need both blood and decorations (ps3 hires target is 2 side by side,so these need to go first)
	for (int pass = 0; pass<2; pass++)
	{
		for (i=0; i<activeCount; i++)
		{
			if (targetIndex>=kMaxHiResBloodRenderTargets || m_ActiveDamageSets[i]->GetDistanceToCamera() > m_DamageData.m_HiResTargetDistanceCutoff*lodScale)  // stop doing hires if we're far enough away
				break;
		
			if ( processed[i]==false )
			{
				int targetsNeeded = m_ActiveDamageSets[i]->NeedsDecorationTarget()?2:1;
				
				if (targetsNeeded == 2-pass) // just look at the one that need 2 or 1 depending on the pass
				{
					if (targetIndex+targetsNeeded<=kMaxMedResBloodRenderTargets) // make sure there is room for the pair
					{	
						processed[i] = true;
						useableActiveSets++;
						m_ActiveDamageSets[i]->SetRenderTargets(targetIndex,kHiResDamageTarget);
						targetIndex+=targetsNeeded;
						damageTargetsNeeded[0]--;
						decorationTargetsNeeded[0] -= (targetsNeeded-1);// first pass is sets with decorations 
					}
				}
			}
		}
	}
	
	damageTargetsNeeded[1] += damageTargetsNeeded[0];			// if we did not get them as high, we need them as medium
	decorationTargetsNeeded[1] += decorationTargetsNeeded[0];  

	// Now use up as many medium res with decorations
	targetIndex *= kMedResTargetsPerHighResTarget;

	// if we have more targets than we have Medium LOD slots for, we need to change some to use as low res.
	int medResTargetsNeeded =  damageTargetsNeeded[1]+ decorationTargetsNeeded[1];
	int medResTargetsAvailable = kMaxMedResBloodRenderTargets;
	if (medResTargetsNeeded>medResTargetsAvailable) // reduce available, so some will get demoted, but at least they will render
		medResTargetsAvailable -= (int)ceilf((medResTargetsNeeded-medResTargetsAvailable)/float(kLowResTargetsPerMedResTarget-1));

	for (i=0; i<activeCount; i++) 
	{
		if (targetIndex>=kMaxMedResBloodRenderTargets || useableActiveSets >= m_DamageData.m_MaxMedResTargetsPerFrame  || m_ActiveDamageSets[i]->GetDistanceToCamera() > m_DamageData.m_MedResTargetDistanceCutoff*lodScale)  // stop doing hires if we're far enough away
			break;
	
		if ( processed[i]==false )
		{
			int targetsNeeded = m_ActiveDamageSets[i]->NeedsDecorationTarget()?2:1;

			if (targetIndex+targetsNeeded<=kMaxMedResBloodRenderTargets) // make sure there is room for the pair
			{	
				processed[i] = true;
				useableActiveSets++;
				m_ActiveDamageSets[i]->SetRenderTargets(targetIndex,kMedResDamageTarget);
				targetIndex+=targetsNeeded;
				damageTargetsNeeded[1]--;
				decorationTargetsNeeded[1] -= targetsNeeded-1;// first pass is sets with decorations 
			}
		}
	}

	damageTargetsNeeded[2] += damageTargetsNeeded[1];			// if we did not get them as high, we need them as medium
	decorationTargetsNeeded[2] += decorationTargetsNeeded[1];  
	targetIndex *= kLowResTargetsPerMedResTarget;  // currently the low res target overlap, so we need to skip some

	// now do the low res with decorations
	for (i=0; i<activeCount; i++) 
	{
		if (targetIndex>=kMaxLowResBloodRenderTargets || useableActiveSets >= m_DamageData.m_MaxTotalTargetsPerFrame  || m_ActiveDamageSets[i]->GetDistanceToCamera() > m_DamageData.m_LowResTargetDistanceCutoff)  
			break;
	
		if ( processed[i]==false )
		{
			int targetsNeeded = m_ActiveDamageSets[i]->NeedsDecorationTarget()?2:1;

			if (targetIndex+targetsNeeded<=kMaxLowResBloodRenderTargets) // make sure there is room for the pair
			{	
				processed[i] = true;
				useableActiveSets++;
				m_ActiveDamageSets[i]->SetRenderTargets(targetIndex,kLowResDamageTarget);
				targetIndex+=targetsNeeded;
				damageTargetsNeeded[2]--;
				decorationTargetsNeeded[2] -= targetsNeeded-1;// first pass is sets with decorations 
			}
		}
	}

	if (Verifyf(useableActiveSets <= m_ActiveDamageSets.GetCount(), "CPedDamageManager::SortPedDamageSets: active set count (%d), usable count (%d)", m_ActiveDamageSets.GetCount(), useableActiveSets))
	{
		m_ActiveDamageSets.SetCount(useableActiveSets);
	}
	else
	{
		// Should never hit this
		m_ActiveDamageSets.SetCount(0);
	}
}

bool CPedDamageSet::IsForPlayer() const	
{
	return m_pPed && m_pPed->GetPlayerInfo(); 
}

void CPedDamageManager::ClearPedBlood(u8 damageIndex)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ClearPedBlood: ped=0x%p(%s), damageId=%d", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex);

		m_DamageSetPool[damageIndex]->ClearDamage();
		if (!m_DamageSetPool[damageIndex]->HasDecorations()) // if nothing is left, release the set all together, otherwise we keep it connected
		{
			m_DamageSetPool[damageIndex]->ReleasePed(false);
		}
	}
}

#if GTA_REPLAY
void CPedDamageManager::ReplayClearPedBlood(u8 damageIndex, u32 bloodBlitID)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ReplayClearPedBlood: ped=0x%p(%s), damageId=%d", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex);

		m_DamageSetPool[damageIndex]->ReplayClearPedBlood(bloodBlitID);
		if (!m_DamageSetPool[damageIndex]->HasDecorations()) // if nothing is left, release the set all together, otherwise we keep it connected
		{
			m_DamageSetPool[damageIndex]->ReleasePed(false);
		}
	}

}

bool CPedDamageManager::ReplayIsBlitIdValid(u8 damageIndex, u32 bloodBlitID)
{
	bool valid = true;
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		valid = m_DamageSetPool[damageIndex]->ReplayIsBlitIdValid(bloodBlitID);
	}
	return valid;
}
#endif //GTA_REPLAY

// this should only be for the player (normal peds don't so scars)
// find all the blood and move it to it's scar phase, if blood does not have a scare associated, just delete it
void CPedDamageManager::PromoteBloodToScars(u8 damageIndex, bool bScarOverride, float scarU, float scarV, float scarScale)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't Promote blood for kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::PromoteBloodToScars: ped=0x%p(%s), damageId=%d", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex);
	
		// also clear the armour bullet impacts (they are like blood, but on the armor, so they are damage decals, and do not leave scars)
		// Since this is how the game clears the ped damage when you die, if we don't remove them too, you get old bullet decals on you new cloths.
		m_DamageSetPool[damageIndex]->ClearDamageDecals(kDamageZoneInvalid,ATSTRINGHASH("ArmorBullet",0x5e3ff621));

		if (!m_DamageSetPool[damageIndex]->PromoteBloodToScars(bScarOverride, scarU, scarV, scarScale)) // if we did not add any scars, we can delete release the set
		{
			if (!m_DamageSetPool[damageIndex]->HasDecorations())  // if nothing is left, release the set all together, otherwise we keep it connected
			{
				m_DamageSetPool[damageIndex]->ReleasePed(false);
			}
		}
	}
}

void CPedDamageManager::ClearPedBlood(u8 damageIndex, ePedDamageZones zone)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ClearPedBlood: ped=0x%p(%s), damageId=%d, zone=%d(%s)", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex, zone, s_ZoneEmumNames[zone]);

		m_DamageSetPool[damageIndex]->ClearDamage(zone);
		if (!m_DamageSetPool[damageIndex]->HasDecorations() && !m_DamageSetPool[damageIndex]->HasBlood() && !m_DamageSetPool[damageIndex]->WasClonedFromLocalPlayer())  // if nothing is left, release the set all together, otherwise we keep it connected, unless the set was cloned from the local player (we need to remember that or we will not draw in mirrors)
		{
			m_DamageSetPool[damageIndex]->ReleasePed(false);
		}
	}
}

void CPedDamageManager::LimitZoneBlood(u8 damageIndex, ePedDamageZones zone, int limit)
{
	if (damageIndex != kInvalidPedDamageSet)
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::LimitZoneBlood: ped=0x%p(%s), damageId=%d, zone=%d(%s), limit=%d", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex, zone, s_ZoneEmumNames[zone], limit);	

		m_DamageSetPool[damageIndex]->LimitZoneBlood(zone,limit);
	}
}

void CPedDamageManager::ClearDamageDecals(u8 damageIndex, ePedDamageZones zone, atHashWithStringBank damageDecalNameHash)
{
	if (damageIndex != kInvalidPedDamageSet)
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ClearDamageDecals: ped=0x%p(%s), damageId=%d, zone=%d(%s), decal name=%s"
			, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0
			, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) && m_DamageSetPool[damageIndex]->GetPed() ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : ""
			, damageIndex
			, zone
			, (zone >= kDamageZoneTorso) && (zone < kDamageZoneMedals) ? s_ZoneEmumNames[zone] : ""
			, damageDecalNameHash.IsNotNull() ? damageDecalNameHash.GetCStr() : "");

		m_DamageSetPool[damageIndex]->ClearDamageDecals(zone, damageDecalNameHash);
	}
}

void CPedDamageManager::SetPedBloodClearInfo(u8 damageIndex, float redIntensity, float alphaIntensity)
{
	if (damageIndex != kInvalidPedDamageSet)
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::SetPedBloodClearInfo: ped=0x%p(%s), damageId=%d, redIntensity=%f, alphaIntensity=%f", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex, redIntensity, alphaIntensity);	

		m_DamageSetPool[damageIndex]->SetClearColor(redIntensity<0.0f?0.0f:redIntensity, alphaIntensity<0.0f?0.0f:alphaIntensity);
	}
}

void CPedDamageManager::ClonePedDamage(CPed *pPed, const CPed *source, bool bCloneCompressedDamage)
{
	if (pPed && source)
	{
		
		u8 srcDamageIndex = source->GetDamageSetID();
		if (srcDamageIndex != kInvalidPedDamageSet) // only clone damage if there is any
		{					
			PEDDEBUG_DISPLAYF("CPedDamageManager::ClonePedDamage: ped=0x%p(%s), src=0x%p(%s)", pPed, GetPedName(pPed), source, GetPedName(source));
			// lock the source damage (so the new allocation does not steal the source before it's cloned)
			bool wasLocked = m_DamageSetPool[srcDamageIndex]->Lock();
			
			// allocate new damage
			u8 dstDamageIndex = PEDDAMAGEMANAGER.GetDamageID(pPed); 
			if(Verifyf(dstDamageIndex != kInvalidPedDamageSet, "Could not allocate a ped damage set for the clone"))
			{
				m_DamageSetPool[dstDamageIndex]->Clone(pPed, *m_DamageSetPool[srcDamageIndex]);
				m_DamageSetPool[dstDamageIndex]->SetWasClonedFromLocalPlayer( source->IsLocalPlayer() || m_DamageSetPool[srcDamageIndex]->WasClonedFromLocalPlayer());
			}
			
			// unlock the source damage
			if (wasLocked)
				m_DamageSetPool[srcDamageIndex]->UnLock();
		}

		// do the compressed textures too if needed
		if(bCloneCompressedDamage)
		{
			// clear out the old damage
			srcDamageIndex = source->GetCompressedDamageSetID();
			if (srcDamageIndex != kInvalidPedDamageSet) // only clone damage if there is any
			{				
				CPEDDEBUG_DISPLAYF("CPedDamageManager::ClonePedDamage: ped=0x%p(%s), src=0x%p(%s). ped is%s local", pPed, GetPedName(pPed), source, GetPedName(source), pPed && pPed->IsLocalPlayer()?"":" not");
			
				if(Verifyf(!pPed->IsLocalPlayer(),"cloning a ped with compressed textures to the local ped")) // don't clone compressed damage to the local ped
				{
					// allocate new damage
					u8 dstDamageIndex = AllocateCompressedDamageSetProxy(pPed, true);
					if(Verifyf(dstDamageIndex != kInvalidPedDamageSet, "Could not allocate a compressed ped damage set for the clone"))
					{
						if (pPed->GetCompressedDamageSetID() != kInvalidPedDamageSet)
						{
							CPEDDEBUG_DISPLAYF("CPedDamageManager::ClonePedDamage: clone destination already has compressed damage");
						}
						m_CompressedDamageSetProxyPool[dstDamageIndex]->Clone(pPed, m_CompressedDamageSetProxyPool[srcDamageIndex]);
						pPed->SetCompressedDamageSetID(dstDamageIndex);
					}
				}
			}
		}
	}
}

bool CPedDamageManager::WasPedDamageClonedFromLocalPlayer(u8 damageIndex) const
{
	return (damageIndex != kInvalidPedDamageSet && m_DamageSetPool[damageIndex]->WasClonedFromLocalPlayer());
}


void CPedDamageManager::ClearPedDecorations(u8 damageIndex, bool keepNonStreamed, bool keepOnlyScarsAndBruises, bool keepOnlyOtherDecorations)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ClearPedDecorations: ped=0x%p(%s), damageIndex=%d, keepNonStreamed = %s, keepOnlyScarsAndBruises = %s, keepOnlyOtherDecorations = %s", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex, keepNonStreamed?"true":"false", keepOnlyScarsAndBruises?"true":"false", keepOnlyOtherDecorations?"true":"false");

		m_DamageSetPool[damageIndex]->ClearDecorations(keepNonStreamed, keepOnlyScarsAndBruises, keepOnlyOtherDecorations);
		if (!m_DamageSetPool[damageIndex]->HasBlood() && !m_DamageSetPool[damageIndex]->HasDecorations() && !m_DamageSetPool[damageIndex]->WasClonedFromLocalPlayer()) // don't auto release the set clearing damage if from cloned from the local player (we need to remember that or we will not draw in mirrors)
		{
			m_DamageSetPool[damageIndex]->ReleasePed(false);
		}
	}
}

void CPedDamageManager::ClearClanDecorations(u8 damageIndex)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ClearClanDecorations: ped=0x%p(%s), damageIndex=%d", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "", damageIndex);
		m_DamageSetPool[damageIndex]->ClearClanDecorations();
	}
}


void CPedDamageManager::ClearCompressedPedDecorations(u8 damageIndex)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		CPEDDEBUG_DISPLAYF("CPedDamageManager::ClearCompressedPedDecorations: ped=0x%p(%s), damageIndex=%d", (damageIndex<kMaxDamagedPeds) && (m_CompressedDamageSetProxyPool[damageIndex]) ? m_CompressedDamageSetProxyPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_CompressedDamageSetProxyPool[damageIndex]) ? GetPedName(m_CompressedDamageSetProxyPool[damageIndex]->GetPed()) : "", damageIndex);
	
		m_CompressedDamageSetProxyPool[damageIndex]->GetCompressedTextureSet()->ClearDecorations();
	}
}

// clear a specific damage blit
void CPedDamageManager::ClearPedDecoration(u8 damageIndex, int index)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		PEDDEBUG_DISPLAYF("CPedDamageManager::ClearPedDecoration: ped=0x%p(%s), damageIndex=%d, index=%d", (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? m_DamageSetPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_DamageSetPool[damageIndex]) ? GetPedName(m_DamageSetPool[damageIndex]->GetPed()) : "" , damageIndex, index );
		
		m_DamageSetPool[damageIndex]->ClearDecoration(index);
	}
}

// clear a specific damage blit
void CPedDamageManager::ClearCompressedPedDecoration(u8 damageIndex, int index)
{
	if (Verifyf(damageIndex != kInvalidPedDamageSet,"Can't release kInvalidPedDamageSet!"))
	{
		CPEDDEBUG_DISPLAYF("CPedDamageManager::ClearCompressedPedDecoration: ped=0x%p(%s), damageIndex=%d, index=%d, ", (damageIndex<kMaxDamagedPeds) && (m_CompressedDamageSetProxyPool[damageIndex]) ? m_CompressedDamageSetProxyPool[damageIndex]->GetPed() : 0, (damageIndex<kMaxDamagedPeds) && (m_CompressedDamageSetProxyPool[damageIndex]) ? GetPedName(m_CompressedDamageSetProxyPool[damageIndex]->GetPed()) : "", damageIndex, index);
		
		m_CompressedDamageSetProxyPool[damageIndex]->GetCompressedTextureSet()->ClearDecoration(index);
	}
}

void CPedDamageManager::ClearAllPlayerBlood()
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::ClearAllPlayerBlood");
	
	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (m_DamageSetPool[i]->IsForPlayer())
		{
			ClearPedBlood((u8)i);
		}
	}
}

void CPedDamageManager::PromotePlayerBloodToScars()
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::PromotePlayerBloodToScars");
	
	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (m_DamageSetPool[i]->IsForPlayer())
		{
			PromoteBloodToScars((u8)i);
		}
	}
}


void CPedDamageManager::ClearAllPlayerDecorations()
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::ClearAllPlayerDecorations");
	
	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (m_DamageSetPool[i]->IsForPlayer())
		{
			ClearPedDecorations((u8)i);
		}
	}
}

void CPedDamageManager::ClearAllPlayerScars(const CPed* pPed)
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::ClearAllPlayerScars pPed[%p] model[%s] logname[%s]",pPed,pPed ? pPed->GetModelName() : "", pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");

	if (!pPed)
		return;

	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (m_DamageSetPool[i]->IsForPlayer() && (m_DamageSetPool[i]->GetPed() == pPed))
		{
			ClearPedDecorations((u8)i,false,false,true);
		}
	}
}

void CPedDamageManager::ClearAllNonPlayerBlood()
{
	PEDDEBUG_DISPLAYF("CPedDamageManager::ClearAllNonPlayerBlood");
	
	for (int i=0;i< m_DamageSetPool.GetMaxCount();i++)
	{
		if (!m_DamageSetPool[i]->IsForPlayer())
			ReleaseDamageSet((u8)i);
	}
}

CPedDamageSet * CPedDamageManager::GetPlayerDamageSet()
{
	// find See if we have a damage set for the player.
	if (CPed * pPed = CGameWorld::FindLocalPlayer())
	{
		u16 damageIndex = pPed->GetDamageSetID();
		if (damageIndex!= kInvalidPedDamageSet)
		{
			return m_DamageSetPool[damageIndex];
		}
	}
	return NULL;
}

const CCompressedPedDamageSet* CPedDamageManager::GetCompressedDamageSet(const CPed* pPed)	const
{
	if (pPed == NULL)
	{
		return NULL;
	}

	u8 dmgIdx = pPed->GetCompressedDamageSetID();
	if (dmgIdx != kInvalidPedDamageSet)
	{
		return m_CompressedDamageSetProxyPool[dmgIdx]->GetCompressedTextureSet();
	}

	return NULL;
}

const CPedDamageSet* CPedDamageManager::GetDamageSet(const CPed* pPed)const
{
	if (pPed == NULL)
	{
		return NULL;
	}

	u8 dmgIdx = pPed->GetDamageSetID();
	if (dmgIdx != kInvalidPedDamageSet)
	{
		return m_DamageSetPool[dmgIdx];
	}

	return NULL;
}


int ScarDataSortCompare( CPedScarNetworkData const* A,  CPedScarNetworkData const* B)
{
	float diff = A->age - B->age;
	return (diff < 0.0f) ? -1 : ((diff >0.0f) ? 1: 0);
}

int BloodDataSortCompare( CPedBloodNetworkData const* A,  CPedBloodNetworkData const* B)
{
	// Sort by is my component Torso and how close my UV.x is to 0.75 (front)
	if((A->zone == kDamageZoneTorso) && (B->zone != kDamageZoneTorso))
	{
		return -1;
	}
	else if((A->zone != kDamageZoneTorso) && (B->zone == kDamageZoneTorso))
	{
		return 1;
	}
	else if((A->zone == kDamageZoneTorso) && (B->zone == kDamageZoneTorso))
	{
		float AdiffFromFront = Abs(A->uvPos.x - 0.75f);
		float BdiffFromFront = Abs(B->uvPos.x - 0.75f);

		if(AdiffFromFront < BdiffFromFront)
		{
			return -1;
		}
		else if(BdiffFromFront < AdiffFromFront)
		{
			return 1;
		}

		return 0;
	}
	else if((A->zone != kDamageZoneTorso) && (B->zone != kDamageZoneTorso))
	{
		return 0;
	}

	return 0;
}

bool CPedDamageManager::GetBloodDataForLocalPed(CPed* pLocalPed, const atArray<CPedBloodNetworkData>*& pOutBloodDataArray)
{
	if (pLocalPed == NULL)
	{	
		Assertf(0, "CPedDamageManager::GetBloodDataForLocalPed: ped is NULL");
		return false;
	} 

	CPedDamageSet* pDamageSet = const_cast<CPedDamageSet*>(PEDDAMAGEMANAGER.GetDamageSet(pLocalPed));

	if (pDamageSet == NULL)
	{
		return false;
	}

	sm_CachedLocalPedBloodData.Reset();

	atFixedArray<CPedDamageBlitBlood, CPedDamageSetBase::kMaxBloodDamageBlits> & bloodDamageList = pDamageSet->GetPedBloodDamageList();

	for(int i = 0; i < bloodDamageList.GetCount(); ++i)
	{
		CPedDamageBlitBlood& blood = bloodDamageList[i];
		const CPedBloodDamageInfo* bloodInfo = blood.GetBloodInfoPtr();

		Vector4 uvCoords = blood.GetUVCoords();
		
		float rotation = 0.0f;
		Vector2 dir(uvCoords.z,uvCoords.w);
		float mag = dir.Mag();
		if (mag>0.001)
		{
			dir /= mag;
			rotation = atan2(dir.y,dir.x)*RtoD;
			rotation = (rotation >= 0.0f) ? rotation : 360.0f+rotation;
		}

		float age = TIME.GetElapsedTime() - blood.GetBirthTime();
		age = Clamp(age,0.0f,512.0f);

		float scale = blood.GetScale();

		CPedBloodNetworkData networkBloodData;
		networkBloodData.rotation		= rotation;
		networkBloodData.uvPos.x		= uvCoords.x;
		networkBloodData.uvPos.y		= uvCoords.y;
		networkBloodData.uvFlags		= blood.GetUVFlipFlags();
		networkBloodData.scale			= scale;
		networkBloodData.age			= age;
		networkBloodData.bloodNameHash	= bloodInfo->m_Name.GetHash();
		networkBloodData.zone			= blood.GetZone();

		sm_CachedLocalPedBloodData.PushAndGrow(networkBloodData);
	}

	// sort blood data by zone and UV.x so we get torso shots at the front of the ped.
	sm_CachedLocalPedBloodData.QSort(0,-1,BloodDataSortCompare);

	pOutBloodDataArray = &sm_CachedLocalPedBloodData;
	return true;
}

bool CPedDamageManager::GetScarDataForLocalPed(CPed* pLocalPed, const atArray<CPedScarNetworkData>*& pOutScarDataArray)
{
	if (pLocalPed == NULL)
	{	
		Assertf(0, "CPedDamageManager::GetScarDataForLocalPed: ped is NULL");
		return false;
	}

	// sorry, but quite a few of the functions in other members haven't got a const version,
	// plus we're family anyway!
	CPedDamageSet* pDamageSet = const_cast<CPedDamageSet*>(PEDDAMAGEMANAGER.GetDamageSet(pLocalPed));

	// ped doesn't have a damage set, so no scars
	if (pDamageSet == NULL)
	{
		return false;
	}

	// cached data is not valid, gather the scar info from the damage set
	if (sm_IsCachedLocaPedScarDataValid == false)
	{
		sm_CachedLocaPedScarData.Reset();

		atVector<CPedDamageBlitDecoration>& decorationArray = pDamageSet->GetDecorationList();

		for (int i = 0; i < decorationArray.GetCount(); i++)
		{
			CPedDamageBlitDecoration& decoration = decorationArray[i];
			const CPedDamageDecalInfo* pSourceInfo = decoration.GetSourceInfo();

			// not a scar/bruise or it's not active any more
			if (decoration.IsDone() || decoration.GetTextureInfo() == NULL || decoration.GetSourceNameHash() == atHashValue::Null() || pSourceInfo==NULL)
			{
				continue;
			}

			// we only want to sync scars, not armor bullets, dirt or bruises.
			if (decoration.IsOnClothOnly()/* || decoration.HasBumpMap()*/) // slightly hacky scars are not on cloth and they always have bump (dirt and bruises do not)
			{
				continue;
			}

			// Fill in scar data
			Vector4 uvCoords = decoration.GetUVCoords(); 
			
			float rotation = 0.0f;
			Vector2 dir(uvCoords.z,uvCoords.w);
			float mag = dir.Mag();
			if (mag>0.001)
			{
				dir /= mag;
				rotation = atan2(dir.y,dir.x)*RtoD;
				rotation = (rotation >= 0.0f) ? rotation : 360.0f+rotation;
			}
			
			float age = TIME.GetElapsedTime() - decoration.GetBirthTime();
			age = Clamp(age,0.0f,512.0f);

			float scale = decoration.GetScale().x*100;  // convert scale into 0.0 to 1.0 range
		
			if ((pSourceInfo->m_MaxSize-pSourceInfo->m_MinSize)>0.0f)
				scale = (scale - pSourceInfo->m_MinSize)/(pSourceInfo->m_MaxSize-pSourceInfo->m_MinSize); 
			else
				scale = 1.0f;

			CPedScarNetworkData scarData;
			scarData.uvPos.x		= uvCoords.x;
			scarData.uvPos.y		= uvCoords.y;
			scarData.uvFlags		= decoration.GetUVFlipFlags();
			scarData.scale			= Clamp(scale,0.0f,1.0f);
			scarData.age			= age;
			scarData.rotation		= rotation;
			scarData.zone			= decoration.GetZone();
			scarData.scarNameHash	= decoration.GetSourceNameHash();
			scarData.forceFrame		= (int)decoration.GetFixedFrame();

			sm_CachedLocaPedScarData.PushAndGrow(scarData);
			
		}

		// sort scar data to the most recent are the ones we sync
		sm_CachedLocaPedScarData.QSort(0,-1,ScarDataSortCompare);
	
		// TODO: Cache will be valid unless a scar is added or removed
		// sm_IsCachedLocaPedScarDataValid = true;
	}

	pOutScarDataArray = &sm_CachedLocaPedScarData;
	return true;
}

#if __BANK

#include "grcore/font.h"

static int		s_BloodType = 1;
static int		s_BloodZone = 0;

static int		s_DebugBloodType = 1;
static int		s_DebugBloodZone = kDamageZoneTorso;
static Vector2	s_DebugBloodUVPos(0.25f,0.5f);

static bool		s_EnableTestBlood=false;   
static bool 	s_LastEnableTestBlood=false;
static bool		s_EnableBloodUVPush = false;


static Vector3	s_BloodTestPos(0.380f,-0.22f,.380f);
static Vector3	s_BloodTestDir(0.3f,0.0f,0.0f);
static bool		s_DebugTargetDisplay = false;
static bool		s_ShowTestBloodPos = false;
static bool		s_DebugCylinderDisplay = false;	
static float	s_BloodTestRotation = 0.0f;
static float	s_BloodTestScale = 1.0f;
static int		s_BloodTestForcedFrame = -1;
static float	s_BloodTestPreAge = 0.0f;
static bool		s_ForceNotStanding = false;

static int		s_DebugDecorationType=0;
static bool		s_EnableTestDecoration=false;
static int		s_DebugDecorationZone=1;
static Vector2	s_DebugDecorationPos(0.5f,0.65f);
static float	s_DebugDecorationRotation=0.0f;
static float	s_DebugDecorationScale=.4f;
static int		s_DebugDamageForcedFrame=-1;


static int		s_DebugDamageDecalType=0;
static bool		s_EnableTestDamageDecal=false;

static int		s_DebugDamageDecalZone=0;
static Vector2	s_DebugDamageDecalPos(0.25f,0.65f);
static float	s_DebugDamageDecalRotation=0.0f;
static float	s_DebugDamageDecalScale=1.0f;
static float	s_DebugDamageDecalAlpha=-1.0f;

static bool		s_UseOneBloodTypeForMassiveTest = false;
static int		s_MassiveTestBloodType = 0;
static float	s_MassiveTestPreAge=180.0f;

static int		s_DamagePackIndex=0;
static float	s_DamagePackPreAge=0.0f;
static float	s_DamagePackAlpha=1.0f;
static bool		s_ContinuousDamagePackUpdate = false;

static int		s_DebugBloodLimitZone = 0;
static int		s_DebugBloodLimit = -1;

static int		s_UsePlayerPed = 1;
static char		s_TxdName[1024];
static char		s_TxtName[1024];
static CPed *	s_LastDebugPed=NULL;

static int		s_ZoneToComponent[kDamageZoneNumZones] = {
					RAGDOLL_SPINE0,			//	kDamageZoneTorso,	
					RAGDOLL_HEAD,			//	kDamageZoneHead,
					RAGDOLL_UPPER_ARM_LEFT,	//	kDamageZoneLeftArm,
					RAGDOLL_UPPER_ARM_RIGHT,//	kDamageZoneRightArm,
					RAGDOLL_THIGH_LEFT,		//	kDamageZoneLeftLeg,
					RAGDOLL_THIGH_RIGHT		//	kDamageZoneRightLeg,
				};

CPed * CPedDamageManager::GetDebugPed()
{
	if (s_UsePlayerPed)
		return CGameWorld::FindLocalPlayer();
	else
		return CPedVariationDebug::GetDebugPed();
}

void  CPedDamageManager::UpdateDebugMarker()
{
	if (s_ShowTestBloodPos)
	{
		if (CPed * pPed = GetDebugPed())
			s_LastHitPos = VEC3V_TO_VECTOR3(Transform(pPed->GetMatrix(), VECTOR3_TO_VEC3V(s_BloodTestPos)));
	}
}

void  CPedDamageManager::CheckDebugPed()
{
	if (!s_UsePlayerPed)
	{
		if (s_LastDebugPed != CPedVariationDebug::GetDebugPed()) // debug ped changed, we need to refresh dynamic tests
			TestUpdate();
	}
}


void CPedDamageManager::TestBlood()
{
	if (CPed * pPed = GetDebugPed())
	{
		u16 component = static_cast<u16>( s_ZoneToComponent[s_BloodZone] );

		Vec3V worldPos = Transform(pPed->GetMatrix(), VECTOR3_TO_VEC3V(s_BloodTestPos));

		PEDDAMAGEMANAGER.AddPedBlood(pPed, component, VEC3V_TO_VECTOR3(worldPos),
			PEDDAMAGEMANAGER.m_DamageData.GetBloodDamageInfoByIndex(s_BloodType)->m_Name, s_ForceNotStanding); 
	}
}

void CPedDamageManager::TestBloodUV()
{
	if (CPed * pPed = GetDebugPed())
	{
		PEDDAMAGEMANAGER.AddPedBlood(pPed, (ePedDamageZones)s_DebugBloodZone, s_DebugBloodUVPos, s_BloodTestRotation, s_BloodTestScale, PEDDAMAGEMANAGER.m_DamageData.GetBloodDamageInfoByIndex(s_DebugBloodType)->m_Name, s_ForceNotStanding, s_BloodTestPreAge, s_BloodTestForcedFrame, s_EnableBloodUVPush);
	}
}

void CPedDamageManager::TestStab()
{
	if (CPed * pPed = GetDebugPed())
	{
		u16 component = static_cast<u16>( s_ZoneToComponent[s_BloodZone] );

		Vec3V worldPos = Transform(pPed->GetMatrix(), VECTOR3_TO_VEC3V(s_BloodTestPos));
		Vec3V stabDir = Transform3x3(pPed->GetMatrix(), VECTOR3_TO_VEC3V(s_BloodTestDir));

		PEDDAMAGEMANAGER.AddPedStab(pPed, component, VEC3V_TO_VECTOR3(worldPos),VEC3V_TO_VECTOR3(stabDir), 
									PEDDAMAGEMANAGER.m_DamageData.GetBloodDamageInfoByIndex(s_BloodType)->m_Name); 
	}
}

void CPedDamageManager::TestDecal()
{
	if (CPed * pPed = GetDebugPed())
	{
		if (CPedDamageDecalInfo * pDecalInfo = PEDDAMAGEMANAGER.GetDamageData().GetDamageDecalInfoByIndex(s_DebugDamageDecalType))
		{
			PEDDAMAGEMANAGER.AddPedDamageDecal( pPed, (ePedDamageZones)s_DebugDamageDecalZone, s_DebugDamageDecalPos, s_DebugDamageDecalRotation, s_DebugDamageDecalScale, pDecalInfo->m_Name,false,-1,s_DebugDamageDecalAlpha);
		}
	}
}


void CPedDamageManager::TestDamagePack()
{
	if (CPed * pPed = GetDebugPed())
	{
		PEDDAMAGEMANAGER.AddPedDamagePack(pPed, PEDDAMAGEMANAGER.m_DamageData.m_DamagePacks[s_DamagePackIndex]->m_Name, s_DamagePackPreAge, s_DamagePackAlpha);
	}
}


void CPedDamageManager::TestLimitZoneBlood()
{
	if (CPed * pPed = GetDebugPed())
	{
		u8 damageID = PEDDAMAGEMANAGER.GetDamageID(pPed);
		if (damageID!=kInvalidPedDamageSet)
			PEDDAMAGEMANAGER.LimitZoneBlood(damageID, (s_DebugBloodLimitZone>=kDamageZoneNumBloodZones) ? kDamageZoneInvalid : (ePedDamageZones)s_DebugBloodLimitZone, s_DebugBloodLimit);
	}
}


void CPedDamageManager::TestPromoteBloodToScars()
{
	if (CPed * pPed = GetDebugPed())
	{
		u8 damageID = PEDDAMAGEMANAGER.GetDamageID(pPed);
		if (damageID!=kInvalidPedDamageSet)
			PEDDAMAGEMANAGER.PromoteBloodToScars(damageID);
	}
}

void CPedDamageManager::DumpDecals()
{
	if (const CPed * pPed = GetDebugPed())
	{
		if (const CPedDamageSet* damageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed))
		{
			const atVector<CPedDamageBlitDecoration> & decorationList =  damageSet->GetDecorationList();
			if (decorationList.GetCount()>0)
			{
				Displayf("");
				for (int i = 0;i<decorationList.GetCount();i++)
				{
					decorationList[i].DumpScriptParams(i==0);
				}
				Displayf("");
			}
		}
	}
}

void CPedDamageManager::DumpDamagePack()
{
	if (const CPed * pPed = GetDebugPed())
	{
		if (const CPedDamageSet* damageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed))
		{
			Displayf("    <Item type=\"CPedDamagePack\">");
			Displayf("      <Name>ReplaceWithName</Name>");
			Displayf("      <Entries>");
		
			for (int zone = 0;zone<kDamageZoneNumBloodZones;zone++)
			{
				bool first = true;
				const atFixedArray<CPedDamageBlitBlood, CPedDamageSetBase::kMaxBloodDamageBlits> & bloodList =  damageSet->GetPedBloodDamageList();
				if (bloodList.GetCount()>0)
				{				
					for (int i = 0;i<bloodList.GetCount();i++)
					{
						first = bloodList[i].DumpDamagePackEntry(zone, first);
					}
				}

				const atVector<CPedDamageBlitDecoration> & decorationList =  damageSet->GetDecorationList();
				if (decorationList.GetCount()>0)
				{				
					for (int i = 0;i<decorationList.GetCount();i++)
					{
						first = decorationList[i].DumpDamagePackEntry(zone, first);
					}
				}
			}
			Displayf("      </Entries>");
			Displayf("    </Item>");
		}
	}
}

void CPedDamageManager::DumpBlood()
{
	if (const CPed * pPed = GetDebugPed())
	{
		if (const CPedDamageSet* damageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed))
		{
			const atFixedArray<CPedDamageBlitBlood, CPedDamageSetBase::kMaxBloodDamageBlits> & bloodList =  damageSet->GetPedBloodDamageList();
			if(bloodList.GetCount()>0)
			{
				Displayf("");
				for (int i = 0;i<bloodList.GetCount();i++)
				{
					bloodList[i].DumpScriptParams(i==0);
				}
				Displayf("");
			}
		}
	}
}

void CPedDamageManager::TestUpdate()
{
	TestUpdateBlood();
	TestUpdateDecorations();
	s_LastDebugPed = (s_UsePlayerPed) ? NULL : CPedVariationDebug::GetDebugPed();
}

void CPedDamageManager::TestUpdateBlood()
{
	if (CPed * pPed = GetDebugPed())
	{
		if (s_EnableTestBlood || s_EnableTestBlood!=s_LastEnableTestBlood)
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex!=kInvalidPedDamageSet)
				PEDDAMAGEMANAGER.ClearPedBlood(damageIndex);  // clear previous damage (or they stack up)

			if (s_EnableTestBlood)
				PEDDAMAGEMANAGER.AddPedBlood(pPed, (ePedDamageZones)s_DebugBloodZone, s_DebugBloodUVPos, PEDDAMAGEMANAGER.m_DamageData.GetBloodDamageInfoByIndex(s_DebugBloodType)->m_Name, s_ForceNotStanding);
		}
	}
	s_LastEnableTestBlood = s_EnableTestBlood;
}

void CPedDamageManager::TestUpdateBloodScriptParams()
{
	if (CPed * pPed = GetDebugPed())
	{
		if (s_EnableTestBlood || s_EnableTestBlood!=s_LastEnableTestBlood)
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex!=kInvalidPedDamageSet)
				PEDDAMAGEMANAGER.ClearPedBlood(damageIndex);  // clear previous damage (or they stack up)

			if (s_EnableTestBlood)
				PEDDAMAGEMANAGER.AddPedBlood(pPed, (ePedDamageZones)s_DebugBloodZone, s_DebugBloodUVPos, s_BloodTestRotation, s_BloodTestScale, PEDDAMAGEMANAGER.m_DamageData.GetBloodDamageInfoByIndex(s_DebugBloodType)->m_Name, s_ForceNotStanding, s_BloodTestPreAge, s_BloodTestForcedFrame, s_EnableBloodUVPush);
		}
	}
	s_LastEnableTestBlood = s_EnableTestBlood;
}

void CPedDamageManager::UpdateDecorationTestModes()
{
	// clear all the extras if we're done
	if (!(s_EnableTestDecoration || s_EnableTestDamageDecal))
	{
		if (CPed * pPed = GetDebugPed())
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex!=kInvalidPedDamageSet)
				PEDDAMAGEMANAGER.ClearPedDecorations(damageIndex); // clear previous decoration (or they stack up)
		}
	}

	TestUpdateDecorations();
}

void CPedDamageManager::TestUpdateDecorations()
{
	if (CPed * pPed = GetDebugPed())
	{
		if (s_EnableTestDecoration || s_EnableTestDamageDecal)
		{
			strStreamingObjectName nullHash;

			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex!=kInvalidPedDamageSet)
				PEDDAMAGEMANAGER.ClearPedDecorations(damageIndex); // clear previous decoration (or they stack up)
		
			if (s_EnableTestDecoration)
				PEDDAMAGEMANAGER.AddPedDecoration(pPed, nullHash, nullHash, (ePedDamageZones)s_DebugDecorationZone,s_DebugDecorationPos, s_DebugDecorationRotation, Vector2(s_DebugDecorationScale, s_DebugDecorationScale), false,
												s_TxdName, s_TxtName, (s_DebugDecorationType==1));
			
			if (s_EnableTestDamageDecal)
			{
				if (CPedDamageDecalInfo * pDecalInfo = PEDDAMAGEMANAGER.GetDamageData().GetDamageDecalInfoByIndex(s_DebugDamageDecalType))
					PEDDAMAGEMANAGER.AddPedDamageDecal(pPed, (ePedDamageZones)s_DebugDamageDecalZone, s_DebugDamageDecalPos, s_DebugDamageDecalRotation, s_DebugDamageDecalScale, pDecalInfo->m_Name, false, s_DebugDamageForcedFrame, s_DebugDamageDecalAlpha);
			}
		}
	}	  
}

void CPedDamageManager::TestUpdateDamagePack()
{
	if (CPed * pPed = GetDebugPed())
	{
		if (s_ContinuousDamagePackUpdate)
		{
			u8 damageIndex = pPed->GetDamageSetID();
			if (damageIndex!=kInvalidPedDamageSet)
			{
				PEDDAMAGEMANAGER.ClearPedBlood(damageIndex);  // clear previous damage (or they stack up)
				PEDDAMAGEMANAGER.ClearPedDecorations(damageIndex); 
			}
			PEDDAMAGEMANAGER.AddPedDamagePack(pPed, PEDDAMAGEMANAGER.m_DamageData.m_DamagePacks[s_DamagePackIndex]->m_Name, s_DamagePackPreAge, s_DamagePackAlpha);
		}
	}
}

// apply max amount of blood damage and decals possible, spread across each zone
void CPedDamageManager::TestMassiveDamage()
{
	if (CPed * pPed = GetDebugPed())
	{
		int bloodTypeCount = GetInstance().m_DamageData.m_BloodData.GetCount();  // how many types of blood do we have
		int bloodPerZone = CPedDamageSetBase::kMaxBloodDamageBlits/kDamageZoneNumBloodZones;
		int vCount = (int)sqrtf((float)bloodPerZone);
		int uCount = bloodPerZone/vCount;
		float deltaU = 1.0f/uCount;
		float deltaV = 1.0f/vCount;

		int bloodType = s_UseOneBloodTypeForMassiveTest ? s_MassiveTestBloodType : 0;

		for (int zone = 0; zone< kDamageZoneNumBloodZones; zone ++)
		{
			float bloodRotation = 0;
			for (int uStep = 0; uStep <uCount; uStep++)
			{
				for (int vStep = 0; vStep <uCount; vStep++)
				{
					Vector2 uv(uStep*deltaU,vStep*deltaV);
					PEDDAMAGEMANAGER.AddPedBlood(pPed, (ePedDamageZones)zone, uv, bloodRotation, 1.0, PEDDAMAGEMANAGER.m_DamageData.GetBloodDamageInfoByIndex(bloodType)->m_Name, false, s_MassiveTestPreAge, -1);
					// todo: we should preage them enough so all are fading between two frames...
					
					if (!s_UseOneBloodTypeForMassiveTest)
					{
						if (++bloodType>=bloodTypeCount) 
							bloodType = 0;
					}

					bloodRotation+= 360.0f/bloodPerZone;
				}
			}
		}

		// do decals too
		//	int damageDecalTypeCount = GetInstance().m_DamageData.m_DamageDecalData.GetCount(); // how many decals
	}
}

void CPedDamageManager::TestDamageClearPlayer()
{
	PEDDAMAGEMANAGER.ClearAllPlayerBlood();
	PEDDAMAGEMANAGER.ClearAllPlayerDecorations();
}

void CPedDamageManager::TestDamageClearNonPlayer()
{
	PEDDAMAGEMANAGER.ClearAllNonPlayerBlood();
}

void CPedDamageManager::TestDamageBloodToScars()	
{
	PEDDAMAGEMANAGER.PromotePlayerBloodToScars();
}

void CPedDamageManager::TestClone()
{
	CPed * srcPed = CGameWorld::FindLocalPlayer();
	CPed * dstPed = CPedVariationDebug::GetDebugPed();
	if (srcPed && dstPed)	
		PEDDAMAGEMANAGER.ClonePedDamage(dstPed, srcPed, true);
}


void CPedDamagePackEntry::DamagePackUpdated()
{
	CPedDamageManager::TestUpdateDamagePack();
}

void CPedDamageManager::ReloadTuning()
{
#if 0	 // NYI
	// start multiframe delay counter
	GetInstance().sm_ReloadCounter = 4;
	
	// release all ped data
	for (u8 i=0; i< GetInstance().m_DamageSetPool.GetMaxCount(); i++)
		GetInstance().ReleaseDamageSet(i);

	// release the widgets
	if (GetInstance().sm_WidgetGroup)
	{
		bkWidget *i = GetInstance().sm_WidgetGroup->GetChild();
		while ( i ) 
		{
			bkWidget* next = i->GetNext();
			i->Destroy();
			i = next;
		}
	}
#endif
}


void CPedDamageManager::ReloadUpdate()
{
#if 0	 // NYI
	if (GetInstance().sm_ReloadCounter>0)
	{
		GetInstance().sm_ReloadCounter--;
		if (GetInstance().sm_ReloadCounter==0)
		{
			// TODO: empty the parser data
			// TODO: reload data

			// rebuild widgets
			if (CPed::ms_pBank)
				GetInstance().AddWidgets(*CPed::ms_pBank);
		}
	}
#endif
}


void CPedDamageManager::AddWidgets(rage::bkBank& bank)
{
	static const char *s_DebugNames[5] ={"None","Show Zones","Show U","Show V","Show UV Grid"};
	static const char *s_DebugPedType[2] ={"Debug Ped","Player Ped"};
	static const char *s_DecorationNames[2] ={"Custom wo/Bump", "Custom w/Bump"};
	
	static const char **s_BloodTypeNames = NULL;
	int bloodTypeCount = GetInstance().m_DamageData.m_BloodData.GetCount();
	if (bloodTypeCount>0)
	{
		if (s_BloodTypeNames) delete s_BloodTypeNames; // in case we're reloading
		s_BloodTypeNames = rage_new const char*[bloodTypeCount];
		for (int i=0;i<bloodTypeCount;i++)
			s_BloodTypeNames[i] = GetInstance().m_DamageData.m_BloodData[i]->m_Name.GetCStr();
	}
	static const char **s_DamageDecalTypeNames = NULL;
	int damageDecalTypeCount = GetInstance().m_DamageData.m_DamageDecalData.GetCount();
	if (damageDecalTypeCount>0 )
	{
		if (s_DamageDecalTypeNames) delete s_DamageDecalTypeNames; // in case we're reloading
		s_DamageDecalTypeNames = rage_new const char*[damageDecalTypeCount];
		for (int i=0;i<damageDecalTypeCount;i++)
			s_DamageDecalTypeNames[i] = GetInstance().m_DamageData.m_DamageDecalData[i]->m_Name.GetCStr();
	}

	static const char **s_DamagePackNames = NULL;
	int damagePackCount = GetInstance().m_DamageData.m_DamagePacks.GetCount();
	if (damagePackCount>0)
	{
		if (s_DamagePackNames) delete s_DamagePackNames; // in case we're reloading
		s_DamagePackNames = rage_new const char*[damagePackCount];
		for (int i=0;i<damagePackCount;i++)
			s_DamagePackNames[i] = GetInstance().m_DamageData.m_DamagePacks[i]->m_Name.GetCStr();
	}

	TestUpdateDecorations();	  // prime the txd and txt names.

	if (sm_WidgetGroup==NULL)
	{
		sm_WidgetGroup = bank.PushGroup("Ped Damage", false );
		bank.PopGroup();
	}
	
	bank.SetCurrentGroup(*sm_WidgetGroup);

		// I've added this "local function" here to hook up the widget, feel free to re-implement this if the syntax is confusing
		class DebugRenderMode_update { public: static void func()
		{
			if (CPedDamageManager::sm_DebugRenderMode > 0)
			{
				CRenderPhaseDebugOverlayInterface::TogglePedDamageModeSetEnabled(false); // turn off to clear wireframe settings
				CRenderPhaseDebugOverlayInterface::TogglePedDamageModeSetEnabled(true);  // then turn it turn on
			}
			else
			{
				CRenderPhaseDebugOverlayInterface::TogglePedDamageModeSetEnabled(false);
			}
		}};

		bank.PushGroup("Blits");
			bank.AddToggle("Override Blit Alpha", &sm_EnableBlitAlphaOverride);
			bank.AddSlider("Blit Alpha Override", &sm_BlitAlphaOverride, 0.0f, 1.0f, 0.01f);
		bank.PopGroup();

		bank.AddToggle("Lock Compression In Blitting Stage", &g_DebugLockCompressionInBlittingStage);
		bank.AddToggle("Apply Damage", &sm_ApplyDamage, NullCB, "Allow peds to receive damage (effects and events will still play)");
		bank.AddCombo("Debug Render Mode", &sm_DebugRenderMode, 5, s_DebugNames,0,DebugRenderMode_update::func,"Torso=Red, Head=Green, LArm=Yellow, RArm=Blue, LLeg=Magenta, RLeg=Cyan");
		bank.AddCombo("Apply Widget Damage To:", &s_UsePlayerPed, 2, s_DebugPedType,0,datCallback(CFA(CPedDamageManager::TestUpdate)),"The Debug Ped is set in the Peds/Ped Type Widget bank");

		bank.AddButton("Clear Nonplayer Blood/Decorations",datCallback(CFA(CPedDamageManager::TestDamageClearNonPlayer)),"clear the blood damage and decorations from all peds except the player");
		bank.AddButton("Clear Player Blood/Decorations",datCallback(CFA(CPedDamageManager::TestDamageClearPlayer)),"clear the blood damage and decorations from the the player");
		bank.AddButton("Change Player Blood to Scars",datCallback(CFA(CPedDamageManager::TestDamageBloodToScars)),"clear all the blood damage on the player and convert it to scars");
		
		bank.PushGroup("XML Tuning", false );
			bank.AddButton("Save Tuning",datCallback(CFA(CPedDamageManager::SaveXML))); 
//			NYI Reload tuning file function
//			bank.AddButton("Reload Tuning",datCallback(CFA(CPedDamageManager::ReloadTuning))); 
			CPedDamageManager::GetInstance().m_DamageData.AddWidgets(bank);
		bank.PopGroup();

		if ( bloodTypeCount>0 )
		{
// 			bank.PushGroup("Test Blood by UV (Interactive)", false, "Interactively move Ped Blood Damage by specifying zone and UV position");
// 				bank.AddToggle("Enable Dynamic Blood Test",&s_EnableTestBlood,datCallback(CFA(CPedDamageManager::TestUpdateBlood)),"Enable interactive Ped Blood placement test");
// 				bank.AddToggle("Force 'Prone/dead' Damage",&s_ForceNotStanding,datCallback(CFA(CPedDamageManager::TestUpdateBlood)),"Force the use of the laying down/dead version of the damage");
// 				bank.AddCombo("Blood Effect", &s_DebugBloodType, bloodTypeCount, s_BloodTypeNames,datCallback(CFA(CPedDamageManager::TestUpdateBlood)),"The Blood damage effect to apply");
// 				bank.AddCombo("Zone", &s_DebugBloodZone, kDamageZoneNumBloodZones, s_ZoneCommonNames,datCallback(CFA(CPedDamageManager::TestUpdateBlood)),"The ped damage zone to apply the blood effect to");
// 				bank.AddSlider("Decoration UV", &s_DebugBloodUVPos, 0.0f, 1.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateBlood)),"The UV coords within the selected zone to apply the blood effect to");
// 			bank.PopGroup();
		
			bank.PushGroup("Test Blood by UV (Script Params)", false, "Interactively move Ped Blood Damage by specifying zone and UV position, rotations and scale");
				bank.AddToggle("Enable Dynamic Blood Testing",&s_EnableTestBlood,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"Enable interactive Ped Blood placement test");
				bank.AddToggle("Enable UV Limiting",&s_EnableBloodUVPush,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"Enable uv pushing (simulated bullet push from seams)");
				bank.AddCombo("Blood Effect", &s_DebugBloodType, bloodTypeCount, s_BloodTypeNames,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"The Blood damage effect to apply");
				bank.AddCombo("Zone", &s_DebugBloodZone, kDamageZoneNumBloodZones, s_ZoneCommonNames,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"The ped damage zone to apply the blood effect to");
				bank.AddSlider("Decoration UV", &s_DebugBloodUVPos, 0.0f, 1.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"The UV coords within the selected zone to apply the blood effect to");
				bank.AddSlider("Scale", &s_BloodTestScale, 0.0f, 1.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"Scale for the applied blood effect");
				bank.AddSlider("Rotation", &s_BloodTestRotation, 0, 360, 0.1f,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"Rotation for the applied blood effect");
				bank.AddSlider("PreAge", &s_BloodTestPreAge, 0.0f, 10000.0f, 1.0f,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"amount of time to preage the blood blit");
				bank.AddSlider("ForcedFrame", &s_BloodTestForcedFrame, -1, 32, 1,datCallback(CFA(CPedDamageManager::TestUpdateBloodScriptParams)),"Forced frame for the blood damage (-1 use random frame)");
				bank.AddToggle("Force 'Prone/dead' Damage",&s_ForceNotStanding,NullCB,"Force the use of the laying down/dead version of the damage");
				bank.AddButton("Add Single Blood Blit",datCallback(CFA(CPedDamageManager::TestBloodUV)),"Apply damage decal damage with the current test settings (Enable Dynamic Damage Decal Test must be disabled)");
				bank.AddButton("Dump Blood list",datCallback(CFA(CPedDamageManager::DumpBlood)),"Dump the coords and type info for the current set of blood damamage");
			bank.PopGroup();
		}

		if (damageDecalTypeCount>0)
		{
			bank.PushGroup("Test Damage Decals by UV (Script Params)", false, "Interactively move a Ped damage decals (scars/tattoos/dirt/armor damage) by specifying zone and UV position");
				bank.AddToggle("Enable Dynamic Damage Decal Testing",&s_EnableTestDamageDecal,datCallback(CFA(CPedDamageManager::UpdateDecorationTestModes)),"Enable interactive Ped damage decal placement test");
				bank.AddCombo("Type", &s_DebugDamageDecalType, damageDecalTypeCount, s_DamageDecalTypeNames,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The damage decal effect to apply");
				
				bank.AddCombo("Zone", &s_DebugDamageDecalZone, kDamageZoneNumBloodZones, s_ZoneCommonNames,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The ped damage zone to apply the damage decal to");
				bank.AddSlider("Decoration UV", &s_DebugDamageDecalPos, 0.0f, 1.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The UV coords within the selected zone to apply the damage decal to");
				bank.AddSlider("Rotation", &s_DebugDamageDecalRotation, 0.0f, 360.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The damage decal rotation value (in degrees)");
				bank.AddSlider("Scale", &s_DebugDamageDecalScale, 0.0f, 1.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The damage decal scale value");
				bank.AddSlider("Alpha", &s_DebugDamageDecalAlpha, -1.0f, 1.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The damage decal alpha value");
				bank.AddSlider("ForcedFrame", &s_DebugDamageForcedFrame, -1, 32, 1,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"Forced frame for the blood damage (-1 use random frame)");
				bank.AddButton("Add Single Decal",datCallback(CFA(CPedDamageManager::TestDecal)),"Apply damage decal damage with the current test settings (Enable Dynamic Damage Decal Test must be disabled)");
				bank.AddButton("Dump Decal list",datCallback(CFA(CPedDamageManager::DumpDecals)),"Dump the coords and type info for the current set of damage decals");
			bank.PopGroup();
		}

		bank.PushGroup("Test Tattoos by UV (Interactive)", false, "Interactively move a Ped Decoration by specifying zone and UV position");
			bank.AddToggle("Enable Test Tattoo",&s_EnableTestDecoration,datCallback(CFA(CPedDamageManager::UpdateDecorationTestModes)),"Enable interactive Ped Tattoo placement test");
			bank.AddCombo("Test Texture", &s_DebugDecorationType, 2, s_DecorationNames, datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The decoration type. Pick 'Custom wo/Bump' or 'Custom w/Bump' pick your own below");
			bank.AddText("TXD Name:",s_TxdName,1024,false,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)));
			bank.AddText("TXT Name:",s_TxtName,1024,false,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)));
	
			bank.AddCombo("Zone", &s_DebugDecorationZone, kDamageZoneNumBloodZones, s_ZoneCommonNames,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The ped damage zone to apply the decoration to");
			bank.AddSlider("Decoration UV", &s_DebugDecorationPos, 0.0f, 1.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The UV coords within the selected zone to apply the decoration to");
			bank.AddSlider("Rotation", &s_DebugDecorationRotation, 0.0f, 360.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The decoration rotation value (in degrees)");
			bank.AddSlider("Scale", &s_DebugDecorationScale, 0.0f, 2.0f, 0.01f,datCallback(CFA(CPedDamageManager::TestUpdateDecorations)),"The decoration scale value (in degrees)");
		bank.PopGroup();
		
		if ( bloodTypeCount>0 )
		{
			bank.PushGroup("Test Blood by Position", false, "Test Ped Blood by specifying a location");
				bank.AddCombo("Zone", &s_BloodZone, kDamageZoneNumBloodZones, s_ZoneCommonNames,NullCB,"The ped damage zone to apply the blood damage to");
				bank.AddCombo("Blood Type", &s_BloodType, bloodTypeCount, s_BloodTypeNames,NullCB,"The Blood damage effect to apply");
				bank.AddVector("LocalPos", &s_BloodTestPos, -2.0f, 2.0f, 0.01f,NullCB,"The position (in character local space) to apply the blood");
				bank.AddToggle("Force 'Prone/dead' Damage",&s_ForceNotStanding,NullCB,"Force the use of the laying down/dead version of the damage");
				bank.AddButton("Add Blood",datCallback(CFA(CPedDamageManager::TestBlood)),"Apply blood damage with the current test settings");
// 				bank.AddVector("Stab Dir", &s_BloodTestDir, -10.0f, 10.0f, 0.01f,NullCB,"The direction (in character local space) of the stroke for stab blood damage");
// 				bank.AddButton("Add Stab",datCallback(CFA(CPedDamageManager::TestStab)),"Apply blood damage using the stab interface and the current test setting");
			bank.PopGroup();
		}

		if ( damagePackCount>0 )
		{
			bank.PushGroup("Test Damage Pack", false, "Test Ped damage Packs");
			bank.AddCombo("PackName", &s_DamagePackIndex, damagePackCount, s_DamagePackNames,datCallback(CFA(CPedDamageManager::TestUpdateDamagePack)),"Damage Packs available tfor testing");
			bank.AddSlider("PreAge", &s_DamagePackPreAge, 0.0f, 10000.0f, 1.0f,datCallback(CFA(CPedDamageManager::TestUpdateDamagePack)),"Damage pack pre age time");
			bank.AddSlider("Alpha", &s_DamagePackAlpha, 0.0f, 1.0f, 0.01f, datCallback(CFA(CPedDamageManager::TestUpdateDamagePack)),"The damage pack alpha value (applies to Damage decorations only, not blood)");
			bank.AddButton("Add Damage Pack",datCallback(CFA(CPedDamageManager::TestDamagePack)),"Apply the specified Damage Pack with the selected preAge and alpha values");
			bank.AddToggle("Continuously Update Damage Pack",&s_ContinuousDamagePackUpdate,datCallback(CFA(CPedDamageManager::TestUpdateDamagePack)),"remove and re-add damage pack every frame (helps with tuning)");
			bank.AddButton("Dump as Damage Pack",datCallback(CFA(CPedDamageManager::DumpDamagePack)),"Dump the coords and type info for the blood and damage in xml form for use as a damage pack");
			bank.PopGroup();
		}

		bank.PushGroup("Programmer Widgets", false, "Enable various programmer debug views/tests");
			bank.AddToggle("Debug Blood Cylinders", &s_DebugCylinderDisplay, NullCB, "Draw the damage zone cylinders (in bind pose space)");
			bank.AddToggle("Show Test Blood Pos", &s_ShowTestBloodPos, NullCB, "Draw a green dot at the 'test Blood by Position' location");
			bank.AddToggle("Debug Push Zones",&s_DebugDrawPushZones, NullCB, "draw the damage push zone in the render target");
			bank.AddToggle("Debug Fade Freeze",&s_TestFreezeFade, NullCB, "for debugging the fade freeze during cutscene (without being in a cut scene)");
			if (PARAM_debugpeddamagetargets.Get())
			{
				bank.AddToggle("Debug Blood Target Display", &s_DebugTargetDisplay,NullCB, "Debug display the ped damage render target (requires '-extramemory=2000 -debugpeddamagetargets' on your command line)");
				bank.AddSlider("Debug Blood Target Index", &s_DebugTargetIndex,0,kMaxDamagedPeds-1,1,NullCB,"Which damage render target to display");
			}

			bank.AddButton("Clone Player to debug ped",datCallback(CFA(CPedDamageManager::TestClone)),"test the ped damage clone funtion");
			bank.AddButton("Promote Blood To Scars",datCallback(CFA(CPedDamageManager::TestPromoteBloodToScars)),"test the promote blood to scars function");

			bank.PushGroup("Massive Damage Test", false, "test for throwing a lot of damage at the player ped");
				bank.AddButton("Add Massive Damage",datCallback(CFA(CPedDamageManager::TestMassiveDamage)),"Apply max damage and scars to the ped for performance test");
				bank.AddToggle("Use One Type For Massive Test", &s_UseOneBloodTypeForMassiveTest,NullCB, "Debug display the ped damage render target (requires '-extramemory=2000 -debugpeddamagetargets' on your command line)");
				bank.AddCombo("Blood Type For Massive Test", &s_MassiveTestBloodType, bloodTypeCount, s_BloodTypeNames,NullCB,"The Blood damage effect to apply during massive damage test");
				bank.AddSlider("PreAge Time for Massive Test", &s_MassiveTestPreAge,-1,1000,1,NullCB,"preage time for massive damage");
			bank.PopGroup();	 

			bank.PushGroup("Limit Zone Blood Test", false, "test limiting blood per zone");
				bank.AddCombo("Zone", &s_DebugBloodLimitZone, kDamageZoneNumBloodZones+1, s_ZoneCommonNames, NullCB,"The ped damage zone to apply the decoration to");
				bank.AddSlider("Limit", &s_DebugBloodLimit, -1, (int)CPedDamageSet::kMaxBloodDamageBlits, 1, NullCB,"The limit for the specified zone (-1 is no limit)");
				bank.AddButton("Apply",datCallback(CFA(CPedDamageManager::TestLimitZoneBlood)),"show blood in the specified zone");
			bank.PopGroup();
			
			bank.AddToggle("add extra seam blits", &s_AddExtraBlitsAtSeams,NullCB, "Enable adding extra blit to try to cover seams");

			CPedDamageSet::AddWidgets(bank);
		bank.PopGroup();

	bank.UnSetCurrentGroup(*sm_WidgetGroup);
}

void CPedDamageManager::SaveXML()
{
	CPedDamageManager::GetInstance().m_DamageData.SaveXML();
}

void CPedDamageManager::SaveDebugTargets(const grcTexture * bloodTarget, const  grcTexture * tattooTarget)
{
	sm_DebugDisplayDamageTex = bloodTarget;
	sm_DebugDisplayTattooTex = tattooTarget;

	if (s_DebugTargetDisplay && CPedDamageSet::GetDebugTarget() && (sm_DebugDisplayDamageTex || sm_DebugDisplayTattooTex))
	{
		PIXBegin(0, "DebugCopy");

		grcViewport *oldVP = grcViewport::GetCurrent();
	
		grcTextureFactory::GetInstance().LockRenderTarget(0, CPedDamageSet::GetDebugTarget(), NULL);
		grcViewport vp;
		vp.Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
		grcViewport::SetCurrent(&vp);

		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull,grcStateBlock::DSS_IgnoreDepth,grcStateBlock::BS_Default);

		for (int pass = 0; pass<2;pass++)
		{
			const grcTexture * target = (pass==0) ? sm_DebugDisplayDamageTex : sm_DebugDisplayTattooTex;

			if (target)
			{
				const float startX = (pass==0)? 0.0f : 0.5f;
				grcBindTexture(target);

				static float uOffset = 0.5f/target->GetWidth();
				static float vOffset = 0.5f/target->GetHeight();
				float dualScale = 1.0f;
				float dualOffset = 0.0f;

				// rotate the RT to be vertical in the debug target
				grcBegin(drawTriStrip, 4);
				if(target->GetWidth()>target->GetHeight()) // rotated Target
				{
					grcVertex(0.0f+startX, 1.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (1.0f*dualScale+dualOffset)+vOffset, 1.0f+uOffset);
					grcVertex(0.0f+startX, 0.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (0.0f*dualScale+dualOffset)+vOffset, 1.0f+uOffset);
					grcVertex(0.5f+startX, 1.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (1.0f*dualScale+dualOffset)+vOffset, 0.0f+uOffset);
					grcVertex(0.5f+startX, 0.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (0.0f*dualScale+dualOffset)+vOffset, 0.0f+uOffset);
				}
				else
				{
					grcVertex(0.0f+startX, 1.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (0.0f*dualScale+dualOffset)+uOffset, 1.0f+vOffset);
					grcVertex(0.0f+startX, 0.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (0.0f*dualScale+dualOffset)+uOffset, 0.0f+vOffset);
					grcVertex(0.5f+startX, 1.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (1.0f*dualScale+dualOffset)+uOffset, 1.0f+vOffset);
					grcVertex(0.5f+startX, 0.0f, 0.0f, 0, 0, -1, Color32(0xffffffff), (1.0f*dualScale+dualOffset)+uOffset, 0.0f+vOffset);
				}
				grcEnd();	
			}
		}
		
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		grcViewport::SetCurrent(oldVP);
		PIXEnd();
	}
}


void CPedDamageManager::DebugDraw()
{
	PEDDECORATIONBUILDER.DebugDraw();

	if(s_DebugTargetDisplay && CPedDamageSet::GetDebugTarget() && (sm_DebugDisplayDamageTex || sm_DebugDisplayTattooTex)) 
	{
		PUSH_DEFAULT_SCREEN();
		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull,grcStateBlock::DSS_IgnoreDepth,grcStateBlock::BS_Default);

		for (int pass = 0; pass<2;pass++)
		{
			const grcTexture * target = (pass==0) ? sm_DebugDisplayDamageTex : sm_DebugDisplayTattooTex;

			if (target)
			{
				int width = target->GetWidth();
				int height = target->GetHeight();  
				if (width>height)
				{
					// some are side ways
					width = target->GetHeight();
					height = target->GetWidth();  		
				}

				if (height>720) // big one does not fit ;(
				{
					width  /= 2;
					height /= 2;
				}

				const float startX = (float)((pass==0)? 64 : GRCDEVICE.GetWidth() - 65 - width);
				const float startY = 32;
				const float startU = (pass==0)? 0.0f : 0.5f;
				

				float uOffset = 0.0f;
				float vOffset = 0.0f;

				// now need to blit this to the screen
				grcBindTexture(CPedDamageSet::GetDebugTarget());
				grcBegin(drawTriStrip, 4);
				grcVertex(startX,		startY+height,	0.0f, 0, 0, -1, Color32(0xffffffff), startU+0.0f+uOffset, 1.0f+vOffset);
				grcVertex(startX,		startY,			0.0f, 0, 0, -1, Color32(0xffffffff), startU+0.0f+uOffset, 0.0f+vOffset);
				grcVertex(startX+width, startY+height,	0.0f, 0, 0, -1, Color32(0xffffffff), startU+0.5f+uOffset, 1.0f+vOffset);
				grcVertex(startX+width, startY,			0.0f, 0, 0, -1, Color32(0xffffffff), startU+0.5f+uOffset, 0.0f+vOffset);
				grcEnd();	

				// draw a borders
				Color32 borderColor = (pass==0) ? Color32(0xff808080):  Color32(0xff808080);
				grcBindTexture(NULL);
				grcBegin(drawLineStrip, 5);
				grcVertex(startX,		startY+height,	0.0f, 0, 0, -1, borderColor, 0.0f, 1.0f);
				grcVertex(startX,		startY,			0.0f, 0, 0, -1, borderColor, 0.0f, 0.0f);
				grcVertex(startX+width, startY,			0.0f, 0, 0, -1, borderColor, 1.0f, 0.0f);
				grcVertex(startX+width, startY+height,	0.0f, 0, 0, -1, borderColor, 1.0f, 1.0f);
				grcVertex(startX,		startY+height,	0.0f, 0, 0, -1, borderColor, 0.0f, 1.0f);
				grcEnd();	

				for (int i=0;i<kDamageZoneNumBloodZones; i++)
				{
					float y = height*s_ViewportMappings[i].y;
				
					grcBegin(drawLines, 2);
					grcVertex(startX,		startY+y,	0.0f, 0, 0, -1, borderColor, 0.0f, 1.0f);
					grcVertex(startX+width, startY+y,	0.0f, 0, 0, -1, borderColor, 1.0f, 1.0f);
					grcEnd();	
				}

			}
		}

		POP_DEFAULT_SCREEN();
	}
}

void CPedDamageManager::DebugDraw3D()
{
	if(s_ShowTestBloodPos) 
	{
		grcWorldIdentity();
		grcBindTexture(NULL);

		static float boxSize = .01f;

		Matrix34 centerMtx(M34_IDENTITY);
		centerMtx.d = s_LastHitPos;
		grcDrawSolidBox(Vector3(boxSize,boxSize,boxSize), centerMtx, Color32(0.0f,1.0f,0.0f,1.0f));
	}
	
	if (s_DebugCylinderDisplay)
	{
		grcWorldIdentity();
		grcBindTexture(NULL);
		grcColor3f(1.0f,1.0f,1.0f);

		for(int i = 0; i < CPedDamageManager::GetInstance().m_ActiveDamageSets.size(); ++i)
		{
			if( CPedDamageManager::GetInstance().m_ActiveDamageSets[i] ){ CPedDamageManager::GetInstance().m_ActiveDamageSets[i]->DebugDraw(); }
		}
		//if(CPedDamageManager::GetInstance().m_DamageSetPool[kPlayerTarget]->GetPed())
		//{
		//	CPedDamageManager::GetInstance().m_DamageSetPool[kPlayerTarget]->DebugDraw();
		//}
	}
}

#endif


grcTexture* LoadVolumeTexture( const char* fname )
{
	grcTextureFactory::TextureCreateParams params(grcTextureFactory::TextureCreateParams::VIDEO, grcTextureFactory::TextureCreateParams::TILED );
	return rage::grcTextureFactory::GetInstance().Create(fname, &params);
}

void PedDetailManager::Init()
{
	// DX11 TODO: These textures are huge on any platform, should use a more compact format.
	// Also since the assets are in the "common" directory we don't want accidentally waste space on the console versions.
	m_DetailTextureVolume = LoadVolumeTexture("common:/data/ped_detail_atlas" D3D11_ONLY("_32") ORBIS_ONLY("_32"));
	Assert( m_DetailTextureVolume != NULL);
	grcTextureFactory::GetInstance().RegisterTextureReference("__DetailAtlas__", m_DetailTextureVolume);

	m_VehicleDetailTextureVolume = LoadVolumeTexture("common:/data/vehicle_detailAtlas" D3D11_ONLY("_32") ORBIS_ONLY("_32"));
	Assert( m_VehicleDetailTextureVolume != NULL);
	grcTextureFactory::GetInstance().RegisterTextureReference("__VehicleDetailAtlas__", m_VehicleDetailTextureVolume);

	// now setup texture dictionary for detail map
	CModelInfo::SetGlobalResidentTxd(vfxUtil::InitTxd("mapdetail"));
}

void PedDetailManager::Shutdown()
{
	SAFE_RELEASE(m_DetailTextureVolume);
	SAFE_RELEASE(m_VehicleDetailTextureVolume);
	vfxUtil::ShutdownTxd("mapdetail");
}


CPedDamageData::CPedDamageData()
{
	m_BloodColor.Set(0.4f,0.0f,0.0f);
	m_SoakColor.Set(0.5f,0.25f,0.0f);
	
	m_BloodSpecExponent = 1.0f; 
	m_BloodSpecIntensity = 0.125f; 
	m_BloodFresnel = 0.95f;
	m_BloodReflection = 0.6f;

	m_WoundBumpAdjust = 0.4f;

	m_HiResTargetDistanceCutoff = 2.0f; 
	m_MedResTargetDistanceCutoff = 5.0f;
	m_LowResTargetDistanceCutoff = 30.0f;
	m_CutsceneLODDistanceScale = 1.0f;
	m_MaxMedResTargetsPerFrame = 7; 
	m_MaxTotalTargetsPerFrame = 16;
	m_NumWoundsToScarsOnDeathSP = 1;
	m_NumWoundsToScarsOnDeathMP = 1;
	m_MaxPlayerBloodWoundsSP = 7;	 // B*1378283
	m_MaxPlayerBloodWoundsMP = -1;
	m_TattooTintAdjust.Set(0.35f,0.35f,0.35f,1.0f);

}

void CPedDamageData::ClearArrays()
{
	ClearPointerArray(m_BloodData);
	ClearPointerArray(m_DamageDecalData);
	ClearPointerArray(m_DamagePacks);
	ClearPointerArray(m_CylinderInfoSets);
}

void CPedDamageData::Load(const char* fileName)
{
	ClearArrays();
	fwPsoStoreLoader::LoadDataIntoObject(fileName, NULL, *this);
}

void CPedDamageData::AppendUniques(const char* fileName)
{
	CPedDamageData pedDamageData;
	fwPsoStoreLoader::LoadDataIntoObject(fileName, NULL, pedDamageData);

	// Append NEW damage data
	AppendUniqueElements<CPedBloodDamageInfo>(m_BloodData, pedDamageData.m_BloodData);
	AppendUniqueElements<CPedDamageDecalInfo>(m_DamageDecalData, pedDamageData.m_DamageDecalData);
	AppendUniqueElements<CPedDamagePack>(m_DamagePacks, pedDamageData.m_DamagePacks);

	// Clear cylinder info set data.
	ClearPointerArray(pedDamageData.m_CylinderInfoSets);
}

void CPedDamageData::UnloadAppend(const char* fileName)
{
	CPedDamageData pedDamageData;
	fwPsoStoreLoader::LoadDataIntoObject(fileName, NULL, pedDamageData);

	// Delete appended damage data
	CleanupAppendedElements<CPedBloodDamageInfo>(m_BloodData, pedDamageData.m_BloodData);
	CleanupAppendedElements<CPedDamageDecalInfo>(m_DamageDecalData, pedDamageData.m_DamageDecalData);
	CleanupAppendedElements<CPedDamagePack>(m_DamagePacks, pedDamageData.m_DamagePacks);

	// Clear cylinder info set data.
	ClearPointerArray(pedDamageData.m_CylinderInfoSets);
}


void PreLoadTexture(CPedDamageTexture & textureInfo, const char * OUTPUT_ONLY(type), const char * OUTPUT_ONLY(name))
{
	if (!(textureInfo.m_TextureName.IsNull() || textureInfo.m_TextureName.GetHash()==ATSTRINGHASH("None",0x1d632ba1) ||   textureInfo.m_TextureName.GetHash()==ATSTRINGHASH("Null",0x3adb3357)))
	{
		
#if __ASSERT
		Assertf(textureInfo.m_FrameMax == float(int(textureInfo.m_FrameMax)), "CPedDamageData - Damage texture %s, fractions values for m_FrameMax are not allowed",textureInfo.m_TextureName.GetCStr());
		Assertf(textureInfo.m_FrameMin == float(int(textureInfo.m_FrameMin)), "CPedDamageData - Damage texture %s, fractions values for m_FrameMin are not allowed",textureInfo.m_TextureName.GetCStr());
		Assertf(textureInfo.m_AnimationFrameCount == float(int(textureInfo.m_AnimationFrameCount)), "CPedDamageData - Damage texture %s, fractions values for m_AnimationFrameCount are not allowed",textureInfo.m_TextureName.GetCStr());
		Assertf(textureInfo.m_TexGridCols == float(int(textureInfo.m_TexGridCols)), "CPedDamageData - Damage texture %s, fractions values for m_TexGridCols are not allowed",textureInfo.m_TextureName.GetCStr());
		Assertf(textureInfo.m_TexGridRows == float(int(textureInfo.m_TexGridRows)), "CPedDamageData - Damage texture %s, fractions values for m_TexGridRows are not allowed",textureInfo.m_TextureName.GetCStr());

// make sure the animation ranges fit in the 4 bits we have for offset/sequence
		if (textureInfo.m_AnimationFPS>0.0f)
		{
			int totalFrames = 1 + (int)textureInfo.m_FrameMax - (int)textureInfo.m_FrameMin;
			Assertf(textureInfo.m_AnimationFrameCount>0,"CPedDamageData - Damage texture %s is animated (m_AnimationFPS>0.0f) but specifies an animation frame count of 0", textureInfo.m_TextureName.GetCStr());
			
			if (textureInfo.m_AnimationFrameCount==0)
				textureInfo.m_AnimationFrameCount = 1;

			Assertf(totalFrames>=textureInfo.m_AnimationFrameCount,"CPedDamageData - Damage texture %s specifies more animation frames than it has defined by the m_FrameMin and m_FrameMax range", textureInfo.m_TextureName.GetCStr());
			int sequenceCount = totalFrames/(int)textureInfo.m_AnimationFrameCount; 
			Assertf(sequenceCount<=16,"CPedDamageData - Damage texture %s has more than 16 animation sequences specified", textureInfo.m_TextureName.GetCStr());
		}
		else
		{
			Assertf(1+textureInfo.m_FrameMax-textureInfo.m_FrameMin<=16,"CPedDamageData - Damage texture %s has more than 16 random fixed frames supported", textureInfo.m_TextureName.GetCStr());
		}
#endif

		PEDDEBUG_DISPLAYF("CPedDamageData::OnPostLoad() - Loading %s from txd %s", textureInfo.m_TextureName.GetCStr(), textureInfo.m_TxdName.GetCStr());

		char szTdxFilename[RAGE_MAX_PATH];
		const char * txdName = textureInfo.m_TxdName.GetCStr();
			
		if(strchr(txdName,':'))
			safecpy(szTdxFilename,txdName);							// use their path if they gave one
		else
			formatf(szTdxFilename,"platform:/textures/%s",txdName);	// otherwise add this one so we always find the textures correctly

		strLocalIndex txdSlot = strLocalIndex(vfxUtil::InitTxd(szTdxFilename));
	
		if (txdSlot.Get()>=0)
		{
			if(Verifyf(g_TxdStore.Get(txdSlot),"texture dictionary '%s' missing",szTdxFilename))
			{
				textureInfo.m_TxdName.SetFromString(szTdxFilename);
				textureInfo.m_txdId = txdSlot.Get();
				textureInfo.m_pTexture = g_TxdStore.Get(txdSlot)->Lookup( textureInfo.m_TextureName.GetHash());
				g_TxdStore.AddRef(txdSlot, REF_RENDER);
				if (textureInfo.m_pTexture==NULL)
				{
					Errorf("%s for '%s', texture '%s' not found in texture dictionary '%s'", type, name,textureInfo.m_TextureName.TryGetCStr(), textureInfo.m_TxdName.TryGetCStr());
				}
			}
		}
		else
		{
			Errorf("%s for '%s', requested texture '%s' from non existent texture dictionary '%s'", type, name, textureInfo.m_TextureName.TryGetCStr(), textureInfo.m_TxdName.TryGetCStr());
		}
	}
}

void CPedDamageData::SetUniqueTexId(CPedDamageTexture & damageTexture, int index, u16 &nextUniqueId )
{
	u16 uniqueId = 0;

	if (grcTexture * pTex = damageTexture.m_pTexture)
	{
		int i;
		for (i=0;i<=index;i++)  // up to and including 'index', so it can match with it's other textures
		{
			if (m_BloodData[i]->m_SoakTexture.m_pTexture==pTex && m_BloodData[i]->m_SoakTexture.m_uniqueId!=0)
			{
				if (m_BloodData[i]->m_SoakTexture.m_TexGridCols == damageTexture.m_TexGridCols && m_BloodData[i]->m_SoakTexture.m_TexGridRows == damageTexture.m_TexGridRows)
				{
					uniqueId = m_BloodData[i]->m_SoakTexture.m_uniqueId;
					break;
				}
			}
			if (m_BloodData[i]->m_SoakTextureGravity.m_pTexture==pTex && m_BloodData[i]->m_SoakTextureGravity.m_uniqueId!=0)
			{
				if (m_BloodData[i]->m_SoakTextureGravity.m_TexGridCols == damageTexture.m_TexGridCols && m_BloodData[i]->m_SoakTextureGravity.m_TexGridRows == damageTexture.m_TexGridRows)
				{
					uniqueId = m_BloodData[i]->m_SoakTextureGravity.m_uniqueId; 
					break;
				}
			}
			if (m_BloodData[i]->m_WoundTexture.m_pTexture==pTex && m_BloodData[i]->m_WoundTexture.m_uniqueId!=0)
			{
				if (m_BloodData[i]->m_WoundTexture.m_TexGridCols == damageTexture.m_TexGridCols && m_BloodData[i]->m_WoundTexture.m_TexGridRows == damageTexture.m_TexGridRows)
				{
					uniqueId = m_BloodData[i]->m_WoundTexture.m_uniqueId; 
					break;
				}
			}
			if (m_BloodData[i]->m_SplatterTexture.m_pTexture==pTex && m_BloodData[i]->m_SplatterTexture.m_uniqueId!=0) 
			{
				if (m_BloodData[i]->m_SplatterTexture.m_TexGridCols == damageTexture.m_TexGridCols && m_BloodData[i]->m_SplatterTexture.m_TexGridRows == damageTexture.m_TexGridRows)
				{
					uniqueId = m_BloodData[i]->m_SplatterTexture.m_uniqueId;
					break;
				}
			}
		}
		if (i>index)
			uniqueId = nextUniqueId++;
	}
	damageTexture.m_uniqueId = uniqueId;
}

void CPedDamageData::OnPostLoad()
{
	if (m_MaxMedResTargetsPerFrame>kMaxMedResBloodRenderTargets)
		Warningf( "m_MaxMedResTargetsPerFrame>kMaxMedResBloodRenderTargets, m_MaxMedResTargetsPerFrame will be ignored");
	if (m_MaxTotalTargetsPerFrame>kMaxLowResBloodRenderTargets)
		Warningf( "m_MaxTotalTargetsPerFrame>kMaxLowResBloodRenderTargets, m_MaxTotalTargetsPerFrame will be ignored");
	
	u16 nextUniqueID = 1;

	// load all the blood, scar and dirt textures.
	for (int i=0;i<m_BloodData.GetCount(); i++)
	{
#if __FINAL
		const char *pNameString = "";
#else
		const char *pNameString = m_BloodData[i]->m_Name.GetCStr();
#endif	//	__FINAL
		PreLoadTexture(m_BloodData[i]->m_SoakTexture, "CPedBloodDamageInfo m_SoakTetxure", pNameString);
		PreLoadTexture(m_BloodData[i]->m_SoakTextureGravity, "CPedBloodDamageInfo m_SoakTextureGravity", pNameString);
		PreLoadTexture(m_BloodData[i]->m_WoundTexture, "CPedBloodDamageInfo m_WoundTexture", pNameString);
		PreLoadTexture(m_BloodData[i]->m_SplatterTexture, "CPedBloodDamageInfo m_SplatterTexture", pNameString);
		Assertf(m_BloodData[i]->m_WoundTexture.m_pTexture!=NULL,"Required Wound Texture is missing for Blood Damage Data '%s'",pNameString);
		
		SetUniqueTexId(m_BloodData[i]->m_SoakTexture, i, nextUniqueID);
		SetUniqueTexId(m_BloodData[i]->m_SoakTextureGravity, i, nextUniqueID);
		SetUniqueTexId(m_BloodData[i]->m_WoundTexture, i, nextUniqueID);
		SetUniqueTexId(m_BloodData[i]->m_SplatterTexture, i, nextUniqueID);

		Assertf(nextUniqueID<32,"too many unique textures used in blood damage"); // this could be removed if we need to, by using some sort of xor/hash, but really there should not be many!
		
		// calc combined UniqueID
		m_BloodData[i]->m_UniqueTexComboIDGravity = (m_BloodData[i]->m_SoakTextureGravity.m_uniqueId<<10) | (m_BloodData[i]->m_SplatterTexture.m_uniqueId<<5) | (m_BloodData[i]->m_WoundTexture.m_uniqueId);
		m_BloodData[i]->m_UniqueTexComboIDNonGravity = (m_BloodData[i]->m_SoakTexture.m_uniqueId<<10) | (m_BloodData[i]->m_SplatterTexture.m_uniqueId<<5) | (m_BloodData[i]->m_WoundTexture.m_uniqueId);
	
// 		Displayf("m_BloodData[%d (%s)]->m_UniqueTextureIDGravity = 0x%04x",i,pNameString,m_BloodData[i]->m_UniqueTextureIDGravity);
// 		Displayf("m_BloodData[%d (%s)]->m_UniqueTextureIDNonGravity = 0x%04x",i,pNameString,m_BloodData[i]->m_UniqueTextureIDNonGravity);
	}

	nextUniqueID = 1;

	for (int i=0;i<m_DamageDecalData.GetCount(); i++)
	{
#if __FINAL
		const char *pNameString = "";
#else
		const char *pNameString = m_DamageDecalData[i]->m_Name.GetCStr();
#endif	//	__FINAL
		PreLoadTexture(m_DamageDecalData[i]->m_Texture, "CPedDamageDecalInfo m_Texture", pNameString);
		PreLoadTexture(m_DamageDecalData[i]->m_BumpTexture, "CPedDamageDecalInfo m_BumpTexture", pNameString);
		
		// see if if another decoration decal uses the same underlying texture texture
		if (grcTexture * pTex=m_DamageDecalData[i]->m_Texture.m_pTexture)
		{
			int j;
			for (j=0;j<i;j++)
			{
				if (m_DamageDecalData[j]->m_Texture.m_pTexture == pTex)
				{
					if (m_DamageDecalData[i]->m_Texture.m_TexGridCols == m_DamageDecalData[j]->m_Texture.m_TexGridCols && m_DamageDecalData[i]->m_Texture.m_TexGridRows == m_DamageDecalData[j]->m_Texture.m_TexGridRows)
					{
						m_DamageDecalData[i]->m_Texture.m_uniqueId = m_DamageDecalData[j]->m_Texture.m_uniqueId;
						break;
					}
				}
			}

			if(j==i) // could not find one, make new one
				m_DamageDecalData[i]->m_Texture.m_uniqueId = nextUniqueID++;
		}

		if (grcTexture * pTex=m_DamageDecalData[i]->m_BumpTexture.m_pTexture)
		{
			int j;
			for (j=0;j<i;j++)
			{
				if (m_DamageDecalData[j]->m_BumpTexture.m_pTexture == pTex)
				{
					if (m_DamageDecalData[i]->m_BumpTexture.m_TexGridCols == m_DamageDecalData[j]->m_BumpTexture.m_TexGridCols && m_DamageDecalData[i]->m_BumpTexture.m_TexGridRows == m_DamageDecalData[j]->m_BumpTexture.m_TexGridRows)
					{
						m_DamageDecalData[i]->m_BumpTexture.m_uniqueId = m_DamageDecalData[j]->m_BumpTexture.m_uniqueId;
						break;
					}
				}
			}

			if(j==i) // could not find one, make new one
				m_DamageDecalData[i]->m_BumpTexture.m_uniqueId = nextUniqueID++;
		}
	}

	// pre-look up the scar data associated with each type of blood.
	for (int i=0;i<m_BloodData.GetCount(); i++)
	{
		m_BloodData[i]->m_ScarIndex = GetDamageDecalInfoIndex(m_BloodData[i]->m_ScarName);
#if !__FINAL
		if (m_BloodData[i]->m_ScarIndex<0 && !(m_BloodData[i]->m_ScarName.IsNull() || m_BloodData[i]->m_ScarName.GetHash()==ATSTRINGHASH("None",0x1d632ba1) ||  m_BloodData[i]->m_ScarName.GetHash()==ATSTRINGHASH("Null",0x3adb3357)))
			Errorf("blood info for '%s', is referencing a scar named '%s' which does not exist. Please check the names in peddamage.xml",m_BloodData[i]->m_Name.GetCStr(), m_BloodData[i]->m_ScarName.GetCStr());
#endif
	}

	// precalc the minscale for soak (based on wound/splatter range)
	for (int i=0;i<m_BloodData.GetCount(); i++)
	{
		const CPedDamageTexture* splatterTexture = &m_BloodData[i]->m_SplatterTexture;
		if (splatterTexture == NULL || splatterTexture->m_pTexture==NULL) // if there is no splatter texture, use the wound size.
			m_BloodData[i]->m_SoakScaleMin = m_BloodData[i]->m_WoundMinSize / m_BloodData[i]->m_WoundMaxSize;
		else
			m_BloodData[i]->m_SoakScaleMin = m_BloodData[i]->m_SplatterMinSize / m_BloodData[i]->m_SplatterMaxSize;
	}

	// verify the pack info (and preset some flags)
	for (int i=0; i<m_DamagePacks.GetCount(); i++)
		m_DamagePacks[i]->PreCheckType();
}


int  CPedDamageData::GetBloodDamageInfoIndex( atHashWithStringBank bloodNameHash)
{
	for (int bloodIndex=0; bloodIndex<m_BloodData.GetCount(); bloodIndex++)
	{
		if (m_BloodData[bloodIndex]->m_Name.GetHash() == bloodNameHash.GetHash())
			return bloodIndex;
	}
	return -1;
}

int  CPedDamageData::GetDamageDecalInfoIndex(atHashWithStringBank damageDecalNameHash)
{
	for (int damageDecalIndex=0; damageDecalIndex<m_DamageDecalData.GetCount(); damageDecalIndex++)
	{
		if (m_DamageDecalData[damageDecalIndex]->m_Name.GetHash() == damageDecalNameHash.GetHash())
			return damageDecalIndex;
	}
	return -1;
}


CPedBloodDamageInfo* CPedDamageData::GetBloodDamageInfoByIndex(int bloodDamageIndex )
{
	return (bloodDamageIndex>=0) ? m_BloodData[bloodDamageIndex] : NULL;
}

CPedBloodDamageInfo* CPedDamageData::GetBloodDamageInfoByHash( atHashWithStringBank bloodNameHash)
{
	return GetBloodDamageInfoByIndex(GetBloodDamageInfoIndex(bloodNameHash));
}


CPedDamageDecalInfo* CPedDamageData::GetDamageDecalInfoByIndex(int damageDecalIndex)
{
	return (damageDecalIndex>=0) ? m_DamageDecalData[damageDecalIndex] : NULL;
}

CPedDamageDecalInfo* CPedDamageData::GetDamageDecalInfoByHash( atHashWithStringBank damageDecalNameHash)
{
	return GetDamageDecalInfoByIndex(GetDamageDecalInfoIndex(damageDecalNameHash));
}

CPedDamageCylinderInfoSet* CPedDamageData::GetCylinderInfoSetByIndex(int cylInfSetIndex)
{
	if (Verifyf(cylInfSetIndex >=0 && cylInfSetIndex < m_CylinderInfoSets.size(), "CPedDamageData: Ped type not recognized from audio type (PedTypes in game/Peds/rendering/PedDamage.psc must match PedTypes in game/audio/gameobjects.h, and there needs to be a CPedDamageCylinderInfoSet entry in peddamage.xml for each PedType)"))
	{
		cylInfSetIndex = Clamp(cylInfSetIndex, 0, m_CylinderInfoSets.size()-1);
		return m_CylinderInfoSets[cylInfSetIndex];
	}
	else
	{
		return NULL;
	}
}

CPedDamageCylinderInfoSet* CPedDamageData::GetCylinderInfoSetByPedType(PedTypes cylInfSetType)
{
	return GetCylinderInfoSetByIndex(static_cast<int>(cylInfSetType));
}


#if __BANK

void CPedDamageData::SaveXML()
{
	ASSET.PushFolder("common:/data/effects/peds");
	PARSER.SaveObject("PedDamage", "xml", this, parManager::XML);
	ASSET.PopFolder();
}

void CPedDamageData::AddWidgets(rage::bkBank& bank )
{
	PARSER.AddWidgets(bank, this);
}
#endif



CPedDamageTexture::CPedDamageTexture()
{
	m_TextureName="None";
	m_TxdName="None";
	m_TexGridCols=1;
	m_TexGridRows=1;
	m_FrameMin=0;
	m_FrameMax=0;
	m_AnimationFrameCount = 0;
	m_AnimationFPS=0.0f;
	m_CenterOffset.Set(0.0f,0.0f);
	m_pTexture=NULL;
	m_txdId = -1;
	m_uniqueId = 0;
	m_AllowUFlip = false;
	m_AllowVFlip = false;
}


CPedBloodDamageInfo::CPedBloodDamageInfo()
{
	m_Name.SetFromString("None");	

	m_RotationType = kNoRotation;
	m_SyncSoakWithWound = false;

	m_WoundMinSize=1;
	m_WoundMaxSize=1;

	m_WoundIntensityWet = 1.0f;
	m_WoundIntensityDry = 0.75f;
	m_WoundDryingTime = 10.0f;
	
	m_SplatterMinSize=0;
	m_SplatterMaxSize=0;
	m_SplatterIntensityWet = 0.1f;
	m_SplatterIntensityDry = 0.08f;
	m_SplatterDryingTime = 10.0f;

	m_SoakStartSize=0;
	m_SoakEndSize=0;
	m_SoakStartSizeGravity=0;
	m_SoakEndSizeGravity=0;
	m_SoakScaleTime=5.0f;

	m_SoakIntensityWet = 0.25f;
	m_SoakIntensityDry = 0.55f;
	m_SoakDryingTime = 5.0f;

	m_BloodFadeStartTime=400.0f;
	m_BloodFadeTime=10.f;

	m_ScarName.SetFromString("None");			
	m_ScarStartTime=-1.0f;
	
	m_UniqueTexComboIDGravity = 0;
	m_UniqueTexComboIDNonGravity = 0;
}


CPedDamageDecalInfo::CPedDamageDecalInfo()
{
	m_Name.SetFromString("None");
	m_MinSize = 50.0f;
	m_MinStartAlpha = 1.0f;
	m_MaxStartAlpha = 1.0f;
	m_MaxSize = 50.0f;
	m_FadeInTime=0.0f;
	m_FadeOutTime=3.0f;
	m_FadeOutStartTime = 300.0f;
	m_CoverageType = kSkinOnly;
}

CPedDamageDecalInfo::~CPedDamageDecalInfo()
{
}

int CPedDamageTexture::CalcFixedFrameOrSequence(int requested) const
{
	float totalFrames =  1 + m_FrameMax - m_FrameMin;
	
	if(m_AnimationFPS==0.0f)
	{
		return (int)((requested<0) ? s_Random.GetRanged(0.0f, totalFrames) : Min((float)requested,totalFrames));
	}
	else 
	{
		if (totalFrames>m_AnimationFrameCount)	// if there are less frames in the animation than specified by the min max range, we have multiple animations to choose from
		{
			int SequenceCount = (int)(totalFrames/m_AnimationFrameCount);  // how many sequences are available
			return (requested<0) ? s_Random.GetRanged(0, SequenceCount-1) : Min(requested, SequenceCount-1);			// pick one
		}
		else
		{
			return 0;
		}
	}
}


//////////////////////////////////////////////////////////////
void CPedDamagePackEntry::Apply(CPed * pPed, float preAge, float alpha)
{
	if (m_Type == kDamagePackEntryTypeBlood)
	{
		PEDDAMAGEMANAGER.AddPedBlood(pPed, m_Zone, m_uvPos, m_Rotation, m_Scale, m_DamageName, false, preAge, m_ForcedFrame);
	}
	else if (m_Type == kDamagePackEntryTypeDamageDecal)
	{
		PEDDAMAGEMANAGER.AddPedDamageDecal( pPed, m_Zone, m_uvPos, m_Rotation, m_Scale, m_DamageName, false, m_ForcedFrame, alpha, preAge);
	}
}


// called after loading so we don't have to check every time
void CPedDamagePackEntry::PreCheckType(const char * ASSERT_ONLY(packName))
{
	m_Type = kDamageTypeInvalid;			// not in parser file

	if (PEDDAMAGEMANAGER.GetDamageData().GetBloodDamageInfoIndex(m_DamageName) >= 0)
		m_Type = kDamagePackEntryTypeBlood;
	else if (PEDDAMAGEMANAGER.GetDamageData().GetDamageDecalInfoIndex(m_DamageName) >= 0)
		m_Type = kDamagePackEntryTypeDamageDecal;

#if __ASSERT
	Assertf(m_Type != kDamageTypeInvalid, "Ped Damage Pack %s, has an invalid entry(%s)", packName, m_DamageName.GetCStr());
	
	// also check frame indexes
	if (m_Type == kDamagePackEntryTypeBlood)
	{
		CPedBloodDamageInfo * info  = PEDDAMAGEMANAGER.GetDamageData().GetBloodDamageInfoByHash(m_DamageName);
		int maxFrame = int((info->m_SplatterTexture.m_pTexture!=NULL) ? (info->m_SplatterTexture.m_FrameMax - info->m_SplatterTexture.m_FrameMin) : (info->m_WoundTexture.m_FrameMax - info->m_WoundTexture.m_FrameMin));
		Assertf(m_ForcedFrame==-1 || m_ForcedFrame <= maxFrame, "Ped Damage Pack '%s' has an entry using '%s' with an invalid force frame value (%d), the max for that '%s' is %d", packName, m_DamageName.GetCStr(), m_ForcedFrame, m_DamageName.GetCStr(), maxFrame); 
	}
	else if ( m_Type == kDamagePackEntryTypeDamageDecal)
	{
		CPedDamageDecalInfo * info  = PEDDAMAGEMANAGER.GetDamageData().GetDamageDecalInfoByHash(m_DamageName);
		Assertf(m_ForcedFrame==-1 || m_ForcedFrame <= (info->m_Texture.m_FrameMax-info->m_Texture.m_FrameMin), "Ped Damage Pack '%s' has an entry using '%s' with an invalid force frame value (%d), the max for that '%s' is %f", packName, m_DamageName.GetCStr(), m_ForcedFrame, m_DamageName.GetCStr(), info->m_Texture.m_FrameMax-info->m_Texture.m_FrameMin); 
	}
#endif
	
}

void CPedDamagePack::Apply( CPed * pPed, float preAge, float alpha)
{
	for (int i=0;i<m_Entries.GetCount();i++)
		m_Entries[i].Apply( pPed, preAge, alpha);

	if (NetworkInterface::IsGameInProgress() && pPed && pPed->IsLocalPlayer() && pPed->GetNetworkObject())
	{
		//Replicate to the remote invocation of the player
		CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject());
		if (pNetObjPlayer)
		{
			pNetObjPlayer->SetNetworkedDamagePack(m_Name);
		}
	}
}

void CPedDamagePack::PreCheckType()
{
 	for (int i=0;i<m_Entries.GetCount();i++)
 	{
#if __ASSERT
		m_Entries[i].PreCheckType( m_Name.GetCStr());
#else
		m_Entries[i].PreCheckType( NULL);
#endif
	}
}

#if __BANK
void CPedBloodDamageInfo::PreAddWidgets(bkBank& bank)  {bank.PushGroup(m_Name.GetCStr(), false);}
void CPedBloodDamageInfo::PostAddWidgets(bkBank& bank) {bank.PopGroup();}
void CPedDamageDecalInfo::PreAddWidgets(bkBank& bank)  {bank.PushGroup(m_Name.GetCStr(), false);}
void CPedDamageDecalInfo::PostAddWidgets(bkBank& bank) {bank.PopGroup();}
#endif

bool CCompressedTextureManager::Init(bool bUseUncompressedTextures, int rtWidth, int rtHeight)
{
	m_availableTextures.Reset();
	m_texturePool.Reset();
	m_bEnableUncompressedTextures = bUseUncompressedTextures;

	for (int i = 0; i < kMaxCompressedTextures; i++)
	{
		CompressedTextureInfo info;
		char name[RAGE_MAX_PATH];
		if (PEDDECORATIONBUILDER.IsUsingHiResCompressedMPDecorations())
			formatf(name,RAGE_MAX_PATH,"tat_rt_%03d_a_uni_hires", i);
		else
			formatf(name,RAGE_MAX_PATH,"tat_rt_%03d_a_uni", i);

		info.bInUse = false;
		info.txdHash = atHashString(name);

		info.textureHash = info.txdHash;

		m_texturePool.Push(info);
		m_availableTextures.Push(&m_texturePool[i]);

		m_renderTargetPool[i] = NULL;
	}

	return AllocateRenderTargets(rtWidth, rtHeight);
}

grcRenderTarget* CCompressedTextureManager::GetRenderTarget(atHashString& txdName)
{
	for (int i = 0; i < m_texturePool.GetCount(); i++)
	{
		const CompressedTextureInfo& info = m_texturePool[i];
		if (info.bInUse && info.txdHash == txdName)
			return info.pRT;
	}

	return NULL;
}

bool CCompressedTextureManager::AllocateRenderTargets(int iWidth, int iHeight)
{
	if (m_bEnableUncompressedTextures == false)
		return true;

	//Create render targets.
	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.Format = grctfA8R8G8B8;
	int bpp = 32;

	if (Verifyf(m_texturePool.GetCount() == kMaxCompressedTextures, "Cannot allocate render targets, texture pool is possibly unitialised (%d)", m_texturePool.GetCount()))
	{
		for (int i = 0; i < kMaxCompressedTextures; i++)
		{
			CompressedTextureInfo& info = m_texturePool[i];
			m_renderTargetPool[i] = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, info.txdHash.TryGetCStr(), grcrtPermanent, iWidth, iHeight, bpp, &params, 0 WIN32PC_ONLY(, m_renderTargetPool[i]));
			info.pRT = m_renderTargetPool[i];

			if (m_renderTargetPool[i] == NULL)
				return false;
		}

		return true;
	}

	return false;
}

void CCompressedTextureManager::ReleaseRenderTargets()
{
	if (m_bEnableUncompressedTextures == false)
		return;

	bool bPatchTexturePool = (m_texturePool.GetCount() == kMaxCompressedTextures);

	for (int i = 0; i < kMaxCompressedTextures; i++)
	{
		if (m_renderTargetPool[i])
		{
			m_renderTargetPool[i]->Release();
			m_renderTargetPool[i] = NULL;

			if (bPatchTexturePool)
			{
				m_texturePool[i].pRT = NULL;
			}
		}
	}
}

void CCompressedTextureManager::Shutdown()
{
	ReleaseRenderTargets();
	m_availableTextures.Reset();
	m_texturePool.Reset();
}

bool CCompressedTextureManager::Alloc(atHashString& txdName, atHashString& txtName)
{
	if (m_availableTextures.GetCount() == 0)
	{
		Warningf("CCompressedTextureManager::Alloc: ran out of texture slots");
		txdName = atHashString(0U);
		txtName = txdName;
		return false;
	}

	CompressedTextureInfo* pInfo = m_availableTextures.Pop();
	Assert(pInfo && pInfo->bInUse == false);
	pInfo->bInUse = true;

	txdName = pInfo->txdHash;
	txtName = pInfo->textureHash;

	CPEDDEBUG_DISPLAYF("CCompressedTextureManager::Alloc(): txdName=%s", txdName.GetCStr());	

	return true;
}

void CCompressedTextureManager::Release(atHashString txdHash)
{
	int i = Find(txdHash);

	if (i < 0 || i > m_texturePool.GetCount())
	{
		return;
	}

	m_texturePool[i].bInUse = false;
	m_availableTextures.PushTop(&m_texturePool[i]);
	
	CPEDDEBUG_DISPLAYF("CCompressedTextureManager::Release(): txdHash=%s", txdHash.GetCStr());	
}

int CCompressedTextureManager::Find(atHashString txdHash) const
{
	for (int i = 0; i < m_texturePool.GetCount(); i++)
	{
		if (m_texturePool[i].txdHash == txdHash)
		{
			return i;
		}
	}

	return -1;
}
