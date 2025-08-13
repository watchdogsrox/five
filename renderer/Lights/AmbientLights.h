#ifndef AMBIENT_LIGHTS_H_
#define AMBIENT_LIGHTS_H_

#include "grcore/array.h"

#include "timecycle/TimeCycle.h"

class AmbientLights
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();
#if __BANK
	static u32	  GetRenderedCount() { return sm_renderedCount; }
#endif // __BANK
private:
	static grcDepthStencilStateHandle sm_DSState;
	static grcRasterizerStateHandle sm_RState;
	static grcBlendStateHandle sm_BState;
	
	static float sm_pullBack;
	static float sm_pullFront;
	static float sm_falloff;
	static float sm_ebPullBack;
	static float sm_ebPullFront;
	static float sm_ebFalloff;
	
#if __BANK
	static unsigned int sm_renderedCount;
#endif // __BANK
};

#endif // AMBIENT_LIGHTS_H_

