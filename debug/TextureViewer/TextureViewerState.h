// ========================================
// debug/textureviewer/textureviewerstate.h
// (c) 2010 RockstarNorth
// ========================================

#ifndef _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERSTATE_H_
#define _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERSTATE_H_

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

#include "atl/array.h"                               // atArray
#include "paging/ref.h"                              // pgRef
#include "fwmaths/rect.h"                            // fwBox2D
#include "debug/textureviewer/textureviewersearch.h" // CAssetRef
namespace rage { class grcTexture; }
namespace rage { class Vector3; }

// ================================================================================================

class CDTVStateCommon : public datBase
{
public:
	void UpdateCommon(const CDTVStateCommon* rhs) { *this = *rhs; }

	int     m_previewTxdGrid;
	float   m_previewTxdGridOffset;
	int     m_previewTextureViewMode; // DebugTextureViewerDTQ::eDTQViewMode
	bool    m_previewTextureForceOpaque;
	bool    m_previewTextureShowMips;
	bool    m_previewTextureFiltered;
	int     m_previewTextureLOD;
	int     m_previewTextureLayer;
	float   m_previewTextureVolumeSlice;
	int     m_previewTextureVolumeSliceNumer;
	int     m_previewTextureVolumeSliceDenom;
	int     m_previewTextureVolumeCoordSwap;
	bool    m_previewTextureCubeForceOpaque;
	bool    m_previewTextureCubeSampleAllMips;
	bool    m_previewTextureCubeReflectionMap;
	float   m_previewTextureCubeCylinderAspect;
	float   m_previewTextureCubeFaceZoom;
	int     m_previewTextureCubeFace;
	Vec3V   m_previewTextureCubeRotationYPR;
	int     m_previewTextureCubeProjection;
	bool    m_previewTextureCubeProjClip;
	bool    m_previewTextureCubeNegate;
	float   m_previewTextureCubeRefDots;
	float   m_previewTextureOffsetX;
	float   m_previewTextureOffsetY;
	float   m_previewTextureZoom;
	float   m_previewTextureBrightness;
	bool    m_previewTextureStretchToFit;
	Vector3 m_previewTextureSpecularL;
	float   m_previewTextureSpecularBumpiness;
	float   m_previewTextureSpecularExponent;
	float   m_previewTextureSpecularScale;

#if DEBUG_TEXTURE_VIEWER_HISTOGRAM
	bool    m_histogramEnabled;
	int     m_histogramShift; // 0 -> 256 buckets, 1 -> 128 buckets, 2 -> 64 buckets, etc.
	float   m_histogramScale;
	int     m_histogramAlphaMin; // [0..255]
	int     m_histogramBucketMin; // min bucket value to affect range
	int     m_histogramOutliers;
#endif // DEBUG_TEXTURE_VIEWER_HISTOGRAM

#if DEBUG_TEXTURE_VIEWER_GETPIXELTEST
	bool    m_getPixelTest;
	bool    m_getPixelTestUseOldCode;
	bool    m_getPixelTestShowAlpha;
	int     m_getPixelMipIndex;
	int     m_getPixelTestX;
	int     m_getPixelTestY;
	int     m_getPixelTestW;
	int     m_getPixelTestH;
	float   m_getPixelTestSize;
	bool    m_getPixelTestSelectedPixelEnabled;
	int     m_getPixelTestMousePosX;
	int     m_getPixelTestMousePosY;
#endif // DEBUG_TEXTURE_VIEWER_GETPIXELTEST

#if DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
	char    m_dumpTextureDir[RAGE_MAX_PATH];
	bool    m_dumpTexture;
	bool    m_dumpTextureRaw;
	bool    m_dumpTxdRaw;

	void StartDumpTexture()    { m_dumpTexture = true; }
	void StartDumpTextureRaw() { m_dumpTextureRaw = true; }
	void StartDumpTxdRaw()     { m_dumpTxdRaw = true; }
#endif // DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
};

class CDTVState : public CDTVStateCommon
{
public:
	CDTVState();
	~CDTVState();

	void Render(const grcTexture* texture, int viewMode, const fwBox2D& dstBounds_, const fwBox2D& srcBounds) const;
	void Render() const;
#if DEBUG_TEXTURE_VIEWER_HISTOGRAM
	void RenderHistogram() const;
#endif // DEBUG_TEXTURE_VIEWER_HISTOGRAM
#if DEBUG_TEXTURE_VIEWER_GETPIXELTEST
	void RenderGetPixelTest() const;
#endif // DEBUG_TEXTURE_VIEWER_GETPIXELTEST
#if DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE
	void DumpTexture(const grcTexture* texture, const grcTexture* original, bool bConvertToA8R8G8B8 WIN32PC_ONLY(, bool bIsCopy)) const;
	void DumpTexture(const grcTexture* texture, bool bConvertToA8R8G8B8) const { DumpTexture(texture, texture, bConvertToA8R8G8B8 WIN32PC_ONLY(, false)); }
#endif // DEBUG_TEXTURE_VIEWER_DUMP_TEXTURE

	void Cleanup(); // remove refs, etc.

	fwBox2D                 m_previewSrcBounds; // these must be aligned, keep them here to stop the compiler from bitching
	fwBox2D                 m_previewDstBounds;
	bool                    m_verbose;
	const CBaseModelInfo*   m_selectedModelInfo;  // so it can be accessed by the renderthread
	u32                     m_selectedShaderPtr;  // so it can be accessed by the renderthread
	atString                m_selectedShaderName; // so it can be accessed by the renderthread
	pgRef<const fwTxd>      m_previewTxd;
	pgRef<const grcTexture> m_previewTexture;
	atArray<CAssetRef>      m_assetRefs;
};

#endif // __BANK
#endif // _DEBUG_TEXTUREVIEWER_TEXTUREVIEWERSTATE_H_
