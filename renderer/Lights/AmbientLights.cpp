#include "Renderer/Lights/AmbientLights.h"
#include "grcore/allocscope.h"
#include "camera/caminterface.h"
#include "debug/debugdraw.h"
#include "fwsys/gameskeleton.h"
#include "fwtl/customvertexpushbuffer.h"
#include "renderer/deferred/deferredlighting.h"
#include "renderer/postprocessfx.h"
#include "renderer/rendertargets.h"
#include "renderer/util/shaderutil.h"
#include "shaders/shaderlib.h"

RENDER_OPTIMISATIONS()

#define AMBIENT_VTX_CNT (1024U)

float AmbientLights::sm_pullBack = 0.5f;
float AmbientLights::sm_pullFront = 2.0f;
float AmbientLights::sm_falloff = 1.0f; 
float AmbientLights::sm_ebPullBack = 3.0f;
float AmbientLights::sm_ebPullFront = 5.0f;
float AmbientLights::sm_ebFalloff = 3.0f;

#if __BANK
unsigned int AmbientLights::sm_renderedCount = 0;
bool g_drawAmbientLightsDebug = false;
#endif // __BANK

grcDepthStencilStateHandle AmbientLights::sm_DSState;
grcRasterizerStateHandle AmbientLights::sm_RState;
grcBlendStateHandle AmbientLights::sm_BState;

struct VtxPB_AL_trait 
{
	//float3 pos : POSITION;
	//float4 portalNormal : NORMAL; // normal.xyz, intensity.w
	//float4 portalPos : TEXCOORD0; // portalPos.xyz, pullFront.w 
	//float4 unit1 : TEXCOORD1;	// unit1.xyz, line1Length.w
	//float4 unit2 : TEXCOORD2;	// unit2.xyz, line2Length.w

	float		_pos[3];		// grcFvf::grcdsFloat3;	// POSITION
	float		_normal[4];		// grcFvf::grcdsFloat3;	// NORMAL
	float		_uv0[4];		// grcFvf::grcdsFloat4;	// TEXCOORD0
	float		_uv1[4];		// grcFvf::grcdsFloat4;	// TEXCOORD1
	float		_uv2[4];		// grcFvf::grcdsFloat4;	// TEXCOORD2

	static inline void fvfSetup(grcFvf &fvf)
	{
		fvf.SetPosChannel(true, grcFvf::grcdsFloat3);			// POSITION
		fvf.SetNormalChannel(true, grcFvf::grcdsFloat4);		// NORMAL
		fvf.SetTextureChannel(0, true, grcFvf::grcdsFloat4);	// TEXCOORD0
		fvf.SetTextureChannel(1, true, grcFvf::grcdsFloat4);	// TEXCOORD1
		fvf.SetTextureChannel(2, true, grcFvf::grcdsFloat4);	// TEXCOORD2
	}

	static inline int GetComponentCount() { return 5; }

	static inline void SetPosition(traitParam& param, const Vector3& vp)
	{
		Assert(param.m_VtxElement == 0);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=vp.x;
		*(ptr++)=vp.y; 
		*(ptr++)=vp.z;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP

		ASSERT_ONLY(param.m_VtxElement = 1);
	}

	static inline void SetNormal(traitParam& param, const Vector4& dp)
	{
		Assert(param.m_VtxElement == 1);
#if 0 == VTXBUFFER_USES_NOOP	
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=dp.x;
		*(ptr++)=dp.y;
		*(ptr++)=dp.z;
		*(ptr++)=dp.w;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 2);
	}

	static inline void SetUV(traitParam& param, const Vector4& fv)
	{
		Assert(param.m_VtxElement == 2);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=fv.x;
		*(ptr++)=fv.y;
		*(ptr++)=fv.z;
		*(ptr++)=fv.w;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 3);
	}

	static inline void SetUV1(traitParam& param, const Vector4& sv)
	{
		Assert(param.m_VtxElement == 3);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=sv.x;
		*(ptr++)=sv.y;
		*(ptr++)=sv.z;
		*(ptr++)=sv.w;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 4);
	}

	static inline void SetUV2(traitParam& param, const Vector4& m)
	{
		Assert(param.m_VtxElement == 4);
#if 0 == VTXBUFFER_USES_NOOP
		float *ptr=(float*)param.m_lockPointer;
		*(ptr++)=m.x;
		*(ptr++)=m.y;
		*(ptr++)=m.z;
		*(ptr++)=m.w;
		param.m_lockPointer = ptr;
#endif // 0 == VTXBUFFER_USES_NOOP	
		ASSERT_ONLY(param.m_VtxElement = 5);
	}

	static inline void SetUV3(traitParam& UNUSED_PARAM(param), const Vector4& UNUSED_PARAM(m))
	{
		// No op
	}

	static inline void SetUV4(traitParam& UNUSED_PARAM(param), const Vector4& UNUSED_PARAM(m))
	{
		// No op
	}

	static inline void SetUV(traitParam& UNUSED_PARAM(param), const Vector2& UNUSED_PARAM(t))
	{
		// No op
	}

	static inline void SetTangent(traitParam& UNUSED_PARAM(param), const Vector4& UNUSED_PARAM(t))
	{
		// No op
	}
};

static VtxPushBuffer<VtxPB_AL_trait> g_ALVtxBuffer;

void AmbientLights::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		grcDepthStencilStateDesc DSSdesc;
		DSSdesc.DepthEnable = true;
		DSSdesc.DepthWriteMask = false;
		DSSdesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATEREQUAL);
		DSSdesc.StencilEnable = true;	
		DSSdesc.StencilReadMask = DEFERRED_MATERIAL_INTERIOR;
		DSSdesc.StencilWriteMask = 0x00;
		DSSdesc.FrontFace.StencilFunc =  grcRSV::CMP_NOTEQUAL;
		DSSdesc.BackFace = DSSdesc.FrontFace; 
		sm_DSState = grcStateBlock::CreateDepthStencilState(DSSdesc);
		
		grcRasterizerStateDesc RSDesc;
		RSDesc.CullMode = grcRSV::CULL_FRONT;
		sm_RState = grcStateBlock::CreateRasterizerState(RSDesc);

		grcBlendStateDesc BSDesc;
		BSDesc.BlendRTDesc[0].BlendEnable = TRUE;
		BSDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
		BSDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
		BSDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		BSDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		sm_BState = grcStateBlock::CreateBlendState(BSDesc);
		
		g_ALVtxBuffer.Init(drawTris, AMBIENT_VTX_CNT);
	}
}

void AmbientLights::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		g_ALVtxBuffer.Shutdown();
	}
}

void AmbientLights::Update()
{

}

void PushBox(const Vec3V *points, const Vector4& portalNormal, const Vector4 &portalPos, const Vector4 &unit1, const Vector4 &unit2)
{
#define ALSetVertex(pos) \
	g_ALVtxBuffer.BeginVertex(); \
	g_ALVtxBuffer.SetPosition(pos); \
	g_ALVtxBuffer.SetNormal(portalNormal); \
	g_ALVtxBuffer.SetUV(portalPos); \
	g_ALVtxBuffer.SetUV1(unit1); \
	g_ALVtxBuffer.SetUV2(unit2); \
	g_ALVtxBuffer.EndVertex();

	const Vector3 a = RCC_VECTOR3(points[0]);
	const Vector3 b = RCC_VECTOR3(points[1]);
	const Vector3 c = RCC_VECTOR3(points[2]);
	const Vector3 d = RCC_VECTOR3(points[3]);

	const Vector3 e = RCC_VECTOR3(points[4]);
	const Vector3 f = RCC_VECTOR3(points[5]);
	const Vector3 g = RCC_VECTOR3(points[6]);
	const Vector3 h = RCC_VECTOR3(points[7]);

	ALSetVertex(d);
	ALSetVertex(c);
	ALSetVertex(b);

	ALSetVertex(d);
	ALSetVertex(b);
	ALSetVertex(a);

	ALSetVertex(e);
	ALSetVertex(f);
	ALSetVertex(g);

	ALSetVertex(e);
	ALSetVertex(g);
	ALSetVertex(h);

	ALSetVertex(b);
	ALSetVertex(c);
	ALSetVertex(g);

	ALSetVertex(b);
	ALSetVertex(g);
	ALSetVertex(f);

	ALSetVertex(c);
	ALSetVertex(d);
	ALSetVertex(h);

	ALSetVertex(c);
	ALSetVertex(h);
	ALSetVertex(g);

	ALSetVertex(a);
	ALSetVertex(e);
	ALSetVertex(h);

	ALSetVertex(a);
	ALSetVertex(h);
	ALSetVertex(d);

	ALSetVertex(a);
	ALSetVertex(b);
	ALSetVertex(f);

	ALSetVertex(a);
	ALSetVertex(f);
	ALSetVertex(e);
}

#if __BANK
void PushBoxDebug(const Vec3V *points)
{
	const Vec3V a = points[0];
	const Vec3V b = points[1];
	const Vec3V c = points[2];
	const Vec3V d = points[3];

	const Vec3V e = points[4];
	const Vec3V f = points[5];
	const Vec3V g = points[6];
	const Vec3V h = points[7];

	grcDebugDraw::Poly(d, c, b, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(d, b, a, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(e, f, g, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(e, g, h, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(b, c, g, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(b, g, f, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(c, d, h, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(c, h, g, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(a, e, h, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(a, h, d, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(a, b, f, Color32(0xff,0x00,0xff,0x7f));

	grcDebugDraw::Poly(a, f, e, Color32(0xff,0x00,0xff,0x7f));
}
#endif // __BANK

void AmbientLights::Render()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	const CTimeCycle::portalList& portals = g_timeCycle.GetLocalPortalList();

	int portalCnt = 0;
	for(int i=0;i<portals.GetCount();i++)
	{
		const CTimeCycle::portalData &portal = portals[i];
		
		if( portal.GetInUse() )	
			portalCnt++;
	}
#if __BANK
	sm_renderedCount = portalCnt;
#endif

	if( portalCnt )
	{
#if __PPU
		const int stencilMask = DEFERRED_MATERIAL_INTERIOR;
		const int stencilRef  = DEFERRED_MATERIAL_INTERIOR;

		CRenderTargets::RefreshStencilCull(false, stencilRef, stencilMask);
#endif // __PPU
		
		grcDepthStencilStateHandle currentDSS = grcStateBlock::DSS_Active;
		grcRasterizerStateHandle currentRS = grcStateBlock::RS_Active;
		grcBlendStateHandle currentBS = grcStateBlock::BS_Active;

		grcStateBlock::SetRasterizerState(sm_RState);
		grcStateBlock::SetDepthStencilState(sm_DSState,DEFERRED_MATERIAL_INTERIOR);
		grcStateBlock::SetBlendState(sm_BState);

		DeferredLighting::SetProjectionShaderParams(DEFERRED_SHADER_AMBIENT, MM_DEFAULT);
		DeferredLighting::SetShaderGBufferTargets(WIN32PC_ONLY(true));		// Bind depth copy as we use the true depth as depth buffer for stencil usage
		DeferredLighting::SetShaderSSAOTarget(DEFERRED_SHADER_AMBIENT);
		
#if __XENON
		PostFX::SetLDR8bitHDR10bit();
#elif __PS3
		PostFX::SetLDR8bitHDR16bit();
#else
		PostFX::SetLDR16bitHDR16bit();
#endif

		grcWorldIdentity();

		if (ShaderUtil::StartShader("Ambient Lights", DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_AMBIENT],
			DeferredLighting::m_techniques[MM_DEFAULT][DEFERRED_TECHNIQUE_AMBIENT_LIGHT], 0)) 
		{
			int portalsPerBatch = g_ALVtxBuffer.GetVtxBatchSize() / (12*3);
			int portalsToDraw = portalCnt;
			int portalsPushed = 0;			
			const ScalarV pullBack(sm_pullBack);
			const ScalarV pullFront(sm_pullFront);
			const ScalarV ebPullBack(sm_ebPullBack);
			const ScalarV ebPullFront(sm_ebPullFront);
			const ScalarV falloff(sm_falloff);
			const ScalarV ebFalloff(sm_ebFalloff);
			

			for(int i=0;i<portals.GetCount();i++)
			{
				const CTimeCycle::portalData &portal = portals[i];
				
				if( portal.GetInUse() )
				{
					if( portalsPushed == 0 )
					{
						g_ALVtxBuffer.Begin();
						g_ALVtxBuffer.Lock(12*3*Min(portalsPerBatch, portalsToDraw));
					}
					
					const fwPortalCorners &corners = portal.GetCorners();
					const Vec3V corner0 = corners.GetCornerV(0);
					const Vec3V corner1 = corners.GetCornerV(1);
					const Vec3V corner2 = corners.GetCornerV(2);
					const Vec3V corner3 = corners.GetCornerV(3);

					const Vec3V axis1 = corner0 - corner1;
					const Vec3V axis2 = corner1 - corner2;
					
					const Vec3V line1 = axis1;
					const Vec3V line2 = axis2;
					
					const Vec3V cross = Cross(line1, line2);
					const Vec3V normal = NormalizeFast(cross);
					const Vec3V unit1 = NormalizeFast(line1);
					const Vec3V unit2 = NormalizeFast(line2);

					spdSphere sphere;
					corners.CalcBoundingSphere(sphere);

					const ScalarV back = portal.GetExpandBox() ? ebPullBack : pullBack;
					const ScalarV front = portal.GetExpandBox() ? ebPullFront : pullFront;
					const ScalarV actFalloff = portal.GetExpandBox() ? ebFalloff : falloff;
					
					const Vec3V backOffset = back * normal;
					const Vec3V frontOffset = front * normal;
					
					Vec3V points[8];
					Vec4V a,b,c,d;
					
					const ScalarV camDist = Mag(sphere.GetCenter() - VECTOR3_TO_VEC3V(camInterface::GetPos()));
					const ScalarV fadeDist = Max(ScalarV(V_FIFTEEN),sphere.GetRadius());
					const ScalarV intensity = Clamp((fadeDist - camDist)/ScalarV(V_TWO),ScalarV(V_ZERO),ScalarV(V_ONE));
					
					if(IsGreaterThanAll(intensity,ScalarV(V_ZERO)))
					{
						points[0] = corner0 - backOffset;
						points[1] = corner1 - backOffset; 
						points[2] = corner2 - backOffset;
						points[3] = corner3 - backOffset;
						points[4] = corner0 + frontOffset;
						points[5] = corner1 + frontOffset; 
						points[6] = corner2 + frontOffset;
						points[7] = corner3 + frontOffset;
						

						a = Vec4V(normal,intensity);
						b = Vec4V(sphere.GetCenter(),actFalloff);
						c = Vec4V(unit1,Mag(line1) * ScalarV(V_HALF));
						d = Vec4V(unit2,Mag(line2) * ScalarV(V_HALF));
					}
					else
					{
						points[0] = Vec3V(V_ZERO);
						points[1] = Vec3V(V_ZERO);
						points[2] = Vec3V(V_ZERO);
						points[3] = Vec3V(V_ZERO);
						points[4] = Vec3V(V_ZERO);
						points[5] = Vec3V(V_ZERO);
						points[6] = Vec3V(V_ZERO);
						points[7] = Vec3V(V_ZERO);
						a = Vec4V(V_ZERO);
						b = Vec4V(V_ZERO);
						c = Vec4V(V_ZERO);
						d = Vec4V(V_ZERO);
					}
					
					PushBox(points,RCC_VECTOR4(a), RCC_VECTOR4(b), RCC_VECTOR4(c), RCC_VECTOR4(d));
					portalsPushed++;
#if __BANK
					if( g_drawAmbientLightsDebug )
					{
						PushBoxDebug(points);
					}
#endif // __BANK
					
					if( portalsPushed == Min(portalsPerBatch, portalsToDraw) )
					{
						g_ALVtxBuffer.UnLock();
						g_ALVtxBuffer.End();
						portalsToDraw -= portalsPushed;
						portalsPushed = 0;
					}
					
				}
			}
			
			ShaderUtil::EndShader(DeferredLighting::m_deferredShaders[MM_DEFAULT][DEFERRED_SHADER_AMBIENT]);	
		}

		grcStateBlock::SetRasterizerState(currentRS);
		grcStateBlock::SetDepthStencilState(currentDSS);
		grcStateBlock::SetBlendState(currentBS);
	}
}
