/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    renderThread.cpp
// PURPOSE : stuff for setting up separate render thread & functions it requires
// AUTHOR :  john w.
// CREATED : 1/8/06
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _RENDERTHREAD_H_
#define _RENDERTHREAD_H_

#include "atl/functor.h"

#if __PPU
	#include "atl/queue.h"
	#include "grcore/device.h"
#endif
#include "system/ipc.h"
#include "system/system.h"
#include "grcore/stateblock.h"

#include "fwrenderer/renderthread.h"

#if __BANK
enum PtfxSafeModeOperationModeDebug
{
	PTFX_SAFE_MODE_OPERATION_DEBUG_CALLBACKEFFECTS,
	PTFX_SAFE_MODE_OPERATION_DEBUG_INSTANCE_SORTING,
	PTFX_SAFE_MODE_OPERATION_DEBUG_ALL
};
#endif

class CGtaRenderThreadGameInterface : public rage::fwRenderThreadGameInterface
{
public:
	virtual void DefaultPreRenderFunction();
	virtual void DefaultPostRenderFunction();
	virtual void NonDefaultPostRenderFunction(bool bUpdateDone);
	#if __WIN32
	virtual void FlushScaleformBuffers(void);
	#endif
	virtual void RenderThreadInit();
	virtual void RenderThreadShutdown();
	virtual void RenderFunctionChanged(RenderFn oldFn, RenderFn newFn);
	virtual void Flush();

	void PerformPtfxSafeModeOperations(BANK_ONLY(PtfxSafeModeOperationModeDebug ptfxSafeModeOperationDebug));

	virtual void PerformSafeModeOperations();

	static void InitClass();
	static void ShutdownLevel(unsigned /*shutdownMode*/);
};

class CGtaOldLoadingScreen
{
public:
	static void SetLoadingScreenIfRequired();
	static void SetLoadingRenderFunction();
	static void ForceLoadingRenderFunction(bool bForce);
//	static bool	IsUsingDefaultRenderFunction();
	static bool	IsUsingLoadingRenderFunction();

	static void Render();
	static void LoadingRenderFunction();

private:
	static bool m_bForceLoadingScreen;
	static grcDepthStencilStateHandle m_depthState;
};

#endif //_RENDERTHREAD_H_
