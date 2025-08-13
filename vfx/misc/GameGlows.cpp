///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "GameGlows.h"

#define	GAMEGLOWS_MAX					(64)


// Rage header
#include "atl/array.h"
#include "grcore/texture.h"

// Framework headers
#include "fwsys/gameskeleton.h"
#include "vfx/channel.h"

// Game header 
#include "Peds/Ped.h"
#include "renderer/DrawLists/drawList.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/rendertargets.h"
#include "renderer/sprite2d.h"
#include "scene/world/GameWorld.h"
#include "shaders/ShaderLib.h"
#include "vfx/vfxwidget.h"
#include "renderer/PostProcessFX.h"

VFX_MISC_OPTIMISATIONS()

typedef struct 
{
	Vec3V pos;
	float size;
	float intensity;
	Color32 col;
} gameGlow;

static atFixedArray<gameGlow,GAMEGLOWS_MAX> s_gameGlows;
static gameGlow s_fullScreenGameGlow;
static bool s_invertFullScreenGameGlow = false;
static bool s_markerFullScreenGameGlow = false;
static bool s_enableFullScreenGameGlow = false;

#if __BANK
static bool s_fullScreenGameGlowFollowPlayer = false;
static int s_numOverflown = 0;
#endif // __BANK


bank_float s_intensityScaleBright = 1.5f;
bank_float s_intensityScaleDark = 1.0f;

class dlCmdDrawGlowQuads : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_DrawGlowQuads,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	dlCmdDrawGlowQuads(atFixedArray<gameGlow,GAMEGLOWS_MAX> &glowsArray) 
	{ 
		quadCount = glowsArray.GetCount();
		glows = gDCBuffer->AllocateObjectFromPagedMemory<gameGlow>(DPT_LIFETIME_RENDER_THREAD, sizeof(gameGlow) * quadCount);
		sysMemCpy(glows,glowsArray.GetElements(),sizeof(gameGlow) * quadCount);		
	}
	
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdDrawGlowQuads &) cmd).Execute(); }
	
	void Execute()
	{ 
		gameGlow* points = glows;

		if( quadCount > 0 )
		{
			grcViewport::SetCurrentWorldIdentity();
			grcWorldIdentity();
			grcBindTexture(grcTexture::None);
			CShaderLib::SetGlobalEmissiveScale(1.0f,true);
			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Normal);
			grcLighting(false);
			
			const grcViewport* pCurrViewport = grcViewport::GetCurrent();
			const Mat34V &camMtx = pCurrViewport->GetCameraMtx();
			const Vec3V camRight = camMtx.a();
			const Vec3V camUp = camMtx.b();
			//const Vec3V camPos = camMtx.d();
			
			CSprite2d glowSprite;
			grcRenderTarget* depthbuffer = CRenderTargets::PS3_SWITCH(GetDepthBufferAsColor(), GetDepthBuffer());
#if RSG_PC
			depthbuffer = CRenderTargets::LockReadOnlyDepth();
#endif
			glowSprite.SetDepthTexture(depthbuffer);


			const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
			const Vector4 genParam = Vector4(Vector4::ZeroType);

			glowSprite.SetGeneralParams(projParams,genParam);
			glowSprite.BeginCustomList(CSprite2d::CS_GAMEGLOWS,NULL);

			const float sizeScale = 2.0f;
			const float intensityScale = Lerp(PostFX::GetLinearExposure(), s_intensityScaleBright, s_intensityScaleDark);
			
			const int numVertsPerQuad = __D3D11 ? 6 : 4;
			const int maxQuadPerBatch = grcBeginMax/numVertsPerQuad;
			
			int quads = quadCount;
			while( quads > 0 )
			{
				int batchSize = Min(quads,maxQuadPerBatch);
				quads -= batchSize;
				
				grcDrawMode drawMode = __D3D11 ? drawTriStrip : drawQuads;
				grcBegin(drawMode,batchSize*numVertsPerQuad);

				for(int i=0;i<batchSize;i++)
				{
					const Vec3V pos = points->pos;
					const ScalarV scale(points->size * sizeScale);
					const Color32 col = points->col;
					const float size = points->size;
					const float intensity = points->intensity * intensityScale;
					points++;

					const Vec3V right = camRight * scale;
					const Vec3V up = camUp * scale;
					
					const Vec3V p1 = pos - right + up ;
					const Vec3V p2 = pos + right + up ;
					const Vec3V p3 = pos + right - up ;
					const Vec3V p4 = pos - right - up ;
					
#if __D3D11
					grcVertex(	VEC3V_ARGS(p1), VEC3V_ARGS(pos), col, size, intensity);
					grcVertex(	VEC3V_ARGS(p1), VEC3V_ARGS(pos), col, size, intensity);

					grcVertex(	VEC3V_ARGS(p2), VEC3V_ARGS(pos), col, size, intensity);
					grcVertex(	VEC3V_ARGS(p4), VEC3V_ARGS(pos), col, size, intensity);

					grcVertex(	VEC3V_ARGS(p3), VEC3V_ARGS(pos), col, size, intensity);
					grcVertex(	VEC3V_ARGS(p3), VEC3V_ARGS(pos), col, size, intensity);
#else
					grcVertex(	VEC3V_ARGS(p1), VEC3V_ARGS(pos), col, size, intensity);
					grcVertex(	VEC3V_ARGS(p2), VEC3V_ARGS(pos), col, size, intensity);
					grcVertex(	VEC3V_ARGS(p3), VEC3V_ARGS(pos), col, size, intensity);
					grcVertex(	VEC3V_ARGS(p4), VEC3V_ARGS(pos), col, size, intensity);
#endif
				}
				
				grcEnd();
			}
			glowSprite.EndCustomList();

#if RSG_PC
			CRenderTargets::UnlockReadOnlyDepth();
#endif
		}
	}
	
private:
	int quadCount;
	gameGlow* glows;
};

class dlCmdDrawFullScreenGlowQuads : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_DrawFullScreenGlowQuads,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	dlCmdDrawFullScreenGlowQuads(gameGlow fsGlow, bool invertFsGlow, bool markerFsGlow) 
	{ 
		fullScreenGameGlow = fsGlow;
		invertFullScreenGameGlow = invertFsGlow;
		markerFullScreenGameGlow = markerFsGlow;
	}
	
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdDrawFullScreenGlowQuads &) cmd).Execute(); }
	
	void Execute()
	{ 
#if RSG_PC
		grcTextureFactory::GetInstance().LockRenderTarget(0, grcTextureFactory::GetInstance().GetBackBuffer(), NULL);
#endif
		grcViewport::SetCurrentWorldIdentity();
		grcWorldIdentity();
		grcBindTexture(grcTexture::None);
		CShaderLib::SetGlobalEmissiveScale(1.0f,true);
		CShaderLib::UpdateGlobalDevScreenSize();			
		grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Normal);
		grcLighting(false);
			
		CSprite2d glowSprite;
		grcRenderTarget* depthbuffer = CRenderTargets::PS3_SWITCH(GetDepthBufferAsColor(), GetDepthBuffer());
#if RSG_PC
		depthbuffer = GRCDEVICE.IsReadOnlyDepthAllowed()?CRenderTargets::GetDepthBuffer():CRenderTargets::GetDepthBufferCopy();
#endif
		if( markerFullScreenGameGlow )
			depthbuffer = CRenderTargets::GetDepthBuffer_PreAlpha();

		glowSprite.SetTexelSize(Vector2(VideoResManager::GetSceneWidth() / (float)VideoResManager::GetUIWidth(), VideoResManager::GetSceneHeight() / (float)VideoResManager::GetUIHeight()));
		glowSprite.SetDepthTexture(depthbuffer);

		const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
		const float size = invertFullScreenGameGlow ? -fullScreenGameGlow.size : fullScreenGameGlow.size;
		const Vector4 genParam = Vector4(fullScreenGameGlow.pos.GetXf(), fullScreenGameGlow.pos.GetYf(), fullScreenGameGlow.pos.GetZf(), size);
		glowSprite.SetGeneralParams(projParams,genParam);

		CSprite2d::CustomShaderType spriteMode = markerFullScreenGameGlow ? CSprite2d::CS_FSGAMEGLOWSMARKER : CSprite2d::CS_FSGAMEGLOWS;
#if __D3D11

		glowSprite.BeginCustomList(spriteMode,depthbuffer);
#else
		glowSprite.BeginCustomList(spriteMode,NULL);
#endif

		grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, fullScreenGameGlow.intensity, 1.0f, fullScreenGameGlow.intensity, 1.0f, fullScreenGameGlow.col);

		glowSprite.EndCustomList();

#if RSG_PC
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif
	}
	
private:
	gameGlow fullScreenGameGlow;
	bool invertFullScreenGameGlow;
	bool markerFullScreenGameGlow;
};
void GameGlows::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		DLC_REGISTER_EXTERNAL(dlCmdDrawGlowQuads);
		DLC_REGISTER_EXTERNAL(dlCmdDrawFullScreenGlowQuads);
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
		s_gameGlows.Reset();
    }
}


void GameGlows::Render()
{
	if( s_gameGlows.GetCount() )
	{
		DLC (dlCmdDrawGlowQuads, (s_gameGlows));
	}

	if( s_enableFullScreenGameGlow )
	{
		DLC (dlCmdDrawFullScreenGlowQuads, (s_fullScreenGameGlow, s_invertFullScreenGameGlow, s_markerFullScreenGameGlow));
	}

	s_gameGlows.Reset();
	s_enableFullScreenGameGlow = false;

#if __BANK
	s_numOverflown=0;

	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( s_fullScreenGameGlowFollowPlayer && pPlayer)
	{
		s_fullScreenGameGlow.pos = pPlayer->GetTransform().GetPosition();
		s_enableFullScreenGameGlow = true;
	}
#endif // __BANK
}


void GameGlows::Add(Vec3V_In pos,const float size,const Color32 col,const float intensity)
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "GameGlows::Add - not called from the update thread");
	
	if( vfxVerifyf(!s_gameGlows.IsFull(),"too many game glows") )
	{
		gameGlow &glow = s_gameGlows.Append();
		glow.pos = pos;
		glow.size = size;
		glow.col = col;
		glow.intensity = intensity;
	}
#if __BANK
	else
	{
		s_numOverflown++;
	}
#endif	
}

bool GameGlows::AddFullScreenGameGlow(Vec3V_In vPos, const float size, const Color32 col, const float intensity, const bool inverted, const bool marker, const bool fromScript)
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "GameGlows::AddFullScreenGameGlow - not called from the update thread");

	bool result = fromScript == false || s_enableFullScreenGameGlow == false;

	s_fullScreenGameGlow.pos = vPos;
	s_fullScreenGameGlow.size = size;
	s_fullScreenGameGlow.col = col;
	s_fullScreenGameGlow.intensity = intensity;
	s_invertFullScreenGameGlow = inverted;
	s_markerFullScreenGameGlow = marker;
	s_enableFullScreenGameGlow = true;

	return result;
}



#if __BANK
void GameGlows::InitWidgets()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("GameGlows", false);
	{
		pVfxBank->AddSlider("Bright Scaler",&s_intensityScaleBright,0.0f,4.0f,0.1f);
		pVfxBank->AddSlider("Dark Scaler",&s_intensityScaleDark,0.0f,4.0f,0.1f);

		pVfxBank->PushGroup("Fullscreen GameGlow", false);

		pVfxBank->AddToggle("Follow Player", &s_fullScreenGameGlowFollowPlayer);
		pVfxBank->AddVector("Pos", &s_fullScreenGameGlow.pos,-4000.0f,4000.0f,1.0f);
		pVfxBank->AddSlider("Size", &s_fullScreenGameGlow.size,0.0f,500.0f,0.1f);
		pVfxBank->AddColor("Color",&s_fullScreenGameGlow.col);
		pVfxBank->AddSlider("Intensity", &s_fullScreenGameGlow.intensity,0.0f,1.0f,0.1f);
		pVfxBank->AddToggle("Inverted", &s_invertFullScreenGameGlow);
		pVfxBank->AddToggle("Marker", &s_markerFullScreenGameGlow);

		pVfxBank->PopGroup();
	}
	pVfxBank->PopGroup();
}
#endif // __BANK

