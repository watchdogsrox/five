#ifndef _VIDEORECORDINGFRAMEBUFFER_H_
#define _VIDEORECORDINGFRAMEBUFFER_H_

// rage
#include "bank/bank.h"
#include "bank/button.h"
#include "bank/bkmgr.h"

// framework
#include "fwui/Common/fwuiScaling.h"
#include "video/mediaencoder_params.h"

// game
#include "control/replay/ReplaySettings.h"
#include "renderer/rendertargets.h"

#define VIDEO_CONVERSION_ENABLED	( (0 && RSG_DURANGO) || (1 && RSG_ORBIS) || ( 1 && RSG_PC )) && GTA_REPLAY

#if RSG_ORBIS
#define VIDEO_BUFFER_COUNT (3)
#else
#define VIDEO_BUFFER_COUNT (1)
#endif

class VideoRecordingFrameBuffer
{
public:
	VideoRecordingFrameBuffer();
	~VideoRecordingFrameBuffer();

	bool					Init( u32 const width, u32 const height, fwuiScaling::eMode const scaleMode = rage::fwuiScaling::UI_SCALE_DEFAULT );
	void					Shutdown();

	bool					ConvertTarget(grcRenderTarget*& pDst, float* timeStepOut, const grcRenderTarget* pSrc, float const timeStepIn, bool invertFrame, bool isEncoderPaused);	
	bool					IsInitialized();

private:

	bool					HasFramesInBuffer();
	void					ResetFrameCounters();

	int								m_outputWidth;
	int								m_outputHeight;

	fwuiScaling::eMode	            m_scaleMode;
	bool							m_isInitialized;

	u8								m_currentFrame;
	u8								m_earliestFrame;
	u8								m_previousFrame;

	bool							m_isFrameReady[VIDEO_BUFFER_COUNT];
	float							m_frameTime[VIDEO_BUFFER_COUNT];

	grcRenderTarget*				m_conversionTarget[VIDEO_BUFFER_COUNT];	
	grcViewport*					m_conversionViewport;

#if RSG_ORBIS
	u8*								m_conversionTargetMemoryBlock[VIDEO_BUFFER_COUNT];
	grcTexture*						m_conversionTexture[VIDEO_BUFFER_COUNT];
#endif
};

#endif // _VIDEOFRAME_H_