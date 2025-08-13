

#include "Puddles.h"
//rage
#include "grcore/device.h"
#include "grcore/quads.h"
#include "grcore/texture.h"
#include "bank/bank.h"

// framework
#include "fwsys/timer.h"
#include "fwmaths/vector.h"

// game
#include "game/weather.h"
#include "Peds\Ped.h"
#include "Peds\PedFootstepHelper.h"
#include "phEffects\wind.h"
#include "renderer\Deferred\DeferredLighting.h"
#include "Renderer/rendertargets.h"
#include "renderer/water.h"
#include "renderer\RenderPhases\RenderPhaseReflection.h"
#include "renderer\SSAO.h"
#include "Shaders/ShaderLib.h"
#include "scene\world\GameWorld.h"
#include "timecycle/TimeCycleConfig.h"




#if __PS3

#define PUDDLE_MAX_RANGE  ( 220.f )
static void SetDepthBoundsFromRange(float nearClip, float farClip, float farZ)
{
	grcDevice::SetDepthBoundsTestEnable(true);

	const float linearMin =0.f;
	const float linearMax = Clamp<float>(farZ, nearClip, farClip);

	const float Q = farClip/(farClip - nearClip);
	const float sampleMax = (Q/linearMax)*(linearMax - nearClip);

	grcDevice::SetDepthBounds(0.f, sampleMax);
}	
#endif // __PS3


class Ripples
{
	static const int MaxRipples = 96;
	static const int killTime = 5;
	static const int MaxShaderRipples = 128;
	
	class RippleVertices
	{
		grcVertexDeclaration* m_vertexDecl;

	public:
		struct Vertex
		{
			Vec4V pos;
			Vec4V wave;

			void Set( ScalarV_In x, ScalarV_In y, Vec4V_In _wave, ScalarV_In _life){
				pos = Vec4V( x, y, _life, ScalarV(V_ZERO));
				wave =_wave;
			}
		};
		
		static const int BatchSize = (grcDevice::BEGIN_VERTICES_MAX_SIZE )/(sizeof(Vertex)*grcVerticesPerQuad);
		RippleVertices()
		{
			grcVertexElement VSRTElements[] =
			{
				grcVertexElement(0, grcVertexElement::grcvetPosition, 0, sizeof(Vec4V), grcFvf::grcdsFloat4),
				grcVertexElement(0, grcVertexElement::grcvetTexture, 0, sizeof(Vec4V), grcFvf::grcdsFloat4),
			};

			m_vertexDecl = GRCDEVICE.CreateVertexDeclaration(VSRTElements, sizeof(VSRTElements)/sizeof(grcVertexElement));
		}
		~RippleVertices(){
			GRCDEVICE.DestroyVertexDeclaration( m_vertexDecl );
		}
		RETURNCHECK(Vertex*) Begin(int amt ) const
		{
			GRCDEVICE.SetVertexDeclaration(m_vertexDecl);  
			return (Vertex*)GRCDEVICE.BeginVertices(grcQuadPrimitive, grcVerticesPerQuad*amt ,m_vertexDecl->Stream0Size);		
		}
		void Add(Vertex*& v, ScalarV_In x1, ScalarV_In y1, ScalarV_In x2, ScalarV_In y2, Vec4V_In wave, ScalarV_In data)  const
		{
#if RSG_PS3		// stolen from quad
			v[0].Set(x1, y1, wave,data);
			v[1].Set(x1, y2, wave,data);
			v[2].Set(x2, y2, wave,data);
			v[3].Set(x2, y1, wave,data);
#elif RSG_XENON || RSG_ORBIS
			v[0].Set(x1, y1, wave,data);
			v[1].Set(x2, y1, wave,data);
			v[2].Set(x1, y2, wave,data);
#else
			v[0].Set(x1, y1, wave,data);
			v[1].Set(x1, y2, wave,data);
			v[2].Set(x2, y1, wave,data);

			v[3].Set(x1, y2, wave,data);
			v[4].Set(x2, y2, wave,data);
			v[5].Set(x2, y1, wave,data);
#endif
			v += grcVerticesPerQuad;
		}
		void End()  const
		{
			GRCDEVICE.EndVertices();
		}
	};

	

	struct Ripple
	{
		float x;
		float y;
		float wlen;
		float sphase;
		float kTime;
		Ripple() {}

		Ripple( float _x, float _y, float _wlen, float rPhase, float _killTime ) 
		{
			sphase = rPhase;
			x=_x; 
			y=_y; 
			wlen = _wlen;
			kTime =_killTime;
		}  
		float actPhase( float rPhase)const   { 			return rPhase - sphase;		}
		float freq()	const   {			return 1.f/wlen;		}
		float life(float rPhase )	const   {			return 1.f- (actPhase(rPhase)/ kTime);		}
		float lifeScale() const { return 1.f/kTime; }
		float radius(float rPhase) const  {
			float ft = actPhase(rPhase)+1.5f;
			return ft*wlen;
		}
		Vec4V PackedWave(float rPhase) const  {
			return Vec4V( x, y, freq(), actPhase(rPhase));
		}
	};

	typedef atFixedArray<Ripple,MaxRipples> RippleArray;
	RippleArray					m_ripples;
	float						m_Phase;
	float						m_playerPhase;
	


	// Player Ripples
	RippleArray						m_playerRipples;
	Vec3V							m_playerOffset;
	BANK_ONLY(bool					m_drawPlayerRipples);


	// shader side
	static const int RippleTextureWidth = 256;
	grcRenderTarget*		m_rt;
	grcEffectVar			m_rippleDataLocalVar;
	grcEffectVar			m_rippleWindBumpLocalVar;
	grcEffectVar			m_rippleScaleOffset;
	grcEffectVar			m_ripplePlayerBumpLocalVar;

	grcEffectTechnique		m_windRippleTech;
	grcEffectTechnique		m_rippleTech;
	grcEffectTechnique	    m_footRippleTech;
	grcEffectTechnique		m_clearRippleTech;

	
	grcRenderTarget*		m_playerRt;

	// params
	float					m_maxSize;
	float					m_minSize;
	float					m_minDuration;
	float					m_maxDuration;
	float					m_speed;
	float					m_rainFactor;
	float					m_windMaxBumpStrength;
	float					m_windBumpStrength;
	float					m_rippleBumpStrength;
	
	mthRandom				m_rnd;

	float					m_mapSize;
	float					m_playerMapSize;
	float					m_playerSpeed;

#if __BANK
	float			m_debugWindVel;
	mutable int		m_debugNumWaves;  // mutable only for debug spew
	bool	m_showBump;
#endif

	float 					m_windOffset;
	float 					m_windDamping;

	RippleVertices			m_rippleVerts;

	sysCriticalSectionToken	m_lockToken;
public:
	Ripples()
	{
		grcTextureFactory::CreateParams qparams;
		qparams.Format						= XENON_SWITCH(grctfG8R8,grctfA8R8G8B8);
		qparams.HasParent					= true;
		qparams.Parent 						= CRenderTargets::GetBackBuffer();
		qparams.MipLevels					= 6;

		int bitdepth = (__XENON ) ? 16 : 32;
#if __PS3 
		RTMemPoolID poolId = PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED;
		u8 heapId = 6; // ripple heap
#elif __XENON
		RTMemPoolID poolId = XENON_RTMEMPOOL_GENERAL1;
		u8 heapId = kPuddleHeap;
#else
		RTMemPoolID poolId = RTMEMPOOL_NONE;
		u8 heapId = 0;
#endif

#if !__PS3
		m_playerRt = CRenderTargets::CreateRenderTarget( poolId, "Player Ripple Target",
			grcrtPermanent,
			RippleTextureWidth,RippleTextureWidth,
			bitdepth, 
			&qparams,
			heapId);

		if ( poolId !=RTMEMPOOL_NONE)
			m_playerRt->AllocateMemoryFromPool();		
#endif //!__PS3

		 // On PS3 player ripple is packed into BA of ripple texture
		m_rt = CRenderTargets::CreateRenderTarget( poolId,	"Ripple Target",
			grcrtPermanent,
			RippleTextureWidth,RippleTextureWidth,
			bitdepth, 
			&qparams,
			heapId);

		if ( poolId !=RTMEMPOOL_NONE)
			m_rt->AllocateMemoryFromPool();
			
		PS3_ONLY(m_playerRt = m_rt;)

		m_Phase = 0;

		m_maxDuration = (float)killTime;
		m_minDuration = (float)killTime-1.f;
		m_maxSize 		= 0.012f;
		m_minSize 		= 0.02f;
		m_speed 		= 10.f;
		m_rainFactor	= 0.8f;
		
		m_rippleBumpStrength = 0.7f;
#if __BANK
		m_showBump 			= false;
		m_debugNumWaves 	= 0;
#endif

		m_playerOffset = Vec3V(V_ZERO);
		BANK_ONLY(m_drawPlayerRipples = true);
		BANK_ONLY(m_debugWindVel = 0.f);

		m_mapSize 		= 2.f;
		m_playerMapSize	= 8.f;
		m_playerPhase 	= 0.f;
		m_playerSpeed 	= 10.f;

		m_windMaxBumpStrength 	= .12f;
		m_windOffset	= 0.f;
		m_windDamping 	= 0.05f;
		m_windBumpStrength 	= .0f;
	}

	void GetSettings()
	{
		Assert(g_visualSettings.GetIsLoaded());
		m_minSize = g_visualSettings.Get( "puddles.ripples.minsize", m_minSize );
		m_maxSize = g_visualSettings.Get( "puddles.ripples.maxsize", m_maxSize );
		m_speed = g_visualSettings.Get( "puddles.ripples.speed", m_speed );

		m_minDuration = g_visualSettings.Get( "puddles.ripples.minduration" , m_minDuration);
		m_maxDuration = g_visualSettings.Get( "puddles.ripples.maxduration" ,m_maxDuration );
		m_rainFactor = g_visualSettings.Get( "puddles.ripples.dropfactor", m_rainFactor );

		m_windMaxBumpStrength = g_visualSettings.Get( "puddles.ripples.windstrength", m_windMaxBumpStrength );
		m_rippleBumpStrength = g_visualSettings.Get( "puddles.ripples.ripplestrength" , m_rippleBumpStrength);

		m_playerSpeed = g_visualSettings.Get( "puddles.ripples.playerspeed" , m_playerSpeed);
		m_playerMapSize = g_visualSettings.Get( "puddles.ripples.playermapsize" ,m_playerMapSize );
		m_windDamping = g_visualSettings.Get( "puddles.ripples.winddamping", m_windDamping );
	}
	void Init ( grmShader* shader )
	{
		m_rippleDataLocalVar = shader->LookupVar("RippleData");
		m_rippleWindBumpLocalVar  = shader->LookupVar("PuddleBump");  // reuse puddle bump sampler
		m_rippleScaleOffset  = shader->LookupVar("RippleScaleOffset");
		m_ripplePlayerBumpLocalVar = shader->LookupVar("PuddlePlayerBump");

		m_windRippleTech = shader->LookupTechnique("WindRippleTechnique");
		m_rippleTech = shader->LookupTechnique("RippleTechnique");
		m_footRippleTech = shader->LookupTechnique("FootRippleTechnique");
		m_clearRippleTech = shader->LookupTechnique("ClearRippleTechnique");
	}
	~Ripples(){
		SAFE_DELETE(m_rt);	
#if !__PS3
		SAFE_DELETE(m_playerRt);	
#endif			 
	}
	
	void killDeadRipples( RippleArray& ripples, float phase )
	{
		for (int i=0;i< ripples.GetCount(); ) {    // clear out any dead waves
			if ( ripples[i].life(phase) < 0.f) {    
				ripples.DeleteFast(i); 
			}
			else
				i++;
		}
	}
	void Update(  float rainyness ) 
	{
		if (!fwTimer::IsGamePaused())
		{
			float secs = (float)TIME.GetSeconds();
			m_Phase+=m_speed*secs;
			m_playerPhase +=m_playerSpeed*secs * m_mapSize/m_playerMapSize;

			Vec3V wind;
			WIND.GetGlobalVelocity(WIND_TYPE_AIR,wind);		
			const float windVel = MagXY( wind).Getf();
			m_windOffset += m_windDamping * windVel * secs;
			m_windBumpStrength = m_windMaxBumpStrength * Clamp(windVel, 0.f, 16.f)*1.f/16.f;
			BANK_ONLY(m_debugWindVel = windVel);
		}
		
		float rc = rainyness*m_rainFactor;
		killDeadRipples( m_ripples, m_Phase );

		float noRainRipples = g_timeCycle.GetStaleNoRainRipples();
		if(noRainRipples > 0.5f)
		{
			return;
		}
		
		for (int i=0;i<12;i++)
		{
			if ( m_rnd.GetFloat() <rc  && !m_ripples.IsFull() ) 
			{			
				float wavelen = m_minSize + m_rnd.GetFloat()*rainyness*(m_maxSize-m_minSize);
				float life = m_minDuration + m_rnd.GetFloat()*(m_maxDuration-m_minDuration);
				m_ripples.Append()= Ripple( m_rnd.GetFloat(), m_rnd.GetFloat(), wavelen, m_Phase, life);			
			}
		}		
	}

	bool UpdatePlayerRipples( Vec3V_In playerPos, SplashData& splash )
	{
		killDeadRipples( m_playerRipples, m_playerPhase);

		bool addSplash =false;
		if (!CGameWorld::FindLocalPlayer())
			return addSplash;

		float pz = CGameWorld::FindLocalPlayer()->GetGroundPos().z;
		float heading = CGameWorld::FindLocalPlayer()->GetCurrentHeading();
		static bank_float  bias = 0.05f;
		static bank_float  footsize = 0.15f;  // should be ~ half a foot

		Vector3 pedHdg(-rage::Sinf(heading), rage::Cosf(heading),0.f);
		pedHdg.Scale( footsize);
	
		CPedFootStepHelper& footstep = CGameWorld::FindLocalPlayer()->GetFootstepHelper();
		for (int i=0;i<2;i++)
		{
			Vec3V pos;
			footstep.GetPositionForPedSound((FeetTags)i, pos);
			pos += VECTOR3_TO_VEC3V(pedHdg);
			if ( pos.GetZf()  < ( pz+bias)){
				float vel = footstep.GetFootVelocity((FeetTags)i).Getf();
				AddPlayerRipple(pos, vel);

				static bank_float wavethreshold = 0.1f;
				if (  m_rnd.GetFloat()*wavethreshold < vel){
					splash.position = pos;
					splash.vel=vel;
					addSplash=true;
				}
			}
		}
		m_playerOffset = -playerPos;
		return addSplash;
	}
	void AddPlayerRipple( Vec3V_In pos, float vel)
	{
		static bank_float wavethreshold = 0.1f;
		bool addRipple = m_rnd.GetFloat()*wavethreshold < vel;

		if ( addRipple && !m_playerRipples.IsFull() )
		{
			static bank_float sizeMod = 0.6f;
			static bank_float lifeMode = 2.f;
			static bank_float sizeFactor =0.3f;

			float wavelen = m_minSize + m_rnd.GetFloat()*(m_maxSize-m_minSize);
			float life = m_minDuration + m_rnd.GetFloat()*(m_maxDuration-m_minDuration);
			Ripple ripple( pos.GetXf(), pos.GetYf(), 
				wavelen*sizeMod *Min(vel*sizeFactor+(1.f-sizeFactor),1.3f),
				m_playerPhase, life*lifeMode );	
			{
				// allow for multiple threads updating this
				sysCriticalSection lock(m_lockToken);
				if (!m_playerRipples.IsFull() )  // check again 
					m_playerRipples.Append()= ripple;
			}			
		}	
	}

	grcTexture* Get() const { return m_rt;}

	grcTexture* GetPlayer() const { return m_playerRt;}

	void SetPosition( grmShader* shader, Vec3V_In pos ){
		Vec4V splashPos(pos);
		splashPos.SetW( 1.0f );
		shader->SetVar( m_rippleDataLocalVar, splashPos );
	}
	void DrawRipples( grmShader* shader, const Vec4V tiledWaves[MaxShaderRipples],  const Ripple* parentWaves[MaxShaderRipples], int amt, bool isfoot ) const
	{
		float phase = isfoot ? m_playerPhase : m_Phase;
		shader->SetVar( m_rippleDataLocalVar, Vec3V(0.f , m_windBumpStrength*0.25f, m_rippleBumpStrength) );

		if (shader->BeginDraw(grmShader::RMC_DRAW, true, isfoot ? m_footRippleTech : m_rippleTech))
		{
			shader->Bind(0);
			for(int j=0;j<amt;){
#if RSG_PC && !__BANK
				int batchSize = Min(RippleVertices::BatchSize*(int)GRCDEVICE.GetGPUCount(), amt-j);
#else
				int batchSize = Min(RippleVertices::BatchSize, amt-j);
#endif
				RippleVertices::Vertex* verts= m_rippleVerts.Begin(batchSize);
				if ( verts){

					for(int i=0;i<batchSize;i++){

						ScalarV rad(parentWaves[i]->radius(phase));
						ScalarV l = tiledWaves[i].GetX() - rad;
						ScalarV r = tiledWaves[i].GetX() + rad;
						ScalarV t = tiledWaves[i].GetY() - rad;
						ScalarV b = tiledWaves[i].GetY() + rad;
						m_rippleVerts.Add( verts, l,t,r,b, tiledWaves[i], ScalarV(parentWaves[i]->life(phase)));
					}
					m_rippleVerts.End();
				}

				j+= batchSize;
			}
			shader->UnBind();
		}
		shader->EndDraw();	

	}
	void FlattenEdges(grmShader* shader ) const{
		// set the edges to a flat color
		grcStateBlock::SetBlendState( grcStateBlock::BS_Default);

		float borderRange = 2.f/(float)RippleTextureWidth;		
		if (shader->BeginDraw(grmShader::RMC_DRAW, true,  m_clearRippleTech))
		{
			shader->Bind(0);
			grcBeginQuads(4);
			grcDrawQuadf(0.f,0.f ,1.f, 0.f+borderRange,		0.0f, 0.f,0.f,0.f,0.f, Color32());
			grcDrawQuadf(0.f,0.f , borderRange, 1.f,			0.0f, 0.f,0.f,0.f,0.f, Color32());
			grcDrawQuadf(0.f,1.f ,1.f, 1.f-borderRange,		0.0f, 0.f,0.f,0.f,0.f, Color32());
			grcDrawQuadf(1.f-borderRange,1.f ,1.f, 0.f,		0.0f, 0.f,0.f,0.f,0.f, Color32());
			grcEndQuads();
			shader->UnBind();
		}
		shader->EndDraw();
	}

	void Generate( grmShader* shader ) const {
		PF_PUSH_MARKER("Calculate Ripple Texture");
		grcStateBlock::SetStates( grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_Default, grcStateBlock::BS_Default );

		Vec4V tiledRipples[MaxShaderRipples];
		const Ripple* parents[MaxShaderRipples];
		int amt = genTiledWaves( tiledRipples, parents);

		shader->SetVar( m_rippleDataLocalVar, Vec3V(m_windOffset , m_windBumpStrength, m_rippleBumpStrength) );
		shader->SetVar( m_rippleWindBumpLocalVar, Water::GetBumpTexture());

		CRenderTargets::UnlockSceneRenderTargets();
		grcTextureFactory::GetInstance().LockRenderTarget(0, m_rt, NULL);

		if (shader->BeginDraw(grmShader::RMC_DRAW, true, m_windRippleTech))
		{
			shader->Bind(0);
			grcDrawSingleQuadf(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
			shader->UnBind();
		}
		shader->EndDraw();


		DrawRipples( shader, tiledRipples, parents, amt, false);

		grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if RSG_ORBIS
		m_rt->GenerateMipmaps();
#endif

		PF_POP_MARKER();

		BANK_ONLY( if ( m_drawPlayerRipples) )
		{
			PF_PUSH_MARKER("Calculate Player Ripple Texture");
#if !__PS3
			grcTextureFactory::GetInstance().LockRenderTarget(0, m_playerRt, NULL);

			// clear to normal up
			GRCDEVICE.Clear( true,Color32(128,128,255, 0),false,false,false,0);
#endif

			shader->SetVar( m_rippleScaleOffset, GetPlayerRTInvScaleOffset());
			amt = genPlayerWaves(  tiledRipples, parents, GetPlayerRTScaleOffset() );
			DrawRipples( shader, tiledRipples, parents, amt, true);
			FlattenEdges(shader);
			grcResolveFlags* resolveFlags = NULL;
#if __D3D11
			grcResolveFlags resolveMips;
			resolveMips.MipMap = true;
			resolveFlags = &resolveMips;
#endif
			grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags);
#if RSG_ORBIS
			m_playerRt->GenerateMipmaps();
#endif
			PF_POP_MARKER();
		}
		shader->SetVar( m_rippleScaleOffset, GetPlayerRTScaleOffset());  // default scale offset is fine here.
		shader->SetVar(  m_ripplePlayerBumpLocalVar, GetPlayer());

#if	USE_COMBINED_PUDDLEMASK_PASS
		if (GRCDEVICE.IsReadOnlyDepthAllowed())
			CRenderTargets::LockSceneRenderTargets_DepthCopy();
		else
			CRenderTargets::LockSceneRenderTargets();
#else // USE_COMBINED_PUDDLEMASK_PASS
		CRenderTargets::LockSceneRenderTargets();				
#endif // USE_COMBINED_PUDDLEMASK_PASS
	}
	Vec4V GetPlayerRTScaleOffset() const {
		ScalarV playerScale(m_mapSize/m_playerMapSize);   
		Vec4V scaleOffset( playerScale, playerScale, playerScale*m_playerOffset.GetX()+ScalarV(V_HALF),  playerScale*m_playerOffset.GetY()+ScalarV(V_HALF));
		return scaleOffset;
	}
	Vec4V GetPlayerRTInvScaleOffset() const {
		ScalarV playerScale(m_mapSize/m_playerMapSize);  
		playerScale = Invert( playerScale);
		Vec4V scaleOffset( playerScale, playerScale, -m_playerOffset.GetX()-ScalarV(V_HALF)*playerScale,  -m_playerOffset.GetY()-ScalarV(V_HALF)*playerScale);
		return scaleOffset;
	}
	int genPlayerWaves( Vec4V tiledWaves[MaxShaderRipples], const Ripple* parentWaves[MaxShaderRipples], Vec4V_In scaleOffset ) const
	{
		Assert(  MaxShaderRipples >= m_playerRipples.GetMaxCount());
		for (int i=0;i< m_playerRipples.GetCount();i++) {
			tiledWaves[ i]=m_playerRipples[i].PackedWave(m_playerPhase);
			tiledWaves[i].SetX( tiledWaves[i].GetX() * scaleOffset.GetX() + scaleOffset.GetZ());
			tiledWaves[i].SetY( tiledWaves[i].GetY() * scaleOffset.GetY() + scaleOffset.GetW());
			parentWaves[i ] = &m_playerRipples[i];		
		}
		return m_playerRipples.GetCount();
	}
	int genTiledWaves( Vec4V tiledWaves[MaxShaderRipples], const Ripple* parentWaves[MaxShaderRipples] ) const
	{
		int numTiledWaves = 0;
		int amt=Min( MaxShaderRipples, m_ripples.GetCount());
		for (int i=0;i< amt;i++) {
			tiledWaves[ numTiledWaves]=m_ripples[i].PackedWave(m_Phase);
			parentWaves[numTiledWaves ] = &m_ripples[i];

			numTiledWaves = Min( numTiledWaves+1, MaxShaderRipples -1);
			// check for tiling along edges
			float wr =m_ripples[i].radius(m_Phase);
			for (int k=0;k<2;k++)    
			{
				float p = k==0 ? m_ripples[i].x : m_ripples[i].y;
				float nx = p + ( p < .5 ? 1.f : -1.f);
				if ( ( nx+wr > 0 && nx+wr < 1) ||
					(nx-wr > 0 && nx-wr < 1) )
				{
					tiledWaves[ numTiledWaves]=m_ripples[i].PackedWave(m_Phase);
					tiledWaves[ numTiledWaves][k]=nx;
					parentWaves[numTiledWaves ] = &m_ripples[i];
					numTiledWaves = Min( numTiledWaves+1, MaxShaderRipples -1);
				}
			}
		}			
		BANK_ONLY(m_debugNumWaves = numTiledWaves );
		return numTiledWaves;
	}
#if __BANK
	void AddWidgets( bkBank& bk)
	{
		bk.PushGroup("Ripple Gen");
		
		bk.AddSlider("Min Size", &m_minSize, 0.00001f, 0.3f, 0.005f);
		bk.AddSlider("Max Size", &m_maxSize, 0.00001f, 0.3f, 0.005f);
		bk.AddSlider("Speed", &m_speed, 0.f, 20.f, 0.1f);
		bk.AddSlider("Min Duration", &m_minDuration, 0.f, 20.f, 0.1f);
		bk.AddSlider("Max Duration", &m_maxDuration, 0.f, 20.f, 0.1f);

		bk.AddSlider("Rain Drop Factor", &m_rainFactor, 0.f, 1.f, 0.05f);	
		bk.AddSlider("Ripple Strength", &m_rippleBumpStrength, 0.f, 2.f, 0.05f);	
		bk.PushGroup("Wind");
		bk.AddSlider("Wind Strength", &m_windMaxBumpStrength, 0.f, 2.f, 0.05f);
		bk.AddSlider("Damping", &m_windDamping, 0.f, 2.f, 0.05f);
		bk.AddText("Velocity", &m_debugWindVel);
		bk.PopGroup();

		bk.AddText("Num Waves", &m_debugNumWaves );
		bk.AddToggle("Show Ripple", &m_showBump);

		bk.PopGroup();

		bk.PushGroup("Player Ripples");
		bk.AddToggle("Activate", &m_drawPlayerRipples);
		bk.AddSlider("Map Size", &m_playerMapSize, 0.001f, 20.f, 0.01f);
		bk.AddSlider("Speed", &m_playerSpeed, 0.f, 20.f, 0.1f);		
		bk.PopGroup();
	}

	bool ShowBump() const { return m_showBump; }
#endif
};
 


void SplashCallbackFunc(const SplashData& OUTPUT_ONLY(splash ))
{
	Displayf(" Splash FX %f Ah Ah!!!!", splash.vel);
}

void SplashCallbackFuncAudio(const SplashData& UNUSED_PARAM(splash) )
{
	CPedFootStepHelper::SetWalkingOnPuddle(true);
}
class SplashQueries
{

	static const int MinFramesForResult =  3; //PS3 seems faster than PC/360
	static const int QueryAmt = 12;
public:
	class COcclusionQuery
	{
		grcOcclusionQuery	m_query;
		int					m_frameNo;
		SplashData			m_data;
	public:
		COcclusionQuery(){
			m_query	= GRCDEVICE.CreateOcclusionQuery();		
			m_frameNo = -1;
		}
		~COcclusionQuery(){
			GRCDEVICE.DestroyOcclusionQuery(m_query);
		}
		void Begin(const SplashData& data)
		{
			Assert( Available());
			m_data = data;
			m_frameNo = TIME.GetFrameCount();
			GRCDEVICE.BeginOcclusionQuery(m_query);
		}
		void End()
		{
			GRCDEVICE.EndOcclusionQuery(m_query);
		}
		bool Active(int currentFrame ){
			return !Available() && (currentFrame - m_frameNo) >= MinFramesForResult;
		}
		bool Collect( int currentFrame, int threshold ){
			if ( !Active(currentFrame))
				return false;

			u32 queryResult;
			bool isVisible = false;
			if(m_query && (GRCDEVICE.GetOcclusionQueryData(m_query, queryResult) != false))			
			{
				isVisible = queryResult > threshold;
			}
			m_frameNo = -1;
			return isVisible;
		}
		bool Available(){
			return m_frameNo ==-1;
		}
		const SplashData& GetData()  { return m_data; }
	};

	atRangeArray<COcclusionQuery, QueryAmt> m_splashQueries;

	COcclusionQuery* GetAvailableQuery()
	{
		for (int i=0;i<m_splashQueries.GetMaxCount();i++)
			if ( m_splashQueries[i].Available())
				return &m_splashQueries[i];
		return NULL;
	}
	void UpdateResults(atFixedArray<SplashCallback,8>& callbacks){
		int currentFrame = TIME.GetFrameCount();
		for (int i=0; i< m_splashQueries.GetMaxCount();i++)
		{
			if ( m_splashQueries[i].Collect( currentFrame, 16))
			{
				for(int j=0;j<callbacks.GetCount();j++)
					callbacks[j]( m_splashQueries[i].GetData());			
			}
		}
	}
};

#define _PUDDLES_NO_SCREEN_REFLECTION 1

PuddlePass::PuddlePass() 
{

	m_PuddleBumpTextureLocalVar=grcevNONE;
	// setup defaults.
	scale=0.050f;
	range=0.03f;
	depth=0.65f;
	depthTest=0.0f;
	reflectivity=1.f;
	minAlpha=0.8f;
	aoCutoff=0.2f;
	reflectTransitionPoint = 0.6f;
	grcRasterizerStateDesc rastDesc; 
	rastDesc.DepthBiasDX9         = 0.000005f;
	m_rastDepthBias_RS = grcStateBlock::CreateRasterizerState(rastDesc);

	u8 materialMask = (u8)(~(DEFERRED_MATERIAL_TERRAIN|DEFERRED_MATERIAL_SPAREOR1|DEFERRED_MATERIAL_SPAREOR2));

	grcDepthStencilStateDesc stencilSetupDepthStencilState;
	stencilSetupDepthStencilState.DepthEnable=false;
	stencilSetupDepthStencilState.DepthWriteMask=false;
	stencilSetupDepthStencilState.DepthFunc = grcRSV::CMP_LESS;
	stencilSetupDepthStencilState.StencilEnable = TRUE;	
	stencilSetupDepthStencilState.StencilReadMask= materialMask;  // ignore terrain bit and lighting bits
	stencilSetupDepthStencilState.StencilWriteMask=0x0;

	// only check the interior bit
	
	stencilSetupDepthStencilState.FrontFace.StencilFunc =  grcRSV::CMP_EQUAL;
	stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
	m_StencilOutInterior_DS = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState); 


	stencilSetupDepthStencilState.StencilWriteMask = 0; // want below but breaks scull
#if __XENON
	stencilSetupDepthStencilState.StencilWriteMask = DEFERRED_MATERIAL_SPECIALBIT;
	stencilSetupDepthStencilState.FrontFace.StencilPassOp =  grcRSV::STENCILOP_ZERO; 
	stencilSetupDepthStencilState.StencilReadMask =(u8)~(DEFERRED_MATERIAL_SPAREOR1|DEFERRED_MATERIAL_SPAREOR2);
#else
	stencilSetupDepthStencilState.StencilReadMask = materialMask ;
#endif

	stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
	m_StencilUseMask_DS = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState); 

	stencilSetupDepthStencilState.StencilReadMask=materialMask;  // ignore terrain bit and lighting bits
	stencilSetupDepthStencilState.StencilWriteMask = DEFERRED_MATERIAL_SPECIALBIT;
	stencilSetupDepthStencilState.FrontFace.StencilFunc =  grcRSV::CMP_GREATER;   // use greater so only terrain and default are marked
	stencilSetupDepthStencilState.FrontFace.StencilPassOp =  grcRSV::STENCILOP_REPLACE; 
	stencilSetupDepthStencilState.BackFace = stencilSetupDepthStencilState.FrontFace; 
	m_StencilSetupMask_DS = grcStateBlock::CreateDepthStencilState(stencilSetupDepthStencilState); 


	grcBlendStateDesc BSDesc;
	BSDesc.BlendRTDesc[0].BlendEnable = false;
	BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	m_StencilSetupMask_BS = grcStateBlock::CreateBlendState(BSDesc);


	grcBlendStateDesc BSApplyDesc;
	BSApplyDesc.BlendRTDesc[0].BlendEnable = true;

	BSApplyDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA; 
	BSApplyDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	BSApplyDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
	BSApplyDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;

	m_applyPuddles_BS = grcStateBlock::CreateBlendState(BSApplyDesc);

	useRippleGen = true;
	usePuddleMask = __PS3 || __XENON || __D3D11 || RSG_ORBIS;
	m_ripples = rage_new Ripples();
	m_splashes = NULL;

//	AddSplashCallback( SplashCallbackFunc);
	AddSplashCallback( SplashCallbackFuncAudio);
}
PuddlePass::~PuddlePass(){
	SAFE_DELETE(m_splashes);
	SAFE_DELETE(m_ripples);
}
void PuddlePass::GetSettings()
{
	if (g_visualSettings.GetIsLoaded())
	{
		scale = g_visualSettings.Get( "puddles.scale" );
		range = g_visualSettings.Get( "puddles.range" );
		depth = g_visualSettings.Get( "puddles.depth" );
		depthTest = g_visualSettings.Get( "puddles.depthtest" );
		reflectivity = g_visualSettings.Get( "puddles.reflectivity" , reflectivity);	

		if(m_ripples)
			m_ripples->GetSettings();
	}
}
grcEffectTechnique PuddlePass::GetTechnique() const
{
#if USE_COMBINED_PUDDLEMASK_PASS
	return m_puddleMaskPassCombinedTechnique;
#else
#if __BANK
	if ( m_ripples->ShowBump())
		return m_puddleDebugTechnique;
#endif
	return m_puddleTechnique;
#endif
}
void DrawScreenQuad( const grmShader* shader, grcEffectTechnique tech )
{
	if (shader->BeginDraw(grmShader::RMC_DRAW, true, tech))
	{
		shader->Bind(0);
		grcDrawSingleQuadf(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
		shader->UnBind();
	}
	shader->EndDraw();
}

bool PuddlePass::GenerateRipples( grmShader* shader, SplashData& splash)
{
	if ( Unlikely( m_PuddleBumpTextureLocalVar == grcevNONE ) )
		SetShaderVars(shader);

	Vector3 playerPos = CGameWorld::FindLocalPlayerCoors();
	Vec3V vPlayerPos = RCC_VEC3V(playerPos);

	bool addSplash= m_ripples->UpdatePlayerRipples( vPlayerPos, splash );
	m_ripples->Update(g_weather.GetRain() );
	m_ripples->Generate( shader );
	return addSplash;
}


void PuddlePass::UpdateVisualDataSettings()
{
#if !__GAMETOOL
	PuddlePassSingleton::GetInstance().GetSettings();
#endif
}


void PuddlePass::SetShaderVars( grmShader* shader )
{
	m_PuddleBumpTextureLocalVar = shader->LookupVar("PuddleBump");
	m_Puddle_ScaleXY_RangeLocalVar	= shader->LookupVar("Puddle_ScaleXY_Range");
	m_PuddleParamsLocalVar= shader->LookupVar("PuddleParams");
	m_PuddleMaskTextureLocalVar = shader->LookupVar("PuddleMask");		
	m_AOTextureLocalVar =  shader->LookupVar("deferredLightTexture2");
	m_GBufferTexture3ParamLocalVar = shader->LookupVar("GBufferTexture3Param");

	m_puddleTechnique = shader->LookupTechnique("puddlepass", false);
	m_puddleMaskTechnique = shader->LookupTechnique("puddleMask", false);
#if USE_COMBINED_PUDDLEMASK_PASS
	m_puddleMaskPassCombinedTechnique = shader->LookupTechnique("puddleMaskAndPassCombined");
#endif
	BANK_ONLY(m_puddleDebugTechnique = shader->LookupTechnique("puddlePassDebug"));

	m_puddleTestTechnique = shader->LookupTechnique("PuddleTestTechnique");
	m_splashes = rage_new SplashQueries();
	m_ripples->Init(shader);
	GetSettings();
}

void PuddlePass::GetQueriesResults()
{
	if( m_splashes )
	{
		m_splashes->UpdateResults(m_callbacks);
	}
}

void PuddlePass::Draw( grmShader* shader, bool generateRipples )
{
	if ( Unlikely( m_PuddleBumpTextureLocalVar == grcevNONE ) )
		SetShaderVars(shader);
	
	DeferredLighting::SetShaderGBufferTargets();
#if __D3D11
	DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBufferCopy());
#endif
	BANK_ONLY(m_wetness = g_weather.GetWetness() );
	
	float GBuffer3SizeX = (float)GBuffer::GetTarget(GBUFFER_RT_3)->GetWidth();
	float GBuffer3SizeY = (float)GBuffer::GetTarget(GBUFFER_RT_3)->GetHeight();
	shader->SetVar(m_GBufferTexture3ParamLocalVar, Vec4V(GBuffer3SizeX, GBuffer3SizeY, 1.0f/GBuffer3SizeX, 1.0f/GBuffer3SizeY));

	float fdepth= 1.0f - Clamp(g_weather.GetWetness() * depth, 0.0f, 1.0f);
	float reflectAmt = Clamp( (g_weather.GetWetness()-reflectTransitionPoint) * reflectivity/(1.f-reflectTransitionPoint), 0.0f, 1.0f);

	grcTexture* puddleLayout = CShaderLib::LookupTexture("puddle_layout");
	Assertf( puddleLayout, " Puddle layout texture isn't available" );
	shader->SetVar(m_PuddleMaskTextureLocalVar, puddleLayout);
	float rs = range*7.f;
	shader->SetVar(m_Puddle_ScaleXY_RangeLocalVar, Vec4V(scale,rs-1.f, 1.f/rs,depthTest) );
	shader->SetVar(m_PuddleParamsLocalVar, Vec4V(fdepth,reflectAmt,minAlpha,aoCutoff) );
	
	// ripples use the puddlemask texture so we have to set it here.
	if ( useRippleGen && generateRipples ) {
		SplashData splash;
		if ( GenerateRipples( shader, splash))
		{
			// add a splash visiblity query
			SplashQueries::COcclusionQuery* query = m_splashes->GetAvailableQuery();
			if ( query)
			{
				static dev_float testRad=.1f;
				grcStateBlock::SetDepthStencilState( m_StencilUseMask_DS, 0);	
				m_ripples->SetPosition( shader, splash.position);
				query->Begin(splash);
				if (shader->BeginDraw(grmShader::RMC_DRAW, true, m_puddleTestTechnique))
				{
					shader->Bind(0);
					grcDrawSphere( testRad, splash.position,8, true, true);
					shader->UnBind();
				}
				shader->EndDraw();
				query->End();				
			}
		}
	}
#if __D3D11
	if (GRCDEVICE.IsReadOnlyDepthAllowed())
		DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthBuffer());
#endif

	DeferredLighting::SetShaderGBufferTarget(GBUFFER_RT_1,	GBuffer::GetTarget(GBUFFER_RT_1));

	shader->SetVar(m_PuddleBumpTextureLocalVar,	m_ripples->Get());
	shader->SetVar( m_AOTextureLocalVar, SSAO::GetSSAOTexture());
	
	CRenderPhaseReflection::SetReflectionMap();

#if USE_COMBINED_PUDDLEMASK_PASS
	CRenderTargets::UnlockSceneRenderTargets();
	if (GRCDEVICE.IsReadOnlyDepthAllowed())
		CRenderTargets::LockSceneRenderTargets_DepthCopy();
	else
		CRenderTargets::LockSceneRenderTargets();
	grcStateBlock::SetDepthStencilState( m_StencilUseMask_DS, 0);	
#else
	if ( usePuddleMask)
	{
		// Mark stencil only
#if __PS3
		u8 materialMask = (u8)(~(DEFERRED_MATERIAL_TERRAIN|DEFERRED_MATERIAL_SPAREOR1|DEFERRED_MATERIAL_SPAREOR2));
		CRenderTargets::ResetStencilCull(true,  DEFERRED_MATERIAL_SPECIALBIT, materialMask);
		SetDepthBoundsFromRange( grcViewport::GetCurrent()->GetNearClip(),grcViewport::GetCurrent()->GetFarClip(), PUDDLE_MAX_RANGE);
#endif //__PS3
		CRenderTargets::UnlockSceneRenderTargets();
		grcTextureFactory::GetInstance().LockRenderTarget(0, NULL, CRenderTargets::GetDepthBuffer());  // GBuffer::Get4xHiZRestore()) gives 25% speedup on 360

		grcStateBlock::SetDepthStencilState( m_StencilSetupMask_DS, DEFERRED_MATERIAL_SPECIALBIT);
		int RefValue=20;
		grcStateBlock::SetBlendState( m_StencilSetupMask_BS, ~0U, ~0ULL, RefValue);

		// Draw
		DrawScreenQuad( shader, m_puddleMaskTechnique );

		grcResolveFlags flags;
		flags.NeedResolve = false;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);
		CRenderTargets::LockSceneRenderTargets();
		
#if __XENON
		CRenderTargets::RefreshStencilCull(true, DEFERRED_MATERIAL_SPECIALBIT, DEFERRED_MATERIAL_CLEAR);
#endif
		grcStateBlock::SetDepthStencilState( m_StencilUseMask_DS, DEFERRED_MATERIAL_SPECIALBIT);	
	}
	else {
		grcStateBlock::SetDepthStencilState( m_StencilOutInterior_DS, DEFERRED_MATERIAL_DEFAULT);
		PS3_ONLY( SetDepthBoundsFromRange( grcViewport::GetCurrent()->GetNearClip(),grcViewport::GetCurrent()->GetFarClip(), PUDDLE_MAX_RANGE));

#if !__WIN32PC && !RSG_DURANGO && !RSG_ORBIS
#if __XENON
		CRenderTargets::RefreshStencilCull(false, DEFERRED_MATERIAL_CLEAR,DEFERRED_MATERIAL_CLEAR);
#else
		CRenderTargets::RefreshStencilCull(true, DEFERRED_MATERIAL_DEFAULT,(u8)(~(DEFERRED_MATERIAL_TERRAIN|DEFERRED_MATERIAL_SPAREOR1|DEFERRED_MATERIAL_SPAREOR2)) PS3_ONLY(, false));
#endif
#endif
	}
#endif //USE_COMBINED_PUDDLEMASK_PASS
	
	grcStateBlock::SetBlendState(m_applyPuddles_BS);
	DrawScreenQuad( shader, GetTechnique() );

#if USE_COMBINED_PUDDLEMASK_PASS
	CRenderTargets::UnlockSceneRenderTargets();
	CRenderTargets::LockSceneRenderTargets();
#endif

#if __XENON
	if ( usePuddleMask)
	{
		CRenderTargets::ClearStencilCull( DEFERRED_MATERIAL_SPECIALBIT );
		CRenderTargets::RefreshStencilCull(true, DEFERRED_MATERIAL_SPECIALBIT|DEFERRED_MATERIAL_TERRAIN,DEFERRED_MATERIAL_CLEAR);
		grcStateBlock::SetDepthStencilState( m_StencilUseMask_DS,  DEFERRED_MATERIAL_SPECIALBIT|DEFERRED_MATERIAL_TERRAIN);	
		grcStateBlock::SetBlendState(m_applyPuddles_BS);
		DrawScreenQuad( shader, GetTechnique() );
	}
#endif

#if __PS3
	grcDevice::SetDepthBoundsTestEnable(false);
	CRenderTargets::RefreshStencilCull(false, DEFERRED_MATERIAL_CLEAR, 0x07); //refresh stencil for sky/refresh hi-z for water main pass
#endif //__PS3

	XENON_ONLY(CRenderTargets::ClearStencilCull( usePuddleMask ? DEFERRED_MATERIAL_SPECIALBIT|DEFERRED_MATERIAL_TERRAIN : DEFERRED_MATERIAL_DEFAULT);)
}

#if __BANK
void PuddlePass::InitWidgets()
{
	bkBank *pMainBank = BANKMGR.FindBank("Renderer");
	Assert(pMainBank);
	pMainBank->PushGroup("Puddles", false);
	bkBank& bank = *pMainBank;

	bank.AddSlider("scale", &scale, 0.0f, 1.0f, 0.001f);
	bank.AddSlider("range", &range,0.0f,1.f,0.0001f);
	bank.AddSlider("depth", &depth,0.0f,1.f,0.01f);
	bank.AddSlider("depthTest", &depthTest,0.0f,1.f,0.01f);
	bank.AddSlider("reflectivity", &reflectivity, 0.f, 2.f, 0.001f);
	bank.AddSlider("reflection transition point", &reflectTransitionPoint, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("min alpha", &minAlpha, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("ao cutoff", &aoCutoff, 0.0f, 1.0f, 0.01f);
	bank.AddToggle("use ripple gen", &useRippleGen);
	bank.AddToggle("use puddle mask", &usePuddleMask);
	bank.AddText("Wetness",&m_wetness);
	m_ripples->AddWidgets(bank);
	pMainBank->PopGroup();

}

#endif

void PuddlePass::AddPlayerRipple( Vec3V_In rippleLocation, float velocity ){
	m_ripples->AddPlayerRipple( rippleLocation, velocity);	
}

const grcTexture* PuddlePass::GetRippleTexture()
{
	return m_ripples->Get();
}
