// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage

// framework

// game
#include "Debug/Rendering/DebugCamera.h"
#include "camera/viewports/ViewportManager.h"

#if __BANK

// =============================================================================================== //
// DEFINES
// =============================================================================================== //

RENDER_OPTIMISATIONS()

bool     DebugCamera::m_cameraLocked;
Matrix34 DebugCamera::m_cameraLockMatrix;

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

void DebugCamera::SetCameraLock(bool bLock)
{
	if (bLock)
	{
		m_cameraLockMatrix = RCC_MATRIX34(gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraMtx());
	}

	m_cameraLocked = bLock;
}

bool DebugCamera::IsCameraLocked()
{
	return m_cameraLocked;
}

const Matrix34& DebugCamera::GetCameraLockMatrix(const Matrix34* cam)
{
	return m_cameraLocked ? m_cameraLockMatrix : (cam ? *cam : RCC_MATRIX34(gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraMtx()));
}

#endif