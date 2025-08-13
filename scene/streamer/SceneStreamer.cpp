/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamer.cpp
// PURPOSE : based on the scene streamer from GTAIV
// AUTHOR :  John Whyte, Ian Kiigan
// CREATED : 09/10/09
//
/////////////////////////////////////////////////////////////////////////////////

//framework includes
#include "fwscene/world/InteriorLocation.h"

//game includes
#include "camera/CamInterface.h"
#include "debug/GtaPicker.h"
#include "frontend/PauseMenu.h"
#include "scene/Entity.h"
#include "scene/portals/PortalTracker.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "Renderer/RenderPhases/RenderPhase.h"
#include "scene/LoadScene.h"
#include "scene/portals/PortalTracker.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/streamer/SceneStreamer.h"
#include "scene/streamer/SceneStreamerList.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/streamer/StreamVolume.h"

#if __BANK
#include "fwdebug/vectormap.h"
#include "scene/debug/SceneStreamerDebug.h"
#include "vector/colors.h"
#endif	//__BANK


#if STREAM_STATS
EXT_PF_VALUE_INT(PriorityRequestsODD);
EXT_PF_VALUE_INT(SecondaryRequestsODD);
EXT_PF_VALUE_INT(PriorityRequestsHDD);
EXT_PF_VALUE_INT(SecondaryRequestsHDD);
#endif // STREAM_STATS

// If this is true, we don't differentiate between data on optical disc and harddrive.
// We'll just assume everything is on hard drive. This just means that we don't keep separate quotas,
// we'll use one for all, and we'll use the hard drive quotas (which are optimized for better streaming
// quality (=important things first) rather than LSN seeks). On a lower level, we'll still use the
// proper streaming queues.
#define ALL_MAP_DATA_ON_HARDDRIVE		(1)



#define IMMEDIATE_CHECK_IF_LOADED (1)

int PriorityCountMin[pgStreamer::DEVICE_COUNT] = { CSceneStreamerBase::PRIORITYCOUNT_MIN, 2 };
int LowPriorityCountMin[pgStreamer::DEVICE_COUNT] = { 0, 0 };
int TotalRequestMax[pgStreamer::DEVICE_COUNT] = { CSceneStreamerBase::SECONDARYLIST_MAX, 8 };

float LowPriorityCutoff = (float) STREAMBUCKET_VEHICLESANDPEDS_CUTSCENE;

SCENE_OPTIMISATIONS();

#if __BANK
bool	CSceneStreamer::sm_FullSVSceneLog = false;
#endif // __BANK

bool	CSceneStreamer::sm_EnableMasterCutoff = true;
bool	CSceneStreamer::sm_NewSceneStreamer = true;

CSceneStreamer::CSceneStreamer()
: m_PrioRequests(0)
{
}

void CSceneStreamer::IssueRequests(BucketEntry::BucketEntryArray &entityList, int bucketCutoff)
{
	if (!m_EnableIssueRequests)
	{
		return;
	}

	STRVIS_SET_CONTEXT(strStreamingVisualize::SCENESTREAMER);

	u32 streamFlags = 0;
	int entityCount = entityList.GetCount();
	int limit = Max(entityCount - SECONDARYLIST_MAX, 0);

	const bool newSceneStreamer = /*sm_NewSceneStreamer*/ true;
	const bool isBurstMode = strStreamingEngine::GetBurstMode();

	float cutoffScore = (float) bucketCutoff;
	int requestCount[pgStreamer::DEVICE_COUNT] = {0};
	int nonLowRequestCount[pgStreamer::DEVICE_COUNT] = {0};
	bool throttled[pgStreamer::DEVICE_COUNT] = {0};
	int totalRequestCount = 0;

	if ( g_PlayerSwitch.GetLongRangeMgr().IsActive() && g_PlayerSwitch.GetLongRangeMgr().ShouldPinAssets() )
	{
		streamFlags |= STRFLAG_LOADSCENE;
	}

	if (g_LoadScene.IsActive() && CStreamVolumeMgr::IsAnyVolumeActive() && (camInterface::IsFadedOut() || CPauseMenu::GetPauseRenderPhasesStatus()))
	{
		streamFlags |= STRFLAG_LOADSCENE;
	}
	bool sceneStreamingDisabled = CStreaming::IsSceneStreamingDisabled();

	int prioRequests[pgStreamer::DEVICE_COUNT] = {0};
#if STREAM_STATS
	int secondaryRequests[pgStreamer::DEVICE_COUNT] = {0};
#endif // STREAM_STATS

	float masterCutoff = g_SceneStreamerMgr.GetStreamingLists().GetMasterCutoff();

	if (!sm_EnableMasterCutoff)
	{
		// Disable it.
		masterCutoff = 0.0f;
	}

#if __BANK
	bool requestAllPriorities = CSceneStreamerDebug::RequestAllPriorities();
#endif // __BANK

	ASSERT_ONLY(float prevScore = FLT_MAX;)
	//STRVIS_SET_MARKER_TEXT("Scene Streamer, EC=%d, limit=%d", entityCount, limit);

	for (s32 i=entityCount-1; i>=limit; i--)
	{
		float score = entityList[i].m_Score;
		Assertf(score <= prevScore, "Scene streamer list not sorted by score - %f vs %f at %d/%d", score, prevScore, i, entityCount);
		ASSERT_ONLY(prevScore = score;)

		CEntity* pEntity = entityList[i].GetEntity();
		bool isPriority = score >= cutoffScore;
		bool isLowPriority = score <= LowPriorityCutoff;

		if (score < masterCutoff)
		{
			if (totalRequestCount == 0)
			{
				//STRVIS_SET_MARKER_TEXT("Master cut-off stalled streaming. Slowly opening floodgates.");
				// There's nothing to stream. Let things slowly trickle in.
				STR_TELEMETRY_MESSAGE_WARNING("(streaming/scenestreamer)%s has score %f, the cut-off is %f, but nothing else is trying to stream - moving cut-off",
					pEntity->GetModelName(), score, masterCutoff);

				masterCutoff = score;
				g_SceneStreamerMgr.GetStreamingLists().SetMasterCutoff(score);
			}
			else
			{
				//STRVIS_SET_MARKER_TEXT("Master cut-off slowed us to %d requests.", totalRequestCount);
				STR_TELEMETRY_MESSAGE_WARNING("(streaming/scenestreamer)Invoking master cut-off, trying to stream %s at %f, but cut-off is %f",
					pEntity->GetModelName(), score, masterCutoff);

#if __BANK
				if (sm_FullSVSceneLog && strStreamingVisualize::IsInstantiated())
				{
					// Send a fake list of requests to indicate the remainder of the scene.
					STRVIS_SET_CONTEXT(strStreamingVisualize::FAILMASTERCUTOFF);
					SendFakeSceneRemainder(entityList, i-1);
				}
#endif // __BANK

				break;
			}
		}

		pgStreamer::Device device = pgStreamer::OPTICAL;

		if (newSceneStreamer)
		{
			strLocalIndex objIndex = strLocalIndex(pEntity->GetModelIndex());
			strIndex index = CModelInfo::GetStreamingModule()->GetStreamingIndex(objIndex);

			// Don't consider invalid archetypes.
			if (!strStreamingEngine::GetInfo().GetStreamingInfoRef(index).IsInImage())
			{
				//STRVIS_SET_MARKER_TEXT("%d - Invalid archetype", i);
				continue;
			}

#if ALL_MAP_DATA_ON_HARDDRIVE
			device = pgStreamer::HARDDRIVE;
#else // ALL_MAP_DATA_ON_HARDDRIVE
			u32 deviceMask = strStreamingEngine::GetInfo().GetDependencyDeviceMask(index);

			// Let's consider it "optical" if it involves streaming from optical, "harddrive" otherwise.
			device = (deviceMask & (1 << pgStreamer::OPTICAL)) ? pgStreamer::OPTICAL : pgStreamer::HARDDRIVE;
#endif // ALL_MAP_DATA_ON_HARDDRIVE


			if (requestCount[device] >= TotalRequestMax[device])
			{
				throttled[device] = true;
				//STRVIS_SET_MARKER_TEXT("Throttled with %d requests", requestCount[device]);

#if __BANK
				if (sm_FullSVSceneLog && strStreamingVisualize::IsInstantiated())
				{
					// Send a fake list of requests to indicate the remainder of the scene.
					STRVIS_AUTO_CONTEXT(strStreamingVisualize::FAILTHROTTLE);
					SendFakeSceneRequest(entityList[i]);
				}
#endif // __BANK

				if (ALL_MAP_DATA_ON_HARDDRIVE || throttled[1-device])
				{
#if __BANK
					if (sm_FullSVSceneLog && strStreamingVisualize::IsInstantiated())
					{
						// Send a fake list of requests to indicate the remainder of the scene.
						STRVIS_AUTO_CONTEXT(strStreamingVisualize::FAILTHROTTLE);
						SendFakeSceneRemainder(entityList, i-1);
					}
#endif // __BANK
					break;
				}

				continue;
			}
		}
		
		bool throttle = !isPriority && prioRequests[device] > PriorityCountMin[device];
		throttle |= isLowPriority && nonLowRequestCount[device] > LowPriorityCountMin[device];

		if (throttle && !isBurstMode BANK_ONLY(&& !requestAllPriorities))
		{
			//STRVIS_SET_MARKER_TEXT("Throttled with %d prio, %d low, %d/%d", prioRequests[device], requestCount[device], (int) isPriority, (int) isLowPriority);
			// If there are a certain number of high priority requests, serve those first before
			// moving on to the secondary ones.
			//break;
			throttled[device] = true;

			// If both devices are full, we can stop now.
			if (ALL_MAP_DATA_ON_HARDDRIVE || !newSceneStreamer || throttled[1-device])
			{
				break;
			}

			continue;
		}

		STR_TELEMETRY_MESSAGE_LOG("(streaming/scenestreamer)Requesting assets for %s, score=%f (prio=%d), master cut-off at %f", pEntity->GetModelName(), score, (int) isPriority, masterCutoff);
		STRVIS_SET_SCORE(score);

		int prevRealRequests = strStreamingEngine::GetInfo().GetNumberRealObjectsRequested();

		// don't need to store high priority requests as they are always requested immediately
		if(sceneStreamingDisabled || CModelInfo::RequestAssets(pEntity->GetBaseModelInfo(), streamFlags))
		{
			int realRequestCount = strStreamingEngine::GetInfo().GetNumberRealObjectsRequested() - prevRealRequests;

#if IMMEDIATE_CHECK_IF_LOADED
			// sometimes dummy requests are satisfied immediately...
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			bool wasCreated = false;
			
			if (pModelInfo->GetHasLoaded() && !pEntity->GetDrawHandlerPtr())
			{
				STR_TELEMETRY_MESSAGE_LOG("(streaming/scenestreamer)Insta-creating %s", pEntity->GetModelName());
				pEntity->CreateDrawable();
				wasCreated = true;
				//STRVIS_SET_MARKER_TEXT("%s instacreated, real Count=%d", pEntity->GetModelName(), realRequestCount);
			}

			// Don't count this as a request if it didn't spawn off any physical requests - no need to throttle us due to that.
			wasCreated |= (realRequestCount == 0);

			//STRVIS_SET_MARKER_TEXT("%d: %s requested, real Count=%d", i, pEntity->GetModelName(), realRequestCount);

			if (!wasCreated || !newSceneStreamer)
#endif	//IMMEDIATE_CHECK_IF_LOADED
			{
#if __BANK
				g_SceneStreamerMgr.GetStreamingLists().GetNeededEntityList().GetBucketSet().TrackRequest((int) entityList[i].m_Score);
				if (CSceneStreamerDebug::DrawRequestsOnVmap()) { fwVectorMap::Draw3DBoundingBoxOnVMap(pEntity->GetBoundBox(), (isPriority) ? Color_red : Color_green3); }
#endif	//__BANK

				requestCount[device]++;
				totalRequestCount++;

				if (!isLowPriority)
				{
					nonLowRequestCount[device]++;
				}

				if (isPriority)
				{
					strStreamingEngine::SetIsLoadingPriorityObjects(true);
					prioRequests[device]++;
				}
				else
				{
#if STREAM_STATS
					secondaryRequests[device]++;
#endif // STREAM_STATS
				}
			}
		}
		else
		{
			//STRVIS_SET_MARKER_TEXT("Discovered invalid archetype %s", pEntity->GetModelName());

			if (!sceneStreamingDisabled)
			{
				// If we get here, it means that RequestAssets failed, which probably means that the model is missing
				// entities - that's an asset problem. If we don't fix this, we'll try to stream this asset forever and
				// might hang on a loading screen.
				pEntity->GetBaseModelInfo()->FlagModelAsMissing();
			}

		}

		STRVIS_RESET_SCORE();
	}

#if __BANK
	if (sm_FullSVSceneLog && strStreamingVisualize::IsInstantiated())
	{
		// Send a fake list of requests to indicate the remainder of the scene.
		STRVIS_SET_CONTEXT(strStreamingVisualize::FAILLIMIT);
		SendFakeSceneRemainder(entityList, limit-1);
	}
#endif // __BANK


#if STREAM_STATS
	PF_SET(PriorityRequestsODD, prioRequests[0]);
	PF_SET(PriorityRequestsHDD, prioRequests[1]);
	PF_SET(SecondaryRequestsODD, secondaryRequests[0]);
	PF_SET(SecondaryRequestsHDD, secondaryRequests[1]);
#endif // STREAM_STATS

	m_PrioRequests = prioRequests[0] + prioRequests[1];

	//STRVIS_SET_MARKER_TEXT("Finish scene stremer, %d/%d requests", m_PrioRequests, requestCount[0] + requestCount[1]);
	STRVIS_SET_CONTEXT(strStreamingVisualize::NONE);
}

#if __BANK
void CSceneStreamer::SendFakeSceneRemainder(const BucketEntry::BucketEntryArray &entityList, int firstEntityIndex)
{
	for (;firstEntityIndex >= 0; --firstEntityIndex)
	{
		SendFakeSceneRequest(entityList[firstEntityIndex]);
	}
}

void CSceneStreamer::SendFakeSceneRequest(const BucketEntry &entry)
{
	float score = entry.m_Score;
	const CEntity* pEntity = entry.GetEntity();

	strLocalIndex objIndex = strLocalIndex(pEntity->GetModelIndex());
	strIndex index = CModelInfo::GetStreamingModule()->GetStreamingIndex(objIndex);

	// Don't consider invalid archetypes.
	if (strStreamingEngine::GetInfo().GetStreamingInfoRef(index).IsInImage())
	{
		STRVIS_SET_SCORE(score);
		STREAMINGVIS.Request(index, 0);
		STREAMINGVIS.Unrequest(index);
	}
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsHighPriorityEntity
// PURPOSE:		returns true if specified entity is high priority
//////////////////////////////////////////////////////////////////////////
bool CSceneStreamer::IsHighPriorityEntity(const CEntity* RESTRICT pEntity, const Vector3& vCamDir, const Vector3& vCamPos) const
{
#if __BANK
	if (CSceneStreamerDebug::TraceSelected() && pEntity==g_PickerManager.GetSelectedEntity())
	{
		__debugbreak();
	}
#endif

	bool bHighPriority = false;

	// for camera outside, then high priority is a narrow frustum along the view vector
	fwInteriorLocation primaryIntLoc = CPortalVisTracker::GetInteriorLocation();

	if (!primaryIntLoc.IsValid())
	{
		if(!pEntity->IsBaseFlagSet(fwEntity::LOW_PRIORITY_STREAM))
		{
			Vector3 vCentre;
			float fRadius = pEntity->GetBoundCentreAndRadius(vCentre);

			Vector3 vDir = vCentre - vCamPos;
			float fDist = vDir.Mag();

			vDir *= 1.0f / (fDist+0.00001f); // normalize the direction
			float fDot = vDir.Dot(vCamDir);

			if(fDot < 0.0f)
			{
				// if true, sphere overlaps camera so high priority
				// otherwise it is behind us so forget it
				bHighPriority = (fDist < fRadius);
			}
			else
			{
				// we want the cos of the angle subtended by the sphere, which is adjacent/hypot. Adjacent is dist to sphere
				float fHypot = sqrtf(fDist*fDist+fRadius*fRadius);
				float fCosofApproxAngleSubtendedBySphere = (fDist / fHypot);
				float fSubtendedAngle = acosf(fCosofApproxAngleSubtendedBySphere);

				// 2.0 because subtendedAngle is actually half angle
				if(fSubtendedAngle*2.0f*180.0f/PI>ms_fScreenYawForPriorityConsideration &&	// if it subtends a small angle then ignore it
					acosf(fDot)-fSubtendedAngle<(ms_fHighPriorityYaw/180.0f)*PI)				// does it overlap a central cone
				{
					bHighPriority = true;
				}
			}
		}
	}
	// for camera inside, high priority is those entities in the same interior as the camera
	else 
	{
		s32 interiorProxyID = primaryIntLoc.GetInteriorProxyIndex();

		if (pEntity->GetInteriorLocation().GetInteriorProxyIndex() == interiorProxyID)
		{
			bHighPriority = true;
		}
	}

	return bHighPriority;
}

#if __BANK
void CSceneStreamer::AddWidgets(bkGroup &group)
{
	group.AddSlider("ODD - Max priority reqs before scheduling secondary", &PriorityCountMin[pgStreamer::OPTICAL], 0, 300, 1);
	group.AddSlider("HDD - Max priority reqs before scheduling secondary", &PriorityCountMin[pgStreamer::HARDDRIVE], 0, 300, 1);
	group.AddSlider("ODD - Max regular requests before scheduling low", &LowPriorityCountMin[pgStreamer::OPTICAL], 0, 300, 1);
	group.AddSlider("HDD - Max regular requests before scheduling low", &LowPriorityCountMin[pgStreamer::HARDDRIVE], 0, 300, 1);
	group.AddSlider("ODD - Total max requests", &TotalRequestMax[pgStreamer::OPTICAL], 0, 300, 1);
	group.AddSlider("HDD - Total max requests", &TotalRequestMax[pgStreamer::HARDDRIVE], 0, 300, 1);
	group.AddToggle("Send full scene request list to SV", &sm_FullSVSceneLog);
}
#endif // __BANK
