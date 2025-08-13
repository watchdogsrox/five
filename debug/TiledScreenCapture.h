// ==========================
// debug/TiledScreenCapture.h
// (c) 2013 RockstarNorth
// ==========================

#ifndef _DEBUG_TILEDSCREENCAPTURE_H_
#define _DEBUG_TILEDSCREENCAPTURE_H_

#if __BANK

namespace rage { class bkBank; }
namespace rage { class grcViewport; }
namespace rage { class spdAABB; }

namespace TiledScreenCapture
{
	void AddWidgets(bkBank& bk);
	bool IsEnabled();
	bool IsEnabledOrthographic();
	void SetCurrentPVSSize(int currentPVSSize);
	void GetTileBounds(spdAABB& tileBounds, bool bForShadows = false);
	float GetSSAOScale();
	const grcTexture* GetWaterReflectionTexture();
	void UpdateViewportFrame(grcViewport& vp);
	bool UpdateViewportPerspective(grcViewport& vp, float fov, float aspect, float nearClip, float farClip);
	void Update();
	void BuildDrawList();
}

#endif // __BANK
#endif // _DEBUG_TILEDSCREENCAPTURE_H_
