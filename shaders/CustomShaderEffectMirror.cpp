//
// Filename: CustomShaderEffectMirror.h
//

// Rage headers
#include "bank/bkmgr.h"
#include "grmodel/ShaderFx.h"
#include "grmodel/Geometry.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/texturereference.h"
#include "rmcore/drawable.h"
#include "system/nelem.h"

// Framework headers
#include "fwscene/scan/Scan.h"
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"

// Game headers
#include "debug/debug.h"
#include "renderer/Mirrors.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "scene/Entity.h"
#include "scene/world/VisibilityMasks.h"
#include "shaders/CustomShaderEffectMirror.h"

#if !__FINAL
bool  CCustomShaderEffectMirror::ms_debugMirrorUseWhiteTexture = false;
float CCustomShaderEffectMirror::ms_debugMirrorSuperReflective = 0.0f;
float CCustomShaderEffectMirror::ms_debugMirrorBrightnessScale = 1.0f;
#endif // !__FINAL

bool CCustomShaderEffectMirrorType::Initialise(rmcDrawable* pDrawable)
{
	Assert(pDrawable);

	m_idVarMirrorBounds = pDrawable->GetShaderGroup().LookupVar("MirrorBounds", true);
#if __PS3
	m_idVarMirrorReflection = pDrawable->GetShaderGroup().LookupVar("MirrorReflection", true);
#endif // __PS3
#if !__FINAL
	m_idVarMirrorDebugParams = pDrawable->GetShaderGroup().LookupVar("MirrorDebugParams", false);
#endif // !__FINAL

	return true;
}

CCustomShaderEffectBase* CCustomShaderEffectMirrorType::CreateInstance(CEntity* pEntity)
{
	pEntity->GetRenderPhaseVisibilityMask().ClearFlag(VIS_PHASE_MASK_MIRROR_REFLECTION | VIS_PHASE_MASK_WATER_REFLECTION | VIS_PHASE_MASK_PARABOLOID_REFLECTION);
	pEntity->SetBaseFlag(fwEntity::DRAW_FIRST_SORTED);
	pEntity->ClearBaseFlag(fwEntity::USE_SCREENDOOR);
	pEntity->RequestUpdateInWorld(); // note that the entity's m_nFlags.bInMloRoom must be valid at this point, which seems to be the case so we're ok i think

	return rage_new CCustomShaderEffectMirror(pEntity, this);
}

#if !__FINAL
PARAM(nomirrors,""); // temp hack to disable mirror reflection, this "fixes" GPU deadlock BS#741936
PARAM(nomirrors_360,"");
#endif // !__FINAL

void CCustomShaderEffectMirror::Initialise()
{
#if !__FINAL
	if ((PARAM_nomirrors.Get()) ||
		(PARAM_nomirrors_360.Get() && __XENON))
	{
		ms_debugMirrorUseWhiteTexture = true;
	}
#endif // !__FINAL
}

CCustomShaderEffectMirror::CCustomShaderEffectMirror(CEntity* pEntity, CCustomShaderEffectMirrorType* pType) : CCustomShaderEffectBase(sizeof(*this), CSE_MIRROR), m_pType(pType), m_bIsLowPriorityMirrorFloor(false)
{
	const CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

	if (AssertVerify(pModelInfo))
	{
		// [HACK GTAV] hack for BS#1527956,1601629 etc. (sometimes this mirror floor is visible even while other mirrors are visible)
		if (pModelInfo->GetModelNameHash() == ATSTRINGHASH("v_57_mirrorfloor", 0x59B4C4FB) ||
			pModelInfo->GetModelNameHash() == ATSTRINGHASH("v_5_bank_mirrorfloor", 0x490C6177))
		{
			m_bIsLowPriorityMirrorFloor = true;
		}
	}

#if CSE_MIRROR_STORE_ENTITY_LIST
	const bool g_verbose = false; // TODO REMOVE -- this array was overflowing (BS#347070), should be fixed now, can remove this code soon 

	if (g_verbose) { Displayf("CCustomShaderEffectMirror(\"%s\") .. currently tracking %d entities", pEntity->GetModelName(), sm_mirrorEntities.GetCount()); }

	for (int i = 0; i < sm_mirrorEntities.GetCount(); i++)
	{
		if (sm_mirrorEntities[i] == pEntity) // already in the list
		{
			pEntity = NULL;
		}
	}

	for (int i = 0; i < sm_mirrorEntities.GetCount(); i++)
	{
		if (sm_mirrorEntities[i] == NULL) // old entity is gone
		{
			if (pEntity) // replace with new entity
			{
				if (g_verbose) { Displayf("  entity[%d] is NULL, replacing with new entity", i); }
				sm_mirrorEntities[i] = pEntity;
				pEntity = NULL;
			}
			else if (i < sm_mirrorEntities.GetCount() - 1) // pop array and replace .. decrement index in case popped entity is NULL too
			{
				if (g_verbose) { Displayf("  entity[%d] is NULL, replacing with entity[%d]", i, sm_mirrorEntities.GetCount() - 2); }
				sm_mirrorEntities[i--] = sm_mirrorEntities.Pop();
			}
			else // just pop the array
			{
				if (g_verbose) { Displayf("  entity[%d] is NULL, removing", i); }
				sm_mirrorEntities.Pop();
			}
		}
	}

	if (pEntity && AssertVerify(!sm_mirrorEntities.IsFull()))
	{
		if (g_verbose) { Displayf("  adding new entity"); }
		sm_mirrorEntities.Append() = pEntity;
	}

	if (g_verbose)
	{
		for (int i = 0; i < sm_mirrorEntities.GetCount(); i++)
		{
			if (sm_mirrorEntities[i])
			{
				Displayf("  now tracking entity[%d] \"%s\"", i, sm_mirrorEntities[i]->GetModelName());
			}
		}
	}
#endif // CSE_MIRROR_STORE_ENTITY_LIST
}

static bool g_MirrorVisible = false;
static void SetMirrorVisible(bool bMirrorVisible)
{
	g_MirrorVisible = bMirrorVisible;
}

void CCustomShaderEffectMirror::SetShaderVariables(rmcDrawable* pDrawable)
{
#if __BANK
	if(DRAWLISTMGR->IsExecutingDebugOverlayDrawList())
		return;
#endif

	Assert(!gDrawListMgr->IsExecutingDrawList(DL_RENDERPHASE_REFLECTION_MAP)); // we should have prevented this by not calling CCustomShaderEffectDC

	if (pDrawable)
	{
		grmShaderGroup& shaderGroup = pDrawable->GetShaderGroup();
		Assert(shaderGroup.GetShaderGroupVarCount() > 0);

		const grcTexture* reflectionTex = NOTFINAL_ONLY(ms_debugMirrorUseWhiteTexture ? grcTexture::None :) CMirrors::GetMirrorTexture();

		if (!g_MirrorVisible)
		{
			reflectionTex = grcTexture::NoneBlack;
		}

		shaderGroup.SetVar(m_pType->m_idVarMirrorBounds, CMirrors::GetMirrorBounds_renderthread());
#if __PS3
		shaderGroup.SetVar(m_pType->m_idVarMirrorReflection, reflectionTex);
#else
		CRenderPhaseReflection::SetReflectionMapExt(reflectionTex);
#endif

#if __XENON
	PostFX::SetLDR10bitHDR10bit();
#elif __PS3
	PostFX::SetLDR8bitHDR16bit();
#else
	PostFX::SetLDR16bitHDR16bit();
#endif

#if !__FINAL
		const float superReflective = ms_debugMirrorSuperReflective;
		const float brightnessScale = ms_debugMirrorBrightnessScale;

		if (m_pType->m_idVarMirrorDebugParams != grmsgvNONE)
			shaderGroup.SetVar(m_pType->m_idVarMirrorDebugParams, Vec4f(superReflective, brightnessScale, 0.0f, 0.0f));
#endif // !__FINAL
	}
}

void CCustomShaderEffectMirror::AddToDrawList(u32 modelIndex, bool bExecute)
{
	// for some reason even though we're excluding the mirror from VIS_PHASE_MASK_PARABOLOID_REFLECTION, we're still seeing it in DL_RENDERPHASE_REFLECTION_MAP for a few frames .. so we have to handle this
	if (gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_REFLECTION_MAP) || gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_MIRROR_REFLECTION))
		return;

	bool bMirrorVisible = CMirrors::GetIsMirrorVisible();

	if (m_bIsLowPriorityMirrorFloor && !fwScan::GetScanResults().GetMirrorFloorVisible())
	{
		bMirrorVisible = false;
	}

	DLC_Add(SetMirrorVisible, bMirrorVisible);
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
}

void CCustomShaderEffectMirror::AddToDrawListAfterDraw()
{
#if __BANK
	if(DRAWLISTMGR->IsBuildingDebugOverlayDrawList())
		return;
#endif

	// for some reason even though we're excluding the mirror from VIS_PHASE_MASK_PARABOLOID_REFLECTION, we're still seeing it in DL_RENDERPHASE_REFLECTION_MAP for a few frames .. so we have to handle this
	if (gDrawListMgr->IsBuildingDrawList(DL_RENDERPHASE_REFLECTION_MAP))
		return;

#if __XENON
	DLC_Add(PostFX::SetLDR8bitHDR10bit);
#elif __PS3
	DLC_Add(PostFX::SetLDR8bitHDR16bit);
#else
	DLC_Add(PostFX::SetLDR16bitHDR16bit);
#endif

#if !__PS3
	DLC_Add(CRenderPhaseReflection::SetReflectionMap); // restore reflection map
#endif // !__PS3
}

#if __BANK

void CCustomShaderEffectMirror::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Mirror", false);
	{
		bank.AddToggle("Use White Texture", &ms_debugMirrorUseWhiteTexture);
		bank.AddSlider("Super Reflective", &ms_debugMirrorSuperReflective, 0.0f, 1.0f, 1.0f/32.0f);
		bank.AddSlider("Brightness Scale", &ms_debugMirrorBrightnessScale, 0.0f, 16.0f, 1.0f/32.0f);
	}
	bank.PopGroup();
}

#endif // __BANK

#if CSE_MIRROR_STORE_ENTITY_LIST

int CCustomShaderEffectMirror::GetNumMirrorEntities()
{
	return sm_mirrorEntities.GetCount();
}

CEntity* CCustomShaderEffectMirror::GetMirrorEntity(int index)
{
	return sm_mirrorEntities[index];
}

atFixedArray<RegdEnt,16> CCustomShaderEffectMirror::sm_mirrorEntities;

#endif // CSE_MIRROR_STORE_ENTITY_LIST
