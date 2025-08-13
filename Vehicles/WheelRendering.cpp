// Some wheel code is shared accross SPU tasks so we put all the functions here to avoid 
// including wheel.cpp everywhere

#include "control/replay/replay.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/Automobile.h"
#include "vehicles/vehicle.h"
#include "vehicles/wheel.h"
#include "vehicles/Planes.h"
#include "vehicles/Heli.h"
#include "crskeleton/skeletondata.h"
#include "fwmaths/vectorutil.h"

VEHICLE_OPTIMISATIONS()

// adding these constants in here so SPU jobs can share the numbers easily
dev_float CWheel::ms_fWheelIndepRotMult = 0.75f;
dev_float CWheel::ms_fWheelIndepPivotDistCompression = 0.3f;
dev_float CWheel::ms_fWheelIndepPivotDistExtension = 0.5f;

///////////////////
// Position the wheel bone correctly for rendering
//
const eHierarchyId CWheel::ms_HubComponents[VEH_WHEEL_RM3 - VEH_WHEEL_LF + 1] =
{
	VEH_WHEELHUB_LF,	//VEH_WHEEL_LF,
	VEH_WHEELHUB_RF,	//VEH_WHEEL_RF,
	VEH_WHEELHUB_LR,	//VEH_WHEEL_LR,
	VEH_WHEELHUB_RR,	//VEH_WHEEL_RR,
	VEH_WHEELHUB_LM1,	//VEH_WHEEL_LM1,
	VEH_WHEELHUB_RM1,	//VEH_WHEEL_RM1,
	VEH_WHEELHUB_LM2,	//VEH_WHEEL_LM2,
	VEH_WHEELHUB_RM2,	//VEH_WHEEL_RM2,
	VEH_WHEELHUB_LM3,	//VEH_WHEEL_LM3,
	VEH_WHEELHUB_RM3	//VEH_WHEEL_RM3,
};
const eHierarchyId CWheel::ms_OpposingWheels[VEH_WHEEL_RM3 - VEH_WHEEL_LF + 1] =
{
	VEH_WHEEL_RF,	//VEH_WHEEL_LF,
	VEH_WHEEL_LF,	//VEH_WHEEL_RF,
	VEH_WHEEL_RR,	//VEH_WHEEL_LR,
	VEH_WHEEL_LR,	//VEH_WHEEL_RR,
	VEH_WHEEL_RM1,	//VEH_WHEEL_LM1,
	VEH_WHEEL_LM1,	//VEH_WHEEL_RM1,
	VEH_WHEEL_RM2,	//VEH_WHEEL_LM2,
	VEH_WHEEL_LM2,	//VEH_WHEEL_RM2,
	VEH_WHEEL_RM3,	//VEH_WHEEL_LM3,
	VEH_WHEEL_LM3	//VEH_WHEEL_RM3,
};




void CWheel::ProcessWheelMatrixCore(crSkeleton & rSkeleton, CVehicleStructure & rStructure, CWheel * const * ppWheels, int numWheels, bool bAnimated, float fSteerAngle)
{
	int nBoneIndex = rStructure.m_nBoneIndices[m_WheelId];
	if(nBoneIndex < 0 || bAnimated)
	{
		return;
	}

	//Displayf("nBoneIndex = %i pStructure = %p m_WheelId = %i", nBoneIndex, &rStructure, m_WheelId);

	//
	const crSkeletonData & rSkeletonData = rSkeleton.GetSkeletonData();
	fwFlags32 flagsConfig = GetConfigFlags();

	// Prefetch the wheel matrix that we will be writing to at the end
	PrefetchObject(&(rSkeleton.GetLocalMtx(nBoneIndex)));

	// Consts
	const ScalarV fZero(V_ZERO);
	const ScalarV fOne(V_ONE);
	const ScalarV fNegOne(V_NEGONE);

	// Convert members to vector pipe
	const Vec3V mvecAxleAxis(RCC_VEC3V(m_vecAxleAxis));
	const Vec3V mvecSuspensionAxis(RCC_VEC3V(m_vecSuspensionAxis));
	Vec3V vWheelPos(RCC_VEC3V(m_aWheelLines.B));
	const ScalarV mfWheelRadius(m_fWheelRadius);
	const ScalarV mfStaticDelta(m_fStaticDelta);
	const ScalarV mfWheelCompression(m_fWheelCompression);
	const ScalarV mfRotAng(m_fRotAng);
	const ScalarV mfExtraSinkDepth(m_fExtraSinkDepth);
	const ScalarV mfWheelImpactOffset(m_fWheelImpactOffset);
	const ScalarV mfSteerAngle(fSteerAngle);
	const ScalarV msfWheelIndepRotMult(ms_fWheelIndepRotMult);

	Mat34V mtxWheel;

	Assert(fabsf(1.0f-m_vecSuspensionAxis.Mag2())<0.01f && fabsf(1.0f-m_vecAxleAxis.Mag2())<0.01f);
	mtxWheel.Setc(mvecSuspensionAxis);
	mtxWheel.Seta(mvecAxleAxis);
	mtxWheel.Setb(Cross(mtxWheel.c(), mtxWheel.a()));

	ScalarV specialFlightModeRatio = ScalarV( m_pParentVehicle->GetSpecialFlightModeRatio() );
	ScalarV fWheelCompression = specialFlightModeRatio.Getf() < 1.0f ? mfWheelCompression : ScalarV( m_fCompression );

	if(flagsConfig.IsFlagSet(WCF_RENDER_WITH_ZERO_COMPRESSION))
	{
		fWheelCompression = ScalarV(-(m_fWheelRadius + m_aWheelLines.B.z));
	}
	else if( m_pParentVehicle->GetSpecialFlightModeRatio() > 0.0f )
	{
		ScalarV minCompression( m_aWheelLines.A.z - m_aWheelLines.B.z );
		minCompression *= specialFlightModeRatio;
		minCompression -= ScalarV( m_fWheelRadius );

		if( specialFlightModeRatio.Getf() == 1.0f )
		{
			fWheelCompression = minCompression;
		}
		else 
		{
			fWheelCompression = Max( fWheelCompression, minCompression ); 
		}
	}

	if(m_fTyreHealth < TYRE_HEALTH_DEFAULT)
	{
		fWheelCompression -= ScalarV(GetTyreBurstCompression());
		fWheelCompression = Max(fZero, fWheelCompression);
	}

	// tyres sink into soft ground
	fWheelCompression -= mfExtraSinkDepth;
	fWheelCompression += mfWheelImpactOffset;
	fWheelCompression = Max(fZero, fWheelCompression);

	vWheelPos = vWheelPos + mtxWheel.c() * (fWheelCompression + mfWheelRadius);
	// don't do this for trikes with a center wheel
	if (!flagsConfig.IsFlagSet(WCF_CENTRE_WHEEL))
	{
		// do some tilting as suspension compresses
		if (MI_PLANE_TULA.IsValid() &&
			m_pParentVehicle->GetModelIndex() == MI_PLANE_TULA &&
			!flagsConfig.IsFlagSet(WCF_REARWHEEL))
		{
			CPlane* plane = static_cast<CPlane*>(m_pParentVehicle);
			const CLandingGear&  gear = plane->GetLandingGear();
			float gearRatio = 1.0f - gear.GetGearDeployRatio();
			static float tulaWheelRotateMax = -0.59f;

			gearRatio *= tulaWheelRotateMax;

			static dev_float extraWheelRetractDistance = -0.2f;
			vWheelPos = vWheelPos + mtxWheel.c() * ScalarV(extraWheelRetractDistance * gearRatio);

			if (!flagsConfig.IsFlagSet(WCF_LEFTWHEEL))
			{
				gearRatio = -gearRatio;
			}

			Mat34VRotateGlobalY(mtxWheel, ScalarV(gearRatio));
		}
		else if ((m_pParentVehicle->InheritsFromPlane() ||
			m_pParentVehicle->InheritsFromHeli()) &&
			m_pParentVehicle->GetStatus() == STATUS_WRECKED &&
			mfWheelCompression.Getf() == 0.0f)
		{
			float gearRatio = 0.0f;

			if (m_pParentVehicle->InheritsFromPlane())
			{
				CPlane* plane = static_cast<CPlane*>(m_pParentVehicle);
				const CLandingGear&  gear = plane->GetLandingGear();
				gearRatio = 1.0f - gear.GetGearDeployRatio();
			}
			else
			{
				CHeli* heli = static_cast<CHeli*>(m_pParentVehicle);
				const CLandingGear&  gear = heli->GetLandingGear();
				gearRatio = 1.0f - gear.GetGearDeployRatio();
			}

			float extraWheelRetractDistance = m_fStaticDelta;

			vWheelPos = vWheelPos + mtxWheel.c() * ScalarV(extraWheelRetractDistance * gearRatio);
		}
		else if (flagsConfig.IsFlagSet(WCF_TILT_INDEP))
		{
			const ScalarV msfWheelIndepPivotDistExtension(ms_fWheelIndepPivotDistExtension);
			const ScalarV msfWheelIndepPivotDistCompression(ms_fWheelIndepPivotDistCompression);

			static ScalarV sf_ScaleRaiseAmount(-0.1f);
			static ScalarV sf_ScaleRaiseAmountExtra(-0.3f);
			static ScalarV sf_ScaleInsetAmount(-0.175f);

			float fSuspensionLoweredAmount = 0.0f;

			if (m_pParentVehicle->InheritsFromAutomobile())
			{
				CAutomobile& rAutomobile = static_cast<CAutomobile&>(*m_pParentVehicle);

				if (rAutomobile.HasHydraulicSuspension())
				{
					float fUpperBound;
					float fLowerBound;

					rAutomobile.GetHydraulicsBounds(fUpperBound, fLowerBound);

					float fTotalHydraulicLength = fUpperBound - fLowerBound;

					if (m_fSuspensionRaiseAmount < 0.0f)
					{
						fSuspensionLoweredAmount = Abs(m_fSuspensionRaiseAmount - fUpperBound);
					}
					else
					{
						fSuspensionLoweredAmount = Abs(fUpperBound - m_fSuspensionRaiseAmount);
					}

					fSuspensionLoweredAmount /= fTotalHydraulicLength;
					fSuspensionLoweredAmount -= 0.5f;
				}
			}

			ScalarV suspensionLoweredFactor(fSuspensionLoweredAmount);

			if (!(m_pParentVehicle->pHandling->mFlags & MF_EXTRA_CAMBER) ||
				(m_pParentVehicle->pHandling->mFlags & MF_AXLE_R_SOLID &&
					flagsConfig.IsFlagSet(WCF_REARWHEEL)) ||
					(m_pParentVehicle->pHandling->mFlags & MF_AXLE_F_SOLID &&
						!flagsConfig.IsFlagSet(WCF_REARWHEEL)))
			{
				suspensionLoweredFactor *= sf_ScaleRaiseAmount;
			}
			else
			{
				suspensionLoweredFactor *= sf_ScaleRaiseAmountExtra;
			}

			ScalarV fRatio = fWheelCompression - mfStaticDelta;

			// Select the first of these to approximate the sin, the second for more accuracy
			ScalarV fRotate = msfWheelIndepRotMult * Clamp(fRatio - suspensionLoweredFactor, fNegOne, fOne);
			//const ScalarV fRotate = ms_fWheelIndepRotMult*rage::Asinf(rage::Clamp(fRatio, fNegOne, fOne));

			//vWheelPos.x *= /*0.5f + 0.5f**/rage::Cosf(fRatio);
			const ScalarV fPivotDist = SelectFT(IsGreaterThan(fRatio, fZero), msfWheelIndepPivotDistExtension, msfWheelIndepPivotDistCompression);
			ScalarV fLoweredSuspensionInset(0.0f);

			fLoweredSuspensionInset = Abs(suspensionLoweredFactor) * sf_ScaleInsetAmount;

			ScalarV fTemp = square(fPivotDist + fLoweredSuspensionInset);

			fTemp -= square(fRatio);

			fRatio = fPivotDist - SqrtSafe(fTemp, fZero);

			if (specialFlightModeRatio.Getf() > 0.0f)
			{
				static ScalarV ratioToStartWheelRotation(0.6f);
				static ScalarV ratioToStartWheelRotationScale = ScalarV(1.0f) / (ScalarV(1.0f) - ratioToStartWheelRotation);
				ScalarV minRotation = (specialFlightModeRatio - ratioToStartWheelRotation) * ratioToStartWheelRotationScale;

				if (minRotation.Getf() > 0.0f)
				{
					minRotation *= minRotation;
					minRotation = Min(ScalarV(1.0f), minRotation);
					minRotation *= msfWheelIndepRotMult;
					fRotate = Max(ScalarV(minRotation), fRotate);
				}
			}

			if (!flagsConfig.IsFlagSet(WCF_LEFTWHEEL))
			{
				fRotate = Negate(fRotate);
				fRatio = Negate(fRatio);
			}

			fRatio *= ScalarV(1.0f) - specialFlightModeRatio;
			Mat34VRotateGlobalY(mtxWheel, fRotate);

			vWheelPos.SetX(vWheelPos.GetX() + fRatio);
		}
		else if (flagsConfig.IsFlagSet(WCF_TILT_SOLID))
		{
			const CWheel * pOpposingWheel = 0;
			for (int i = 0; i < numWheels; ++i)
			{
				if (ppWheels[i]->GetHierarchyId() == ms_OpposingWheels[m_WheelId - VEH_WHEEL_LF])
				{
					pOpposingWheel = ppWheels[i];
					break;
				}
			}
			if (pOpposingWheel)
			{
				ScalarV fWheelCompressionOpposing(pOpposingWheel->GetWheelCompression());
				if (pOpposingWheel->GetTyreHealth() < TYRE_HEALTH_DEFAULT &&
					m_pParentVehicle->GetModelIndex() != MI_CAR_COMET4)
				{
					const ScalarV fOpposingTyreHealthFraction(pOpposingWheel->GetTyreHealth()*TYRE_HEALTH_DEFAULT_INV);
					const ScalarV fOpposingWheelRadius(pOpposingWheel->GetWheelRadius() - pOpposingWheel->GetWheelRimRadius());
					fWheelCompressionOpposing = fWheelCompressionOpposing - Max(fZero, fOne - fOpposingTyreHealthFraction * fOpposingWheelRadius);
					fWheelCompressionOpposing = Max(fZero, fWheelCompressionOpposing);
				}

				const ScalarV fNegTwo(V_NEGTWO);
				ScalarV fRatio = fWheelCompression - fWheelCompressionOpposing;
				fRatio = fRatio / (fNegTwo * vWheelPos.GetX());
				fRatio = Clamp(fRatio, fNegOne, fOne);

				Mat34VRotateGlobalY(mtxWheel, fRatio);

				vWheelPos.SetX(vWheelPos.GetX() * Cos(fRatio));
			}
		}
	}

	mtxWheel.Setd(vWheelPos);

	// Amphibious quads can retract their wheels. Apply shift inwards and tilt sideways
	if(m_pParentVehicle->InheritsFromAmphibiousQuadBike())
	{
		CAmphibiousQuadBike* pQuad = static_cast<CAmphibiousQuadBike*>(m_pParentVehicle);

		float fTransformDirection = ( flagsConfig.IsFlagSet(WCF_LEFTWHEEL) ? 1.0f : -1.0f );

		// Rotate inwards
		Mat34VRotateGlobalY(mtxWheel, ScalarV(pQuad->GetWheelTiltAmount() * fTransformDirection * DtoR));

		// Shift inwards
		mtxWheel.Setd( mtxWheel.d() + ( Vec3V(pQuad->GetWheelShiftAmount() * fTransformDirection, 0.0f, 0.0f) ) );
	}

    if( m_pParentVehicle->pHandling->GetCarHandlingData() )
    {
        float fCamberDirection = ( flagsConfig.IsFlagSet(WCF_LEFTWHEEL) ? 1.0f : -1.0f );
        float fCamberAmount = flagsConfig.IsFlagSet( WCF_REARWHEEL ) ? -m_pParentVehicle->pHandling->GetCarHandlingData()->m_fCamberRear : -m_pParentVehicle->pHandling->GetCarHandlingData()->m_fCamberFront;

		if( m_pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_INCREASE_CAMBER_WITH_SUSPENSION_MOD ) )
		{
			static dev_float sfCamberIncreaseScale = 10.0f;
			fCamberAmount += Max( fCamberAmount, 0.01f ) * ( (float)m_pParentVehicle->GetVariationInstance().GetSuspensionModifier() / 100.0f ) * sfCamberIncreaseScale;
		}

        if( flagsConfig.IsFlagSet( WCF_STEER ) )
        {
            fCamberAmount += Arcsin( Sin( ScalarV( m_pParentVehicle->pHandling->GetCarHandlingData()->m_fCastor ) ) * Sin( ScalarV( m_fSteerAngle ) ) ).Getf() * fCamberDirection;
        }

        // Rotate inwards
        Mat34VRotateGlobalY(mtxWheel, ScalarV( fCamberAmount * fCamberDirection ) );
    }

	// do suspension component tilt at the same time
	if(ms_HubComponents[m_WheelId - VEH_WHEEL_LF] > -1 && rStructure.m_nBoneIndices[ms_HubComponents[m_WheelId - VEH_WHEEL_LF]] > -1)
	{
		Mat34V mtxHub(V_ZERO);
		int iHubBoneIndex = rStructure.m_nBoneIndices[ms_HubComponents[m_WheelId - VEH_WHEEL_LF]];

		if(!flagsConfig.IsFlagSet(WCF_DONT_RENDER_HUB))
		{
			mtxHub = mtxWheel;
			Vec3V vWheelToHub(rSkeletonData.GetBoneData(iHubBoneIndex)->GetDefaultTranslation() - rSkeletonData.GetBoneData(nBoneIndex)->GetDefaultTranslation());
			vWheelToHub = UnTransform3x3Ortho(mtxHub,vWheelToHub);
			mtxHub.Setd(mtxHub.d() + vWheelToHub);

			if(!flagsConfig.IsFlagSet(WCF_DONT_RENDER_STEER) )
			{
				// When quad wheels are sufficiently tucked in, we don't want to render steering on them
				if( ( !m_pParentVehicle->InheritsFromAmphibiousQuadBike() || static_cast<CAmphibiousQuadBike*>(m_pParentVehicle)->ShouldRenderWheelSteerAndSpin() ) &&
					m_pParentVehicle->GetSpecialFlightModeRatio() < 0.75f )
				{
					Mat34VRotateLocalZ( mtxHub, mfSteerAngle * ( ScalarV( 1.0f ) - Min( ScalarV( 1.0f ) , specialFlightModeRatio * ScalarV( 1.5f ) ) ) );
				}
			}
		}

		rSkeleton.GetLocalMtx(iHubBoneIndex) = mtxHub;
	}

	// When quad wheels are sufficiently tucked in, we don't want to render steering or spinning on them
	if( ( !m_pParentVehicle->InheritsFromAmphibiousQuadBike() || ( static_cast<CAmphibiousQuadBike*>(m_pParentVehicle)->ShouldRenderWheelSteerAndSpin() ) ) &&
		m_pParentVehicle->GetSpecialFlightModeRatio() < 0.75f )
	{
		if(!flagsConfig.IsFlagSet(WCF_DONT_RENDER_STEER))
		{
			Mat34VRotateLocalZ( mtxWheel, mfSteerAngle * ScalarV( 1.0f - Min( 1.0f, m_pParentVehicle->GetSpecialFlightModeRatio() * 1.5f ) ) );
		}

		m_bUpdateWheelRotation = true;
	}
	else
	{
		// If wheels tucked in, don't update the spin of the wheel so that we can extend them back seamlessly (they don't magically pop to the new spin angle)
		m_bUpdateWheelRotation = false;
	}

	Mat34VRotateLocalX(mtxWheel,mfRotAng);

	// Wheel matrix is now in local car matrix space, but could be child of non-root bone, so put in space of its parent
	int iParentIndex = rSkeletonData.GetParentIndex(nBoneIndex);
	if(iParentIndex > 0)
	{
		UnTransformOrtho(mtxWheel,rSkeleton.GetObjectMtx(iParentIndex),mtxWheel);
	}

	rSkeleton.GetLocalMtx(nBoneIndex) = mtxWheel;

	// Landing gear wheel requires updating the object and global skeleton with the wheel matrix change
	if(GetConfigFlags().IsFlagSet(WCF_PLANE_WHEEL))
	{
		rSkeleton.PartialUpdate(nBoneIndex);
	}
}

#define DEBUG_ANIMATED_WHEEL_DEFORMATION 0
void CWheel::ProcessWheelDeformationMatrixCore(crSkeleton & rSkeleton, CVehicleStructure & rStructure, bool bAnimated)
{
	int nBoneIndex = rStructure.m_nBoneIndices[m_WheelId];
	if(nBoneIndex < 0 || !bAnimated)
	{
		return;
	}

#if DEBUG_ANIMATED_WHEEL_DEFORMATION
	Mat34V matWheelDebug;
	rSkeleton.GetGlobalMtx(nBoneIndex, matWheelDebug);
	grcDebugDraw::Axis(matWheelDebug, 0.4f, true);
#endif

	// Prefetch the wheel matrix that we will be writing to at the end
	PrefetchObject(&(rSkeleton.GetLocalMtx(nBoneIndex)));

	// Get the undeformed wheel matrix
	Mat34V mtxWheelUndeformed = rSkeleton.GetLocalMtx(nBoneIndex);
	// Wheel matrix is now in local car matrix space, but could be child of non-root bone, so put in space of its parent
	const crSkeletonData & rSkeletonData = rSkeleton.GetSkeletonData();
	int iParentIndex = rSkeletonData.GetParentIndex(nBoneIndex);
	if(iParentIndex > 0)
	{
		Transform(mtxWheelUndeformed,rSkeleton.GetObjectMtx(iParentIndex),mtxWheelUndeformed);
	}

	// Compute deformed wheel position by projecting the undeformed position to deformed suspension axis
	Vec3V vSuspensionAxis(RCC_VEC3V(m_vecSuspensionAxis));
	Vec3V vSuspensionEndPos(RCC_VEC3V(m_aWheelLines.B));
	Vec3V vWheelPosition = vSuspensionEndPos + vSuspensionAxis * ((mtxWheelUndeformed.d().GetZ() - vSuspensionEndPos.GetZ()) / vSuspensionAxis.GetZ());
//	Vec3V vWheelPosition = RCC_VEC3V(m_aWheelLines.B) + vSuspensionAxis * Dot((mtxWheelUndeformed.d() - RCC_VEC3V(m_aWheelLines.B)), vSuspensionAxis);

	// Build a plane with deformed suspension axis and steering direction
	Mat34V mtxWheelDeformed;
	Assert(fabsf(1.0f-m_vecSuspensionAxis.Mag2())<0.01f);
	mtxWheelDeformed.Setc(vSuspensionAxis);
	mtxWheelDeformed.Seta(mtxWheelUndeformed.a());
	mtxWheelDeformed.Setb(Cross(mtxWheelDeformed.c(), mtxWheelDeformed.a()));
	mtxWheelDeformed.Seta(Cross(mtxWheelDeformed.b(), mtxWheelDeformed.c()));
	mtxWheelDeformed.Seta(NormalizeFast(mtxWheelDeformed.a()));
	mtxWheelDeformed.Setd(vWheelPosition);
	Vec4V projectionPlane = BuildPlane(mtxWheelDeformed.d(), mtxWheelDeformed.a());

	// Project wheel matrix to deformed plane
	Vec3V vTemp;
	mtxWheelDeformed.Setb(PlaneProjectVector(projectionPlane, mtxWheelUndeformed.b()));
	mtxWheelDeformed.Setb(NormalizeFast(mtxWheelDeformed.b()));
	mtxWheelDeformed.Setc(PlaneProjectVector(projectionPlane, mtxWheelUndeformed.c()));
	mtxWheelDeformed.Setc(NormalizeFast(mtxWheelDeformed.c()));

	// Wheel matrix is now in local car matrix space, but could be child of non-root bone, so put in space of its parent
	if(iParentIndex > 0)
	{
		UnTransformOrtho(mtxWheelDeformed,rSkeleton.GetObjectMtx(iParentIndex),mtxWheelDeformed);
	}

	rSkeleton.GetLocalMtx(nBoneIndex) = mtxWheelDeformed;
	rSkeleton.PartialUpdate(nBoneIndex);

#if DEBUG_ANIMATED_WHEEL_DEFORMATION
	rSkeleton.GetGlobalMtx(nBoneIndex, matWheelDebug);
	grcDebugDraw::Axis(matWheelDebug, 0.8f, true);
	const Mat34V *pMatParent = rSkeleton.GetParentMtx();
	Vec3V vDebugSuspensionA, vDebugSuspensionB;
	vDebugSuspensionA = Transform(*pMatParent, RCC_VEC3V(m_aWheelLines.A));
	vDebugSuspensionB = Transform(*pMatParent, RCC_VEC3V(m_aWheelLines.B));
	grcDebugDraw::Arrow(vDebugSuspensionB, vDebugSuspensionA, 0.1f, Color_wheat);
#endif
}

void CWheel::ProcessWheelScale(crSkeleton* pSkeleton, CVehicleStructure* pStructure, CWheel * const * UNUSED_PARAM(ppWheels))
{
    int nBoneIndex = pStructure->m_nBoneIndices[m_WheelId];
    if(nBoneIndex < 0)
        return;

    //Displayf("nBoneIndex = %i pStructure = %p m_WheelId = %i", nBoneIndex, pStructure, m_WheelId);
    Matrix34& wheelMatrix = RC_MATRIX34(pSkeleton->GetObjectMtx(nBoneIndex));

    //for animated wheels just make sure that they are in the correct orientation.
    if(REPLAY_ONLY(!CReplayMgr::IsEditModeActive() &&) GetConfigFlags().IsFlagSet(WCF_INSTANCED))
    {
        if(!GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
        {
            wheelMatrix.a.Scale(-1.0f);
            wheelMatrix.c.Scale(-1.0f);
        }

        // This is now done in FragDrawCarWheels
        /*if(GetConfigFlags().IsFlagSet(WCF_INSTANCED_SCALE_REAR))
        {
            static bool scale = true;
            if(scale)
            {
                CWheel* pWheelFrontLeft = ppWheels[0];
                wheelMatrix.a.Scale(GetWheelWidth() / pWheelFrontLeft->GetWheelWidth());
                float fRadiusScale = GetWheelRadius() / pWheelFrontLeft->GetWheelRadius();
                wheelMatrix.b.Scale(fRadiusScale);
                wheelMatrix.c.Scale(fRadiusScale);
            }
        }*/
    }
}

