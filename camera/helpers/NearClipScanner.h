//
// camera/helpers/NearClipScanner.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef NEAR_CLIP_SCANNER_H
#define NEAR_CLIP_SCANNER_H

#include "physics/shapetest.h"

#include "camera/base/BaseObject.h"
#include "camera/helpers/Frame.h"
#include "atl/string.h"

const u32 g_NumBoundVerts = 8;

class camBaseCamera;
class camNearClipScannerMetadata;

class camNearClipScanner : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camNearClipScanner, camBaseObject);

public:
	camNearClipScanner(const camBaseObjectMetadata& metadata);

	float	ComputeMaxNearClipAtPosition(const Vector3& position, bool shouldConsiderFov = false, float fov = g_DefaultFov) const;

	void	Update(const camBaseCamera* activeCamera, camFrame& sourceFrame);
#if __BANK
	virtual void AddWidgetsForInstance();
	float		GetNearClip() const { return m_DebugNearClip; }
	const char *GetNearClipReason() const { return m_DebugNearClipReason.c_str(); }
	Vec3V_Out	GetNearClipPosition() const { return m_DebugNearClipPosition; }
#endif // __BANK

private:
	float	ComputeMaxSafeNearClipUsingQuadTest(const camBaseCamera* activeCamera, const camFrame& frame);
	bool	ComputeIsClippingWater(const Matrix34& worldMatrix, const Vector3* verts) const;
	void	UpdateExcludeInstances();
#if __BANK
	void	DebugRender();
#endif // __BANK

	static bool InstanceRejectCallback(const phInst* instance);

	const camNearClipScannerMetadata& m_Metadata;

	phShapeTest<phShapeScalingSweptQuad> m_ScaledQuadTest;

#if __BANK
	camFrame	m_DebugFrameCache;
	Vector3		m_DebugBoundVerts[g_NumBoundVerts];
	u32			m_DebugNearClipAlpha;
	bool		m_ShouldDebugUseGameplayFrame;
	bool		m_ShouldDebugRenderNearClip;
	bool		m_ShouldDebugRenderCollisionFrustum;
	float		m_DebugNearClip;
	atString	m_DebugNearClipReason;
	Vec3V		m_DebugNearClipPosition;
#endif // __BANK
};

#endif // NEAR_CLIP_SCANNER_H
