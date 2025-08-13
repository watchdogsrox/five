///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	BrightLights.cpp
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	01 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "BrightLights.h"

// Rage headers
#include "grcore/allocscope.h"
#include "grcore/light.h"

// Framework headers
#include "fwmaths/angle.h"
#include "fwmaths/random.h"
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Core/Game.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderThread.h"
#include "scene/lod/LodScale.h"
#include "Shaders/ShaderLib.h"
#include "TimeCycle/TimeCycle.h"
#include "Timecycle/TimeCycleConfig.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Systems/VfxPed.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

dev_s32				BRIGHTLIGHTS_TRAFFIC_VEH_NUM_SEGMENTS	= 20;
dev_float			BRIGHTLIGHTS_TRAFFIC_VEH_SEGMENT_MULT	= 0.11f;

dev_s32				BRIGHTLIGHTS_RAILWAY_TRAFFIC_NUM_SEGMENTS	= 20;
dev_float			BRIGHTLIGHTS_RAILWAY_TRAFFIC_SEGMENT_MULT	= 0.16f;

dev_s32				BRIGHTLIGHTS_RAILWAY_BARRIER_NUM_SEGMENTS	= 8;
dev_float			BRIGHTLIGHTS_RAILWAY_BARRIER_SEGMENT_MULT	= 0.055f;

dev_float			BRIGHTLIGHTS_TRAFFIC_PED_STOP_UV		= 0.486f;
dev_float			BRIGHTLIGHTS_TRAFFIC_PED_GO_UV		= 0.5f;

dev_float 			BRIGHTLIGHTS_TRAFFIC_PED_WIDTH_MULT		= 0.21f;
dev_float 			BRIGHTLIGHTS_TRAFFIC_PED_HEIGHT_MULT	= 0.23f;
dev_float 			BRIGHTLIGHTS_TRAFFIC_PED_FORWARD_MULT_NEAR	= 0.0f;
dev_float 			BRIGHTLIGHTS_TRAFFIC_PED_FORWARD_MULT_FAR	= 0.074f;




///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CBrightLights		g_brightLights;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CBrightLights
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CBrightLights::Init					(unsigned initMode)
{
    if (initMode == INIT_CORE)
    {
	    // init the texture dictionary
		s32 txdSlot = CShaderLib::GetTxdId();

	    // load in the misc textures
	    LoadTextures(txdSlot);
    }
    else if (initMode == INIT_AFTER_MAP_LOADED)
    {
        // reset the counts
	    for (s32 i=0; i<BRIGHTLIGHTS_NUM_BUFFERS; i++)
	    {
		    m_numBrightLights[i] = 0;
	    }
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CBrightLights::Shutdown				(unsigned /*shutdownMode*/)
{
}

///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

void			CBrightLights::Render				()
{
#if __BANK
	if (m_disableRender)
	{
		return;
	}
#endif

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	s32 bufferId = gRenderThreadInterface.GetRenderBuffer();

	// check if we have bright lights to draw
	if (m_numBrightLights[bufferId])
	{
		// set up the graphical state
		grcViewport::SetCurrentWorldIdentity();
		grcLightState::SetEnabled(false);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
		grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_LessEqual_Invert);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

		// render railway crossing light
		CShaderLib::SetGlobalEmissiveScale(m_railwayIntensity);
		grcBindTexture(m_pTexRailwayLightDiffuse);
		for (s32 i=0; i<m_numBrightLights[bufferId]; i++)
		{
			BrightLight_t& currBrightLight = m_brightLights[bufferId][i];

			if (currBrightLight.type==BRIGHTLIGHTTYPE_RAILWAY_TRAFFIC_RED)
			{
				grcBegin(drawTriFan, BRIGHTLIGHTS_RAILWAY_TRAFFIC_NUM_SEGMENTS+2);

				grcVertex(currBrightLight.vPos.GetXf(), currBrightLight.vPos.GetYf(), currBrightLight.vPos.GetZf(), 0.0f, 0.0f, 1.0f, currBrightLight.color, 0.5f, 0.5f);

				for (s32 j=0; j<=BRIGHTLIGHTS_RAILWAY_TRAFFIC_NUM_SEGMENTS; j++)
				{
					const float normalizedRight	= -rage::Sinf(j * (TWO_PI/BRIGHTLIGHTS_RAILWAY_TRAFFIC_NUM_SEGMENTS));
					const float normalizedUp	= rage::Cosf(j * (TWO_PI/BRIGHTLIGHTS_RAILWAY_TRAFFIC_NUM_SEGMENTS));
					const float right			= BRIGHTLIGHTS_RAILWAY_TRAFFIC_SEGMENT_MULT * normalizedRight;
					const float up				= BRIGHTLIGHTS_RAILWAY_TRAFFIC_SEGMENT_MULT * normalizedUp;
					const float textureU		= (normalizedRight * 0.5f) + 0.5f;
					const float textureV		= (-normalizedUp * 0.5f) + 0.5f;

					grcVertex(currBrightLight.vPos.GetXf() + (up * currBrightLight.vUp.GetXf()) + (right * currBrightLight.vRight.GetXf()), 
							  currBrightLight.vPos.GetYf() + (up * currBrightLight.vUp.GetYf()) + (right * currBrightLight.vRight.GetYf()), 
							  currBrightLight.vPos.GetZf() + (up * currBrightLight.vUp.GetZf()) + (right * currBrightLight.vRight.GetZf()),
							  0.0f, 0.0f, 1.0f, currBrightLight.color, textureU, textureV);
				}

				grcEnd();
			}
		}

		// render vehicle traffic lights
		CShaderLib::SetGlobalEmissiveScale(m_globalIntensity);
		grcBindTexture(NULL);
		for (s32 i=0; i<m_numBrightLights[bufferId]; i++)
		{
			BrightLight_t& currBrightLight = m_brightLights[bufferId][i];

			if (currBrightLight.type==BRIGHTLIGHTTYPE_VEH_TRAFFIC_GREEN || 
				currBrightLight.type==BRIGHTLIGHTTYPE_VEH_TRAFFIC_AMBER || 
				currBrightLight.type==BRIGHTLIGHTTYPE_VEH_TRAFFIC_RED)
			{
				grcBegin(drawTriFan, BRIGHTLIGHTS_TRAFFIC_VEH_NUM_SEGMENTS+2);

				grcVertex(currBrightLight.vPos.GetXf(), currBrightLight.vPos.GetYf(), currBrightLight.vPos.GetZf(), 0.0f, 0.0f, 1.0f, currBrightLight.color, 0.0f, 0.0f);

				for (s32 j=0; j<=BRIGHTLIGHTS_TRAFFIC_VEH_NUM_SEGMENTS; j++)
				{

					float right = -BRIGHTLIGHTS_TRAFFIC_VEH_SEGMENT_MULT * rage::Sinf(j * (TWO_PI/BRIGHTLIGHTS_TRAFFIC_VEH_NUM_SEGMENTS));
					float up	=  BRIGHTLIGHTS_TRAFFIC_VEH_SEGMENT_MULT * rage::Cosf(j * (TWO_PI/BRIGHTLIGHTS_TRAFFIC_VEH_NUM_SEGMENTS));

					grcVertex(currBrightLight.vPos.GetXf()+up * currBrightLight.vUp.GetXf()+right * currBrightLight.vRight.GetXf(), 
							  currBrightLight.vPos.GetYf()+up * currBrightLight.vUp.GetYf()+right * currBrightLight.vRight.GetYf(), 
							  currBrightLight.vPos.GetZf()+up * currBrightLight.vUp.GetZf()+right * currBrightLight.vRight.GetZf(),
							  0.0f, 0.0f, 1.0f, currBrightLight.color, 0.0f, 0.0f);
				}

				grcEnd();
			}
			else if (currBrightLight.type==BRIGHTLIGHTTYPE_RAILWAY_BARRIER_RED)
			{
				grcBegin(drawTriFan, BRIGHTLIGHTS_RAILWAY_BARRIER_NUM_SEGMENTS+2);

				grcVertex(currBrightLight.vPos.GetXf(), currBrightLight.vPos.GetYf(), currBrightLight.vPos.GetZf(), 0.0f, 0.0f, 1.0f, currBrightLight.color, 0.0f, 0.0f);

				for (s32 j=0; j<=BRIGHTLIGHTS_RAILWAY_BARRIER_NUM_SEGMENTS; j++)
				{

					float right = -BRIGHTLIGHTS_RAILWAY_BARRIER_SEGMENT_MULT * rage::Sinf(j * (TWO_PI/BRIGHTLIGHTS_RAILWAY_BARRIER_NUM_SEGMENTS));
					float up	=  BRIGHTLIGHTS_RAILWAY_BARRIER_SEGMENT_MULT * rage::Cosf(j * (TWO_PI/BRIGHTLIGHTS_RAILWAY_BARRIER_NUM_SEGMENTS));

					grcVertex(currBrightLight.vPos.GetXf()+up * currBrightLight.vUp.GetXf()+right * currBrightLight.vRight.GetXf(), 
								currBrightLight.vPos.GetYf()+up * currBrightLight.vUp.GetYf()+right * currBrightLight.vRight.GetYf(), 
								currBrightLight.vPos.GetZf()+up * currBrightLight.vUp.GetZf()+right * currBrightLight.vRight.GetZf(),
								0.0f, 0.0f, 1.0f, currBrightLight.color, 0.0f, 0.0f);
				}

				grcEnd();
			}
		}

		// render the pedestrian traffic lights
		grcBindTexture(m_pTexTrafficLightDiffuse);
		for (s32 i=0; i<m_numBrightLights[bufferId]; i++)
		{
			BrightLight_t& currBrightLight = m_brightLights[bufferId][i];

			if (currBrightLight.type==BRIGHTLIGHTTYPE_PED_TRAFFIC_RED || 
				currBrightLight.type==BRIGHTLIGHTTYPE_PED_TRAFFIC_GREEN)
			{
				Vec3V vPos = currBrightLight.vPos;
				Vec3V vRight = currBrightLight.vRight * ScalarVFromF32(BRIGHTLIGHTS_TRAFFIC_PED_WIDTH_MULT);
				Vec3V vUp = currBrightLight.vUp * ScalarVFromF32(BRIGHTLIGHTS_TRAFFIC_PED_HEIGHT_MULT);

				grcBegin(drawTriFan, 4);

				if (currBrightLight.type == BRIGHTLIGHTTYPE_PED_TRAFFIC_RED)
				{
					const Vec3V v0 = vPos + vUp;
					const Vec3V v1 = vPos + vUp - vRight;
					const Vec3V v2 = vPos - vUp - vRight;
					const Vec3V v3 = vPos - vUp;

					grcVertex(VEC3V_ARGS(v0),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	BRIGHTLIGHTS_TRAFFIC_PED_STOP_UV, 0.0f);
					grcVertex(VEC3V_ARGS(v1),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	0.0f, 0.0f);
					grcVertex(VEC3V_ARGS(v2),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	0.0f, 1.0f);
					grcVertex(VEC3V_ARGS(v3),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	BRIGHTLIGHTS_TRAFFIC_PED_STOP_UV, 1.0f);
				}
				else
				{
					const Vec3V v0 = vPos + vUp + vRight;
					const Vec3V v1 = vPos + vUp;
					const Vec3V v2 = vPos - vUp;
					const Vec3V v3 = vPos - vUp + vRight;

					grcVertex(VEC3V_ARGS(v0),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	1.0f, 0.0f);
					grcVertex(VEC3V_ARGS(v1),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	BRIGHTLIGHTS_TRAFFIC_PED_GO_UV, 0.0f);
					grcVertex(VEC3V_ARGS(v2),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	BRIGHTLIGHTS_TRAFFIC_PED_GO_UV, 1.0f);
					grcVertex(VEC3V_ARGS(v3),	0.0f, 0.0f, 1.0f, 	currBrightLight.color, 	1.0f, 1.0f);
				}

				grcEnd();
			}
		}

		// restore the graphical state
		CShaderLib::SetGlobalEmissiveScale(1.0f);

		// reset the count
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateAfterRender
///////////////////////////////////////////////////////////////////////////////

void			CBrightLights::UpdateAfterRender			()
{
	// we want to clear the batchSize in the buffer we have just rendered (batchIndex.e. 1-updatebuffer)
	const s32 bufferId = 1-gRenderThreadInterface.GetUpdateBuffer();
	m_numBrightLights[bufferId] = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  Register
///////////////////////////////////////////////////////////////////////////////

void			CBrightLights::Register				(BrightLightType_e type, Vec3V_In vPos, Vec3V_In vUp, Vec3V_In vRight, Vec3V_In vForward, const Vector3& lightColor, float coronaSize, float coronaIntensity, bool noDistCheck, float alphaValue)
{
	// check that the type is valid
	vfxAssertf(type>BRIGHTLIGHTTYPE_INVALID && type<BRIGHTLIGHTTYPE_MAX, "CBrightLights::Register - invalid type");

	// get the current render buffer
	s32 bufferId = gRenderThreadInterface.GetUpdateBuffer();

	// check if there is space to add this
	if (m_numBrightLights[bufferId] < BRIGHTLIGHTS_MAX)
	{
		// get the distance from the camera
		Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
		float distToCam = Mag(vPos - vCamPos).Getf();
		distToCam /= g_LodScale.GetGlobalScale();

		// check if the bright light is in range 
		float fadeStart = noDistCheck ? m_notDistCulledFadeStartDistance : m_distCulledFadeStartDistance;
		float fadeEnd = noDistCheck ? m_notDistCulledFadeEndDistance : m_distCulledFadeEndDistance;

		if ( distToCam <= fadeEnd || noDistCheck )
		{
			// calc the colour
			float alpha = alphaValue;
			alpha *= 1.0f - Clamp((distToCam - fadeStart)/(fadeEnd - fadeStart),0.0f,1.0f);

			Vec3V camAt = RCC_VEC3V(camInterface::GetMat().b);
			float dotp = Clamp(Dot(vForward, camAt).Getf(),0.0f,1.0f);
			alpha *= dotp;

			Color32 col;
			col.Setf(lightColor.x, lightColor.y, lightColor.z, alpha);

			if( alpha )
			{
				BrightLight_t& currBrightLight = m_brightLights[bufferId][m_numBrightLights[bufferId]];

				// set up the bright light
				currBrightLight.type = type;
				currBrightLight.vPos = vPos;
				currBrightLight.vUp = vUp;
				currBrightLight.vRight = vRight;
				currBrightLight.vForward = vForward;
				currBrightLight.color = col;

				// if this is a ped light, go ahead and calculate the position offset based on the zbias near/far
				if (currBrightLight.type==BRIGHTLIGHTTYPE_PED_TRAFFIC_RED || currBrightLight.type==BRIGHTLIGHTTYPE_PED_TRAFFIC_GREEN)
				{
					float zBiasMult = Lerp(distToCam / 25.0f, BRIGHTLIGHTS_TRAFFIC_PED_FORWARD_MULT_NEAR, BRIGHTLIGHTS_TRAFFIC_PED_FORWARD_MULT_FAR);
					currBrightLight.vPos += (currBrightLight.vForward * ScalarVFromF32(-zBiasMult));
				}

				// increment the count
				m_numBrightLights[bufferId]++;
			}
			
			// add a corona if this is a vehicle traffic lights
			if (type==BRIGHTLIGHTTYPE_VEH_TRAFFIC_GREEN || type==BRIGHTLIGHTTYPE_VEH_TRAFFIC_AMBER || type==BRIGHTLIGHTTYPE_VEH_TRAFFIC_RED)
			{
				Vec3V vFront = -vForward;

				const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
				g_coronas.Register(vPos, 
								   currKeyframe.GetVar(TCVAR_SPRITE_SIZE) * coronaSize, 
								   col, 
								   currKeyframe.GetVar(TCVAR_SPRITE_BRIGHTNESS) * coronaIntensity,
								   m_coronaZBias, 
								   vFront,
								   1.0f,
								   CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
								   CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
								   CORONA_DONT_REFLECT | CORONA_HAS_DIRECTION | CORONA_DONT_RENDER_FLARE);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  LoadTextures
///////////////////////////////////////////////////////////////////////////////

void		 	CBrightLights::LoadTextures			(s32 txdSlot)
{
	// load in the traffic light signs
	m_pTexTrafficLightDiffuse = g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("traffic_light_glow", 0x0e0925b1e));
	vfxAssertf(m_pTexTrafficLightDiffuse, "CBrightLights::LoadTextures - traffic light diffuse texture load failed");

	// load in the railway light textures
	m_pTexRailwayLightDiffuse = g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("traffic_rail_light", 0x6C06C19F));
	vfxAssertf(m_pTexRailwayLightDiffuse, "CBrightLights::LoadTextures - railway light diffuse texture load failed");
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CBrightLights::InitWidgets		()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Bright Lights", false);
	{
#if __DEV
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");

		pVfxBank->AddSlider("Global Dist Culled Fade Start", &m_distCulledFadeStartDistance, 0.0f, 1000.0f, 1.0f);
		pVfxBank->AddSlider("Global Dist Culled Fade End", &m_distCulledFadeEndDistance, 0.0f, 1000.0f, 1.0f);
		pVfxBank->AddSlider("Global Non Dist Culled Fade Start", &m_notDistCulledFadeStartDistance, 0.0f, 1000.0f, 1.0f);
		pVfxBank->AddSlider("Global Non Dist Culled Fade End", &m_notDistCulledFadeEndDistance, 0.0f, 1000.0f, 1.0f);
		pVfxBank->AddSlider("Global Intensity", &m_globalIntensity, 0.0f, 16.0f, 0.1f);
		pVfxBank->AddSlider("Railway Intensity", &m_railwayIntensity, 0.0f, 16.0f, 0.1f);
		pVfxBank->AddSlider("Global Corona", &m_coronaZBias, 0.0f, 16.0f, 0.1f);

		pVfxBank->AddSlider("Veh Traffic Light - Num Segments", &BRIGHTLIGHTS_TRAFFIC_VEH_NUM_SEGMENTS, 0, 360, 1, NullCB, "");
		pVfxBank->AddSlider("Veh Traffic Light - Segment Mult", &BRIGHTLIGHTS_TRAFFIC_VEH_SEGMENT_MULT, 0.0f, 100.0f, 0.5f, NullCB, "");		

		pVfxBank->AddSlider("Railway Traffic Light - Num Segments", &BRIGHTLIGHTS_RAILWAY_TRAFFIC_NUM_SEGMENTS, 0, 360, 1, NullCB, "");
		pVfxBank->AddSlider("Railway Traffic Light - Segment Mult", &BRIGHTLIGHTS_RAILWAY_TRAFFIC_SEGMENT_MULT, 0.0f, 100.0f, 0.01f, NullCB, "");		

		pVfxBank->AddSlider("Railway Barrier Light - Num Segments", &BRIGHTLIGHTS_RAILWAY_BARRIER_NUM_SEGMENTS, 0, 360, 1, NullCB, "");
		pVfxBank->AddSlider("Railway Barrier Light - Segment Mult", &BRIGHTLIGHTS_RAILWAY_BARRIER_SEGMENT_MULT, 0.0f, 100.0f, 0.01f, NullCB, "");		

		pVfxBank->AddSlider("Ped Traffic Light - Width Mult", &BRIGHTLIGHTS_TRAFFIC_PED_WIDTH_MULT, -5.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Ped Traffic Light - Height Mult", &BRIGHTLIGHTS_TRAFFIC_PED_HEIGHT_MULT, -5.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Ped Traffic Light - Forward Mult Near", &BRIGHTLIGHTS_TRAFFIC_PED_FORWARD_MULT_NEAR, 0.0f, 5.0f, 0.001f, NullCB, "");
		pVfxBank->AddSlider("Ped Traffic Light - Forward Mult Far", &BRIGHTLIGHTS_TRAFFIC_PED_FORWARD_MULT_FAR, 0.0f, 5.0f, 0.001f, NullCB, "");

		pVfxBank->AddSlider("Ped Traffic Light - Stop UV", &BRIGHTLIGHTS_TRAFFIC_PED_STOP_UV, 0.0f, 1.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Ped Traffic Light - Walk UV", &BRIGHTLIGHTS_TRAFFIC_PED_GO_UV, 0.0f, 1.0f, 0.05f, NullCB, "");
#endif

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Disable Render", &m_disableRender);
	}
	pVfxBank->PopGroup();
}
#endif


void			CBrightLights::UpdateVisualDataSettings			()
{
	if (g_visualSettings.GetIsLoaded())
	{
		g_brightLights.m_distCulledFadeStartDistance		= g_visualSettings.Get("misc.brightlight.distculled.FadeStart",60.0f);
		g_brightLights.m_distCulledFadeEndDistance			= g_visualSettings.Get("misc.brightlight.distculled.FadeEnd",80.0f);
		g_brightLights.m_notDistCulledFadeStartDistance		= g_visualSettings.Get("misc.brightlight.notdistculled.FadeStart",60.0f);
		g_brightLights.m_notDistCulledFadeEndDistance		= g_visualSettings.Get("misc.brightlight.notdistculled.FadeEnd",80.0f);
		g_brightLights.m_globalIntensity					= g_visualSettings.Get("misc.brightlight.globalIntensity", 12.0f);
		g_brightLights.m_railwayIntensity					= g_visualSettings.Get("misc.brightlight.railwayItensity", 2.5f);
		g_brightLights.m_coronaZBias						= g_visualSettings.Get("misc.brightlight.coronaZBias", 1.0f);
	}
}


