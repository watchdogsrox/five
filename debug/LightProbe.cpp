// ===========================
// debug/LightProbe.cpp
// (c) 2013 Rockstar San Diego
// ===========================

#if __BANK

#include "atl/array.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "file/asset.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/effect_typedefs.h"
#include "grcore/image.h"
#include "grcore/quads.h"
#include "grcore/stateblock.h"
#include "grcore/texture.h"
#include "grmodel/shader.h"
#include "math/float16.h"
#include "system/memops.h"
#include "vectormath/classes.h"

#include "fwutil/popups.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/LightProbe.h"
#include "debug/TextureViewer/TextureViewer.h" // for setting preview texture
#include "debug/TextureViewer/TextureViewerDTQ.h" // for eDTQCubeFace
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/rendertargets.h"
#include "streaming/streaming.h"

#define LIGHT_PROBE_SPHERE_RENDER (1 && __DEV)
#define LIGHT_PROBE_SCENE_CUBEMAP (1)// && __DEV)

namespace LightProbe {

enum eLightProbeSaveAs
{
	LP_SAVE_AS_FACES,
	LP_SAVE_AS_4x3_CROSS,
	LP_SAVE_AS_CUBEMAP,
	LP_SAVE_AS_PANORAMA,
	LP_SAVE_AS_COUNT
};

static const char* g_lightProbeSaveAsStrings[] =
{
	"Faces",
	"4x3 Cross",
	"Cubemap",
	"Panorama",
};
CompileTimeAssert(NELEM(g_lightProbeSaveAsStrings) == LP_SAVE_AS_COUNT);

static bool           g_capture = false;
static float          g_skyHDRExponent = 0.0f;
static float          g_brightnessScale = 1.0f; // this gets set from g_brightnessExponent
static float          g_brightnessExponent = 0.0f;
static int            g_saveAs = LP_SAVE_AS_CUBEMAP;
static float          g_debugDrawOpacity = LIGHT_PROBE_SPHERE_RENDER ? 0.0f : 1.0f;
static atArray<Vec4V> g_debugDrawSpheres;
bool                  g_capturingPanorama = false; // turns off things like the bar, hud, lens flares etc.

#if LIGHT_PROBE_SPHERE_RENDER
static float              g_lightProbeOpacity = 1.0f;
static int                g_lightProbeMipIndex = -1;
static float              g_lightProbeReflectionAmount = 1.0f;
static Vec4V              g_lightProbeSphere = Vec4V(V_ZERO);
static float              g_lightProbeYaw = 0.0f;
static float              g_lightProbePitch = 0.0f;
static float              g_lightProbeRoll = 0.0f;
static grcTexture*        g_lightProbeTexture = NULL;
static grmShader*         g_lightProbeShader = NULL;
static grcEffectTechnique g_lightProbeShaderTechnique = grcetNONE;
static grcEffectVar       g_lightProbeShaderRotation = grcevNONE;
static grcEffectVar       g_lightProbeShaderSphere = grcevNONE;
static grcEffectVar       g_lightProbeShaderParams = grcevNONE;
static grcEffectVar       g_lightProbeShaderTexture = grcevNONE;
static char               g_lightProbePanoramaPath[256] = "";
static int                g_lightProbePanoramaFrame = 0;
static grcRenderTarget*   g_lightProbePanoramaRT = NULL;
#endif // LIGHT_PROBE_SPHERE_RENDER

} // namespace LightProbe

// ================================================================================================

// i'm leaving this system off by default, as it currently forces the reflection cubemap to use a more expensive format with fewer mips
PARAM(lightprobe,"");
bool LightProbe::IsEnabled()
{
	return PARAM_lightprobe.Get();
}

bool LightProbe::IsCapturingPanorama()
{
	return g_capturingPanorama;
}

void LightProbe::CreateRenderTargetsAndInitShader()
{
#if LIGHT_PROBE_SPHERE_RENDER
	if (IsEnabled())
	{
		ASSET.PushFolder("common:/shaders");
		g_lightProbeShader = grmShaderFactory::GetInstance().Create();
		g_lightProbeShader->Load("debug_lightprobe");
		g_lightProbeShaderTechnique = g_lightProbeShader->LookupTechnique("lightprobe");
		g_lightProbeShaderRotation = g_lightProbeShader->LookupVar("lightProbeRotation");
		g_lightProbeShaderSphere = g_lightProbeShader->LookupVar("lightProbeSphere");
		g_lightProbeShaderParams = g_lightProbeShader->LookupVar("lightProbeParams");
		g_lightProbeShaderTexture = g_lightProbeShader->LookupVar("lightProbeTexture");
		ASSET.PopFolder();

		grcTextureFactory::CreateParams params;
		params.Format = grctfA32B32G32R32F;
		params.Lockable = true;

		const grcRenderTargetType type = grcrtPermanent;
		const RTMemPoolID         pool = RTMEMPOOL_NONE;
		const u8                  heap = 0;
		const int                 bpp  = 128;

		const int w = 2048;
		const int h = w/2;

		g_lightProbePanoramaRT = CRenderTargets::CreateRenderTarget(
			pool,
			"REFLECTION_PANORAMA",
			type,
			w,
			h,
			bpp,
			&params,
			heap
		);
	}
#endif // LIGHT_PROBE_SPHERE_RENDER
}

static void CopyCubemapFaceSetup(int& ox, int& oy, int& dxx, int& dxy, int& dyx, int& dyy, int faceSize, int faceIndex)
{
	//         +-------+
	//         |face:5 |
	//         |dir=-z |
	//         |rot=0  |
	// +-------+-------+-------+-------+
	// |face:0 |face:3 |face:1 |face:2 |
	// |dir=+x |dir=-y |dir=-x |dir=+y |
	// |rot=270|rot=180|rot=90 |rot=0  |
	// +-------+-------+-------+-------+
	//         |face:4 |
	//         |dir=+z |
	//         |rot=180|
	//         +-------+

	switch (faceIndex)
	{
	case 0: ox = 0*faceSize,     oy = 2*faceSize - 1, dxx = +0, dxy = -1, dyx = +1, dyy = +0; return;
	case 1: ox = 3*faceSize - 1, oy = 1*faceSize,     dxx =  0, dxy = +1, dyx = -1, dyy =  0; return;
	case 2: ox = 3*faceSize,     oy = 1*faceSize,     dxx = +1, dxy =  0, dyx =  0, dyy = +1; return;
	case 3: ox = 2*faceSize - 1, oy = 2*faceSize - 1, dxx = -1, dxy =  0, dyx =  0, dyy = -1; return;
	case 4: ox = 2*faceSize - 1, oy = 3*faceSize - 1, dxx = -1, dxy =  0, dyx =  0, dyy = -1; return;
	case 5: ox = 1*faceSize,     oy = 0*faceSize,     dxx = +1, dxy =  0, dyx =  0, dyy = +1; return;
	}

	Assert(false);
}

template <typename T> static void CopyFromCubemapFaceTo4x3Cross(T* crossImage, const T* faceImage, int faceSize, int faceIndex)
{
	const int w = faceSize*4;
	int ox, oy, dxx, dxy, dyx, dyy;
	CopyCubemapFaceSetup(ox, oy, dxx, dxy, dyx, dyy, faceSize, faceIndex);

	for (int y = 0; y < faceSize; y++)
	{
		for (int x = 0; x < faceSize; x++)
		{
			const int xx = ox + x*dxx + y*dyx;
			const int yy = oy + x*dxy + y*dyy;

			crossImage[xx + yy*w] = faceImage[x + y*faceSize];
		}
	}
}

template <typename T> static void CopyToCubemapFaceFrom4x3Cross(const T* crossImage, T* faceImage, int faceSize, int faceIndex)
{
	const int w = faceSize*4;
	int ox, oy, dxx, dxy, dyx, dyy;
	CopyCubemapFaceSetup(ox, oy, dxx, dxy, dyx, dyy, faceSize, faceIndex);

	for (int y = 0; y < faceSize; y++)
	{
		for (int x = 0; x < faceSize; x++)
		{
			const int xx = ox + x*dxx + y*dyx;
			const int yy = oy + x*dxy + y*dyy;

			faceImage[x + y*faceSize] = crossImage[xx + yy*w];
		}
	}
}

static void ScaleImageBrightness(grcImage* image, float scale)
{
	if (scale == 0.0f)
	{
		sysMemSet(image->GetBits(), 0, image->GetSize());
	}
	else if (scale != 1.0f)
	{
		if (image->GetFormat() == grcImage::A32B32G32R32F ||
			image->GetFormat() == grcImage::G32R32F ||
			image->GetFormat() == grcImage::R32F)
		{
			float* ptr = (float*)image->GetBits();
			float* end = (float*)((u8*)ptr + image->GetSize());

			while (ptr < end)
			{
				*(ptr++) *= scale;
			}
		}

		if (image->GetFormat() == grcImage::A16B16G16R16F ||
			image->GetFormat() == grcImage::G16R16F ||
			image->GetFormat() == grcImage::R16F)
		{
			Float16* ptr = (Float16*)image->GetBits();
			Float16* end = (Float16*)((u8*)ptr + image->GetSize());

			while (ptr < end)
			{
				ptr->SetFloat16_FromFloat32(ptr->GetFloat32_FromFloat16()*scale);
				ptr++;
			}
		}
		else
		{
			return; // format not supported for scale
		}
	}
	else
	{
		return;
	}

	if (image->GetNext())
	{
		ScaleImageBrightness(image->GetNext(), scale);
	}

	if (image->GetNextLayer())
	{
		ScaleImageBrightness(image->GetNextLayer(), scale);
	}
}

#if LIGHT_PROBE_SCENE_CUBEMAP

enum CubemapCaptureState
{
	CCS_Idle,					// Capture not active
	CCS_SetCamera,				// Setting the camera to the appropriate face
	CCS_WaitStreamIn,			// Waiting for streaming to complete
	CCS_Capturing,				// Waiting on render thread to capture
	CCS_GameTick,				// Performing normal game tick
	CCS_PostGameWait,			// Wait for game to settle after the tick
};

static int                 g_sceneCubemapFaceIndex        = -1;
static int                 g_sceneCubemapFaceAdvanceTimer = 0;
static grcImage*           g_sceneCubemap4x3Cross[2]      = {NULL, NULL};
static grcImage*           g_sceneCubemap[2]              = {NULL, NULL};
static bool                g_sceneCubemapCapture          = false;
static int                 g_cubemapIndex                 = 0;
static int                 g_frameWait                    = 0;
static CubemapCaptureState g_cubemapCaptureState          = CCS_Idle;
static Mat34V              g_camBaseMatrix                = Mat34V(V_IDENTITY);
static bool                g_continualCapture             = true;
static float               g_continualCaptureFPS          = 30.0f;
static int                 g_continualCaptureFrameDelay   = 1;
#if GTA_REPLAY
static bool				   g_FinishedReplayTick = false;
#endif

static void SetCamera_internal(DebugTextureViewerDTQ::eDTQCubeFace faceIndex)
{
	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		camFrame& cam = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();

		cam.SetFov(90.0f);

		float pitch=0.0f, roll=0.0f, heading=0.0f;
		switch ((int)faceIndex)
		{
		case DebugTextureViewerDTQ::eDTQCF_PZ_UP   : heading = 0.0f;    pitch = 90.0f;  break; // up
		case DebugTextureViewerDTQ::eDTQCF_NZ_DOWN : heading = 0.0f;    pitch = -90.0f; break; // down
		case DebugTextureViewerDTQ::eDTQCF_NX_LEFT : heading = 90.0f;   pitch = 0.0f;   break; // left
		case DebugTextureViewerDTQ::eDTQCF_PX_RIGHT: heading = -90.0f;  pitch = 0.0f;   break; // right
		case DebugTextureViewerDTQ::eDTQCF_PY_FRONT: heading = 0.0f;    pitch = 0.0f;   break; // front
		case DebugTextureViewerDTQ::eDTQCF_NY_BACK : heading = -180.0f; pitch = 0.0f;   break; // back
		}

		Mat33V cubemapFaceMatrix;
		const Vec3V eulers(pitch*DtoR, roll*DtoR, heading*DtoR);
		Mat33VFromEulersYXZ(cubemapFaceMatrix, eulers); //NOTE: YXZ rotation ordering is used to avoid aliasing of the roll.

		// Camera = heading * cubemap rotation
		Mat33V worldMatrix;
		Multiply(worldMatrix, g_camBaseMatrix.GetMat33(), cubemapFaceMatrix);

		// Convert to 3x4 matrix and apply to camera
		Mat34V worldMatrix34(worldMatrix, g_camBaseMatrix.GetCol3());
		cam.SetWorldMatrix(RCC_MATRIX34(worldMatrix34));
	}
}

template <DebugTextureViewerDTQ::eDTQCubeFace faceIndex> static void SetCamera_button()
{
	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		g_camBaseMatrix = Mat34V(V_IDENTITY);
		SetCamera_internal(faceIndex);
		g_sceneCubemapFaceIndex = faceIndex;
	}
	else
	{
		Displayf("SetCamera_button: failed, need to activate free cam first!");
	}
}

static void CaptureSceneCubemapFace_button()
{
	if (g_sceneCubemapFaceIndex != -1)
	{
		g_sceneCubemapCapture = true;
	}
}

static void CaptureSceneCubemapFace_internal(grcImage*& cross, grcImage*& cubemap, const void* frameData, int w, int h, grcImage::Format format)
{
	const int faceSize = h;
	const int bpp = grcImage::GetFormatBitsPerPixel(format);

	if (cross == NULL)
	{
		USE_DEBUG_MEMORY();
		cross = grcImage::Create(faceSize*4, faceSize*3, 1, format, grcImage::STANDARD, 0, 0);
		sysMemSet(cross->GetBits(), 0, cross->GetSize());
	}

	if (cubemap == NULL)
	{
		USE_DEBUG_MEMORY();
		cubemap = grcImage::Create(faceSize, faceSize, 1, format, grcImage::CUBE, 0, 5);
	}

	//         +-------+
	//         |       |
	//         |  UP   |
	//         |       |
	// +-------+-------+-------+-------+
	// |       |       |       |       |
	// | LEFT  | FRONT | RIGHT | BACK  |
	// |       |       |       |       |
	// +-------+-------+-------+-------+
	//         |       |
	//         | DOWN  |
	//         |       |
	//         +-------+

	int xo = 0;
	int yo = 0;

	switch (g_sceneCubemapFaceIndex)
	{
	case DebugTextureViewerDTQ::eDTQCF_PZ_UP   : xo = 1; yo = 0; break;
	case DebugTextureViewerDTQ::eDTQCF_NZ_DOWN : xo = 1; yo = 2; break;
	case DebugTextureViewerDTQ::eDTQCF_NX_LEFT : xo = 0; yo = 1; break;
	case DebugTextureViewerDTQ::eDTQCF_PX_RIGHT: xo = 2; yo = 1; break;
	case DebugTextureViewerDTQ::eDTQCF_PY_FRONT: xo = 1; yo = 1; break;
	case DebugTextureViewerDTQ::eDTQCF_NY_BACK : xo = 3; yo = 1; break;
	}

	const int crossWidth = cross->GetWidth();
	u8* dst = (u8*)cross->GetBits() + (xo + yo*crossWidth)*faceSize*bpp/8;
	u8* src = (u8*)frameData + ((w - h)/2)*bpp/8;

	for (int j = 0; j < faceSize; j++)
	{
		sysMemCpy(dst, src, faceSize*bpp/8);
		dst += crossWidth*bpp/8;
		src += w*bpp/8;
	}
}

static void CaptureSceneCubemapFace(grcImage*& cross, grcImage*& cubemap, const grcTexture* frame)
{
	if (frame)
	{
		grcTextureLock lock;
		if (AssertVerify(frame->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock)))
		{
			CaptureSceneCubemapFace_internal(cross, cubemap, lock.Base, frame->GetWidth(), frame->GetHeight(), (grcImage::Format)frame->GetImageFormat());
			frame->UnlockRect(lock);
		}
	}
	else
	{
		USE_DEBUG_MEMORY();
		grcImage* screen = GRCDEVICE.CaptureScreenShot();
		if (AssertVerify(screen))
		{
			CaptureSceneCubemapFace_internal(cross, cubemap, screen->GetBits(), screen->GetWidth(), screen->GetHeight(), screen->GetFormat());
			screen->Release();
		}
	}
}

static void CaptureSceneCubemapFace_render()
{
	if (g_sceneCubemapFaceIndex != -1 && g_sceneCubemapCapture)
	{
		CaptureSceneCubemapFace(g_sceneCubemap4x3Cross[0], g_sceneCubemap[0], CRenderTargets::GetBackBuffer());
		CaptureSceneCubemapFace(g_sceneCubemap4x3Cross[1], g_sceneCubemap[1], NULL);

		g_sceneCubemapCapture = false;
		g_sceneCubemapFaceAdvanceTimer = 3;
	}
}

static void ExportSceneCubemap(const grcImage* cross, grcImage* cubemap, int globalIndex, const char* suffix)
{
	if (cross && cubemap)
	{
		const int bpp = grcImage::GetFormatBitsPerPixel(cubemap->GetFormat());
		grcImage* cubeFace = cubemap;

		for (int faceIndex = 0; faceIndex < 6; faceIndex++)
		{
			if      (bpp ==   8) { CopyToCubemapFaceFrom4x3Cross((const u8   *)cross->GetBits(), (u8   *)cubeFace->GetBits(), cubeFace->GetWidth(), faceIndex); }
			else if (bpp ==  16) { CopyToCubemapFaceFrom4x3Cross((const u16  *)cross->GetBits(), (u16  *)cubeFace->GetBits(), cubeFace->GetWidth(), faceIndex); }
			else if (bpp ==  32) { CopyToCubemapFaceFrom4x3Cross((const u32  *)cross->GetBits(), (u32  *)cubeFace->GetBits(), cubeFace->GetWidth(), faceIndex); }
			else if (bpp ==  64) { CopyToCubemapFaceFrom4x3Cross((const u64  *)cross->GetBits(), (u64  *)cubeFace->GetBits(), cubeFace->GetWidth(), faceIndex); }
			else if (bpp == 128) { CopyToCubemapFaceFrom4x3Cross((const Vec4f*)cross->GetBits(), (Vec4f*)cubeFace->GetBits(), cubeFace->GetWidth(), faceIndex); }

			cubeFace = cubeFace->GetNextLayer();
		}

		ScaleImageBrightness(cubemap, LightProbe::g_brightnessScale);
		cubemap->SaveDDS(atVarString("x:/scene_cubemap_%04d%s.dds", globalIndex, suffix).c_str());
	}
}

// Declared in timer.cpp
namespace rage {
extern bank_bool s_newUpdate;
}

static void CaptureSceneCubemapFace_update()
{
	// Psuedo-config vars
	static const int s_framesToWaitBeforeStreaming = 4;
	static const int s_framesToWaitAfterFlaggingCapture = 2;
	static const int s_framesToDoGameTick = 1;

	// Update continual capture state
	static bool s_prevNewUpdate;
	switch (g_cubemapCaptureState)
	{
		case CCS_Idle:
		{
			// Continual capture becoming enabled?
			if (LightProbe::g_capturingPanorama && g_continualCapture)
			{
				camInterface::GetDebugDirector().ActivateFreeCamAndCloneFrame();
				g_camBaseMatrix = RCC_MAT34V(camInterface::GetDebugDirector().GetFreeCamFrameNonConst().GetWorldMatrix());
				fwTimer::SetFixedFrameTime(1.0f/g_continualCaptureFPS);
				fwTimer::SetDeferredDebugPause(true);
			
#if GTA_REPLAY
				//make sure the replay is pause
				if(CReplayMgr::IsEditModeActive())
				{
					CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PAUSE );
				}
#endif
				g_sceneCubemapFaceIndex = 0;
				g_cubemapCaptureState = CCS_SetCamera;
				s_prevNewUpdate = s_newUpdate;
				s_newUpdate = false;
			}
		}
		break;

		case CCS_SetCamera:
		{
			// Set camera to new face and wait for streaming requests. Wait for visibility jobs
			// to finish and streaming requests are issued.
			SetCamera_internal((DebugTextureViewerDTQ::eDTQCubeFace)g_sceneCubemapFaceIndex);
			g_frameWait = s_framesToWaitBeforeStreaming;
			g_cubemapCaptureState = CCS_WaitStreamIn;
		}
		break;

		case CCS_WaitStreamIn:
		{
			if (--g_frameWait <= 0)
			{
				// Flush streaming and flag capture for render thread
				CStreaming::LoadAllRequestedObjects();
				Assertf(strStreamingEngine::GetInfo().GetNumberRealObjectsRequested() == 0, "Still objects streaming!");
				g_cubemapCaptureState = CCS_Capturing;
				g_sceneCubemapCapture = true;
				g_frameWait = s_framesToWaitAfterFlaggingCapture;
			}
		}
		break;

		case CCS_Capturing:
		{
			if (--g_frameWait <= 0)
			{
				Assertf(!g_sceneCubemapCapture, "Render thread not finished capturing yet!");

				// Move to next face
				if (++g_sceneCubemapFaceIndex < DebugTextureViewerDTQ::eDTQCF_Count)
				{
					g_cubemapCaptureState = CCS_SetCamera;
				}
				else
				{
					// Done all faces, save cubemap and advance game time
					ExportSceneCubemap(g_sceneCubemap4x3Cross[1], g_sceneCubemap[1], g_cubemapIndex++, "");
					camInterface::GetDebugDirector().DeactivateFreeCam();
					fwTimer::SetDeferredDebugPause(false);

#if GTA_REPLAY
					//unpause the replay so it can process the next timestep
					if(CReplayMgr::IsEditModeActive())
					{
						CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PLAY );
					}
#endif

					g_cubemapCaptureState = CCS_GameTick;
					g_sceneCubemapFaceIndex = 0;
					g_frameWait = s_framesToDoGameTick-1;
				}
			}
		}
		break;

		case CCS_GameTick:
		{
			// Was continual capture disabled?
			if (!LightProbe::g_capturingPanorama || !g_continualCapture)
			{
				fwTimer::SetFixedFrameTime(0.0f);
				g_sceneCubemapFaceIndex = 0;
				g_cubemapCaptureState = CCS_Idle;
				s_newUpdate = s_prevNewUpdate;
#if GTA_REPLAY
				//make sure the replay is paused if we quit out of this mode
				if(CReplayMgr::IsEditModeActive())
				{
					CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PAUSE );
				}
#endif
			}
			else
			{
				if (--g_frameWait <= 0
					REPLAY_ONLY(&& (CReplayMgr::IsEditModeActive() == false || g_FinishedReplayTick)))
				{
					// Game tick done, wait one frame for things to settle
					camInterface::GetDebugDirector().ActivateFreeCamAndCloneFrame();
					g_camBaseMatrix = RCC_MAT34V(camInterface::GetDebugDirector().GetFreeCamFrameNonConst().GetWorldMatrix());
					fwTimer::SetDeferredDebugPause(true);

#if GTA_REPLAY
					g_FinishedReplayTick = false;

					//we need to pause the replay again so we can capture the next panoramic frame
					if(CReplayMgr::IsEditModeActive())
					{
						CReplayMgr::SetNextPlayBackState( REPLAY_STATE_PAUSE );
					}
#endif
					g_cubemapCaptureState = CCS_PostGameWait;
					g_frameWait = g_continualCaptureFrameDelay;
				}
			}
		}
		break;

		case CCS_PostGameWait:
		{
			if (--g_frameWait <= 0)
			{
				// Begin capturing new frame
				g_cubemapCaptureState = CCS_SetCamera;
			}
		}
		break;
	}

	// Early-out if we're doing a continual capture
	if (g_cubemapCaptureState != CCS_Idle)
	{
		return;
	}
	
	// Non-continual capture update
	if (g_sceneCubemapFaceAdvanceTimer > 0)
	{
		if (--g_sceneCubemapFaceAdvanceTimer == 0)
		{
			const char* faceStrings[] = {"UP", "DOWN", "LEFT", "RIGHT", "FRONT", "BACK"};
			CompileTimeAssert(NELEM(faceStrings) == DebugTextureViewerDTQ::eDTQCF_Count);
			Displayf("captured face %s", faceStrings[g_sceneCubemapFaceIndex]);

			if (++g_sceneCubemapFaceIndex < DebugTextureViewerDTQ::eDTQCF_Count)
			{
				SetCamera_internal((DebugTextureViewerDTQ::eDTQCubeFace)g_sceneCubemapFaceIndex);
			}
			else
			{
				g_sceneCubemapFaceIndex = -1;
				SetCamera_internal(DebugTextureViewerDTQ::eDTQCF_NX_LEFT);
				Displayf("capture complete");
			}
		}
	}
}

static void ExportSceneCubemap_button()
{
	ExportSceneCubemap(g_sceneCubemap4x3Cross[0], g_sceneCubemap[0], g_cubemapIndex, "_HDR");
	ExportSceneCubemap(g_sceneCubemap4x3Cross[1], g_sceneCubemap[1], g_cubemapIndex, "");
	g_cubemapIndex++;
}

#endif // LIGHT_PROBE_SCENE_CUBEMAP

#define ADD_SIMPLE_BUTTON(bk,name,code) \
{ \
	class _private_button \
	{ \
	public: \
		static void func() { code; } \
	}; \
	bk.AddButton(name, _private_button::func); \
}

void LightProbe::AddWidgets()
{
	//if (IsEnabled())
	{
		bkBank& bk = BANKMGR.CreateBank("Panoramic Screenshot");

#if LIGHT_PROBE_SCENE_CUBEMAP
		bk.PushGroup("Scene Cubemap", false);
		{
			bk.AddToggle("Capturing Panorama (hide overlays)", &g_capturingPanorama);
			bk.AddSlider("Exposure Override", &PostFX::g_overwrittenExposure, -16.0f, 16.0f, 1.0f/32.0f);
			bk.AddSeparator();
			bk.AddButton("Set Camera Up"   , SetCamera_button<DebugTextureViewerDTQ::eDTQCF_PZ_UP   >);
			bk.AddButton("Set Camera Down" , SetCamera_button<DebugTextureViewerDTQ::eDTQCF_NZ_DOWN >);
			bk.AddButton("Set Camera Left" , SetCamera_button<DebugTextureViewerDTQ::eDTQCF_NX_LEFT >);
			bk.AddButton("Set Camera Right", SetCamera_button<DebugTextureViewerDTQ::eDTQCF_PX_RIGHT>);
			bk.AddButton("Set Camera Front", SetCamera_button<DebugTextureViewerDTQ::eDTQCF_PY_FRONT>);
			bk.AddButton("Set Camera Back" , SetCamera_button<DebugTextureViewerDTQ::eDTQCF_NY_BACK >);
			bk.AddButton("Capture Cubemap Face", CaptureSceneCubemapFace_button);
			bk.AddButton("Export Cubemap", ExportSceneCubemap_button);
			bk.AddSeparator();

			bk.AddToggle("Continual Capture", &g_continualCapture);
			bk.AddSlider("Continual Capture FPS", &g_continualCaptureFPS, 1.0f, 120.0f, 1.0f/32.0f);
			bk.AddSlider("Continual Capture Delay", &g_continualCaptureFrameDelay, 1, 120, 1);
		}
		bk.PopGroup();
#else
		bk.AddToggle("Capturing Panorama (hide overlays)", &g_capturingPanorama); // this is still useful without LIGHT_PROBE_SCENE_CUBEMAP
		bk.AddSlider("Exposure Override", &PostFX::g_overwrittenExposure, -16.0f, 16.0f, 1.0f/32.0f);
#endif
		if (IsEnabled())
		{
#if RDR_VERSION
			bk.AddToggle("Reflection Map - Everything Visible", &CRenderPhaseReflection::GetRenderEverythingVisibleRef());
			bk.AddToggle("Reflection Map - Disable LOD Mask", &CRenderPhaseReflection::GetRenderDisableLODMaskRef());
			bk.AddToggle("Reflection Map - Render HD Exterior", &CRenderPhaseReflection::GetRenderHDExteriorRef());
			bk.AddToggle("Reflection Map - Render Trees", &CRenderPhaseReflection::GetRenderTreesRef());
#endif // RDR_VERSION

			ADD_SIMPLE_BUTTON(bk, "Show Reflection Target",
			{
				if (!CRenderTargets::IsShowingRenderTarget("REFLECTION_MAP_COLOUR_CUBE"))
				{
					CRenderTargets::ShowRenderTarget("REFLECTION_MAP_COLOUR_CUBE");
					CRenderTargets::ShowRenderTargetBrightness(g_brightnessScale);
					CRenderTargets::ShowRenderTargetNativeRes(false, false);
					CRenderTargets::ShowRenderTargetFullscreen(true);
					CRenderTargets::ShowRenderTargetInfo(false); // we're probably displaying model counts too, so don't clutter up the screen
				}
				else
				{
					CRenderTargets::ShowRenderTargetReset();
				}
			});

			ADD_SIMPLE_BUTTON(bk, "Capture", g_capture = true);
			bk.AddSlider("Sky HDR Exponent", &g_skyHDRExponent, -8.0f, 8.0f, 1.0f/64.0f);

			class BrightnessExponent_update { public: static void func()
			{
				g_brightnessScale = powf(2.0f, g_brightnessExponent);

				if (CRenderTargets::IsShowingRenderTarget("REFLECTION_MAP_COLOUR_CUBE"))
				{
					CRenderTargets::ShowRenderTargetBrightness(g_brightnessScale);
				}
			}};
			bk.AddSlider("Brightness Exponent", &g_brightnessExponent, -8.0f, 8.0f, 1.0f/64.0f, BrightnessExponent_update::func);
			bk.AddCombo("Save As", &g_saveAs, NELEM(g_lightProbeSaveAsStrings), g_lightProbeSaveAsStrings);
			bk.AddSlider("Debug Draw Opacity", &g_debugDrawOpacity, 0.0f, 1.0f, 1.0f/32.0f);
			ADD_SIMPLE_BUTTON(bk, "Debug Draw Clear", g_debugDrawSpheres.clear());

#if LIGHT_PROBE_SPHERE_RENDER
			bk.AddTitle("");
			ADD_SIMPLE_BUTTON(bk, "Set Reflection Preview", CDebugTextureViewerInterface::SelectPreviewTexture(g_lightProbeTexture, "Reflection", false, false));
			bk.AddSlider("Render Opacity", &g_lightProbeOpacity, 0.0f, 1.0f, 1.0f/32.0f);
			bk.AddSlider("Render Mip Index", &g_lightProbeMipIndex, -1, 16, 1);
			bk.AddSlider("Render Reflect Amount", &g_lightProbeReflectionAmount, -1.0f, 1.0f, 1.0f/32.0f);
			bk.AddVector("Render Sphere", &g_lightProbeSphere, -16000.0f, 16000.0f, 1.0f/32.0f);
			bk.AddAngle("Yaw", &g_lightProbeYaw, bkAngleType::DEGREES);
			bk.AddAngle("Pitch", &g_lightProbePitch, bkAngleType::DEGREES);
			bk.AddAngle("Roll", &g_lightProbeRoll, bkAngleType::DEGREES);
#endif // LIGHT_PROBE_SPHERE_RENDER
		}
	}
}

void LightProbe::Capture(grcTexture* reflection)
{
	ForceNoPopupsAuto nopopups(false);

	if (g_capture && reflection)
	{
		static int globalIndex = 0;

#if LIGHT_PROBE_SPHERE_RENDER
		if (g_saveAs == LP_SAVE_AS_PANORAMA)
		{
			sprintf(g_lightProbePanoramaPath, "x:/reflection_%04d_panorama.dds", globalIndex);
			g_lightProbePanoramaFrame = 1;
		}
		else
#endif // LIGHT_PROBE_SPHERE_RENDER
		{
			// XboxOne can't lock multiple faces at once, but pc seems to have trouble _not_ doing that ..
			// this is why these two codepaths are completely different. It's really quite annoying.
#if RSG_DURANGO
			const grcImage::Format format = (grcImage::Format)reflection->GetImageFormat();
			const int faceSize = reflection->GetWidth();
			const int bpp = reflection->GetBitsPerPixel();
			
			if (g_saveAs == LP_SAVE_AS_FACES)
			{
				for (int faceIndex = 0; faceIndex < 6; faceIndex++)
				{
					grcImage* image = grcImage::Create(faceSize, faceSize, 1, format, grcImage::STANDARD, reflection->GetMipMapCount() - 1, 0);
					if (AssertVerify(image))
					{
						const grcImage* mip = image;
						for (int mipIndex = 0; mipIndex < reflection->GetMipMapCount() && mip; mipIndex++)
						{
							grcTextureLock lock;
							if (AssertVerify(reflection->LockRect(faceIndex, mipIndex, lock, grcsRead | grcsAllowVRAMLock)))
							{
								sysMemCpy(mip->GetBits(), lock.Base, mip->GetSize());
								reflection->UnlockRect(lock);
							}

							mip = mip->GetNext();
						}

						ScaleImageBrightness(image, g_brightnessScale);
						image->SaveDDS(atVarString("x:/reflection_%04d_face[%d].dds", globalIndex, faceIndex).c_str());
						image->Release();
					}
				}
			}
			else if (g_saveAs == LP_SAVE_AS_4x3_CROSS)
			{
				const int w = faceSize*4;
				const int h = faceSize*3;

				grcImage* image = grcImage::Create(w, h, 1, format, grcImage::STANDARD, 0, 0);
				if (AssertVerify(image))
				{
					sysMemSet(image->GetBits(), 0, w*h*bpp/8);

					for (int faceIndex = 0; faceIndex < 6; faceIndex++)
					{
						grcTextureLock lock;
						if (AssertVerify(reflection->LockRect(faceIndex, 0, lock, grcsRead | grcsAllowVRAMLock)))
						{
							if      (bpp ==   8) { CopyFromCubemapFaceTo4x3Cross((u8   *)image->GetBits(), (const u8   *)lock.Base, faceSize, faceIndex); }
							else if (bpp ==  16) { CopyFromCubemapFaceTo4x3Cross((u16  *)image->GetBits(), (const u16  *)lock.Base, faceSize, faceIndex); }
							else if (bpp ==  32) { CopyFromCubemapFaceTo4x3Cross((u32  *)image->GetBits(), (const u32  *)lock.Base, faceSize, faceIndex); }
							else if (bpp ==  64) { CopyFromCubemapFaceTo4x3Cross((u64  *)image->GetBits(), (const u64  *)lock.Base, faceSize, faceIndex); }
							else if (bpp == 128) { CopyFromCubemapFaceTo4x3Cross((Vec4f*)image->GetBits(), (const Vec4f*)lock.Base, faceSize, faceIndex); }

							reflection->UnlockRect(lock);
						}
					}

					ScaleImageBrightness(image, g_brightnessScale);
					image->SaveDDS(atVarString("x:/reflection_%04d_4x3cross.dds", globalIndex).c_str());
					image->Release();
				}
			}
			else if (g_saveAs == LP_SAVE_AS_CUBEMAP)
			{
				grcImage* image = grcImage::Create(faceSize, faceSize, 1, format, grcImage::CUBE, reflection->GetMipMapCount() - 1, 5);
				if (AssertVerify(image))
				{
					const grcImage* face = image;
					for (int faceIndex = 0; faceIndex < 6; faceIndex++)
					{
						const grcImage* mip = face;
						for (int mipIndex = 0; mipIndex < reflection->GetMipMapCount() && mip; mipIndex++)
						{
							grcTextureLock lock;
							if (AssertVerify(reflection->LockRect(faceIndex, mipIndex, lock, grcsRead | grcsAllowVRAMLock)))
							{
								sysMemCpy(mip->GetBits(), lock.Base, mip->GetSize());
								reflection->UnlockRect(lock);
							}

							mip = mip->GetNext();
						}

						face = face->GetNextLayer();
					}

					ScaleImageBrightness(image, g_brightnessScale);
					image->SaveDDS(atVarString("x:/reflection_%04d_cube.dds", globalIndex).c_str());
					image->Release();
				}
			}
#else // pc/ps4
			grcTextureLock lock;
			if (AssertVerify(reflection->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock))) // this locks all faces of the cubemap
			{
				const grcImage::Format format = (grcImage::Format)reflection->GetImageFormat();
				const int faceSize = reflection->GetWidth();
				const int bpp = reflection->GetBitsPerPixel();
				char* base = (char*)lock.Base;
				int mipOffsets[16];
				int layerStride = 0;

				for (int mip = 0; mip < reflection->GetMipMapCount(); mip++) // this only works for "large" mips ..
				{
					const int mw = Max<int>(1, reflection->GetWidth() >> mip);
					const int mh = Max<int>(1, reflection->GetHeight() >> mip);

					mipOffsets[mip] = layerStride;
					layerStride += mw*mh*bpp/8;
				}

				if (g_saveAs == LP_SAVE_AS_FACES)
				{
					for (int faceIndex = 0; faceIndex < 6; faceIndex++)
					{
						lock.Base = base + layerStride*faceIndex;
						grcImage* image = rage_new grcImage();
						image->LoadAsTextureAlias(reflection, &lock, reflection->GetMipMapCount());
						ScaleImageBrightness(image, g_brightnessScale);
						image->SaveDDS(atVarString("x:/reflection_%04d_face[%d].dds", globalIndex, faceIndex).c_str());
						image->ReleaseAlias();
					}
				}
				else if (g_saveAs == LP_SAVE_AS_4x3_CROSS)
				{
					const int w = faceSize*4;
					const int h = faceSize*3;

					grcImage* image = grcImage::Create(w, h, 1, format, grcImage::STANDARD, 0, 0);
					if (AssertVerify(image))
					{
						sysMemSet(image->GetBits(), 0, w*h*bpp/8);

						for (int faceIndex = 0; faceIndex < 6; faceIndex++)
						{
							if      (bpp ==   8) { CopyFromCubemapFaceTo4x3Cross((u8   *)image->GetBits(), (const u8   *)(base + layerStride*faceIndex), faceSize, faceIndex); }
							else if (bpp ==  16) { CopyFromCubemapFaceTo4x3Cross((u16  *)image->GetBits(), (const u16  *)(base + layerStride*faceIndex), faceSize, faceIndex); }
							else if (bpp ==  32) { CopyFromCubemapFaceTo4x3Cross((u32  *)image->GetBits(), (const u32  *)(base + layerStride*faceIndex), faceSize, faceIndex); }
							else if (bpp ==  64) { CopyFromCubemapFaceTo4x3Cross((u64  *)image->GetBits(), (const u64  *)(base + layerStride*faceIndex), faceSize, faceIndex); }
							else if (bpp == 128) { CopyFromCubemapFaceTo4x3Cross((Vec4f*)image->GetBits(), (const Vec4f*)(base + layerStride*faceIndex), faceSize, faceIndex); }
						}

						ScaleImageBrightness(image, g_brightnessScale);
						image->SaveDDS(atVarString("x:/reflection_%04d_4x3cross.dds", globalIndex).c_str());
						image->Release();
					}
				}
				else if (g_saveAs == LP_SAVE_AS_CUBEMAP)
				{
					grcImage* image = grcImage::Create(faceSize, faceSize, 1, format, grcImage::CUBE, reflection->GetMipMapCount() - 1, 5);
					if (AssertVerify(image))
					{
						const grcImage* face = image;
						for (int faceIndex = 0; faceIndex < 6; faceIndex++)
						{
							const grcImage* mip = face;
							for (int mipIndex = 0; mipIndex < reflection->GetMipMapCount() && mip; mipIndex++)
							{
								sysMemCpy(face->GetBits(), base + layerStride*faceIndex + mipOffsets[mipIndex], (faceSize>>mipIndex)*(faceSize>>mipIndex)*bpp/8);
								mip = mip->GetNext();
							}

							face = face->GetNextLayer();
						}

						ScaleImageBrightness(image, g_brightnessScale);
						image->SaveDDS(atVarString("x:/reflection_%04d_cube.dds", globalIndex).c_str());
						image->Release();
					}
				}

				reflection->UnlockRect(lock);
			}
#endif // pc/ps4
		}

		globalIndex++;

		const Mat34V cameraMtx = gVpMan.GetRenderGameGrcViewport()->GetCameraMtx();
		const Vec3V camPos = +cameraMtx.GetCol3();
		const Vec3V camDir = -cameraMtx.GetCol2();
		const Vec4V sphere = Vec4V(camPos + camDir*ScalarV(V_THREE), ScalarV(V_ONE));
		g_debugDrawSpheres.PushAndGrow(sphere);

#if LIGHT_PROBE_SPHERE_RENDER
		g_lightProbeSphere = sphere;
		g_lightProbeTexture = reflection; // TODO -- make a copy
#endif // LIGHT_PROBE_SPHERE_RENDER

		g_capture = false;
	}
}

#if LIGHT_PROBE_SPHERE_RENDER
static void DrawCube(Vec3V_In bbmin, Vec3V_In bbmax, Color32 colour, bool bFlip)
{
	const Vec3V corners[] =
	{
		bbmin,
		GetFromTwo<Vec::X2,Vec::Y1,Vec::Z1>(bbmin, bbmax),
		GetFromTwo<Vec::X1,Vec::Y2,Vec::Z1>(bbmin, bbmax),
		GetFromTwo<Vec::X2,Vec::Y2,Vec::Z1>(bbmin, bbmax),
		GetFromTwo<Vec::X1,Vec::Y1,Vec::Z2>(bbmin, bbmax),
		GetFromTwo<Vec::X2,Vec::Y1,Vec::Z2>(bbmin, bbmax),
		GetFromTwo<Vec::X1,Vec::Y2,Vec::Z2>(bbmin, bbmax),
		bbmax,
	};

	//   6---7
	//  /|  /|
	// 4---5 |
	// | | | |
	// | 2-|-3
	// |/  |/
	// 0---1

	const int quads[6][4] =
	{
		{1,5,4,0},
		{3,7,5,1},
		{2,6,7,3},
		{0,4,6,2},
		{2,3,1,0},
		{5,7,6,4},
	};

	grcBegin(drawTris, NELEM(quads)*2*3);

	for (int i = 0; i < NELEM(quads); i++)
	{
		grcVertex(VEC3V_ARGS(corners[quads[i][0]]),             0.0f,0.0f,0.0f, colour, 0.0f,0.0f);
		grcVertex(VEC3V_ARGS(corners[quads[i][bFlip ? 2 : 1]]), 0.0f,0.0f,0.0f, colour, 1.0f,0.0f);
		grcVertex(VEC3V_ARGS(corners[quads[i][bFlip ? 1 : 2]]), 0.0f,0.0f,0.0f, colour, 1.0f,1.0f);

		grcVertex(VEC3V_ARGS(corners[quads[i][0]]),             0.0f,0.0f,0.0f, colour, 0.0f,0.0f);
		grcVertex(VEC3V_ARGS(corners[quads[i][bFlip ? 3 : 2]]), 0.0f,0.0f,0.0f, colour, 1.0f,1.0f);
		grcVertex(VEC3V_ARGS(corners[quads[i][bFlip ? 2 : 3]]), 0.0f,0.0f,0.0f, colour, 0.0f,1.0f);
	}

	grcEnd();
}
#endif // LIGHT_PROBE_SPHERE_RENDER

void LightProbe::Update()
{
#if LIGHT_PROBE_SCENE_CUBEMAP
	CaptureSceneCubemapFace_update();
#endif // LIGHT_PROBE_SCENE_CUBEMAP
}

void LightProbe::Render()
{
#if LIGHT_PROBE_SCENE_CUBEMAP
	CaptureSceneCubemapFace_render();
#endif // LIGHT_PROBE_SCENE_CUBEMAP

	if (g_debugDrawOpacity > 0.0f)
	{
		for (int i = 0; i < g_debugDrawSpheres.GetCount(); i++)
		{
			grcDebugDraw::Sphere(g_debugDrawSpheres[i].GetXYZ(), g_debugDrawSpheres[i].GetWf(), Color32(0,255,0,(u8)(0.5f + 255.0f*g_debugDrawOpacity)), false, 1, 16);
		}
	}

#if LIGHT_PROBE_SPHERE_RENDER
	if (g_lightProbeOpacity > 0.0f && g_lightProbeSphere.GetWf() > 0.0f && g_lightProbeShader && g_lightProbeTexture)
	{
		const grcRasterizerStateHandle   RS_prev = grcStateBlock::RS_Active;
		const grcDepthStencilStateHandle DS_prev = grcStateBlock::DSS_Active;
		const grcBlendStateHandle        BS_prev = grcStateBlock::BS_Active;

		grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

		Mat34V rotation;
		Mat34VFromEulersXYZ(rotation, Vec3V(g_lightProbePitch, g_lightProbeRoll, -g_lightProbeYaw)*ScalarV(V_TO_RADIANS));

		g_lightProbeShader->SetVar(g_lightProbeShaderRotation, rotation);
		g_lightProbeShader->SetVar(g_lightProbeShaderSphere, g_lightProbeSphere);
		g_lightProbeShader->SetVar(g_lightProbeShaderParams, Vec4f(g_lightProbeReflectionAmount, (float)Max<int>(0, g_lightProbeMipIndex), g_brightnessScale*4.0f, g_lightProbeMipIndex >= 0 ? 1.0f : 0.0f));
		g_lightProbeShader->SetVar(g_lightProbeShaderTexture, g_lightProbeTexture);

		if (AssertVerify(g_lightProbeShader->BeginDraw(grmShader::RMC_DRAW, false, g_lightProbeShaderTechnique)))
		{
			g_lightProbeShader->Bind(0);

			const Vec3V bbmin = g_lightProbeSphere.GetXYZ() - Vec3V(g_lightProbeSphere.GetW());
			const Vec3V bbmax = g_lightProbeSphere.GetXYZ() + Vec3V(g_lightProbeSphere.GetW());
			DrawCube(bbmin, bbmax, Color32(255,255,255,(u8)(0.5f + 255.0f*g_lightProbeOpacity)), true);

			g_lightProbeShader->UnBind();
			g_lightProbeShader->EndDraw();
		}

		grcStateBlock::SetStates(RS_prev, DS_prev, BS_prev); // restore states
	}

	if (g_lightProbePanoramaFrame == 1)
	{
		if (AssertVerify(g_lightProbePanoramaRT && g_lightProbeTexture))
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, g_lightProbePanoramaRT, NULL);

			const grcRasterizerStateHandle   RS_prev = grcStateBlock::RS_Active;
			const grcDepthStencilStateHandle DS_prev = grcStateBlock::DSS_Active;
			const grcBlendStateHandle        BS_prev = grcStateBlock::BS_Active;

			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

			CSprite2d sprite;
			const Vec4V params0(V_ONE);
			const Vec4V params1((float)g_lightProbeMipIndex, 1.0f, 0.0f, 0.0f);
			CSprite2d::SetGeneralParams(RCC_VECTOR4(params0), RCC_VECTOR4(params1));
			sprite.BeginCustomList(CSprite2d::CS_BLIT_CUBE_MAP, g_lightProbeTexture);
			grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(255,255,255,255));
			sprite.EndCustomList();

			grcStateBlock::SetStates(RS_prev, DS_prev, BS_prev); // restore states
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		}
	}

	if (g_lightProbePanoramaFrame > 0)
	{
		if (++g_lightProbePanoramaFrame >= 3)
		{
			if (AssertVerify(g_lightProbePanoramaRT))
			{
				grcImage* image = rage_new grcImage();
				grcTextureLock lock;
				g_lightProbePanoramaRT->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
				image->LoadAsTextureAlias(g_lightProbePanoramaRT, &lock);
				ScaleImageBrightness(image, g_brightnessScale);
				image->SaveDDS(g_lightProbePanoramaPath);
				image->ReleaseAlias();
				g_lightProbePanoramaRT->UnlockRect(lock);
			}

			g_lightProbePanoramaFrame = 0;
		}
	}
#endif // LIGHT_PROBE_SPHERE_RENDER
}

float LightProbe::GetSkyHDRMultiplier()
{
	return powf(2.0f, g_skyHDRExponent);
}

#if GTA_REPLAY
s32 LightProbe::GetFixedPanoramaTimestepMS()
{
	return s32(1.0f/g_continualCaptureFPS * 1000);
}

void LightProbe::OnFinishedReplayTick()
{
	g_FinishedReplayTick = true;
}
#endif

#endif // __BANK
