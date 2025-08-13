// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "input/mouse.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "fwmaths/VectorUtil.h"
#include "grcore/debugdraw.h"

// game
#include "Debug/Rendering/DebugView.h"
#include "Camera/viewports/Viewport.h"
#include "renderer/Util/ShaderUtil.h"
#include "Debug/Rendering/DebugLights.h"
#include "Debug/Rendering/DebugRendering.h"
#include "Debug/Rendering/DebugDeferred.h"
#include "renderer/PostProcessFX.h"

#if __XENON
#include "system/xtl.h"
#define DBG 0
#include <xgraphics.h>
#undef DBG
#endif // __XENON

#if __BANK

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

RENDER_OPTIMISATIONS()

DebugViewInfo DebugView::m_views[DEBUGVIEW_NUM];
Vector3 DebugView::m_mousePos = Vector3(0, 0, 0);
u32 DebugView::m_histogram[3][256];
u32 DebugView::m_maxHistogramValue = 65536;

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void DebugView::Init()
{
	// Light graph view
	m_views[DEBUGVIEW_LIGHTGRAPH].m_name = atString("Light falloff graph");
	m_views[DEBUGVIEW_LIGHTGRAPH].m_size = Vector2(float(SCREEN_WIDTH / 4), float(SCREEN_HEIGHT / 4));
	m_views[DEBUGVIEW_LIGHTGRAPH].m_position = Vector2(100.0f, float(SCREEN_HEIGHT / 2) - m_views[DEBUGVIEW_LIGHTGRAPH].m_size.y / 2.0f);

	// Tone-mapping curve view
	m_views[DEBUGVIEW_TONEMAPPING_GRAPH].m_name = atString("Tonemapping Curve");
	m_views[DEBUGVIEW_TONEMAPPING_GRAPH].m_size = Vector2(float(SCREEN_WIDTH / 2), float(SCREEN_HEIGHT / 2));
	m_views[DEBUGVIEW_TONEMAPPING_GRAPH].m_position = Vector2(
		float(SCREEN_WIDTH / 2) - m_views[DEBUGVIEW_TONEMAPPING_GRAPH].m_size.x / 2.0f, 
		float(SCREEN_HEIGHT / 2) - m_views[DEBUGVIEW_TONEMAPPING_GRAPH].m_size.y / 2.0f);
	m_views[DEBUGVIEW_TONEMAPPING_GRAPH].m_show = false;

	m_views[DEBUGVIEW_SCREEN_HISTOGRAM].m_name = atString("Screen histogram");
	m_views[DEBUGVIEW_SCREEN_HISTOGRAM].m_size = Vector2(float(SCREEN_WIDTH / 2), float(SCREEN_HEIGHT / 2));
	m_views[DEBUGVIEW_SCREEN_HISTOGRAM].m_position = Vector2(
		float(SCREEN_WIDTH / 2) - m_views[DEBUGVIEW_SCREEN_HISTOGRAM].m_size.x / 2.0f, 
		float(SCREEN_HEIGHT / 2) - m_views[DEBUGVIEW_SCREEN_HISTOGRAM].m_size.y / 2.0f);
	m_views[DEBUGVIEW_SCREEN_HISTOGRAM].m_show = false;

	// Exposure
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_name = atString("Exposure Curve");
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_size = Vector2(float(SCREEN_WIDTH / 2), float(SCREEN_HEIGHT / 2));
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_position = Vector2(
		float(SCREEN_WIDTH / 2) - m_views[DEBUGVIEW_EXPOSURE_CURVE].m_size.x / 2.0f, 
		float(SCREEN_HEIGHT / 2) - m_views[DEBUGVIEW_EXPOSURE_CURVE].m_size.y / 2.0f);
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_minX =  0.0f;
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_maxX =  2.0f;
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_minY = -4.0f;
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_maxY =  4.0f;
	m_views[DEBUGVIEW_EXPOSURE_CURVE].m_show = false;

}

// ----------------------------------------------------------------------------------------------- //

void DebugView::Update()
{
	if ((ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) && ioMapper::DebugKeyDown(KEY_CONTROL))
	{
		m_mousePos = Vector3((float)ioMouse::GetX() * (float)ioMouse::GetInvWidth(),
							 (float)ioMouse::GetY() * (float)ioMouse::GetInvHeight(),
							 0.0);
	}
}

// ----------------------------------------------------------------------------------------------- //

template <typename T> __forceinline T GetFromLocalMem(const T* ptr, int i = 0)
{
#if __XENON || __PS3
	const u32 addr = reinterpret_cast<const u32>(ptr + i);
	const u32 base = addr & ~15; // aligned

	ALIGNAS(16) u8 temp[16] ;

#if __XENON
	*reinterpret_cast<Vec::Vector_4V_uint*>(temp) = __lvx(reinterpret_cast<const u32*>(base), 0);
#else
	*reinterpret_cast<Vec::Vector_4V_uint*>(temp) = vec_ld(0, reinterpret_cast<const u32*>(base));
#endif
	return *reinterpret_cast<const T*>(temp + (addr & 15));
#else
	return ptr[i];	
#endif
}

// ----------------------------------------------------------------------------------------------- //

void DebugView::GenerateHistogram()
{
	if (!DebugView::IsEnabled(DEBUGVIEW_SCREEN_HISTOGRAM))
		return;

	memset(m_histogram[0], 0, 256 * sizeof(u32));
	memset(m_histogram[1], 0, 256 * sizeof(u32));
	memset(m_histogram[2], 0, 256 * sizeof(u32));

	grcTexture* backBuffer = (grcTexture*)PS3_SWITCH(grcTextureFactory::GetInstance().GetFrontBuffer(), grcTextureFactory::GetInstance().GetBackBuffer(false));

	const int w = backBuffer->GetWidth();
	const int h = backBuffer->GetHeight();
	grcTextureLock lock;

	if (backBuffer->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock))
	{
		for (u32 y = 0; y < h; y++)
		{
			for (u32 x = 0; x < w; x++)
			{
				#if __XENON
					const Color32 c = GetFromLocalMem((const Color32*)lock.Base, XGAddress2DTiledOffset(x, y, w, sizeof(Color32)));
					const int r = c.GetRed();
					const int g = c.GetGreen();
					const int b = c.GetBlue();
				#else
					const Color32 c = GetFromLocalMem((const Color32*)lock.Base, x + y*w);
					const int r = c.GetBlue(); // swap r,b
					const int g = c.GetGreen();
					const int b = c.GetRed(); // swap r,b
				#endif

				m_histogram[0][r]++;
				m_histogram[1][g]++;
				m_histogram[2][b]++;
			}
		}

		backBuffer->UnlockRect(lock);
	}
}

// ----------------------------------------------------------------------------------------------- //

void DebugView::DrawHistogram()
{
	DebugViewInfo& view = m_views[DEBUGVIEW_SCREEN_HISTOGRAM];

#if __PPU
	float lineWidth = 1.25;
	GCM_STATE(SetLineWidth, (u32)(lineWidth * 8.0f)); // 6.3 fixed point
#endif // __PPU

	const Vector2 screenSize = Vector2(1.0f / (float)GRCDEVICE.GetWidth(), 1.0f / (float)GRCDEVICE.GetHeight());
	const Vector2 minBox = view.m_position;
	const Vector2 maxBox = (view.m_position + view.m_size);

	// Background box
	grcDebugDraw::RectAxisAligned(minBox * screenSize, maxBox * screenSize, Color32(0, 0, 0, 64), true);

	u32 maxR = 0, maxG = 0, maxB = 0;
	for (u32 i = 0; i < 256; i++)
	{
		maxR = Max<u32>(maxR, m_histogram[0][i]);
		maxG = Max<u32>(maxG, m_histogram[1][i]);
		maxB = Max<u32>(maxB, m_histogram[2][i]);
	}
	const u32 finalMax = Min<u32>(Max<u32>(maxR, Max<u32>(maxG, maxB)), m_maxHistogramValue);

	// Actual graph
	grcBegin(drawLineStrip, 256);
	grcColor3f(1.0f, 0.0f, 0.0f);
	for (int x = 0; x < 256; x++)
	{
		float fx = x / 256.0f;
		float fy = m_histogram[0][x] / float(finalMax);
		Vector2 pos = view.GetScreenPos(fx, fy);
		grcVertex2f(pos);
	}
	grcEnd();

	grcBegin(drawLineStrip, 256);
	grcColor3f(0.0f, 1.0f, 0.0f);
	for (int x = 0; x < 256; x++)
	{
		float fx = x / 256.0f;
		float fy = m_histogram[1][x] / float(finalMax);
		Vector2 pos = view.GetScreenPos(fx, fy);
		grcVertex2f(pos);
	}
	grcEnd();

	grcBegin(drawLineStrip, 256);
	grcColor3f(0.0f, 0.0f, 1.0f);
	for (int x = 0; x < 256; x++)
	{
		float fx = x / 256.0f;
		float fy = m_histogram[2][x] / float(finalMax);
		Vector2 pos = view.GetScreenPos(fx, fy);
		grcVertex2f(pos);
	}
	grcEnd();

#if __PPU
	GCM_STATE(SetLineWidth, 8); // restore
#endif // __PPU
}

// ----------------------------------------------------------------------------------------------- //

void DebugView::DrawLightGraph()
{
	DebugViewInfo& view = m_views[DEBUGVIEW_LIGHTGRAPH];

#if __PPU
	float lineWidth = 1.25;
	GCM_STATE(SetLineWidth, (u32)(lineWidth * 8.0f)); // 6.3 fixed point
#endif // __PPU

	CLightSource* light = DebugLights::GetCurrentLightSource(false);
	if (light)
	{
		grcBegin(drawLineStrip, (int)view.m_size.x);

		for (int x = 0; x < view.m_size.x; x++)
		{
			float fx = x / view.m_size.x;
			float fy = PowApprox(1.0f - fx, light->GetFalloffExponent());

			Vector2 pos = view.GetScreenPos(fx, fy);

			if (fy < 0.001f)
			{
				grcColor3f(1.0f, 0.0f, 1.0f);
			}
			else if (fy < 0.01f)
			{
				grcColor3f(0.0f, 0.0f, 1.0f);
			}
			else
			{
				grcColor3f(1.0f - fy, fy, 0.0f);
			}

			grcVertex2f(pos);
		}

		grcEnd();
	}

#if __PPU
	GCM_STATE(SetLineWidth, 8); // restore
#endif // __PPU
}

// ----------------------------------------------------------------------------------------------- //

void DebugView::DrawTonemappingCurve()
{
	DebugViewInfo& view = m_views[DEBUGVIEW_TONEMAPPING_GRAPH];

#if __PPU
	float lineWidth = 1.25;
	GCM_STATE(SetLineWidth, (u32)(lineWidth * 8.0f)); // 6.3 fixed point
#endif // __PPU

	const float whitePoint = PostFX::GetToneMapWhitePoint();
	const float tonemappedWhitePoint = PostFX::FilmicTweakedTonemap(whitePoint);

	// Background box
	grcDebugDraw::RectAxisAligned(view.GetScreenPos(0.0f, 0.0f), view.GetScreenPos(1.0f, 1.0f), Color32(0, 0, 0, 64), true);

	// Draw line
	grcDebugDraw::Line(view.GetScreenPos(0.0f, 0.0f), view.GetScreenPos(1.0f, 1.0f), Color32(128, 128, 128, 64));

	// Graph lines
	Vector2 vertTop, vertBottom;
	float numSteps, stepSize;
	char label[16];
	numSteps = ceilf(whitePoint);
	stepSize = 1.0f / numSteps;

	for (u32 i = 0; i <= numSteps; i++)
	{
		Vector2 topPoint = view.GetScreenPos(0.0f + i * stepSize, 1.0f);
		Vector2 bottomPoint = view.GetScreenPos(0.0f + i * stepSize, 0.0f);
		grcDebugDraw::Line(topPoint, bottomPoint, Color32(128, 128, 128, 64));
		sprintf(label, "%.2f", whitePoint * stepSize * i);
		grcDebugDraw::Text(bottomPoint, Color32(255, 255, 255, 255), label, true);
	}

	// Actual graph
	u32 currentX = 0;
	u32 totalVerts = (u32)view.m_size.x;
	u32 loop = (u32)ceilf(totalVerts / (float)grcBeginMax);
	const float maxX = whitePoint;
	const float maxY = 1.0f;

	for (u32 i = 0; i < loop; i++)
	{
		const u32 numVerts = (totalVerts > grcBeginMax) ? grcBeginMax : totalVerts;

		grcBegin(drawLineStrip, numVerts);
		for (int x = currentX; x < currentX + numVerts; x++)
		{
			float fx = x / (float)view.m_size.x * whitePoint;
			float fy = PostFX::FilmicTweakedTonemap(fx) / tonemappedWhitePoint;
			
			Vector2 pos = view.GetScreenPos(fx / maxX, fy / maxY);

			if (fy / maxY < fx / maxX)
			{
				grcColor4f(0.0f, 1.0f, 0.0f, 1.0f);
			}
			else if (fy / maxY > fx / maxX)
			{
				grcColor4f(1.0f, 0.0f, 0.0f, 1.0f);
			}
			else
			{
				grcColor4f(0.0f, 0.0f, 1.0f, 1.0f);
			}
			
			grcVertex2f(pos);
		}
		grcEnd();

		currentX += numVerts;
		totalVerts -= numVerts;
	}

#if __PPU
	GCM_STATE(SetLineWidth, 8); // restore
#endif // __PPU

}

// ----------------------------------------------------------------------------------------------- //

void DebugView::DrawExposureCurve()
{
	DebugViewInfo& view = m_views[DEBUGVIEW_EXPOSURE_CURVE];

	#if __PPU
		float lineWidth = 1.00;
		GCM_STATE(SetLineWidth, (u32)(lineWidth * 8.0f)); // 6.3 fixed point
	#endif // __PPU

	// Actual graph
	u32 currentX = 0;
	u32 totalVerts = (u32)view.m_size.x;
	u32 loop = (u32)ceilf(totalVerts / (float)grcBeginMax);

	// Background box
	grcBegin(drawQuads, 4);
		grcColor4f(Vector4(64.0f, 64.0f, 64.0f, 255.0f) / 255.0f);
		grcVertex2f(view.GetScreenPos(0.0f, 0.0f));
		grcVertex2f(view.GetScreenPos(1.0f, 0.0f));
		grcVertex2f(view.GetScreenPos(1.0f, 1.0f));
		grcVertex2f(view.GetScreenPos(0.0f, 1.0f));
	grcEnd();

	float zeroLineY = 0.0f - view.m_minY / (view.m_maxY - view.m_minY);

	grcBegin(drawLines, 2);
		grcColor4f(Vector4(128.0f, 128.0f, 128.0f, 255.0f) / 255.0f);
		grcVertex2f(view.GetScreenPos(0.0f, zeroLineY));
		grcVertex2f(view.GetScreenPos(1.0f, zeroLineY));
	grcEnd();

	for (u32 i = 0; i < loop; i++)
	{
		const u32 numVerts = (totalVerts > grcBeginMax) ? grcBeginMax : totalVerts;

		grcBegin(drawLineStrip, numVerts);
		for (int x = currentX; x < currentX + numVerts; x++)
		{
			float fx = x / (float)view.m_size.x;
			const float exposure = PostFX::GetExposureValue((fx * (view.m_maxX - view.m_minX)) - view.m_minX);
			float fy = Saturate((exposure - view.m_minY) / (view.m_maxY - view.m_minY));

			Vector2 pos = view.GetScreenPos(fx, fy);
			Vector4 col = (exposure < 0.0f) ? Vector4(0.0f, 1.0f, 0.0f, 1.0f) : Vector4(1.0f, 0.0f, 0.0f, 1.0f);

			grcColor4f(col);
			grcVertex2f(pos);
		}
		grcEnd();

		currentX += numVerts;
		totalVerts -= numVerts;
	}

	#if __PPU
		GCM_STATE(SetLineWidth, 8); // restore
	#endif // __PPU
}

// ----------------------------------------------------------------------------------------------- //

void DebugView::Draw()
{
	grcViewport* currentVp = grcViewport::GetCurrent();

	Mat34V worldMtx		= currentVp->GetWorldMtx();
	Mat34V cameraMtx	= currentVp->GetCameraMtx();
	Mat44V projMtx		= currentVp->GetProjection();

	currentVp->SetWorldIdentity();
	currentVp->SetCameraIdentity();
	currentVp->Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Normal);

	if (m_views[DEBUGVIEW_LIGHTGRAPH].m_show) { DrawLightGraph(); }
	if (m_views[DEBUGVIEW_TONEMAPPING_GRAPH].m_show) { DrawTonemappingCurve(); }
	if (m_views[DEBUGVIEW_SCREEN_HISTOGRAM].m_show) { DrawHistogram(); }
	if (m_views[DEBUGVIEW_EXPOSURE_CURVE].m_show) { DrawExposureCurve(); }

	currentVp->SetWorldMtx(worldMtx);
	currentVp->SetCameraMtx(cameraMtx);
	currentVp->SetProjection(projMtx);
}

// ----------------------------------------------------------------------------------------------- //

void DebugView::AddWidgets(bkBank *bk)
{
	bk->PushGroup("Window");

	const Vector2 minVec = Vector2(0, 0), 
		maxVec = Vector2(float(SCREEN_WIDTH), float(SCREEN_HEIGHT)),
		delta(1.0, 1.0f);

	for (u32 i = 0; i < DEBUGVIEW_NUM; i++)
	{
		bk->PushGroup(m_views[i].m_name.c_str());
			bk->AddToggle("Show window", &m_views[i].m_show);
			bk->AddSlider("Position", &m_views[i].m_position, minVec, maxVec, delta);
			bk->AddSlider("Size", &m_views[i].m_size, minVec, maxVec, delta);
			bk->AddSlider("Min X", &m_views[i].m_minX, -16.0f, 16.0f, 0.01f);
			bk->AddSlider("Max X", &m_views[i].m_maxX, -16.0f, 16.0f, 0.01f);
			bk->AddSlider("Min Y", &m_views[i].m_minY, -16.0f, 16.0f, 0.01f);
			bk->AddSlider("Max Y", &m_views[i].m_maxY, -16.0f, 16.0f, 0.01f);

		bk->PopGroup();
	}

	bk->AddSeparator();
	bk->AddSlider("Max histogram value", &m_maxHistogramValue, 0, 921600, 100);

	bk->PopGroup();
}

// ----------------------------------------------------------------------------------------------- //

#endif
