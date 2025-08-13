//
// scene/portals/FrustumDebug.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_FRUSTUM_DEBUG_H_
#define INC_FRUSTUM_DEBUG_H_

#if __BANK

// Framework headers

// Rage headers
#include "system/codecheck.h"
#include "vector/color32.h"
#include "vector/colors.h"

namespace rage
{
	class bkBank;
}

class CFrustumDebug
{
public:
	enum
	{
		FRUSTUM_LEFT, 
		FRUSTUM_RIGHT, 
		FRUSTUM_BOTTOM, 
		FRUSTUM_TOP, 
		FRUSTUM_NEAR, 
		FRUSTUM_FAR, 
		MAX_FRUSTUM_PLANES, // = 6
		MAX_FRUSTUM_CORNERS = 8
	};

	CFrustumDebug() {}
	CFrustumDebug(const grcViewport& viewport) { Set(viewport); }
	CFrustumDebug(const Matrix34& worldMatrix, const float nearClip, const float farClip, const float fov, const float screenWidth, const float screenHeight) { Set(worldMatrix, nearClip, farClip, fov, screenWidth, screenHeight); }

	void Set(const grcViewport& viewport);
	void Set(const Matrix34& worldMatrix, const float nearClip, const float farClip, const float fov, const float screenWidth, const float screenHeight);

	static void DebugDump(const CFrustumDebug* const frustum);
	static void DebugRender(const CFrustumDebug* const frustum, const Color32 colour);

	static void AddWidgets(bkBank& bk);
	void DebugRender() const;

	static void DebugDrawArrayOfLines(const u32 numLines, const Vector3* const points, const s32* const lines, const Color32 startColour = Color_white, const Color32 endColour = Color_white);
	static void DebugDrawPolys(const Vector3* const points, const s32* const idx, const u32 numPolys, const Color32 colour = Color_green, const bool shouldDrawBackFaces = true);
	static void DebugRenderPlanes(Vector4* planes, const u32 numPlanes, const Color32 colour);

private:
	void ComputeFrustumCornersDebug(Vector3 corners[MAX_FRUSTUM_CORNERS]) const;
	static FASTRETURNCHECK(bool) ComputeIntersectionOf3PlanesDebug(const Vector4& plane1, const Vector4& plane2, const Vector4& plane3, Vector3& dest);

	Vector4 m_Planes[MAX_FRUSTUM_PLANES];
};

#endif // __BANK

#endif // !defined INC_FRUSTUM_DEBUG_H_
