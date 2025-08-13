//
//
//
//
#include "system/nelem.h"

#include "ai/ambient/AmbientModelSetManager.h"
#include "modelinfo/modelinfo.h"
#include "modelinfo/modelinfo_factories.h"
#include "task/scenario/info/scenarioinfo.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "task/scenario/scenariomanager.h"
#include "task/scenario/scenariopointmanager.h"
#include "task/scenario/scenariopointgroup.h"
#include "task/scenario/taskscenario.h"
#include "entity/archetype.h"
#include "grcore/debugdraw.h"
#include "fwscene/stores/txdstore.h"
#include "objects/object.h"
#include "objects/DummyObject.h"
#include "scene/building.h"
#include "scene/AnimatedBuilding.h"
#include "renderer/Lights/LightSource.h"
#include "scene/2deffect.h"
#include "scene/loader/mapdata_extensions.h"
#include "scene/worldpoints.h"
#include "script/script.h"
#include "vfx/channel.h"
#include "vfx/metadata/PtFxAssetInfo.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/systems/vfxentity.h"
#include "camera/viewports/ViewportManager.h"
#include "audio/northaudioengine.h"


//auto IDs
AUTOID_IMPL(CLightAttr);
AUTOID_IMPL(CExplosionAttr);
AUTOID_IMPL(CParticleAttr);
AUTOID_IMPL(CDecalAttr);
AUTOID_IMPL(CWindDisturbanceAttr);
AUTOID_IMPL(CBuoyancyAttr);
AUTOID_IMPL(CProcObjAttr);
AUTOID_IMPL(CWorldPointAttr);
AUTOID_IMPL(CLadderInfo);
AUTOID_IMPL(CAudioCollisionInfo);
AUTOID_IMPL(CAudioAttr);
AUTOID_IMPL(CSpawnPoint);
AUTOID_IMPL(CLightShaftAttr);
AUTOID_IMPL(CScrollBarAttr);
AUTOID_IMPL(CExpressionExtension);

#if __DECLARESTRUCT

void ColourData::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(ColourData);
	STRUCT_FIELD(red);
	STRUCT_FIELD(green);
	STRUCT_FIELD(blue);
	STRUCT_END();
}

void VectorData::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(VectorData);
	STRUCT_FIELD(x);
	STRUCT_FIELD(y);
	STRUCT_FIELD(z);
	STRUCT_END();
}

#endif //__DECLARESTRUCT

#if __BANK
XPARAM(audiotag);
#endif

const u32 s_defaultAudioModelCollisions = ATSTRINGHASH("MOD_DEFAULT", 0x05f03e8c2);

CLightAttr::CLightAttr()
{
}

CLightAttr::CLightAttr(datResource& UNUSED_PARAM(rsc)) 
{
}

void CLightAttr::Set(const CLightAttrDef *attr)
{
	// C2dEffect data
	m_posn.x				= attr->m_posn[0];
	m_posn.y				= attr->m_posn[1];
	m_posn.z				= attr->m_posn[2];

	// General data
	m_colour.red			= attr->m_colour[0];
	m_colour.green			= attr->m_colour[1];
	m_colour.blue			= attr->m_colour[2];
	m_flashiness			= attr->m_flashiness;
	m_intensity				= attr->m_intensity;
	m_flags					= attr->m_flags;
	m_boneTag				= attr->m_boneTag;
	m_lightType				= attr->m_lightType;
	m_groupId				= attr->m_groupId;
	m_timeFlags				= attr->m_timeFlags;
	m_falloff				= attr->m_falloff;
	m_falloffExponent		= attr->m_falloffExponent;
	m_cullingPlane[0]		= attr->m_cullingPlane[0];
	m_cullingPlane[1]		= attr->m_cullingPlane[1];
	m_cullingPlane[2]		= attr->m_cullingPlane[2];
	m_cullingPlane[3]		= attr->m_cullingPlane[3];

	// Volume data
	m_volIntensity			= attr->m_volIntensity;
	m_volSizeScale			= attr->m_volSizeScale;
	m_volOuterColour		.Set(Color32(attr->m_volOuterColour[0], attr->m_volOuterColour[1], attr->m_volOuterColour[2]));
	m_lightHash				= attr->m_lightHash;
	m_volOuterIntensity		= attr->m_volOuterIntensity;

	// Corona data
	m_coronaSize			= attr->m_coronaSize;
	m_volOuterExponent		= attr->m_volOuterExponent;
	m_coronaIntensity		= attr->m_coronaIntensity;
	m_coronaZBias			= attr->m_coronaZBias;

	// Spot data
	m_direction				.Set(Vector3(attr->m_direction[0], attr->m_direction[1], attr->m_direction[2]));
	m_tangent				.Set(Vector3(attr->m_tangent[0], attr->m_tangent[1], attr->m_tangent[2]));
	m_coneInnerAngle		= attr->m_coneInnerAngle;
	m_coneOuterAngle		= attr->m_coneOuterAngle;

	// Line data
	m_extents				.Set(Vector3(attr->m_extents[0], attr->m_extents[1], attr->m_extents[2]));

	// Texture data
	m_projectedTextureKey	= attr->m_projectedTextureKey;

	m_lightFadeDistance		 = attr->m_lightFadeDistance;
	m_shadowFadeDistance	 = attr->m_shadowFadeDistance;
	m_specularFadeDistance	 = attr->m_specularFadeDistance;
	m_volumetricFadeDistance = attr->m_volumetricFadeDistance;
	m_shadowNearClip		 = attr->m_shadowNearClip;

	m_shadowBlur			 = attr->m_shadowBlur;

	//Need to initialise as its getting used in a hack fix in LightEntity.cpp
	m_padding1				= 0;

#if RSG_BANK
	m_padding3				= 0;	// hack: initialize extraFlags
#endif
}

#if !__FINAL
void CLightAttr::Reset(const Vector3& pos, const Vector3& dir, float falloff, float coneOuter)
{
	SetPos(pos);

	// General data
	m_colour					. Set(Color32(255, 255, 255, 0));
	m_flashiness				= FL_CONSTANT;
	m_intensity					= 1.0f;
	m_flags						= 0;
	m_boneTag					= 0;
	m_lightType					= LIGHT_TYPE_SPOT;
	m_groupId					= 0;
	m_timeFlags					= (1<<24) - 1;
	m_falloff					= falloff;
	m_falloffExponent			= 8.0f;
	m_cullingPlane[0]			= 0.0f;
	m_cullingPlane[1]			= 0.0f;
	m_cullingPlane[2]			= 1.0f;
	m_cullingPlane[3]			= -pos.z;

	// Volume data
	m_volIntensity				= 1.0f;
	m_volSizeScale				= 1.0f;
	m_volOuterColour			. Set(Color32(255, 255, 255, 0));
	m_lightHash 				= 0;
	m_volOuterIntensity			= 1.0f;

	// Corona data
	m_coronaSize				= 0.1f;
	m_volOuterExponent			= 1.0f;
	m_coronaIntensity			= 1.0f;
	m_coronaZBias				= CORONA_DEFAULT_ZBIAS;

	// Spot data
	m_direction					. Set(dir);
	m_tangent					. Set(Vector3(1.0f, 0.0f, 0.0f));
	m_coneInnerAngle			= coneOuter*0.5f;
	m_coneOuterAngle			= coneOuter;

	// Line data
	m_extents					. Set(Vector3(1.0f, 0.0f, 0.0f));

	// Texture data
	m_projectedTextureKey		= 0;

	m_lightFadeDistance			= 0U;
	m_shadowFadeDistance		= 0U;
	m_specularFadeDistance		= 0U;
	m_volumetricFadeDistance	= 0U;

	m_shadowNearClip			= 0.01f;

	m_shadowBlur				= 0;

	m_padding1					= 0;
	m_padding3					= 0;
}
#endif

#if __DECLARESTRUCT

void C2dEffect::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(C2dEffect);
	STRUCT_FIELD(m_posn);
	STRUCT_END();
}

void CLightAttr::DeclareStruct(datTypeStruct &s)
{
	C2dEffect::DeclareStruct(s);

	SSTRUCT_BEGIN_BASE(CLightAttr, C2dEffect)
	SSTRUCT_FIELD(CLightAttr, m_colour)
	SSTRUCT_FIELD(CLightAttr, m_flashiness)
	SSTRUCT_FIELD(CLightAttr, m_intensity)
	SSTRUCT_FIELD(CLightAttr, m_flags)
	SSTRUCT_FIELD(CLightAttr, m_boneTag)
	SSTRUCT_FIELD(CLightAttr, m_lightType)
	SSTRUCT_FIELD(CLightAttr, m_groupId)
	SSTRUCT_FIELD(CLightAttr, m_timeFlags)
	SSTRUCT_FIELD(CLightAttr, m_falloff)
	SSTRUCT_FIELD(CLightAttr, m_falloffExponent)
	SSTRUCT_CONTAINED_ARRAY_COUNT(CLightAttr, m_cullingPlane, NELEM(m_cullingPlane))
	SSTRUCT_FIELD(CLightAttr, m_shadowBlur)
	SSTRUCT_FIELD(CLightAttr, m_padding1)
	SSTRUCT_FIELD(CLightAttr, m_padding2)
	SSTRUCT_FIELD(CLightAttr, m_padding3)
	SSTRUCT_FIELD(CLightAttr, m_volIntensity)
	SSTRUCT_FIELD(CLightAttr, m_volSizeScale)
	SSTRUCT_FIELD(CLightAttr, m_volOuterColour)
	SSTRUCT_FIELD(CLightAttr, m_lightHash)
	SSTRUCT_FIELD(CLightAttr, m_volOuterIntensity)
	SSTRUCT_FIELD(CLightAttr, m_coronaSize)
	SSTRUCT_FIELD(CLightAttr, m_volOuterExponent)
	SSTRUCT_FIELD(CLightAttr, m_lightFadeDistance)
	SSTRUCT_FIELD(CLightAttr, m_shadowFadeDistance)
	SSTRUCT_FIELD(CLightAttr, m_specularFadeDistance)
	SSTRUCT_FIELD(CLightAttr, m_volumetricFadeDistance)
	SSTRUCT_FIELD(CLightAttr, m_shadowNearClip)
	SSTRUCT_FIELD(CLightAttr, m_coronaIntensity)
	SSTRUCT_FIELD(CLightAttr, m_coronaZBias)
	SSTRUCT_FIELD(CLightAttr, m_direction)
	SSTRUCT_FIELD(CLightAttr, m_tangent)
	SSTRUCT_FIELD(CLightAttr, m_coneInnerAngle)
	SSTRUCT_FIELD(CLightAttr, m_coneOuterAngle)
	SSTRUCT_FIELD(CLightAttr, m_extents)
	SSTRUCT_FIELD(CLightAttr, m_projectedTextureKey)
	SSTRUCT_END(CLightAttr)
}

#endif // __DECLARESTRUCT

void C2dEffect::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* /*archetype*/)
{
	const CExtensionDef& def = *smart_cast<const CExtensionDef*>(definition);
	m_posn.x = def.m_offsetPosition.x;
	m_posn.y = def.m_offsetPosition.y;
	m_posn.z = def.m_offsetPosition.z;
}

void C2dEffect::CopyToDefinition(fwExtensionDef* definition)
{
	CExtensionDef& def = *smart_cast<CExtensionDef*>(definition);
	def.m_offsetPosition.x = m_posn.x;
	def.m_offsetPosition.y = m_posn.y;
	def.m_offsetPosition.z = m_posn.z;
}

void CExplosionAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefExplosionEffect& def = *smart_cast<const CExtensionDefExplosionEffect*>(definition);
	CBaseModelInfo& modelinfo = *smart_cast<CBaseModelInfo*>(archetype);
	
	qX = def.m_offsetRotation.x;
	qY = def.m_offsetRotation.y;
	qZ = def.m_offsetRotation.z;
	qW = def.m_offsetRotation.w;

	m_tagHashName = atHashWithStringNotFinal(def.m_explosionName);

	m_boneTag = def.m_boneTag;
	m_explosionType = def.m_explosionType;

	m_ignoreDamageModel = (def.m_flags & (1<<1)) != 0;
	m_playOnParent = (def.m_flags & (1<<2)) != 0;
	m_onlyOnDamageModel = (def.m_flags & (1<<3)) != 0;
	m_allowRubberBulletShotFx = (def.m_flags & (1<<4)) != 0;
	m_allowElectricBulletShotFx = (def.m_flags & (1<<5)) != 0;

	if (m_explosionType==XT_SHOT_PT_FX || m_explosionType==XT_SHOT_GEN_FX)
		modelinfo.SetHasFxEntityShot(true);
	else if (m_explosionType==XT_BREAK_FX)
		modelinfo.SetHasFxEntityBreak(true);
	else if (m_explosionType==XT_DESTROY_FX)
		modelinfo.SetHasFxEntityDestroy(true);
}

void CParticleAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefParticleEffect& def = *smart_cast<const CExtensionDefParticleEffect*>(definition);
	CBaseModelInfo& modelinfo = *smart_cast<CBaseModelInfo*>(archetype);

	qX = def.m_offsetRotation.x;
	qY = def.m_offsetRotation.y;
	qZ = def.m_offsetRotation.z;
	qW = def.m_offsetRotation.w;

	m_tagHashName = atHashWithStringNotFinal(def.m_fxName);

	m_hasTint = def.m_flags & (1<<0);
	m_tintR = def.m_color.GetRed();
	m_tintG = def.m_color.GetGreen();
	m_tintB = def.m_color.GetBlue();
	m_tintA = def.m_color.GetAlpha();

	m_fxType = def.m_fxType;
	m_boneTag = def.m_boneTag;
	m_scale = def.m_scale;
	m_probability = def.m_probability;

	m_ignoreDamageModel = (def.m_flags & (1<<1)) != 0;
	m_playOnParent = (def.m_flags & (1<<2)) != 0;
	m_onlyOnDamageModel = (def.m_flags & (1<<3)) != 0;
	m_allowRubberBulletShotFx = (def.m_flags & (1<<4)) != 0;
	m_allowElectricBulletShotFx = (def.m_flags & (1<<5)) != 0;

	// check particle effects haven't been disabled already on this entity
	// this happens when an entity is found that uses particle effects from multiple particle assets (we can only have a single particle asset per model info)
#if !__FINAL
	if (modelinfo.GetHasFxEntityDisableAll()==false)
#endif
	{
		if (m_fxType==PT_AMBIENT_FX)
			modelinfo.SetHasFxEntityAmbient(true);
		else if (m_fxType==PT_COLLISION_FX)
			modelinfo.SetHasFxEntityCollision(true);
		else if (m_fxType==PT_SHOT_FX)
			modelinfo.SetHasFxEntityShot(true);
		else if (m_fxType==PT_BREAK_FX)
			modelinfo.SetHasFxEntityBreak(true);
		else if (m_fxType==PT_DESTROY_FX)
			modelinfo.SetHasFxEntityDestroy(true);
		else if (m_fxType==PT_ANIM_FX)
			modelinfo.SetHasFxEntityAnim(true);
		else if (m_fxType==PT_INWATER_FX)
			modelinfo.SetHasFxEntityInWater(true);

		// don't set up a particle asset dependency if the effect is part of the core asset
		atHashWithStringNotFinal ptFxAssetHashName = g_vfxEntity.GetPtFxAssetName((eParticleType)m_fxType, m_tagHashName);
		if (ptFxAssetHashName==atHashString("core"))
		{
			return;
		}

		// set up the particle asset
		int slotCurr = modelinfo.GetPtFxAssetSlot();
		if (slotCurr==-1)
		{
			// the slot hasn't been set yet - find and set it
			if (vfxVerifyf(ptFxAssetHashName!=0, "%s references undefined particle tag %s (type %d)", modelinfo.GetModelName(), m_tagHashName.TryGetCStr(), m_fxType))
			{
				strLocalIndex slotNew = ptfxManager::GetAssetStore().FindSlotFromHashKey(ptFxAssetHashName);
				if(vfxVerifyf(slotNew.IsValid(), "%s has particle extension using unrecognised particle asset %s", modelinfo.GetModelName(), ptFxAssetHashName.TryGetCStr()))
				{
					modelinfo.SetPtFxAssetSlot(slotNew.Get());
				}
			}
		}
		else
		{
			// the slot is already set up - find the new slot
			if (vfxVerifyf(ptFxAssetHashName!=0, "%s references undefined particle tag %s (type %d)", modelinfo.GetModelName(), m_tagHashName.TryGetCStr(), m_fxType))
			{
				strLocalIndex slotNew = ptfxManager::GetAssetStore().FindSlotFromHashKey(ptFxAssetHashName);

				// check if the new slot is different to the current one
				if (vfxVerifyf(slotNew.IsValid(), "%s has particle extension using unrecognised particle asset %s", modelinfo.GetModelName(), ptFxAssetHashName.TryGetCStr()) &&
					slotNew!=slotCurr)
				{
					// check if the new slot is already a dependency of the current slot
					bool hasDependency = false;
					strLocalIndex slotParent = ptfxManager::GetAssetStore().GetParentIndex(strLocalIndex(slotCurr));
					while (slotParent.IsValid())
					{
						if (slotParent==slotNew)
						{
							hasDependency = true;
							break;
						}

						slotParent = ptfxManager::GetAssetStore().GetParentIndex(strLocalIndex(slotParent));
					}

					// if no dependency is found we need to check if the new slot has the current as a dependency
					if (hasDependency==false)
					{
						strLocalIndex slotParent = ptfxManager::GetAssetStore().GetParentIndex(strLocalIndex(slotNew));
						while (slotParent.IsValid())
						{
							if (slotParent==slotCurr)
							{
								modelinfo.SetPtFxAssetSlot(slotNew.Get());
								hasDependency = true;
								break;
							}

							slotParent = ptfxManager::GetAssetStore().GetParentIndex(strLocalIndex(slotParent));
						}
					}
					
					// check if no dependency was set up
					if (hasDependency==false)
					{
						// more than one particle asset is being referenced on this model info - disable all particle effects
						vfxAssertf(0, "%s has multiple particle asset dependencies but no link between them (%s and %s)", modelinfo.GetModelName(), ptFxAssetHashName.TryGetCStr(), ptfxManager::GetAssetStore().GetName(strLocalIndex(slotCurr)));
						modelinfo.SetHasFxEntityAmbient(false);
						modelinfo.SetHasFxEntityCollision(false);
						modelinfo.SetHasFxEntityShot(false);
						modelinfo.SetHasFxEntityBreak(false);
						modelinfo.SetHasFxEntityDestroy(false);
						modelinfo.SetHasFxEntityAnim(false);
						modelinfo.SetHasFxEntityInWater(false);
#if !__FINAL
						modelinfo.SetHasFxEntityDisableAll(true);
#endif
					}
				}
			}
		}
	}
}

void CDecalAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefDecal& def = *smart_cast<const CExtensionDefDecal*>(definition);
	CBaseModelInfo& modelinfo = *smart_cast<CBaseModelInfo*>(archetype);

	qX = def.m_offsetRotation.x;
	qY = def.m_offsetRotation.y;
	qZ = def.m_offsetRotation.z;
	qW = def.m_offsetRotation.w;

	m_tagHashName = atHashWithStringNotFinal(def.m_decalName);

	m_decalType = def.m_decalType;
	m_boneTag = def.m_boneTag;
	m_scale = def.m_scale;
	m_probability = def.m_probability;

	m_ignoreDamageModel = (def.m_flags & (1<<1)) != 0;
	m_playOnParent = (def.m_flags & (1<<2)) != 0;
	m_onlyOnDamageModel = (def.m_flags & (1<<3)) != 0;
	m_allowRubberBulletShotFx = (def.m_flags & (1<<4)) != 0;
	m_allowElectricBulletShotFx = (def.m_flags & (1<<5)) != 0;

	if (m_decalType==DT_COLLISION_FX)
		modelinfo.SetHasFxEntityCollision(true);
	else if (m_decalType==DT_SHOT_FX)
		modelinfo.SetHasFxEntityShot(true);
	else if (m_decalType==DT_BREAK_FX)
		modelinfo.SetHasFxEntityBreak(true);
	else if (m_decalType==DT_DESTROY_FX)
		modelinfo.SetHasFxEntityDestroy(true);
}

void CWindDisturbanceAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefWindDisturbance& def = *smart_cast<const CExtensionDefWindDisturbance*>(definition);

	qX = def.m_offsetRotation.x;
	qY = def.m_offsetRotation.y;
	qZ = def.m_offsetRotation.z;
	qW = def.m_offsetRotation.w;

	m_disturbanceType = def.m_disturbanceType;
	m_boneTag = def.m_boneTag;
	m_sizeX = def.m_size.x;
	m_sizeY = def.m_size.y;
	m_sizeZ = def.m_size.z;
	m_sizeW = def.m_size.w;
	m_strength = def.m_strength;

	m_strengthMultipliesGlobalWind = (def.m_flags & (1<<0)) != 0;
}

void CBuoyancyAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
}

void CProcObjAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefProcObject& def = *smart_cast<const CExtensionDefProcObject*>(definition);

	radiusInner	= def.m_radiusInner;
	radiusOuter	= def.m_radiusOuter;
	objHash		= def.m_objectHash;
	spacing		= def.m_spacing;
	minScale	= def.m_minScale;
	maxScale	= def.m_maxScale;
	minScaleZ	= def.m_minScaleZ;
	maxScaleZ	= def.m_maxScaleZ;
	zOffsetMin	= def.m_minZOffset;
	zOffsetMax	= def.m_maxZOffset;
	isAligned	= (def.m_flags != 0);	// TODO - is this really correct?
}

void CWorldPointAttr::Reset(bool bResetScriptBrainToLoad)
{
	BrainStatus = CWorldPoints::OUT_OF_BRAIN_RANGE;
	LastUpdatedInFrame = 0;

	if (bResetScriptBrainToLoad)
	{
		ScriptBrainToLoad = -1;
	}
}

void CLadderInfo::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefLadder& def = *smart_cast<const CExtensionDefLadder*>(definition);
	CBaseModelInfo& modelinfo = *smart_cast<CBaseModelInfo*>(archetype);

	modelinfo.SetAttributes(MODEL_ATTRIBUTE_IS_LADDER);
	modelinfo.SetDoesNotProvideAICover(true);
	 
	vBottom.x = def.m_bottom.x;
	vBottom.y = def.m_bottom.y;
	vBottom.z = def.m_bottom.z;

	vTop.x = def.m_top.x;
	vTop.y = def.m_top.y;	
	vTop.z = def.m_top.z;

	vNormal.x = def.m_normal.x;
	vNormal.y = def.m_normal.y;
	vNormal.z = def.m_normal.z;

	ladderdef = def.m_template;
	bCanGetOffAtTop = def.m_canGetOffAtTop;

	// The (x,y) coords have to be the same or very similar (ladders must be vertical).
	Assertf(AreNearlyEqual(vBottom.x,vTop.x,1.0e-3f) && AreNearlyEqual(vBottom.y, vTop.y,1.0e-3f), "Ladder attached to %s is not vertical", archetype->GetModelName());
}

void CAudioCollisionInfo::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	static ModelAudioCollisionSettings * defaultSettings = NULL;
	if(!defaultSettings)
	{
		defaultSettings = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(s_defaultAudioModelCollisions);
	}

	BANK_ONLY(if(!PARAM_audiotag.Get()))
	{
		C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
		const CExtensionDefAudioCollisionSettings& def = *smart_cast<const CExtensionDefAudioCollisionSettings*>(definition);

		settings = NULL;

		audMetadataObjectInfo objectInfo;
		if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(def.m_settings, objectInfo) && objectInfo.GetType() == ModelAudioCollisionSettings::TYPE_ID)
		{
			settings = audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(def.m_settings);
		}

		if(!settings )
		{
			settings = defaultSettings;
		}
	}
#if __BANK
	else
	{
		settings = defaultSettings;
	}
#endif
}

void CAudioAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefAudioEmitter& def = *smart_cast<const CExtensionDefAudioEmitter*>(definition);

	qx = def.m_offsetRotation.x;
	qy = def.m_offsetRotation.y;
	qz = def.m_offsetRotation.z;
	qw = def.m_offsetRotation.w;

	effectindex = static_cast<s32>(def.m_effectHash);
}

void CSpawnPoint::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefSpawnPoint& def = *smart_cast<const CExtensionDefSpawnPoint*>(definition);

	s32 scenariotype = SCENARIOINFOMGR.GetScenarioIndex(def.m_spawnType, true, true);
#if __ASSERT
	Vector3 vPos;
	GetPos(vPos);
#endif

	if(!taskVerifyf(scenariotype!=-1,"Unknown scenario type (%s) at coords (%.2f, %.2f, %.2f) [%d][0x%08X]", def.m_spawnType.GetCStr(), vPos.x, vPos.y, vPos.z, def.m_spawnType.GetHash() , def.m_spawnType.GetHash() ))
	{
		// This will arbitrarily use type 0 instead, which is not going to be correct, but should avoid a crash.
		scenariotype = 0;
	}
	iType = (unsigned)scenariotype;

	if(def.m_offsetRotation.z == 1)
	{
		m_fDirection = PI;
	}
	else if(def.m_offsetRotation.w == 1)
	{
		m_fDirection = 0.0f;
	}
	else
	{
		float fAtan2X = 2.0f * (def.m_offsetRotation.z * def.m_offsetRotation.w - def.m_offsetRotation.x * def.m_offsetRotation.y);
		float fAtan2Y = 1.0f - 2.0f * (def.m_offsetRotation.z * def.m_offsetRotation.z + def.m_offsetRotation.x * def.m_offsetRotation.x);
		m_fDirection = rage::Atan2f(fAtan2X, fAtan2Y);
	}

	iUses = 0;
	Assign(iTimeStartOverride, def.m_start);
	Assign(iTimeEndOverride, def.m_end);
	// These are u8's, this cannot fail: Assert(def.m_start < 256 && def.m_end < 256);
	iInteriorState = CSpawnPoint::IS_Unknown;
	unsigned int avail = def.m_availableInMpSp;

	SetTimeTillPedLeaves(def.m_timeTillPedLeaves);

	Assertf(avail == CSpawnPoint::kBoth || avail == CSpawnPoint::kOnlyMp || avail == CSpawnPoint::kOnlySp, "Bad PSO value (%s)", def.m_spawnType.GetCStr());
	iAvailableInMpSp = avail;
	Assert(iAvailableInMpSp == avail);	// Catch overflow

	Assign(iScenarioGroup, SCENARIOPOINTMGR.FindGroupByNameHash(def.m_group));

	m_Flags = def.m_flags;
	m_bExtendedRange = def.m_extendedRange;
	m_bShortRange = def.m_shortRange;
	m_bHighPriority = def.m_highPri;

	Assign(m_uiModelSetHash, CScenarioManager::LegacyDefaultModelSetConvert(def.m_pedType));
	if( m_uiModelSetHash!=CScenarioManager::MODEL_SET_USE_POPULATION &&
		CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(CAmbientModelSetManager::kPedModelSets, m_uiModelSetHash)==-1)
	{
		if( archetype )
		{
			taskAssertf(0,	"Scenario (%s) on object (%s) has unknown model override (%s)", def.m_spawnType.GetCStr(), archetype->GetModelName(), def.m_pedType.GetCStr() );
		}
		else
		{
			taskAssertf(0,	"Scenario (%s) at coords (%.2f, %.2f, %.2f) has unknown model override (%s)", def.m_spawnType.GetCStr(), vPos.x, vPos.y, vPos.z, def.m_pedType.GetCStr() );
		}
		m_uiModelSetHash = CScenarioManager::MODEL_SET_USE_POPULATION;
	}

#if __BANK
	bEditable = false;
	bFree = false;
#endif // __BANK
}

void CSpawnPoint::CopyToDefinition(fwExtensionDef * definition)
{
	C2dEffect::CopyToDefinition(definition);
	CExtensionDefSpawnPoint& def = *smart_cast<CExtensionDefSpawnPoint*>(definition);

	def.m_spawnType = SCENARIOINFOMGR.GetHashForScenario(iType);

	Quaternion q;
	q.FromRotation(ZAXIS, m_fDirection);
	def.m_offsetRotation.Set(q.xyzw);

	def.m_start = iTimeStartOverride;
	def.m_end = iTimeEndOverride;
	def.m_availableInMpSp = (AvailabilityMpSp)iAvailableInMpSp;
	//mjc - fix iInteriorState

	def.m_pedType = m_uiModelSetHash;

	def.m_timeTillPedLeaves = GetTimeTillPedLeaves();

	def.m_extendedRange = m_bExtendedRange;
	def.m_shortRange = m_bShortRange;
	def.m_highPri = m_bHighPriority;
	def.m_flags = m_Flags;

	if(iScenarioGroup != CScenarioPointManager::kNoGroup)
	{
		def.m_group = SCENARIOPOINTMGR.GetGroupByIndex(iScenarioGroup - 1).GetNameHashString();
	}
	else
	{
		def.m_group.Clear();
	}
}

void CSpawnPoint::Reset()
{
	SetPos(ORIGIN);
	m_fDirection = 0.0f;
	iUses = 0;
	iInteriorState = CSpawnPoint::IS_Unknown;
	m_uiModelSetHash = CScenarioManager::MODEL_SET_USE_POPULATION;
	iType = 0;

	iScenarioGroup = CScenarioPointManager::kNoGroup;
	iTimeStartOverride = 0;
	iTimeEndOverride = 24;
	iAvailableInMpSp = kBoth;
	iTimeTillPedLeaves = kTimeTillPedLeavesNone;

	m_bShortRange = m_bExtendedRange = m_bHighPriority = false;
	m_Flags.Reset();

#if __BANK
	bEditable = false;
	bFree = false;
#endif // __BANK
}


float CSpawnPoint::GetTimeTillPedLeaves() const
{
	if(iTimeTillPedLeaves == kTimeTillPedLeavesInfinite)
	{
		return -1.0f;
	}
	return (float)iTimeTillPedLeaves;
}


void CSpawnPoint::SetTimeTillPedLeaves(float timeInSeconds)
{
	if(timeInSeconds < 0.0f)
	{
		// Any negative number is interpreted as infinite.
		iTimeTillPedLeaves = (u8)kTimeTillPedLeavesInfinite;
		return;
	}
	else if(timeInSeconds <= SMALL_FLOAT)
	{
		// No override, use what's in CScenarioInfo.
		iTimeTillPedLeaves = kTimeTillPedLeavesNone;
		return;
	}

	// If it's a small but non-zero value, we wouldn't want it to be stored as zero (kTimeTillPedLeavesNone).
	timeInSeconds = Max(timeInSeconds, 0.5f);

	// Round, convert to integer, and clamp.
	const unsigned int rounded = (unsigned int)floorf(timeInSeconds + 0.5f);
	const unsigned int clamped = Min(rounded, (unsigned int)kTimeTillPedLeavesMax);

	// Note: I was considering a more sophisticated representation, using
	// different quantization for different ranges of values. In the end, unless we
	// really need large values, high precision, or a few more bits for other things,
	// it didn't seem to be worth the extra code complexity.

	iTimeTillPedLeaves = (u8)clamped;
}


void CLightShaft::Init()
{
	m_corners[0]        = Vec3V(V_ZERO);
	m_corners[1]        = Vec3V(V_ZERO);
	m_corners[2]        = Vec3V(V_ZERO);
	m_corners[3]        = Vec3V(V_ZERO);
	m_direction         = Vec3V(V_Z_AXIS_WZERO)*ScalarV(V_NEGONE);
	m_directionAmount   = 0.0f;
	m_length            = 1.0f;
	m_fadeInTimeStart   = 0.0f;
	m_fadeInTimeEnd     = 0.0f;
	m_fadeOutTimeStart  = 0.0f;
	m_fadeOutTimeEnd    = 0.0f;
	m_fadeDistanceStart = 0.0f;
	m_fadeDistanceEnd   = 0.0f;
	m_colour            = Vec4V(V_ONE);
	m_intensity         = 1.0f;
	m_flashiness        = FL_CONSTANT;
	m_flags             = LIGHTSHAFTFLAG_DEFAULT;
	m_densityType       = LIGHTSHAFT_DENSITYTYPE_CONSTANT;
	m_volumeType        = LIGHTSHAFT_VOLUMETYPE_SHAFT;
	m_softness          = 0.0f;

#if __BANK
	m_attrExt.m_drawNormalDirection = false;
	m_attrExt.m_scaleByInteriorFill = true;
	m_attrExt.m_gradientAlignToQuad = false;
	m_attrExt.m_gradientColour      = Vec3V(V_ONE);
	m_attrExt.m_shaftOffset         = Vec3V(V_ZERO);
	m_attrExt.m_shaftRadius         = 1.0f;
	m_entity = NULL;
#endif // __BANK
}

void CLightShaftAttr::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	C2dEffect::InitArchetypeExtensionFromDefinition(definition, archetype);
	const CExtensionDefLightShaft& def = *smart_cast<const CExtensionDefLightShaft*>(definition);

	m_data.m_corners[0]        = RCC_VEC3V(def.m_cornerA);
	m_data.m_corners[1]        = RCC_VEC3V(def.m_cornerB);
	m_data.m_corners[2]        = RCC_VEC3V(def.m_cornerC);
	m_data.m_corners[3]        = RCC_VEC3V(def.m_cornerD);
	m_data.m_direction         = RCC_VEC3V(def.m_direction);
	m_data.m_directionAmount   = def.m_directionAmount;
	m_data.m_length            = def.m_length;
	m_data.m_fadeInTimeStart   = def.m_fadeInTimeStart;
	m_data.m_fadeInTimeEnd     = def.m_fadeInTimeEnd;
	m_data.m_fadeOutTimeStart  = def.m_fadeOutTimeStart;
	m_data.m_fadeOutTimeEnd    = def.m_fadeOutTimeEnd;
	m_data.m_fadeDistanceStart = def.m_fadeDistanceStart;
	m_data.m_fadeDistanceEnd   = def.m_fadeDistanceEnd;
	m_data.m_colour            = def.m_color.GetRGBA();
	m_data.m_intensity         = Abs<float>(def.m_intensity); // some light shafts has negative intensity, but this makes no sense
	m_data.m_flashiness        = def.m_flashiness;
	m_data.m_flags             = def.m_flags;
	m_data.m_densityType       = def.m_densityType;
	m_data.m_volumeType        = def.m_volumeType;
	m_data.m_softness          = def.m_softness;

	if (def.m_scaleBySunIntensity)
	{
		m_data.m_flags |= LIGHTSHAFTFLAG_SCALE_BY_SUN_INTENSITY;
	}

#if __BANK
	m_data.m_attrExt.Init();
	m_data.m_entity = NULL;
#endif // __BANK
}




#if __DEV
bool C2dEffect::m_PrintStats = false;
#endif


Vector3 C2dEffect::GetWorldPosition(const CEntity* pOwner) const
{
	Vector3 vPos;
	GetPos(vPos);
	if( pOwner )
	{
		vPos = pOwner->TransformIntoWorldSpace(vPos);
	}
	return vPos;
}

#if __BANK
void C2dEffect::FindDebugColourAndText(Color32 *col, char *string, bool bSecondText, CEntity* /*pAttachedEntity*/)
{
	switch(GetType())
	{
		case ET_LIGHT:
			col->Set(255, 255, 0, 255);
			if (bSecondText)
			{
				sprintf(string, "Size:%.1f Hdr:%.1f", this->GetLight()->m_falloff, this->GetLight()->m_intensity);
			}
			else
			{
				strcpy(string, "Light");
			}
			break;
		case ET_PARTICLE:
			col->Set(255, 0, 0, 255);
			strcpy(string, "Particle");
			break;
		case ET_DECAL:
			col->Set(0, 255, 0, 255);
			strcpy(string, "Decal");
			break;
		case ET_WINDDISTURBANCE:
			col->Set(0, 0, 255, 255);
			strcpy(string, "Wind Disturbance");
			break;
		case ET_COORDSCRIPT:
			col->Set(255, 0, 255, 255);
			sprintf(string, "Script Trigger (%s)", this->GetWorldPoint()->m_scriptName.GetCStr());
			break;
		case ET_SCROLLBAR:
			col->Set(255, 255, 128, 255);
			strcpy(string, "Scrollbar");
			break;
		case ET_AUDIOEFFECT:
			col->Set(0, 128, 255, 255);
			strcpy(string, "AudioEffect");
			break;
		case ET_EXPLOSION:
			col->Set(0, 0, 128, 255);
			strcpy(string, "Explosion");
			break;
		case ET_PROCEDURALOBJECTS:
			col->Set(128, 128, 0, 255);
			strcpy(string, "Procedural");
			break;
		case ET_LADDER:
			col->Set(128, 0, 128, 255);
			strcpy(string, "ladder");
			break;
		case ET_SPAWN_POINT:
			col->Set(255,0,0,255);
			sprintf(string, "Deprecated Sp:%s should be using scenario points", CScenarioManager::GetScenarioName((s32)this->GetSpawnPoint()->iType));
			//Deprecated ... should be using scenariopoints
//			col->Set(150,222,209,255); //egg shell blue B* 316623
// 			if( pAttachedEntity && pAttachedEntity->GetIsTypeObject() )
// 			{
// 				sprintf(string, "Sp:%s, %d", CScenarioManager::GetScenarioName((s32)this->GetSpawnPoint()->iType), ((CObject*)pAttachedEntity)->Get2dEffectUsage(this));
// 			}
// 			else
// 			{
// 				sprintf(string, "Sp:%s, %d", CScenarioManager::GetScenarioName((s32)this->GetSpawnPoint()->iType), this->GetSpawnPoint()->iUses);
// 			}
// 			if( pAttachedEntity && !pAttachedEntity->m_nFlags.bWillSpawnPeds )
// 			{
// 				strcat( string, " Disabled" );
// 			}

			break;
		case ET_LIGHT_SHAFT:
			col->Set(128, 0, 255, 255);
			strcpy(string, "Lightshaft");
			break;
		default:
			col->Set(64, 64, 64, 255);
			strcpy(string, "Unknown");
			break;
	}
}
#endif // __BANK

#if __BANK
static const char *effectNames[] = {
	"ET_LIGHT",
	"ET_PARTICLE",
	"ET_EXPLOSION",
	"ET_DECAL",
	"ET_WINDDISTURBANCE",
	"NOT USED",
	"NOT USED",
	"NOT USED",
	"NOT USED",
	"NOT USED",
	"NOT_USED",
	"NOT USED",
	"ET_PROCEDURALOBJECTS",
	"ET_COORDSCRIPT",
	"ET_LADDER",
	"ET_AUDIOEFFECT",
	"NOT USED",
	"ET_SPAWN_POINT",
	"ET_LIGHT_SHAFT",
	"ET_SCROLLBAR",
	"NOT USED",
	"NOT_USED",
	"ET_BUOYANCY",
	"ET_EXPRESSION",
	"ET_AUDIOCOLLISION"
};
CompileTimeAssert(NELEM(effectNames) == ET_MAX_2D_EFFECTS);
#endif	// __BANK

#if __DEV
s32 gNumber, gType;

void C2dEffect::Count2dEffects()
{
	if (gType == GetType())
	{
		gNumber++;
	}
}


void C2dEffect::Count2dEffects(s32 type, s32 &num, s32 &numWorld)
{
	gNumber = 0;
	num = numWorld = 0;
	gType = type;

	num = gNumber;
	gNumber = 0;
	numWorld = CModelInfo::GetWorld2dEffectArray().GetCount();
}

void C2dEffect::PrintDebugStats()
{
	s32 num, numWorld;

	grcDebugDraw::AddDebugOutput("*** 2dEffects   worldusage:%d/%d\n",
		CModelInfo::GetWorld2dEffectArray().GetCount(), CModelInfo::GetWorld2dEffectArray().GetCapacity() );

	for (s32 type = 0; type <= ET_MAX_2D_EFFECTS; type++)
	{
		Count2dEffects(type, num, numWorld);
		if (num != 0 || numWorld != 0)
		{
			grcDebugDraw::AddDebugOutput("%s    usage:%d   worldusage:%d\n", effectNames[type], num, numWorld);

		}
	}
}

#endif

Vector3 CSpawnPoint::GetSpawnPointDirection(CEntity* pOwner) const
{
	Vector3 vDir(0.0f, 1.0f, 0.0f);
	vDir.RotateZ(m_fDirection);
	if( pOwner )
	{
		vDir = VEC3V_TO_VECTOR3(pOwner->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDir)));
	}
	return vDir;
}

void CExpressionExtension::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	fwExtension::InitArchetypeExtensionFromDefinition( definition, archetype );
	const CExtensionDefExpression&	expressionDef = *smart_cast< const CExtensionDefExpression* >( definition );

	m_expressionDictionaryName = expressionDef.m_expressionDictionaryName;
	m_expressionName = expressionDef.m_expressionName;
	m_creatureMetadataName = expressionDef.m_creatureMetadataName;
}

#if	__BANK

bool	C2dEffectDebugRenderer::m_bEnabled = false;
float	C2dEffectDebugRenderer::m_fDrawRadius = 50.0f;
bool	C2dEffectDebugRenderer::m_bRenderEntity2dEffects = true;
bool	C2dEffectDebugRenderer::m_bRenderWorld2dEffects = true;

bool	C2dEffectDebugRenderer::m_bRenderDummy2dEffects = true;			// Not set able in RAG, just for debugging.
bool	C2dEffectDebugRenderer::m_bRenderObject2dEffects = true;
bool	C2dEffectDebugRenderer::m_bRenderBuilding2dEffects = true;
bool	C2dEffectDebugRenderer::m_bRenderABuilding2dEffects = true;

void C2dEffectDebugRenderer::InitWidgets(bkBank& bank)
{
	bank.PushGroup("2D Effect Debug");
	bank.AddToggle("Enable 2D Effect Rendering", &m_bEnabled);
	bank.AddSlider("Include Radius", &m_fDrawRadius, 1.0f, 10000.0f, 1.0f);
	bank.AddToggle("Render Entity attached 2dEffects", &m_bRenderEntity2dEffects);
	bank.AddToggle("Render World 2dEffects", &m_bRenderWorld2dEffects);
	bank.AddButton("Dump 2dEffect Usage Counts", &Tally2DEffects );
	bank.PopGroup();
}

void C2dEffectDebugRenderer::Update()
{
	if( m_bEnabled )
	{
		// Set about drawing them
		DebugDraw2DEffects();
	}
}

void C2dEffectDebugRenderer::DebugDraw2DEffects()
{

	// Render 2dEffects that are attached to "things"
	if(m_bRenderEntity2dEffects)
	{
		// DUMMY OBJECTS
		if(m_bRenderDummy2dEffects)
		{
			CDummyObject::Pool* pPool = CDummyObject::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pEntity = pPool->GetSlot(i);
				if(pEntity)
				{
					const bool	alreadyInWorld = pEntity->GetIsRetainedByInteriorProxy() || pEntity->GetOwnerEntityContainer() != NULL;

					if( !alreadyInWorld )
					{
						CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
						if(pModelInfo)
						{
							GET_2D_EFFECTS(pModelInfo);
							for(int j=0;j<numEffects;j++)
							{
								C2dEffect *pEffect = a2dEffects[j];
								Render2dEffect(pEntity, pEffect);
							}
						}
					}
				}
			}
		}

		// OBJECTS
		if(m_bRenderObject2dEffects)
		{
			CObject::Pool* pPool = CObject::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pEntity = pPool->GetSlot(i);
				if(pEntity)
				{
					CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
					if(pModelInfo)
					{
						GET_2D_EFFECTS(pModelInfo);
						for(int j=0;j<numEffects;j++)
						{
							C2dEffect *pEffect = a2dEffects[j];
							Render2dEffect(pEntity, pEffect);
						}
					}
				}
			}
		}

		// BUILDINGS
		if(m_bRenderBuilding2dEffects)
		{	
			CBuilding::Pool* pPool = CBuilding::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pEntity = pPool->GetSlot(i);
				if(pEntity)
				{
					CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
					if(pModelInfo)
					{
						GET_2D_EFFECTS(pModelInfo);
						for(int j=0;j<numEffects;j++)
						{
							C2dEffect *pEffect = a2dEffects[j];
							Render2dEffect(pEntity, pEffect);
						}
					}
				}
			}
		}

		// ANIMATED BUILDINGS
		if(m_bRenderABuilding2dEffects)
		{	
			CAnimatedBuilding::Pool* pPool = CAnimatedBuilding::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pEntity = pPool->GetSlot(i);
				if(pEntity)
				{
					CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
					if(pModelInfo)
					{
						GET_2D_EFFECTS(pModelInfo);
						for(int j=0;j<numEffects;j++)
						{
							C2dEffect *pEffect = a2dEffects[j];
							Render2dEffect(pEntity, pEffect);
						}
					}
				}
			}
		}


	}

	// Render world 2dEffects
	if(m_bRenderWorld2dEffects)
	{
		for (int i = 0; i < CModelInfo::GetWorld2dEffectArray().GetCount(); i++)
		{
			C2dEffect *pEffect = CModelInfo::GetWorld2dEffectArray()[i];

			if(pEffect)
			{
				Render2dEffect(NULL, pEffect);
			}
		}
	}
}


void C2dEffectDebugRenderer::Render2dEffect(CEntity *pEntity, C2dEffect *pEffect)
{
	Vector3 camPos = VEC3V_TO_VECTOR3(gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition());
	Vector3 position = pEffect->GetWorldPosition(pEntity);

	float dist2 = (position - camPos).Mag2();
	if( dist2 < (m_fDrawRadius*m_fDrawRadius))
	{
		// Draw the "coordinate frame"
		DrawCoordinateFrame(position);

		// Draw the associated text
		Vector3 textPos = position + ZAXIS;
		for(int i=0;i<1;i++)	// Loop for second string, but only used for one type, so not printed.
		{
			char string[128];
			Color32 Colour;
			pEffect->FindDebugColourAndText(&Colour, string, false, pEntity);
			grcDebugDraw::Text(textPos, Colour, 0, 0, string);
		}
	}
}

void C2dEffectDebugRenderer::DrawCoordinateFrame(Vector3 &pos)
{
	grcDebugDraw::Line(pos, pos + XAXIS, Color_red, Color_red4);
	grcDebugDraw::Line(pos, pos + YAXIS, Color_green, Color_green4);
	grcDebugDraw::Line(pos, pos + ZAXIS, Color_blue, Color_blue4);
}


atFixedArray<int, ET_MAX_2D_EFFECTS>	C2dEffectDebugRenderer::m_TallyCounts;

void C2dEffectDebugRenderer::Tally2DEffects()
{
	m_TallyCounts.Reset();
	m_TallyCounts.Resize(ET_MAX_2D_EFFECTS);
	for(int i=0;i<ET_MAX_2D_EFFECTS;i++)
	{
		m_TallyCounts[i]=0;
	}

	// DUMMY OBJECTS (only count ones not instantiated in the world, or don't include them?)
	{
		CDummyObject::Pool* pPool = CDummyObject::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			CEntity *pEntity = pPool->GetSlot(i);
			if(pEntity)
			{
				const bool	alreadyInWorld = pEntity->GetIsRetainedByInteriorProxy() || pEntity->GetOwnerEntityContainer() != NULL;
				if( !alreadyInWorld )
				{
					CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
					if(pModelInfo)
					{
						GET_2D_EFFECTS(pModelInfo);
						for(int j=0;j<numEffects;j++)
						{
							C2dEffect *pEffect = a2dEffects[j];
							TallyEffect(pEffect);
						}
					}
				}
			}
		}
	}

	// OBJECTS
	{
		CObject::Pool* pPool = CObject::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			CEntity *pEntity = pPool->GetSlot(i);
			if(pEntity)
			{
				CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
				if(pModelInfo)
				{
					GET_2D_EFFECTS(pModelInfo);
					for(int j=0;j<numEffects;j++)
					{
						C2dEffect *pEffect = a2dEffects[j];
						TallyEffect(pEffect);
					}
				}
			}
		}
	}

	// BUILDINGS
	{
		CBuilding::Pool* pPool = CBuilding::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			CEntity *pEntity = pPool->GetSlot(i);
			if(pEntity)
			{
				CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
				if(pModelInfo)
				{
					GET_2D_EFFECTS(pModelInfo);
					for(int j=0;j<numEffects;j++)
					{
						C2dEffect *pEffect = a2dEffects[j];
						TallyEffect(pEffect);
					}
				}
			}
		}
	}

	// ANIMATED BUILDINGS
	{
		CAnimatedBuilding::Pool* pPool = CAnimatedBuilding::GetPool();
		for(int i = 0;i<pPool->GetSize(); i++)
		{
			CEntity *pEntity = pPool->GetSlot(i);
			if(pEntity)
			{
				CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
				if(pModelInfo)
				{
					GET_2D_EFFECTS(pModelInfo);
					for(int j=0;j<numEffects;j++)
					{
						C2dEffect *pEffect = a2dEffects[j];
						TallyEffect(pEffect);
					}
				}
			}
		}
	}

	// World 2D effects
	for (int i = 0; i < CModelInfo::GetWorld2dEffectArray().GetCount(); i++)
	{
		C2dEffect *pEffect = CModelInfo::GetWorld2dEffectArray()[i];
		if(pEffect)
		{
			TallyEffect(pEffect);
		}
	}
	DumpEffectTally();
}

void C2dEffectDebugRenderer::TallyEffect(C2dEffect *pEffect)
{
	int idx = pEffect->GetType();
	m_TallyCounts[idx]++;
}

void C2dEffectDebugRenderer::DumpEffectTally()
{
	for(int i=0;i<ET_MAX_2D_EFFECTS;i++)
	{
		int	effectSize = GetEffectSize((u8)i);

		if(effectSize != 0)
		{
			Displayf("C2dEffect::%s - %d entries (%d bytes * %d == %d)", effectNames[i], m_TallyCounts[i], effectSize, m_TallyCounts[i], m_TallyCounts[i] * effectSize );
		}
	}
}

int	C2dEffectDebugRenderer::GetEffectSize(u8 type)
{
	switch(type)
	{
	case ET_LIGHT:
		return sizeof(CLightAttr);
		break;
	case ET_PARTICLE:
		return sizeof(CParticleAttr);
		break;
	case ET_DECAL:
		return sizeof(CDecalAttr);
		break;
	case ET_WINDDISTURBANCE:
		return sizeof(CWindDisturbanceAttr);
		break;
	case ET_COORDSCRIPT:
		return sizeof(CWorldPointAttr);
		break;
	case ET_SCROLLBAR:
		return sizeof(CScrollBarAttr);
		break;
	case ET_AUDIOEFFECT:
		return sizeof(CAudioAttr);
		break;
	case ET_EXPLOSION:
		return sizeof(CExplosionAttr);
		break;
	case ET_PROCEDURALOBJECTS:
		return sizeof(CProcObjAttr);
		break;
	case ET_LADDER:
		return sizeof(CLadderInfo);
		break;
	case ET_SPAWN_POINT:
		return sizeof(CSpawnPoint);
		break;
	case ET_LIGHT_SHAFT:
		return sizeof(CLightShaftAttr);
		break;
	default:
		return 0;
		break;
	}
}

#endif	//__BANK
