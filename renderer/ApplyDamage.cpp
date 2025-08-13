//
// filename:	ApplyDamage.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "ApplyDamage.h"

#if GPU_DAMAGE_WRITE_ENABLED

// RAGE headers
#include "profile/timebars.h"
#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "grcore/gfxcontext.h"
#include "grcore/im.h"
#include "grcore/texture.h"
#include "grcore/image.h"
#include "grcore/viewport.h"
#include "grmodel/shaderfx.h"
#include "bank/bank.h"
#include "input/mouse.h"
#include "fwscene/world/SceneGraphNode.h"
#include "fwrenderer/renderthread.h"
#include "system/xtl.h"
#include "math/vecrand.h"
#include "phbound/surfacegrid.h"

//For bank
#include "atl/array.h"

#if __PPU
#	include "grcore/wrapper_gcm.h"
#endif

#if __D3D
#	include "system/d3d9.h"
#	if __XENON
#		include "system/xtl.h"
#		define DBG 0			// yuck
#		include <xgraphics.h>
#	endif
#endif

// Game headers
#include "scene/Building.h"
#include "scene/AnimatedBuilding.h"
#include "Objects/DummyObject.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "pickups/Pickup.h"
#include "Vehicles/vehicle.h"
#include "Renderer/Util/ShaderUtil.h"
#include "Renderer/sprite2d.h"
#include "system/system.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicleDamage.h"
#include "vehicles/VehicleFactory.h"
#include "control/replay/ReplayExtensions.h"

#if __XENON
#include "grcore/texturexenon.h"
#endif

#if RSG_PC
#include "fwdrawlist/drawlistmgr.h" // for gDrawListMgr
#endif

RENDER_OPTIMISATIONS()

// PC polls on the map return value of the damage texture, so the fence is redundant. Consoles need it though.
#define ADD_FENCE_TO_DAMAGE ((RSG_ORBIS * 1) || (RSG_DURANGO * 1) || (RSG_PC * 0) || (RSG_XENON * 0) || (RSG_PS3 * 0))
// Switches to help SLI issues investigation
#define GPU_DAMAGE_LOG		(0 && __DEV && RSG_PC)
#define GPU_DAMAGE_DUMP		(0 && __DEV && RSG_PC)

// --- Constants ----------------------------------------------------------------
bool CApplyDamage::ms_bApplyDamageRenderPassEnabled = false; // this must be false while the game is loading
grcBlendStateHandle CApplyDamage::m_ApplyDamageBlendState;
rage::sysIpcMutex CApplyDamage::sm_Mutex = NULL;

#if RSG_PC
int CApplyDamage::ms_iGPUCount = 1;
bool CApplyDamage::ms_bCurrentFrame = true;

#if MULTIPLE_RENDER_THREADS
/*static*/ void CApplyDamage::PrimaryRenderThreadBeginFrameCallback(void* UNUSED_PARAM(userdata))
{
	CApplyDamage::DrawDamage();
	CApplyDamage::Update();
}
#endif // MULTIPLE_RENDER_THREADS

/*static*/ bool CApplyDamage::UseSeparateRenderTarget()
{
#if VEHICLE_ROLLING_TEXTURE_ARRAY
	return true;
#else
	return GRCDEVICE.GetGPUCount(true) > 1;
#endif
}

#endif // RSG_PC

static grmShader *s_ApplyDamageShader = NULL;

class DamageDrawItemPool;

class DamageDrawItem
{
private:

	// Only DamageDrawItemPool is allowed to actually call the ctor and dtor.
	friend class DamageDrawItemPool;

	DamageDrawItem()
	{
		m_Damage.Zero();
		m_DamageOffsetUV.Zero();
		m_ResetDamage = false;
		m_DrawCount   = 0;

#if RSG_PC
		m_GpuDoneMask = 0;
#endif

#if ADD_FENCE_TO_DAMAGE
		m_FenceBusy = 0;
#endif

		// No need to initialize this, DamageDrawItemPool initializes it.
		//m_Next = NULL;
	}

#if ADD_FENCE_TO_DAMAGE && __ASSERT
	~DamageDrawItem();
#endif

public:

	void Init(const Vector4& damage, const Vector4& offset, bool resetDamage BANK_ONLY(, CVehicle *pOwner));

	Vector4 m_Damage;
	Vector4 m_DamageOffsetUV;
	bool    m_ResetDamage;
	int     m_DrawCount;

#if RSG_PC
	bool IsGpuDone() const;
	void SetGpuDone();
	bool AllGpuDone() const;

	u8		m_GpuDoneMask;
#endif

#if ADD_FENCE_TO_DAMAGE
	// Even though we only write 32-bits from the GPU, must be 64-bit aligned,
	// so simplest just to use a u64.  Because the GPU is writing to this, the
	// DamageDrawItem struct must be allocated from ONION/WB memory (ie, "game
	// virtual").
	volatile u64 m_FenceBusy;
#endif

#if __BANK
	enum eInPoolState
	{
		FREE,
		USED,
		TO_BE_DELETE
	};
	CVehicle *m_pParentVehicle;
	eInPoolState m_eState;
#endif

	DamageDrawItem *m_Next;
};

void DamageDrawItem::Init(const Vector4& vecDamage, const Vector4& vecOffset, bool resetDamage BANK_ONLY(, CVehicle *pOwner))
{
	if (resetDamage)
	{
		m_Damage.Zero();
		m_DamageOffsetUV.Zero();
	}
	else
	{
		m_Damage = vecDamage;

		Vector3 offset;
		vecOffset.GetVector3(offset);

		ScalarV fTexRange = CVehicleDeformation::GetTexRange(offset, vecDamage.w, false);
		m_Damage.w = fTexRange.Getf();

		Vec4V vOffset = VECTOR3_TO_VEC4V(vecOffset);
		Vec4V damageOffsetUV = Vec4V(CVehicleDeformation::GetDamageTexCoordFromOffset(vOffset.GetIntrin128(), false));
		m_DamageOffsetUV = VEC4V_TO_VECTOR4(damageOffsetUV);
	}

	m_ResetDamage = resetDamage;
	m_DrawCount   = 0;

#if RSG_PC
	m_GpuDoneMask = 0;
#endif

#if ADD_FENCE_TO_DAMAGE
	Assert(!m_FenceBusy);
#endif

#if __BANK
	m_pParentVehicle = pOwner;
#endif
}

#if ADD_FENCE_TO_DAMAGE && __ASSERT
DamageDrawItem::~DamageDrawItem()
{
	Assert(!m_FenceBusy);
}
#endif

#if RSG_PC
bool DamageDrawItem::IsGpuDone() const
{
	const u32 gpu = GRCDEVICE.GPUIndex();
	return (m_GpuDoneMask & (1<<gpu)) != 0;
}

void DamageDrawItem::SetGpuDone()
{
	const u32 gpu = GRCDEVICE.GPUIndex();
	Assertf(!(m_GpuDoneMask & (1<<gpu)), "Compressed ped damage is processing twice on the same GPU");
	m_GpuDoneMask |= (1<<gpu);
}

bool DamageDrawItem::AllGpuDone() const
{
	const u8 gpuMask = (1 << GRCDEVICE.GetGPUCount(true)) - 1;
	return (m_GpuDoneMask & gpuMask) == gpuMask;
}
#endif

class DamageDrawItemPool
{
public:

	explicit DamageDrawItemPool(unsigned numEntries)
	{
		Assert(numEntries);
		m_nTotalNumOfItmes = numEntries;

		// rage_new allocates from "game virtual", which means for PS4 & XB1,
		// the memory is ONION/WB, which is required for the GPU fences.
		m_All = rage_new DamageDrawItem[numEntries];

		// Add all items to the free list.
		for (unsigned i=0; i<numEntries-1; ++i)
		{
			m_All[i].m_Next = m_All+i+1;
#if __BANK
			m_All[i].m_eState = DamageDrawItem::FREE;
#endif
		}
		m_All[numEntries-1].m_Next = NULL;
		m_Free = m_All;

#if ADD_FENCE_TO_DAMAGE
		m_DelayedDelete = NULL;
#endif
	}

	~DamageDrawItemPool()
	{
		delete[] m_All;
	}

	DamageDrawItem *Alloc()
	{
		SYS_CS_SYNC(m_Mutex);

#if ADD_FENCE_TO_DAMAGE
		if (!m_Free)
		{
			CheckDelayedDeletes();
		}
#endif
		if (!m_Free)
		{
			Warningf("Ran out of DamageDrawItems");
			return NULL;
		}
		DamageDrawItem *ret = m_Free;
		m_Free = ret->m_Next;
#if __BANK
		ret->m_eState = DamageDrawItem::USED;
#endif
		return ret;
	}

	void DelayedDelete(DamageDrawItem *item)
	{
		SYS_CS_SYNC(m_Mutex);

#if ADD_FENCE_TO_DAMAGE
		if (item->m_FenceBusy)
		{
			item->m_Next = m_DelayedDelete;
			m_DelayedDelete = item;
#if __BANK
			m_DelayedDelete->m_eState = DamageDrawItem::TO_BE_DELETE;
#endif
		}
		else
#endif
		{
			item->m_Next = m_Free;
			m_Free = item;
#if __BANK
			m_Free->m_eState = DamageDrawItem::FREE;
#endif
		}
	}

private:
#if __BANK
public:
#endif
	int m_nTotalNumOfItmes;
	sysCriticalSectionToken     m_Mutex;
	DamageDrawItem             *m_All;
	DamageDrawItem             *m_Free;

#if ADD_FENCE_TO_DAMAGE
	DamageDrawItem             *m_DelayedDelete;

	void CheckDelayedDeletes()
	{
		DamageDrawItem **prev = &m_DelayedDelete;
		DamageDrawItem *curr = m_DelayedDelete;
		while (curr)
		{
			DamageDrawItem *next = curr->m_Next;
			if (curr->m_FenceBusy)
			{
				prev = &(curr->m_Next);
			}
			else
			{
				*prev = next;
				curr->m_Next = m_Free;
				m_Free = curr;
#if __BANK
				m_Free->m_eState = DamageDrawItem::FREE;
#endif
			}
			curr = next;
		}
	}
#endif
};

static DamageDrawItemPool g_DamageDrawItemPool(16/*64*/);

DamageDrawVehicle::DamageDrawVehicle(CVehicle* pVehicle)
{
	m_DamageRT      = NULL;
	m_DamageTexture = NULL;
#if VEHICLE_ROLLING_TEXTURE_ARRAY
	m_pTexCache = NULL;
#endif
	Assert(pVehicle && pVehicle->GetVehicleDamage() && pVehicle->GetVehicleDamage()->GetDeformation() && pVehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture());
	m_pVehicle = pVehicle;
	if (pVehicle && pVehicle->GetVehicleDamage()->GetDeformation())
	{
		m_DamageRT = pVehicle->GetVehicleDamage()->GetDeformation()->GetDamageRenderTarget(); // write into this render target, which is never locked in the main game thread
		m_DamageTexture = pVehicle->GetVehicleDamage()->GetDeformation()->GetDamageTexture(); // get the underlying texture so we can copy the resource back from GPU to CPU (PC Only)

#if VEHICLE_ROLLING_TEXTURE_ARRAY
		m_pTexCache = pVehicle->GetVehicleDamage()->GetDeformation()->GetTexCache();
#endif
	}
	m_DamageDrawItems.Reset();
}

DamageDrawVehicle::~DamageDrawVehicle()
{
	const unsigned numItems = m_DamageDrawItems.GetCount();
	for (unsigned i=0; i<numItems; ++i)
	{
		g_DamageDrawItemPool.DelayedDelete(m_DamageDrawItems[i]);
	}
	m_DamageDrawItems.Reset();
}

// --- Structure/Class Definitions ----------------------------------------------
class CApplyDamageDraw
{
public:
	void PreDebugRender(grcRenderTarget *texture, bool filtering, grcBlendStateHandle blendState, grcDepthStencilStateHandle depthState)
	{
		if(!texture)
			return;

		CSprite2d::CustomShaderType type = CSprite2d::CS_BLIT;
		switch(texture->GetType())
		{
		case grcrtDepthBuffer:
		case grcrtShadowMap:
			type = CSprite2d::CS_BLIT_DEPTH;
			break;

		default:
			type = (filtering ? CSprite2d::CS_BLIT : CSprite2d::CS_BLIT_NOFILTERING);
			break;
		}

		static const Vector4 sBlitColorMultParams(1.0f, 1.0f, 1.0f, 1.0f);
		mBlit.SetTexture(texture);
		mBlit.SetGeneralParams(sBlitColorMultParams, Vector4(0.0f, 0.0, 0.0, 0.0));

		grcStateBlock::SetBlendState(blendState);
		grcStateBlock::SetDepthStencilState(depthState);

		mBlit.BeginCustomList(type, texture);
	}

	void PostDebugRender()
	{
		mBlit.EndCustomList();
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	}

	static void DrawFullscreen()
	{
		DrawFullscreen(Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	}

	static void DrawFullscreen(const Vector2 &uv1, const Vector2 &uv2)
	{
		grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, uv1.x, uv1.y, uv2.x, uv2.y, Color32(0,0,0,0));
	}

	static void DrawScreenQuad(const Vector2 &pos, const Vector2 &size)
	{
		const float sx = 1.0f / (float)GRCDEVICE.GetWidth();
		const float sy = 1.0f / (float)GRCDEVICE.GetHeight();

		float x1 = pos.x;
		float y1 = GRCDEVICE.GetHeight() - pos.y;
		float x2 = (pos.x + size.x);
		float y2 = GRCDEVICE.GetHeight() - (pos.y + size.y);

		Vector4 normCoords(x1 * sx, y1 * sy, x2 * sx, y2 * sy); //Transform to [0, 1] space.
		normCoords = (normCoords - Vector4(0.5f, 0.5f, 0.5f, 0.5f)) * Vector4(2.0f, 2.0f, 2.0f, 2.0f); //Transform to [-1, 1] space.

		grcDrawSingleQuadf(normCoords.x, normCoords.y, normCoords.z, normCoords.w, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(255,255,255,255));
	}

private:
	CSprite2d mBlit;
};


// --- Globals ------------------------------------------------------------------

// --- Static Globals ----------------------------------------------------------

// --- Static class members -----------------------------------------------------
CApplyDamage* CApplyDamage::s_pApplyDamage = NULL;
grcTexture* CApplyDamage::ms_NoiseTexture  = NULL;

// --- Code ---------------------------------------------------------------------

//--------------------------------------------------------------------
CApplyDamage::CApplyDamage()
{
	sm_Mutex = rage::sysIpcCreateMutex();
	unsigned short numVehiclesInMap = MAX_DAMAGED_VEHICLES;

#if RSG_PC
	ms_iGPUCount     = Min(MAX_GPUS, (int)GRCDEVICE.GetGPUCount());
	ms_bCurrentFrame = false;
	numVehiclesInMap *= (unsigned short)ms_iGPUCount;
#endif

	m_DamagedVehicleMap.Recompute(numVehiclesInMap);

	grcBlendStateDesc applyDamageBlendStateDesc;
	applyDamageBlendStateDesc.BlendRTDesc[0].BlendEnable	= 1;
	applyDamageBlendStateDesc.BlendRTDesc[0].SrcBlend		= grcRSV::BLEND_ONE;
	applyDamageBlendStateDesc.BlendRTDesc[0].DestBlend		= grcRSV::BLEND_ONE;
	applyDamageBlendStateDesc.BlendRTDesc[0].SrcBlendAlpha	= grcRSV::BLEND_ONE;
	applyDamageBlendStateDesc.BlendRTDesc[0].DestBlendAlpha	= grcRSV::BLEND_ONE;
	applyDamageBlendStateDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;

	m_ApplyDamageBlendState									= grcStateBlock::CreateBlendState(applyDamageBlendStateDesc);

	m_ApplyDamageVar	   = grcevNONE;
	m_ApplyDamageOffsetVar = grcevNONE;
	m_NoiseTextureVar	   = grcevNONE;
	m_DamageCopyTextureVar = grcevNONE;

	s_ApplyDamageShader = grmShaderFactory::GetInstance().Create();
	Assert(s_ApplyDamageShader);

	if( s_ApplyDamageShader )
	{
		s_ApplyDamageShader->Load("vehicle_apply_damage");

		m_ApplyDamageTechnique	= s_ApplyDamageShader->LookupTechnique("ApplyDamage");
		m_ApplyDamageVar		= s_ApplyDamageShader->LookupVar("GeneralParams0");     // x,y,z = normalized damage color intensity, w = radius
		m_ApplyDamageOffsetVar	= s_ApplyDamageShader->LookupVar("GeneralParams1");     //  x,y = u,v center of damage offset, y,z = unused
		m_NoiseTextureVar	    = s_ApplyDamageShader->LookupVar("ApplyDamageTexture");
		m_DamageCopyTextureVar	= s_ApplyDamageShader->LookupVar("ApplyDamageTexture"); // re-use the same var to copy damage over

		Assert(m_ApplyDamageOffsetVar != grcevNONE);
		Assert(m_ApplyDamageVar       != grcevNONE);
		Assert(m_NoiseTextureVar      != grcevNONE);
		Assert(m_DamageCopyTextureVar != grcevNONE);
	}

#if RSG_PC && MULTIPLE_RENDER_THREADS
	Assert(gDrawListMgr);
	if (gDrawListMgr)
	{
		gDrawListMgr->AddPrimaryRenderThreadBeginFrameCallback(PrimaryRenderThreadBeginFrameCallback, NULL);
	}
#endif
}

CApplyDamage::~CApplyDamage()
{
	sysIpcDeleteMutex(sm_Mutex);

	m_DamagedVehicleMap.Reset();

	if(s_ApplyDamageShader)
	{
		s_ApplyDamageShader->SetVar(s_pApplyDamage->m_NoiseTextureVar,	(grcTexture*)NULL);

		delete s_ApplyDamageShader;
		s_ApplyDamageShader = NULL;
	}

	if (ms_NoiseTexture)
	{
		ms_NoiseTexture->Release();
		ms_NoiseTexture = NULL;
	}

	if (m_ApplyDamageBlendState)
	{
		grcStateBlock::DestroyBlendState(m_ApplyDamageBlendState);
	}

#if RSG_PC && MULTIPLE_RENDER_THREADS
	Assert(gDrawListMgr);
	if (gDrawListMgr)
	{
		gDrawListMgr->RemovePrimaryRenderThreadBeginFrameCallback(PrimaryRenderThreadBeginFrameCallback, NULL);
	}
#endif
}

void CApplyDamage::InitializePerlinNoiseTexture()
{
	if (ms_NoiseTexture)
	{
		return;
	}

	const ScalarV noiseScale = BANK_SWITCH(CVehicleDeformation::ms_fVehDefColVarAddMax, VEHICLE_DEFORMATION_COLOR_VAR_ADD_MAX);
	const ScalarV noiseFraction = Min(ScalarV(V_FLT_SMALL_1), noiseScale);
	const ScalarV noiseRange =  Clamp(BANK_SWITCH(CVehicleDeformation::ms_fVehDefColVarMultMax, VEHICLE_DEFORMATION_COLOR_VAR_MULT_MAX) - BANK_SWITCH(CVehicleDeformation::ms_fVehDefColVarMultMin, VEHICLE_DEFORMATION_COLOR_VAR_MULT_MIN), 
		ScalarV(V_ZERO), ScalarV(V_ONE));

	grcImage* pNoiseImage = grcImage::Create(VEHICLE_DEFORMATION_NOISE_WIDTH, VEHICLE_DEFORMATION_NOISE_HEIGHT, 1, grcImage::R32F, grcImage::STANDARD, 0, 0);

	for(int y=0; y < VEHICLE_DEFORMATION_NOISE_HEIGHT; ++y)
	{
		for(int x=0; x < VEHICLE_DEFORMATION_NOISE_WIDTH; x += 4)
		{
			Vec4V randomNoise = CVehicleDeformation::GetSmoothedPerlinNoise(x,y);
			Vec4V noiseMult = (randomNoise * Vec4V(noiseRange)) + Vec4V(BANK_SWITCH(CVehicleDeformation::ms_fVehDefColVarMultMin, VEHICLE_DEFORMATION_COLOR_VAR_MULT_MIN));
			Vec4V noiseToAdd = (randomNoise - Vec4V(V_HALF)) * Vec4V(noiseFraction);
			noiseMult += noiseToAdd;

			float noise = noiseMult.GetX().Getf();
			pNoiseImage->SetPixel(x, y, *(u32*)&noise);

			noise = noiseMult.GetY().Getf();
			pNoiseImage->SetPixel(x+1, y, *(u32*)&noise);

			noise = noiseMult.GetZ().Getf();
			pNoiseImage->SetPixel(x+2, y, *(u32*)&noise);

			noise = noiseMult.GetW().Getf();
			pNoiseImage->SetPixel(x+3, y, *(u32*)&noise);
		}
	}

	BANK_ONLY(grcTexture::SetCustomLoadName("Vehicle Noise");)
	grcTextureFactory::TextureCreateParams param(grcTextureFactory::TextureCreateParams::SYSTEM, grcTextureFactory::TextureCreateParams::LINEAR);
	ms_NoiseTexture = grcTextureFactory::GetInstance().Create(pNoiseImage, &param);
	pNoiseImage->Release();
	BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
}


void CApplyDamage::ResetDamage(CVehicle* pVehicle)
{
	EnqueueDamage(pVehicle, true);
    pVehicle->m_bHasDeformationBeenApplied = false;
}

void CApplyDamage::EnqueueDamage(CVehicle* pVehicle, bool resetDamage)
{	
	if (!s_pApplyDamage || !pVehicle)
	{
		return;
	}

	bool somethingToDo = (pVehicle->GetVehicleDamage()->GetDeformation()->GetImpactStoreCount() > 0 || resetDamage);

	if (!somethingToDo)
	{
		//Displayf("EnqueueDamage - NOTHING TO DO");
		return;
	}

    pVehicle->m_bHasDeformationBeenApplied = true;

	rage::sysIpcLockMutex(sm_Mutex);
	DamageDrawVehicle *pNewDamagedVehicle = InsertDamageIntoMap(s_pApplyDamage->m_DamagedVehicleMap, pVehicle);
	
	CVehicleDamage* pVehDamage = pVehicle->GetVehicleDamage();
	Assert(pVehDamage);

	CVehicleDeformation* pVehDeformation = pVehDamage ? pVehDamage->GetDeformation() : NULL;
	Assert(pVehDeformation);

	if (!pVehDeformation) { rage::sysIpcUnlockMutex(sm_Mutex); return; } // fatal error

	s32 impactStoreCount = pVehDeformation->GetImpactStoreCount();

	if (resetDamage)
	{
		if (!pNewDamagedVehicle->m_DamageDrawItems.IsFull())
		{
			DamageDrawItem* item = g_DamageDrawItemPool.Alloc();
			if (item)
			{
				Vector4 vUnused;
				item->Init(vUnused, vUnused, true BANK_ONLY(, pVehicle));
				pNewDamagedVehicle->m_DamageDrawItems.Push(item);
			}
#if __BANK
			else
			{
				Warningf("Deformation reset is not processed due to running out of DamageDrawItems pool, vehicle %s", pVehicle ? pVehicle->GetDebugNameFromObjectID() : "");
			}
#endif
		}
#if __BANK
		else
		{
			Warningf("Deformation reset is not processed due to running out of DamageDrawItems queue, vehicle %s", pVehicle ? pVehicle->GetDebugNameFromObjectID() : "");
		}
#endif
	}

	for (s32 impactIndex = 0; impactIndex < impactStoreCount; ++impactIndex)
	{
		if (!pNewDamagedVehicle->m_DamageDrawItems.IsFull())
		{
			DamageDrawItem* item = g_DamageDrawItemPool.Alloc();
			if (item)
			{
				item->Init(pVehDeformation->GetImpactDamage((u32)impactIndex), pVehDeformation->GetImpactOffset((u32)impactIndex), false BANK_ONLY(, pVehicle));
				pNewDamagedVehicle->m_DamageDrawItems.Push(item);
			}
			else
			{
#if __BANK
				Warningf("Deformation is not processed due to running out of DamageDrawItems pool, vehicle %s, damage [%3.2f,%3.2f,%3.2f,%3.2f], offset [%3.2f,%3.2f,%3.2f,%3.2f]", 
					(pVehicle ? pVehicle->GetDebugNameFromObjectID() : ""),
					pVehDeformation->GetImpactDamage((u32)impactIndex).x, pVehDeformation->GetImpactDamage((u32)impactIndex).y, 
					pVehDeformation->GetImpactDamage((u32)impactIndex).z, pVehDeformation->GetImpactDamage((u32)impactIndex).w,
					pVehDeformation->GetImpactOffset((u32)impactIndex).x, pVehDeformation->GetImpactOffset((u32)impactIndex).y, 
					pVehDeformation->GetImpactOffset((u32)impactIndex).z, pVehDeformation->GetImpactOffset((u32)impactIndex).w);
#endif
				break;
			}
		}
		else
		{
#if __BANK
			Warningf("Deformation is not processed due to running out of DamageDrawItems queue, vehicle %s, damage [%3.2f,%3.2f,%3.2f,%3.2f], offset [%3.2f,%3.2f,%3.2f,%3.2f]", 
				(pVehicle ? pVehicle->GetDebugNameFromObjectID() : ""),
				pVehDeformation->GetImpactDamage((u32)impactIndex).x, pVehDeformation->GetImpactDamage((u32)impactIndex).y, 
				pVehDeformation->GetImpactDamage((u32)impactIndex).z, pVehDeformation->GetImpactDamage((u32)impactIndex).w,
				pVehDeformation->GetImpactOffset((u32)impactIndex).x, pVehDeformation->GetImpactOffset((u32)impactIndex).y, 
				pVehDeformation->GetImpactOffset((u32)impactIndex).z, pVehDeformation->GetImpactOffset((u32)impactIndex).w);
#endif
			break;
		}
	}

	Assertf(pNewDamagedVehicle->m_DamageDrawItems.GetCount() == CApplyDamage::GetNumDamageDrawItemsInPool(pVehicle), "CApplyDamage::EnqueueDamage draw items in list is not matching to draw items in pool");

	pVehDeformation->ClearStoredImpacts();
	
	rage::sysIpcUnlockMutex(sm_Mutex);
}

int CApplyDamage::GetNumDamagePending(CVehicle *pVehicle)
{
	atMap<CVehicle*,DamageDrawVehicle>& damagedVehicleMap = s_pApplyDamage->m_DamagedVehicleMap;

	if (DamageDrawVehicle *pDamageDrawVehicle = damagedVehicleMap.Access(pVehicle))
	{
		return pDamageDrawVehicle->m_DamageDrawItems.GetCount();
	}
	return 0;
}

DamageDrawVehicle *CApplyDamage::InsertDamageIntoMap(atMap<CVehicle*, DamageDrawVehicle>& mapToInsertTo, CVehicle *pVehicle)
{
	//Check if it's in the map already
	if (mapToInsertTo.Access(pVehicle) == NULL)
	{
		mapToInsertTo.Insert(pVehicle, DamageDrawVehicle(pVehicle));
	}
	return &mapToInsertTo[pVehicle];
}


void CApplyDamage::Init()
{
	if (!s_pApplyDamage)
	{
		s_pApplyDamage = rage_new CApplyDamage();
	}
}

void CApplyDamage::ShutDown()
{
	if (s_pApplyDamage)
	{
		delete s_pApplyDamage;
		s_pApplyDamage = NULL;
	}
}

void CApplyDamage::RemoveVehicleDeformation(CVehicle* pVehicle)
{
	if (!s_pApplyDamage || !pVehicle)
	{
		return;
	}

	PF_PUSH_TIMEBAR_BUDGETED("RemoveVehicleDeformation", 0.05f);
	rage::sysIpcLockMutex(sm_Mutex);

	atMap<CVehicle*,DamageDrawVehicle>& damagedVehicleMap = s_pApplyDamage->m_DamagedVehicleMap;

	if (damagedVehicleMap.Access(pVehicle) != NULL)
	{
		DamageDrawVehicle& damagedVehicle = damagedVehicleMap[pVehicle];
		int numItems = damagedVehicle.m_DamageDrawItems.GetCount();

		for (int itemIndex = 0; itemIndex < numItems; ++itemIndex)
		{
			DamageDrawItem* item = damagedVehicle.m_DamageDrawItems[itemIndex];
			
#if GPU_DAMAGE_LOG
			Displayf("[VehicleDamage] Releasing item %p", item);
#endif
			g_DamageDrawItemPool.DelayedDelete(item);
			damagedVehicle.m_DamageDrawItems.Delete(itemIndex);
			--itemIndex;
			--numItems;
		}

#if VEHICLE_ROLLING_TEXTURE_ARRAY
		if (damagedVehicle.m_pTexCache)
		{
			damagedVehicle.m_pTexCache->ClearDamageDownloads();
		}
#endif

		Assertf(!HasAnyDamageDrawItemsInPool(pVehicle), "Removing a vehicle deformation while it still has uncleared draw items");
		damagedVehicleMap.Delete(pVehicle);
	}
	
	rage::sysIpcUnlockMutex(sm_Mutex);

	PF_POP_TIMEBAR();
}

#if __BANK
bool CApplyDamage::HasAnyDamageDrawItemsInPool(CVehicle* pVeh)
{
#if VEHICLE_ROLLING_TEXTURE_ARRAY
	DamageDrawVehicle *pDamageDrawVehicle = s_pApplyDamage->m_DamagedVehicleMap.Access(pVeh);
	
	if (pDamageDrawVehicle && pDamageDrawVehicle->m_pTexCache && pDamageDrawVehicle->m_pTexCache->IsDamageQueued())
	{
		return true;
	}
#endif

	for(int i = 0; i < g_DamageDrawItemPool.m_nTotalNumOfItmes; i++)
	{
		if(g_DamageDrawItemPool.m_All[i].m_eState == DamageDrawItem::USED && g_DamageDrawItemPool.m_All[i].m_pParentVehicle == pVeh)
		{
			return true;
		}
	}
	return false;
}

int CApplyDamage::GetNumDamageDrawItemsInPool(CVehicle* pVeh)
{
	int count = 0;
	for(int i = 0; i < g_DamageDrawItemPool.m_nTotalNumOfItmes; i++)
	{
		if(g_DamageDrawItemPool.m_All[i].m_eState == DamageDrawItem::USED && g_DamageDrawItemPool.m_All[i].m_pParentVehicle == pVeh)
		{
			count++;
		}
	}
	return count;
}
#endif

void CApplyDamage::Update()
{
	if (!s_pApplyDamage || !CVehicleDamage::ms_bEnableGPUDamage)
	{
		return;
	}

	PF_PUSH_TIMEBAR_BUDGETED("Update", 0.5f);	
	rage::sysIpcLockMutex(sm_Mutex);

	atMap<CVehicle*,DamageDrawVehicle>& damagedVehicleMap = s_pApplyDamage->m_DamagedVehicleMap;
	int numVehicles = damagedVehicleMap.GetNumUsed();

	if (numVehicles == 0)
	{
		rage::sysIpcUnlockMutex(sm_Mutex);
		PF_POP_TIMEBAR();
		return;
	}

	atArray<CVehicle*> vehiclesToDelete;

	for(atMap<CVehicle*,DamageDrawVehicle>::Iterator It = damagedVehicleMap.CreateIterator(); !It.AtEnd(); It.Next())
	{
		DamageDrawVehicle& damagedVehicle = It.GetData();

		bool bEnableDamage  = true;

#if VEHICLE_ROLLING_TEXTURE_ARRAY
		if (!damagedVehicle.m_pTexCache || !damagedVehicle.m_pVehicle)
		{
			//Displayf("CApplyDamage::Update - continue");
			continue;
		}

		bool bDamageIsReady = DownloadAvailableDamageToCPU(damagedVehicle);
#else
		if (!damagedVehicle.m_DamageTexture || !damagedVehicle.m_pVehicle)
		{
			//Displayf("CApplyDamage::Update - continue");
			continue;
		}

		bool bDamageIsReady = IsDamageReady(damagedVehicle, bEnableDamage);
#endif

		if (bDamageIsReady)
		{
			//Displayf("CApplyDamage::Update - SetDamageUpdatedByGPU to TRUE");
			PF_PUSH_TIMEBAR_BUDGETED("SetDamageUpdatedByGPU", 0.1f);
			damagedVehicle.m_pVehicle->SetDamageUpdatedByGPU(true, bEnableDamage);
			PF_POP_TIMEBAR();
		}

		if (Cleanup(damagedVehicle))
		{
			//Displayf("CApplyDamage::Update - Cleanup");
			CVehicle* pVehicle = damagedVehicle.m_pVehicle;
			Assertf(!HasAnyDamageDrawItemsInPool(pVehicle), "Removing a vehicle deformation while it still has uncleared draw items");
			vehiclesToDelete.PushAndGrow(pVehicle);

#if GTA_REPLAY
			if( CReplayMgr::IsEditModeActive() )
			{
				//Allow more replay impacts to be added after any previous damage has been cleared up
				if(pVehicle){
					pVehicle->ResetNumAppliedVehicleDamage();
				}
			}
#endif //GTA_REPLAY
		}
	}

	for (int i=0; i < vehiclesToDelete.GetCount(); ++i)
	{
		CVehicle* pVehicle = vehiclesToDelete[i];
		damagedVehicleMap.Delete(pVehicle);
	}

	rage::sysIpcUnlockMutex(sm_Mutex);
	PF_POP_TIMEBAR();
}

void CApplyDamage::DrawDamage()
{
	if (!s_pApplyDamage || !CVehicleDamage::ms_bEnableGPUDamage)
	{
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if !(RSG_PC && MULTIPLE_RENDER_THREADS)
	Update();
#endif

	PF_PUSH_TIMEBAR_BUDGETED("DrawDamage", 2.0f);
	rage::sysIpcLockMutex(sm_Mutex);

	atMap<CVehicle*,DamageDrawVehicle>& damagedVehicleMap = s_pApplyDamage->m_DamagedVehicleMap;
	int numVehicles = damagedVehicleMap.GetNumUsed();

	if (numVehicles == 0)
	{
		rage::sysIpcUnlockMutex(sm_Mutex);
		PF_POP_TIMEBAR();
		return;
	}

	//Set the viewport once for all draw calls
	const grcViewport* prevViewport = grcViewport::GetCurrent();
	grcViewport viewport;
	viewport.Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	grcViewport::SetCurrent(&viewport);

	Assert(s_pApplyDamage->m_ApplyDamageOffsetVar != grcevNONE);
	Assert(s_pApplyDamage->m_ApplyDamageVar != grcevNONE);

	for(atMap<CVehicle*,DamageDrawVehicle>::Iterator It = damagedVehicleMap.CreateIterator(); !It.AtEnd(); It.Next())
	{
		DamageDrawVehicle& damagedVehicle = It.GetData();
		Assert(damagedVehicle.m_pVehicle);

#if __ASSERT
		CVehicleDamage* pVehDamage = damagedVehicle.m_pVehicle ? damagedVehicle.m_pVehicle->GetVehicleDamage() : NULL;
		Assert(pVehDamage);

		CVehicleDeformation* pVehDeformation = pVehDamage ? pVehDamage->GetDeformation() : NULL;
		Assert(pVehDeformation);

		Assert(damagedVehicle.m_DamageRT && pVehDeformation && damagedVehicle.m_DamageRT == pVehDeformation->GetDamageRenderTarget());

#if VEHICLE_ROLLING_TEXTURE_ARRAY
		Assert(damagedVehicle.m_pTexCache && pVehDeformation && damagedVehicle.m_pTexCache == pVehDeformation->GetTexCache());
#else
		Assert(damagedVehicle.m_DamageTexture && pVehDeformation && damagedVehicle.m_DamageTexture == pVehDeformation->GetDamageTexture());
#endif
#endif //__ASSERT

		if (!damagedVehicle.m_DamageRT || damagedVehicle.m_DamageDrawItems.IsEmpty())
		{
			//Displayf("CApplyDamage::DrawDamage - no render target or no damage draw items = %d", damagedVehicle.m_DamageDrawItems.GetCount());
			continue;
		}

		DrawDamageItemsForVehicle(damagedVehicle);
	}

	rage::sysIpcUnlockMutex(sm_Mutex);

	grcViewport::SetCurrent(prevViewport);
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);
	PF_POP_TIMEBAR();

#if RSG_PC
	//Flip frames for SLI
	ms_bCurrentFrame = !ms_bCurrentFrame;
#endif
}

bool CApplyDamage::DrawDamageItemsForVehicle(DamageDrawVehicle& damagedVehicle)
{
	int numItems = damagedVehicle.m_DamageDrawItems.GetCount();

	if (numItems == 0)
	{
		//Displayf("CApplyDamage::DrawDamageItemsForVehicle - numItems = 0");
		return false;
	}

	Assert(damagedVehicle.m_DamageRT);

	if (damagedVehicle.m_DamageRT == NULL)
	{
		//Displayf("CApplyDamage::DrawDamageItemsForVehicle - damagedVehicle.m_DamageRT = NULL");
		return false;
	}

	grcTextureFactory::GetInstance().LockRenderTarget(0, damagedVehicle.m_DamageRT, NULL);

	int lastIndexDrawn = -1;

#if VEHICLE_ROLLING_TEXTURE_ARRAY
	bool initialDamageDraw = false;
#endif

	for (int itemIndex = 0; itemIndex < numItems; ++itemIndex)
	{
		DamageDrawItem* item = damagedVehicle.m_DamageDrawItems[itemIndex];

#if RSG_PC
		if (item->IsGpuDone())
#else
		if (item->m_DrawCount) // only draw stuff that hasn't been drawn already
#endif
			continue;

#if VEHICLE_ROLLING_TEXTURE_ARRAY
		if (item->m_GpuDoneMask == 0)
			initialDamageDraw = true;
#endif

#if GPU_DAMAGE_LOG
		Displayf("[VehicleDamage] %s item %p to RT %s, GPU %d, Damage(%.1f,%.1f,%.1f,%.1f), Offset(%.1f,%.1f,%.1f,%.1f)",
			item->m_ResetDamage ? "Clearing" : "Drawing", item, damagedVehicle.m_DamageRT->GetName(), GRCDEVICE.GPUIndex(),
			item->m_Damage.x, item->m_Damage.y, item->m_Damage.z, item->m_Damage.w,
			item->m_DamageOffsetUV.x, item->m_DamageOffsetUV.y, item->m_DamageOffsetUV.z, item->m_DamageOffsetUV.w);
#endif //GPU_DAMAGE_LOG

		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth,
			item->m_ResetDamage ? grcStateBlock::BS_Default : m_ApplyDamageBlendState);

		s_ApplyDamageShader->SetVar(s_pApplyDamage->m_ApplyDamageVar, item->m_Damage);
		s_ApplyDamageShader->SetVar(s_pApplyDamage->m_ApplyDamageOffsetVar, item->m_DamageOffsetUV);
		s_ApplyDamageShader->SetVar(s_pApplyDamage->m_NoiseTextureVar, ms_NoiseTexture);

		AssertVerify(s_ApplyDamageShader->BeginDraw(grmShader::RMC_DRAW, true, s_pApplyDamage->m_ApplyDamageTechnique));
		s_ApplyDamageShader->Bind(0);

		//Just draw a unit quad
		CApplyDamageDraw::DrawFullscreen();

		s_ApplyDamageShader->UnBind();
		s_ApplyDamageShader->EndDraw();

		++(item->m_DrawCount);
		lastIndexDrawn = itemIndex;

#if RSG_PC
		item->SetGpuDone();
#endif
#if GPU_DAMAGE_DUMP
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		{
			grcRenderTarget *const rt = damagedVehicle.m_DamageRT;
			static grcImage *s_damageImage = NULL;
			if (!s_damageImage)
			{
				s_damageImage = grcImage::Create(rt->GetWidth(), rt->GetHeight(), 1, (grcImage::Format)rt->GetImageFormat(), grcImage::STANDARD, 0, 0);
			}
			static_cast<grcRenderTargetD3D11*>(rt)->CopyTo(s_damageImage);
			static char name[128];
			sprintf(name, "SaveRT_%p_to_%s_GPU[%d].dds", item, rt->GetName(), GRCDEVICE.GPUIndex());
			s_damageImage->SaveDDS(name);
		}
		grcTextureFactory::GetInstance().LockRenderTarget(0, damagedVehicle.m_DamageRT, NULL);
#endif //GPU_DAMAGE_DUMP
	} // for

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if RSG_PC && __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
	if (damagedVehicle.m_pTexCache)
	{
		if (initialDamageDraw)
		{
			damagedVehicle.m_pTexCache->IncrementQueuedDownload();
			damagedVehicle.m_pTexCache->m_HasDamageDrawn = true;
		}
		grcTexture *damageTexture = damagedVehicle.m_pTexCache->GetDownloadQueuedTexture();
		damageTexture->Copy(damagedVehicle.m_DamageRT);
		(static_cast<grcTextureD3D11*>(damageTexture))->CopyFromGPUToStagingBuffer();
	}
#else
	Assert(damagedVehicle.m_DamageTexture);
	grcTextureD3D11* dx11Tex = damagedVehicle.m_DamageTexture ? static_cast<grcTextureD3D11*>(damagedVehicle.m_DamageTexture) : NULL;
	if (dx11Tex)
	{
		PF_PUSH_TIMEBAR_BUDGETED("Damage GPUToStaging", 0.2f);
		if (UseSeparateRenderTarget() && AssertVerify(damagedVehicle.m_DamageRT))
		{
			damagedVehicle.m_DamageTexture->Copy(damagedVehicle.m_DamageRT);
		}
		dx11Tex->CopyFromGPUToStagingBuffer();
		PF_POP_TIMEBAR();
	}
#endif
#elif RSG_PC
	#error "Unknown Platform"
#endif

#if ADD_FENCE_TO_DAMAGE
	if (lastIndexDrawn != -1)
	{
		DamageDrawItem* item = damagedVehicle.m_DamageDrawItems[lastIndexDrawn];
		Assert(!item->m_FenceBusy);
		item->m_FenceBusy = 1;
		grcContextHandle *ctx = g_grcCurrentContext;
#if RSG_DURANGO
		// WriteValueBottomOfPipe issues a CACHE_FLUSH_AND_INV_TS_EVENT, so
		// there is no need for additional cache flushes here.
		ctx->WriteValueBottomOfPipe((void*)(&item->m_FenceBusy), 0);
#elif RSG_ORBIS
		ctx->writeAtEndOfPipe(sce::Gnm::kEopFlushCbDbCaches,
 			sce::Gnm::kEventWriteDestMemory, (void*)(&item->m_FenceBusy),
 			sce::Gnm::kEventWriteSource32BitsImmediate, 0,
 			sce::Gnm::kCacheActionNone, sce::Gnm::kCachePolicyLru);
#else
#error "not yet implemented for this platform"
#endif
		//Displayf("CApplyDamage::DrawDamageItemsForVehicle - Inserted Fence");
	}
#endif

	return (lastIndexDrawn != -1);
}

#if VEHICLE_ROLLING_TEXTURE_ARRAY
bool CApplyDamage::DownloadAvailableDamageToCPU(DamageDrawVehicle& damagedVehicle)
{
	if (damagedVehicle.m_pTexCache->IsDamageQueued())
	{
		if (damagedVehicle.m_pTexCache->DamageQueuedIsReady())
		{
			grcTextureD3D11* dx11Tex = static_cast<grcTextureD3D11*>(damagedVehicle.m_pTexCache->GetDamageDownloadTexture());
			PF_PUSH_TIMEBAR_BUDGETED("IsDamageReady", 0.04f);
			if (dx11Tex->MapStagingBufferToBackingStore(true))
			{
				PF_POP_TIMEBAR();
				damagedVehicle.m_pTexCache->AdvanceActiveTexture();
				return true;
			}
			PF_POP_TIMEBAR();
		}
	}
	return false;
}
#else //VEHICLE_ROLLING_TEXTURE_ARRAY
bool CApplyDamage::IsDamageReady(DamageDrawVehicle& damagedVehicle, bool &bEnableDamage)
{
	int numItems = damagedVehicle.m_DamageDrawItems.GetCount();
	bool bAnyDamageIsReady = false;

	for (int itemIndex = 0; itemIndex < numItems; ++itemIndex)
	{
		DamageDrawItem* item = damagedVehicle.m_DamageDrawItems[itemIndex];

#if RSG_PC
		if (!item->AllGpuDone())
#else
		if (item->m_DrawCount == 0)
#endif
		{
			//If an item wasn't drawn yet, nothing after it would be drawn yet either, so early return
			//Displayf("CApplyDamage::IsDamageReady reached item not drawn yet - bAnyDamageIsReady = %s", bAnyDamageIsReady ? "TRUE" : "FALSE");
			break;
		}

#if ADD_FENCE_TO_DAMAGE
		if (item->m_FenceBusy)
		{
			//Displayf("CApplyDamage::IsDamageReady fence is still pending - bAnyDamageIsReady = %s", bAnyDamageIsReady ? "TRUE" : "FALSE");
			return bAnyDamageIsReady;
		}

		//Displayf("CApplyDamage::IsDamageReady - Fence is no longer pending, Done");
#endif

		bAnyDamageIsReady = true;
		bEnableDamage = !item->m_ResetDamage;
	}

#if RSG_PC && __D3D11
	grcTextureD3D11* dx11Tex = bAnyDamageIsReady && damagedVehicle.m_DamageTexture ? static_cast<grcTextureD3D11*>(damagedVehicle.m_DamageTexture) : NULL;

	//Poll on the damage being updated to the CPU copy
	if (bAnyDamageIsReady && dx11Tex)
	{
		PF_PUSH_TIMEBAR_BUDGETED("IsDamageReady", 0.04f);
		bAnyDamageIsReady &= dx11Tex->MapStagingBufferToBackingStore(true);
		PF_POP_TIMEBAR();
	}
#elif RSG_PC
	#error 1 && "Unknown Platform"
#endif

	return bAnyDamageIsReady;
}
#endif //VEHICLE_ROLLING_TEXTURE_ARRAY

bool CApplyDamage::Cleanup(DamageDrawVehicle& damagedVehicle)
{
	int numItems = damagedVehicle.m_DamageDrawItems.GetCount();

	for (int itemIndex = 0; itemIndex < numItems; ++itemIndex)
	{
		DamageDrawItem* item = damagedVehicle.m_DamageDrawItems[itemIndex];
		bool bCleanupItem = false;

#if RSG_PC
		bCleanupItem = (item->AllGpuDone());
#else
		bCleanupItem = (item->m_DrawCount == 1);
#endif

#if ADD_FENCE_TO_DAMAGE
		if (bCleanupItem)
		{
			if (item->m_FenceBusy)
			{
				//Displayf("CApplyDamage::Cleanup fence is still pending, don't clean up yet");
				return false; // don't clean anything up yet
			}
		}
#endif

		if (bCleanupItem)
		{
#if GPU_DAMAGE_LOG
			Displayf("[VehicleDamage] Releasing item %p", item);
#endif
			g_DamageDrawItemPool.DelayedDelete(item);
			damagedVehicle.m_DamageDrawItems.Delete(itemIndex);
			--itemIndex;
			--numItems;
		}
	}

	Assertf(damagedVehicle.m_DamageDrawItems.GetCount() == CApplyDamage::GetNumDamageDrawItemsInPool(damagedVehicle.m_pVehicle), "CApplyDamage::Cleanup draw items in list is not matching to draw items in pool");

#if VEHICLE_ROLLING_TEXTURE_ARRAY
	return damagedVehicle.m_DamageDrawItems.IsEmpty() && !damagedVehicle.m_pTexCache->IsDamageQueued();
#else
	return damagedVehicle.m_DamageDrawItems.IsEmpty();
#endif
}


#endif //GPU_DAMAGE_WRITE_ENABLED
