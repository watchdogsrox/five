// =================================================
// renderer/Shadows/CascadeShadowsDepthHistogram.inl
// (c) 2012 RockstarNorth
// =================================================

#if CSM_DEPTH_HISTOGRAM

class CDepthHistogramBucket
{
public:
	CDepthHistogramBucket() {}
	CDepthHistogramBucket(float d0, float d1, float h0, float h1) : m_depthRangeMin(d0), m_depthRangeMax(d1), m_depthCount(0), m_heightRangeMin(h0), m_heightRangeMax(h1), m_heightCount(0)
	{
#if __DEV
		m_depthDebugFlag  = false;
		m_heightDebugFlag = false;
#endif // __DEV
	}

	float m_depthRangeMin;
	float m_depthRangeMax;
	int   m_depthCount;
#if __DEV
	bool  m_depthDebugFlag;
#endif // __DEV

	float m_heightRangeMin;
	float m_heightRangeMax;
	int   m_heightCount;
#if __DEV
	bool  m_heightDebugFlag;
#endif // __DEV
};

class CDepthHistogram : private datBase
{
public:
	CDepthHistogram();

	void Init();
#if __DEV
	void Dump();
#endif // __DEV
	void Update();
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	void UpdateHeightTrack_render();
	void UpdateHeightTrackDepthBufferLock() { if (m_heightTrackDepthBufferLock) { m_heightTrackDepthBufferCalc = true; } }
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
#if __BANK
	void InitWidgets(bkBank& bk);
	void DebugDraw() const;
#endif // __BANK

	bool  m_enabled;
#if __DEV
	bool  m_dump;
#endif // __DEV
	float m_depthRangeMin;
	float m_depthRangeMax;
	float m_depthPerspective;
	float m_heightRangeMin;
	float m_heightRangeMax;
	bool  m_heightRangeFull;
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	float m_heightTrackMin;
	float m_heightTrackMax;
	float m_heightTrackMin_render;
	float m_heightTrackMax_render;
	float m_heightTrackBias;
	bool  m_heightTrackDebugDraw;
	bool  m_heightTrackDepthBuffer; // sample depth buffer every frame to determine min/max recvr height (slow!)
	int   m_heightTrackDepthBufferSkip; // number of pixels to skip when sampling depth buffer (n x n)
	bool  m_heightTrackDepthBufferLock; // sample entire depth buffer for one frame (skip=1) and lock
	bool  m_heightTrackDepthBufferCalc;
	float m_heightRecvrMin;
	float m_heightRecvrMax;
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	int   m_numBuckets;
	bool  m_maxCountUpdate;
	int   m_maxDepthCount;
	int   m_maxHeightCount;
#if __DEV
	bool  m_debugTileEnabled; // for debugging a specific tile
	int   m_debugTileX;
	int   m_debugTileY;
	float m_debugTileDepthMin;
	float m_debugTileDepthMax;
	float m_debugTileHeightMin;
	float m_debugTileHeightMax;
#endif // __DEV

	atArray<CDepthHistogramBucket> m_buckets;
};

CDepthHistogram::CDepthHistogram()
{
	m_enabled                    = false;
#if __DEV
	m_dump                       = false;
#endif // __DEV
	m_depthRangeMin              = 0.0f;
	m_depthRangeMax              = 100.0f;
	m_depthPerspective           = 0.0f;
	m_heightRangeMin             = 0.0f;
	m_heightRangeMax             = 100.0f;
	m_heightRangeFull            = false;
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	m_heightTrackMin             = 0.0f;
	m_heightTrackMax             = 0.0f;
	m_heightTrackMin_render      = 0.0f;
	m_heightTrackMax_render      = 0.0f;
	m_heightTrackBias            = 0.0f;
	m_heightTrackDebugDraw       = false;
	m_heightTrackDepthBuffer     = false;
	m_heightTrackDepthBufferSkip = 5;
	m_heightTrackDepthBufferLock = false;
	m_heightTrackDepthBufferCalc = false;
	m_heightRecvrMin             = 0.0f;
	m_heightRecvrMax             = 0.0f;
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	m_numBuckets                 = 16;
	m_maxCountUpdate             = true;
	m_maxDepthCount              = 0;
	m_maxHeightCount             = 0;
#if __DEV
	m_debugTileEnabled           = true;
	m_debugTileX                 = 0;
	m_debugTileY                 = 0;
	m_debugTileDepthMin          = 0.0f;
	m_debugTileDepthMax          = 0.0f;
	m_debugTileHeightMin         = 0.0f;
	m_debugTileHeightMax         = 0.0f;
#endif // __DEV
}

void CDepthHistogram::Init()
{
	m_maxDepthCount  = 0;
	m_maxHeightCount = 0;
	m_buckets.clear();

	for (int i = 0; i < m_numBuckets; i++)
	{
		const float t0 = (float)(i + 0)/(float)m_numBuckets;
		const float t1 = (float)(i + 1)/(float)m_numBuckets;
		const float d0 = PerspectiveInterp(m_depthRangeMin, m_depthRangeMax, t0, m_depthPerspective);
		const float d1 = PerspectiveInterp(m_depthRangeMin, m_depthRangeMax, t1, m_depthPerspective);
		const float h0 = m_heightRangeMin + (m_heightRangeMax - m_heightRangeMin)*t0;
		const float h1 = m_heightRangeMin + (m_heightRangeMax - m_heightRangeMin)*t1;

		m_buckets.PushAndGrow(CDepthHistogramBucket(d0, d1, h0, h1));
	}
}

#if __DEV
void CDepthHistogram::Dump()
{
	m_dump = true;
}
#endif // __DEV

void CDepthHistogram::Update()
{
	if (m_enabled)
	{
		if (m_maxCountUpdate)
		{
			m_maxDepthCount  = 0;
			m_maxHeightCount = 0;
		}

		for (int i = 0; i < m_buckets.GetCount(); i++)
		{
			m_buckets[i].m_depthCount  = 0;
			m_buckets[i].m_heightCount = 0;
#if __DEV
			m_buckets[i].m_depthDebugFlag  = false;
			m_buckets[i].m_heightDebugFlag = false;
#endif // __DEV
		}

#if __DEV
		m_debugTileX         = ioMouse::GetX()/CTiledLighting::GetTileSize();
		m_debugTileY         = ioMouse::GetY()/CTiledLighting::GetTileSize();
		m_debugTileDepthMin  = 0.0f;
		m_debugTileDepthMax  = 0.0f;
		m_debugTileHeightMin = 0.0f;
		m_debugTileHeightMax = 0.0f;
#endif // __DEV

#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
		if (!m_heightTrackDepthBufferLock)
		{
			m_heightTrackMin = +99999.0f;
			m_heightTrackMax = -99999.0f;
		}
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK

#if __DEV
		m_dump = false;
#endif // __DEV

#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
		if (!m_heightTrackDepthBufferLock)
		{
			if (m_heightTrackDepthBuffer) // copy from renderthread
			{
				m_heightTrackMin = m_heightTrackMin_render;
				m_heightTrackMax = m_heightTrackMax_render;
			}

			m_heightTrackMin += m_heightTrackBias;
			m_heightTrackMax += m_heightTrackBias;
		}

		if (m_heightTrackDebugDraw) // draw big red square at the min height, so we can see if it pokes through
		{
			const grcViewport* vp = gVpMan.GetUpdateGameGrcViewport();

			const Vec3V p = Vec3V(vp->GetCameraPosition().GetXY(), ScalarV(m_heightTrackMin));
			const Vec3V p00 = p + Vec3V(-256.0f, -256.0f, 0.0f);
			const Vec3V p10 = p + Vec3V(+256.0f, -256.0f, 0.0f);
			const Vec3V p01 = p + Vec3V(-256.0f, +256.0f, 0.0f);
			const Vec3V p11 = p + Vec3V(+256.0f, +256.0f, 0.0f);

			grcDebugDraw::Poly(p00, p10, p11, Color32(255,0,0,255));
			grcDebugDraw::Poly(p00, p11, p01, Color32(255,0,0,255));
		}
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	}
}

#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK

void CDepthHistogram::UpdateHeightTrack_render()
{
	if (m_heightTrackDepthBuffer)
	{
		grcRenderTarget* rt = GBuffer::GetDepthTarget();
		const int w = rt->GetWidth ();
		const int h = rt->GetHeight();
		grcTextureLock lock;

		m_heightTrackMin_render = +99999.0f;
		m_heightTrackMax_render = -99999.0f;

		if (rt->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock))
		{
			const grcViewport* vp = gVpMan.GetRenderGameGrcViewport();

			const Mat34V localToWorld = vp->GetCameraMtx();
			const float  tanHFOV      = vp->GetTanHFOV();
			const float  tanVFOV      = vp->GetTanVFOV();

			const int skip = m_heightTrackDepthBufferCalc ? 1 : m_heightTrackDepthBufferSkip;

			for (int y = 0; y < h; y += skip)
			{
				for (int x = 0; x < w; x += skip)
				{
					const int addr = x + y*w;
#if __PS3
					u32 r = __lwbrx(&((u32*)lock.Base)[addr]); // load using lwbrx so compiler doesn't try to use lvlx (which doesn't work with vram)
					r = __lwbrx(&r); // reverse again .. *sigh*. or we could use __lwx, if it were available as an intrinsic
#else
					const u32 r = ((u32*)lock.Base)[addr]; // raw data from depth buffer
#endif
					const float d = (float)(r >> 8)/(float)((1<<24) - 1); // TODO -- to support on 360, convert floating point depth
					const float z = ShaderUtil::GetLinearDepth(vp, ScalarV(d)).Getf();

					if (z < 4900.0f) // sky is >4999, but let's be safe
					{
						const float fx = tanHFOV*(-1.0f + 2.0f*Clamp<float>(((float)x + 0.5f)/(float)GRCDEVICE.GetWidth (), 0.0f, 1.0f));
						const float fy = tanVFOV*(+1.0f - 2.0f*Clamp<float>(((float)y + 0.5f)/(float)GRCDEVICE.GetHeight(), 0.0f, 1.0f));
						const float fh = Transform(localToWorld, Vec3V(fx, fy, -1.0f)*ScalarV(z)).GetZf();

						m_heightTrackMin_render = Min<float>(fh, m_heightTrackMin_render);
						m_heightTrackMax_render = Max<float>(fh, m_heightTrackMax_render);
					}
				}
			}

			rt->UnlockRect(lock);

			m_heightTrackDepthBufferCalc = false;
		}
	}
}

#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK

#if __BANK

void CDepthHistogram::InitWidgets(bkBank& bk)
{
	bk.AddToggle("Enabled"           , &m_enabled);
#if __DEV
	bk.AddButton("Dump"              , datCallback(MFA(CDepthHistogram::Dump), this));
#endif // __DEV
	bk.AddSlider("Depth Range Min"   , &m_depthRangeMin, 0.0f, 512.0f, 1.0f/32.0f, datCallback(MFA(CDepthHistogram::Init), this));
	bk.AddSlider("Depth Range Max"   , &m_depthRangeMax, 0.0f, 512.0f, 1.0f/32.0f, datCallback(MFA(CDepthHistogram::Init), this));
	bk.AddSlider("Depth Perspective" , &m_depthPerspective, 0.0f, 1.0f, 1.0f/32.0f, datCallback(MFA(CDepthHistogram::Init), this));
	bk.AddSlider("Height Range Min"  , &m_heightRangeMin, 0.0f, 512.0f, 1.0f/32.0f, datCallback(MFA(CDepthHistogram::Init), this));
	bk.AddSlider("Height Range Max"  , &m_heightRangeMax, 0.0f, 512.0f, 1.0f/32.0f, datCallback(MFA(CDepthHistogram::Init), this));
	bk.AddToggle("Height Range Full" , &m_heightRangeFull, datCallback(MFA(CDepthHistogram::Init), this));
	bk.AddSlider("Num Buckets"       , &m_numBuckets, 1, 64, 1, datCallback(MFA(CDepthHistogram::Init), this));
	bk.AddToggle("Max Count Update"  , &m_maxCountUpdate);
#if __DEV
	bk.AddToggle("Debug Tile Enabled", &m_debugTileEnabled);
#endif // __DEV
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
	bk.AddSeparator();
	bk.AddSlider("Height Track Bias"                      , &m_heightTrackBias, -50.0f, 50.0f, 1.0f/32.0f);
	bk.AddToggle("Height Track Debug Draw"                , &m_heightTrackDebugDraw);
	bk.AddToggle("Height Track Depth Buffer (slow!)"      , &m_heightTrackDepthBuffer);
	bk.AddSlider("Height Track Depth Buffer Skip"         , &m_heightTrackDepthBufferSkip, 1, 16, 1);
	bk.AddToggle("Height Track Depth Buffer Lock (broken)", &m_heightTrackDepthBufferLock, datCallback(MFA(CDepthHistogram::UpdateHeightTrackDepthBufferLock), this));
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
}

void CDepthHistogram::DebugDraw() const
{
	if (m_enabled)
	{
		const Vec2V invScreenSize(1.0f/(float)GRCDEVICE.GetWidth(), 1.0f/(float)GRCDEVICE.GetHeight());

		const float boundsX0 = 40.0f;
		const float boundsY0 = 64.0f;
		const float boundsX1 = 256.0f + boundsX0;
		const float boundsY1 = 400.0f + boundsY0;

		const Vec2V boundsMin = Vec2V(boundsX0, boundsY0);
		const Vec2V boundsMax = Vec2V(boundsX1, boundsY1);

		if (m_maxDepthCount > 0)
		{
			for (int i = 0; i < m_buckets.GetCount(); i++) // depth histogram
			{
				const float y0 = (float)(i + 0)/(float)m_buckets.GetCount();
				const float y1 = (float)(i + 1)/(float)m_buckets.GetCount();
				const float x0 = 0.0f;
				const float x1 = (float)m_buckets[i].m_depthCount/(float)m_maxDepthCount;

				const Vec2V vMin = RoundToNearestIntNegInf(boundsMin + (boundsMax - boundsMin)*Vec2V(x0, y0));
				const Vec2V vMax = RoundToNearestIntNegInf(boundsMin + (boundsMax - boundsMin)*Vec2V(x1, y1)) - Vec2V(V_Y_AXIS_WZERO)*ScalarV(V_THREE);

				Color32 c = Color32(255,255,0,64);
#if __DEV
				if (m_buckets[i].m_depthDebugFlag)
				{
					c = Color32(0,255,64,64);
				}
#endif // __DEV
				grcDebugDraw::RectAxisAligned(invScreenSize*vMin, invScreenSize*vMax, c, true);
				grcDebugDraw::RectAxisAligned(invScreenSize*vMin, invScreenSize*vMax, Color32(255,255,0,255), false);
			}
		}

		if (m_maxHeightCount > 0)
		{
			for (int i = 0; i < m_buckets.GetCount(); i++) // height histogram
			{
				const float y0 = (float)(i + 0)/(float)m_buckets.GetCount();
				const float y1 = (float)(i + 1)/(float)m_buckets.GetCount();
				const float x0 = 1.2f;
				const float x1 = 1.2f + (float)m_buckets[i].m_heightCount/(float)m_maxHeightCount;

				const Vec2V vMin = RoundToNearestIntNegInf(boundsMin + (boundsMax - boundsMin)*Vec2V(x0, y0));
				const Vec2V vMax = RoundToNearestIntNegInf(boundsMin + (boundsMax - boundsMin)*Vec2V(x1, y1)) - Vec2V(V_Y_AXIS_WZERO)*ScalarV(V_THREE);

				Color32 c = Color32(0,64,255,64);
#if __DEV
				if (m_buckets[i].m_heightDebugFlag)
				{
					c = Color32(0,255,64,64);
				}
#endif // __DEV
				grcDebugDraw::RectAxisAligned(invScreenSize*vMin, invScreenSize*vMax, c, true);
				grcDebugDraw::RectAxisAligned(invScreenSize*vMin, invScreenSize*vMax, Color32(0,64,255,255), false);
			}
		}
#if __DEV
		if (m_debugTileEnabled)
		{
			const ScalarV tileX0 = ScalarV(Clamp<float>((float)((m_debugTileX + 0)*CTiledLighting::GetTileSize())/(float)GRCDEVICE.GetWidth (), 0.0f, 1.0f));
			const ScalarV tileY0 = ScalarV(Clamp<float>((float)((m_debugTileY + 0)*CTiledLighting::GetTileSize())/(float)GRCDEVICE.GetHeight(), 0.0f, 1.0f));
			const ScalarV tileX1 = ScalarV(Clamp<float>((float)((m_debugTileX + 1)*CTiledLighting::GetTileSize())/(float)GRCDEVICE.GetWidth (), 0.0f, 1.0f));
			const ScalarV tileY1 = ScalarV(Clamp<float>((float)((m_debugTileY + 1)*CTiledLighting::GetTileSize())/(float)GRCDEVICE.GetHeight(), 0.0f, 1.0f));

			grcDebugDraw::RectAxisAligned(Vec2V(tileX0, tileY0), Vec2V(tileX1, tileY1), Color32(0,255,64,64), true);
			grcDebugDraw::RectAxisAligned(Vec2V(tileX0, tileY0), Vec2V(tileX1, tileY1), Color32(0,255,64,255), false);

			Vector2 pos = Vector2(tileX1.Getf()*(float)GRCDEVICE.GetWidth(), tileY0.Getf()*(float)GRCDEVICE.GetHeight());

			pos += Vector2(4.0f, 8.0f);

			grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color32(255,255,255,255), atVarString("tile X=%d,Y=%d", m_debugTileX, m_debugTileY), true, 1.0f, 1.0f); pos.y += 9.0f;
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color32(255,255,255,255), atVarString("depth  min=%f,max=%f", m_debugTileDepthMin , m_debugTileDepthMax ), true, 1.0f, 1.0f); pos.y += 9.0f;
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color32(255,255,255,255), atVarString("height min=%f,max=%f", m_debugTileHeightMin, m_debugTileHeightMax), true, 1.0f, 1.0f); pos.y += 9.0f;
#if CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
			grcDebugDraw::Text(pos, DD_ePCS_Pixels, Color32(255,255,255,255), atVarString("track  min=%f,rec=%f", m_heightTrackMin, m_heightRecvrMin), true, 1.0f, 1.0f); pos.y += 9.0f;
#endif // CSM_DEPTH_HISTOGRAM_RECVR_HEIGHT_TRACK
			grcDebugDraw::TextFontPop();
		}
#endif // __DEV
	}
}

#endif // __BANK

CDepthHistogram gDepthHistogram;

#endif // CSM_DEPTH_HISTOGRAM
