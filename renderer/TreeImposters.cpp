////////////////////////////////////////////////////////////////////////////////////
// Title	:	TreeImposters.cpp
// Author	:	
// Started	:	2008/02/19
//			:	
////////////////////////////////////////////////////////////////////////////////////


// Rage headers
#include "bank/bkmgr.h"
#include "file/asset.h"
//#include "fwmaths/Maths.h"
#include "fwmaths/random.h"
#include "fwmaths/vector.h"
#include "grcore/image.h"
#include "grcore/vertexbuffer.h"
#include "grcore/vertexdecl.h"
#include "grmodel/modelfactory.h"
#include "grModel/shaderFX.h"
#include "grmodel/shadergroup.h"
#include "pheffects/wind.h"
#include "rmcore/drawable.h"
#include "vector/matrix34.h"

// Game headers
#include "TreeImposters.h"
#include "debug/Debug.h"
#include "game/wind.h"	
#include "renderer/Renderer.h"
#include "renderer/rendertargets.h"
#include "renderer/renderthread.h"
#include "scene/Scene.h"
#include "shaders/CustomShaderEffectBase.h"
#include "shaders/CustomShaderEffectTree.h"
#include "system/param.h"
#include "fwsys/timer.h"


#if USE_TREE_IMPOSTERS

#if __DEV
//OPTIMISATIONS_OFF()
#endif


grmShader* CTreeImposters::ms_imposterShader;
grcEffectTechnique CTreeImposters::ms_imposterTechnique;
grcEffectTechnique CTreeImposters::ms_imposterShadowTechnique;
grcEffectTechnique CTreeImposters::ms_blitAverageTechnique;
grcEffectVar CTreeImposters::ms_imposterTexture_ID;
grcEffectVar CTreeImposters::ms_blitTexture_ID;
grcEffectVar CTreeImposters::ms_imposterWorldX_ID;
grcEffectVar CTreeImposters::ms_imposterWorldY_ID;
grcEffectVar CTreeImposters::ms_imposterWorldZ_ID;

s32	CTreeImposters::ms_imposterTechniqueGroupId;
s32	CTreeImposters::ms_imposterDeferredTechniqueGroupId;

//grcRenderTarget* CTreeImposters::ms_cacheRT;
//grcRenderTarget* CTreeImposters::ms_colRT;
grcRenderTarget* CTreeImposters::ms_sharedDepthRT;

#if __PPU
	grcRenderTarget* CTreeImposters::ms_paletteRT;
//	int CTreeImposters::ms_renderFlags;
#endif

CImposterCacheInfo	CTreeImposters::ms_imposterCache[16]; 

float CTreeImposters::ms_viewport_fov;

#if __BANK
	bool CTreeImposters::ms_Enable				= false;
	bool CTreeImposters::ms_EnableShadow		= true;
	bool CTreeImposters::ms_FlushCache			= false;
	int	 CTreeImposters::ms_ForcedUpdateIndex	= -1;

	bool CTreeImposters::ms_debugNoLeaves		= false;
	bool CTreeImposters::ms_debugNoLeafShadows	= false;
	bool CTreeImposters::ms_debugNoTrunks		= false;
	bool CTreeImposters::ms_debugNoTrunkShadows	= false;
	bool CTreeImposters::ms_debugOverride		= false;
	bool CTreeImposters::ms_debugDisableAllTrees= false;
#endif //__BANK...

float CTreeImposters::ms_FadeStart=15.0f;
float CTreeImposters::ms_FadeEnd=10.0f;
float CTreeImposters::ms_FadeStartShadow=40.0f;
float CTreeImposters::ms_FadeEndShadow=40.0f;

Vector3	CTreeImposters::ms_windDir=Vector3(1,0,0);
float	CTreeImposters::ms_windAng=0.0f;


Vector3 normTable[16]={	Vector3( 0, 0, 0),

						Vector3( 0, 0,-1),
						Vector3( 0, 0.5f,-0.8660254f),
						Vector3( 0.4330127f,-0.25f,-0.8660254f), 
						Vector3(-0.4330127f,-0.25f,-0.8660254f),

						Vector3(0,0.93969262f,0.34202014f), 
						Vector3(0.81379768f,0.46984631f, -0.34202014f), 
						Vector3(0.81379768f,-0.46984631f, 0.34202014f), 
						Vector3(0,-0.93969262f,-0.34202014f), 
						Vector3(-0.81379768f,-0.46984631f, 0.34202014f),
						Vector3(-0.81379768f, 0.46984631f,-0.34202014f),

						Vector3(-0.4330127f, 0.25f,0.8660254f), 
						Vector3( 0.4330127f, 0.25f,0.8660254f),
						Vector3( 0, -0.5f,-0.8660254f),
						Vector3( 0, 0, 1),

						Vector3( 0, 0, 0)}; 

grcTexture* CTreeImposters::ms_normTextureLUT;

int CTreeImposters::ms_RTUsedFlags[2];
int CTreeImposters::ms_RTBuffer[2][16];
int CTreeImposters::ms_genModelIndex=-1;

//#if __XENON
	int CTreeImposters::ms_bufferID=1;
//#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Init
// PURPOSE :  Initialises everything at the start of game.
/////////////////////////////////////////////////////////////////////////////////

void CTreeImposters::Init()
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);
	
	CCustomShaderEffectTree::InitClass();
	grcTextureFactory::CreateParams params;
#if __XENON
	params.UseFloat = true;
	params.HasParent = true; 
	params.Parent = NULL; 
	params.IsResolvable=false;
	params.IsRenderable=true;
	params.UseHierZ=true;
	params.Multisample = 1;	
	ms_sharedDepthRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "tree_imposter_shared_depth", grcrtDepthBuffer,	64, 64, 32, &params);
#elif __PPU
	params.HasParent = true;
	params.Parent = NULL;
	params.InTiledMemory = true;
	params.IsSwizzled = false;
	ms_sharedDepthRT = CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_Z32_SEPSTENCIL_REGULAR_BBUFFER,"ImposterSharedDepth 64x64", grcrtDepthBuffer, 64, 64, 32, &params, 3);
	ms_sharedDepthRT->AllocateMemoryFromPool();
#elif __WIN32PC || RSG_DURANGO || RSG_ORBIS // TODO - Make this scalable with res
	ms_sharedDepthRT = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "tree_imposter_shared_depth", grcrtDepthBuffer,	64, 64, 32);
#endif

#if __PPU
	params.HasParent = true;
	params.Parent = ms_sharedDepthRT;
	params.Format = grctfA8R8G8B8;
	params.InTiledMemory = true;
	params.IsSwizzled = false;
	#if 1 //not sure about this...
		params.EnableCompression = false;
	#endif
	ms_paletteRT=CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "tree_palette_target 64x64", grcrtPermanent, 64, 64, 32, &params, 0);
	ms_paletteRT->AllocateMemoryFromPool();
#endif
	for (int i=0;i<16;i++)
	{
		ms_imposterCache[i].m_index=i;
		ms_imposterCache[i].m_age=-1;
		ms_imposterCache[i].m_modelIndex=fwModelId::MI_INVALID;
		ms_imposterCache[i].m_fade=0.0f;
	#if __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
		params.IsResolvable=true;
		params.IsRenderable=true;
		params.Parent = ms_sharedDepthRT;
		params.Format = grctfA8R8G8B8;
		ms_imposterCache[i].m_renderTarget=CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "tree_imposter_cache_target 64x64", grcrtPermanent, 64, 64, 32, &params);
	#elif __PPU
		params.HasParent = true;
		params.Parent = ms_sharedDepthRT;
		params.Format = grctfA8R8G8B8;
		params.InTiledMemory = true;
		params.IsSwizzled = false;
		#if 1 //not sure about this...
			params.EnableCompression = false;
		#endif
		ms_imposterCache[i].m_renderTarget=CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED, "tree_imposter_cache_target 64x64", grcrtPermanent, 64, 64, 32, &params, 0);
		ms_imposterCache[i].m_renderTarget->AllocateMemoryFromPool();
	#endif

		ms_RTBuffer[0][i]=-1;
		ms_RTBuffer[1][i]=-1;
	}

	ms_RTUsedFlags[0]=0x0;
	ms_RTUsedFlags[1]=0x0;


	ASSET.PushFolder("common:/shaders");
	ms_imposterShader = grmShaderFactory::GetInstance().Create();
	ms_imposterShader->Load("billboard_nobump");
	ASSET.PopFolder();

	ms_imposterTechnique=ms_imposterShader->LookupTechnique("deferred_draw");
	ms_imposterShadowTechnique=grcetNONE; //ms_imposterShader->LookupTechnique("wd_draw");
	ms_blitAverageTechnique=ms_imposterShader->LookupTechnique("blit_average");

	ms_imposterTexture_ID = ms_imposterShader->LookupVar("imposterTexture");
	ms_blitTexture_ID = ms_imposterShader->LookupVar("blitTexture");

	ms_imposterWorldX_ID = ms_imposterShader->LookupVar("imposterWorldX");
	ms_imposterWorldY_ID = ms_imposterShader->LookupVar("imposterWorldY");
	ms_imposterWorldZ_ID = ms_imposterShader->LookupVar("imposterWorldZ");

	ms_imposterTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("imposter");
	Assert(-1 != ms_imposterTechniqueGroupId);
	
	ms_imposterDeferredTechniqueGroupId = grmShaderFx::FindTechniqueGroupId("imposterdeferred");
	Assert(-1 != ms_imposterDeferredTechniqueGroupId);
 
	ms_windAng=0.0f;

#if 1 //__PPU
	if (ms_normTextureLUT==NULL)
	{
		grcImage* normImage = grcImage::Create(16, 16, 1, grcImage::A8R8G8B8, grcImage::STANDARD, 0, 0);

		for(int xx=0; xx<16; xx++)
		{
			for(int yy=0; yy<16; yy++)
			{
				Vector3 norm=normTable[xx];

				norm.x+=fwRandom::GetRandomNumberInRange(-0.25f,0.25f);
				norm.y+=fwRandom::GetRandomNumberInRange(-0.25f,0.25f);
				norm.z+=fwRandom::GetRandomNumberInRange(-0.25f,0.50f);
				norm.Normalize();

				u32 a=u32(fwRandom::GetRandomNumberInRange( 0.0f, 255.0f));
				u32 r=u32(rage::Clamp((norm.x*0.5f)+0.5f, 0.0f, 1.0f)*255.0f);
				u32 g=u32(rage::Clamp((norm.y*0.5f)+0.5f, 0.0f, 1.0f)*255.0f);
				u32 b=u32(rage::Clamp((norm.z*0.5f)+0.5f, 0.0f, 1.0f)*255.0f);

				u32 colour=(a<<24)|(r<<16)|(g<<8)|(b<<0);

				normImage->SetPixel(xx, yy, colour);
			}
		}

		// create a texture from the image
		ms_normTextureLUT = grcTextureFactory::GetInstance().Create(normImage);		
		normImage->Release();
	}	
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Shutdown
// PURPOSE :  Frees everything on game shutdown.
/////////////////////////////////////////////////////////////////////////////////

void CTreeImposters::Shutdown()
{
	//clean up cache?
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Initialises everything at the start of a level.
/////////////////////////////////////////////////////////////////////////////////

#define TREE_FADE_UP_STEP (0.2f)

void CTreeImposters::Update()
{
//#if __XENON
	const int bufferID=ms_bufferID;
//#else
//	int b=gRenderThreadInterface.GetUpdateBuffer();
//#endif

	for (int i=0;i<16;i++)
	{
		if (ms_imposterCache[i].m_modelIndex!=fwModelId::MI_INVALID)
		{
			if (ms_RTUsedFlags[bufferID]&(0x1<<i))
				ms_imposterCache[i].m_age=0;
			else
				ms_imposterCache[i].m_age++;

			ms_imposterCache[i].m_fade=rage::Min(ms_imposterCache[i].m_fade+TREE_FADE_UP_STEP,1.0f);
		}
	#if __BANK
		if (ms_FlushCache==true)
		{
			ms_imposterCache[i].m_age=-1;
			ms_imposterCache[i].m_fade=0.0f;
		}
	#endif
	}
	ms_RTUsedFlags[bufferID]=0x0;

#if __BANK
	ms_FlushCache=false;

	if (ms_ForcedUpdateIndex!=-1)
	{
		ms_imposterCache[ms_ForcedUpdateIndex].m_age=-1;
	}
#endif

	if (ms_genModelIndex!=-1)
	{	
		int oldestAge=0;
		int oldestEntry=-1;
		int emptyEntry=-1;

		for (int i=0;i<16;i++)
		{
			if (emptyEntry==-1 && ms_imposterCache[i].m_modelIndex==fwModelId::MI_INVALID)
			{
				emptyEntry=i;
			}

			if (ms_imposterCache[i].m_modelIndex!=fwModelId::MI_INVALID && ms_imposterCache[i].m_age>oldestAge)
			{
				oldestAge=ms_imposterCache[i].m_age;
				oldestEntry=i;
			}
		}

		if (emptyEntry!=-1)
		{
			ms_imposterCache[emptyEntry].m_modelIndex=ms_genModelIndex;
			ms_imposterCache[emptyEntry].m_age=-1;
			ms_imposterCache[emptyEntry].m_fade=0.0f;
		}
		else if (oldestEntry!=-1)
		{
			ms_imposterCache[oldestEntry].m_modelIndex=ms_genModelIndex;
			ms_imposterCache[oldestEntry].m_age=-1;
			ms_imposterCache[oldestEntry].m_fade=0.0f;
		}

		ms_genModelIndex=-1;
	}

	for (int i=0;i<16;i++)
	{
		ms_RTBuffer[bufferID][i]=ms_imposterCache[i].m_modelIndex;
	}

	if (fwTimer::IsGamePaused()==false)
	{
		Vector3 wind;
		WIND.GetGlobalVelocity(WIND_TYPE_AIR, RC_VEC3V(wind));
		// This is odd, the above function was occasionally returning QNANs
		// but it went away when I added this assert.  Must be a threading / timing issue (thanks Jon)
		Assert(wind.x==wind.x);

		static float debugWindLag=0.5f;
		ms_windDir=ms_windDir+(wind-ms_windDir)*debugWindLag;

		static float debugMaxStrength=2.0f;
		float windStrength=rage::Min(ms_windDir.Mag(),debugMaxStrength);

		static float debugAngScale=0.001f;
		ms_windAng+=fwTimer::GetTimeStepInMilliseconds()*windStrength*debugAngScale;
	}

//#if __XENON
	ms_bufferID=1-ms_bufferID;
//#endif

#if __BANK
	if (ms_debugOverride==false)
	{	
		if (ms_debugDisableAllTrees==true)
		{
			ms_debugNoLeaves=true;
			ms_debugNoLeafShadows=true;
			ms_debugNoTrunks=true;
			ms_debugNoTrunkShadows=true;			
		}
		else
		{
			ms_debugNoLeaves=false;
			ms_debugNoLeafShadows=false;
			ms_debugNoTrunks=false;
			ms_debugNoTrunkShadows=false;		
		}
	}
#endif
}

int CTreeImposters::GetImposterCacheIndex(int modelIndex)
{
//#if __XENON
	int bufferID=1-ms_bufferID;
//#else
//	int b=gRenderThreadInterface.GetRenderBuffer();
//#endif

	for (int i=0;i<16;i++)
	{
		if (ms_RTBuffer[bufferID][i]==modelIndex)
		{
			ms_RTUsedFlags[bufferID]|=(0x1<<i);
			return i;
		}
	}

	if (ms_genModelIndex==-1) ms_genModelIndex=modelIndex;

	return -1;
}

/*
int CTreeImposters::GetImposterCacheIndex_MT(int modelIndex)
{
	int b=gRenderThreadInterface.GetRenderBuffer();

	for (int i=0;i<16;i++)
	{
		if (ms_RTBuffer[b][i]==modelIndex)
		{
			return i;
		}
	}
	
	if (ms_NewModel==-1) ms_NewModel=modelIndex;

	return -1;
}
*/
grcRenderTarget* CTreeImposters::GetCacheRenderTarget(int cache_index)
{
	return ms_imposterCache[cache_index].m_renderTarget;
}

grcRenderTarget* CTreeImposters::GetSharedDepthTarget()
{
	return ms_sharedDepthRT;
}

#if __PPU
grcRenderTarget* CTreeImposters::GetPaletteTarget()
{
	return ms_paletteRT;
}
#endif

//Vector3 g_imposterDir[8]={Vector3(0,0,1), Vector3(1,0,-0.577), Vector3(-1,0,-0.577), Vector3(0,-1.732,-1), Vector3(1,-2.309,0.577), Vector3(-1,-2.309,0.577), Vector3(0,-1,0) };

#define R3 (0.57735026918962576450914878050196f)

Vector3 g_imposterDir[8]={	Vector3( 1, 0, 0),
							Vector3( R3, R3,-R3),
							Vector3( 0, 1, 0), 
							Vector3(-R3, R3,-R3), 
							Vector3(-1, 0, 0), 
							Vector3(-R3,-R3,-R3), 
							Vector3( 0,-1, 0), 
							Vector3( R3,-R3,-R3)};


Vector3 CTreeImposters::GetImposterViewDir(int i)
{
	Vector3 dir=g_imposterDir[i];

	dir.Normalize();

	return dir;
}

Vector3 CTreeImposters::GetImposterViewUp(int i)
{
	Vector3 dir=GetImposterViewDir(i);
	
	Vector3 up=Vector3(0,0,1);
	up.Cross(dir);
	up.Cross(dir);

	up.Normalize();

	static float debugScale=1.0f;

	return up*debugScale;
}

CImposterCacheInfo* CTreeImposters::GetCacheEntryToRender()
{
	for (int i=0;i<16;i++)
	{
		if (ms_imposterCache[i].m_modelIndex!=fwModelId::MI_INVALID && ms_imposterCache[i].m_age==-1)
		{
			return &ms_imposterCache[i];
		}
	}

	return NULL;
}

float CTreeImposters::GetImposterFadeShadow(int cache_index, float dist)
{
	if (CTreeImposters::ms_FadeStartShadow==CTreeImposters::ms_FadeEndShadow)
	{
		return 1.0f;
	}

	float fov=ms_viewport_fov;

	float size=ms_imposterCache[cache_index].m_radius/dist;

	float start=fov*CTreeImposters::ms_FadeStartShadow*0.01f;
	float end=fov*CTreeImposters::ms_FadeEndShadow*0.01f;

	float fade=rage::Clamp((size-start)/(end-start), 0.0f, 1.0f);

	fade=rage::Min(fade,ms_imposterCache[cache_index].m_fade);

	return fade;
}

float CTreeImposters::GetImposterFade(int cache_index, float dist)
{
	float fov=ms_viewport_fov;

	float size=ms_imposterCache[cache_index].m_radius/dist;

	float start=fov*CTreeImposters::ms_FadeStart*0.01f;
	float end=fov*CTreeImposters::ms_FadeEnd*0.01f;

	float fade=rage::Clamp((size-start)/(end-start), 0.0f, 1.0f);

	fade=rage::Min(fade,ms_imposterCache[cache_index].m_fade);

	return fade;
}

#if __PPU
void CTreeImposters::RenderLeafColour(int UNUSED_PARAM(cache_index))
#else
void CTreeImposters::RenderLeafColour(int cache_index)
#endif
{
#if __PPU
	ms_imposterShader->SetVar(ms_blitTexture_ID, ms_paletteRT);
#else
	ms_imposterShader->SetVar(ms_blitTexture_ID, ms_imposterCache[cache_index].m_renderTarget);
#endif

//#if __PPU
	static float debugSize=(1.0f/32.0f);
//#else
// 	static float debugSize=(1.0f/32.0f);
//#endif

	float q=1.0f-debugSize;
	float p=1.0f;
	Vector3 v0=Vector3( q, q, 0);
	Vector3 v1=Vector3( p, q, 0);
	Vector3 v2=Vector3(	q, p, 0);
	Vector3 v3=Vector3( p, p, 0);

	int ret=ms_imposterShader->BeginDraw(grmShader::RMC_DRAW, TRUE, ms_blitAverageTechnique);
	if (ret>0)
	{		
		ms_imposterShader->Bind(0);
		Color32 col=Color32(1.0f,1.0f,1.0f,1.0f);
		Vector3 norm=Vector3(0,0,1); 
		grcBegin(drawTriStrip,4);
			grcVertex(v0.x,v0.y,v0.z,norm.x,norm.y,norm.z,col,0.0f,0.0f);
			grcVertex(v1.x,v1.y,v1.z,norm.x,norm.y,norm.z,col,1.0f,0.0f);
			grcVertex(v2.x,v2.y,v2.z,norm.x,norm.y,norm.z,col,0.0f,1.0f);
			grcVertex(v3.x,v3.y,v3.z,norm.x,norm.y,norm.z,col,1.0f,1.0f);
		grcEnd();
		ms_imposterShader->UnBind();
	}
	ms_imposterShader->EndDraw();

}

void CTreeImposters::RenderTreeImposterShadow(int cache_index, const Matrix34& mat, u8 alpha)
{
	if (ms_imposterShadowTechnique == grcetNONE)
	{
		return;
	}

	ms_imposterShader->SetVar(ms_blitTexture_ID, ms_normTextureLUT);
	ms_imposterShader->SetVar(ms_imposterTexture_ID, ms_imposterCache[cache_index].m_renderTarget);

	static float debugScale0=1.0f;
	static float debugScale1=1.0f;
	static float debugScale2=1.0f;
	static float debugScale3=1.0f;

	const Matrix34 &camMtx = RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx());
	Vector3 viewRight = camMtx.a*debugScale0;
	Vector3 viewUp = camMtx.b*debugScale1;
	Vector3 viewDir = camMtx.c*debugScale2;
	Vector3 viewPos = camMtx.d;

	Vector3 off=ms_imposterCache[cache_index].m_centre;
	float size=ms_imposterCache[cache_index].m_radius;

	Vector3 pos=mat.d;

	static float debugZOffset=0.0f;
	pos.z+=debugZOffset;

	Matrix34 t_mat=mat;
	t_mat.d=Vector3(0,0,0);
	t_mat.Transform(off);
	
	t_mat.Transpose();

	Vector3 ndir=viewDir*debugScale3;
	t_mat.Transform(ndir);

	Vector3 v0=pos+off-viewRight*size-viewUp*size;
	Vector3 v1=pos+off+viewRight*size-viewUp*size;
	Vector3 v2=pos+off-viewRight*size+viewUp*size;
	Vector3 v3=pos+off+viewRight*size+viewUp*size;

	grcViewport::SetCurrentWorldIdentity();

	int ret=ms_imposterShader->BeginDraw(grmShader::RMC_DRAW, TRUE, ms_imposterShadowTechnique);
	if (ret)
	{		
		float noiseScale=rage::Clamp(size/16.0f, 0.0f, 1.0f);

		ms_imposterShader->Bind(0);
		Color32 col=Color32(noiseScale,1.0f,1.0f,alpha/255.0f);
		grcBegin(drawTriStrip,4);
			grcVertex(v0.x,v0.y,v0.z,ndir.x,ndir.y,ndir.z,col,0.0f,0.0f);
			grcVertex(v1.x,v1.y,v1.z,ndir.x,ndir.y,ndir.z,col,1.0f,0.0f);
			grcVertex(v2.x,v2.y,v2.z,ndir.x,ndir.y,ndir.z,col,0.0f,1.0f);
			grcVertex(v3.x,v3.y,v3.z,ndir.x,ndir.y,ndir.z,col,1.0f,1.0f);
		grcEnd();
		ms_imposterShader->UnBind();
	}
	ms_imposterShader->EndDraw();

}

void CTreeImposters::RenderTreeImposter(int cache_index, const Matrix34& mat, u8 alpha, u8 ambient)
{
	(void) ambient;

	static int debugIndex=-1;
	if (debugIndex!=-1)
	{
		cache_index=debugIndex;
	}

//	static int debugSwitch=0;

	ms_imposterShader->SetVar(ms_blitTexture_ID, ms_normTextureLUT);
	ms_imposterShader->SetVar(ms_imposterTexture_ID, ms_imposterCache[cache_index].m_renderTarget);

/*	if (debugSwitch==1 || debugSwitch==3)
	{
		ms_imposterShader->SetVar(ms_imposterWorldX_ID, Vector3(1,0,0));
		ms_imposterShader->SetVar(ms_imposterWorldY_ID, Vector3(0,1,0));
		ms_imposterShader->SetVar(ms_imposterWorldZ_ID, Vector3(0,0,1));		
	}
	else*/
	{
		ms_imposterShader->SetVar(ms_imposterWorldX_ID, mat.a);
		ms_imposterShader->SetVar(ms_imposterWorldY_ID, mat.b);
		ms_imposterShader->SetVar(ms_imposterWorldZ_ID, mat.c);
	}

	const Matrix34 &camMtx = RCC_MATRIX34(grcViewport::GetCurrent()->GetCameraMtx());
	Vector3 viewRight = camMtx.a;
	Vector3 viewUp = camMtx.b;
	Vector3 viewDir = camMtx.c;
	Vector3 viewPos = camMtx.d;

	Vector3 off=ms_imposterCache[cache_index].m_centre;
	float size=ms_imposterCache[cache_index].m_radius;

	Vector3 pos=mat.d;
	Matrix34 t_mat=mat;
/*	if (debugSwitch==2 || debugSwitch==3)
	{
		viewRight=Vector3(1,0,0);
		viewUp=Vector3(0,1,0);
		viewDir=Vector3(0,0,1);	

		static bool debugIncludePos=false;
		if (debugIncludePos)
		{
			viewPos=Vector3(0,0,0);
			off=Vector3(0,0,0);
		}

		static bool debugIncludeMat=true;
		if (debugIncludeMat)
		{
			t_mat.a=Vector3(1,0,0);
			t_mat.b=Vector3(0,1,0);
			t_mat.c=Vector3(0,0,1);
		}
	}*/

	t_mat.d=Vector3(0,0,0);

	t_mat.Transform(off);
	Vector3 tdir=((pos+off)-viewPos);
	Vector3 tright=Vector3(0,0,1);
	tright.Cross(tdir);
	Vector3 tup=tright;
	tup.Cross(tdir);

/*	static float debugScale0=1.0f;
	static float debugScale1=1.0f;
	static float debugScale2=1.0f;
	static float debugScale3=1.0f;

	tdir*=debugScale0;
	tright*=debugScale1;
	tup*=debugScale2;*/

	tdir.Normalize();
	tright.Normalize();
	tup.Normalize();

	t_mat.Transpose();

	Vector3 ndir=tdir;
	t_mat.Transform(ndir);

//	ndir*=debugScale3;

/*	if (debugSwitch==4)
	{
		ndir=viewDir;
	}*/

	Vector3 v0=pos+off-tright*size-tup*size;
	Vector3 v1=pos+off+tright*size-tup*size;
	Vector3 v2=pos+off-tright*size+tup*size;
	Vector3 v3=pos+off+tright*size+tup*size;

	// Vector2 uv0=Vector2(0.0f,0.0f);
	// Vector2 uv1=Vector2(1.0f,0.0f);
	// Vector2 uv2=Vector2(0.0f,1.0f);
	// Vector2 uv3=Vector2(1.0f,1.0f);

	grcViewport::SetCurrentWorldIdentity();

	int ret=ms_imposterShader->BeginDraw(grmShader::RMC_DRAW, TRUE, ms_imposterTechnique);
	if (ret)
	{		
		ms_imposterShader->Bind(0);
		Color32 col=Color32(1.0f,1.0f,1.0f,alpha/255.0f);

		grcBegin(drawTriStrip,4);
			grcVertex(v0.x,v0.y,v0.z,ndir.x,ndir.y,ndir.z,col,0.0f,0.0f);
			grcVertex(v1.x,v1.y,v1.z,ndir.x,ndir.y,ndir.z,col,1.0f,0.0f);
			grcVertex(v2.x,v2.y,v2.z,ndir.x,ndir.y,ndir.z,col,0.0f,1.0f);
			grcVertex(v3.x,v3.y,v3.z,ndir.x,ndir.y,ndir.z,col,1.0f,1.0f);
		grcEnd();

		ms_imposterShader->UnBind();
	}
	ms_imposterShader->EndDraw();

}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitWidgets
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

void CTreeImposters::InitWidgets()
{
	bkBank *pMainBank = BANKMGR.FindBank("Renderer");
	Assert(pMainBank);
	bkBank& bank = *pMainBank;

	bank.PushGroup("Trees", true);
		bank.AddToggle("Override global toggle in optimization bank",	&CTreeImposters::ms_debugOverride);

		bank.AddToggle("Trees: Don't Render Leaves",		&CTreeImposters::ms_debugNoLeaves);
		bank.AddToggle("Trees: Don't Render Leaf Shadows",	&CTreeImposters::ms_debugNoLeafShadows);

		bank.AddToggle("Trees: Don't Render Trunks",		&CTreeImposters::ms_debugNoTrunks);
		bank.AddToggle("Trees: Don't Render Shadows",		&CTreeImposters::ms_debugNoTrunkShadows);

		bank.AddToggle("Enable Imposters",					&CTreeImposters::ms_Enable);

		bank.AddToggle("Flush Cache",						&CTreeImposters::ms_FlushCache);	
		bank.AddSlider("Forced Update Cache Entry",			&CTreeImposters::ms_ForcedUpdateIndex, -1, 15, 1);

		bank.AddSlider("Imposters: Fade Start",				&CTreeImposters::ms_FadeStart, 1.0f, 100.0f, 0.1f );
		bank.AddSlider("Imposters: Fade End",				&CTreeImposters::ms_FadeEnd, 1.0f, 100.0f, 0.1f );

		bank.AddToggle("Enable Shadow Imposters",			&CTreeImposters::ms_EnableShadow);

		bank.AddSlider("Shadow Imposter Fade Start",		&CTreeImposters::ms_FadeStartShadow, 1.0f, 100.0f, 0.1f );
		bank.AddSlider("Shadow Imposter Fade End",			&CTreeImposters::ms_FadeEndShadow, 1.0f, 100.0f, 0.1f );

	#if CSE_TREE_EDITABLEVALUES
		CCustomShaderEffectTree::InitWidgets(bank);
	#endif
	bank.PopGroup();

	bkBank *pBank = BANKMGR.FindBank("Optimization");
	Assert(pBank);
	pBank->AddToggle("Disable Trees", &CTreeImposters::ms_debugDisableAllTrees);
}
#endif //__BANK..


#endif // USE_TREE_IMPOSTERS
