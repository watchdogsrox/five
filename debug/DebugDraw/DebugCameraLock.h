// =================================
// debug/DebugDraw/DebugCameraLock.h
// (c) 2011 RockstarNorth
// =================================

#ifndef _DEBUG_DEBUGDRAW_DEBUGCAMERALOCK_H_
#define _DEBUG_DEBUGDRAW_DEBUGCAMERALOCK_H_

#if __BANK

#include "vectormath/classes.h"

namespace rage { class grcViewport; }
namespace rage { class bkBank; }

class CDebugCameraLock //: public datBase
{
public:
	void InitWidgets(bkBank& bk);

	bool Update(const grcViewport* vp, ScalarV_In z0, ScalarV_In z1);
	void DebugDraw() const;

	Mat34V_Out GetViewToWorld() const;
	ScalarV_Out GetTanHFOV() const;
	ScalarV_Out GetTanVFOV() const;
	ScalarV_Out GetZ0() const;
	ScalarV_Out GetZ1() const;

private:
	void UpdateCameraLocked();
	void Reset();

	Mat34V  m_viewToWorld_camera; // updated from viewport camera, never locked
	Mat34V  m_viewToWorld_locked;
	Mat34V  m_viewToWorld; // locked and adjusted
	ScalarV m_tanHFOV;
	ScalarV m_tanVFOV;
	ScalarV m_z0;
	ScalarV m_z1;

	float   m_opacity;
	bool    m_locked;
	float   m_yaw;
	float   m_pitch;
	float   m_roll;
	Vec4V   m_scale; // z scales both x and y, w scales far clip
	Vec3V   m_localOffset;
	Vec3V   m_worldOffset;
	bool    m_kDOP;
	bool    m_psdd;
	bool    m_psddSweepEnabled;
	Vec3V   m_psddSweep;
	u32     m_psddFlags;
};

#endif // __BANK
#endif // _DEBUG_DEBUGDRAW_DEBUGCAMERALOCK_H_
