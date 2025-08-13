/*
///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxRipple.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	03 Jan 2007
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxRipple.h"

// Rage headers

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/random.h"
#include "fwscene/stores/txdstore.h"
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Core/Game.h"
#include "renderer/lights/lights.h"
#include "Renderer/PostProcessFX.h"
#include "Renderer/Water.h"
#include "Vfx/vfx_channel.h"
#include "Vfx/Systems/VfxWater.h"
#include "Vfx/VisualEffects.h"
#include "Renderer/Util/ShaderUtil.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

// rag tweakable settings														ped		boat	heli
dev_float		VFXRIPPLE_SPEED_THRESH_A			[VFXRIPPLE_NUM_TYPES]	= {	1.50f,	0.20f,	0.15f	};
dev_s32			VFXRIPPLE_FREQ_A					[VFXRIPPLE_NUM_TYPES]	= {	10,		4,		5		};
dev_float		VFXRIPPLE_START_SIZE_MIN_A			[VFXRIPPLE_NUM_TYPES]	= {	0.00f,	0.30f,	3.0f	};
dev_float		VFXRIPPLE_START_SIZE_MAX_A			[VFXRIPPLE_NUM_TYPES]	= {	0.00f,	0.40f,	5.5f	};
dev_float		VFXRIPPLE_SPEED_MIN_A				[VFXRIPPLE_NUM_TYPES]	= {	0.60f,	0.40f,	21.0f	};
dev_float		VFXRIPPLE_SPEED_MAX_A				[VFXRIPPLE_NUM_TYPES]	= {	0.70f,	0.50f,	25.0f	};
dev_float		VFXRIPPLE_LIFE_MIN_A				[VFXRIPPLE_NUM_TYPES]	= {	1.30f,	1.00f,	0.80f	};
dev_float		VFXRIPPLE_LIFE_MAX_A				[VFXRIPPLE_NUM_TYPES]	= {	1.60f,	2.50f,	0.95f	};
dev_float		VFXRIPPLE_ALPHA_MIN_A				[VFXRIPPLE_NUM_TYPES]	= {	0.22f,	0.40f,	0.20f	};
dev_float		VFXRIPPLE_ALPHA_MAX_A				[VFXRIPPLE_NUM_TYPES]	= {	0.30f,	0.50f,	0.28f	};
dev_float		VFXRIPPLE_PEAK_ALPHA_RATIO_MIN_A	[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.25f,	0.22f	};
dev_float		VFXRIPPLE_PEAK_ALPHA_RATIO_MAX_A	[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.30f,	0.40f	};
dev_float		VFXRIPPLE_ROT_SPEED_MIN_A			[VFXRIPPLE_NUM_TYPES]	= {	-25.0f, -5.0f,	-30.0f	};
dev_float		VFXRIPPLE_ROT_SPEED_MAX_A			[VFXRIPPLE_NUM_TYPES]	= {	25.0f,	5.0f,	30.0f	};
dev_float		VFXRIPPLE_ROT_INIT_MIN_A			[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.0f,	-180.0f	};
dev_float		VFXRIPPLE_ROT_INIT_MAX_A			[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.0f,	180.0f	};
																						
dev_float		VFXRIPPLE_SPEED_THRESH_B			[VFXRIPPLE_NUM_TYPES]	= {	2.50f,	20.00f,	1.00f	};
dev_s32			VFXRIPPLE_FREQ_B					[VFXRIPPLE_NUM_TYPES]	= {	5,		100,	8		};
dev_float		VFXRIPPLE_START_SIZE_MIN_B			[VFXRIPPLE_NUM_TYPES]	= {	0.00f,	0.30f,	8.0f	};
dev_float		VFXRIPPLE_START_SIZE_MAX_B			[VFXRIPPLE_NUM_TYPES]	= {	0.00f,	0.40f,	10.0f	};
dev_float		VFXRIPPLE_SPEED_MIN_B				[VFXRIPPLE_NUM_TYPES]	= {	0.40f,	0.30f,	12.0f	};
dev_float		VFXRIPPLE_SPEED_MAX_B				[VFXRIPPLE_NUM_TYPES]	= {	0.50f,	0.50f,	15.0f	};
dev_float		VFXRIPPLE_LIFE_MIN_B				[VFXRIPPLE_NUM_TYPES]	= {	1.00f,	0.10f,	1.0f	};
dev_float		VFXRIPPLE_LIFE_MAX_B				[VFXRIPPLE_NUM_TYPES]	= {	1.20f,	0.10f,	1.4f	};
dev_float		VFXRIPPLE_ALPHA_MIN_B				[VFXRIPPLE_NUM_TYPES]	= {	0.22f,	0.00f,	0.05f	};
dev_float		VFXRIPPLE_ALPHA_MAX_B				[VFXRIPPLE_NUM_TYPES]	= {	0.30f,	0.00f,	0.09f	};
dev_float		VFXRIPPLE_PEAK_ALPHA_RATIO_MIN_B	[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.0f,	0.22f	};
dev_float		VFXRIPPLE_PEAK_ALPHA_RATIO_MAX_B	[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.0f,	0.30f	};
dev_float		VFXRIPPLE_ROT_SPEED_MIN_B			[VFXRIPPLE_NUM_TYPES]	= {	0.00f,	-0.0f,	-10.0f	};
dev_float		VFXRIPPLE_ROT_SPEED_MAX_B			[VFXRIPPLE_NUM_TYPES]	= {	0.00f,	0.0f,	10.0f	};
dev_float		VFXRIPPLE_ROT_INIT_MIN_B			[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.0f,	-180.0f	};
dev_float		VFXRIPPLE_ROT_INIT_MAX_B			[VFXRIPPLE_NUM_TYPES]	= {	0.0f,	0.0f,	180.0f	};

grcBlendStateHandle			CRippleManager::ms_exitBlendState;
grcDepthStencilStateHandle	CRippleManager::ms_exitDepthStencilState;
grcRasterizerStateHandle	CRippleManager::ms_exitRasterizerState;

grcBlendStateHandle			CRippleManager::ms_renderBlendState;
grcDepthStencilStateHandle	CRippleManager::ms_renderDepthStencilState;
grcDepthStencilStateHandle  CRippleManager::ms_renderStencilOffDepthStencilState;
grcRasterizerStateHandle	CRippleManager::ms_renderRasterizerState;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CRippleManager
///////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
//	Init
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::Init						(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
	    // init the texture dictionary
	    s32 txdSlot = vfxUtil::InitTxd("fxdecal");

	    // load in the misc textures
	    LoadTextures(txdSlot);

		// init render state blocks
		InitRenderStateBlocks();

#if __BANK
	    m_disableRipples = false;
	    m_debugRipplePoints = false;
	    m_numActiveRipples = 0;
#endif
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
	    // init the shader
	    InitShader();
    }
    else if(initMode == INIT_SESSION)
    {
        // initialise the ripple point pool
	    vfxAssertf(m_ripplePointPool.GetNumItems()==0, "CRippleManager::Init - ripple point pool is not empty");
	    for (s32 i=0; i<VFXRIPPLE_MAX_POINTS; i++)
	    {
		    m_ripplePointPool.AddItem(&m_ripplePoints[i]);
	    }
	    vfxAssertf(m_ripplePointPool.GetNumItems()==VFXRIPPLE_MAX_POINTS, "CRippleManager::Init - ripple point pool is not full");

	    // reset the number of registered ripples
	    m_numRegRipples = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////////
//	Shutdown
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::Shutdown					(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	    // shutdown the texture dictionary
	    vfxUtil::ShutdownTxd("fxdecal");
    }
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		// init the shader
		ShutdownShader();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
        // shutdown any active ripples
	    for (s32 i=0; i<VFXRIPPLE_NUM_TYPES; i++)
	    {
		    for (s32 j=0; j<VFXRIPPLE_NUM_TEXTURES; j++)
		    {
			    CRipplePoint* pRipplePoint = static_cast<CRipplePoint*>(m_ripplePointList[i][j].GetHead());
			    while (pRipplePoint)
			    {
				    CRipplePoint* pNextRipplePoint = static_cast<CRipplePoint*>(m_ripplePointList[i][j].GetNext(pRipplePoint));		
    		
				    m_ripplePointList[i][j].RemoveItem(pRipplePoint);
				    ReturnRipplePointToPool(pRipplePoint);

				    pRipplePoint = pNextRipplePoint;
			    }
		    }
	    }

	    vfxAssertf(m_ripplePointPool.GetNumItems()==VFXRIPPLE_MAX_POINTS, "CRippleManager::ShutdownLevel - not all ripple points have been returned to the pool");
	    m_ripplePointPool.RemoveAll();
	    vfxAssertf(m_ripplePointPool.GetNumItems()==0, "CRippleManager::ShutdownLevel - ripple point pool has not emptied correctly");
    }
}


/////////////////////////////////////////////////////////////////////////////////
//	LoadTextures
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::LoadTextures				(s32 txdSlot)
{
	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][0] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple1");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][0], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][1] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple1");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][1], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][2] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple1");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][2], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][3] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple1");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_PED][3], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][0] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple2");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][0], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][1] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple2");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][1], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][2] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple2");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][2], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][3] = g_TxdStore.Get(txdSlot)->Lookup("water_ripple2");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_BOAT_WAKE][3], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][0] = g_TxdStore.Get(txdSlot)->Lookup("water_downwash");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][0], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][1] = g_TxdStore.Get(txdSlot)->Lookup("water_downwash");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][1], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][2] = g_TxdStore.Get(txdSlot)->Lookup("water_downwash");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][2], "CRippleManager::LoadTextures - failed to load ripple texture");

	m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][3] = g_TxdStore.Get(txdSlot)->Lookup("water_downwash");
	vfxAssertf(m_pTexWaterRipplesDiffuse[VFXRIPPLE_TYPE_HELI][3], "CRippleManager::LoadTextures - failed to load ripple texture");
}

/////////////////////////////////////////////////////////////////////////////////
//	Update
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::Update						(float deltaTime)
{
	// go through the registered ripples
	for (s32 i=0; i<m_numRegRipples; i++)
	{
		s32 index = m_pRegRipples[i].rippleType;

		// add the ripple to the list
		CRipplePoint* pNewRipplePoint = GetRipplePointFromPool();
		if (pNewRipplePoint)
		{
			float rippleStartSizeMin = VFXRIPPLE_START_SIZE_MIN_A[index] + ((VFXRIPPLE_START_SIZE_MIN_B[index]-VFXRIPPLE_START_SIZE_MIN_A[index])*m_pRegRipples[i].ratio);
			float rippleStartSizeMax = VFXRIPPLE_START_SIZE_MAX_A[index] + ((VFXRIPPLE_START_SIZE_MAX_B[index]-VFXRIPPLE_START_SIZE_MAX_A[index])*m_pRegRipples[i].ratio);

			float rippleSpeedMin = VFXRIPPLE_SPEED_MIN_A[index] + ((VFXRIPPLE_SPEED_MIN_B[index]-VFXRIPPLE_SPEED_MIN_A[index])*m_pRegRipples[i].ratio);
			float rippleSpeedMax = VFXRIPPLE_SPEED_MAX_A[index] + ((VFXRIPPLE_SPEED_MAX_B[index]-VFXRIPPLE_SPEED_MAX_A[index])*m_pRegRipples[i].ratio);

			float rippleLifeMin = VFXRIPPLE_LIFE_MIN_A[index] + ((VFXRIPPLE_LIFE_MIN_B[index]-VFXRIPPLE_LIFE_MIN_A[index])*m_pRegRipples[i].ratio);
			float rippleLifeMax = VFXRIPPLE_LIFE_MAX_A[index] + ((VFXRIPPLE_LIFE_MAX_B[index]-VFXRIPPLE_LIFE_MAX_A[index])*m_pRegRipples[i].ratio);

			float rippleAlphaMin = VFXRIPPLE_ALPHA_MIN_A[index] + ((VFXRIPPLE_ALPHA_MIN_B[index]-VFXRIPPLE_ALPHA_MIN_A[index])*m_pRegRipples[i].ratio);
			float rippleAlphaMax = VFXRIPPLE_ALPHA_MAX_A[index] + ((VFXRIPPLE_ALPHA_MAX_B[index]-VFXRIPPLE_ALPHA_MAX_A[index])*m_pRegRipples[i].ratio);

			float ripplePeakAlphaMin = VFXRIPPLE_PEAK_ALPHA_RATIO_MIN_A[index] + ((VFXRIPPLE_PEAK_ALPHA_RATIO_MIN_B[index]-VFXRIPPLE_PEAK_ALPHA_RATIO_MIN_A[index])*m_pRegRipples[i].ratio);
			float ripplePeakAlphaMax = VFXRIPPLE_PEAK_ALPHA_RATIO_MAX_A[index] + ((VFXRIPPLE_PEAK_ALPHA_RATIO_MAX_B[index]-VFXRIPPLE_PEAK_ALPHA_RATIO_MAX_A[index])*m_pRegRipples[i].ratio);

			float rippleRotMin = VFXRIPPLE_ROT_SPEED_MIN_A[index] + ((VFXRIPPLE_ROT_SPEED_MIN_B[index]-VFXRIPPLE_ROT_SPEED_MIN_A[index])*m_pRegRipples[i].ratio);
			float rippleRotMax = VFXRIPPLE_ROT_SPEED_MAX_A[index] + ((VFXRIPPLE_ROT_SPEED_MAX_B[index]-VFXRIPPLE_ROT_SPEED_MAX_A[index])*m_pRegRipples[i].ratio);

			float rippleRotInitMin = VFXRIPPLE_ROT_INIT_MIN_A[index] + ((VFXRIPPLE_ROT_INIT_MIN_B[index]-VFXRIPPLE_ROT_INIT_MIN_A[index])*m_pRegRipples[i].ratio);
			float rippleRotInitMax = VFXRIPPLE_ROT_INIT_MAX_A[index] + ((VFXRIPPLE_ROT_INIT_MAX_B[index]-VFXRIPPLE_ROT_INIT_MAX_A[index])*m_pRegRipples[i].ratio);

			s32 listIndex = 0;
			if (m_pRegRipples[i].ratio>0.0f)
			{
				listIndex = g_DrawRand.GetRanged(1, 3);
			}

			m_ripplePointList[index][listIndex].AddItem(pNewRipplePoint);
			pNewRipplePoint->m_currLife = 0.0f;
			pNewRipplePoint->m_maxLife = g_DrawRand.GetRanged(rippleLifeMin, rippleLifeMax) * m_pRegRipples[i].lifeScale;
			pNewRipplePoint->m_speed = g_DrawRand.GetRanged(rippleSpeedMin, rippleSpeedMax);
			pNewRipplePoint->m_maxAlpha = g_DrawRand.GetRanged(rippleAlphaMin, rippleAlphaMax);
			pNewRipplePoint->m_peakAlpha = g_DrawRand.GetRanged(ripplePeakAlphaMin, ripplePeakAlphaMax);;
			pNewRipplePoint->m_startSize = g_DrawRand.GetRanged(rippleStartSizeMin, rippleStartSizeMax);
			pNewRipplePoint->m_vPos = m_pRegRipples[i].vPos;

			pNewRipplePoint->m_theta = ( DtoR * g_DrawRand.GetRanged(rippleRotMin, rippleRotMax));
			pNewRipplePoint->m_thetaInit = ( DtoR * g_DrawRand.GetRanged(rippleRotInitMin, rippleRotInitMax));

			pNewRipplePoint->m_flipTex = m_pRegRipples[i].flipTex;

			pNewRipplePoint->m_vDir.SetXf(m_pRegRipples[i].vDir.GetXf());
			pNewRipplePoint->m_vDir.SetYf(m_pRegRipples[i].vDir.GetYf());			

			// update the initial rotation - offset from the initial direction passed in 
			if (pNewRipplePoint->m_thetaInit!=0.0f)
			{
				float newRot = pNewRipplePoint->m_thetaInit;
				float newX = rage::Cosf(newRot)*pNewRipplePoint->m_vDir.GetXf() - rage::Sinf(newRot)*pNewRipplePoint->m_vDir.GetYf(); 
				float newY = rage::Sinf(newRot)*pNewRipplePoint->m_vDir.GetXf() + rage::Cosf(newRot)*pNewRipplePoint->m_vDir.GetYf();
				pNewRipplePoint->m_vDir.SetXf(newX);
				pNewRipplePoint->m_vDir.SetYf(newY);
			}

			pNewRipplePoint->m_vDir = Normalize(pNewRipplePoint->m_vDir);

			// tweak the heli ripple alpha so it fades out at the heli gets higher from the water
			if (m_pRegRipples[i].rippleType==VFXRIPPLE_TYPE_HELI)
			{
				#define HELI_FADE_RATIO (0.8f)
				if (m_pRegRipples[i].ratio>HELI_FADE_RATIO)
				{
					pNewRipplePoint->m_maxAlpha *= ((1.0f-m_pRegRipples[i].ratio)/(1.0f-HELI_FADE_RATIO));
				}
			}

// 			if (m_pRegRipples[i].ratio<=0.0f)
// 			{
// 				GetRandomVector2d(pNewRipplePoint->m_dir);
// 				pNewRipplePoint->m_theta = ( DtoR * g_DrawRand.GetRanged(VFXRIPPLE_ROT_SPEED_MIN_A[index], VFXRIPPLE_ROT_SPEED_MAX_A[index]));
// 			}
// 			else
// 			{
// 				pNewRipplePoint->m_dir.x = m_pRegRipples[i].dir.x;
// 				pNewRipplePoint->m_dir.y = m_pRegRipples[i].dir.y;
// 				pNewRipplePoint->m_dir.Normalize();
// 				pNewRipplePoint->m_theta = 0.0f;
// 			}
		}
	}

	// reset the number of registered ripples
	m_numRegRipples = 0;

	// update the ripples
	for (s32 i=0; i<VFXRIPPLE_NUM_TYPES; i++)
	{
		for (s32 j=0; j<VFXRIPPLE_NUM_TEXTURES; j++)
		{
			CRipplePoint* pRipplePoint = static_cast<CRipplePoint*>(m_ripplePointList[i][j].GetHead());
			while (pRipplePoint)
			{
				CRipplePoint* pNextRipplePoint = static_cast<CRipplePoint*>(m_ripplePointList[i][j].GetNext(pRipplePoint));

				// update life
				pRipplePoint->m_currLife += deltaTime;

				// update rotation
				if (pRipplePoint->m_theta!=0.0f)
				{
					float actualRot = pRipplePoint->m_theta * deltaTime;
					float newX = rage::Cosf(actualRot)*pRipplePoint->m_vDir.GetXf() - rage::Sinf(actualRot)*pRipplePoint->m_vDir.GetYf(); 
					float newY = rage::Sinf(actualRot)*pRipplePoint->m_vDir.GetXf() + rage::Cosf(actualRot)*pRipplePoint->m_vDir.GetYf();

					pRipplePoint->m_vDir.SetXf(newX);
					pRipplePoint->m_vDir.SetYf(newY);
				}

				// check for expired ripples
				if (pRipplePoint->m_currLife>pRipplePoint->m_maxLife)
				{
					// expired - remove
					m_ripplePointList[i][j].RemoveItem(pRipplePoint);
					ReturnRipplePointToPool(pRipplePoint);
				}

				// update z pos
				float waterLevel;
				if (Water::GetWaterLevel(RCC_VECTOR3(pRipplePoint->m_vPos), &waterLevel, true, POOL_DEPTH, REJECTIONABOVEWATER,NULL))
				{
					pRipplePoint->m_vPos.SetZ(waterLevel);
				}

				pRipplePoint = pNextRipplePoint;
			}
		}
	}

#if __BANK
	m_numActiveRipples = 0;
	for (s32 i=0; i<VFXRIPPLE_NUM_TYPES; i++)
	{
		for (s32 j=0; j<VFXRIPPLE_NUM_TEXTURES; j++)
		{
			m_numActiveRipples += m_ripplePointList[i][j].GetNumItems();
		}
	}
#endif
}


/////////////////////////////////////////////////////////////////////////////////
//	InitShader
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::InitShader					()
{
	m_pShader = grmShaderFactory::GetInstance().Create();
	vfxAssertf(m_pShader, "CRippleManager::Init - cannot create 'projtex2' shader");
	if (!m_pShader->Load("projtex2"))
	{
		vfxAssertf(0, "CRippleManager::Init - error loading 'projtex2' shader");
	}

	m_shaderVarIdDecalTexture = m_pShader->LookupVar("decalTexture");
	m_shaderVarIdDepthTexture = m_pShader->LookupVar("gbufferTextureDepth");
	m_shaderVarIdOOScreenSize = m_pShader->LookupVar("deferredLightScreenSize");
	m_shaderVarIdProjParams = m_pShader->LookupVar("deferredProjectionParams");
}


/////////////////////////////////////////////////////////////////////////////////
//	ShutdownShader
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::ShutdownShader				()
{
	delete m_pShader;
}


/////////////////////////////////////////////////////////////////////////////////
//	SetShaderVarsConst
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::SetShaderVarsConst			()
{
	//use postfx depth buffer (includes water)
#if __PPU
	//CDeferredLightingHelper::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetPatchedDepthBuffer());
	m_pShader->SetVar(m_shaderVarIdDepthTexture, CRenderTargets::GetPatchedDepthBuffer());
#else // __PPU
	//CDeferredLightingHelper::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBuffer());
	m_pShader->SetVar(m_shaderVarIdDepthTexture, CRenderTargets::GetDepthBuffer());
#endif // __PPU

	// calc and set the one over screen size shader var
	Vec4V vOOScreenSize(0.0f, 0.0f, 1.0f/GRCDEVICE.GetWidth(), 1.0f/GRCDEVICE.GetHeight());
	m_pShader->SetVar(m_shaderVarIdOOScreenSize, RCC_VECTOR4(vOOScreenSize));

	const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	m_pShader->SetVar(m_shaderVarIdProjParams, projParams);
}


/////////////////////////////////////////////////////////////////////////////////
//	StartShader
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::StartShader					(s32 i, s32 j)
{				
	//CDeferredLightingHelper::SetDeferredTexture(m_pTexWaterRipplesDiffuse[i][j]);
	m_pShader->SetVar(m_shaderVarIdDecalTexture, m_pTexWaterRipplesDiffuse[i][j]);

	//CDeferredLightingHelper::StartShader(deflight_waterfx,0);
	decalVerifyf(m_pShader->BeginDraw(grmShader::RMC_DRAW, false), "shader begin draw call failed");
	m_pShader->Bind(0);
}


/////////////////////////////////////////////////////////////////////////////////
//	EndShader
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::EndShader					()
{
	//CDeferredLightingHelper::EndShader();
	m_pShader->UnBind();
	m_pShader->EndDraw();
}


/////////////////////////////////////////////////////////////////////////////////
//	Render
/////////////////////////////////////////////////////////////////////////////////


void CRippleManager::Render()
{	
	// calc the camera height above water
	float camHeightAboveWater = 20.0f;
	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	float waterZ;
	if (Water::GetWaterLevelNoWaves(RCC_VECTOR3(vCamPos), &waterZ, POOL_DEPTH, REJECTIONABOVEWATER,NULL))
	{
		camHeightAboveWater = Max(0.0f, vCamPos.GetZf()-waterZ);
	}

	// set up the constant shader variables (they don't change per ripple)
	SetShaderVarsConst();

	static dev_bool debugEnableStencil=true;

	grcStateBlock::SetRasterizerState(ms_renderRasterizerState);
	grcStateBlock::SetBlendState(ms_renderBlendState);
	grcStateBlock::SetDepthStencilState(debugEnableStencil ? ms_renderDepthStencilState : ms_renderStencilOffDepthStencilState);

	grcWorldIdentity();

	grcLightState::SetEnabled(true);
	Lights::m_lightingGroup.SetAmbientLightingGlobals();

	// reset global alpha
	CShaderLib::ResetStippleAlpha();
	CShaderLib::SetGlobalAlpha(1.0f);
	CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);

	for (s32 i=0; i<VFXRIPPLE_NUM_TYPES; i++)
	{
		for (s32 j=0; j<VFXRIPPLE_NUM_TEXTURES; j++)
		{
			s32 numRipples = m_ripplePointList[i][j].GetNumItems();
			if (numRipples>0)
			{
				StartShader(i, j);

				s32 numRipplesLeft = numRipples;
				s32 numRipplesThisPass = numRipples;
				CRipplePoint* pRipplePoint = static_cast<CRipplePoint*>(m_ripplePointList[i][j].GetHead());

				int numStripVertsPerRipple=20;
				int stripIndex[20]={0,0,1,3,2,7,6,4,5,5,2,2,6,1,5,0,4,3,7,7};

				while (numRipplesLeft>0)
				{
					if ((numRipplesThisPass*numStripVertsPerRipple)>grcBeginMax)
					{
						numRipplesThisPass = grcBeginMax/numStripVertsPerRipple;
					}

					grcBegin(drawTriStrip, numRipplesThisPass*numStripVertsPerRipple);

					// render the ripples
					for (s32 k=0; k<numRipplesThisPass; k++)
					{	
						float size = pRipplePoint->m_startSize + pRipplePoint->m_currLife * pRipplePoint->m_speed;

						float lifeRatio = pRipplePoint->m_currLife/pRipplePoint->m_maxLife;

						float alphaFade;
						if (lifeRatio<pRipplePoint->m_peakAlpha)
						{
							alphaFade = lifeRatio/pRipplePoint->m_peakAlpha;
						}
						else
						{
							alphaFade = (1.0f-lifeRatio)/(1.0f-pRipplePoint->m_peakAlpha);
						}

						float alpha = alphaFade * pRipplePoint->m_maxAlpha;
						
						Vec3V vVerts[8];
						Vec3V vHeightVec(0.0f, 0.0f, 3.0f);
						Vec3V vForwardVec(pRipplePoint->m_vDir.GetXY(), ScalarV(V_ZERO));
						Vec3V vSideVec(pRipplePoint->m_vDir.GetY(), -pRipplePoint->m_vDir.GetX(), ScalarV(V_ZERO));

						vForwardVec *= ScalarVFromF32(size);
						vSideVec *= ScalarVFromF32(size);

						vVerts[0] = pRipplePoint->m_vPos + vForwardVec - vSideVec + vHeightVec;
						vVerts[1] = pRipplePoint->m_vPos + vForwardVec + vSideVec + vHeightVec;
						vVerts[2] = pRipplePoint->m_vPos - vForwardVec + vSideVec + vHeightVec;
						vVerts[3] = pRipplePoint->m_vPos - vForwardVec - vSideVec + vHeightVec;	

						vVerts[4] = vVerts[0] - ScalarV(V_TWO)*vHeightVec;		
						vVerts[5] = vVerts[1] - ScalarV(V_TWO)*vHeightVec;		
						vVerts[6] = vVerts[2] - ScalarV(V_TWO)*vHeightVec;		
						vVerts[7] = vVerts[3] - ScalarV(V_TWO)*vHeightVec;	

#if __PPU
						dev_float debugScale=0.99f;
#else
						dev_float debugScale=1.0f;
#endif

						vForwardVec.SetXf(vForwardVec.GetXf()*debugScale);
						vForwardVec.SetYf(vForwardVec.GetYf()*debugScale);


						// decrease the size of heli ripples if the camera is close to the water and flat
						dev_float fadeHeight = 5.0f;
						dev_float endFadeHeight = 1.0f;
						float posScale = 1.0f;
						if (i==VFXRIPPLE_TYPE_HELI && camHeightAboveWater<fadeHeight)
						{
							float scale = Clamp((camHeightAboveWater-endFadeHeight)/(fadeHeight - endFadeHeight),0.0f,1.0f);
							vForwardVec.SetXf(vForwardVec.GetXf()*scale);
							vForwardVec.SetY(vForwardVec.GetYf()*scale);

							alpha *= scale;
							posScale = (scale == 0.0f) ? 0.0f : 1.0f;
						}

						Color32 colflip((pRipplePoint->m_flipTex)?1.0f:0.0f, 1.0f, 1.0f, alpha);

						for (s32 v=0; v<numStripVertsPerRipple; v++)
						{	
							int index=stripIndex[v];
							grcVertex(vVerts[index].GetXf()*posScale, vVerts[index].GetYf()*posScale, vVerts[index].GetZf()*posScale, pRipplePoint->m_vPos.GetXf(), pRipplePoint->m_vPos.GetYf(), pRipplePoint->m_vPos.GetZf(), colflip, vForwardVec.GetXf(), vForwardVec.GetYf());
						}
#if __BANK
						if (g_vfxWater.GetRippleInterface().m_debugRipplePoints)
						{
							Color32 col(1.0f, 1.0f, 1.0f, 1.0f);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[0]), RCC_VECTOR3(vVerts[1]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[1]), RCC_VECTOR3(vVerts[2]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[2]), RCC_VECTOR3(vVerts[3]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[3]), RCC_VECTOR3(vVerts[0]), col);

							grcDebugDraw::Line(RCC_VECTOR3(vVerts[4]), RCC_VECTOR3(vVerts[5]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[5]), RCC_VECTOR3(vVerts[6]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[6]), RCC_VECTOR3(vVerts[6]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[7]), RCC_VECTOR3(vVerts[4]), col);

							grcDebugDraw::Line(RCC_VECTOR3(vVerts[0]), RCC_VECTOR3(vVerts[4]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[1]), RCC_VECTOR3(vVerts[5]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[2]), RCC_VECTOR3(vVerts[6]), col);
							grcDebugDraw::Line(RCC_VECTOR3(vVerts[3]), RCC_VECTOR3(vVerts[7]), col);
						}
#endif 

						pRipplePoint = static_cast<CRipplePoint*>(m_ripplePointList[i][j].GetNext(pRipplePoint));
					}

					grcEnd();

					numRipplesLeft -= numRipplesThisPass;
					numRipplesThisPass = numRipplesLeft;
				}

				EndShader();
			}
		}
	}

	grcStateBlock::SetRasterizerState(ms_exitRasterizerState);
	grcStateBlock::SetBlendState(ms_exitBlendState);
	grcStateBlock::SetDepthStencilState(ms_exitDepthStencilState);

	//restore gbuffer depth
//#if __PPU
//	CDeferredLightingHelper::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetPatchedDepthBuffer());
//#else // __PPU
//	//CDeferredLightingHelper::SetShaderGBufferTarget(GBUFFER_DEPTH, CDeferredLightingHelper::GetGBufferTarget(GBUFFER_DEPTH));
//	m_pShader->SetVar(m_shaderVarIdDepthTexture, CDeferredLightingHelper::GetGBufferTarget(4));
//#endif
}


/////////////////////////////////////////////////////////////////////////////////
//	Register
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::Register					(s32 rippleType, Vec3V_In vPos, Vec3V_In vDir, float lifeScale, float ratio, bool flipTexture)
{
#if __BANK
	if (!m_disableRipples)
#endif
	{
		if (m_numRegRipples<VFXRIPPLE_MAX_REGISTERED)
		{
			m_pRegRipples[m_numRegRipples].rippleType = rippleType;
			m_pRegRipples[m_numRegRipples].vPos = vPos;
			m_pRegRipples[m_numRegRipples].vDir = vDir;
			m_pRegRipples[m_numRegRipples].lifeScale = lifeScale;
			m_pRegRipples[m_numRegRipples].ratio = ratio;
			m_pRegRipples[m_numRegRipples].flipTex = flipTexture;
			m_numRegRipples++;
		}
//#if __ASSERT
//		else
//		{
//			Assertf(0, "Not enough space to register ripple");
//		}
//#endif
	}
}



/////////////////////////////////////////////////////////////////////////////////
//	GetRipplePointFromPool
/////////////////////////////////////////////////////////////////////////////////

CRipplePoint*	CRippleManager::GetRipplePointFromPool		()
{
	return static_cast<CRipplePoint*>(m_ripplePointPool.RemoveHead());
}


/////////////////////////////////////////////////////////////////////////////////
//	ReturnRipplePointToPool
/////////////////////////////////////////////////////////////////////////////////

void			CRippleManager::ReturnRipplePointToPool		(CRipplePoint* pRipplePoint)
{
	m_ripplePointPool.AddItem(pRipplePoint);
}


/////////////////////////////////////////////////////////////////////////////////
//	InitWidgets
/////////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CRippleManager::InitWidgets			()
{
	bkBank* pVfxBank = vfxWidget::GetBank();

	pVfxBank->PushGroup("Vfx Ripple", false);
	{

		pVfxBank->AddTitle("INFO");
		pVfxBank->AddSlider("Num Active Ripples", &m_numActiveRipples, 0, VFXRIPPLE_MAX_POINTS, 0);

#if __DEV
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");
		pVfxBank->PushGroup("Ped Ripples", false);
		{
			InitSettingWidgets(pVfxBank, VFXRIPPLE_TYPE_PED);
		}
		pVfxBank->PopGroup();

		pVfxBank->PushGroup("Boat Wake Ripples", false);
		{
			InitSettingWidgets(pVfxBank, VFXRIPPLE_TYPE_BOAT_WAKE);
		}
		pVfxBank->PopGroup();

		pVfxBank->PushGroup("Heli Ripples", false);
		{
			InitSettingWidgets(pVfxBank, VFXRIPPLE_TYPE_HELI);
		}
		pVfxBank->PopGroup();
#endif

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Disable Ripples", &m_disableRipples);
		pVfxBank->AddToggle("Debug Ripple Points", &m_debugRipplePoints);
	}
	pVfxBank->PopGroup();
}
#endif


/////////////////////////////////////////////////////////////////////////////////
//	InitSettingWidgets
/////////////////////////////////////////////////////////////////////////////////

#if __BANK
#if __DEV
void			CRippleManager::InitSettingWidgets			(bkBank* pVfxBank, s32 rippleType)
{
	if (rippleType==VFXRIPPLE_TYPE_HELI)
	{
		pVfxBank->AddSlider("Height Thresh A", &VFXRIPPLE_SPEED_THRESH_A[rippleType], 0.0f, 30.0f, 0.1f, NullCB, "The height at which the base settings are used");
	}
	else
	{
		pVfxBank->AddSlider("Speed Thresh A", &VFXRIPPLE_SPEED_THRESH_A[rippleType], 0.0f, 30.0f, 0.1f, NullCB, "The speed at which the base settings are used");
	}

	pVfxBank->AddSlider("Freq A", &VFXRIPPLE_FREQ_A[rippleType], 1, 100, 1, NullCB, "The base frequence of ripple");
	pVfxBank->AddSlider("Start Size Min A", &VFXRIPPLE_START_SIZE_MIN_A[rippleType], 0.0f, 20.0f, 0.1f, NullCB, "The base start size (min) of the ripple");
	pVfxBank->AddSlider("Start Size Max A", &VFXRIPPLE_START_SIZE_MAX_A[rippleType], 0.0f, 20.0f, 0.1f, NullCB, "The base start size (max) of the ripple");

	if (rippleType==VFXRIPPLE_TYPE_HELI)
	{
		pVfxBank->AddSlider("Speed Min A", &VFXRIPPLE_SPEED_MIN_A[rippleType], 0.1f, 30.0f, 0.1f, NullCB, "The base speed (min) of the ripple");
		pVfxBank->AddSlider("Speed Max A", &VFXRIPPLE_SPEED_MAX_A[rippleType], 0.1f, 30.0f, 0.1f, NullCB, "The base speed (max) of the ripple");
	}
	else
	{
		pVfxBank->AddSlider("Speed Min A", &VFXRIPPLE_SPEED_MIN_A[rippleType], 0.1f, 3.0f, 0.1f, NullCB, "The base speed (min) of the ripple");
		pVfxBank->AddSlider("Speed Max A", &VFXRIPPLE_SPEED_MAX_A[rippleType], 0.1f, 3.0f, 0.1f, NullCB, "The base speed (max) of the ripple");
	}

	pVfxBank->AddSlider("Life Min A", &VFXRIPPLE_LIFE_MIN_A[rippleType], 0.1f, 10.0f, 0.1f, NullCB, "The base life (min) of the ripple");
	pVfxBank->AddSlider("Life Max A", &VFXRIPPLE_LIFE_MAX_A[rippleType], 0.1f, 10.0f, 0.1f, NullCB, "The base life (max) of the ripple");
	pVfxBank->AddSlider("Max Alpha Min A", &VFXRIPPLE_ALPHA_MIN_A[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The base start alpha (min) of the ripple");
	pVfxBank->AddSlider("Max Alpha Max A", &VFXRIPPLE_ALPHA_MAX_A[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The base start alpha (max) of the ripple");
	pVfxBank->AddSlider("Peak Alpha Ratio Min A", &VFXRIPPLE_PEAK_ALPHA_RATIO_MIN_A[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The base end alpha (min) of the ripple");
	pVfxBank->AddSlider("Peak Alpha Ratio Max A", &VFXRIPPLE_PEAK_ALPHA_RATIO_MAX_A[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The base end alpha (max) of the ripple");
	pVfxBank->AddSlider("Rot Speed Min A", &VFXRIPPLE_ROT_SPEED_MIN_A[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The base rotation speed (min) of the ripple");
	pVfxBank->AddSlider("Rot Speed Max A", &VFXRIPPLE_ROT_SPEED_MAX_A[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The base rotation speed (max) of the ripple");
	pVfxBank->AddSlider("Rot Init Min A", &VFXRIPPLE_ROT_INIT_MIN_A[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The base initial angle (min) of the ripple");
	pVfxBank->AddSlider("Rot Init Max A", &VFXRIPPLE_ROT_INIT_MAX_A[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The base initial angle (max) of the ripple");




	if (rippleType==VFXRIPPLE_TYPE_HELI)
	{
		pVfxBank->AddSlider("Height Thresh B", &VFXRIPPLE_SPEED_THRESH_B[rippleType], 0.0f, 30.0f, 0.1f, NullCB, "The height at which the evolved settings are used");
	}
	else
	{
		pVfxBank->AddSlider("Speed Thresh B", &VFXRIPPLE_SPEED_THRESH_B[rippleType], 0.0f, 30.0f, 0.1f, NullCB, "The speed at which the evolved settings are used");
	}

	pVfxBank->AddSlider("Freq B", &VFXRIPPLE_FREQ_B[rippleType], 1, 100, 1, NullCB, "The evolved frequence of ripple");
	pVfxBank->AddSlider("Start Size Min B", &VFXRIPPLE_START_SIZE_MIN_B[rippleType], 0.0f, 20.0f, 0.1f, NullCB, "The evolved start size (min) of the ripple");
	pVfxBank->AddSlider("Start Size Max B", &VFXRIPPLE_START_SIZE_MAX_B[rippleType], 0.0f, 20.0f, 0.1f, NullCB, "The evolved start size (max) of the ripple");

	if (rippleType==VFXRIPPLE_TYPE_HELI)
	{
		pVfxBank->AddSlider("Speed Min B", &VFXRIPPLE_SPEED_MIN_B[rippleType], 0.1f, 30.0f, 0.1f, NullCB, "The evolved speed (min) of the ripple");
		pVfxBank->AddSlider("Speed Max B", &VFXRIPPLE_SPEED_MAX_B[rippleType], 0.1f, 30.0f, 0.1f, NullCB, "The evolved speed (max) of the ripple");
	}
	else
	{
		pVfxBank->AddSlider("Speed Min B", &VFXRIPPLE_SPEED_MIN_B[rippleType], 0.1f, 3.0f, 0.1f, NullCB, "The evolved speed (min) of the ripple");
		pVfxBank->AddSlider("Speed Max B", &VFXRIPPLE_SPEED_MAX_B[rippleType], 0.1f, 3.0f, 0.1f, NullCB, "The evolved speed (max) of the ripple");
	}
	pVfxBank->AddSlider("Life Min B", &VFXRIPPLE_LIFE_MIN_B[rippleType], 0.1f, 10.0f, 0.1f, NullCB, "The evolved life (min) of the ripple");
	pVfxBank->AddSlider("Life Max B", &VFXRIPPLE_LIFE_MAX_B[rippleType], 0.1f, 10.0f, 0.1f, NullCB, "The evolved life (max) of the ripple");
	pVfxBank->AddSlider("Max Alpha Min B", &VFXRIPPLE_ALPHA_MIN_B[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The evolved start alpha (min) of the ripple");
	pVfxBank->AddSlider("Max Alpha Max B", &VFXRIPPLE_ALPHA_MAX_B[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The evolved start alpha (max) of the ripple");
	pVfxBank->AddSlider("Peak Alpha Ratio Min B", &VFXRIPPLE_PEAK_ALPHA_RATIO_MIN_B[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The evolved end alpha (min) of the ripple");
	pVfxBank->AddSlider("Peak Alpha Ratio Max B", &VFXRIPPLE_PEAK_ALPHA_RATIO_MAX_B[rippleType], 0.0f, 1.0f, 0.01f, NullCB, "The evolved end alpha (max) of the ripple");
	pVfxBank->AddSlider("Rot Speed B", &VFXRIPPLE_ROT_SPEED_MIN_B[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The evolved rotation speed (min) of the ripple");
	pVfxBank->AddSlider("Rot Speed B", &VFXRIPPLE_ROT_SPEED_MAX_B[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The evolved rotation speed (max) of the ripple");
	pVfxBank->AddSlider("Rot Init B", &VFXRIPPLE_ROT_INIT_MIN_B[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The evolved initial angle (min) of the ripple");
	pVfxBank->AddSlider("Rot Init B", &VFXRIPPLE_ROT_INIT_MAX_B[rippleType], -360.0f, 360.0f, 0.5f, NullCB, "The evolved initial angle (max) of the ripple");
}
#endif
#endif

/////////////////////////////////////////////////////////////////////////////////
//	InitRenderStateBlocks
/////////////////////////////////////////////////////////////////////////////////
void CRippleManager::InitRenderStateBlocks()
{

	// exit state blocks
	ms_exitRasterizerState = grcStateBlock::RS_NoBackfaceCull;

	grcBlendStateDesc defaultExitStateB;
	defaultExitStateB.BlendRTDesc[0].BlendEnable = 1;
	defaultExitStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	defaultExitStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	defaultExitStateB.AlphaTestEnable = 1;
	defaultExitStateB.AlphaTestComparison = grcRSV::CMP_NOTEQUAL;
	ms_exitBlendState = grcStateBlock::CreateBlendState(defaultExitStateB);

	grcDepthStencilStateDesc defaultExitStateDS;
	defaultExitStateDS.DepthFunc = grcRSV::CMP_LESSEQUAL;
	defaultExitStateDS.DepthWriteMask = 0;
	ms_exitDepthStencilState = grcStateBlock::CreateDepthStencilState(defaultExitStateDS);

	// render state blocks
	ms_renderRasterizerState = grcStateBlock::RS_NoBackfaceCull;

	grcBlendStateDesc renderStateB;
	renderStateB.BlendRTDesc[0].BlendEnable = 1;
	renderStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	renderStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	renderStateB.AlphaTestEnable = 1;
	renderStateB.AlphaTestComparison = grcRSV::CMP_NOTEQUAL;
	ms_renderBlendState = grcStateBlock::CreateBlendState(renderStateB);

	grcDepthStencilStateDesc depthStencilBlockDesc;
	depthStencilBlockDesc.StencilEnable = true;
	depthStencilBlockDesc.DepthFunc = grcRSV::CMP_LESSEQUAL;
	depthStencilBlockDesc.DepthWriteMask = 0;
	depthStencilBlockDesc.StencilReadMask = 0x10;
	depthStencilBlockDesc.StencilWriteMask = 0xf0;
	depthStencilBlockDesc.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
	depthStencilBlockDesc.BackFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilBlockDesc.BackFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilBlockDesc.BackFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
	depthStencilBlockDesc.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
	depthStencilBlockDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilBlockDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
	depthStencilBlockDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;

	ms_renderDepthStencilState = grcStateBlock::CreateDepthStencilState(depthStencilBlockDesc);

	depthStencilBlockDesc.StencilEnable = false;
	ms_renderStencilOffDepthStencilState =  grcStateBlock::CreateDepthStencilState(depthStencilBlockDesc);

}
*/
