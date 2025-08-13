// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage headers
#include "grcore/debugdraw.h"

#include "fwmaths/random.h"
#include "fwsys/timer.h"

// game headers
#include "audio/radioaudioentity.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/replay.h"
#include "debug/Rendering/DebugLights.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/TiledScreenCapture.h"
#include "game/weather.h"
#include "game/Clock.h"
#include "Network/Network.h"
#include "peds/Ped.h"
#include "renderer/Mirrors.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/LightCulling.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/PostScan.h"
#include "audio/emitteraudioentity.h"
#include "scene/portals/InteriorInst.h"
#include "scene/world/gameworld.h"
#include "scene/world/VisibilityMasks.h"
#include "TimeCycle/TimeCycle.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Misc/LODLightManager.h"
#include "Vfx/Systems/VfxEntity.h"
#include "Vfx/VfxHelper.h"
#include "scene/loader/MapData_Extensions.h"
#include "fwscene/scan/ScanNodes.h"
#include "fwscene/scan/Scan.h"
#include "renderer/rendertargets.h"
#include "objects/DummyObject.h"
#include "Objects/Door.h"
#if __ASSERT
#include "diag/art_channel.h"
#endif
// =============================================================================================== //
// DEFINES
// =============================================================================================== //

//Temporarily force all shadows on for PC/NG, we really want art markup for this but thats not available
//at the moment.
#define SCENELIGHT_SHADOWS_FADE_DISTANCE	(50)

//Shadow blur stored as u8, use this to convert back to float in this range.
#define MAX_SHADOW_BLUR	(10)

RENDER_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_PAGEDPOOL(CLightEntity, CONFIGURED_FROM_FILE, 64, atHashString("LightEntity",0xca211161));

AUTOID_IMPL(CLightExtension);

XPARAM(randomseed);

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
bool CLightEntity::ms_enableFogVolumes = true;
#endif

static mthRandom RNG;

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void CLightExtension::InitEntityExtensionFromDefinition(const fwExtensionDef* definition, fwEntity* entity)
{
	fwExtension::InitEntityExtensionFromDefinition( definition, entity );
	const CExtensionDefLightEffect&	def = *smart_cast< const CExtensionDefLightEffect* >( definition );

	m_instances.Resize(def.m_instances.GetCount());

	for (int i = 0; i < def.m_instances.GetCount(); i++)
	{
		m_instances[i].Set(&def.m_instances[i]);
	}
}

// ----------------------------------------------------------------------------------------------- //

CLightAttr* CLightExtension::GetLightAttr(u8 hashCode)
{
	// Default value
	if (hashCode == 0)
	{
		return NULL;
	}

	for (int i = 0; i < m_instances.GetCount(); i++)
	{
		if (m_instances[i].m_lightHash == hashCode)
		{
			return &(m_instances[i]);
		}
	}

	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

CLightEntity::CLightEntity(const eEntityOwnedBy ownedBy)
	: CEntity( ownedBy )
{
	SetTypeLight();

	SetVisibilityType( VIS_ENTITY_LIGHT );
	GetRenderPhaseVisibilityMask().SetAllFlags( VIS_PHASE_MASK_GBUF | VIS_PHASE_MASK_REFLECTIONS );

	DisableCollision();
	SetBaseFlag(fwEntity::HAS_PRERENDER);
	SetBaseFlag(fwEntity::IS_LIGHT);

	m_parentEntity = NULL;

	Set2dEffectIdAndType(255, ET_NULLEFFECT);
	SetCustomFlags(LIGHTENTITY_FLAG_SCISSOR_LIGHT);

	m_overridenLightColor.SetBytes(0,0,0,0);

#if __BANK
	DebugLights::AddLightToGuidMap(this);
#endif
}

// ----------------------------------------------------------------------------------------------- //

CLightEntity::~CLightEntity()
{
#if __BANK
	DebugLights::RemoveLightFromGuidMap(this);
#endif
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::SetLightBounds(const CLightAttr* attr, const Vector3& pos, const Vector3 &dir)
{
	Assert(Get2dEffectType() == ET_LIGHT);

	// Do the same adjust done in shaders
	const float extraRadius = attr->m_falloff * EXTEND_LIGHT(attr->m_lightType);
	const float radius = attr->m_falloff + extraRadius;

	spdAABB newBoundBox;

	switch(attr->m_lightType)
	{
		case LIGHT_TYPE_SPOT:
		{
			const Vector3 adjustedPos = pos - dir * extraRadius;
			newBoundBox = fwBoxFromCone(adjustedPos, radius + extraRadius, dir, attr->m_coneOuterAngle * DtoR);
			break;
		}
		
		case LIGHT_TYPE_POINT:
		{
			Vector3 offset(radius, radius, radius);
			newBoundBox.Set(VECTOR3_TO_VEC3V(pos - offset), VECTOR3_TO_VEC3V(pos + offset));
			break;
		}

		case LIGHT_TYPE_CAPSULE:
		{
			const Vector3 pointA = pos + (dir * (attr->m_extents.x * 0.5f));
			const Vector3 pointB = pos - (dir * (attr->m_extents.x * 0.5f));
			
			ScalarV adjustedRadius = ScalarV(radius);
			const spdSphere capSphereA = spdSphere(RCC_VEC3V(pointA), adjustedRadius);
			const spdSphere capSphereB = spdSphere(RCC_VEC3V(pointB), adjustedRadius);

			newBoundBox.Invalidate();
			newBoundBox.GrowSphere(capSphereA);
			newBoundBox.GrowSphere(capSphereB);

			break;
		}

		default: 
			Assertf(false, "Tried to set bounds for light type we don't know (%d)", attr->m_lightType);
	}

	// Quick and dirty check if we need to update the entity descriptor
	Vec3V posDiff = Abs(Subtract(m_boundBox.GetCenter(), newBoundBox.GetCenter()));
	ScalarV totalDiff = Dot(posDiff, Vec3V(V_ONE));

	if (IsGreaterThanAll(totalDiff, ScalarV(V_QUARTER)))
	{
		RequestUpdateInWorld();
	}

	m_boundBox.SetMinPreserveUserData(newBoundBox.GetMin());
	m_boundBox.SetMaxPreserveUserData(newBoundBox.GetMax());
}

void CLightEntity::SetLightShaftBounds(const CLightShaftAttr* attr, bool addLightToScene)
{
	Assert(Get2dEffectType() == ET_LIGHT_SHAFT);
	Assert(m_parentEntity);

	const Mat34V matrix = m_parentEntity->GetMatrix();

	Vec3V corners[4];
	corners[0] = Transform(matrix, attr->m_data.m_corners[3]); // reversed
	corners[1] = Transform(matrix, attr->m_data.m_corners[2]);
	corners[2] = Transform(matrix, attr->m_data.m_corners[1]);
	corners[3] = Transform(matrix, attr->m_data.m_corners[0]);

	float shaftIntensity = attr->m_data.m_intensity;
	float shaftLength    = attr->m_data.m_length;

	if (attr->m_data.m_fadeInTimeStart != attr->m_data.m_fadeOutTimeEnd)
	{
		shaftIntensity *= Lights::CalculateTimeFade(
			attr->m_data.m_fadeInTimeStart,
			attr->m_data.m_fadeInTimeEnd,
			attr->m_data.m_fadeOutTimeStart,
			attr->m_data.m_fadeOutTimeEnd
		);

		if (shaftIntensity == 0.0f)
		{
			return;
		}
	}

	if (attr->m_data.m_fadeDistanceStart < attr->m_data.m_fadeDistanceEnd)
	{
		const grcViewport& grcVP = *gVpMan.GetUpdateGameGrcViewport();
		const Vec3V camPos = grcVP.GetCameraPosition();
		const Vec3V centre = (corners[0] + corners[1] + corners[2] + corners[3])*ScalarV(V_QUARTER);

		const float dist = Mag(centre - camPos).Getf();

		if (dist >= attr->m_data.m_fadeDistanceEnd)
		{
			return;
		}
		else if (dist > attr->m_data.m_fadeDistanceStart)
		{
			shaftIntensity *= (dist - attr->m_data.m_fadeDistanceStart)/(attr->m_data.m_fadeDistanceEnd - attr->m_data.m_fadeDistanceStart);
		}
	}

	bool lightOn = false;
	ApplyEffectsToLightCommon(attr->m_data.m_flashiness, RCC_MATRIX34(matrix), shaftIntensity, lightOn);

	if (!lightOn)
	{
		return;
	}

	// NOTE -- i'm using the stale sun value because this function can be called by LightEntityMgr::AddAttachedLights (called by CEntity::CreateDrawable)
	const Vec4V sun = g_timeCycle.GetStaleBaseDirLight();

	if (attr->m_data.m_flags & LIGHTSHAFTFLAG_SCALE_BY_SUN_INTENSITY)
	{
		shaftIntensity *= sun.GetWf();
	}

#if __BANK
	if (attr->m_data.m_attrExt.m_scaleByInteriorFill)
#endif // __BANK
	{
		CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation(GetInteriorLocation());

		if (pIntInst)
		{
			shaftIntensity *= pIntInst->GetDaylightFillScale();
		}
	}

	Vec3V shaftDir;

	if (attr->m_data.m_flags & LIGHTSHAFTFLAG_USE_SUN_LIGHT_DIRECTION)
	{
		const Vec3V sunDir = VECTOR3_TO_VEC3V(Lights::GetUpdateDirectionalLight().GetDirection());

		if (attr->m_data.m_directionAmount != 0.0f)
		{
			shaftDir = Transform3x3(matrix, attr->m_data.m_direction);
			shaftDir = Normalize(AddScaled(sunDir, shaftDir - sunDir, ScalarV(attr->m_data.m_directionAmount)));
		}
		else
		{
			shaftDir = sunDir;
		}
	}
	else
	{
		shaftDir = Transform3x3(matrix, attr->m_data.m_direction);
	}

#if __BANK
	Vec3V shaftOffset = DebugLighting::m_drawLightShaftsOffset;
	float shaftRadius = DebugLighting::m_LightShaftParams.z;

	shaftIntensity *= DebugLighting::m_LightShaftParams.x;
	shaftLength    *= DebugLighting::m_LightShaftParams.y;

	shaftOffset += attr->m_data.m_attrExt.m_shaftOffset;
	shaftRadius *= attr->m_data.m_attrExt.m_shaftRadius;

	if (attr->m_data.m_attrExt.m_drawNormalDirection BANK_ONLY(|| DebugLighting::m_drawLightShaftsNormal))
	{
		shaftDir = Normalize(Cross(corners[1] - corners[0], corners[2] - corners[0]));
	}

	const Vec3V centre = (corners[0] + corners[1] + corners[2] + corners[3])*ScalarV(V_QUARTER);

	corners[0] = centre + (corners[0] - centre)*ScalarV(shaftRadius) + shaftOffset;
	corners[1] = centre + (corners[1] - centre)*ScalarV(shaftRadius) + shaftOffset;
	corners[2] = centre + (corners[2] - centre)*ScalarV(shaftRadius) + shaftOffset;
	corners[3] = centre + (corners[3] - centre)*ScalarV(shaftRadius) + shaftOffset;
#endif // __BANK

	const Vec3V offset = shaftDir*ScalarV(shaftLength);
	spdAABB bound(corners[0], corners[0]);
	bound.GrowPoint(corners[1]);
	bound.GrowPoint(corners[2]);
	bound.GrowPoint(corners[3]);
	bound.GrowPoint(corners[0] + offset);
	bound.GrowPoint(corners[1] + offset);
	bound.GrowPoint(corners[2] + offset);
	bound.GrowPoint(corners[3] + offset);
	m_boundBox.SetMinPreserveUserData(bound.GetMin());
	m_boundBox.SetMaxPreserveUserData(bound.GetMax());

	if (addLightToScene && (shaftIntensity > 0.001f BANK_ONLY(|| DebugLights::m_debug)))
	{
		if (__BANK || (attr->m_data.m_flags & LIGHTSHAFTFLAG_USE_SUN_LIGHT_DIRECTION) != 0)
		{
			UpdateWorldFromEntity(); // need to update the entity in the world since the bounds might have changed
		}

		CLightShaft* shaft = Lights::AddLightShaft(BANK_ONLY(this));

		if (shaft != NULL)
		{
			shaft->m_corners[0]        = corners[0];
			shaft->m_corners[1]        = corners[1];
			shaft->m_corners[2]        = corners[2];
			shaft->m_corners[3]        = corners[3];
			shaft->m_direction         = shaftDir;
			shaft->m_directionAmount   = attr->m_data.m_directionAmount;
			shaft->m_length            = shaftLength;
			shaft->m_fadeInTimeStart   = attr->m_data.m_fadeInTimeStart;
			shaft->m_fadeInTimeEnd     = attr->m_data.m_fadeInTimeEnd;
			shaft->m_fadeOutTimeStart  = attr->m_data.m_fadeOutTimeStart;
			shaft->m_fadeOutTimeEnd    = attr->m_data.m_fadeOutTimeEnd;
			shaft->m_fadeDistanceStart = attr->m_data.m_fadeDistanceStart;
			shaft->m_fadeDistanceEnd   = attr->m_data.m_fadeDistanceEnd;
			shaft->m_colour            = attr->m_data.m_colour;
			shaft->m_intensity         = shaftIntensity/(32.0f*10.0f); // scale intensity by 1/32 to "match" old light shafts. And *sigh* another 1/10th to match the tuning that was done while the ps3 had a serious bug.
			shaft->m_flashiness        = attr->m_data.m_flashiness;
			shaft->m_flags             = attr->m_data.m_flags;
			shaft->m_densityType       = attr->m_data.m_densityType;
			shaft->m_volumeType        = attr->m_data.m_volumeType;
			shaft->m_softness          = attr->m_data.m_softness;

			Vec4V sunColour = Vec4V(V_ONE);

			if (attr->m_data.m_flags & LIGHTSHAFTFLAG_USE_SUN_LIGHT_COLOUR)
			{
				sunColour = sun;
				sunColour.SetW(ScalarV(V_ONE));
			}

			if (attr->m_data.m_flags & LIGHTSHAFTFLAG_SCALE_BY_SUN_COLOUR)
			{
				shaft->m_colour = attr->m_data.m_colour*sunColour;
			}
			else if (attr->m_data.m_flags & LIGHTSHAFTFLAG_USE_SUN_LIGHT_COLOUR)
			{
				shaft->m_colour = sunColour;
			}
			else
			{
				shaft->m_colour = attr->m_data.m_colour;
			}

#if __BANK
			shaft->m_attrExt = attr->m_data.m_attrExt;
			Assert(shaft->m_entity == this);

			if (this == DebugLights::GetIsolatedLightShaftEntity())
			{
				grcDebugDraw::AddDebugOutput("light shaft colour = %.3f.,%.3f,%.3f, intensity = %f", VEC3V_ARGS(shaft->m_colour), shaft->m_intensity);
			}
#endif // __BANK

			g_vfxEntity.UpdatePtFxLightShaft(this, shaft, Get2dEffectId());
		}
		else
		{
			Assertf(false, "We are trying to add more than %d light shafts", MAX_STORED_LIGHT_SHAFTS);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

Vector3	CLightEntity::GetBoundCentre() const
{
	return VEC3V_TO_VECTOR3(m_boundBox.GetCenter());
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::GetBoundCentre(Vector3& ret) const
{
	ret = VEC3V_TO_VECTOR3(m_boundBox.GetCenter());
}

// ----------------------------------------------------------------------------------------------- //

float CLightEntity::GetBoundRadius() const
{
	const ScalarV r = Mag(m_boundBox.GetMax() - m_boundBox.GetMin())*ScalarV(V_HALF);
	return r.Getf();
}

// ----------------------------------------------------------------------------------------------- //

float CLightEntity::GetBoundCentreAndRadius(Vector3& centre_) const
{
	const Vec3V boxMin = m_boundBox.GetMin();
	const Vec3V boxMax = m_boundBox.GetMax();
	const Vec3V centre = (boxMax + boxMin)*ScalarV(V_HALF);
	const Vec3V extent = (boxMax - boxMin)*ScalarV(V_HALF);

	centre_ = RCC_VECTOR3(centre);
	return Mag(extent).Getf();
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::GetBoundSphere(spdSphere& sphere) const
{
	const Vec3V boxMin = m_boundBox.GetMin();
	const Vec3V boxMax = m_boundBox.GetMax();
	const Vec3V centre = (boxMax + boxMin)*ScalarV(V_HALF);
	const Vec3V extent = (boxMax - boxMin)*ScalarV(V_HALF);

	sphere.Set(centre, Mag(extent));
}

// ----------------------------------------------------------------------------------------------- //

const Vector3& CLightEntity::GetBoundingBoxMin() const
{
	return m_boundBox.GetMinVector3();
}

// ----------------------------------------------------------------------------------------------- //

const Vector3& CLightEntity::GetBoundingBoxMax() const
{
	return m_boundBox.GetMaxVector3();
}

// ----------------------------------------------------------------------------------------------- //

FASTRETURNCHECK(const spdAABB &) CLightEntity::GetBoundBox(spdAABB& /*box*/) const
{
	return m_boundBox;
}

// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //


void CLightEntity::ProcessVisibleLight(const fwVisibilityFlags& visFlags)
{
#if __BANK
	if (TiledScreenCapture::IsEnabledOrthographic())
	{
		return;
	}
#endif // __BANK

	// the visFlags says in which render phases the light is visible.
	// Suppose you want to do some gbuf-specific processing you can do like this:
	//
	// CRenderPhase*	gbuf = ...;
	// if ( visFlags.GetVisBit( gbuf->GetRenderPhaseId ) )
	//   { ... }

	// As a performance optimization, we'll simply check to see if the parent entity 
	// has a draw handler pointer as a proxy for checking the drawable. Obtaining the 
	// drawable potentially incurs many cache misses, and shouldn't be necessary - 
	// existence of a draw handler should be an equivalent check. We'll assert that 
	// this assumption is correct.
	Assert( (m_parentEntity == NULL) || (m_parentEntity->GetDrawable() != NULL) == (m_parentEntity->GetDrawHandlerPtr() != NULL) );

	if (m_parentEntity != NULL && m_parentEntity->GetDrawHandlerPtr() && m_parentEntity->GetAlpha()

#if RSG_PC
		&& (m_parentEntity->GetModelNameHash()!=ATSTRINGHASH("SM_20_tun1_LOD", 0x5a0a5d24) || g_LodMgr.WithinVisibleRange(m_parentEntity, false))
#endif

		)
	{
		const bool addLightToScene = visFlags.GetVisBit(g_SceneToGBufferPhaseNew->GetEntityListIndex()) ||
									 visFlags.GetVisBit(CMirrors::GetMirrorRenderPhase()->GetEntityListIndex());

		ProcessLightSource(addLightToScene, true);
	}
}

// ----------------------------------------------------------------------------------------------- //

fwDrawData* CLightEntity::AllocateDrawHandler(rmcDrawable*)
{
	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

static bool FlashyLightOn(const u32 randomSeed)
{
	bool ApplyOnVersion = false;
	const u32 frameCount = CNetwork::GetSyncedTimeInMilliseconds() / 33;

	if ( (frameCount^(randomSeed<<3))&96)
	{
		ApplyOnVersion = true;
	}
	else
	{
		ApplyOnVersion = false;
	}

	// Make sure light doesn't flicker all the time
	if (((CNetwork::GetSyncedTimeInMilliseconds()>>11) ^ randomSeed) & 3) ApplyOnVersion = true;

	return ApplyOnVersion;
}

// ----------------------------------------------------------------------------------------------- //

float CLightEntity::CalcLightIntensity(const bool ignoreDayNightSettings, const CLightAttr* lightData)
{
	const u32 timeFlags = lightData->m_timeFlags;
	const u8 flashiness = lightData->m_flashiness;
	
	// Only activate lights at specific times
	float intensity = (!ignoreDayNightSettings) ? Lights::CalculateTimeFade(timeFlags) : 0.0f;

	//special case that forces this light type on if the road is over a certain wetness
	if(flashiness == FL_RANDOM_OVERRIDE_IF_WET)
	{
		if(g_weather.GetWetness() > 0.5f)
		{
			intensity = 1.0;
		}
	}

	return intensity;
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::AddCorona(const CLightSource &light, CLightAttr* lightData, bool bInteriorLocationValid)
{

	//early out if size/intensity is zero
	if(lightData->m_coronaSize <= 0.0f || light.GetIntensity() <= 0.0f || lightData->m_coronaIntensity <= 0.0f)
	{
		return;
	}

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();

	float sizeMult = 2.5f; // Let's just multiply all corona size by 2.5. #yolo.
	float intensityMult = light.GetIntensity();

	if (!bInteriorLocationValid BANK_ONLY(|| g_coronas.GetForceExterior()))
	{
		sizeMult       *= g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SPRITE_SIZE);
		intensityMult  *= g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_SPRITE_BRIGHTNESS);
	}

	u8 coronaFlags = 0;
	coronaFlags |= (light.GetType() == LIGHT_TYPE_SPOT) ? CORONA_HAS_DIRECTION : 0;

	//Enable the fadeoff Dir Mult only if the light has a distant Light
	//All lights in small category do not have distant lights
	const int lodLightCategory = Lights::CalculateLightCategory(
		(u8)light.GetType(), 
		light.GetRadius(), 
		light.GetIntensity(), 
		light.GetCapsuleExtent(), 
		lightData->m_flags);
	
	if(light.GetType() == LIGHT_TYPE_SPOT && lodLightCategory> Lights::LIGHT_CATEGORY_SMALL )
	{
		coronaFlags |= CORONA_FADEOFF_DIR_MULT;
	}
	coronaFlags |= ((lightData->m_flags & LIGHTFLAG_NOT_IN_REFLECTION) != 0) ? CORONA_DONT_REFLECT : 0;
	coronaFlags |= ((lightData->m_flags & LIGHTFLAG_ONLY_IN_REFLECTION) != 0) ? CORONA_ONLY_IN_REFLECTION : 0;
 
	Vec3V dir = VECTOR3_TO_VEC3V(light.GetDirection());
	Vec3V pos = VECTOR3_TO_VEC3V(light.GetPosition());

	float finalSize = 0.0f;
	float finalIntensity = 0.0f;
	float zBias = 0.0f;

	Vec4V fadeValues;
	Lights::CalculateFadeValues(&light, grcVP->GetCameraPosition(), fadeValues);
	float finalFadeValue = Lights::CalculateAdjustedLightFade(fadeValues.GetXf(), this, light, true);

	// Don't do corona adjust on interior lights as they dont' go to LOD lights
	if (light.GetFlag(LIGHTFLAG_INTERIOR_ONLY) || light.GetLightFadeDistance() > 0)
	{
		finalSize = lightData->m_coronaSize * sizeMult;
		intensityMult *= finalFadeValue;
	}
	else if (lodLightCategory == Lights::LIGHT_CATEGORY_SMALL)
	{
		const float range = CLODLights::GetUpdateLodLightRange(LODLIGHT_CATEGORY_SMALL);
		const float fadeRange = CLODLights::GetLodLightFadeRange(LODLIGHT_CATEGORY_SMALL);

		const ScalarV rangeSqrV = ScalarV(range * range);
		const ScalarV fadeRangeSqrV = ScalarV(fadeRange * fadeRange);

		const ScalarV distToCameraV = MagSquared(Subtract(pos, grcVP->GetCameraPosition()));

		ScalarV intensityMultAdj = Subtract( 
			distToCameraV,  
			Subtract(rangeSqrV, fadeRangeSqrV));
		intensityMultAdj = Saturate(InvScale(intensityMultAdj, fadeRangeSqrV));
		intensityMult *= 1.0f - intensityMultAdj.Getf();

		static dev_float threshold = 0.75;
		finalFadeValue = Saturate((finalFadeValue - threshold) / (1.0f - threshold));
		finalSize = Lerp(finalFadeValue, CLODLights::GetCoronaSize(), lightData->m_coronaSize) * sizeMult;
	}
	else
	{
		// Adjust corona size/intensity to match HD -> LOD light transition
		static dev_float threshold = 0.75;
		finalFadeValue = Saturate((finalFadeValue - threshold) / (1.0f - threshold));
		finalSize = Lerp(finalFadeValue, CLODLights::GetCoronaSize(), lightData->m_coronaSize) * sizeMult;
	}

	zBias = finalSize * g_coronas.GetZBiasFromFinalSizeMultiplier();
	finalIntensity = lightData->m_coronaIntensity * intensityMult;

	if (finalIntensity > 0.0f && finalSize > 0.0f)
	{
		g_coronas.Register(
			pos,
			finalSize,
			IsLightColorOverriden()? GetOverridenLightColor() : lightData->m_colour.GetColor32(),
			finalIntensity ,
			zBias,
			dir,
			1.0f,
			lightData->m_coneInnerAngle,
			lightData->m_coneOuterAngle,
			coronaFlags);
	}
}

// ----------------------------------------------------------------------------------------------- //

// generate a unique identifier for each shadowed light that does not change during the game.
fwUniqueObjId CLightEntity::CalcShadowTrackingID()
{
	const CEntity *parentPtr = GetParent();
	fwUniqueObjId shadowTrackingID = (fwUniqueObjId)parentPtr;

	if (parentPtr != NULL)
	{
		if (parentPtr->GetType()==ENTITY_TYPE_OBJECT)
		{
			if (CDummyObject * dummyPtr = ((CObject*)parentPtr)->GetRelatedDummy()) 
				shadowTrackingID = (fwUniqueObjId)dummyPtr;	// use the dummy ptr if we can, it does not change when CLEAR_AREA is called
		}	

		shadowTrackingID = shadowTrackingID<<4; // though away the top 4 bits, this gives us enough room so we don't bump into the next CObject, assuming Get2dEffectId() < 1216  (sizeof(CDummyObject)*16)
		shadowTrackingID += Get2dEffectId(); 
	}

	return shadowTrackingID;
}

// ----------------------------------------------------------------------------------------------- //

float CLightEntity::ProcessOne2dEffectLight(
	CLightSource& t_lightSource, 
	const Matrix34& mtx, 
	float intensityMult,
	fwInteriorLocation intLoc, 
	s32 texDictionary,
	const CEntityFlags &entityFlags,
	const u32 lightOverrideFlags,
	const CLightAttr* lightData)
{
	Assertf(lightData->m_falloff > 0.0f, "Attached to %s", m_parentEntity->GetModelName());

	const Color32 _lightColour = IsLightColorOverriden()? GetOverridenLightColor() : lightData->m_colour.GetColor32();
	
	Vector3 lightColour = Vector3(_lightColour.GetRedf(), _lightColour.GetGreenf(), _lightColour.GetBluef());

	float intensity = lightData->m_intensity * intensityMult;

	// Calculate direction and tangent (rotate correctly because light is attached)
	Vector3	tdir(0, 0, -1);
	Vector3	ttan(0, 1,  0);

	if (lightData->m_projectedTextureKey > 0 || lightData->m_lightType == LIGHT_TYPE_SPOT || lightData->m_lightType == LIGHT_TYPE_CAPSULE)
	{
		Vector3	ldir = lightData->m_direction.GetVector3();
		Vector3	ltan = lightData->m_tangent.GetVector3();

		if (ldir.Mag2() > 1.0f) ldir.Normalize();
		if (ltan.Mag2() > 1.0f) ltan.Normalize();

		mtx.Transform3x3(ldir, tdir);
		mtx.Transform3x3(ltan, ttan);
	}

	u32 flags = lightData->m_flags | lightOverrideFlags;
	if (!entityFlags.bLightsCanCastStaticShadows) { flags &= ~LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS; }
	if (!entityFlags.bLightsCanCastDynamicShadows) { flags &= ~LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS; }
	if ((flags & (LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS))) { flags |= LIGHTFLAG_CAST_SHADOWS; }
	if (lightData->m_projectedTextureKey != 0) { flags |= LIGHTFLAG_TEXTURE_PROJECTION; }

	if (flags & LIGHTFLAG_CALC_FROM_SUN)
	{
		//use hacked non-modified directional lights
		Vec4V vBaseDirLight = g_timeCycle.GetBaseDirLight();
		Vector4 TimeCycle_Directional = RCC_VECTOR4(vBaseDirLight);
		lightColour *= Vector3(TimeCycle_Directional.x, TimeCycle_Directional.y, TimeCycle_Directional.z);
		float dirScale = TimeCycle_Directional.w;
		intensity *= dirScale;
	}

	// Add the actual light
	float finalIntensity = intensity;

	t_lightSource.SetCommon((eLightType)lightData->m_lightType, flags, mtx.d, lightColour, finalIntensity, lightData->m_timeFlags);
	t_lightSource.SetTexture(lightData->m_projectedTextureKey, texDictionary);
	
	float radius = lightData->m_falloff;
	if( !intLoc.IsValid() BANK_ONLY(&& DebugLights::m_EnableLightFalloffScaling) )
	{
		radius *= g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_LIGHT_FALLOFF_MULT);
	}
	t_lightSource.SetRadius(radius);

	t_lightSource.SetInInterior(intLoc);

	float volumeIntensityScale = 1.0f;
	float volumeSizeScale = 1.0f;

#if LIGHT_VOLUMES_IN_FOG
	const float volumeRange = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_FOG_VOLUME_RANGE);
	const float fadeRange = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_FOG_VOLUME_FADE_RANGE);
	const float volumeIntensityScaleVal = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_FOG_VOLUME_INTENSITY_SCALE);
	const float volumeSizeScaleVal = g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_FOG_VOLUME_SIZE_SCALE);
	const bool enableFogVolumes = WIN32PC_SWITCH(ms_enableFogVolumes, true) && BANK_SWITCH(DebugLighting::m_FogLightVolumesEnabled, true);

	if (enableFogVolumes && volumeIntensityScaleVal > 0.0f && volumeSizeScaleVal > 0.0f)
	{
		const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
		const Vec3V lightPosition = VECTOR3_TO_VEC3V(mtx.d);
		const float distToLight = Mag(lightPosition - grcVP->GetCameraPosition()).Getf();
	
		if(!intLoc.IsValid() && 
		   distToLight < volumeRange && 
		   (eLightType)lightData->m_lightType == LIGHT_TYPE_SPOT && 
		   !t_lightSource.GetFlag(LIGHTFLAG_DRAW_VOLUME))
		{
			float distanceFade = (distToLight - (volumeRange - fadeRange)) / fadeRange;
			distanceFade = Clamp(distanceFade, 0.0f, 1.0f);
			t_lightSource.SetFlag(LIGHTFLAG_DRAW_VOLUME);
			flags |= LIGHTFLAG_DRAW_VOLUME;
			volumeIntensityScale = volumeIntensityScaleVal *(1.0f - distanceFade);
			volumeSizeScale = volumeSizeScaleVal;
		}
	}
#endif //LIGHT_VOLUMES_IN_FOG

	if (flags & LIGHTFLAG_DRAW_VOLUME)
	{
		const float lightFade = (float)m_parentEntity->GetAlpha() / 255.0f;

		t_lightSource.SetLightVolumeIntensity(
			lightData->m_volIntensity * intensity * lightFade * volumeIntensityScale,
			lightData->m_volSizeScale * volumeSizeScale,
			Vec4V(VECTOR3_TO_VEC3V(lightData->m_volOuterColour.GetVector3()), ScalarV(lightData->m_volOuterIntensity)),
			lightData->m_volOuterExponent);
	}
	else
	{
		t_lightSource.SetLightVolumeIntensity(
			0.0f,
			0.0f,
			Vec4V(V_ZERO),
			1.0f);
	}

	if ((flags & LIGHTFLAG_CAST_SHADOWS) != 0)
	{
		t_lightSource.SetShadowTrackingId(CalcShadowTrackingID());
		t_lightSource.SetShadowNearClip(lightData->m_shadowNearClip);
		t_lightSource.SetShadowBlur(((float)lightData->m_shadowBlur / 255.0f) * MAX_SHADOW_BLUR);		
	}

	const fwBaseEntityContainer* entityContainer = GetOwnerEntityContainer();
	fwScreenQuadPair quadPair = fwScan::GetGbufScreenQuadPair(entityContainer->GetOwnerSceneNode());
	fwScreenQuad quad(SCREEN_QUAD_FULLSCREEN);
	bool validQuad0 = quadPair.GetScreenQuad0().IsValid();
	bool validQuad1 = quadPair.GetScreenQuad1().IsValid();
	bool quadIsValid = false;
	if (validQuad0 && validQuad1)
	{
		quad = quadPair.GetScreenQuad0();
		quad.Grow(quadPair.GetScreenQuad1());
		quadIsValid = true;
	}
	else if (validQuad0)
	{
		quad = quadPair.GetScreenQuad0();
		quadIsValid = true;
	}
	else if (validQuad1)
	{
		quad = quadPair.GetScreenQuad1();
		quadIsValid = true;
	}

	if (GetCustomFlag(LIGHTENTITY_FLAG_SCISSOR_LIGHT) && quadIsValid == true)
	{
		Vec4V scQd = quad.ToVec4V();

		scQd = (scQd * Vec4V(0.5f, -0.5f, 0.5f, -0.5f)) + Vec4V(V_HALF);

		float gameWidth  = (float)VideoResManager::GetSceneWidth();
		float gameHeight = (float)VideoResManager::GetSceneHeight();

		int scissorRectX	  = int(scQd[0] * gameWidth);
		int scissorRectY	  = int(scQd[3] * gameHeight);
		int scissorRectWidth  = int((scQd[2] - scQd[0]) * gameWidth);
		int scissorRectHeight = int((scQd[1] - scQd[3]) * gameHeight);

		t_lightSource.SetScissorRect(scissorRectX, scissorRectY, scissorRectWidth, scissorRectHeight);
	}
	else
	{
		t_lightSource.SetScissorRect(-1, -1, -1, -1);
	}

	t_lightSource.SetFalloffExponent(lightData->m_falloffExponent);
	t_lightSource.SetPlane(
		lightData->m_cullingPlane[0],
		lightData->m_cullingPlane[1],
		lightData->m_cullingPlane[2],
		lightData->m_cullingPlane[3],
		mtx);

	if (t_lightSource.GetType() == LIGHT_TYPE_SPOT)
	{
		t_lightSource.SetSpotlight(lightData->m_coneInnerAngle, lightData->m_coneOuterAngle);
		t_lightSource.SetDirTangent(tdir, ttan);
	}

	if (t_lightSource.GetType() == LIGHT_TYPE_CAPSULE)
	{
		t_lightSource.SetCapsule(lightData->m_extents.x);
		t_lightSource.SetDirTangent(tdir, ttan);
	}

	t_lightSource.SetLightFadeDistance(lightData->m_lightFadeDistance);
	t_lightSource.SetShadowFadeDistance(lightData->m_shadowFadeDistance);
	t_lightSource.SetSpecularFadeDistance(lightData->m_specularFadeDistance);
	t_lightSource.SetVolumetricFadeDistance(lightData->m_volumetricFadeDistance);

	SetLightBounds(lightData, mtx.d, t_lightSource.GetDirection());

	Assertf(t_lightSource.GetType() != LIGHT_TYPE_SPOT || t_lightSource.GetConeOuterAngle() > 0.0f, "Invalid Spotlight ConeOuterAngle (%f)", t_lightSource.GetConeOuterAngle());
	Assertf(t_lightSource.GetType() != LIGHT_TYPE_SPOT || t_lightSource.GetConeBaseRadius() != 0.0f, "Spot light GetConeBaseRadius() is 0.0");
	Assertf(lightData->m_falloff > 0.0f, "Light attached to %s has radius <= 0", m_parentEntity->GetModelName());

	return finalIntensity;
}


float CLightEntity::ProcessOne2dEffectLightCoronaOnly(
	CLightSource& t_lightSource, 
	const Matrix34& mtx, 
	float intensityMult,
	fwInteriorLocation intLoc,
	const u32 lightOverrideFlags,
	const CLightAttr* lightData)
{
	Assertf(lightData->m_falloff > 0.0f, "Attached to %s", m_parentEntity->GetModelName());

	float intensity = lightData->m_intensity * intensityMult;

	// Calculate direction and tangent (rotate correctly because light is attached)
	Vector3	tdir(0, 0, -1);
	Vector3	ttan(0, 1,  0);

	if (lightData->m_projectedTextureKey > 0 || lightData->m_lightType == LIGHT_TYPE_SPOT || lightData->m_lightType == LIGHT_TYPE_CAPSULE)
	{
		Vector3	ldir = lightData->m_direction.GetVector3();
		Vector3	ltan = lightData->m_tangent.GetVector3();

		if (ldir.Mag2() > 1.0f) ldir.Normalize();
		if (ltan.Mag2() > 1.0f) ltan.Normalize();

		mtx.Transform3x3(ldir, tdir);
		mtx.Transform3x3(ltan, ttan);
	}

	u32 flags = lightData->m_flags | lightOverrideFlags;

	if (flags & LIGHTFLAG_CALC_FROM_SUN)
	{
		//use hacked non-modified directional lights
		intensity *= g_timeCycle.GetBaseDirLight().GetWf();
	}

	t_lightSource.SetType((eLightType)lightData->m_lightType);
	t_lightSource.SetFlags(flags);
	t_lightSource.SetPosition(mtx.d);
	t_lightSource.SetIntensity(intensity);
	t_lightSource.SetTimeFlags(lightData->m_timeFlags);
	t_lightSource.SetLightFadeDistance(lightData->m_lightFadeDistance);

	if (t_lightSource.GetType() == LIGHT_TYPE_SPOT || t_lightSource.GetType() == LIGHT_TYPE_CAPSULE)
	{
		t_lightSource.SetDirTangent(tdir, ttan);
	}

	t_lightSource.SetInInterior(intLoc);

	return intensity;
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::ApplyEffectsToLight(const Matrix34 &mtx, float &intensityMult, bool &lightOn, u8 flashiness)
{
	ApplyEffectsToLightCommon(flashiness, mtx, intensityMult, lightOn);
}

void CLightEntity::ApplyEffectsToLightCommon(u8 flashiness, const Matrix34 &mtx, float &intensityMult, bool &lightOn)
{
	// fwRandom is relatively quite expensive. Previously, we would always set the random seed, 
	// then set it back, even if the particular light type didn't need to use it. Now, this is split 
	// into two switches - the first, for lights that don't need fwRandom, and the second for 
	// light types that do need it. Considering we often process 100-200 lights per frame, and most 
	// lights don't need to use fwRandom, this change speeds up the average cost of this function 
	// by over ~90%.

#if GTA_REPLAY
	bool replayDisableFlickerOnPause = false;
#endif

	// Process lights that don't need to use fwRandom, and then return.
	switch(flashiness)
	{
	case FL_CONSTANT:
		lightOn = true;
		return;

	case FL_OFF:
		return;

	case FL_UNUSED1:
		// PD_TODO: TEMPORARILY KILL ASSERT WHILE WE SORT OUT DATA.
		// Warningf("Light used with an undefined flashiness flag");
		return;

	case FL_ALARM:
		if ((CNetwork::GetSyncedTimeInMilliseconds() & 511) < 60)
		{
			lightOn = true;
		}
		return;

	case FL_ON_WHEN_RAINING:
		if (g_weather.GetRain() > 0.0001f)
		{
			intensityMult *= g_weather.GetRain();
			lightOn = true;
		}
		return;

	case FL_CYCLE_1:
	case FL_CYCLE_2:
	case FL_CYCLE_3:
		{
#define TOTALCYCLETIME (10000)
			s32	TimeInCycle = CNetwork::GetSyncedTimeInMilliseconds();
			TimeInCycle += (flashiness - FL_CYCLE_1) * (TOTALCYCLETIME / 3);
			TimeInCycle += (s32)(mtx.d.x * 20);
			TimeInCycle += (s32)(mtx.d.y * 10);
			TimeInCycle = TimeInCycle % TOTALCYCLETIME;
			s32	Zone = (TimeInCycle * 9) / TOTALCYCLETIME;
			float	ValueWithinZone = (TimeInCycle - Zone * (TOTALCYCLETIME / 9)) / float(TOTALCYCLETIME / 9);
			lightOn = true;
			switch (Zone)
			{
			case 0:		// fade in.
				intensityMult *= ValueWithinZone;
				break;
			case 1:
			case 2:
				intensityMult *= 1.0f;
				break;
			case 3:
				intensityMult *= 1.0f - ValueWithinZone;
				break;
			default:
				lightOn = false;
				break;
			}
		}
		return;
	}

	// Constants
	u16 randomSeed = 0;
	const u32 frameCount = CNetwork::GetSyncedTimeInMilliseconds() / 33;
	RNG.Reset(frameCount);

	// Process lights that do need to use fwRandom, then break, so we can reset the random seed after.
	switch(flashiness)
	{
	case FL_RANDOM:
	case FL_RANDOM_OVERRIDE_IF_WET:
		if(FlashyLightOn(randomSeed))
		{
			lightOn = true;		
		}
		break;

	case FL_ONCE_SECOND:
		if ((CNetwork::GetSyncedTimeInMilliseconds() + (randomSeed & 7) * (1024 / 8) ) & 512)
		{
			lightOn = true;
		}
		break;

	case FL_TWICE_SECOND:
		if ((CNetwork::GetSyncedTimeInMilliseconds() + (randomSeed & 7) * (2048 / 8) ) & 1024)
		{
			lightOn = true;
		}
		break;

	case FL_FIVE_SECOND:
		if ((CNetwork::GetSyncedTimeInMilliseconds() + (randomSeed & 7) * (4096 / 8) ) & 2048)
		{
			lightOn = true;
		}
		break;

	case FL_RANDOM_FLASHINESS:
		if ((randomSeed & 255) <= 16)
		{				
			if(FlashyLightOn(randomSeed))
			{
				lightOn = true;		
			}
		}
		else
		{
			lightOn = true;
		}
		break;

	case FL_DISCO:
		{

			float timeTilBeat, bpm;
			s32 beatNum;
			if(!g_RadioAudioEntity.GetCachedNextAudibleBeat(timeTilBeat, bpm, beatNum))
			{			
				// fall back to 1 second if there is no valid beat info
				if ((CNetwork::GetSyncedTimeInMilliseconds() + (randomSeed & 7) * (1024 / 8) ) & 512)
				{
					lightOn = true;
				}
			}
			else
			{
				if((frameCount + randomSeed) & 0x100 || beatNum != 4)
				{
					if(beatNum == 1 || beatNum == 3)
					{
						lightOn = true;
					}
				}
				else
				{
					const float beatLength = 60.f / bpm;
					const float barLength = beatLength * 4.f;
					float timeThroughBar = beatNum*beatLength + (beatLength - timeTilBeat);
					const float subdiv = barLength * 0.125f;
					if(fmodf(timeThroughBar, subdiv) <= subdiv * 0.5f)
					{
						lightOn = true;
					}
				}
			}
		}
		break;

	case FL_CANDLE:
		{
		float num = RNG.GetFloat();
		intensityMult *= (num * (1.0f - 0.85f)) + 0.85f;
		lightOn = true;
		break;
		}

	case FL_PLANE:
		{
			s32	F = (frameCount + randomSeed) & 63;

			switch (F)
			{
			case 0:
				break;
			case 1:
				intensityMult *= 0.5f;
				break;
			default:
				intensityMult *= 0.0f;
				break;
			}
		}
		break;

	case FL_FIRE:
		{
		float num = RNG.GetFloat();
		intensityMult *= (num * (1.0f - 0.50f)) + 0.50f;
		lightOn = true;
		break;
		}

	case FL_THRESHOLD:
		{
			float ref = 0.5f;
#if __BANK
			ref = DebugDeferred::m_lightVolumeExperimental;
#endif // __BANK
			const float slope = 8.0f; // can be adjusted ..
			const float thresh = -(1.0f + 1.0f/slope) + ref*(2.0f + 2.0f/slope); // +/-(1 + 1/slope)
			const float s = ((frameCount + randomSeed) & 1) != 0 ? -1.0f : 1.0f;
			const float r = s*fwRandom::GetRandomNumberInRange(-1.0f, 1.0f);
			const float t = Clamp<float>((r - thresh)*slope, -1.0f, 1.0f)*0.5f + 0.5f; // [0..1]
			intensityMult *= t;
			lightOn = true;
#if GTA_REPLAY
			replayDisableFlickerOnPause = true;
#endif
		}
		break;

	case FL_ELECTRIC:
		{
			intensityMult *= 1.0f - 0.012f * (fwRandom::GetRandomNumber() & 31);
			lightOn = true;

#if GTA_REPLAY
			replayDisableFlickerOnPause = true;
#endif
		}
		break;

	case FL_STROBE:
		{
			if ((frameCount + randomSeed) & 3)
			{
				intensityMult *= 0.0f;
				lightOn = false;
			}
			else
			{
				lightOn = true;
			}
		}
		break;
	}

#if GTA_REPLAY
	//When playing back a replay and we pause it we need to stopo some light flickering that are using a different random number generator
	//that doesnt have a fixed seed.
	if( replayDisableFlickerOnPause && CReplayMgr::IsEditModeActive() && CReplayMgr::IsPlaybackPaused() )
	{
		intensityMult = 1.0f;
		lightOn = true;
	}
#endif
}

// ----------------------------------------------------------------------------------------------- //

bool CLightEntity::GetWorldPosition(Vector3& out) const
{
	Matrix34 mat(M34_IDENTITY);
	const bool ret = GetWorldMatrix(mat);
	
	out = mat.d;
	return ret;
}

// ----------------------------------------------------------------------------------------------- //


bool CLightEntity::GetWorldMatrix(Matrix34& mat) const
{
	C2dEffect* p2DEffect = Get2dEffect();
	CLightAttr* light = (Get2dEffectType() == ET_LIGHT) ? GetLight(static_cast<CLightAttr*>(p2DEffect)) : NULL;
	return GetWorldMatrix(mat, p2DEffect, light);
}

// ----------------------------------------------------------------------------------------------- //

bool CLightEntity::GetWorldMatrix(Matrix34& mat, C2dEffect* pEffect, CLightAttr* lightData) const
{
	const e2dEffectType effectType = Get2dEffectType();

#if __DEV
	if (m_parentEntity == NULL)
	{
		return false;
	}

	// Make sure we can actually get the light data from the parent entity
	CBaseModelInfo* pModelInfo = m_parentEntity->GetBaseModelInfo();
	if (!pModelInfo)
	{
		return false;
	}

	const s32 effectId = Get2dEffectId();
	if (effectId < 0 || effectId >= pModelInfo->GetNum2dEffects()) 
	{
		return false;
	}
#endif

	// Get the bone tag only for lights (light shafts are not attached)
	s32 boneTag = -1;
	if (effectType == ET_LIGHT)
	{
		boneTag = lightData->m_boneTag;
	}

	// Transform position of the light based on bone
	const Vector3 Pos = pEffect->GetPosDirect().GetVector3();
	if (boneTag != -1)
	{
		CVfxHelper::GetMatrixFromBoneTag_Lights(RC_MAT34V(mat), m_parentEntity, boneTag);
		if (mat.a.IsZero())
		{
			return false;
		}
		else
		{
			mat.Transform(Pos, mat.d);
		}
	}
	else
	{
		mat = MAT34V_TO_MATRIX34(m_parentEntity->GetNonOrthoMatrix());
		mat.d = m_parentEntity->TransformIntoWorldSpace(Pos);
	}

	return true;
}

// ----------------------------------------------------------------------------------------------- //

void AddCoronaOnlyLightHash(const CLightEntity *lightEntity, const CLightSource &
#if __BANK
	lightSource
#endif
	)
{
	if (!CLODLights::Enabled())
		return;

	LightingData& lightingData = Lights::GetUpdateLightingData();

	const u32 hash = (lightEntity != NULL && lightEntity->GetParent() != NULL) ? CLODLightManager::GenerateLODLightHash(lightEntity) : UINT_MAX;
	
	if (lightingData.m_coronaLightHashes.GetCount() < MAX_CORONA_ONLY_LIGHTS)
	{
		lightingData.m_coronaLightHashes.Push(hash);
	}
	else
	{
		Assertf(false, "We've already registered %d corona-only lights", MAX_CORONA_ONLY_LIGHTS);
	}
	
	#if __BANK
	if (DebugLighting::m_showLODLightHashes)
	{
		char buffer[255];
		sprintf(buffer, "(Corona) Light Entity hash: %u", hash);
		Vec3V vPosition = VECTOR3_TO_VEC3V(lightSource.GetPosition());
		grcDebugDraw::Text(vPosition, Color32(255, 255, 0, 255), 0, 10, buffer, true);
	}
	#endif
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::ProcessLightSource(const bool addLightToScene, const bool addCorona, const bool addToPreviousLightList)
{
	C2dEffect* p2DEffect = Get2dEffect();
	const e2dEffectType effectType = Get2dEffectType();

	CLightAttr* light = (effectType == ET_LIGHT) ? GetLight(static_cast<CLightAttr*>(p2DEffect)) : NULL;

	const bool ignoreDayNightSettings = (bool)m_nFlags.bLightsIgnoreDayNightSetting | (bool)m_parentEntity->m_nFlags.bLightsIgnoreDayNightSetting;
	
	float intensityMult = light ? CalcLightIntensity(ignoreDayNightSettings, light) : 1.0f;
	if(intensityMult <= 0.0f)
	{
		return;
	}

	Matrix34 mat;
	const bool isOn = GetWorldMatrix(mat, p2DEffect, light);

	// kill the light if it's off
	if (!isOn || (!m_parentEntity->GetIsVisible() && !addToPreviousLightList) || (light && (light->m_flags & LIGHTFLAG_FAR_LOD_LIGHT) != 0)) // if adding to previous lights, is a shadow thing, and the parent might not have been visible last frame
	{		
		return;
	}

	fragInst *pInst = m_parentEntity->GetFragInst();

	// Kill for broken frags
	if (pInst)
	{
		const fragPhysicsLOD* pPhysicsLOD = pInst->GetTypePhysics();
		bool bBroken = false;

		if (pPhysicsLOD->GetMinMoveForce() != 0.0f)
		{
			bBroken = pInst->GetBroken();
		}
		else
		{
			const fragType* pType = pInst->GetType();
			const crSkeletonData& skeletonData = pType->GetSkeletonData();
			int boneIndex = 0;
			skeletonData.ConvertBoneIdToIndex((u16)light->m_boneTag, boneIndex);

			int nGroupIndex = pType->GetGroupFromBoneIndex(0, boneIndex);
			while (!bBroken && nGroupIndex != 0xff && nGroupIndex != -1)
			{
				bBroken = pInst->GetGroupBroken(nGroupIndex);
				nGroupIndex = pPhysicsLOD->GetGroup(nGroupIndex)->GetParentGroupPointerIndex();
			}
		}

		if (bBroken)
		{
			return;
		}
	}

	if(light)
	{		
		u32 lightOverrideFlags = 0;
		
		// Calculate light intensity and if its on or not
		bool lightOn = false;
		ApplyEffectsToLight(mat, intensityMult, lightOn, light->m_flashiness);

		// Hack to make sure we add the light so its hash is added too.
		if (lightOn == false)
		{
			intensityMult = 0.0001f;
			lightOn = true;
		}
		
		if( m_parentEntity->GetType() == ENTITY_TYPE_OBJECT && ((CObject*)(m_parentEntity.Get()))->IsADoor() )
		{
			const CDoor *door = (CDoor*)m_parentEntity.Get();

			// Numbers were pulled from GetDoorOcclusion, portal.cpp
			static dev_float debugTransThreshold=0.25f;
			static dev_float debugOrientThreshold=0.5f;
			float doorOcc = 1.0f - door->GetDoorOcclusion(0.01f, 0.03f, debugTransThreshold, debugOrientThreshold);
			
			intensityMult *= doorOcc;
		}
		
		if (lightOn && intensityMult > 0.0f)
		{
			const s32 effectId = Get2dEffectId();
			g_EmitterAudioEntity.UpdateLightAudio(this, m_parentEntity, p2DEffect, lightOn && intensityMult > 0.0, effectId);

			CLightSource lightSource;
			lightSource.SetLightEntity(this);
			if (GetParent())
			{
				lightSource.SetParentAlpha(float(GetParent()->GetAlpha()) / 255.0f);
			}
			
			const bool bUseCoronaOnlyPath = !addLightToScene || (((light->m_flags | lightOverrideFlags) & LIGHTFLAG_CORONA_ONLY) != 0);

			float finalIntensity = 0.0f;

			if(bUseCoronaOnlyPath)
			{
				finalIntensity = ProcessOne2dEffectLightCoronaOnly(
					lightSource, 
					mat, 
					intensityMult,
					GetInteriorLocation(),
					lightOverrideFlags,
					light);

				// If light is on add a corona
				if (addCorona)
				{
					AddCorona(lightSource, light, GetIsInInterior());
					AddCoronaOnlyLightHash(this, lightSource);
				}
			}
			else
			{
				CEntityFlags lightEntityFlags = m_nFlags;
				s32 texDictionary = m_parentEntity->GetBaseModelInfo()->GetAssetParentTxdIndex();
				fwInteriorLocation interiorLocation = GetInteriorLocation();
				
				if (Lights::m_forceSceneLightFlags & light->m_lightType)
				{
					lightEntityFlags.bLightsCanCastStaticShadows = (Lights::m_forceSceneShadowTypes & LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS) ? 1 : 0;
					lightEntityFlags.bLightsCanCastDynamicShadows = (Lights::m_forceSceneShadowTypes & LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS) ? 1 : 0;
				}

				finalIntensity = ProcessOne2dEffectLight(
					lightSource, 
					mat, 
					intensityMult,
					interiorLocation, 
					texDictionary, 
					lightEntityFlags,
					lightOverrideFlags | (addToPreviousLightList ? LIGHTFLAG_DELAY_RENDER : 0), // add in extra flag, so the shadow know it was added to the previous list late (just used for debug)
					light);

				// If light is on add a corona
				if (finalIntensity > 0.0)
				{
					// Add light and corona to scene if the light was actually on
					if(addLightToScene && !lightSource.IsCoronaOnly())
					{
					#if RSG_PC
						//	Dirty hack
						lightSource.SetExtraFlag(light->m_padding1);
					#endif
					#if RSG_BANK
						lightSource.SetExtraFlag(u8(light->m_padding3));	// set any extra flags kept in padding3 (B*6817429 - PED only light option in flags for CS lighting)
					#endif
						Assert(effectType == ET_LIGHT);
						Lights::AddSceneLight(lightSource, this, addToPreviousLightList);
					}

					if (addCorona)
					{
						AddCorona(lightSource, light, GetIsInInterior());
					}

				}
			}
		}
		else
		{
			// Light was off or intensity was 0
			if (light->m_flashiness != 0)
			{
				
			}
		}
	}
	else
	{
		#if __BANK
		if (DebugLights::m_freeze)
			return;
		#endif

		SetLightShaftBounds(static_cast<CLightShaftAttr*>(p2DEffect), addLightToScene);
	}
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::SetupLight(CEntity* ASSERT_ONLY(entity))
{
	C2dEffect* p2DEffect = Get2dEffect();
	CLightAttr* data = GetLight(static_cast<CLightAttr*>(p2DEffect));
	
#if __ASSERT
	Assertf(data->m_lightType == LIGHT_TYPE_POINT || 
			data->m_lightType == LIGHT_TYPE_SPOT ||
			data->m_lightType == LIGHT_TYPE_CAPSULE, "Light entity being created has incorrect light type - %d (%s)", 
			data->m_lightType, 
			entity->GetModelName());
#endif

	Matrix34 mat;
	Vector3 dir = data->m_direction.GetVector3();
	
	GetWorldMatrix(mat, p2DEffect, data);
	mat.Transform3x3(dir, dir);

	SetLightBounds(data, mat.d, dir);
}

// ----------------------------------------------------------------------------------------------- //

void CLightEntity::SetupLightShaft()
{
	CLightShaftAttr *data = GetLightShaft();

	SetLightShaftBounds(data); // just update the bounds
}

// ----------------------------------------------------------------------------------------------- //

CLightAttr* CLightEntity::GetLight() const
{ 
	if (Get2dEffectType() != ET_LIGHT)
		return NULL;

	// Get from parent
	if (m_parentEntity != NULL)
	{
		const s32 effectId = Get2dEffectId();
		CBaseModelInfo* pModelInfo = m_parentEntity->GetBaseModelInfo();
		Assertf(effectId < pModelInfo->GetNum2dEffects(), "Incorrect 2deffect index when accessing light");
		CLightAttr* pEffect = static_cast<CLightAttr*>(pModelInfo->Get2dEffect(effectId));

		CLightExtension* pExtension = m_parentEntity->GetExtension<CLightExtension>();

		if (pExtension != NULL && pEffect != NULL)
		{
			CLightAttr* pInstance = pExtension->GetLightAttr(pEffect->m_lightHash);
			if (pInstance)
			{
				return pInstance;
			}
		}

		return pEffect;
	}

	Assertf(false, "No instance and parent NULL when trying to get the modelInfo for light");
	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

CLightAttr* CLightEntity::GetLight(CLightAttr* pEffect) const
{
	// Get from parent
	if (m_parentEntity != NULL)
	{
		CLightExtension* pExtension = m_parentEntity->GetExtension<CLightExtension>();
		if (pExtension != NULL)
		{
			CLightAttr* pInstance = pExtension->GetLightAttr(pEffect->m_lightHash);
			if (pInstance)
			{
				return pInstance;
			}
		}

		return pEffect;
	}

	Assertf(false, "No instance and parent NULL when trying to get the modelInfo for light");
	return NULL;	
}

// ----------------------------------------------------------------------------------------------- //

CLightShaftAttr* CLightEntity::GetLightShaft() const
{ 
	if (Get2dEffectType() != ET_LIGHT_SHAFT)
		return NULL;

	if (m_parentEntity != NULL)
	{
		const s32 effectId = Get2dEffectId();
		CBaseModelInfo* pModelInfo = m_parentEntity->GetBaseModelInfo();
		Assertf(effectId < pModelInfo->GetNum2dEffects(), "Incorrect 2deffect index when accessing light");
		C2dEffect* pEffect = pModelInfo->Get2dEffect(effectId);
		return static_cast<CLightShaftAttr*>(pEffect); 
	}

	Assertf(false, "Parent NULL when trying to get the modelInfo for light shaft");
	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

C2dEffect* CLightEntity::Get2dEffect() const
{ 
	if (m_parentEntity != NULL)
	{
		const s32 effectId = Get2dEffectId();
		CBaseModelInfo* pModelInfo = m_parentEntity->GetBaseModelInfo();
		Assertf(effectId < pModelInfo->GetNum2dEffects(), "Incorrect 2deffect index when accessing light");
		C2dEffect* pEffect = pModelInfo->Get2dEffect(effectId);
		return pEffect; 
	}

	Assertf(m_parentEntity != NULL, "Parent NULL when trying to get the modelInfo for light");
	return NULL;
}

// ----------------------------------------------------------------------------------------------- //

Color32	CLightEntity::GetOriginalLightColor() const
{
	CLightAttr *attr = GetLight();
	if(attr)
	{
		return attr->m_colour.GetColor32();
	}

	return Color32(0x00000000);
}

// ----------------------------------------------------------------------------------------------- //

fwModelId LightEntityMgr::ms_lightModelId;
bool LightEntityMgr::ms_flushLights = false;

void LightEntityMgr::Init(unsigned int initMode)
{
	if(initMode == INIT_BEFORE_MAP_LOADED)
	{
		USE_MEMBUCKET(MEMBUCKET_WORLD);
	
		Assert(CLightEntity::GetPool()->GetNoOfUsedSpaces()==0);

		ms_lightModelId.Invalidate();
		CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("LightObject",0xD6AEA7E2),&ms_lightModelId);

		if (!ms_lightModelId.IsValid()) 
		{
			CModelInfo::AddBaseModel("LightObject");
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromName(strStreamingObjectName("LightObject",0xD6AEA7E2),&ms_lightModelId);

			pModelInfo->SetHasLoaded(true);
			pModelInfo->SetLodDistance(40.0f);
			pModelInfo->SetDrawableTypeAsAssetless();
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void LightEntityMgr::Shutdown(unsigned int)
{
	const s32 maxNumLightEntities = (s32) CLightEntity::GetPool()->GetSize();
	for(s32 i=0;i<maxNumLightEntities;i++)
	{
		if(!CLightEntity::GetPool()->GetIsFree(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			Assert(pLightEntity);
			
			Remove(pLightEntity);
		}
	}

	Assert(CLightEntity::GetPool()->GetNoOfUsedSpaces()==0);
}

// ----------------------------------------------------------------------------------------------- //

void LightEntityMgr::Remove(CLightEntity *lightEntity)
{
	CGameWorld::Remove(lightEntity);
	
#if __BANK
	if( DebugLights::m_currentLightEntity == lightEntity )
	{
		DebugLights::m_currentLightEntity = NULL;
		EditLightSource::SetIndex(-1);
	}
	if( DebugLights::m_currentLightShaftEntity == lightEntity )
	{
		DebugLights::m_currentLightShaftEntity = NULL;
		EditLightShaft::SetIndex(-1);
	}
#endif // __BANK

	delete lightEntity;
}

// ----------------------------------------------------------------------------------------------- //

void LightEntityMgr::Update()
{
	BANK_ONLY(if(!DebugLights::m_freeze))
	{
		CLightEntity::Pool* lightEntityPool = CLightEntity::GetPool();

		u32 frameCount = fwTimer::GetFrameCount();
		REPLAY_ONLY(if(CReplayMgr::IsReplayInControlOfWorld()) frameCount = fwTimer::GetSystemFrameCount();)

		// Make sure its nicely divisible to save some work
		const u32 numSlices = 4; 
		const u32 maxNumLightEntities = (u32) lightEntityPool->GetSize();
		Assert((maxNumLightEntities % numSlices) == 0);

		const u32 stepSize = maxNumLightEntities / numSlices;
		const u32 startIndex = (frameCount % numSlices) * stepSize;
		const u32 endIndex = startIndex + stepSize;

		for(u32 i = startIndex; i < endIndex; i++)
		{
			if(!lightEntityPool->GetIsFree(i))
			{
				CLightEntity* pLightEntity = lightEntityPool->GetSlot(i);

				if (pLightEntity->GetParent() == NULL)
				{
					Remove(pLightEntity);
				}
			}
		}
	}

	if (ms_flushLights)
	{
		Flush();
		ms_flushLights = false;
	}
}

// ----------------------------------------------------------------------------------------------- //


u32 LightEntityMgr::AddAttachedLights(CEntity* pEntity)
{
	u32 addedLights = 0;

	u32 extendLightFadeDistance = 0;
	if (pEntity && pEntity->GetIsInInterior() && pEntity->GetArchetype()->GetModelNameHash() ==  ATSTRINGHASH("v_60_hangerint", 0xb7f54b2f))
	{
		extendLightFadeDistance = 50;
	}

	if (pEntity->m_nFlags.bLightObject == false 
		BANK_ONLY(&& !DebugLights::m_freeze)
		)
	{
		// go through the 2d effects looking for info about light object creation
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		s32 num2dEffects = pModelInfo->GetNum2dEffects();

		for (s32 i=0; i<num2dEffects; i++)
		{
			C2dEffect* pEffect = pModelInfo->Get2dEffect(i);
			u8 effectType = pEffect->GetType();
			const s32 maxNumLightEntities = (s32) CLightEntity::GetPool()->GetSize();

			if( (effectType == ET_LIGHT_SHAFT || effectType == ET_LIGHT) && CLightEntity::GetPool()->GetNoOfUsedSpaces() < maxNumLightEntities )
			{
				fwInteriorLocation loc;
				CLightEntity* pLightEntity = rage_new CLightEntity( ENTITY_OWNEDBY_OTHER );
				pLightEntity->SetParent(pEntity);
				pLightEntity->Set2dEffectIdAndType(i, effectType);
				addedLights = 1;

#if RSG_PC
				if(pLightEntity->GetLight())
				{
					//	Find the point light causing the flickering in the prologue and set it as highpriority to get a stable sort
					pLightEntity->GetLight()->m_padding1 = 0;

					if(	CMiniMap::GetInPrologue() && 
						pEntity->GetArchetype()->GetModelNameHash() == ATSTRINGHASH("plogint_sid_det", 0x591e570f) &&
						pLightEntity->GetLight()->m_lightType == LIGHT_TYPE_POINT
						)
					{
						pLightEntity->GetLight()->m_padding1 = EXTRA_LIGHTFLAG_HIGH_PRIORITY;
					}
				}
#endif

				CEntity *pLocEntity;
				if (!pEntity->IsRetainedFlagSet() && 
					!pEntity->GetOwnerEntityContainer() && 
					pEntity->GetIsTypeObject() &&
					static_cast<CObject*>(pEntity)->GetRelatedDummy())
				{
					pLocEntity = static_cast<CObject*>(pEntity)->GetRelatedDummy();
				}
				else
				{
					pLocEntity = pEntity;
				}
				loc = pLocEntity->GetInteriorLocation();

				if(effectType == ET_LIGHT)
				{
					pLightEntity->SetupLight(pEntity);

					// If this Lights' LODlight was previously broken, un-break it
					CLODLightManager::UnbreakLODLight(pLightEntity);
					CLightAttr *pAttr = pLightEntity->GetLight();
					if (pAttr->m_flags & LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR)
					{
						pLightEntity->ClearCustomFlag(LIGHTENTITY_FLAG_SCISSOR_LIGHT);
					}
					else if ( pEntity->GetIsFixedFlagSet() )
					{					
						if (loc.IsValid())
						{
							if (loc.IsAttachedToRoom() && loc.GetRoomIndex() == 0)
							{
								pLightEntity->ClearCustomFlag(LIGHTENTITY_FLAG_SCISSOR_LIGHT);
							}
							else
							{
								fwBaseEntityContainer *pContainer = pLocEntity->GetOwnerEntityContainer();
								fwSceneGraphNode *pSceneNode = pContainer->GetOwnerSceneNode();
								if (pSceneNode->IsTypeRoom())
								{
									CInteriorInst* pInteriorInst = CInteriorInst::GetInteriorForLocation( loc );
									s32 roomIndex = loc.GetRoomIndex();
	#if 0 //__ASSERT
									if(!(pAttr->m_flags & LIGHTFLAG_CORONA_ONLY))
									{
										CInteriorProxy* pIntProxy;
										s32				roomIdx;
										Vector3			probePoint;
										const float		probeLength = 16.0f;
										Vector3 position;
										pLightEntity->GetWorldPosition(position);


										bool validCollision = CPortalTracker::ProbeForInteriorProxy(position, 
																									pIntProxy, 
																									roomIdx,  
																									&probePoint, 
																									probeLength);
										const Vector3 &parentPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
									
										if (validCollision && roomIdx != 0)
										{
											artAssertf(loc.GetRoomIndex() == roomIdx, "In interior %s, the light at position %f, %f, %f which is attached to entity "
																					  "%s at position %f, %f, %f, is located in a different room to its parent, light room %d, parent of "
																				      "light room %d",
																					  pInteriorInst->GetModelName(),
																				      position.x, 
																					  position.y, 
																					  position.z, 
																					  pEntity->GetModelName(), 
																					  parentPosition.x, 
																					  parentPosition.y, 
																					  parentPosition.z, 
																					  roomIdx, 
																					  loc.GetRoomIndex());
										}
									}
	#endif
									float extraRadius = pAttr->m_falloff * EXTEND_LIGHT(pAttr->m_lightType);
									float radius = pAttr->m_falloff + extraRadius;
									Vector3 pos;
									pLightEntity->GetWorldPosition(pos);

									for(u32 i = 0; i < pInteriorInst->GetNumPortalsInRoom(roomIndex); i++)
									{
										fwPortalCorners portal;
										pInteriorInst->GetPortalInRoom(roomIndex, i , portal);
										if (!pInteriorInst->IsMirrorPortal(roomIndex, i))
										{
											ScalarV distance = portal.CalcDistanceToPoint(VECTOR3_TO_VEC3V(pos));
											if (distance.Getf() < radius)
											{
												pLightEntity->ClearCustomFlag(LIGHTENTITY_FLAG_SCISSOR_LIGHT);
												break;
											}
										}
									}
								}
							}
						}

					}
				}
				else if(effectType == ET_LIGHT_SHAFT)
				{
					pLightEntity->SetupLightShaft();
					pLightEntity->ClearCustomFlag(LIGHTENTITY_FLAG_SCISSOR_LIGHT);
				}

				pLightEntity->SetModelId(ms_lightModelId);
				pLightEntity->SetLodDistance(pEntity->GetLodDistance() + extendLightFadeDistance);
				
				CPostScan::PossiblyAddToLateLightList(pLightEntity);
				CGameWorld::Add(pLightEntity, loc);
			}
		}
	}

	#if __BANK
	const s32 maxNumLightEntities = (s32) CLightEntity::GetPool()->GetSize();

	if (CLightEntity::GetPool()->GetNoOfUsedSpaces() >= maxNumLightEntities)
	{
		Assertf(
			false,
			"We've run out space in the light entities pool (%d / %d)", 
			(s32)CLightEntity::GetPool()->GetNoOfUsedSpaces(),
			maxNumLightEntities);
		DebugLighting::DumpParentImaps(false);
	}
	#endif

	return addedLights;
}

// ----------------------------------------------------------------------------------------------- //

void LightEntityMgr::RemoveAttachedLights(CEntity* pEntity)
{
	if (pEntity->m_nFlags.bLightObject == true 
		BANK_ONLY(&& !DebugLights::m_freeze)
		)
	{
		pEntity->m_nFlags.bLightObject = false;
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

		if (pModelInfo)
		{
			const s32 maxNumLightEntities = (s32) CLightEntity::GetPool()->GetSize();

			for(s32 i = 0; i < maxNumLightEntities; i++)
			{
				if(!CLightEntity::GetPool()->GetIsFree(i))
				{
					CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
					Assert(pLightEntity);

					CEntity* pLightParent = pLightEntity->GetParent();

					if ( pLightParent )
					{
						CBaseModelInfo* pLightParentInfo = pLightParent->GetBaseModelInfo();
						if ( pLightParentInfo == pModelInfo )
						{
							pLightEntity->SetParent(NULL);
						}
					}
				}
			}
		}

	}
}

// ----------------------------------------------------------------------------------------------- //

u32 LightEntityMgr::Flush()
{
	u32 numRemoved = 0;

	fwRscMemPagedPool<CLightEntity, true>* lightEntityPool = CLightEntity::GetPool();
	atPagedPoolIterator<CLightEntity, fwDefaultRscMemPagedPoolAllocator<CLightEntity, true> > iterator(*lightEntityPool);

	while (!iterator.IsAtEnd()) 
	{
		atPagedPoolIndex index = iterator.GetPagedPoolIndex();
		CLightEntity *light = lightEntityPool->Get(index);
		++iterator;

		if (light->GetParent() != NULL && light->GetParent()->GetDrawHandlerPtr() == NULL)
		{
			CGameWorld::Remove(light);
			light->GetParent()->m_nFlags.bLightObject = false;
			delete lightEntityPool->Get(index);
			++numRemoved;
		}
	}

#if __ASSERT
	for(u32 i = 0; i < lightEntityPool->GetSize(); i++)
	{
		if(!lightEntityPool->GetIsFree(i))
		{
			CLightEntity* pLightEntity = lightEntityPool->GetSlot(i);

			if (pLightEntity->GetParent() != NULL)
			{
				Assertf(pLightEntity->GetParent()->m_nFlags.bLightObject == true, "Light entity with parent that hasn't added lights");	
			}
		}
	}
#endif

	return numRemoved;
}

// ----------------------------------------------------------------------------------------------- //

void LightEntityMgr::UpdateLightsForEntity(const fwEntity *pEntity)
{
	fwRscMemPagedPool<CLightEntity, true>* lightEntityPool = CLightEntity::GetPool();
	atPagedPoolIterator<CLightEntity, fwDefaultRscMemPagedPoolAllocator<CLightEntity, true> > iterator(*lightEntityPool);

	while (!iterator.IsAtEnd()) 
	{
		atPagedPoolIndex index = iterator.GetPagedPoolIndex();
		CLightEntity *light = lightEntityPool->Get(index);
		++iterator;

		if (light->GetParent() != NULL && light->GetParent() == pEntity)
		{
			light->SetupLight(light->GetParent());
			light->RequestUpdateInWorld();
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void LightEntityMgr::UpdateLightsLocationFromEntity(const CEntity *pEntity)
{
	if(!pEntity)
		return;

	fwRscMemPagedPool<CLightEntity, true>* lightEntityPool = CLightEntity::GetPool();
	atPagedPoolIterator<CLightEntity, fwDefaultRscMemPagedPoolAllocator<CLightEntity, true> > iterator(*lightEntityPool);

	while (!iterator.IsAtEnd()) 
	{
		atPagedPoolIndex index = iterator.GetPagedPoolIndex();
		CLightEntity *light = lightEntityPool->Get(index);
		++iterator;

		if (light && light->GetParent() != NULL && light->GetParent() == pEntity)
		{
			const CEntity *pLocEntity;
			fwInteriorLocation loc = CGameWorld::OUTSIDE;
			if (!pEntity->IsRetainedFlagSet() && 
				!pEntity->GetOwnerEntityContainer() && 
				pEntity->GetIsTypeObject() &&
				static_cast<const CObject*>(pEntity)->GetRelatedDummy())
			{
				pLocEntity = static_cast<const CObject*>(pEntity)->GetRelatedDummy();
			}
			else
			{
				pLocEntity = pEntity;
			}

			if(pLocEntity)
			{
				loc = pLocEntity->GetInteriorLocation();
			}

			if(!light->GetInteriorLocation().IsSameLocation(loc))
			{
				CGameWorld::Remove(light, true, true);
				CGameWorld::Add(light, loc);
			}
		}
	}
}
