// =========================================
// debug/DebugDraw/DebugHangingCableTest.cpp
// (c) 2011 RockstarNorth
// =========================================

#if __DEV

#include "debug/DebugDraw/DebugHangingCableTest.h"

#if HANGING_CABLE_TEST

#define HANGING_CABLE_SHADER (1 && __DEV)

#include "grcore/debugdraw.h"
#include "grcore/image.h"
#include "grcore/stateblock.h"
#include "grcore/texture.h"
#include "grmodel/shader.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "file/asset.h"
#include "system/param.h"
#include "system/nelem.h"
#include "vectormath/classes.h"

#include "fwmaths/vectorutil.h"
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"

#include "camera/viewports/ViewportManager.h"
#include "shaders/ShaderLib.h"

PARAM(hangingcabletest, "hangingcabletest");

#if HANGING_CABLE_SHADER

class CHangingCableVertex
{
public:
	CHangingCableVertex() {}
	CHangingCableVertex(Vec3V_In position, Vec3V_In tangent, Vec2V_In texcoord, Color32 c) : m_position(position), m_tangent(tangent), m_texcoord(texcoord), m_c(c) {}

	Vec3V   m_position;
	Vec3V   m_tangent;
	Vec2V   m_texcoord; // x=r, y=d
	Color32 m_c; // rg=phase, b=texcoord.x, a=unused
};

#endif // HANGING_CABLE_SHADER

class CHangingCable
{
public:
	CHangingCable() {}
	CHangingCable(Vec3V_In p0, Vec3V_In p1, float radius = 0.05f, float theta = 0.0f, float phi = 0.0f, bool bApplySlopeTest = false) : m_p0(p0), m_p1(p1), m_radius(radius), m_windPhaseTheta(theta), m_windPhasePhi(phi), m_applySlopeTest(bApplySlopeTest), m_needsUpdate(true) {}

#if HANGING_CABLE_SHADER
	void Update(int numSegs, float slack, float windTime, bool bApplyWindOnCPU);
	void Render() const;
#endif // HANGING_CABLE_SHADER

	Vec3V   m_p0;
	Vec3V   m_p1;
	ScalarV m_radius;
	float   m_windPhaseTheta;
	float   m_windPhasePhi;
	bool    m_applySlopeTest;
	bool    m_needsUpdate;

#if HANGING_CABLE_SHADER
	atArray<CHangingCableVertex> m_vertices; // can only be written in the render thread .. not buffered
#endif // HANGING_CABLE_SHADER
};

static void UpdateCables(); // called when certain widgets are changed

class CHangingCableTest
{
public:
	CHangingCableTest()
	{
		m_cableEnabled                = false;
#if HANGING_CABLE_SHADER
		m_cableShaderEnabled          = false;
		m_cableColour                 = Color32(0,0,0,255);
		m_cableFadeExponentBias       = 0.0f;
		m_cableThickness              = 1.0f;
		m_pixelThickness              = 1.0f;
		m_pixelExpansion              = 0.0f;
		m_textureIndex                = 1;
		m_showNormals                 = false;
#endif // HANGING_CABLE_SHADER
		m_cableCount                  = 0;
		m_cableP0                     = Vec3V(85.333f, 108.341f, 17.000f); // hanging cable between garages in cptestbed
		m_cableP1                     = Vec3V(85.334f, 101.393f, 14.001f);
		m_cableSlopeTest              = 0.0f;
		m_cableSlack                  = 0.1f;
		m_cableLength                 = 0.0f;
		m_cableLengthFixed            = false;
		m_cableWindTheta              = 0.0f;
		m_cableWindPhi                = 0.0f;
		m_cableWindMotionThetaEnabled = false;
		m_cableWindMotionThetaAmp     = 0.236f;
		m_cableWindMotionThetaFreq    = 0.700f;
		m_cableWindMotionPhiAmp       = 0.361f;
		m_cableWindMotionPhiFreq      = 0.480f;
		m_cableWindMotionPhaseScale   = 1.0f;
		m_cableWindMotionComputeOnGPU = true;
		m_cableWindMotionUseSkew      = true;
		m_cableWindMotionSkewAdjust   = 1.0f;
		m_cableNumSegs                = 64;
		m_cableNumIterations1         = 25;
		m_cableNumIterations2         = 25;
		m_cableThreadOpacity          = 255;
		m_cableThreadLength           = 0.0f;
#if HANGING_CABLE_SHADER
		m_shader                      = NULL;
		m_shaderTechnique             = grcetNONE;
		m_shaderParamsID              = grcevNONE;
		m_shaderTextureID             = grcevNONE;
		m_textures[0]                 = NULL;
		m_textures[1]                 = NULL;
		m_textures[2]                 = NULL;
#endif // HANGING_CABLE_SHADER
	}

	void Init();

	void InitWidgets(bkBank& bk)
	{
		const float radius = 0.05f;

		m_cables.PushAndGrow(CHangingCable(Vec3V(-13.027f, -41.300f, 35.196f), Vec3V(-29.931f, -41.300f, 35.196f + 0.001f), radius*2.0f, 0.0f, 0.0f));
		m_cables.PushAndGrow(CHangingCable(Vec3V(-13.453f, -42.504f, 35.196f), Vec3V(-29.489f, -42.504f, 35.196f + 0.001f), radius*2.0f, 0.2f, 0.3f));
		m_cables.PushAndGrow(CHangingCable(Vec3V(-13.027f, -43.687f, 35.196f), Vec3V(-29.931f, -43.687f, 35.196f + 0.001f), radius*2.0f, 0.1f, 0.4f));

		m_cables.PushAndGrow(CHangingCable(Vec3V(-11.663f, -42.504f, 33.888f), Vec3V(-31.268f, -42.504f, 33.888f + 0.001f), radius*1.0f, 0.0f, 0.1f, true));
		m_cables.PushAndGrow(CHangingCable(Vec3V(-11.663f, -42.504f, 32.777f), Vec3V(-31.268f, -42.504f, 32.777f + 0.001f), radius*1.0f, 0.3f, 0.2f, true));
		m_cables.PushAndGrow(CHangingCable(Vec3V(-11.663f, -42.504f, 31.666f), Vec3V(-31.268f, -42.504f, 31.666f + 0.001f), radius*1.0f, 0.4f, 0.2f, true));

		m_cables.PushAndGrow(CHangingCable(Vec3V(-12.922f, -41.346f, 35.106f), Vec3V(-10.164f, -41.346f, 35.106f + 0.001f), radius*0.5f, 0.3f, 0.3f));
		m_cables.PushAndGrow(CHangingCable(Vec3V(-12.922f, -43.672f, 35.106f), Vec3V(-10.164f, -43.672f, 35.106f + 0.001f), radius*0.5f, 0.4f, 0.1f));
		m_cables.PushAndGrow(CHangingCable(Vec3V(-30.035f, -41.346f, 35.106f), Vec3V(-32.725f, -41.346f, 35.106f + 0.001f), radius*0.5f, 0.1f, 0.2f));
		m_cables.PushAndGrow(CHangingCable(Vec3V(-30.035f, -43.672f, 35.106f), Vec3V(-32.725f, -43.672f, 35.106f + 0.001f), radius*0.5f, 0.2f, 0.2f));

		m_cableCount = m_cables.size();

		bk.AddToggle("m_cableEnabled"               , &m_cableEnabled);
#if HANGING_CABLE_SHADER
		bk.AddToggle("m_cableShaderEnabled"         , &m_cableShaderEnabled);
		bk.AddColor ("m_cableColour"                , &m_cableColour);
		bk.AddSlider("m_cableFadeExponentBias"      , &m_cableFadeExponentBias, -4.0f, 4.0f, 1.0f/32.0f);
		bk.AddSlider("m_cableThickness"             , &m_cableThickness, 1.0f/128.0f, 8.0f, 1.0f/128.0f);
		bk.AddSlider("m_pixelThickness"             , &m_pixelThickness, 1.0f/32.0f, 8.0f, 1.0f/128.0f);
		bk.AddSlider("m_pixelExpansion"             , &m_pixelExpansion, 0.0f, 3.0f, 1.0f/128.0f);
		bk.AddSlider("m_textureIndex"               , &m_textureIndex, 0, NELEM(m_textures) - 1, 1);
		bk.AddToggle("m_showNormals"                , &m_showNormals);
#endif // HANGING_CABLE_SHADER
		bk.AddSeparator();
		bk.AddSlider("m_cableCount"                 , &m_cableCount, 0, m_cables.size(), 1);
		bk.AddVector("m_cableP0"                    , &m_cableP0, -200.0f, 200.0f, 1.0f/32.0f);
		bk.AddVector("m_cableP1"                    , &m_cableP1, -200.0f, 200.0f, 1.0f/32.0f);
		bk.AddSlider("m_cableSlopeTest"             , &m_cableSlopeTest, 0.0f, 20.0f, 1.0f/32.0f, UpdateCables);
		bk.AddSlider("m_cableSlack"                 , &m_cableSlack, 0.0f, 4.0f, 1.0f/256.0f, UpdateCables);
		bk.AddSlider("m_cableLength"                , &m_cableLength, 0.0f, 50.0f, 1.0f/32.0f);
		bk.AddToggle("m_cableLengthFixed"           , &m_cableLengthFixed);
		bk.AddToggle("m_cableEqualLengthSegs"       , &m_cableEqualLengthSegs, UpdateCables);
		bk.AddAngle ("m_cableWindTheta"             , &m_cableWindTheta, bkAngleType::RADIANS, -60.0f, 60.0f);
		bk.AddAngle ("m_cableWindPhi"               , &m_cableWindPhi, bkAngleType::RADIANS, -60.0f, 60.0f);
		bk.AddToggle("m_cableWindMotionThetaEnabled", &m_cableWindMotionThetaEnabled);
		bk.AddSlider("m_cableWindMotionThetaAmp"    , &m_cableWindMotionThetaAmp, 0.0f, PI/2.0f, 1.0f/256.0f);
		bk.AddSlider("m_cableWindMotionThetaFreq"   , &m_cableWindMotionThetaFreq, 0.0f, 10.0f, 1.0f/256.0f);
		bk.AddToggle("m_cableWindMotionPhiEnabled"  , &m_cableWindMotionPhiEnabled);
		bk.AddSlider("m_cableWindMotionPhiAmp"      , &m_cableWindMotionPhiAmp, 0.0f, PI/2.0f, 1.0f/256.0f);
		bk.AddSlider("m_cableWindMotionPhiFreq"     , &m_cableWindMotionPhiFreq, 0.0f, 10.0f, 1.0f/256.0f);
		bk.AddSlider("m_cableWindMotionPhaseScale"  , &m_cableWindMotionPhaseScale, 0.0f, 10.0f, 1.0f/32.0f);
		bk.AddSeparator();
		bk.AddToggle("m_cableWindMotionComputeOnGPU", &m_cableWindMotionComputeOnGPU, UpdateCables);
		bk.AddToggle("m_cableWindMotionUseSkew"     , &m_cableWindMotionUseSkew, UpdateCables);
		bk.AddSlider("m_cableWindMotionSkewAdjust"  , &m_cableWindMotionSkewAdjust, 0.0f, 1.0f, 1.0f/256.0f, UpdateCables);
		bk.AddSeparator();
		bk.AddSlider("m_cableNumSegs"               , &m_cableNumSegs, 1, 64, 1, UpdateCables);
		bk.AddSlider("m_cableNumIterations1"        , &m_cableNumIterations1, 5, 256, 1, UpdateCables);
		bk.AddSlider("m_cableNumIterations2"        , &m_cableNumIterations2, 5, 256, 1, UpdateCables);
		bk.AddSlider("m_cableThreadOpacity"         , &m_cableThreadOpacity, 0, 255, 1);
		bk.AddSlider("m_cableThreadLength"          , &m_cableThreadLength, 0.0f, 1.0f, 1.0f/256.0f);
	}

	void SimulateHangingCable(Vec3V* points, const CHangingCable& cable, float slack, int numSegs, float windTime, bool bApplyWindOnCPU, bool bDebugDraw) const;
	void Update();
	void Render();

	bool                   m_cableEnabled;
#if HANGING_CABLE_SHADER
	bool                   m_cableShaderEnabled;
	Color32                m_cableColour;
	float                  m_cableFadeExponentBias;
	float                  m_cableThickness;
	float                  m_pixelThickness;
	float                  m_pixelExpansion;
	int                    m_textureIndex;
	bool                   m_showNormals;
#endif // HANGING_CABLE_SHADER
	int                    m_cableCount;
	atArray<CHangingCable> m_cables;
	Vec3V                  m_cableP0;
	Vec3V                  m_cableP1;
	float                  m_cableSlopeTest; // z offset for 2nd endpoints of a specific set of cables .. for testing slope
	float                  m_cableSlack;
	float                  m_cableLength;
	bool                   m_cableLengthFixed;
	bool                   m_cableEqualLengthSegs;
	float                  m_cableWindTheta;
	float                  m_cableWindPhi;
	bool                   m_cableWindMotionThetaEnabled;
	float                  m_cableWindMotionThetaAmp;
	float                  m_cableWindMotionThetaFreq;
	bool                   m_cableWindMotionPhiEnabled;
	float                  m_cableWindMotionPhiAmp;
	float                  m_cableWindMotionPhiFreq;
	float                  m_cableWindMotionPhaseScale;
	bool                   m_cableWindMotionComputeOnGPU; // compute micromovements on GPU
	bool                   m_cableWindMotionUseSkew; // use cheaper skew instead of true rotation
	float                  m_cableWindMotionSkewAdjust;
	int                    m_cableNumSegs;
	int                    m_cableNumIterations1;
	int                    m_cableNumIterations2;
	int                    m_cableThreadOpacity;
	float                  m_cableThreadLength;
#if HANGING_CABLE_SHADER
	grmShader*             m_shader;
	grcEffectTechnique     m_shaderTechnique;
	grcEffectVar           m_shaderParamsID;
	grcEffectVar           m_shaderTextureID;
	grcTexture*            m_textures[3];
#endif // HANGING_CABLE_SHADER
};

CHangingCableTest gHangingCableTest;

__COMMENT(static) void UpdateCables()
{
	for (int i = 0; i < gHangingCableTest.m_cables.size(); i++)
	{
		gHangingCableTest.m_cables[i].m_needsUpdate = true; // will be updated in render thread
	}
}

#if !RSG_PS3 && !RSG_ORBIS
// http://en.wikipedia.org/wiki/Hyperbolic_function
static __forceinline float asinhf(float x)
{
	return logf(x + sqrtf(1.0f + x*x));
}
#endif // !__PS3

static float SolveCatenary(float x0, float dx, float m, float b, int numIter) // returns x
{
	while (coshf(x0 + dx) < m*(x0 + dx) + b)
	{
		dx *= 2.0f;
	}

	float x1 = x0 + dx;

	for (int iter = 0; iter < numIter; iter++)
	{
		const float x2 = (x1 + x0)/2.0f;
		const float y2 = coshf(x2);

		if (y2 < m*x2 + b)
		{
			x0 = x2;
		}
		else
		{
			x1 = x2;
		}
	}

	return (x1 + x0)/2.0f;
}

static float SolveSlack(float& x0, float& x1, float xmid, float m, float b, int numIter) // returns slack
{
	x0 = SolveCatenary(xmid, -1.0f, m, b, numIter);
	x1 = SolveCatenary(xmid, +1.0f, m, b, numIter);

	const float dx   = x1 - x0;
	const float dy   = coshf(x1) - coshf(x0);
	const float arc  = sinhf(x1) - sinhf(x0); // arc length along cosh(x) between [x0,x1]
	const float dist = sqrtf(dx*dx + dy*dy);

	return arc/dist - 1.0f;
}

static float SolveHangingCable(float& x0, float& x1, float m, float slack, int numIter, int numIter2) // returns b
{
	const float xmid = asinhf(m);

	float b0 = sqrtf(1.0f + m*m) - m*xmid; // m*x + b0 is tangent to cosh(x) at x = xmid
	float db = 1.0f;

	if (slack <= 0.0f)
	{
		x0 = xmid;
		x1 = xmid;

		return b0;
	}

	while (SolveSlack(x0, x1, xmid, m, b0 + db, numIter) < slack)
	{
		db *= 2.0f;
	}

	float b1 = b0 + db;

	for (int iter = 0; iter < numIter2; iter++)
	{
		const float b2 = (b1 + b0)/2.0f;

		if (SolveSlack(x0, x1, xmid, m, b2, numIter) < slack)
		{
			b0 = b2;
		}
		else
		{
			b1 = b2;
		}
	}

	return (b1 + b0)/2.0f;
}

#if HANGING_CABLE_SHADER

void CHangingCable::Update(int numSegs, float slack, float windTime, bool bApplyWindOnCPU)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const Vec3V slopeTest = m_applySlopeTest ? Vec3V(0.0f, 0.0f, -gHangingCableTest.m_cableSlopeTest) : Vec3V(V_ZERO);
	Vec3V points[64 + 1];

	gHangingCableTest.SimulateHangingCable(points, *this, slack, numSegs, windTime, bApplyWindOnCPU, false);

	if (m_vertices.size() > numSegs + 1)
	{
		m_vertices.Resize(numSegs*2 + 2);
	}
	else while (m_vertices.size() < numSegs*2 + 2)
	{
		m_vertices.PushAndGrow(CHangingCableVertex());
	}

	const Vec3V   line = Normalize(m_p1 + slopeTest - m_p0);
	const Color32 data = Color32((int)(m_windPhaseTheta*255.0f + 0.5f), (int)(m_windPhasePhi*255.0f + 0.5f), 0, 0);
	
	for (int i = 0; i <= numSegs; i++)
	{
		const Vec3V   position = points[i];
		const Vec3V   tangent  = Normalize(points[Min<int>(i + 1, numSegs)] - points[Max<int>(i - 1, 0)]);
		const Vec3V   v        = position - m_p0;
		const ScalarV v_dot_l  = Dot(v, line);
		const ScalarV distance = Sqrt(MagSquared(v) - v_dot_l*v_dot_l);
		const float   tx       = (float)i/(float)numSegs;

		Color32 c = data;

		c.SetBlue((int)(tx*255.0f + 0.5f));

		m_vertices[i*2 + 0] = CHangingCableVertex(position, tangent, Vec2V(+m_radius, distance), c);
		m_vertices[i*2 + 1] = CHangingCableVertex(position, tangent, Vec2V(-m_radius, distance), c);
	}

	m_needsUpdate = false;
}

void CHangingCable::Render() const
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	if (m_vertices.size() > 0)
	{
		grcBegin(drawTriStrip, m_vertices.size());

		for (int i = 0; i < m_vertices.size(); i++)
		{
			const CHangingCableVertex& vertex = m_vertices[i];

			grcVertex(
				VEC3V_ARGS(vertex.m_position),
				VEC3V_ARGS(vertex.m_tangent),
				vertex.m_c,
				VEC2V_ARGS(vertex.m_texcoord)
			);
		}

		grcEnd();
	}
}

#endif // HANGING_CABLE_SHADER

static void ComputeWindRotation(Mat33V_InOut rot, float theta, float phi) // order-independent biaxial rotation
{
	if (theta != 0.0f || phi != 0.0f)
	{
		// this can probably be optimised ..
		const Vec3V v     = Normalize(Vec3V(sinf(theta), sinf(phi), 1.0f));
		const Vec3V axis  = Normalize(CrossZAxis(v));
		const float angle = -acosf(v.GetZf()); // = -1/sqrt(sin(theta)^2 + sin(phi)^2))

		Mat33VFromAxisAngle(rot, axis, ScalarV(angle));
	}
}

void CHangingCableTest::Init()
{
	static bool bOnce = true;

	if (bOnce)
	{
		bOnce = false;

#if HANGING_CABLE_SHADER
		const char* shaderName = "debug_cable";

		ASSET.PushFolder("common:/shaders");
		m_shader = grmShaderFactory::GetInstance().Create();
		Assert(m_shader);

		if (m_shader && m_shader->Load(shaderName, NULL, false))
		{
			m_shaderTechnique = m_shader->LookupTechnique("dbg_cable");
			m_shaderParamsID  = m_shader->LookupVar("params");
			m_shaderTextureID = m_shader->LookupVar("texture");
		}
		else
		{
			Assertf(false, "Failed to load \"%s\", try rebuilding the shader or consider bumping c_MaxEffects in effect.h!", shaderName);
			m_shader = NULL;
		}

		ASSET.PopFolder();

		if (1) // create texture
		{
			const int w = 1;
			const int h = 16;

			int numMips = 0;

			while ((Max<int>(w, h) >> numMips) >= 4) // smallest mip should be 4 pixels, i.e. [0,255,255,0]
			{
				numMips++;
			}

			grcImage* image = grcImage::Create(
				w,
				h,
				1, // depth
				grcImage::L8,
				grcImage::STANDARD,
				numMips - 1, // extraMipmaps
				0 // extraLayers
			);
			Assert(image);

			for (int index = 0; index < NELEM(m_textures); index++)
			{
				grcImage* mip = image;

				while (mip)
				{
					const int w0 = mip->GetWidth();
					const int h0 = mip->GetHeight();

					u8* data = mip->GetBits();

					for (int i = 0, y = 0; y < h0; y++)
					{
						u8 a = 0xff;

						switch (index)
						{
						case 0: a = 0xff; break;
						case 1: a = (y <= 0 || y >= h0 - 1) ? 0x00 : 0xff; break; // 2-pixel thick in last mip
						case 2: a = (y <= 1 || y >= h0 - 1) ? 0x00 : 0xff; break; // 1-pixel thick in last mip
						}

						for (int x = 0; x < w0; x++, i++)
						{
							data[i] = a;
						}
					}

					mip = mip->GetNext();
				}

				m_textures[index] = grcTextureFactory::GetInstance().Create(image);
				Assert(m_textures[index]);
			}

			image->Release();
		}
#endif // HANGING_CABLE_SHADER
	}
}

void CHangingCableTest::SimulateHangingCable(Vec3V* points, const CHangingCable& cable, float slack, int numSegs, float windTime, bool bApplyWindOnCPU, bool bDebugDraw) const
{
	const Vec3V slopeTest = cable.m_applySlopeTest ? Vec3V(0.0f, 0.0f, -m_cableSlopeTest) : Vec3V(V_ZERO);

	points[0]       = cable.m_p0;
	points[numSegs] = cable.m_p1 + slopeTest;

	if (slack > 0.0f)
	{
		if (bApplyWindOnCPU)
		{
			float  windTheta = m_cableWindTheta;
			float  windPhi   = m_cableWindPhi;
			Mat33V windRot   = Mat33V(V_IDENTITY);

			if (m_cableWindMotionThetaEnabled)
			{
				const float t = windTime*m_cableWindMotionThetaFreq + cable.m_windPhaseTheta*m_cableWindMotionPhaseScale;

				windTheta += sinf(t*2.0f*PI)*m_cableWindMotionThetaAmp;
			}

			if (m_cableWindMotionPhiEnabled)
			{
				const float t = windTime*m_cableWindMotionPhiFreq + cable.m_windPhasePhi*m_cableWindMotionPhaseScale;

				windPhi += sinf(t*2.0f*PI)*m_cableWindMotionPhiAmp;
			}

			if (!m_cableWindMotionUseSkew)
			{
				ComputeWindRotation(windRot, windTheta, windPhi);
			}

			const Vec3V p1 = Multiply(windRot, cable.m_p1 + slopeTest - cable.m_p0) + cable.m_p0;

			const Vec3V v = p1 - cable.m_p0;
			const float m = v.GetZf()/Mag(v.GetXY()).Getf();

			float x0;
			float x1;
			SolveHangingCable(x0, x1, m, slack, m_cableNumIterations1, m_cableNumIterations2);

			const float y0 = coshf(x0);
			const float y1 = coshf(x1);

			for (int i = 1; i < numSegs; i++)
			{
				const float t = (float)i/(float)numSegs;
				const float x = m_cableEqualLengthSegs ? asinhf(sinhf(x0) + t*(sinhf(x1) - sinhf(x0))) : (x0 + t*(x1 - x0));
				const float y = coshf(x);

				const Vec2V   pxy = (cable.m_p0 + (p1 - cable.m_p0)*ScalarV((x - x0)/(x1 - x0))).GetXY();
				const ScalarV pz  = (cable.m_p0 + (p1 - cable.m_p0)*ScalarV((y - y0)/(y1 - y0))).GetZ();
				const Vec3V   p0  = Vec3V(pxy, pz);

				if (!m_cableWindMotionUseSkew)
				{
					points[i] = Multiply(p0 - cable.m_p0, windRot) + cable.m_p0;
				}
				else
				{
					const Vec3V   v0 = p0 - cable.m_p0;
					const Vec3V   v1 = p1 - cable.m_p0;
					const ScalarV k  = Dot(v0, v1);
					const ScalarV d  = Sqrt(MagSquared(v0) - k*k/MagSquared(v1)); // distance to line (PRECALCULATE)

					const Vec3V windSkew = Vec3V(sinf(windTheta), sinf(windPhi), 0.0f);

					points[i] = p0 + windSkew*d;

					if (m_cableWindMotionSkewAdjust != 0.0f)
					{
						points[i] += Vec3V(V_Z_AXIS_WZERO)*MagSquared(windSkew)*d*ScalarV(m_cableWindMotionSkewAdjust);
					}
				}
			}
		}
		else // no wind
		{
			const Vec3V v = cable.m_p1 + slopeTest - cable.m_p0;
			const float m = v.GetZf()/Mag(v.GetXY()).Getf();

			float x0;
			float x1;
			SolveHangingCable(x0, x1, m, slack, m_cableNumIterations1, m_cableNumIterations2);

			const float y0 = coshf(x0);
			const float y1 = coshf(x1);

			for (int i = 1; i < numSegs; i++)
			{
				const float t = (float)i/(float)numSegs;
				const float x = m_cableEqualLengthSegs ? asinhf(sinhf(x0) + t*(sinhf(x1) - sinhf(x0))) : (x0 + t*(x1 - x0));
				const float y = coshf(x);

				const Vec2V   pxy = (cable.m_p0 + (cable.m_p1 + slopeTest - cable.m_p0)*ScalarV((x - x0)/(x1 - x0))).GetXY();
				const ScalarV pz  = (cable.m_p0 + (cable.m_p1 + slopeTest - cable.m_p0)*ScalarV((y - y0)/(y1 - y0))).GetZ();
				const Vec3V   p0  = Vec3V(pxy, pz);

				points[i] = p0;
			}
		}
	}
	else // straight cable
	{
		for (int i = 1; i < numSegs; i++)
		{
			const float t = (float)i/(float)numSegs;

			points[i] = cable.m_p0 + (cable.m_p1 + slopeTest - cable.m_p0)*ScalarV(t);
		}
	}

	if (bDebugDraw)
	{
		for (int i = 0; i < numSegs; i++)
		{
			grcDebugDraw::Line(points[i], points[i + 1], Color32(255,0,0,255));
		}

		if (m_cableThreadOpacity > 0 && m_cableThreadLength > 0.0f)
		{
			const Vec3V   dv = Vec3V(V_Z_AXIS_WZERO)*ScalarV(m_cableThreadLength);
			const Color32 dc = Color32(0,0,255,255).MergeAlpha((u8)m_cableThreadOpacity);

			for (int i = 1; i < numSegs; i++)
			{
				grcDebugDraw::Line(points[i], points[i] - dv, dc);
			}
		}
	}
}

void CHangingCableTest::Update()
{
#if HANGING_CABLE_SHADER
	if (m_cableShaderEnabled)
	{
		return; // don't simulate and render here, we'll do it from the render thread
	}
#endif // HANGING_CABLE_SHADER

	if (m_cableEnabled)
	{
		Vec3V points[64 + 1];
		const float windTime = (float)fwTimer::GetSystemTimeInMilliseconds()/1000.0f;

		for (int j = 0; j < m_cableCount; j++)
		{
			SimulateHangingCable(points, m_cables[j], m_cableSlack, m_cableNumSegs, windTime, true, true);
		}

		if (m_cableLengthFixed && m_cableLength != 0.0f) // compute slack from length
		{
			m_cableSlack = Max<float>(0.0f, m_cableLength/Mag(m_cableP1 - m_cableP0).Getf() - 1.0f);
		}

		if (m_cableSlack > 0.0f)
		{
			SimulateHangingCable(points, CHangingCable(m_cableP0, m_cableP1), m_cableSlack, m_cableNumSegs, windTime, true, true);

			if (!m_cableLengthFixed) // update cable length
			{
				m_cableLength = 0.0f;

				for (int i = 0; i < m_cableNumSegs; i++)
				{
					m_cableLength += Mag(points[i + 1] - points[i]).Getf();
				}
			}
		}
		else
		{
			grcDebugDraw::Line(m_cableP0, m_cableP1, Color32(255,0,0,255));

			if (!m_cableLengthFixed) // update cable length
			{
				m_cableLength = Mag(m_cableP1 - m_cableP0).Getf();
			}
		}

		grcDebugDraw::Sphere(m_cableP0, 0.08f, Color32(0,0,255,255));
		grcDebugDraw::Sphere(m_cableP1, 0.08f, Color32(0,0,255,255));
	}
}

void CHangingCableTest::Render()
{
#if HANGING_CABLE_SHADER
	if (m_cableShaderEnabled)
	{
		Init();

		if (m_shader)
		{
			const grcViewport* vp = gVpMan.GetRenderGameGrcViewport();

			const float windTime = (float)fwTimer::GetSystemTimeInMilliseconds()/1000.0f;
			const Vec4f params[] =
			{
				Vec4f(
					(float)GRCDEVICE.GetHeight()/(vp->GetTanVFOV()*m_pixelThickness),
					m_cableThickness,
					m_pixelExpansion,
					powf(2.0f, m_cableFadeExponentBias)
				),
				Vec4f(
					m_cableColour.GetRedf  (),
					m_cableColour.GetGreenf(),
					m_cableColour.GetBluef (),
					m_cableColour.GetAlphaf()
				),
				Vec4f(
					m_cableWindMotionThetaFreq*windTime,
					m_cableWindMotionPhiFreq  *windTime,
					m_cableWindMotionThetaAmp *(m_cableWindMotionThetaEnabled ? 1.0f : 0.0f),
					m_cableWindMotionPhiAmp   *(m_cableWindMotionPhiEnabled   ? 1.0f : 0.0f)
				),
				Vec4f(
					m_cableWindTheta,
					m_cableWindPhi,
					m_cableWindMotionPhaseScale,
					m_cableWindMotionSkewAdjust
				),
			};

			const bool bApplyWind      = m_cableWindMotionThetaEnabled || m_cableWindMotionPhiEnabled || m_cableWindTheta != 0.0f || m_cableWindPhi != 0.0f;
			const bool bApplyWindOnCPU = bApplyWind && !(m_cableWindMotionComputeOnGPU && m_cableWindMotionUseSkew);
			const bool bApplyWindOnGPU = bApplyWind && !bApplyWindOnCPU;

			m_shader->SetVar(m_shaderParamsID, params, NELEM(params));
			m_shader->SetVar(m_shaderTextureID, m_textures[m_textureIndex]);

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, CShaderLib::DSS_TestOnly_LessEqual_Invert, grcStateBlock::BS_Normal);

			const int pass = (bApplyWindOnGPU ? 1 : 0) + (m_showNormals ? 2 : 0);

			AssertVerify(m_shader->BeginDraw(grmShader::RMC_DRAW, false, m_shaderTechnique));
			m_shader->Bind(pass);

			for (int j = 0; j < m_cableCount; j++)
			{
				if (m_cables[j].m_needsUpdate || bApplyWindOnCPU)
				{
					m_cables[j].Update(m_cableNumSegs, m_cableSlack, windTime, bApplyWindOnCPU);
				}

				m_cables[j].Render();
			}

			m_shader->UnBind();
			m_shader->EndDraw();
		}
	}
#endif // HANGING_CABLE_SHADER
}

// ================================================================================================

__COMMENT(static) void CHangingCableTestInterface::InitWidgets() { if (PARAM_hangingcabletest.Get()) { gHangingCableTest.InitWidgets(BANKMGR.CreateBank("Hanging Cable Test")); } }
__COMMENT(static) void CHangingCableTestInterface::Update     () { if (PARAM_hangingcabletest.Get()) { gHangingCableTest.Update(); } }
__COMMENT(static) void CHangingCableTestInterface::Render     () { if (PARAM_hangingcabletest.Get()) { gHangingCableTest.Render(); } }

#endif // HANGING_CABLE_TEST
#endif // __DEV
