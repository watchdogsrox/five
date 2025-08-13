// game headers
#include "river.h"

#include "audio/northaudioengine.h"
#include "game/Clock.h"
#include "grcore/image_dxt.h"
#include "grcore/image.h"
#include "camera/CamInterface.h"
#include "debug/TiledScreenCapture.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/Lights/lights.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/SSAO.h"
#include "renderer/Water.h"
#include "renderer/Mirrors.h"
#include "scene/RegdRefTypes.h"
#include "spatialdata/aabb.h"
#include "timecycle/TimeCycle.h"
#include "physics/WorldProbe/worldprobe.h"
#include "vfx/sky/Sky.h"

#include "grcore\texture_gnm.h"
#include "grcore\wrapper_gnm.h"

#include "system/xtl.h"
#if __D3D
#	include "system/d3d9.h"
#	if __XENON
#		include "system/xtl.h"
#		define DBG 0			// yuck
#		include <xgraphics.h>
#	elif RSG_DURANGO
#		include <xg.h>
	#endif
#endif



#if __DEV
//OPTIMISATIONS_OFF()
#endif

#if RSG_PC && !RSG_FINAL
PARAM(disableRiverFlowReadback, "Disable river flow read back");
#endif // RSG_PC && !RSG_FINAL

bank_float	m_RiverPushForce	= 0.1f;

static grcEffectGlobalVar m_RiverReflectionTextureVar;
static grcEffectGlobalVar m_RiverRefractionTextureVar;
static grcEffectGlobalVar m_RiverNoiseTextureVar;
static grcEffectGlobalVar m_RiverBumpTextureVar;
static grcEffectGlobalVar m_RiverWetTextureVar;
static grcEffectGlobalVar m_RiverScaledTimeVar;
static grcEffectGlobalVar m_RiverFlowParamsVar;
static grcEffectGlobalVar m_RiverFlowParams2Var;
#if WATER_SHADER_DEBUG
static grcEffectGlobalVar m_RiverFogLightVar;
#endif // WATER_SHADER_DEBUG
static grcEffectGlobalVar m_RiverRefractionDepthTextureVar;

grcEffectGlobalVar River::GetBumpTextureVar() { return m_RiverBumpTextureVar; }
grcEffectGlobalVar River::GetWetTextureVar() { return m_RiverWetTextureVar; }

static grcEffectVar m_RiverFlowVar; //This comes from the river effect, not the ocean

float m_RiverHeightAtCamera;
bool m_CameraOnRiver = false;

s32 m_RiverFogTechniqueGroup	= -1;

static atFixedArray<WaterEntity, MAXRIVERENTITIES> m_WaterEntities;

struct RiverDrawable
{
	fwModelId	m_ModelId;
	Mat34V		m_Matrix;
};
static atFixedArray<RiverDrawable, 16> m_RiverDrawables[2];
static s32 m_WaterBucketCount[2];

#if __BANK
bank_s32 m_NumRiverEntities		= 0;
bank_s32 m_NumRiverDrawables	= 0;
#endif

s32 m_NumRiverClassEntities		= 0;

using namespace River;

grcTexture* m_OffsetTexture4x = NULL;

void River::Init()
{
	m_RiverReflectionTextureVar			= grcEffect::LookupGlobalVar("ReflectionTex2");
	m_RiverRefractionTextureVar			= grcEffect::LookupGlobalVar("RefractionTexture");
	m_RiverNoiseTextureVar				= grcEffect::LookupGlobalVar("NoiseTexture");
	m_RiverBumpTextureVar				= grcEffect::LookupGlobalVar("WaterBumpTexture");
	m_RiverWetTextureVar				= grcEffect::LookupGlobalVar("WetTexture");
	m_RiverScaledTimeVar				= grcEffect::LookupGlobalVar("RiverScaledTime");
	m_RiverFlowParamsVar				= grcEffect::LookupGlobalVar("RiverFlowParams");
	m_RiverFlowParams2Var				= grcEffect::LookupGlobalVar("RiverFlowParams2");
#if WATER_SHADER_DEBUG
	m_RiverFogLightVar					= grcEffect::LookupGlobalVar("RiverFogLight");
#endif // WATER_SHADER_DEBUG

	m_RiverRefractionDepthTextureVar	= grcEffect::LookupGlobalVar("RefractionDepthTexture");

	m_OffsetTexture4x = CShaderLib::LookupTexture("offset4x");
	Assert(m_OffsetTexture4x);
	if(m_OffsetTexture4x)
		m_OffsetTexture4x->AddRef();

	m_RiverFogTechniqueGroup			= grmShader::FindTechniqueGroupId("imposter");
}

void CalcFlowParams(float scaledTimeF, float numSamples, Vector4* flowParams)
{
	Vector4 scaledTime	= Vector4(scaledTimeF);
	scaledTime.y		+=1.0f;
	scaledTime.z		+=2.0f;
	scaledTime.w		+=3.0f;
	Vector4 grossTime;
	grossTime.x			= fmod(scaledTime.x, numSamples);
	grossTime.y			= fmod(scaledTime.y, numSamples);
	grossTime.z			= fmod(scaledTime.z, numSamples);
	grossTime.w			= fmod(scaledTime.w, numSamples);

	Vector4 offset		= Vector4(numSamples/2.0f);
	Vector4 t			= grossTime - offset;
	grossTime			= scaledTime - grossTime;

	Vector4 tAbs		= t;
	tAbs.Abs();
	Vector4 c			= offset-tAbs;

	float cLength		= c.x*c.x + c.y*c.y;
	if(numSamples>2.1f)
		cLength			+= c.z*c.z;
	if(numSamples>3.1f)
		cLength			+= c.w*c.w;
	cLength				= sqrt(cLength);
	
	flowParams[0]		= Vector4(t.x, t.y, c.x/cLength, c.y/cLength);
	if(numSamples > 2.0f)
		flowParams[1]	= Vector4(t.z, t.w, c.z/cLength, c.w/cLength);
}

s32 m_PreviousForcedTechniqueGroupId = -1;
void River::BeginRenderRiverFog()
{
	m_PreviousForcedTechniqueGroupId	= grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(m_RiverFogTechniqueGroup);
}

void River::EndRenderRiverFog()
{
	grmShaderFx::SetForcedTechniqueGroupId(m_PreviousForcedTechniqueGroupId);
}

void River::SetWaterPassGlobalVars(float elapsedTime, bool XENON_ONLY(useMirrorWaterRT))
{

#if __XENON
	const grcTexture* reflectionTexture = useMirrorWaterRT? CMirrors::GetMirrorWaterRenderTarget() : Water::GetReflectionRT();
#else
	const grcTexture* reflectionTexture = Water::GetReflectionRT();
#endif //__XENON
	const grcTexture* refractionTexture = Water::GetRefractionRT();

#if __BANK
	switch (Water::m_debugReflectionSource)
	{
	case Water::WATER_REFLECTION_SOURCE_BLACK: reflectionTexture = grcTexture::NoneBlack; break;
	case Water::WATER_REFLECTION_SOURCE_WHITE: reflectionTexture = grcTexture::NoneWhite; break;
	}

	if (TiledScreenCapture::IsEnabled())
	{
		reflectionTexture = TiledScreenCapture::GetWaterReflectionTexture();
		refractionTexture = TiledScreenCapture::GetWaterReflectionTexture();
	}
#endif // __BANK

	grcEffect::SetGlobalVar(m_RiverReflectionTextureVar,		reflectionTexture);
	grcEffect::SetGlobalVar(m_RiverRefractionTextureVar,		refractionTexture);
	grcEffect::SetGlobalVar(m_RiverRefractionDepthTextureVar,	CRenderTargets::GetDepthBufferQuarterLinear());
	grcEffect::SetGlobalVar(m_RiverNoiseTextureVar,				Water::GetNoiseTexture());
	Water::SetWorldBase();

	//Calculate flow params
	Vector4 flowParams[2] = { Vector4(0.f,0.f,0.f,0.f), Vector4(0.f,0.f,0.f,0.f) };
	static dev_float timeScale	= 2.0f;
	CalcFlowParams(elapsedTime*timeScale, 2.0f, flowParams);//Get flow params for 2x flow
	grcEffect::SetGlobalVar(m_RiverFlowParamsVar, (Vec4V&)flowParams[0]);

	const float time = CClock::GetDayRatio();
	grcEffect::SetGlobalVar(m_RiverFlowParams2Var, Vec4V(flowParams[1].x, Water::GetUnderwaterRefractionIndex(), flowParams[1].z, fmodf(time*timeScale, 1.0f)));

#if WATER_SHADER_DEBUG
	// hijacking two unused params for debugging
	grcEffect::SetGlobalVar(m_RiverFlowParams2Var, Vec4f(
		Water::m_debugDistortionScale*(Water::m_debugDistortionEnabled ? 1.0f : 0.0f),
		0.0f,
		Water::m_debugReflectionBrightnessScale,
		0.0f));

	grcEffect::SetGlobalVar(m_RiverFogLightVar, Vec4f(
		Water::m_debugRefractionOverride*0.5f + floorf(Water::m_debugAlphaBias*256.0f),
		Water::m_debugReflectionOverride,
		Water::m_debugReflectionOverrideParab,
		Water::m_debugHighlightAmount));
#endif // WATER_SHADER_DEBUG
}

void River::SetFogPassGlobalVars()
{
	grcEffect::SetGlobalVar(m_RiverWetTextureVar,			Water::GetWetMap());

	grcViewport* vp=grcViewport::GetCurrent();
	float n = vp->GetNearClip();
	float f = vp->GetFarClip();

	float fogLightIntensity = g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_WATER_FOGLIGHT);
	grcEffect::SetGlobalVar(m_RiverScaledTimeVar, Vec4f(fwTimer::GetTimeInMilliseconds()/10000.0f, fogLightIntensity, -n*(f/(f-n)), f/(f-n)));

	Water::SetWorldBase();

	grcEffect::SetGlobalVar(m_RiverNoiseTextureVar,				m_OffsetTexture4x);
	grcEffect::SetGlobalVar(m_RiverRefractionTextureVar,		CRenderPhaseCascadeShadowsInterface::GetSmoothStepTexture());
	grcEffect::SetGlobalVar(m_RiverRefractionDepthTextureVar,	CRenderTargets::GetDepthBufferQuarterLinear());

#if WATER_SHADER_DEBUG
	grcEffect::SetGlobalVar(m_RiverFogLightVar, Vec4f(
		Water::m_debugWaterFogDepthBias  *(Water::m_debugWaterFogBiasEnabled ? 1.0f : 0.0f),
		Water::m_debugWaterFogRefractBias*(Water::m_debugWaterFogBiasEnabled ? 1.0f : 0.0f),
		Water::m_debugWaterFogColourBias *(Water::m_debugWaterFogBiasEnabled ? 1.0f : 0.0f),
		Water::m_debugWaterFogShadowBias *(Water::m_debugWaterFogBiasEnabled ? 1.0f : 0.0f)));
#endif // WATER_SHADER_DEBUG
}

void River::SetDecalPassGlobalVars()
{
	Water::GetCameraRangeForRender();
	Water::SetWorldBase();
	grcEffect::SetGlobalVar(m_RiverWetTextureVar,	BANK_ONLY(TiledScreenCapture::IsEnabled() ? grcTexture::NoneBlack :) Water::GetWetMap());
	grcEffect::SetGlobalVar(m_RiverBumpTextureVar,	BANK_ONLY(TiledScreenCapture::IsEnabled() ? grcTexture::NoneBlack :) Water::GetWaterBumpHeightRT());
}

void River::DrawRiverCaustics(grmShader* shader, grcEffectTechnique tech, s32 pass)
{
	s32 b = Water::GetWaterRenderIndex();

	s32 count = m_RiverDrawables[b].GetCount();

	for(s32 i = 0; i < count; i++)
	{
		Mat34V matrix = m_RiverDrawables[b][i].m_Matrix;
		const rmcDrawable* drawable = CModelInfo::GetBaseModelInfo(m_RiverDrawables[b][i].m_ModelId)->GetDrawable();
		if(Verifyf(drawable, "River drawable is null! It shouldn't be..."))
		{
			grcViewport::SetCurrentWorldMtx(matrix);
			AssertVerify(shader->BeginDraw(grmShader::RMC_DRAW, true, tech));
			shader->Bind(pass);
			drawable->DrawNoShaders(RCC_MATRIX34(matrix), 0xFFFFFFFF, 0);
			shader->UnBind();
			shader->EndDraw();
		}
	}
}

void River::ResetRiverDrawList(){ 	m_RiverDrawables[Water::GetWaterUpdateIndex()].Reset(); }

void River::FillRiverDrawList(){

	s32 b = Water::GetWaterUpdateIndex();

	fwRenderListDesc& renderList = gRenderListGroup.GetRenderListForPhase(Water::GetWaterRenderPhase()->GetEntityListIndex());

	fwEntityList &entityList	= renderList.GetList(RPASS_WATER);
	s32 entityCount				= entityList.GetCount();
	m_WaterBucketCount[b]		= entityCount;


	for(s32 i = 0; i < entityCount; i++)
	{
		if(m_RiverDrawables[b].IsFull())
			break;

		fwEntity* entity = entityList.GetEntity(i);
		fwDrawData* drawHandler = entity->GetDrawHandlerPtr();

		if(!drawHandler)
			continue;

		rmcDrawable* drawable = drawHandler->GetDrawable();
		const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();
		const grmShader* shader = shaderGroup.GetShaderPtr(0);

		bool riverType = false;
		if(shader->GetHashCode() == ATSTRINGHASH("water_river", 0x38B8A4BB))
			riverType = true;
		if(shader->GetHashCode() == ATSTRINGHASH("water_riverocean", 0x81CC67B3))
			riverType = true;
		if(shader->GetHashCode() == ATSTRINGHASH("water_rivershallow", 0x938E5D80))
			riverType = true;
		if(shader->GetHashCode() == ATSTRINGHASH("water_fountain", 0x7569CA0))
			riverType = true;

		if(!riverType)
			continue;

		RiverDrawable riverDrawable;
		riverDrawable.m_ModelId	= entity->GetModelId();
		riverDrawable.m_Matrix	= entity->GetMatrix();

		if(drawHandler)
			m_RiverDrawables[b].Append() = riverDrawable;
	}

#if __BANK
	m_NumRiverDrawables = m_RiverDrawables[b].GetCount();
#endif //__BANK
}

void River::AddToRiverEntityList(CEntity* entity)
{
	m_NumRiverClassEntities++;

	Assert(entity);
	if(m_WaterEntities.IsFull())
		return;

	if( !entity->GetLodData().IsHighDetail() )
		return;
		
	const CBaseModelInfo *modelInfo = entity->GetBaseModelInfo();
	const rmcDrawable *drawable = modelInfo->GetDrawable();
	const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();
	const grmShader* shader = shaderGroup.GetShaderPtr(0);

	WaterEntity waterEntity;
	waterEntity.m_Entity		= RegdEnt(entity);
	if(	shader->GetHashCode() == ATSTRINGHASH("water_river",		0x38B8A4BB) ||
		shader->GetHashCode() == ATSTRINGHASH("water_shallow",		0xE585594F) ||
		shader->GetHashCode() == ATSTRINGHASH("water_rivershallow", 0x938E5D80))
		waterEntity.m_IsRiver	= true;
	else
		waterEntity.m_IsRiver	= false;
#if RSG_ORBIS || RSG_PC
	waterEntity.flowTextureData = NULL;
#endif
#if RSG_PC
	waterEntity.m_WaitingForFlowData = false;
#endif

	spdAABB localBBox = modelInfo->GetBoundingBox();
	spdAABB worldAABB = TransformAABB(entity->GetTransform().GetMatrix(),localBBox);
	waterEntity.m_Min = worldAABB.GetMin();
	waterEntity.m_Max = worldAABB.GetMax();

	m_WaterEntities.Append()	= waterEntity;
		
#if __BANK
	m_NumRiverEntities = m_WaterEntities.GetCount();

	if(m_WaterEntities.IsFull())
	{
		Warningf("[River] River entities list full!");
		for(s32 i = 0; i < m_WaterEntities.GetCount(); i++)
		{
			const CEntity* entity = m_WaterEntities[i].m_Entity;
			Warningf("[River] %d %p river entity %s at [%f, %f, %f]", i, entity,
					entity->GetModelName(),
					VEC3V_ARGS(entity->GetTransform().GetPosition()));
		}
	}
#endif //__BANK

	if(m_RiverFlowVar == grcevNONE)
	{
		if(!waterEntity.m_IsRiver)
			return;

		const CBaseModelInfo *modelInfo = entity->GetBaseModelInfo();
		const rmcDrawable *drawable = modelInfo->GetDrawable();

		Assert(drawable);

		const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();

		s32 count = shaderGroup.GetCount();
		
		for(s32 i = 0; i < count; i++)
		{
			const grmShader* shader = shaderGroup.GetShaderPtr(i);
			if(	shader->GetHashCode() == ATSTRINGHASH("water_river",		0x38B8A4BB) ||
				shader->GetHashCode() == ATSTRINGHASH("water_shallow",		0xE585594F) ||
				shader->GetHashCode() == ATSTRINGHASH("water_rivershallow", 0x938E5D80))
			{
				grcEffectVar flowVar = shader->LookupVar("FlowTexture");
				Assertf(flowVar != grcevNONE, "Flow var not found in entity: %s!", entity->GetModelName());
				Displayf("[River] Flow Var Index obtained: %d", flowVar);
				m_RiverFlowVar = flowVar;
				break;
			}
		}

		Assertf(m_RiverFlowVar != grcevNONE, "Failed to obtain flow flow var index from  entity: %s!", entity->GetModelName());
	}
}
void River::RemoveFromRiverEntityList(const CEntity* entity)
{
	m_NumRiverClassEntities--;

	Assert(entity);

	if( !entity->GetLodData().IsHighDetail() )
		return;

	s32 count = m_WaterEntities.GetCount();
	for(s32 i =0; i < count; i++)
	{
		if(m_WaterEntities[i].m_Entity == entity)
		{
#if RSG_ORBIS || RSG_PC
			if (m_WaterEntities[i].flowTextureData) 
			{
				delete [] m_WaterEntities[i].flowTextureData;
				m_WaterEntities[i].flowTextureData = NULL;
			}
#endif
			m_WaterEntities.DeleteFast(i--);
			BANK_ONLY(m_NumRiverEntities = m_WaterEntities.GetCount();)
			return;
		}
	}
	BANK_ONLY(m_NumRiverEntities = m_WaterEntities.GetCount();)
}

s32 River::GetRiverEntityCount()
{
	return m_WaterEntities.GetCount();
}

s32 River::GetRiverDrawableCount()
{
	return m_RiverDrawables[Water::GetWaterCurrentIndex()].GetCount();
}

s32 River::GetWaterBucketCount()
{
	return m_WaterBucketCount[Water::GetWaterCurrentIndex()];
}

const CEntity* River::GetRiverEntity(s32 entityIndex)
{
	return m_WaterEntities[entityIndex].m_Entity.Get();
}

Color32 GetTiledDXT1PixelColor32(s32 x, s32 y, s32 width, const void* bits, const grcTexture* texture)
{
	s32 offset;

#if __XENON
	s32 bytesPerBlock = 8;
	(void)texture;
	offset = XGAddress2DTiledOffset(x/4, y/4, (width+3)/4, bytesPerBlock)*bytesPerBlock;
#elif RSG_DURANGO
	(void)width;
	(void)bits;
	XGTextureAddressComputer *pxgComputer = NULL;
	XG_TEXTURE2D_DESC xgDesc;
	(reinterpret_cast<const grcTextureDurango*>(texture))->GetPlacementTexture()->FillInXGDesc2D(xgDesc);
	XGCreateTexture2DComputer(&xgDesc, &pxgComputer);
	offset = pxgComputer->GetTexelElementOffsetBytes(0, 0, x/4, y/4, 0, 0);
	pxgComputer->Release();
#else
	s32 bytesPerBlock = 8;
	(void)texture;
	const s32 bx = x/4;
	const s32 by = y/4;
	const s32 blocksPerRow  = (width + 3)/4;
	offset = (bx + by*blocksPerRow)*bytesPerBlock;
#endif //__XENON

	Color32 block[4*4];
	reinterpret_cast<DXT::DXT1_BLOCK*>((u8 *)bits + offset)->Decompress(block);
	return block[(x&3) + (y&3)*4];
}

#if RSG_PC
void River::UpdateFlowMaps() 
{
#if !RSG_FINAL
	if (PARAM_disableRiverFlowReadback.Get())
		return;
#endif // !RSG_FINAL

	s32 count = m_WaterEntities.GetCount();
	for(s32 i =0; i < count; i++)
	{
		WaterEntity &waterEntity = m_WaterEntities[i];
		if (waterEntity.m_IsRiver && waterEntity.flowTextureData == NULL)
		{
			const CEntity* entity = waterEntity.m_Entity;
			const CBaseModelInfo *modelInfo = entity->GetBaseModelInfo();
			const rmcDrawable *drawable = modelInfo->GetDrawable();

			Assert(drawable);
			const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();

			//Get shader
			s32 numShaders = shaderGroup.GetCount();
			s32 shaderGroupIndex = -1;
			for(s32 i = 0; i < numShaders; i++)
			{
				const grmShader* shader = shaderGroup.GetShaderPtr(i);
				if(	shader->GetHashCode() == ATSTRINGHASH("water_river",		0x38B8A4BB) ||
					shader->GetHashCode() == ATSTRINGHASH("water_shallow",		0xE585594F) ||
					shader->GetHashCode() == ATSTRINGHASH("water_rivershallow", 0x938E5D80))
				{
					shaderGroupIndex = i;
					break;
				}
			}
			const grmShader* shader = shaderGroup.GetShaderPtr(shaderGroupIndex);

			grcTexture* texture = NULL;

			shader->GetVar(m_RiverFlowVar, texture);

			if (!Verifyf(texture, "Flow map lookup failed on entity with model: [%s] and shader: [%s]", entity->GetModelName(), shader->GetName()))
				continue;

			grcTextureDX11* flowTexture = (grcTextureDX11*)texture;

			bool bHasStagingTexture = flowTexture->HasStagingTexture();
			if (!bHasStagingTexture)
			{
				flowTexture->InitializeTempStagingTexture();
			}

			if ( !waterEntity.m_WaitingForFlowData )
			{
				// If the read-back hasn't already been started, kick it off now
				flowTexture->UpdateCPUCopy(true);
				waterEntity.m_WaitingForFlowData = true;

			}
			else 
			{
				// The texture read-back was kicked off already, check to see if it's done yet
				if ( flowTexture->MapStagingBufferToBackingStore(true) )
				{
					waterEntity.m_WaitingForFlowData = false;
				
					grcTextureLock lock;
					if (flowTexture->LockRect( 0, 0, lock, grcsRead ))
					{
						u32 textureSize = flowTexture->GetStride(0) * flowTexture->GetRowCount(0);
						waterEntity.flowTextureData = rage_new u8[textureSize];

						sysMemCpy( waterEntity.flowTextureData, (u8*)lock.Base, flowTexture->GetStride(0) * flowTexture->GetRowCount(0));

						flowTexture->UnlockRect(lock);
					}
					if (!bHasStagingTexture)
						flowTexture->ReleaseTempStagingTexture();
				}
			}
		}
	}
}
#endif

void River::GetRiverFlowFromEntity(WaterEntity& waterEntity, const Vector3& position, Vector2& outFlow)
{
	const CEntity* entity = waterEntity.m_Entity;
	const CBaseModelInfo *modelInfo = entity->GetBaseModelInfo();
	const rmcDrawable *drawable = modelInfo->GetDrawable();

	spdAABB localBBox = modelInfo->GetBoundingBox();
	spdAABB worldAABB = TransformAABB(entity->GetTransform().GetMatrix(),localBBox);
	Vector3 min = worldAABB.GetMinVector3();
	Vector3 max = worldAABB.GetMaxVector3();
	
	Assert(drawable);
	const grmShaderGroup& shaderGroup = drawable->GetShaderGroup();

	//Get shader
	s32 numShaders = shaderGroup.GetCount();
	s32 shaderGroupIndex = -1;
	for(s32 i = 0; i < numShaders; i++)
	{
		const grmShader* shader = shaderGroup.GetShaderPtr(i);
		if(	shader->GetHashCode() == ATSTRINGHASH("water_river",		0x38B8A4BB) ||
			shader->GetHashCode() == ATSTRINGHASH("water_shallow",		0xE585594F) ||
			shader->GetHashCode() == ATSTRINGHASH("water_rivershallow", 0x938E5D80))
		{
			shaderGroupIndex = i;
			break;
		}
	}
	const grmShader* shader = shaderGroup.GetShaderPtr(shaderGroupIndex);

	grcTexture* texture = NULL;

	shader->GetVar(m_RiverFlowVar, texture);

	if(!Verifyf(texture, "Flow map lookup failed on entity with model: [%s] and shader: [%s]", entity->GetModelName(), shader->GetName()))
	{

#if __ASSERT
		grcEffectVar flowVar = shader->LookupVar("FlowTexture");
		if(flowVar != m_RiverFlowVar)
			Assertf(texture, "Flow map found at index [%d] but is suppose to be at [%d]", flowVar, m_RiverFlowVar);
		else if(flowVar == grcevNONE)
			Assertf(texture, "Flow sampler not found in this shader [%s]", shader->GetName());
		else
			Assertf(texture, "Flow var appears correct yet a texture could not be found");
#endif //_BANK

		return;
	}


	s32 width	= texture->GetWidth();
	s32 height	= texture->GetHeight();

#if RSG_PC
	if (!waterEntity.flowTextureData)
	{
		return;
	}

	void* fastLock = waterEntity.flowTextureData;
#elif RSG_ORBIS
	if (waterEntity.flowTextureData == NULL)
	{
		const sce::Gnm::Texture *gnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(texture->GetTexturePtr());

		uint64_t offset;
		uint64_t size;
#if SCE_ORBIS_SDK_VERSION < (0x00930020u)
		sce::GpuAddress::computeSurfaceOffsetAndSize(
#else
		sce::GpuAddress::computeTextureSurfaceOffsetAndSize(
#endif
			&offset,&size,gnmTexture,0,0);

		waterEntity.flowTextureData = rage_new u8[size];

		sce::GpuAddress::TilingParameters tp;
		tp.initFromTexture(gnmTexture, 0, 0);

		sce::GpuAddress::detileSurface(waterEntity.flowTextureData, ((char*)gnmTexture->getBaseAddress() + offset),&tp);
	}

	if (!waterEntity.flowTextureData) return;

	void* fastLock = waterEntity.flowTextureData;
#else
	grcTextureLock lock;
	if (!texture->LockRect(0, 0, lock, grcsRead))
		return;

	void* fastLock = lock.Base;
#endif //RSG_PC

	Vector3 offsetV3 = -min;
	Vector3 scaleV3 = Vector3((float)width, (float)height, 1.0f)/(max-min);
	Vector2 offset = Vector2(offsetV3.x, offsetV3.y);
	Vector2 scale = Vector2(scaleV3.x, scaleV3.y);

	Vector2 pos = Vector2(position.x, position.y);
	// Vector2 flow = Vector2(0, 0);
	if((pos.x >= min.x) && (pos.x <= max.x) && (pos.y >= min.y) && (pos.y <= max.y))
	{
		Vector2 texCoords = (pos + offset) * scale;

		s32 x = (s32)texCoords.x;
		s32 y = height-1-(s32)texCoords.y;

		if(!(x < width && x >= 0))
		{
			// fix for rounding errors
			if(texCoords.x + 0.1f < 0)
			{
				Errorf("invalid x of %d [%d-%d]", x, 0, width);
				outFlow.Set(0.0f, 0.0f);
				return;
			}
			else
				x = 0;

			if(texCoords.x - 0.1f > width)
			{
				Errorf("invalid x of %d [%d-%d]", x, 0, width);
				outFlow.Set(0.0f, 0.0f);
				return;
			}
			else
				x = width -1;
		}
		if(!(y < height && y >= 0))
		{
			// fix for rounding errors
			if(texCoords.y + 0.1f < 0)
			{
				Errorf("invalid x of %d [%d-%d]", x, 0, width);
				outFlow.Set(0.0f, 0.0f);
				return;
			}
			else
				y = 0;

			if(texCoords.y - 0.1f > height)
			{
				Errorf("invalid x of %d [%d-%d]", y, 0, height);
				outFlow.Set(0.0f, 0.0f);
				return;
			}
			else
				y = height -1;
		}

		Color32 color = GetTiledDXT1PixelColor32(x, y, width, fastLock, texture);
		Vector2 colorV = Vector2(color.GetRedf(), color.GetGreenf());
		colorV.Scale(2.0f);
		colorV.Subtract(Vector2(1.0f, 1.0f));
		outFlow = colorV * m_RiverPushForce;
	}
	else
		outFlow.Set(0.0f, 0.0f);

#if !__XENON && !RSG_ORBIS && !RSG_PC
	texture->UnlockRect(lock);
#endif //__XENON
}

bool River::IsPositionNearRiver_Impl(Vec3V_In pos, s32* pEntityIndex, bool riversOnly, VecBoolV_In channelsToConsider, ScalarV_In maxDistanceToConsider)
{
	// Walk through the array of river entities and determine if the given position
	// is within any of their bounding boxes. Returns the index of the first entity
	// which we are in by reference.

	Vec3V boundingBoxExpandForMaxDistance(maxDistanceToConsider);
	
	for(WaterEntity* ent = m_WaterEntities.begin(); ent != m_WaterEntities.end(); ++ent)
	{
		if(riversOnly && !ent->m_IsRiver)
			continue;

		Vec3V min = ent->m_Min - boundingBoxExpandForMaxDistance;
		Vec3V max = ent->m_Max + boundingBoxExpandForMaxDistance;

		VecBoolV posVsMin = IsLessThan(pos, min);
		VecBoolV posVsMax = IsGreaterThan(pos, max);
		VecBoolV isOutside = posVsMin | posVsMax;
		if (IsEqualIntAll(isOutside & channelsToConsider, VecBoolV(V_FALSE)))
		{
			if(pEntityIndex)
				*pEntityIndex = ptrdiff_t_to_int(ent - m_WaterEntities.begin());
			return true;
		}
	}

	if(pEntityIndex)
		*pEntityIndex = -1;
	return false;
}

void River::GetRiverFlowFromPosition(Vec3V_In pos, Vector2& outFlow)
{
	s32 entityIndex;
	if(IsPositionNearRiver(pos, &entityIndex, true))
	{
		Vector3 posV = VEC3V_TO_VECTOR3(pos);
		GetRiverFlowFromEntity(m_WaterEntities[entityIndex], posV, outFlow);
		return;
	}
	outFlow.Set(0.0f, 0.0f);
}

bool hit = false;
void River::Update()
{
	Vector3 cameraPosition = camInterface::GetPos();
	float height;
	bool hasFoundWater = CWaterTestHelper::GetRiverHeightAtPosition(cameraPosition, &height);

	if(hasFoundWater)
	{
		m_CameraOnRiver				= true;
		m_RiverHeightAtCamera		= height;
	}
	else
	{
		m_CameraOnRiver				= false;
		m_RiverHeightAtCamera		= 0.0f;
	}
}

float River::GetRiverPushForce(){ return m_RiverPushForce; }
float River::GetRiverHeightAtCamera(){ return m_RiverHeightAtCamera; }
bool River::CameraOnRiver(){ return m_CameraOnRiver; }

#if __BANK

void PrintFlowList()
{
	s32 count = m_WaterEntities.GetCount();

	for(s32 i = 0; i < count; i++)
	{
		CEntity* entity = m_WaterEntities[i].m_Entity;
		Displayf("%s", entity->GetModelName());
	}
}

void River::InitWidgets(bkBank& bank)
{
	bank.AddTitle("Rivers");
	bank.AddSlider("River Push Force",		&m_RiverPushForce, 0.0f,5.0f, 0.1f);
	bank.AddText("River Entities",			&m_NumRiverEntities);
	bank.AddText("River Drawables",			&m_NumRiverDrawables);   
	bank.AddText("River Class Entities",	&m_NumRiverClassEntities);
	bank.AddText("Water Bucket Count",		&m_WaterBucketCount[0]);
	bank.AddText("CameraRiverHeight",		&m_RiverHeightAtCamera);
	bank.AddButton("Print Flow List",		PrintFlowList);
}

#endif
