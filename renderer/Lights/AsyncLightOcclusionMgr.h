#ifndef ASYNC_LIGHT_OCCLUSION_MGR_H_
#define ASYNC_LIGHT_OCCLUSION_MGR_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// game 
#include "renderer/Lights/LightSource.h"

// =============================================================================================== //
// CLASS
// =============================================================================================== //

namespace CAsyncLightOcclusionMgr
{
	void Init();
	void Shutdown();

	void StartAsyncProcessing();
	void ProcessResults();

	CLightSource* AddLight();
};

#endif

