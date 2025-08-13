// ===================================
// debug/DebugDraw/DebugCameraLock.cpp
// (c) 2011 RockstarNorth
// ===================================

#if __BANK

#include "bank/bank.h"
#include "grcore/debugdraw.h"
#include "spatialdata/transposedplaneset.h"
#include "system/memory.h"
#include "system/nelem.h"
#include "vector/matrix33.h" // old shite

#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

#include "camera/viewports/ViewportManager.h"
#include "debug/DebugDraw/DebugVolume.h"
#include "debug/DebugDraw/DebugCameraLock.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

void CDebugCameraLock::InitWidgets(bkBank& bk)
{
	Reset();

	bk.AddSlider("Opacity"     , &m_opacity, 0.0f, 1.0f, 1.0f/32.0f);
	bk.AddToggle("Locked"      , &m_locked);//, datCallback(MFA(CDebugCameraLock::UpdateCameraLocked), this));
	bk.AddAngle ("Yaw"         , &m_yaw, bkAngleType::DEGREES, 0.0f, 360.0f);
	bk.AddAngle ("Pitch"       , &m_pitch, bkAngleType::DEGREES, 0.0f, 360.0f);
	bk.AddAngle ("Roll"        , &m_roll, bkAngleType::DEGREES, 0.0f, 360.0f);
	bk.AddVector("Scale"       , &m_scale, 0.125f, 8.0f, 1.0f/32.0f);
	bk.AddVector("Local Offset", &m_localOffset, -100.0f, 100.0f, 1.0f/32.0f);
	bk.AddVector("World Offset", &m_worldOffset, -100.0f, 100.0f, 1.0f/32.0f);
	bk.AddToggle("Show kDOP"   , &m_kDOP);

	bk.AddSeparator();

	bk.AddToggle("Show Plane Set Debug Draw"   , &m_psdd);
	bk.AddToggle("Show Plane Set Sweep Enabled", &m_psddSweepEnabled);
	bk.AddVector("Show Plane Set Sweep"        , &m_psddSweep, -100.0f, 100.0f, 1.0f/4.0f);
	bk.AddToggle("Show Plane Set Sweep Points" , &m_psddFlags, BIT(0x12));

	for (int i = 0; i < 18; i++)
	{
		switch (i)
		{
		case  0: bk.AddTitle("Silhouette Group A"); break;
		case  4: bk.AddTitle("Silhouette Group B"); break;
		case  8: bk.AddTitle("Endcap Group C"); break;
		case 13: bk.AddTitle("Endcap Group D"); break;
		}

		bk.AddToggle(atVarString("Show Plane Set Sweep Flag %d", i).c_str(), &m_psddFlags, BIT(i));
	}
}

bool CDebugCameraLock::Update(const grcViewport* vp, ScalarV_In z0, ScalarV_In z1)
{
	if (vp == NULL)
	{
		vp = &gVpMan.GetCurrentViewport()->GetGrcViewport();
	}

	m_viewToWorld_camera = vp->GetCameraMtx();

	if (!m_locked)
	{
		m_viewToWorld_locked = m_viewToWorld_camera;
	}

	m_viewToWorld = m_viewToWorld_locked;
	m_tanHFOV     = ScalarV(vp->GetTanHFOV());
	m_tanVFOV     = ScalarV(vp->GetTanVFOV());
	m_z0          = z0;
	m_z1          = z1;

	// adjust
	{
		if (m_yaw != 0.0f)
		{
			Matrix33 rot;
			rot.MakeRotateZ(-DtoR*m_yaw);
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol0Ref()));
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol1Ref()));
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol2Ref()));
		}

		if (m_pitch != 0.0f)
		{
			Matrix33 rot;
			rot.MakeRotate(VEC3V_TO_VECTOR3(m_viewToWorld.GetCol0()), DtoR*m_pitch);
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol0Ref()));
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol1Ref()));
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol2Ref()));
		}

		if (m_roll != 0.0f)
		{
			Matrix33 rot;
			rot.MakeRotate(VEC3V_TO_VECTOR3(m_viewToWorld.GetCol2()), -DtoR*m_roll);
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol0Ref()));
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol1Ref()));
			rot.Transform(RC_VECTOR3(m_viewToWorld.GetCol2Ref()));
		}

		m_viewToWorld.GetCol3Ref() += Transform3x3(m_viewToWorld, m_localOffset*Vec3V(1.0f, 1.0f, -1.0f));
		m_viewToWorld.GetCol3Ref() += m_worldOffset;

		m_tanHFOV *= m_scale.GetX()*m_scale.GetZ();
		m_tanVFOV *= m_scale.GetY()*m_scale.GetZ();

		m_z1 *= m_scale.GetW();
	}

	return m_locked;
}

void CDebugCameraLock::DebugDraw() const
{
	USE_DEBUG_MEMORY();

	if (m_opacity > 0.0f)
	{
		if (MagSquared(m_viewToWorld.GetCol3() - m_viewToWorld_camera.GetCol3()).Getf() <= 0.01f*0.01f)
		{
			return; // hacky way to prevent the camera volume from being drawn when it is right in your eye
		}

		CDebugVolume* vol = NULL;

		if (1) // use custom colourstyle that works better for daylight situations
		{
			static CDebugVolume* s_debugVolume = NULL;

			if (s_debugVolume == NULL)
			{
				s_debugVolume = rage_new CDebugVolume();

				s_debugVolume->m_faceColourStyle.Set(
					Vector4(0.0f, 0.0f, 0.0f, 0.250f), // colour
					Vector4(0.0f, 0.0f, 0.5f, 0.125f), // colourGlancing
					Vector4(4.0f, 4.0f, 4.0f, 4.000f), // colourExponent
					Vector4(0.0f, 0.0f, 0.0f, 0.095f)  // colourBackface
				);
				s_debugVolume->m_gridColourStyle.Set(
					Vector4(1.0f, 1.0f, 1.0f, 0.500f), // colour
					Vector4(0.0f, 0.0f, 1.0f, 0.500f), // colourGlancing
					Vector4(2.0f, 2.0f, 2.0f, 2.000f), // colourExponent
					Vector4(0.5f, 0.5f, 1.0f, 0.125f)  // colourBackface
				);
				s_debugVolume->m_edgeColour           = CDebugVolumeColour(Vector4(0.0f, 0.0f, 0.0f, 0.33f));
				s_debugVolume->m_edgeColourBackface   = CDebugVolumeColour(Vector4(0.0f, 0.0f, 1.0f, 0.22f));
				s_debugVolume->m_edgeColourSilhouette = CDebugVolumeColour(Vector4(0.0f, 0.0f, 0.0f, 1.00f));
			}

			vol = s_debugVolume;
			vol->ClearGeometry();
		}
		else
		{
			vol = &CDebugVolume::GetGlobalDebugVolume(true);
		}

		vol->SetFrustum(RCC_MATRIX34(m_viewToWorld), m_z0.Getf(), m_z1.Getf(), m_tanVFOV.Getf(), m_tanHFOV.Getf()/m_tanVFOV.Getf());

		if (m_kDOP)
		{
			vol->ConvertToKDOP18(0.0f, 0.0000001f);
		}

		if (m_psdd)
		{
			if (IsZeroAll(m_psddSweep) || !m_psddSweepEnabled)
			{
				Vec4V planes[6];

				if (AssertVerify(vol->m_faceCount <= NELEM(planes)))
				{
					for (int i = 0; i < vol->m_faceCount; i++)
					{
						planes[i] = -RCC_VEC4V(vol->m_faces[i].m_plane);
					}

					PlaneSetDebugDraw(planes, vol->m_faceCount, Color32(255,0,0,(u8)(0.5f + 255.0f*m_opacity)));
				}
			}
			else
			{
				int actualPlaneCount = 0;
				Vec4V extraPlanes[16];
				const spdTransposedPlaneSet8 planeSet = GenerateSweptFrustum(m_viewToWorld, m_tanHFOV, m_tanVFOV, m_z1, m_psddSweep, &actualPlaneCount, extraPlanes, m_psddFlags);

				Vec4V planes[32];
				planeSet.GetPlanes(planes);

				for (int i = 8; i < actualPlaneCount; i++)
				{
					planes[i] = extraPlanes[i - 8];
				}

				PlaneSetDebugDraw(planes, actualPlaneCount, Color32(255,0,0,(u8)(0.5f + 255.0f*m_opacity)));
				grcDebugDraw::AddDebugOutput("actual plane count = %d", actualPlaneCount);
			}
		}
		else
		{
			grcDebugDraw::SetDoDebugZTest(true);

			vol->Draw(VEC3V_TO_VECTOR3(m_viewToWorld_camera.GetCol3()), VEC3V_TO_VECTOR3(m_viewToWorld_camera.GetCol2()), m_opacity);
		}
	}
}

Mat34V_Out CDebugCameraLock::GetViewToWorld() const
{
	return m_viewToWorld;
}

ScalarV_Out CDebugCameraLock::GetTanHFOV() const
{
	return m_tanHFOV;
}

ScalarV_Out CDebugCameraLock::GetTanVFOV() const
{
	return m_tanVFOV;
}

ScalarV_Out CDebugCameraLock::GetZ0() const
{
	return m_z0;
}

ScalarV_Out CDebugCameraLock::GetZ1() const
{
	return m_z1;
}

void CDebugCameraLock::Reset()
{
	m_viewToWorld_camera  = Mat34V(V_IDENTITY);
	m_viewToWorld_locked  = Mat34V(V_IDENTITY);
	m_viewToWorld         = Mat34V(V_IDENTITY);
	m_tanHFOV             = ScalarV(V_ONE);
	m_tanVFOV             = ScalarV(V_ONE);
	m_z0                  = ScalarV(V_ZERO);
	m_z1                  = ScalarV(V_ONE);

	m_opacity             = 1.0f;
	m_locked              = false;
	m_yaw                 = 0.0f;
	m_pitch               = 0.0f;
	m_roll                = 0.0f;
	m_scale               = Vec4V(V_ONE);
	m_localOffset         = Vec3V(V_ZERO);
	m_worldOffset         = Vec3V(V_ZERO);
	m_kDOP                = false;
	m_psdd                = false;
	m_psddSweepEnabled    = false;
	m_psddSweep           = Vec3V(V_ZERO);
	m_psddFlags           = 0x0003ffff;
}

#endif // __BANK
