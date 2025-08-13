//
// Filename: CustomShaderEffectCable.h
//

// Rage headers
#include "bank/bkmgr.h"
#include "grmodel/ShaderFx.h"
#include "grmodel/Geometry.h"
#include "grcore/debugdraw.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grcore/texturereference.h"
#include "rmcore/drawable.h"
#include "pheffects/wind.h"
#include "system/nelem.h"

// Framework headers
#include "entity/drawdata.h"
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/debug.h"
#include "Objects/DummyObject.h"
#include "renderer/PostProcessFX.h"
#include "renderer/renderphases/RenderPhaseEntitySelect.h"
#include "scene/AnimatedBuilding.h"
#include "scene/Building.h"
#include "scene/Entity.h"
#include "shaders/CustomShaderEffectCable.h"

#include "Objects/object.h"

#define CABLE_DEFAULT_RADIUS_SCALE       1.0000f
#define CABLE_DEFAULT_PIXEL_THICKNESS    2.2500f//2.000f
#define CABLE_DEFAULT_SHADOW_THICKNESS	 0.0125f;	// Using fixed-width cables in shadow pass as scaling by camera doesn't make sense.
#define CABLE_DEFAULT_SHADOW_WIND_SPEED	 0.5f;		// Multiplier used for the wind speed in shadow pass (looks too fast otherwise)
#define CABLE_DEFAULT_FADE_EXPONENT_ADJ  0.5000f
#define CABLE_DEFAULT_WIND_MOTION_FREQ_X 0.7000f
#define CABLE_DEFAULT_WIND_MOTION_FREQ_Y 0.4800f
#define CABLE_DEFAULT_WIND_MOTION_AMP_X  0.0236f
#define CABLE_DEFAULT_WIND_MOTION_AMP_Y  0.0361f

#define CABLE_DEFAULT_WIND_RESPONSE_VEL_MIN  0.0f
#define CABLE_DEFAULT_WIND_RESPONSE_VEL_MAX 50.0f
#define CABLE_DEFAULT_WIND_RESPONSE_VEL_EXP  0.0f
#define CABLE_DEFAULT_WIND_RESPONSE_AMOUNT  10.0f

#if __BANK
bool        CCustomShaderEffectCable::ms_debugCableEnable                  = true;
bool        CCustomShaderEffectCable::ms_debugCableForceInteriorAmbientOn  = false;
bool        CCustomShaderEffectCable::ms_debugCableForceInteriorAmbientOff = false;
bool        CCustomShaderEffectCable::ms_debugCableForceExteriorAmbientOn  = false;
bool        CCustomShaderEffectCable::ms_debugCableForceExteriorAmbientOff = false;
bool        CCustomShaderEffectCable::ms_debugCableForceDepthWritesOn      = false;
bool        CCustomShaderEffectCable::ms_debugCableForceDepthWritesOff     = false;
bool        CCustomShaderEffectCable::ms_debugCableDrawNormals             = false;
float       CCustomShaderEffectCable::ms_debugCableDrawNormalsLength       = 1.0f;
bool        CCustomShaderEffectCable::ms_debugCableDrawWindTestGlobal      = false;
bool        CCustomShaderEffectCable::ms_debugCableDrawWindTestLocal       = false;
bool        CCustomShaderEffectCable::ms_debugCableDrawWindTestLocalCamera = false;
float       CCustomShaderEffectCable::ms_debugCableDrawWindTestLength      = 1.0f;
int         CCustomShaderEffectCable::ms_debugCableTextureOverride         = 0;
grcTexture* CCustomShaderEffectCable::ms_debugCableTextures[]              = {NULL,NULL,NULL};
float       CCustomShaderEffectCable::ms_debugCableRadiusScale             = CABLE_DEFAULT_RADIUS_SCALE;
float       CCustomShaderEffectCable::ms_debugCablePixelThickness          = CABLE_DEFAULT_PIXEL_THICKNESS;
float       CCustomShaderEffectCable::ms_debugCableShadowThickness         = CABLE_DEFAULT_SHADOW_THICKNESS;
float		CCustomShaderEffectCable::ms_debugCableShadowWindModifier	   = CABLE_DEFAULT_SHADOW_WIND_SPEED;
float       CCustomShaderEffectCable::ms_debugCableFadeExponentAdj         = CABLE_DEFAULT_FADE_EXPONENT_ADJ;
bool        CCustomShaderEffectCable::ms_debugCableDiffuseOverride         = false;
Color32     CCustomShaderEffectCable::ms_debugCableDiffuse                 = Color32(0,0,0,255);
bool        CCustomShaderEffectCable::ms_debugCableAmbientOverride         = false;
float       CCustomShaderEffectCable::ms_debugCableAmbientNatural          = 0.4f;
float       CCustomShaderEffectCable::ms_debugCableAmbientArtificial       = 0.4f;
float       CCustomShaderEffectCable::ms_debugCableAmbientEmissive         = 0.0f;
float       CCustomShaderEffectCable::ms_debugCableOpacity                 = 1.0f;
bool        CCustomShaderEffectCable::ms_debugCableWindMotionEnabled       = true;
float       CCustomShaderEffectCable::ms_debugCableWindMotionFreqScale     = 1.0f;
float       CCustomShaderEffectCable::ms_debugCableWindMotionFreqX         = CABLE_DEFAULT_WIND_MOTION_FREQ_X;
float       CCustomShaderEffectCable::ms_debugCableWindMotionFreqY         = CABLE_DEFAULT_WIND_MOTION_FREQ_Y;
float       CCustomShaderEffectCable::ms_debugCableWindMotionAmpScale      = 1.0f;
float       CCustomShaderEffectCable::ms_debugCableWindMotionAmpX          = CABLE_DEFAULT_WIND_MOTION_AMP_X;
float       CCustomShaderEffectCable::ms_debugCableWindMotionAmpY          = CABLE_DEFAULT_WIND_MOTION_AMP_Y;
bool        CCustomShaderEffectCable::ms_debugCableWindResponseEnabled     = true;
float       CCustomShaderEffectCable::ms_debugCableWindResponseAmount      = CABLE_DEFAULT_WIND_RESPONSE_AMOUNT;
float       CCustomShaderEffectCable::ms_debugCableWindResponseVelMin      = CABLE_DEFAULT_WIND_RESPONSE_VEL_MIN;
float       CCustomShaderEffectCable::ms_debugCableWindResponseVelMax      = CABLE_DEFAULT_WIND_RESPONSE_VEL_MAX;
float       CCustomShaderEffectCable::ms_debugCableWindResponseVelExp      = CABLE_DEFAULT_WIND_RESPONSE_VEL_EXP;
#if CABLE_AUTO_CHECK
float       CCustomShaderEffectCable::ms_debugCableCheckSphereRadius       = 0.25f;
u32         CCustomShaderEffectCable::ms_debugCableCheckFlags              = 0;
#endif // CABLE_AUTO_CHECK

#endif // __BANK

bool CCustomShaderEffectCableType::Initialise(rmcDrawable* pDrawable)
{
	Assert(pDrawable);

	m_idVarViewProjection	= pDrawable->GetShaderGroup().LookupVar("ViewProjection", true);
	m_idVarCableParams		= pDrawable->GetShaderGroup().LookupVar("CableParams", true);
	m_idVarCableTexture		= pDrawable->GetShaderGroup().LookupVar("Texture", true);
	m_idVarAlphaTest		= pDrawable->GetShaderGroup().LookupVar("AlphaTestValue", true);

	CCustomShaderEffectCable::Init(); // init once

	//	[hack] make sure to use tessellation technique if required
	rmcLodGroup& lodGroup = pDrawable->GetLodGroup();
	for (int lodIdx = 0; lodIdx < LOD_COUNT; ++lodIdx)
	{
		if (lodGroup.ContainsLod(lodIdx))
		{
			rmcLod& lod = lodGroup.GetLod(lodIdx);

			for(int modelIdx = 0; modelIdx < lod.GetCount(); ++modelIdx)
			{
				grmModel* model = lod.GetModel(modelIdx);

#if __ASSERT
				const grmShaderGroup& shaderGroup = pDrawable->GetShaderGroup();

				//	sanity check
				for(int i = 0; i < model->GetGeometryCount(); ++i)
				{
					const grmShader& shader = shaderGroup.GetShader(model->GetShaderIndex(i));
					Assertf(stricmp(shader.GetName(), "cable") == 0, "Not using cable shader : %s", shader.GetName());
				}
#endif

				model->ForceTessallation();
			}
		}
	}

	return true;
}

CCustomShaderEffectBase* CCustomShaderEffectCableType::CreateInstance(CEntity* pEntity)
{
	CCustomShaderEffectCable* pNewShaderEffect = rage_new CCustomShaderEffectCable(pEntity);

	pNewShaderEffect->m_pType = this;
	pNewShaderEffect->m_bDepthWrite = false;

	pEntity->GetRenderPhaseVisibilityMask().ClearFlag(VIS_PHASE_MASK_PARABOLOID_REFLECTION);
	pEntity->RequestUpdateInWorld(); // note that the entity's m_nFlags.bInMloRoom must be valid at this point, which seems to be the case so we're ok i think
	pEntity->SetBaseFlag(fwEntity::HAS_HYBRID_ALPHA);

	return pNewShaderEffect;
}

#if CABLE_AUTO_CHECK
CCustomShaderEffectCable::CCustomShaderEffectCable(CEntity* pEntity) : CCustomShaderEffectBase(sizeof(*this), CSE_CABLE)
{
	XPARAM(CheckCablesAuto);
	if (PARAM_CheckCablesAuto.Get())
	{
		CheckDrawable(pEntity->GetDrawable(), pEntity->GetModelName(), false, &m_indices);
	}
}
#else
CCustomShaderEffectCable::CCustomShaderEffectCable(CEntity*) : CCustomShaderEffectBase(sizeof(*this), CSE_CABLE) {}
#endif // CABLE_AUTO_CHECK

static grcRasterizerStateHandle g_rs_core = grcStateBlock::RS_Invalid;
static grcBlendStateHandle g_bs_core = grcStateBlock::BS_Invalid;
static DECLARE_MTR_THREAD grcRasterizerStateHandle g_rs_prev	= grcStateBlock::RS_Invalid;
static DECLARE_MTR_THREAD grcDepthStencilStateHandle g_dss_prev	= grcStateBlock::DSS_Invalid;
static DECLARE_MTR_THREAD grcBlendStateHandle g_bs_prev			= grcStateBlock::BS_Invalid;
static DECLARE_MTR_THREAD u8 g_dss_ref_prev = 0xFF;
static DECLARE_MTR_THREAD u32 g_bs_blend_factors_prev = 0;
static DECLARE_MTR_THREAD u64 g_bs_sample_mask_prev = 0xFF;
// forced to be different on update and render threads
static __THREAD bool g_bDrawCoreFlag = false;
#if RSG_PC
static bool g_bDepthWrite = false;
#endif
static float g_fWindTime, g_fWindVelocity;

static BankFloat s_fDrawCoreMaxDepth = 100.f * CABLE_DEFAULT_RADIUS_SCALE;
static BankFloat s_fAlphaTestValue = 0.1f;
static BankFloat s_fCoreAlphaTestValue = 1.f;			// optimal test value for particle interactions is 0.95, but the limit is 1.0, and it's never reached
static BankFloat s_fCoreBlurredAlphaTestValue = 0.5f;	// optimal test value for the fog and DoF is 0.5, can be reached in foggy weather

static bool IsDepthInverted()
{
	return	USE_INVERTED_PROJECTION_ONLY && (
			DRAWLISTMGR->IsExecutingGBufDrawList() || 
			DRAWLISTMGR->IsExecutingDrawSceneDrawList()
			);
}

static void SwitchDrawCore_Render(bool enable)
{
	g_bDrawCoreFlag = enable;
}

void CCustomShaderEffectCable::UpdateWindParams()
{
	g_fWindTime = (float)fwTimer::GetTimeInMilliseconds()/1000.0f;
	Vec3V wind;
	WIND.GetGlobalVelocity(WIND_TYPE_AIR, wind);
	g_fWindVelocity = Mag(wind).Getf();
}

void CCustomShaderEffectCable::SwitchDrawCore(bool enable)
{
	// assuming the DLC dispatch is executed on a different thread (render != update)
	g_bDrawCoreFlag = enable;
	DLC_Add(SwitchDrawCore_Render, enable);
}

#if RSG_PC
void CCustomShaderEffectCable::SwitchDepthWrite(bool enable)
{
	g_bDepthWrite = enable;
}
#endif

bool CCustomShaderEffectCable::IsCableVisible(CEntity* pEntity)
{
	if (g_bDrawCoreFlag)
	{
		Vector3 boundCenter;
		float boundRadius = pEntity->GetBoundCentreAndRadius(boundCenter);
		const float distance = boundCenter.Dist(camInterface::GetPos()) - boundRadius;
	
		return distance < s_fDrawCoreMaxDepth
			BANK_ONLY(|| CRenderPhaseEntitySelect::GetIsBuildingDrawList() )
			;
	}else
	{
		return true;
	}
}

static void CCustomShaderEffectCable_SetRenderStates(bool depthWrite, bool useDefaultBlend)
{
	g_rs_prev = grcStateBlock::RS_Active;
	g_dss_prev = grcStateBlock::DSS_Active;
	g_dss_ref_prev = grcStateBlock::ActiveStencilRef;
	g_bs_prev = grcStateBlock::BS_Active;
	g_bs_blend_factors_prev = grcStateBlock::ActiveBlendFactors;
	g_bs_sample_mask_prev = grcStateBlock::ActiveSampleMask;

	grcStateBlock::SetStates(
		g_bDrawCoreFlag ? g_rs_core : grcStateBlock::RS_NoBackfaceCull,
		IsDepthInverted() ?
			(depthWrite || g_bDrawCoreFlag ? CShaderLib::DSS_LessEqual_Invert : CShaderLib::DSS_TestOnly_LessEqual_Invert) :
			(depthWrite || g_bDrawCoreFlag ? grcStateBlock::DSS_LessEqual : grcStateBlock::DSS_TestOnly_LessEqual),
		g_bDrawCoreFlag ? g_bs_core : useDefaultBlend ? grcStateBlock::BS_Default : grcStateBlock::BS_Normal
		);
}

static void CCustomShaderEffectCable_RestoreRenderStates()
{
	// ==============================================================================================
	// NOTE -- AddToDrawListAfterDraw can be called even if AddToDrawList is not called, due to logic
	// in CEntityDrawHandler::BeforeAddToDrawList. When this happens, the previous state will be
	// invalid so we just ignore it (BS#900773)
	// ==============================================================================================
	if (g_rs_prev == grcStateBlock::RS_Invalid)
		return;
	
	grcStateBlock::SetRasterizerState(g_rs_prev);
	grcStateBlock::SetDepthStencilState(g_dss_prev, g_dss_ref_prev);
	grcStateBlock::SetBlendState(g_bs_prev, g_bs_blend_factors_prev, g_bs_sample_mask_prev);
	
	g_rs_prev = grcStateBlock::RS_Invalid;
}

void CCustomShaderEffectCable::AddToDrawList(u32 modelIndex, bool bExecute)
{
	bool bDepthWrite = m_bDepthWrite;

#if RSG_PC
	bDepthWrite = g_bDepthWrite;
#endif

#if __BANK
	if (!ms_debugCableEnable)
	{
		return;
	}
	else if (ms_debugCableForceDepthWritesOn)
	{
		bDepthWrite = true;
	}
	else if (ms_debugCableForceDepthWritesOff)
	{
		bDepthWrite = false;
	}
#endif // __BANK

	DLC_Add(CCustomShaderEffectCable_SetRenderStates, bDepthWrite, BANK_SWITCH(CRenderPhaseEntitySelect::GetIsBuildingDrawList(),false));
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
}

void CCustomShaderEffectCable::AddToDrawListAfterDraw()
{
#if __BANK
	if (!ms_debugCableEnable)
	{
		return;
	}
#endif // __BANK

	DLC_Add(CCustomShaderEffectCable_RestoreRenderStates);
}

#if __BANK

namespace rage { extern u32 g_AllowVertexBufferVramLocks; }

/*
class CDebugCableError
{
public:
	s16 m_index;
	s16 m_error;
};

class CDebugCableErrorLODGeomErrors
{
public:
	int m_LODIndex;
	int m_geomIndex;

	atArray<CDebugCableError> m_vertexErrors;
	atArray<CDebugCableError> m_triangleErrors;
};

class CDebugCableErrorRecord
{
public:
	atString m_name;
	atArray<CDebugCableErrorLODGeomErrors> m_errors;
};

static atArray<CDebugCableErrorRecord> m_debugCableErrorRecords;
*/

#endif // __BANK

void CCustomShaderEffectCable::Update(fwEntity* BANK_ONLY(pEntity))
{
#if __BANK
	if (ms_debugCableDrawNormals ||
		ms_debugCableDrawWindTestGlobal ||
		ms_debugCableDrawWindTestLocal ||
		ms_debugCableDrawWindTestLocalCamera)
	{
		const rmcDrawable* pDrawable = static_cast<CEntity*>(pEntity)->GetDrawable();
		const Mat34V mat = pEntity->GetMatrix();
		spdSphere sphere = static_cast<CEntity*>(pEntity)->GetBoundSphere();
		const Vec3V camPos = RCC_VEC3V(camInterface::GetPos());

		sphere.SetRadiusf(Max<float>(sphere.GetRadiusf()*2.0f, 50.0f)); // 2x radius

		if (pDrawable && sphere.ContainsPoint(camPos))
		{
			const ScalarV normalsLen  = ScalarV(ms_debugCableDrawNormalsLength);
			const ScalarV windTestLen = ScalarV(ms_debugCableDrawWindTestLength);

			Vec3V windTestGlobal      = Vec3V(V_ZERO);
			Vec3V windTestLocalCamera = Vec3V(V_ZERO);

			WIND.GetGlobalVelocity(WIND_TYPE_AIR, windTestGlobal);
			WIND.GetLocalVelocity(camPos, windTestLocalCamera);

			windTestGlobal      *= windTestLen;
			windTestLocalCamera *= windTestLen;

			const u32 bAllowVBLocks = g_AllowVertexBufferVramLocks; // save
			g_AllowVertexBufferVramLocks = 1;

			const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

			for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
			{
				if (lodGroup.ContainsLod(lodIndex))
				{
					const rmcLod& lod = lodGroup.GetLod(lodIndex);

					for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
					{
						const grmModel* pModel = lod.GetModel(lodModelIndex);

						if (pModel)
						{
							const grmModel& model = *pModel;

							for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
							{
								grmGeometry& geom = model.GetGeometry(geomIndex);

								if (geom.GetType() == grmGeometry::GEOMETRYQB)
								{
									grcVertexBuffer* vb = geom.GetVertexBuffer();
									const grcFvf* fvf = vb->GetFvf();

									if (fvf->IsChannelActive(grcFvf::grcfcPosition) &&
										fvf->IsChannelActive(grcFvf::grcfcNormal))
									{
										grcVertexBufferEditor vbe(vb, true, true); // lock=true, readOnly=true

										for (int i = 0; i < vb->GetVertexCount(); i++)
										{
											const Vec3V pos = Transform(mat, VECTOR3_TO_VEC3V(vbe.GetPosition(i)));

											if (ms_debugCableDrawNormals)
											{
												const Vec3V nrm = Transform3x3(mat, VECTOR3_TO_VEC3V(vbe.GetNormal(i)));

												grcDebugDraw::Line(pos, pos + nrm*normalsLen, Color32(255,0,0,255));
												grcDebugDraw::Line(pos, pos - nrm*normalsLen, Color32(0,0,255,255));
#if CABLE_AUTO_CHECK
												if (ms_debugCableCheckFlags != 0)
												{
													const u32* flags = m_indices.Access((u16)i);

													if (flags && (*flags & ms_debugCableCheckFlags) != 0)
													{
														grcDebugDraw::Sphere(pos, ms_debugCableCheckSphereRadius, Color32(255,255,0,255), false);
													}
												}
#endif // CABLE_AUTO_CHECK
											}

											if (ms_debugCableDrawWindTestGlobal)
											{
												grcDebugDraw::Line(pos, pos + windTestGlobal, Color32(0,255,0,255));
											}

											if (ms_debugCableDrawWindTestLocal)
											{
												Vec3V windTestLocal;
												WIND.GetLocalVelocity(pos, windTestLocal);
												grcDebugDraw::Line(pos, pos + windTestLocal*windTestLen, Color32(0,128,0,255));
											}

											if (ms_debugCableDrawWindTestLocalCamera)
											{
												grcDebugDraw::Line(pos, pos + windTestLocalCamera, Color32(0,128,128,255));
											}
										}
									}
								}
							}
						}
					}
				}
			}

			g_AllowVertexBufferVramLocks = bAllowVBLocks; // restore
		}
	}
#endif // __BANK

	// TODO -- update wind vars etc.
}

void CCustomShaderEffectCable::SetShaderVariables(rmcDrawable* pDrawable)
{
	if (pDrawable)
	{
		grmShaderGroup& shaderGroup = pDrawable->GetShaderGroup();
		Assert(shaderGroup.GetShaderGroupVarCount() > 0);

		Mat44V viewProj;

		const grcViewport* vp = grcViewport::GetCurrent();
		Multiply(viewProj, vp->GetProjection(), vp->GetViewMtx());

		shaderGroup.SetVar(m_pType->m_idVarViewProjection, viewProj);

		bool bInteriorAmbient = true;// m_bInterior;
		bool bExteriorAmbient = true;//!m_bInterior;
		float windAmount = 1.0f;

#if __BANK
		if      (ms_debugCableForceInteriorAmbientOn)  { bInteriorAmbient = true; }
		else if (ms_debugCableForceInteriorAmbientOff) { bInteriorAmbient = false; }

		if      (ms_debugCableForceExteriorAmbientOn)  { bExteriorAmbient = true; }
		else if (ms_debugCableForceExteriorAmbientOff) { bExteriorAmbient = false; }

		if (ms_debugCableWindResponseEnabled && ms_debugCableWindResponseAmount > 0.0f)
#endif // __BANK
		{
			const float windVelMin = BANK_SWITCH(ms_debugCableWindResponseVelMin, CABLE_DEFAULT_WIND_RESPONSE_VEL_MIN);
			const float windVelMax = BANK_SWITCH(ms_debugCableWindResponseVelMax, CABLE_DEFAULT_WIND_RESPONSE_VEL_MAX);
			const float windVelExp = BANK_SWITCH(ms_debugCableWindResponseVelExp, CABLE_DEFAULT_WIND_RESPONSE_VEL_EXP);

			float windResponse = Clamp<float>((g_fWindVelocity - windVelMin)/(windVelMax - windVelMin), 0.0f, 1.0f);

			if (windVelExp != 0.0f)
			{
				windResponse = powf(windResponse, powf(2.0f, windVelExp));
			}

			windAmount += windResponse*BANK_SWITCH(ms_debugCableWindResponseAmount, CABLE_DEFAULT_WIND_RESPONSE_AMOUNT);
		}

		const float pixelThickness = BANK_SWITCH(ms_debugCablePixelThickness, CABLE_DEFAULT_PIXEL_THICKNESS);
		const float shadowThickness = BANK_SWITCH(ms_debugCableShadowThickness, CABLE_DEFAULT_SHADOW_THICKNESS);
		const float shadowWindModifier = BANK_SWITCH(ms_debugCableShadowWindModifier, CABLE_DEFAULT_SHADOW_WIND_SPEED);
		const Vec4f params[] =
		{
			Vec4f(
				(float)GRCDEVICE.GetHeight()/(pixelThickness*vp->GetTanVFOV()),
				BANK_SWITCH(ms_debugCableRadiusScale    , CABLE_DEFAULT_RADIUS_SCALE     ),
				BANK_SWITCH(ms_debugCableFadeExponentAdj, CABLE_DEFAULT_FADE_EXPONENT_ADJ),
				BANK_SWITCH(ms_debugCableOpacity, 1.0f)
			),
			Vec4f(
				BANK_SWITCH(ms_debugCableWindMotionFreqX*ms_debugCableWindMotionFreqScale, CABLE_DEFAULT_WIND_MOTION_FREQ_X)*g_fWindTime,
				BANK_SWITCH(ms_debugCableWindMotionFreqY*ms_debugCableWindMotionFreqScale, CABLE_DEFAULT_WIND_MOTION_FREQ_Y)*g_fWindTime,
				BANK_SWITCH(ms_debugCableWindMotionAmpX *ms_debugCableWindMotionAmpScale , CABLE_DEFAULT_WIND_MOTION_AMP_X ) BANK_ONLY(*(ms_debugCableWindMotionEnabled ? 1.0f : 0.0f))*windAmount,
				BANK_SWITCH(ms_debugCableWindMotionAmpY *ms_debugCableWindMotionAmpScale , CABLE_DEFAULT_WIND_MOTION_AMP_Y ) BANK_ONLY(*(ms_debugCableWindMotionEnabled ? 1.0f : 0.0f))*windAmount
			),
			Vec4f(
				bInteriorAmbient ? 1.0f : 0.0f,
				bExteriorAmbient ? 1.0f : 0.0f,
				shadowThickness,
				shadowWindModifier
			),
			Vec4f(
				BANK_SWITCH(ms_debugCableDiffuse.GetRedf  (), 0.0f),
				BANK_SWITCH(ms_debugCableDiffuse.GetGreenf(), 0.0f),
				BANK_SWITCH(ms_debugCableDiffuse.GetBluef (), 0.0f),
				BANK_SWITCH(ms_debugCableDiffuseOverride, false) ? 1.0f : 0.0f
			),
			Vec4f(
				BANK_SWITCH(ms_debugCableAmbientNatural   , 0.4f),
				BANK_SWITCH(ms_debugCableAmbientArtificial, 0.4f),
				BANK_SWITCH(ms_debugCableAmbientEmissive  , 0.0f),
				BANK_SWITCH(ms_debugCableAmbientOverride, false) ? 1.0f : 0.0f
			),
		};

		shaderGroup.SetVar(m_pType->m_idVarCableParams, params, NELEM(params));

		float alphaTest = s_fAlphaTestValue;
		if (g_bDrawCoreFlag)
		{
			// depending on the weather conditions, we lower the core alpha test with more environmental blur
			const PostFX::PostFXParamBlock::paramBlock& settings = PostFX::PostFXParamBlock::GetRenderThreadParams();
			const float blur = Clamp((settings.m_EnvironmentalBlurParams.GetZ() - g_MinDofBlurDiskRadius) / g_MaxDofBlurDiskRadius, 0.f, 1.f);
			alphaTest = Lerp(blur, s_fCoreAlphaTestValue, s_fCoreBlurredAlphaTestValue);
		}
		
		shaderGroup.SetVar(m_pType->m_idVarAlphaTest, alphaTest);

#if __BANK
		if (ms_debugCableTextureOverride != 0)
		{
			shaderGroup.SetVar(m_pType->m_idVarCableTexture, ms_debugCableTextures[ms_debugCableTextureOverride - 1]);
		}

		CCustomShaderEffectCable::SetEditableShaderValues(&shaderGroup, pDrawable);
#endif // __BANK
	}
}

void CCustomShaderEffectCable::Init()
{
	if (g_rs_core == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc desc;
		//desc.CullMode = grcRSV::CULL_FRONT;	// would only work when tessellation is disabled
		const float offset = USE_INVERTED_PROJECTION_ONLY ? -1.0 : 1.0;
#if __D3D11
		desc.SlopeScaledDepthBias = offset;
		desc.DepthBiasDX10 = static_cast<int>(offset);
#elif RSG_ORBIS
		desc.SlopeScaledDepthBias = 16.f * offset;
		desc.DepthBiasDX9 = offset;
#endif
		g_rs_core = grcStateBlock::CreateRasterizerState(desc);
	}
	if (g_bs_core == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc desc;
		desc.BlendRTDesc[0].RenderTargetWriteMask = 0;
		g_bs_core = grcStateBlock::CreateBlendState(desc);
	}
#if __BANK
	if (ms_debugCableTextures[0] == NULL) // create textures
	{
		const int w = __D3D11 ? 16 : 1;
		const int h = 16;

		int numMips = 0;

		while ((Max<int>(w, h) >> numMips) >= 4) // smallest mip should be 4 pixels, i.e. [0,255,255,0]
		{
			numMips++;
		}

		grcImage* image = grcImage::Create(
			w,
			h,
			1, // depth
			grcImage::L8,
			grcImage::STANDARD,
			numMips - 1, // extraMipmaps
			0 // extraLayers
		);
		Assert(image);

		for (int index = 0; index < NELEM(ms_debugCableTextures); index++)
		{
			grcImage* mip = image;

			while (mip)
			{
				const int w0 = mip->GetWidth();
				const int h0 = mip->GetHeight();

				u8* data = mip->GetBits();

				for (int i = 0, y = 0; y < h0; y++)
				{
					u8 a = 0xff;

					switch (index)
					{
					case 0: a = 0xff; break;
					case 1: a = (y <= 0 || y >= h0 - 1) ? 0x00 : 0xff; break; // 2-pixel thick in last mip
					case 2: a = (y <= 1 || y >= h0 - 1) ? 0x00 : 0xff; break; // 1-pixel thick in last mip
					}

					for (int x = 0; x < w0; x++, i++)
					{
						data[i] = a;
					}
				}

				mip = mip->GetNext();
			}

			BANK_ONLY(grcTexture::SetCustomLoadName("debugCableTextures");)
			ms_debugCableTextures[index] = grcTextureFactory::GetInstance().Create(image);
			Assert(ms_debugCableTextures[index]);
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
		}

		image->Release();
	}
#endif // __BANK
}

#if __BANK
#if __DEV

static bool CheckCablesForEntity(void* item, void*)
{
	const CEntity* pEntity = static_cast<const CEntity*>(item);

	if (pEntity)
	{
		CCustomShaderEffectCable::CheckDrawable(pEntity->GetDrawable(), pEntity->GetModelName());
	}

	return true;
}

static void CheckCables()
{
	CBuilding        ::GetPool()->ForAll(CheckCablesForEntity, NULL);
	CAnimatedBuilding::GetPool()->ForAll(CheckCablesForEntity, NULL);
	CObject          ::GetPool()->ForAll(CheckCablesForEntity, NULL);
	CDummyObject     ::GetPool()->ForAll(CheckCablesForEntity, NULL);
}

static const CEntity* g_closestCableEntity = NULL;
static float          g_closestCableDistance = 0.0f;

static bool FindClosestCableForEntity(void* item, void*)
{
	const CEntity* pEntity = static_cast<const CEntity*>(item);

	if (pEntity)
	{
		float distToCamera = FLT_MAX;

		CCustomShaderEffectCable::CheckDrawable(pEntity->GetDrawable(), pEntity->GetModelName(), false, NULL, pEntity->GetMatrix(), &distToCamera);

		if (distToCamera < g_closestCableDistance || g_closestCableEntity == NULL)
		{
			g_closestCableEntity = pEntity;
			g_closestCableDistance = distToCamera;
		}
	}

	return true;
}

static void FindClosestCable()
{
	g_closestCableEntity = NULL;
	g_closestCableDistance = 0.0f;

	CBuilding        ::GetPool()->ForAll(FindClosestCableForEntity, NULL);
	CAnimatedBuilding::GetPool()->ForAll(FindClosestCableForEntity, NULL);
	CObject          ::GetPool()->ForAll(FindClosestCableForEntity, NULL);
	CDummyObject     ::GetPool()->ForAll(FindClosestCableForEntity, NULL);

	if (g_closestCableEntity)
	{
		Displayf("closest cable entity: \"%s\", distance = %f", g_closestCableEntity->GetModelName(), g_closestCableDistance);
	}
	else
	{
		Displayf("closest cable entity: NONE");
	}
}

#endif // __DEV

void CCustomShaderEffectCable::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Cable", false);
	{
		bank.AddToggle("Enable", &ms_debugCableEnable);
		bank.AddToggle("Force Interior Ambient On", &ms_debugCableForceInteriorAmbientOn);
		bank.AddToggle("Force Interior Ambient Off", &ms_debugCableForceInteriorAmbientOff);
		bank.AddToggle("Force Exterior Ambient On", &ms_debugCableForceExteriorAmbientOn);
		bank.AddToggle("Force Exterior Ambient Off", &ms_debugCableForceExteriorAmbientOff);
		bank.AddToggle("Force Depth Writes On", &ms_debugCableForceDepthWritesOn);
		bank.AddToggle("Force Depth Writes Off", &ms_debugCableForceDepthWritesOff);
#if __DEV
		bank.AddButton("Check Cables", CheckCables);
		bank.AddButton("Find Closest Cable", FindClosestCable);
#endif // __DEV
		bank.AddToggle("Debug Draw Normals", &ms_debugCableDrawNormals);
		bank.AddSlider("Debug Draw Length", &ms_debugCableDrawNormalsLength, 1.0f/8.0f, 4.0f, 1.0f/32.0f);
		bank.AddToggle("Debug Draw Wind Test - Global", &ms_debugCableDrawWindTestGlobal);
		bank.AddToggle("Debug Draw Wind Test - Local", &ms_debugCableDrawWindTestLocal);
		bank.AddToggle("Debug Draw Wind Test - Local Camera", &ms_debugCableDrawWindTestLocalCamera);
		bank.AddSlider("Debug Draw Wind Length", &ms_debugCableDrawWindTestLength, 1.0f/64.0f, 64.0f, 1.0f/128.0f);

		const char* textureOverrideStrings[] =
		{
			"None",
			"No Border",
			"1-1 Texel Border",
			"1-2 Texel Border",
		};

		bank.AddCombo ("Texture Override", &ms_debugCableTextureOverride, NELEM(textureOverrideStrings), textureOverrideStrings);
		bank.AddSlider("Radius Scale", &ms_debugCableRadiusScale, 1.0f/8.0f, 8.0f, 1.0f/128.0f);
		bank.AddSlider("Pixel Thickness", &ms_debugCablePixelThickness, 1.0f, 8.0f, 1.0f/32.0f);
		bank.AddSlider("Cable shadow thickness", &ms_debugCableShadowThickness, 0.0f, 0.25f, 1.0f/128.0f);
		bank.AddSlider("Cable shadow wind modifier", &ms_debugCableShadowWindModifier, 0.0f, 1.0f, 0.1f);
		bank.AddSlider("Fade Exp Adj", &ms_debugCableFadeExponentAdj, -1.0f, 1.0f, 1.0f/32.0f);
		bank.AddToggle("Cable Diffuse Override", &ms_debugCableDiffuseOverride);
		bank.AddColor ("Cable Diffuse", &ms_debugCableDiffuse);
		bank.AddToggle("Cable Ambient Override", &ms_debugCableAmbientOverride);
		bank.AddSlider("Cable Ambient Natural", &ms_debugCableAmbientNatural, 0.0f, 1.0f, 1.0f/32.0f);
		bank.AddSlider("Cable Ambient Artificial", &ms_debugCableAmbientArtificial, 0.0f, 1.0f, 1.0f/32.0f);
		bank.AddSlider("Cable Ambient Emissive", &ms_debugCableAmbientEmissive, 0.0f, 32.0f, 1.0f/32.0f);
		bank.AddSlider("Cable Opacity", &ms_debugCableOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bank.AddSeparator();
		bank.AddToggle("Wind Motion Enabled", &ms_debugCableWindMotionEnabled);
		bank.AddSlider("Wind Motion Freq Scale", &ms_debugCableWindMotionFreqScale, 0.0f, 64.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Motion Freq X", &ms_debugCableWindMotionFreqX, 0.0f, 10.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Motion Freq Y", &ms_debugCableWindMotionFreqY, 0.0f, 10.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Motion Amp Scale", &ms_debugCableWindMotionAmpScale, 0.0f, 64.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Motion Amp X", &ms_debugCableWindMotionAmpX, 0.0f, PI/2.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Motion Amp Y", &ms_debugCableWindMotionAmpY, 0.0f, PI/2.0f, 1.0f/256.0f);
		bank.AddSeparator();
		bank.AddToggle("Wind Response Enabled", &ms_debugCableWindResponseEnabled);
		bank.AddSlider("Wind Response Amount", &ms_debugCableWindResponseAmount, 0.0f, 50.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Response Vel Min", &ms_debugCableWindResponseVelMin, 0.0f, 64.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Response Vel Max", &ms_debugCableWindResponseVelMax, 0.0f, 64.0f, 1.0f/256.0f);
		bank.AddSlider("Wind Response Vel Exp", &ms_debugCableWindResponseVelExp, -4.0f, 4.0f, 1.0f/256.0f);

		bank.AddSeparator();
		bank.AddSlider("Alpha test value", &s_fAlphaTestValue, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Core cable max depth", &s_fDrawCoreMaxDepth, 0.0f, 10000.0f, 1.f);
		bank.AddSlider("Core alpha test value", &s_fCoreAlphaTestValue, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Core (blurred) alpha test value", &s_fCoreBlurredAlphaTestValue, 0.0f, 1.0f, 0.01f);

		// note: you can scale global wind speed by adjusting "Weather/Weather System/Settings/Wind Speed Max" in rag
		// bank.AddSeparator();
		// bank.AddSlider("Global Wind Scale", &WIND.GetGlobalWindScaleRef(), 0.0f, 50.0f, 1.0f/32.0f);

#if CABLE_AUTO_CHECK
		bank.PushGroup("Cable Check", false);
		{
			bank.AddSlider("Cable Check Sphere Radius", &ms_debugCableCheckSphereRadius, 0.1f, 10.0f, 1.0f/32.0f);

			for (int i = 0; i < 20; i++)
			{
				bank.AddToggle(atVarString("Cable Check BIT(%d)", i).c_str(), &ms_debugCableCheckFlags, BIT(i));
			}

			class ClearAllFlags { public: static void func()
			{
				ms_debugCableCheckFlags = 0;
			}};

			class SetAllFlags { public: static void func()
			{
				ms_debugCableCheckFlags = ~0U;
			}};

			bank.AddButton("Clear All Flags", ClearAllFlags::func);
			bank.AddButton("Set All Flags", SetAllFlags::func);
		}
		bank.PopGroup();
#endif // CABLE_AUTO_CHECK
	}
	bank.PopGroup();
}

void CCustomShaderEffectCable::InitOptimisationWidgets(bkBank& bank)
{
	bank.AddToggle("Draw Cable", &ms_debugCableEnable);
}

void CCustomShaderEffectCable::SetEditableShaderValues(grmShaderGroup* UNUSED_PARAM(pShaderGroup), rmcDrawable* UNUSED_PARAM(pDrawable))
{
}

#if __DEV

static void AddIndex(atMap<u16,u32>* indices, u32 flags, u16 a, u16 b)
{
	if (indices)
	{
		u32* a_ptr = indices->Access(a); if (a_ptr) { *a_ptr |= flags; } else { indices->operator[](a) = flags; }
		u32* b_ptr = indices->Access(b); if (b_ptr) { *b_ptr |= flags; } else { indices->operator[](b) = flags; }
	}
}

static void AddIndex(atMap<u16,u32>* indices, u32 flags, u16 a)
{
	if (indices)
	{
		u32* a_ptr = indices->Access(a); if (a_ptr) { *a_ptr |= flags; } else { indices->operator[](a) = flags; }
	}
}

bool CCustomShaderEffectCable::CheckDrawable(const Drawable* pDrawable, const char* path, bool bSimple, atMap<u16,u32>* indices, Mat34V_In matrix, float* distToCamera)
{
	if (pDrawable == NULL)
	{
		return true;
	}

	float maxSegmentAngle = 0.0f;
	float maxTangentError = 0.0f;
	int numBadTangents = 0;
	int numOkTangents = 0;
	float maxTangentError2 = 0.0f;
	int numBadTangents2 = 0;
	int numOkTangents2 = 0;
	float maxTangentError3 = 0.0f;
	int numBadTangents3 = 0;
	int numOkTangents3 = 0;
	int numBadOppositeTangents = 0;
	int numOkOppositeTangents = 0;
	int numBadOppositeTangents2a = 0;
	int numOkOppositeTangents2a = 0;
	int numBadOppositeTangents2b = 0;
	int numOkOppositeTangents2b = 0;
	int numBadOppositeTangents2c = 0;
	int numOkOppositeTangents2c = 0;
	int numBadOppositeTexcoord = 0;
	int numOkOppositeTexcoord = 0;
	int numBadOppositeTexcoord2a = 0;
	int numOkOppositeTexcoord2a = 0;
	int numBadOppositeTexcoord2b = 0;
	int numOkOppositeTexcoord2b = 0;
	int numBadOppositeTexcoord2c = 0;
	int numOkOppositeTexcoord2c = 0;
	int numBadTopology = 0; // general topology errors
	int numDegenerateTriangles = 0; // triangles which have all 3 vertices the same position
	int numNonStripTriangles = 0; // triangles which have all 3 vertices unique position
	int numRedundantVertices = 0; // vertices which are completely redundant
	int numOkVertices = 0;
	int numCableGeoms = 0;
	int numCableBadEndpoints = 0;
	int numCableOkEndpoints = 0;
	int numBadTriangleWinding = 0;
	int numOkTriangleWinding = 0;
	int numBadTriangles = 3; // TODO -- re-enable this, if we need to dump verbose output
	int numTriangleErrorsNonEndpoint = 0;

	const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();

	for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
	{
		if (lodGroup.ContainsLod(lodIndex))
		{
			const rmcLod& lod = lodGroup.GetLod(lodIndex);

			for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
			{
				const grmModel* pModel = lod.GetModel(lodModelIndex);

				if (pModel)
				{
					const grmModel& model = *pModel;

					for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
					{
						const int shaderIndex = model.GetShaderIndex(geomIndex);
						const grmShader& shader = pDrawable->GetShaderGroup().GetShader(shaderIndex);

						if (strcmp(shader.GetName(), "cable") != 0)
						{
							continue;
						}

						grmGeometry& geom = model.GetGeometry(geomIndex);

						if (geom.GetType() == grmGeometry::GEOMETRYQB)
						{
							grcVertexBuffer* vb = geom.GetVertexBuffer(true);
							grcIndexBuffer*  ib = geom.GetIndexBuffer (true);

							Assert(vb && ib);
							Assert(geom.GetPrimitiveType() == drawTris);
							Assert(geom.GetPrimitiveCount()*3 == (u32)ib->GetIndexCount());

							const int numTriangles = ib->GetIndexCount()/3;
							u16* indexData = rage_new u16[ib->GetIndexCount()];

							// copy index data, otherwise we get an assert "Attempting to lock an index buffer while vertex buffers are locked. Potential dead lock condition."
							{
								sysMemCpy(indexData, ib->LockRO(), ib->GetIndexCount()*sizeof(u16));
								ib->UnlockRO();
								ib = NULL;
							}

							PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM
							grcVertexBufferEditor vertexBufferEditor(vb, true, true); // lock=true, readOnly=true
							const int numVerts = vb->GetVertexCount();

							if (distToCamera) // if this is not NULL, then we're only interested in getting the distance to the camera
							{
								const Vec3V camPos = RCC_VEC3V(camInterface::GetPos());

								for (int i = 0; i < numVerts; i++)
								{
									const Vec3V position = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(i));

									*distToCamera = Min<float>(*distToCamera, Mag(Transform(matrix, position) - camPos).Getf());
								}
							}
							else
							{
								for (int i = 0; i < numTriangles; i++)
								{
									bool bTriangleMightBeEndpoint = false;
									u32 triangleErrorFlags = 0;

									const Vec3V temp[3] =
									{
										VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(indexData[i*3 + 0])),
										VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(indexData[i*3 + 1])),
										VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(indexData[i*3 + 2])),
									};

									int j = -1;

									if (IsEqualAll(temp[0], temp[1]) &
										IsEqualAll(temp[0], temp[2]))
									{
										numDegenerateTriangles++;
										continue;
									}

									if      (IsEqualAll(temp[0], temp[1])) { j = 2; }
									else if (IsEqualAll(temp[1], temp[2])) { j = 0; }
									else if (IsEqualAll(temp[2], temp[0])) { j = 1; }

									if (j != -1)
									{
										const u16 idx0 = indexData[i*3 + (j + 0)];
										const u16 idx1 = indexData[i*3 + (j + 1)%3];
										const u16 idx2 = indexData[i*3 + (j + 2)%3];

										const Vec3V v0 = temp[(j + 0)]; // v0 should be unique position
										const Vec3V v1 = temp[(j + 1)%3]; // v1 and v2 are the same position but should have opposite signs in texcoord.x
										const Vec3V v2 = temp[(j + 2)%3];

										const Vec3V tangent0 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx0));
										const Vec3V tangent1 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx1));
										const Vec3V tangent2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx2));

										const Vector2 tex0 = vertexBufferEditor.GetUV(idx0, 0);
										const Vector2 tex1 = vertexBufferEditor.GetUV(idx1, 0);
										const Vector2 tex2 = vertexBufferEditor.GetUV(idx2, 0);

										const Vec2V texcoord0 = Vec2V(tex0.x, 1.0f - tex0.y);
										const Vec2V texcoord1 = Vec2V(tex1.x, 1.0f - tex1.y);
										const Vec2V texcoord2 = Vec2V(tex2.x, 1.0f - tex2.y);

										if (Abs<float>(texcoord0.GetYf()) <= 0.001f ||
											Abs<float>(texcoord1.GetYf()) <= 0.001f ||
											Abs<float>(texcoord2.GetYf()) <= 0.001f)
										{
											bTriangleMightBeEndpoint = true;
										}

										const bool sign0 = texcoord0.GetXf() > 0.0f;
										const bool sign1 = texcoord1.GetXf() > 0.0f;
										const bool sign2 = texcoord2.GetXf() > 0.0f;

										if (sign0 != sign2 || sign0 == sign1)
										{
											numBadTriangleWinding++;
											continue; // can't handle this
										}
										else
										{
											numOkTriangleWinding++;
										}

										u32 errorFlags = 0;

										if (IsEqualAll(tangent1, tangent2) == 0) { numBadOppositeTangents++; errorFlags |= BIT(0); AddIndex(indices, BIT(0), idx1, idx2); }
										else                                     { numOkOppositeTangents++; }

										if (IsEqualAll(texcoord1, Vec2V(-1.0f, 1.0f)*texcoord2) == 0) { numBadOppositeTexcoord++; errorFlags |= BIT(1); AddIndex(indices, BIT(1), idx1, idx2); }
										else                                                          { numOkOppositeTexcoord++; }

										int numFoundNeighbours2 = 0;
										int alongsideTriangleIndex = -1;
										int alongsideTriangleCorner = 0;

										for (int i_2 = 0; i_2 < numTriangles; i_2++)
										{
											u32 errorFlags_2 = errorFlags;

											if (i_2 == i)
											{
												continue;
											}

											const Vec3V temp_2[3] =
											{
												VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(indexData[i_2*3 + 0])),
												VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(indexData[i_2*3 + 1])),
												VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(indexData[i_2*3 + 2])),
											};

											int j_2 = -1;

											if      (IsEqualAll(v0, temp_2[0]) & IsEqualAll(v0, temp_2[1])) { j_2 = 2; }
											else if (IsEqualAll(v0, temp_2[1]) & IsEqualAll(v0, temp_2[2])) { j_2 = 0; }
											else if (IsEqualAll(v0, temp_2[2]) & IsEqualAll(v0, temp_2[0])) { j_2 = 1; }

											if (j_2 != -1)
											{
												const u16 idx0_2 = indexData[i_2*3 + (j_2 + 0)];
												const u16 idx1_2 = indexData[i_2*3 + (j_2 + 1)%3];
												const u16 idx2_2 = indexData[i_2*3 + (j_2 + 2)%3];

												const Vec3V v0_2 = temp_2[(j_2 + 0)]; // v0_2 should be unique position
												const Vec3V v1_2 = temp_2[(j_2 + 1)%3]; // v1_2 and v2_2 are the same position as v0 but should have opposite signs in texcoord.x
												const Vec3V v2_2 = temp_2[(j_2 + 2)%3];

												const Vec3V tangent0_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx0_2));
												const Vec3V tangent1_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx1_2));
												const Vec3V tangent2_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx2_2));

												const Vector2 tex0_2 = vertexBufferEditor.GetUV(idx0_2, 0);
												const Vector2 tex1_2 = vertexBufferEditor.GetUV(idx1_2, 0);
												const Vector2 tex2_2 = vertexBufferEditor.GetUV(idx2_2, 0);

												const Vec2V texcoord0_2 = Vec2V(tex0_2.x, 1.0f - tex0_2.y);
												const Vec2V texcoord1_2 = Vec2V(tex1_2.x, 1.0f - tex1_2.y);
												const Vec2V texcoord2_2 = Vec2V(tex2_2.x, 1.0f - tex2_2.y);

												if (Abs<float>(texcoord0_2.GetYf()) <= 0.001f ||
													Abs<float>(texcoord1_2.GetYf()) <= 0.001f ||
													Abs<float>(texcoord2_2.GetYf()) <= 0.001f)
												{
													bTriangleMightBeEndpoint = true;
												}

												if (IsEqualAll(v0_2, v1)) // triangle i_2 lies alongside triangle i ..
												{
													// ...1=0------2...
													//    |\ \     |
													//    | \ \  i |
													//    |  \ \   |
													//    |   \ \  |
													//    |i_2 \ \ |
													//    |     \ \|
													// ...2------0=1...

													if (IsEqualAll(tangent1_2, tangent0) == 0) { numBadOppositeTangents2a++; errorFlags_2 |= BIT(2); AddIndex(indices, BIT(2), idx1_2, idx0); }
													else                                       { numOkOppositeTangents2a++; }

													if (IsEqualAll(tangent0_2, tangent1) == 0) { numBadOppositeTangents2b++; errorFlags_2 |= BIT(3); AddIndex(indices, BIT(3), idx0_2, idx1); }
													else                                       { numOkOppositeTangents2b++; }

													if (IsEqualAll(texcoord1_2, texcoord0) == 0) { numBadOppositeTexcoord2a++; errorFlags_2 |= BIT(4); AddIndex(indices, BIT(4), idx1_2, idx0); }
													else                                         { numOkOppositeTexcoord2a++; }

													if (IsEqualAll(texcoord0_2, texcoord1) == 0) { numBadOppositeTexcoord2b++; errorFlags_2 |= BIT(5); AddIndex(indices, BIT(5), idx0_2, idx1); }
													else                                         { numOkOppositeTexcoord2b++; }

													if (errorFlags_2 != 0 && numBadTriangles < 3)
													{
														numBadTriangles++;

														Displayf("bad triangle found (i1=%d,i2=%d), flags=0x%04x, configuration 1:", i, i_2, errorFlags_2);
														Displayf("  i1 pos: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(v0         ), VEC3V_ARGS(v1         ), VEC3V_ARGS(v2         ));
														Displayf("  i1 tan: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(tangent0   ), VEC3V_ARGS(tangent1   ), VEC3V_ARGS(tangent2   ));
														Displayf("  i1 tex: (%f,%f), (%f,%f), (%f,%f)"         , VEC2V_ARGS(texcoord0  ), VEC2V_ARGS(texcoord1  ), VEC2V_ARGS(texcoord2  ));
														Displayf("  i2 pos: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(v0_2       ), VEC3V_ARGS(v1_2       ), VEC3V_ARGS(v2_2       ));
														Displayf("  i2 tan: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(tangent0_2 ), VEC3V_ARGS(tangent1_2 ), VEC3V_ARGS(tangent2_2 ));
														Displayf("  i2 tex: (%f,%f), (%f,%f), (%f,%f)"         , VEC2V_ARGS(texcoord0_2), VEC2V_ARGS(texcoord1_2), VEC2V_ARGS(texcoord2_2));
													}

													alongsideTriangleIndex = i_2;
													alongsideTriangleCorner = j_2;
												}
												else // .. triangle i_2 lies adjacent to triangle i
												{
													// ...0------2=0------2...
													//     \     |  \     |   
													//      \i_2 |   \  i |
													//       \   |    \   |   
													//        \  |     \  |   
													//         \ |      \ |   
													//          \|       \|   
													// ..........1........1...

													if (IsEqualAll(tangent2_2, tangent0) == 0) { numBadOppositeTangents2c++; errorFlags_2 |= BIT(6); AddIndex(indices, BIT(6), idx2_2, idx0); }
													else                                       { numOkOppositeTangents2c++; }

													if (IsEqualAll(texcoord2_2, texcoord0) == 0) { numBadOppositeTexcoord2c++; errorFlags_2 |= BIT(7); AddIndex(indices, BIT(7), idx2_2, idx0); }
													else                                         { numOkOppositeTexcoord2c++; }

													const Vec3V expectedTangent0 = Normalize(v2 - v0_2);

													const float tangentErrorA = Abs<float>(1.0f - Dot(expectedTangent0, tangent0).Getf());
													const float tangentErrorB = Abs<float>(1.0f + Dot(expectedTangent0, tangent0).Getf());
													const float tangentError = Min<float>(tangentErrorA, tangentErrorB);

													maxTangentError = Max<float>(tangentError, maxTangentError);

													if (tangentError > 1.0f/127.0f) { numBadTangents++; errorFlags_2 |= BIT(8); AddIndex(indices, BIT(8), idx0); }
													else                            { numOkTangents++; }

													numFoundNeighbours2++;

													if (errorFlags_2 != 0 && numBadTriangles < 3)
													{
														numBadTriangles++;

														Displayf("bad triangle found (i1=%d,i2=%d), flags=0x%04x, configuration 2:", i, i_2, errorFlags_2);
														Displayf("  i1 pos: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(v0         ), VEC3V_ARGS(v1         ), VEC3V_ARGS(v2         ));
														Displayf("  i1 tan: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(tangent0   ), VEC3V_ARGS(tangent1   ), VEC3V_ARGS(tangent2   ));
														Displayf("  i1 tex: (%f,%f), (%f,%f), (%f,%f)"         , VEC2V_ARGS(texcoord0  ), VEC2V_ARGS(texcoord1  ), VEC2V_ARGS(texcoord2  ));
														Displayf("  i2 pos: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(v0_2       ), VEC3V_ARGS(v1_2       ), VEC3V_ARGS(v2_2       ));
														Displayf("  i2 tan: (%f,%f,%f), (%f,%f,%f), (%f,%f,%f)", VEC3V_ARGS(tangent0_2 ), VEC3V_ARGS(tangent1_2 ), VEC3V_ARGS(tangent2_2 ));
														Displayf("  i2 tex: (%f,%f), (%f,%f), (%f,%f)"         , VEC2V_ARGS(texcoord0_2), VEC2V_ARGS(texcoord1_2), VEC2V_ARGS(texcoord2_2));
													}

													maxSegmentAngle = Max<float>(RtoD*acosf(Dot(Normalize(v1 - v0), Normalize(v1_2 - v0_2)).Getf()), maxSegmentAngle);
												}
											}

											triangleErrorFlags |= errorFlags_2;
										}

										if (numFoundNeighbours2 > 1)
										{
											numBadTopology++; // more than two cables connected at a point?
										}
										else if (numFoundNeighbours2 == 0) // v0 is at the end of the cable
										{
											// 1=0------2...
											// |\ \     |
											// | \ \  i |
											// |  \ \   |
											// |   \ \  |
											// |i_2 \ \ |
											// |     \ \|
											// 2------0=1...

											const Vec3V expectedTangent0 = Normalize(v2 - v0);

											// check expected tangent 0
											{
												const float tangentErrorA = Abs<float>(1.0f - Dot(expectedTangent0, tangent0).Getf());
												const float tangentErrorB = Abs<float>(1.0f + Dot(expectedTangent0, tangent0).Getf());
												const float tangentError2 = Min<float>(tangentErrorA, tangentErrorB);

												maxTangentError2 = Max<float>(tangentError2, maxTangentError2);

												if (tangentError2 > 1.0f/127.0f) { numBadTangents2++; errorFlags |= BIT(9); AddIndex(indices, BIT(9), idx0); }
												else                             { numOkTangents2++; }
											}

											const int i_2 = alongsideTriangleIndex;
											const int j_2 = alongsideTriangleCorner;

											if (i_2 != -1)
											{
												const u16 idx0_2 = indexData[i_2*3 + (j_2 + 0)];
											//	const u16 idx1_2 = indexData[i_2*3 + (j_2 + 1)%3];
												const u16 idx2_2 = indexData[i_2*3 + (j_2 + 2)%3];

												const Vec3V v0_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(idx0_2));
											//	const Vec3V v1_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(idx1_2));
												const Vec3V v2_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(idx2_2));

											//	const Vec3V tangent0_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx0_2));
											//	const Vec3V tangent1_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx1_2));
												const Vec3V tangent2_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(idx2_2));

												// check expected tangent 1 (on alongside triangle)
												{
													const Vec3V expectedTangent1_2 = Normalize(v0_2 - v2_2);

													const float tangentErrorA = Abs<float>(1.0f - Dot(expectedTangent1_2, tangent2_2).Getf());
													const float tangentErrorB = Abs<float>(1.0f + Dot(expectedTangent1_2, tangent2_2).Getf());
													const float tangentError3 = Min<float>(tangentErrorA, tangentErrorB);

													maxTangentError3 = Max<float>(tangentError3, maxTangentError3);

													if (tangentError3 > 1.0f/127.0f) { numBadTangents3++; errorFlags |= BIT(10); AddIndex(indices, BIT(10), idx2_2); }
													else                             { numOkTangents3++; }
												}

												numCableOkEndpoints++;
											}
											else
											{
												AddIndex(indices, BIT(11), idx0, idx1);
												numCableBadEndpoints++;
											}
										}

										triangleErrorFlags |= errorFlags;
									}
									else
									{
										triangleErrorFlags |= BIT(12);

										AddIndex(indices, BIT(12), indexData[i*3 + (j + 0)]);
										AddIndex(indices, BIT(12), indexData[i*3 + (j + 1)%3]);
										AddIndex(indices, BIT(12), indexData[i*3 + (j + 2)%3]);
										numNonStripTriangles++;
									}

									if (triangleErrorFlags != 0 && !bTriangleMightBeEndpoint)
									{
										numTriangleErrorsNonEndpoint++;
									}
								}

								for (int i = 0; i < numVerts; i++)
								{
									const Vec3V   position = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(i));
									const Vector2 tex      = vertexBufferEditor.GetUV(i, 0);
									const Vec2V   texcoord = Vec2V(tex.x, 1.0f - tex.y);
									const Vec3V   tangent  = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(i));
									const Color32 colour   = vertexBufferEditor.GetDiffuse(i);

									bool bRedundant = false;

									for (int i_2 = i + 1; i_2 < numVerts; i_2++)
									{
										const Vec3V   position_2 = VECTOR3_TO_VEC3V(vertexBufferEditor.GetPosition(i_2));
										const Vector2 tex_2      = vertexBufferEditor.GetUV(i_2, 0);
										const Vec2V   texcoord_2 = Vec2V(tex_2.x, 1.0f - tex_2.y);
										const Vec3V   tangent_2  = VECTOR3_TO_VEC3V(vertexBufferEditor.GetNormal(i_2));
										const Color32 colour_2   = vertexBufferEditor.GetDiffuse(i_2);

										if (IsEqualAll(position_2, position) &
											IsEqualAll(texcoord_2, texcoord) &
											IsEqualAll(tangent_2, tangent))
										{
											if (colour == colour_2)
											{
												AddIndex(indices, BIT(13), (u16)i);
												numRedundantVertices++;
												bRedundant = true;
												break;
											}
										}
									}

									if (!bRedundant)
									{
										numOkVertices++;
									}
								}
							}

							delete[] indexData;
							PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: finished with editing VBs in VRAM

							numCableGeoms++;
						}
					}
				}
			}
		}
	}

	if (distToCamera)
	{
		return true;
	}

	if (numCableGeoms > 0)
	{
		bool bSimpleErrorsOnly = bSimple;

		if (numBadTriangleWinding        > 0 ||
			numDegenerateTriangles       > 0 ||
			numNonStripTriangles         > 0 ||
			numRedundantVertices         > 0 ||
			numTriangleErrorsNonEndpoint > 0)
		{
			bSimpleErrorsOnly = false;
		}

		atArray<atString> errors;

		if (!lodGroup.ContainsLod(LOD_HIGH))
		{
			errors.PushAndGrow(atString("does not have LOD_HIGH"));
		}

		if (numBadTriangleWinding > 0)
		{
			errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad triangle winding", numBadTriangleWinding, numBadTriangleWinding + numOkTriangleWinding, 100.0f*(float)numBadTriangleWinding/(float)(numBadTriangleWinding + numOkTriangleWinding)));
		}
		else if (!bSimpleErrorsOnly)
		{
			if (numBadTangents > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad tangents, max err = %f (%f degrees)", numBadTangents, numBadTangents + numOkTangents, 100.0f*(float)numBadTangents/(float)(numBadTangents + numOkTangents), maxTangentError, RtoD*acosf(1.0f - maxTangentError)));
			}

			if (numBadTangents2 > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad tangents (2), max err = %f (%f degrees)", numBadTangents2, numBadTangents2 + numOkTangents2, 100.0f*(float)numBadTangents2/(float)(numBadTangents2 + numOkTangents2), maxTangentError2, RtoD*acosf(1.0f - maxTangentError2)));
			}

			if (numBadTangents3 > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad tangents (3), max err = %f (%f degrees)", numBadTangents3, numBadTangents3 + numOkTangents3, 100.0f*(float)numBadTangents3/(float)(numBadTangents3 + numOkTangents3), maxTangentError3, RtoD*acosf(1.0f - maxTangentError3)));
			}

			if (numBadOppositeTangents > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite tangents", numBadOppositeTangents, numBadOppositeTangents + numOkOppositeTangents, 100.0f*(float)numBadOppositeTangents/(float)(numBadOppositeTangents + numOkOppositeTangents)));
			}

			if (numBadOppositeTangents2a > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite tangents (2a)", numBadOppositeTangents2a, numBadOppositeTangents2a + numOkOppositeTangents2a, 100.0f*(float)numBadOppositeTangents2a/(float)(numBadOppositeTangents2a + numOkOppositeTangents2a)));
			}

			if (numBadOppositeTangents2b > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite tangents (2b)", numBadOppositeTangents2b, numBadOppositeTangents2b + numOkOppositeTangents2b, 100.0f*(float)numBadOppositeTangents2b/(float)(numBadOppositeTangents2b + numOkOppositeTangents2b)));
			}

			if (numBadOppositeTangents2c > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite tangents (2c)", numBadOppositeTangents2c, numBadOppositeTangents2c + numOkOppositeTangents2c, 100.0f*(float)numBadOppositeTangents2c/(float)(numBadOppositeTangents2c + numOkOppositeTangents2c)));
			}

			if (numBadOppositeTexcoord > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite texcoord", numBadOppositeTexcoord, numBadOppositeTexcoord + numOkOppositeTexcoord, 100.0f*(float)numBadOppositeTexcoord/(float)(numBadOppositeTexcoord + numOkOppositeTexcoord)));
			}

			if (numBadOppositeTexcoord2a > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite texcoord (2a)", numBadOppositeTexcoord2a, numBadOppositeTexcoord2a + numOkOppositeTexcoord2a, 100.0f*(float)numBadOppositeTexcoord2a/(float)(numBadOppositeTexcoord2a + numOkOppositeTexcoord2a)));
			}

			if (numBadOppositeTexcoord2b > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite texcoord (2b)", numBadOppositeTexcoord2b, numBadOppositeTexcoord2b + numOkOppositeTexcoord2b, 100.0f*(float)numBadOppositeTexcoord2b/(float)(numBadOppositeTexcoord2b + numOkOppositeTexcoord2b)));
			}

			if (numBadOppositeTexcoord2c > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad opposite texcoord (2c)", numBadOppositeTexcoord2c, numBadOppositeTexcoord2c + numOkOppositeTexcoord2c, 100.0f*(float)numBadOppositeTexcoord2c/(float)(numBadOppositeTexcoord2c + numOkOppositeTexcoord2c)));
			}

			if (numCableBadEndpoints > 0)
			{
				errors.PushAndGrow(atVarString("%d of %d (%.2f%%) bad endpoints", numCableBadEndpoints, numCableBadEndpoints + numCableOkEndpoints, 100.0f*(float)numCableBadEndpoints/(float)(numCableBadEndpoints + numCableOkEndpoints)));
			}

			if (numBadTopology > 0)
			{
				errors.PushAndGrow(atVarString("%d bad topology", numBadTopology));
			}
		}

		if (numDegenerateTriangles > 0)
		{
			errors.PushAndGrow(atVarString("%d degenerate triangles", numDegenerateTriangles));
		}

		if (numNonStripTriangles > 0)
		{
			errors.PushAndGrow(atVarString("%d non-strip triangles", numNonStripTriangles));
		}

		if (numRedundantVertices > 0)
		{
			errors.PushAndGrow(atVarString("%d of %d (%.2f%%) redundant vertices", numRedundantVertices, numRedundantVertices + numOkVertices, 100.0f*(float)numRedundantVertices/(float)(numRedundantVertices + numOkVertices)));
		}

		if (numTriangleErrorsNonEndpoint > 0)
		{
			errors.PushAndGrow(atVarString("%d non-endpoint triangle errors", numTriangleErrorsNonEndpoint));
		}

		if (bSimple)
		{
			if (errors.GetCount() > 0)
			{
				Displayf("%s: cable geometry has %d errors", path, errors.GetCount());
				return false;
			}
			else
			{
				Displayf("%s: cable geometry ok", path);
				return true;
			}
		}
		else if (errors.GetCount() > 0)
		{
			/*
			// ================================================
			// sc1_rd_wire_04: pos=[467.109, -1756.44, 35.0768]
			// sc1_rd_wire_03: pos=[-2.49, -1801.98, 35.5121]
			// ================================================

			sc1_rd_cablemesh13074_thvy: cable geometry has errors (max angle = 2.366868 degrees)
			  8 of 32 (25.00%) bad opposite tangents (2c)
			  2 of 32 (6.25%) bad opposite texcoord (2c)
			sc1_rd_cablemesh13006_thvy: cable geometry ok (2 endpoints, max angle = 2.564374 degrees, max tangent error = 4.573246/2.756692 degrees)
			sc1_rd_cable_tram_00h: cable geometry ok (16 endpoints, max angle = 4.465281 degrees, max tangent error = 4.834199/4.479383 degrees)
			sc1_rd_cable_tram_00f: cable geometry has errors (max angle = 11.207901 degrees)
			  4 of 68 (5.88%) bad tangents, max err = 0.007667 (7.099454 degrees)
			  8 of 68 (11.76%) bad opposite tangents (2c)
			sc1_rd_cablemesh13729_thvy: cable geometry has errors (max angle = 10.817143 degrees)
			  2 of 50 (4.00%) bad tangents, max err = 0.007608 (7.072074 degrees)
			  2 of 50 (4.00%) bad opposite tangents (2c)
			sc1_rd_tram_ufib: cable geometry has errors (max angle = 4.826859 degrees)
			  18 of 74 (24.32%) bad opposite tangents (2c)
			  14 of 74 (18.92%) bad opposite texcoord (2c)
			sc1_rd_wire_01: cable geometry ok (116 endpoints, max angle = 6.084408 degrees, max tangent error = 5.162322/5.443594 degrees)
			sc1_rd_wire_00: cable geometry ok (152 endpoints, max angle = 14.956794 degrees, max tangent error = 5.295019/5.354304 degrees)
			sc1_rd_wire_11: cable geometry ok (84 endpoints, max angle = 18.725433 degrees, max tangent error = 5.144452/5.417793 degrees)
			sc1_rd_wire_04: cable geometry has errors (max angle = 127.776550 degrees)
			  2 of 842 (0.24%) bad tangents, max err = 0.964317 (87.955063 degrees)
			  2 of 842 (0.24%) bad opposite tangents (2c)
			  2 of 842 (0.24%) bad opposite texcoord (2c)
			sc1_rd_wire_03: cable geometry has errors (max angle = 176.494232 degrees)
			  10 of 908 (1.10%) bad tangents, max err = 0.947288 (86.978401 degrees)
			  18 of 908 (1.98%) bad opposite tangents (2c)
			  18 of 908 (1.98%) bad opposite texcoord (2c)
			  4 bad topology
			sc1_rd_wire_021: cable geometry ok (342 endpoints, max angle = 9.891592 degrees, max tangent error = 5.305260/5.200251 degrees)
			sc1_rd_wire_02: cable geometry ok (32 endpoints, max angle = 15.723387 degrees, max tangent error = 5.124455/4.863773 degrees)
			sc1_rd_wire_10: cable geometry has errors (max angle = 8.510639 degrees)
			  4 of 134 (2.99%) bad tangents, max err = 0.006560 (6.566382 degrees)
			  30 of 134 (22.39%) bad opposite tangents (2c)
			  18 of 134 (13.43%) bad opposite texcoord (2c)
			sc1_rd_wire_09: cable geometry ok (12 endpoints, max angle = 1.487142 degrees, max tangent error = 5.001631/4.408022 degrees)
			sc1_rd_wire_07: cable geometry has errors (max angle = 10.659848 degrees)
			  7 of 120 (5.83%) bad tangents, max err = 0.009612 (7.950483 degrees)
			  28 of 120 (23.33%) bad opposite tangents (2c)
			sc1_rd_wire_05: cable geometry ok (278 endpoints, max angle = 13.337345 degrees, max tangent error = 5.475682/5.124111 degrees)
			sc1_rd_wire_18: cable geometry has errors (max angle = 65.919594 degrees)
			  6 of 806 (0.74%) bad tangents, max err = 0.173674 (34.276814 degrees)
			  6 of 806 (0.74%) bad opposite tangents (2c)
			  2 of 806 (0.25%) bad opposite texcoord (2c)
			sc1_rd_wire_16: cable geometry ok (40 endpoints, max angle = 9.339719 degrees, max tangent error = 4.865705/5.077827 degrees)
			sc1_rd_wire_15: cable geometry ok (52 endpoints, max angle = 14.273819 degrees, max tangent error = 5.242356/5.129385 degrees)
			sc1_rd_wire_13: cable geometry ok (26 endpoints, max angle = 17.328814 degrees, max tangent error = 5.661781/5.334395 degrees)
			sc1_rd_wire_12: cable geometry ok (80 endpoints, max angle = 13.752825 degrees, max tangent error = 5.164332/5.051909 degrees)
			sc1_rd_wire_22: cable geometry has errors (max angle = 49.621712 degrees)
			  86 of 366 (23.50%) bad tangents, max err = 0.143807 (31.108215 degrees)
			  15 of 66 (22.73%) bad tangents (2), max err = 0.006673 (6.622556 degrees)
			  15 of 66 (22.73%) bad tangents (3), max err = 0.006673 (6.622556 degrees)
			  6 of 366 (1.64%) bad opposite tangents (2c)
			  792 of 1296 (61.11%) redundant vertices
			  36 non-endpoint triangle errors
			sc1_rd_wire_20: cable geometry has errors (max angle = 151.511673 degrees)
			  15 of 134 (11.19%) bad tangents, max err = 0.956590 (87.511986 degrees)
			  34 of 134 (25.37%) bad opposite tangents (2c)
			  6 of 134 (4.48%) bad opposite texcoord (2c)
			sc1_rd_wire_17: cable geometry ok (62 endpoints, max angle = 20.841707 degrees, max tangent error = 5.357598/5.254639 degrees)
			sc1_rd_wire_19: cable geometry ok (36 endpoints, max angle = 6.858496 degrees, max tangent error = 5.277746/4.798323 degrees)

			dt1_00_telegraph_cables_02: cable geometry has errors (max angle = 3.024203 degrees)
			  18 of 110 (16.36%) bad tangents, max err = 0.006525 (6.549067 degrees)
			  4 of 22 (18.18%) bad tangents (2), max err = 0.006367 (6.468872 degrees)
			  4 of 22 (18.18%) bad tangents (3), max err = 0.006367 (6.468872 degrees)
			  242 of 396 (61.11%) redundant vertices
			  6 non-endpoint triangle errors
			dt1_00_telegraph_cables_01: cable geometry has errors (max angle = 3.175272 degrees)
			  26 of 170 (15.29%) bad tangents, max err = 0.006776 (6.673530 degrees)
			  3 of 34 (8.82%) bad tangents (2), max err = 0.007135 (6.848251 degrees)
			  3 of 34 (8.82%) bad tangents (3), max err = 0.007135 (6.848251 degrees)
			  374 of 612 (61.11%) redundant vertices
			  18 non-endpoint triangle errors
			dt1_00_tramlines_01: cable geometry has errors (max angle = 23.803526 degrees)
			  24 of 226 (10.62%) bad tangents, max err = 0.026832 (13.302615 degrees)
			  1 of 14 (7.14%) bad tangents (2), max err = 0.005065 (5.768980 degrees)
			  1 of 14 (7.14%) bad tangents (3), max err = 0.005065 (5.768980 degrees)
			  26 of 226 (11.50%) bad opposite tangents (2c)
			  436 of 720 (60.56%) redundant vertices
			dt1_15_newwires01: cable geometry ok (14 endpoints, max angle = 4.332461 degrees, max tangent error = 4.846099/4.717544 degrees)
			dt1_15_newwires02: cable geometry has errors (max angle = 172.260468 degrees)
			  42 of 172 (24.42%) bad tangents, max err = 0.979210 (88.808701 degrees)
			  8 of 32 (25.00%) bad tangents (2), max err = 0.361877 (50.348019 degrees)
			  8 of 32 (25.00%) bad tangents (3), max err = 0.361877 (50.348019 degrees)
			  2 of 172 (1.16%) bad opposite tangents (2c)
			  2 of 172 (1.16%) bad opposite texcoord (2c)
			  24 non-endpoint triangle errors
			dt1_rd1_cablemesh118326_thvy: cable geometry has errors (max angle = 26.008318 degrees)
			  59 of 514 (11.48%) bad tangents, max err = 0.022180 (12.089919 degrees)
			  15 of 66 (22.73%) bad tangents (2), max err = 0.020230 (11.544271 degrees)
			  15 of 66 (22.73%) bad tangents (3), max err = 0.020230 (11.544271 degrees)
			  24 of 514 (4.67%) bad opposite tangents (2c)
			  30 non-endpoint triangle errors
			dt1_rd1_cablemesh117237_thvy: cable geometry has errors (max angle = 8.668321 degrees)
			  6 of 290 (2.07%) bad tangents, max err = 0.011566 (8.722716 degrees)
			  17 of 58 (29.31%) bad tangents (2), max err = 0.023800 (12.525518 degrees)
			  17 of 58 (29.31%) bad tangents (3), max err = 0.023800 (12.525518 degrees)
			dt1_rd1_cablemesh116158_thvy: cable geometry has errors (max angle = 3.538229 degrees)
			  9 of 60 (15.00%) bad tangents (2), max err = 0.006659 (6.615891 degrees)
			  9 of 60 (15.00%) bad tangents (3), max err = 0.006659 (6.615891 degrees)
			dt1_rd1_cablemesh115623_thvy: cable geometry has errors (max angle = 8.072461 degrees)
			  9 of 399 (2.26%) bad triangle winding
			  3 non-endpoint triangle errors
			dt1_rd1_cablemeshdt1_22_thvy: cable geometry ok (38 endpoints, max angle = 8.836246 degrees, max tangent error = 4.870454/5.694125 degrees)
			dt1_rd1_cablemesh115078_thvy: cable geometry has errors (max angle = 5.497898 degrees)
			  12 of 480 (2.50%) bad tangents, max err = 0.006645 (6.608743 degrees)
			  21 of 96 (21.88%) bad tangents (2), max err = 0.011266 (8.608386 degrees)
			  21 of 96 (21.88%) bad tangents (3), max err = 0.011266 (8.608386 degrees)
			dt1_rd1_cablemesh116749_thvy: cable geometry has errors (max angle = 8.523068 degrees)
			  14 of 330 (4.24%) bad tangents, max err = 0.006413 (6.492080 degrees)
			  10 of 66 (15.15%) bad tangents (2), max err = 0.013357 (9.374985 degrees)
			  10 of 66 (15.15%) bad tangents (3), max err = 0.013357 (9.374985 degrees)
			dt1_rd1_wire246: cable geometry ok (58 endpoints, max angle = 5.324873 degrees, max tangent error = 4.922945/4.800324 degrees)
			dt1_rd1_wire245: cable geometry ok (70 endpoints, max angle = 8.434077 degrees, max tangent error = 5.102423/5.010241 degrees)
			dt1_rd1_wire244: cable geometry has errors (max angle = 13.581218 degrees)
			  4 of 342 (1.17%) bad tangents, max err = 0.023548 (12.458546 degrees)
			  32 of 342 (9.36%) bad opposite tangents (2c)

			kt1_rd_kt1_tel_007: cable geometry ok (72 endpoints, max angle = 9.896502 degrees, max tangent error = 5.052646/4.936339 degrees)
			kt1_rd_kt1_tram_top: cable geometry has errors (max angle = 32.076836 degrees)
			  50 of 344 (14.53%) bad tangents, max err = 0.059346 (19.838274 degrees)
			  54 of 344 (15.70%) bad opposite tangents (2c)
			kt1_rd_kt1_tel_008: cable geometry ok (34 endpoints, max angle = 20.837988 degrees, max tangent error = 5.549625/5.210301 degrees)
			kt1_rd_tram_cable_bot: cable geometry has errors (max angle = 16.746725 degrees)
			  12 of 170 (7.06%) bad tangents, max err = 0.011629 (8.746351 degrees)
			  1 of 4 (25.00%) bad tangents (2), max err = 0.017430 (10.713054 degrees)
			  1 of 4 (25.00%) bad tangents (3), max err = 0.017430 (10.713054 degrees)
			  52 of 170 (30.59%) bad opposite tangents (2c)
			  4 of 170 (2.35%) bad opposite texcoord (2c)

			cs4_rd_props_cs4_el_st01: cable geometry has errors (max angle = 10.857084 degrees)
			  25 of 292 (8.56%) bad tangents, max err = 0.009428 (7.873747 degrees)
			  2 of 28 (7.14%) bad tangents (2), max err = 0.005200 (5.845496 degrees)
			  2 of 28 (7.14%) bad tangents (3), max err = 0.005200 (5.845496 degrees)
			  4 of 292 (1.37%) bad opposite tangents (2c)
			  608 of 960 (63.33%) redundant vertices
			  18 non-endpoint triangle errors
			cs4_rd_props_cs4_el_st02: cable geometry has errors (max angle = 14.253195 degrees)
			  98 of 298 (32.89%) bad tangents, max err = 0.011674 (8.763179 degrees)
			  6 of 22 (27.27%) bad tangents (2), max err = 0.005758 (6.151731 degrees)
			  6 of 22 (27.27%) bad tangents (3), max err = 0.005758 (6.151731 degrees)
			  10 of 298 (3.36%) bad opposite tangents (2c)
			  608 of 960 (63.33%) redundant vertices
			  66 non-endpoint triangle errors
			cs4_rd_props_cs4_wire102: cable geometry has errors (max angle = 10.408586 degrees)
			  35 of 146 (23.97%) bad tangents, max err = 0.012838 (9.190605 degrees)
			  3 of 14 (21.43%) bad tangents (2), max err = 0.005990 (6.274199 degrees)
			  3 of 14 (21.43%) bad tangents (3), max err = 0.005990 (6.274199 degrees)
			  2 of 146 (1.37%) bad opposite tangents (2c)
			  2 of 146 (1.37%) bad opposite texcoord (2c)
			  304 of 480 (63.33%) redundant vertices
			  32 non-endpoint triangle errors
			cs4_rd_props_cs4_rd_w_564: cable geometry has errors (max angle = 149.146210 degrees)
			  22 of 146 (15.07%) bad tangents, max err = 0.741488 (75.018219 degrees)
			  6 of 14 (42.86%) bad tangents (2), max err = 0.006613 (6.592879 degrees)
			  6 of 14 (42.86%) bad tangents (3), max err = 0.006613 (6.592879 degrees)
			  2 of 146 (1.37%) bad opposite tangents (2c)
			  304 of 480 (63.33%) redundant vertices
			  14 non-endpoint triangle errors
			cs4_rd_props_cs4_rd_w_563: cable geometry has errors (max angle = 4.047866 degrees)
			  42 of 90 (46.67%) bad tangents, max err = 0.006938 (6.753049 degrees)
			  3 of 10 (30.00%) bad tangents (2), max err = 0.005478 (5.999857 degrees)
			  3 of 10 (30.00%) bad tangents (3), max err = 0.005478 (5.999857 degrees)
			  190 of 300 (63.33%) redundant vertices
			  32 non-endpoint triangle errors
			cs4_rd_props_cs4_wires03: cable geometry has errors (max angle = 180.000000 degrees)
			  66 of 218 (30.28%) bad tangents, max err = 0.943359 (86.752991 degrees)
			  10 of 19 (52.63%) bad tangents (2), max err = 0.006147 (6.356101 degrees)
			  10 of 19 (52.63%) bad tangents (3), max err = 0.006147 (6.356101 degrees)
			  32 of 240 (13.33%) bad opposite tangents (2a)
			  32 of 240 (13.33%) bad opposite tangents (2b)
			  36 of 218 (16.51%) bad opposite tangents (2c)
			  32 of 240 (13.33%) bad opposite texcoord (2a)
			  32 of 240 (13.33%) bad opposite texcoord (2b)
			  32 of 218 (14.68%) bad opposite texcoord (2c)
			  29 bad topology
			  390 of 624 (62.50%) redundant vertices
			  54 non-endpoint triangle errors
			cs4_rd_props_cs4_wires02: cable geometry has errors (max angle = 166.176300 degrees)
			  249 of 642 (38.79%) bad tangents, max err = 0.874368 (82.782745 degrees)
			  41 of 78 (52.56%) bad tangents (2), max err = 0.008456 (7.456510 degrees)
			  41 of 78 (52.56%) bad tangents (3), max err = 0.008456 (7.456510 degrees)
			  12 of 642 (1.87%) bad opposite tangents (2c)
			  4 of 642 (0.62%) bad opposite texcoord (2c)
			  1350 of 2160 (62.50%) redundant vertices
			  160 non-endpoint triangle errors
			cs4_rd_props_cs4_wires01: cable geometry has errors (max angle = 0.940660 degrees)
			  36 of 108 (33.33%) bad tangents, max err = 0.006705 (6.638528 degrees)
			  3 of 12 (25.00%) bad tangents (2), max err = 0.006322 (6.446219 degrees)
			  3 of 12 (25.00%) bad tangents (3), max err = 0.006322 (6.446219 degrees)
			  228 of 360 (63.33%) redundant vertices
			  30 non-endpoint triangle errors
			cs4_rd_props_cs4_wires14: cable geometry has errors (max angle = 13.687166 degrees)
			  124 of 352 (35.23%) bad tangents, max err = 0.014206 (9.669325 degrees)
			  21 of 48 (43.75%) bad tangents (2), max err = 0.007186 (6.873123 degrees)
			  21 of 48 (43.75%) bad tangents (3), max err = 0.007186 (6.873123 degrees)
			  2 of 352 (0.57%) bad opposite tangents (2c)
			  750 of 1200 (62.50%) redundant vertices
			  82 non-endpoint triangle errors
			cs4_rd_props_cs4_wires10: cable geometry has errors (max angle = 111.810760 degrees)
			  162 of 478 (33.89%) bad tangents, max err = 0.647214 (69.342163 degrees)
			  24 of 66 (36.36%) bad tangents (2), max err = 0.007687 (7.108816 degrees)
			  24 of 66 (36.36%) bad tangents (3), max err = 0.007687 (7.108816 degrees)
			  2 of 478 (0.42%) bad opposite tangents (2c)
			  2 of 478 (0.42%) bad opposite texcoord (2c)
			  1020 of 1632 (62.50%) redundant vertices
			  100 non-endpoint triangle errors
			cs4_rd_props_cs4_wires08: cable geometry has errors (max angle = 100.035751 degrees)
			  56 of 166 (33.73%) bad tangents, max err = 0.386930 (52.188175 degrees)
			  5 of 13 (38.46%) bad tangents (2), max err = 0.007560 (7.049821 degrees)
			  5 of 13 (38.46%) bad tangents (3), max err = 0.007560 (7.049821 degrees)
			  12 of 166 (7.23%) bad opposite tangents (2c)
			  2 of 166 (1.20%) bad opposite texcoord (2c)
			  3 bad topology
			  330 of 528 (62.50%) redundant vertices
			  28 non-endpoint triangle errors
			cs4_rd_props_cs4_wire150: cable geometry has errors (max angle = 2.129184 degrees)
			  54 of 198 (27.27%) bad tangents, max err = 0.006851 (6.710660 degrees)
			  6 of 22 (27.27%) bad tangents (2), max err = 0.005924 (6.239762 degrees)
			  6 of 22 (27.27%) bad tangents (3), max err = 0.005924 (6.239762 degrees)
			  418 of 660 (63.33%) redundant vertices
			  50 non-endpoint triangle errors
			cs4_rd_props_cs4_wires17: cable geometry has errors (max angle = 10.944221 degrees)
			  38 of 208 (18.27%) bad tangents, max err = 0.009473 (7.892821 degrees)
			  3 of 16 (18.75%) bad tangents (2), max err = 0.005528 (6.027078 degrees)
			  3 of 16 (18.75%) bad tangents (3), max err = 0.005528 (6.027078 degrees)
			  12 of 208 (5.77%) bad opposite tangents (2c)
			  420 of 672 (62.50%) redundant vertices
			  10 non-endpoint triangle errors
			cs4_rd_props_cs4_wires16a: cable geometry has errors (max angle = 134.205093 degrees)
			  60 of 116 (51.72%) bad tangents, max err = 0.733590 (74.549255 degrees)
			  6 of 12 (50.00%) bad tangents (2), max err = 0.007609 (7.072463 degrees)
			  6 of 12 (50.00%) bad tangents (3), max err = 0.007609 (7.072463 degrees)
			  4 of 116 (3.45%) bad opposite tangents (2c)
			  4 of 116 (3.45%) bad opposite texcoord (2c)
			  240 of 384 (62.50%) redundant vertices
			  38 non-endpoint triangle errors
			cs4_rd_props_cs4_wires16: cable geometry has errors (max angle = 96.785545 degrees)
			  246 of 610 (40.33%) bad tangents, max err = 0.533098 (62.166611 degrees)
			  32 of 78 (41.03%) bad tangents (2), max err = 0.008028 (7.264876 degrees)
			  32 of 78 (41.03%) bad tangents (3), max err = 0.008028 (7.264876 degrees)
			  8 of 610 (1.31%) bad opposite tangents (2c)
			  1290 of 2064 (62.50%) redundant vertices
			  168 non-endpoint triangle errors
			cs4_rd_props_cs4_wires15: cable geometry has errors (max angle = 142.319107 degrees)
			  212 of 514 (41.25%) bad tangents, max err = 0.952792 (87.294159 degrees)
			  31 of 62 (50.00%) bad tangents (2), max err = 0.007644 (7.088780 degrees)
			  31 of 62 (50.00%) bad tangents (3), max err = 0.007644 (7.088780 degrees)
			  10 of 514 (1.95%) bad opposite tangents (2c)
			  4 of 514 (0.78%) bad opposite texcoord (2c)
			  1080 of 1728 (62.50%) redundant vertices
			  142 non-endpoint triangle errors
			cs4_rd_props_cs4_wires23: cable geometry has errors (max angle = 10.946756 degrees)
			  62 of 200 (31.00%) bad tangents, max err = 0.009241 (7.795155 degrees)
			  10 of 24 (41.67%) bad tangents (2), max err = 0.005958 (6.257771 degrees)
			  10 of 24 (41.67%) bad tangents (3), max err = 0.005958 (6.257771 degrees)
			  4 of 200 (2.00%) bad opposite tangents (2c)
			  420 of 672 (62.50%) redundant vertices
			  44 non-endpoint triangle errors
			cs4_rd_props_cs4_wires22: cable geometry has errors (max angle = 11.511169 degrees)
			  40 of 228 (17.54%) bad tangents, max err = 0.008542 (7.494152 degrees)
			  9 of 28 (32.14%) bad tangents (2), max err = 0.005680 (6.109680 degrees)
			  9 of 28 (32.14%) bad tangents (3), max err = 0.005680 (6.109680 degrees)
			  4 of 228 (1.75%) bad opposite tangents (2c)
			  480 of 768 (62.50%) redundant vertices
			  20 non-endpoint triangle errors
			cs4_rd_props_cs4_wires21: cable geometry has errors (max angle = 3.760829 degrees)
			  32 of 196 (16.33%) bad tangents, max err = 0.005555 (6.041923 degrees)
			  3 of 28 (10.71%) bad tangents (2), max err = 0.005677 (6.108268 degrees)
			  3 of 28 (10.71%) bad tangents (3), max err = 0.005677 (6.108268 degrees)
			  420 of 672 (62.50%) redundant vertices
			  20 non-endpoint triangle errors
			cs4_rd_props_cs4_wires18: cable geometry has errors (max angle = 5.428020 degrees)
			  32 of 114 (28.07%) bad tangents, max err = 0.007411 (6.980024 degrees)
			  5 of 14 (35.71%) bad tangents (2), max err = 0.005849 (6.200205 degrees)
			  5 of 14 (35.71%) bad tangents (3), max err = 0.005849 (6.200205 degrees)
			  2 of 114 (1.75%) bad opposite tangents (2c)
			  2 of 114 (1.75%) bad opposite texcoord (2c)
			  240 of 384 (62.50%) redundant vertices
			  20 non-endpoint triangle errors
			cs4_rd_props_cs4_wires27: cable geometry has errors (max angle = 170.120590 degrees)
			  36 of 186 (19.35%) bad tangents, max err = 0.901438 (84.343620 degrees)
			  1 of 22 (4.55%) bad tangents (2), max err = 0.006085 (6.324093 degrees)
			  1 of 22 (4.55%) bad tangents (3), max err = 0.006085 (6.324093 degrees)
			  4 of 186 (2.15%) bad opposite tangents (2c)
			  2 of 186 (1.08%) bad opposite texcoord (2c)
			  390 of 624 (62.50%) redundant vertices
			  26 non-endpoint triangle errors
			cs4_rd_props_cs4_wires26: cable geometry has errors (max angle = 27.413601 degrees)
			  48 of 214 (22.43%) bad tangents, max err = 0.034330 (15.056559 degrees)
			  8 of 26 (30.77%) bad tangents (2), max err = 0.006648 (6.610167 degrees)
			  8 of 26 (30.77%) bad tangents (3), max err = 0.006648 (6.610167 degrees)
			  4 of 214 (1.87%) bad opposite tangents (2c)
			  450 of 720 (62.50%) redundant vertices
			  28 non-endpoint triangle errors
			cs4_rd_props_cs4_wires25: cable geometry has errors (max angle = 15.277818 degrees)
			  36 of 126 (28.57%) bad tangents, max err = 0.006994 (6.780527 degrees)
			  4 of 18 (22.22%) bad tangents (2), max err = 0.006271 (6.420097 degrees)
			  4 of 18 (22.22%) bad tangents (3), max err = 0.006271 (6.420097 degrees)
			  270 of 432 (62.50%) redundant vertices
			  24 non-endpoint triangle errors
			cs4_rd_props_cs4_wires24: cable geometry has errors (max angle = 1.897130 degrees)
			  12 of 42 (28.57%) bad tangents, max err = 0.006554 (6.563216 degrees)
			  3 of 6 (50.00%) bad tangents (2), max err = 0.006093 (6.328339 degrees)
			  3 of 6 (50.00%) bad tangents (3), max err = 0.006093 (6.328339 degrees)
			  90 of 144 (62.50%) redundant vertices
			  6 non-endpoint triangle errors
			cs4_rd_props_cs4_wires35: cable geometry has errors (max angle = 4.866954 degrees)
			  18 of 84 (21.43%) bad tangents, max err = 0.005878 (6.215206 degrees)
			  1 of 12 (8.33%) bad tangents (2), max err = 0.006777 (6.674236 degrees)
			  1 of 12 (8.33%) bad tangents (3), max err = 0.006777 (6.674236 degrees)
			  180 of 288 (62.50%) redundant vertices
			  14 non-endpoint triangle errors
			cs4_rd_props_cs4_wires34: cable geometry has errors (max angle = 1.727937 degrees)
			  16 of 56 (28.57%) bad tangents, max err = 0.006815 (6.693074 degrees)
			  3 of 8 (37.50%) bad tangents (2), max err = 0.006577 (6.575127 degrees)
			  3 of 8 (37.50%) bad tangents (3), max err = 0.006577 (6.575127 degrees)
			  120 of 192 (62.50%) redundant vertices
			  8 non-endpoint triangle errors
			cs4_rd_props_cs4_wires32: cable geometry has errors (max angle = 2.274602 degrees)
			  16 of 84 (19.05%) bad tangents, max err = 0.006218 (6.392613 degrees)
			  2 of 12 (16.67%) bad tangents (2), max err = 0.006980 (6.773755 degrees)
			  2 of 12 (16.67%) bad tangents (3), max err = 0.006980 (6.773755 degrees)
			  180 of 288 (62.50%) redundant vertices
			  14 non-endpoint triangle errors
			cs4_rd_props_cs4_wires28: cable geometry has errors (max angle = 1.110299 degrees)
			  8 of 168 (4.76%) bad tangents, max err = 0.005179 (5.833579 degrees)
			  3 of 24 (12.50%) bad tangents (2), max err = 0.005247 (5.871826 degrees)
			  3 of 24 (12.50%) bad tangents (3), max err = 0.005247 (5.871826 degrees)
			  360 of 576 (62.50%) redundant vertices
			  8 non-endpoint triangle errors
			cs4_rd_props_cs4_wires40: cable geometry has errors (max angle = 1.377998 degrees)
			  50 of 196 (25.51%) bad tangents, max err = 0.006719 (6.645703 degrees)
			  12 of 28 (42.86%) bad tangents (2), max err = 0.006287 (6.428339 degrees)
			  12 of 28 (42.86%) bad tangents (3), max err = 0.006287 (6.428339 degrees)
			  420 of 672 (62.50%) redundant vertices
			  34 non-endpoint triangle errors
			cs4_rd_props_cs4_wires38: cable geometry has errors (max angle = 8.379534 degrees)
			  48 of 170 (28.24%) bad tangents, max err = 0.008682 (7.555487 degrees)
			  5 of 22 (22.73%) bad tangents (2), max err = 0.005413 (5.963942 degrees)
			  5 of 22 (22.73%) bad tangents (3), max err = 0.005413 (5.963942 degrees)
			  2 of 170 (1.18%) bad opposite tangents (2c)
			  360 of 576 (62.50%) redundant vertices
			  32 non-endpoint triangle errors
			cs4_rd_props_cs4_wires37: cable geometry has errors (max angle = 172.434250 degrees)
			  116 of 284 (40.85%) bad tangents, max err = 0.595769 (66.157066 degrees)
			  12 of 36 (33.33%) bad tangents (2), max err = 0.007583 (7.060470 degrees)
			  12 of 36 (33.33%) bad tangents (3), max err = 0.007583 (7.060470 degrees)
			  4 of 284 (1.41%) bad opposite tangents (2c)
			  2 of 284 (0.70%) bad opposite texcoord (2c)
			  600 of 960 (62.50%) redundant vertices
			  76 non-endpoint triangle errors
			cs4_rd_props_cs4_wires36: cable geometry has errors (max angle = 11.749938 degrees)
			  30 of 112 (26.79%) bad tangents, max err = 0.011619 (8.742557 degrees)
			  1 of 8 (12.50%) bad tangents (2), max err = 0.005271 (5.885231 degrees)
			  1 of 8 (12.50%) bad tangents (3), max err = 0.005271 (5.885231 degrees)
			  4 of 112 (3.57%) bad opposite tangents (2c)
			  228 of 360 (63.33%) redundant vertices
			  22 non-endpoint triangle errors
			cs4_rd_props_cs4_wires47: cable geometry has errors (max angle = 1.367735 degrees)
			  6 of 18 (33.33%) bad tangents, max err = 0.006056 (6.309007 degrees)
			  1 of 2 (50.00%) bad tangents (2), max err = 0.005182 (5.835629 degrees)
			  1 of 2 (50.00%) bad tangents (3), max err = 0.005182 (5.835629 degrees)
			  38 of 60 (63.33%) redundant vertices
			  2 non-endpoint triangle errors
			cs4_rd_props_cs4_wires45: cable geometry has errors (max angle = 12.218386 degrees)
			  82 of 186 (44.09%) bad tangents, max err = 0.014237 (9.679852 degrees)
			  6 of 14 (42.86%) bad tangents (2), max err = 0.005789 (6.167835 degrees)
			  6 of 14 (42.86%) bad tangents (3), max err = 0.005789 (6.167835 degrees)
			  6 of 186 (3.23%) bad opposite tangents (2c)
			  380 of 600 (63.33%) redundant vertices
			  60 non-endpoint triangle errors
			cs4_rd_props_cs4_wires44: cable geometry has errors (max angle = 8.939339 degrees)
			  72 of 200 (36.00%) bad tangents, max err = 0.010688 (8.384453 degrees)
			  7 of 20 (35.00%) bad tangents (2), max err = 0.006344 (6.457403 degrees)
			  7 of 20 (35.00%) bad tangents (3), max err = 0.006344 (6.457403 degrees)
			  2 of 200 (1.00%) bad opposite tangents (2c)
			  418 of 660 (63.33%) redundant vertices
			  54 non-endpoint triangle errors
			cs4_rd_props_cs4_wires43: cable geometry has errors (max angle = 45.682610 degrees)
			  100 of 256 (39.06%) bad tangents, max err = 0.209242 (37.743568 degrees)
			  21 of 32 (65.63%) bad tangents (2), max err = 0.007135 (6.848251 degrees)
			  21 of 32 (65.63%) bad tangents (3), max err = 0.007135 (6.848251 degrees)
			  4 of 256 (1.56%) bad opposite tangents (2c)
			  540 of 864 (62.50%) redundant vertices
			  68 non-endpoint triangle errors
			cs4_rd_props_cs4_wires56: cable geometry has errors (max angle = 1.576443 degrees)
			  48 of 196 (24.49%) bad tangents, max err = 0.006583 (6.578138 degrees)
			  2 of 28 (7.14%) bad tangents (2), max err = 0.007070 (6.816933 degrees)
			  2 of 28 (7.14%) bad tangents (3), max err = 0.007070 (6.816933 degrees)
			  420 of 672 (62.50%) redundant vertices
			  40 non-endpoint triangle errors
			cs4_rd_props_cs4_wires54: cable geometry has errors (max angle = 12.207649 degrees)
			  60 of 126 (47.62%) bad tangents, max err = 0.006948 (6.758014 degrees)
			  8 of 14 (57.14%) bad tangents (2), max err = 0.007493 (7.018559 degrees)
			  8 of 14 (57.14%) bad tangents (3), max err = 0.007493 (7.018559 degrees)
			  266 of 420 (63.33%) redundant vertices
			  48 non-endpoint triangle errors
			cs4_rd_props_cs4_wires52: cable geometry has errors (max angle = 177.405273 degrees)
			  68 of 240 (28.33%) bad tangents, max err = 0.767578 (76.560295 degrees)
			  5 of 23 (21.74%) bad tangents (2), max err = 0.006300 (6.434894 degrees)
			  5 of 23 (21.74%) bad tangents (3), max err = 0.006300 (6.434894 degrees)
			  6 of 240 (2.50%) bad opposite tangents (2c)
			  2 of 240 (0.83%) bad opposite texcoord (2c)
			  3 bad topology
			  494 of 780 (63.33%) redundant vertices
			  46 non-endpoint triangle errors
			cs4_rd_props_cs4_wires48: cable geometry has errors (max angle = 5.416636 degrees)
			  78 of 324 (24.07%) bad tangents, max err = 0.007024 (6.794887 degrees)
			  16 of 36 (44.44%) bad tangents (2), max err = 0.006389 (6.479956 degrees)
			  16 of 36 (44.44%) bad tangents (3), max err = 0.006389 (6.479956 degrees)
			  684 of 1080 (63.33%) redundant vertices
			  54 non-endpoint triangle errors
			cs4_rd_props_cs4_wires61: cable geometry has errors (max angle = 1.209013 degrees)
			  14 of 216 (6.48%) bad tangents, max err = 0.005174 (5.831093 degrees)
			  2 of 24 (8.33%) bad tangents (2), max err = 0.005393 (5.953019 degrees)
			  2 of 24 (8.33%) bad tangents (3), max err = 0.005393 (5.953019 degrees)
			  456 of 720 (63.33%) redundant vertices
			  12 non-endpoint triangle errors
			cs4_rd_props_cs4_wires60: cable geometry has errors (max angle = 89.110992 degrees)
			  72 of 308 (23.38%) bad tangents, max err = 0.441724 (56.063377 degrees)
			  4 of 32 (12.50%) bad tangents (2), max err = 0.006163 (6.364394 degrees)
			  4 of 32 (12.50%) bad tangents (3), max err = 0.006163 (6.364394 degrees)
			  2 of 308 (0.65%) bad opposite tangents (2c)
			  2 of 308 (0.65%) bad opposite texcoord (2c)
			  646 of 1020 (63.33%) redundant vertices
			  52 non-endpoint triangle errors
			cs4_rd_props_cs4_wires58: cable geometry has errors (max angle = 0.963271 degrees)
			  54 of 180 (30.00%) bad tangents, max err = 0.006376 (6.473660 degrees)
			  1 of 20 (5.00%) bad tangents (2), max err = 0.005123 (5.802248 degrees)
			  1 of 20 (5.00%) bad tangents (3), max err = 0.005123 (5.802248 degrees)
			  380 of 600 (63.33%) redundant vertices
			  50 non-endpoint triangle errors
			cs4_rd_props_cs4_wires57: cable geometry has errors (max angle = 1.018372 degrees)
			  24 of 180 (13.33%) bad tangents, max err = 0.006054 (6.307858 degrees)
			  2 of 20 (10.00%) bad tangents (2), max err = 0.005608 (6.070990 degrees)
			  2 of 20 (10.00%) bad tangents (3), max err = 0.005608 (6.070990 degrees)
			  380 of 600 (63.33%) redundant vertices
			  18 non-endpoint triangle errors
			cs4_rd_props_cs4_wires69: cable geometry has errors (max angle = 2.504287 degrees)
			  174 of 576 (30.21%) bad tangents, max err = 0.007484 (7.014113 degrees)
			  31 of 64 (48.44%) bad tangents (2), max err = 0.006553 (6.562738 degrees)
			  31 of 64 (48.44%) bad tangents (3), max err = 0.006553 (6.562738 degrees)
			  1216 of 1920 (63.33%) redundant vertices
			  118 non-endpoint triangle errors
			cs4_rd_props_cs4_wires68: cable geometry has errors (max angle = 2.285247 degrees)
			  132 of 324 (40.74%) bad tangents, max err = 0.006635 (6.603667 degrees)
			  9 of 36 (25.00%) bad tangents (2), max err = 0.006505 (6.538880 degrees)
			  9 of 36 (25.00%) bad tangents (3), max err = 0.006505 (6.538880 degrees)
			  684 of 1080 (63.33%) redundant vertices
			  104 non-endpoint triangle errors
			cs4_rd_props_cs4_wires63: cable geometry has errors (max angle = 177.896439 degrees)
			  10 of 146 (6.85%) bad tangents, max err = 0.878762 (83.036469 degrees)
			  2 of 146 (1.37%) bad opposite tangents (2c)
			  2 of 146 (1.37%) bad opposite texcoord (2c)
			  304 of 480 (63.33%) redundant vertices
			  8 non-endpoint triangle errors
			cs4_rd_props_cs4_wires62: cable geometry has errors (max angle = 12.192542 degrees)
			  48 of 180 (26.67%) bad tangents, max err = 0.006505 (6.538940 degrees)
			  9 of 20 (45.00%) bad tangents (2), max err = 0.006130 (6.347427 degrees)
			  9 of 20 (45.00%) bad tangents (3), max err = 0.006130 (6.347427 degrees)
			  380 of 600 (63.33%) redundant vertices
			  32 non-endpoint triangle errors
			cs4_rd_props_cs4_wires79: cable geometry has errors (max angle = 16.287470 degrees)
			  158 of 336 (47.02%) bad tangents, max err = 0.007779 (7.151269 degrees)
			  18 of 48 (37.50%) bad tangents (2), max err = 0.007793 (7.157823 degrees)
			  18 of 48 (37.50%) bad tangents (3), max err = 0.007793 (7.157823 degrees)
			  720 of 1152 (62.50%) redundant vertices
			  108 non-endpoint triangle errors
			cs4_rd_props_cs4_wires77: cable geometry has errors (max angle = 7.543271 degrees)
			  54 of 140 (38.57%) bad tangents, max err = 0.006742 (6.656818 degrees)
			  7 of 20 (35.00%) bad tangents (2), max err = 0.006023 (6.291581 degrees)
			  7 of 20 (35.00%) bad tangents (3), max err = 0.006023 (6.291581 degrees)
			  300 of 480 (62.50%) redundant vertices
			  34 non-endpoint triangle errors
			cs4_rd_props_cs4_wires72: cable geometry has errors (max angle = 177.503296 degrees)
			  99 of 314 (31.53%) bad tangents, max err = 0.504884 (60.322590 degrees)
			  10 of 38 (26.32%) bad tangents (2), max err = 0.006608 (6.590350 degrees)
			  10 of 38 (26.32%) bad tangents (3), max err = 0.006608 (6.590350 degrees)
			  6 of 314 (1.91%) bad opposite tangents (2c)
			  4 of 314 (1.27%) bad opposite texcoord (2c)
			  660 of 1056 (62.50%) redundant vertices
			  60 non-endpoint triangle errors
			cs4_rd_props_cs4_wires70: cable geometry has errors (max angle = 3.078098 degrees)
			  98 of 360 (27.22%) bad tangents, max err = 0.006691 (6.631493 degrees)
			  12 of 40 (30.00%) bad tangents (2), max err = 0.006325 (6.447618 degrees)
			  12 of 40 (30.00%) bad tangents (3), max err = 0.006325 (6.447618 degrees)
			  760 of 1200 (63.33%) redundant vertices
			  72 non-endpoint triangle errors
			cs4_rd_props_cs4_wires86: cable geometry has errors (max angle = 5.542488 degrees)
			  54 of 168 (32.14%) bad tangents, max err = 0.007309 (6.931718 degrees)
			  8 of 24 (33.33%) bad tangents (2), max err = 0.006854 (6.711859 degrees)
			  8 of 24 (33.33%) bad tangents (3), max err = 0.006854 (6.711859 degrees)
			  360 of 576 (62.50%) redundant vertices
			  38 non-endpoint triangle errors
			cs4_rd_props_cs4_wires85: cable geometry has errors (max angle = 173.659805 degrees)
			  192 of 482 (39.83%) bad tangents, max err = 0.947266 (86.977142 degrees)
			  20 of 49 (40.82%) bad tangents (2), max err = 0.007337 (6.945062 degrees)
			  20 of 49 (40.82%) bad tangents (3), max err = 0.007337 (6.945062 degrees)
			  20 of 482 (4.15%) bad opposite tangents (2c)
			  12 of 482 (2.49%) bad opposite texcoord (2c)
			  3 bad topology
			  990 of 1584 (62.50%) redundant vertices
			  122 non-endpoint triangle errors
			cs4_rd_props_cs4_wires82: cable geometry has errors (max angle = 10.721556 degrees)
			  154 of 378 (40.74%) bad tangents, max err = 0.007412 (6.980502 degrees)
			  23 of 54 (42.59%) bad tangents (2), max err = 0.007010 (6.788159 degrees)
			  23 of 54 (42.59%) bad tangents (3), max err = 0.007010 (6.788159 degrees)
			  810 of 1296 (62.50%) redundant vertices
			  110 non-endpoint triangle errors
			cs4_rd_props_cs4_wires80: cable geometry has errors (max angle = 3.407202 degrees)
			  116 of 252 (46.03%) bad tangents, max err = 0.008086 (7.291268 degrees)
			  15 of 36 (41.67%) bad tangents (2), max err = 0.007376 (6.963424 degrees)
			  15 of 36 (41.67%) bad tangents (3), max err = 0.007376 (6.963424 degrees)
			  540 of 864 (62.50%) redundant vertices
			  82 non-endpoint triangle errors
			cs4_rd_props_cs4_wires_100: cable geometry has errors (max angle = 45.597897 degrees)
			  130 of 332 (39.16%) bad tangents, max err = 0.087979 (24.213785 degrees)
			  5 of 28 (17.86%) bad tangents (2), max err = 0.006686 (6.629453 degrees)
			  5 of 28 (17.86%) bad tangents (3), max err = 0.006686 (6.629453 degrees)
			  8 of 332 (2.41%) bad opposite tangents (2c)
			  684 of 1080 (63.33%) redundant vertices
			  110 non-endpoint triangle errors
			cs4_rd_props_w_spline511: cable geometry has errors (max angle = 47.425907 degrees)
			  128 of 256 (50.00%) bad tangents, max err = 0.145235 (31.266251 degrees)
			  11 of 24 (45.83%) bad tangents (2), max err = 0.006224 (6.396047 degrees)
			  11 of 24 (45.83%) bad tangents (3), max err = 0.006224 (6.396047 degrees)
			  4 of 256 (1.56%) bad opposite tangents (2c)
			  4 of 256 (1.56%) bad opposite texcoord (2c)
			  532 of 840 (63.33%) redundant vertices
			  94 non-endpoint triangle errors
			cs4_rd_props_w_spline515: cable geometry has errors (max angle = 4.136543 degrees)
			  126 of 252 (50.00%) bad tangents, max err = 0.007797 (7.159412 degrees)
			  14 of 28 (50.00%) bad tangents (2), max err = 0.007195 (6.877431 degrees)
			  14 of 28 (50.00%) bad tangents (3), max err = 0.007195 (6.877431 degrees)
			  532 of 840 (63.33%) redundant vertices
			  94 non-endpoint triangle errors
			cs4_rd_props_w_spline514: cable geometry has errors (max angle = 14.185695 degrees)
			  34 of 110 (30.91%) bad tangents, max err = 0.014470 (9.758926 degrees)
			  2 of 110 (1.82%) bad opposite tangents (2c)
			  228 of 360 (63.33%) redundant vertices
			  28 non-endpoint triangle errors
			cs4_rd_props_w_spline513: cable geometry has errors (max angle = 130.460052 degrees)
			  136 of 384 (35.42%) bad tangents, max err = 0.949004 (87.076874 degrees)
			  15 of 36 (41.67%) bad tangents (2), max err = 0.008379 (7.422089 degrees)
			  15 of 36 (41.67%) bad tangents (3), max err = 0.008379 (7.422089 degrees)
			  6 of 384 (1.56%) bad opposite tangents (2c)
			  2 of 384 (0.52%) bad opposite texcoord (2c)
			  798 of 1260 (63.33%) redundant vertices
			  100 non-endpoint triangle errors
			cs4_rd_props_w_spline512: cable geometry has errors (max angle = 112.667694 degrees)
			  344 of 908 (37.89%) bad tangents, max err = 0.850765 (81.417419 degrees)
			  40 of 95 (42.11%) bad tangents (2), max err = 0.007528 (7.034612 degrees)
			  40 of 95 (42.11%) bad tangents (3), max err = 0.007528 (7.034612 degrees)
			  8 of 908 (0.88%) bad opposite tangents (2c)
			  2 of 908 (0.22%) bad opposite texcoord (2c)
			  3 bad topology
			  1900 of 3000 (63.33%) redundant vertices
			  260 non-endpoint triangle errors
			cs4_rd_props_w_spline520: cable geometry has errors (max angle = 173.706360 degrees)
			  70 of 236 (29.66%) bad tangents, max err = 0.853101 (81.552765 degrees)
			  6 of 24 (25.00%) bad tangents (2), max err = 0.006629 (6.600637 degrees)
			  6 of 24 (25.00%) bad tangents (3), max err = 0.006629 (6.600637 degrees)
			  2 of 236 (0.85%) bad opposite tangents (2c)
			  2 of 236 (0.85%) bad opposite texcoord (2c)
			  494 of 780 (63.33%) redundant vertices
			  52 non-endpoint triangle errors
			cs4_rd_props_w_spline519: cable geometry has errors (max angle = 162.707092 degrees)
			  110 of 262 (41.98%) bad tangents, max err = 0.985468 (89.167374 degrees)
			  7 of 18 (38.89%) bad tangents (2), max err = 0.008218 (7.350363 degrees)
			  7 of 18 (38.89%) bad tangents (3), max err = 0.008218 (7.350363 degrees)
			  10 of 262 (3.82%) bad opposite tangents (2c)
			  6 of 262 (2.29%) bad opposite texcoord (2c)
			  532 of 840 (63.33%) redundant vertices
			  78 non-endpoint triangle errors
			cs4_rd_props_w_spline517: cable geometry has errors (max angle = 8.026879 degrees)
			  46 of 146 (31.51%) bad tangents, max err = 0.007437 (6.991957 degrees)
			  3 of 14 (21.43%) bad tangents (2), max err = 0.005740 (6.142067 degrees)
			  3 of 14 (21.43%) bad tangents (3), max err = 0.005740 (6.142067 degrees)
			  2 of 146 (1.37%) bad opposite tangents (2c)
			  304 of 480 (63.33%) redundant vertices
			  34 non-endpoint triangle errors
			cs4_rd_props_w_spline516: cable geometry has errors (max angle = 177.038055 degrees)
			  22 of 316 (6.96%) bad tangents, max err = 0.987472 (89.282166 degrees)
			  5 of 27 (18.52%) bad tangents (2), max err = 0.006177 (6.371629 degrees)
			  5 of 27 (18.52%) bad tangents (3), max err = 0.006177 (6.371629 degrees)
			  10 of 316 (3.16%) bad opposite tangents (2c)
			  2 of 316 (0.63%) bad opposite texcoord (2c)
			  3 bad topology
			  646 of 1020 (63.33%) redundant vertices
			  8 non-endpoint triangle errors
			cs4_rd_props_w_spline_480: cable geometry has errors (max angle = 89.125793 degrees)
			  34 of 148 (22.97%) bad tangents, max err = 0.441419 (56.042267 degrees)
			  1 of 12 (8.33%) bad tangents (2), max err = 0.006178 (6.372245 degrees)
			  1 of 12 (8.33%) bad tangents (3), max err = 0.006178 (6.372245 degrees)
			  4 of 148 (2.70%) bad opposite tangents (2c)
			  2 of 148 (1.35%) bad opposite texcoord (2c)
			  304 of 480 (63.33%) redundant vertices
			  26 non-endpoint triangle errors
			cs4_rd_props_w_spline523: cable geometry has errors (max angle = 73.911469 degrees)
			  42 of 148 (28.38%) bad tangents, max err = 0.337287 (48.492878 degrees)
			  3 of 12 (25.00%) bad tangents (2), max err = 0.005779 (6.162907 degrees)
			  3 of 12 (25.00%) bad tangents (3), max err = 0.005779 (6.162907 degrees)
			  4 of 148 (2.70%) bad opposite tangents (2c)
			  2 of 148 (1.35%) bad opposite texcoord (2c)
			  304 of 480 (63.33%) redundant vertices
			  28 non-endpoint triangle errors
			cs4_rd_props_w_spline522: cable geometry has errors (max angle = 13.381625 degrees)
			  76 of 180 (42.22%) bad tangents, max err = 0.006890 (6.729601 degrees)
			  5 of 20 (25.00%) bad tangents (2), max err = 0.005682 (6.110771 degrees)
			  5 of 20 (25.00%) bad tangents (3), max err = 0.005682 (6.110771 degrees)
			  380 of 600 (63.33%) redundant vertices
			  64 non-endpoint triangle errors
			cs4_rd_props_w_spline521: cable geometry has errors (max angle = 172.916534 degrees)
			  140 of 408 (34.31%) bad tangents, max err = 0.966797 (88.097252 degrees)
			  16 of 35 (45.71%) bad tangents (2), max err = 0.006958 (6.762685 degrees)
			  16 of 35 (45.71%) bad tangents (3), max err = 0.006958 (6.762685 degrees)
			  12 of 408 (2.94%) bad opposite tangents (2c)
			  8 of 408 (1.96%) bad opposite texcoord (2c)
			  3 bad topology
			  836 of 1320 (63.33%) redundant vertices
			  86 non-endpoint triangle errors
			cs4_rd_props_w_spline_485: cable geometry has errors (max angle = 176.359543 degrees)
			  348 of 770 (45.19%) bad tangents, max err = 0.987417 (89.279022 degrees)
			  29 of 73 (39.73%) bad tangents (2), max err = 0.007310 (6.932029 degrees)
			  29 of 73 (39.73%) bad tangents (3), max err = 0.007310 (6.932029 degrees)
			  14 of 770 (1.82%) bad opposite tangents (2c)
			  4 of 770 (0.52%) bad opposite texcoord (2c)
			  3 bad topology
			  1596 of 2520 (63.33%) redundant vertices
			  270 non-endpoint triangle errors
			cs4_rd_props_w_spline_483: cable geometry has errors (max angle = 116.260345 degrees)
			  130 of 472 (27.54%) bad tangents, max err = 0.608317 (66.940727 degrees)
			  16 of 48 (33.33%) bad tangents (2), max err = 0.006933 (6.750697 degrees)
			  16 of 48 (33.33%) bad tangents (3), max err = 0.006933 (6.750697 degrees)
			  4 of 472 (0.85%) bad opposite tangents (2c)
			  2 of 472 (0.42%) bad opposite texcoord (2c)
			  988 of 1560 (63.33%) redundant vertices
			  102 non-endpoint triangle errors
			cs4_rd_props_w_spline_482: cable geometry has errors (max angle = 10.034934 degrees)
			  44 of 182 (24.18%) bad tangents, max err = 0.007930 (7.220424 degrees)
			  4 of 18 (22.22%) bad tangents (2), max err = 0.005758 (6.151348 degrees)
			  4 of 18 (22.22%) bad tangents (3), max err = 0.005758 (6.151348 degrees)
			  2 of 182 (1.10%) bad opposite tangents (2c)
			  380 of 600 (63.33%) redundant vertices
			  28 non-endpoint triangle errors
			cs4_rd_props_w_spline_481: cable geometry has errors (max angle = 177.947952 degrees)
			  48 of 404 (11.88%) bad tangents, max err = 0.994984 (89.712601 degrees)
			  3 of 36 (8.33%) bad tangents (2), max err = 0.005963 (6.260152 degrees)
			  3 of 36 (8.33%) bad tangents (3), max err = 0.005963 (6.260152 degrees)
			  8 of 404 (1.98%) bad opposite tangents (2c)
			  8 of 404 (1.98%) bad opposite texcoord (2c)
			  836 of 1320 (63.33%) redundant vertices
			  24 non-endpoint triangle errors
			cs4_rd_props_w_spline_489: cable geometry has errors (max angle = 2.381130 degrees)
			  92 of 306 (30.07%) bad tangents, max err = 0.006710 (6.641275 degrees)
			  5 of 34 (14.71%) bad tangents (2), max err = 0.005969 (6.263314 degrees)
			  5 of 34 (14.71%) bad tangents (3), max err = 0.005969 (6.263314 degrees)
			  646 of 1020 (63.33%) redundant vertices
			  66 non-endpoint triangle errors
			cs4_rd_props_w_spline_488: cable geometry has errors (max angle = 137.742157 degrees)
			  264 of 688 (38.37%) bad tangents, max err = 0.938544 (86.476585 degrees)
			  26 of 86 (30.23%) bad tangents (2), max err = 0.007377 (6.963564 degrees)
			  26 of 86 (30.23%) bad tangents (3), max err = 0.007377 (6.963564 degrees)
			  16 of 688 (2.33%) bad opposite tangents (2c)
			  4 of 688 (0.58%) bad opposite texcoord (2c)
			  6 bad topology
			  1440 of 2304 (62.50%) redundant vertices
			  168 non-endpoint triangle errors
			cs4_rd_props_w_spline_487: cable geometry has errors (max angle = 2.981437 degrees)
			  172 of 648 (26.54%) bad tangents, max err = 0.006800 (6.685334 degrees)
			  24 of 72 (33.33%) bad tangents (2), max err = 0.007204 (6.881650 degrees)
			  24 of 72 (33.33%) bad tangents (3), max err = 0.007204 (6.881650 degrees)
			  1368 of 2160 (63.33%) redundant vertices
			  122 non-endpoint triangle errors
			cs4_rd_props_w_spline_486: cable geometry has errors (max angle = 179.687836 degrees)
			  522 of 1162 (44.92%) bad tangents, max err = 0.782066 (77.412292 degrees)
			  52 of 118 (44.07%) bad tangents (2), max err = 0.007447 (6.996726 degrees)
			  52 of 118 (44.07%) bad tangents (3), max err = 0.007447 (6.996726 degrees)
			  10 of 1162 (0.86%) bad opposite tangents (2c)
			  4 of 1162 (0.34%) bad opposite texcoord (2c)
			  2432 of 3840 (63.33%) redundant vertices
			  396 non-endpoint triangle errors
			*/

			Displayf("%s: cable geometry has errors (max angle = %f degrees)", path, maxSegmentAngle);

			for (int i = 0; i < errors.GetCount(); i++)
			{
				Displayf("  %s", errors[i].c_str());
			}

			return false;
		}
		else
		{
			Displayf("%s: cable geometry ok (%d endpoints, max angle = %f degrees, max tangent error = %f/%f degrees)", path, numCableOkEndpoints, maxSegmentAngle, RtoD*acosf(1.0f - maxTangentError), RtoD*acosf(1.0f - maxTangentError2));
		}
	}

	return true;
}

#endif // __DEV
#endif // __BANK
