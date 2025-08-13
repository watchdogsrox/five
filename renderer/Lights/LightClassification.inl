//
//	LightClassification.inl
//
//  This inline CPP file contains functions and data shared between TiledLighting
//  and LightClassificationSPU (which is __SPU build).
//
//	2011/01/31	- Piotr:	- initial TiledLighting module;
//	2011/07/26	- Andrzej:	- initial LightClassificationSPU;
//
//
//
//

#ifndef __LIGHTCLASSIFICATION_INL__
#define __LIGHTCLASSIFICATION_INL__

// =============================================================================================== //
// HELPER FUNCTIONS
// =============================================================================================== //

__forceinline Vec4V_Out GetDistances4(const Vec4V planes[4], Vec3V_In point)
{
	Vec4V d = planes[3];
	d = AddScaled(d, planes[0], SplatX(point));
	d = AddScaled(d, planes[1], SplatY(point));
	d = AddScaled(d, planes[2], SplatZ(point));
	return d;
}

// ----------------------------------------------------------------------------------------------- //

__forceinline Vec4V_Out GetDistances4(const Vec4V plane, Vec4V_In points[3])
{
	const Vec4V planeX = Vec4V(SplatX(plane));
	const Vec4V planeY = Vec4V(SplatY(plane));
	const Vec4V planeZ = Vec4V(SplatZ(plane));
	const Vec4V planeW = Vec4V(SplatW(plane));

	Vec4V d = planeW;
	d = AddScaled(d, planeX, points[0]);
	d = AddScaled(d, planeY, points[1]);
	d = AddScaled(d, planeZ, points[2]);
	return d;
}

// ----------------------------------------------------------------------------------------------- //

__forceinline Vec4V_Out GetDistances4(const Vec4V planes[4], const Vec4V points[3])
{
	Vec4V d = planes[3];
	d = AddScaled(d, planes[0], points[0]);
	d = AddScaled(d, planes[1], points[1]);
	d = AddScaled(d, planes[2], points[2]);
	return d;
}

// ----------------------------------------------------------------------------------------------- //

__forceinline Vec4V_Out Dot4(const Vec4V vec1[3], Vec3V_In point)
{
	Vec4V d =  Scale(vec1[0], SplatX(point));
	d = AddScaled(d, vec1[1], SplatY(point));
	d = AddScaled(d, vec1[2], SplatZ(point));
	return d;
}

// ----------------------------------------------------------------------------------------------- //

__forceinline Vec4V_Out Dot4(const Vec4V vec0, const Vec4V vec1, const Vec4V vec2, Vec3V_In point)
{
	Vec4V d =  Scale(vec0, SplatX(point));
	d = AddScaled(d, vec1, SplatY(point));
	d = AddScaled(d, vec2, SplatZ(point));
	return d;
}

// ----------------------------------------------------------------------------------------------- //

__forceinline void Cross4(Vec4V dst[3], const Vec4V src[3], Vec3V_In point)
{
	dst[0] = SubtractScaled(Scale(src[1], SplatZ(point)), src[2], SplatY(point));
	dst[1] = SubtractScaled(Scale(src[2], SplatX(point)), src[0], SplatZ(point));
	dst[2] = SubtractScaled(Scale(src[0], SplatY(point)), src[1], SplatX(point));
}

// ----------------------------------------------------------------------------------------------- //

__forceinline void Cross4(Vec4V dst[3], Vec3V_In point, const Vec4V src[3])
{
	const Vec4V pointX = Vec4V(SplatX(point));
	const Vec4V pointY = Vec4V(SplatY(point));
	const Vec4V pointZ = Vec4V(SplatZ(point));

	dst[0] = SubtractScaled(Scale(pointY, src[2]), pointZ, src[1]);
	dst[1] = SubtractScaled(Scale(pointZ, src[0]), pointX, src[2]);
	dst[2] = SubtractScaled(Scale(pointX, src[1]), pointY, src[0]);
}

// ----------------------------------------------------------------------------------------------- //

__forceinline void Normalize4(Vec4V dst[3], const Vec4V src[3])
{
	const Vec4V xx = Scale(src[0], src[0]);
	const Vec4V xx_yy = AddScaled(xx, src[1], src[1]);
	const Vec4V xx_yy_zz = AddScaled(xx_yy, src[2], src[2]);

	const Vec4V invsqrt_xx_yy_zz = InvSqrt(xx_yy_zz);

	dst[0] = Scale(src[0], invsqrt_xx_yy_zz);
	dst[1] = Scale(src[1], invsqrt_xx_yy_zz);
	dst[2] = Scale(src[2], invsqrt_xx_yy_zz);
}

// =============================================================================================== //
// HELPER FUNCTIONS
// =============================================================================================== //

static __forceinline u32 IntersectPointLight(
	const PointLightData &pointLight, 
	const Vec4V transposedPlanes0[4], 
	const Vec4V transposedPlanes1[4])
{
	const Vec3V   v_spherePos    = pointLight.m_pointPosAndRadius.GetXYZ();
	const ScalarV v_sphereRadius = pointLight.m_pointPosAndRadius.GetW();

	const Vec4V v_distToCentre0 = GetDistances4(transposedPlanes0, v_spherePos);
	const Vec4V v_distToCentre1 = GetDistances4(transposedPlanes1, v_spherePos);

	const Vec4V v_minDistToCentre = Min(v_distToCentre0, v_distToCentre1);
	const Vec4V v_negativeSphereRadius = Vec4V(Negate(v_sphereRadius));

	return IsGreaterThanOrEqualAll(v_minDistToCentre, v_negativeSphereRadius);
}

// ----------------------------------------------------------------------------------------------- //

static __forceinline u32 IntersectSpotLightAndPlanes_internal(
	const Vec4V transposedPlanes[4],
	Vec3V_In    pos,
	ScalarV_In  falloff,
	Vec3V_In    dir,
	ScalarV_In  cosAngle,
	ScalarV_In  baseX,
	ScalarV_In  baseY,
	ScalarV_In  baseZ,
	ScalarV_In  baseRadius)
{
	Vec4V v_m[3];

	//  == planes - dir * (Dot(planes, dir)) = Cross(dir, Cross(planes, dir)) ==
	const Vec4V pDotd = Dot4(transposedPlanes, dir);
	v_m[0] = SubtractScaled(transposedPlanes[0], Vec4V(SplatX(dir)), pDotd);
	v_m[1] = SubtractScaled(transposedPlanes[1], Vec4V(SplatY(dir)), pDotd);
	v_m[2] = SubtractScaled(transposedPlanes[2], Vec4V(SplatZ(dir)), pDotd);

	Normalize4(v_m, v_m);

	v_m[0] = AddScaled(Vec4V(baseX), v_m[0], baseRadius);
	v_m[1] = AddScaled(Vec4V(baseY), v_m[1], baseRadius);
	v_m[2] = AddScaled(Vec4V(baseZ), v_m[2], baseRadius);

	// Dist to planes from apex
	const Vec4V v_distToApex = GetDistances4(transposedPlanes, pos);

	// Dist to planes from border (cap rim)
	const Vec4V v_distToBorder = GetDistances4(transposedPlanes, v_m);

	// Angle to point on sphere vs direction
	const Vec4V v_cosPhi = Dot4(transposedPlanes, dir);

	const VecBoolV bDistToApex = IsGreaterThanOrEqual(v_distToApex, Vec4V(V_ZERO));
	const VecBoolV bDistToBorder = IsGreaterThanOrEqual(v_distToBorder, Vec4V(V_ZERO));
	const VecBoolV bDistToCap = And(IsGreaterThanOrEqual(v_distToApex, Vec4V(Negate(falloff))), IsGreaterThanOrEqual(v_cosPhi, Vec4V(cosAngle)));

	return IsTrueAll(Or(Or(bDistToApex, bDistToBorder), bDistToCap));
}

// ----------------------------------------------------------------------------------------------- //

static __forceinline u32 IntersectSpotLight(
	const SpotLightData &spotLight, 
	const Vec4V transposedPlanes0[4], 
	const Vec4V transposedPlanes1[4])
{
	const Vec3V   v_pos        = spotLight.m_spotPosAndFalloff.GetXYZ();
	const ScalarV v_falloff    = spotLight.m_spotPosAndFalloff.GetW();
	const Vec3V   v_dir        = spotLight.m_spotDirAndCosAngle.GetXYZ();
	const ScalarV v_cosAngle   = spotLight.m_spotDirAndCosAngle.GetW();
	const ScalarV v_baseX      = spotLight.m_spotBaseAndBaseRadius.GetX();
	const ScalarV v_baseY      = spotLight.m_spotBaseAndBaseRadius.GetY();
	const ScalarV v_baseZ      = spotLight.m_spotBaseAndBaseRadius.GetZ();
	const ScalarV v_baseRadius = spotLight.m_spotBaseAndBaseRadius.GetW();

	return (IntersectSpotLightAndPlanes_internal(transposedPlanes1, v_pos, v_falloff, v_dir, v_cosAngle, v_baseX, v_baseY, v_baseZ, v_baseRadius) &&
			IntersectSpotLightAndPlanes_internal(transposedPlanes0, v_pos, v_falloff, v_dir, v_cosAngle, v_baseX, v_baseY, v_baseZ, v_baseRadius));
}

// ----------------------------------------------------------------------------------------------- //

static __forceinline u32 IntersectCapsuleLight(
	const CapsuleLightData &capsuleLight, 
	const Vec4V transposedPlanes0[4], 
	const Vec4V transposedPlanes1[4])
{
	const Vec3V p1 = capsuleLight.m_capsuleEndCap0PosAndRadius.GetXYZ();
	const Vec3V p2 = capsuleLight.m_capsuleEndCap1PosAndRadius.GetXYZ();
	const Vec4V negativeRadius = Vec4V(Negate(capsuleLight.m_capsuleEndCap0PosAndRadius.GetW()));

	const Vec3V p1_minus_p2 = Subtract(p1, p2);
	const Vec3V p2_minus_p1 = Negate(p1_minus_p2);

	const Vec4V p1_to_planes0 = GetDistances4(transposedPlanes0, p1);
	const Vec4V p1_to_planes1 = GetDistances4(transposedPlanes1, p1);

	const Vec4V Dot_p1_planes0 = Dot4(transposedPlanes0, p1_minus_p2);
	const Vec4V Dot_p1_planes1 = Dot4(transposedPlanes1, p1_minus_p2);

	const Vec4V u0 = InvScale(p1_to_planes0, Dot_p1_planes0);
	const Vec4V u1 = InvScale(p1_to_planes1, Dot_p1_planes1);

	const Vec4V clamped_u0 = Saturate(u0);
	const Vec4V clamped_u1 = Saturate(u1);

	Vec4V points0[3];
	points0[0] = AddScaled(Vec4V(p1.GetX()), Vec4V(p2_minus_p1.GetX()), clamped_u0);
	points0[1] = AddScaled(Vec4V(p1.GetY()), Vec4V(p2_minus_p1.GetY()), clamped_u0);
	points0[2] = AddScaled(Vec4V(p1.GetZ()), Vec4V(p2_minus_p1.GetZ()), clamped_u0);

	Vec4V points1[3];
	points1[0] = AddScaled(Vec4V(p1.GetX()), Vec4V(p2_minus_p1.GetX()), clamped_u1);
	points1[1] = AddScaled(Vec4V(p1.GetY()), Vec4V(p2_minus_p1.GetY()), clamped_u1);
	points1[2] = AddScaled(Vec4V(p1.GetZ()), Vec4V(p2_minus_p1.GetZ()), clamped_u1);

	const Vec4V distToPlanes0 = GetDistances4(transposedPlanes0, points0);
	const Vec4V distToPlanes1 = GetDistances4(transposedPlanes1, points1);

	return IsGreaterThanOrEqualAll(Min(distToPlanes0, distToPlanes1), negativeRadius);
}

// =============================================================================================== //
// FUNCTIONS
// =============================================================================================== //

static void CalcPlanes(
	u32 tileCoordX,
	u32 tileCoordY,
	const u32 tileSize,
	Vec4V transposedPlanes0[4],
	Vec4V transposedPlanes1[4],
	Vec3V points[4],
	Mat34V_In viewToWorld,
	ScalarV_In nearDist,
	ScalarV_In farDist,
	Vec4V_In screenSize,
	Vec4V_In cameraFOV)
{
	Vec4V tile = Vec4V(
		float(tileCoordX) * tileSize, 
		float(tileCoordY) * tileSize,
		float(tileCoordX + 1) * tileSize,
		float(tileCoordY + 1) * tileSize);
	tile = Min(tile, screenSize);

	const Vec4V invScreenSize2 = InvScale(Vec4V(V_TWO), screenSize);
	tile = Scale(Subtract(Scale(tile, invScreenSize2), Vec4V(V_ONE)), cameraFOV);
	
	// Compute plane frustum
	Vec3V cs_p00 = Vec3V(tile.Get<Vec::X, Vec::Y>(), ScalarV(V_NEGONE));
	Vec3V cs_p10 = Vec3V(tile.Get<Vec::Z, Vec::Y>(), ScalarV(V_NEGONE));
	Vec3V cs_p01 = Vec3V(tile.Get<Vec::X, Vec::W>(), ScalarV(V_NEGONE));
	Vec3V cs_p11 = Vec3V(tile.Get<Vec::Z, Vec::W>(), ScalarV(V_NEGONE));

	Vec3V p00 = Transform(viewToWorld, cs_p00);
	Vec3V p10 = Transform(viewToWorld, cs_p10);
	Vec3V p01 = Transform(viewToWorld, cs_p01);
	Vec3V p11 = Transform(viewToWorld, cs_p11);

	const Vec3V apex = viewToWorld.GetCol3();
	const Vec3V cameraDir = viewToWorld.GetCol2();

	Vec4V planes0[4];
	planes0[0] = BuildPlane(apex, p10, p11);
	planes0[1] = BuildPlane(apex, p00, p10);
	planes0[2] = BuildPlane(apex, p01, p00);	
	planes0[3] = BuildPlane(apex, p11, p01);

	Transpose4x4(
		transposedPlanes0[0],
		transposedPlanes0[1],
		transposedPlanes0[2],
		transposedPlanes0[3],
		planes0[0],
		planes0[1],
		planes0[2],
		planes0[3]
	);

	points[0] = p00;
	points[1] = p10;
	points[2] = p01;
	points[3] = p11;

	// Create near and far clip
	Vec4V nearPlane = BuildPlane(AddScaled(apex, Subtract(p00, apex), nearDist), Negate(cameraDir));
	Vec4V farPlane = BuildPlane(AddScaled(apex, Subtract(p11, apex), farDist), cameraDir);

	Vec4V planes1[4];
	planes1[0] = nearPlane;
	planes1[1] = farPlane;
	planes1[2] = nearPlane;
	planes1[3] = farPlane;

	Transpose4x4(
		transposedPlanes1[0],
		transposedPlanes1[1],
		transposedPlanes1[2],
		transposedPlanes1[3],
		planes1[0],
		planes1[1],
		planes1[2],
		planes1[3]);
}

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

static u8 IntersectPointLightsAndPlanes(
	atFixedBitSet<MAX_STORED_LIGHTS> *lightsActive,
	u32 tileIndex,
	u8* lightsIntersected,
	const Vec4V transposedPlanes0[4],
	const Vec4V transposedPlanes1[4],
	const u32 numPointLights,
	const PointLightData* pointData,
	u32* pointLightIndex)
{
	u8 numLightsIntersected = 0;
	for (u8 l = 0; l < numPointLights; ++l)
	{
		if (IntersectPointLight(pointData[l], transposedPlanes0, transposedPlanes1))
		{
		#if __SPU
			sm_lightStats[ pointLightIndex[l] ].m_numTiles64x64++;
		#endif
			lightsIntersected[numLightsIntersected++] = l;
			lightsActive[tileIndex].Set(pointLightIndex[l]);
		}
	}
	return numLightsIntersected;
}

// ----------------------------------------------------------------------------------------------- //

static void IntersectPointLightsAndPlanes(
	const u8 numTilePointLights,
	u8* tilePointLights,
	atFixedBitSet<MAX_STORED_LIGHTS> *lightsActive,
	u32 tileIndex,
	const Vec4V transposedPlanes0[4],
	const Vec4V transposedPlanes1[4],
	const PointLightData* pointData,
	u32* pointLightIndex)
{
	for (u8 l = 0; l < numTilePointLights; ++l)
	{
		const u8 lightIndex = tilePointLights[l];
		if (IntersectPointLight(pointData[lightIndex], transposedPlanes0, transposedPlanes1))
		{
		#if __SPU
			sm_lightStats[ pointLightIndex[lightIndex] ].m_numTiles16x16++;
		#endif
			lightsActive[tileIndex].Set(pointLightIndex[lightIndex]);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

void IntersectSpotLightsAndPlanes(
	const u8 numTileSpotLights,
	u8* tileSpotLights,
	atFixedBitSet<MAX_STORED_LIGHTS> *lightsActive,
	u32 tileIndex,
	const Vec4V transposedPlanes0[4],
	const Vec4V transposedPlanes1[4],
	const SpotLightData* spotData,
	u32* spotLightIndex)
{
	for (u8 l = 0; l < numTileSpotLights; ++l)
	{
		const u8 lightIndex = tileSpotLights[l];
		if (IntersectSpotLight(spotData[lightIndex], transposedPlanes0, transposedPlanes1))
		{
		#if __SPU
			sm_lightStats[ spotLightIndex[lightIndex] ].m_numTiles16x16++;
		#endif
			lightsActive[tileIndex].Set(spotLightIndex[lightIndex]);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

static u8 IntersectSpotLightsAndPlanes(
	atFixedBitSet<MAX_STORED_LIGHTS> *lightsActive,
	u32 tileIndex,
	u8* lightsIntersected,
	const Vec4V transposedPlanes0[4],
	const Vec4V transposedPlanes1[4],
	const u32 numSpotLights,
	const SpotLightData* spotData,
	u32 *spotLightIndex)
{
	u8 numLightsIntersected = 0;
	for (u8 l = 0; l < numSpotLights; ++l)
	{
		if (IntersectSpotLight(spotData[l], transposedPlanes0, transposedPlanes1))
		{
		#if __SPU
			sm_lightStats[ spotLightIndex[l] ].m_numTiles64x64++;
		#endif
			lightsIntersected[numLightsIntersected++] = l;
			lightsActive[tileIndex].Set(spotLightIndex[l]);
		}
	}
	return numLightsIntersected;
}

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

static void IntersectCapsuleLightsAndPlanes(
	const u8 numTileCapsuleLights,
	u8* tileCapsuleLights,
	atFixedBitSet<MAX_STORED_LIGHTS> *lightsActive,
	u32 tileIndex,
	const Vec4V transposedPlanes0[4],
	const Vec4V transposedPlanes1[4],
	const CapsuleLightData* capsuleData,
	u32* capsuleLightIndex)
{
	for (u8 l = 0; l < numTileCapsuleLights; ++l)
	{
		const u8 lightIndex = tileCapsuleLights[l];
		if (IntersectCapsuleLight(capsuleData[lightIndex], transposedPlanes0, transposedPlanes1))
		{
		#if __SPU
			sm_lightStats[ capsuleLightIndex[lightIndex] ].m_numTiles16x16++;
		#endif
			lightsActive[tileIndex].Set(capsuleLightIndex[lightIndex]);
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

static u8 IntersectCapsuleLightsAndPlanes(
	atFixedBitSet<MAX_STORED_LIGHTS> *lightsActive,
	u32 tileIndex,
	u8* lightsIntersected,
	const Vec4V transposedPlanes0[4],
	const Vec4V transposedPlanes1[4],
	const u32 numCapsuleLights,
	const CapsuleLightData* capsuleData,
	u32 *capsuleLightIndex)
{
	u8 numLightsIntersected = 0;
	for (u8 l = 0; l < numCapsuleLights; ++l)
	{
		if (IntersectCapsuleLight(capsuleData[l], transposedPlanes0, transposedPlanes1))
		{
		#if __SPU
			sm_lightStats[ capsuleLightIndex[l] ].m_numTiles64x64++;
		#endif
			lightsIntersected[numLightsIntersected++] = l;
			lightsActive[tileIndex].Set(capsuleLightIndex[l]);
		}
	}
	return numLightsIntersected;
}

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

/*
static void ProcessLightCullingPlanes(
	atFixedBitSet<MAX_STORED_LIGHTS>* lightsActive,
	const u32 tileIndex,
	const Vec4V* planeData,
	const Vec3V points[4],
	const Vec3V apex,
	ScalarV_In nearDist,
	ScalarV_In farDist)
{
	Vec4V tranposePoints[3];

	Transpose3x4to4x3(
		tranposePoints[0],
		tranposePoints[1],
		tranposePoints[2],
		points[0],
		points[1],
		points[2],
		points[3]
		);

	Vec4V transposeApex[3];
	transposeApex[0] = Vec4V(SplatX(apex));
	transposeApex[1] = Vec4V(SplatY(apex));
	transposeApex[2] = Vec4V(SplatZ(apex));

	Vec4V cornerVector[3];
	cornerVector[0] = Subtract(tranposePoints[0], transposeApex[0]);
	cornerVector[1] = Subtract(tranposePoints[1], transposeApex[1]);
	cornerVector[2] = Subtract(tranposePoints[2], transposeApex[2]);
	
	Vec4V cornerDirections[3];
	Normalize4(cornerDirections, cornerVector);

	Vec4V nearPoints[3];
	nearPoints[0] = AddScaled(transposeApex[0], cornerDirections[0], nearDist);
	nearPoints[1] = AddScaled(transposeApex[1], cornerDirections[1], nearDist);
	nearPoints[2] = AddScaled(transposeApex[2], cornerDirections[2], nearDist);

	Vec4V farPoints[3];
	farPoints[0] = AddScaled(transposeApex[0], cornerDirections[0], farDist);
	farPoints[1] = AddScaled(transposeApex[1], cornerDirections[1], farDist);
	farPoints[2] = AddScaled(transposeApex[2], cornerDirections[2], farDist);

	for (int i = 0; i < MAX_STORED_LIGHTS; i++)
	{
		if (lightsActive[tileIndex].IsSet(i))
		{
			const Vec4V v_distToPlane0 = GetDistances4(planeData[i], nearPoints);
			const Vec4V v_distToPlane1 = GetDistances4(planeData[i], farPoints);

			if (IsLessThanAll(Max(v_distToPlane0, v_distToPlane1), Vec4V(V_ZERO)))
			{
				lightsActive[tileIndex].Clear(i);
			}
		}
	}
}
*/

// ----------------------------------------------------------------------------------------------- //
// ----------------------------------------------------------------------------------------------- //

static void ClassifyLights(
	const u32 tileStartX, 
	const u32 tileEndX,
	const u32 tileStartY, 
	const u32 tileEndY,
	const u32 tileOffsetX, 
	const u32 tileOffsetY, 
	const u32 totalTiles,
	const u32 totalSubTiles,
	atFixedBitSet<MAX_STORED_LIGHTS>* tileActiveLights,
	const float* tileData,
	atFixedBitSet<MAX_STORED_LIGHTS>* subTileActiveLights,
	const float* subTileData,
	Vec4V_In screenSize,
	Vec4V_In cameraFOV,
	Mat34V_In viewToWorld,
	LightData* lightData)
{
	atFixedBitSet<MAX_STORED_LIGHTS> allLightsOff;
	allLightsOff.Reset(MAX_STORED_LIGHTS);

	for (u32 i = 0; i < totalTiles; ++i) { tileActiveLights[i] = allLightsOff; }
	for (u32 i = 0; i < totalSubTiles; ++i) { subTileActiveLights[i] = allLightsOff; }

	// Early drop out
	if ((lightData->m_numPointLights + lightData->m_numSpotLights + lightData->m_numCapsuleLights) == 0)
	{
		return;
	}

	for (u32 y = tileStartY; y < tileEndY; ++y)
	{
		for (u32 x = tileStartX; x < tileEndX; ++x)
		{
			const u32 tileIndex = y * NUM_TILES_X + x;
			const u32 cornerSubTileX = (x * TILE_SIZE) / SUB_TILE_SIZE;
			const u32 cornerSubTileY = (y * TILE_SIZE) / SUB_TILE_SIZE;

			// Get minimum and maximum distance for sub tile
			const ScalarV tileNearDist = ScalarV(tileData[tileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MIN]);
			const ScalarV tileFarDist = ScalarV(tileData[tileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MAX]);

			if (IsGreaterThanAll(tileNearDist, tileFarDist))
			{
				continue;
			}

			Vec4V tileTransposedPlanes0[4], tileTransposedPlanes1[4];
			Vec3V tileCornerPoints[4];

			// Calculate planes for tile
			CalcPlanes(
				x + tileOffsetX,
				y + tileOffsetY,
				TILE_SIZE,
				tileTransposedPlanes0, 
				tileTransposedPlanes1,
				tileCornerPoints,
				viewToWorld,
				tileNearDist,
				tileFarDist,
				screenSize,
				cameraFOV);

			// Point Lights
			u8 tilePointLights[MAX_STORED_LIGHTS];
			const u8 numTilePointLights = IntersectPointLightsAndPlanes(
				tileActiveLights,
				tileIndex,
				tilePointLights,
				tileTransposedPlanes0, 
				tileTransposedPlanes1,
				lightData->m_numPointLights,
				lightData->m_pointLights,
				lightData->m_pointLightIndex);

			// Spot Lights
			u8 tileSpotLights[MAX_STORED_LIGHTS];
			const u8 numTileSpotLights = IntersectSpotLightsAndPlanes(
				tileActiveLights,
				tileIndex,
				tileSpotLights,
				tileTransposedPlanes0, 
				tileTransposedPlanes1,
				lightData->m_numSpotLights,
				lightData->m_spotLights,
				lightData->m_spotLightIndex);

			// Capsule Lights
			u8 tileCapsuleLights[MAX_STORED_LIGHTS];
			const u8 numTileCapsuleLights = IntersectCapsuleLightsAndPlanes(
				tileActiveLights,
				tileIndex,
				tileCapsuleLights,
				tileTransposedPlanes0, 
				tileTransposedPlanes1,
				lightData->m_numCapsuleLights,
				lightData->m_capsuleLights,
				lightData->m_capsuleLightIndex);

			/*
			ProcessLightCullingPlanes(
				tileActiveLights,
				tileIndex,
				lightData->m_clippingPlanes,
				tileCornerPoints,
				viewToWorld.GetCol3(),
				tileNearDist,
				tileFarDist);
			*/

			for (u32 sy = 0; sy < SUB_TILE_TO_TILE_OFFSET; ++sy)
			{
				for (u32 sx = 0; sx < SUB_TILE_TO_TILE_OFFSET; ++sx)
				{
					const u32 subTileX = Min(cornerSubTileX + sx, u32(NUM_SUB_TILES_X));
					const u32 subTileY = Min(cornerSubTileY + sy, u32(NUM_SUB_TILES_Y));
					const u32 subTileIndex = subTileY * NUM_SUB_TILES_X + subTileX;

					// Get minimum and maximum distance for sub tile
					const ScalarV subTileNearDist = ScalarV(subTileData[subTileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MIN]);
					const ScalarV subTileFarDist = ScalarV(subTileData[subTileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MAX]);

					if (IsGreaterThanAll(subTileNearDist, subTileFarDist))
					{
						continue;
					}

					// Calculate transpose planes
					Vec4V subTileTransposedPlanes0[4], subTileTransposedPlanes1[4];
					Vec3V subTileCornerPoints[4];

					CalcPlanes(
						subTileX + (tileOffsetX * SUB_TILE_TO_TILE_OFFSET),
						subTileY + (tileOffsetY * SUB_TILE_TO_TILE_OFFSET),
						SUB_TILE_SIZE,
						subTileTransposedPlanes0, 
						subTileTransposedPlanes1,
						subTileCornerPoints,
						viewToWorld,
						subTileNearDist,
						subTileFarDist,
						screenSize,
						cameraFOV);

					// Process all lights for this tile
					IntersectPointLightsAndPlanes(
						numTilePointLights,
						tilePointLights,
						subTileActiveLights,
						subTileIndex,
						subTileTransposedPlanes0, 
						subTileTransposedPlanes1,
						lightData->m_pointLights,
						lightData->m_pointLightIndex);

					IntersectSpotLightsAndPlanes(
						numTileSpotLights,
						tileSpotLights,
						subTileActiveLights,
						subTileIndex,
						subTileTransposedPlanes0, 
						subTileTransposedPlanes1,
						lightData->m_spotLights,
						lightData->m_spotLightIndex);

					IntersectCapsuleLightsAndPlanes(
						numTileCapsuleLights,
						tileCapsuleLights,
						subTileActiveLights,
						subTileIndex,
						subTileTransposedPlanes0, 
						subTileTransposedPlanes1,
						lightData->m_capsuleLights,
						lightData->m_capsuleLightIndex);

					/*
					ProcessLightCullingPlanes(
						subTileActiveLights,
						subTileIndex,
						lightData->m_clippingPlanes,
						subTileCornerPoints,
						viewToWorld.GetCol3(),
						subTileNearDist,
						subTileFarDist);
					*/
				}
			}
		}
	}

	/*
	// All the sub-tiles
	for (u32 t = subTileStartIndex; t < subTileEndIndex; t++) // Process all sub tiles
	{
		// Get minimum and maximum distance for sub tile
		const float minDist = subTileData[t * NUM_TILE_COMPONENTS + TILE_DEPTH_MIN];
		const float maxDist = subTileData[t * NUM_TILE_COMPONENTS + TILE_DEPTH_MAX];

		subTileActiveLights[t] = initialLights;

		if (minDist > maxDist)
		{
			subTileActiveLights[t] = allLightsOff;
			continue;
		}

		// Calculate transpose planes
		Vec4V transposedPlanes0[4], transposedPlanes1[4];
		Vec3V points[4];
		const ScalarV nearDist(minDist), farDist(maxDist);

		// Work out tile x/y co-ords
		const u32 subTileX = t % NUM_SUB_TILES_X;
		const u32 subTileY = (t - subTileX) / NUM_SUB_TILES_X;

		// Calculate planes for tile
		CalcPlanes(
			subTileX,
			subTileY,
			SUB_TILE_SIZE,
			transposedPlanes0, 
			transposedPlanes1,
			points,
			camDir,
			viewToWorld,
			nearDist,
			farDist,
			screenSize,
			cameraFOV);

		// Process all lights for this tile
		IntersectPointLightsAndPlanes(
			subTileActiveLights,
			t,
			transposedPlanes0,
			transposedPlanes1,
			lightData->m_numPointLights,
			lightData->m_pointLights,
			lightData->m_pointLightIndex);

		IntersectSpotLightsAndPlanes(
			subTileActiveLights,
			t,
			transposedPlanes0,
			transposedPlanes1,
			lightData->m_numSpotLights,
			lightData->m_spotLights,
			lightData->m_spotLightIndex);

		IntersectCapsuleLightsAndPlanes(
			subTileActiveLights,
			t,
			transposedPlanes0,
			transposedPlanes1,
			lightData->m_numCapsuleLights,
			lightData->m_capsuleLights,
			lightData->m_capsuleLightIndex);

		if (subTileActiveLights[t].CountOnBits() > 0)
		{
			// Process light culling planes
			ProcessLightCullingPlanes(
				subTileActiveLights,
				t,
				lightData->m_clippingPlanes,
				points,
				viewToWorld.GetCol3(),
				nearDist,
				farDist);
		}
	}
	*/
}

// ----------------------------------------------------------------------------------------------- //
#if __SPU
// ----------------------------------------------------------------------------------------------- //

static void DepthDownsampleUnpackSPU(Vec::Vector_4V* dst, Vec::Vector_4V* src, const u32 totalTiles)
{
	u32 srcOffset = 0;
	for (u32 i = 0; i < totalTiles / 2; i++) 
	{
		Vec::Vector_4V a0, b0;
		Vec::V4Float16Vec8Unpack(a0, b0, src[srcOffset]);
		dst[0] = a0; 
		dst[1] = b0;
		dst += 2;
		srcOffset++;
	}
}

// ----------------------------------------------------------------------------------------------- //

static void ClassifyLightsSPU(
	const u32 tileStartX,
	const u32 tileEndX,
	const u32 tileStartY,
	const u32 tileEndY,
	const u32 tileOffsetX,
	const u32 tileOffsetY,
	const u32 totalTiles,
	const u32 totalSubTiles,
	atFixedBitSet<MAX_STORED_LIGHTS>* tileActiveLights,
	const float* tileClassificationData,
	atFixedBitSet<MAX_STORED_LIGHTS>* subTileActiveLights,
	const float* subTileClassificationData,
	CSPULightClassificationInfo* lcInfo)
{
	// Screen size and FOV
	const Vec4V screenSize(
		lcInfo->m_vpWidth, lcInfo->m_vpHeight,
		lcInfo->m_vpWidth, lcInfo->m_vpHeight);

	const Vec4V cameraFOV(
		lcInfo->m_vpTanHFOV, -lcInfo->m_vpTanVFOV,
		lcInfo->m_vpTanHFOV, -lcInfo->m_vpTanVFOV);

	ClassifyLights(
		tileStartX, 
		tileEndX,
		tileStartY,
		tileEndY,
		tileOffsetX,
		tileOffsetY,
		totalTiles,
		totalSubTiles,
		tileActiveLights,
		tileClassificationData,
		subTileActiveLights,
		subTileClassificationData,
		screenSize,
		cameraFOV,
		lcInfo->m_vpCameraMtx,
		&lcInfo->m_lightData);
}

// ----------------------------------------------------------------------------------------------- //
#endif // __SPU
// ----------------------------------------------------------------------------------------------- //

// ----------------------------------------------------------------------------------------------- //
#if __PPU || __XENON
// ----------------------------------------------------------------------------------------------- //
static void ClassifyLightsGPU(const grcViewport* UNUSED_PARAM(viewport))
{
	/*
	s32 tileStartIndex = 0, 
		tileEndIndex = TOTAL_SUB_TILES;

	u32 lightStartIndex = 0,
		lightEndIndex = Lights::m_numRenderLights;

#if __BANK
	if (!(DebugDeferred::m_bool[ENABLE_TILED_LIGHTING]))
		return;

	DebugLights::GetLightIndexRange(lightStartIndex, lightEndIndex);

	DebugDeferred::GetTileRange(tileStartIndex, tileEndIndex);
#endif //__BANK...

	// Prepare variables for classification
	Vec4V_In screenSize(
		viewport->GetWidth(), viewport->GetHeight(),
		viewport->GetWidth(), viewport->GetHeight());

	const Vec4V cameraFOV(
		viewport->GetTanHFOV(), -viewport->GetTanVFOV(),
		viewport->GetTanHFOV(), -viewport->GetTanVFOV());

	ClassifyLights(
		CTiledLighting::GetActiveLights(),
		tileStartIndex, 
		tileEndIndex,
		0,
		0,
		TOTAL_TILES,
		TOTAL_SUB_TILES,
		CTiledLighting::GetSubTileData(),
		screenSize,
		cameraFOV,
		viewport->GetCameraMtx(),
		VECTOR3_TO_VEC3V(camInterface::GetFront()));
	*/
};

// ----------------------------------------------------------------------------------------------- //
#endif // __PPU...
// ----------------------------------------------------------------------------------------------- //

#endif //__LIGHTCLASSIFICATION_INL__...
