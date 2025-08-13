#include "VideoRecordingFrameBuffer.h"

#if VIDEO_CONVERSION_ENABLED

#if RSG_ORBIS
#include "grcore/texture_gnm.h"
#endif

#include "atl/string.h"
#include "file/cachepartition.h"
#include "grcore/device.h"
#include "grcore/quads.h"
#include "grcore/viewport.h"
#include "math/amath.h"

#include "system/controlMgr.h"
#include "system/keyboard.h"
#include "system/service.h"

#include "video/video_channel.h"

static u8* VideoRecordingFrameBuffer_Alloc(size_t size,size_t align)
{
#if USE_FLEX_MEMORY_FOR_BUFFERS
	return static_cast<u8*>(MEMMANAGER.GetFlexAllocator()->Allocate(size, align));
#else
	(void)align;
	u8* ptr = rage_aligned_new(align) u8[size];
	return ptr;
#endif
}

static void VideoRecordingFrameBuffer_Free(u8* ptr)
{
	if (ptr)
	{
#if USE_FLEX_MEMORY_FOR_BUFFERS
		MEMMANAGER.GetFlexAllocator()->Free(ptr);
#else
		delete ptr;
#endif
	}
}

VideoRecordingFrameBuffer::VideoRecordingFrameBuffer() 
	: m_outputWidth( 0 )
	, m_outputHeight( 0 )
	, m_scaleMode( fwuiScaling::UI_SCALE_DEFAULT )
	, m_isInitialized( false )
	, m_conversionViewport( NULL )
{
	for(int i = 0; i < VIDEO_BUFFER_COUNT; i++)
	{
#if RSG_ORBIS
		m_conversionTargetMemoryBlock[i] = NULL;
		m_conversionTexture[i] = NULL;
#endif

		m_isFrameReady[i] = false;
		m_frameTime[i] = 0.0f;
		m_conversionTarget[i] = NULL;
	}
}

VideoRecordingFrameBuffer::~VideoRecordingFrameBuffer() 
{
	Shutdown();
}


bool VideoRecordingFrameBuffer::Init( u32 const width, u32 const height, fwuiScaling::eMode const scaleMode )
{
	bool success = true;

	m_outputWidth = width;
	m_outputHeight = height;
	m_scaleMode = scaleMode;

	grcTextureFactory::CreateParams params;
	params.Format = grctfA8R8G8B8;
	params.Lockable = true;

	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 

	params.InTiledMemory	= true;
	params.IsSwizzled		= false;
	params.EnableCompression = false;
	
#if RSG_ORBIS
	params.ForceNoTiling = true;

	sce::Gnm::Texture tex;
	sce::Gnm::TileMode tileMode;
	sce::Gnm::DataFormat format = sce::Gnm::kDataFormatB8G8R8A8Unorm;

	sce::GpuAddress::computeSurfaceTileMode(
		&tileMode, 
		sce::GpuAddress::kSurfaceTypeColorTarget,
		format, 
		1);

	sce::Gnm::SizeAlign result = tex.initAs2d( m_outputWidth, m_outputHeight, 1, format, sce::Gnm::kTileModeDisplay_LinearAligned, sce::Gnm::kNumFragments1);
	grcTextureFactory::TextureCreateParams temp_params(grcTextureFactory::TextureCreateParams::SYSTEM,grcTextureFactory::TextureCreateParams::LINEAR);
	for(int i = 0; i < VIDEO_BUFFER_COUNT; i++)
	{				
		m_conversionTargetMemoryBlock[i] = VideoRecordingFrameBuffer_Alloc(result.m_size, 16);

		m_conversionTexture[i] = grcTextureFactory::GetInstance().Create( m_outputWidth, m_outputHeight, grctfA8R8G8B8, m_conversionTargetMemoryBlock[i], 1, &temp_params);
		m_conversionTarget[i] = grcTextureFactory::GetInstance().CreateRenderTarget( "VideoEncode", m_conversionTexture[i]->GetTexturePtr());	
	}
#else
	for(int i = 0; i < VIDEO_BUFFER_COUNT; i++)
	{
		m_conversionTarget[i] = grcTextureFactory::GetInstance().CreateRenderTarget( "VideoEncode", grcrtPermanent, 
			m_outputWidth, m_outputHeight, 32, &params );
	}
#endif

	for(int i = 0 ; i < VIDEO_BUFFER_COUNT; i++)
	{
		success &= videoVerifyf( m_conversionTarget[i] != NULL, "VideoFrame::Init: Failed to create render target for conversion" );
	}
	
	ResetFrameCounters();
	
	m_isInitialized = success;

	return success;
}

void VideoRecordingFrameBuffer::Shutdown()
{
	for(int i = 0 ; i < VIDEO_BUFFER_COUNT; i++)
	{
		SafeRelease(m_conversionTarget[i]);

#if RSG_ORBIS
		VideoRecordingFrameBuffer_Free(m_conversionTargetMemoryBlock[i]);
		m_conversionTargetMemoryBlock[i] = NULL;

		SafeRelease( m_conversionTexture[i] );
#endif
	}
	
	if( m_conversionViewport )
	{
		delete m_conversionViewport;
		m_conversionViewport = NULL;
	}

	ResetFrameCounters();

	m_isInitialized = false;
}

bool VideoRecordingFrameBuffer::IsInitialized()
{
	return m_isInitialized;
}

bool VideoRecordingFrameBuffer::HasFramesInBuffer()
{
	for(int i = 0; i < VIDEO_BUFFER_COUNT; i++)
	{
		if (m_isFrameReady[i])
			return true;
	}

	return false;
}

void VideoRecordingFrameBuffer::ResetFrameCounters()
{
	m_currentFrame = 0;
	m_earliestFrame = (m_currentFrame + 1) % VIDEO_BUFFER_COUNT;
	m_previousFrame = 0;

	for(int i = 0; i < VIDEO_BUFFER_COUNT; i++)
	{
		m_isFrameReady[i] = false;
		m_frameTime[i] = 0.0f;
	}
}


bool VideoRecordingFrameBuffer::ConvertTarget(grcRenderTarget*& pDst, float* timeStepOut, const grcRenderTarget* pSrc, float const timeStepIn, bool invertFrame, bool isEncoderPaused)
{	
	if(!pSrc)
	{
		if (HasFramesInBuffer())
		{
			bool frameToReturn = false;
			if(m_isFrameReady[m_earliestFrame])
			{
				pDst = m_conversionTarget[m_earliestFrame];
				*timeStepOut = m_frameTime[m_earliestFrame];
				m_isFrameReady[m_earliestFrame] = false;
				frameToReturn = true;
			}

			m_earliestFrame = (++m_earliestFrame) % VIDEO_BUFFER_COUNT;
			m_previousFrame = m_currentFrame;
			m_currentFrame = (++m_currentFrame) % VIDEO_BUFFER_COUNT;

			return frameToReturn;
		}
		else
		{
			return false;
		}
	}

	if(!m_isInitialized) 
		return false;

	bool isFrameReady = false;	

	if (timeStepIn > 0.0f)
	{

#if RSG_ORBIS
		(void)invertFrame;
		float const v1 = 0.f;
		float const v2 = 1.f;
#else // used to be PC
		float const v1 = invertFrame ? 1.f : 0.f;
		float const v2 = invertFrame ? 0.f : 1.f;
#endif

#if RSG_ORBIS
		CRenderTargets::UnlockUIRenderTargets(false);
#endif

		grcTextureFactory::GetInstance().LockRenderTarget( 0, m_conversionTarget[m_currentFrame], NULL );

		int const c_renderWidth = m_outputWidth;
		int const c_renderHeight = m_outputHeight;
		bool const c_centerImage = fwuiScaling::ShouldCenterImageForScaleMode( m_scaleMode );

		float const c_x1 = c_centerImage ? (m_outputWidth - c_renderWidth ) / 2.f : 0.f;
		float const c_y1 = c_centerImage ? (m_outputHeight - c_renderHeight ) / 2.f : 0.f;

		float const c_x2 = c_x1 + c_renderWidth;
		float const c_y2 = c_y1 + c_renderHeight;

		{
			GRC_ALLOC_SCOPE_AUTO_PUSH_POP();
			GRC_VIEWPORT_AUTO_PUSH_POP();

			if( m_conversionViewport == NULL )
			{
				m_conversionViewport = rage_new grcViewport();
				videoAssertf( m_conversionViewport, "MediaEncoderBuffersWindows::Initialize: Failed render target for conversion" );
			}

			if( m_conversionViewport )
			{
				grcViewport::SetCurrent( m_conversionViewport );
				m_conversionViewport->SetWindow( 0, 0, m_outputWidth, m_outputHeight );
				m_conversionViewport->Screen();
			}

			grcBindTexture( pSrc );
			grcDrawSingleQuadf( c_x1, c_y1, c_x2, c_y2, 0.f, 
				0.f, v1, 1.f, v2, 
				Color32( 255, 255, 255, 255 ) );

			grcBindTexture( NULL );

			grcTextureFactory::GetInstance().UnlockRenderTarget( 0 );
		}

#if RSG_ORBIS
		CRenderTargets::LockUIRenderTargets();
#endif
	}

	if( m_conversionViewport )
	{
		delete m_conversionViewport;
		m_conversionViewport = NULL;
	}

	m_isFrameReady[m_currentFrame] = !isEncoderPaused;

#if RSG_ORBIS
	// the pause and resume calls occur after the frame. they have to be to be timed nicely per frame rather than halfway through
	// but does mean that on a change, the previous frame won't be flagged correctly, so change it
	if (m_currentFrame != m_previousFrame)
	{
		if (m_isFrameReady[m_currentFrame] != m_isFrameReady[m_previousFrame])
		{
			m_isFrameReady[m_previousFrame] = m_isFrameReady[m_currentFrame];
		}
	}
#endif

	m_frameTime[m_currentFrame] = timeStepIn;

	if(m_isFrameReady[m_earliestFrame])
	{
		pDst = m_conversionTarget[m_earliestFrame];
		*timeStepOut = m_frameTime[m_earliestFrame];
		m_isFrameReady[m_earliestFrame] = false;
		isFrameReady = true;
	}

	m_currentFrame = (++m_currentFrame) % VIDEO_BUFFER_COUNT;
	m_previousFrame = m_currentFrame;
	m_earliestFrame = (++m_earliestFrame) % VIDEO_BUFFER_COUNT;

	return isFrameReady;
}

#endif	//VIDEO_FRAME_ENABLED
