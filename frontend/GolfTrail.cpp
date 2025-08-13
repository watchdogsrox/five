// ======================
// frontend/GolfTrail.cpp
// (c) 2011 RockstarNorth
// ======================

#include "grcore/debugdraw.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "grmodel/shader.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "file/asset.h"
#include "system/param.h"
#include "system/memops.h"
#include "system/nelem.h"
#include "vectormath/classes.h"

#include "fwdebug/picker.h"
#include "fwsys/timer.h"
#include "fwutil/xmacro.h"

#include "camera/viewports/ViewportManager.h"
#include "frontend/GolfTrail.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/rendertargets.h"
#include "scene/Entity.h"

enum
{
	GOLF_TRAIL_MAX_CONTROL_POINTS = 16,
	GOLF_TRAIL_MAX_TESSELLATION = 16,
};

class CGolfTrailVertex
{
public:
	CGolfTrailVertex() {}
	CGolfTrailVertex(Vec3V_In position, Vec3V_In tangent, Vec2V_In texcoord, Color32 colour) : m_position(position), m_tangent(tangent), m_texcoord(texcoord), m_colour(colour) {}

	Vec3V   m_position;
	Vec3V   m_tangent;
	Vec2V   m_texcoord; // x=radius, y=texcoord.x
	Color32 m_colour;
};

class CGolfTrailControlPoint
{
public:
	CGolfTrailControlPoint() {}
	CGolfTrailControlPoint(Vec3V_In position, ScalarV_In radius, Color32 colour) : m_positionAndRadius(position, radius), m_colour(colour.GetRGBA()) {}

	Vec4V m_positionAndRadius;
	Vec4V m_colour;
};

class CGolfTrailState
{
public:
	CGolfTrailState()
	{
		Reset();
	}

	void Reset()
	{
		m_enabled               = false;
		m_renderBeforePostFX    = false;

		m_trailPositionStart    = Vec3V(0.0f, 70.0f, 7.537f); // cptestbed
		m_trailVelocityStart    = Vec3V(0.0f, 1.0f, 1.0f);
		m_trailVelocityScale    = 30.0f;
		m_trailZ1               = 7.537f;
		m_trailAscending        = false;

		m_trailRadiusStart      = 0.02f;
		m_trailRadiusMiddle     = 0.5f;
		m_trailRadiusEnd        = 0.1f;
		m_trailColourStart      = Color32(0,0,255,64);
		m_trailColourMiddle     = Color32(0,0,255,255);
		m_trailColourEnd        = Color32(0,128,255,128);
		m_trailNumControlPoints = 8;
		m_trailTessellation     = 16;

		m_fixedControlPointsEnabled = false;
		sysMemSet(m_fixedControlPoints, 0, sizeof(m_fixedControlPoints));

		m_pixelThickness        = 2.0f;
		m_pixelExpansion        = 0.0f;
		m_fadeOpacity           = 1.0f;
		m_fadeExponentBias      = 0.0f;
		m_textureFill           = 0.5f;

		m_facing                = false;
	}

	bool    m_enabled;
	bool    m_renderBeforePostFX; // this could be bank-only (default=false)

	// trail path
	Vec3V   m_trailPositionStart;
	Vec3V   m_trailVelocityStart;
	float   m_trailVelocityScale;
	float   m_trailZ1; // world z at impact
	bool    m_trailAscending; // is ascending at impact?

	// trail params
	float   m_trailRadiusStart;
	float   m_trailRadiusMiddle;
	float   m_trailRadiusEnd;
	Color32 m_trailColourStart;
	Color32 m_trailColourMiddle;
	Color32 m_trailColourEnd;
	int     m_trailNumControlPoints;
	int     m_trailTessellation;

	// fixed control points
	bool                   m_fixedControlPointsEnabled;
	CGolfTrailControlPoint m_fixedControlPoints[GOLF_TRAIL_MAX_CONTROL_POINTS];

	// shader params
	float   m_pixelThickness;
	float   m_pixelExpansion;
	float   m_fadeOpacity; // global opacity
	float   m_fadeExponentBias;
	float   m_textureFill;

	bool    m_facing;
};

class CGolfTrail
{
public:
	CGolfTrail()
	{
#if __DEV
		m_overrideScript           = false;
		m_debugDraw                = false;
		m_dumpControlPoints        = false;
#endif // __DEV
		m_shader                   = NULL;
		m_shaderTechnique          = grcetNONE;
		m_shaderParamsID           = grcevNONE;
		m_shaderGolfTrailTextureID = grcevNONE;
		m_shaderDepthBufferID      = grcevNONE;
		m_textures[0]              = NULL;
	}

	void Init();
#if __DEV
	void InitWidgets(bkBank& bk);
#endif // __DEV
	void Shutdown();
	void UpdateAndRender(bool bRenderBeforePostFX);
	static void Render(const CGolfTrailState* state);

	CGolfTrailState const& GetUpdateState() const { return m_state; }
	CGolfTrailState      & GetUpdateState()       { return m_state; }

	void Script_SetTrailEnabled(bool bEnabled)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.m_enabled = bEnabled;
	}

	void Script_SetTrailPath(Vec3V_In positionStart, Vec3V_In velocityStart, float velocityScale, float z1, bool bAscending)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.m_trailPositionStart = positionStart;
		state.m_trailVelocityStart = velocityStart;
		state.m_trailVelocityScale = velocityScale;
		state.m_trailZ1            = z1;
		state.m_trailAscending     = bAscending;
	}

	void Script_SetTrailRadius(float radiusStart, float radiusMiddle, float radiusEnd)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.m_trailRadiusStart  = radiusStart;
		state.m_trailRadiusMiddle = radiusMiddle;
		state.m_trailRadiusEnd    = radiusEnd;
	}

	void Script_SetTrailColour(Color32 colourStart, Color32 colourMiddle, Color32 colourEnd)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.m_trailColourStart  = colourStart;
		state.m_trailColourMiddle = colourMiddle;
		state.m_trailColourEnd    = colourEnd;
	}

	void Script_SetTrailTessellation(int numControlPoints, int tessellation)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		if (!AssertVerify(numControlPoints >= 3 && numControlPoints <= GOLF_TRAIL_MAX_CONTROL_POINTS))
		{
			numControlPoints = Clamp<int>(numControlPoints, 3, GOLF_TRAIL_MAX_CONTROL_POINTS);
		}
		
		if (!AssertVerify(tessellation >= 1 && tessellation <= GOLF_TRAIL_MAX_TESSELLATION))
		{
			tessellation = Clamp<int>(tessellation, 1, GOLF_TRAIL_MAX_TESSELLATION);
		}

		Assertf(!state.m_fixedControlPointsEnabled, "can't set tessellation while fixed control points are enabled, you must set tessellation first and then enable fixed control points");

		if (AssertVerify(numControlPoints <= GOLF_TRAIL_MAX_CONTROL_POINTS))
		{
			state.m_trailNumControlPoints = numControlPoints;
			state.m_trailTessellation     = tessellation;
		}
	}

	void Script_SetTrailFixedControlPointsEnabled(bool bEnable)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.m_fixedControlPointsEnabled = bEnable;

		if (bEnable)
		{
			sysMemSet(state.m_fixedControlPoints, 0, NELEM(state.m_fixedControlPoints)*sizeof(CGolfTrailControlPoint));
		}
	}

	void Script_SetTrailFixedControlPoint(int controlPointIndex, Vec3V_In position, float radius, Color32 colour)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		Assert(state.m_fixedControlPointsEnabled);

		if (AssertVerify(controlPointIndex >= 0 && controlPointIndex < Min<int>(state.m_trailNumControlPoints, NELEM(state.m_fixedControlPoints))))
		{
			state.m_fixedControlPoints[controlPointIndex].m_positionAndRadius = Vec4V(position, ScalarV(radius));
			state.m_fixedControlPoints[controlPointIndex].m_colour = colour.GetRGBA();
		}
	}

	void Script_SetTrailShaderParams(float pixelThickness, float pixelExpansion, float fadeOpacity, float fadeExponentBias, float textureFill)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.m_pixelThickness   = pixelThickness;
		state.m_pixelExpansion   = pixelExpansion;
		state.m_fadeOpacity      = fadeOpacity;
		state.m_fadeExponentBias = fadeExponentBias;
		state.m_textureFill      = textureFill;
	}

	void Script_SetTrailFacing(bool bFacing)
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.m_facing = bFacing;
	}

	void Script_ResetTrail()
	{
		DEV_ONLY(if (m_overrideScript) { return; });
		CGolfTrailState& state = GetUpdateState();

		state.Reset();
	}

	float Script_GetTrailMaxHeight() const
	{
		const CGolfTrailState& state = GetUpdateState();
		float z = 0.0f;

		if (state.m_fixedControlPointsEnabled)
		{
			ScalarV height(V_NEG_FLT_MAX);

			for (int controlPointIndex = 0; controlPointIndex < state.m_trailNumControlPoints; controlPointIndex++)
			{
				height = Max(state.m_fixedControlPoints[controlPointIndex].m_positionAndRadius.GetZ(), height);
			}

			z = height.Getf();
		}
		else
		{
			const Vec3V p0  = state.m_trailPositionStart;
			const Vec3V v0  = state.m_trailVelocityStart*ScalarV(state.m_trailVelocityScale);
			const float p0z = p0.GetZf();
			const float v0z = v0.GetZf();
			const float G   = 9.81f; // gravity

			z = p0z + v0z*v0z/(4.0f*G);
		}

		return z;
	}

	Vec3V_Out Script_GetTrailVisualControlPoint(int controlPointIndex) const
	{
		const CGolfTrailState& state = GetUpdateState();
		Vec3V point(V_ZERO);

		Assert(controlPointIndex >= 0 && controlPointIndex < state.m_trailNumControlPoints);

		if (state.m_fixedControlPointsEnabled)
		{
			point = state.m_fixedControlPoints[controlPointIndex].m_positionAndRadius.GetXYZ();
#if __ASSERT
			if (!IsFiniteStable(point))
			{
				char desc[2048] = "";

				strcat(desc, atVarString("\ncontrolPointIndex=%d", controlPointIndex).c_str());
				strcat(desc, atVarString("\nm_trailNumControlPoints=%d", state.m_trailNumControlPoints).c_str());

				Assertf(0, "golf trail visual control point is invalid (%f,%f,%f)%s", VEC3V_ARGS(point), desc);
			}
#endif // __ASSERT
		}
		else
		{
			const Vec3V p0  = state.m_trailPositionStart;
			const Vec3V v0  = state.m_trailVelocityStart*ScalarV(state.m_trailVelocityScale);
			const float p0z = p0.GetZf();
			const float v0z = v0.GetZf();
			const float G   = 9.81f; // gravity
			const float q   = v0z*v0z + 4.0f*G*(p0z - state.m_trailZ1);

			if (q >= 0.0f)
			{
				const float t1 = (v0z + (state.m_trailAscending ? -1.0f : 1.0f)*sqrtf(q))/(2.0f*G);

				const float x = (float)controlPointIndex/(float)(state.m_trailNumControlPoints - 1); // [0..1/2]
				const float t = t1*x; // [0..t1/2]

				point = p0 + v0*ScalarV(t) + Vec3V(0.0f, 0.0f, -G*t*t);
#if __ASSERT
				const Vec3V p = p0 + v0*ScalarV(t) + Vec3V(0.0f, 0.0f, -G*t*t);

				if (!IsFiniteStable(point))
				{
					char desc[2048] = "";

					strcat(desc, atVarString("\ncontrolPointIndex=%d", controlPointIndex).c_str());
					strcat(desc, atVarString("\nm_trailNumControlPoints=%d", state.m_trailNumControlPoints).c_str());
					strcat(desc, atVarString("\nm_trailPositionStart=%f,%f,%f", VEC3V_ARGS(state.m_trailPositionStart)).c_str());
					strcat(desc, atVarString("\nm_trailVelocityStart=%f,%f,%f", VEC3V_ARGS(state.m_trailVelocityStart)).c_str());
					strcat(desc, atVarString("\nm_trailVelocityScale=%f", state.m_trailVelocityScale).c_str());
					strcat(desc, atVarString("\nm_trailAscending=%s", state.m_trailAscending ? "TRUE" : "FALSE").c_str());
					strcat(desc, atVarString("\nm_trailZ1=%f", state.m_trailZ1).c_str());
					strcat(desc, atVarString("\nt1=%f", t1).c_str());
					strcat(desc, atVarString("\nx=%f", x).c_str());
					strcat(desc, atVarString("\nt=%f", t).c_str());
					strcat(desc, atVarString("\np=%f,%f,%f", VEC3V_ARGS(p)).c_str());

					Assertf(0, "golf trail visual control point is invalid (%f,%f,%f)%s", VEC3V_ARGS(point), desc);
				}
#endif // __ASSERT
			}
			else
			{
#if __DEV
				grcDebugDraw::AddDebugOutput("GOLF TRAIL PATH IS INVALID (q=%f)", q);
#endif // __DEV
				point = Vec3V(V_FLT_MAX);
			}
		}

		return point;
	}

#if __DEV
	bool               m_overrideScript;
	bool               m_debugDraw;
	bool               m_dumpControlPoints;
#endif // __DEV
	grmShader*         m_shader;
	grcEffectTechnique m_shaderTechnique;
	grcEffectVar       m_shaderParamsID;
	grcEffectVar       m_shaderGolfTrailTextureID;
	grcEffectVar       m_shaderDepthBufferID;
	grcTexture*        m_textures[1];

	CGolfTrailState    m_state;
};

CGolfTrail gGolfTrail;

void CGolfTrail::Init()
{
	const char* shaderName = "golf_trail";

	ASSET.PushFolder("common:/shaders");
	m_shader = grmShaderFactory::GetInstance().Create();
	Assert(m_shader);

	if (m_shader && m_shader->Load(shaderName, NULL, false))
	{
		m_shaderTechnique          = m_shader->LookupTechnique("golf_trail");
		m_shaderParamsID           = m_shader->LookupVar("params");
		m_shaderGolfTrailTextureID = m_shader->LookupVar("GolfTrailTexture");
		m_shaderDepthBufferID      = m_shader->LookupVar("DepthBuffer");
	}
	else
	{
		Assertf(false, "Failed to load \"%s\", try rebuilding the shader or consider bumping c_MaxEffects in effect.h!", shaderName);
		m_shader = NULL;
	}

	ASSET.PopFolder();

	if (1) // create texture
	{
		const int w = __D3D11 ? 64 : 1;
		const int h = 64;

		int numMips = 0;

		while ((Max<int>(w, h) >> numMips) >= 4) // smallest mip should be 4 pixels, i.e. [0,255,255,0]
		{
			numMips++;
		}

		grcImage* image = grcImage::Create(
			w,
			h,
			1, // depth
			grcImage::A8L8,
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
					u8 a = 0;
					u8 l = 0;

					switch (Min<int>(y, h0 - y - 1))
					{
					case  0: a = 0; l = 0; break;
					case  1: a = 255; l = 255 - 32*0; break;
					case  2: a = 255; l = 255 - 32*1; break;
					case  3: a = 255; l = 255 - 32*2; break;
					case  4: a = 255; l = 255 - 32*3; break;
					case  5: a = 255; l = 255 - 32*4; break;
					default: a = 255; l = 255 - 32*5; break;
					}

					for (int x = 0; x < w0; x++)
					{
						data[i++] = l;
						data[i++] = a;
					}
				}

				mip = mip->GetNext();
			}
			BANK_ONLY(char szName[255];)
			BANK_ONLY(formatf(szName, "%s - %d", shaderName, index);)
			BANK_ONLY(grcTexture::SetCustomLoadName(szName);)
			m_textures[index] = grcTextureFactory::GetInstance().Create(image);
			BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
			Assert(m_textures[index]);
		}

		image->Release();
	}
}

#if __DEV

PARAM(golfpoints,"");

static int g_testNumControlPoints = 3;
static int g_testTessellation     = 1;

void CGolfTrail::InitWidgets(bkBank& bk)
{
	class ResetTrailState_button { public: static void func()
	{
		CGolfTrailState& state = gGolfTrail.GetUpdateState();

		state.Reset();
	}};

	class DumpControlPoints_button { public: static void func()
	{
		gGolfTrail.m_dumpControlPoints = true;
	}};

	class ConvertToFixedTrail_button { public: static void func()
	{
		CGolfTrailState& state = gGolfTrail.GetUpdateState();

		if (state.m_enabled)
		{
			Vec3V pos[GOLF_TRAIL_MAX_CONTROL_POINTS];

			for (int i = 0; i < state.m_trailNumControlPoints; i++)
			{
				pos[i] = CGolfTrailInterface::Script_GetTrailVisualControlPoint(i);
			}

			CGolfTrailInterface::Script_SetTrailFixedControlPointsEnabled(true);

			for (int i = 0; i < state.m_trailNumControlPoints; i++)
			{
				Displayf("setting control point %d to (%f,%f,%f)", i, VEC3V_ARGS(pos[i]));
				CGolfTrailInterface::Script_SetTrailFixedControlPoint(i, pos[i], 0.25f, Color32(255,0,0,255));
			}
		}
	}};

	class AttachToSelected_1_button { public: static void func()
	{
		if (gGolfTrail.GetUpdateState().m_enabled)
		{
			gGolfTrail.GetUpdateState().Reset();
			return;
		}

		const CEntity* pEntity = (CEntity*)g_PickerManager.GetSelectedEntity();

		if (pEntity && !pEntity->GetIsRetainedByInteriorProxy())
		{
			fwBaseEntityContainer* pContainer = pEntity->GetOwnerEntityContainer();

			if (pContainer)
			{
				u32 index = pContainer->GetEntityDescIndex(pEntity);
				if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
				{
					spdAABB aabb;
					pContainer->GetApproxBoundingBox(index, aabb);

					const Vec3V p0 = aabb.GetCenter() + Vec3V(V_Z_AXIS_WZERO)*aabb.GetExtent().GetZ()*ScalarV(V_HALF);
					const Vec3V p1 = p0 + Vec3V(V_Z_AXIS_WZERO)*ScalarV(V_FIVE);
					const Vec3V p2 = p1 + Vec3V(V_Z_AXIS_WZERO)*ScalarV(V_FIVE);
					const Vec3V p3 = p2 + Vec3V(V_Z_AXIS_WZERO)*ScalarV(V_FIVE);

					CGolfTrailInterface::Script_SetTrailEnabled(true);
					CGolfTrailInterface::Script_SetTrailTessellation(4, 8);
					CGolfTrailInterface::Script_SetTrailFixedControlPointsEnabled(true);
					CGolfTrailInterface::Script_SetTrailFixedControlPoint(0, p0, 0.25f, Color32(255,0,0,255));
					CGolfTrailInterface::Script_SetTrailFixedControlPoint(1, p1, 0.50f, Color32(255,255,0,192));
					CGolfTrailInterface::Script_SetTrailFixedControlPoint(2, p2, 0.50f, Color32(0,255,0,128));
					CGolfTrailInterface::Script_SetTrailFixedControlPoint(3, p3, 0.25f, Color32(0,255,255,64));
					CGolfTrailInterface::Script_SetTrailFacing(true);
				}
			}
		}
	}};

	class AttachToSelected_2_button { public: static void func()
	{
		if (gGolfTrail.GetUpdateState().m_enabled)
		{
			gGolfTrail.GetUpdateState().Reset();
			return;
		}

		const CEntity* pEntity = (CEntity*)g_PickerManager.GetSelectedEntity();

		if (pEntity && !pEntity->GetIsRetainedByInteriorProxy())
		{
			fwBaseEntityContainer* pContainer = pEntity->GetOwnerEntityContainer();

			if (pContainer)
			{
				u32 index = pContainer->GetEntityDescIndex(pEntity);
				if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
				{
					spdAABB aabb;
					pContainer->GetApproxBoundingBox(index, aabb);

					const Vec3V p0 = aabb.GetCenter();

					CGolfTrailInterface::Script_SetTrailEnabled(true);
					CGolfTrailInterface::Script_SetTrailTessellation(g_testNumControlPoints, g_testTessellation);
					CGolfTrailInterface::Script_SetTrailFixedControlPointsEnabled(true);
					CGolfTrailInterface::Script_SetTrailFixedControlPoint(0, p0, 1.0f, Color32(255,0,0,255));

					for (int i = 1; i < g_testNumControlPoints; i++)
					{
						const Vec3V p = p0 + Vec3V(V_Z_AXIS_WZERO)*ScalarV(10.0f*(float)i/(float)(g_testNumControlPoints - 1));

						CGolfTrailInterface::Script_SetTrailFixedControlPoint(i, p, 1.0f, Color32(255,255,0,255));
					}

					CGolfTrailInterface::Script_SetTrailFacing(true);
				}
			}
		}
	}};

	CGolfTrailState& state = GetUpdateState();

	bk.AddToggle("Enabled"               , &state.m_enabled);
	bk.AddToggle("Render Before Post FX" , &state.m_renderBeforePostFX);
	bk.AddToggle("Override Script"       , &m_overrideScript);
	bk.AddToggle("Debug Draw"            , &m_debugDraw);
	bk.AddButton("Dump Control Points"   , DumpControlPoints_button::func);
	bk.AddButton("Convert To Fixed Trail", ConvertToFixedTrail_button::func);
	bk.AddButton("Attach To Selected (1)", AttachToSelected_1_button::func);
	bk.AddButton("Attach To Selected (2)", AttachToSelected_2_button::func);
	bk.AddSlider("- num control points"  , &g_testNumControlPoints, 3, 8, 1);
	bk.AddSlider("- tessellation"        , &g_testTessellation, 1, 8, 1);
	bk.AddButton("Reset Trail State"     , ResetTrailState_button::func);
	bk.AddTitle ("Trail Path");
	bk.AddVector("Position Start"        , &state.m_trailPositionStart, -8000.0f, 8000.0f, 1.0f/32.0f);
	bk.AddVector("Velocity Start"        , &state.m_trailVelocityStart, -10.0f, 10.0f, 1.0f/256.0f);
	bk.AddSlider("Velocity Scale"        , &state.m_trailVelocityScale, 0.0f, 500.0f, 1.0f/32.0f);
	bk.AddSlider("Z1"                    , &state.m_trailZ1, -20.0f, 200.0f, 1.0f/256.0f);
	bk.AddToggle("Ascending"             , &state.m_trailAscending);
	bk.AddTitle ("Trail Params");
	bk.AddSlider("Radius Start"          , &state.m_trailRadiusStart, 0.0f, 64.0f, 1.0f/32.0f);
	bk.AddSlider("Radius Middle"         , &state.m_trailRadiusMiddle, 0.0f, 64.0f, 1.0f/32.0f);
	bk.AddSlider("Radius End"            , &state.m_trailRadiusEnd, 0.0f, 64.0f, 1.0f/32.0f);
	bk.AddColor ("Colour Start"          , &state.m_trailColourStart);
	bk.AddColor ("Colour Middle"         , &state.m_trailColourMiddle);
	bk.AddColor ("Colour End"            , &state.m_trailColourEnd);
	bk.AddSlider("Num Control Points"    , &state.m_trailNumControlPoints, 3, GOLF_TRAIL_MAX_CONTROL_POINTS, 1);
	bk.AddSlider("Tessellation"          , &state.m_trailTessellation, 1, GOLF_TRAIL_MAX_TESSELLATION, 1);
	bk.AddToggle("Fixed Control Points"  , &state.m_fixedControlPointsEnabled);

	if (PARAM_golfpoints.Get()) // don't normally need to fiddle with these ..
	{
		for (int i = 0; i < GOLF_TRAIL_MAX_CONTROL_POINTS; i++)
		{
			bk.AddTitle(atVarString("point %d", i).c_str());
			bk.AddVector("pos", &state.m_fixedControlPoints[i].m_positionAndRadius, -1000.0f, 1000.0f, 0.001f);
			bk.AddVector("col", &state.m_fixedControlPoints[i].m_colour, -1000.0f, 1000.0f, 0.001f);
		}
	}

	bk.AddTitle ("Shader Params");
	bk.AddSlider("Pixel Thickness (min)" , &state.m_pixelThickness, 0.1f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("Pixel Expansion"       , &state.m_pixelExpansion, 0.0f, 8.0f, 1.0f/32.0f);
	bk.AddSlider("Fade Opacity"          , &state.m_fadeOpacity, 0.0f, 1.0f, 1.0f/32.0f);
	bk.AddSlider("Fade Exponent Bias"    , &state.m_fadeExponentBias, -4.0f, 4.0f, 1.0f/32.0f);
	bk.AddSlider("Texture Fill"          , &state.m_textureFill, 0.0f, 1.0f, 1.0f/32.0f);
	bk.AddSeparator();
	bk.AddToggle("Facing"                , &state.m_facing);
}

#endif // __DEV

void CGolfTrail::Shutdown()
{
}

void CGolfTrail::UpdateAndRender(bool bRenderBeforePostFX)
{
	Assert(__BANK || !bRenderBeforePostFX);

	if (GetUpdateState().m_enabled)
	{
		static CGolfTrailState s;
		s = GetUpdateState(); // copy state

		if (bRenderBeforePostFX == s.m_renderBeforePostFX)
		{
			CGolfTrailControlPoint* controlPoints = s.m_fixedControlPoints;

			if (!s.m_fixedControlPointsEnabled)
			{
				// solve(z1 = p0z + v0z*t1 - G*t1^2, t1)
				// t1 = (v0.z + sqrt(v0.z^2 + 4*G*(p0.z - z1)))/(2*G)

				const Vec3V p0  = s.m_trailPositionStart;
				const Vec3V v0  = s.m_trailVelocityStart*ScalarV(s.m_trailVelocityScale);
				const float p0z = p0.GetZf();
				const float v0z = v0.GetZf();
				const float G   = 9.81f; // gravity
				const float q   = v0z*v0z + 4.0f*G*(p0z - s.m_trailZ1);

				if (q >= 0.0f)
				{
					const float t1 = (v0z + (s.m_trailAscending ? -1.0f : 1.0f)*sqrtf(q))/(2.0f*G);

					const float r0 = s.m_trailRadiusStart;
					const float r1 = s.m_trailRadiusMiddle;
					const float r2 = s.m_trailRadiusEnd;
					const Vec4V c0 = s.m_trailColourStart.GetRGBA();
					const Vec4V c1 = s.m_trailColourMiddle.GetRGBA();
					const Vec4V c2 = s.m_trailColourEnd.GetRGBA();

					for (int i = 0; i < s.m_trailNumControlPoints/2; i++)
					{
						const float x = (float)i/(float)(s.m_trailNumControlPoints - 1); // [0..1/2]
						const float t = t1*x; // [0..t1/2]
						const Vec3V p = p0 + v0*ScalarV(t) + Vec3V(0.0f, 0.0f, -G*t*t);

						// interpolate from start to middle
						const float interp = 1.0f - (1.0f - 2.0f*x)*(1.0f - 2.0f*x);
						const float radius = r0 + (r1 - r0)*interp;
						const Vec4V colour = c0 + (c1 - c0)*ScalarV(interp);

						controlPoints[i] = CGolfTrailControlPoint(p, ScalarV(radius), Color32(colour));
					}

					for (int i = s.m_trailNumControlPoints/2; i < s.m_trailNumControlPoints; i++)
					{
						const float x = (float)i/(float)(s.m_trailNumControlPoints - 1); // [1/2..1]
						const float t = t1*x; // [t1/2..t1]
						const Vec3V p = p0 + v0*ScalarV(t) + Vec3V(0.0f, 0.0f, -G*t*t);

						// interpolate from middle to end
						const float interp = (2.0f*x - 1.0f)*(2.0f*x - 1.0f);
						const float radius = r1 + (r2 - r1)*interp;
						const Vec4V colour = c1 + (c2 - c1)*ScalarV(interp);

						controlPoints[i] = CGolfTrailControlPoint(p, ScalarV(radius), Color32(colour));
					}
				}
				else
				{
#if __DEV
					grcDebugDraw::AddDebugOutput("GOLF TRAIL PATH IS INVALID (q=%f)", q);
#endif // __DEV
					return;
				}
			}

#if __DEV
			if (m_debugDraw)
			{
				for (int i = 0; i < s.m_trailNumControlPoints; i++)
				{
					grcDebugDraw::Sphere(
						controlPoints[i].m_positionAndRadius.GetXYZ(),
						controlPoints[i].m_positionAndRadius.GetWf(),
						Color32(controlPoints[i].m_colour),
						false
					);
				}
			}

			if (m_dumpControlPoints)
			{
				Displayf("golf trail: dumping %d control points", s.m_trailNumControlPoints);

				for (int i = 0; i < s.m_trailNumControlPoints; i++)
				{
					Displayf(
						"point %d: xyz = %f,%f,%f, radius = %f, colour = %d,%d,%d,%d",
						i,
						VEC4V_ARGS(controlPoints[i].m_positionAndRadius),
						(int)(0.5f + 255.0f*controlPoints[i].m_colour.GetXf()),
						(int)(0.5f + 255.0f*controlPoints[i].m_colour.GetYf()),
						(int)(0.5f + 255.0f*controlPoints[i].m_colour.GetZf()),
						(int)(0.5f + 255.0f*controlPoints[i].m_colour.GetWf())
					);
				}

				Displayf("use GOLF_TRAIL_SET_FIXED_CONTROL_POINT to set fixed control points");

				m_dumpControlPoints = false;
			}
#endif // __DEV

			if (!bRenderBeforePostFX)
			{
				DLC(dlCmdSetCurrentViewport, (gVpMan.GetUpdateGameGrcViewport()));
			}

			DLC_Add(CGolfTrail::Render, &s);
		}
	}
}

static __forceinline Vec4V_Out CubicSplineInterpolate(ScalarV_In t, Vec4V_In p0, Vec4V_In p1, Vec4V_In p2, Vec4V_In p3)
{
	const ScalarV t2 = t*t;
	const ScalarV t3 = t*t2;

	const ScalarV c0 = ScalarV(V_NEGONE  )*t3 + ScalarV(V_TWO    )*t2 + ScalarV(V_NEGONE)*t;
	const ScalarV c1 = ScalarV(V_THREE   )*t3 + ScalarV(V_NEGFIVE)*t2 + ScalarV(V_TWO   );
	const ScalarV c2 = ScalarV(V_NEGTHREE)*t3 + ScalarV(V_FOUR   )*t2 +                   t;
	const ScalarV c3 =                     t3 -                    t2;

	return ScalarV(V_HALF)*(p0*c0 + p1*c1 + p2*c2 + p3*c3);
}

__COMMENT(static) void CGolfTrail::Render(const CGolfTrailState* state)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();

	Assert(state->m_enabled);

	const int numControlPoints = state->m_trailNumControlPoints;
	const int numVertices = (numControlPoints - 1)*state->m_trailTessellation*2 + 2;

	const CGolfTrailControlPoint* controlPoints = state->m_fixedControlPoints;

	CGolfTrailVertex* vertices = Alloca(CGolfTrailVertex, numVertices);
	int               vertexIndex = 0;

	for (int patchIndex = 1; patchIndex < numControlPoints; patchIndex++)
	{
		const CGolfTrailControlPoint& cp0 = controlPoints[numControlPoints - 1 - Max<int>(0, patchIndex - 2)];
		const CGolfTrailControlPoint& cp1 = controlPoints[numControlPoints - 1 - (patchIndex - 1)];
		const CGolfTrailControlPoint& cp2 = controlPoints[numControlPoints - 1 - (patchIndex - 0)];
		const CGolfTrailControlPoint& cp3 = controlPoints[numControlPoints - 1 - Min<int>(patchIndex + 1, numControlPoints - 1)];

		for (int j = 0; j <= state->m_trailTessellation; j++)
		{
			if (j == state->m_trailTessellation && patchIndex < numControlPoints - 1)
			{
				break;
			}

			const ScalarV t = ScalarV((float)j/(float)state->m_trailTessellation);

			const Vec4V positionAndRadius = CubicSplineInterpolate(
				t,
				cp0.m_positionAndRadius,
				cp1.m_positionAndRadius,
				cp2.m_positionAndRadius,
				cp3.m_positionAndRadius
			);

			const Color32 colour     = Color32(cp1.m_colour + (cp2.m_colour - cp1.m_colour)*ScalarV(t)); // linear interp
			const ScalarV texcoord_x = ScalarV((float)((patchIndex - 1)*state->m_trailTessellation + j)/(float)(numVertices/2));

			vertices[vertexIndex++] = CGolfTrailVertex(positionAndRadius.GetXYZ(), Vec3V(V_ZERO), Vec2V(+positionAndRadius.GetW(), texcoord_x), colour);
			vertices[vertexIndex++] = CGolfTrailVertex(positionAndRadius.GetXYZ(), Vec3V(V_ZERO), Vec2V(-positionAndRadius.GetW(), texcoord_x), colour);
		}
	}

	for (int i = 0; i < numVertices; i += 2)
	{
		const Vec3V tangent = Normalize(vertices[Min<int>(i + 2, numVertices - 2)].m_position - vertices[Max<int>(0, i - 2)].m_position);

		vertices[i + 0].m_tangent = tangent;
		vertices[i + 1].m_tangent = tangent;
	}

	if (gGolfTrail.m_shader)
	{
#if RSG_PC
		// We are supposed to take the result and use if for SetVar(), but this make all the HUD elements to disapper. TODO as a separate fix.
		if(VideoResManager::IsSceneScaled())
			CRenderTargets::LockReadOnlyDepth(false, true, CRenderTargets::GetUIDepthResolved(), NULL, CRenderTargets::GetUIDepthBufferResolved_ReadOnly());
		else		
			CRenderTargets::LockReadOnlyDepth(false, true, CRenderTargets::GetDepthResolved(), NULL, CRenderTargets::GetDepthBufferResolved_ReadOnly());
#endif
		const grcViewport* vp = gVpMan.GetRenderGameGrcViewport();

		const Vec4V params[] =
		{
			Vec4V(
				(float)GRCDEVICE.GetHeight()/(vp->GetTanHFOV()*state->m_pixelThickness),
				state->m_pixelExpansion,
				state->m_fadeOpacity,
				powf(2.0f, state->m_fadeExponentBias)
			),
			Vec4V(
				state->m_textureFill,
				0.0f,
				0.0f,
				0.0f
			),
			VECTOR4_TO_VEC4V(ShaderUtil::CalculateProjectionParams(vp)),
		};

		gGolfTrail.m_shader->SetVar(gGolfTrail.m_shaderParamsID, params, NELEM(params));
		gGolfTrail.m_shader->SetVar(gGolfTrail.m_shaderGolfTrailTextureID, gGolfTrail.m_textures[0]);
		gGolfTrail.m_shader->SetVar(gGolfTrail.m_shaderDepthBufferID,
#if DEVICE_MSAA
			GRCDEVICE.GetMSAA() ? CRenderTargets::GetDepthResolved() : 
#endif
			CRenderTargets::PS3_SWITCH(GetDepthBufferAsColor(), GetDepthBuffer)()
		);

		AssertVerify(gGolfTrail.m_shader->BeginDraw(grmShader::RMC_DRAW, false, gGolfTrail.m_shaderTechnique));
		gGolfTrail.m_shader->Bind((DEV_SWITCH(state->m_facing, false) ? 1 : 0) + (state->m_renderBeforePostFX ? 2 : 0));

		grcBegin(drawTriStrip, numVertices);

		for (int i = 0; i < numVertices; i++)
		{
			const CGolfTrailVertex& vertex = vertices[i];

			grcVertex(
				VEC3V_ARGS(vertex.m_position),
				VEC3V_ARGS(vertex.m_tangent),
				vertex.m_colour,
				VEC2V_ARGS(vertex.m_texcoord)
			);
		}

		grcEnd();

		gGolfTrail.m_shader->UnBind();
		gGolfTrail.m_shader->EndDraw();
#if RSG_PC
		CRenderTargets::UnlockReadOnlyDepth();
#endif
	}
}

// ================================================================================================

__COMMENT(static) void CGolfTrailInterface::Init() { gGolfTrail.Init(); }
#if __DEV
__COMMENT(static) void CGolfTrailInterface::InitWidgets() { gGolfTrail.InitWidgets(BANKMGR.CreateBank("Golf Trail")); }
#endif // __DEV
__COMMENT(static) void CGolfTrailInterface::Shutdown() { gGolfTrail.Shutdown(); }
__COMMENT(static) void CGolfTrailInterface::UpdateAndRender(bool bRenderBeforePostFX) { gGolfTrail.UpdateAndRender(bRenderBeforePostFX); }

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailEnabled(bool bEnabled)
{
	gGolfTrail.Script_SetTrailEnabled(bEnabled);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailPath(Vec3V_In positionStart, Vec3V_In velocityStart, float velocityScale, float z1, bool bAscending)
{
	gGolfTrail.Script_SetTrailPath(positionStart, velocityStart, velocityScale, z1, bAscending);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailRadius(float radiusStart, float radiusMiddle, float radiusEnd)
{
	gGolfTrail.Script_SetTrailRadius(radiusStart, radiusMiddle, radiusEnd);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailColour(Color32 colourStart, Color32 colourMiddle, Color32 colourEnd)
{
	gGolfTrail.Script_SetTrailColour(colourStart, colourMiddle, colourEnd);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailTessellation(int numControlPoints, int tessellation)
{
	gGolfTrail.Script_SetTrailTessellation(numControlPoints, tessellation);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailFixedControlPointsEnabled(bool bEnable)
{
	gGolfTrail.Script_SetTrailFixedControlPointsEnabled(bEnable);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailFixedControlPoint(int controlPointIndex, Vec3V_In position, float radius, Color32 colour)
{
	gGolfTrail.Script_SetTrailFixedControlPoint(controlPointIndex, position, radius, colour);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailShaderParams(float pixelThickness, float pixelExpansion, float fadeOpacity, float fadeExponentBias, float textureFill)
{
	gGolfTrail.Script_SetTrailShaderParams(pixelThickness, pixelExpansion, fadeOpacity, fadeExponentBias, textureFill);
}

__COMMENT(static) void CGolfTrailInterface::Script_SetTrailFacing(bool bFacing)
{
	gGolfTrail.Script_SetTrailFacing(bFacing);
}

__COMMENT(static) void CGolfTrailInterface::Script_ResetTrail()
{
	gGolfTrail.Script_ResetTrail();
}

__COMMENT(static) float CGolfTrailInterface::Script_GetTrailMaxHeight()
{
	return gGolfTrail.Script_GetTrailMaxHeight();
}

__COMMENT(static) Vec3V_Out CGolfTrailInterface::Script_GetTrailVisualControlPoint(int controlPointIndex)
{
	return gGolfTrail.Script_GetTrailVisualControlPoint(controlPointIndex);
}
