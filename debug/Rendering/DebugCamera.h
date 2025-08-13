#ifndef DEBUG_CAMERA_H_
#define DEBUG_CAMERA_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// game
#include "Debug/Rendering/DebugRendering.h"

// rage
#include "vector/matrix34.h"

#if __BANK

// =============================================================================================== //
// CLASS
// =============================================================================================== //

class DebugCamera : public DebugRendering
{
public:

	// Functions
	static void SetCameraLock(bool bLock);
	static bool IsCameraLocked();
	static const Matrix34& GetCameraLockMatrix(const Matrix34* cam = NULL);

	// Variables
	static bool     m_cameraLocked;
	static Matrix34 m_cameraLockMatrix;
};

#endif

#endif
