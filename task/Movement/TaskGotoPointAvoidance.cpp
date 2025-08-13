//
// TaskGotoPointAvoidance
// 
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Movement/TaskGotoPointAvoidance.h"


// Rage headers
#include "Math/angmath.h"

//#pragma optimize("", off)
AI_OPTIMISATIONS()
AI_NAVIGATION_OPTIMISATIONS()

namespace TaskGotoPointAvoidance
{

#if __BANK && defined(DEBUG_DRAW)

	void debug_draw_localspace_rectangle(Vec2V_In in_LocalBoxMin, Vec2V_In in_LocalBoxMax, Vec2V_In in_WorldPos, ScalarV_In in_Heading, ScalarV_In in_Z, Color32 in_Color )
	{

		Vec2V localV1 = Vec2V(in_LocalBoxMin.GetX(),  in_LocalBoxMin.GetY());
		Vec2V localV2 = Vec2V(in_LocalBoxMin.GetX(),  in_LocalBoxMax.GetY());
		Vec2V localV3 = Vec2V(in_LocalBoxMax.GetX(),  in_LocalBoxMax.GetY());
		Vec2V localV4 = Vec2V(in_LocalBoxMax.GetX(),  in_LocalBoxMin.GetY());

		static ScalarV s_Sign = ScalarV(V_ONE);	

		Vec2V worldV1 = Rotate(localV1, in_Heading * s_Sign) + in_WorldPos;
		Vec2V worldV2 = Rotate(localV2, in_Heading * s_Sign) + in_WorldPos;
		Vec2V worldV3 = Rotate(localV3, in_Heading * s_Sign) + in_WorldPos;
		Vec2V worldV4 = Rotate(localV4, in_Heading * s_Sign) + in_WorldPos;

		Vec3V worldV13d = Vec3V(worldV1, in_Z );
		Vec3V worldV23d = Vec3V(worldV2, in_Z );
		Vec3V worldV33d = Vec3V(worldV3, in_Z );
		Vec3V worldV43d = Vec3V(worldV4, in_Z );

		grcDebugDraw::Line(worldV13d, worldV23d, in_Color );
		grcDebugDraw::Line(worldV23d, worldV33d, in_Color );
		grcDebugDraw::Line(worldV33d, worldV43d, in_Color );
		grcDebugDraw::Line(worldV43d, worldV13d, in_Color );

	}

#endif

	namespace box_avoidance
	{
		bool compute_ray_slab_intersection( float slabmin, float slabmax, float raystart, float raydir, float& tbenter, float& tbexit)
		{	
			if (fabs(raydir) < FLT_EPSILON)	
			{		
				if(raystart < slabmin || raystart > slabmax)		
				{
					return false;		
				}
				else		
				{			
					return true;
				}	
			}	

			float tsenter = (slabmin - raystart) / raydir;	
			float tsexit = (slabmax - raystart) / raydir;	

			if(tsenter > tsexit)	
			{					
				float tmp = tsenter;
				tsenter = tsexit;
				tsexit = tmp;
			}	

			tbenter = Max(tbenter, tsenter);		
			tbexit = Min(tbexit, tsexit);		
			return tbenter < tbexit;	
		}

		bool compute_circle_to_box_collision(ScalarV_Ref o_time, Vec2V_In in_point1, Vec2V_In in_vel1, ScalarV_In in_radius1, 
			Vec2V_In in_point2, Vec2V_In in_vel2, Vec2V_In in_localBoxMin, Vec2V_In in_localBoxMax, ScalarV_In in_heading)
		{
			// created padded box
			Vec2V boxMin = in_localBoxMin - Vec2V(in_radius1, in_radius1);
			Vec2V boxMax = in_localBoxMax + Vec2V(in_radius1, in_radius1);

			Vec2V vel1reltovel2 = in_vel1 - in_vel2;
			Vec2V pt1reltopt2 = in_point1 - in_point2;

			static ScalarV s_Sign = ScalarV(V_NEGONE);		

			Vec2V pt1reltopt2InLocalSpace = Rotate(pt1reltopt2, s_Sign * in_heading);
			Vec2V vel1reltovel2InLocalSpace = Rotate(vel1reltovel2, s_Sign * in_heading);

			Vec2V segStart = pt1reltopt2InLocalSpace;
			Vec2V segVel = vel1reltovel2InLocalSpace;

			float tenter = 0;
			float texit = FLT_MAX; 	

			if (!compute_ray_slab_intersection(boxMin.GetXf(), boxMax.GetXf(), segStart.GetXf(), segVel.GetXf(), tenter, texit))
			{		
				return false;	
			}	

			if (!compute_ray_slab_intersection(boxMin.GetYf(), boxMax.GetYf(), segStart.GetYf(), segVel.GetYf(), tenter, texit))
			{		
				return false;	
			}		

			o_time.Set(tenter);

			return true;
		}

		//
		// find the box vertex that requires the most left and the most right turn
		//
		void compute_box_tangent_to_point(Vec2V_Ref o_Left, Vec2V_Ref o_Right, Vec2V_In in_point1, Vec2V_In in_LocalBoxMin, Vec2V_In in_LocalBoxMax, Vec2V_In in_point2, ScalarV_In in_Heading )
		{
			static const ScalarV fOne( V_ONE );
			static const ScalarV fZero( V_ZERO );

			Vec2V localV1 = Vec2V(in_LocalBoxMin.GetX(),  in_LocalBoxMin.GetY());
			Vec2V localV2 = Vec2V(in_LocalBoxMin.GetX(),  in_LocalBoxMax.GetY());
			Vec2V localV3 = Vec2V(in_LocalBoxMax.GetX(),  in_LocalBoxMax.GetY());
			Vec2V localV4 = Vec2V(in_LocalBoxMax.GetX(),  in_LocalBoxMin.GetY());

			Vec2V worldV1 = Rotate(localV1, in_Heading) + in_point2;
			Vec2V worldV2 = Rotate(localV2, in_Heading) + in_point2;
			Vec2V worldV3 = Rotate(localV3, in_Heading) + in_point2;
			Vec2V worldV4 = Rotate(localV4, in_Heading) + in_point2;		

			Vec2V p1Top2 = in_point2 - in_point1;

			Vec2V vFwd = Normalize(p1Top2);
			Vec2V vRight = Vec2V(vFwd.GetY(), Negate(vFwd.GetX()));

			Vec2V vP1To[4];
			vP1To[0] = worldV1 - in_point1;
			vP1To[1] = worldV2 - in_point1;
			vP1To[2] = worldV3 - in_point1;
			vP1To[3] = worldV4 - in_point1;

			ScalarV vMaxLeftDot = ScalarV(FLT_MAX);
			ScalarV vMaxRightDot = ScalarV(FLT_MAX);

			for (int i = 0; i < 4; i++)
			{
				Vec2V vDir = Normalize(vP1To[i]);
				ScalarV vDotRight = Dot(vDir, vRight);
				ScalarV vDotFwd = Dot(vDir, vFwd);
				if ( (vDotRight >= ScalarV(V_ZERO) ).Getb() )
				{
					if ( (vDotFwd <= vMaxRightDot).Getb() )
					{
						vMaxRightDot = vDotFwd;
						o_Right = vDir;
					}
				}
				else
				{
					if ( (vDotFwd <= vMaxLeftDot).Getb() )
					{
						vMaxLeftDot = vDotFwd;
						o_Left = vDir;
					}
				}
			}	
		}


		bool compute_ray_box_intersection(Vec2V_Ref o_Intersection, Vec2V_Ref o_Vertex1, Vec2V_Ref o_Vertex2, Vec2V_In in_Position, Vec2V_In in_EndPosition, const Vec2V in_Points[] )
		{
			float fMaxDistance2 = 0;
			int iMaxIndex = 0;
			for ( int i = 0; i < 4; i++ )
			{
				float fDist2 = DistSquared(in_Position, in_Points[i]).Getf();
				if ( fDist2 > fMaxDistance2 )
				{
					fMaxDistance2 = fDist2;
					iMaxIndex = i;
				}
			}

			for ( int i = 0; i < 4; i++ )
			{
				int v1 = i;
				int v2 = (i + 1)%4;

				if (v1 != iMaxIndex && v2 != iMaxIndex )
				{
					o_Vertex1 = in_Points[v1];
					o_Vertex2 = in_Points[v2];

					Vector3 vIntersection;
					Vector3 vstart = Vector3(in_Position.GetXf(), in_Position.GetYf(), 0.0f);
					Vector3 vend = Vector3(in_EndPosition.GetXf(), in_EndPosition.GetYf(), 0.0f);
					Vector3 vsegA = Vector3(o_Vertex1.GetXf(), o_Vertex1.GetYf(), 0.0f);
					Vector3 vsegB = Vector3(o_Vertex2.GetXf(), o_Vertex2.GetYf(), 0.0f);
						
					// check to see if we intersect this segment 
					if(CNavMesh::LineSegsIntersect2D(vstart, vend, vsegA, vsegB, &vIntersection) == SEGMENTS_INTERSECT )
					{					
						// check we're not already on other side of object
						// note assume point ordering here
						o_Intersection = Vec2V(vIntersection.x, vIntersection.y);
						Vec2V vSegDir = Normalize(o_Vertex2 - o_Vertex1);
						Vec2V vSegRight = Vec2V(vSegDir.GetY(), -vSegDir.GetX());
						Vec2V vIntersectionToPos = Normalize(in_Position - o_Intersection);
						if ( (Dot(vSegRight, vIntersectionToPos ) >= ScalarV(0.0f) ).Getb() )
						{
							return true;
						}
					}
				}
			}
			return false;
		}

	}

	namespace circle_avoidance
	{
		bool compute_circle_tangent_to_point(ScalarV_Ref o_angle, Vec2V_In in_point1, Vec2V_In in_point2, ScalarV_In in_radius )
		{
			ScalarV hypotenuse = Dist(in_point2, in_point1);
			if ( (hypotenuse > in_radius).Getb() )
			{
				ScalarV opposite = in_radius;

				o_angle.Setf(asin(opposite.Getf()/hypotenuse.Getf()));
				return true;
			}
			return false;
		}

		bool compute_circle_collision(ScalarV_Ref o_time, Vec2V_In in_point1, Vec2V_In in_vel1, ScalarV_In in_radius1, Vec2V_In in_point2, Vec2V_In in_vel2, ScalarV_In in_radius2 )
		{
			static const ScalarV fZero( V_ZERO );
			Vec2V p1top2 = in_point2 - in_point1;
			Vec2V p1velreltop2 = in_vel1 - in_vel2;
			Vec2V p1dirreltop2 = Normalize(p1velreltop2);
			ScalarV l = Dot(p1dirreltop2, p1top2);
			if ( IsGreaterThan(l, fZero).Getb() )
			{
				ScalarV a_squared = square(l);			
				ScalarV c_squared = MagSquared(p1top2);			
				ScalarV b_squared = c_squared - a_squared;
				ScalarV r_squared = square(in_radius1 + in_radius2);

				if ( (b_squared < r_squared).Getb() )
				{
					Vec2V p3 = in_point1 + p1dirreltop2 * l;
					if ( abs( p1velreltop2.GetXf() ) > FLOAT_EPSILON )
					{
						o_time.Setf((p3.GetXf() - in_point1.GetXf()) / p1velreltop2.GetXf());
					}
					else
					{
						o_time.Setf((p3.GetYf() - in_point1.GetYf()) / p1velreltop2.GetYf());
					}
					return true;
				}
			}

			return false;				
		}
	}


	dev_float            ZUPFOROBJECTTOBEUPRIGHT                                = 0.9f;

	void ComputeLocalSpaceBoundRectAndHeading(ScalarV_Ref o_fHeading, Vec2V_Ref o_vMinXYOut, Vec2V_Ref o_vMaxXYOut, const CPhysical& in_Object)
	{
		spdAABB localBox;
		localBox = in_Object.GetLocalSpaceBoundBox(localBox);
		o_vMinXYOut = localBox.GetMin().GetXY();
		o_vMaxXYOut = localBox.GetMax().GetXY();
		o_fHeading = ScalarV(in_Object.GetTransform().GetHeading());

		const Matrix34& mat = MAT34V_TO_MATRIX34(in_Object.GetTransform().GetMatrix());
		if( mat.c.z > ZUPFOROBJECTTOBEUPRIGHT )
		{
		//	o_vMinXYOut = localBox.GetMin().GetXY();
		//	o_vMaxXYOut = localBox.GetMax().GetXY();
		//	o_fHeading = ScalarV(in_Object.GetTransform().GetHeading());
		}
		else if( mat.c.z < -ZUPFOROBJECTTOBEUPRIGHT )
		{
			o_vMinXYOut = localBox.GetMax().GetXY();
			o_vMaxXYOut = localBox.GetMin().GetXY();
			o_fHeading = ScalarV(-in_Object.GetTransform().GetHeading());
		}
		else if( mat.b.z > ZUPFOROBJECTTOBEUPRIGHT )
		{
			o_vMinXYOut = Vec2V( localBox.GetMin().GetX(), localBox.GetMin().GetZ());
			o_vMaxXYOut = Vec2V( localBox.GetMax().GetX(), localBox.GetMax().GetZ());
			o_fHeading = ScalarV(in_Object.GetTransform().GetPitch());
		}
		else if( mat.b.z < -ZUPFOROBJECTTOBEUPRIGHT )
		{
			o_vMinXYOut = Vec2V( localBox.GetMax().GetX(), localBox.GetMax().GetZ());
			o_vMaxXYOut = Vec2V( localBox.GetMin().GetX(), localBox.GetMin().GetZ());
			o_fHeading = ScalarV(-in_Object.GetTransform().GetPitch());
		}
		else if( mat.a.z > ZUPFOROBJECTTOBEUPRIGHT )
		{
			o_vMinXYOut = Vec2V( localBox.GetMin().GetY(), localBox.GetMin().GetZ());
			o_vMaxXYOut = Vec2V( localBox.GetMax().GetY(), localBox.GetMax().GetZ());
			o_fHeading = ScalarV(in_Object.GetTransform().GetRoll());
		}
		else if( mat.a.z < -ZUPFOROBJECTTOBEUPRIGHT )
		{
			o_vMinXYOut = Vec2V( localBox.GetMax().GetY(), localBox.GetMax().GetZ());
			o_vMaxXYOut = Vec2V( localBox.GetMin().GetY(), localBox.GetMin().GetZ());
			o_fHeading = ScalarV(-in_Object.GetTransform().GetRoll());
		}
	}
	
	void GenerateObjectAvoidanceArrays(sObjAvoidanceArrays& o_AvoidanceArrays, const sAvoidanceParams& in_Params)
	{
		const ScalarV fTwo( V_TWO );
		const Vec2V vPedPos(in_Params.vPedPos3d.GetIntrin128ConstRef());

		CEntityScannerIterator entityList = in_Params.pPed->GetPedIntelligence()->GetNearbyObjects();
		for( CEntity* pEnt = entityList.GetFirst(); pEnt && o_AvoidanceArrays.iNumEntities < CEntityScanner::MAX_NUM_ENTITIES; pEnt = entityList.GetNext() )
		{
			const CObject* pObj = (CObject*)pEnt;

			if(pObj == in_Params.pDontAvoidEntity)
				continue;

			if ( !pObj->IsInPathServer() )
				continue;

			if( pObj->GetIsAttached() ) 
				continue;

			if  ( pObj->m_nObjectFlags.bIsPickUp )
				continue;

			if ( pObj->IsADoor() )
				continue;

			Vec3V vObjPos3d(pObj->GetTransform().GetPosition());
			if( (Abs(in_Params.vPedPos3d.GetZ() - vObjPos3d.GetZ()) > fTwo).Getb() )
			{
				continue;
			}

			Vec2V vOtherPos(vObjPos3d.GetIntrin128ConstRef());
			if((DistSquared(vPedPos, vOtherPos) < square(in_Params.fScanDistance)).Getb())
			{		
				const TDynamicObject& obj = CPathServerGta::m_PathServerThread.GetDynamicObject(pObj->GetPathServerDynamicObjectIndex());

				o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][0] = Vec2V(obj.m_vVertices[0].x, obj.m_vVertices[0].y);
				o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][1] = Vec2V(obj.m_vVertices[1].x, obj.m_vVertices[1].y);
				o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][2] = Vec2V(obj.m_vVertices[2].x, obj.m_vVertices[2].y);
				o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][3] = Vec2V(obj.m_vVertices[3].x, obj.m_vVertices[3].y);	
				//o_AvoidanceArrays.vOtherEntPos[o_AvoidanceArrays.iNumEntities] = vOtherPos;
				o_AvoidanceArrays.vOtherEntVel[o_AvoidanceArrays.iNumEntities] = RCC_VEC3V(pObj->GetVelocity()).GetXY();
				o_AvoidanceArrays.iNumEntities++;
			}
		}

		/*
		// Also nearby fires
		if(in_Params.pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_AVOID_FIRE))
		{
			const Vector3 * pNearbyFires = in_Params.pPed->GetPedIntelligence()->GetEventScanner()->GetFireScanner().GetNearbyFires();
			for(s32 f=0; f<CNearbyFireScanner::NUM_NEARBY_FIRES && o_AvoidanceArrays.iNumEntities < CEntityScanner::MAX_NUM_ENTITIES; f++)
			{
				if(pNearbyFires->IsZero())
					break;

				if( (Abs(in_Params.vPedPos3d.GetZ() - RCC_VEC3V(pNearbyFires[f]).GetZ()) > fTwo).Getb() )
				{
					continue;
				}
				Vec2V vFirePos;
				vFirePos.SetIntrin128( RCC_VEC3V(pNearbyFires[f]).GetIntrin128ConstRef() );

				if((DistSquared(vPedPos, vFirePos) < square(in_Params.fScanDistance)).Getb())
				{
					ScalarV fFireRadius = RCC_VEC3V(pNearbyFires[f]).GetW();
					o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][0] = Vec2V(vFirePos.GetX() - fFireRadius, vFirePos.GetY() - fFireRadius);
					o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][1] = Vec2V(vFirePos.GetX() + fFireRadius, vFirePos.GetY() - fFireRadius);
					o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][2] = Vec2V(vFirePos.GetX() + fFireRadius, vFirePos.GetY() + fFireRadius);
					o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][3] = Vec2V(vFirePos.GetX() - fFireRadius, vFirePos.GetY() + fFireRadius);
					o_AvoidanceArrays.vOtherEntVel[o_AvoidanceArrays.iNumEntities] = Vec2V(ScalarV(V_ZERO), ScalarV(V_ZERO));
					o_AvoidanceArrays.iNumEntities++;
				}
			}
		}
		*/
	}

	void GenerateVehicleAvoidanceArrays(sObjAvoidanceArrays& o_AvoidanceArrays, const sAvoidanceParams& in_Params)
	{
		if ( !in_Params.pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignore_All ) )
		{
			const ScalarV fTwo( V_TWO );
			const Vec2V vPedPos(in_Params.vPedPos3d.GetIntrin128ConstRef());

			// Create list of peds which may be obstructions
			const CEntityScannerIterator entityList = in_Params.pPed->GetPedIntelligence()->GetNearbyVehicles();
			for( const CEntity* pEnt = entityList.GetFirst(); pEnt && o_AvoidanceArrays.iNumEntities < CEntityScanner::MAX_NUM_ENTITIES; pEnt = entityList.GetNext() )
			{
				const CVehicle * pVehicle = static_cast<const CVehicle*>(pEnt);
				if(pVehicle)
				{
					// Peds should not avoid certain entities
					if(pVehicle == in_Params.pDontAvoidEntity)
					{
						continue;
					}

					Vec3V vVehiclePos3d(pVehicle->GetTransform().GetPosition());
					const Vec2V vOtherPos(vVehiclePos3d.GetIntrin128ConstRef());

					if( (Abs(in_Params.vPedPos3d.GetZ() - vVehiclePos3d.GetZ()) > fTwo).Getb() )
					{
						continue;
					}

					if ( !pVehicle->IsInPathServer() )
					{
						continue;
					}

					if((DistSquared(vPedPos, vOtherPos) < square(in_Params.fScanDistance)).Getb())
					{
						const TDynamicObject& obj = CPathServerGta::m_PathServerThread.GetDynamicObject(pVehicle->GetPathServerDynamicObjectIndex());
						o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][0] = Vec2V(obj.m_vVertices[0].x, obj.m_vVertices[0].y);
						o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][1] = Vec2V(obj.m_vVertices[1].x, obj.m_vVertices[1].y);
						o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][2] = Vec2V(obj.m_vVertices[2].x, obj.m_vVertices[2].y);
						o_AvoidanceArrays.vOtherEntBoundVerts[o_AvoidanceArrays.iNumEntities][3] = Vec2V(obj.m_vVertices[3].x, obj.m_vVertices[3].y);
						o_AvoidanceArrays.vOtherEntVel[o_AvoidanceArrays.iNumEntities] = RCC_VEC3V(pVehicle->GetVelocity()).GetXY();
						o_AvoidanceArrays.iNumEntities++;
					}
				}
			}
		}
	}
	
	void GeneratePedAvoidanceArrays(sPedAvoidanceArrays& o_AvoidanceArrays, const sAvoidanceParams& in_Params)
	{

		if ( !in_Params.pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignore_All ) )
		{
			const ScalarV fTwo( V_TWO );
			const Vec2V vPedPos(in_Params.vPedPos3d.GetIntrin128ConstRef());

			const Vec2V vStartToPed = vPedPos - in_Params.vSegmentStart;
			const ScalarV vDotFromPath = Dot(in_Params.vUnitSegment, vStartToPed);
			const Vec2V vClosestPoint = in_Params.vSegmentStart + in_Params.vUnitSegment * vDotFromPath;
			const ScalarV vDistFromPath = Dist(vClosestPoint, vPedPos);

			// Create list of peds which may be obstructions
			const CEntityScannerIterator entityList = in_Params.pPed->GetPedIntelligence()->GetNearbyPeds();
			for( const CEntity* pEnt = entityList.GetFirst(); pEnt && o_AvoidanceArrays.iNumEntities < CEntityScanner::MAX_NUM_ENTITIES; pEnt = entityList.GetNext() )
			{
				const CPed * pOtherPed = static_cast<const CPed*>(pEnt);
				if(pOtherPed)
				{
					// Peds should not avoid certain entities
					if(pOtherPed == in_Params.pDontAvoidEntity)
					{
						continue;
					}

					// Peds will not bother with avoiding dead-peds, unless they are wandering non-mission peds
					if(pOtherPed->IsDead() && !in_Params.bShouldAvoidDeadPeds)
					{
						continue;
					}

					// Peds in cars shouldn't be avoided!
					if(pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pOtherPed->GetMyVehicle())
					{
						continue;
					}

					static bool s_disableForFleeingPeds = false;
					if(s_disableForFleeingPeds &&
						in_Params.pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning) &&
						pOtherPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning))
					{
						// Disable only if both are running!
						if (in_Params.pPed->GetMotionData()->GetCurrentMoveBlendRatio().y > MBR_RUN_BOUNDARY &&
							pOtherPed->GetMotionData()->GetCurrentMoveBlendRatio().y > MBR_RUN_BOUNDARY)
						{
							continue;
						}
					}

					// We might want to make this check more "neat" but for bicycles and bikes we don't want
					// to avoid peds that are mostly on the bike which they are for most of the animation.
					if (pOtherPed->GetAttachState() == ATTACH_STATE_PED_ENTER_CAR)
					{
						fwEntity* pEntity = pOtherPed->GetAttachParent();
						if (pEntity && pEntity->GetType() == ENTITY_TYPE_VEHICLE)
						{
							CVehicle* pVehicle = (CVehicle*)pEntity;
							if (pVehicle->InheritsFromBike())
							{
								continue;
							}
						}
					}

					// this ped is ignored by everyone for steering purposes
					if(pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignored_by_All ))
					{
						continue;
					}

					// Too light weight, get out of my way!
					if(pOtherPed->GetMass() < CTaskMoveGoToPoint::ms_fMinMass)
					{
						continue;
					}

					// this ped is a member of group 1 and I'm avoiding group1 peds then ignore this ped
					if( pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Member_of_Group1 ) 
						&& in_Params.pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignore_Group1 ) )
					{
						continue;
					}

					if(pOtherPed->GetMyMount())
					{
						continue;
					}

					Vec3V vOtherPedPos3d(pOtherPed->GetTransform().GetPosition());
					const Vec2V vOtherPos(vOtherPedPos3d.GetIntrin128ConstRef());

					if( (Abs(in_Params.vPedPos3d.GetZ() - vOtherPedPos3d.GetZ()) > fTwo).Getb() )
					{
						continue;
					}

					if ( !CTaskMoveGoToPoint::ms_bEnableSteeringAroundPedsBehindMe )
					{
						static float s_BackwardsAngleMin = -0.26f; // -cos(15.0f)
						static float s_BackwardsAngleMax = -1.0f;
						static float s_DistFromPathToUseMinBackwardsAngle = 0.75f;
						static float s_DistFromPathToUseMaxBackwardsAngle = 1.75f;

						const Vec2V vToPed = vOtherPos - vPedPos;
						const ScalarV fDotToTarget = Dot(in_Params.vToTarget, vToPed);
						const ScalarV vTValue = Clamp((vDistFromPath - ScalarV(s_DistFromPathToUseMinBackwardsAngle) ) / (ScalarV(s_DistFromPathToUseMaxBackwardsAngle)- ScalarV(s_DistFromPathToUseMinBackwardsAngle)), ScalarV(0.0f), ScalarV(1.0f));
						const ScalarV vBackwardsAngleMax = Lerp(vTValue, ScalarV(s_BackwardsAngleMin), ScalarV(s_BackwardsAngleMax));
						
						if ( ( fDotToTarget < vBackwardsAngleMax ).Getb() )
						{
							continue;
						}
					}

					if((DistSquared(vPedPos, vOtherPos) < square(in_Params.fScanDistance)).Getb())
					{
						Vec2V vOtherVel = RCC_VEC3V(pOtherPed->GetVelocity()).GetXY();

						ScalarV fOtherPedRadius(pOtherPed->GetCapsuleInfo()->GetHalfWidth());
						if ( (MagSquared(vOtherVel) > square(fTwo) ).Getb() )
						{
							const ScalarV fRunningRadiusExtra( 0.2f );	// 0.1f
							fOtherPedRadius += fRunningRadiusExtra;
						}

						// avoid weird peds by a slightly larger amounts
						if ( pOtherPed->GetPedModelInfo()->GetPersonalitySettings().GetIsWeird() 
							&& !in_Params.pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Avoidance_Ignore_WeirdPedBuffer ) 
							&& !in_Params.pPed->m_nFlags.bInMloRoom 
							&& in_Params.pPed->PopTypeIsRandom() )
						{
							// don't apply if both peds are in the same ped group
							if(in_Params.pPed->GetPedGroupIndex() == PEDGROUP_INDEX_NONE || in_Params.pPed->GetPedGroupIndex() != pOtherPed->GetPedGroupIndex())
							{
								const ScalarV fTrevorRadiusExtra(V_HALF);
								fOtherPedRadius += fTrevorRadiusExtra;
							}
						}

						if (pOtherPed->GetTaskData().GetIsFlagSet(CTaskFlags::ExpandAvoidanceRadius)
							&& in_Params.pPed->GetTaskData().GetIsFlagSet(CTaskFlags::ExpandAvoidanceRadius))
						{
							const ScalarV fExtraTaskFlagRadius(V_ONE);
							fOtherPedRadius += fExtraTaskFlagRadius;
						}

						o_AvoidanceArrays.vOtherEntPos[o_AvoidanceArrays.iNumEntities] = vOtherPos;
						o_AvoidanceArrays.vOtherEntVel[o_AvoidanceArrays.iNumEntities] = vOtherVel;
						o_AvoidanceArrays.fOtherEntRadii[o_AvoidanceArrays.iNumEntities] = fOtherPedRadius;
						o_AvoidanceArrays.iNumEntities++;
					}
				}
			}
		}

		// Also nearby fires
		if(in_Params.pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_AVOID_FIRE))
		{
			ScalarV fTwo(V_TWO);
			const Vec2V vPedPos(in_Params.vPedPos3d.GetIntrin128ConstRef());

			const Vector3 * pNearbyFires = in_Params.pPed->GetPedIntelligence()->GetEventScanner()->GetFireScanner().GetNearbyFires();
			for(s32 f=0; f<CNearbyFireScanner::NUM_NEARBY_FIRES && o_AvoidanceArrays.iNumEntities < CEntityScanner::MAX_NUM_ENTITIES; f++)
			{
				if(pNearbyFires->IsZero())
					break;

				if( (Abs(in_Params.vPedPos3d.GetZ() - RCC_VEC3V(pNearbyFires[f]).GetZ()) > fTwo).Getb() )
				{
					continue;
				}
				Vec2V vFirePos;
				vFirePos.SetIntrin128( RCC_VEC3V(pNearbyFires[f]).GetIntrin128ConstRef() );

				if((DistSquared(vPedPos, vFirePos) < square(in_Params.fScanDistance)).Getb())
				{
					o_AvoidanceArrays.vOtherEntPos[o_AvoidanceArrays.iNumEntities] = vFirePos;
					o_AvoidanceArrays.vOtherEntVel[o_AvoidanceArrays.iNumEntities] = Vec2V(ScalarV(V_ZERO), ScalarV(V_ZERO));
					o_AvoidanceArrays.fOtherEntRadii[o_AvoidanceArrays.iNumEntities] = RCC_VEC3V(pNearbyFires[f]).GetW();
					o_AvoidanceArrays.iNumEntities++;
				}
			}
		}
	}

	void ComputeMaxAngleLeftAndRight(ScalarV_Ref o_LeftAngle, ScalarV_Ref o_RightAngle, const sAvoidanceParams& in_Parameters )
	{
#if __BANK && defined(DEBUG_DRAW)
		bool bDebugDraw = false;
		if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAvoidance
			|| CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksFullDebugging 
			|| CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksNoText )
		{
			if(CPedDebugVisualiserMenu::GetFocusPed()==NULL)
			{
				bDebugDraw = true;
			}
			else if(in_Parameters.pPed == CPedDebugVisualiserMenu::GetFocusPed())
			{
				bDebugDraw = true;	
			}
		}
#endif


		Vector3 vPedPosition = VEC3V_TO_VECTOR3(in_Parameters.vPedPos3d);
		Vector3 vTargetPosition = vPedPosition + VEC3V_TO_VECTOR3(in_Parameters.vToTarget);
		Vector3 vLeftVector =  -VEC3V_TO_VECTOR3(in_Parameters.vUnitSegmentRight) * in_Parameters.fScanDistance.Getf();
		Vector3 vRightVector = VEC3V_TO_VECTOR3(in_Parameters.vUnitSegmentRight) * in_Parameters.fScanDistance.Getf();
		Vector3 vIntersection, vNavLosVertex1, vNavLosVertex2;

		if ( in_Parameters.pPed->GetNavMeshTracker().QueryLosCheck(vIntersection, vNavLosVertex1, vNavLosVertex2, vTargetPosition, vTargetPosition + vLeftVector) )
		{
			Vector3 vPedToLeft = vIntersection - vPedPosition;
			Vec2V vPedToLeft2d = Vec2V(vPedToLeft.x, vPedToLeft.y);
			o_LeftAngle = -Angle(vPedToLeft2d, in_Parameters.vToTarget);

	#if __BANK && defined(DEBUG_DRAW)
				if ( bDebugDraw )
				{
					const Vector3 z_Offset(0.0f, 0.0f, 0.25f);
					grcDebugDraw::Sphere(vNavLosVertex1 + z_Offset, 0.125f, Color_SkyBlue, false);
					grcDebugDraw::Sphere(vNavLosVertex2 + z_Offset, 0.125f, Color_pink, false);
					grcDebugDraw::Line(vNavLosVertex1 + z_Offset, vNavLosVertex2 + z_Offset, Color_pink);
				}
	#endif

					}
		
		if ( in_Parameters.pPed->GetNavMeshTracker().QueryLosCheck(vIntersection, vNavLosVertex1, vNavLosVertex2, vTargetPosition, vTargetPosition + vRightVector) )
		{
			Vector3 vPedToRight = vIntersection - vPedPosition;
			Vec2V vPedToRight2d = Vec2V(vPedToRight.x, vPedToRight.y);
			o_RightAngle = Angle(vPedToRight2d, in_Parameters.vToTarget);

	#if __BANK && defined(DEBUG_DRAW)
				if ( bDebugDraw )
				{
					const Vector3 z_Offset(0.0f, 0.0f, 0.25f);
					grcDebugDraw::Sphere(vNavLosVertex1 + z_Offset, 0.125f, Color_SkyBlue, false);
					grcDebugDraw::Sphere(vNavLosVertex2 + z_Offset, 0.125f, Color_pink, false);
					grcDebugDraw::Line(vNavLosVertex1 + z_Offset, vNavLosVertex2 + z_Offset, Color_black);
				}
	#endif
			}
		}


	bool ProcessAvoidance(sAvoidanceResults& o_Results, const sAvoidanceParams& in_Parameters)
	{

#if __BANK && defined(DEBUG_DRAW)
		bool bDebugDraw = false;
		if(CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eAvoidance
			|| CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksFullDebugging 
			|| CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksNoText )
		{
			if(CPedDebugVisualiserMenu::GetFocusPed()==NULL)
			{
				bDebugDraw = true;
			}
			else if(in_Parameters.pPed == CPedDebugVisualiserMenu::GetFocusPed())
			{
				bDebugDraw = true;	
			}
		}
#endif
		
		// Don't avoid if we're on a boat.  We will very likely walk off into the water.
		if(in_Parameters.pPed->GetGroundPhysical() && in_Parameters.pPed->GetGroundPhysical()->GetType()==ENTITY_TYPE_VEHICLE && ((CVehicle*)in_Parameters.pPed->GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT)
		{
			return false;
		}

		const Vec2V vPedVel = RCC_VEC3V(in_Parameters.pPed->GetVelocity()).GetXY();
		static const ScalarV fStillVelSqr( 0.1f * 0.1f );
		if((MagSquared(vPedVel) < fStillVelSqr).Getb())
		{
			return false;
		}

		
		// compute distance from path
		const Vec2V vPedPos = in_Parameters.vPedPos3d.GetXY();
		const Vec2V vStartToPed = vPedPos - in_Parameters.vSegmentStart;

		bool bShouldPerformPedAvoidance = true;
		bool bShouldPerformVehicleAvoidance = true;
		bool bShouldPerformNavMeshAvoidance = (in_Parameters.fDistanceFromPath >= ScalarV(in_Parameters.m_AvoidanceSettings.m_fDistanceFromPathToEnableNavMeshAvoidance)).Getb();
		bool bShouldPerformObjectAvoidance = (in_Parameters.fDistanceFromPath >= ScalarV(in_Parameters.m_AvoidanceSettings.m_fDistanceFromPathToEnableObstacleAvoidance)).Getb();

		sPedAvoidanceArrays pedAvoidanceArrays;
		sObjAvoidanceArrays objAvoidanceArrays;
		sObjAvoidanceArrays vehAvoidanceArrays;

		if ( in_Parameters.pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundPeds ) 
			&& !in_Parameters.pPed->GetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundPeds )
			&& CTaskMoveGoToPoint::ms_bEnableSteeringAroundPeds)
		{
			GeneratePedAvoidanceArrays(pedAvoidanceArrays, in_Parameters);
		}

		if ( in_Parameters.pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundObjects ) 
			&& !in_Parameters.pPed->GetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundObjects )	
			&& CTaskMoveGoToPoint::ms_bEnableSteeringAroundObjects )
		{
			GenerateObjectAvoidanceArrays(objAvoidanceArrays, in_Parameters);
		}

		if ( in_Parameters.pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SteersAroundVehicles ) 
			&& !in_Parameters.pPed->GetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundVehicles ) 
			&& CTaskMoveGoToPoint::ms_bEnableSteeringAroundVehicles )
		{
			GenerateVehicleAvoidanceArrays(vehAvoidanceArrays, in_Parameters);
		}

		// no objects to avoid
		if ( pedAvoidanceArrays.iNumEntities <= 0 
			&& objAvoidanceArrays.iNumEntities <= 0 
			&& vehAvoidanceArrays.iNumEntities <= 0 )
		{
			return false;
		}

		static const ScalarV fZero( V_ZERO );
		static const ScalarV fOne( V_ONE );

		const Vec2V vToTarget = in_Parameters.vToTarget;
		const Vec2V vPedFwd = (in_Parameters.pPed->IsStrafing() ? Normalize(vPedVel) : in_Parameters.pPed->GetTransform().GetB().GetXY());

		const Vec2V vUnitToTarget = vToTarget;
		const ScalarV fMaxAngleRelToDesired(CTaskMoveGoToPoint::ms_MaxAngleAvoidanceRelToDesired * DtoR);
		const ScalarV fMaxAngleRelToForward(CTaskMoveGoToPoint::ms_MaxAngleAvoidanceRelToDesired * DtoR);
		const ScalarV fMaxSteerDistanceFromSegment(CTaskMoveGoToPoint::ms_MaxDistanceFromPath);
		const ScalarV fMinSteerDistanceFromSegment(CTaskMoveGoToPoint::ms_MinDistanceFromPath);

		ScalarV fCollisionRadiusExtra(CTaskMoveGoToPoint::ms_CollisionRadiusExtra);
		ScalarV fTangentRadiusExtra(CTaskMoveGoToPoint::ms_TangentRadiusExtra);
		ScalarV fObjectBoxPaddingExtra(CTaskMoveGoToPoint::ms_CollisionRadiusExtraTight);

		if (in_Parameters.pPed->GetPedResetFlag( CPED_RESET_FLAG_UseTighterAvoidanceSettings ))
		{
			fCollisionRadiusExtra = ScalarV(CTaskMoveGoToPoint::ms_CollisionRadiusExtraTight);
			fTangentRadiusExtra = ScalarV(CTaskMoveGoToPoint::ms_TangentRadiusExtraTight);
		}

		const ScalarV fPedRadius( in_Parameters.pPed->GetCapsuleInfo()->GetHalfWidth() );
		const ScalarV fPedVelMag( in_Parameters.pPed->GetVelocity().XYMag() );
		const ScalarV fMaxTValue( in_Parameters.m_AvoidanceSettings.m_fMaxAvoidanceTValue );

		ScalarV fAngleRelToForwardWeight( CTaskMoveGoToPoint::ms_AngleRelToForwardAvoidanceWeight );
		ScalarV fTimeOfCollisionWeight( CTaskMoveGoToPoint::ms_TimeOfCollisionAvoidanceWeight );
		ScalarV fDistanceFromPathWeight( CTaskMoveGoToPoint::ms_DistanceFromPathAvoidanceWeight );
		ScalarV fAngleRelToDesiredWeight ( CTaskMoveGoToPoint::ms_AngleRelToDesiredAvoidanceWeight );

		if ( in_Parameters.pPed->GetMotionData()->GetCurrentMoveBlendRatio().y > MBR_RUN_BOUNDARY )
		{
			// penalize turning more when running
			fAngleRelToForwardWeight *= ScalarV(1.25f);
		}


		const ScalarV fBestPossibleScore( fMaxAngleRelToDesired * fAngleRelToDesiredWeight + (fMaxTValue * fPedVelMag * fTimeOfCollisionWeight) );

		ScalarV fRelToTargetAngleLeft( V_FLT_MAX );
		ScalarV fRelToTargetAngleRight( V_NEG_FLT_MAX );
		ScalarV fRelToForwardAngleLeft( V_ZERO );
		ScalarV fRelToForwardAngleRight( V_ZERO );

		ScalarV fRelToTargetTestAngle( V_ZERO );
		ScalarV fRelToTargetBestAngle( V_ZERO );
		ScalarV fRelToForwardTestAngle( V_ZERO );
		ScalarV fRelToForwardBestAngle( V_ZERO );

		ScalarV fCollisionTime(V_FLT_MAX);
		ScalarV fPreviousCollisionTime(V_FLT_MAX);
		ScalarV fAvoidedObjectCollisionTime(V_FLT_MAX);

		ScalarV fCollisionSpeed(0.0f);
		ScalarV fPreviousCollisionSpeed(0.0f);
		ScalarV fAvoidedObjectCollisionSpeed(0.0f);

		ScalarV fBestScore( V_ZERO );
		ScalarV fTestScore( fBestPossibleScore );

		ScalarV fLeftScore( V_ZERO );
		ScalarV fRightScore( V_ZERO );

		ScalarV fLeftPenaltyScore( V_ZERO );
		ScalarV fRightPenaltyScore( V_ZERO );

		ScalarV fLeftRelToTargetAngleMax(-CTaskMoveGoToPoint::ms_MaxAngleAvoidanceRelToDesired * DtoR);
		ScalarV fRightRelToTargetAngleMax(CTaskMoveGoToPoint::ms_MaxAngleAvoidanceRelToDesired * DtoR);
		ComputeMaxAngleLeftAndRight(fLeftRelToTargetAngleMax, fRightRelToTargetAngleMax, in_Parameters);

		static bool s_tighterAngleForFleeingPeds = true;
		if((s_tighterAngleForFleeingPeds &&
			in_Parameters.pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning) &&
			in_Parameters.pPed->GetMotionData()->GetCurrentMoveBlendRatio().y > MBR_RUN_BOUNDARY)
			|| in_Parameters.pPed->GetPedResetFlag( CPED_RESET_FLAG_UseTighterAvoidanceSettings ) )
		{
			fLeftRelToTargetAngleMax = Max(fLeftRelToTargetAngleMax, ScalarV(-CTaskMoveGoToPoint::ms_MaxRelToTargetAngleFlee * DtoR));
			fRightRelToTargetAngleMax = Min(fRightRelToTargetAngleMax, ScalarV(CTaskMoveGoToPoint::ms_MaxRelToTargetAngleFlee * DtoR));
		}

		const ScalarV normalizedDistFromPath = Max(( in_Parameters.fDistanceFromPath - fMinSteerDistanceFromSegment ), fZero) / (fMaxSteerDistanceFromSegment - fMinSteerDistanceFromSegment);

		if ( ( Dot(in_Parameters.vUnitSegmentRight, vStartToPed) <= fZero ).Getb() )
		{
			fLeftPenaltyScore = normalizedDistFromPath * fDistanceFromPathWeight;
		}
		else
		{
			fRightPenaltyScore = normalizedDistFromPath * fDistanceFromPathWeight;
		}

		// const Vec2V vDesiredVel = vUnitToTarget * fPedVelMag;
		const Vec2V vUnitDirRight(vUnitToTarget.GetYf(), -vUnitToTarget.GetXf());

		// initialize test score
		fRelToForwardTestAngle = Angle(vUnitToTarget, vPedFwd);
		ScalarV fAngleRelToDesiredPenalty = ScalarV(V_ZERO);
		ScalarV fAngleRelToForwardPenalty = ( Min(fRelToForwardTestAngle, fMaxAngleRelToForward ) ) * fAngleRelToForwardWeight;

		fTestScore = fBestPossibleScore;
		fTestScore -= fAngleRelToDesiredPenalty;			 
		fTestScore -= fAngleRelToForwardPenalty;		

		while( (fTestScore > fBestScore).Getb() )
		{
			Vector3 vNavLosVertex1, vNavLosVertex2, vIntersection;
			const Vec2V vTestVecDir = Rotate( vUnitToTarget, Negate(fRelToTargetTestAngle) );	
			const Vec2V vTestVecVel = vTestVecDir * fPedVelMag;

			eAvoidanceCollisionType avoidanceCollisionType = eAvoidanceCollisionType_None;

			ScalarV closest_s2(0.0f);
			ScalarV closest_t(V_FLT_MAX);	
			int closestPedIndex = pedAvoidanceArrays.iNumEntities;
			int closestObjIndex = objAvoidanceArrays.iNumEntities;
			int closestVehIndex = vehAvoidanceArrays.iNumEntities;

			//
			// step 1
			// find closest collision
			//
			if ( bShouldPerformPedAvoidance )
			{
				for ( int i = 0; i < pedAvoidanceArrays.iNumEntities; i++ )
				{
					Vec2V vRelTestVel = vTestVecVel - pedAvoidanceArrays.vOtherEntVel[i];

					ScalarV t(V_FLT_MAX);
					if ( circle_avoidance::compute_circle_collision(t, vPedPos, vTestVecVel, fPedRadius + fCollisionRadiusExtra
						, pedAvoidanceArrays.vOtherEntPos[i], pedAvoidanceArrays.vOtherEntVel[i], pedAvoidanceArrays.fOtherEntRadii[i] ) )
					{
						if ( (t < fMaxTValue).Getb() )
						{
							if ( (t < closest_t).Getb() )
							{
								closest_s2 = MagSquared(vRelTestVel);
								closest_t = t;
								closestPedIndex = i;
								avoidanceCollisionType = eAvoidanceCollisionType_Ped;
							}
						}
					}				
				}
			}

			if ( bShouldPerformObjectAvoidance )
			{
				for ( int i = 0; i < objAvoidanceArrays.iNumEntities; i++ )
				{
					float t = FLT_MAX;
					Vec2V vIntersection;
					Vec2V vVertex1;
					Vec2V vVertex2;
					Vec2V vTestVector =  vTestVecDir * in_Parameters.fScanDistance;
					Vec2V vRelTestVel = vTestVecVel - objAvoidanceArrays.vOtherEntVel[i];

					if (box_avoidance::compute_ray_box_intersection(vIntersection, vVertex1, vVertex2, vPedPos, vPedPos + vTestVector, objAvoidanceArrays.vOtherEntBoundVerts[i] ) )
					{		
						Vec2V delta = vIntersection - vPedPos;
						if ( fabs(vTestVecVel.GetXf()) > 0.0f )
						{
							t = fabs(delta.GetXf() / vTestVecVel.GetXf());
						}
						else
						{
							t = fabs(delta.GetXf() / vTestVecVel.GetYf());
						}

						if ( t < fMaxTValue.Getf() )
						{
							if ( t < closest_t.Getf() )
							{
								closest_s2 = MagSquared(vRelTestVel);
								closest_t = ScalarV(t);
								avoidanceCollisionType = eAvoidanceCollisionType_Obj;

#if __BANK && defined(DEBUG_DRAW)
								if ( bDebugDraw )
								{
									const Vector3 z_Offset(0.0f, 0.0f, 0.25f);
									Vector3 v1 = Vector3(vVertex1.GetXf(), vVertex1.GetYf(), in_Parameters.vPedPos3d.GetZf());
									Vector3 v2 = Vector3(vVertex2.GetXf(), vVertex2.GetYf(), in_Parameters.vPedPos3d.GetZf());
									grcDebugDraw::Sphere(v1 + z_Offset, 0.125f, Color_blue, false);
									grcDebugDraw::Sphere(v2 + z_Offset, 0.125f, Color_green, false);
									grcDebugDraw::Line(v1 + z_Offset, v2 + z_Offset, Color_brown);
								}
#endif

							}	
						}	
					}
				}
			}

			if ( bShouldPerformVehicleAvoidance)
			{
				for ( int i = 0; i < vehAvoidanceArrays.iNumEntities; i++ )
				{
					float t = FLT_MAX;
					Vec2V vIntersection;
					Vec2V vVertex1;
					Vec2V vVertex2;
					Vec2V vTestVector = vTestVecDir * in_Parameters.fScanDistance;
					Vec2V vRelTestVel = vTestVecVel - vehAvoidanceArrays.vOtherEntVel[i];

					if (box_avoidance::compute_ray_box_intersection(vIntersection, vVertex1, vVertex2, vPedPos, vPedPos + vTestVector, vehAvoidanceArrays.vOtherEntBoundVerts[i] ) )
					{		
						Vec2V delta = vIntersection - vPedPos;
						if ( fabs(vRelTestVel.GetXf()) > 0.0f )
						{
							t = fabs(delta.GetXf() / vRelTestVel.GetXf());
						}
						else
						{
							t = fabs(delta.GetXf() / vRelTestVel.GetYf());
						}

						if ( t < fMaxTValue.Getf() )
						{
							if ( t < closest_t.Getf() )
							{
								closest_s2 = MagSquared(vRelTestVel);
								closest_t = ScalarV(t);
								avoidanceCollisionType = eAvoidanceCollisionType_Veh;

#if __BANK && defined(DEBUG_DRAW)
								if ( bDebugDraw )
								{
									const Vector3 z_Offset(0.0f, 0.0f, 0.25f);
									Vector3 v1 = Vector3(vVertex1.GetXf(), vVertex1.GetYf(), in_Parameters.vPedPos3d.GetZf());
									Vector3 v2 = Vector3(vVertex2.GetXf(), vVertex2.GetYf(), in_Parameters.vPedPos3d.GetZf());
									grcDebugDraw::Sphere(v1 + z_Offset, 0.125f, Color_blue, false);
									grcDebugDraw::Sphere(v2 + z_Offset, 0.125f, Color_green, false);
									grcDebugDraw::Line(v1 + z_Offset, v2 + z_Offset, Color_brown);
								}
#endif
								
							}	
						}	
					}
				}	
			}


			// Alright so if we find nothing that will collide in our path we stop here
			// We don't want to care about the navmesh in this case
			if (closestPedIndex != pedAvoidanceArrays.iNumEntities 
				|| closestObjIndex != objAvoidanceArrays.iNumEntities
				|| closestVehIndex != vehAvoidanceArrays.iNumEntities
				)
			{
				bShouldPerformNavMeshAvoidance = true;	// Flag that at least some avoidance will be made so we need to consider navmesh also from now
				bShouldPerformObjectAvoidance = true;
				bShouldPerformVehicleAvoidance = true;
			}

			// NAVLOS collisions
			if ( bShouldPerformNavMeshAvoidance && in_Parameters.m_AvoidanceSettings.m_bAllowNavmeshAvoidance 
				&& !in_Parameters.pPed->GetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundNavMeshEdges ))
			{ 
				float t;
				Vector3 vPedPosition = VEC3V_TO_VECTOR3(in_Parameters.vPedPos3d);
				Vector3 vTestVector =  VEC3V_TO_VECTOR3(vTestVecDir) * in_Parameters.fScanDistance.Getf();
				if ( in_Parameters.pPed->GetNavMeshTracker().QueryLosCheck(vIntersection, vNavLosVertex1, vNavLosVertex2, vPedPosition, vPedPosition + vTestVector) )
				{		
					Vector3 delta = vIntersection - vPedPosition;
					if ( fabs(vTestVecVel.GetXf()) > 0.0f )
					{
						t = fabs(delta.x / vTestVecVel.GetXf());
					}
					else
					{
						t = fabs(delta.y / vTestVecVel.GetYf());
					}

					if ( t < fMaxTValue.Getf() )
					{
						if ( t < closest_t.Getf() )
						{
							closest_s2 = MagSquared(vTestVecVel);
							closest_t = ScalarV(t);
							avoidanceCollisionType = eAvoidanceCollisionType_NavMesh;

#if __BANK && defined(DEBUG_DRAW)
							if ( bDebugDraw )
							{
								const Vector3 z_Offset(0.0f, 0.0f, 0.25f);
								grcDebugDraw::Sphere(vNavLosVertex1 + z_Offset, 0.125f, Color_blue, false);
								grcDebugDraw::Sphere(vNavLosVertex2 + z_Offset, 0.125f, Color_green, false);
								grcDebugDraw::Line(vNavLosVertex1 + z_Offset, vNavLosVertex2 + z_Offset, Color_brown);
							}
#endif
						}	
					}	
				}
			}	


			//
			// finalize test score by applying collision
			//
			// switched to use temp variables because this was crashing on PC
			// due to an mmx instruction trying to work on unaligned data
			ScalarV tempMin = Min( closest_t, fMaxTValue );
			ScalarV tempSquare = square( tempMin - fMaxTValue);
			ScalarV fCollisionPenalty = (tempSquare / fMaxTValue); 
			fCollisionPenalty *= fPedVelMag;
			fCollisionPenalty *= fTimeOfCollisionWeight;
			fTestScore -= fCollisionPenalty;

			//    
			// if our score is better than the current best score
			// save this off as the potential best
			//
			if ( (fTestScore > fBestScore ).Getb() )
			{
				fBestScore = fTestScore;
				fRelToTargetBestAngle = fRelToTargetTestAngle;
				fRelToForwardBestAngle = fRelToForwardTestAngle;
				fCollisionTime = closest_t;
				fCollisionSpeed = Sqrt(closest_s2);
				fAvoidedObjectCollisionTime = fPreviousCollisionTime;
				fAvoidedObjectCollisionSpeed = fPreviousCollisionSpeed;	
			}

			// update the previous collision time
			fPreviousCollisionTime = closest_t;
			fPreviousCollisionSpeed = Sqrt(closest_s2);

			//
			// compute next left and right angle to test based on tangents to collision object
			//
			if ( avoidanceCollisionType != eAvoidanceCollisionType_None )
			{

				ScalarV leftAngle;		
				ScalarV rightAngle;		
				Vec2V left;
				Vec2V right;

				if ( avoidanceCollisionType == eAvoidanceCollisionType_Ped )
				{
					ScalarV tangentAngle;		
					Vec2V vTestLocation = pedAvoidanceArrays.vOtherEntPos[closestPedIndex] + pedAvoidanceArrays.vOtherEntVel[closestPedIndex] * closest_t;	
					Vec2V pedToOtherEntity = vTestLocation - vPedPos;
					Vec2V pedToOtherEntityDir = Normalize(pedToOtherEntity);
					ScalarV testRadius = pedAvoidanceArrays.fOtherEntRadii[closestPedIndex] + fPedRadius + fTangentRadiusExtra;

					if ( circle_avoidance::compute_circle_tangent_to_point(tangentAngle, vPedPos, vTestLocation, testRadius ) )
					{
						leftAngle = tangentAngle;
						rightAngle = Negate(tangentAngle);
					}
					else
					{
						// assume some arbitrary degree tangents
						// best guess
						float noTangentAngle = Lerp(Clamp(square(closest_t.Getf()), 0.0f, 1.0f), CTaskMoveGoToPoint::ms_NoTangentAvoidCollisionAngleMax, CTaskMoveGoToPoint::ms_NoTangentAvoidCollisionAngleMin );
						rightAngle = ScalarV(-noTangentAngle  * DtoR);
						leftAngle = ScalarV(noTangentAngle * DtoR);
					}


#if __BANK && defined(DEBUG_DRAW)
					if ( bDebugDraw )
					{
						Vector3 drawTestLocation(vTestLocation.GetXf(), vTestLocation.GetYf(), in_Parameters.vPedPos3d.GetZf());
						grcDebugDraw::Circle(drawTestLocation, pedAvoidanceArrays.fOtherEntRadii[closestPedIndex].Getf(), Color_red, XAXIS, YAXIS );
						grcDebugDraw::Circle(drawTestLocation, testRadius.Getf(), Color_purple, XAXIS, YAXIS );
					}
#endif
					left = Rotate(pedToOtherEntityDir, leftAngle);
					right = Rotate(pedToOtherEntityDir, rightAngle);

					TUNE_BOOL(s_RemoveTangentPeds, false); 
					if ( s_RemoveTangentPeds )
					{
						// remove ped from list to check
						pedAvoidanceArrays.vOtherEntPos[closestPedIndex] = pedAvoidanceArrays.vOtherEntPos[pedAvoidanceArrays.iNumEntities-1];
						pedAvoidanceArrays.vOtherEntVel[closestPedIndex] = pedAvoidanceArrays.vOtherEntVel[pedAvoidanceArrays.iNumEntities-1];
						pedAvoidanceArrays.fOtherEntRadii[closestPedIndex] = pedAvoidanceArrays.fOtherEntRadii[pedAvoidanceArrays.iNumEntities-1];
						pedAvoidanceArrays.iNumEntities--;				
					}
				}
 				else
				{
					rightAngle = ScalarV(-CTaskMoveGoToPoint::ms_LOSBlockedAvoidCollisionAngle  * DtoR);
					leftAngle = ScalarV(CTaskMoveGoToPoint::ms_LOSBlockedAvoidCollisionAngle * DtoR);

					left = Rotate(vTestVecDir, leftAngle);
					right = Rotate(vTestVecDir, rightAngle);
				}

				bool bStop = true;

				ScalarV signAngleLeft = IsGreaterThan(Dot(vUnitDirRight, left), fZero).Getb() ? fOne : Negate(fOne);
				ScalarV signAngleRight = IsGreaterThan(Dot(vUnitDirRight, right), fZero).Getb() ? fOne : Negate(fOne);

				ScalarV relToTargetAngleLeft = Max( Angle(left, vUnitToTarget) * signAngleLeft, fLeftRelToTargetAngleMax);
				ScalarV relToTargetAngleRight = Min(Angle(right, vUnitToTarget) * signAngleRight, fRightRelToTargetAngleMax);

				ScalarV relToForwardAngleLeft = Angle(left, vPedFwd);
				ScalarV relToForwardAngleRight = Angle(right, vPedFwd);


				if ( (relToTargetAngleLeft < fRelToTargetAngleLeft).Getb() )
				{
					bStop = false;

					fRelToTargetAngleLeft = relToTargetAngleLeft;
					fRelToForwardAngleLeft = relToForwardAngleLeft;

					ScalarV fAngleRelToDesiredPenalty = ( Min(Abs(fRelToTargetAngleLeft), fMaxAngleRelToDesired ) ) * fAngleRelToDesiredWeight;
					ScalarV fAngleRelToForwardPenalty = ( Min(fRelToForwardAngleLeft, fMaxAngleRelToForward ) ) * fAngleRelToForwardWeight;

					fLeftScore = fBestPossibleScore;
					fLeftScore -= fAngleRelToDesiredPenalty;			 
					fLeftScore -= fAngleRelToForwardPenalty;		
					fLeftScore -= fLeftPenaltyScore;

#if __BANK && defined(DEBUG_DRAW)
					if ( bDebugDraw )
					{
						Vec3V drawLeft(left.GetXf(), left.GetYf(), 0.0f);
						grcDebugDraw::Line(in_Parameters.vPedPos3d, in_Parameters.vPedPos3d + drawLeft, Color_orange);
					}
#endif
				}

				if ( (relToTargetAngleRight > fRelToTargetAngleRight).Getb() )
				{
					bStop = false;

					fRelToTargetAngleRight = relToTargetAngleRight;
					fRelToForwardAngleRight = relToForwardAngleRight;

					ScalarV fAngleRelToDesiredPenalty = ( Min(Abs(fRelToTargetAngleRight), fMaxAngleRelToDesired ) ) * fAngleRelToDesiredWeight;
					ScalarV fAngleRelToForwardPenalty = ( Min(fRelToForwardAngleRight, fMaxAngleRelToForward ) ) * fAngleRelToForwardWeight;

					fRightScore = fBestPossibleScore;
					fRightScore -= fAngleRelToDesiredPenalty;			 
					fRightScore -= fAngleRelToForwardPenalty;		
					fRightScore -= fRightPenaltyScore;

#if __BANK && defined(DEBUG_DRAW)
					if ( bDebugDraw )
					{
						Vec3V drawRight(right.GetXf(), right.GetYf(), 0.0f);
						grcDebugDraw::Line(in_Parameters.vPedPos3d, in_Parameters.vPedPos3d + drawRight, Color_orange1);
					}
#endif
				}	


				if (  (fLeftScore > fRightScore).Getb() )
				{
					fRelToTargetTestAngle = fRelToTargetAngleLeft;
					fRelToForwardTestAngle = fRelToForwardAngleLeft;
					fTestScore = fLeftScore;
				}
				else
				{
					fRelToTargetTestAngle = fRelToTargetAngleRight;
					fRelToForwardTestAngle = fRelToForwardAngleRight;
					fTestScore = fRightScore;
				}	

				// angles looped around
				if ( bStop )
				{
					break;
				}
			}
		}

		//
		// fill in our output parameters
		//
		Vec2V vFinalVec = Rotate( vUnitToTarget, Negate(fRelToTargetBestAngle) );
		o_Results.vAvoidanceVec = VEC3V_TO_VECTOR3( vFinalVec );
		o_Results.vAvoidanceVec.z = 0.0f;

		// 
		// @TODO 
		// need to decide whether we want to return the actual collision time or the time of collision with object being avoided
		// 
		static bool s_UseActualCollisionTime = false;
		if ( s_UseActualCollisionTime )
		{
			o_Results.fOutTimeToCollide = fCollisionTime.Getf();
			o_Results.fOutCollisionSpeed = fCollisionSpeed.Getf();
		}
		else
		{
			o_Results.fOutTimeToCollide = fAvoidedObjectCollisionTime.Getf();
			o_Results.fOutCollisionSpeed = fAvoidedObjectCollisionSpeed.Getf();
		}

		return true;
	}
}
