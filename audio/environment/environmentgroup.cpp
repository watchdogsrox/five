
// Rage headers
#include "audioengine/controller.h"
#include "audioengine/curverepository.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiohardware/driverdefs.h"
#include "grcore/debugdraw.h"
#include "system/threadtype.h"

// Framework headers
//#include "fwmaths/Maths.h"

// Game headers
#include "audio/ambience/ambientaudioentity.h"
#include "audio/cutsceneaudioentity.h"
#include "audio/debugaudio.h"
#include "audio/environment/environmentgroup.h"
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "debug/debugglobals.h"
#include "modelinfo/MloModelInfo.h"
#include "modelinfo/MloModelInfo.h"
#include "pathserver/PathServer.h"
#include "peds/ped.h"
#include "modelinfo/MloModelInfo.h"
#include "vehicles/vehicle.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/portals/portal.h"
#include "scene/portals/PortalInst.h"

AUDIO_ENVIRONMENT_OPTIMISATIONS()

sysCriticalSectionToken naEnvironmentGroupCriticalSection::sm_Lock;
bool naEnvironmentGroupCriticalSection::sm_UseLock = false;

#if COMMERCE_CONTAINER
	FW_INSTANTIATE_CLASS_POOL_NO_FLEX_SPILLOVER(naEnvironmentGroup, MAX_ENVIRONMENT_GROUPS, 0.41f, atHashString("naEnvironmentGroup",0x737bae9d));
#else
	FW_INSTANTIATE_CLASS_POOL_SPILLOVER(naEnvironmentGroup, MAX_ENVIRONMENT_GROUPS, 0.41f, atHashString("naEnvironmentGroup",0x737bae9d));
#endif

BANK_ONLY(sysCriticalSectionToken naEnvironmentGroup::sm_DebugPrintToken);

bool g_DisableOcclusionIfForcedSameRoom = true;
u32 g_DeclinedPathServerRequestsThisFrame = 0;
u32 g_MaxAcceptedPathServerRequestsInAFrame = 0;
u32 g_AcceptedPathServerRequestsThisFrame = 0;

f32 g_ReverbSmallSmootherRate = 1.0f;
f32 g_ReverbMediumSmootherRate = 1.0f;
f32 g_ReverbLargeSmootherRate = 1.0f;
bool g_OverrideSourceEnvironment = false;
f32 g_OverridenSourceReverbWet = 0.0f;
f32 g_OverridenSourceReverbSize = 0.0f;
bool g_DisplayInteriorInfo = false;

f32 g_OutsideOcclusionDampingFactor = 0.4f;

f32 g_OcclusionProbeSmootherRate = 0.5f;
f32 g_OcclusionPortalSmootherRate = 1.25f;
f32 g_SameRoomRate = 2.0f;

bool g_FillSpace = false;
f32 g_LeakageFactorInLeakyCutscenes = 0.6f;
bool g_PretendCutsceneLeaks = false;
f32 g_DefoOccludedAdditionalAttenuation = -6.0f;

f32 g_MaxSharedOcclusionGroupDist = 2.0f;

// We don't want the occlusion to snap on and off when a probe is just missing/hitting geometry so dampen the occlusion
f32 g_ProbeBasedOcclusionDamping = 0.5f;

#if __BANK
extern bool g_DebugAllEnvironmentGroups;
extern bool g_HidePlayerMixGroup;
bool g_DisplaySourceEnvironmentDebug = false;
bool g_DisplayOcclusionDebug = false;
bool g_JustReverb = false;
bool g_DebugDrawMixGroups = false;
bool g_DrawEnvironmentGroupPoolDebugInfo = false;
u32 g_DebugKillMixGroupHash = 0;
bool g_DrawMixgroupBoundingBoxes = true;
bool g_HasPrintedEnvironmentPoolForFailedAlloc = false;
#endif


naEnvironmentGroup::naEnvironmentGroup()
	: m_LinkedEnvironmentGroup (NULL)
{
}

naEnvironmentGroup::~naEnvironmentGroup()
{
	// Make sure we don't leave a dangling pathserver request
	CancelAnyPendingSourceEnvironmentRequest();
}

void naEnvironmentGroup::InitClass()
{	
	naDisplayf("Sizeof naEnvironmentGroup: %" SIZETFMT "d", sizeof(naEnvironmentGroup));

	naEnvironmentGroup::InitPool(MEMBUCKET_AUDIO);
}

void naEnvironmentGroup::ShutdownClass()
{
	naEnvironmentGroup::ShutdownPool();
}

#if __BANK
naEnvironmentGroup* naEnvironmentGroup::Allocate(const char* debugIdentifier)
#else
naEnvironmentGroup* naEnvironmentGroup::Allocate(const char* UNUSED_PARAM(debugIdentifier))
#endif
{
	if(!naVerifyf(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Trying to allocate an environment group from the main thread while the audio update thread is running."))
	{
		return NULL;
	}

	if (naEnvironmentGroup::GetPool()->GetNoOfFreeSpaces()>0)
	{
		naEnvironmentGroup* group = rage_new naEnvironmentGroup;

#if __BANK
		if(group)
		{
			group->SetDebugIdentifier(debugIdentifier);
		}
#endif

		return group;
	}
#if __BANK
	else if(!g_HasPrintedEnvironmentPoolForFailedAlloc)
	{
		naEnvironmentGroup::DebugPrintEnvironmentGroupPool();
		Assertf(0, "Failed to allocate naEnvironmentGroup - environment pool usage printed to output");
		g_HasPrintedEnvironmentPoolForFailedAlloc = true;
	}
#endif

	return NULL;
}

#if __BANK
void naEnvironmentGroup::DebugDrawEnvironmentGroupPool()
{
	if (g_DrawEnvironmentGroupPoolDebugInfo)
	{
		SYS_CS_SYNC(naEnvironmentGroup::sm_DebugPrintToken);
		for (u32 i = 0; i < naEnvironmentGroup::GetPool()->GetSize(); i++)
		{
			naEnvironmentGroup* environmentGroup = naEnvironmentGroup::GetPool()->GetSlot(i);

			if (environmentGroup)
			{
				environmentGroup->DrawDebug();
			}
		}
	}	
}

void naEnvironmentGroup::DebugPrintEnvironmentGroupPool()
{
	SYS_CS_SYNC(naEnvironmentGroup::sm_DebugPrintToken);
	audDisplayf(",slot,identifier,entity,position,distance,references,sounds,soundtype,soundstate,lifetime,playtime");

	for(u32 i = 0; i < naEnvironmentGroup::GetPool()->GetSize(); i++)
	{
		naEnvironmentGroup* environmentGroup = naEnvironmentGroup::GetPool()->GetSlot(i);

		if(environmentGroup)
		{
			const char* identifier = environmentGroup->GetDebugIdentifier() != NULL? environmentGroup->GetDebugIdentifier() : "Unknown";
			const char* entityName = environmentGroup->GetEntity()? environmentGroup->GetEntity()->GetName():"no entity";
			const u32 numSoundReferences = environmentGroup->GetNumberOfSoundReference();
			const f32 distanceFromListener = environmentGroup->GetPosition().Dist(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()));

			char positionBuf[64];
			formatf(positionBuf, sizeof(positionBuf), "%.02f %.02f %.02f", environmentGroup->GetPosition().GetX(), environmentGroup->GetPosition().GetY(), environmentGroup->GetPosition().GetZ());					
			
			audDisplayf(", %d, %s, %s, %s, %.02f, %d", i, identifier, entityName, positionBuf, distanceFromListener, numSoundReferences);
			audSound::GetStaticPool().DebugPrintSoundsUsingEnvironmentGroup(environmentGroup, i);
		}
	}
}
#endif

void naEnvironmentGroup::Init(naAudioEntity* entity, f32 cheapDistance, u32 sourceEnvironmentUpdateTime, 
								u32 sourceEnvironmentCheapUpdateTime, f32 sourceEnvironmentMinimumDistanceUpdate, u32 maxUpdateTime)
{
	audEnvironmentGroupInterface::BaseInit(entity);

	m_CheapDistanceSqrd = cheapDistance*cheapDistance;
	m_IsUnderCover = false;
	m_HasGotUnderCoverResult = false;
	m_Position.Zero();
	m_PositionLastUpdate.Zero();

	// Source Environment stuff
	m_SourceEnvironmentTimeLastUpdated = 0;
	m_SourceEnvironmentPositionLastUpdate.Zero();
	Assign(m_SourceEnvironmentUpdateTime, sourceEnvironmentUpdateTime);
	Assign(m_SourceEnvironmentCheapUpdateTime, sourceEnvironmentCheapUpdateTime);
	m_SourceEnvironmentMinimumDistanceUpdateSquared = sourceEnvironmentMinimumDistanceUpdate*sourceEnvironmentMinimumDistanceUpdate;
	m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;
	m_HasUpdatedSourceEnvironment = false;
	m_ReverbSmallUnsmoothed = 0.0f;
	m_ReverbMediumUnsmoothed = 0.0f;
	m_ReverbLargeUnsmoothed = 0.0f;
	m_ReverbSmall = 0.0f;
	m_ReverbMedium = 0.0f;
	m_ReverbLarge = 0.0f;
	m_ReverbOverrideSmall = 0;
	m_ReverbOverrideMedium = 0;
	m_ReverbOverrideLarge = 0;
	m_ForceSourceEnvironmentUpdate = false;
	m_LinkedEnvironmentGroup = NULL;
	m_UseCloseProximityGroupsForUpdates = false;
	m_LastUpdateTime = 0;
	m_MaxUpdateTime = maxUpdateTime;
	m_FillsInteriorSpace = false;
	m_ForcedSameRoomAsListenerRatio = 0.f;
	m_IsCurrentlyFillingListenerSpace = false;
	m_InSameRoomAsListenerRatio = 0.0f;
	m_MaxLeakage = 1.0f;
	m_MinLeakageDistance = 0;
	m_MaxLeakageDistance = 0;
	m_UseInteriorDistanceMetrics = false;
	m_LeaksInTaggedCutscenes = false;
	m_HasValidRoomInfo = false;
	m_InteriorSettings = NULL;
	m_InteriorRoom = NULL;

	m_NotOccluded = false;

	m_FadeTime = 0;
	m_MixgroupState = State_Default;

	m_InteriorLocation.MakeInvalid();

	// Start high and then work these down through data, that way we can trim non-important sounds rather than have to do work for important ones.
	m_MaxPathDepth = audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth();	

	m_UsePortalOcclusion = true;

	m_OcclusionValue = 0.0f;
	m_ExtraOcclusion = 0.0f;
	m_HasUpdatedOcclusion = false;
	m_ForceFullOcclusion = false;
	m_ForceMaxPathDepth = false;
	m_IsVehicle = false;
	m_ForceUpdate = true;	// Force an update for new groups

#if __BANK
	m_DebugTextPosY = 0;
	m_DebugMeBaby = false;
#endif
}

void naEnvironmentGroup::SetSourceEnvironmentUpdates(u32 sourceEnvironmentUpdateTime, u32 sourceEnvironmentCheapUpdateTime,
								 f32 sourceEnvironmentMinimumDistanceUpdate)
{
	Assign(m_SourceEnvironmentUpdateTime, sourceEnvironmentUpdateTime);
	Assign(m_SourceEnvironmentCheapUpdateTime, sourceEnvironmentCheapUpdateTime);
	m_SourceEnvironmentMinimumDistanceUpdateSquared = sourceEnvironmentMinimumDistanceUpdate * sourceEnvironmentMinimumDistanceUpdate;
}

void naEnvironmentGroup::SetSource(const Vec3V& source)
{
	m_Position = VEC3V_TO_VECTOR3(source);
}

void naEnvironmentGroup::ForceSourceEnvironmentUpdate(const CEntity* owningEntity)
{
	m_ForceSourceEnvironmentUpdate = true;
	UpdateSourceEnvironment(audNorthAudioEngine::GetCurrentTimeInMs(), owningEntity);
	m_ForceSourceEnvironmentUpdate = false;
}

bool naEnvironmentGroup::ShouldCloseProximityGroupBeUsedForUpdate(naEnvironmentGroup* closeEnvironmentGroup, u32 timeInMs)
{
	if(closeEnvironmentGroup && closeEnvironmentGroup->GetLastUpdateTime() == timeInMs
		&& (m_Position - closeEnvironmentGroup->GetPosition()).Mag2() < g_MaxSharedOcclusionGroupDist*g_MaxSharedOcclusionGroupDist
		&& m_InteriorLocation.IsSameLocation(closeEnvironmentGroup->GetInteriorLocation()))
	{
		return true;
	}
	return false;
}

void naEnvironmentGroup::FullyUpdate(u32 timeInMs)
{
	m_LastUpdateTime = timeInMs;
}

void naEnvironmentGroup::UpdateSourceEnvironment(u32 timeInMs, const CEntity* owningEntity)
{
	DEBUG_DRAW_ONLY(static char gSourceEnvString[256];)

#if __BANK
	if (g_DisplaySourceEnvironmentDebug && (m_DebugMeBaby || g_DebugAllEnvironmentGroups))
	{
		char occlusionDebug[256] = "";
		sprintf(occlusionDebug, "Debug occlusion");
		grcDebugDraw::PrintToScreenCoors(occlusionDebug,20,20);
	}
#endif

	// Still need to run this if we've got a pending source update request
	if (m_LinkedEnvironmentGroup)
	{
		m_EnvironmentGameMetric.SetReverb(m_LinkedEnvironmentGroup->GetEnvironmentGameMetric());
		// Cache the values so if lose the linked group we can smooth off the values we're grabbing here
		m_ReverbSmall = m_LinkedEnvironmentGroup->GetEnvironmentGameMetric().GetReverbSmall();
		m_ReverbMedium = m_LinkedEnvironmentGroup->GetEnvironmentGameMetric().GetReverbMedium();
		m_ReverbLarge = m_LinkedEnvironmentGroup->GetEnvironmentGameMetric().GetReverbLarge();
		CancelAnyPendingSourceEnvironmentRequest();
		m_HasUpdatedSourceEnvironment = true;
		return;
	}

	// Don't update things with an update time of 0 - while testing
	if (m_SourceEnvironmentUpdateTime == 0 && !m_ForceSourceEnvironmentUpdate)
	{
		CancelAnyPendingSourceEnvironmentRequest();
	}
	else
	{
		audEntity* owningAudioEntity = ((audEnvironmentGroupInterface*)this)->GetEntity();
		naAudioEntity* owningNaAudioEntity = NULL;

		if (!owningEntity && owningAudioEntity)
		{
			owningNaAudioEntity = (naAudioEntity*)owningAudioEntity;
			owningEntity = owningNaAudioEntity->GetOwningEntity();
		}

		if(owningEntity)
		{
			m_InteriorLocation = owningEntity->GetAudioInteriorLocation();
		}
		
		// Treat interiors differently - unless they're subways, in which case go with the navmesh flow
		if (m_InteriorLocation.IsValid())
		{
			CancelAnyPendingSourceEnvironmentRequest();

			// Our fwInteriorLocation is out of date by 1 frame, so when the interior streams out, it's possible we'll get a NULL InteriorSettings
			// in which case don't do anything as the next frame our fwInteriorLocation will reflect that.
			const InteriorSettings* intSettings = NULL;
			const InteriorRoom* intRoom = NULL;
			audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInteriorLocation(GetRoomAttachedInteriorLocation(), intSettings, intRoom);
			if (intSettings && intRoom)
			{
				m_ReverbSmallUnsmoothed = intRoom->ReverbSmall;
				m_ReverbMediumUnsmoothed = intRoom->ReverbMedium;
				m_ReverbLargeUnsmoothed = intRoom->ReverbLarge;

				m_HasValidRoomInfo = true;
				m_InteriorSettings = intSettings;
				m_InteriorRoom = intRoom;

				m_EnvironmentGameMetric.SetIsInInterior(true);
			}
		}
		else
		{
			if (owningEntity)
			{
				// we'd know if we were in an interior, and we're not
				m_HasValidRoomInfo = true;
				m_InteriorSettings = NULL;
				m_InteriorRoom = NULL;
				m_InteriorLocation.MakeInvalid();
				m_EnvironmentGameMetric.SetIsInInterior(false);
			}

			if (m_SourceEnvironmentPathHandle != PATH_HANDLE_NULL)
			{
				// don't NULL out m_InteriorSettings here - we don't have an entity, so we'll only have set it on a force update with entity call,
				// which we'll only do for static things
				// we're waiting on a request, so let's see if it's done yet.
				EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_SourceEnvironmentPathHandle);
				if (ret==ERequest_Ready)
				{
					m_ForceSourceEnvironmentUpdate = false;
					u32 returnedSourceEnvironment[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
					for (s32 i=0; i<MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES; i++)
					{
						returnedSourceEnvironment[i] = 0;
					}
					u32 numResults = 0;
					float polyAreas[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
					Vector3 polyCentroids[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
					u8 polyFlags[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
					sysMemSet(&polyFlags,0,sizeof(polyFlags));
					EPathServerErrorCode errorCode = CPathServer::GetAudioPropertiesResult(m_SourceEnvironmentPathHandle, numResults, 
						returnedSourceEnvironment, polyAreas, polyCentroids, polyFlags);
					if (errorCode == PATH_NO_ERROR && numResults>0)
					{
						// The poly we're inside
						u32 sourceEnvironment = returnedSourceEnvironment[0];
						f32 ownPolySEReverbWet = audNorthAudioEngine::GetGtaEnvironment()->GetSourceEnvironmentReverbWetFromSEMetric(sourceEnvironment);
						f32 ownPolySEReverbSize = audNorthAudioEngine::GetGtaEnvironment()->GetSourceEnvironmentReverbSizeFromSEMetric(sourceEnvironment);

						// Neighbouring poly's
						f32 combinedNeighbourSEReverbWet = 0.0f;
						f32 combinedNeighbourSEReverbSize = 0.0f;
						f32 ownPolyPropWet = 1.0f;
						f32 ownPolyPropSize = 1.0f;
						u32 numNonZeroNeighbours = 0;
						m_IsUnderCover = (polyFlags[0] & CAudioRequest::FLAG_POLYGON_IS_SHELTERED);
						m_HasGotUnderCoverResult = true;
						if (numResults>1)
						{
							for (u32 i=1; i<numResults; i++)
							{
								if (returnedSourceEnvironment[i]!=0)
								{
									numNonZeroNeighbours++;
									combinedNeighbourSEReverbSize += audNorthAudioEngine::GetGtaEnvironment()->GetSourceEnvironmentReverbSizeFromSEMetric((u8)returnedSourceEnvironment[i]);
								}
								combinedNeighbourSEReverbWet += audNorthAudioEngine::GetGtaEnvironment()->GetSourceEnvironmentReverbWetFromSEMetric((u8)returnedSourceEnvironment[i]);
							}

							// How much do we take of our poly, and how much of our neighbours? If only one neighbour, 3/4 us, if 5, 1/3 us:
							combinedNeighbourSEReverbWet = combinedNeighbourSEReverbWet  / (numResults-1);
							ownPolyPropWet = audCurveRepository::GetLinearInterpolatedValue(0.75f, 0.33f, 1.0f, 5.0f, (f32)(numResults-1));
							if (numNonZeroNeighbours>0)
							{
								combinedNeighbourSEReverbSize = combinedNeighbourSEReverbSize / numNonZeroNeighbours;
								ownPolyPropSize = audCurveRepository::GetLinearInterpolatedValue(0.75f, 0.33f, 1.0f, 5.0f, (f32)numNonZeroNeighbours);
							}
						}

						f32 allPolysSEReverbWet = Lerp(ownPolyPropWet, combinedNeighbourSEReverbWet, ownPolySEReverbWet);
						f32 allPolysSEReverbSize = Lerp(ownPolyPropSize, combinedNeighbourSEReverbSize, ownPolySEReverbSize);

						audNorthAudioEngine::GetGtaEnvironment()->GetReverbFromSEMetric(allPolysSEReverbWet, allPolysSEReverbSize, &m_ReverbSmallUnsmoothed, &m_ReverbMediumUnsmoothed, &m_ReverbLargeUnsmoothed);
#if DEBUG_DRAW
						if ((owningEntity && owningEntity->GetIsTypePed() && ((CPed*)owningEntity)->IsPlayer()) ||
							(owningEntity && owningEntity->GetIsTypeVehicle() && ((CVehicle*)owningEntity)->GetStatus() == STATUS_PLAYER))
						{
							sprintf(gSourceEnvString, "ownSize: %f; ownWet: %f; avSize: %f; avWet: %f; verbSm: %.02f; verbMed: %.02f; verbLrg: %.02f", 
								ownPolySEReverbSize, ownPolySEReverbWet, allPolysSEReverbSize, allPolysSEReverbWet, 
								m_ReverbSmallUnsmoothed, m_ReverbMediumUnsmoothed, m_ReverbLargeUnsmoothed);
						}
#endif // DEBUG_DRAW
					}
					else
					{
						m_ReverbSmallUnsmoothed = 0.0f;
						m_ReverbMediumUnsmoothed = 0.0f;
						m_ReverbLargeUnsmoothed = 0.0f;
					}

					m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;
					// And now update our time and position, so we don't fire in another request too soon. 
					// (You could argue these should be set when the request is made, but better to update too slowly than thrash the pathserver.)
					// (Actually, I'll ALSO set them when the request goes in, to be super friendly.)
					m_SourceEnvironmentTimeLastUpdated = timeInMs;
					m_SourceEnvironmentPositionLastUpdate = m_Position;
				}
				else if(ret == ERequest_NotFound)
				{
					m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;
				}
				else
				{
					// If we've been waiting more than 2 seconds, something's broken - we've probably lost track of our path handle somehow
					// Or you've paused the game... or things have got really busy and there's nothing we can do... or it's a full moon.
					// I'll enable this locally and keep an eye on it.
					//naAssertf(timeInMs < (m_SourceEnvironmentTimeLastUpdated + 2000) || g_TempFakedPaused, "We've been waiting for a result from the path server for more than 2s in UpdateSourceEnvironment...it may be broken, busy or a full moon");
				}
			}
			else if (m_SourceEnvironmentUpdateTime!=1) // code for still updating, but only if we're in an interior, as that's cheap
			{
				// We don't have a pending request, so see if it's about time we did.
				// This is a bit crap - why aren't I using the proper mic position? (copied from occlusion update)
				Vector3 micPosition = camInterface::GetPos();
				// See if we should be using cheap checks
				Vector3 relativePosition = micPosition - m_Position;
				u32 minimumTimeToUpdate = m_SourceEnvironmentUpdateTime;
				if (relativePosition.Mag2() > m_CheapDistanceSqrd)
				{
					minimumTimeToUpdate = m_SourceEnvironmentCheapUpdateTime;
				}
				f32 distanceMovedSqrd = (m_Position - m_SourceEnvironmentPositionLastUpdate).Mag2();
				if (((timeInMs > (m_SourceEnvironmentTimeLastUpdated + minimumTimeToUpdate)) &&
					(distanceMovedSqrd > m_SourceEnvironmentMinimumDistanceUpdateSquared)) ||
					m_ForceSourceEnvironmentUpdate)
				{
					// It's been a while, and we've moved, so fire in an update request

					// If the owning entity is being tracked in the navmesh then we can make this query much more
					// efficient by passing in the tracked navmesh location & cached position, if valid.
					TNavMeshAndPoly knownNavmeshLocation;
					knownNavmeshLocation.Reset();
					Vector3 vSourcePos = m_Position;

#if __TRACK_PEDS_IN_NAVMESH
					if(owningEntity && owningEntity->GetIsTypePed())
					{
						CPed * pPed = (CPed*)owningEntity;
						if(pPed->GetNavMeshTracker().IsUpToDate(m_Position))
						{
							knownNavmeshLocation = pPed->GetNavMeshTracker().GetNavMeshAndPoly();
							vSourcePos = pPed->GetNavMeshTracker().GetLastPosition();
						}
					}
#endif //__TRACK_PEDS_IN_NAVMESH

					m_SourceEnvironmentPathHandle = CPathServer::RequestAudioProperties(vSourcePos, &knownNavmeshLocation, -1.0f, Vector3(0.0f, 0.0f, 0.0f), false, (fwEntity*)owningEntity);
					if (m_SourceEnvironmentPathHandle!=PATH_HANDLE_NULL)
					{
						// And make sure we don't update again for a while - we also update these number when we get the results back,
						// doing it here is mainly to ensure that a failed request doesn't start thrashing.
						m_SourceEnvironmentTimeLastUpdated = timeInMs;
						m_SourceEnvironmentPositionLastUpdate = m_Position;
#if __BANK
						audNorthAudioEngine::GetGtaEnvironment()->IncrementAcceptedPathServerRequestCount();
#endif

					}
					else
					{
#if __BANK
						audNorthAudioEngine::GetGtaEnvironment()->IncrementDeclinedPathServerRequestCount();
#endif
					}
				}
			}
		}

		if (g_OverrideSourceEnvironment)
		{
			audNorthAudioEngine::GetGtaEnvironment()->GetReverbFromSEMetric(g_OverridenSourceReverbWet, g_OverridenSourceReverbSize, &m_ReverbSmallUnsmoothed, &m_ReverbMediumUnsmoothed, &m_ReverbLargeUnsmoothed);
		}

		// Factor in the ambient environment zones.  The averaged ambient zone data is at the listener position, so this isn't quite right.
		// But the other option is getting the averaged rule data at each point for every environmentGroup which would probably be too expensive.
		const EnvironmentRule* avgEnvRule = g_AmbientAudioEntity.GetAmbientEnvironmentRule();
		const f32 envRuleReverbScale = g_AmbientAudioEntity.GetAmbientEnvironmentRuleReverbScale();
		if(avgEnvRule && envRuleReverbScale != 0.0f)
		{
			const f32 scaledReverbDamp = avgEnvRule->ReverbDamp * envRuleReverbScale;

			m_ReverbSmallUnsmoothed *= 1.0f - scaledReverbDamp;	
			m_ReverbMediumUnsmoothed *= 1.0f - scaledReverbDamp;
			m_ReverbLargeUnsmoothed *= 1.0f - scaledReverbDamp;

			m_ReverbSmallUnsmoothed = Max(m_ReverbSmallUnsmoothed, avgEnvRule->ReverbSmall * envRuleReverbScale);
			m_ReverbMediumUnsmoothed = Max(m_ReverbMediumUnsmoothed, avgEnvRule->ReverbMedium * envRuleReverbScale);
			m_ReverbLargeUnsmoothed = Max(m_ReverbLargeUnsmoothed, avgEnvRule->ReverbLarge * envRuleReverbScale);
		}

		m_ReverbSmall = GetSmoothedValue(timeInMs, m_ReverbSmallUnsmoothed, m_ReverbSmall, g_ReverbSmallSmootherRate, m_HasUpdatedSourceEnvironment);
		m_ReverbMedium = GetSmoothedValue(timeInMs, m_ReverbMediumUnsmoothed, m_ReverbMedium, g_ReverbMediumSmootherRate, m_HasUpdatedSourceEnvironment);
		m_ReverbLarge = GetSmoothedValue(timeInMs, m_ReverbLargeUnsmoothed, m_ReverbLarge, g_ReverbLargeSmootherRate, m_HasUpdatedSourceEnvironment);
	}

	m_EnvironmentGameMetric.SetReverbSmall(Max(m_ReverbSmall, m_ReverbOverrideSmall*0.01f));
	m_EnvironmentGameMetric.SetReverbMedium(Max(m_ReverbMedium, m_ReverbOverrideMedium*0.01f));
	m_EnvironmentGameMetric.SetReverbLarge(Max(m_ReverbLarge, m_ReverbOverrideLarge*0.01f));

	m_HasUpdatedSourceEnvironment = true;

	if (g_DisplayInteriorInfo)
	{
#if DEBUG_DRAW
		grcDebugDraw::PrintToScreenCoors(gSourceEnvString, 5,20);
#endif // DEBUG_DRAW
	}

#if __BANK
	if (g_DisplaySourceEnvironmentDebug && (m_DebugMeBaby || g_DebugAllEnvironmentGroups))
	{
		char occlusionDebug[256] = "";
		formatf(occlusionDebug, "Unsmoothed:    sml: %.2f; med: %.2f; lrg: %.2f;", m_ReverbSmallUnsmoothed, m_ReverbMediumUnsmoothed, m_ReverbLargeUnsmoothed);
		grcDebugDraw::Text(m_Position, Color32(255,0,0), 0, GetDebugTextPosY(), occlusionDebug, true, 5);
		formatf(occlusionDebug, "Smoothed:      sml: %.2f; med: %.2f; lrg: %.2f;", m_ReverbSmall, m_ReverbMedium, m_ReverbLarge);
		grcDebugDraw::Text(m_Position, Color32(255,0,0), 0, GetDebugTextPosY(), occlusionDebug, true, 5);
		formatf(occlusionDebug, "Override:      sml: %.2f; med: %.2f; lrg: %.2f;", m_ReverbOverrideSmall*0.01f, m_ReverbOverrideMedium*0.01f, m_ReverbOverrideLarge*0.01f);
		grcDebugDraw::Text(m_Position, Color32(255,0,0), 0, GetDebugTextPosY(), occlusionDebug, true, 5);
		formatf(occlusionDebug, "Final:         sml: %.2f; med: %.2f; lrg: %.2f;", 
			m_EnvironmentGameMetric.GetReverbSmall(),m_EnvironmentGameMetric.GetReverbMedium(), m_EnvironmentGameMetric.GetReverbLarge());
		grcDebugDraw::Text(m_Position, Color32(255,0,0), 0, GetDebugTextPosY(), occlusionDebug, true, 5);
		formatf(occlusionDebug, "Time Since Last Update: %d", timeInMs - m_SourceEnvironmentTimeLastUpdated);
		grcDebugDraw::Text(m_Position, Color32(255,0,0), 0, GetDebugTextPosY(), occlusionDebug, true, 5);
	}
#endif
	return;
}

void naEnvironmentGroup::ClearInteriorInfo()
{
	m_InteriorLocation.MakeInvalid();
	m_InteriorSettings = NULL;
	m_InteriorRoom = NULL;
}

void naEnvironmentGroup::SetInteriorLocation(const fwInteriorLocation interiorLocation)
{
	// If we're switching locations, it's important to do an update because there's a good chance we'll be repositioning the sound
	if(!interiorLocation.IsSameLocation(m_InteriorLocation))
	{
		SetShouldForceUpdate(true);
	}
	m_InteriorLocation = interiorLocation;
}

void naEnvironmentGroup::SetInteriorMetrics(const InteriorSettings* intSettings, const InteriorRoom* intRoom)
{
	if(naVerifyf(intSettings, "Null InteriorSettings* passed into SetInteriorSettingsWithHashkey")
		&& naVerifyf(intRoom, "Null InteriorRoom* passed into SetInteriorSettingsWithHashkey"))
	{
		m_ReverbSmall = intRoom->ReverbSmall;
		m_ReverbMedium = intRoom->ReverbMedium;
		m_ReverbLarge = intRoom->ReverbLarge;

		m_EnvironmentGameMetric.SetIsInInterior(true);

		m_HasValidRoomInfo = true;
		m_InteriorSettings = intSettings;
		m_InteriorRoom = intRoom;
	}
}

void naEnvironmentGroup::SetInteriorInfoWithEntity(const CEntity* entity)
{
	if(naVerifyf(entity, "Passed NULL CEntity* when setting environment group interior info"))
	{
		SetInteriorInfoWithInteriorLocation(entity->GetAudioInteriorLocation());
	}
}

void naEnvironmentGroup::SetInteriorInfoWithInteriorLocation(const fwInteriorLocation interiorLocation)
{
	m_HasValidRoomInfo = true;
	m_EnvironmentGameMetric.SetIsInInterior(false);

	SetInteriorLocation(interiorLocation);

	if(interiorLocation.IsValid())
	{
		const InteriorSettings* intSettings = NULL;
		const InteriorRoom* intRoom = NULL;
		audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInteriorLocation(interiorLocation, intSettings, intRoom);
		if(intSettings && intRoom)
		{
			SetInteriorMetrics(intSettings, intRoom);
		}
	}
}

void naEnvironmentGroup::SetInteriorInfoWithInteriorInstance(const CInteriorInst* intInst, const s32 roomIdx)
{
	// Set this to true, as we could be specifying that this interior is outside by passing a NULL CInteriorInst*
	m_HasValidRoomInfo = true;
	m_EnvironmentGameMetric.SetIsInInterior(false);

	if(intInst && roomIdx != INTLOC_INVALID_INDEX && roomIdx < (s32)intInst->GetNumRooms())
	{
		const InteriorSettings* intSettings = NULL;
		const InteriorRoom* intRoom = NULL;
		audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(intInst, roomIdx, intSettings, intRoom);
		if(intSettings && intRoom)
		{
			SetInteriorMetrics(intSettings, intRoom);
		}
		SetInteriorLocation(CInteriorProxy::CreateLocation(intInst->GetProxy(), roomIdx));
	}
}

void naEnvironmentGroup::SetInteriorInfoWithInteriorHashkey(const u32 interiorSettingsHash, const u32 intRoomHash)
{
	m_HasValidRoomInfo = true;
	m_EnvironmentGameMetric.SetIsInInterior(false);

	const InteriorSettings* interiorSettings = audNorthAudioEngine::GetObject<InteriorSettings>(interiorSettingsHash);
	const InteriorRoom* intRoom = audNorthAudioEngine::GetObject<InteriorRoom>(intRoomHash);
	if(interiorSettings && intRoom)
	{
		SetInteriorInfoWithInteriorGO(interiorSettings, intRoom);
	}
}

void naEnvironmentGroup::SetInteriorInfoWithInteriorGO(const InteriorSettings* interiorSettings, const InteriorRoom* intRoom)
{
	m_HasValidRoomInfo = true;
	m_EnvironmentGameMetric.SetIsInInterior(false);

	if(interiorSettings && intRoom)
	{
		SetInteriorMetrics(interiorSettings, intRoom);

		CInteriorInst::Pool* pool = CInteriorInst::GetPool();
		const s32 numSlots = pool->GetSize();
		for(s32 slot = 0; slot < numSlots; slot++)
		{
			const CInteriorInst* intInst = pool->GetSlot(slot);
			if(intInst && intInst->IsPopulated())
			{
				const CMloModelInfo* modelInfo = intInst->GetMloModelInfo();
				if(modelInfo)
				{
					const InteriorSettings* currentIntSettings = audNorthAudioEngine::GetObject<InteriorSettings>(modelInfo->GetHashKey());
					if(interiorSettings == currentIntSettings)
					{
						// We found the interior, so now try and find the room
						const s32 roomIdx = intInst->FindRoomByHashcode(intRoom->RoomName);
						if(naVerifyf(roomIdx != INTLOC_INVALID_INDEX, "naEnvironmentGroup::SetInteriorSettingsWithGOHashkey Couldn't find room by name in interior: %s", modelInfo->GetModelName()))
						{
							SetInteriorLocation(CInteriorProxy::CreateLocation(intInst->GetProxy(), roomIdx));
							break;
						}
					}
				}
			}
		}
	}
}

void naEnvironmentGroup::CancelAnyPendingSourceEnvironmentRequest()
{
	if (m_SourceEnvironmentPathHandle != PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_SourceEnvironmentPathHandle);
		m_SourceEnvironmentPathHandle = PATH_HANDLE_NULL;
	}
}

void naEnvironmentGroup::UpdateMixGroup()
{
	if(GetMixGroupIndex() < 0)
	{
		if(m_MixgroupState != State_Removed)
		{
			m_EnvironmentGameMetric.SetMixGroupApplyValue(0);
			m_FadeTime = 0;
			m_MixgroupState = State_Removed;
		}
		return;
	}

#if __BANK
	if(g_DebugKillMixGroupHash && g_DebugKillMixGroupHash == DYNAMICMIXMGR.GetMixGroupHash(GetMixGroupIndex()))
	{
		UnsetMixGroup();
	}
#endif

	//audDisplayf("Update mixgroup index adress: 0x%p", &m_MixGroupIndex);
	m_EnvironmentGameMetric.SetMixGroup(GetMixGroupIndex());
	m_EnvironmentGameMetric.SetPrevMixGroup(GetPrevMixGroupIndex());
	//Update the mixGroupApplyValue 
	s32 applyValue = m_EnvironmentGameMetric.GetMixGroupApplyValue();
	if (  m_FadeTime != 0 )
	{
		switch (m_MixgroupState)
		{
			case State_Adding:
			case State_Transitioning:
				applyValue += Max((u8)1,(u8)(255 * (fwTimer::GetTimeStep() / m_FadeTime)));
				if(applyValue >= 255)
				{
					m_EnvironmentGameMetric.SetMixGroupApplyValue(255);
					m_FadeTime = 0;
					SetPrevMixGroup(-1);
					m_EnvironmentGameMetric.SetPrevMixGroup(GetPrevMixGroupIndex());
					m_MixgroupState = State_Added;
				}
				else
				{
					m_EnvironmentGameMetric.SetMixGroupApplyValue((u8)applyValue);
				}
				break;
			case State_Removing:
				applyValue -= Max((u8)1,(u8)(255 * (fwTimer::GetTimeStep() / m_FadeTime)));
				if(applyValue <= 0)
				{
					m_EnvironmentGameMetric.SetMixGroupApplyValue(0);
					m_FadeTime = 0;
					m_MixgroupState = State_Removed;
					DEV_ONLY(dmDebugf1("Removing reference from mixgroup %s in environmentgroup line %u", g_AudioEngine.GetDynamicMixManager().GetMixGroupNameFromIndex(GetMixGroupIndex()), __LINE__);)
					UnsetMixGroup();
				}
				else
				{
					m_EnvironmentGameMetric.SetMixGroupApplyValue((u8)applyValue);
				}
				break;
			case State_Default: break;
			default: break;
		}
	}
	else if(applyValue > 0 && m_MixgroupState != State_Added)
	{
		m_EnvironmentGameMetric.SetMixGroupApplyValue(255);
		m_FadeTime = 0;
		SetPrevMixGroup(-1);
		m_EnvironmentGameMetric.SetPrevMixGroup(GetPrevMixGroupIndex());
		m_MixgroupState = State_Added;
	}
	else if(applyValue <= 0 && m_MixgroupState != State_Removed)
	{
		m_EnvironmentGameMetric.SetMixGroupApplyValue(0);
		m_FadeTime = 0;
		m_MixgroupState = State_Removed;
		DEV_ONLY(dmDebugf1("Removing reference from mixgroup %s in environmentgroup line %u", g_AudioEngine.GetDynamicMixManager().GetMixGroupNameFromIndex(GetMixGroupIndex()), __LINE__);)
		UnsetMixGroup();
	}
}

void naEnvironmentGroup::UpdateOcclusion(u32 timeInMs, const bool isPlayerEnvGroup)
{
#if __DEV
	if (g_DisplayOcclusionDebug && (m_DebugMeBaby || g_DebugAllEnvironmentGroups))
	{
		naAssertf(1, "Debug assert in UpdateOcclusion");
	}
	if (g_DisplayOcclusionDebug && (m_DebugMeBaby || g_DebugAllEnvironmentGroups))
	{
		char occlusionDebug[256] = "";
		sprintf(occlusionDebug, "Debug occlusion");
		grcDebugDraw::PrintToScreenCoors(occlusionDebug,20,20);
	}
#endif

	if (m_LinkedEnvironmentGroup) 
	{
		m_EnvironmentGameMetric.SetOcclusion(m_LinkedEnvironmentGroup->GetEnvironmentGameMetric());
		m_HasUpdatedOcclusion = true;
		return;
	}

	// We have InteriorSettings but an invalid interior location if the interior info was set before the CInteriorInst finished streaming in, so try again.
	if (m_InteriorSettings && !m_InteriorLocation.IsValid())
	{
		SetInteriorInfoWithInteriorGO(m_InteriorSettings, m_InteriorRoom);
	}

	// Grab this here because it can be a lot of work if the interior location is attached to a portal
	const fwInteriorLocation intLoc = GetRoomAttachedInteriorLocation();

	const InteriorSettings* listenerInteriorSettings = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorSettingsPtr();
	const InteriorRoom* listenerInteriorRoom = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorRoomPtr();

	// We'll turn this to true if we have a valid occlusion path that we're processing
	SetUseOcclusionPath(false);

	f32 occlusionValue = 1.0f;			// Mark us as fully occluded, and we'll mark it down from there.
	f32 occlusionDamping = 1.0f;		
	f32 sameRoomRatio = 0.0f;			
	bool forceFullOcclusion = m_ForceFullOcclusion;
	f32 defoOccludedAdditionalAttenuation = 0.0f;
	bool isCurrentlyFillingListenerSpace = false;

	// Smooth portal vs. probe occlusion separately so we can slowly smooth jumps in probes, whereas portal occlusion has less jumps and is more accurate.
	f32 occlusionSmoothRate = g_OcclusionProbeSmootherRate;	

	// portal occlusion metrics that are set in ComputeOcclusionPathMetrics
	f32 pathDistance = 0.0f;
	Vector3 playPos;

	// If we never set any interior information then just do what we can and use the probes
	if(!m_HasValidRoomInfo)
	{
		occlusionValue = audNorthAudioEngine::GetOcclusionManager()->GetBoundingBoxOcclusionFactorForPosition(m_Position);
		occlusionDamping *= g_ProbeBasedOcclusionDamping;
		occlusionSmoothRate = g_OcclusionProbeSmootherRate;
	}
	else
	{
		// Is the listener inside?
		if(listenerInteriorSettings && (listenerInteriorRoom && AUD_GET_TRISTATE_VALUE(listenerInteriorSettings->Flags, FLAG_ID_INTERIORSETTINGS_USEBOUNDINGBOXOCCLUSION) != AUD_TRISTATE_TRUE))
		{
			// Is the source in the same room and the same interior as the listener?
			if(listenerInteriorSettings == m_InteriorSettings && listenerInteriorRoom == m_InteriorRoom)
			{
				occlusionValue = audNorthAudioEngine::GetOcclusionManager()->GetBoundingBoxOcclusionFactorForPosition(m_Position);
				sameRoomRatio = 1.0f;
				isCurrentlyFillingListenerSpace = true;
				occlusionDamping = listenerInteriorRoom->RoomOcclusionDamping;
				occlusionDamping *= g_ProbeBasedOcclusionDamping;
				occlusionSmoothRate = g_OcclusionProbeSmootherRate;
			}
			else	// the source is in a different room or outside
			{
				if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled() && m_UsePortalOcclusion)
				{
					if(audNorthAudioEngine::GetOcclusionManager()->ComputeOcclusionPathMetrics(this, playPos, pathDistance, occlusionValue))
					{
						if(naVerifyf(FPIsFinite(pathDistance), "non-finite pathDistance"))
						{
							SetPlayPosition(playPos);
							SetPathDistance(pathDistance);
							SetUseOcclusionPath(true);
							occlusionSmoothRate = g_OcclusionPortalSmootherRate;
						}
					}
					else
					{
						occlusionValue = 1.0f;
						occlusionDamping = 1.0f;
						occlusionSmoothRate = g_OcclusionProbeSmootherRate;
					}
				}
				else
				{
					const bool isRoomInLinkedRoomList = (m_InteriorSettings && audNorthAudioEngine::GetGtaEnvironment()->IsRoomInLinkedRoomList(m_InteriorSettings, m_InteriorRoom));

					// For non-path based occlusion, if in a different room in the same interior or a linked interior use the probes
					// and if outside, then use the exterior occlusion data ( which is based off portals )
					if(m_InteriorSettings == listenerInteriorSettings || isRoomInLinkedRoomList)
					{
						occlusionValue = audNorthAudioEngine::GetOcclusionManager()->GetBoundingBoxOcclusionFactorForPosition(m_Position);
						occlusionDamping *= g_ProbeBasedOcclusionDamping;
						occlusionSmoothRate = g_OcclusionProbeSmootherRate;
					}
					else	// it's outside or in an interior that's not linked
					{
						occlusionValue = audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionFor3DPosition(m_Position);
						occlusionDamping *= g_ProbeBasedOcclusionDamping;
						occlusionSmoothRate = g_OcclusionProbeSmootherRate;
					}
				}
			}
		}
		else	// Listener is outside
		{
			// Is the source inside?  Need to check both IntSettings and the roomIdx, because doors leading outside will have m_InteriorSettings, but a roomIdx of 0 ( limbo )_
			if(m_InteriorSettings && (intLoc.GetRoomIndex() != INTLOC_ROOMINDEX_LIMBO && AUD_GET_TRISTATE_VALUE(m_InteriorSettings->Flags, FLAG_ID_INTERIORSETTINGS_USEBOUNDINGBOXOCCLUSION) != AUD_TRISTATE_TRUE))
			{
				// If the source is inside, use the source's interior info to determine how much to dampen the occlusion based on the portals
				if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled() && m_UsePortalOcclusion)
				{
					if(intLoc.IsValid() && 
						audNorthAudioEngine::GetOcclusionManager()->AreOcclusionPathsLoadedForInterior(CInteriorProxy::GetFromLocation(intLoc)) &&
						audNorthAudioEngine::GetOcclusionManager()->ComputeOcclusionPathMetrics(this, playPos, pathDistance, occlusionValue))
					{
						if(naVerifyf(FPIsFinite(pathDistance), "non-finite pathDistance"))
						{
							SetPlayPosition(playPos);
							SetPathDistance(pathDistance);
							SetUseOcclusionPath(true);
							occlusionSmoothRate = g_OcclusionPortalSmootherRate;
						}
					}
					else
					{
						occlusionValue = 1.0f;
						occlusionDamping = 1.0f;
						occlusionSmoothRate = g_OcclusionProbeSmootherRate;
					}
				}
				else
				{
					occlusionValue = audNorthAudioEngine::GetOcclusionManager()->GetBoundingBoxOcclusionFactorForPosition(m_Position);
					occlusionDamping *= g_ProbeBasedOcclusionDamping;
					occlusionSmoothRate = g_OcclusionProbeSmootherRate;
				}
			}
			else	// Both the source and the listener are outside.
			{
				// Do custom occlusion for vehicles so they don't pop in and out when outside due to probe occlusion and their speed
				f32 vehOcclusion = 0.0f;
				if(m_IsVehicle && GetEntity())
				{
					audVehicleAudioEntity* vehAudEnt = static_cast<audVehicleAudioEntity*>(GetEntity());
					if(vehAudEnt)
					{
						// Note that it's important to check audVehicleAudioEntity::GetIsInAirCached() from the audio thread as CVehicle::IsInAir is unsafe.
						vehOcclusion = audNorthAudioEngine::GetOcclusionManager()->ComputeVehicleOcclusion(vehAudEnt->GetVehicle(), vehAudEnt->GetIsInAirCached());
					}
				}
				const f32 boundingBoxOcclusion = audNorthAudioEngine::GetOcclusionManager()->GetBoundingBoxOcclusionFactorForPosition(m_Position);

				occlusionValue = Max(vehOcclusion, boundingBoxOcclusion);
				occlusionDamping = g_OutsideOcclusionDampingFactor;
				occlusionSmoothRate = g_OcclusionProbeSmootherRate;
			}
		}
	}

	// Don't occlude the player's group, also don't use path metrics so we don't get any panning issues due to distant cameras.
	if(isPlayerEnvGroup)
	{
		occlusionDamping = 0.0f;
		SetUseOcclusionPath(false);
	}

	sameRoomRatio = Max(sameRoomRatio, m_ForcedSameRoomAsListenerRatio);	
	m_InSameRoomAsListenerRatio = GetSmoothedValue(timeInMs, sameRoomRatio, m_InSameRoomAsListenerRatio, g_SameRoomRate, m_HasUpdatedOcclusion);
	m_IsCurrentlyFillingListenerSpace = isCurrentlyFillingListenerSpace || m_ForcedSameRoomAsListenerRatio == 1.f;

	occlusionValue *= occlusionDamping;

	if(!naVerifyf(FPIsFinite(occlusionValue), "Non-finite occlusion value computed"))
	{
		occlusionValue = 1.f;
	}

	if (g_DisableOcclusionIfForcedSameRoom && m_ForcedSameRoomAsListenerRatio > 0.f)
	{
		occlusionValue = 0.f;
	}

	// Now that we're done calculating a final result, override it here.
	if(forceFullOcclusion)
	{
		occlusionValue = 1.0f;
		defoOccludedAdditionalAttenuation = g_DefoOccludedAdditionalAttenuation;
	}
	if(m_NotOccluded)
	{
		occlusionValue = 0.0f;
	}
	
#if __ASSERT
	if (audVerifyf(FPIsFinite(occlusionValue), "naEnvironmentGroup::UpdateOcclusion detected non-finite occlusionValue"))
	{
		audAssertf(occlusionValue >= 0.0f && occlusionValue <= 1.0f, "naEnvironmentGroup::UpdateOcclusion detected occlusionValue out of range (%f)", occlusionValue);
	}
#endif

	occlusionValue += m_ExtraOcclusion;
	occlusionValue = Clamp(occlusionValue, 0.0f, 1.0f);

	m_OcclusionValue = GetSmoothedValue(timeInMs, occlusionValue, m_OcclusionValue, occlusionSmoothRate, m_HasUpdatedOcclusion);

	f32 occlusionAttenuationLinear = 1.0f - rage::Powf(m_OcclusionValue, 10);

	// In order to get an even change in the LPF, we need to base it off the pitch, not the frequency.
	f32 occlusionCutoffLinear = Lerp(1.0f - m_OcclusionValue, 0.25f, 1.0f);

	// Use the linear vol and filter values so the environment_game update doesn't need to convert dB->Lin, dampen, and then Lin->dB.
	m_EnvironmentGameMetric.SetOcclusionAttenuationLin(occlusionAttenuationLinear * audDriverUtil::ComputeLinearVolumeFromDb(defoOccludedAdditionalAttenuation));
	m_EnvironmentGameMetric.SetOcclusionFilterCutoffLin(occlusionCutoffLinear);
	
	Vector3 relativePosition;
	if(m_EnvironmentGameMetric.GetUseOcclusionPath())
	{
		relativePosition = VEC3V_TO_VECTOR3(m_EnvironmentGameMetric.GetOcclusionPathPosition()) - audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	}
	else
	{
		relativePosition = m_Position - audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
	}

	// Check how much leakage we should give it too
	f32 occlusionLeakage = 0.8f * m_OcclusionValue;
	// Now see how much to tone that down by distance
	static const audThreePointPiecewiseLinearCurve leakageCurve(10.0f, 1.0f, 28.0f, 0.0f, 51.0f, 0.0f);

	f32 relativeDistance = relativePosition.Mag();
	f32 distanceOcclusionLeakageDamping = leakageCurve.CalculateValue(relativeDistance);
	f32 leakage = occlusionLeakage*distanceOcclusionLeakageDamping;

	if(m_MaxLeakage > 0.0f)
	{
		f32 fillsSpaceLeakage = 0.0f;
		if(m_FillsInteriorSpace)
		{
			fillsSpaceLeakage = m_InSameRoomAsListenerRatio * m_MaxLeakage;
		}

		f32 distanceLeakage = 0.0f;
		if(m_MaxLeakageDistance > 0.0f)
		{
			distanceLeakage = RampValueSafe(relativeDistance, (f32)m_MinLeakageDistance, (f32)m_MaxLeakageDistance, m_MaxLeakage, 0.0f);
		}

		m_EnvironmentGameMetric.SetDistanceAttenuationDamping(1.0f - Max(fillsSpaceLeakage, distanceLeakage));

		leakage = Max(leakage, fillsSpaceLeakage, distanceLeakage);
	}
	else
	{
		m_EnvironmentGameMetric.SetDistanceAttenuationDamping(1.0f);
	}

	// If we're positioned radio, and we're in a leaky cutscene, leak
	// For now - let's leak in all cutscenes, even scripted ones
	//	if (m_LeaksInTaggedCutscenes && ((CCutsceneManager::IsStreamedCutscene() && CCutsceneManager::ShouldLeakRadio()) || g_PretendCutsceneLeaks))
	if (m_LeaksInTaggedCutscenes && ((CutSceneManager::GetInstance()&&CutSceneManager::GetInstance()->IsStreamedCutScene()) || g_PretendCutsceneLeaks))
	{
		leakage = Max(leakage, g_LeakageFactorInLeakyCutscenes);
	}

	naAssertf(leakage<1.1f && leakage>-0.1f, "naEnvironmentGroup::UpdateOcclusion Occlusion group leakage out of range");
	m_EnvironmentGameMetric.SetLeakageFactor(leakage);

	if(!m_HasUpdatedOcclusion)
	{
		m_HasUpdatedOcclusion = true;
	}

#if __BANK
	if(g_JustReverb && listenerInteriorSettings && listenerInteriorSettings==m_InteriorSettings)
	{
		m_EnvironmentGameMetric.SetJustReverb(true);
	}
	else
	{
		m_EnvironmentGameMetric.SetJustReverb(false);
	}

	if (g_DisplayOcclusionDebug && (m_DebugMeBaby || g_DebugAllEnvironmentGroups))
	{
		if(m_EnvironmentGameMetric.GetUseOcclusionPath())
		{
			grcDebugDraw::Sphere(m_EnvironmentGameMetric.GetOcclusionPathPosition(), 0.5f, Color_aquamarine2, true);
			grcDebugDraw::Line(m_Position, VEC3V_TO_VECTOR3(m_EnvironmentGameMetric.GetOcclusionPathPosition()), Color_aquamarine3);
		}
		char occlusionDebug[256] = "";
		f32 distance = relativePosition.Mag();
		sprintf(occlusionDebug, "Occlusion: us:%.2f; s:%.2f; lpf:%.0f; vol:%.2f; damp:%.2f;", occlusionValue, m_OcclusionValue, 	
				audDriverUtil::ComputeHzFrequencyFromLinear(occlusionCutoffLinear), audDriverUtil::ComputeDbVolumeFromLinear(occlusionAttenuationLinear), occlusionDamping);
		grcDebugDraw::Text(m_Position, Color32(255,0,0), 0, GetDebugTextPosY(), occlusionDebug, true, 5);
		sprintf(occlusionDebug, "dist:%.2f; leak:%.2f; srr:%.2f;", distance, leakage, m_InSameRoomAsListenerRatio);
		grcDebugDraw::Text(m_Position, Color32(255,0,0), 0, GetDebugTextPosY(), occlusionDebug, true, 5);
	}
#endif

	return;
}

bool naEnvironmentGroup::IsInADifferentInteriorToListener()
{
	// We could consider returning false if we're in a subway and so's the listener - this might currently be too restrictive
	if (m_InteriorSettings && m_InteriorSettings!=audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorSettingsPtr())
	{
		return true;
	}
	return false;
}

f32 naEnvironmentGroup::GetSmoothedValue(u32 timeInMs, f32 input, f32 valueLastFrame, f32 smootherRate, bool hasHadUpdate)
{
	f32 smoothedValue;

	if(!hasHadUpdate)
	{
		smoothedValue = input;
	}
	else
	{
		const f32 maxChange = (timeInMs - m_LastUpdateTime) * smootherRate * 0.001f;
		const f32 newValueIncreasing = Min(input, (valueLastFrame + maxChange));
		const f32 newValueDecreasing = Max(input, (valueLastFrame - maxChange));
		smoothedValue = Selectf(input - valueLastFrame, newValueIncreasing, newValueDecreasing);
		smoothedValue = Clamp(smoothedValue, 0.f, 1.f);
	}

	return smoothedValue;
}

fwInteriorLocation naEnvironmentGroup::GetRoomAttachedInteriorLocation() const
{
	fwInteriorLocation intLoc = m_InteriorLocation;
	if(m_InteriorLocation.IsAttachedToPortal())
	{
		intLoc = audNorthAudioEngine::GetGtaEnvironment()->GetRoomIntLocFromPortalIntLoc(m_InteriorLocation);
	}
	return intLoc;
}

s32 naEnvironmentGroup::GetRoomIdx() const
{
	// Initialize to "outside"
	s32 roomIdx = INTLOC_ROOMINDEX_LIMBO;

	// We can set the m_InteriorSettings prior to getting a valid m_InteriorLocation because we're waiting for the CInteriorInst to stream in
	// So if we don't have an m_InteriorSettings we know we're definitely outside
	if(m_InteriorSettings)
	{
		if(m_InteriorLocation.IsValid())
		{
			const fwInteriorLocation roomAttachedIntLoc = GetRoomAttachedInteriorLocation();
			roomIdx = roomAttachedIntLoc.GetRoomIndex();
		}
		else
		{
			// We know the envGroup is inside because we have m_InteriorSettings, but we don't have a valid intLoc because the CInteriorInst hasn't streamed in yet.
			// So set the room to an invalid index, so we don't try and process portal occlusion paths
			roomIdx = INTLOC_INVALID_INDEX;
		}
	}

	return roomIdx;
}

const CInteriorInst* naEnvironmentGroup::GetInteriorInst() const
{
	// Initialize to "outside"
	const CInteriorInst* intInst = NULL;

	if(m_InteriorLocation.IsValid())
	{
		const fwInteriorLocation roomAttachedIntLoc = GetRoomAttachedInteriorLocation();
		intInst = CInteriorInst::GetInteriorForLocation(roomAttachedIntLoc);
	}

	return intInst;
}

#if __BANK
void naEnvironmentGroup::DrawDebug()
{
	m_DebugTextPosY = -50;

	if(g_DebugDrawMixGroups && GetMixGroupIndex() >= 0)
	{
		u32 mixGroupHash = DYNAMICMIXMGR.GetMixGroupHash(GetMixGroupIndex());
		if(mixGroupHash !=0)
		{
			if(g_HidePlayerMixGroup && mixGroupHash == ATSTRINGHASH("PLAYER_GROUP", 0xC5FA40AC))
			{
				return;
			}
			Color32 mixGroupColor = Color32(mixGroupHash | 0xFF000000);
			Color32 mixGroupColorAlpha = mixGroupColor;
			mixGroupColorAlpha.SetAlpha(96);

			grcDebugDraw::Sphere(GetPosition() ,0.5f, mixGroupColorAlpha, true);
			const char * name = DYNAMICMIXMGR.GetMetadataManager().GetObjectName(mixGroupHash);
			if(!name)
			{
				name = "Invalid Mixgroup";
			}
			Vector3 namePos(GetPosition());
			namePos.z += 0.7f;
			grcDebugDraw::Text(namePos, mixGroupColor, 0, GetDebugTextPosY(), name, true);
			
			if(g_DrawMixgroupBoundingBoxes)
			{
				DrawEntityBoundingBox(mixGroupColor);
			}	
		}
	}

	if (g_DrawEnvironmentGroupPoolDebugInfo)
	{
		char occlusionDebug[256] = "";
		
		formatf(occlusionDebug, "Ptr:           %llu", this);
		grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);

		if (m_InteriorSettings)
		{
			formatf(occlusionDebug, "Interior:      %s;", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_InteriorSettings->NameTableOffset));	
			grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);
		}

		if (m_InteriorRoom)
		{
			formatf(occlusionDebug, "Room:          %s;", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_InteriorRoom->NameTableOffset));
			grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);
		}

		formatf(occlusionDebug, "Occlusion:     Val:%.2f; Extra:%.2f; Same Room Ratio:%.2f", m_OcclusionValue, m_ExtraOcclusion, m_InSameRoomAsListenerRatio);
		grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);		
		formatf(occlusionDebug, "Unsmoothed:    sml: %.2f; med: %.2f; lrg: %.2f;", m_ReverbSmallUnsmoothed, m_ReverbMediumUnsmoothed, m_ReverbLargeUnsmoothed);
		grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);
		formatf(occlusionDebug, "Smoothed:      sml: %.2f; med: %.2f; lrg: %.2f;", m_ReverbSmall, m_ReverbMedium, m_ReverbLarge);
		grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);
		formatf(occlusionDebug, "Override:      sml: %.2f; med: %.2f; lrg: %.2f;", m_ReverbOverrideSmall*0.01f, m_ReverbOverrideMedium*0.01f, m_ReverbOverrideLarge*0.01f);
		grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);
		formatf(occlusionDebug, "Final:         sml: %.2f; med: %.2f; lrg: %.2f;",
			m_EnvironmentGameMetric.GetReverbSmall(), m_EnvironmentGameMetric.GetReverbMedium(), m_EnvironmentGameMetric.GetReverbLarge());
		grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);
		formatf(occlusionDebug, "Time Since Last Update: %d", audNorthAudioEngine::GetCurrentTimeInMs() - m_SourceEnvironmentTimeLastUpdated);
		grcDebugDraw::Text(m_Position, Color32(255, 0, 0), 0, GetDebugTextPosY(), occlusionDebug, true);

		grcDebugDraw::Sphere(GetPosition(), 0.5f, Color32(0.f, 1.f, 0.f, 0.3f), true);
	}
}

void PrintEnvironmentGroupPoolInfo()
{
	naEnvironmentGroup::DebugPrintEnvironmentGroupPool();
}

void naEnvironmentGroup::AddWidgets(bkBank &bank)
{
	bank.AddToggle("Disable Occlusion if Forced Same Room", &g_DisableOcclusionIfForcedSameRoom);
	bank.AddToggle("Draw Environment Group Pool Debug Info", &g_DrawEnvironmentGroupPoolDebugInfo);
	bank.AddButton("PrintEnvironmentGroupInfo", CFA(PrintEnvironmentGroupPoolInfo));
	bank.AddToggle("DisplaySourceEnvironmentDebug", &g_DisplaySourceEnvironmentDebug);
	bank.AddToggle("DisplayOcclusionDebug", &g_DisplayOcclusionDebug);
	bank.AddSlider("OutsideOcclusionDampingFactor", &g_OutsideOcclusionDampingFactor, 0.0f, 1.0f, 0.001f);
	bank.AddSlider("OcclusionProbeSmootherRate", &g_OcclusionProbeSmootherRate, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("OcclusionPortalSmootherRate", &g_OcclusionPortalSmootherRate, 0.0f, 10.0f, 0.01f);
	bank.AddToggle("OverrideSourceEnvironment", &g_OverrideSourceEnvironment);
	bank.AddSlider("ReverbSmallSmootherRate", &g_ReverbSmallSmootherRate, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("ReverbMediumSmootherRate", &g_ReverbMediumSmootherRate, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("ReverbLargeSmootherRate", &g_ReverbLargeSmootherRate, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("SourceReverbWet", &g_OverridenSourceReverbWet, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("SourceReverbSize", &g_OverridenSourceReverbSize, 0.0f, 1.0f, 0.01f);
	bank.AddToggle("DisplayInteriorInfo", &g_DisplayInteriorInfo);
	bank.AddToggle("FillSpace", &g_FillSpace);
	bank.AddToggle("g_JustReverb", &g_JustReverb);
	bank.AddSlider("SameRoomRate", &g_SameRoomRate, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("LeakageFactorInLeakyCutscenes", &g_LeakageFactorInLeakyCutscenes, 0.0f, 1.0f, 0.01f);
	bank.AddToggle("PretendCutsceneLeaks", &g_PretendCutsceneLeaks);
	bank.AddSlider("DefoOccludedAdditionalAttenuation", &g_DefoOccludedAdditionalAttenuation, -100.0f, 0.0f, 1.0f);
	bank.AddSlider("MaxSharedOcclusionGroupDist", &g_MaxSharedOcclusionGroupDist, 0.0f, 20.0f, 1.0f);
	bank.AddSlider("ProbeBasedOcclusionDamping", &g_ProbeBasedOcclusionDamping, 0.0f, 1.0f, 0.01f);
}
#endif
