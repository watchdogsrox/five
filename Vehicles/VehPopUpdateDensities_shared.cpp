#include "vehicles/vehiclepopulation.h"

#include "vehicles/vehpopulation_channel.h"

#include "fwmaths/random.h"

#include "math/amath.h"
#include "math/angmath.h"

float CVehiclePopulation::CVehGenLink::GetVehicleDensityContributionFromLink(const CVehiclePopulation::CVehGenLink &from) const
{
	// Density contribution computation logic abstracted from GetAssociatedPathLinks_AreaSearch()

	// Don't scan further than 200m (desired spacings could get greater than this if multiplier is very low)
	const float fMaxScanRange = Min(ms_fCurrentVehiclesSpacing[1], 200.0f);

	const Vec3V fromPos(
		(float) ((from.m_iMinX + from.m_iMaxX) >> 1),
		(float) ((from.m_iMinY + from.m_iMaxY) >> 1),
		(float) from.m_iMaxZ
	);

	const int iMinX = (int)(fromPos.GetXf() - fMaxScanRange);
	const int iMinY = (int)(fromPos.GetYf() - fMaxScanRange);
	const int iMinZ = (int)(fromPos.GetZf() - fMaxScanRange);
	const int iMaxX = (int)(fromPos.GetXf() + fMaxScanRange);
	const int iMaxY = (int)(fromPos.GetYf() + fMaxScanRange);
	const int iMaxZ = (int)(fromPos.GetZf() + fMaxScanRange);

	if(m_iMaxX > iMinX && m_iMaxY > iMinY && m_iMaxZ > iMinZ 
		&& iMaxX > m_iMinX && iMaxY > m_iMinY && iMaxZ > m_iMinZ)
	{
		const float fScanDistanceSquared = ms_fCurrentVehiclesSpacing[m_iDensity] * ms_fCurrentVehiclesSpacing[m_iDensity];

		const Vec3V toPos(
			(float) ((m_iMinX + m_iMaxX) >> 1),
			(float) ((m_iMinY + m_iMaxY) >> 1),
			(float) m_iMaxZ
		);

		Vec3V vOriginToPoint = fromPos - toPos;

		// Only go to the expense of recalculating cost for this link if it was within range of the new vehicle's creation position
		if(MagSquared(vOriginToPoint).Getf() < fScanDistanceSquared)
		{
			const bool bMatchLinkHeading = m_iDensity > 2;

			const float fSameHdgEps = 110.0f * DtoR;
			const float fHdgContributionFalloffStart = 80.0f * DtoR;
			const float fHdgContributionRange = (fSameHdgEps - fHdgContributionFalloffStart);

			const float fVerticalFalloffStart = m_bOverlappingRoads ? ms_popGenDensitySamplingVerticalFalloffStart * 0.25f : ms_popGenDensitySamplingVerticalFalloffStart;
			const float fVerticalFalloffEnd = m_bOverlappingRoads ? ms_popGenDensitySamplingVerticalFalloffEnd * 0.25f : ms_popGenDensitySamplingVerticalFalloffEnd;

			const float fVerticalFalloffRange = fVerticalFalloffEnd - fVerticalFalloffStart;

			const float fHdgDelta = Abs(rage::SubtractAngleShorterFast(GetLinkHeading(), from.GetLinkHeading()));

			if(!(ms_bSearchLinksMustMatchVehHeading && bMatchLinkHeading && fHdgDelta > fSameHdgEps))
			{
				Vec3V vNormal(ANGLE_TO_VECTOR_X(GetLinkHeading() * RtoD), ANGLE_TO_VECTOR_Y(GetLinkHeading() * RtoD), 0.0f);

				const float fHeadingScale = (bMatchLinkHeading && fHdgDelta > fHdgContributionFalloffStart) ? 1.0f - ((fHdgDelta - fHdgContributionFalloffStart) / fHdgContributionRange) : 1.0f;
				const float fZDist = vOriginToPoint.GetZf();

				ScalarV fScalar = Dot(vNormal, vOriginToPoint);
				const Vec3V vPlanarOffset = vNormal * fScalar;
				ScalarV fPlanarDistance = MagXYFast(vPlanarOffset);

				if(IsTrue(fScalar < ScalarV(V_ZERO)))
				{
					fPlanarDistance = -fPlanarDistance;
				}

				const float fGradeOffset = fPlanarDistance.Getf() * GetLinkGrade();

				const float fContribution = 1.0f - Clamp((Abs(fZDist - fGradeOffset) - ms_popGenDensitySamplingVerticalFalloffStart) / fVerticalFalloffRange, 0.0f, 1.0f);
				return fContribution * fHeadingScale;
			}
		}
	}

	return 0.0f;
}

void CVehiclePopulation::CVehGenLink::ComputeScore()
{
	// Score computation logic moved from SelectAndUpdateVehGenLinkToGenerateFrom()
	static dev_float fScaleDownForNonZoomed = 0.25f;

	m_fScore = m_fDesiredNearbyVehicleCountDelta;

	if(m_iDensity > 2)
	{
		if(m_iCategory == CPopGenShape::GC_KeyHoleOuterBand_on)
		{
			const float fDensityScale = ((float)m_iDensity+1) / 16.0f;
			m_fScore += ms_popGenOuterBandLinkScoreBoost * fDensityScale;
		}
	}

	if(ms_bZoomed && m_fScore > 0.0f)
	{
		if(m_iCategory != CPopGenShape::GC_KeyHoleOuterBand_on && m_iCategory != CPopGenShape::GC_KeyHoleSideWall_on)
		{
			m_fScore *= fScaleDownForNonZoomed;
		}
	}

	if(m_iDensity > 10)
	{
		m_fScore -= 0.5f;
	}
}

int CVehiclePopulation::ActiveLinkScoreComparison(const void *a, const void *b)
{
	int aI = *reinterpret_cast<const int*>(a);
	int bI = *reinterpret_cast<const int*>(b);

#if __BANK
	if(CVehiclePopulation::ms_displayInnerKeyholeDisparities && ms_aGenerationLinks[aI].m_bIsWithinKeyholeShape && !ms_aGenerationLinks[bI].m_bIsWithinKeyholeShape && ms_aGenerationLinks[bI].m_fScore > ms_fMinGenerationScore)
	{
		return 1;
	}
	else if(CVehiclePopulation::ms_displayInnerKeyholeDisparities && !ms_aGenerationLinks[aI].m_bIsWithinKeyholeShape && ms_aGenerationLinks[bI].m_bIsWithinKeyholeShape && ms_aGenerationLinks[aI].m_fScore > ms_fMinGenerationScore)
	{
		return -1;
	}
	else 
#endif // __BANK
	if(ms_aGenerationLinks[aI].m_fScore > ms_fMinGenerationScore && ms_aGenerationLinks[bI].m_fScore < ms_fMinGenerationScore)
	{
		return -1;
	}
	else if(ms_aGenerationLinks[aI].m_fScore < ms_fMinGenerationScore && ms_aGenerationLinks[bI].m_fScore > ms_fMinGenerationScore)
	{
		return 1;
	}
	else if(!ms_bZoomed && ms_aGenerationLinks[aI].m_bOnPlayersRoad && !ms_aGenerationLinks[bI].m_bOnPlayersRoad && ms_aGenerationLinks[aI].m_fDensity < ms_fOnPlayersRoadPrioritizationThreshold)
	{
		return -1;
	}
	else if(!ms_bZoomed && !ms_aGenerationLinks[aI].m_bOnPlayersRoad && ms_aGenerationLinks[bI].m_bOnPlayersRoad && ms_aGenerationLinks[bI].m_fDensity < ms_fOnPlayersRoadPrioritizationThreshold)
	{
		return 1;
	}
	else if(ms_aGenerationLinks[aI].m_fScore > ms_aGenerationLinks[bI].m_fScore)
	{
		return -1;
	}
	else if(ms_aGenerationLinks[aI].m_fScore < ms_aGenerationLinks[bI].m_fScore)
	{
		return 1;
	}

	return fwRandom::GetRandomNumberInRange(0, 1) == 1 ? -1 : 1;
}

void CVehiclePopulation::UpdateDensities(sScheduledLink* scheduledLinks, u32 numScheduledLinks)
{
	// Update the test and density measures for the active links.
	// All active links must be up to date.
	OUTPUT_ONLY(u32 updatedLinks = 0;)

	//**********************************************************************************
	// For all active paths links determine the number of vehicles near the path link.

	u32 currTime = fwTimer::GetTimeInMilliseconds();

	// Reset active links
	ms_numActiveVehGenLinks = 0;
	ms_iNumActiveLinks = 0;

#if __BANK
	CVehiclePopulation::ms_innerKeyholeDisparities = 0;
#endif // __BANK

	/////////////////////////////
	// set link activity first
	for( u32 i =0; i < ms_numGenerationLinks; ++i)
	{
		bool isActive = false;

		const CPathNode * pNodeA = ThePaths.FindNodePointerSafe(ms_aGenerationLinks[i].m_nodeA);
		const CPathNode * pNodeB = ThePaths.FindNodePointerSafe(ms_aGenerationLinks[i].m_nodeB);

		if(pNodeA && pNodeB)
		{
			Vector3 node0Pos, node1Pos;
			pNodeA->GetCoors(node0Pos);
			pNodeB->GetCoors(node1Pos);

			// If we want to instantly populate the nearby nodes, then we don't care about categorization -
			// simple mark as active all the nodes which are within the creation area.

			if( ms_bInstantFillPopulation )
			{
				const Vector3 vDiff0 = ms_popGenKeyholeShape.GetCenter() - node0Pos;
				const Vector3 vDiff1 = ms_popGenKeyholeShape.GetCenter() - node1Pos;

				isActive =
					vDiff0.XYMag2() < ms_popGenKeyholeShape.GetOuterBandRadiusMax()*ms_popGenKeyholeShape.GetOuterBandRadiusMax() ||
					vDiff1.XYMag2() < ms_popGenKeyholeShape.GetOuterBandRadiusMax()*ms_popGenKeyholeShape.GetOuterBandRadiusMax();

				ms_aGenerationLinks[i].m_iCategory = isActive ? (u8)CPopGenShape::GC_KeyHoleInnerBand_on : (u8)CPopGenShape::GC_Off;
			}
			else
			{
				Vector2	p0;
				node0Pos.GetVector2XY(p0);
				Vector2	p1;
				node1Pos.GetVector2XY(p1);

				// Do the categorization.
				ms_aGenerationLinks[i].m_iCategory = (u8)ms_popGenKeyholeShape.CategoriseLink(p0, p1);

				// Cache if the link is active or not for convenience (and possibly speed).
				isActive = 
					ms_aGenerationLinks[i].m_iCategory == CPopGenShape::GC_KeyHoleInnerBand_on ||
					ms_aGenerationLinks[i].m_iCategory == CPopGenShape::GC_KeyHoleOuterBand_on ||
					ms_aGenerationLinks[i].m_iCategory == CPopGenShape::GC_KeyHoleSideWall_on;

#if __BANK
				if(CVehiclePopulation::ms_displayInnerKeyholeDisparities && !isActive)
				{
					Vector2 popGenKeyholeCenter;
					ms_popGenKeyholeShape.GetCenter().GetVector2XY(popGenKeyholeCenter);
					const Vector2 vDiff0 = popGenKeyholeCenter - p0;
					const Vector2 vDiff1 = popGenKeyholeCenter - p1;
					const float fInnerBandRadiusMin2 = ms_popGenKeyholeShape.GetInnerBandRadiusMin() * ms_popGenKeyholeShape.GetInnerBandRadiusMin();

					ms_aGenerationLinks[i].m_bIsWithinKeyholeShape = ms_aGenerationLinks[i].m_iCategory == CPopGenShape::GC_InFOV_usableIfOccluded;
					ms_aGenerationLinks[i].m_bIsWithinKeyholeShape |= vDiff0.Mag2() < fInnerBandRadiusMin2;
					ms_aGenerationLinks[i].m_bIsWithinKeyholeShape |= vDiff1.Mag2() < fInnerBandRadiusMin2;

					isActive = ms_aGenerationLinks[i].m_bIsWithinKeyholeShape;
				}
#endif // __BANK
			}

			if(isActive)
			{
				++ms_numActiveVehGenLinks;

				// If link has just become active, then mark it as out of date
				if(!ms_aGenerationLinks[i].m_bIsActive)
				{
					ms_aGenerationLinks[i].m_bUpToDate = false;
					ms_aGenerationLinks[i].m_iLastUpdateTime = 0;
				}
			}

			ms_aGenerationLinks[i].m_bIsActive = isActive;
		}
	}
	/////////////////////////////

	//calculate a density modifier based upon ratio of stationary to moving traffic
	float fTrafficDensityModifier = 1.0f;
	Vec3V vTrafficCenter = CVehiclePopulation::ms_stationaryVehicleCenter;
	if(CVehiclePopulation::ms_bUseTrafficDensityModifier)
	{	
		//don't include cars on highways as they won't be contributing to traffic issues
		s32 iNumAmbientVehicles = CVehiclePopulation::GetTotalAmbientMovingVehsOnMap() - CVehiclePopulation::ms_numAmbientCarsOnHighway;
		if(iNumAmbientVehicles > ms_iMinAmbientVehiclesForThrottle)
		{
			float fProportionAmbientVehStopped = CVehiclePopulation::ms_numStationaryAmbientCars / (float)iNumAmbientVehicles;
			fTrafficDensityModifier = RampValue(fProportionAmbientVehStopped, ms_fMinRatioForTrafficThrottle, ms_fMaxRatioForTrafficThrottle, 1.0f, 0.0f);
		}
	}

	for( u32 i =0; i < ms_numGenerationLinks; ++i)
	{
		// Check if this path is path is active.
		if(!ms_aGenerationLinks[i].m_bIsActive)
			continue;

		// link is up to date and active, or is about to become up to date
		ms_aActiveLinks[ms_iNumActiveLinks++] = i;

		// If this link was recently updated, then we will trust the information stored therein
		const s32 iDeltaTime = currTime - ms_aGenerationLinks[i].m_iLastUpdateTime;
		if(iDeltaTime < ms_iActiveLinkUpdateFreqMs && ms_aGenerationLinks[i].m_bUpToDate && !ms_bInstantFillPopulation)
		{
			continue;
		}

		ms_aGenerationLinks[i].m_iLastUpdateTime = currTime;
		ms_aGenerationLinks[i].m_bUpToDate = true;

		// Determine the number of vehicles associated with this path
		// link (within densitySamplingDist meters and total SummedDesiredNumVehs
		// not less than minSummedDesiredNumVehs).

		if(ms_bUsePlayersRoadDensityModifier)
		{
			s32 iDensity = ms_aGenerationLinks[i].m_iInitialDensity;

			if( ms_iNumNodesOnPlayersRoad > 8 )
			{
				if( ms_aGenerationLinks[i].m_bOnPlayersRoad )
				{
					if(ms_fCurrentVehicleSpacingMultiplier >= 1.0f)
					{
						iDensity = Min(15, iDensity + ms_iPlayerRoadDensityInc[iDensity]);
					}
				}
				else
				{
					iDensity = Max(1, iDensity - ms_iNonPlayerRoadDensityDec[iDensity]);
				}
			}

			ms_aGenerationLinks[i].m_iDensity = iDensity;
		}

		const Vector3 vMid(
			(float) ((ms_aGenerationLinks[i].m_iMinX + ms_aGenerationLinks[i].m_iMaxX) >> 1),
			(float) ((ms_aGenerationLinks[i].m_iMinY + ms_aGenerationLinks[i].m_iMaxY) >> 1),
			(float) ms_aGenerationLinks[i].m_iMaxZ );

		//throttle links around high density stationary traffic
		if(CVehiclePopulation::ms_bUseTrafficDensityModifier && fTrafficDensityModifier < 1.0f )
		{
			float fLinkHeading = ms_aGenerationLinks[i].GetLinkHeading();
			
			float fDistToTraffic = vMid.Dist(RCC_VECTOR3(vTrafficCenter));
			float fDistanceModifier = RampValue(fDistToTraffic, ms_fMinDistanceForTrafficThrottle, ms_fMaxDistanceForTrafficThrottle, 0.0f, 1.0f);

			//don't affect links heading away from traffic center
			bool bAdjustDensity = true;
			if(fDistToTraffic > ms_fMinDistanceForTrafficThrottle)
			{
				Vec3V vNormal(ANGLE_TO_VECTOR_X(fLinkHeading * RtoD), ANGLE_TO_VECTOR_Y(fLinkHeading * RtoD), 0.0f);
				Vec3V vToTrafficCenter = vTrafficCenter - VECTOR3_TO_VEC3V(vMid);
				vToTrafficCenter.SetZf(0.0f);
				vToTrafficCenter = Normalize(vToTrafficCenter);
				ScalarV fScalar = Dot(vToTrafficCenter, vNormal);

				if(IsLessThanAll(fScalar, ScalarV(-0.2f)))
				{
					bAdjustDensity = false;
				}
			}

			if(bAdjustDensity)
			{
				float fLinkTrafficModifier = Lerp(fDistanceModifier, fTrafficDensityModifier, 1.0f);

				float iNewDensity = (float)ms_aGenerationLinks[i].m_iDensity * fLinkTrafficModifier;
				ms_aGenerationLinks[i].m_iDensity = Max(1,(rage::round(iNewDensity)));
			}
		}

		const float fScanDistance = ms_fCurrentVehiclesSpacing[ ms_aGenerationLinks[i].m_iDensity ];
		const bool bMatchLinkHeading = ms_aGenerationLinks[i].m_iDensity > 2;
		CAmbientPopulationVehData *pVehData = ms_aAmbientVehicleData.GetElements();
		u32 vehDataCount = ms_aAmbientVehicleData.GetCount();
		ms_aGenerationLinks[i].m_preferedMi = fwModelId::MI_INVALID;
		BANK_ONLY(ms_aGenerationLinks[i].m_fNumNearbyVehiclesIncludedByGrade = 0.0f);
		BANK_ONLY(ms_aGenerationLinks[i].m_iNumTrafficVehicles = 0);
		const float fNumNearbyVehicles = GetAssociatedPathLinks_AreaSearch( ms_aGenerationLinks[i], vMid, ms_aGenerationLinks[i].GetLinkHeading(), ms_aGenerationLinks[i].GetLinkGrade(), fScanDistance, bMatchLinkHeading, ms_aGenerationLinks[i].m_bOverlappingRoads, pVehData, vehDataCount, ms_aGenerationLinks[i].m_preferedMi, scheduledLinks, numScheduledLinks);
		ms_aGenerationLinks[i].SetNearbyVehiclesCount(fNumNearbyVehicles);

		OUTPUT_ONLY(updatedLinks++;)
	}

	vehPopDebugf3("World search for vehicles close by done for %d links! updated freq: %dms", updatedLinks, ms_iActiveLinkUpdateFreqMs);

	// Re-Calculate the disparity for each and every active link.
	for(s32 i = 0; i < ms_iNumActiveLinks; ++i)
	{
		CVehGenLink &link = ms_aGenerationLinks[ms_aActiveLinks[i]];

		const float fRandVal = (link.m_iDensity > 1) ? fwRandom::GetRandomNumberInRange(0.0f, ms_popGenRandomizeLinkDensity) : 0.0f;
		const float fDensity = rage::Clamp( (float) link.m_iDensity + fRandVal, 0.0f, 15.0f);
		float fDesiredVehiclesPerMeterPerLane = GetDesiredNumVehiclesPerLanePerMeter(fDensity);

		u8 iLanes = link.m_iLinkLanes;

		if(!link.m_bOnPlayersRoad)
		{
			fDesiredVehiclesPerMeterPerLane *= ms_fVehiclePerMeterScaleForNonPlayersRoad;

			//fLanes -= Max(link.m_iLinkLanes - 1.5f, 0.0f) * 0.667f;

			if(iLanes > 2)
			{
				--iLanes;
			}
		}

		const float linkMetersTimesLanes = link.GetLinkLengths() * iLanes;

		float desiredNearbyVehCount = linkMetersTimesLanes * fDesiredVehiclesPerMeterPerLane;
		float nearbyVehicleCount = link.GetNearbyVehiclesCount();

		link.m_fDesiredNearbyVehicleCountDelta = desiredNearbyVehCount - nearbyVehicleCount;

		link.m_fDensity = nearbyVehicleCount / desiredNearbyVehCount;
		link.m_fVehicleDensityContribution = 1.0f / desiredNearbyVehCount;
		
		// Ready our links for sorting.
		link.ComputeScore();
	}

	// Precompute link order, sorted by score, simulating vehicle generation as we go.
	ms_iNumActiveLinksUsingLowerScore = 0;
	if(ms_iNumActiveLinks != 0)
	{
		Assert(VEHGEN_NUM_PRESELECTED_LINKS <= MAX_GENERATION_LINKS);

		s32 preselectedLinks[VEHGEN_NUM_PRESELECTED_LINKS];

		for(u32 i = 0; i < VEHGEN_NUM_PRESELECTED_LINKS; ++i)
		{
			preselectedLinks[i] = -1;
		}

		qsort(ms_aActiveLinks, ms_iNumActiveLinks, sizeof(s32), ActiveLinkScoreComparison);

		// If the highest scoring link's score is greater than the minimum score required to generate a vehicle,
		// pretend that we've created a vehicle at that link and update all link scores accordingly.
		// Sort, repeat.
		for(u32 i = 0; ms_aGenerationLinks[ms_aActiveLinks[0]].m_fScore > ms_fMinGenerationScore && i < VEHGEN_NUM_PRESELECTED_LINKS; ++i)
		{
			//this link was accepted as we've lowered the required score
			//we don't want to include it in disparity calculations
			if(ms_aGenerationLinks[ms_aActiveLinks[0]].m_fScore < ms_fMinBaseGenerationScore)
			{
				++ms_iNumActiveLinksUsingLowerScore;
			}

			preselectedLinks[i] = ms_aActiveLinks[0];

			CVehGenLink &selectedLink = ms_aGenerationLinks[ms_aActiveLinks[0]];

			selectedLink.m_fDesiredNearbyVehicleCountDelta -= 1.0f;
			selectedLink.m_fDensity += selectedLink.m_fVehicleDensityContribution;
			selectedLink.ComputeScore();

			// Simulate vehicle generation by calculating the density contribution of our "vehicle" for all qualifying links.
			for(s32 j = 1; j < ms_iNumActiveLinks && ms_aGenerationLinks[ms_aActiveLinks[j]].m_fScore > ms_fMinGenerationScore; ++j)
			{
				CVehGenLink &otherLink = ms_aGenerationLinks[ms_aActiveLinks[j]];

				float contribution = otherLink.GetVehicleDensityContributionFromLink(selectedLink);

				if(contribution != 0.0f)
				{
					otherLink.m_fDesiredNearbyVehicleCountDelta -= contribution;
					otherLink.m_fDensity += otherLink.m_fVehicleDensityContribution * contribution;
					otherLink.ComputeScore();
				}
			}

			// Continue to consider all links as long as they meet minimum score requirements.
			// Doing so results in a comprehensive list of link disparities, and lessens our
			// chances of starving vehicle generation.
			qsort(ms_aActiveLinks, ms_iNumActiveLinks, sizeof(s32), ActiveLinkScoreComparison);
		}

		// Copy our precomputed list into active links.
		for(ms_iNumActiveLinks = 0; ms_iNumActiveLinks < VEHGEN_NUM_PRESELECTED_LINKS && preselectedLinks[ms_iNumActiveLinks] != -1; ++ms_iNumActiveLinks)
		{
#if __BANK
			// Skip inner keyhole links, these are collected for debug display purposes only
			bool bSkip = false;
			
			if(ms_displayInnerKeyholeDisparities && ms_aGenerationLinks[preselectedLinks[ms_iNumActiveLinks]].m_bIsWithinKeyholeShape)
			{
				++CVehiclePopulation::ms_innerKeyholeDisparities;
				bSkip = true;
			}
			
			if(!bSkip)
			{
#endif // __BANK
				ms_aActiveLinks[ms_iNumActiveLinks] = preselectedLinks[ms_iNumActiveLinks];
#if __BANK
			}
#endif // __BANK
		}

#if __BANK
		if(ms_displayInnerKeyholeDisparities)
		{
			ms_iNumActiveLinks -= CVehiclePopulation::ms_innerKeyholeDisparities;
		}
#endif
	}
}

float CVehiclePopulation::GetAssociatedPathLinks_AreaSearch(CVehGenLink& link, const Vector3 & vLinkCenter, const float fLinkHeading, const float fLinkGrade, const float fScanDistance, const bool bMatchLinkHeading, const bool bOverlappingRoads, CAmbientPopulationVehData *pVehData, u32 vehDataCount, u32& preferedMi, sScheduledLink* UNUSED_PARAM(scheduledLinks), u32& UNUSED_PARAM(numScheduledLinks))
{
	const float fSameHdgEps = 110.0f * DtoR;
	const float fHdgContributionFalloffStart = 80.0f * DtoR;
	const float fHdgContributionRange = (fSameHdgEps - fHdgContributionFalloffStart);

	const float fVerticalFalloffStart = bOverlappingRoads ? ms_popGenDensitySamplingVerticalFalloffStart * 0.25f : ms_popGenDensitySamplingVerticalFalloffStart;
	const float fVerticalFalloffEnd = bOverlappingRoads ? ms_popGenDensitySamplingVerticalFalloffEnd * 0.25f : ms_popGenDensitySamplingVerticalFalloffEnd;

	ScalarV fZero(V_ZERO);
	float fNumNearbyVehicles = 0.0f;
	const float fVerticalFalloffRange = fVerticalFalloffEnd - fVerticalFalloffStart;
	const float fRangeSqr = fScanDistance*fScanDistance;
	ScalarV svRangeSqr = ScalarVFromF32(fRangeSqr);
	Vec3V vMidV = RCC_VEC3V(vLinkCenter);
	u8 iNumVehiclesWaiting = 0;

#if 0
	const u32 maxModelsToKeepTrackOf = 32; // TODO: too few for pc
	struct sVehModelDistInfo
	{
		sVehModelDistInfo() : closestDistSqr(V_FLT_MAX), mi(fwModelId::MI_INVALID) {}

		ScalarV closestDistSqr;
		u32 mi;
	};
	sVehModelDistInfo models[maxModelsToKeepTrackOf];
#endif

	Vec3V vNormal(ANGLE_TO_VECTOR_X(fLinkHeading * RtoD), ANGLE_TO_VECTOR_Y(fLinkHeading * RtoD), 0.0f);
	//if we don't have a traffic light node, then just looks forwards alone link direction by twice the normal scan distance
// 	Vector3 vTrafficMid = VEC3V_TO_VECTOR3(vMidV + (vNormal * ScalarV(fScanDistance * 2.0f)));
// #if VEHGEN_CACHE_LINK_NEXT_JUNCTION_NODE
// 	if(!link.m_junctionNode.IsEmpty())
// 	{
// 		const CPathNode * pJunNode = ThePaths.FindNodePointer(link.m_junctionNode);
// 		if(pJunNode)
// 		{
// 			pJunNode->GetCoors(vTrafficMid);
// 		}		
// 	}
// #endif

	for(u32 i = 0; i < vehDataCount; i++)
	{
		CAmbientPopulationVehData &pVeh = pVehData[i];

		ScalarV distSqr = DistSquared(pVeh.m_Pos, vMidV);

		if (Likely(pVeh.m_fHeading > -360.f))
		{
			const float fHdgDelta = Abs(rage::SubtractAngleShorterFast( fLinkHeading, pVeh.m_fHeading ));
			if( ms_bSearchLinksMustMatchVehHeading && bMatchLinkHeading && fHdgDelta > fSameHdgEps )
			{
				continue;
			}

			if(IsLessThanAll(distSqr, svRangeSqr))
			{
				const float fHeadingScale = (bMatchLinkHeading && fHdgDelta > fHdgContributionFalloffStart) ? 1.0f - ((fHdgDelta - fHdgContributionFalloffStart) / fHdgContributionRange) : 1.0f;
				const float fZDist = pVeh.m_Pos.GetZf() - vLinkCenter.z;

				ScalarV fScalar = Dot(vNormal, pVeh.m_Pos - vMidV);
				const Vec3V vPlanarOffset = vNormal * fScalar;
				ScalarV fPlanarDistance = MagXYFast(vPlanarOffset);

				if(IsTrue(fScalar < ScalarV(V_ZERO)))
				{
					fPlanarDistance = -fPlanarDistance;
				}

				const float fGradeOffset = fPlanarDistance.Getf() * fLinkGrade;

				const float fContribution = 1.0f - Clamp((Abs(fZDist - fGradeOffset) - fVerticalFalloffStart) / fVerticalFalloffRange, 0.0f, 1.0f);
				fNumNearbyVehicles += fContribution * fHeadingScale;

#if __BANK
				const float fContributionExcludingGrade = 1.0f - Clamp((Abs(fZDist) - fVerticalFalloffStart) / fVerticalFalloffRange, 0.0f, 1.0f);
				link.m_fNumNearbyVehiclesIncludedByGrade += (fContribution - fContributionExcludingGrade) * fHeadingScale;

#if DR_VEHGEN_GRADE_DEBUGGING
				link.m_fDrDistance = Dist(pVeh.m_Pos, vMidV).Getf();
				link.m_fDrPlanarDistance = fPlanarDistance.Getf();
				link.m_fDrZDist = fZDist;
				link.m_fDrGradeOffset = fGradeOffset;
#endif // DR_VEHGEN_GRADE_DEBUGGING
#endif // __BANK
			}

#if VEHGEN_CHECK_FOR_TRAFFIC_JAMS
			static dev_float s_fMinSpeedSqr = 4.0f;
			//static dev_float s_fSameHeadingEp = 45.0f * DtoR;
			static u8 s_iMaxCarsPerLane = 4;
			static dev_float s_fStoppedCarMultiplier = 0.7f;
			static ScalarV s_fSpawnRadiusSqr = ScalarV(30.0f*30.0f);

			if(pVeh.m_fSpeed < s_fMinSpeedSqr && fHdgDelta < PI) //check heading so we don't prevent link in other direction
			{
				ScalarV distSqrTraffic = DistSquared(pVeh.m_SpawnPos, vMidV);
				if(IsLessThanAll(distSqrTraffic, s_fSpawnRadiusSqr))
				{
					BANK_ONLY(++link.m_iNumTrafficVehicles);
					++iNumVehiclesWaiting;
					if( iNumVehiclesWaiting >= s_iMaxCarsPerLane * link.m_iLinkLanes )
					{
						//more lanes - traffic counts for less		
						float fMaxPenalty = s_fStoppedCarMultiplier / link.m_iLinkLanes;
						fNumNearbyVehicles += RampValue(distSqrTraffic.Getf(), 10.0f*10.0f, s_fSpawnRadiusSqr.Getf(), fMaxPenalty, 0.0f);
					}					
				}
			}

//			static dev_float s_fStoppedCarMultiplier = 0.4f;
// 			//check for nearby vehicles around us and ahead at traffic lights to see if there are many not moving
// 			ScalarV distSqrTraffic = DistSquared(pVeh.m_Pos, VECTOR3_TO_VEC3V(vTrafficMid));
// 			if(IsLessThanAll(distSqrTraffic, svRangeSqr) || IsLessThanAll(distSqr, svRangeSqr))
// 			{

// 				if(pVeh.m_fSpeed < s_fMinSpeedSqr && fHdgDelta < s_fSameHeadingEp)
// 				{		
// 					BANK_ONLY(++link.m_iNumTrafficVehicles);
// 					++iNumVehiclesWaiting;
// 					if( iNumVehiclesWaiting >= s_iMaxCarsPerLane * link.m_iLinkLanes )
// 					{
// 						//more lanes - traffic counts for less
// 						fNumNearbyVehicles += s_fStoppedCarMultiplier / link.m_iLinkLanes;					
// 					}					
// 				}
// 			}	
#endif
		}

#if 0
		// find closest distance to this link from any instance of the same model
		for (u32 f = 0; f < maxModelsToKeepTrackOf; ++f)
		{
			if (models[f].mi == pVeh.m_modelIndex)
			{
				if (IsLessThanAll(distSqr, models[f].closestDistSqr))
					models[f].closestDistSqr = distSqr;

				break;
			}

			if (models[f].mi == fwModelId::MI_INVALID)
			{
				models[f].mi = pVeh.m_modelIndex;
				models[f].closestDistSqr = distSqr;
				break;
			}
		}
#endif
	}

#if 0
	u32 link = 0;
	for (link = 0; link < numScheduledLinks; ++link)
	{
		u32 mi = (u32)scheduledLinks[link].pos_and_mi.GetWi();
		if (mi == fwModelId::MI_INVALID)
			break;

		ScalarV distSqr = DistSquared(scheduledLinks[link].pos_and_mi.GetXYZ(), vMidV);

		for (u32 f = 0; f < maxModelsToKeepTrackOf; ++f)
		{
			if (models[f].mi == mi)
			{
				if (IsLessThanAll(distSqr, models[f].closestDistSqr))
					models[f].closestDistSqr = distSqr;

				break;
			}
		}
	}
#endif

	preferedMi = fwModelId::MI_INVALID;
#if 0
	ScalarV highestClosestDistSqr(V_FLT_MIN);
	for (u32 i = 0; i < maxModelsToKeepTrackOf; ++i)
	{
		if (models[i].mi == fwModelId::MI_INVALID)
			break;

		if (IsGreaterThanAll(models[i].closestDistSqr, highestClosestDistSqr))
		{
			highestClosestDistSqr = models[i].closestDistSqr;
			preferedMi = models[i].mi;
		}
	}

    ScalarV minAcceptedDistSqr(50.f * 50.f); // anything less than this we don't schedule, we'll let the logic randomize something better
    if (IsLessThanAll(highestClosestDistSqr, minAcceptedDistSqr))
        preferedMi = fwModelId::MI_INVALID;

	if (link < ms_numGenerationLinks)
	{
		scheduledLinks[link].pos_and_mi.SetXYZ(vMidV);
		scheduledLinks[link].pos_and_mi.SetWi(preferedMi);
		numScheduledLinks++;
	}
#endif

	return fNumNearbyVehicles;
}

float CVehiclePopulation::GetDesiredNumVehiclesPerLanePerMeter(const float fDensity)
{
	const int iDensity = (int)fDensity;

	if(iDensity==15)
		return ms_fVehiclesPerMeterDensityLookup[15];

	const float t = fDensity - ((float)iDensity);
	return (ms_fVehiclesPerMeterDensityLookup[iDensity] * (1.0f-t)) + (ms_fVehiclesPerMeterDensityLookup[iDensity+1] * t);
}
