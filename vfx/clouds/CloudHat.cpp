// 
// vfx/clouds/CloudHat.cpp 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#include "CloudHat.h"

// Parser generated files
#include "CloudHat_parser.h"

#include "diag/tracker.h"
#include "file/remote.h"
#include "grcore/viewport.h"
#include "grcore/effect_typedefs.h"
#include "grcore/effect.h"
#include "grcore/stateblock.h"
#include "rmcore/drawable.h"
#include "pheffects/wind.h"
#include "fwscene/stores/drawablestore.h"
//#include "streaming/streamingmodule.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "renderer/rendertargets.h"
#include "grprofile/pix.h"
#include "system/param.h"
//#include "system/timemgr.h"
#include "fwsys/timer.h"
#include "CloudHatSettings.h"
#include "renderer/lights/lights.h"
#include "timecycle/TimeCycle.h"
#include "vfx/sky/Sky.h"

#include "paging/rscbuilder.h"

#include "bank/bank.h"
#include "bank/bkmgr.h"

#include "camera/viewports/ViewportManager.h"

#include "control/replay/Effects/LightningFxPacket.h"
#include "control/replay/replay.h"

#if  __XENON
#include "system/xtl.h"
#include "system/d3d9.h"
#endif

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()

PARAM(cloudhatdebug,"[debug] Output Ped cloud transition debug info");

#if __BANK
	#define CLOUDDEBUG_DISPLAYF(fmt,...) if (PARAM_cloudhatdebug.Get()) Displayf("[CHD] " fmt,##__VA_ARGS__)
#else
	#define CLOUDDEBUG_DISPLAYF(...) 
#endif

BankBool gEnableWindEffects = true;
BankBool gUseGustWindForCloudHats = false;
BankBool gUseGlobalVariationWindForCloudHats = false;

BankFloat gWindEffectMinSpeed = 0.0f;
BankFloat gWindEffectMaxSpeed = 5.0f;

static BankFloat s_AltitudeStreamInThreshold = 50.0f;
static BankFloat s_AltitudeStreamOutThreshold = 100.0f;

bool g_UseKeyFrameCloudLight = false;  // this is temp until the light tuning is set up

BANK_SWITCH_NT(ScalarV, const ScalarV) gWindDirMultForOppDirCloudLayer = ScalarV(V_HALF);

namespace
{
	static const char *sDefaultConfigFileExtention = "xml";

	float TimeMsToSec(u32 ms)
	{
		return static_cast<float>(ms) / 1000.0f;
	}
}

class DisableHiStencilWrites
{
	XENON_ONLY(int PrevHiStencilState);
public:
	DisableHiStencilWrites()
	{	
#if __XENON
		GRCDEVICE.GetCurrent()->GetRenderState( D3DRS_HISTENCILWRITEENABLE, (DWORD*)&PrevHiStencilState);
		GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, false);
#endif
	}
	~DisableHiStencilWrites()
	{
		XENON_ONLY(GRCDEVICE.GetCurrent()->SetRenderState( D3DRS_HISTENCILWRITEENABLE, PrevHiStencilState));
	}
};


PARAM(disableclouds, "[sagrender] Disable the awesome new clouds :(");
PARAM(UseKeyFrameCloudLight,"Use the cloud keyframe light info for sun/moon light, instead of reading it from the timecycle values");

namespace rage
{

const char *CloudVarCache::sCloudVarName[NUM_CLOUD_VARS] =
{
	"SunDirection", //SUN_DIRECTION
	"SunColor", //SUN_COLOR,
	"CloudColor", //CLOUD_COLOR,
	"AmbientColor", //AMBIENT_COLOR,
	"SkyColorCloud", //SKY_COLOR,
	"BounceColor", //BOUNCE_COLOR,
	"EastColorCloud", //EAST_COLOR,
	"WestColorCloud", //WEST_COLOR,
	"DensityShiftScale", //DENSITY_SHIFT_SCALE
	"ScatterG_GSquared_PhaseMult_Scale", //SCATTER_G_GSQUARED_PHASEMULT_SCALE
	"PiercingLightPower_Strength_NormalStrength_Thickness", //PIERCING_LIGHT_POWER_STRENGTH_NORMAL_STRENGTH_THICKNESS
	"ScaleDiffuseFillAmbient", //SCALE_DIFFUSE_FILL_AMBIENT
	"WrapLighting_MSAARef", //WRAP_LIGHTING_MSAARef
	"NearFarQMult", //NEAR_FAR_Q_MULT
	"AnimCombine", //ANIM_COMBINE
	"AnimSculpt", //ANIM_SCULPT
	"AnimBlendWeights", //ANIM_BLEND_WEIGHTS
	"UVOffset", //UV_OFFSET
	"CloudViewProjection", //VIEW_PROJ
	"CameraPos", // CAMERA_POS
	XENON_ONLY("DitherTex",) // DITHER_TEX
	"DepthMapTexture" //
};



#if NUMBER_OF_RENDER_THREADS > 1
sysCriticalSectionToken CloudVarCache::sCritSection;
#endif // NUMBER_OF_RENDER_THREADS > 1

namespace
{
	enum AnimModes
	{
		MODE_COMBINE,
		MODE_SCULPT,

		NUM_ANIM_MODES
	};

#if __BANK
	static const char *sAnimModeNames[NUM_ANIM_MODES] = 
	{
		"Combine", //MODE_COMBINE
		"Sculpt" //MODE_SCULPT
	};
#endif

	static grcCullStatus ModelNeverCullCallBack(const spdAABB &/*aabb*/)
	{
		return cullClipped;
	}

#if __BANK
	//Static frame settings override for clouds for immediate visual results.
	static bool sBANKONLY_CopyCurrSettings = false;
	static bool sBANKONLY_OverrideSettings = false;
	static CloudFrame sBANKONLY_CloudFrameOverride;

	void BANKONLY_InitFrameOverrideSettings()
	{
		sBANKONLY_CloudFrameOverride.m_PiercingLightPower_Strength_NormalStrength_Thickness = Vec4V(10.0f, 1.0f, 1.0f, 1.0f);
		sBANKONLY_CloudFrameOverride.m_ScaleDiffuseFillAmbient = Vec3V(V_X_AXIS_WZERO);	//(1.0f, 0.0f, 0.0f);
		sBANKONLY_CloudFrameOverride.m_CloudColor = Vec3V(V_ONE);  //(1.0f, 1.0f, 1.0f);
		sBANKONLY_CloudFrameOverride.m_AmbientColor = Vec3V(V_ONE);  //(1.0f, 1.0f, 1.0f);
		//These default fill colors help visualize normals
		sBANKONLY_CloudFrameOverride.m_SkyColor = Vec3V(0.0f, 1.0f, 0.5f);
		sBANKONLY_CloudFrameOverride.m_BounceColor = Vec3V(0.0f, 0.0f, 0.5f);
		sBANKONLY_CloudFrameOverride.m_EastColor = Vec3V(1.0f, 0.0f, 0.5f);
		sBANKONLY_CloudFrameOverride.m_WestColor = Vec3V(0.0f, 0.0f, 0.5f);

		sBANKONLY_CloudFrameOverride.m_DensityShift_Scale = Vec2V(V_Y_AXIS_WZERO); //(0.0f, 1.0f)
		sBANKONLY_CloudFrameOverride.m_ScatteringConst = 0.7f;
		sBANKONLY_CloudFrameOverride.m_ScatteringScale = 0.1f;
		sBANKONLY_CloudFrameOverride.m_WrapAmount = 1.0f;
		sBANKONLY_CloudFrameOverride.m_MSAAErrorRef = 0.5f;
	}

	void BANKONLY_UpdateGlobalSettings(const CloudFrame &currFrame)
	{
		if(sBANKONLY_CopyCurrSettings)
		{
			sBANKONLY_CloudFrameOverride = currFrame;
		}
	}

	bkGroup *AddFrameOverrideWidgets(bkBank &bank)
	{
		bkGroup *dbgGroup = bank.PushGroup("**Single Frame Edit**",false,"Tune a single frame of sky keyframe data");
		{
//			bank.AddToggle("Copy Current Frame Settings to Override Settings:", &sBANKONLY_CopyCurrSettings);
			bank.AddToggle("Override the per-frame settings:", &sBANKONLY_OverrideSettings);
			bank.AddButton("Reset Default Override Settings", datCallback(CFA(BANKONLY_InitFrameOverrideSettings)));
			bank.AddSeparator();

			// Add Frame Override's Widgets
			sBANKONLY_CloudFrameOverride.AddWidgets(bank);
		}
		bank.PopGroup();

		return dbgGroup;
	}

#endif

}

#if __BANK
const CloudFrame &CloudHatManager::OverrideFrameSettings(const CloudFrame &currFrame)
{
	return (sBANKONLY_OverrideSettings ? sBANKONLY_CloudFrameOverride : currFrame);
}
#endif

void CloudVarCache::ResetShaderVariables()
{
	std::fill(mCloudVar.begin(), mCloudVar.end(), grmsgvNONE);
}

void CloudVarCache::InvalidateCachedShaderVariables()
{
	mCacheInvalid = true;
}

void CloudVarCache::CacheShaderVariables(const rmcDrawable *drawable)
{
#if NUMBER_OF_RENDER_THREADS > 1
	sysCriticalSection cs(sCritSection);
#endif // NUMBER_OF_RENDER_THREADS > 1

	if(	Verifyf(drawable, "NULL drawable passed in to CacheShaderVariables(drawable *)!") && 
		(mCacheInvalid || mCachedDrawable != drawable))
	{
		CLOUDDEBUG_DISPLAYF("updating the cached shader variables for drawable 0x%p",drawable);

		ResetShaderVariables();
		mCachedDrawable = drawable;
		mCacheInvalid = false;

		const grmShaderGroup &shaderGroup = drawable->GetShaderGroup();
		for(int i = 0; i < NUM_CLOUD_VARS; ++i)
		{
			mCloudVar[i] = shaderGroup.LookupVar(sCloudVarName[i], false);
		}

#if !__FINAL
		// check a couple that absolutely must exist or something is very wrong
		if (shaderGroup.LookupVar(sCloudVarName[VIEW_PROJ], true)==grmsgvNONE)
			Errorf ("cloud shader missing shader variable %s", sCloudVarName[VIEW_PROJ]);
		if (shaderGroup.LookupVar(sCloudVarName[CAMERA_POS], true)==grmsgvNONE)
			Errorf ("cloud shader missing shader variable %s", sCloudVarName[CAMERA_POS]);
#endif
	}
}

CloudHatFragContainer::CloudHatFragContainer()
: mPosition(V_ZERO)
, mRotation(V_ZERO)
, mScale(V_ONE)
, mTransitionAlphaRange(0.0f)
, mTransitionMidPoint(0.5f)
, mEnabled(false)
, mActive(false)
, mAngularVelocity(0.f, 0.0f, 0.0f)
, mAngularAccumRot(0.f, 0.0f, 0.0f)
, mAnimBlendWeights(1.0f, 1.0f, 1.0f)
, mEnableAnimations(false)
#if __BANK
, mGroup(NULL)
, mMtlGroup(NULL)
#endif
{
	mName[0] = 0;

	for(int i = 0; i < sNumAnimLayers; ++i)
	{
		mUVVelocity[i] = Vec2V(V_ZERO);
		mAnimUVAccum[i] = Vec2V(V_ZERO);
		mAnimMode[i] = MODE_COMBINE;
		mShowLayer[i] = (i == 0);
	}

	for (int i=0;i<2;i++)
	{
		mRenderActive[i] = false;
		mAngularAccumRotBuffered[i] = Vec3V(V_ZERO); 
		for(int j = 0; j < sNumAnimLayers; ++j)
			mAnimUVAccumBuffered[i][j] = Vec2V(V_ZERO);
	}

	ResetAnimState();
}

CloudHatFragContainer::~CloudHatFragContainer()
{
	Deactivate();
}

CloudHatFragLayer::CloudHatFragLayer() 
: mActive(false)
, mPreloaded(false)
, mTransitioning(false)
, mHasStreamingRequestedPagedIn(false)
, mDrawableSlot(-1)
{
	for (int i=0;i<2;i++)
	{
		mRenderActive[i] = false;
		mRenderDrawableSlot[i] = -1;
		mRenderTransitionAlpha[i] = 0.0f;
	}
}

CloudHatFragLayer::~CloudHatFragLayer()
{
}

#define		FAST_CAMERA5	1

void CloudHatFragContainer::Draw(const CloudFrame &settings, bool noDepthAvailable, const grcTexture* depthBuffer, float globalAlpha)
{
	if(IsActiveRender())
	{
		BANK_ONLY(BANKONLY_UpdateGlobalSettings(settings));
		
		grcViewport *viewPort = grcViewport::GetCurrent();

		if(Verifyf(viewPort, "grcViewport::GetCurrent() returned NULL. The camera matrix is necessary to draw the CloudHat."))
		{
			//
			// calc all the non layer specific variables
			// TODO: more of this could be done in the update thread
			//

			////////
			//Pre-compute Wrap Lighting
			float wrap = settings.m_WrapAmount;
			float wrapMult = 1.0f / (1.0f + wrap);
			Vec3V WrapLighting_MSAARef(wrapMult, wrap * wrapMult, settings.m_MSAAErrorRef);

			////////
			//Pre-compute Scattering parameters
			//Valid range for g is [0.999999, -0.999999]. 1 & -1 will cause undefined behaviour, so we need to clamp.
			static const float sMaxG = 0.999999f;
			float g = Clamp(settings.m_ScatteringConst, sMaxG * -1.0f, sMaxG);
			float g2 = g * g;
			float phaseMult = (3 * (1 - g2)) / (2 * (2 + g2));
			Vec4V scatteringConst(g, g2, phaseMult, settings.m_ScatteringScale);

			////////
			//Constants necessary for soft particles
			float nearClip = viewPort->GetNearClip();
			float farClip = viewPort->GetFarClip();
			float Q = farClip / (farClip - nearClip);
			Vec4V NearFarQMult(nearClip, farClip, Q, -(nearClip * Q));
			
			// save a copy of the viewport matrix (before we adjust the far clip plane, but we want the original)
			viewPort->SetWorldIdentity();
			Mat44V compositeMtx = viewPort->GetCompositeMtx(); // could do this higher up it does not change for any cloudhats
			
			ScalarV InverseDepthProjection = ScalarV(viewPort->GetInvertZInProjectionMatrix()?1.0f:0.0f);

			//Animation mode and Show Layer scaler
			Vec3V modeCombine(mAnimMode[0] == MODE_COMBINE ? 1.0f : 0.0f, mAnimMode[1] == MODE_COMBINE ? 1.0f : 0.0f, mAnimMode[2] == MODE_COMBINE ? 1.0f : 0.0f);
			Vec3V modeSculpt(mAnimMode[0] == MODE_SCULPT ? 1.0f : 0.0f, mAnimMode[1] == MODE_SCULPT ? 1.0f : 0.0f, mAnimMode[2] == MODE_SCULPT ? 1.0f : 0.0f);
			Vec3V showLayersBlendScale(mShowLayer[0] ? 1.0f : 0.0f, mShowLayer[1] ? 1.0f : 0.0f, mShowLayer[2] ? 1.0f : 0.0f);

			//Set relevant variables

#if LOCK_CLOUD_COLORS_TO_TIMECYCLE
			const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
			Vec3V skyColor(currKeyframe.GetVarV3(TCVAR_SKY_ZENITH_COL_R));
			Vec3V eastColor(currKeyframe.GetVarV3(TCVAR_SKY_AZIMUTH_EAST_COL_R));
			Vec3V westColor(currKeyframe.GetVarV3(TCVAR_SKY_AZIMUTH_WEST_COL_R));
#else
			Vec3V skyColor = settings.m_SkyColor;
			Vec3V eastColor = settings.m_EastColor;
			Vec3V westColor = settings.m_WestColor;
#endif

			//On PC we need to do this as float4's as the shader compiler expands an array anything less than a float4 into float4s without packing but the code side tries to pack them.
			Vec4V animUVData[2];

			Vec3V camScroll(CLOUDHATMGR->GetCameraPos()); 
			camScroll *= Vec3V(CLOUDHATMGR->GetAltitudeScrollScaler(),CLOUDHATMGR->GetAltitudeScrollScaler(),0.0f);

			int buf = 1-CClouds::GetUpdateBuffer();

			animUVData[0] = Vec4V(mAnimUVAccumBuffered[buf][0], mAnimUVAccumBuffered[buf][1]);
			animUVData[1] = Vec4V(mAnimUVAccumBuffered[buf][2], Vec2V(V_ZERO));

			Mat34V worldMtx;

			//Setup complete, now draw the model
			const int lod = rage::LOD_HIGH;
			int bucket = -1;

			// now tweak the far clip to be far enough so edge does not clip any cloud hat polys 
			PS3_ONLY(float realFarClip = viewPort->GetFarClip());		
			PS3_ONLY(viewPort->SetFarClip(10000.0f)); // this is not needed on anything but ps3, since it's just to bypass Edge clipping/culling

			//NeverCull
			grmModel::ModelCullbackType prev = grmModel::GetModelCullback();
			grmModel::SetModelCullback(ModelNeverCullCallBack);

			float altitude = CLOUDHATMGR->GetCameraPos().GetZf(); 
			PIXBegin(0, "CloudHat");
			
			for (int i=0; i< mLayers.GetCount(); i++)
			{
				if (rmcDrawable *drawable = mLayers[i].GetRenderDrawable())
				{
					if ((mLayers[i].mHeightTigger>0 && altitude<mLayers[i].mHeightTigger)) // skip layer if we're below the height trigger
						continue;

					if ((mLayers[i].mHeightTrigger2>0 && altitude>mLayers[i].mHeightTrigger2)) // skip layer if we're above the height trigger2
						continue;

					// compute the layer specific density, etc.
					float transitionedInT = mLayers[i].GetRenderFadeAlpha();

					if (transitionedInT<=0.0f)
						continue;

					//Extract density data for computation
					float origDensityShift = settings.m_DensityShift_Scale.GetXf();
					float origDensityScale = settings.m_DensityShift_Scale.GetYf();

					//Compute Alpha transition scaler for density
					float transInAlphaT = mTransitionAlphaRange > 0.0f ? rage::Saturate(transitionedInT / mTransitionAlphaRange) : 1.0f;
					float alphaDensityScale = rage::Lerp(transInAlphaT, 0.0f, origDensityScale);

					//Lastly, compute final transitioned density
					float densityShift = rage::Lerp(transitionedInT, 1.0f, origDensityShift);
					float desiredScale = densityShift < 1.0f ? alphaDensityScale / (1.0f - densityShift) : 0.0f;
					float densityScale = rage::Lerp(transitionedInT, desiredScale, origDensityScale);
					
					Vec4V densityShiftScale(densityShift * globalAlpha, densityScale * globalAlpha, (noDepthAvailable)?0.0f:1.0f, 0.0f);

					// set the shader group variables

					grmShaderGroup &shaderGroup = drawable->GetShaderGroup();
					CloudVarCache & cache = mLayers[i].GetShaderVarCache();
					cache.CacheShaderVariables(drawable);

					ScalarV sunZ = settings.m_SunDirection.GetZ();
					ScalarV sunFade = ScalarV(V_ONE);

					if (mLayers[i].mHeightTigger>0)
					{
						static ScalarV fadeStart(0.05f); 
						static ScalarV fadeEnd(-0.1f); 
						sunFade = (fadeStart-sunZ)/(fadeStart-fadeEnd);
						sunFade = ScalarV(V_ONE) - Clamp(sunFade,ScalarV(V_ZERO),ScalarV(V_ONE));
					}

					cache.SetCloudVar(shaderGroup, SUN_COLOR, sunFade * settings.m_LightColor);

					cache.SetCloudVar(shaderGroup, SUN_DIRECTION, settings.m_SunDirection);

					cache.SetCloudVar(shaderGroup, CLOUD_COLOR, settings.m_CloudColor);
					cache.SetCloudVar(shaderGroup, AMBIENT_COLOR, settings.m_AmbientColor);
					cache.SetCloudVar(shaderGroup, BOUNCE_COLOR, settings.m_BounceColor*settings.m_ScaleSkyBounceEastWest.GetY());

					cache.SetCloudVar(shaderGroup, SKY_COLOR, skyColor*settings.m_ScaleSkyBounceEastWest.GetX());

					// "east color" is really (east color - west color), so the shader does not do that for every pixel
					cache.SetCloudVar(shaderGroup, EAST_COLOR, eastColor*settings.m_ScaleSkyBounceEastWest.GetZ() - westColor*settings.m_ScaleSkyBounceEastWest.GetW());
					cache.SetCloudVar(shaderGroup, WEST_COLOR, westColor*settings.m_ScaleSkyBounceEastWest.GetW());

					cache.SetCloudVar(shaderGroup, DENSITY_SHIFT_SCALE, densityShiftScale);
					cache.SetCloudVar(shaderGroup, SCATTER_G_GSQUARED_PHASEMULT_SCALE, scatteringConst);

					cache.SetCloudVar(shaderGroup, PIERCING_LIGHT_POWER_STRENGTH_NORMAL_STRENGTH_THICKNESS, settings.m_PiercingLightPower_Strength_NormalStrength_Thickness);
					cache.SetCloudVar(shaderGroup, SCALE_DIFFUSE_FILL_AMBIENT, settings.m_ScaleDiffuseFillAmbient);
					cache.SetCloudVar(shaderGroup, WRAP_LIGHTING_MSAARef, WrapLighting_MSAARef);

					//Soft Particle Effect
					cache.SetCloudVar(shaderGroup, NEAR_FAR_Q_MULT, NearFarQMult);

					//For Layer Animations
					cache.SetCloudVar(shaderGroup, ANIM_COMBINE, modeCombine);
					cache.SetCloudVar(shaderGroup, ANIM_SCULPT, modeSculpt);
					cache.SetCloudVar(shaderGroup, ANIM_BLEND_WEIGHTS, mAnimBlendWeights * showLayersBlendScale);

					cache.SetCloudVar(shaderGroup, VIEW_PROJ, compositeMtx);

					Vec4V camPos = Vec4V(CLOUDHATMGR->GetCameraPos(),InverseDepthProjection); // pack inverse depth flag in unused w of camera pos
					cache.SetCloudVar(shaderGroup, CAMERA_POS, camPos);
					
					XENON_ONLY(	cache.SetCloudVar(shaderGroup, DITHER_TEX, g_sky.GetDitherTexture()));

					if(mLayers[i].mHeightTigger>0.0f) // for altitude clouds, calc a UV slide based on the camera
					{
						Mat33V rotMtx;
						Mat33VFromEulersXYZ(rotMtx, mRotation + mAngularAccumRotBuffered[buf]*ScalarV(mLayers[i].mRotationScale));
						Vec3V scrollUV = Multiply(camScroll, rotMtx);
						animUVData[1].SetZ(scrollUV.GetX()); 
						animUVData[1].SetW(scrollUV.GetY());
					}
					cache.SetCloudVar(shaderGroup, UV_OFFSET, animUVData, 2);

					cache.SetCloudVar(shaderGroup, DEPTHBUFFER, depthBuffer);

					Vec3V cloudPos = AddScaled(mPosition, CLOUDHATMGR->GetCameraPos(), CLOUDHATMGR->GetCamPositionScaler() * Vec3V(ScalarV(V_ONE),ScalarV(V_ONE),ScalarV(mLayers[i].mCamPositionScalerAdjust))); 

					Mat34VFromEulersXYZ(worldMtx, mRotation + mAngularAccumRotBuffered[buf]*ScalarV(mLayers[i].mRotationScale), cloudPos);
					//Apply Scale to 3*3 matrix which contains rotation
					worldMtx.GetCol0Ref() *= mScale;
					worldMtx.GetCol1Ref() *= mScale;
					worldMtx.GetCol2Ref() *= mScale;
					worldMtx.SetCol3(worldMtx.GetCol3() - CLOUDHATMGR->GetCameraPos());

					if(shaderGroup.GetCount() > 0)
					{
						const grmShader &shader = shaderGroup.GetShader(0);	
						bucket = shader.GetDrawBucketMask();
					}

					drawable->Draw(RCC_MATRIX34(worldMtx), bucket, lod, 0);
				}
			}
			PIXEnd();

			// restore the far clip
			PS3_ONLY(viewPort->SetFarClip(realFarClip));

			// restore normal cull
			grmModel::SetModelCullback(prev);
		}
	}
}

rmcDrawable * CloudHatFragLayer::GetRenderDrawable() const
{
	return IsActiveRender() ? g_DrawableStore.Get(mRenderDrawableSlot[1-CClouds::GetUpdateBuffer()]) : NULL; 
}

bool CloudHatFragContainer::AreAllLayersStreamedIn() const
{
	if (!mActive) 
		return false;
	
	for (int i=0; i< mLayers.GetCount(); i++)
	{
		if (!mLayers[i].IsStreamedIn())
			return false;
	}  
	
	return true;
}

bool CloudHatFragContainer::IsAnyLayerStreamedIn() const
{
	if (!mActive) 
		return false;

	for (int i=0; i< mLayers.GetCount(); i++)
	{
		if (mLayers[i].IsStreamedIn())
			return true;
	}  

	return false;
}



bool CloudHatFragLayer::IsStreamedIn() const
{
	strLocalIndex drawableSlot = mDrawableSlot;
	return mActive && (drawableSlot.IsValid() && g_DrawableStore.HasObjectLoaded(drawableSlot));
}


void CloudHatFragContainer::Update(bool streamingOnly)
{
	if(mEnabled && IsTransitioning())
	{
		for (int i=0; i< mLayers.GetCount(); i++)
			mLayers[i].Update();
	}

	if (!streamingOnly)
		UpdateBufferedStates();
}


void CloudHatFragLayer::Update()
{
	if(IsStreamedIn())
	{
		if(!mHasStreamingRequestedPagedIn)
			PagedIn();
	}
}

bool CloudHatFragContainer::CalculatePositionOnCloudHat(Vector3 &out_position, const Vector2 &in_direction) const
{
	bool bValidPositionFound = false;
	out_position.Set(0.0f, 0.0f, 0.0f);
	for (u32 i = 0; i < mLayers.GetCount(); i++)
	{
		if(mEnabled && mLayers[i].IsStreamedIn())
		{
			Vector3 dir(0.0f, 1.0f, 0.0f);
			dir.RotateX(in_direction.y*DtoR);
			dir.RotateZ(in_direction.x*DtoR);

			if (rmcDrawable *drawable =  g_DrawableStore.Get(mLayers[i].GetDrawableSlot())) // is this called from the render thread or the main thread
			{
				Vector3 boundsMin, boundsMax;
				drawable->GetLodGroup().GetBoundingBox(boundsMin, boundsMax);
				Vector3 centerPos = (boundsMin + boundsMax) * 0.5f;
				centerPos.z = boundsMin.z;
				Vector3 scaler;
				scaler = RCC_VECTOR3(mScale);
				//Need to set the height to zero as we are always at the bottom of the cloud hat
				scaler *= (boundsMax - boundsMin) * Vector3(0.5, 0.5, 1.0f);
				scaler.Abs();
				out_position = dir * scaler;

				bValidPositionFound = true;
				break;
			}
		}
	}
	return bValidPositionFound;
}


//Stream in clouds
void CloudHatFragContainer::PreLoad()
{
	if(!mActive && !mTransitioning )
	{
		for (int i=0; i< mLayers.GetCount(); i++)
		{
			mLayers[i].Activate(true); // request them, but don't really activate them yet
		}
	}
}

void CloudHatFragLayer::Activate(bool preActivate)
{
	//Request stream in only if it hasn't been requested before.
	if(!mActive && CLOUDHATMGR && CLOUDHATMGR->GetStreamingNameToSlotFunctor().IsValid())
	{
		//Find a slot for drawable... if none exists, add one.
		mDrawableSlot = CLOUDHATMGR->GetStreamingNameToSlotFunctor()(mFilename);
		
		CLOUDDEBUG_DISPLAYF("CloudHatFragLayer::Activate() - cloudhat layer %s %sactivated (%salready resident)",(mFilename.GetCStr() ? mFilename.GetCStr() : "<Unknown>"),preActivate?"pre":"",(mDrawableSlot.Get()>-1 && g_DrawableStore.HasObjectLoaded(mDrawableSlot))?"":"not ");

		//If model is found, stream it in.
		if(	Verifyf(	mDrawableSlot.IsValid(), "Could not find the DrawableIndex for cloud drawable \"%s\" (hash = %d)",
			(mFilename.GetCStr() ? mFilename.GetCStr() : "<Unknown>"), mFilename.GetHash()) && 
			Verifyf(	g_DrawableStore.StreamingRequest(mDrawableSlot, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD),
			"Streaming request failed for cloud drawable \"%s\"!", (mFilename.GetCStr() ? mFilename.GetCStr() : "<Unknown>")) )
		{
			mActive = !preActivate;
			if (preActivate)
				mPreloaded = true; // to keep it from streaming out when the normal usage is done with it.
		}
	}
}

//Stream out all clouds	(script Only)
void CloudHatFragContainer::Deactivate() 
{
	if(mActive)
	{
		for (int i=0; i< mLayers.GetCount(); i++)
		{
			mLayers[i].Deactivate();
		}
		mActive = false;
		mTransitioning = false;
	}
}

// we're going to deactivate a preloaded cloudhat, assuming it's not in use and have not been deactivated naturally already
void CloudHatFragContainer::DeactivatePreLoad() 
{
	if(!mActive && !mTransitioning) // as long as we're not using it, release the preloaded cloudhat
	{
		for (int i=0; i< mLayers.GetCount(); i++)
		{
			if (mLayers[i].GetDrawableSlot().Get()>-1 && mLayers[i].mPreloaded)
			{
				// only if not in the process of streaming in, because it needs to stay set until the ref count is added after load
				if (!mLayers[i].mActive || mLayers[i].mHasStreamingRequestedPagedIn) // if !active, it has not been requested, is mHasStreamingRequestedPagedIn, it's been streamed in and has a ref count
					g_DrawableStore.ClearRequiredFlag(mLayers[i].GetDrawableSlot().Get(), STRFLAG_DONTDELETE);
			}
			mLayers[i].mPreloaded = false;
			mLayers[i].mDrawableSlot = -1;
		}
	}
}

void CloudHatFragLayer::Deactivate(bool instant)
{
	if(mActive)
	{
		CLOUDDEBUG_DISPLAYF("CloudHatFragLayer::Deactivate() - cloudhat layer %s deactivated, instant = %s, mHasStreamingRequestedPagedIn = %s, mPreAvivate = %s",(mFilename.GetCStr() ? mFilename.GetCStr() : "<Unknown>"), instant?"true":"false", mHasStreamingRequestedPagedIn?"true":"false", mPreloaded?"true":"false");

		if (mHasStreamingRequestedPagedIn)	 // don't remove ref if we did not add one
		{
			if (instant)
				g_DrawableStore.RemoveRef(mDrawableSlot, REF_RENDER);
			else
				gDrawListMgr->AddRefCountedModuleIndex(mDrawableSlot.Get(), &g_DrawableStore);
		}

		mActive = false;
		mDrawableSlot = -1;
		mHasStreamingRequestedPagedIn = false;
	}
}

void CloudHatFragContainer::ResetAnimState()
{
	mAngularAccumRot = Vec3V(V_ZERO);

	for(int i = 0; i < sNumAnimLayers; ++i)
	{
		mAnimUVAccum[i] = Vec2V(V_ZERO);
	}
}

float CloudHatFragLayer::GetTransitionTimeElapsed() const
{
	if(!IsStreamedIn())
		return 0.0f;// Still streaming in, transition has not started yet.

	const float currTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds());
	float elapsedTime = currTime - mTransitionStartTime;

	return rage::Clamp(elapsedTime, 0.0f, GetTransitionDuration());
}


void CloudHatFragContainer::AltitudeUpdate(float height)
{
	if ( (mActive && !IsTransitioning()) || IsTransitioningIn() )  // don't turn on altitude clouds if we already transitioning out
	{
		for (int i=0; i<mLayers.GetCount(); i++)
		{
			if (mLayers[i].AltitudeUpdate(height))
			{
				// we just turned on a previously inactive layer. need to enable transitioning on the cloud hat to it gets updated
				if( !IsTransitioningIn()) // if we were already transitioning in, we're good.
				{
					mTransitioning = true;
					mTransitioningIn = true;
				}
			}
		}
	}
}

bool CloudHatFragLayer::IsAboveStreamInTrigger(float height) const
{
	return (mHeightTigger<=0.0f || (height > mHeightTigger - s_AltitudeStreamInThreshold));
}

bool CloudHatFragLayer::IsBelowStreamOutTrigger(float height) const
{
	return (mHeightTigger>0.0f && (height < mHeightTigger - s_AltitudeStreamOutThreshold));
}

bool CloudHatFragLayer::AltitudeUpdate(float height)
{
	if (IsAboveStreamInTrigger(height)) 
	{
		if (!mActive) 
		{
			if (!IsTransitioning() )  // if we're already transitioning in, we don't need to do anything
			{
				CLOUDDEBUG_DISPLAYF("CloudHatFragLayer::AltitudeUpdate() - Activating cloud hat layer '%s'",mFilename.GetCStr() );
			
				mTransitionDuration = 10.0f;
				mTransitionDelay = 0.0f;
				mTransitionStartTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds());
				mTransitioning = true;
				mTransitioningIn = true;

				return true; // return if we are just started transitioning in.
			}
		}
		else  // we are active.
		{
			if (IsTransitioningIn() && height<mHeightTigger && IsStreamedIn()) // we're transitioning in, but we're below the trigger height, skip fade in, to it's ready to height fade.
			{
				mTransitioning = false;
				mTransitioningIn = false;
			}
		}
	}
	else if (IsBelowStreamOutTrigger(height))
	{
		if (mActive || IsTransitioningIn() ) // if we are active or transitioning in, we need to clean up a bit)
		{
			CLOUDDEBUG_DISPLAYF("CloudHatFragLayer::AltitudeUpdate() - deactivating cloud hat layer '%s', which was%s streamed in",mFilename.GetCStr(), mHasStreamingRequestedPagedIn?"":" not");

			if (mActive && mDrawableSlot.IsValid() && !mHasStreamingRequestedPagedIn )  // not streamed in yet, so release the don't delete flag and let it dies after streaming in
			{
				if(!mPreloaded)
					g_DrawableStore.ClearRequiredFlag(mDrawableSlot.Get(), STRFLAG_DONTDELETE);
			}

			Deactivate(false);
			mTransitioning = false;
			mTransitioningIn = false;
		}		
	}
	return false;
}

void CloudHatFragContainer::StartTransitionIn(float transitionDuration, float alreadyElapsedTime, bool assumedMaxLength)
{
	CLOUDDEBUG_DISPLAYF("CloudHatFragContainer::StartTransitionIn(%f, %f) for cloud hat '%s'",transitionDuration, alreadyElapsedTime, mName);

	if (!mTransitioning && !mActive)
	{
		ResetAnimState(); // don't reset this if it's in transition or already loaded
	}

	mTransitioningIn = true;
	mTransitioning = false;
	mActive = false;
	mTransitionStartTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - alreadyElapsedTime;

	for (int i=0; i<mLayers.GetCount(); i++)
	{
		mLayers[i].StartTransitionIn(transitionDuration, alreadyElapsedTime, assumedMaxLength);
		mTransitioning = mTransitioning || mLayers[i].IsTransitioningIn(); // mark the CloudHat as transitioning in if any layer is still transitioning in
		mActive  = mActive || mLayers[i].IsActive(); // if any layer is active, the cloud hat is active
	}
}

void CloudHatFragLayer::StartTransitionIn(float transitionDuration, float alreadyElapsedTime, bool assumedMaxLength)
{
	if (mHeightTigger>0) // for altitude layers, we don't start with the rest of the layers if we're below the trigger
	{
		if (gVpMan.GetCurrentViewport() && !IsAboveStreamInTrigger(gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraPosition().GetZf()))   
		{
			if (mTransitioning && !mTransitioningIn) // if it was going out and we're below the trigger, just stop it's transition.
				mTransitioning = false;

			return;
		}
	}
	float currentPercent = GetTransitionPercent(); // save the percent, in case we need to adjust the current start time
	mTransitionDuration = transitionDuration * mTransitionInTimePercent;
	mTransitionDelay = transitionDuration * mTransitionInDelayPercent;
	mTransitionStartTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - alreadyElapsedTime;

	//because the triggering code assumes the time taken was would be the full amount we adjust the start position to compensate
	if(assumedMaxLength)
	{
		float diffFromFulltime = transitionDuration - mTransitionDuration;
		mTransitionStartTime = diffFromFulltime * 0.5f;
	}
	
	// if we were transitioning in, this is were we need to a calc the time so we START with the same alpha,etc
	if (mTransitioning)
	{
		if ( !mTransitioningIn ) // we were transitioning out, now reverse and transition in
		{
			mTransitioningIn = true;
			mTransitionStartTime -= (1-currentPercent)*mTransitionDuration;			// adjust time so alpha is consistent with new duration, etc.
		}
		else // we are already transitioning in.
		{
			if(!mHasStreamingRequestedPagedIn)  // we have not streamed in yet
			{	
					// no need to adjust start time in this case
			}
			else 	// we already started showing the clouds, so we need to recalibrate the time to keep alpha from popping
			{
				mTransitionStartTime -= currentPercent*mTransitionDuration;		// adjust time so alpha is consistent with new duration, etc.
			}
		}
	}
	else // we were not transition before
	{
		if (mActive) // if we're active and not transitioning, we we fully in, so no need to transition in "more"
		{
			// nothing to do here
			CLOUDDEBUG_DISPLAYF("CloudHatFragLayer::StartTransitionIn() for Layer '%s', Already fully transitioned in and active",mFilename.GetCStr()); 
		}
		else // we were not in or coming in
		{
			mTransitioningIn = true;
			mTransitioning = true;
		}
	}
}

void CloudHatFragContainer::TransitionInUpdate(float costAvailable)
{
	if (IsTransitioningIn())
	{
		mTransitioning = false;
		mActive = false;
		float cloudHatTransitionTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - mTransitionStartTime;
	
		if (costAvailable<=0.0f) // if no one is out yet, we'll hold back our timer, so they don't all come in at once when the cloud hats are finally streamed out.
			mTransitionStartTime += cloudHatTransitionTime;

		for (int i=0; i<mLayers.GetCount(); i++)
		{
			costAvailable = mLayers[i].TransitionInUpdate(cloudHatTransitionTime, costAvailable);
			mTransitioning = mTransitioning || mLayers[i].IsTransitioningIn(); // mark the CloudHat as still transitioning in if any layer is still transitioning in
			mActive = mActive || mLayers[i].IsActive(); // if any layer is active, the cloud hat is active
		}
	}
}

float CloudHatFragLayer::TransitionInUpdate(float cloudHatTransitionTime, float costAvailable)
{
	if (mTransitioning) 
	{
		float time = TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - mTransitionStartTime;

		if (!IsStreamedIn())	 // if we're not streamed in or active, don't count this time towards the fade in!
		{
			mTransitionStartTime += time;  
			time = 0;
		}

		if (!mActive)
		{
			if (costAvailable>=mCostFactor && cloudHatTransitionTime>=mTransitionDelay)
				Activate();
		}


		if (mActive && mHasStreamingRequestedPagedIn)
		{
			if (time >= mTransitionDuration)
			{
				mTransitioning = false; // we're all the way in, so we're not transitioning anymore
			}
		}
	}

	if (mActive)
		costAvailable -= mCostFactor;

	return costAvailable;
}


void CloudHatFragContainer::StartTransitionOut(float transitionDuration, float alreadyElapsedTime, bool assumedMaxLength)
{
	if (mTransitioning || mActive)
	{
		CLOUDDEBUG_DISPLAYF("CloudHatFragContainer::StartTransitionOut(%f, %f) for cloud hat '%s'",transitionDuration, alreadyElapsedTime, mName);

		mTransitioningIn = false;
		mTransitioning = false;
		mActive = false;
		mTransitionStartTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - alreadyElapsedTime;

		for (int i=mLayers.GetCount()-1; i>=0; i--)
		{
			mLayers[i].StartTransitionOut(transitionDuration, alreadyElapsedTime, assumedMaxLength);
			mTransitioning = mTransitioning || mLayers[i].IsTransitioningOut(); // only mark cloud hat as transitioning if at least one of the layers needs to transition out
			mActive  = mActive || mLayers[i].IsActive(); // if any layer is active, the cloud hat is active
		}
	}
}

void CloudHatFragLayer::StartTransitionOut(float transitionDuration, float alreadyElapsedTime,  bool assumedMaxLength)
{
	float currentPercent = GetTransitionPercent(); // save the percent, incase we need to adjust the current start time
	mTransitionDuration = transitionDuration * mTransitionOutTimePercent;
	mTransitionDelay = transitionDuration * mTransitionInDelayPercent;
	mTransitionStartTime = (TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - alreadyElapsedTime);

	//because the triggering code assumes the time taken was would be the full amount we adjust the start position to compensate
	if(assumedMaxLength)
	{
		float diffFromFulltime = transitionDuration - mTransitionDuration;
		mTransitionStartTime += diffFromFulltime * 0.5f;
	}

	if (mTransitioning)
	{
		if ( mTransitioningIn)
		{
			if (!mActive)
			{
				// never activated, so just skip transition
				mTransitioning = false;
				return;
			}
			else
			{
				// if activated, but not streamed in, we can just skip the transition too
				if(!mHasStreamingRequestedPagedIn)
				{
					// TODO can we cancel it too, or just let it expire?
					if(!mPreloaded)
						g_DrawableStore.ClearRequiredFlag(mDrawableSlot.Get(), STRFLAG_DONTDELETE);
					
					mTransitioning = false;
					return;
				}

				mTransitioningIn = false;
				mTransitionStartTime -= (1.0f-currentPercent)*mTransitionDuration; 	// adjust time so alpha is consistent with new duration, etc.
			}
		}
		else // we are already transitioning out, just keep going
		{
			mTransitionStartTime -= currentPercent*mTransitionDuration;			// adjust time so alpha is consistent with new duration, etc.
		}
	}
	else
	{
		if (!mActive)
		{
			mTransitioning = false; // no need to transition out, we were not even in.
		}
		else 
		{
			mTransitioningIn = false;
			mTransitioning = true;
		}
	}
}


float CloudHatFragContainer::TransitionOutUpdate(bool instant)
{
	float totalCost = 0.0f;
	if (IsTransitioningOut())
	{
		mTransitioning = false;
		mActive = false;
		float cloudHatTransitionTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - mTransitionStartTime;

		for (int i=mLayers.GetCount()-1; i>=0; i--)
		{
			totalCost += mLayers[i].TransitionOutUpdate(cloudHatTransitionTime, instant);
			mTransitioning = mTransitioning || mLayers[i].IsTransitioningOut();  // if any layer is still transitioning, we're transitioning
			mActive = mActive || mLayers[i].IsActive(); // if any layer is active, the cloud hat is active
		}
	}

	return totalCost;
}

float CloudHatFragLayer::TransitionOutUpdate(float cloudHatTransitionTime, bool instant)
{
	if (mActive)	
	{
		float time = TimeMsToSec(fwTimer::GetTimeInMilliseconds()) - mTransitionStartTime;
		
		if (cloudHatTransitionTime<mTransitionDelay) // keep reseting our start time until our delay is ready.
		{
			mTransitionStartTime += time;
			time = 0.0;
		}

		if (time >= mTransitionDuration)
		{
			Deactivate(instant);
			mTransitioning = false;
			return 0.0f;
		}
		else
		{
			return mCostFactor;
		}
	}
	return 0.0f;
}

// some variable need to be double buffered to prevent popping, etc when script loads a cloud hat.
void CloudHatFragContainer::UpdateBufferedStates()
{
	int buf = CClouds::GetUpdateBuffer();
	mRenderActive[buf] = mActive && mEnabled;

	if (mRenderActive[buf]) // is we're not active, we don't need to update the double buffered layer variables
 	{
		for (int i=0; i<mLayers.GetCount(); i++)
			mLayers[i].UpdateBufferedStates(buf, mTransitionMidPoint);
	}

	////////
	//Animation Layers!
	ScalarV deltaTime = ScalarVFromF32(CLOUDHATMGR->IsSimulationPaused() ? 0.0f : CClock::GetApproxTimeStepMS() / 1000.0f);

	//Update trivial animation accumulator.
	mAngularAccumRot += mAngularVelocity * deltaTime * ScalarV(g_sky.GetCloudHatSpeed());	

	//Don't updated accumulated animation unless animations are enabled.
	if(mEnableAnimations)
	{
		ScalarV windMult = ScalarV(V_ONE);
		if(gEnableWindEffects)
		{
			//Add wind effects to clouds
			//This will determine how much the clouds should rotate based on the wind speed
			Vec3V windVelocity;
			WIND.GetGlobalVelocity(WIND_TYPE_AIR, windVelocity, gUseGustWindForCloudHats, gUseGlobalVariationWindForCloudHats);
			float windSpeed = Mag(windVelocity).Getf();
			windMult = ScalarVFromF32(rage::Clamp((windSpeed - gWindEffectMinSpeed)/ (gWindEffectMaxSpeed - gWindEffectMinSpeed), 0.0f, 1.0f));
		}

		for(int i = 0; i < sNumAnimLayers; ++i)
		{ 
			mAnimUVAccum[i] += mUVVelocity[i] * deltaTime * windMult;
		}
	}

	mAngularAccumRotBuffered[buf] = mAngularAccumRot;  // Buffered versions for the render thread
	for(int i = 0; i < sNumAnimLayers; ++i)
		mAnimUVAccumBuffered[buf][i] = mAnimUVAccum[i];
}

void CloudHatFragLayer::UpdateBufferedStates(int buf, float transitionMidPoint )
{
	mRenderActive[buf] = mActive && mHasStreamingRequestedPagedIn && (mDrawableSlot.IsValid());

	if (mRenderActive[buf])
	{
		mRenderDrawableSlot[buf] = mDrawableSlot;
		
		//Compute transitioned density
		float transitionedInT = 1.0f;
		if (IsTransitioning())
		{
			transitionedInT = IsTransitioningIn() ? GetTransitionPercent() : 1.0f - GetTransitionPercent();

			//Apply transition midpoint scale.
			if (transitionMidPoint > 0.0f && transitionMidPoint < 1.0f)	// divide by zero creates NaNs on pc which don't clamp and propagate into shader constants
			{
				float tans1stHalf = rage::Saturate(transitionedInT  / transitionMidPoint) * 0.5f;
				float tans2ndHalf = rage::Saturate((transitionedInT -  transitionMidPoint) / (1.0f - transitionMidPoint)) * 0.5f;
				transitionedInT = tans1stHalf + tans2ndHalf;
			}
		}

		// some layers fade on with height TODO: only load the layer when we hit the altitude trigger!
		if (mHeightTigger>0 && gVpMan.GetCurrentViewport())
		{
			float heightDelta = gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraPosition().GetZf() - mHeightTigger; // is there a better way to get the camera height?
			if (heightDelta<0)
			{
				transitionedInT = 0.0f; // below the trigger, we draw nothing
			}
			else if (mHeightFadeRange>0)
			{
				transitionedInT *= Clamp(heightDelta/mHeightFadeRange,0.0f,1.0f); // above it we fade in
			}
		}

		if (mHeightTrigger2>0 && gVpMan.GetCurrentViewport())
		{
			const float heightDelta2 = mHeightTrigger2 - gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraPosition().GetZf();
			if( heightDelta2 < 0.0f)
			{
				transitionedInT = 0.0f; // above the trigger, we draw nothing
			}
			else if (mHeightFadeRange2>0)
			{
				transitionedInT *= Clamp(heightDelta2/mHeightFadeRange2,0.0f,1.0f); // above it we fade in
			}
		}

		mRenderTransitionAlpha[buf] = transitionedInT;	
	}
}

void CloudHatFragLayer::PagedIn()
{
	//Add a reference to the streamed in drawable and then remove the donotdelete flag
	Assertf(g_DrawableStore.HasObjectLoaded(mDrawableSlot), "PagedIn() called before drawable has completely streamed in.");
	g_DrawableStore.AddRef(strLocalIndex(mDrawableSlot), REF_RENDER);
	
	if (!mPreloaded)
		g_DrawableStore.ClearRequiredFlag(mDrawableSlot.Get(), STRFLAG_DONTDELETE);

	mVariableCache.InvalidateCachedShaderVariables();

	//Fragment is now streamed in.
	Assertf(IsStreamedIn(), "PagedIn() called. CloudHat fragment should be streamed in now, but I don't think it is!");

	//Save off time to allow transitioning in
	mTransitionStartTime = TimeMsToSec(fwTimer::GetTimeInMilliseconds()); // Wait, this does not take into Account the "alreadyElapsedTime"

	mHasStreamingRequestedPagedIn = true;
}



#if __BANK

void CloudHatFragContainer::LoadFromWidgets()
{
	CLOUDHATMGR->LoadCloudHatByName(mName, CLOUDHATMGR->GetBankTransistionTime());
}

void CloudHatFragContainer::UnloadFromWidgets()
{
	CLOUDHATMGR->UnloadCloudHatByName(mName, CLOUDHATMGR->GetBankTransistionTime());
}

void CloudHatFragContainer::PreloadFromWidgets()
{
	CLOUDHATMGR->PreloadCloudHatByName(mName);
}

void CloudHatFragContainer::ReleasePreloadFromWidgets()
{
	CLOUDHATMGR->ReleasePreloadCloudHatByName(mName);
}

void CloudHatFragContainer::AddWidgets(bkBank &bank)
{
	mGroup = bank.PushGroup(mName);

	//Add buttons to stream CloudHat in/out.
	bank.AddButton("Load CloudHat (script)", datCallback(MFA(CloudHatFragContainer::LoadFromWidgets), this));
	bank.AddButton("Unload CloudHat (script)", datCallback(MFA(CloudHatFragContainer::UnloadFromWidgets), this));
	
	// just for testing, don't want to polute the artist widgets.
	// 	bank.AddButton("PreLoad CloudHat", datCallback(MFA(CloudHatFragContainer::PreloadFromWidgets), this));
	// 	bank.AddButton("Release Preloaded CloudHat", datCallback(MFA(CloudHatFragContainer::ReleasePreloadFromWidgets), this));
	
	// we'll want to pick between loading as script or loading as weather cloudhat, but I have not figured out the loading as weather version yet, so they only load from script
	//bank.AddToggle("Add as Request Script (otherwise as weather request)",&mBankLoadAsScriptRequest)
	// need to set clouds.cpp's sForceOverrideSettingsName to get it to load as weather.
	bank.AddSeparator();

	AddCloudHatWidgets(bank);

	bank.PopGroup();
}

void CloudHatFragContainer::UpdateWidgets()
{
	// TODO - add support for all the layers
	if(mLayers.GetCount()>0 && mLayers[0].IsActive() && mLayers[0].IsStreamedIn() )
	{
		if(mMtlGroup == NULL)
		{
			if(mGroup)
			{
				//Add shader group to bank.
				bkBank &bank = CLOUDHATMGR->GetBank();
				bank.SetCurrentGroup(*mGroup);
				AddMaterialGroup(bank);
				bank.UnSetCurrentGroup(*mGroup);
			}
		}
	}
	else
	{
		if(mMtlGroup != NULL)
		{
			//Remove unnecessary widgets
			bkBank &bank = CLOUDHATMGR->GetBank();
			bank.DeleteGroup(*mMtlGroup);
			mMtlGroup = NULL;
		}
	}
}


bkWidget *CloudHatFragContainer::RemoveWidgets(bkBank &bank)
{
	//Remove all widgets
	bkWidget *next = NULL;

	if(mMtlGroup != NULL)
	{
		bank.DeleteGroup(*mMtlGroup);
		mMtlGroup = NULL;
	}

	if(mGroup != NULL)
	{
		next = mGroup->GetNext();
		bank.DeleteGroup(*mGroup);
		mGroup = NULL;
	}

	return next;
}


void CloudHatFragContainer::UpdateTextures()
{
	std::for_each(BANKONLY_CloudTexArray.begin(), BANKONLY_CloudTexArray.end(), std::mem_fun_ref(&grcTexChangeData::UpdateTextureParams));
}


void CloudHatFragContainer::AddCloudHatWidgets(bkBank &bank)
{
//	static const char *sUnknownStr = "<Unkown>";

	//Add controls for loading and rendering the fragment
	bank.AddToggle("Draw this CloudHat", &mEnabled);
//	const char *filename = mFilename.GetCStr() ? mFilename.GetCStr() : sUnknownStr;
//	u32 filenameLen = (mFilename.GetCStr() ? mFilename.GetLength() : ustrlen(sUnknownStr)) + 1;
//	bank.AddText("Cloud Filename", const_cast<char *>(filename), filenameLen, true); //I know, const_cast is nasty! Just displaying the text so it should be ok.

	bank.AddSeparator();
	bank.AddTitle("Transformation Values");
	bank.AddVector("Position Offset (From Camera)", &mPosition, -1000.0f, 1000.0f, 1.0f);
	bank.AddVector("Scale", &mScale, 0.0f, 10000.0f, 0.01f);
	//bank.AddVector("Rotation", &mRotation, 0.0f, 2.0f * PI, PI / 180.0f);
	bank.AddAngle("Rotation about X", &(mRotation[0]), bkAngleType::RADIANS);
	bank.AddAngle("Rotation about Y", &(mRotation[1]), bkAngleType::RADIANS);
	bank.AddAngle("Rotation about Z", &(mRotation[2]), bkAngleType::RADIANS);
	bank.AddVector("Model Rotation Anim Rate:", &mAngularVelocity, -2.0f * PI, 2.0f * PI, PI / 180.0f);

	//Animation Widgets
	bank.AddSeparator();
	bank.AddToggle("Enable Cloud Animations: (Non-trivial)", &mEnableAnimations);
	bank.AddVector("Animation Blend Weights:", &mAnimBlendWeights, 0.0f, 50.0f, 0.1f);
	bank.AddButton("Reset Animations", datCallback(MFA(CloudHatFragContainer::ResetAnimState), this));
	for(int i = 0; i < sNumAnimLayers; ++i)
	{
		bank.AddTitle("Layer %d:", i);
		bank.AddToggle("Activate Layer:", &mShowLayer[i]);
		bank.AddCombo("Animation Mode:", &mAnimMode[i], NUM_ANIM_MODES, sAnimModeNames);
		bank.AddVector("UV Velocity:" , &mUVVelocity[i], -1.0f, 1.0f, 0.001f);
	}

	//Transition Widgets
	bank.AddSeparator();
	bank.AddTitle("Transition Controls:");
	bank.AddSlider("Transition Alpha Range", &mTransitionAlphaRange, 0.0f, 1.0f, 0.1f);
	bank.AddSlider("Transition Midpoint Rescale", &mTransitionMidPoint, 0.0f, 1.0f, 0.1f);

	//Add shader materials
	bank.AddSeparator();
	AddMaterialGroup(bank);
}

void CloudHatFragContainer::AddMaterialGroup(bkBank & bank)
{
	// TODO Add support for each layer's materials
	if(mLayers.GetCount()>0 && mLayers[0].IsActive() && mLayers[0].IsStreamedIn())
	{
		RAGE_TRACK(CloudHatWidgets);

		if (mLayers[0].IsActive() && mLayers[0].IsStreamedIn())
		{
			//Now, add shader controls for easy tuning
			rmcDrawable *drawable = g_DrawableStore.Get(strLocalIndex(mLayers[0].GetDrawableSlot()));
			if(drawable)
			{
				mMtlGroup = bank.PushGroup("Shaders", false);
	#		if HACK_GTA4 // is this still HACK_GTA4?
				// AddWidgets_WithLoad populates this with relevant info
				// so first clear it.
		
				BANKONLY_CloudTexArray.clear();
			
				bkGroup *lastAdded = NULL;
				grmShaderGroup &shaderGroup = drawable->GetShaderGroup();

				for(u32 i = 0; i < shaderGroup.GetCount(); i++)
				{
					grmShader &shader = shaderGroup.GetShader(i);
					lastAdded = bank.PushGroup(shader.GetName(), true);
					{
						grcInstanceData &data = shader.GetInstanceData();
						data.AddWidgets_WithLoad(bank, BANKONLY_CloudTexArray);
					}
					bank.PopGroup();
				}

				if(lastAdded)
				{
					lastAdded->SetFocus();
				}
	#		else //HACK_GTA4
				drawable->GetShaderGroup().AddWidgets(bank);
	#		endif //HACK_GTA4
				bank.PopGroup();
			}
		}
	}
}

#endif



CloudHatManager *CloudHatManager::sm_instance = NULL;

CloudHatManager::CloudHatManager()
: mCamPositionScaler(V_ONE)
, mAltitudeScrollScaler(0.0f)
, mDesiredTransitionTimeSec(0.0f)
, mCurrentCloudHat(mCloudHatFrags.size())
, mNextCloudHat(mCloudHatFrags.size())
, mIsSimulationPaused(false)	//By default, always animate clouds.
, mUseCloudLighting(false)
, mRequestedWeatherCloudhat(-1)
, mRequestedScriptCloudhat(-1)

#if __BANK
, mBank(NULL)
, mGroup(NULL)
, mDbgGroup(NULL)
, mBlockAutoTransitions(false)
, mBankDesiredTransitionTimeSec(5.0f)
#endif

#if GTA_REPLAY
, m_PreviousTransitionTime(0.0f)
, m_PreviousAssumedMaxLength(false)
#endif //GTA_REPLAY
{
	for(int i=0; i<MAX_NUM_CLOUD_LIGHTS; i++)
	{
		m_cloudLightSources[i] = Lights::m_blackLight;
	}
#if __BANK
	mCurrCloudHatText[0] = 0;
#endif
}

void CloudHatManager::Create()
{
	if(!sm_instance)
	{
		RAGE_TRACK(CloudHatManager);
		sm_instance = rage_new CloudHatManager;
		Assertf(sm_instance, "Unable to allocate CloudHatManager Instance!");
	}

	g_UseKeyFrameCloudLight = PARAM_UseKeyFrameCloudLight.Get();

	BANK_ONLY(BANKONLY_InitFrameOverrideSettings());
}

void CloudHatManager::Destroy()
{
	delete sm_instance;
	sm_instance = NULL;
}

void CloudHatManager::Draw(const CloudFrame &settings, bool reflectionPhaseMode, float globalAlpha)
{
	bool bIsUsingLitTechnique = grcLightState::IsEnabled();
	int previousForcedTechniqueGroupId	= grmShaderFx::GetForcedTechniqueGroupId();

	//Check to see if anyone has set any cloud lights sources
	//If we are in reflection phase, disable the cloud lights sources
	if(mUseCloudLighting && !reflectionPhaseMode)
	{
		//Use lit (draw) technique
		Lights::m_lightingGroup.SetForwardLights(m_cloudLightSources, 4);

		//this will make sure it uses the lit technique
		grcLightState::SetEnabled(true);

		// don't worry about separate optimized non soft technique groups for lighting clouds, they are not very frequent
	}
	else
	{
		//Set all lights to black and technique is unchanged
		Lights::m_lightingGroup.DisableForwardLights();
		grcLightState::SetEnabled(false);

		// if there is no depth, we use a different technique for that never does soft particles (could skip some other stuff too)
		// depth is not available in reflection mode
		if (reflectionPhaseMode) 
		{
			static int s_WaterReflectionTechGroup = -1;
			if (s_WaterReflectionTechGroup)
				s_WaterReflectionTechGroup = grmShaderFx::FindTechniqueGroupId("waterreflection");

			grmShaderFx::SetForcedTechniqueGroupId(s_WaterReflectionTechGroup);
		}
	}		

	//Compute and set cloud settings
	const grcTexture* farDepthTexture	= __XENON? grcTexture::NoneBlack : grcTexture::NoneWhite;
#if RSG_ORBIS
	grcRenderTarget* depthBuffer		= CRenderTargets::GetDepthResolved();
#elif DEVICE_MSAA
	grcRenderTarget* depthBuffer		= GRCDEVICE.GetMSAA() ? CRenderTargets::GetDepthResolved() : CRenderTargets::GetDepthBufferCopy();
#elif __PS3
	grcRenderTarget* depthBuffer		= CRenderTargets::GetDepthBufferAsColor();
#elif __WIN32PC
	grcRenderTarget* depthBuffer		= CRenderTargets::GetDepthBufferCopy();
#else
	grcRenderTarget* depthBuffer		= CRenderTargets::GetDepthBuffer();
#endif

	DisableHiStencilWrites	disWrites;
	grcDepthStencilStateHandle DSS = reflectionPhaseMode ? grcStateBlock::DSS_TestOnly_LessEqual : CShaderLib::DSS_TestOnly_LessEqual_Invert;

#if __BANK && (__D3D11 || __GNM)
	if(ms_bForceWireframe)
	{
		CloudFrame& frame = *((CloudFrame*)&settings);
		frame.m_CloudColor	=
		frame.m_LightColor	=
		frame.m_AmbientColor= 
		frame.m_SkyColor	=
		frame.m_BounceColor	=
		frame.m_EastColor	=
		frame.m_WestColor	= Vec3V(V_ZERO);

		grcStateBlock::SetStates(grcStateBlock::RS_WireFrame,DSS,grcStateBlock::BS_Default);
	}
	else
#endif //__BANK...
	{
		grcStateBlock::SetStates(grcStateBlock::RS_Default,DSS,grcStateBlock::BS_Normal);
	}

	for (int i=0;i<mCloudHatFrags.GetCount();i++)
		mCloudHatFrags[i].Draw(settings, reflectionPhaseMode, reflectionPhaseMode ? farDepthTexture : depthBuffer, globalAlpha);

#if __BANK
	if(ms_bForceWireframe)
	{
		grcStateBlock::SetStates(grcStateBlock::RS_Default,DSS,grcStateBlock::BS_Normal);
	}
#endif

	grmShaderFx::SetForcedTechniqueGroupId(previousForcedTechniqueGroupId);
	grcLightState::SetEnabled(bIsUsingLitTechnique);
}

void CloudHatManager::SetCloudLights(const CLightSource *lightSources)
{
	bool useCloudLighting = false;
	for(int i=0; i<MAX_NUM_CLOUD_LIGHTS; i++)
	{
		m_cloudLightSources[i] = lightSources[i];
		const Color32 lightColor(lightSources[i].GetColorTimesIntensity());
		if(lightColor != Color_black)
		{
			useCloudLighting = true;
		}
	}
	mUseCloudLighting = useCloudLighting;
}

void CloudHatManager::Update(bool skipWeather)
{

	if (!skipWeather)
	{
		////////
		//If functors are set, perform active cloud transition triggering
		if(mTransitionPredicate.IsValid() && mInstantTransitionPredicate.IsValid() &&  mAssumedMaxLengthTransitionPredicate.IsValid() && mNextCloudHatFunctor.IsValid())
		{
			if(mTransitionPredicate() REPLAY_ONLY( && !CReplayMgr::IsReplayInControlOfWorld()))
				RequestWeatherCloudHat(mNextCloudHatFunctor(), mInstantTransitionPredicate() ? 0.0f : mDesiredTransitionTimeSec, mAssumedMaxLengthTransitionPredicate());
		}
	}

	////////
	//Perform transition computations if necessary
	UpdateTransitions();

	////////
	//Let the CloudHatFrags finish up their streaming, etc during the update thread (can't AddRef in the render thread)
	array_type::iterator begin = mCloudHatFrags.begin();
	array_type::iterator end = mCloudHatFrags.end();
	for(array_type::iterator iter = begin; iter != end; ++iter)
	{
		iter->Update();
	}
}


CloudHatManager::size_type CloudHatManager::GetActiveCloudHatIndex() const
{
	const int numCloudHats = GetNumCloudHats();
	for(int index=0; index < numCloudHats; index++)
	{
		const CloudHatFragContainer& cloudHat = GetCloudHat(index);
		if(cloudHat.IsActive())
			return index;
	}

	return -1;

}

bool CloudHatManager::IsSimulationPaused()
{
	if(mSimulationPausedPredicate.IsValid())
	{
		mIsSimulationPaused = mSimulationPausedPredicate();
	}

	return mIsSimulationPaused;
}



#if __BANK
bool CloudHatManager::ms_bForceWireframe = false;

namespace
{
	static const u32 sBANKONLY_MaxPathLen = 256;
	char sBANKONLY_CurrConfigFilename[sBANKONLY_MaxPathLen]		= {'\0'};

	void BANKONLY_SetCurrentConfigFilename(const char *filename, const char *ext = NULL)
	{
		char buff[sBANKONLY_MaxPathLen]; //Use temp buffer in case filename or ext alias path var
		const char *extInFilename = fiAssetManager::FindExtensionInPath(filename);
		if(ext && !extInFilename)
			formatf(buff, "%s.%s", filename, ext);
		else
			formatf(buff, "%s", filename);

		formatf(sBANKONLY_CurrConfigFilename, "%s", buff);
	}

	bool BANKONLY_ShowSelectFileDialog(bool isSaving)
	{
		//Parse the file extension.
		char extFilter[32] = {'\0'};
		const char *ext = fiAssetManager::FindExtensionInPath(sBANKONLY_CurrConfigFilename);
		formatf(extFilter, "*.%s", (ext != NULL ? ++ext : sDefaultConfigFileExtention));

		char filename[sBANKONLY_MaxPathLen];
		bool fileSelected = BANKMGR.OpenFile(filename, 256, extFilter, isSaving, "Vehicle License Plate Config File");

		//Make this file our new config file if selected
		if(fileSelected && filename)
		{
			formatf(sBANKONLY_CurrConfigFilename, "%s", filename);
		}

		//return sBANKONLY_CurrConfigFilename;
		return fileSelected;
	}

	////////////////
	//Support for reloading rag widgets
	//bkGroup *sBANKONLY_MainConfigUiGroup = NULL;

	//Support for saving/loading/reloading config files
	void sBANKONLY_ReloadSettings(CloudHatManager *obj)
	{
		if(obj)
		{
			//Parse the filename into name and extension.
			char configName[sBANKONLY_MaxPathLen] = {'\0'};
			fiAssetManager::RemoveExtensionFromPath(configName, sBANKONLY_MaxPathLen, sBANKONLY_CurrConfigFilename);
			const char *ext = fiAssetManager::FindExtensionInPath(sBANKONLY_CurrConfigFilename);
			if(ext) ++ext;	//FindExtensionInPath returns a pointer to the file extension including the '.' character.

			//Load the settings
			/*bool success =*/ obj->Load(configName, ext);

			//Need to reload bank widgets to reflect newly loaded settings
			obj->ReloadWidgets();
		}
	}

	void sBANKONLY_LoadNewSettings(CloudHatManager *obj)
	{
		if(obj)
		{
			if(BANKONLY_ShowSelectFileDialog(false))
				sBANKONLY_ReloadSettings(obj);
		}
	}

	void sBANKONLY_SaveNewSettings(CloudHatManager *obj)
	{
		if(obj)
		{
			if(BANKONLY_ShowSelectFileDialog(true))
			{
				BANKONLY_SetCurrentConfigFilename(sBANKONLY_CurrConfigFilename);
				obj->Save(sBANKONLY_CurrConfigFilename);
			}
		}
	}
}

void CloudHatManager::AddWidgets(bkBank &bank)
{
	//using namespace std;
	RAGE_TRACK(CloudHatAddWidgets);

	//Cache off bank
	mBank = &bank;
	mGroup = bank.PushGroup("Clouds");
	{
		AddCloudHatWidgetsAndIO(bank);
	}
	bank.PopGroup();
}

void CloudHatManager::AddDebugWidgets(bkBank &bank)
{
	//using namespace std;
	RAGE_TRACK(CloudHatAddWidgets);
	mDbgGroup = AddFrameOverrideWidgets(bank);
}

void CloudHatManager::UpdateWidgets()
{
	std::for_each(mCloudHatFrags.begin(), mCloudHatFrags.end(), std::mem_fun_ref(&array_type::value_type::UpdateWidgets));
}

bool CloudHatManager::Save(const char *filename)
{
	const char *ext = fiAssetManager::FindExtensionInPath(filename);
	ext = (ext != NULL) ? ++ext : sDefaultConfigFileExtention;
	bool success = PARSER.SaveObject(filename, ext, this);

	return success;
}

void CloudHatManager::UpdateTextures()
{
	std::for_each(mCloudHatFrags.begin(), mCloudHatFrags.end(), std::mem_fun_ref(&array_type::value_type::UpdateTextures));
}

void CloudHatManager::ReloadWidgets()
{
	if(mBank && mGroup)
	{
		//Destroy all widgets in the group
		while(bkWidget *child = mGroup->GetChild())
			child->Destroy();

		//Reset bank context and re-add all the widgets
		mBank->SetCurrentGroup(*mGroup);
		AddCloudHatWidgetsAndIO(*mBank);
		mBank->UnSetCurrentGroup(*mGroup);
	}
}

void CloudHatManager::AddCloudHatWidgetsAndIO(bkBank &bank)
{
	static const bool sIsConfigFileReadOnly = false;
	bank.AddText("Config File:", sBANKONLY_CurrConfigFilename, sBANKONLY_MaxPathLen, sIsConfigFileReadOnly);
	bank.AddButton("Reload", datCallback(CFA1(sBANKONLY_ReloadSettings), reinterpret_cast<CallbackData>(this)));
	bank.AddButton("Save", datCallback(MFA1(CloudHatManager::Save), this, reinterpret_cast<CallbackData>(sBANKONLY_CurrConfigFilename)));
	bank.AddButton("Save As...", datCallback(CFA1(sBANKONLY_SaveNewSettings), reinterpret_cast<CallbackData>(this)));
	bank.AddButton("Load New File...", datCallback(CFA1(sBANKONLY_LoadNewSettings), reinterpret_cast<CallbackData>(this)));
	bank.AddSeparator();

	bank.AddToggle("Block Automatic Weather-Based Transitions", &mBlockAutoTransitions);
	bank.AddText("Transition Time (sec):", &mDesiredTransitionTimeSec);
	bank.AddText("Bank Transition Time (sec):", &mBankDesiredTransitionTimeSec);
	bank.AddTitle(" ");

	bank.AddVector("Cam Position Scaler:", &mCamPositionScaler, 0.0f, 1.0f, 0.1f);
	bank.AddSlider("Altitude UV Scroll Scaler:", &mAltitudeScrollScaler, 0.0f, 1.0f, 0.001f);
	bank.AddTitle(" ");

	bank.AddToggle("Wireframe", &ms_bForceWireframe);
	bank.AddToggle("Enable Wind Effects", &gEnableWindEffects);
	bank.AddSlider("Min Speed for Wind", &gWindEffectMinSpeed, 0.0f, 20.0f, 0.001f);
	bank.AddSlider("Max Speed for Wind", &gWindEffectMaxSpeed, 0.0f, 20.0f, 0.001f);
	bank.AddToggle("Use Gust Wind Velocity", &gUseGustWindForCloudHats);
	bank.AddToggle("Use Global Variation Wind Velocity", &gUseGlobalVariationWindForCloudHats);
	bank.AddSlider("Wind Multiplier for Cloud Layer Rotating in Opp Direction", &gWindDirMultForOppDirCloudLayer, 0.0f, 1.0f, 0.001f);
	bank.AddTitle(" ");
	bank.AddText("Active CloudHat:", mCurrCloudHatText, CloudHatFragContainer::kMaxNameLength, true);
	UpdateCurrCloudHatText();

	array_type::iterator begin = mCloudHatFrags.begin();
	array_type::iterator end = mCloudHatFrags.end();
	for(array_type::iterator iter = begin; iter != end; ++iter)
	{
		iter->AddWidgets(bank);
	}

	//Notify the game that cloud widgets have been constructed
	if(mWidgetsChangedFunc.IsValid())
		mWidgetsChangedFunc(bank);
}

CloudFrame &CloudHatManager::GetCloudOverrideSettings()
{
	return sBANKONLY_CloudFrameOverride;
}

void CloudHatManager::SetCloudOverrideSettings(const CloudFrame &settings)
{
	sBANKONLY_CloudFrameOverride = settings;
}

const char *CloudHatManager::GetCloudHatName(size_type index) const
{
	static const char *sNoCloudHat = "NONE";
	return IsCloudHatIndexValid(index) ? GetCloudHat(index).GetName() : sNoCloudHat;
}

void CloudHatManager::UpdateCurrCloudHatText()
{
	safecpy(mCurrCloudHatText, GetCloudHatName(mCurrentCloudHat), CloudHatFragContainer::kMaxNameLength);
}
#endif //__BANK
  
void CloudHatManager::UpdateTransitions()
{	
	float cost = 0.0f;

	// update all the outgoing hats.
	array_type::iterator begin = mCloudHatFrags.begin();
	array_type::iterator end = mCloudHatFrags.end();

	// update the outgoing hats and add up how much cost they still have
	for(array_type::iterator iter = begin; iter != end; ++iter)
	{
		cost += iter->TransitionOutUpdate();
	}

	// streaming the incoming hats with whatever cost factor is available (there should only be 1 incoming hat!)
	for(array_type::iterator iter = begin; iter != end; ++iter)
	{
		iter->TransitionInUpdate(Max(0.0f, 1.0f - cost));
	}

	// check for altitude layers changing status
	if (gVpMan.GetCurrentViewport())
	{
		float height = gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraPosition().GetZf();
		for(array_type::iterator iter = begin; iter != end; ++iter)
		{
			iter->AltitudeUpdate(height);
		}
	}
}

void CloudHatManager::RequestWeatherCloudHat(int index, float transitionTime, bool assumedMaxLength)
{
	CLOUDDEBUG_DISPLAYF("CloudHatManager::RequestWeatherCloudHat(%d, %f) - '%s'",index,transitionTime, IsCloudHatIndexValid(index)?GetCloudHat(index).GetName():"None");

#if GTA_REPLAY
	int previousIndex = mRequestedWeatherCloudhat;
#endif

	mRequestedWeatherCloudhat = index;
	
	if (!IsCloudHatIndexValid(mRequestedScriptCloudhat) && IsCloudHatIndexValid(index)) // we don't load a new hat while script hat is active
	{
		GetCloudHat(index).StartTransitionIn( transitionTime, 0.0f, assumedMaxLength);
		if (transitionTime<=0.0f)
			GetCloudHat(index).Update(true); // give it a chance to load instantly if preloaded...

		for (int i = 0; i < mCloudHatFrags.GetCount(); i++)
		{
			if (i != index)
				GetCloudHat(i).StartTransitionOut( transitionTime,  0.0f, assumedMaxLength);	
		}
	}

	if (transitionTime<=0.0f)
		CloudHatManager::Update(true); // pump the transitions and update the buffered variables if instant request

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx(CPacketRequestCloudHat(false, index, transitionTime, assumedMaxLength, previousIndex, m_PreviousTransitionTime, m_PreviousAssumedMaxLength));
		m_PreviousTransitionTime = transitionTime;
		m_PreviousAssumedMaxLength = assumedMaxLength;
	}
#endif // GTA_REPLAY
}

void CloudHatManager::RequestScriptCloudHat(int index,  float transitionTime  )
{
	CLOUDDEBUG_DISPLAYF("CloudHatManager::RequestScriptCloudHat(%d, %f) - '%s'",index,transitionTime,IsCloudHatIndexValid(index)?GetCloudHat(index).GetName():"None");

#if GTA_REPLAY
	int previousIndex = mRequestedScriptCloudhat;
#endif

	mRequestedScriptCloudhat = index;

	if (IsCloudHatIndexValid(index))
	{
		GetCloudHat(index).StartTransitionIn( transitionTime );
		if (transitionTime<=0.0f)
			GetCloudHat(index).Update(true); // give it a chance to load instantly if preloaded...
	}

	for (int i = 0; i<mCloudHatFrags.GetCount(); i++)
	{
		if (i != index)
			GetCloudHat(i).StartTransitionOut( transitionTime );	
	}

	if (transitionTime<=0.0f)
		CloudHatManager::Update(true); // pump the transitions and update the buffered variables if instant request

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx(CPacketRequestCloudHat(true, index, transitionTime, false, previousIndex, m_PreviousTransitionTime, false));
		m_PreviousTransitionTime = transitionTime;
	}
#endif // GTA_REPLAY
}

void CloudHatManager::ReleaseScriptCloudHat(int UNUSED_PARAM(index), float transitionTime )
{
	CLOUDDEBUG_DISPLAYF("CloudHatManager::ReleaseScriptCloudHat()");

	// return to the last requested weather based cloud hat.
	// we don't really care about the index, just switch back to the weather target
	mRequestedScriptCloudhat = -1;
	RequestWeatherCloudHat(mRequestedWeatherCloudhat, transitionTime, false);
}

// the following 2 function are for script for special cutscene or trailers, the should be used with care!
// request a cloudhat be streaming in now.  it will be released when the CloudHat transitions out naturally or ReleasePreloadCloudHat() is called 
void CloudHatManager::PreloadCloudHatByName(const char *name)
{
#if __BANK
	Displayf("[CHD] CloudHatManager::PreloadCloudHatByName('%s') called from script", name);
#endif

	for (int index=0; index < GetNumCloudHats(); index++)
	{
		const char * cloudHatName = GetCloudHat(index).GetName();
		if (!stricmp(name,cloudHatName))
		{
			GetCloudHat(index).PreLoad();
			break;
		}
	}
}	

// release a preloaded cloudhat.
void CloudHatManager::ReleasePreloadCloudHatByName(const char *name)
{
#if __BANK
	Displayf("[CHD] CloudHatManager::ReleasePreloadCloudHatByName('%s') called from script", name);
#endif

	for (int index=0; index < GetNumCloudHats(); index++)
	{
		const char * cloudHatName = GetCloudHat(index).GetName();
		if (!stricmp(name,cloudHatName))
		{
			// if this clouds hat is not in use, free it
			GetCloudHat(index).DeactivatePreLoad();
			break;
		}
	}
}


// NOTE: this is for the trailer only, not intended for actual game usage...
// this should be changed to replace the weather requested cloud hat, not add to it.
void CloudHatManager::LoadCloudHatByName(const char * name, float transitionTime)
{	 	 
#if __BANK
	Displayf("[CHD] CloudHatManager::LoadCloudHatByName('%s', %f) called from script", name, transitionTime);
#endif

	for (int index=0; index < GetNumCloudHats(); index++)
	{
		const char * cloudHatName = GetCloudHat(index).GetName();
		if (!stricmp(name,cloudHatName))
		{
			RequestScriptCloudHat(index, transitionTime);
			break;
		}
	}
}

// NOTE: this is for the trailer only, not intended for actual game usage...
void CloudHatManager::UnloadCloudHatByName(const char * name, float transitionTime)
{
#if __BANK
	Displayf("[CHD] CloudHatManager::UnloadCloudHatByName('%s', %f) called from script", name, transitionTime);
#endif

	for (int index=0; index < GetNumCloudHats(); index++)
	{
		const char * cloudHatName = GetCloudHat(index).GetName();
		if (!stricmp(name,cloudHatName))
		{
			ReleaseScriptCloudHat(index, transitionTime);
			break;
		}
	}
}

// NOTE: this is for the trailer only, not intended for actual game usage...
void CloudHatManager::UnloadAllCloudHats()
{
#if __BANK
	Displayf("[CHD] CloudHatManager::UnloadAllCloudHats() called from script");
#endif
	// new system: just unload the script cloud hats. same as (ReleaseScriptCloudHat(), but clear the current and next queued script request)
	for (int index=0; index < GetNumCloudHats(); index++)
	{
		GetCloudHat(index).Deactivate();
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx(CPacketUnloadAllCloudHats());
	}
#endif // GTA_REPLAY
}

void CloudHatManager::Flush(bool out)
{
	if (out)
	{
		// just dump everything 
		for (int i = 0; i < mCloudHatFrags.GetCount(); i++)
		{
			GetCloudHat(i).StartTransitionOut( 0.0f );	
			GetCloudHat(i).TransitionOutUpdate(true);	
		}
	}
	else
	{
		// bring back in script hat if it was active, otherwise the weather hat
		if (IsCloudHatIndexValid(mRequestedScriptCloudhat))
			GetCloudHat(mRequestedScriptCloudhat).StartTransitionIn( 0.0f );
		else if (IsCloudHatIndexValid(mRequestedWeatherCloudhat))
			GetCloudHat(mRequestedWeatherCloudhat).StartTransitionIn( 0.0f );
	}
}

bool CloudHatManager::Load(const char *filename, const char *ext)
{
	if(PARAM_disableclouds.Get())
		return false;

	//Need to reset the array. If you don't it's possible that PARSER.LoadObject() may crash if the array is already
	//initialized with at least 1 element and the load causes the array to try to resize to greater than the current capacity.
	//This will eventually be fixed in Rage.
	mCloudHatFrags.Reset();

	ext = (ext != NULL) ? ext : sDefaultConfigFileExtention;
	bool success = PARSER.LoadObject(filename, ext, *this);
	BANK_ONLY(BANKONLY_SetCurrentConfigFilename(filename, ext));

	//Update Manager Settings
	mCurrentCloudHat = mNextCloudHat = GetNumCloudHats();

	return success;
}

} //namespace rage
