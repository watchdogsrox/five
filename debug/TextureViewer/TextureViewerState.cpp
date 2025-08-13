// ==========================================
// debug/textureviewer/textureviewerstate.cpp
// (c) 2010 RockstarNorth
// ==========================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

// Rage headers
#include "grcore/debugdraw.h"
#include "grcore/image.h"
#include "grcore/setup.h"
#include "grcore/stateblock.h"
#include "grcore/texture.h"
#include "grcore/texture_d3d11.h"
#include "grcore/texture_durango.h"
#include "grcore/texture_gnm.h"
#include "grcore/texturefactory_d3d11.h"
#include "grcore/viewport.h"
#include "system/memory.h"

// Framework headers
#include "fwutil/popups.h"
#include "fwutil/xmacro.h"
#include "fwscene/stores/txdstore.h"

// Game headers
#include "core/game.h"
#include "renderer/color.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"

#include "debug/textureviewer/textureviewerutil.h"
#include "debug/textureviewer/textureviewerDTQ.h"
#include "debug/textureviewer/textureviewerstate.h"

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

// ================================================================================================

CDTVState::CDTVState()
{
	Cleanup();
}

CDTVState::~CDTVState()
{
	Cleanup();
}

void CDTVState::Render(const grcTexture* texture, int viewMode, const fwBox2D& dstBounds_, const fwBox2D& srcBounds) const
{
	using namespace DebugTextureViewerDTQ;

	const int numMips = texture->GetMipMapCount() - m_previewTextureLOD;
	fwBox2D dstBounds = dstBounds_;

	for (int mipIndex = 0; mipIndex < numMips; mipIndex++)
	{
		DTQBlit(
			(eDTQViewMode)viewMode,
			m_previewTextureFiltered,
			(float)(m_previewTextureLOD + mipIndex),
			m_previewTextureLayer,
			m_previewTextureVolumeSlice + (m_previewTextureVolumeSliceDenom ? (float)m_previewTextureVolumeSliceNumer/(float)m_previewTextureVolumeSliceDenom : 0.0f),
			m_previewTextureVolumeCoordSwap,
			m_previewTextureCubeForceOpaque,
			m_previewTextureCubeSampleAllMips,
			m_previewTextureCubeCylinderAspect,
			m_previewTextureCubeFaceZoom,
			(eDTQCubeFace)m_previewTextureCubeFace,
			m_previewTextureCubeRotationYPR,
			(eDTQCubeProjection)m_previewTextureCubeProjection,
			m_previewTextureCubeProjClip,
			m_previewTextureCubeNegate,
			m_previewTextureCubeRefDots,
			m_previewTextureSpecularL,
			m_previewTextureSpecularBumpiness,
			m_previewTextureSpecularExponent,
			m_previewTextureSpecularScale,
			eDTQPCS_Pixels,
			dstBounds,
			0.0f, // z
			eDTQTCS_Texels,
			srcBounds,
			CRGBA_White(),
			powf(2.0f, m_previewTextureBrightness),
			texture
		);

		if (!m_previewTextureShowMips)
		{
			break;
		}

		const float dstW = (dstBounds.x1 - dstBounds.x0);
		const float dstH = (dstBounds.y1 - dstBounds.y0);

		if (mipIndex == 0) // right
		{
			dstBounds.x0 += dstW;
			dstBounds.x1 += dstW*0.5f;
			dstBounds.y1 -= dstH*0.5f;
		}
		else // down
		{
			dstBounds.y0 += dstH;
			dstBounds.x1 -= dstW*0.5f;
			dstBounds.y1 += dstH*0.5f;
		}
	}
}

class CSortedTextureProxy
{
public:
	CSortedTextureProxy() {}
	CSortedTextureProxy(const grcTexture* texture) : m_texture(texture) {}

	static s32 CompareFuncPtr(CSortedTextureProxy* const* a, CSortedTextureProxy* const* b) { return CompareFunc(*a, *b); }
	static s32 CompareFunc   (CSortedTextureProxy  const* a, CSortedTextureProxy  const* b)
	{
		const char *nameA = (a->m_texture) ? a->m_texture->GetName() : "";
		const char *nameB = (b->m_texture) ? b->m_texture->GetName() : "";
		return stricmp(nameA, nameB);
	}

	const grcTexture* m_texture;
};

void CDTVState::Render() const
{
	using namespace DebugTextureViewerDTQ;
	const grcTexture* texture = m_previewTexture;

	if (m_previewTextureCubeReflectionMap)
	{
		texture = CRenderPhaseReflection::ms_renderTargetCube;
	}

	if (texture)
	{
		if (m_previewTextureViewMode == eDTQVM_RGB_plus_A)
		{
			const float dstW = (m_previewDstBounds.x1 - m_previewDstBounds.x0);
			const float dstH = (m_previewDstBounds.y1 - m_previewDstBounds.y0);
			fwBox2D dstBounds;

			dstBounds = m_previewDstBounds;
			dstBounds.x1 -= dstW*0.5f;
			dstBounds.y1 -= dstH*0.5f;

			Render(texture, eDTQVM_RGB, dstBounds, m_previewSrcBounds);

			dstBounds = m_previewDstBounds;
			dstBounds.x1 -= dstW*0.5f;
			dstBounds.y0 += dstH*0.5f;

			Render(texture, eDTQVM_A_monochrome, dstBounds, m_previewSrcBounds);
		}
		else if (m_previewTextureViewMode == eDTQVM_R_G_B_A)
		{
			const float dstW = (m_previewDstBounds.x1 - m_previewDstBounds.x0);
			const float dstH = (m_previewDstBounds.y1 - m_previewDstBounds.y0);
			fwBox2D dstBounds;

			dstBounds = m_previewDstBounds;
			dstBounds.x1 -= dstW*0.5f;
			dstBounds.y1 -= dstH*0.5f;

			Render(texture, eDTQVM_R_monochrome, dstBounds, m_previewSrcBounds); // top left

			dstBounds = m_previewDstBounds;
			dstBounds.x0 += dstW*0.5f;
			dstBounds.y1 -= dstH*0.5f;

			Render(texture, eDTQVM_G_monochrome, dstBounds, m_previewSrcBounds); // top right

			dstBounds = m_previewDstBounds;
			dstBounds.x0 += dstW*0.5f;
			dstBounds.y0 += dstH*0.5f;

			Render(texture, eDTQVM_B_monochrome, dstBounds, m_previewSrcBounds); // bottom right

			dstBounds = m_previewDstBounds;
			dstBounds.x1 -= dstW*0.5f;
			dstBounds.y0 += dstH*0.5f;

			Render(texture, eDTQVM_A_monochrome, dstBounds, m_previewSrcBounds); // bottom left
		}
		else if (0 && m_previewTextureViewMode == eDTQVM_specular)
		{
			const float dstW = (m_previewDstBounds.x1 - m_previewDstBounds.x0);
			const float dstH = (m_previewDstBounds.y1 - m_previewDstBounds.y0);
			fwBox2D dstBounds;

			dstBounds = m_previewDstBounds;
			dstBounds.x1 -= dstW*0.5f;
			dstBounds.y1 -= dstH*0.5f;

			Render(texture, eDTQVM_RGBA, dstBounds, m_previewSrcBounds);

			dstBounds = m_previewDstBounds;
			dstBounds.x1 -= dstW*0.5f;
			dstBounds.y0 += dstH*0.5f;

			Render(texture, eDTQVM_specular, dstBounds, m_previewSrcBounds);
		}
		else
		{
			Render(texture, m_previewTextureViewMode, m_previewDstBounds, m_previewSrcBounds);
		}

#if DEBUG_TEXTURE_VIEWER_HISTOGRAM
		if (m_histogramEnabled) // slow as shit ..
		{
			RenderHistogram();
		}
#endif // DEBUG_TEXTURE_VIEWER_HISTOGRAM
#if DEBUG_TEXTURE_VIEWER_GETPIXELTEST
		if (m_getPixelTest && Min<int>(texture->GetWidth(), texture->GetHeight()) >= 4)
		{
			RenderGetPixelTest();
		}
#endif // DEBUG_TEXTURE_VIEWER_GETPIXELTEST
	}
	else if (m_previewTxd)
	{
		const int numCols = m_previewTxdGrid + 1;
		const int numRows = m_previewTxdGrid + 1;
		int textureIndex = numCols*(int)(m_previewTxdGridOffset*((float)(m_previewTxd->GetCount()/numCols - numRows)));

		atArray<CSortedTextureProxy> textures; // create sorted array of textures

		for (int i = 0; i < m_previewTxd->GetCount(); i++)
		{
			const grcTexture* texture = m_previewTxd->GetEntry(i);

			if (texture)
			{
				textures.PushAndGrow(CSortedTextureProxy(texture));
			}
		}

		textures.QSort(0, -1, CSortedTextureProxy::CompareFunc);

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				if (textureIndex < textures.size())
				{
					const grcTexture* texture = textures[textureIndex++].m_texture;

					if (texture)
					{
						const fwBox2D dstBounds(
							m_previewDstBounds.x0 + (m_previewDstBounds.x1 - m_previewDstBounds.x0)*(float)(col + 0)/(float)numCols,
							m_previewDstBounds.y0 + (m_previewDstBounds.y1 - m_previewDstBounds.y0)*(float)(row + 0)/(float)numRows,
							m_previewDstBounds.x0 + (m_previewDstBounds.x1 - m_previewDstBounds.x0)*(float)(col + 1)/(float)numCols,
							m_previewDstBounds.y0 + (m_previewDstBounds.y1 - m_previewDstBounds.y0)*(float)(row + 1)/(float)numRows
						);
						const fwBox2D srcBounds(
							0.0f,
							0.0f,
							(float)texture->GetWidth (),
							(float)texture->GetHeight()
						);

						Render(texture, m_previewTextureViewMode, dstBounds, srcBounds);
					}
				}
			}
		}
	}

#if DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
	if (m_dumpTexture)
	{
		if (texture)
		{
			Displayf("dumping %s", texture->GetName());
			DumpTexture(texture, true);
		}
		else
		{
			Displayf("no preview texture to dump");
		}
	}

	if (m_dumpTextureRaw)
	{
		if (texture)
		{
			Displayf("dumping %s", texture->GetName());
			DumpTexture(texture, false);
		}
		else
		{
			Displayf("no preview texture to dump");
		}
	}

	if (m_dumpTxdRaw)
	{
		const fwTxd* txd = m_previewTxd;

		if (txd)
		{
			Displayf("dumping preview txd (%d textures)", txd->GetCount());

			for (int i = 0; i < txd->GetCount(); i++)
			{
				const grcTexture* texture = txd->GetEntry(i);

				if (texture)
				{
					Displayf("dumping %s", texture->GetName());
					DumpTexture(texture, false);
				}
			}
		}
		else
		{
			Displayf("no preview txd to dump");
		}
	}
#endif // DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
}

#if DEBUG_TEXTURE_VIEWER_HISTOGRAM

static int GetTextureHistogram(int r[256], int g[256], int b[256], int a[256], int shift, u8 alphaMin, const grcTexture* texture)
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	int count = 0;

	sysMemSet(r, 0, (256 >> shift)*sizeof(int));
	sysMemSet(g, 0, (256 >> shift)*sizeof(int));
	sysMemSet(b, 0, (256 >> shift)*sizeof(int));
	sysMemSet(a, 0, (256 >> shift)*sizeof(int));

	if (texture)
	{
		const int w = texture->GetWidth ();
		const int h = texture->GetHeight();

		grcTextureLock lock;
		texture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);

		grcImage* image = rage_new grcImage();
		image->LoadAsTextureAlias(texture, &lock);

		for (int y = 0; y <= h - 4; y += 4)
		{
			for (int x = 0; x <= w - 4; x += 4)
			{
				Color32 block[4*4];

				image->GetPixelBlock(block, x, y);

				for (int yy = 0; yy < 4; yy++)
				{
					for (int xx = 0; xx < 4; xx++)
					{
						const Color32 colour = block[xx + yy*4];
						const u8 alpha = colour.GetAlpha();

						if (alpha >= alphaMin)
						{
							r[colour.GetRed  () >> shift]++;
							g[colour.GetGreen() >> shift]++;
							b[colour.GetBlue () >> shift]++;
							a[colour.GetAlpha() >> shift]++;
							count++;
						}
					}
				}
			}
		}

		image->ReleaseAlias();
		texture->UnlockRect(lock);
	}

	return count;
}

void CDTVState::RenderHistogram() const
{
	int r[256];
	int g[256];
	int b[256];
	int a[256];

	const int numBuckets = 256 >> m_histogramShift;

	GetTextureHistogram(r, g, b, a, m_histogramShift, static_cast<u8>(m_histogramAlphaMin), m_previewTexture);

	if (m_histogramOutliers > 0)
	{
		// remove outliers (low)
		{
			int r_remove = m_histogramOutliers;
			int g_remove = m_histogramOutliers;
			int b_remove = m_histogramOutliers;
			int a_remove = m_histogramOutliers;

			for (int i = 0; i < numBuckets; i++) { if (r[i] < r_remove) { r_remove -= r[i]; r[i] = 0; } else { r[i] -= r_remove; break; } }
			for (int i = 0; i < numBuckets; i++) { if (g[i] < g_remove) { g_remove -= g[i]; g[i] = 0; } else { g[i] -= g_remove; break; } }
			for (int i = 0; i < numBuckets; i++) { if (b[i] < b_remove) { b_remove -= b[i]; b[i] = 0; } else { b[i] -= b_remove; break; } }
			for (int i = 0; i < numBuckets; i++) { if (a[i] < a_remove) { a_remove -= a[i]; a[i] = 0; } else { a[i] -= a_remove; break; } }
		}

		// remove outliers (high)
		{
			int r_remove = m_histogramOutliers;
			int g_remove = m_histogramOutliers;
			int b_remove = m_histogramOutliers;
			int a_remove = m_histogramOutliers;

			for (int i = numBuckets - 1; i >= 0; i--) { if (r[i] < r_remove) { r_remove -= r[i]; r[i] = 0; } else { r[i] -= r_remove; break; } }
			for (int i = numBuckets - 1; i >= 0; i--) { if (g[i] < g_remove) { g_remove -= g[i]; g[i] = 0; } else { g[i] -= g_remove; break; } }
			for (int i = numBuckets - 1; i >= 0; i--) { if (b[i] < b_remove) { b_remove -= b[i]; b[i] = 0; } else { b[i] -= b_remove; break; } }
			for (int i = numBuckets - 1; i >= 0; i--) { if (a[i] < a_remove) { a_remove -= a[i]; a[i] = 0; } else { a[i] -= a_remove; break; } }
		}
	}

	int r_max = 0;
	int g_max = 0;
	int b_max = 0;
	int a_max = 0;

	int r_range[2] = {-1,-1};
	int g_range[2] = {-1,-1};
	int b_range[2] = {-1,-1};
	int a_range[2] = {-1,-1};

	for (int i = 0; i < numBuckets; i++)
	{
		r_max = Max<int>(r[i], r_max);
		g_max = Max<int>(g[i], g_max);
		b_max = Max<int>(b[i], b_max);
		a_max = Max<int>(a[i], a_max);

		if (r[i] >= m_histogramBucketMin) { if (r_range[0] == -1) { r_range[0] = r_range[1] = i; } else { r_range[1] = i; } }
		if (g[i] >= m_histogramBucketMin) { if (g_range[0] == -1) { g_range[0] = g_range[1] = i; } else { g_range[1] = i; } }
		if (b[i] >= m_histogramBucketMin) { if (b_range[0] == -1) { b_range[0] = b_range[1] = i; } else { b_range[1] = i; } }
		if (a[i] >= m_histogramBucketMin) { if (a_range[0] == -1) { a_range[0] = a_range[1] = i; } else { a_range[1] = i; } }
	}

	const float oor = m_histogramScale/(float)Max<int>(1, r_max);
	const float oog = m_histogramScale/(float)Max<int>(1, g_max);
	const float oob = m_histogramScale/(float)Max<int>(1, b_max);
	const float ooa = m_histogramScale/(float)Max<int>(1, a_max);

	const float scaleX = 1.0f/grcViewport::GetDefaultScreen()->GetWidth ();
	const float scaleY = 1.0f/grcViewport::GetDefaultScreen()->GetHeight();

	// draw midpoint lines
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*300.0f), Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*(300.0f - 95.0f)), Color32(0,0,0,32));
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*400.0f), Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*(400.0f - 95.0f)), Color32(0,0,0,32));
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*500.0f), Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*(500.0f - 95.0f)), Color32(0,0,0,32));
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*600.0f), Vector2(scaleX*(float)(48 + numBuckets/2), scaleY*(600.0f - 95.0f)), Color32(0,0,0,32));

	for (int i = 0; i < numBuckets; i++)
	{
		const float x = scaleX*(float)(48 + i);

		// red
		{
			const float y0 = scaleY*(300.0f);
			const float y1 = scaleY*(300.0f - 95.0f*Min<float>(1.0f, (float)r[i]*oor));

			grcDebugDraw::Line(Vector2(x, y0), Vector2(x, y1), CRGBA_Red());
		}
		// green
		{
			const float y0 = scaleY*(400.0f);
			const float y1 = scaleY*(400.0f - 95.0f*Min<float>(1.0f, (float)g[i]*oog));

			grcDebugDraw::Line(Vector2(x, y0), Vector2(x, y1), CRGBA_Green());
		}
		// blue
		{
			const float y0 = scaleY*(500.0f);
			const float y1 = scaleY*(500.0f - 95.0f*Min<float>(1.0f, (float)b[i]*oob));

			grcDebugDraw::Line(Vector2(x, y0), Vector2(x, y1), CRGBA_Blue());
		}
		// alpha
		{
			const float y0 = scaleY*(600.0f);
			const float y1 = scaleY*(600.0f - 95.0f*Min<float>(1.0f, (float)a[i]*ooa));

			grcDebugDraw::Line(Vector2(x, y0), Vector2(x, y1), CRGBA_White());
		}
	}

	// black bars cover entire range of buckets
	grcDebugDraw::Line(Vector2(scaleX*(float)(48), scaleY*300.0f), Vector2(scaleX*(float)(48 + numBuckets), scaleY*300.0f), CRGBA_Black());
	grcDebugDraw::Line(Vector2(scaleX*(float)(48), scaleY*400.0f), Vector2(scaleX*(float)(48 + numBuckets), scaleY*400.0f), CRGBA_Black());
	grcDebugDraw::Line(Vector2(scaleX*(float)(48), scaleY*500.0f), Vector2(scaleX*(float)(48 + numBuckets), scaleY*500.0f), CRGBA_Black());
	grcDebugDraw::Line(Vector2(scaleX*(float)(48), scaleY*600.0f), Vector2(scaleX*(float)(48 + numBuckets), scaleY*600.0f), CRGBA_Black());

	// yellow bars cover actual range of values
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + r_range[0]), scaleY*300.0f), Vector2(scaleX*(float)(48 + r_range[1]), scaleY*300.0f), CRGBA_Yellow());
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + g_range[0]), scaleY*400.0f), Vector2(scaleX*(float)(48 + g_range[1]), scaleY*400.0f), CRGBA_Yellow());
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + b_range[0]), scaleY*500.0f), Vector2(scaleX*(float)(48 + b_range[1]), scaleY*500.0f), CRGBA_Yellow());
	grcDebugDraw::Line(Vector2(scaleX*(float)(48 + a_range[0]), scaleY*600.0f), Vector2(scaleX*(float)(48 + a_range[1]), scaleY*600.0f), CRGBA_Yellow());
}

#endif // DEBUG_TEXTURE_VIEWER_HISTOGRAM

#if DEBUG_TEXTURE_VIEWER_GETPIXELTEST

void CDTVState::RenderGetPixelTest() const
{
	DEBUG_TEXTURE_VIEWER_USE_DEBUG_MEMORY();

	using namespace DebugTextureViewerDTQ;

	Assert(m_previewTexture);

	const float drawOffsetX = 100.0f;
	const float drawOffsetY = 100.0f;

	grcTextureLock lock;
	
	if (m_previewTexture->LockRect(0, Min<int>(m_getPixelMipIndex, m_previewTexture->GetMipMapCount() - 1), lock, grcsRead | grcsAllowVRAMLock))
	{
		const int w = lock.Width;
		const int h = lock.Height;

		grcImage* image = rage_new grcImage();
		image->LoadAsTextureAlias(m_previewTexture, &lock);

		const int mouseX = (int)floorf(((float)m_getPixelTestMousePosX - drawOffsetX)/m_getPixelTestSize) + (m_getPixelTestX&~3);
		const int mouseY = (int)floorf(((float)m_getPixelTestMousePosY - drawOffsetY)/m_getPixelTestSize) + (m_getPixelTestY&~3);

		fwBox2D selectedPixelBounds  = fwBox2D(0.0f, 0.0f, 0.0f, 0.0f);
		Color32 selectedPixelColour  = Color32(0);
		bool    selectedPixelEnabled = false;

		for (int y0 = 0; y0 < (m_getPixelTestH&~3); y0 += 4)
		{
			for (int x0 = 0; x0 < (m_getPixelTestW&~3); x0 += 4)
			{
				const int x = x0 + (m_getPixelTestX&~3);
				const int y = y0 + (m_getPixelTestY&~3);

				if (x >= w || y >= h)
				{
					continue;
				}

				Color32 block[4*4];

				if (m_getPixelTestUseOldCode) // testing 1,2,3 ..
				{
					for (int yy = 0; yy < 4; yy++)
					{
						for (int xx = 0; xx < 4; xx++)
						{
							block[xx + yy*4] = Color32(image->GetPixel(x + xx, y + yy));
						}
					}
				}
				else
				{
					image->GetPixelBlock(block, x, y);
				}

				for (int yy = 0; yy < 4; yy++)
				{
					for (int xx = 0; xx < 4; xx++)
					{
						Color32 colour = m_previewTexture->Swizzle(block[xx + yy*4]);
						Color32 alpha  = colour;

						const fwBox2D bounds(
							drawOffsetX + m_getPixelTestSize*(float)(x0 + xx + 0),
							drawOffsetY + m_getPixelTestSize*(float)(y0 + yy + 0),
							drawOffsetX + m_getPixelTestSize*(float)(x0 + xx + 1),
							drawOffsetY + m_getPixelTestSize*(float)(y0 + yy + 1)
						);

						if (m_getPixelTestSelectedPixelEnabled &&
							mouseX == (x + xx) &&
							mouseY == (y + yy))
						{
							selectedPixelBounds  = bounds;
							selectedPixelColour  = colour;
							selectedPixelEnabled = true;
						}

						colour.SetAlpha(255);
						alpha.Set(alpha.GetAlpha(), alpha.GetAlpha(), alpha.GetAlpha(), 255);

						if (1)
						{
							DTQBlitSolidColour(
								eDTQPCS_Pixels,
								bounds.x0,
								bounds.y0,
								bounds.x1,
								bounds.y1,
								0.0f,
								colour,
								true
							);
						}

						if (m_getPixelTestShowAlpha)
						{
							DTQBlitSolidColour(
								eDTQPCS_Pixels,
								bounds.x0 + m_getPixelTestSize*(float)((m_getPixelTestW&~3) + 1),
								bounds.y0,
								bounds.x1 + m_getPixelTestSize*(float)((m_getPixelTestW&~3) + 1),
								bounds.y1,
								0.0f,
								alpha,
								true
							);
						}
					}
				}
			}
		}

		image->ReleaseAlias();
		m_previewTexture->UnlockRect(lock);

		if (selectedPixelEnabled)
		{
			DTQBlitSolidColour(
				eDTQPCS_Pixels,
				selectedPixelBounds.x0 - 1.0f,
				selectedPixelBounds.y0 - 1.0f,
				selectedPixelBounds.x1 + 1.0f,
				selectedPixelBounds.y1 + 1.0f,
				0.0f,
				Color32(255,0,0,255),
				false
			);

			grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Normal);

			for (int pass = 1; pass >= 0; pass--)
			{
				const grcFont* font = grcSetup::GetMiniFixedWidthFont();
				const float x = selectedPixelBounds.x1 + 20.0f + (float)pass;
				const float y = selectedPixelBounds.y0         + (float)pass;

				grcColor(pass ? Color32(0,0,0,255) : Color32(255,255,255,255));

				grcDraw2dText(
					x,
					y,
					atVarString(
						"X=%d,Y=%d",
						mouseX,
						mouseY
					),
					true,
					1.0f, // scaleX
					1.0f, // scaleY
					font
				);

				grcDraw2dText(
					x,
					y + 8.0f,
					atVarString(
						"R=%d,G=%d,B=%d,A=%d",
						selectedPixelColour.GetRed  (),
						selectedPixelColour.GetGreen(),
						selectedPixelColour.GetBlue (),
						selectedPixelColour.GetAlpha()
					),
					true,
					1.0f, // scaleX
					1.0f, // scaleY
					font
				);
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_GETPIXELTEST

#if DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE

static grcImage* ImageConvertToA8R8G8B8(const grcImage* srcImage)
{
	grcImage* dstImage = grcImage::Create(
		srcImage->GetWidth(),
		srcImage->GetHeight(),
		srcImage->GetDepth(),
		grcImage::A8R8G8B8,
		srcImage->GetType(),
		srcImage->GetExtraMipCount(),
		srcImage->GetLayerCount() - 1
	);

	if (dstImage)
	{
		grcImage* dstLayer = dstImage;
		grcImage* srcLayer = const_cast<grcImage*>(srcImage);

		for (int layerIndex = 0; layerIndex < srcImage->GetLayerCount(); layerIndex++)
		{
			grcImage* dstMip = dstLayer;
			grcImage* srcMip = srcLayer;

			for (int mipIndex = 0; mipIndex <= srcImage->GetExtraMipCount(); mipIndex++)
			{
				const int w = srcMip->GetWidth();
				const int h = srcMip->GetHeight();
				const int d = srcMip->GetDepth();

				for (int k = 0; k < d; k++)
				{
					for (int j = 0; j < h; j += 4)
					{
						for (int i = 0; i < w; i += 4)
						{
							Color32 block[4*4];
							srcMip->GetPixelBlock(block, i, j, k);

							for (int jj = 0; jj < 4; jj++)
							{
								for (int ii = 0; ii < 4; ii++)
								{
									if (i + ii < w &&
										j + jj < h)
									{
										((Color32*)dstMip->GetBits())[(i + ii) + (j + jj)*w] = block[ii + jj*4];
									}
								}
							}
						}
					}
				}

				dstMip = dstMip->GetNext();
				srcMip = srcMip->GetNext();
			}

			dstLayer = dstLayer->GetNextLayer();
			srcLayer = srcLayer->GetNextLayer();
		}
	}

	return dstImage;
}

void CDTVState::DumpTexture(const grcTexture* texture, const grcTexture* original, bool bConvertToA8R8G8B8 WIN32PC_ONLY(, bool bIsCopy)) const
{
	ForceNoPopupsAuto nopopups(false);

	if (texture)
	{
		const grcImage::Format format = bConvertToA8R8G8B8 ? grcImage::A8R8G8B8 : (grcImage::Format)texture->GetImageFormat();

#if RSG_PC && __D3D11 // probably can't lock non-rendertarget texture .. but we can try to copy it
		if (!bIsCopy && dynamic_cast<const grcRenderTarget*>(texture) == NULL)
		{
			const u32 dxgiformat = grcTextureFactoryDX11::GetD3DFromatFromGRCImageFormat(format);

			if (AssertVerify(dxgiformat != 0))
			{
				grcTextureFactory::TextureCreateParams params(grcTextureFactory::TextureCreateParams::STAGING, grcTextureFactory::TextureCreateParams::LINEAR);
				params.MipLevels = texture->GetMipMapCount(); // why is this specified twice?
				params.LockFlags = grcsRead;
				grcTexture* temp = grcTextureFactory::GetInstance().Create(
					texture->GetWidth(),
					texture->GetHeight(),
					grcTextureFactoryDX11::TranslateToRageFormat(dxgiformat),
					NULL,
					texture->GetMipMapCount(),
					&params
				);

				if (AssertVerify(temp))
				{
					if (AssertVerify(temp->Copy(texture)))
					{
						DumpTexture(temp, texture, bConvertToA8R8G8B8, true);
					}

					temp->Release();
				}
			}
		}
		else
#endif // RSG_PC && __D3D11
		{
			grcImage* dump = NULL;
			int tileMode = -1;

#if RSG_DURANGO // TODO -- implement CreateLinearImageCopy on orbis too
			// see C:\Program Files (x86)\SCE\ORBIS SDKs\1.020\target\include_common\gpu_address.h
			// detileSurface(void *outUntiledPixels, const void *tiledPixels, const TilingParameters *tp);
			// see MeshBlendManager.cpp:
			//		sce::GpuAddress::computeTextureSurfaceOffsetAndSize(&offset,&size,srcGnmTexture,mipIdx,0);
			//		sce::GpuAddress::TilingParameters tp;
			//		tp.initFromTexture(srcGnmTexture, mipIdx, 0);
			//		sce::GpuAddress::detileSurface(pSrc, ((char*)srcGnmTexture->getBaseAddress() + offset),&tp);
			if (dynamic_cast<const grcRenderTarget*>(texture) == NULL)
			{
				tileMode = static_cast<const grcTextureDurango*>(texture)->GetPlacementTextureCellGcmTextureWrapper().GetTileMode();
				dump = static_cast<const grcTextureDurango*>(texture)->CreateLinearImageCopy();
			}
			else
#endif // RSG_DURANGO
			{
				grcImage::ImageType imageType = grcImage::STANDARD;

				if (dynamic_cast<const grcRenderTarget*>(texture))
				{
					if (texture == CRenderPhaseReflection::ms_renderTargetCube)
					{
						imageType = grcImage::CUBE;
					}
					else
					{
					// TODO -- support render targets
				}
				}
				else
				{
#if RSG_ORBIS
					imageType = (grcImage::ImageType)static_cast<const grcTextureGNM*>(texture)->GetImageType();
#elif RSG_DURANGO
					imageType = (grcImage::ImageType)static_cast<const grcTextureDurango*>(texture)->GetImageType();
#elif RSG_PC
					imageType = (grcImage::ImageType)static_cast<const grcTextureDX11*>(texture)->GetImageType();
#else
					#error 1 && "Unknown Platform"
#endif
				}

				Assertf(0, "dump texture not supported for rendertargets or pc/orbis yet");
				Assert(texture->GetArraySize() == 1); // TODO -- support texture arrays and cubemaps

				dump = grcImage::Create(
					texture->GetWidth(),
					texture->GetHeight(),
					texture->GetDepth(),
					format,
					imageType,
					texture->GetMipMapCount() - 1,
					texture->GetArraySize() - 1
				);

				grcImage* layer = dump;

				for (int layerIndex = 0; layerIndex < texture->GetArraySize(); layerIndex++)
				{
					grcImage* mip = layer;

					for (int mipIndex = 0; mipIndex < texture->GetMipMapCount(); mipIndex++)
					{
						grcTextureLock lock;
						if (AssertVerify(texture->LockRect(layerIndex, mipIndex, lock, grcsRead | grcsAllowVRAMLock)))
						{
							/*if (bConvertToA8R8G8B8)
							{
								grcImage* image = rage_new grcImage();
								image->LoadAsTextureAlias(texture, &lock);

								Color32* dst = (Color32*)mip->GetBits();

								const int w = mip->GetWidth();
								const int h = mip->GetHeight();
								const int d = mip->GetDepth();

								for (int z = 0; z < d; z++)
								{
									for (int y = 0; y < h; y += 4)
									{
										for (int x = 0; x < w; x += 4)
										{
											Color32 block[4*4];
											image->GetPixelBlock(block, x, y, z);

											for (int yy = 0; yy < 4; yy++)
											{
												for (int xx = 0; xx < 4; xx++)
												{
													if (x + xx < w &&
														y + yy < h)
													{
														dst[(x + xx) + ((y + yy) + z*h)*w] = block[xx + yy*4];
													}
												}
											}
										}
									}
								}

								image->ReleaseAlias();
							}
							else*/
							{
								sysMemCpy(mip->GetBits(), lock.Base, mip->GetSize());
							}

							texture->UnlockRect(lock);
						}

						mip = mip->GetNext();
					}

					layer = layer->GetNextLayer();
				}
			}

			if (dump)
			{
				char path[512] = "";
				strcpy(path, GetFriendlyTextureName(original).c_str());
				const char* name = strrchr(path, '/');

				if (name)
				{
					name++;
				}
				else
				{
					name = path;
				}

				const char* typeStrings[] = {"2D", "CUBE", "DEPTH", "3D"};
				Displayf("dumping texture (%s): w=%d, h=%d, d=%d, fmt=%s, type=%s, mips=%d, layers=%d, tilemode=%d",
					name,
					dump->GetWidth(),
					dump->GetHeight(),
					dump->GetDepth(),
					grcImage::GetFormatString(dump->GetFormat()),
					typeStrings[dump->GetType()],
					dump->GetExtraMipCount() + 1,
					dump->GetLayerCount(),
					tileMode
				);

				if (bConvertToA8R8G8B8 && dump->GetFormat() != grcImage::A8R8G8B8)
				{
					grcImage* temp = ImageConvertToA8R8G8B8(dump);

					if (AssertVerify(temp))
					{
						dump->Release();
						dump = temp;
					}
				}

				dump->SaveDDS(atVarString("%s/%s.dds", m_dumpTextureDir, name));
				dump->Release();
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE

void CDTVState::Cleanup()
{
	m_selectedModelInfo  = NULL;
	m_selectedShaderPtr  = 0;
	m_selectedShaderName = "";
	m_previewTxd         = NULL;
	m_previewTexture     = NULL; // because it's a pgRef

	for (int i = 0; i < m_assetRefs.size(); i++)
	{
		m_assetRefs[i].RemoveRefWithoutDelete(m_verbose);
	}

	m_assetRefs.clear();
}

#endif // __BANK
